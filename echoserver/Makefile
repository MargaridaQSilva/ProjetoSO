CC = gcc
CFLAGS = -O2 -Wall -I .

# This flag includes the Pthreads library on a Linux box.
# Others systems will probably require something different.
LIB = -lpthread

all: mainclient mainserver 

mainserver: mainserver.c csapp.o
	$(CC) $(CFLAGS) -o mainserver mainserver.c csapp.o $(LIB)


mainclient: mainclient.c csapp.o
	$(CC) $(CFLAGS) -o mainclient mainclient.c csapp.o $(LIB)


csapp.o:
	$(CC) $(CFLAGS) -c csapp.c


clean:
	rm -f *.o mainclient mainserver *~

