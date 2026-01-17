#ifndef POWER_MANAGEMENT_DEFS_H
#define POWER_MANAGEMENT_DEFS_H

#include <stddef.h>
#include <inttypes.h>
#include "esp_event.h"
#include "sdkconfig.h"

/**
 * @brief Power management states
 * In deep-sleep device mode, the SRAM state on SoC like ESP32 is not preserved, 
 * so any device state transition from REBOOT/SHUTDOWN/SLEEP always will be to INIT.
 * 
 * - INIT - initializing basic peripherals to be able to interact with PMIC (or other peripherals to indicate the state)
 * 
 * - OFF_CHARGER - as the PMIC cannot disable the system at all,
 * and just disables the battery and the system power will be applied from the charger when device is off, 
 * this state is used for these cases (e.g. display charge state on display/LED/other). 
 * Goes to SHUTDOWN when power button is not pressed and the charger is unplugged.
 * 
 * - SETUP - initiate all the rest peripherals to be able to use them, also restoring the last device state
 * 
 * - IDLE - device activity in idle mode (comes to sleep/shutdown if the inactivty time expired)
 * 
 * - ACTIVE - device activity in active mode (keeping device up for specific scenarios, e.g. music playing, OTA updating, and others)
 * 
 * - SHUTDOWN_PREPARE - prepare the device to be shutdown (e.g. stop all the activities, save the last state, show the state on device).
 * In this state, the power shutdown request (immediately or delayed) is sent to the PMIC.
 * The button wakeup interrupt will be set up anyway to be able to turn on device immediately.
 * 
 * - SHUTDOWN - device is shutdown
 * (dummy state for the cases when the PMIC accidentally applies the power of system but device turning on conditions are not met)
 * 
 * - REBOOT_PREPARE - prepare the device to be rebooted (like, display the device state, save the unsaved states, etc.) and reboot calling.
 * No need the apart REBOOT state by design.
 * 
 * - SLEEP_PREPARE - prepare the device to be put to sleep (like, display the device state, save the unsaved states, etc.), 
 * and then calling the sleep call.
 * 
 * - SLEEP - device is sleeping 
 */
typedef enum : uint8_t {
    POWER_MANAGEMENT_STATE_INIT = 0,
    POWER_MANAGEMENT_STATE_OFF_CHARGER,
    POWER_MANAGEMENT_STATE_SETUP,
    POWER_MANAGEMENT_STATE_DEV_IDLE,
    POWER_MANAGEMENT_STATE_DEV_ACTIVE,
    POWER_MANAGEMENT_STATE_SHUTDOWN_PREPARE,
    POWER_MANAGEMENT_STATE_SHUTDOWN,
    POWER_MANAGEMENT_STATE_REBOOT_PREPARE,
    POWER_MANAGEMENT_STATE_SLEEP_PREPARE,
    POWER_MANAGEMENT_STATE_SLEEP,
    POWER_MANAGEMENT_STATE_MAX
} power_management_state_t;

extern const char * POWER_MANAGEMENT_EVENT_BASE;

typedef enum {
    POWER_MANAGEMENT_EVENT_ANY = ESP_EVENT_ANY_ID,
    POWER_MANAGEMENT_EVENT_BATTERY_LOW,
    POWER_MANAGEMENT_EVENT_BATTERY_CRITICALLY_LOW,
    POWER_MANAGEMENT_EVENT_BATTERY_FULLY_CHARGED,
    POWER_MANAGEMENT_EVENT_BATTERY_DEAD,
    POWER_MANAGEMENT_EVENT_BATTERY_CONNECTED,
    POWER_MANAGEMENT_EVENT_BATTERY_TOO_COLD,
    POWER_MANAGEMENT_EVENT_BATTERY_COOL,
    POWER_MANAGEMENT_EVENT_BATTERY_WARM,
    POWER_MANAGEMENT_EVENT_BATTERY_TOO_HOT,
    POWER_MANAGEMENT_EVENT_OFF_CHARGER,
    POWER_MANAGEMENT_EVENT_CHARGE_CONNECTED_CHARGER,
    POWER_MANAGEMENT_EVENT_CHARGE_STARTED,
    POWER_MANAGEMENT_EVENT_CHARGE_WEAK,
    POWER_MANAGEMENT_EVENT_CHARGE_POWER_CHANGED,
    POWER_MANAGEMENT_EVENT_CHARGE_DISCONNECTED_CHARGER,
    POWER_MANAGEMENT_EVENT_OTG_DEVICE_CONNECTED,
    POWER_MANAGEMENT_EVENT_OTG_DEVICE_DISCONNECTED,
    POWER_MANAGEMENT_EVENT_BUTTON_RELEASED,
    POWER_MANAGEMENT_EVENT_BUTTON_PRESSED,
    POWER_MANAGEMENT_EVENT_BUTTON_CLICKED,
    POWER_MANAGEMENT_EVENT_BUTTON_LONG_PRESSED,
    POWER_MANAGEMENT_EVENT_BUTTON_VERY_LONG_PRESSED,
    POWER_MANAGEMENT_EVENT_IDLE_TIMER_EXPIRED,
    POWER_MANAGEMENT_EVENT_DEVICE_SHUTDOWN,
    POWER_MANAGEMENT_EVENT_DEVICE_SLEEP,
    POWER_MANAGEMENT_EVENT_DEVICE_REBOOT,
    POWER_MANAGEMENT_EVENT_DEVICE_SETUP_FINISHED,
    POWER_MANAGEMENT_EVENT_PMIC_STATUS_UPDATED,
    POWER_MANAGEMENT_EVENT_PMIC_CONTROL_UPDATED,
    POWER_MANAGEMENT_EVENT_BATTERY_LEVEL_UPDATED,
    POWER_MANAGEMENT_EVENT_PORT_CURRENT_UPDATED,
    POWER_MANAGEMENT_EVENT_USER,
    POWER_MANAGEMENT_EVENT_MAX
} power_management_event_t;

