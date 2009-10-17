/*
 
 stm.h
 
 Copyright 2009 Shel Kaphan
 
 This file is part of stmmap.
 
 stmmap is free software: you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 stmmap is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public License
 along with stmmap.  If not, see <http://www.gnu.org/licenses/>.
 
 */



#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>       // for PROT_NONE, PROT_READ, PROT_WRITE
#include <sys/stat.h>       // for ino_t
#include <setjmp.h>
#include <time.h>



typedef uint32_t transaction_id_t;

extern jmp_buf stm_jmp_buf;

/*
 Call stm_init() to initialize the STM package.  It sets up signal handlers and verbosity level for 
 subsequent activity.  On Mac OS you get SIGBUS
 signals on page access faults.  You might need to change it to handle SIGSEGV instead on some systems.
 
 Args:
 verbose    Controls whether to print out errors on stderr. This is a bit mask
            Bit     Meaning
            1       report errors
            2       report conflicts that cause transactions to abort
            4       report pages modified during each transaction commit or abort
 
 Return value:
  0         success
 -1         failure, stm_error contains error code.
 */
int stm_init(int verbose);

/* 
 Call stm_open_shared_segment() to open a shared memory segment in each process that wants to access it.
 You can have as many shared areas as you like.  You specify a file that is shared among all
 processes that want to communicate through each shared area.  If it is important to control the
 virtual address at which the shared area exists in your address space, you can specify it here
 (or pass in NULL to let mmap decide where to map it).   The shared file, as well as a metadata file
 with the same name but ending in ".metadata", will be created if it does not exist, and will be grown
 to the requested length if it is not long enough.
 
 Args:
 filename       Pathname of file to back this shared segment.  does not need to exist - will be created.
                Also, <filename>.metadata will be created to hold transaction metadata
 size           Length in bytes of the shared segment you need.  File will be grown if it is not this large already.
 requested_va   If NULL, the shared segment will be allocated at will.  If specified, the address where you would
                like your shared segment.
 prot_flags     Either PROT_NONE, or the binary OR of PROT_READ and PROT_WRITE (or just one of them).
                Controls access to shared segment between transactions.
 
 Return value:
 NULL           failure; stm_error contains error code.
 non-NULL       pointer to a shared_segment object.  This is an opaque object (the struct internals are not exposed)
                The API for accessing it is defined in this file.
 
 */
struct shared_segment *stm_open_shared_segment(char *filename, size_t size, void *requested_va, int prot_flags);

/*
 Until you start a transaction, you have full unsynchronized read-write access to the shared area
 (depending on the protection you set when the shared segment was opened).
 Once you call stm_start_transaction(), your writes will be private and not seen by other processes until
 you commit the transaction.  In addition, you are guaranteed either read consistency (no other process will
 have written into any page you read from during the transaction) or else the transaction will abort 
 and retry.  (You should design transactions to be short, have no other side effects (like I/O), and be restartable!).
 Transactions can be nested.  Only the outermost transaction actually commits changes when it is done.
 This allows transactions to be built up of other transactions.
 The name you provide must match the name in the matching commit.
 
 Argument:
 trans_name     name-tag for this transaction.  Cannot be NULL.  Must match name in corresponding commit.
 
 Return value:
  0             success
 -1             failure, stm_error contains error code.
 */


#define STM_MIN_DELAY 10

/*
 Call this macro, not the internal function it calls.  This is what enables transaction restarts.
 */
#define stm_start_transaction(trans_name) \
{   if (_stm_transaction_stack_empty()) {\
        int _status_, _delay_ = STM_MIN_DELAY;\
        struct timespec _ts_;\
        if ((_status_ = setjmp(stm_jmp_buf)) > 0) {\
            _ts_.tv_sec = 0;\
            _ts_.tv_nsec = _delay_;\
            nanosleep(&_ts_, NULL);\
            _delay_ += _delay_>>2;\
        } else if (_status_ < 0) {\
            exit (-1);\
        }\
    }\
    _stm_start_transaction(trans_name);\
}



/*
 Call stm_commit_transaction() when you are ready to commit the changes you have made during a transaction
 to the shared area(s).  If any other processes have modified the pages you accessed during the transaction
 your transaction will fail and you should retry the transaction.
 
 This should be in the same scope as the corresponding stm_start_transaction().
 
 Arguments:
 trans_name     matching name-tag to name given in stm_start_transaction
 Return values:
  0     Success
 -1     Serious error other than conflict with another process, which causes a retry.
        Do not retry the transaction, figure out the error.
 */
int stm_commit_transaction(char *trans_name);


/*
 When you are done with a shared segment you can close it with stm_close_shared_segment().  You pass it
 the object that was returned by stm_open_shared_segment().   Any transactions in progress will abort their
 changes to this segment.  You do not normally need to do this - you can call stm_close() to close everything instead.
 
 Arguments:
 seg    pointer to shared_segment, as provided by stm_open_shared_segment
 */
void stm_close_shared_segment(struct shared_segment *seg);


/*
 Call this to release all resources and virtual memory mappings associated with the STM manager when you
 are done with it.  Also restores default signal handling.
 */
void stm_close();



/*
 Returns the shared_segment associated with a virtual address, or NULL.  Used by stmalloc.c.
 */
struct shared_segment *stm_find_shared_segment(void *va);


/*
 Returns the first virtual address within a shared memory segment.
 */
void *stm_segment_base(struct shared_segment *seg);

/*
 Returns the size in bytes of a shared memory segment.
 */
size_t stm_segment_size(struct shared_segment *seg);

/*
 Returns the page size in bytes of a shared memory segment  (should be the same for all segments!)
 */
size_t stm_page_size(struct shared_segment *seg);


/*
 Returns the file descriptor associated with an open shared memory segment.
 */
int stm_segment_fd(struct shared_segment *seg);

extern int stm_errno;       // on errors, this variable will contain one of the following codes:

/*
 Possible values of stm_errno.
 */

#define STM_COLLISION_ERROR 1
#define STM_FILETYPE_ERROR 2
#define STM_FILESIZE_ERROR 3
#define STM_ALLOC_ERROR 4
#define STM_OPEN_ERROR 5
#define STM_MMAP_ERROR 6
#define STM_ACCESS_ERROR 7
#define STM_SIGNAL_ERROR 8
#define STM_NULL_NAME_ERROR 9
#define STM_WRITE_ERROR 10
#define STM_TRANS_STACK_ERROR 11
#define STM_OWNERSHIP_ERROR 12



/*
 Private functions that your program should not use.
 */

/*
 The following function is for use by stmalloc.c
 Returns a pointer to the head of the free list for a shared_segment.  Note the head of the
 free list is *in* the segment because it is shared by all processes that use the segment!
 */
struct segalloc_node **stm_free_list_addr(struct shared_segment *seg);

/*
 The following function is for use by stmalloc.c
 Records the head of the free list into the shared_segment object.
 */
void stm_set_free_list_addr(struct shared_segment *seg, struct segalloc_node **free_list_addr);


/*
 These are really private functions so do not call them directly.  They have to be exposed for use by the
 above macro.
 */
int _stm_transaction_stack_empty();
int _stm_start_transaction(char *trans_name);




