INC_DIR	=  ../../../include

PROG = proctest filetest
LIB = libtest2.a
CFLAGS = -g -I.
HEADER = libc.h u.h

CFLAGS        += -nostdinc -fno-builtin \
        $(addprefix -I, $(INC_DIR)) $(addprefix -D, $(DEFINES))


all:	filetest proctest $(LIB)

filetest:	filetest.c $(LIB) $(HEADER)
	$(CC) $(CFLAGS) -o $@ $@.c $(LIB)

proctest:	proctest.c $(LIB)
	$(CC) $(CFLAGS) -o $@ $@.c $(LIB)


getopt.o:	getopt.c
	$(CC) $(CFLAGS) -c $<

str.o:		str.c
	$(CC) $(CFLAGS) -c $<

libtest2.a:	getopt.o str.o
	rm -f $@
	ar r $@ getopt.o str.o

clean:
	rm -f *.o *.a $(PROG)
