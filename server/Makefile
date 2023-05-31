ifndef CROSS_COMPILE
	CC=gcc
else
	CC=$(CROSS_COMPILE)gcc
endif


INCLUDES:= -I ./include
LIBS:= -L./libs 
CFLAGS += -g -Wall -Werror
LDFLAGS += -lrt -pthread

all:	aesdsocket
default:aesdsocket

aesdsocket: aesdsocket.c utility_funcs.c ./include/utility.h
	$(CC) $(INCLUDES) $(CFLAGS) -c aesdsocket.c -o aesdsocket.o
	$(CC) $(INCLUDES) $(CFLAGS) -c utility_funcs.c -o utility_funcs.o
	$(CC) $(LIBS) utility_funcs.o aesdsocket.o -o aesdsocket $(LDFLAGS) 

.PHONY: clean
clean:
	rm -rf *.o aesdsocket
