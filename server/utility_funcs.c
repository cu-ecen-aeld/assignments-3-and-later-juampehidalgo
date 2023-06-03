#include "utility.h"
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <stdbool.h>
#include <string.h>


#define TMP_BUF_SIZE 1024
#define CHUNK_SIZE 512

void* thread_run_function(void* args) {
    struct thread_information* thread_info = args;
    syslog(LOG_INFO, "Thread with ID: %ld spawned to handle incoming connection", pthread_self());
    char* buffer;
    size_t buffer_size;

    while (true) {
        buffer = NULL;
        buffer_size = 0;
        int ret_val = read_str_from_socket(thread_info->socketd, &buffer, &buffer_size);
        if (ret_val) {
            /* something wen't wrong while reading from the socket but the memory allocated is already freed */
            /* so we can simply end thread execution here */
            if (buffer != NULL) {
                free(buffer);
                buffer = NULL;
            }
            thread_info->thread_return_value = EXIT_FAILURE;
            break;
        }
        /* so now that we got all the string into the buffer, dump it to the file, after getting hold of the mutex */
        ret_val = pthread_mutex_lock(thread_info->mutex_ptr);
        if (ret_val) {
            syslog(LOG_ERR, "Something bad happened when locking the mutex within thread ID %ld, error %s", pthread_self(), strerror(errno));
            if (buffer != NULL) {
                free(buffer);
                buffer = NULL;
            }
            thread_info->thread_return_value = EXIT_FAILURE;
            break;
        }
        ret_val = dump_buffer_to_file(buffer, buffer_size, thread_info->filed);
        if (ret_val) {
            if (buffer != NULL) {
                free(buffer);
                buffer = NULL;
            }
            thread_info->thread_return_value = EXIT_FAILURE;
            break;
        }
        /* let's flush and make sure contents of file are there before releasing lock */
        if (fsync(thread_info->filed) < 0) {
            syslog(LOG_ERR, "Failed to sync outout file from thread ID %ld, error: %s", pthread_self(), strerror(errno));
            if (buffer != NULL) {
                free(buffer);
                buffer = NULL;
            }
            thread_info->thread_return_value = EXIT_FAILURE;
            break;
        }
        /* release mutex, we're done writing to the file from this thread */
        ret_val = pthread_mutex_unlock(thread_info->mutex_ptr);
        if (ret_val) {
            syslog(LOG_ERR, "Something bad happening while unlocking the mutex from thread ID %ld, error: %s", pthread_self(), strerror(errno));
            if (buffer != NULL) {
                free(buffer);
                buffer = NULL;
            }
            thread_info->thread_return_value = EXIT_FAILURE;
            break;
        }

        if (buffer != NULL) {
            free(buffer);
            buffer = NULL;
        }
        /* now dump complete file contents to remote party */
        ret_val = dump_file_to_socket(thread_info->filed, thread_info->socketd);
        if (ret_val) {
            thread_info->thread_return_value = EXIT_FAILURE;
            break;
        }

    }


    /* thread_info->thread_return_value = EXIT_SUCCESS; */
    /* pthread_exit(&thread_info->thread_return_value); */ /* No more use of pthread_exit since the Yocto image is missing one library and the process will crash when calling this */
    return NULL;
}

