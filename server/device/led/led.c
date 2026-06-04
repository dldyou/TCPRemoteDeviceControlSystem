#include "led.h"

#include <pthread.h>

#include <softPwm.h>
#include <wiringPi.h>

static pthread_mutex_t led_lock = PTHREAD_MUTEX_INITIALIZER;
static bool led_state = false;
static bool pwm_started = false;

int led_init(void) {
    if (wiringPiSetupGpio() == -1) {
        return DEVICE_RESULT_GPIO_SETUP_FAILED;
    }

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    pthread_mutex_lock(&led_lock);
    led_state = false;
    pwm_started = false;
    pthread_mutex_unlock(&led_lock);

    return DEVICE_RESULT_OK;
}

void led_cleanup(void) {
    if (pwm_started) {
        softPwmStop(LED_PIN);
        pwm_started = false;
    }

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    pthread_mutex_lock(&led_lock);
    led_state = false;
    pthread_mutex_unlock(&led_lock);
}

int led_on(void) {
    if (pwm_started) {
        softPwmStop(LED_PIN);
        pwm_started = false;
    }

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    pthread_mutex_lock(&led_lock);
    led_state = true;
    pthread_mutex_unlock(&led_lock);

    return DEVICE_RESULT_OK;
}

int led_on_bright(int brightness) {
    if (brightness < 0 || brightness > 255) {
        return DEVICE_RESULT_INVALID_VALUE;
    }

    if (!pwm_started) {
        if (softPwmCreate(LED_PIN, 0, 255) != 0) {
            return DEVICE_RESULT_DEVICE_FAILED;
        }
        pwm_started = true;
    }

    softPwmWrite(LED_PIN, brightness);

    pthread_mutex_lock(&led_lock);
    led_state = brightness > 0;
    pthread_mutex_unlock(&led_lock);

    return DEVICE_RESULT_OK;
}

int led_off(void) {
    if (pwm_started) {
        softPwmWrite(LED_PIN, 0);
        softPwmStop(LED_PIN);
        pwm_started = false;
    }

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    pthread_mutex_lock(&led_lock);
    led_state = false;
    pthread_mutex_unlock(&led_lock);

    return DEVICE_RESULT_OK;
}

bool led_is_on(void) {
    bool state;

    pthread_mutex_lock(&led_lock);
    state = led_state;
    pthread_mutex_unlock(&led_lock);

    return state;
}
