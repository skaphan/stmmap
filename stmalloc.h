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

void stm_alloc_init(struct shared_segment *seg, int mode);

void stm_free(void *va);

void *stm_alloc(struct shared_segment *seg, size_t size);



