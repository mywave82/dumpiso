CC=gcc
CCLD=gcc
CFLAGS=-g -Wall
CCLDFLAGS=-g
RM=rm

all: dumpiso

clean:
	$(RM) -f *.o dumpiso

.PHONY: all clean

cdfs.o: cdfs.c \
	cdfs.h \
	iso9660.h
	$(CC) $(CFLAGS) $< -o $@ -c

iso9660.o: iso9660.c \
	amiga.c      \
	ElTorito.c   \
	rockridge.c  \
	susp.c       \
	cdfs.h       \
	iso9660.h    \
	main.h
	$(CC) $(CFLAGS) $< -o $@ -c

main.o: main.c \
	cdfs.h \
	iso9660.h \
	main.h \
	udf.h
	$(CC) $(CFLAGS) $< -o $@ -c

udf.o: udf.c \
	cdfs.h \
	main.h \
	udf.h
	$(CC) $(CFLAGS) $< -o $@ -c

dumpiso: cdfs.o iso9660.o main.o udf.o
	$(CCLD) $(CCLDFLAGS) $^ -o $@
