# Power management library for ESP32x devices

The Power management library is intended to manage general device states in terms of power management and UX.

It's eligible for such cases as:
- device powered from mains and equipped with backup-battery
- battery-powered devices that goes to DeepSleep and wakes up periodically
- battery-powered devices with display UI

# How to install

Please note that this library is for ESP-IDF (tested with v5.5.1), not adapted to work with Arduino.

Clone this repository or download it to "components" folder of your project.

# How to use

Here is the Power management usage skeleton in main.c:
```
#include "power_management.h"   // Power management library
#include "esp_event.h"          // To init default event loop
// ...
#include "driver/i2c_master.h"  // To control the PMIC
#include "driver/gpio.h"        // To poll the button state
#include "esp_sleep.h"          // MCU sleep calls
#include "driver/rtc_io.h"      // RTC pins management
#include "esp_system.h"         // Access to restart call
// ...

void on_setup() {
    // TODO: Actions to additional check wake-up states and device/peripheral full initialization
}

void on_sleep() {
    // TODO: Actions to perform immediately before device goes to sleep (including sleep action).
    // For example, we use esp_deep_sleep_start();
    esp_deep_sleep_start();
}

void on_reboot() {
    // TODO: Actions to perform immediately before device goes to reboot, including esp_restart() call
    esp_restart();
}

void on_shutdown() {
    // TODO: Actions to prepare device to shutdown
    // including the shutdown command (immediately/delayed) sending to PMIC
    // and esp_deep_sleep_start() for cases when PMIC cannot turn off device when it's charging.
    esp_deep_sleep_start();
}

void off_charger_setup() {
    // TODO: Actions when Power management goes to PM_OFF_CHARGER
}

void off_charger_loop() {
    // TODO: Actions to be performed periodically in PM_OFF_CHARGER state
    // If you design the device powered from mains with backup-battery
    // use the request power_management_trigger_power_on() for start-up the device once the mains power is applied
}

bool on_button_state() {
    // TODO: get raw button state (handled within Power Management). 
    // It must return "true" if button is pressed (even if the button active state is LOW) 
}

bool on_charger_connected_state() {
    // TODO: get the state if charger is connected
}

bool on_device_woken_up() {
    // TODO: get the state if device is woken up, or additional checks for it
}

void pmic_event(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data) {
    power_management_event_t event = (power_management_event_t)id;
    ESP_LOGW(TAG, "PMIC event %s", power_management_event_to_str(event));

    // Here goes the event handling
    switch(event) {
        // States handling
        // ...
        default:
            break;
    }
}

void on_pmic_loop() {
    // TODO: actions in device working mode (PM_DEV_ACTIVE/PM_DEV_IDLE).
    // It performs PMIC polling, setting, battery measurement, etc.
}


extern "C" void app_main(void) {
    ESP_LOGI(TAG, "System starting");

    // Mandatory! initiate default event loop
    esp_event_loop_create_default();
    
    // TODO: button GPIO and PMIC interface init
    // ... 

    // All the callbacks must be set
    power_management_set_setup_cb(on_setup);
    power_management_set_sleep_cb(on_sleep);
    power_management_set_reboot_cb(on_reboot);
    power_management_set_shutdown_cb(on_shutdown);
    power_management_set_off_charger_setup_cb(off_charger_setup);
    power_management_set_off_charger_loop_cb(off_charger_loop);
    power_management_set_button_cb(on_button_state);
    power_management_set_charger_connected_cb(on_charger_connected_state);
    power_management_set_device_woken_up_cb(on_device_woken_up);
    power_management_set_loop_cb(on_pmic_loop);

    // Adding event handlers
    // You can use universal event handler like this
    // or you can use specific event handler
    if (power_management_register_event_handler(POWER_MANAGEMENT_EVENT_ANY, pmic_event) != ESP_OK) {
        ESP_LOGE(TAG, "Cannot register PMIC event!");
        // Erroneous state, to be defined by developer
    }
    else {
        ESP_LOGW(TAG, "PMIC event has been registered successfully!");
    }

    // And other events handlers for Power Management to be attached
    // ...

    power_management_init();
    
    // Other user code
    // ...
}
```

# How it works

The Power management state machine diagram is presented below.
![Power management FSM](images/PowerManagement.drawio.png)

