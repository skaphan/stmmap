/*
 *  stm.h
 *  stmtest
 *
 *  Created by Shel Kaphan on 9/18/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */


/*
 The purpose of this package is to provide something akin to "Software Transactional Memory" functionality in C and C++,
 without requiring much extra complexity for the programmer.	
 
 It is built on top of file-mapping -- Unix's mmap() system call.  One or more shared memory areas are opened by each
 process that uses this package, using mmap().  When no transaction is in progress, the shared area is
 visible to any processes that access the shared, mapped file.   When a transaction is in progress, the shared area is
 access-protected, and all accesses are trapped.   A signal handler keeps track of the accessed pages.  Each accessed
 page is then mapped "private" on first access so that our modifications are not visible to any other process.  On commit,
 we attempt to establish ownership of all modified pages and then write those pages back to the shared file.
 No I/O need actually occur -- the writes just affect the mapped file's memory buffers.  At this time,
 we check to see if any other processes have modified the pages we have accessed during the transaction,
 by means of a transaction ID associated with each page.  If they have, our transaction is aborted and all changes discarded.
 
 This mechanism provides read consistency in that if our transaction succeeds, we guarantee that no other transaction will
 have modified the pages while our transaction was in progress.  However, we do not guarantee the transaction will succeed.
 
 Accesses not during transactions are "real time" and there is no guarantee of consistency.
 The access control on shared segments not during transactions can be specified for each shared segment.
 
 Transactions are composable -- that is, they can be nested.  This is so that complex transactions can be built up
 out of simpler ones.
 
 Processes can have a number of shared memory areas, limited by the number of file descriptors and virtual memory available.
 
 This package works at the process level, not at the thread level.  If you are looking for a multi-threaded approach, this is not for
 you.  However, it does work using arbitrarily large shared memory areas between processes, limited by the amount of virtual
 address space available.   Each process can map the shared segment to a known virtual location, so arbitrary data structures
 can exist in the shared area.  They just should not refer outside the shared area to objects that only exist in the private
 parts of each process's address space.
 
 It is arguable that it is better for threads of execution (processes, in this case) to share only the data structure they
 *intend* to share, not everything in the address space, that they only accidentally share.
 
 Another limitation of this package is that it operates on the OS page level.  That is, only one process can write to a page
 during a transaction, and any other process's transaction that accesses that page will have to be aborted and retried.
 This may not be as bad as it sounds if you code your transactions to be relatively short.  The arbitration between processes
 seeking to own a page during a transaction is on a first-come first-served basis.  Since pages are always reserved in
 order of virtual address, there can be no deadlock.  As soon as a transaction tries to obtain a page that has been modified
 by another process's transaction, it aborts.
 
 Another restriction is that this package will only work on systems where the contents of shared, mapped files are immediately
 visible to all processes that have them mapped shared.  This could possibly fail on some systems without sufficient cache
 coherency, for example.  This package does *not* depend on private mappings being kept up to date with the current contents
 of a file.
 
 Warning: since transactions will be retried until they succeed, variables outside the shared memory segment(s) that are
 referenced within a transaction must be handled with care.  In particular, you should not access anything that you later modify
 in the same transaction.  If the transaction is retried, the earlier reference will pick up the value set in an earlier
 try. You can set and then use a variable in the same transaction, but you can't use a variable and then set it to a new value
 and expect that to work right if retries are necessary.  Only information in the shared segments is managed transactionally.
 
 */



#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>		// for PROT_NONE, PROT_READ, PROT_WRITE
#include <sys/stat.h>		// for ino_t
#include <setjmp.h>
#include <time.h>



typedef uint32_t transaction_id_t;

extern jmp_buf stm_jmp_buf;

/*
 Call stm_init() to initialize the STM package.  It sets up signal handlers and verbosity level for 
 subsequent activity.  On Mac OS you get SIGBUS
 signals on page access faults.  You might need to change it to handle SIGSEGV instead on some systems.
 
 Args:
 verbose	Controls whether to print out errors on stderr. This is a bit mask
			Bit		Meaning
			1		report errors
			2		report conflicts that cause transactions to abort
			4		report pages modified during each transaction commit or abort
 
 Return value:
  0			success
 -1			failure, stm_error contains error code.
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
 filename		Pathname of file to back this shared segment.  does not need to exist - will be created.
				Also, <filename>.metadata will be created to hold transaction metadata
 size			Length in bytes of the shared segment you need.  File will be grown if it is not this large already.
 requested_va	If NULL, the shared segment will be allocated at will.  If specified, the address where you would
				like your shared segment.
 prot_flags		Either PROT_NONE, or one or the binary OR of PROT_READ and PROT_WRITE.  Controls access to shared 
				segment between transactions.
 
 Return value:
 NULL			failure, stm_error contains error code.
 non-NULL		pointer to a shared_segment object (see above).
 
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
 trans_name		name-tag for this transaction.  Cannot be NULL.  Must match name in corresponding commit.
 
 Return value:
  0				success
 -1				failure, stm_error contains error code.
 */


#define STM_MIN_DELAY 10


#define stm_start_transaction(trans_name) \
{	if (stm_transaction_stack_empty()) {\
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

int _stm_start_transaction(char *trans_name);


/*
 Call stm_commit_transaction() when you are ready to commit the changes you have made during a transaction
 to the shared area(s).  If any other processes have modified the pages you accessed during the transaction
 your transaction will fail and you should retry the transaction.
 
 Arguments:
 trans_name		matching name-tag to name given in stm_start_transaction
 Return values:
  0		success
  1		collision during commit.  you should retry the transaction
 -1		serious error other than collision.  do not retry the transaction, figure out the error.
 
 */
int stm_commit_transaction(char *trans_name);


/*
 When you are done with a shared segment you can close it with stm_close_shared_segment().  You pass it
 the object that was returned by stm_open_shared_segment().   Any transactions in progress will abort their
 changes to this segment.  You do not need to do this - you can call stm_close() to close everything instead.
 
 Arguments:
 seg	pointer to shared_segment, as provided by stm_open_shared_segment
 */
void stm_close_shared_segment(struct shared_segment *seg);


/*
 Call this to release all resources and virtual memory mappings associated with the STM manager when you
 are done with it.  Also restores default signal handling.
 */
void stm_close();


int stm_transaction_stack_empty();

struct shared_segment *stm_find_shared_segment(void *va);

struct stmalloc_node **stm_free_list_addr(struct shared_segment *seg);

void *stm_segment_base(struct shared_segment *seg);

size_t stm_segment_size(struct shared_segment *seg);

size_t stm_page_size(struct shared_segment *seg);

void stm_set_free_list_addr(struct shared_segment *seg, struct stmalloc_node **free_list_addr);

int stm_segment_fd(struct shared_segment *seg);

extern int stm_errno;		// on errors, this variable will contain one of the following codes:

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











