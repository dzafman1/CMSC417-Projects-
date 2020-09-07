CC=gcc
LDLIBS=-lcrypto
CFLAGS=-Wall -Iincludes -Wextra -std=c99 -ggdb
VPATH=.

all: server client

server: server.o hash.o

client: client.o hash.o

clean:
	rm -rf *.o server client