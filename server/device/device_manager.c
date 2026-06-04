#include "device_manager.h"
#include "device_worker.h"
#include "plugin_registry.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SEGMENT_REFRESH_DELAY_MS 1000
#define SEGMENT_IDLE_DELAY_MS 20

typedef struct {
    int value;
} IntTaskContext;

static PluginSet plugins;

static pthread_mutex_t device_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t segment_lock = PTHREAD_MUTEX_INITIALIZER;

static DeviceWorker led_worker;
static DeviceWorker buzzer_worker;
static DeviceWorker light_worker;
static pthread_t segment_thread;

static bool initialized = false;
static bool segment_thread_started = false;
static bool segment_thread_running = false;
static bool segment_countdown_enabled = false;
static int segment_countdown_value = 0;

/**
 * Checks whether the device manager has completed initialization.
 * @return DEVICE_RESULT_OK if initialized, or DEVICE_RESULT_NOT_INITIALIZED.
 */
static int ensure_initialized(void) {
    int result;

    pthread_mutex_lock(&device_lock);
    result = initialized ? DEVICE_RESULT_OK : DEVICE_RESULT_NOT_INITIALIZED;
    pthread_mutex_unlock(&device_lock);

    return result;
}

/**
 * Sleeps for the requested number of milliseconds.
 * @param milliseconds The sleep duration in milliseconds.
 */
static void sleep_ms(int milliseconds) {
    usleep((useconds_t)milliseconds * 1000U);
}

/**
 * Allocates an integer task context for worker-submitted commands.
 * @param value The integer value to store.
 * @return The allocated context, or NULL on allocation failure.
 */
static IntTaskContext *create_int_context(int value) {
    IntTaskContext *context = malloc(sizeof(*context));
    if (context == NULL) {
        return NULL;
    }

    context->value = value;
    return context;
}

/**
 * Copies a string into heap memory for asynchronous worker execution.
 * @param value The string to copy.
 * @return The allocated string copy, or NULL on failure.
 */
static char *copy_string_context(const char *value) {
    char *copy;
    size_t length;

    if (value == NULL) {
        return NULL;
    }

    length = strlen(value);
    copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, value, length + 1);
    return copy;
}

/**
 * Frees a worker task context allocated for asynchronous commands.
 * @param context The context pointer to free.
 */
static void free_context(void *context) {
    free(context);
}

/**
 * Worker task wrapper for turning the LED on.
 * @param context Unused task context.
 * @return The plugin result code.
 */
static int run_led_on(void *context) {
    (void)context;
    return plugins.led.on();
}

/**
 * Worker task wrapper for setting LED brightness.
 * @param context IntTaskContext containing the brightness value.
 * @return The plugin result code.
 */
static int run_led_on_bright(void *context) {
    IntTaskContext *task_context = context;
    return plugins.led.on_bright(task_context->value);
}

/**
 * Worker task wrapper for turning the LED off.
 * @param context Unused task context.
 * @return The plugin result code.
 */
static int run_led_off(void *context) {
    (void)context;
    return plugins.led.off();
}

/**
 * Worker task wrapper for playing a buzzer melody from a WAV file.
 * @param context Heap-allocated file path string.
 * @return The plugin result code.
 */
static int run_buzzer_music(void *context) {
    return plugins.buzzer.music((char *)context);
}

/**
 * Worker task wrapper for turning the buzzer on.
 * @param context Unused task context.
 * @return The plugin result code.
 */
static int run_buzzer_on(void *context) {
    (void)context;
    return plugins.buzzer.on();
}

/**
 * Worker task wrapper for playing a timed buzzer beep.
 * @param context IntTaskContext containing the beep duration.
 * @return The plugin result code.
 */
static int run_buzzer_beep(void *context) {
    IntTaskContext *task_context = context;
    return plugins.buzzer.beep(task_context->value);
}

/**
 * Worker task wrapper for reading the light sensor.
 * @param context Pointer to the integer that receives the sensor value.
 * @return The plugin result code.
 */
static int run_light_read(void *context) {
    return plugins.light.read((int *)context);
}

/**
 * Refreshes the seven-segment countdown display on its own thread.
 * @param arg Unused thread argument.
 * @return NULL when the thread exits.
 */
static void *segment_refresh_loop(void *arg) {
    (void)arg;

    while (1) {
        bool running;
        bool enabled;
        int value;

        pthread_mutex_lock(&segment_lock);
        running = segment_thread_running;
        enabled = segment_countdown_enabled;
        value = segment_countdown_value;
        pthread_mutex_unlock(&segment_lock);

        if (!running) {
            break;
        }

        if (!enabled) {
            plugins.segment.clear();
            sleep_ms(SEGMENT_IDLE_DELAY_MS);
            continue;
        }

        plugins.segment.display(value);
        sleep_ms(SEGMENT_REFRESH_DELAY_MS);

        pthread_mutex_lock(&segment_lock);
        if (segment_thread_running && segment_countdown_enabled) {
            if (segment_countdown_value == 0) {
                segment_countdown_enabled = false;
                pthread_mutex_unlock(&segment_lock);
                plugins.segment.clear();
                device_buzzer_beep(200);
                continue;
            }

            segment_countdown_value -= 1;
        }
        pthread_mutex_unlock(&segment_lock);
    }

    plugins.segment.clear();
    return NULL;
}