typedef enum {
    POWER_MANAGEMENT_BUTTON_STATE_RELEASED = 0,
    POWER_MANAGEMENT_BUTTON_STATE_PRESSED,
    POWER_MANAGEMENT_BUTTON_STATE_LONG_PRESSED,
    POWER_MANAGEMENT_BUTTON_STATE_VERY_LONG_PRESSED
} power_management_button_state_t;

typedef enum {
    POWER_MANAGEMENT_REQUEST_TYPE_IDLE_TIMER_RESET = 0,
    POWER_MANAGEMENT_REQUEST_TYPE_IDLE_INACTIVITY_TIME_SET,
    POWER_MANAGEMENT_REQUEST_TYPE_IDLE_TIMER_EXPIRED_ACTION_SET,
    POWER_MANAGEMENT_REQUEST_TYPE_ACTIVE_LOCK,
    POWER_MANAGEMENT_REQUEST_TYPE_ACTIVE_UNLOCK,
    POWER_MANAGEMENT_REQUEST_TYPE_SLEEP,
    POWER_MANAGEMENT_REQUEST_TYPE_REBOOT,
    POWER_MANAGEMENT_REQUEST_TYPE_SHUTDOWN,
    POWER_MANAGEMENT_REQUEST_TYPE_POWER_ON,
    POWER_MANAGEMENT_REQUEST_TYPE_MAX
} power_management_request_type_t;

typedef enum {
    POWER_MANAGEMENT_IDLE_TIMER_EXPIRED_ACTION_NOT = 0,
    POWER_MANAGEMENT_IDLE_TIMER_EXPIRED_ACTION_SLEEP,
    POWER_MANAGEMENT_IDLE_TIMER_EXPIRED_ACTION_SHUTDOWN
} power_management_idle_timer_expired_action_t;

inline const char * power_management_state_to_str(power_management_state_t state) {
    switch (state) {
        case POWER_MANAGEMENT_STATE_INIT: return "INIT";
        case POWER_MANAGEMENT_STATE_OFF_CHARGER: return "OFF_CHARGER";
        case POWER_MANAGEMENT_STATE_SETUP: return "SETUP";
        case POWER_MANAGEMENT_STATE_DEV_IDLE: return "DEV_IDLE";
        case POWER_MANAGEMENT_STATE_DEV_ACTIVE: return "DEV_ACTIVE";
        case POWER_MANAGEMENT_STATE_SHUTDOWN_PREPARE: return "SHUTDOWN_PREPARE";
        case POWER_MANAGEMENT_STATE_SHUTDOWN: return "SHUTDOWN";
        case POWER_MANAGEMENT_STATE_REBOOT_PREPARE: return "REBOOT_PREPARE";
        case POWER_MANAGEMENT_STATE_SLEEP_PREPARE: return "SLEEP_PREPARE";
        case POWER_MANAGEMENT_STATE_SLEEP: return "SLEEP";
        default: return "UNKNOWN";
    }
}

inline const char * power_management_idle_timer_expired_action_to_str(power_management_idle_timer_expired_action_t action) {
    switch (action) {
        case POWER_MANAGEMENT_IDLE_TIMER_EXPIRED_ACTION_NOT: return "NoAction";
        case POWER_MANAGEMENT_IDLE_TIMER_EXPIRED_ACTION_SLEEP: return "ActionSLEEP";
        case POWER_MANAGEMENT_IDLE_TIMER_EXPIRED_ACTION_SHUTDOWN: return "ActionSHUTDOWN";
        default: return "UNKNOWN";
    }
}

