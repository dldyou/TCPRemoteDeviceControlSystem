#include "server_log.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

#include "app_config.h"

/**
 * Copies a string into a single-line buffer by stopping at CR or LF.
 * @param dest The destination buffer.
 * @param dest_size The size of the destination buffer.
 * @param src The source string to copy.
 */
static void copy_single_line(
    char *dest,
    size_t dest_size,
    const char *src
) {
    size_t i;

    if (dest == NULL || dest_size == 0) {
        return;
    }

    if (src == NULL) {
        dest[0] = '\0';
        return;
    }

    for (i = 0; i + 1 < dest_size && src[i] != '\0'; i++) {
        if (src[i] == '\r' || src[i] == '\n') {
            break;
        }
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

void log_client_command(
    int client_fd,
    const char *request,
    const char *response
) {
    char time_text[32];
    char request_text[BUFFER_SIZE];
    char response_text[BUFFER_SIZE];
    time_t now = time(NULL);
    struct tm local_time;
    struct sockaddr_in client_addr = { 0 };
    socklen_t client_addr_len = sizeof(client_addr);
    const char *client_ip = "unknown";
    int client_port = 0;

    localtime_r(&now, &local_time);
    strftime(time_text, sizeof(time_text), "%Y-%m-%d %H:%M:%S", &local_time);

    if (getpeername(client_fd, (struct sockaddr *)&client_addr, &client_addr_len) == 0) {
        client_ip = inet_ntoa(client_addr.sin_addr);
        client_port = ntohs(client_addr.sin_port);
    }

    copy_single_line(request_text, sizeof(request_text), request);
    copy_single_line(response_text, sizeof(response_text), response);

    printf("[%s] %s:%d \"%s\" -> \"%s\"\n",
        time_text,
        client_ip,
        client_port,
        request_text,
        response_text
    );
}
