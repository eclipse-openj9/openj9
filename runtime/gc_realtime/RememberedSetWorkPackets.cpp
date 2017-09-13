
/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/


#include "modron.h"

#include "Debug.hpp"
#include "RealtimeGC.hpp"
#include "RememberedSetWorkPackets.hpp"
#include "WorkPackets.hpp"

/**
 * Object creation and destruction 
 *
 */

/**
 * Create a new instance the MM_RememberedSetWorkPackets class
 *
 * @param workPackets The workPackets 
 */
MM_RememberedSetWorkPackets *
MM_RememberedSetWorkPackets::newInstance(MM_EnvironmentBase *env, MM_WorkPacketsRealtime *workPackets)
{
	MM_RememberedSetWorkPackets *rememberedSet;
	
	rememberedSet = (MM_RememberedSetWorkPackets *)env->getForge()->allocate(sizeof(MM_RememberedSetWorkPackets), MM_AllocationCategory::WORK_PACKETS, J9_GET_CALLSITE());
	if (NULL != rememberedSet) {
		new(rememberedSet) MM_RememberedSetWorkPackets(env, workPackets);
		if (!rememberedSet->initialize(env)) {
			rememberedSet->kill(env);
			rememberedSet = NULL;
		}
	}
	return rememberedSet;
}

/**
 * Kill the RememberedSetWorkPackets instance
 */
void
MM_RememberedSetWorkPackets::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Initialize the MM_RememberedSetWorkPackets class.
 */
bool
MM_RememberedSetWorkPackets::initialize(MM_EnvironmentBase *env)
{
	return true;
}

/**
 * Teardown the MM_RememberedSetWorkPackets class.
 */
void
MM_RememberedSetWorkPackets::tearDown(MM_EnvironmentBase *env)
{	
}

/**
 * Initialize a fragment to a "null" state such that the first store into it will cause a
 * fragment refresh.
 * @param fragment The fragment to initialize.
 */
void
MM_RememberedSetWorkPackets::initializeFragment(MM_EnvironmentBase* env, J9VMGCRememberedSetFragment* fragment)
{
	fragment->fragmentAlloc = NULL;
	fragment->fragmentTop = NULL;
	fragment->fragmentStorage = NULL;
	
	/* The initial values of the following fields were chosen to ensure the local fragment
	 * index isn't initialized to the J9GC_REMEMBERED_SET_RESERVED_INDEX, since depending
	 * on when the fragment is initialized, it could be interpreted as meaning the double
	 * barrier is active, which isn't the case. Other than that, there is no requirement
	 * for the initial value of these fields. Eg: If the initial values happen to
	 * correspond to the global index, this isn't a problem since the fragment won't be
	 * used because it is considered full and of size 0. 
	 */
	fragment->localFragmentIndex = (J9GC_REMEMBERED_SET_RESERVED_INDEX + 1);
	fragment->preservedLocalFragmentIndex = (J9GC_REMEMBERED_SET_RESERVED_INDEX + 1);
	fragment->fragmentParent = &_rememberedSetStruct;
}

/**
 * Stores a value in the alloc position of the fragment and increments the alloc pointer.
 * @param fragment The fragment in which the value should be stored.
 * @param value The value to store in the fragment. 
 */
void
MM_RememberedSetWorkPackets::storeInFragment(MM_EnvironmentBase* env, J9VMGCRememberedSetFragment* fragment, UDATA* value)
{
	if (!isFragmentValid(env, fragment)) {
		if (!refreshFragment(env, fragment)) {
			_workPackets->overflowItem(env, (void *)value, OVERFLOW_TYPE_BARRIER);
			return;
		}
	}
	
	assume(isFragmentValid(env, fragment), "Refreshed fragment invalid.");
	*(*(fragment->fragmentAlloc)) = (UDATA) value;
	(*(fragment->fragmentAlloc))++;
}

/**
 * Determines if the fragment is valid or not. A valid fragment is defined as a non-full
 * fragment with a local fragment ID that matches the global fragment ID.
 * @param fragment The fragment to validate. 
 */
