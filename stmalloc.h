/*
 *  stmalloc.h
 *  stmtest
 *
 *  Created by Shel Kaphan on 10/15/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include <stddef.h>

struct shared_segment;

void alloc_init(struct shared_segment *seg, int mode);

void alloc_free(void *va);

void *alloc_new(struct shared_segment *seg, size_t size);



