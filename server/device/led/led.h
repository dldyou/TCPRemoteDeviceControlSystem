#pragma once

#include <stdbool.h>

#include "../device_result.h"

#ifndef LED_PIN
#define LED_PIN 17
#endif

/**
 * Initializes the LED GPIO pin and resets LED state.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int led_init(void);

/**
 * Turns the LED off and releases plugin state.
 */
void led_cleanup(void);

/**
 * Turns the LED fully on.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int led_on(void);

/**
 * Turns the LED on using PWM brightness.
 * @param brightness The brightness value from 0 to 255.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int led_on_bright(int brightness);

/**
 * Turns the LED off.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int led_off(void);

/**
 * Reads the cached LED on/off state.
 * @return true if the LED is currently on, false otherwise.
 */
bool led_is_on(void);