int read_str_from_socket(int socketd, char** buf_ptr, size_t* buf_size) {
    *buf_ptr = NULL;
    char* tmp_ptr = NULL;
    size_t chunk_size = CHUNK_SIZE;
    size_t allocated_space = 0;
    size_t total_read = 0;

    do {
        /* allocate memory / resize current allocation (if needed) */
        if ((allocated_space - total_read) < (chunk_size >> 2)) {
            tmp_ptr = realloc(*buf_ptr, allocated_space + chunk_size);
            if (tmp_ptr == NULL) {
                syslog(LOG_ERR, "Failed to allocate/resize read buffer, error: %s", strerror(errno));
                if (*buf_ptr != NULL) {
                    free(*buf_ptr);
                    *buf_ptr = NULL;
                }
                return errno;
            }
            *buf_ptr = tmp_ptr;
            allocated_space += chunk_size;
        }
        
        /* now that we made sure that we have enough memory, read up to chunk size into the buffer */
        int read_bytes = read(socketd, *buf_ptr + total_read, allocated_space - total_read);
        if (read_bytes < 0) {
            syslog(LOG_ERR, "Error while reading from the socket, error: %s", strerror(errno));
            if (*buf_ptr != NULL) {
                free(*buf_ptr);
                *buf_ptr = NULL;
            }
            return errno;
        }
        else if (read_bytes == 0) {
            syslog(LOG_NOTICE, "Looks like remote end close the connection, error: %s", strerror(errno));
            if (*buf_ptr != NULL) {
                free(*buf_ptr);
                *buf_ptr = NULL;
            }
            return -1;
        }
        total_read += read_bytes;
    } while ((*buf_ptr)[total_read - 1] != '\n');
    
    /* here we pass back the size of the effetive data, instead of the allocated size... does not really matter */
    /* but like this we're not restricted to string data delimited with '\n' */
    *buf_size = total_read;
    return 0;
}

int dump_buffer_to_file(char* buf_ptr, size_t buf_size, int filed) {
    size_t bytes_left_to_write = buf_size;
    size_t bytes_wrote_overall = 0;
    size_t bytes_wrote = 0;

    /* let's move pointer to the very end of the file */
    if (lseek(filed, 0, SEEK_END) < 0) {
        syslog(LOG_ERR, "Could not move file pointer to the end of the file, error: %s", strerror(errno));
        return errno;
    }

    while ((bytes_wrote = write(filed, buf_ptr + bytes_wrote_overall, bytes_left_to_write)) < bytes_left_to_write) {
        if (bytes_wrote <= 0) {
            syslog(LOG_ERR, "Failure to write to output file, error: %s", strerror(errno));
            return errno;
        }

        bytes_left_to_write -= bytes_wrote;
        bytes_wrote_overall += bytes_wrote;
    }

    return 0;
}

int dump_file_to_socket(int filed, int socketd) {
    /* make a backup of the current file pointer */
    off_t current_file_offset = lseek(filed, 0, SEEK_CUR);
    if (current_file_offset < 0) {
        syslog(LOG_ERR, "Could not retrieve the current file offset, error: %s", strerror(errno));
        return errno;
    }
    /* move it to the beginning of the file, since we dump full contents */
    if (lseek(filed, 0, SEEK_SET) < 0) {
        syslog(LOG_ERR, "Failed to move file pointer to the beginning of the file, error: %s", strerror(errno));
        return errno;
    }
    syslog(LOG_NOTICE, "Moved file pointer to beginning of file");
    /* now start reading the file and sending to socket */
    char buf[TMP_BUF_SIZE] = {0};
    int bytes_read = 0;
    while ((bytes_read = read(filed, buf, TMP_BUF_SIZE)) > 0) {
        size_t current_chunk_size = bytes_read;
        size_t write_ptr = 0;
        while (current_chunk_size > 0) {
            int bytes_wrote = write(socketd, buf + write_ptr, current_chunk_size);
            if (bytes_wrote < 0) {
                syslog(LOG_ERR, "Failed to write a chunk of information to socket, error: %s", strerror(errno));
                /* restore original file pointer, if possible */
                if (lseek(filed, current_file_offset, SEEK_SET) < 0) {
                    syslog(LOG_WARNING, "Could not restore the output file pointer to its original value, error: %s", strerror(errno));
                }
                return errno;
            }
            write_ptr += bytes_wrote;
            current_chunk_size -= bytes_wrote;
        }
    }
    if (bytes_read < 0) {
        syslog(LOG_ERR, "Error reading from the source file, error: %s", strerror(errno));
        return errno;
    }
    /* restore original file pointer, if possible */
    if (lseek(filed, current_file_offset, SEEK_SET) < 0) {
        syslog(LOG_WARNING, "Could not restore the output file pointer to its original value, error: %s", strerror(errno));
    }

    return 0;
}
