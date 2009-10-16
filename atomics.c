/*
 *  atomics.c
 *  stmtest
 *
 *  Created by Shel Kaphan on 10/8/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */



#include "atomics.h"



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
