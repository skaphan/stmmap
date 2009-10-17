/*
 
 segalloc.h
 
 This is the interface to the low-level memory allocator for memory segments.  
 It knows nothing of the STM package or any of its objects. It is used by stmalloc.c.
 You probably won't need to access this API.
 
 
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

struct segalloc_node;

struct segalloc_node **seg_alloc_init(void *base_va, size_t size, int mode);

void *seg_alloc(size_t size, struct segalloc_node **free_list);
    
void seg_free(void *object_va, size_t size, void *base_va, struct segalloc_node **free_list);

size_t seg_block_size_for(size_t size);

int seg_verify_tree_integrity(struct segalloc_node *free_list);

