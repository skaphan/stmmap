/*
 
 stm.c
 
 This is the implementation of the Software Transactional Memory system.
 The corresponding API is in stm.h.
 
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
#include <fcntl.h>
#include <sys/errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>         // absolutely need this for pwrite().  (Just spent an hour chasing this...)
                            // (leaving it in even though I'm not using pwrite() right now...)
#include <pthread.h>

#include "atomic-compat.h"
#include "stm.h"


#define MAX_ACTIVE_TRANSACTIONS 100


// The structs used by stm.c are defined here and not in the header file, so they are opaque to other programs.
// To the extent necessary and useful, an access API is defined here and in stm.h.

//
// There's just one of these at the start of the metadata file associated with each shared segment
//
typedef struct transaction_data {
    transaction_id_t  transaction_counter;              // global counter for transaction IDs in each segment
    atomic_lock  transaction_lock;
    int active_transaction_high_water;
    transaction_id_t active_transactions[MAX_ACTIVE_TRANSACTIONS];
    
} transaction_data;


//
// There is an array of these starting in the 2nd page of the metadata file.  Each one represents
// the ID of a transaction currently modifying the page (if any), and keeps track of the most recent
// transaction to have modified the page.
//
typedef struct page_table_element {
    transaction_id_t  current_transaction;              // used to establish ownership of each page during commit
    transaction_id_t  completed_transaction;                // used to keep a record of the last transaction to modify each page
} page_table_element;

//
// This represents a snapshot of a single page.  We take this snapshot on first access (read or write)
// within a transaction.  These are kept in a list sorted by the page's virtual address, so that
// it is easy to lock them in a known order during commit.
//
typedef struct snapshot_list_element {
    struct snapshot_list_element *next;
    void *original_page_va;                 // The virtual address where the "real" copy of this page lives
    void *original_page_snapshot;           // copy of the unmodified page, on first access.
    int page_dirty;                         // during commit, we set this if we have modified the page.
    transaction_id_t snapshot_transaction_id; // the most recent transaction to have affected the page,
                                            // at the time the snapshot is taken.
} snapshot_list_element;

//
// There is a global stack that keeps track of nested transactions.  We only really commit changes when we commit
// the outermost transaction (the last one on the stack).
//
typedef struct transaction_stack_element {
    struct transaction_stack_element *next;
    char *transaction_name;
} transaction_stack_element;


//
// This data structure represents a shared memory area and the metadata that goes with it.
// They are kept on a global list that is sorted by inode, to facilitate locking in a known order
// to prevent deadlocks.
//
typedef struct shared_segment {
    struct shared_segment *next;
    
    char *filename;                                         // Filename of file backing shared memory area
    int fd;                                                 // File descriptor for above file
    ino_t inode;                                            // inode of above file.
    char *metadata_filename;                                // "metadata" file for above file - contains control 
                                                            // info and page table with transaction info
    int metadata_fd;                                        // file descriptor for metadata file
    
    int default_prot_flags;                                 // protection flags (PROT_READ, PROT_WRITE, PROT_NONE)
                                                            // for use on shared memory area *between* transactions
    size_t page_size;                                       // cached value of operating system page size
    
    size_t shared_seg_size;                                 // size of the shared memory area
    void *shared_base_va;                                   // first virtual address of the shared memory area
    
    size_t transaction_data_size;                           // size of the metadata area in memory
    struct transaction_data *segment_transaction_data;      // the "control" information for all transactions on this 
                                                            // shared segment.
    struct page_table_element *segment_page_table;          // the page table describing transactions on this segment
    
    transaction_id_t transaction_id;                        // current transaction ID, if any 
    struct snapshot_list_element *snapshot_list;            // list of snapshotted pages accessed during a transaction
    
    int n_prior_active_transactions;                        // number of transactions active at the time the current one
                                                            // started.
    transaction_id_t prior_active_transactions[MAX_ACTIVE_TRANSACTIONS];
                                                            // array of transaction IDs of transactions active at the time
                                                            // the current one started.
        
    void *free_list_addr;                                  // if stmalloc is in use, this points to the free list header
} shared_segment;


static int stm_verbose;

// There used to be more globals, but now they are in thread-local storage
//
// static shared_segment *shared_segment_list;
// static transaction_stack_element *transaction_stack;
// jmp_buf stm_jmp_buf;
// int stm_errno;


static pthread_key_t shared_segment_list_key;
static pthread_key_t transaction_stack_key;
static pthread_key_t stm_jmp_buf_key;
static pthread_key_t stm_errno_key;



static shared_segment *shared_segment_list() {
    return (shared_segment*)pthread_getspecific(shared_segment_list_key);
}

static void set_shared_segment_list(shared_segment *seg) {
    pthread_setspecific(shared_segment_list_key, seg);
}


static transaction_stack_element *transaction_stack() {
    return (transaction_stack_element *)pthread_getspecific(transaction_stack_key);
}

static void set_transaction_stack(transaction_stack_element *trans) {
    pthread_setspecific(transaction_stack_key, trans);
}

// This one has to be global scope so clients can use it.
jmp_buf *stm_jmp_buf() {
    return (jmp_buf *)pthread_getspecific(stm_jmp_buf_key);
}


void set_stm_jmp_buf(jmp_buf *jb) {
    pthread_setspecific(stm_jmp_buf_key, jb);
}


int stm_errno() {
    return (long int)pthread_getspecific(stm_errno_key);
}

static void set_stm_errno(int err) {
    long int lerr = err;
    pthread_setspecific(stm_errno_key, (void*)lerr);
}


static void create_thread_keys() {
    pthread_key_create(&shared_segment_list_key, NULL);
    pthread_key_create(&transaction_stack_key, NULL);
    pthread_key_create(&stm_jmp_buf_key, NULL);
    pthread_key_create(&stm_errno_key, NULL);
    
}

// Must be called in a thread, except the main thread, before doing any transactions
void stm_init_thread_locals()
{
    set_shared_segment_list(NULL);
    set_transaction_stack(NULL);
    set_stm_errno(0);
    
    set_stm_jmp_buf(calloc(1, sizeof(jmp_buf)));
    
}


//
// The next few routines manage a shared list of active transaction IDs in the metadata segment.  
//
static void add_active_transaction(shared_segment *seg) {
    int i, high_water;
    transaction_data *td;
    
    td = seg->segment_transaction_data;
    for (high_water = td->active_transaction_high_water; 
         high_water < MAX_ACTIVE_TRANSACTIONS; 
         high_water = atomic_increment_32(&td->active_transaction_high_water)) {
        
        if (high_water >= MAX_ACTIVE_TRANSACTIONS)
            break;  // could happen despite 'for' condition because of multiple processes accessing concurrently


        for (i = high_water - 1; i >= 0; i--) {
            if (atomic_compare_and_swap_32(0, seg->transaction_id,
                                           (int32_t*)&(td->active_transactions[i]))) {
                return;
            }           
        }
    
    }
    
    if (stm_verbose & 1)
        fprintf(stderr, "add_active_transaction:  Too many active transactions; recompile for larger number!\n");
    exit(-1);
    // *** Is there a more graceful way to handle this case?  There must be, but it eludes me.
    
}

static void delete_active_transaction(shared_segment *seg) {
    int i;
    transaction_data *td;
    
    td = seg->segment_transaction_data;
    for (i = 0; i < td->active_transaction_high_water; i++) {
        if (td->active_transactions[i] == seg->transaction_id) {
            td->active_transactions[i] = 0;
                        
#if 0
            // *** Functionally this is not necessary.  If not decremented it will truly be the
            // "high water" mark and will just contain some empty elements.
            
            if (i == td->active_transaction_high_water - 1) {       
                atomic_decrement_32(&td->active_transaction_high_water);        
            }   

#endif
            return;
        }           
    }
}



static void snapshot_active_transactions(shared_segment *seg) {
    int i;
    transaction_data *td;
    
    td = seg->segment_transaction_data;
    seg->n_prior_active_transactions = 0;
    
    for (i = 0; i < td->active_transaction_high_water; i++) {
        if (td->active_transactions[i] != 0 && td->active_transactions[i] != seg->transaction_id) {
            seg->prior_active_transactions[seg->n_prior_active_transactions++] = td->active_transactions[i];
        }           
    }       
}

static int find_prior_active_transaction(shared_segment *seg, transaction_id_t trans) {
    int i;
    for (i = 0; i < seg->n_prior_active_transactions; i++) {
        if (seg->prior_active_transactions[i] == trans) {
            return 1;
        }           
    }
    return 0;
}

void print_snapshot_active_transactions(shared_segment *seg) {
    int i;
    for (i=0; i<seg->n_prior_active_transactions; i++) {
        if (seg->prior_active_transactions[i])
            printf("+ %d\n", seg->prior_active_transactions[i]);
    }
}
    


static int check_file_length(int fd, size_t length, ino_t *inode) {
    struct stat sbuf;
    fstat(fd, &sbuf);
    if (inode) *inode = sbuf.st_ino;
    if ((sbuf.st_mode & S_IFMT) != S_IFREG) {
        if (stm_verbose & 1)
            fprintf(stderr, "check_file_length: bad filetype");
        set_stm_errno(STM_FILETYPE_ERROR);
        return -1;
    } else if (length > sbuf.st_size) {
        //      fprintf(stderr, "file too short\n");
        if (ftruncate(fd, length) == -1) {
            if (stm_verbose & 1)
                perror("check_file_length: ftruncate failed");
            set_stm_errno(STM_FILESIZE_ERROR);         
            return -1;
        }
    }
    return 0;
}



shared_segment *stm_open_shared_segment(char *filename, size_t segment_size, void *requested_va, int prot_flags) {
    void *status;
    int mmap_flags;
    int metadata_size;
    shared_segment *s, *prev;
    static const char *metadata_suffix = ".metadata";
    
    shared_segment *seg;
    if ((seg = calloc(1, sizeof(shared_segment))) == NULL) {
        set_stm_errno(STM_ALLOC_ERROR);
        return NULL;
    }
    if ((seg->filename = calloc(1, strlen(filename) + 1)) == NULL) {
        set_stm_errno(STM_ALLOC_ERROR);
        stm_close_shared_segment(seg);
        return NULL;
    }
    strcpy(seg->filename, filename);
    
    if ((seg->fd = open(seg->filename, O_RDWR|O_CREAT, 0777)) < 0) {
        if (stm_verbose & 1)
            fprintf(stderr, "stm_open_shared_segment: could not open file %s: %s\n", seg->filename, strerror(errno));
        set_stm_errno(STM_OPEN_ERROR);
        seg->fd = 0;
        stm_close_shared_segment(seg);
        return NULL;
    }
    
    seg->shared_seg_size = segment_size;
    
    if (check_file_length(seg->fd, seg->shared_seg_size, &seg->inode) != 0) {
        stm_close_shared_segment(seg);
        return NULL;
    }
    
    seg->metadata_filename = calloc(1, strlen(filename) + strlen(metadata_suffix) + 1);
    strcpy(seg->metadata_filename, filename);
    strcat(seg->metadata_filename, metadata_suffix);
    
    seg->page_size = getpagesize();
    
    metadata_size = seg->page_size;
    while (metadata_size < sizeof(transaction_data))
        metadata_size += seg->page_size;
    seg->transaction_data_size = ((segment_size/seg->page_size) * sizeof(page_table_element)) + metadata_size;
    
    if ((seg->metadata_fd = open(seg->metadata_filename, O_RDWR|O_CREAT, 0777)) < 0) {
        if (stm_verbose & 1)
            fprintf(stderr, "stm_open_shared_segment: could not open metadata file %s: %s\n",
                            seg->filename, strerror(errno));
        set_stm_errno(STM_OPEN_ERROR);
        seg->metadata_fd = 0;
        stm_close_shared_segment(seg);
        return NULL;
    }
    
    if (check_file_length(seg->metadata_fd, seg->transaction_data_size, NULL)) {
        stm_close_shared_segment(seg);
        return NULL;
    }
    
    seg->default_prot_flags = prot_flags;
    

    mmap_flags = MAP_SHARED;
    
    
#undef KLUGE
#ifdef KLUGE
    mmap_flags = MAP_PRIVATE;
#endif
    
    if (requested_va != NULL)
        mmap_flags |= MAP_FIXED;
    
    status = mmap(requested_va, seg->shared_seg_size, seg->default_prot_flags, mmap_flags, seg->fd, (off_t)0);  
    
    if (status != (void*)-1) {
        seg->shared_base_va = status;
        //      fprintf(stderr, "shared base va = %x\n", status);
    } else {
        if (stm_verbose & 1)
            perror("stm_open_shared_segment: error mapping shared segment");
        set_stm_errno(STM_MMAP_ERROR);
        stm_close_shared_segment(seg);
        return NULL;
    }   
    
    status = mmap(0, seg->transaction_data_size, PROT_READ|PROT_WRITE, MAP_SHARED, seg->metadata_fd, (off_t)0); 
    
    if (status != (void*)-1) {
        seg->segment_transaction_data = (transaction_data *)status;
        seg->segment_page_table = (page_table_element*)((void*)seg->segment_transaction_data + metadata_size);
    } else {
        if (stm_verbose & 1)
                perror("stm_open_shared_segment: error mapping shared metadata segment");
        set_stm_errno(STM_MMAP_ERROR);
        stm_close_shared_segment(seg);
        return NULL;
    }
    
    // Don't link this onto the segment list until the end, so we don't have to undo it if there is an error
    // above.   And insert it into the segment list in ascending inode order.  Inodes should be unique and stable,
    // so each process using a set of mapped files will be able to list them in the same order, avoiding livelocks
    // during commit.
    
    for(s = shared_segment_list(), prev=NULL; s; prev = s, s = s->next) {
        if (seg->inode < s->inode) {
            break;
        }
    }
    
    seg->next = s;          // either item to insert before, or NULL if no list or we ran off end
    if (prev)
        prev->next = seg;
    else
        set_shared_segment_list(seg);
    
    
    
    
    return seg;
}




#define n_histo_buckets 9
int collision_histo[n_histo_buckets];

void print_collision_histo() {
    int i;
    printf("collision histogram:\n");
    for (i=0; i<n_histo_buckets; i++) {
        printf("%d\t\%d\n", i, collision_histo[i]);
    }
}




static void free_snapshot_list(shared_segment *seg) {   
    snapshot_list_element *sl;
    for ( ; seg->snapshot_list; seg->snapshot_list = sl) {
        sl = seg->snapshot_list->next;
        free(seg->snapshot_list->original_page_snapshot);
        free(seg->snapshot_list);       
    }
}


static void abort_transaction_on_segment(shared_segment *seg) {
    snapshot_list_element *sl;
    int page_num;
    page_table_element *page_table_elt;
    void *status;
    
    if (seg->transaction_id == 0) {
        if (stm_verbose & 2)
            fprintf(stderr, "Aborting transaction but transaction_id is already 0\n");
        return;
    }
    
    if (stm_verbose & 4)
        fprintf(stderr, "Aborting Transaction %d [", seg->transaction_id);
    
    delete_active_transaction(seg);
    
    for(sl = seg->snapshot_list; sl; sl = sl->next) {

        page_num = (sl->original_page_va - seg->shared_base_va)/seg->page_size;         
        page_table_elt = &(seg->segment_page_table[page_num]);
        
        if (stm_verbose & 4) {
            int dirty = memcmp(sl->original_page_va, sl->original_page_snapshot, seg->page_size);
            fprintf(stderr, " %s%x", dirty? "*":"", page_num);          
        }                   
        
        if (page_table_elt->current_transaction == seg->transaction_id) {
            // Only release pages owned by this transaction!
            // Pages that were only read and not modified by this transaction will not be marked as
            // associated with this transaction under optimistic locking. They may even be
            // associated with another transaction.
            
            page_table_elt->current_transaction = 0;
        }
        
    }
    
    if (stm_verbose & 4)
        fprintf(stderr, " ]\n");
    
    free_snapshot_list(seg);
        
    // reprotect *all* pages with the default inter-transaction protection.
    
    status = mmap(seg->shared_base_va, seg->shared_seg_size, seg->default_prot_flags, MAP_FIXED|MAP_SHARED, seg->fd, 
                    (off_t)0);
    if (status == (void*)-1)
        perror("abort_transaction_on_segment: mmap error");
    
    seg->transaction_id = 0;    
            
}



int _stm_transaction_stack_empty() {
    return (transaction_stack() == NULL);
}

static int push_transaction_stack(char *trans_name) {
    transaction_stack_element *trans;
    
    
    if ((trans = calloc(1, sizeof(transaction_stack_element))) == NULL) {
        set_stm_errno(STM_ALLOC_ERROR);
        return -1;
    }
    
#if 0
    if (trans_name) {
        if ((trans->transaction_name = calloc(1, strlen(trans_name)+1)) == null) {
            set_stm_errno(STM_ALLOC_ERROR);
            free(trans);
            return -1;
        }
        strcpy(trans->transaction_name, trans_name);
    }
#endif
    
    trans->transaction_name = trans_name;
    trans->next = transaction_stack();
    set_transaction_stack(trans);
    
#if 0
    printf("> ");
    for (trans = transaction_stack(); trans; trans = trans->next)
        printf("%s ", trans->transaction_name);
    printf("\n");
#endif
    
    return 0;
}


static void pop_transaction_stack() {
    transaction_stack_element *trans;
    
#if 0
    printf("< ");
    for (trans = transaction_stack(); trans; trans = trans->next)
        printf("%s ", trans->transaction_name);
    printf("\n");
#endif

    trans = transaction_stack();
    if (trans) {
        //  if (trans->transaction_name) free(trans->transaction_name);
        set_transaction_stack(trans->next);
        free(trans);
    }
}

static void stm_abort_transaction() {
    shared_segment *seg;
    
    for(seg = shared_segment_list(); seg; seg = seg->next) {
        abort_transaction_on_segment(seg);
    }
    while (transaction_stack())
        pop_transaction_stack();
    
}

static void transaction_error_exit(int error_code, int return_value) {
    if (error_code)
        set_stm_errno(error_code);
    stm_abort_transaction();
    longjmp(*stm_jmp_buf(), return_value);
    
}



static int insert_into_snapshot_list(shared_segment *seg, void *va, transaction_id_t trans_id) {
    
    snapshot_list_element *new_elt, *sl, *prev;
    
    //  fprintf(stderr, "inserting into snapshot list %x\n", va);
    
    if (va < seg->shared_base_va || seg->shared_base_va + seg->shared_seg_size <= va) {
        if (stm_verbose & 1)
            fprintf(stderr, "insert_into_snapshot_list: va %lx not in segment\n", (unsigned long)va);
        set_stm_errno(STM_ACCESS_ERROR);
        return -1;
    }
    
    
    if ((new_elt = calloc(1, sizeof(snapshot_list_element))) == NULL) {
        set_stm_errno(STM_ALLOC_ERROR);
        return -1;
    }
    
    new_elt->original_page_va = va;
    if ((new_elt->original_page_snapshot = malloc(seg->page_size)) == NULL) {
        set_stm_errno(STM_ALLOC_ERROR);
        return -1;
    }
    
    new_elt->page_dirty = 0;
    
    new_elt->snapshot_transaction_id = trans_id;
    
    memcpy(new_elt->original_page_snapshot, va, seg->page_size);
    
    for(sl = seg->snapshot_list, prev=NULL; sl; prev = sl, sl = sl->next) {
        if (va < sl->original_page_va) {
            break;
        } else if (va == sl->original_page_va) {
            if (stm_verbose & 1)
                fprintf(stderr, "insert_into_snapshot_list: duplicate page at %lx\n", (unsigned long)va);
        }
    }
    
    new_elt->next = sl;     // either item to insert before, or NULL if no list or we ran off end
    if (prev)
        prev->next = new_elt;
    else
        seg->snapshot_list = new_elt;
    
    return 0;
    
}


static int defeat_optimizer(volatile int *foo) {
    return *foo;
}

shared_segment *stm_find_shared_segment(void *va) {
    shared_segment *seg;
    for (seg = shared_segment_list(); seg; seg = seg->next) {
        if (seg->shared_base_va <= va &&
            va < seg->shared_base_va + seg->shared_seg_size)
            return seg;
    }
    return NULL;
}

void **stm_free_list_addr(shared_segment *seg) {
    return seg->free_list_addr;
}

void stm_set_free_list_addr(shared_segment *seg, void **free_list_addr) {
    seg->free_list_addr = free_list_addr;
}

void *stm_segment_base(shared_segment *seg) {
    return seg->shared_base_va;
}

size_t stm_segment_size(shared_segment *seg) {
    return seg->shared_seg_size;
}

size_t stm_page_size(shared_segment *seg) {
    return seg->page_size;
}

int stm_segment_fd(shared_segment *seg) {
    return seg->fd;
}


// signal_handler is invoked when there is a read or write access to a shared segment during a transaction.
// It remaps the page accessed to be private, with read and write access allowed.  But it also makes a snapshot
// of the page before it is allowed to be modified.  This allows the commit mechanism to detect dirty pages
// that need to be written.

static void signal_handler(int sig, siginfo_t *si, void *foo) {
    void *page_base;    
    void *status;
    shared_segment *seg;
    page_table_element *page_table_elt;
    transaction_id_t completed_transaction;
    int page_num;   
    
    struct sigaction sa;
    
    sa.sa_flags = 0;
    sa.sa_mask = 0;
    sa.sa_handler = SIG_DFL;
    
    if (transaction_stack() == NULL) {
        if (stm_verbose & 1)
            fprintf(stderr, "signal_handler: virtual address %lx referenced outside transaction\n",
                    (unsigned long)si->si_addr);
        sigaction(SIGBUS, &sa, 0);
        transaction_error_exit(STM_ACCESS_ERROR, -1);       
        return;
    }
    
    seg = stm_find_shared_segment(si->si_addr);
        
    if (seg == NULL) {
        if (stm_verbose & 1)
            fprintf(stderr, "signal_handler: virtual address %lx not found in shared segment\n",
                    (unsigned long)si->si_addr);
        sigaction(SIGBUS, &sa, 0);
        transaction_error_exit(STM_ACCESS_ERROR, -1);               
        return;
    }
    
    if (seg->transaction_id == 0) {
        if (stm_verbose & 1)
            fprintf(stderr, "signal_handler:  signal received outside transaction\n");
        sigaction(SIGBUS, &sa, 0);
        transaction_error_exit(STM_ACCESS_ERROR, -1);               
    }
    
    page_base = (void*)((long)si->si_addr & ~(seg->page_size-1));       
    page_num = (page_base - seg->shared_base_va)/seg->page_size;
    page_table_elt = &(seg->segment_page_table[page_num]);
    completed_transaction = page_table_elt->completed_transaction;
    
#define OPTIMISTIC_LOCKING
    
#ifdef OPTIMISTIC_LOCKING
    
    if (page_table_elt->current_transaction != 0) {
        if (seg->transaction_id != page_table_elt->current_transaction) {
            
            if (stm_verbose & 2)
                fprintf(stderr, "Transaction %d owns page %x while transaction %d is snapshotting it.\n",
                        page_table_elt->current_transaction, page_num, seg->transaction_id);
            collision_histo[0]++;
            transaction_error_exit(STM_COLLISION_ERROR, 1);
            return;
        } else {
            if (stm_verbose & 1)
                fprintf(stderr, "Transaction %d already owns page %x\n", 
                        page_table_elt->current_transaction, page_num);
            transaction_error_exit(STM_OWNERSHIP_ERROR, -1);
        }
    }
    
#else
    
    if (atomic_compare_and_swap_32(0, seg->transaction_id,
                                   (int32_t*)&(page_table_elt->current_transaction))) {
        //              fprintf(stderr, "succeeded in locking page %x\n", page_num);
    } else {    
        if (stm_verbose & 2)
            fprintf(stderr,"Transaction %d owns page %x while transaction %d is snapshotting it.\n",
                    page_table_elt->current_transaction, page_num, seg->transaction_id);
        transaction_error_exit(STM_COLLISION_ERROR, 1); 
        return;
    }
    
#endif
    
    if ((int32_t)completed_transaction - (int32_t)seg->transaction_id > 0) {
        
        if (stm_verbose & 2)
            fprintf(stderr, "On page %x, current transaction %d is before page's completed transaction %d\n",
                    page_num, seg->transaction_id, completed_transaction);
        
        collision_histo[1]++;
        transaction_error_exit(STM_COLLISION_ERROR, 1); 
        return;
    }
    
    if (find_prior_active_transaction(seg, completed_transaction)) {
        if (stm_verbose & 2)
            fprintf(stderr, "On page %x, completed transaction %d was active when transaction %d started\n",
                    page_num, completed_transaction, seg->transaction_id);
        collision_histo[2]++;
        transaction_error_exit(STM_COLLISION_ERROR, 1); 
        return;
    }
    
        
    // Change from shared to private mapping, and make the page readable and writable.
    //
    status = mmap(page_base, seg->page_size, PROT_READ|PROT_WRITE, MAP_FIXED|MAP_PRIVATE, seg->fd,
                  (off_t)(page_base - seg->shared_base_va));
    
    if (status == (void*)-1) {
        if (stm_verbose & 1)
            perror("signal_handler: mmap error in sig handler");
        transaction_error_exit(STM_MMAP_ERROR, -1);
        return;
        
    }
    
    // Some systems evidently allow changes by other processes to be reflected in private mappings.
    // To prevent that (hopefully!) we modify the page (without really changing anything) to 
    // invoke the "copy-on-write" semantics and really make a private copy
    //
    *(volatile int*)page_base = defeat_optimizer((volatile int*)page_base);
    
    if (insert_into_snapshot_list(seg, page_base, completed_transaction) != 0) {
        transaction_error_exit(0, -1);
    }
    
    // Double check to make sure that during the above, nobody grabbed this page.
    
    if (page_table_elt->current_transaction != 0) {
        if (seg->transaction_id != page_table_elt->current_transaction) {
            
            if (stm_verbose & 2)
                fprintf(stderr, "Transaction %d owns page %x while transaction %d is snapshotting it. [2]\n",
                        page_table_elt->current_transaction, page_num, seg->transaction_id);
            collision_histo[3]++;
            transaction_error_exit(STM_COLLISION_ERROR, 1);
            return;
        } else {
#ifdef OPTIMISTIC_LOCKING
            if (stm_verbose & 1)
                fprintf(stderr, "Transaction %d already owns page %x [2]\n", 
                        page_table_elt->current_transaction, page_num);
            transaction_error_exit(STM_OWNERSHIP_ERROR, -1);
#endif
        }
    }
    
    if (completed_transaction != page_table_elt->completed_transaction) {
        if (stm_verbose & 2) {
            fprintf(stderr, "Transaction %d snuck in on transaction %d on page %x during snapshot\n", 
                    page_table_elt->completed_transaction, completed_transaction, page_num);
        }
        collision_histo[4]++;
        transaction_error_exit(STM_COLLISION_ERROR, 1); 
        return;
    }
    
    return;
}


static struct sigaction saved_sigaction;

int stm_init(int verbose) {
    
    int status; 
    struct sigaction sa;
    
    stm_verbose = verbose;
    set_stm_errno(0);
    
    sa.sa_flags = SA_SIGINFO;
    sa.sa_mask = 0;
    sa.sa_sigaction = signal_handler;
    
    if ((status = sigaction(SIGBUS, &sa, &saved_sigaction)) != 0) {     
        if (stm_verbose & 1)
            fprintf(stderr, "sigaction status = %d\n", status);
        set_stm_errno(STM_SIGNAL_ERROR);
    }
    
    create_thread_keys();
    stm_init_thread_locals();
    
    return status;
    
}

static int start_transaction_on_segment(shared_segment *seg) {
    int status;
        

    // There is a small interval between the time we allocate a transaction ID and the time we can register it as an active
    // transaction so other transactions can know of its existence.  So transaction startup has to be
    // single threaded at least up until we add our new transaction ID to the active transactions list.

    atomic_spin_lock_lock(&seg->segment_transaction_data->transaction_lock);

    if ((seg->transaction_id = atomic_increment_32((int32_t*)&seg->segment_transaction_data->transaction_counter)) == 0) {
        // unlikely we'll wrap, but if we do, skip 0.
        seg->transaction_id = atomic_increment_32((int32_t*)&seg->segment_transaction_data->transaction_counter);   
    }
    
    snapshot_active_transactions(seg);
    add_active_transaction(seg);

    atomic_spin_lock_unlock(&seg->segment_transaction_data->transaction_lock);
        
    status = mprotect(seg->shared_base_va, seg->shared_seg_size, PROT_NONE);
    // status = mprotect(seg->shared_base_va, seg->shared_seg_size, PROT_READ|PROT_WRITE);
    
    if (status == -1) {
        if (stm_verbose & 1)
            perror("start_transaction: mprotect error");
        set_stm_errno(STM_MMAP_ERROR);
        return -1;
    }
        
    return 0;
}

int _stm_start_transaction(char *trans_name) {
    shared_segment *seg;
    
        
    set_stm_errno(0);      // This is as good a place as any to re-initialize this error code to 0.
    
    if (trans_name == NULL) {
        if (stm_verbose & 1)
            fprintf(stderr, "stm_start_transaction: tried to start transaction with NULL name\n");
        transaction_error_exit(STM_NULL_NAME_ERROR, -1);    }
    

    if (transaction_stack() == NULL)
        for(seg = shared_segment_list(); seg; seg = seg->next) {
            if (start_transaction_on_segment(seg) != 0) {
                transaction_error_exit(0, -1);
            }
        }
    
    if (push_transaction_stack(trans_name) != 0)
        transaction_error_exit(0, -1);

    return 0;
}

// returns:
//  0 - success
// -1 - non-recoverable error
//  1 - collision error:  should retry aborted transaction
//
static int lock_segment_pages(shared_segment *seg) {
    snapshot_list_element *sl;
    unsigned long page_num;
    page_table_element *page_table_elt;
    
    if (seg->transaction_id == 0) {
        if (stm_verbose & 1)
            fprintf(stderr, "lock_segment_pages:  segment should have active transaction, but doesn't\n");
        return -1;
    }

    
    for (sl = seg->snapshot_list; sl; sl = sl->next) {
        
        page_num = (sl->original_page_va - seg->shared_base_va)/seg->page_size;         
        page_table_elt = &(seg->segment_page_table[page_num]);
        
        // even if this transaction is just reading a page, if any other transaction is writing into it,
        // or has written into it, that is enough to make us abort. In that case we know the information 
        // we are accessing is stale and therefore our results may be inconsistent with results of other transactions

        if (sl->snapshot_transaction_id != page_table_elt->completed_transaction) {
            
            if (stm_verbose & 2)
                fprintf(stderr, "lock_segment_pages: Transaction %d modified page %lx!\n",
                        page_table_elt->completed_transaction, page_num);
            collision_histo[5]++;
            set_stm_errno(STM_COLLISION_ERROR);
            return 1;
            
        }
        
        
        if (page_table_elt->current_transaction != 0 &&
            page_table_elt->current_transaction != seg->transaction_id) {
            if (stm_verbose & 2)
                fprintf(stderr, "lock_segment_pages: Transaction %d is modifying page %lx!\n",
                        page_table_elt->current_transaction, page_num);
            collision_histo[6]++;
            set_stm_errno(STM_COLLISION_ERROR);            
            return 1;
        }   
        
        if (memcmp(sl->original_page_snapshot, sl->original_page_va, seg->page_size) == 0)
            continue;
        
        sl->page_dirty = 1;

        // re-use the page snapshot buffer to temporarily keep a copy of the page so we can re-map the page as shared,
        // then copy the new contents into it.
        // *** If only there were page-remapping syscalls, I wouldn't have to do this copying - I could re-map the modified
        // page out of the way, then re-map it into the right location in the file.  Better yet, if there were a way to associate
        // a file region with a memory region (so the file is written into as opposed to read from) that would solve this.
        
        memcpy(sl->original_page_snapshot, sl->original_page_va, seg->page_size);   
        
        
#ifdef OPTIMISTIC_LOCKING
        
        if (atomic_compare_and_swap_32(0, seg->transaction_id,
                                       (int32_t*)&(page_table_elt->current_transaction))) {
            //              fprintf(stderr, "succeeded in locking page %x\n", page_num);
        } else {    
            if (stm_verbose & 2)
                fprintf(stderr, "lock_segment_pages: Race detected. Failed to lock page %lx\n", page_num);
            collision_histo[7]++;
            set_stm_errno(STM_COLLISION_ERROR);            
            return 1;
        }       
        
#endif
                
        if (page_table_elt->current_transaction != seg->transaction_id) {
            if (stm_verbose & 1) 
                fprintf(stderr, "lock_segment_pages:  page %lx should already be locked by transaction %d, but is owned by %d\n",
                        page_num, seg->transaction_id, page_table_elt->current_transaction);
            set_stm_errno(STM_OWNERSHIP_ERROR);
            return -1;
        }

        if (sl->snapshot_transaction_id != page_table_elt->completed_transaction) {
            
            if (stm_verbose & 2)
                fprintf(stderr, "lock_segment_pages: Transaction %d modified page %lx!\n",
                        page_table_elt->completed_transaction, page_num);
            collision_histo[8]++;
            set_stm_errno(STM_COLLISION_ERROR);
            return 1;           
        }       
    }
    
    return 0;
}


static int write_locked_segment_pages(shared_segment *seg) {
    
    snapshot_list_element *sl;
    void *status;
    unsigned long page_num;
    int result = 0;
    
    page_table_element *page_table_elt;
    
        
    // Re-map shared.
        
    status = mmap(seg->shared_base_va, seg->shared_seg_size, PROT_READ|PROT_WRITE, MAP_FIXED|MAP_SHARED, seg->fd,
                  (off_t)0);
    if (status == (void*)-1) {
        if (stm_verbose & 1)
            perror("write_locked_pages: mmap error");
        set_stm_errno(STM_MMAP_ERROR);
        return -1;
    }
    
    if (stm_verbose & 4)
        fprintf(stderr, "Transaction %d [", seg->transaction_id);
    
    // now copy the new versions of the dirty pages into the shared, mapped file.
    
    for (sl = seg->snapshot_list; sl; sl = sl->next) {
        
        page_num = (sl->original_page_va - seg->shared_base_va)/seg->page_size;     
        page_table_elt = &(seg->segment_page_table[page_num]);
        
        if (sl->page_dirty) {
                        
            if (stm_verbose & 4)
                fprintf(stderr, " %lx", page_num);

            page_table_elt->completed_transaction = seg->transaction_id;
            
            // copy the temporarily saved, modified pages back into the right places
            //
            memcpy(sl->original_page_va, sl->original_page_snapshot, seg->page_size);
            
        }
                
        if (page_table_elt->current_transaction == seg->transaction_id) {
            // Only release pages owned by this transaction!
            // Pages that were only read and not modified by this transaction will not be marked as
            // associated with this transaction under optimistic locking. They may even be
            // associated with another transaction.
            
            page_table_elt->current_transaction = 0;    
        }
        
    }
    
    if (stm_verbose & 4)
        fprintf(stderr, " ]\n");

    // re-protect the segment to be whatever it is supposed to be between transactions

    if (seg->default_prot_flags != (PROT_READ|PROT_WRITE))
        mprotect(seg->shared_base_va, seg->shared_seg_size, seg->default_prot_flags);
    
    free_snapshot_list(seg);
    
    delete_active_transaction(seg);
    seg->transaction_id = 0;
    
    return result;
    
}


int stm_commit_transaction(char *trans_name) {
    shared_segment *seg;
    int result = 0;
    
    set_stm_errno(0);
    
    if (transaction_stack() == NULL) {
        if (stm_verbose & 1)
            fprintf(stderr, "stm_commit_transaction:  empty transaction stack while trying to commit transaction \"%s\"\n",
                    trans_name);
        transaction_error_exit(STM_TRANS_STACK_ERROR, -1);
    }   
    
    if (trans_name == NULL) {
        if (stm_verbose & 1)
            fprintf(stderr, "stm_commit_transaction:  null transaction name\n");
        transaction_error_exit(STM_NULL_NAME_ERROR, -1);
    }
    
    if (strcmp(transaction_stack()->transaction_name, trans_name) != 0) { 
        if (stm_verbose & 1)
            fprintf(stderr, "stm_commit_transaction: \"%s\" is not the innermost transaction (\"%s\" is)\n",
                    trans_name, transaction_stack()->transaction_name);
        
        transaction_error_exit(STM_TRANS_STACK_ERROR, -1);
    }
    
    if (transaction_stack()->next == NULL) {
        // only actually commit on outermost transaction.
        
        sigset_t blocked_signals;
        sigset_t saved_signals;
        
        // We don't want to be interrupted or anything during the commit of the transaction.
        sigfillset(&blocked_signals);
        
        // if (sigprocmask(SIG_SETMASK, &blocked_signals, &saved_signals) == -1) {
        if (pthread_sigmask(SIG_SETMASK, &blocked_signals, &saved_signals) == -1) {
            if (stm_verbose & 1)
                perror("stm_commit_transaction: error blocking signals");
            transaction_error_exit(STM_SIGNAL_ERROR, -1);
        }
        

        for(seg = shared_segment_list(); seg; seg = seg->next) {
            if ((result = lock_segment_pages(seg)) != 0) {
                // if there is a failure on any shared segment, abort on all segments
                transaction_error_exit(0, result);
            }
        }
        
        
        
        for(seg = shared_segment_list(); seg; seg = seg->next) {
            if ((result = write_locked_segment_pages(seg)) != 0) {
                // if there is a failure on any shared segment, abort on all segments
                transaction_error_exit(0, result);
            }
        }
        
        // At this point, it's really too late to reverse anything...
        
        // if (sigprocmask(SIG_SETMASK, &saved_signals, NULL) == -1) {
        if (pthread_sigmask(SIG_SETMASK, &saved_signals, NULL) == -1) {
            if (stm_verbose & 1)
                perror("stm_commit_transaction: error unblocking signals");
            set_stm_errno(STM_SIGNAL_ERROR);
            result = -1;
        }                       
    }
    
    pop_transaction_stack();
    
    return result;
    
}

void stm_close_shared_segment(shared_segment *seg) {
    shared_segment *s, *prev;
    
    if (seg->transaction_id)
        abort_transaction_on_segment(seg);
    
    if (seg->shared_base_va)
        munmap(seg->shared_base_va, seg->shared_seg_size);
    
    if (seg->segment_transaction_data)
        munmap(seg->segment_transaction_data, seg->transaction_data_size);
    
    if (seg->fd)
        close(seg->fd);
    
    if (seg->metadata_fd)
        close(seg->metadata_fd);
    
    for (s = shared_segment_list(), prev=NULL; s; prev = s, s=s->next) {
        if (s == seg) {
            if (prev)
                prev->next = s->next;
            else
                set_shared_segment_list(s->next);
            break;
        }
    }
    
    if (seg->filename) free(seg->filename);
    if (seg->metadata_filename) free(seg->metadata_filename);
    free(seg);
}

void stm_close()
{
    shared_segment *s;
    while ((s = shared_segment_list()) != NULL)
        stm_close_shared_segment(s);
    sigaction(SIGBUS, &saved_sigaction, 0);
}




