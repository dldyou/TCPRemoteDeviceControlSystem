#pragma once

#include <stdbool.h>

#include "device_result.h"

typedef int (*PluginInitFn)(void);
typedef void (*PluginCleanupFn)(void);
typedef int (*PluginVoidFn)(void);
typedef int (*PluginIntFn)(int);
typedef int (*PluginStringFn)(char *);
typedef int (*PluginReadFn)(int *);
typedef bool (*PluginBoolFn)(void);
typedef int (*PluginStatusIntFn)(void);

typedef struct {
    void *handle;
    PluginInitFn init;
    PluginCleanupFn cleanup;
    PluginVoidFn on;
    PluginIntFn on_bright;
    PluginVoidFn off;
    PluginBoolFn is_on;
} LedPlugin;

typedef struct {
    void *handle;
    PluginInitFn init;
    PluginCleanupFn cleanup;
    PluginStringFn music;
    PluginVoidFn on;
    PluginVoidFn off;
    PluginIntFn beep;
    PluginBoolFn is_on;
} BuzzerPlugin;

typedef struct {
    void *handle;
    PluginInitFn init;
    PluginCleanupFn cleanup;
    PluginReadFn read;
    PluginStatusIntFn get_last_value;
} LightPlugin;

typedef struct {
    void *handle;
    PluginInitFn init;
    PluginCleanupFn cleanup;
    PluginIntFn display;
    PluginVoidFn clear;
    PluginBoolFn is_enabled;
    PluginStatusIntFn get_value;
} SegmentPlugin;

typedef struct {
    LedPlugin led;
    BuzzerPlugin buzzer;
    LightPlugin light;
    SegmentPlugin segment;
} PluginSet;

/**
 * Loads every required device plugin into a plugin set.
 * @param plugins The plugin set to initialize.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int plugin_registry_load(PluginSet *plugins);

/**
 * Closes every plugin handle stored in a plugin set.
 * @param plugins The plugin set to close.
 */
void plugin_registry_close(PluginSet *plugins);
