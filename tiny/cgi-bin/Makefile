CC = gcc

#Release
CFLAGS = -O2 -Wall -I ..
#Debug
#CFLAGS = -g -Wall -I ..

OBJS = sha1.o

all: adder proofofwork

adder: adder.c
	$(CC) $(CFLAGS) -o adder adder.c

proofofwork: proofofwork.c $(OBJS)
	$(CC) $(CFLAGS) -o proofofwork proofofwork.c sha1.o
clean:
	rm -f adder proofofwork *~ *.o
