ifndef CROSS_COMPILE
	CC=gcc
else
	CC=$(CROSS_COMPILE)gcc
endif


INCLUDES:= -I ./include
LIBS:=
CFLAGS += -g -Wall -Werror
LDFLAGS += -L./libs

all:	aesdsocket
default:aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(INCLUDES) $(CFLAGS) -c aesdsocket.c -o aesdsocket.o
	$(CC) $(LIBS) $(LDFLAGS) aesdsocket.o -o aesdsocket

clean:
	rm -rf *.o aesdsocket
