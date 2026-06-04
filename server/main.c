#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>

#include "socket.h"
#include "app_config.h"
#include "device/device_manager.h"

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
 * Redirects stdin to /dev/null and stdout/stderr to the daemon log file.
 * @param work_dir The daemon working directory where the log directory is created.
 */
static void redirect_stdio_to_log(const char *work_dir) {
    char log_dir[PATH_MAX];
    char log_path[PATH_MAX];
    int log_fd;
    int null_fd;

    if (snprintf(log_dir, sizeof(log_dir), "%s/log", work_dir) >= (int)sizeof(log_dir)) {
        exit(EXIT_FAILURE);
    }

    if (mkdir(log_dir, 0755) == -1 && errno != EEXIST) {
        exit(EXIT_FAILURE);
    }

    if (snprintf(log_path, sizeof(log_path), "%s/server.log", log_dir) >= (int)sizeof(log_path)) {
        exit(EXIT_FAILURE);
    }

    null_fd = open("/dev/null", O_RDONLY);
    if (null_fd == -1) {
        exit(EXIT_FAILURE);
    }

    log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd == -1) {
        exit(EXIT_FAILURE);
    }

    if (dup2(null_fd, STDIN_FILENO) == -1 ||
        dup2(log_fd, STDOUT_FILENO) == -1 ||
        dup2(log_fd, STDERR_FILENO) == -1) {
        exit(EXIT_FAILURE);
    }

    if (null_fd > STDERR_FILENO) {
        close(null_fd);
    }
    if (log_fd > STDERR_FILENO) {
        close(log_fd);
    }

    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);
}

/**
 * Converts the server process into a daemon and moves it to the runtime directory.
 * @param work_dir The directory to use as the daemon working directory.
 */
static void create_daemon(const char *work_dir) {
    pid_t pid;
    long max_fd;

    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    else if (pid > 0) { // parent
        exit(EXIT_SUCCESS);
    }

    if (setsid() == -1) {
        exit(EXIT_FAILURE);
    }

    signal(SIGHUP, SIG_IGN);

    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    else if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(0);

    if (chdir(work_dir) < 0) {
        exit(EXIT_FAILURE);
    }

    // close fds
    max_fd = sysconf(_SC_OPEN_MAX);
    if (max_fd < 0) {
        max_fd = 1024;
    }

    for (int fd = (int)max_fd; fd >= 0; fd--) {
        close(fd);
    }

    redirect_stdio_to_log(work_dir);

    printf("daemon started\n");
}

/**
 * Starts the daemonized TCP device-control server.
 * @param argc The argument count.
 * @param argv The argument vector.
 * @return 0 when the server exits normally.
 */
int main(int argc, char *argv[]) {
    char work_dir[PATH_MAX];
    char config_path[PATH_MAX];
    int server_fd;
    int device_result;

    if (get_executable_dir(work_dir, sizeof(work_dir)) == -1) {
        fprintf(stderr, "failed to resolve executable directory\n");
        exit(EXIT_FAILURE);
    }

    if (snprintf(config_path, sizeof(config_path), "%s/config.ini", work_dir) >= (int)sizeof(config_path)) {
        fprintf(stderr, "config path too long\n");
        exit(EXIT_FAILURE);
    }

    load_app_config(config_path);

    create_daemon(work_dir);

    device_result = device_init();
    if (device_result != DEVICE_RESULT_OK) {
        fprintf(stderr, "device_init failed: %s\n", device_result_message(device_result));
        exit(EXIT_FAILURE);
    }

    server_fd = socket_init();

    init_clients();

    service(server_fd);

    device_cleanup();
    return 0;
}
