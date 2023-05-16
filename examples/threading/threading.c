#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    struct timespec wait_time;
    
    struct thread_data *ptr = (struct thread_data *)thread_param;
    
    /* wait for initial period */
    unsigned int number_of_seconds = ptr->initial_wait / 1000;
    unsigned int number_of_milliseconds = ptr->initial_wait - (number_of_seconds) * 1000;
    wait_time.tv_sec = number_of_seconds;
    wait_time.tv_nsec = number_of_milliseconds * 1000000;
    int rc = nanosleep(&wait_time, NULL);
    if (rc != 0) {
        printf("Could not wait for long enough at the beginning: %d\n", errno);
        ptr->thread_complete_success = false;
        return thread_param;
    }
    
    /* obtain mutex */
    rc = pthread_mutex_lock(ptr->mutex_var);
    if (rc != 0) {
        printf("Failed to lock mutex with code %d\n", rc);
        ptr->thread_complete_success = false;
        return thread_param;
    }
    
    /* wait again */
    number_of_seconds = ptr->final_wait / 1000;
    number_of_milliseconds = ptr->final_wait - (number_of_seconds) * 1000;
    wait_time.tv_sec = number_of_seconds;
    wait_time.tv_nsec = number_of_milliseconds * 1000000;
    rc = nanosleep(&wait_time, NULL);
    if (rc != 0) {
        printf("Could not wait for long enough at the end: %d\n", errno);
        ptr->thread_complete_success = false;
        return thread_param;
    }
    
    /* release mutex */
    rc = pthread_mutex_unlock(ptr->mutex_var);
    if (rc != 0) {
        printf("Failed to release mutex with code %d\n", rc);
        ptr->thread_complete_success = false;
        return thread_param;
    }
    
    ptr->thread_complete_success = true;
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
     
    /* get memory for the thread_data structure */
    struct thread_data* thread_data_prt = calloc(1, sizeof(struct thread_data));
    /* copy over wait parameters */
    thread_data_prt->initial_wait = wait_to_obtain_ms;
    thread_data_prt->final_wait = wait_to_release_ms;
    thread_data_prt->mutex_var = mutex;
    /* create thread */
    int rc = pthread_create(thread, NULL, threadfunc, thread_data_prt);
    if (rc != 0) {
        printf("Thread failed to be created with code %d\n", rc);
        //free(thread_data);
        return false;
    }
    else {
        return true;
    }
}