bool
MM_RememberedSetWorkPackets::isFragmentValid(MM_EnvironmentBase* env, const J9VMGCRememberedSetFragment* fragment)
{
	if (fragment->fragmentStorage == NULL) {
		return false;
	}
	if (*fragment->fragmentAlloc == *fragment->fragmentTop) {
		return false;
	}
	return (getLocalFragmentIndex(env, fragment) == getGlobalFragmentIndex(env));
}

/**
 * Saves the local fragment index but ensures any inline JIT code that uses the fragment
 * will see a difference in the fragment indexes and force the JIT to go out-of-line.
 * @param fragment The fragment to preserve the index for.
 */
void
MM_RememberedSetWorkPackets::preserveLocalFragmentIndex(MM_EnvironmentBase* env, J9VMGCRememberedSetFragment* fragment)
{
	assume((fragment->localFragmentIndex != J9GC_REMEMBERED_SET_RESERVED_INDEX), "Attempt to preserve an already preserved fragment index.");
	fragment->preservedLocalFragmentIndex = fragment->localFragmentIndex;
	fragment->localFragmentIndex = J9GC_REMEMBERED_SET_RESERVED_INDEX;
}

/**
 * Restores the localFragmentIndex such that JIT code may use the fragment directly.
 * @param fragment The fragment to restore.
 */
void
MM_RememberedSetWorkPackets::restoreLocalFragmentIndex(MM_EnvironmentBase* env, J9VMGCRememberedSetFragment* fragment)
{
	assume((fragment->localFragmentIndex == J9GC_REMEMBERED_SET_RESERVED_INDEX), "Attempt to restore a non-preserved fragment index.");
	fragment->localFragmentIndex = fragment->preservedLocalFragmentIndex;
}

/**
 * Saves the global fragment index but ensures any inline JIT code that uses any fragment
 * will see a difference in the fragment indexes and force the JIT to go out-of-line.
 */
void
MM_RememberedSetWorkPackets::preserveGlobalFragmentIndex(MM_EnvironmentBase* env)
{
	assume((_rememberedSetStruct.globalFragmentIndex != J9GC_REMEMBERED_SET_RESERVED_INDEX), "Attempt to preserve an already preserved global index.");
	_rememberedSetStruct.preservedGlobalFragmentIndex = _rememberedSetStruct.globalFragmentIndex;
	_rememberedSetStruct.globalFragmentIndex = J9GC_REMEMBERED_SET_RESERVED_INDEX;
}

/**
 * Restores the global fragment index such that JIT inline code may use the fragments directly.
 */
void
MM_RememberedSetWorkPackets::restoreGlobalFragmentIndex(MM_EnvironmentBase* env)
{
	assume((_rememberedSetStruct.globalFragmentIndex == J9GC_REMEMBERED_SET_RESERVED_INDEX), "Attempt to restore a non-preserved global index.");
	_rememberedSetStruct.globalFragmentIndex = _rememberedSetStruct.preservedGlobalFragmentIndex;
}

/**
 * @return the actual value corresponding to the fragment index, preserved or not.
 */
UDATA
MM_RememberedSetWorkPackets::getLocalFragmentIndex(MM_EnvironmentBase* env, const J9VMGCRememberedSetFragment* fragment)
{
	/* There should be no synchronization required based on the following assumptions:
	 * 1) The thread starting the GC will call preserveLocalFragmentIndex on all threads "atomically".
	 * 2) Any other write to the fragment will be done by the thread owning the fragment.
	 * 3) All fragment reads are done by the thread owning the fragment. 
	 */
	UDATA localIndex = fragment->localFragmentIndex;
	if (J9GC_REMEMBERED_SET_RESERVED_INDEX == localIndex) {
		return fragment->preservedLocalFragmentIndex;
	}
	return fragment->localFragmentIndex;
}

/**
 * @return the actual value corresponding to the global index, preserved or not.
 */
