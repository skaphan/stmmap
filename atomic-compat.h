/*
 *  atomic-compat.h
 *  stmmap
 *
 *  Created by Shel Kaphan on 10/8/09.
 *
 */

// This is a compatibility package for whatever OS-supplied atomic operators are to be found on your
// platform.  It was originally implemented using MAC OS X  so the API is very similar to that.
// This file and atomics.c can be conditionalized to support additional OS versions.
//


#include <libkern/OSAtomic.h>

typedef OSSpinLock atomic_lock;

void atomic_spin_lock_lock(atomic_lock *lock);

void atomic_spin_lock_unlock(atomic_lock *lock);

int32_t atomic_increment_32(int32_t *addr);

int32_t atomic_decrement_32(int32_t *addr);

int32_t	atomic_compare_and_swap_32(int32_t oldval, int32_t newval, int32_t *addr);




