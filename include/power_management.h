#ifndef POWER_MANAGEMENT_H
#define POWER_MANAGEMENT_H

#include "power_management_defs.h"
#include "esp_err.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * The callbacks that called by power management daemon.
 * Mandatory to set.
 */

/**
 * @brief Set the callback for setup.
 * 
 * The callback is being executed every time when device is turning on or waking up.
 * Useful for initialization the rest of device peripherals or handle the wakeup state.
 */
void power_management_set_setup_cb(void (*cb)());

/**
 * @brief Set the callback for sleep.
 * 
 * The callback is being executed when power management daemon is intented to set device sleep.
 * Useful for setting the device sleep setting implementation.
 */
void power_management_set_sleep_cb(void (*cb)());

/**
 * @brief Set the callback for reboot.
 * 
 * The callback is being executed when power management daemon is intented to reboot device.
 * Useful for reboot device implementation setting.
 */
void power_management_set_reboot_cb(void (*cb)());

/**
 * @brief Set the callback for shutdown.
 * 
 * The callback is being executed when power management daemon is intented to shutdown device.
 * Useful for shutdown device implementation setting.
 */
void power_management_set_shutdown_cb(void (*cb)());

/**
 * @brief Set the callbacks for off charger
 * 
 * The callback is being executed when device is powered on by charger 
 * but used not intented to turn it on.
 * 
 * Useful for scenarios when it's needed to show the device charging state to user but not turn on device.
 */
void power_management_set_off_charger_setup_cb(void (*cb)());

/**
 * @brief Set the callback for off charger loop
 * 
 * The callback is being executed repeatedly in device OFF_CHARGER state.
 * 
 * Useful for PMIC state monitoring when device is in OFF_CHARGER state.
 */
void power_management_set_off_charger_loop_cb(void (*cb)());

/**
 * @brief Set the button handle callback
 * 
 * The function must return the true if button is pressed
 */
void power_management_set_button_cb(bool (*cb)());

/**
 * @brief Set the callback to check if charger is connected
 */
void power_management_set_charger_connected_cb(bool (*cb)());

/**
 * @brief Set the callback to check if device is waking up
 */
void power_management_set_device_woken_up_cb(bool (*cb)());

/**
 * @brief Set the callback for loop interaction with PMIC
 * 
 * Repeatedly called when device is in IDLE and ACTIVE states.
 * 
 * Useful for PMIC state monitoring and commands sending to it.
 * From this function set as callback, it's possible to emit events using the API below.
 */
void power_management_set_loop_cb(void (*cb)());

/**
 * @brief Emits the power management event
 * 
 * It uses default ESP-IDF event loop.
 * To call it, make sure that esp_event_loop_create_default() is called at app start
 */
esp_err_t power_management_emit_event(power_management_event_t event, void * data, size_t data_size);

/**
 * @brief Registers the event handler for the power management events
 * 
 * It uses default ESP-IDF event loop.
 * To call it, make sure that esp_event_loop_create_default() is called at app start
 */
esp_err_t power_management_register_event_handler(
                                                power_management_event_t event, 
                                                void (*evt_cb)(
                                                                void* handler_arg, 
                                                                esp_event_base_t base, 
                                                                int32_t id, 
                                                                void* event_data
                                                            )
                                            );

/**
 * @brief Deregisters the event handler for the power management events using ESP-IDF event loop implementation.
 */
esp_err_t power_management_deregister_event_handler(
                                                power_management_event_t event, 
                                                void (*evt_cb)(
                                                                void* handler_arg, 
                                                                esp_event_base_t base, 
                                                                int32_t id, 
                                                                void* event_data
                                                            )
                                            );

/**
 * @brief Initiates the power management daemon.
 * 
 * Please note that the following callbacks must be set before calling this function:
 * 
 * - setup_cb
 * 
 * - sleeb_cb
 * 
 * - reboot_cb
 * 
 * - shutdown_cb
 * 
 * - off_charger_setup_cb
 * 
 * - off_charger_loop_cb
 * 
 * - button_cb
 * 
 * - charger_connected_cb
 * 
 * - device_woken_up_cb
 * 
 * - loop_cb
 */
void power_management_init();


/**
 * @brief Reset the inactivity timer
 * 
 * When activity in idle state is performed (for example, any button pressed, touch is touched),
 * this function should be called to reset the inactivity timer.
 */
void power_management_idle_reset_timer();

/**
 * @brief Set the idle timeout in milliseconds.
 * 
 * The timeout is used only in IDLE state.
 * Cannot be less than time set in POWER_MANAGEMENT_IDLE_TIMEOUT_MIN_MS.
 */
void power_management_idle_set_timeout(uint64_t timeout_ms);

/**
 * @brief Get the idle timeout in milliseconds.
 */
uint32_t power_management_idle_get_timeout();

/**
 * @brief Set the action for IDLE when timeout expired
 * 
 * There are 3 actions:
 * 
 * - no action
 * 
 * - shutdown
 * 
 * - sleep
 * 
 * Whatever the action set, the IDLE_TIMEOUT_EXPIRED event is emitted when IDLE timeout expired.
 * 
 * If the device is intended to use uninterruptably, use the action [no_action].
 */
void power_management_idle_timer_expired_action_set(power_management_idle_timer_expired_action_t action);

/**
 * @brief Locking the power manager in active state using these mutex functions
 * 
 * If at least one lock is acquired, the power management daemon gets to ACTIVE state, 
 * does not send the IDLE_TIMEOUT_EXPIRED event
 * and remain in ACTIVE state until the program requests lock release.
 * 
 * Please note that this lock is recursive, and the program must equal the lock_acquires and lock releases.
 */
void power_management_active_lock_acquire();
void power_management_active_lock_release();

/**
 * @brief Triggers device to sleep
 * 
 * It sets power management daemon to SLEEP_PREPARE state,
 * send the SLEEP event, awaits for gap time and call the sleep_cb.
 */
void power_management_trigger_sleep();

/**
 * @brief Triggers device to shutdown
 * 
 * It sets power management daemon to SHUTDOWN_PREPARE state,
 * send the SHUTDOWN event, awaits for gap time and call the shutdown_cb.
 */
void power_management_trigger_shutdown();

/**
 * @brief Triggers device to reboot
 * 
 * It sets power management daemon to REBOOT_PREPARE state,
 * send the REBOOT event, awaits for gap time and call the reboot_cb.
 */
void power_management_trigger_reboot();

/**
 * @brief Powers on device
 * Please note that this call will work only from OFF_CHARGE state.
 * Useful for devices that powered from mains but battery is used as a backup power source.
 */
void power_management_trigger_power_on();

#ifdef __cplusplus
}
#endif

#endif // POWER_MANAGEMENT_H