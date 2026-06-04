#include "plugin_registry.h"
#include "plugin_loader.h"

#include <string.h>

#define LOAD_SYMBOL(plugin, field, symbol_name) \
    do { \
        int symbol_result = plugin_loader_symbol((plugin).handle, (symbol_name), (void **)&((plugin).field)); \
        if (symbol_result != DEVICE_RESULT_OK) { \
            return symbol_result; \
        } \
    } while (0)

/**
 * Loads the LED plugin and resolves its required symbols.
 * @param plugins The plugin set to populate.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
static int load_led_plugin(PluginSet *plugins) {
    plugins->led.handle = plugin_loader_open("led");
    if (plugins->led.handle == NULL) {
        return DEVICE_RESULT_PLUGIN_LOAD_FAILED;
    }

    LOAD_SYMBOL(plugins->led, init, "led_init");
    LOAD_SYMBOL(plugins->led, cleanup, "led_cleanup");
    LOAD_SYMBOL(plugins->led, on, "led_on");
    LOAD_SYMBOL(plugins->led, on_bright, "led_on_bright");
    LOAD_SYMBOL(plugins->led, off, "led_off");
    LOAD_SYMBOL(plugins->led, is_on, "led_is_on");

    return DEVICE_RESULT_OK;
}

/**
 * Loads the buzzer plugin and resolves its required symbols.
 * @param plugins The plugin set to populate.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
static int load_buzzer_plugin(PluginSet *plugins) {
    plugins->buzzer.handle = plugin_loader_open("buzzer");
    if (plugins->buzzer.handle == NULL) {
        return DEVICE_RESULT_PLUGIN_LOAD_FAILED;
    }

    LOAD_SYMBOL(plugins->buzzer, init, "buzzer_init");
    LOAD_SYMBOL(plugins->buzzer, cleanup, "buzzer_cleanup");
    LOAD_SYMBOL(plugins->buzzer, music, "buzzer_music");
    LOAD_SYMBOL(plugins->buzzer, on, "buzzer_on");
    LOAD_SYMBOL(plugins->buzzer, off, "buzzer_off");
    LOAD_SYMBOL(plugins->buzzer, beep, "buzzer_beep");
    LOAD_SYMBOL(plugins->buzzer, is_on, "buzzer_is_on");

    return DEVICE_RESULT_OK;
}

/**
 * Loads the light sensor plugin and resolves its required symbols.
 * @param plugins The plugin set to populate.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
static int load_light_plugin(PluginSet *plugins) {
    plugins->light.handle = plugin_loader_open("light");
    if (plugins->light.handle == NULL) {
        return DEVICE_RESULT_PLUGIN_LOAD_FAILED;
    }

    LOAD_SYMBOL(plugins->light, init, "light_init");
    LOAD_SYMBOL(plugins->light, cleanup, "light_cleanup");
    LOAD_SYMBOL(plugins->light, read, "light_read");
    LOAD_SYMBOL(plugins->light, get_last_value, "light_get_last_value");

    return DEVICE_RESULT_OK;
}

/**
 * Loads the seven-segment plugin and resolves its required symbols.
 * @param plugins The plugin set to populate.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
static int load_segment_plugin(PluginSet *plugins) {
    plugins->segment.handle = plugin_loader_open("segment");
    if (plugins->segment.handle == NULL) {
        return DEVICE_RESULT_PLUGIN_LOAD_FAILED;
    }

    LOAD_SYMBOL(plugins->segment, init, "segment_init");
    LOAD_SYMBOL(plugins->segment, cleanup, "segment_cleanup");
    LOAD_SYMBOL(plugins->segment, display, "segment_display");
    LOAD_SYMBOL(plugins->segment, clear, "segment_clear");
    LOAD_SYMBOL(plugins->segment, is_enabled, "segment_is_enabled");
    LOAD_SYMBOL(plugins->segment, get_value, "segment_get_value");

    return DEVICE_RESULT_OK;
}

void plugin_registry_close(PluginSet *plugins) {
    plugin_loader_close(&plugins->segment.handle);
    plugin_loader_close(&plugins->light.handle);
    plugin_loader_close(&plugins->buzzer.handle);
    plugin_loader_close(&plugins->led.handle);
}

int plugin_registry_load(PluginSet *plugins) {
    int result;

    memset(plugins, 0, sizeof(*plugins));

    result = load_led_plugin(plugins);
    if (result != DEVICE_RESULT_OK) {
        plugin_registry_close(plugins);
        return result;
    }

    result = load_buzzer_plugin(plugins);
    if (result != DEVICE_RESULT_OK) {
        plugin_registry_close(plugins);
        return result;
    }

    result = load_light_plugin(plugins);
    if (result != DEVICE_RESULT_OK) {
        plugin_registry_close(plugins);
        return result;
    }

    result = load_segment_plugin(plugins);
    if (result != DEVICE_RESULT_OK) {
        plugin_registry_close(plugins);
        return result;
    }

    return DEVICE_RESULT_OK;
}
