/*
 
 atomic-compat.c
 
 This is a compatibility package for whatever OS-supplied atomic operators are to be found on your
 platform.  It was originally implemented using MAC OS X  so the API is very similar to that.
 This file and atomics.c can be conditionalized to support additional OS versions.
 
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


#include "atomic-compat.h"



int32_t atomic_increment_32(int32_t *addr) {    
    return OSAtomicIncrement32Barrier(addr);    
}

int32_t atomic_decrement_32(int32_t *addr) {    
    return OSAtomicDecrement32Barrier(addr);    
}

int32_t atomic_compare_and_swap_32(int32_t oldval, int32_t newval, int32_t *addr) { 
    return OSAtomicCompareAndSwap32Barrier(oldval, newval, addr);   
}


void atomic_spin_lock_lock(atomic_lock *lock) {
    OSSpinLockLock(lock);
}

void atomic_spin_lock_unlock(atomic_lock *lock) {
    OSSpinLockUnlock(lock);
}
