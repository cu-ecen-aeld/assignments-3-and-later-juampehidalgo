#include "systemcalls.h"

#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>


/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
    int  system_ret = system(cmd);
    if (system_ret == -1 || system_ret == 127 || (cmd == NULL && system_ret == 0)) {
    	syslog(LOG_ERR, "system() call failed with error code %d when cmd was %s", system_ret, cmd == NULL ? "NULL" : cmd);
    	return false;
    }
    return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];

    va_end(args);

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
    
    pid_t pid = fork();
    
    if (pid == 0) {
        int execv_ret = execv(command[0], command);
        if (execv_ret == -1) {
            syslog(LOG_ERR, "execv() call failed in the child process with return code %d and errno %d", execv_ret, errno);
            exit(errno);
        }
    }
    else if (pid == -1) {
        syslog(LOG_ERR, "fork() call failed with error code %d and errno was %d", pid, errno);
        return false;	// fork() failed to create child process
    }
    
    
    if (pid > 0) {
        int status = 0;
        pid_t wait_ret = wait(&status);
        if (wait_ret == -1) {
            syslog(LOG_ERR, "wait() returned %d and errno was set to %d", wait_ret, errno);
            return false;
        }
        if (pid == wait_ret) {
            if (WIFEXITED(status)) {
                syslog(WEXITSTATUS(status) == 0 ? LOG_INFO : LOG_ERR, "Process terminated normally with status code %d", WEXITSTATUS(status));
                return WEXITSTATUS(status) == 0;
            }
            if (WIFSIGNALED(status)) {
                syslog(LOG_ERR, "Process was killed by signal %d %s", WTERMSIG(status), WCOREDUMP(status) ? " (dumped code)" : "");
                return false;
            }
            if (WIFSTOPPED(status)) {
                syslog(LOG_ERR, "Process was stopped by signal %d", WSTOPSIG(status));
                return false;
            }
            if (WIFCONTINUED(status)) {
                syslog(LOG_ERR, "Process continued");
                return false;
            }

        }
    }

    return false;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/

    va_end(args);
    
    int fd = 0;
    if (outputfile != NULL) {
        fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
        if (fd < 0) {
            syslog(LOG_ERR, "Could not open redirection file %s because of error %d", outputfile, errno);
            return false;
        }
    }
    
    pid_t pid = fork();
    if (pid == 0) {
        /* we need to redirect here is outputfile is not NULL */
        if (fd) {
            int dup2_ret = dup2(fd, 1);
            if (dup2_ret < 0) {
                syslog(LOG_ERR, "Redirection requested but dup2() failed to execute with code %d and errno %d", dup2_ret, errno);
                return false;
            }
            close(fd);
        }
        int execv_ret = execv(command[0], command);
        if (execv_ret == -1) {
            syslog(LOG_ERR, "execv() call failed in the child process with return code %d and errno %d", execv_ret, errno);
            exit(errno);
        }
    }
    else if (pid == -1) {
        syslog(LOG_ERR, "fork() call failed with error code %d and errno was %d", pid, errno);
        return false;	// fork() failed to create child process
    }
    
    if (fd) {
        close(fd);
    }

    if (pid > 0) {
        int status = 0;
        pid_t wait_ret = wait(&status);
        if (wait_ret == -1) {
            syslog(LOG_ERR, "wait() returned %d and errno was set to %d", wait_ret, errno);
            return false;
        }
        if (pid == wait_ret) {
            if (WIFEXITED(status)) {
                syslog(WEXITSTATUS(status) == 0 ? LOG_INFO : LOG_ERR, "Process terminated normally with status code %d", WEXITSTATUS(status));
                return WEXITSTATUS(status) == 0;
            }
            if (WIFSIGNALED(status)) {
                syslog(LOG_ERR, "Process was killed by signal %d %s", WTERMSIG(status), WCOREDUMP(status) ? " (dumped code)" : "");
                return false;
            }
            if (WIFSTOPPED(status)) {
                syslog(LOG_ERR, "Process was stopped by signal %d", WSTOPSIG(status));
                return false;
            }
            if (WIFCONTINUED(status)) {
                syslog(LOG_ERR, "Process continued");
                return false;
            }

        }
    }

    return false;
}
