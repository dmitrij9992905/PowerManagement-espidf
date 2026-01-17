#include "power_management.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"


static const char *TAG = "PowerManagement";

const char * POWER_MANAGEMENT_EVENT_BASE = "POWER_MANAGEMENT_EVENT";

static void (*_on_device_setup)() = NULL;
static void (*_on_device_sleep)() = NULL;
static void (*_on_device_reboot)() = NULL;
static void (*_on_device_shutdown)() = NULL;

static void (*_on_off_charger_setup)() = NULL;
static void (*_on_off_charger_loop)() = NULL;

static void (*_on_pmic_loop)() = NULL;
static bool (*_on_button_state)() = NULL;
static bool (*_on_charger_connected_state)() = NULL;
static bool (*_on_device_woken_up)() = NULL;

static uint64_t _idle_timeout_ms_set = POWER_MANAGEMENT_IDLE_TIMEOUT_MS;
static uint64_t _last_activity_millis = 0;
static int _active_lock = 0;

static power_management_button_state_t _button_state = POWER_MANAGEMENT_BUTTON_STATE_RELEASED;
static power_management_idle_timer_expired_action_t _idle_timer_expired_action = POWER_MANAGEMENT_IDLE_TIMER_EXPIRED_ACTION_NOT;

static QueueHandle_t _power_management_requests_queue;

static uint64_t pm_millis() { 
    return (uint64_t)pdTICKS_TO_MS(xTaskGetTickCount()); 
}

static void power_management_handle(void * params);
static void power_management_button_handle(void * params);

void power_management_set_setup_cb(void (*cb)()) { 
    _on_device_setup = cb; 
}

void power_management_set_sleep_cb(void (*cb)()) {
    _on_device_sleep = cb; 
}

void power_management_set_reboot_cb(void (*cb)()) {
    _on_device_reboot = cb; 
}

void power_management_set_shutdown_cb(void (*cb)()) {
    _on_device_shutdown = cb; 
}

void power_management_set_off_charger_setup_cb(void (*cb)()) {
    _on_off_charger_setup = cb;
}

void power_management_set_off_charger_loop_cb(void (*cb)()) {
    _on_off_charger_loop = cb;
}

void power_management_set_button_cb(bool (*cb)()) {
    _on_button_state = cb; 
}

void power_management_set_charger_connected_cb(bool (*cb)()) {
    _on_charger_connected_state = cb;
}

void power_management_set_device_woken_up_cb(bool (*cb)()) {
    _on_device_woken_up = cb;
}

void power_management_set_loop_cb(void (*cb)()) {
    _on_pmic_loop = cb; 
}

esp_err_t power_management_emit_event(power_management_event_t event, void * data, size_t data_size) {
    return esp_event_post(POWER_MANAGEMENT_EVENT_BASE, event, data, data_size, pdMS_TO_TICKS(1000));
}

esp_err_t power_management_register_event_handler(
                                                power_management_event_t event, 
                                                void (*evt_cb)(
                                                                void* handler_arg, 
                                                                esp_event_base_t base, 
                                                                int32_t id, 
                                                                void* event_data
                                                            )
                                            ) {
    return esp_event_handler_register(POWER_MANAGEMENT_EVENT_BASE, event, evt_cb, NULL);
}

esp_err_t power_management_deregister_event_handler(
                                                power_management_event_t event, 
                                                void (*evt_cb)(
                                                                void* handler_arg, 
                                                                esp_event_base_t base, 
                                                                int32_t id, 
                                                                void* event_data
                                                            )
                                            ) {
    return esp_event_handler_unregister(POWER_MANAGEMENT_EVENT_BASE, event, evt_cb);
}

static void power_management_send_request(
                                            power_management_request_type_t req_type, 
                                            uint64_t inactivity_time_ms, 
                                            power_management_idle_timer_expired_action_t idle_timer_expired_action
                                        ) {
    power_management_request_t req;
    req.request_type = req_type;
    req.inactivity_time_ms = inactivity_time_ms;
    req.idle_timer_expired_action = idle_timer_expired_action;

    xQueueSend(_power_management_requests_queue, &req, 10);
}

