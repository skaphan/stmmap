#include <stdio.h>
#include <unistd.h> // for getpid()
#include <string.h> 
#include <pthread.h>


#include "stm.h"
#include "segalloc.h"       // just for seg_verify_tree_integrity()
#include "stmalloc.h"


#ifdef __APPLE__

#include <mach/mach.h>
#include <mach/exception.h>


// This makes it possible to use gdb to debug this, to an extent.
//
static void disable_gdb_nosiness() __attribute__ ((constructor));

static void disable_gdb_nosiness()
{
	// kern_return_t success = 
    task_set_exception_ports(mach_task_self(),
                             EXC_MASK_BAD_ACCESS,
                             MACH_PORT_NULL,
                             EXCEPTION_STATE_IDENTITY,
                             MACHINE_THREAD_STATE);
	// assert(success == KERN_SUCCESS);
}

#endif


#define array_size 128
        
void alloc_test(struct shared_segment *seg, int n_iterations) {
    int i, j, size;
    size_t size_mask = 0xffff;
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
            
        seg_verify_tree_integrity(stm_free_list(seg));

        stm_commit_transaction("blech");
        
        allocated[j] = t;       // don't set this until outside the transaction, since it is referred to earlier within the
                                // same transaction ("blech")!

    }
    
    for (i=0; i<array_size; i++) {
        if (allocated[i])
            stm_free(allocated[i]);     // performs a transaction internally
    }
        
    stm_start_transaction("foo");
    seg_print_free_list(stm_free_list(seg));
    stm_commit_transaction("foo");
}


void *thread_fn(void *arg)
{
    
    int prot_flags = PROT_NONE;
    // int prot_flags = PROT_READ|PROT_WRITE;
    struct shared_segment *seg;
    int segsize = 1<<23;  
    
    stm_init_thread_locals();
    
    if ((seg = stm_open_shared_segment("/tmp/stmtest12345", segsize, (void*) 0,
                                       prot_flags)) == NULL)
        exit (-1);
    
    printf("shared segment base = 0x%lx\n", (unsigned long)stm_segment_base(seg));
    stm_alloc_init(seg, 0); 
    alloc_test(seg, 1000);
    return NULL;
}

// command line args are either "i", which initializes the shared segment free list,
// or nothing, which just runs the test, which consists of allocating and deallocating
// random size chunks.    The idea is to run several copies of this simultaneously against
// the same shared memory and see that it all works.

int main (int argc, const char * argv[]) {
    
    
    
    stm_init(0x7);              // blab a lot - see API and change if you like.

    if (argv[1] && argv[1][0] == 'i') {
        int prot_flags = PROT_NONE;
        struct shared_segment *seg;
        int segsize = 1<<23;   
                
        if ((seg = stm_open_shared_segment("/tmp/stmtest12345", segsize, (void*) 0,
                                           prot_flags)) == NULL)
            exit (-1);
        printf("shared segment base = 0x%lx\n", (unsigned long)stm_segment_base(seg));
        stm_alloc_init(seg, 1);
        stm_start_transaction("foob");
        seg_print_free_list(stm_free_list(seg));
        stm_commit_transaction("foob");
    } else {
#if 0   
        thread_fn(NULL);
#else
        void *thread1_val;
        pthread_t thread1;
        pthread_create(&thread1, NULL, thread_fn, NULL);

        void *thread2_val;
        pthread_t thread2;
        pthread_create(&thread2, NULL, thread_fn, NULL);
        
        
        pthread_join(thread2, &thread2_val);
     
        pthread_join(thread1, &thread1_val);
                
        
#endif
    
    }
    
    stm_close();
    exit (0);
    
}