/**
 * Stops the seven-segment refresh thread if it is running.
 */
static void stop_segment_thread(void) {
    bool should_join;

    pthread_mutex_lock(&segment_lock);
    segment_thread_running = false;
    segment_countdown_enabled = false;
    should_join = segment_thread_started;
    segment_thread_started = false;
    pthread_mutex_unlock(&segment_lock);

    if (should_join) {
        pthread_join(segment_thread, NULL);
    }
}

/**
 * Starts the seven-segment refresh thread.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
static int start_segment_thread(void) {
    pthread_mutex_lock(&segment_lock);
    segment_thread_running = true;
    segment_countdown_enabled = false;
    segment_countdown_value = 0;
    pthread_mutex_unlock(&segment_lock);

    if (pthread_create(&segment_thread, NULL, segment_refresh_loop, NULL) != 0) {
        pthread_mutex_lock(&segment_lock);
        segment_thread_running = false;
        pthread_mutex_unlock(&segment_lock);
        return DEVICE_RESULT_THREAD_FAILED;
    }

    pthread_mutex_lock(&segment_lock);
    segment_thread_started = true;
    pthread_mutex_unlock(&segment_lock);

    return DEVICE_RESULT_OK;
}

/**
 * Starts all per-device worker threads managed by the device manager.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
static int start_device_workers(void) {
    int result;

    result = device_worker_start(&led_worker, "led");
    if (result != DEVICE_RESULT_OK) {
        return result;
    }

    result = device_worker_start(&buzzer_worker, "buzzer");
    if (result != DEVICE_RESULT_OK) {
        device_worker_stop(&led_worker);
        return result;
    }

    result = device_worker_start(&light_worker, "light");
    if (result != DEVICE_RESULT_OK) {
        device_worker_stop(&buzzer_worker);
        device_worker_stop(&led_worker);
        return result;
    }

    result = start_segment_thread();
    if (result != DEVICE_RESULT_OK) {
        device_worker_stop(&light_worker);
        device_worker_stop(&buzzer_worker);
        device_worker_stop(&led_worker);
        return result;
    }

    return DEVICE_RESULT_OK;
}

/**
 * Stops device worker threads and cancels pending asynchronous work.
 */
static void stop_device_workers(void) {
    stop_segment_thread();
    device_worker_stop(&light_worker);
    if (plugins.buzzer.off != NULL) {
        plugins.buzzer.off();
    }
    device_worker_cancel_pending(&buzzer_worker, DEVICE_RESULT_CANCELLED);
    device_worker_stop(&buzzer_worker);
    device_worker_stop(&led_worker);
}

int device_init(void) {
    int result;

    pthread_mutex_lock(&device_lock);
    if (initialized) {
        pthread_mutex_unlock(&device_lock);
        return DEVICE_RESULT_OK;
    }
    pthread_mutex_unlock(&device_lock);

    result = plugin_registry_load(&plugins);
    if (result != DEVICE_RESULT_OK) {
        return result;
    }

    result = plugins.led.init();
    if (result != DEVICE_RESULT_OK) {
        plugin_registry_close(&plugins);
        return result;
    }

    result = plugins.buzzer.init();
    if (result != DEVICE_RESULT_OK) {
        plugins.led.cleanup();
        plugin_registry_close(&plugins);
        return result;
    }

    result = plugins.light.init();
    if (result != DEVICE_RESULT_OK) {
        plugins.buzzer.cleanup();
        plugins.led.cleanup();
        plugin_registry_close(&plugins);
        return result;
    }

    result = plugins.segment.init();
    if (result != DEVICE_RESULT_OK) {
        plugins.light.cleanup();
        plugins.buzzer.cleanup();
        plugins.led.cleanup();
        plugin_registry_close(&plugins);
        return result;
    }

    result = start_device_workers();
    if (result != DEVICE_RESULT_OK) {
        plugins.segment.cleanup();
        plugins.light.cleanup();
        plugins.buzzer.cleanup();
        plugins.led.cleanup();
        plugin_registry_close(&plugins);
        return result;
    }

    pthread_mutex_lock(&device_lock);
    initialized = true;
    pthread_mutex_unlock(&device_lock);

    return DEVICE_RESULT_OK;
}

