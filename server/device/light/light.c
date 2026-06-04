#include "light.h"

#include <pthread.h>

#include <wiringPi.h>

static pthread_mutex_t light_lock = PTHREAD_MUTEX_INITIALIZER;
static int last_light_value = -1;

int light_init(void) {
    if (wiringPiSetupGpio() == -1) {
        return DEVICE_RESULT_GPIO_SETUP_FAILED;
    }

    pinMode(LIGHT_PIN, INPUT);

    pthread_mutex_lock(&light_lock);
    last_light_value = -1;
    pthread_mutex_unlock(&light_lock);

    return DEVICE_RESULT_OK;
}

void light_cleanup(void) {
    pthread_mutex_lock(&light_lock);
    last_light_value = -1;
    pthread_mutex_unlock(&light_lock);
}

int light_read(int *value) {
    int current_value = digitalRead(LIGHT_PIN);

    pthread_mutex_lock(&light_lock);
    last_light_value = current_value;
    pthread_mutex_unlock(&light_lock);

    if (value != NULL) {
        *value = current_value;
    }

    return DEVICE_RESULT_OK;
}

int light_get_last_value(void) {
    int value;

    pthread_mutex_lock(&light_lock);
    value = last_light_value;
    pthread_mutex_unlock(&light_lock);

    return value;
}
