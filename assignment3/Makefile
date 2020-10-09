CC=gcc
LDLIBS=-lcrypto
CFLAGS=-Wall -Iincludes -Wextra -std=c99 -ggdb
VPATH=.

all: rclient

rclient: rclient.o

clean:
	rm -rf *.o rclient