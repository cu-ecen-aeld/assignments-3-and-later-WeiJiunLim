#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
// #define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    int mutex_status;
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    thread_func_args->thread_complete_success = true;

    DEBUG_LOG("Thread started, waiting to obtain lock in %u ms", thread_func_args->wait_to_obtain_ms);
    usleep(thread_func_args->wait_to_obtain_ms * 1000);

    mutex_status = pthread_mutex_lock(thread_func_args->mutex);
    if (mutex_status != 0) {
        ERROR_LOG("Mutex lock status: %u", mutex_status);
        thread_func_args->thread_complete_success = false;
    }

    DEBUG_LOG("Mutex locked, waiting to release in %u ms", thread_func_args->wait_to_release_ms);
    usleep(thread_func_args->wait_to_release_ms * 1000);

    mutex_status = pthread_mutex_unlock(thread_func_args->mutex);
    if (mutex_status != 0) {
        ERROR_LOG("Mutex unlock status: %u", mutex_status);
        thread_func_args->thread_complete_success = false;
    }

    DEBUG_LOG("Thread returning with status: %u", thread_func_args->thread_complete_success);

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */

    int thread_create_status;

    /* Allocate memory for thread parameters, and set up values */
    struct thread_data *thread_params = (struct thread_data*) malloc(sizeof(struct thread_data));

    if (!thread_params) {
        ERROR_LOG("Malloc failed");
        return (false);
    }

    thread_params->mutex = mutex;
    thread_params->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_params->wait_to_release_ms = wait_to_release_ms;
    thread_params->thread_complete_success = false;
    DEBUG_LOG("Thread params setup OK");

    /* Create thread */
    thread_create_status = pthread_create(thread, NULL, threadfunc, thread_params);
    DEBUG_LOG("Thread create status: %u", thread_create_status);

    return (((thread_create_status  == 0) ? true : false));
}

