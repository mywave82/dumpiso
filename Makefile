CC=gcc
CCLD=gcc
CFLAGS=-g -Wall
CCLDFLAGS=-g
RM=rm

all: dumpiso dump_subchannel_rw

clean:
	$(RM) -f *.o dumpiso dump_subchannel_rw

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
	toc.h \
	udf.h
	$(CC) $(CFLAGS) $< -o $@ -c

toc.o: toc.c \
	toc.h \
	cdfs.h
	$(CC) $(CFLAGS) $< -o $@ -c

udf.o: udf.c \
	cdfs.h \
	main.h \
	udf.h
	$(CC) $(CFLAGS) $< -o $@ -c

dumpiso: cdfs.o iso9660.o main.o udf.o toc.o
	$(CCLD) $(CCLDFLAGS) $^ -o $@

dump_subchannel_rw.o: dump_subchannel_rw.c
	$(CC) $(CCFLAGS) $^ -o $@ -c

dump_subchannel_rw: dump_subchannel_rw.o
	$(CCLD) $(CCLDFLAGS) $^ -o $@
