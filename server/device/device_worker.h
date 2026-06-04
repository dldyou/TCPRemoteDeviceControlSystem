#pragma once

#include <stdbool.h>
#include <pthread.h>

#include "device_result.h"

typedef int (*DeviceTaskRunFn)(void *context);
typedef void (*DeviceTaskCleanupFn)(void *context);

typedef struct DeviceTask DeviceTask;

typedef struct {
    const char *name;
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    bool running;
    bool started;
    DeviceTask *head;
    DeviceTask *tail;
} DeviceWorker;

/**
 * Starts a dedicated worker thread for serialized device operations.
 * @param worker The worker instance to start.
 * @param name A readable worker name.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int device_worker_start(DeviceWorker *worker, const char *name);

/**
 * Stops a device worker after its current task finishes.
 * @param worker The worker instance to stop.
 */
void device_worker_stop(DeviceWorker *worker);

/**
 * Queues a task on a device worker.
 * @param worker The worker that will execute the task.
 * @param run The task function to execute.
 * @param cleanup Optional cleanup callback for the task context.
 * @param context User context passed to the task.
 * @param wait true to wait for completion, false to queue asynchronously.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int device_worker_submit(
    DeviceWorker *worker,
    DeviceTaskRunFn run,
    DeviceTaskCleanupFn cleanup,
    void *context,
    bool wait
);

/**
 * Cancels all tasks that are still waiting in the worker queue.
 * @param worker The worker whose pending tasks should be cancelled.
 * @param result The result assigned to cancelled waiting tasks.
 */
void device_worker_cancel_pending(DeviceWorker *worker, int result);
