#include <stdio.h>
#include <unistd.h> // for getpid()
#include <string.h> 


#include "stm.h"
#include "segalloc.h"       // just for seg_verify_tree_integrity()
#include "stmalloc.h"




#define array_size 128
        
void alloc_test(struct shared_segment *seg, int n_iterations) {
    int i, j, size;
    size_t size_mask = 0xfffff;
    void *allocated[array_size];

    memset(allocated, 0, sizeof(allocated));    
    srandom(getpid());
        
    for(i=0; i<array_size; i++) {               
        size = random() & size_mask;    
        allocated[i] = stm_alloc(seg, size);        // performs a transaction internally
    }
    
    for(i=0; i<n_iterations; i++) {
        
        void *t;
        
        j = i % array_size;
        
        stm_start_transaction("blech");
        
        if ((t = allocated[j]) != NULL)
            stm_free(t);            // performs a transaction internally - 
                                    // note nested transaction!
        
        size = random() & size_mask;
        t = stm_alloc(seg, size);           // performs a transaction internally
            
        seg_verify_tree_integrity(*stm_free_list_addr(seg));

        stm_commit_transaction("blech");
        
        allocated[j] = t;       // don't set this until outside the transaction, since it is referred to earlier within the
                                // same transaction ("blech")!

    }
    
    for (i=0; i<array_size; i++) {
        if (allocated[i])
            stm_free(allocated[i]);     // performs a transaction internally
    }
        
    stm_start_transaction("foo");
    seg_print_free_list(*stm_free_list_addr(seg));
    stm_commit_transaction("foo");
}


// command line args are either "i", which initializes the shared segment free list,
// or nothing, which just runs the test, which consists of allocating and deallocating
// random size chunks.    The idea is to run several copies of this simultaneously against
// the same shared memory and see that it all works.

int main (int argc, const char * argv[]) {
    
    int prot_flags = PROT_NONE;
    struct shared_segment *seg;
    int segsize = 1<<23;        
    stm_init(0x7);              // blab a lot - see API and change if you like.
    
    if ((seg = stm_open_shared_segment("/tmp/stmtest12345", segsize, (void*) 0,
                                       prot_flags)) == NULL)
        exit (-1);
    
    printf("shared segment base = 0x%lx\n", (unsigned long)stm_segment_base(seg));
    
    if (argv[1] && argv[1][0] == 'i') {
        stm_alloc_init(seg, 1);
        stm_start_transaction("foob");
        seg_print_free_list(*stm_free_list_addr(seg));
        stm_commit_transaction("foob");
    } else {
        stm_alloc_init(seg, 0); 
        alloc_test(seg, 1000);
    }
        
    stm_close();
    
    exit (0);
    
}
