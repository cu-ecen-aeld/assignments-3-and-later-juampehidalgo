#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>

/* global variables */
int server_socket_descriptor = 0;
int output_file_descriptor = 0;
int connection_socket_descriptor = 0;
char* output_file_path = "/var/tmp/aesdsocketdata";

/* helper methods */
void sync_and_close_output_file(int file_descriptor) {
    if (file_descriptor && fsync(file_descriptor) < 0) {
        syslog(LOG_WARNING, "Failed to flush output file, error %d", errno);
    }
    if (file_descriptor && close(file_descriptor) < 0) {
        syslog(LOG_WARNING, "Failed to close output file, error %d", errno);
    }
}

void close_socket(int sd) {
    if (sd && close(sd) < 0) {
        syslog(LOG_WARNING, "Failed to close incoming socket, error %d", errno);
    }
}

void terminate(int termination_reason) {
    sync_and_close_output_file(output_file_descriptor);
    close_socket(connection_socket_descriptor);
    close_socket(server_socket_descriptor);
    if (remove(output_file_path) < 0) {
        syslog(LOG_ERR, "Failed to remove the file at %s upon termination, error: %s", output_file_path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    exit(termination_reason);
}

/* Signal handler definitions */
static void termination_handler(int signal_number) {
    if (signal_number == SIGINT) {
        syslog(LOG_NOTICE, "Caught signal, exiting");
        terminate(EXIT_SUCCESS);    }
    else if (signal_number == SIGTERM) {
        syslog(LOG_NOTICE, "Caught signal, exiting");
        terminate(EXIT_SUCCESS);
    }
}

void setup_signal_handlers(void) {
    struct sigaction sigact;
    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = termination_handler;
    if (sigaction(SIGTERM, &sigact, NULL) != 0) {
        syslog(LOG_ERR, "Failure when trying to register a handler for the SIGTERM signal");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGINT, &sigact, NULL) != 0) {
        syslog(LOG_ERR, "Failure when trying to register a handler for the SIGINT signal");
        exit(EXIT_FAILURE);
    }
}


void dump_buffer(char* ptr, size_t size, int fd) {
    size_t bytes_to_write = size;
    size_t bytes_wrote_overall = 0;
    size_t bytes_wrote;
    while ((bytes_wrote = write(fd, ptr + bytes_wrote_overall, bytes_to_write)) < bytes_to_write) {
        if (bytes_wrote < 0) {
            syslog(LOG_ERR, "Failure to write to output file, error %d", errno);
            terminate(EXIT_FAILURE);
        }
        bytes_to_write -= bytes_wrote;
        bytes_wrote_overall += bytes_wrote;
    }
}

/* Reception function */
void write_incoming_string_to_file(int socketd, int filed) {
    char* ptr = NULL;
    char* tmp_ptr = NULL;
    size_t chunk_size = 512;
    size_t allocated_space = 0;
    size_t total_read = 0;
    
    do {
        /* allocate memory, if needed */
        if (allocated_space - total_read < chunk_size >> 2) {
            tmp_ptr = realloc(ptr, allocated_space + chunk_size);
            if (tmp_ptr == NULL) {
                /* allocation failed, clear previous buffer and exit */
                syslog(LOG_ERR, "Failed to allocate heap during execution, error %d", errno);
                if (ptr != NULL) {
                    free(ptr);
                }
                terminate(EXIT_FAILURE);
            }
            ptr = tmp_ptr;
            allocated_space += chunk_size;
        }

        
        /* now read up to chunk_size into the next buffer position */
        int read_bytes = read(socketd, ptr + total_read, allocated_space - total_read);
        if (read_bytes < 0) {
            syslog(LOG_ERR, "Error while reading from thewrite_incoming_string_to_file socket, error %d", errno);
            exit(EXIT_FAILURE);
        }
        /* update total_read */
        total_read += read_bytes;
        /* check if LF received */
        if (ptr[total_read-1] == '\n') {
            /* we got one full line read, return pointer to buffer */
            dump_buffer(ptr, total_read, filed);
            break;
        }
        
    } while (true);
    
    /* let's release the memory allocated for the reading from socket */
    free(ptr);
}

void write_output_file_to_socket(int fd, int conn_socket) {

    off_t current_file_offset = lseek(fd, 0, SEEK_CUR);
    if (current_file_offset < 0) {
        syslog(LOG_ERR, "Could not poll the current output file offset, error %d", errno);
        terminate(EXIT_FAILURE);
    }

    /* move file pointer to the beginning */
    if (lseek(fd, 0, SEEK_SET) < 0) {
        syslog(LOG_ERR, "Failed to move the output file pointer to the beginning of the file, error %d", errno);
        terminate(EXIT_FAILURE);
    }
    
    char buffer[1024] = {0};
    int bytes_read = 0;
    /* now start reading file chunk by chunk (e.g. 1024 bytes at a time) */
    while((bytes_read = read(fd, buffer, 1024)) > 0) {
        size_t current_chunk_size = bytes_read;
        size_t write_ptr = 0;
        
        while (current_chunk_size > 0) {
            int bytes_wrote = write(conn_socket, buffer + write_ptr, current_chunk_size);
            if (bytes_wrote < 0) {
                syslog(LOG_ERR, "Failed while writting to the client socket, error: %s", strerror(errno));
                terminate(EXIT_FAILURE);
            }
            write_ptr += bytes_wrote;
            current_chunk_size -= bytes_wrote;
        }

    }
    if (bytes_read < 0) {
        syslog(LOG_ERR, "Failed while reading from the output file, error: %s", strerror(errno));
        terminate(EXIT_FAILURE);
    }
    
    /* get the file offset back to where it was*/
    if (lseek(fd, current_file_offset, SEEK_SET) < 0) {
        syslog(LOG_ERR, "Could not restore the output file offset to its original value, error %d", errno);
        terminate(EXIT_FAILURE);
    }
}

void print_usage(void) {
    printf("Wrong options passed\n");
    printf("Usage:\n");
    printf("aesdsocket [-OPTION] [[value]]\n");
    printf("\t-d\t\t\tRun as daemon.\n");
    printf("\t-p <port number>\tSpecify port number.\n");
    printf("\t-f <file>\t\tOutput file.\n");
}

enum program_parameters {
    NONE,
    RUN_AS_DAEMON,
    PORT_NUMBER,
    OUTPUT_FILE
};

int main(int argc, char* argv[]) {

    bool running_as_daemon = false;
    int opt_val = 1;
    int server_port = 9000;


    openlog("aesdsocket", LOG_CONS | LOG_PERROR | LOG_PID, running_as_daemon ? LOG_DAEMON : LOG_USER);
    
    if (argc > 1) {
        int arg_idx = 1;
        bool reading_value = false;
        enum program_parameters last_parameter = NONE;
        while (arg_idx < argc) {
            if (!reading_value) {
                if (strcmp(argv[arg_idx], "-d") == 0) {
                    reading_value = false;
                    running_as_daemon = true;
                    last_parameter = RUN_AS_DAEMON;
                    arg_idx++;
                }
                else if (strcmp(argv[arg_idx], "-p") == 0) {
                    reading_value = true;
                    last_parameter = PORT_NUMBER;
                    arg_idx++;
                }
                else if (strcmp(argv[arg_idx], "-f") == 0) {
                    reading_value = true;
                    last_parameter = OUTPUT_FILE;
                    arg_idx++;
                }
                else {
                    print_usage();
                    exit(EXIT_FAILURE);
                }
            }
            else {
                switch (last_parameter) {
                    case PORT_NUMBER:
                        server_port = atoi(argv[arg_idx]);
                        reading_value = false;
                        last_parameter = NONE;
                        arg_idx++;
                        break;
                    case OUTPUT_FILE:
                        output_file_path = argv[arg_idx];
                        reading_value = false;
                        last_parameter = NONE;
                        arg_idx++;
                        break;
                    default:
                        print_usage();
                        exit(EXIT_FAILURE);
                        break;
                }
            }
        }
        for (int arg_idx = 1; arg_idx < argc; arg_idx++) {
            printf("Found argument %s at position %d\n", argv[arg_idx], arg_idx);
        }
    }
    
    setup_signal_handlers();
    
    syslog(LOG_NOTICE, "%s as a daemon, on port number %d, dumping to file %s", running_as_daemon ? "Running" : "Not running" , server_port, output_file_path);

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        syslog(LOG_ERR, "Failed to open server socket: %d", errno);
        terminate(EXIT_FAILURE);
    }
    
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt_val, sizeof(opt_val)) < 0) {
        syslog(LOG_ERR, "Failed to set socket options: %d", errno);
        terminate(EXIT_FAILURE);	
    }
    
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(server_port);
    unsigned int addr_length = sizeof(address);
        
    if (bind(socket_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        syslog(LOG_ERR, "Socket bind failed: %d", errno);
        terminate(EXIT_FAILURE);
    }
    
    /* Here is where we need to check if we should be running as a daemon */
    if (running_as_daemon) {
        pid_t sid = 0;
        pid_t process_id = fork();
        
        
        if (process_id < 0) {
            syslog(LOG_ERR, "Program was requested to run as daemon, but fork() call failed, error: %s", strerror(errno));
            terminate(EXIT_FAILURE);
        }
        else if (process_id > 0) {
            /* this code branch is for the parent process, where we need to exit */
            syslog(LOG_NOTICE, "Spun daemon process with PID %d", process_id);
            exit(EXIT_SUCCESS);
        }
        else {
            /*mask(0);*/
            /* close stdin, stdout & stderr */
            if (close(0) < 0) {
                syslog(LOG_ERR, "Failed to close stdin on daemon process, error: %s", strerror(errno));
                terminate(EXIT_FAILURE);
            }
            if (close(1) < 0) {
                syslog(LOG_ERR, "Failed to close stdout on daemon process, error: %s", strerror(errno));
                terminate(EXIT_FAILURE);
            }
            if (close(2) < 0) {
                syslog(LOG_ERR, "Failed to close stderr on daemon process, error: %s", strerror(errno));
                terminate(EXIT_FAILURE);
            }
            
            int null_fd = open("/dev/null", O_APPEND|O_RDWR);
            if (null_fd < 0) {
                syslog(LOG_ERR, "Could not open /dev/null to redirect std streams, error: %s", strerror(errno));
                terminate(EXIT_FAILURE);
            }
            if (dup2(null_fd, 0) < 0) {
                syslog(LOG_ERR, "Failed while trying to redirect stdin, error: %s", strerror(errno));
                terminate(EXIT_FAILURE);
            }
            if (dup2(null_fd, 1) < 0) {
                syslog(LOG_ERR, "Failed while trying to redirect stdout, error: %s", strerror(errno));
                terminate(EXIT_FAILURE);
            }
            if (dup2(null_fd, 2) < 0) {
                syslog(LOG_ERR, "Failed while trying to redirect stderr, error: %s", strerror(errno));
                terminate(EXIT_FAILURE);
            }
            
            /* set new session ID, no terminal, daemon will be the only process in this session */
            sid = setsid();
            if (sid < 0) {
                syslog(LOG_ERR, "Failed to set new session ID for the daemon, error: %s", strerror(errno));
                terminate(EXIT_FAILURE);
            }
            chdir("/");
        }
    }
    
    if (listen(socket_fd, 1) < 0) {
        syslog(LOG_ERR, "Socket listen failed: %d", errno);
        terminate(EXIT_FAILURE);
    }
    
    int conn_socket = 0;
    while ((conn_socket = accept(socket_fd, (struct sockaddr*)&address, (socklen_t*)&addr_length)) > 0) {
        syslog(LOG_NOTICE, "Accepted connection from %s", inet_ntoa(address.sin_addr));
        
        // open output file for write with create flag on
        int fd = open(output_file_path, O_RDWR | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
        if (fd < 0) {
            syslog(LOG_ERR, "Could not open/create output file at %s, error %d", output_file_path, errno);
            terminate(EXIT_FAILURE);
        }
        
        /* read incoming socket and dump string to file */
        write_incoming_string_to_file(conn_socket, fd);
        
        /* make sure buffers are flushed into file */
        if (fsync(fd) < 0) {
            syslog(LOG_ERR, "Failed to flush to output file before returning the information to client");
            terminate(EXIT_FAILURE);
        }
        
        /* now pump complete file contents to socket */
        write_output_file_to_socket(fd, conn_socket);


        // clean up by closing both output file and incoming socket (although there is really no need to open/close the file for each connection since we're syncing the file)
        sync_and_close_output_file(fd);
        close_socket(conn_socket);
        syslog(LOG_NOTICE, "Closed connection from %s", inet_ntoa(address.sin_addr));
    }
    
    if (close(socket_fd) < 0) {
        syslog(LOG_ERR, "Shutdown failed on server socket: %d", errno);
        terminate(EXIT_FAILURE);
    }
    
    terminate(EXIT_SUCCESS);
}