inline const char * power_management_event_to_str(power_management_event_t event) {
    switch (event) {
        case POWER_MANAGEMENT_EVENT_ANY: return "ANY";
        case POWER_MANAGEMENT_EVENT_BATTERY_LOW: return "BATTERY_LOW";
        case POWER_MANAGEMENT_EVENT_BATTERY_CRITICALLY_LOW: return "BATTERY_CRITICALLY_LOW";
        case POWER_MANAGEMENT_EVENT_BATTERY_FULLY_CHARGED: return "BATTERY_FULLY_CHARGED";
        case POWER_MANAGEMENT_EVENT_BATTERY_DEAD: return "BATTERY_DEAD";
        case POWER_MANAGEMENT_EVENT_BATTERY_CONNECTED: return "BATTERY_CONNECTED";
        case POWER_MANAGEMENT_EVENT_BATTERY_TOO_COLD: return "BATTERY_TOO_COLD";
        case POWER_MANAGEMENT_EVENT_BATTERY_COOL: return "BATTERY_COOL";
        case POWER_MANAGEMENT_EVENT_BATTERY_WARM: return "BATTERY_WARM";
        case POWER_MANAGEMENT_EVENT_BATTERY_TOO_HOT: return "BATTERY_TOO_HOT";
        case POWER_MANAGEMENT_EVENT_CHARGE_CONNECTED_CHARGER: return "CHARGE_CONNECTED_CHARGER";
        case POWER_MANAGEMENT_EVENT_CHARGE_STARTED: return "CHARGE_STARTED";
        case POWER_MANAGEMENT_EVENT_CHARGE_WEAK: return "CHARGE_WEAK";
        case POWER_MANAGEMENT_EVENT_CHARGE_POWER_CHANGED: return "CHARGE_POWER_CHANGED";
        case POWER_MANAGEMENT_EVENT_CHARGE_DISCONNECTED_CHARGER: return "CHARGE_DISCONNECTED_CHARGER";
        case POWER_MANAGEMENT_EVENT_OTG_DEVICE_CONNECTED: return "OTG_DEVICE_CONNECTED";
        case POWER_MANAGEMENT_EVENT_OTG_DEVICE_DISCONNECTED: return "OTG_DEVICE_DISCONNECTED";
        case POWER_MANAGEMENT_EVENT_BUTTON_RELEASED: return "BUTTON_RELEASED";
        case POWER_MANAGEMENT_EVENT_BUTTON_PRESSED: return "BUTTON_PRESSED";
        case POWER_MANAGEMENT_EVENT_BUTTON_CLICKED: return "BUTTON_CLICKED";
        case POWER_MANAGEMENT_EVENT_BUTTON_LONG_PRESSED: return "BUTTON_LONG_PRESSED";
        case POWER_MANAGEMENT_EVENT_BUTTON_VERY_LONG_PRESSED: return "BUTTON_VERY_LONG_PRESSED";
        case POWER_MANAGEMENT_EVENT_IDLE_TIMER_EXPIRED: return "IDLE_TIMER_EXPIRED";
        case POWER_MANAGEMENT_EVENT_DEVICE_SHUTDOWN: return "DEVICE_SHUTDOWN";
        case POWER_MANAGEMENT_EVENT_DEVICE_SLEEP: return "DEVICE_SLEEP";
        case POWER_MANAGEMENT_EVENT_DEVICE_REBOOT: return "DEVICE_REBOOT";
        case POWER_MANAGEMENT_EVENT_OFF_CHARGER: return "OFF_CHARGER";
        case POWER_MANAGEMENT_EVENT_DEVICE_SETUP_FINISHED: return "DEVICE_SETUP_FINISHED";
        case POWER_MANAGEMENT_EVENT_PMIC_STATUS_UPDATED: return "PMIC_STATUS_UPDATED";
        case POWER_MANAGEMENT_EVENT_PMIC_CONTROL_UPDATED: return "PMIC_CONTROL_UPDATED";
        case POWER_MANAGEMENT_EVENT_BATTERY_LEVEL_UPDATED: return "BATTERY_LEVEL_UPDATED";
        case POWER_MANAGEMENT_EVENT_PORT_CURRENT_UPDATED: return "PORT_CURRENT_UPDATED";
        case POWER_MANAGEMENT_EVENT_USER: return "USER_EVENT";
        default: return "UNKNOWN";
    }
}

typedef struct {
    power_management_request_type_t request_type;
    power_management_idle_timer_expired_action_t idle_timer_expired_action;
    uint64_t inactivity_time_ms;
} power_management_request_t;

#define POWER_MANAGEMENT_BUTTON_DEBOUNCE_TIME_MS                    CONFIG_POWER_MANAGEMENT_BUTTON_DEBOUNCE_TIME_MS
#define POWER_MANAGEMENT_BUTTON_LONG_PRESS_TIME_MS                  CONFIG_POWER_MANAGEMENT_BUTTON_LONG_PRESS_TIME_MS
#define POWER_MANAGEMENT_BUTTON_VERY_LONG_PRESS_TIME_MS             CONFIG_POWER_MANAGEMENT_BUTTON_VERY_LONG_PRESS_TIME_MS

#define POWER_MANAGEMENT_IDLE_TIMEOUT_MS                            30000
#define POWER_MANAGEMENT_IDLE_TIMEOUT_MIN_MS                        30000
#define POWER_MANAGEMENT_REQUESTS_QUEUE_SIZE                        10
#define POWER_MANAGEMENT_EVENT_AND_ACTION_ON_SLEEP_SHUTDOWN_GAP_MS  3000

#endif