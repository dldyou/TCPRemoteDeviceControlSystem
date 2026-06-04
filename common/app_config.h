#pragma once

#include <stdio.h>
#include <stdbool.h>

// buffer & limits
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 128
#define MAX_USERNAME_LEN 32
#define MAX_ADDR_LEN 64

// debug
#define DEBUG

#ifdef DEBUG
#define DEBUG_LOG(fmt, ...) printf("[%s:%s:%d] " fmt "\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__)
#else
#define DEBUG_LOG(fmt, ...) ((void)0)
#endif

typedef struct {
    char addr[MAX_ADDR_LEN];
    int port;
    int backlog;
} app_config;

extern app_config app;

/**
 * Loads server connection settings from an INI configuration file.
 * @param ini_file_path The path to the configuration file.
 * @return true if the file was loaded, false otherwise.
 */
bool load_app_config(const char *ini_file_path);