void device_cleanup(void) {
    pthread_mutex_lock(&device_lock);
    if (!initialized) {
        pthread_mutex_unlock(&device_lock);
        return;
    }
    initialized = false;
    pthread_mutex_unlock(&device_lock);

    stop_device_workers();

    plugins.segment.cleanup();
    plugins.light.cleanup();
    plugins.buzzer.cleanup();
    plugins.led.cleanup();
    plugin_registry_close(&plugins);
}

int device_led_on(void) {
    int result = ensure_initialized();
    if (result != DEVICE_RESULT_OK) {
        return result;
    }

    return device_worker_submit(&led_worker, run_led_on, NULL, NULL, true);
}

int device_led_on_bright(int brightness) {
    IntTaskContext *context;
    int result = ensure_initialized();
    if (result != DEVICE_RESULT_OK) {
        return result;
    }

    if (brightness < 0 || brightness > 255) {
        return DEVICE_RESULT_INVALID_VALUE;
    }

    context = create_int_context(brightness);
    if (context == NULL) {
        return DEVICE_RESULT_DEVICE_FAILED;
    }

    return device_worker_submit(&led_worker, run_led_on_bright, free_context, context, true);
}

int device_led_off(void) {
    int result = ensure_initialized();
    if (result != DEVICE_RESULT_OK) {
        return result;
    }

    return device_worker_submit(&led_worker, run_led_off, NULL, NULL, true);
}

int device_buzzer_music(char *file_path) {
    char *context;
    int result = ensure_initialized();
    if (result != DEVICE_RESULT_OK) {
        return result;
    }

    if (file_path == NULL || file_path[0] == '\0') {
        return DEVICE_RESULT_INVALID_VALUE;
    }

    context = copy_string_context(file_path);
    if (context == NULL) {
        return DEVICE_RESULT_DEVICE_FAILED;
    }

    return device_worker_submit(&buzzer_worker, run_buzzer_music, free_context, context, false);
}

int device_buzzer_on(void) {
    int result = ensure_initialized();
    if (result != DEVICE_RESULT_OK) {
        return result;
    }

    return device_worker_submit(&buzzer_worker, run_buzzer_on, NULL, NULL, true);
}

int device_buzzer_off(void) {
    int result = ensure_initialized();
    if (result != DEVICE_RESULT_OK) {
        return result;
    }

    device_worker_cancel_pending(&buzzer_worker, DEVICE_RESULT_CANCELLED);
    return plugins.buzzer.off();
}

int device_buzzer_beep(int duration_ms) {
    IntTaskContext *context;
    int result = ensure_initialized();
    if (result != DEVICE_RESULT_OK) {
        return result;
    }

    if (duration_ms <= 0 || duration_ms > 10000) {
        return DEVICE_RESULT_INVALID_VALUE;
    }

    context = create_int_context(duration_ms);
    if (context == NULL) {
        return DEVICE_RESULT_DEVICE_FAILED;
    }

    return device_worker_submit(&buzzer_worker, run_buzzer_beep, free_context, context, false);
}

int device_light_read(int *value) {
    int result = ensure_initialized();
    if (result != DEVICE_RESULT_OK) {
        return result;
    }

    return device_worker_submit(&light_worker, run_light_read, NULL, value, true);
}

int device_segment_display(int value) {
    int result = ensure_initialized();
    if (result != DEVICE_RESULT_OK) {
        return result;
    }

    if (value < 0 || value > 9) {
        return DEVICE_RESULT_INVALID_VALUE;
    }

    pthread_mutex_lock(&segment_lock);
    segment_countdown_value = value;
    segment_countdown_enabled = true;
    pthread_mutex_unlock(&segment_lock);

    return DEVICE_RESULT_OK;
}

int device_segment_clear(void) {
    int result = ensure_initialized();
    if (result != DEVICE_RESULT_OK) {
        return result;
    }

    pthread_mutex_lock(&segment_lock);
    segment_countdown_enabled = false;
    segment_countdown_value = 0;
    pthread_mutex_unlock(&segment_lock);

    return plugins.segment.clear();
}

DeviceStatus device_get_status(void) {
    DeviceStatus status = { 0 };

    pthread_mutex_lock(&device_lock);
    status.initialized = initialized;
    pthread_mutex_unlock(&device_lock);

    if (!status.initialized) {
        status.light_value = -1;
        return status;
    }

    status.led_on = plugins.led.is_on();
    status.buzzer_on = plugins.buzzer.is_on();
    status.light_value = plugins.light.get_last_value();

    pthread_mutex_lock(&segment_lock);
    status.segment_enabled = segment_countdown_enabled;
    status.segment_value = segment_countdown_value;
    pthread_mutex_unlock(&segment_lock);

    return status;
}

PluginStatus plugin_get_status(void) {
    PluginStatus status;

    status.led_loaded = plugins.led.handle != NULL;
    status.buzzer_loaded = plugins.buzzer.handle != NULL;
    status.light_loaded = plugins.light.handle != NULL;
    status.segment_loaded = plugins.segment.handle != NULL;

    return status;
}
