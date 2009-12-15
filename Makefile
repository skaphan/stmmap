
IDIR =/usr/local/boost_1_41_0

CC=gcc
CPP=g++


# Some systems need SIGSEGV, others, like Mac OS, need SIGBUS
# To do:  figure out how to autoconfigure this.
#
CFLAGS = -DPAGE_ACCESS_SIGNAL=SIGSEGV
CPLUSPLUSFLAGS = $(CFLAGS)  -DTHREADS -I$(IDIR)

LIBS=-lpthread

OBJ = stm.o stmalloc.o atomic-compat.o

NOBJ = AVLtree.o segalloc.o example.o

THOBJ = segalloc.th.o AVLtree.th.o example.th.o 


all: stmtest1 stmtest2


# this is a single-threaded test program that uses absolute pointers
# in the shared segment.  Uses pthreads library for thread local storage
# which is the same in both single- and multi-threaded versions.
#
stmtest1: $(OBJ) $(NOBJ)
	$(CC) -o $@ $(OBJ) $(NOBJ) $(LIBS)

# this is a multi-threading test program that uses position-independent
# relative pointers in the shared segment.
#
stmtest2: $(OBJ) $(THOBJ)
	$(CPP) -o $@ $(OBJ) $(THOBJ) $(LIBS)
	
%.o: %.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

# The two following rules must appear in the order they appear here.
%.th.o: %.cpp Makefile 
	$(CPP) -c $(CPLUSPLUSFLAGS) $< -o $@

%.th.o: %.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: clean


clean:
	rm -f *.o *~ core
