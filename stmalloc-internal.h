/*
 *  stmalloc-internal.h
 *  stmmap
 *
 *  Created by Shel Kaphan on 9/20/09.
 *
 */


#include <stddef.h>

struct stmalloc_node;

struct stmalloc_node **stm_alloc_init(void *base_va, size_t size, int mode);

void *stm_alloc(size_t size, struct stmalloc_node **free_list);
	
void stm_free(void *object_va, size_t size, void *base_va, struct stmalloc_node **free_list);

size_t block_size_for(size_t size);

int verify_tree_integrity(struct stmalloc_node *free_list);

