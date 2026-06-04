#include "segment.h"

#include <pthread.h>
#include <stddef.h>

#include <wiringPi.h>

#define SEGMENT_ON_LEVEL HIGH
#define SEGMENT_OFF_LEVEL LOW

static const int segment_pins[SEGMENT_COUNT] = {
    SEGMENT_G_PIN,
    SEGMENT_F_PIN,
    SEGMENT_E_PIN,
    SEGMENT_D_PIN,
    SEGMENT_C_PIN,
    SEGMENT_B_PIN,
    SEGMENT_A_PIN,
    SEGMENT_DP_PIN,
};

static const unsigned char digit_patterns[10] = {
    0b01111110,
    0b00110000,
    0b01101101,
    0b01111001,
    0b00110011,
    0b01011011,
    0b01011111,
    0b01110000,
    0b01111111,
    0b01111011
};

static pthread_mutex_t segment_lock = PTHREAD_MUTEX_INITIALIZER;
static bool segment_enabled = false;
static int segment_value = 0;

/**
 * Turns every seven-segment output pin off.
 */
static void segment_all_off(void) {
    for (size_t i = 0; i < SEGMENT_COUNT; i++) {
        digitalWrite(segment_pins[i], SEGMENT_OFF_LEVEL);
    }
}

/**
 * Writes a digit pattern to the seven-segment GPIO pins.
 * @param pattern The segment bit pattern to output.
 */
static void segment_write_pattern(unsigned char pattern) {
    for (size_t i = 0; i < SEGMENT_COUNT; i++) {
        const int level = (pattern & (1 << i)) ? SEGMENT_OFF_LEVEL : SEGMENT_ON_LEVEL;
        digitalWrite(segment_pins[i], level);
    }
}

int segment_init(void) {
    if (wiringPiSetupGpio() == -1) {
        return DEVICE_RESULT_GPIO_SETUP_FAILED;
    }

    for (size_t i = 0; i < SEGMENT_COUNT; i++) {
        pinMode(segment_pins[i], OUTPUT);
    }

    segment_all_off();

    pthread_mutex_lock(&segment_lock);
    segment_enabled = false;
    segment_value = 0;
    pthread_mutex_unlock(&segment_lock);

    return DEVICE_RESULT_OK;
}

void segment_cleanup(void) {
    segment_all_off();

    pthread_mutex_lock(&segment_lock);
    segment_enabled = false;
    segment_value = 0;
    pthread_mutex_unlock(&segment_lock);
}

int segment_display(int value) {
    if (value < 0 || value > 9) {
        return DEVICE_RESULT_INVALID_VALUE;
    }

    segment_write_pattern(digit_patterns[value]);

    pthread_mutex_lock(&segment_lock);
    segment_enabled = true;
    segment_value = value;
    pthread_mutex_unlock(&segment_lock);

    return DEVICE_RESULT_OK;
}

int segment_clear(void) {
    segment_all_off();

    pthread_mutex_lock(&segment_lock);
    segment_enabled = false;
    segment_value = 0;
    pthread_mutex_unlock(&segment_lock);

    return DEVICE_RESULT_OK;
}

bool segment_is_enabled(void) {
    bool enabled;

    pthread_mutex_lock(&segment_lock);
    enabled = segment_enabled;
    pthread_mutex_unlock(&segment_lock);

    return enabled;
}

int segment_get_value(void) {
    int value;

    pthread_mutex_lock(&segment_lock);
    value = segment_value;
    pthread_mutex_unlock(&segment_lock);

    return value;
}