void power_management_init() {
    assert(_on_pmic_loop);
    assert(_on_device_setup);
    assert(_on_device_reboot);
    assert(_on_device_shutdown);
    assert(_on_device_sleep);
    assert(_on_button_state);
    assert(_on_charger_connected_state);
    assert(_on_device_woken_up);
    assert(_on_off_charger_setup);
    assert(_on_off_charger_loop);

    _power_management_requests_queue = xQueueCreate(POWER_MANAGEMENT_REQUESTS_QUEUE_SIZE, sizeof(power_management_request_t));
    assert(_power_management_requests_queue);

    xTaskCreate(power_management_button_handle, "button_pm", 2048, NULL, 2, NULL);
    xTaskCreate(power_management_handle, "device_pm", 4096, NULL, 20, NULL);

    ESP_LOGI(TAG, "Power management has been started");
}

void power_management_idle_reset_timer() {
    power_management_send_request(
                                    POWER_MANAGEMENT_REQUEST_TYPE_IDLE_TIMER_RESET, 
                                    0, 
                                    POWER_MANAGEMENT_IDLE_TIMER_EXPIRED_ACTION_NOT
                                );
}

void power_management_idle_set_timeout(uint64_t timeout_ms) {
    power_management_send_request(
                                    POWER_MANAGEMENT_REQUEST_TYPE_IDLE_INACTIVITY_TIME_SET, 
                                    timeout_ms, 
                                    POWER_MANAGEMENT_IDLE_TIMER_EXPIRED_ACTION_NOT
                                );
}

uint32_t power_management_idle_get_timeout() {
    return _idle_timeout_ms_set;
}

void power_management_idle_timer_expired_action_set(power_management_idle_timer_expired_action_t action) {
    power_management_send_request(
                                    POWER_MANAGEMENT_REQUEST_TYPE_IDLE_TIMER_EXPIRED_ACTION_SET, 
                                    0, 
                                    action
                                );
}

void power_management_active_lock_acquire() {
    power_management_send_request(
                                    POWER_MANAGEMENT_REQUEST_TYPE_ACTIVE_LOCK, 
                                    0, 
                                    POWER_MANAGEMENT_IDLE_TIMER_EXPIRED_ACTION_NOT
                                );
}

void power_management_active_lock_release() {
    power_management_send_request(
                                    POWER_MANAGEMENT_REQUEST_TYPE_ACTIVE_UNLOCK, 
                                    0, 
                                    POWER_MANAGEMENT_IDLE_TIMER_EXPIRED_ACTION_NOT
                                );
}

void power_management_trigger_sleep() {
    power_management_send_request(
                                    POWER_MANAGEMENT_REQUEST_TYPE_SLEEP, 
                                    0, 
                                    POWER_MANAGEMENT_IDLE_TIMER_EXPIRED_ACTION_NOT
                                );
}

void power_management_trigger_shutdown() {
    power_management_send_request(
                                    POWER_MANAGEMENT_REQUEST_TYPE_SHUTDOWN, 
                                    0, 
                                    POWER_MANAGEMENT_IDLE_TIMER_EXPIRED_ACTION_NOT
                                );
}

void power_management_trigger_reboot() {
    power_management_send_request(
                                    POWER_MANAGEMENT_REQUEST_TYPE_REBOOT, 
                                    0, 
                                    POWER_MANAGEMENT_IDLE_TIMER_EXPIRED_ACTION_NOT
                                );
}

