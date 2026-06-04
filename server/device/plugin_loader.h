#pragma once

#include "device_result.h"

/**
 * Opens a plugin shared library by searching configured and runtime paths.
 * @param plugin_name The plugin name without the lib prefix or .so suffix.
 * @return The dlopen handle, or NULL when the plugin cannot be loaded.
 */
void *plugin_loader_open(const char *plugin_name);

/**
 * Loads a symbol from an opened plugin library.
 * @param handle The dlopen handle.
 * @param symbol_name The symbol name to find.
 * @param symbol Receives the loaded symbol pointer.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int plugin_loader_symbol(
    void *handle,
    const char *symbol_name,
    void **symbol
);

/**
 * Closes an opened plugin library and clears its handle.
 * @param handle Pointer to the plugin handle to close.
 */
void plugin_loader_close(void **handle);
