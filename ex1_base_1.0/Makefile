# Makefile, versao 1
# Sistemas Operativos, DEI/IST/ULisboa 2019-20

CC   = gcc
LD   = gcc
CFLAGS =-Wall -pthread -g -std=gnu99 -I../
LDFLAGS=-lm 

# A phony target is one that is not really the name of a file
# https://www.gnu.org/software/make/manual/html_node/Phony-Targets.html
.PHONY: all clean run

all: tecnicofs-nosync tecnicofs-mutex tecnicofs-rwlock

tecnicofs-nosync: lib/inodes.o lib/bst.o lib/hash.o fs.o main.o
	$(LD) $(CFLAGS) $(LDFLAGS) -o tecnicofs-nosync lib/inodes.o lib/bst.o lib/hash.o fs.o main.o

tecnicofs-mutex: lib/inodes.o lib/bst.o lib/hash.o fsMutex.o mainMutex.o
	$(LD) $(CFLAGS) $(LDFLAGS) -o tecnicofs-mutex lib/inodes.o lib/bst.o lib/hash.o fsMutex.o mainMutex.o

tecnicofs-rwlock: lib/inodes.o lib/bst.o lib/hash.o fsRWLock.o mainRWLock.o
	$(LD) $(CFLAGS) $(LDFLAGS) -o tecnicofs-rwlock lib/inodes.o lib/bst.o lib/hash.o fsRWLock.o mainRWLock.o	

lib/bst.o: lib/bst.c lib/bst.h
	$(CC) $(CFLAGS) -o lib/bst.o -c lib/bst.c

lib/hash.o: lib/hash.c lib/hash.h
	$(CC) $(CFLAGS) -o lib/hash.o -c lib/hash.c

lib/inodes.o: lib/inodes.c lib/inodes.h tecnicofs-api-constants.h
	$(CC) $(CFLAGS) -o lib/inodes.o -c lib/inodes.c

fs.o: fs.c fs.h lib/bst.h lib/inodes.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o fs.o -c fs.c

fsMutex.o: fs.c fs.h lib/bst.h lib/inodes.h
	$(CC) $(CFLAGS) $(LDFLAGS) -DMUTEX -o fsMutex.o -c fs.c

fsRWLock.o: fs.c fs.h lib/bst.h lib/inodes.h
	$(CC) $(CFLAGS) $(LDFLAGS) -DRWLOCK -o fsRWLock.o -c fs.c

main.o: main.c lib/inodes.h fs.h lib/bst.h lib/hash.h tecnicofs-api-constants.h
	$(CC) $(CFLAGS) -o main.o -c main.c

mainMutex.o: main.c lib/inodes.h fs.h lib/bst.h lib/hash.h tecnicofs-api-constants.h
	$(CC) $(CFLAGS) -DMUTEX -o mainMutex.o -c main.c	

mainRWLock.o: main.c lib/inodes.h fs.h lib/bst.h lib/hash.h tecnicofs-api-constants.h
	$(CC) $(CFLAGS) -DRWLOCK -o mainRWLock.o -c main.c

clean:
	@echo Cleaning...
	rm -f tecnicofs-nosync tecnicofs-mutex tecnicofs-rwlock lib/*.o *.o 

run-nosync: tecnicofs-nosync
	./tecnicofs-nosync inputs/test5.txt out.txt 1 1
run-mutex: tecnicofs-mutex
	./tecnicofs-mutex inputs/test5.txt out.txt 7 4
run-rwlock: tecnicofs-rwlock
	./tecnicofs-rwlock inputs/test5.txt out.txt 7 4
