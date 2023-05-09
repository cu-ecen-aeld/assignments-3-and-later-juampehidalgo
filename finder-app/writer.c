#include <stdio.h>
#include <syslog.h>
#include <errno.h>

int main(int argc, char * argv[]) {

    openlog("writer", LOG_PERROR | LOG_CONS, LOG_USER);

    if (argc == 1) {
    	syslog(LOG_ERR, "writer was called without parameters");
    	printf("Usage: writer <filename> <content>\n");
    	return 1;
    }
    else if (argc == 2) {
        syslog(LOG_ERR, "Incorrect number of parameters passed in. Got <filenme> parameter but no content");
        printf("Usage: writer <filename> <content>\n");
        /* return EINVAL; */
        return 1;
    }
    else if (argc > 3) {
    	syslog(LOG_ERR, "Too many parameters were passed in, expected 2 but got %d parameters", argc - 1);
    	printf("Usage: writer <filename> <content>\n");
    	return 1;
    }
    
    FILE * fd = fopen(argv[1], "w");
    if (fd == NULL) {
    	syslog(LOG_ERR, "%d: Could not open user selected file: %s", errno, argv[1]);
    	return errno;
    }
    
    fprintf(fd, "%s\n", argv[2]);

    printf("Number of passed arguments was %d\n", argc);

    fclose(fd);

    return 0;
}

