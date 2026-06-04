#pragma once

#include <stdbool.h>

#include "device_result.h"

typedef struct {
    bool initialized;
    bool led_on;
    bool buzzer_on;
    bool segment_enabled;
    int segment_value;
    int light_value;
} DeviceStatus;

typedef struct {
    bool led_loaded;
    bool buzzer_loaded;
    bool light_loaded;
    bool segment_loaded;
} PluginStatus;

/**
 * Loads device plugins, initializes all devices, and starts worker threads.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int device_init(void);

/**
 * Stops workers, cleans up devices, and closes loaded plugins.
 */
void device_cleanup(void);

/**
 * Turns the LED on.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int device_led_on(void);

/**
 * Turns the LED on with PWM brightness.
 * @param brightness The brightness value from 0 to 255.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int device_led_on_bright(int brightness);

/**
 * Turns the LED off.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int device_led_off(void);

/**
 * Plays a WAV-derived melody asynchronously through the buzzer.
 * @param file_path The WAV file path to read.
 * @return DEVICE_RESULT_OK on successful queueing, or an error code on failure.
 */
int device_buzzer_music(char *file_path);

/**
 * Turns the buzzer on at its default tone.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int device_buzzer_on(void);

/**
 * Turns the buzzer off and cancels pending buzzer work.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int device_buzzer_off(void);

/**
 * Queues a timed buzzer beep.
 * @param duration_ms The beep duration in milliseconds.
 * @return DEVICE_RESULT_OK on successful queueing, or an error code on failure.
 */
int device_buzzer_beep(int duration_ms);

/**
 * Reads the current light sensor value.
 * @param value Receives the sensor value.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int device_light_read(int *value);

/**
 * Displays a countdown value on the seven-segment display.
 * @param value The digit value from 0 to 9.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int device_segment_display(int value);

/**
 * Clears the seven-segment display.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int device_segment_clear(void);

/**
 * Reads the current device-manager status snapshot.
 * @return The current device status.
 */
DeviceStatus device_get_status(void);

/**
 * Reads the current plugin load status snapshot.
 * @return The current plugin load status.
 */
PluginStatus plugin_get_status(void);
