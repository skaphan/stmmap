/*
 *  stmalloc.c
 *  stmmap
 *
 *  Created by Shel Kaphan on 10/15/09.
 *
 */

#include <stdio.h>


#include "stm.h"
#include "segalloc.h"
#include "stmalloc.h"


void alloc_init(struct shared_segment *seg, int mode) {
	stm_start_transaction("alloc.init");
	
	stm_set_free_list_addr(seg, stm_alloc_init(stm_segment_base(seg), stm_segment_size(seg), mode));
	
	stm_commit_transaction("alloc.init");
	
}

void alloc_free(void *va) {
	struct shared_segment *seg;
	size_t size;
	
	stm_start_transaction("alloc.free");
	seg = stm_find_shared_segment(va);
	if (seg) {
		size = *(((size_t*)va)-1);
		stm_free(va - sizeof(size_t), size, stm_segment_base(seg), stm_free_list_addr(seg));
	}
	stm_commit_transaction("alloc.free");
}

void *alloc_new(struct shared_segment *seg, size_t size) {
	void *result;
	size_t real_size = block_size_for(size + sizeof(size_t));
	
	stm_start_transaction("alloc.new");	
	result = stm_alloc(real_size, stm_free_list_addr(seg));
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

