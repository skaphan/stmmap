/*
 
 stmalloc.c
 
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

#include <stdio.h>


#include "stm.h"
#include "segalloc.h"
#include "stmalloc.h"


void stm_alloc_init(struct shared_segment *seg, int mode) {
	stm_start_transaction("alloc.init");
	
	stm_set_free_list_addr(seg, seg_alloc_init(stm_segment_base(seg), stm_segment_size(seg), mode));
	
	stm_commit_transaction("alloc.init");
	
}

void stm_free(void *va) {
	struct shared_segment *seg;
	size_t size;
	
	stm_start_transaction("alloc.free");
	seg = stm_find_shared_segment(va);
	if (seg) {
		size = *(((size_t*)va)-1);
		seg_free(va - sizeof(size_t), size, stm_segment_base(seg), stm_free_list_addr(seg));
	}
	stm_commit_transaction("alloc.free");
}

void *stm_alloc(struct shared_segment *seg, size_t size) {
	void *result;
	size_t real_size = seg_block_size_for(size + sizeof(size_t));
	
	stm_start_transaction("alloc.new");	
	result = seg_alloc(real_size, stm_free_list_addr(seg));
	if (result) {
		*(size_t*)result = real_size;
	} 
	stm_commit_transaction("alloc.new");
	
	if (result == NULL) {
		fprintf(stderr, "Failed to allocate size %ld\n", size);
		return NULL;
	}
	return result + sizeof(size_t);
}

