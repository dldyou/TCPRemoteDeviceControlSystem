#include "device_worker.h"

#include <stdlib.h>
#include <string.h>

struct DeviceTask {
    DeviceTaskRunFn run;
    DeviceTaskCleanupFn cleanup;
    void *context;
    bool wait;
    bool done;
    int result;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    struct DeviceTask *next;
};

/**
 * Destroys a queued or completed worker task.
 * @param task The task to destroy.
 */
static void destroy_task(DeviceTask *task) {
    if (task == NULL) {
        return;
    }

    pthread_cond_destroy(&task->cond);
    pthread_mutex_destroy(&task->lock);
    free(task);
}

/**
 * Creates a worker task and initializes its synchronization state.
 * @param run The task function to execute on the worker thread.
 * @param cleanup Optional cleanup callback for the task context.
 * @param context User context passed to the task function.
 * @param wait Whether the submitter waits for task completion.
 * @return A newly allocated task, or NULL on failure.
 */
static DeviceTask *create_task(
    DeviceTaskRunFn run,
    DeviceTaskCleanupFn cleanup,
    void *context,
    bool wait
) {
    DeviceTask *task;

    if (run == NULL) {
        if (cleanup != NULL) {
            cleanup(context);
        }
        return NULL;
    }

    task = calloc(1, sizeof(*task));
    if (task == NULL) {
        if (cleanup != NULL) {
            cleanup(context);
        }
        return NULL;
    }

    task->run = run;
    task->cleanup = cleanup;
    task->context = context;
    task->wait = wait;
    task->result = DEVICE_RESULT_OK;

    if (pthread_mutex_init(&task->lock, NULL) != 0) {
        free(task);
        if (cleanup != NULL) {
            cleanup(context);
        }
        return NULL;
    }

    if (pthread_cond_init(&task->cond, NULL) != 0) {
        pthread_mutex_destroy(&task->lock);
        free(task);
        if (cleanup != NULL) {
            cleanup(context);
        }
        return NULL;
    }

    return task;
}

/**
 * Runs queued device tasks on a dedicated worker thread.
 * @param arg The DeviceWorker instance.
 * @return NULL when the worker exits.
 */
static void *device_worker_loop(void *arg) {
    DeviceWorker *worker = arg;

    while (1) {
        DeviceTask *task;
        int result;

        pthread_mutex_lock(&worker->lock);
        while (worker->running && worker->head == NULL) {
            pthread_cond_wait(&worker->cond, &worker->lock);
        }

        if (!worker->running && worker->head == NULL) {
            pthread_mutex_unlock(&worker->lock);
            break;
        }

        task = worker->head;
        worker->head = task->next;
        if (worker->head == NULL) {
            worker->tail = NULL;
        }
        pthread_mutex_unlock(&worker->lock);

        result = task->run(task->context);

        if (task->wait) {
            pthread_mutex_lock(&task->lock);
            task->result = result;
            task->done = true;
            pthread_cond_signal(&task->cond);
            pthread_mutex_unlock(&task->lock);
        }
        else {
            if (task->cleanup != NULL) {
                task->cleanup(task->context);
            }
            destroy_task(task);
        }
    }

    return NULL;
}

/**
 * Cancels or completes a pending task with a supplied result code.
 * @param task The task to cancel.
 * @param result The result code assigned to waiting submitters.
 */
static void cancel_task(DeviceTask *task, int result) {
    if (task->wait) {
        pthread_mutex_lock(&task->lock);
        task->result = result;
        task->done = true;
        pthread_cond_signal(&task->cond);
        pthread_mutex_unlock(&task->lock);
        return;
    }

    if (task->cleanup != NULL) {
        task->cleanup(task->context);
    }
    destroy_task(task);
}

int device_worker_start(DeviceWorker *worker, const char *name) {
    memset(worker, 0, sizeof(*worker));
    worker->name = name;
    worker->running = true;

    if (pthread_mutex_init(&worker->lock, NULL) != 0) {
        return DEVICE_RESULT_THREAD_FAILED;
    }

    if (pthread_cond_init(&worker->cond, NULL) != 0) {
        pthread_mutex_destroy(&worker->lock);
        return DEVICE_RESULT_THREAD_FAILED;
    }

    if (pthread_create(&worker->thread, NULL, device_worker_loop, worker) != 0) {
        pthread_cond_destroy(&worker->cond);
        pthread_mutex_destroy(&worker->lock);
        return DEVICE_RESULT_THREAD_FAILED;
    }

    worker->started = true;
    return DEVICE_RESULT_OK;
}

void device_worker_stop(DeviceWorker *worker) {
    if (!worker->started) {
        return;
    }

    pthread_mutex_lock(&worker->lock);
    worker->running = false;
    pthread_cond_broadcast(&worker->cond);
    pthread_mutex_unlock(&worker->lock);

    pthread_join(worker->thread, NULL);

    pthread_cond_destroy(&worker->cond);
    pthread_mutex_destroy(&worker->lock);
    worker->started = false;
}

int device_worker_submit(
    DeviceWorker *worker,
    DeviceTaskRunFn run,
    DeviceTaskCleanupFn cleanup,
    void *context,
    bool wait
) {
    DeviceTask *task;
    int result = DEVICE_RESULT_OK;

    if (!worker->started) {
        if (cleanup != NULL) {
            cleanup(context);
        }
        return DEVICE_RESULT_THREAD_FAILED;
    }

    task = create_task(run, cleanup, context, wait);
    if (task == NULL) {
        return DEVICE_RESULT_DEVICE_FAILED;
    }

    pthread_mutex_lock(&worker->lock);
    if (!worker->running) {
        pthread_mutex_unlock(&worker->lock);
        if (cleanup != NULL) {
            cleanup(context);
        }
        destroy_task(task);
        return DEVICE_RESULT_THREAD_FAILED;
    }

    if (worker->tail == NULL) {
        worker->head = task;
        worker->tail = task;
    }
    else {
        worker->tail->next = task;
        worker->tail = task;
    }
    pthread_cond_signal(&worker->cond);
    pthread_mutex_unlock(&worker->lock);

    if (!wait) {
        return DEVICE_RESULT_OK;
    }

    pthread_mutex_lock(&task->lock);
    while (!task->done) {
        pthread_cond_wait(&task->cond, &task->lock);
    }
    result = task->result;
    pthread_mutex_unlock(&task->lock);

    if (task->cleanup != NULL) {
        task->cleanup(task->context);
    }
    destroy_task(task);

    return result;
}

void device_worker_cancel_pending(DeviceWorker *worker, int result) {
    DeviceTask *task;

    if (!worker->started) {
        return;
    }

    pthread_mutex_lock(&worker->lock);
    task = worker->head;
    worker->head = NULL;
    worker->tail = NULL;
    pthread_mutex_unlock(&worker->lock);

    while (task != NULL) {
        DeviceTask *next = task->next;
        task->next = NULL;
        cancel_task(task, result);
        task = next;
    }
}