static void power_management_button_handle(void * params) {
    bool _button_state_current = false;
    bool _button_state_old = false;
    uint64_t _button_state_change_millis = 0;

    while(1) {
        switch(_button_state) {
            case POWER_MANAGEMENT_BUTTON_STATE_RELEASED:
                {
                    _button_state_current = _on_button_state();

                    if (_button_state_old != _button_state_current) {
                        _button_state_old = _button_state_current;

                        _button_state_change_millis = pm_millis();
                    }

                    if (_button_state_old && (pm_millis()-_button_state_change_millis > POWER_MANAGEMENT_BUTTON_DEBOUNCE_TIME_MS)) {
                        ESP_LOGI(TAG, "Button pressed");
                        _button_state = POWER_MANAGEMENT_BUTTON_STATE_PRESSED;

                        power_management_emit_event(POWER_MANAGEMENT_EVENT_BUTTON_PRESSED, NULL, 0);
                    }
                }
                break;
            case POWER_MANAGEMENT_BUTTON_STATE_PRESSED:
                {
                    if (!_on_button_state()) {
                        ESP_LOGI(TAG, "Button clicked");
                        _button_state = POWER_MANAGEMENT_BUTTON_STATE_RELEASED;

                        // BUTTON_RELEASED event must be sent every time the button is released
                        power_management_emit_event(POWER_MANAGEMENT_EVENT_BUTTON_RELEASED, NULL, 0);

                        // As the button is pressed and released soon, consider as a click and send the event
                        power_management_emit_event(POWER_MANAGEMENT_EVENT_BUTTON_CLICKED, NULL, 0);

                        break;
                    }

                    if (pm_millis() - _button_state_change_millis > POWER_MANAGEMENT_BUTTON_LONG_PRESS_TIME_MS) {
                        ESP_LOGI(TAG, "Button long pressed");
                        _button_state = POWER_MANAGEMENT_BUTTON_STATE_LONG_PRESSED;

                        power_management_emit_event(POWER_MANAGEMENT_EVENT_BUTTON_LONG_PRESSED, NULL, 0);
                        break;
                    }

                    // Resetting the idle timer when button is pressed
                    _last_activity_millis = pm_millis();
                }
                break;
            case POWER_MANAGEMENT_BUTTON_STATE_LONG_PRESSED:
                {
                    if (!_on_button_state()) {
                        ESP_LOGI(TAG, "Button released from LONG_PRESSED");
                        _button_state = POWER_MANAGEMENT_BUTTON_STATE_RELEASED;
                        power_management_emit_event(POWER_MANAGEMENT_EVENT_BUTTON_RELEASED, NULL, 0);
                        break;
                    }

                    if (pm_millis() - _button_state_change_millis > POWER_MANAGEMENT_BUTTON_VERY_LONG_PRESS_TIME_MS) {
                        ESP_LOGI(TAG, "Button very long pressed");
                        _button_state = POWER_MANAGEMENT_BUTTON_STATE_VERY_LONG_PRESSED;
                        power_management_emit_event(POWER_MANAGEMENT_EVENT_BUTTON_VERY_LONG_PRESSED, NULL, 0);
                        break;
                    }
                    
                    // Resetting the idle timer when button is long-pressed
                    _last_activity_millis = pm_millis();
                }
                break;
            case POWER_MANAGEMENT_BUTTON_STATE_VERY_LONG_PRESSED:
                {
                    if (!_on_button_state()) {
                        ESP_LOGI(TAG, "Button released from VERY_LONG_PRESSED");
                        _button_state = POWER_MANAGEMENT_BUTTON_STATE_RELEASED;
                        power_management_emit_event(POWER_MANAGEMENT_EVENT_BUTTON_RELEASED, NULL, 0);
                        break;
                    }

                    // Resetting the idle timer when button is very-long-pressed
                    _last_activity_millis = pm_millis();
                }
                break;
            default:
                break;
        }

        vTaskDelay(1);
    }

    vTaskDelete(NULL);
}

