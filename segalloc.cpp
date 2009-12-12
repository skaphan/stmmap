/*
 
 segalloc.cpp
 
 This is the multi-threading compatible implementation of a low-level memory
 allocator for memory segments.  It knows nothing of the STM package or any of
 its objects. It is used by stmalloc.c.
 
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

#include "AVLtree.hpp"
#include "segalloc.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

typedef char voidish;


class segalloc_node : public AVLtreeNode {
public:
    size_t size;
    size_t size_mask;
    
    // virtual int compareToKey(void *key);
    // virtual void* nodeKey();
    
    segalloc_node(size_t _size) : AVLtreeNode() {
        size = _size;
    };
    
};

static void set_size_mask(segalloc_node *n) {
    segalloc_node *l, *r;

    AVLtreeNode *a = n;
    n->size_mask = n->size;

    l = (segalloc_node *)a->left.get();
    r = (segalloc_node *)a->right.get();
    if (l)
        n->size_mask |= l->size_mask;

    if (r)
        n->size_mask |= r->size_mask;   
    
}

static void set_size_mask_r(segalloc_node *a) {
    segalloc_node *p;
    set_size_mask(a);
    p = (segalloc_node *)a->parent.get();
    if (p)
        set_size_mask_r(p);
}
    
#if 0
void* segalloc_node::nodeKey() {
    return this;
}

int segalloc_node::compareToKey(void *key) {
    
    if (this == key)
        return 0;
    else if ((voidish*)this < (voidish*)key)
        return -1;
    else {
        return 1;
    }
}
#endif

static void *nodekey(void *n) {
    return n;
}

static int nodecmp(void* a, void* b) {
    
    if (a==b)
        return 0;
    else if ((voidish*)a < (voidish*)b)
        return -1;
    else {
        return 1;
    }
    
}


// here size is a number, and we want the smallest power of two that is at least as large as size
//  
static size_t least_power_of_2_ge (size_t size) {   
    size_t remaining_bits, lowbit;
        
    remaining_bits = size;
    while ((lowbit = (remaining_bits & -remaining_bits)) != remaining_bits)
        remaining_bits ^= lowbit;
    
    if (size != remaining_bits)
        remaining_bits <<= 1;   // This is now the smallest power of 2 >= the size requested.
    
    return remaining_bits;
}

size_t seg_block_size_for (size_t size) {   
    static size_t t = 0;
    
    if (t == 0) t = least_power_of_2_ge(sizeof(segalloc_node));
    
    if (size <= t) 
        return t;       
    else
        return least_power_of_2_ge(size);
}


// here x is a bitmask of powers of two, and size is a number with a single bit (a power of two)
// find the least power of 2 in x that is at least as large as size.
//
static size_t least_power_of_2_gt_in (size_t x, size_t size) {
    x &= -size;     // just keep the bits to the left of the size bit.
    return x & -x;  // finds the low order bit of what's left.
}


static size_t greatest_power_of_2_le (size_t size) {
    size_t remaining_bits, low_bit;

    remaining_bits = size;
    while ((low_bit = (remaining_bits & -remaining_bits)) != remaining_bits)
        remaining_bits ^= low_bit;
        
    return low_bit;

}


static void split_node(segalloc_node* t, size_t size, offset_ptr<segalloc_node>* free_list_addr) {
    segalloc_node *new_node;
    while (t->size > size) {
        t->size >>= 1;
        set_size_mask_r(t);
        
        new_node = (segalloc_node*)((voidish*)t + t->size);
        new(new_node) segalloc_node (t->size);          // initialize node in place
#if 0     
         if (AVLsearch(free_list_addr->get(), new_node, nodecmp, nodekey) != NULL) {
            fprintf(stderr, "Already in tree!\n");
        }
#endif  
                
        AVLaddToTree(new_node, (offset_ptr<AVLtreeNode>*)free_list_addr, nodecmp, nodekey);       
    }
}

// Find the best-fitting block on the free-list for a block of size "size" below.
// Size must be a power of two.
//
static segalloc_node* segalloc_search(AVLtreeNode* t, size_t size, offset_ptr<segalloc_node>* free_list_addr) {   
    segalloc_node* result = NULL;
    size_t tsize;

    if (t == NULL)
        return NULL;
    
    if ((tsize = ((segalloc_node*)t)->size) == size)
        return (segalloc_node*)t;
    
    size_t left_smallest = t->left? least_power_of_2_gt_in (((segalloc_node*)t->left.get())->size_mask, size) : 0;
    size_t right_smallest = t->right? least_power_of_2_gt_in (((segalloc_node*)t->right.get())->size_mask, size) : 0;
                
    if (size > tsize) {
        // current node won't do -- just decide between left & right branches
        
        if (left_smallest == 0) {
            if (right_smallest == 0) {
                result = NULL;
            } else {
                result = segalloc_search(t->right.get(), size, free_list_addr);
            }
        } else {
            if (right_smallest == 0) {
                result = segalloc_search(t->left.get(), size, free_list_addr);
            } else {
                if (left_smallest < right_smallest)
                    result = segalloc_search(t->left.get(), size, free_list_addr);
                else
                    result = segalloc_search(t->right.get(), size, free_list_addr);
            }
        }
    } else {
        // current node is usable.
    
        if (left_smallest && left_smallest < tsize) {
            if (right_smallest && right_smallest < tsize) {
                // both left and right branches usable and better than current node
                if (left_smallest > right_smallest)
                    result = segalloc_search(t->right.get(), size, free_list_addr);
                else
                    result = segalloc_search(t->left.get(), size, free_list_addr);
            } else {
                // only left branch usable
                result = segalloc_search(t->left.get(), size, free_list_addr);
            }
        } else {
            if (right_smallest && right_smallest < tsize) {
                // only right branch usable
                result = segalloc_search(t->right.get(), size, free_list_addr);
            } else {
                // neither left or right branches are better than current node
                result = (segalloc_node*)t;
                // check here to split the node, since it is too big.
                split_node((segalloc_node*)t, size, free_list_addr);
            }   
        
        }
        
    }
    return result;
        
}

void *seg_alloc(size_t size, void* free_list_addr) {
    
    offset_ptr<segalloc_node> *ptr_to_free_list_ptr = (offset_ptr<segalloc_node>*)free_list_addr;
    
    AVLtreeNode* result;
    size_t real_size;
    
    result = segalloc_search(ptr_to_free_list_ptr->get(), seg_block_size_for(size), ptr_to_free_list_ptr);
    if (result) {
        real_size = ((segalloc_node *)result)->size;
        AVLremoveFromTree(result, (offset_ptr<AVLtreeNode>*) free_list_addr);
        memset(result, 0, real_size);
    }
    
    
    return ((void*)result);
        
}


static size_t find_potential_buddy(off_t offset, size_t buddy_size) {
    
    size_t buddy_lowbits = ~(-buddy_size);
    if (offset & buddy_lowbits)
        return -1;
    else
        return (offset ^ buddy_size);
}

static void merge_with_buddies(void *base_va, segalloc_node *freed_object,
                               offset_ptr<segalloc_node>* free_list_addr) {
    
    off_t offset, buddy_offset;
    segalloc_node *buddy_block;
    
    
//  while (freed_object->size <= MAX_BLOCK_SIZE) {
    
    while (1) {
        
        offset = (voidish*)freed_object - (voidish*)base_va;
        
        if ((buddy_offset = find_potential_buddy(offset, freed_object->size)) == -1)
            break;
        
        offset_ptr<segalloc_node> *ptr_to_free_list_ptr = (offset_ptr<segalloc_node>*)free_list_addr;
        
        if ((buddy_block = (segalloc_node *)AVLsearch(ptr_to_free_list_ptr->get(),
                                                      (voidish*)base_va + buddy_offset, nodecmp, nodekey)) != NULL  &&
            buddy_block->size == freed_object->size) {
            
            size_t fsize = freed_object->size;
            
            if (buddy_block > freed_object) {
                AVLremoveFromTree(buddy_block, (offset_ptr<AVLtreeNode>*)free_list_addr);                
            } else {                
                AVLremoveFromTree(freed_object, (offset_ptr<AVLtreeNode>*)free_list_addr);
                freed_object = buddy_block;
            }
            freed_object->size = fsize << 1;
            set_size_mask_r(freed_object);
            
        } else {
            // there is no buddy.
            return;
        }
    }
}

static int nodes_overlap_cmp(void* aa, void* bb) {
    voidish *a = (voidish*)aa;
    voidish *b = (voidish*)bb;
    
    if ((a <= b && b < a + ((segalloc_node*)a)->size) ||
        (b <= a && a < b + ((segalloc_node*)b)->size))
        return 0;
    else if (a<b)
        return -1;
    else {
        return 1;
    }
    
}

void seg_free(void *object_va, size_t size, void *base_va, void *free_list_addr) {
    size_t block_size;
    block_size = seg_block_size_for(size);
    
    offset_ptr<segalloc_node> *ptr_to_free_list_ptr = (offset_ptr<segalloc_node>*)free_list_addr;

    if (AVLsearch(ptr_to_free_list_ptr->get(), object_va, nodes_overlap_cmp, nodekey) != NULL) {
        fprintf(stderr, "seg_free: node 0x%lx already in free list!\n", (unsigned long)object_va);
        return;
    }
    
    new(object_va) segalloc_node(block_size);   // initialize node in place
    AVLaddToTree((AVLtreeNode*)object_va, (offset_ptr<AVLtreeNode>*)free_list_addr, nodecmp, nodekey);  
    merge_with_buddies(base_va, (segalloc_node*)object_va, ptr_to_free_list_ptr);  
    
}

void *seg_alloc_init(void *base_va, size_t size, int mode) {
    
    AVLuserHook = (void (*)(AVLtreeNode*))set_size_mask;
    
    if (mode == 1) {
        int first_time = 1;
        void *va = base_va;
        size_t allocated_size;
        size_t remaining_size = size;
        size_t min_block_size;
        offset_ptr<AVLtreeNode> tmp_free_list = NULL;

        min_block_size = least_power_of_2_ge(sizeof(segalloc_node));
        
        // printf("min_block_size = %d, sizeof(segalloc_node) = %d\n", min_block_size, sizeof(segalloc_node));

        while (remaining_size >= min_block_size) {
            allocated_size = greatest_power_of_2_le(remaining_size);
            
            new(va) segalloc_node(allocated_size);  // initialize node in place
            
            if (first_time) {
                AVLaddToTree((segalloc_node*)va, &tmp_free_list, nodecmp, nodekey);
                if (seg_alloc(min_block_size, &tmp_free_list) != base_va) {
                    fprintf(stderr, "seg_alloc_init:  initial allocation != base_va\n");
                    exit(-1);
                }
                
                *(offset_ptr<AVLtreeNode> *)base_va = tmp_free_list;      // move the tree root to the base of the segment
                first_time = 0;
            } else {
                AVLaddToTree((segalloc_node*)va, (offset_ptr<AVLtreeNode>*)base_va, nodecmp, nodekey);
            }

            va = (voidish*)va + allocated_size;
            remaining_size -= allocated_size;
        }
    }
    
    return base_va;             // this is now the address of the offset_ptr that points to the free list.
}

static int verify_tree_integrity(AVLtreeNode *tt, AVLtreeNode* parent, void* lower_bound, void* upper_bound) {
    
    segalloc_node *t;
    size_t size_mask;
    int ldepth, rdepth, depth, bal;
    int result = 0;
    
    t = (segalloc_node *)tt;
    
    if (lower_bound && ((void*)t < lower_bound)) {
        fprintf(stderr, "overlapping nodes: node %lx < lower bound %lx\n",
                (unsigned long)t, (unsigned long)lower_bound);
        result++;
    }
    
    if (upper_bound && (((voidish*)t + t->size) > upper_bound)) {
        fprintf(stderr, "overlapping nodes: node %lx[%lx] > upper bound %lx\n",
                (unsigned long)t, t->size, (unsigned long)upper_bound);
        result++;
    }
        
    
    if (tt->parent.get() != parent) {
        fprintf(stderr, "bad parent: node %lx, parent is %lx, should be %lx\n",
                (unsigned long)tt, (unsigned long)tt->parent.get(), (unsigned long)parent);
        result++;
    }
    size_mask = t->size | (tt->right? ((segalloc_node*)tt->right.get())->size_mask : 0)
                        | (tt->left? ((segalloc_node*)tt->left.get())->size_mask : 0);
    
    if (size_mask != t->size_mask) {
        fprintf(stderr, "Node %lx, size mask is %lx, should be %lx. size=%lx, lmask=%lx, rmask=%lx\n",
                (unsigned long)t, t->size_mask, size_mask, t->size,
                tt->left? ((segalloc_node*)tt->left.get())->size_mask:0,
                tt->right? ((segalloc_node*)tt->right.get())->size_mask:0);
        result++;
    }
    
    ldepth = tt->left? tt->left->depth : 0;
    rdepth = tt->right? tt->right->depth : 0;
    depth = (((ldepth > rdepth)? ldepth : rdepth) + 1);
    
    if (depth != tt->depth) {
        fprintf(stderr, "depth is %d, should be %d\n", tt->depth, depth);
        result++;
    }
    
    bal = ldepth - rdepth;
    
    if (bal < -1 || bal > 1) {
        fprintf(stderr, "tree out of balance: %d\n", bal);
        result++;
    }
    
    if (tt->left && tt->left.get() >= tt) {
        fprintf(stderr, "left branch %lx not to left of its parent %lx\n",
                (unsigned long)tt->left.get(), (unsigned long)tt);
        result++;
    }
                
    if (tt->right && tt->right.get() <= tt) {
        fprintf(stderr, "right branch %lx not to right of its parent %lx\n",
                (unsigned long)tt->right.get(), (unsigned long)tt);
        result++;
    }
                        
                        
    if (tt->left)
        result += verify_tree_integrity(tt->left.get(), tt, lower_bound, tt);
    
    if (tt->right)
        result += verify_tree_integrity(tt->right.get(), tt, (voidish*)t + t->size, upper_bound);
    
    return result;
}

int seg_verify_tree_integrity(segalloc_node *free_list) {
    return verify_tree_integrity(free_list, NULL, NULL, NULL);
}

    
    
static void __overlap_check(segalloc_node *t, void* base, size_t size, void* lower_bound, void* upper_bound) {

    AVLtreeNode *tt = t;
    if (lower_bound && (base < lower_bound)) {
        fprintf(stderr, "overlapping nodes\n");
    }
    
    if (upper_bound && (((voidish*)base + size) > upper_bound)) {
        fprintf(stderr, "overlapping nodes\n");
    }
    
    if (base <= (void*)t) {
        if (tt->left)
            __overlap_check((segalloc_node*)tt->left.get(), base, size, lower_bound, tt);
    
    } 
    if (base >= (void*)t) {
        if (tt->right)
            __overlap_check((segalloc_node*)tt->right.get(), base, size, (voidish*)t + t->size, upper_bound);
    }
}
    

void overlap_check(segalloc_node *t, void *base, size_t size) {
    __overlap_check(t, base, size, NULL, NULL);
}


void seg_print_free_list(segalloc_node *t) {
    AVLtreeNode *a = t;
    
    if (a->left)
        seg_print_free_list((segalloc_node*)a->left.get());
    
    printf("[ %lx, %lx ] %lx\n", (unsigned long)t, (unsigned long)t+t->size, (unsigned long)t->size);
    
    if (a->right)
        seg_print_free_list((segalloc_node*)a->right.get());
}

    
struct segalloc_node* seg_free_list_from_free_list_addr(void *free_list_addr)
{
    offset_ptr<segalloc_node> *ptr_to_free_list_ptr = (offset_ptr<segalloc_node>*)free_list_addr;
    return ptr_to_free_list_ptr->get();
}