UDATA
MM_RememberedSetWorkPackets::getGlobalFragmentIndex(MM_EnvironmentBase* env)
{
	/* There should be no synchronization required based on the following assumptions:
	 * 1) The global fragment index is modified by the thread that iterates over the remembered set
	 *    and the thread that completes the GC cycle, but there will be a call to the ragged barrier
	 *    between those 2 events.
	 * 2) Reading an out of date global ID in a thread is safe until the ragged barrier is notified
	 *    that the particular thread has hit the barrier.
	 */
	UDATA globalIndex = _rememberedSetStruct.globalFragmentIndex;
	if (J9GC_REMEMBERED_SET_RESERVED_INDEX == globalIndex) {
		return _rememberedSetStruct.preservedGlobalFragmentIndex;
	}
	return globalIndex;
}

/**
 * Increments the global fragment index such that all fragments will be refreshed before
 * storing into them.
 * 
 * This method assumes external synchronization will be used to ensure all threads have
 * noticed their caches have been flushed. Ie: it's the callers responsibility to call
 * the ragged barrier after calling this method.
 */
void
MM_RememberedSetWorkPackets::flushFragments(MM_EnvironmentBase* env)
{
	/* If the next index corresponds to the reserved index, skip over it. */
	UDATA nextIndex = (getGlobalFragmentIndex(env) + 1);
	if (J9GC_REMEMBERED_SET_RESERVED_INDEX != nextIndex) {
		setGlobalIndex(env, nextIndex);
	} else {
		setGlobalIndex(env, nextIndex + 1);
	}
}

/**
 * Sets the appropriate global index depending on whether or not the global index
 * is preserved.
 * @param indexValue The new value the global index should take.
 */
void
MM_RememberedSetWorkPackets::setGlobalIndex(MM_EnvironmentBase* env, UDATA indexValue)
{
	if (J9GC_REMEMBERED_SET_RESERVED_INDEX == _rememberedSetStruct.globalFragmentIndex) {
		_rememberedSetStruct.preservedGlobalFragmentIndex = indexValue;
	} else {
		_rememberedSetStruct.globalFragmentIndex = indexValue;
	} 
}

/**
 * Refresh the fragment.
 * 
 * @Note that the refresh fragment mustn't blindly update the localFragmentIndex, 
 * it must determine which of the localFragmentFlushID or preservedFragmentFlushID 
 * is to be updated.
 */
bool
MM_RememberedSetWorkPackets::refreshFragment(MM_EnvironmentBase *env, J9VMGCRememberedSetFragment* fragment)
{
	MM_Packet *packet = NULL;
	bool result = false;
	
	packet = _workPackets->getBarrierPacket(env);
	MM_Packet *oldPacket = (MM_Packet *)fragment->fragmentStorage;
		
	if ((NULL != oldPacket) && (getLocalFragmentIndex(env, fragment) == getGlobalFragmentIndex(env)) && (*fragment->fragmentTop == *fragment->fragmentAlloc)) {
		_workPackets->removePacketFromInUseList(env, oldPacket);
		_workPackets->putFullPacket(env, oldPacket);
	}
	
	if (J9GC_REMEMBERED_SET_RESERVED_INDEX == fragment->localFragmentIndex) {
		fragment->preservedLocalFragmentIndex = getGlobalFragmentIndex(env);
	} else {
		fragment->localFragmentIndex = getGlobalFragmentIndex(env);
	}
    fragment->fragmentParent = &_rememberedSetStruct;
	
	if (NULL != packet) {
		fragment->fragmentAlloc = packet->getCurrentAddr(env);
		fragment->fragmentTop = packet->getTopAddr(env);
		fragment->fragmentStorage = (void *)packet;
	    
	    _workPackets->putInUsePacket(env, packet);
	    
	    result = true;
	} else {
		fragment->fragmentAlloc = NULL;
		fragment->fragmentTop = NULL;
		fragment->fragmentStorage = NULL;
	}
	
	return result;
}

