
IDIR =/usr/local/boost_1_41_0

CC=gcc
CPP=g++
CPPFLAGS = -I$(IDIR)

# Some systems need SIGSEGV, others, like Mac OS, need SIGBUS
# To do:  figure out how to autoconfigure this.
#
CFLAGS = -DPAGE_ACCESS_SIGNAL=SIGSEGV

LIBS=-lpthread

OBJ = stm.o stmalloc.o atomic-compat.o AVLtree.o segalloc.o example.o

CPPSRC = AVLtree.cpp segalloc.cpp

CSRC = AVLtree.c segalloc.c

SRC = stm.c stmalloc.c atomic-compat.c example.c

all: stmmap1 stmmap2


# this is a single-threaded test program that uses absolute pointers
# in the shared segment
#
stmmap1: c-objs example1 $(OBJ)
	$(CC) -o $@ $(OBJ) $(LIBS)

# this is a multi-threading test program that uses position-independent
# relative pointers in the shared segment
#
stmmap2: cpp-objs example2 $(OBJ)
	$(CPP) -o $@ $(OBJ) $(LIBS)

.PHONY : c-objs cpp-objs clean-objs 

cpp-objs: clean-cppobjs example2
	$(CPP) -c $(CPPSRC) $(CPPFLAGS)

c-objs: clean-objs example1
	$(CC) -c $(CSRC)  $(CFLAGS)


example1:
	$(CC) -c example.c  $(CFLAGS)

example2:
	$(CC) -c example.c  $(CFLAGS) -DTHREADS


clean-objs:
	-rm segalloc.o AVLtree.o example.o

clean-cppobjs:
	-rm segalloc.o AVLtree.o example.o

.PHONY: clean


clean:
	rm -f *.o *~ core
