#include "plugin_loader.h"

#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * Resolves the directory that contains the current executable.
 * @param buffer Receives the executable directory path.
 * @param buffer_size The size of the destination buffer.
 * @return 0 on success, -1 on failure.
 */
static int get_executable_dir(char *buffer, size_t buffer_size) {
    ssize_t length;
    char *last_slash;

    if (buffer == NULL || buffer_size == 0) {
        return -1;
    }

    length = readlink("/proc/self/exe", buffer, buffer_size - 1);
    if (length <= 0 || (size_t)length >= buffer_size) {
        return -1;
    }

    buffer[length] = '\0';
    last_slash = strrchr(buffer, '/');
    if (last_slash == NULL) {
        return -1;
    }

    *last_slash = '\0';
    return 0;
}

/**
 * Attempts to open a shared library at a specific path.
 * @param path The shared library path to open.
 * @return The dlopen handle, or NULL when loading fails.
 */
static void *try_open_path(const char *path) {
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

/**
 * Attempts to load a plugin from a configured base directory.
 * @param base_dir The directory that may contain plugin libraries.
 * @param plugin_name The plugin name without the lib prefix or .so suffix.
 * @return The dlopen handle, or NULL when loading fails.
 */
static void *try_open_from_base_dir(const char *base_dir, const char *plugin_name) {
    char path[PATH_MAX * 2];
    void *handle;

    if (base_dir == NULL || base_dir[0] == '\0') {
        return NULL;
    }

    snprintf(path, sizeof(path), "%s/lib%s.so", base_dir, plugin_name);
    handle = try_open_path(path);
    if (handle != NULL) {
        return handle;
    }

    snprintf(path, sizeof(path), "%s/%s/lib%s.so", base_dir, plugin_name, plugin_name);
    return try_open_path(path);
}

void *plugin_loader_open(const char *plugin_name) {
    char path[PATH_MAX * 2];
    char exe_dir[PATH_MAX];
    const char *plugin_base_dir;
    const char *legacy_base_dir;
    void *handle;

    if (plugin_name == NULL || plugin_name[0] == '\0') {
        return NULL;
    }

    plugin_base_dir = getenv("PLUGIN_LIB_DIR");
    handle = try_open_from_base_dir(plugin_base_dir, plugin_name);
    if (handle != NULL) {
        return handle;
    }

    legacy_base_dir = getenv("DEVICE_LIB_DIR");
    handle = try_open_from_base_dir(legacy_base_dir, plugin_name);
    if (handle != NULL) {
        return handle;
    }

    if (get_executable_dir(exe_dir, sizeof(exe_dir)) == 0) {
        snprintf(path, sizeof(path), "%s/lib%s.so", exe_dir, plugin_name);
        handle = try_open_path(path);
        if (handle != NULL) {
            return handle;
        }

        snprintf(path, sizeof(path), "%s/device/%s/lib%s.so", exe_dir, plugin_name, plugin_name);
        handle = try_open_path(path);
        if (handle != NULL) {
            return handle;
        }
    }

    snprintf(path, sizeof(path), "./lib%s.so", plugin_name);
    handle = try_open_path(path);
    if (handle != NULL) {
        return handle;
    }

    snprintf(path, sizeof(path), "./device/%s/lib%s.so", plugin_name, plugin_name);
    handle = try_open_path(path);
    if (handle != NULL) {
        return handle;
    }

    snprintf(path, sizeof(path), "lib%s.so", plugin_name);
    handle = try_open_path(path);
    if (handle != NULL) {
        return handle;
    }

    fprintf(stderr,
            "plugin load failed: %s (set PLUGIN_LIB_DIR or place lib%s.so next to server)\n",
            plugin_name,
            plugin_name);
    return NULL;
}

int plugin_loader_symbol(
    void *handle,
    const char *symbol_name,
    void **symbol
) {
    char *error;

    if (handle == NULL || symbol_name == NULL || symbol == NULL) {
        return DEVICE_RESULT_SYMBOL_LOAD_FAILED;
    }

    dlerror();
    *symbol = dlsym(handle, symbol_name);
    error = dlerror();
    if (error != NULL) {
        fprintf(stderr, "dlsym failed: %s: %s\n", symbol_name, error);
        return DEVICE_RESULT_SYMBOL_LOAD_FAILED;
    }

    return DEVICE_RESULT_OK;
}

void plugin_loader_close(void **handle) {
    if (handle != NULL && *handle != NULL) {
        dlclose(*handle);
        *handle = NULL;
    }
}
