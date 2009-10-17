/*
 
 stmalloc.h
 This is the API for the STM package's optional memory allocator.

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



