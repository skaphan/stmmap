/*
 *  atomic-compat.c
 *  stmmap
 *
 *  Created by Shel Kaphan on 10/8/09.
 *
 *  This file will have to be ported to work with atomic primitives as supplied
 *  by different operating system versions.   This was originally coded to work
 *  with MacOS 10.6.
 *
 */



#include "atomic-compat.h"



int32_t atomic_increment_32(int32_t *addr) {	
	return OSAtomicIncrement32Barrier(addr);	
}

int32_t atomic_decrement_32(int32_t *addr) {	
	return OSAtomicDecrement32Barrier(addr);	
}

int32_t	atomic_compare_and_swap_32(int32_t oldval, int32_t newval, int32_t *addr) {	
	return OSAtomicCompareAndSwap32Barrier(oldval, newval, addr);	
}


void atomic_spin_lock_lock(atomic_lock *lock) {
	OSSpinLockLock(lock);
}

void atomic_spin_lock_unlock(atomic_lock *lock) {
	OSSpinLockUnlock(lock);
}