static void power_management_handle(void * params) {
    power_management_state_t pm_state = POWER_MANAGEMENT_STATE_INIT;
    _last_activity_millis = pm_millis();
    bool _idle_timer_expired_event_sent = false;
    uint64_t _init_start_millis = pm_millis();
    bool _shutdown_init_log = false;

    while(1) {
        switch(pm_state) {
            case POWER_MANAGEMENT_STATE_INIT:
                {
                    ESP_LOGD(TAG, "Power management in INIT state");

                    // If in this state, button is pressed or device is waking up, turn on the device
                    if (_on_button_state() || _on_device_woken_up()) {
                        ESP_LOGW(TAG, "The button is pressed or device is waking up, going to SETUP");
                        pm_state = POWER_MANAGEMENT_STATE_SETUP;
                        
                        break;
                    }

                    // If the button not pressed but charger is connected, prepare the OFF_CHARGER state
                    else if (_on_charger_connected_state()) {
                        ESP_LOGD(TAG, "Device is powered on due to charger connecting, going to OFF_CHARGER");
                        _on_off_charger_setup();
                        vTaskDelay(pdMS_TO_TICKS(3000));
                        pm_state = POWER_MANAGEMENT_STATE_OFF_CHARGER;
                        power_management_emit_event(POWER_MANAGEMENT_EVENT_OFF_CHARGER, NULL, 0);
                        break;
                    }

                    

                    // If the time of init expired and no conditions are met, turn off the device
                    // After shutdown callback calling, the device will not be working further 
                    // until the turn on conditions are met
                    if ((pm_millis()-_init_start_millis) > CONFIG_POWER_MANAGEMENT_INIT_WAIT_FOR_BUTTON_ACTION_MS) {
                        if (!_shutdown_init_log) {
                            ESP_LOGW(TAG, "The device is powered by unknown reason, shutting down");
                            _shutdown_init_log = true;
                        }
                        _on_device_shutdown();
                        // Never been reached here because of power interruption
                        break;
                    }
                }
                break;
            case POWER_MANAGEMENT_STATE_OFF_CHARGER:
                {
                    if (_on_charger_connected_state()) {
                        _on_off_charger_loop();

                        // If button is long pressed in OFF_CHARGE state
                        // evaluate this as device turn on request
                        if (_button_state == POWER_MANAGEMENT_BUTTON_STATE_LONG_PRESSED) {
                            ESP_LOGD(TAG, "The power button is long-pressed during charging, powering on the device and going to SETUP");
                            pm_state = POWER_MANAGEMENT_STATE_SETUP;
                        }

                        vTaskDelay(pdMS_TO_TICKS(100));
                    }
                    else {
                        ESP_LOGD(TAG, "Charger is unplugged, shutting down");
                        _on_device_shutdown();
                        // Never been reached here because of power interruption
                        break;
                    }
                }
                break;
            case POWER_MANAGEMENT_STATE_SETUP:
                {
                    ESP_LOGD(TAG, "Power management in SETUP state");
                    _on_device_setup();
                    vTaskDelay(pdMS_TO_TICKS(3000));
                    power_management_emit_event(POWER_MANAGEMENT_EVENT_DEVICE_SETUP_FINISHED, NULL, 0);
                    pm_state = POWER_MANAGEMENT_STATE_DEV_IDLE;
                }
                break;
            case POWER_MANAGEMENT_STATE_DEV_IDLE:
                {
                    _on_pmic_loop();

                    // If active lock present, then set to ACTIVE state
                    if (_active_lock) {
                        ESP_LOGD(TAG, "Device is locked to activity, going to ACTIVE");
                        pm_state = POWER_MANAGEMENT_STATE_DEV_ACTIVE;
                    }

                    if (pm_millis() - _last_activity_millis > _idle_timeout_ms_set) {
                        ESP_LOGD(TAG, "Idle timeout expired");
                        if (!_idle_timer_expired_event_sent) {
                            power_management_emit_event(POWER_MANAGEMENT_EVENT_IDLE_TIMER_EXPIRED, NULL, 0);
                            _idle_timer_expired_event_sent = true;
                        }

                        switch(_idle_timer_expired_action) {
                            case POWER_MANAGEMENT_IDLE_TIMER_EXPIRED_ACTION_SHUTDOWN:
                                ESP_LOGD(TAG, "Action on idle timeout expired: SHUTDOWN");
                                pm_state = POWER_MANAGEMENT_STATE_SHUTDOWN_PREPARE;
                                break;
                            case POWER_MANAGEMENT_IDLE_TIMER_EXPIRED_ACTION_SLEEP:
                                ESP_LOGD(TAG, "Action on idle timeout expired: SLEEP");
                                pm_state = POWER_MANAGEMENT_STATE_SLEEP_PREPARE;
                                break;
                            default:
                                break;
                        }
                    }
                    else {
                        _idle_timer_expired_event_sent = false;
                    }

                    if (_button_state == POWER_MANAGEMENT_BUTTON_STATE_VERY_LONG_PRESSED) {
                        ESP_LOGD(TAG, "The button is very-long-pressed, rebooting the device");
                        vTaskDelay(pdMS_TO_TICKS(100));
                        pm_state = POWER_MANAGEMENT_STATE_REBOOT_PREPARE;
                    }
                }
                break;
            case POWER_MANAGEMENT_STATE_DEV_ACTIVE:
                {
                    // If active lock not present, then set to IDLE state
                    if (!_active_lock) {
                        ESP_LOGD(TAG, "Device is unlocked from activity, going to IDLE");
                        pm_state = POWER_MANAGEMENT_STATE_DEV_IDLE;
                    }

                    _on_pmic_loop();
                }
                break;
            case POWER_MANAGEMENT_STATE_SHUTDOWN_PREPARE:
                ESP_LOGD(TAG, "Preparing to shutdown the device");
                power_management_emit_event(POWER_MANAGEMENT_EVENT_DEVICE_SHUTDOWN, NULL, 0);
                vTaskDelay(pdMS_TO_TICKS(POWER_MANAGEMENT_EVENT_AND_ACTION_ON_SLEEP_SHUTDOWN_GAP_MS));
                _on_device_shutdown();
                // Never been reached here due to power interruption
                break;
            case POWER_MANAGEMENT_STATE_SHUTDOWN:
                // Dummy state that never been reached
                break;
            case POWER_MANAGEMENT_STATE_REBOOT_PREPARE:
                ESP_LOGD(TAG, "Preparing to reboot the device");
                power_management_emit_event(POWER_MANAGEMENT_EVENT_DEVICE_REBOOT, NULL, 0);
                vTaskDelay(pdMS_TO_TICKS(POWER_MANAGEMENT_EVENT_AND_ACTION_ON_SLEEP_SHUTDOWN_GAP_MS));
                _on_device_reboot();
                // Never been reached at the certain runtime
                break;
            case POWER_MANAGEMENT_STATE_SLEEP_PREPARE:
                ESP_LOGD(TAG, "Preparing to sleep the device");
                power_management_emit_event(POWER_MANAGEMENT_EVENT_DEVICE_SLEEP, NULL, 0);
                vTaskDelay(pdMS_TO_TICKS(POWER_MANAGEMENT_EVENT_AND_ACTION_ON_SLEEP_SHUTDOWN_GAP_MS));
                _on_device_sleep();
                // Never been reached due to power interruption the core in deep-sleep mode
                break;
            case POWER_MANAGEMENT_STATE_SLEEP:
                // Dummy state that never been reached
                break;
            default:
                break;
        }

        vTaskDelay(1);

        if (uxQueueMessagesWaiting(_power_management_requests_queue) > 0) {
            power_management_request_t req;

            if (xQueueReceive(_power_management_requests_queue, &req, 10) == pdTRUE) {
                switch(req.request_type) {
                    case POWER_MANAGEMENT_REQUEST_TYPE_IDLE_TIMER_RESET:
                        ESP_LOGD(TAG, "Resetting idle timer");
                        _last_activity_millis = pm_millis();
                        break;
                    case POWER_MANAGEMENT_REQUEST_TYPE_IDLE_INACTIVITY_TIME_SET:
                        ESP_LOGD(TAG, "Setting idle inactivity time to %d ms", req.inactivity_time_ms);
                        if (req.inactivity_time_ms >= POWER_MANAGEMENT_IDLE_TIMEOUT_MIN_MS)
                            _idle_timeout_ms_set = req.inactivity_time_ms;
                        else {
                            ESP_LOGW(
                                    TAG, 
                                    "The idle timeout set is too small: %llu, changing to %llu", 
                                    req.inactivity_time_ms, 
                                    POWER_MANAGEMENT_IDLE_TIMEOUT_MIN_MS
                                );
                            _idle_timeout_ms_set = POWER_MANAGEMENT_IDLE_TIMEOUT_MIN_MS;
                        }
                        break;
                    case POWER_MANAGEMENT_REQUEST_TYPE_ACTIVE_LOCK:
                        ESP_LOGD(TAG, "Locking device to activity");
                        _last_activity_millis = pm_millis();
                        _active_lock++;
                        break;
                    case POWER_MANAGEMENT_REQUEST_TYPE_ACTIVE_UNLOCK:
                        ESP_LOGD(TAG, "Unlocking device from activity");
                        _last_activity_millis = pm_millis();
                        _active_lock--;
                        
                        if (_active_lock < 0) _active_lock = 0;
                        break;
                    case POWER_MANAGEMENT_REQUEST_TYPE_IDLE_TIMER_EXPIRED_ACTION_SET:
                        ESP_LOGD(
                                TAG, 
                                "Setting idle timer expired action to %s", 
                                power_management_idle_timer_expired_action_to_str(req.idle_timer_expired_action)
                            );
                        _idle_timer_expired_action = req.idle_timer_expired_action;
                        break;
                    case POWER_MANAGEMENT_REQUEST_TYPE_SLEEP:
                        ESP_LOGD(TAG, "Sleep requested");
                        pm_state = POWER_MANAGEMENT_STATE_SLEEP_PREPARE;
                        break;
                    case POWER_MANAGEMENT_REQUEST_TYPE_REBOOT:
                        ESP_LOGD(TAG, "Reboot requested");
                        pm_state = POWER_MANAGEMENT_STATE_REBOOT_PREPARE;
                        break;
                    case POWER_MANAGEMENT_REQUEST_TYPE_SHUTDOWN:
                        ESP_LOGD(TAG, "Shutdown requested");
                        pm_state = POWER_MANAGEMENT_STATE_SHUTDOWN_PREPARE;
                        break;
                    default:
                        break;
                }
            }
        }

        vTaskDelay(1);
    }

    vTaskDelete(NULL);
}