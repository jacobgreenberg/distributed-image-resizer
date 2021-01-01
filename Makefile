
CC=gcc
CFLAGS=-I includes/ -g -lpthread -lm

all: leader worker

leader: leader.c includes/socket_helper.h
		$(CC) -o leader leader.c $(CFLAGS)

worker: worker.c includes/socket_helper.h
		$(CC) -o worker worker.c $(CFLAGS)

clean:
		rm -f worker leader
