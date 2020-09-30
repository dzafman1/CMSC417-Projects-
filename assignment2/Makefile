CC=gcc
CFLAGS=-Wall -Wextra -ggdb -std=gnu99
BISON=bison
FLEX=flex
SRC=rt.c es.c ls.c n2h.c dr.c
OBJ=$(SRC:.c=.o) ru.tab.o lex.ru.o

all:		rt

.SUFFIXES: 	.c .o
.c.o:
		$(CC) -c $(CFLAGS) $*.c

ru.tab.c: ru.y ru.l
		$(BISON) --verbose -d -b ru -p ru ru.y

lex.ru.c: ru.l ru.y
		$(FLEX) -i -Pru $<

ru.tab.o: ru.tab.c
		$(CC)  $(CFLAGS) -c $<

lex.ru.o: lex.ru.c
		$(CC) $(CFLAGS) -c $<

rt: $(OBJ)
		$(CC) $(LIBS) -o rt $(OBJ)

clean:
		rm -f *.o ru.tab.* *.output rt lex.ru.c
