CC = gcc
CFLAGS = -g -Wall -m32


all: myshell  mypipe

myshell: myshell.o LineParser.o
	$(CC) $(CFLAGS) -o myshell myshell.o LineParser.o

mypipe: mypipe.o
	$(CC) $(CFLAGS) -o mypipe mypipe.o

LineParser.o: LineParser.c LineParser.h
	$(CC) $(CFLAGS) -c LineParser.c

myshell.o: myshell.c LineParser.h
	$(CC) $(CFLAGS) -c myshell.c

mypipe.o: mypipe.c
	$(CC) $(CFLAGS) -c mypipe.c

.PHONY: clean all

clean:
	rm -f *.o  myshell mypipe