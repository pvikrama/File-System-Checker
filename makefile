CC=gcc

CFLAGS= -Wall -lm -o 

all:myfsck

myfsck: readwrite.c genhd.h ext2_fs.h
	$(CC) $(CFLAGS) myfsck readwrite.c

clean:
	rm -rf myfsck
