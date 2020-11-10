CC=gcc -lm
CFLAGS=-Wall -Iincludes -Wextra -std=gnu99 -ggdb
LDLIBS=-lprotobuf-c -lcrypto
VPATH=.

all: chord rpc hashtest

chord: chord.c chord.pb-c.c hash.o

chord.pb-c.c: chord.proto
	protoc-c --c_out=. $<

rpc: rpc.c rpc.pb-c.c

rpc.pb-c.c: rpc.proto
	protoc-c --c_out=. $<

hashtest: hash.o hashtest.o

clean:
	rm -rf *~ *.o rpc rpc.pb-c.c rpc.pb-c.h hashtest chord chord.pb-c.c chord.pb-c.h

.PHONY : clean all
