/*
 *  segalloc.h
 *  stmmap
 *
 *  Created by Shel Kaphan on 9/20/09.
 *
 *  This is the interface to the low-level memory allocator for memory segments.  
 *  It knows nothing of the STM package or any of its objects. It is used in stmalloc.c.
 *	You probably won't need to access this API.
 *
 */


#include <stddef.h>

struct segalloc_node;

struct segalloc_node **seg_alloc_init(void *base_va, size_t size, int mode);

void *seg_alloc(size_t size, struct segalloc_node **free_list);
	
void seg_free(void *object_va, size_t size, void *base_va, struct segalloc_node **free_list);

size_t block_size_for(size_t size);

int verify_tree_integrity(struct segalloc_node *free_list);

