/*
 *  stmalloc.h
 *  stmmap
 *
 *  Created by Shel Kaphan on 10/15/09.
 *
 *  This is the API for the STM package's optional memory allocator.
 *
 */

#include <stddef.h>

struct shared_segment;

/*
 Initialize the memory allocator to work on a particular shared memory segment, which should already have
 been opened with stm_open_shared_segment().  
 
 Args:
 seg	shared_segment that was returned by stm_open_shared_segment().
 mode	0 = use existing free-list in segment
		1 = initialize free-list  (you have to know, somehow, that your process is the first to access the shared_segment
 */
void stm_alloc_init(struct shared_segment *seg, int mode);


/* 
 Allocate size bytes out of the shared memory segment seg.  Seg is the object previously returned to you by
 stm_open_shared_segment().
 */
void *stm_alloc(struct shared_segment *seg, size_t size);

/*
 Free a block of memory that was previously allocated with stm_alloc()
 */
void stm_free(void *va);