The PowerManagement has the following states:
- PM_INIT - The point where PowerManagement always starts from. It initiates PowerManagement itself, checks the button state, charger state (if it's connected or not) and flag for waking up from DeepSleep state. If one of conditions is not met (charger connection, power button holding or waking up) and power is applied to device, it will be shut down.
- PM_OFF_CHARGER - the state where device comes to if it was turned off but charger is connected. It may be useful for PMICs that cannot power off consumers physically. As well, it's useful when it's needed to show the charging status when device is turned off (for example, show on screen). In this state, the apart cycle off_charger_loop is handled that may be used for PMIC polling from this state.
- PM_SETUP - init the rest part of overall device and the external peripherals of MCU. Or, it can check the wake-up flag and decide if it's needed the peripherals to initiate, and if MCU was in DeepSleep, do some actions and go back to DeepSleep before the next timer wake-up comes, or the external interrupt comes.
- PM_DEV_IDLE и PM_DEV_ACTIVE - cyclic states for working device. Both of them call pm_loop that polls PMIC periodically, measures the battery voltage, sends events to the rest system and takes actions in some cases (the deep battery discharge or battery overheating). In the state PM_DEV_IDLE, PowerManagement accounts the inactivity time (as a rule, how long the user does not interact with device), and runs the action when the inactivity timer expired (do-nothing, shutdown or sleep). User can request the ACTIVE mode entering or exiting. PM_DEV_ACTIVE does not account the inactivity time. May be useful for long actions, such as software update.
- PM_SLEEP_PREPARE, PM_SHUTDOWN_PREPARE и PM_REBOOT_PREPARE - prepare states before device shuts down, sleeps or reboots. Useful for cases when it's needed to show user that device will do soon, then does it. For example, before device sleeping, it's needed to turn external peripherals to powersave mode; before shutting down, it's needed to send to PMIC the command for delayed shutdown. In the callback for PM_SHUTDOWN_PREPARE it's recommended to initiate the DeepSleep.
- PM_SLEEP и PM_SHUTDOWN - dummy states, the device will never reach them.

The PowerManagement supports the following events to handle/send:
- POWER_MANAGEMENT_EVENT_ANY
- POWER_MANAGEMENT_EVENT_BATTERY_LOW
- POWER_MANAGEMENT_EVENT_BATTERY_CRITICALLY_LOW
- POWER_MANAGEMENT_EVENT_BATTERY_FULLY_CHARGED
- POWER_MANAGEMENT_EVENT_BATTERY_DEAD
- POWER_MANAGEMENT_EVENT_BATTERY_CONNECTED
- POWER_MANAGEMENT_EVENT_BATTERY_TOO_COLD
- POWER_MANAGEMENT_EVENT_BATTERY_COOL
- POWER_MANAGEMENT_EVENT_BATTERY_WARM
- POWER_MANAGEMENT_EVENT_BATTERY_TOO_HOT
- POWER_MANAGEMENT_EVENT_OFF_CHARGER
- POWER_MANAGEMENT_EVENT_CHARGE_CONNECTED_CHARGER
- POWER_MANAGEMENT_EVENT_CHARGE_STARTED
- POWER_MANAGEMENT_EVENT_CHARGE_WEAK
- POWER_MANAGEMENT_EVENT_CHARGE_POWER_CHANGED
- POWER_MANAGEMENT_EVENT_CHARGE_DISCONNECTED_CHARGER
- POWER_MANAGEMENT_EVENT_OTG_DEVICE_CONNECTED
- POWER_MANAGEMENT_EVENT_OTG_DEVICE_DISCONNECTED
- POWER_MANAGEMENT_EVENT_BUTTON_RELEASED
- POWER_MANAGEMENT_EVENT_BUTTON_PRESSED
- POWER_MANAGEMENT_EVENT_BUTTON_CLICKED
- POWER_MANAGEMENT_EVENT_BUTTON_LONG_PRESSED
- POWER_MANAGEMENT_EVENT_BUTTON_VERY_LONG_PRESSED
- POWER_MANAGEMENT_EVENT_IDLE_TIMER_EXPIRED
- POWER_MANAGEMENT_EVENT_DEVICE_SHUTDOWN
- POWER_MANAGEMENT_EVENT_DEVICE_SLEEP
- POWER_MANAGEMENT_EVENT_DEVICE_REBOOT
- POWER_MANAGEMENT_EVENT_DEVICE_SETUP_FINISHED
- POWER_MANAGEMENT_EVENT_PMIC_STATUS_UPDATED
- POWER_MANAGEMENT_EVENT_PMIC_CONTROL_UPDATED
- POWER_MANAGEMENT_EVENT_BATTERY_LEVEL_UPDATED
- POWER_MANAGEMENT_EVENT_PORT_CURRENT_UPDATED
- POWER_MANAGEMENT_EVENT_USER

See power_management_defs.h for states and other definitions.

The PowerManagement can be configured using menuconfig, in the section "Component config">"Device power management config".