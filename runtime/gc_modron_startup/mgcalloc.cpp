/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
 
 /**
 * @file
 * @ingroup GC_Modron_Startup
 */

#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "modronopt.h"
#include "ModronAssertions.h"
#include "omrgc.h"

#include <string.h>

#include "rommeth.h"

#include "modronapi.hpp"

#include "AllocateDescription.hpp"
#include "AtomicOperations.hpp"
#include "EnvironmentBase.hpp"
#include "IndexableObjectAllocationModel.hpp"
#include "mmhook_internal.h"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "MixedObjectAllocationModel.hpp"
#include "ObjectAccessBarrier.hpp"
#include "ObjectAllocationInterface.hpp"
#include "ObjectModel.hpp"

#if defined (J9VM_GC_REALTIME)
#include "Scheduler.hpp"
#endif /* J9VM_GC_REALTIME */
#include "GlobalCollector.hpp"
#include "VMAccess.hpp"

extern "C" {

static uintptr_t stackIterator(J9VMThread *currentThread, J9StackWalkState *walkState);
static void dumpStackFrames(J9VMThread *currentThread);
static void traceAllocateIndexableObject(J9VMThread *vmThread, J9Class* clazz, uintptr_t objSize, uintptr_t numberOfIndexedFields);
static J9Object * traceAllocateObject(J9VMThread *vmThread, J9Object * object, J9Class* clazz, uintptr_t objSize, uintptr_t numberOfIndexedFields=0);
static bool traceObjectCheck(J9VMThread *vmThread);

#define STACK_FRAMES_TO_DUMP	8

/**
 * High level fast path allocate routine (used by VM and JIT) to allocate a single object.  This method does not need to be called with
 * a resolve frame as it cannot cause a GC.  If the attempt at allocation fails, the method will return null and it is the caller's 
 * responsibility to call through to the "slow path" J9AllocateObject function after setting up a resolve frame.
 * NOTE:  This function can only be called for instrumentable allocates!
 * 
 * @param vmThread The thread requesting the allocation
 * @param clazz The class of the object to be allocated
 * @param allocateFlags a bitfield of flags from the following
 *	OMR_GC_ALLOCATE_OBJECT_TENURED forced Old space allocation even if Generational Heap
 *	OMR_GC_ALLOCATE_OBJECT_INSTRUMENTABLE set if this allocate was an instrumentable allocate
 *	OMR_GC_ALLOCATE_OBJECT_HASHED set if this allocation initializes hash slot
 *	OMR_GC_ALLOCATE_OBJECT_NO_GC NOTE: this will be set unconditionally for this call
 * @return Pointer to the object header, or NULL
 */
J9Object *
J9AllocateObjectNoGC(J9VMThread *vmThread, J9Class *clazz, uintptr_t allocateFlags)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);

#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	if (extensions->instrumentableAllocateHookEnabled || !env->isInlineTLHAllocateEnabled()) {
		/* This function is restricted to only being used for instrumentable allocates so we only need to check that one allocation hook.
		 * Note that we can't handle hooked allocates since we might be called without a JIT resolve frame and that is required for us to
		 * report the allocation event.
		 */
		return NULL;
	}
#endif /* J9VM_GC_THREAD_LOCAL_HEAP */

	Assert_MM_true(allocateFlags & OMR_GC_ALLOCATE_OBJECT_INSTRUMENTABLE);
	// TODO: respect or reject tenured flag?
	Assert_MM_false(allocateFlags & OMR_GC_ALLOCATE_OBJECT_TENURED);
	Assert_MM_false(allocateFlags & OMR_GC_ALLOCATE_OBJECT_NON_ZERO_TLH);

	J9Object *objectPtr = NULL;
	
	if(!traceObjectCheck(vmThread)){
		allocateFlags |= OMR_GC_ALLOCATE_OBJECT_NO_GC;
		MM_MixedObjectAllocationModel mixedOAM(env, clazz, allocateFlags);
		if (mixedOAM.initializeAllocateDescription(env)) {
			env->_isInNoGCAllocationCall = true;
			objectPtr = OMR_GC_AllocateObject(vmThread->omrVMThread, &mixedOAM);
			if (NULL != objectPtr) {
				uintptr_t allocatedBytes = env->getExtensions()->objectModel.getConsumedSizeInBytesWithHeader(objectPtr);
				Assert_MM_true(allocatedBytes == mixedOAM.getAllocateDescription()->getContiguousBytes());
				if (J9_ARE_ANY_BITS_SET(J9CLASS_EXTENDED_FLAGS(clazz), J9ClassReservableLockWordInit)) {
					*J9OBJECT_MONITOR_EA(vmThread, objectPtr) = OBJECT_HEADER_LOCK_RESERVED;
				}
			}
			env->_isInNoGCAllocationCall = false;
		}
	}

	return objectPtr;
}

static uintptr_t
stackIterator(J9VMThread *currentThread, J9StackWalkState *walkState)
{
	if (NULL != walkState) {
		J9Method *method = walkState->method;
		const char *mc = "Missing_class";
		const char *mm = "Missing_method";
		const char *ms = "(Missing_signature)";
		U_16 mc_size = (U_16)strlen(mc);
		U_16 mm_size = (U_16)strlen(mm);
		U_16 ms_size = (U_16)strlen(ms);

#ifdef J9VM_INTERP_NATIVE_SUPPORT
		void *jit = walkState->jitInfo;
#else
		void *jit = NULL;
#endif
		
		if (NULL != method) {
			J9Class *methodClass = J9_CLASS_FROM_METHOD(method);
			J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
			
			if (NULL != methodClass) {
				J9UTF8 *className = J9ROMCLASS_CLASSNAME(methodClass->romClass);
				
				if (NULL != className) {
					mc_size = J9UTF8_LENGTH(className);
					mc = (char *)J9UTF8_DATA(className);
				}
			}

			if (NULL != romMethod) {
				J9UTF8 *methodName = J9ROMMETHOD_NAME(romMethod);
				J9UTF8 *methodSignature = J9ROMMETHOD_SIGNATURE(romMethod);

				if (NULL != methodName) {
					mm_size = J9UTF8_LENGTH(methodName);
					mm = (char *)J9UTF8_DATA(methodName);
				}
			
				if (NULL != methodSignature) {
					ms_size = J9UTF8_LENGTH(methodSignature);
					ms = (char *)J9UTF8_DATA(methodSignature);
				}
			}
		}
		Trc_MM_MethodSampleContinue(currentThread, method, mc_size, mc, mm_size, mm, ms_size, ms, jit, walkState->pc);
	}
	return J9_STACKWALK_KEEP_ITERATING;
}

static void
dumpStackFrames(J9VMThread *currentThread)
{
	if (TrcEnabled_Trc_MM_MethodSampleContinue) {
	
		if (NULL != currentThread) {
			J9StackWalkState walkState;
	
			walkState.skipCount = 0;
			walkState.maxFrames = STACK_FRAMES_TO_DUMP;
			walkState.frameWalkFunction = stackIterator;
			walkState.walkThread = currentThread;
			walkState.flags = J9_STACKWALK_ITERATE_FRAMES |
			                  J9_STACKWALK_VISIBLE_ONLY   |
					          J9_STACKWALK_INCLUDE_NATIVES;
			currentThread->javaVM->walkStackFrames(currentThread, &walkState);
		}
	}
}

static void
traceAllocateIndexableObject(J9VMThread *vmThread, J9Class* clazz, uintptr_t objSize, uintptr_t numberOfIndexedFields)
{
	J9ArrayClass* arrayClass = (J9ArrayClass*) clazz;
	uintptr_t arity = arrayClass->arity;
	J9UTF8* utf;
	/* Max arity is 255, so define a bracket array of size 256*2 */
	static const char * brackets =
		"[][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][]"
		"[][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][]"
		"[][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][]"
		"[][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][]";
	

	utf = J9ROMCLASS_CLASSNAME(arrayClass->leafComponentType->romClass);

	Trc_MM_J9AllocateIndexableObject_outOfLineObjectAllocation(vmThread, clazz, J9UTF8_LENGTH(utf), J9UTF8_DATA(utf), arity*2, brackets, objSize, numberOfIndexedFields);
	return;
}

static J9Object *
traceAllocateObject(J9VMThread *vmThread, J9Object * object, J9Class* clazz, uintptr_t objSize, uintptr_t numberOfIndexedFields)
{
	if(traceObjectCheck(vmThread)){
		PORT_ACCESS_FROM_VMC(vmThread);
		MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
		MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
		uintptr_t byteGranularity = extensions->oolObjectSamplingBytesGranularity;
		J9ROMClass *romClass = clazz->romClass;
	
		if (J9ROMCLASS_IS_ARRAY(romClass)){
			traceAllocateIndexableObject(vmThread, clazz, objSize, numberOfIndexedFields);
		}else{
			Trc_MM_J9AllocateObject_outOfLineObjectAllocation(
				vmThread, clazz, J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass)), objSize);
		}

		TRIGGER_J9HOOK_MM_OBJECT_ALLOCATION_SAMPLING(
			extensions->hookInterface,
			vmThread,
			j9time_hires_clock(),
			J9HOOK_MM_OBJECT_ALLOCATION_SAMPLING,
			object,
			clazz,
			objSize);

		/* Keep the remainder, want this to happen so that we don't miss objects
		 * after seeing large objects
		 */
		env->_oolTraceAllocationBytes = (env->_oolTraceAllocationBytes) % byteGranularity;
	}
	return object;
}

/* Required to check if we're going to trace or not since a java stack trace needs
 * stack frames built up; therefore we can't be in the noGC version of allocates
 *
 * Returns true if we should trace the object
 *  */
static bool
traceObjectCheck(J9VMThread *vmThread)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);

	if(extensions->doOutOfLineAllocationTrace){
		uintptr_t byteGranularity = extensions->oolObjectSamplingBytesGranularity;
	
		if(env->_oolTraceAllocationBytes >= byteGranularity){
			return true;
		}
	}
	return false;
}

/**
 * High level fast path allocate routine (used by VM and JIT) to allocate an indexable object.  This method does not need to be called with
 * a resolve frame as it cannot cause a GC.  If the attempt at allocation fails, the method will return null and it is the caller's 
 * responsibility to call through to the "slow path" J9AllocateIndexableObject function after setting up a resolve frame.
 * NOTE:  This function can only be called for instrumentable allocates!
 * 
 * @param vmThread The thread requesting the allocation
 * @param clazz The class of the object to be allocated
 * @param numberOfIndexedFields The number of indexable fields required in the allocated object
 * @param allocateFlags a bitfield of flags from the following
 *	OMR_GC_ALLOCATE_OBJECT_TENURED forced Old space allocation even if Generational Heap
 *	OMR_GC_ALLOCATE_OBJECT_INSTRUMENTABLE set if this allocate was an instrumentable allocate
 *	OMR_GC_ALLOCATE_OBJECT_HASHED set if this allocation initializes hash slot
 *	OMR_GC_ALLOCATE_OBJECT_NO_GC NOTE: this will be set unconditionally for this call
 * @return Pointer to the object header, or NULL
 */
J9Object *
J9AllocateIndexableObjectNoGC(J9VMThread *vmThread, J9Class *clazz, uint32_t numberOfIndexedFields, uintptr_t allocateFlags)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
	
#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	if (extensions->instrumentableAllocateHookEnabled || !env->isInlineTLHAllocateEnabled()) {
		/* This function is restricted to only being used for instrumentable allocates so we only need to check that one allocation hook.
		 * Note that we can't handle hooked allocates since we might be called without a JIT resolve frame and that is required for us to
		 * report the allocation event.
		 */
		return NULL;
	}
#endif /* J9VM_GC_THREAD_LOCAL_HEAP */

	Assert_MM_true(allocateFlags & OMR_GC_ALLOCATE_OBJECT_INSTRUMENTABLE);
	// TODO: respect or reject tenured flag?
	Assert_MM_false(allocateFlags & OMR_GC_ALLOCATE_OBJECT_TENURED);
	if (OMR_GC_ALLOCATE_OBJECT_NON_ZERO_TLH == (allocateFlags & OMR_GC_ALLOCATE_OBJECT_NON_ZERO_TLH)) {
		Assert_MM_true(GC_ObjectModel::SCAN_PRIMITIVE_ARRAY_OBJECT == extensions->objectModel.getScanType(clazz));
	}

	J9Object *objectPtr = NULL;
	if(!traceObjectCheck(vmThread)){
		allocateFlags |= OMR_GC_ALLOCATE_OBJECT_NO_GC;
		MM_IndexableObjectAllocationModel indexableOAM(env, clazz, numberOfIndexedFields, allocateFlags);
		if (indexableOAM.initializeAllocateDescription(env)) {
			env->_isInNoGCAllocationCall = true;
			objectPtr = OMR_GC_AllocateObject(vmThread->omrVMThread, &indexableOAM);
			if (NULL != objectPtr) {
				uintptr_t allocatedBytes = env->getExtensions()->objectModel.getConsumedSizeInBytesWithHeader(objectPtr);
				Assert_MM_true(allocatedBytes == indexableOAM.getAllocateDescription()->getContiguousBytes());
			}
			env->_isInNoGCAllocationCall = false;
		}
	}

	return objectPtr;
}

/**
 * High level allocate routine (used by VM) to allocate a single object
 * @param clazz the class of the object to be allocated
 * @param allocateFlags a bitfield of flags from the following
 *	OMR_GC_ALLOCATE_OBJECT_TENURED forced Old space allocation even if Generational Heap
 *	OMR_GC_ALLOCATE_OBJECT_INSTRUMENTABLE set if this allocate was an instrumentable allocate
 *	OMR_GC_ALLOCATE_OBJECT_HASHED set if this allocation initializes hash slot
 * @return pointer to the object header, or NULL
 */
J9Object *
J9AllocateObject(J9VMThread *vmThread, J9Class *clazz, uintptr_t allocateFlags)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);

#if defined(J9VM_GC_THREAD_LOCAL_HEAP)	
	if (!env->isInlineTLHAllocateEnabled()) {
		/* For duration of call restore TLH allocate fields;
		 * we will hide real heapAlloc again on exit to fool JIT/Interpreter
		 * into thinking TLH is full if needed 
		 */ 
		env->enableInlineTLHAllocate();
	}	
#endif /* J9VM_GC_THREAD_LOCAL_HEAP */	
	
	Assert_MM_false(allocateFlags & OMR_GC_ALLOCATE_OBJECT_NON_ZERO_TLH);
	Assert_MM_false(allocateFlags & OMR_GC_ALLOCATE_OBJECT_NO_GC);

	J9Object *objectPtr = NULL;
	/* Replaced classes have poisoned the totalInstanceSize such that they are not allocatable,
	 * so inline allocate and NoGC allocate have already failed. If this allocator is reached
	 * with a replaced class, update to the current version and allocate that.
	 */
	clazz = J9_CURRENT_CLASS(clazz);
	MM_MixedObjectAllocationModel mixedOAM(env, clazz, allocateFlags);
	if (mixedOAM.initializeAllocateDescription(env)) {
		objectPtr = OMR_GC_AllocateObject(vmThread->omrVMThread, &mixedOAM);
		if (NULL != objectPtr) {
			uintptr_t allocatedBytes = env->getExtensions()->objectModel.getConsumedSizeInBytesWithHeader(objectPtr);
			Assert_MM_true(allocatedBytes == mixedOAM.getAllocateDescription()->getContiguousBytes());
			if (J9_ARE_ANY_BITS_SET(J9CLASS_EXTENDED_FLAGS(clazz), J9ClassReservableLockWordInit)) {
				*J9OBJECT_MONITOR_EA(vmThread, objectPtr) = OBJECT_HEADER_LOCK_RESERVED;
			}
		}
	}

	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	if (env->_failAllocOnExcessiveGC && (NULL != objectPtr)) {
		/* If we have garbage collected too much, return NULL as if we had failed to allocate the object (effectively triggering an OOM).
		 * TODO: The ordering of this call wrt/ allocation really needs to change - this is just a temporary solution until
		 * we reorganize the rest of the code base.
		 */
		objectPtr = NULL;
		/* we stop failing subsequent allocations, to give some room for the Java program
		 * to recover (release some resources) after OutOfMemoryError until next GC occurs
		 */
		env->_failAllocOnExcessiveGC = false;
		extensions->excessiveGCLevel = excessive_gc_fatal_consumed;
		/* sync storage since we have memset the object to NULL and
		 * filled in the header information even though we will be returning NULL
		 */
		MM_AtomicOperations::writeBarrier();
		Trc_MM_ObjectAllocationFailedDueToExcessiveGC(vmThread);
	}

	uintptr_t sizeInBytesRequired = mixedOAM.getAllocateDescription()->getBytesRequested();
	if (NULL != objectPtr) {
		/* The hook could release access and so the object address could change (the value is preserved). */
		if (OMR_GC_ALLOCATE_OBJECT_INSTRUMENTABLE == (OMR_GC_ALLOCATE_OBJECT_INSTRUMENTABLE & allocateFlags)) {
			TRIGGER_J9HOOK_VM_OBJECT_ALLOCATE_INSTRUMENTABLE(
				vmThread->javaVM->hookInterface, 
				vmThread,
				objectPtr, 
				sizeInBytesRequired);
		} else {
			TRIGGER_J9HOOK_VM_OBJECT_ALLOCATE(
				vmThread->javaVM->hookInterface, 
				vmThread,
				objectPtr, 
				sizeInBytesRequired);
		}
		
		if( !mixedOAM.getAllocateDescription()->isCompletedFromTlh()) {
			TRIGGER_J9HOOK_MM_PRIVATE_NON_TLH_ALLOCATION(
				extensions->privateHookInterface,
				vmThread->omrVMThread,
				objectPtr);
		}
		
		uintptr_t lowThreshold = extensions->lowAllocationThreshold;
		uintptr_t highThreshold = extensions->highAllocationThreshold;
		if ( (sizeInBytesRequired >= lowThreshold) && (sizeInBytesRequired <= highThreshold) ) {
			Trc_MM_AllocationThreshold_triggerAllocationThresholdEvent(vmThread,sizeInBytesRequired,lowThreshold,highThreshold);
			TRIGGER_J9HOOK_VM_OBJECT_ALLOCATE_WITHIN_THRESHOLD(
				vmThread->javaVM->hookInterface,
				vmThread,
				objectPtr,
				sizeInBytesRequired,
				lowThreshold,
				highThreshold);
		}
	}

	if(NULL == objectPtr) {
		MM_MemorySpace *memorySpace = mixedOAM.getAllocateDescription()->getMemorySpace();
		PORT_ACCESS_FROM_ENVIRONMENT(env);
		/* we're going to return NULL, trace this */
		Trc_MM_ObjectAllocationFailed(vmThread, sizeInBytesRequired, clazz, memorySpace->getName(), memorySpace);
		dumpStackFrames(vmThread);
		TRIGGER_J9HOOK_MM_PRIVATE_OUT_OF_MEMORY(extensions->privateHookInterface, vmThread->omrVMThread, j9time_hires_clock(), J9HOOK_MM_PRIVATE_OUT_OF_MEMORY, memorySpace, memorySpace->getName());
	} else {
		objectPtr = traceAllocateObject(vmThread, objectPtr, clazz, sizeInBytesRequired);
		if (extensions->isStandardGC()) {
			if (OMR_GC_ALLOCATE_OBJECT_TENURED == (allocateFlags & OMR_GC_ALLOCATE_OBJECT_TENURED)) {
				/* Object must be allocated in Tenure if it is requested */
				Assert_MM_true(extensions->isOld(objectPtr));
			}
#if defined(J9VM_GC_REALTIME) 
		} else if (extensions->isMetronomeGC()) {
			if (env->saveObjects((omrobjectptr_t)objectPtr)) {
				j9gc_startGCIfTimeExpired(vmThread->omrVMThread);
				env->restoreObjects((omrobjectptr_t*)&objectPtr);
			}
#endif /* defined(J9VM_GC_REALTIME) */
		}
	}

#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
	if (extensions->fvtest_disableInlineAllocation || extensions->instrumentableAllocateHookEnabled || extensions->disableInlineCacheForAllocationThreshold) {
		env->disableInlineTLHAllocate();
	}	
#endif /* J9VM_GC_THREAD_LOCAL_HEAP */	

	return objectPtr;
}

/**
 * High level allocate routine (used by VM) to allocate an object array
 * @param allocateFlags a bitfield of flags from the following
 *	OMR_GC_ALLOCATE_OBJECT_TENURED forced Old space allocation even if Generational Heap
 *	OMR_GC_ALLOCATE_OBJECT_INSTRUMENTABLE set if this allocate was an instrumentable allocate
 *	OMR_GC_ALLOCATE_OBJECT_HASHED set if this allocation initializes hash slot
 * @return pointer to the object header, or NULL * 
 */
J9Object *
J9AllocateIndexableObject(J9VMThread *vmThread, J9Class *clazz, uint32_t numberOfIndexedFields, uintptr_t allocateFlags)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);

	Assert_MM_false(allocateFlags & OMR_GC_ALLOCATE_OBJECT_NO_GC);
	if (OMR_GC_ALLOCATE_OBJECT_NON_ZERO_TLH == (allocateFlags & OMR_GC_ALLOCATE_OBJECT_NON_ZERO_TLH)) {
		Assert_MM_true(GC_ObjectModel::SCAN_PRIMITIVE_ARRAY_OBJECT == extensions->objectModel.getScanType(clazz));
	}

#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
	if (!env->isInlineTLHAllocateEnabled()) {
		/* For duration of call restore TLH allocate fields;
		 * we will hide real heapAlloc again on exit to fool JIT/Interpreter
		 * into thinking TLH is full if needed 
		 */ 
		env->enableInlineTLHAllocate();
	}	
#endif /* J9VM_GC_THREAD_LOCAL_HEAP */

	J9Object *objectPtr = NULL;
	uintptr_t sizeInBytesRequired = 0;
	MM_IndexableObjectAllocationModel indexableOAM(env, clazz, numberOfIndexedFields, allocateFlags);
	if (indexableOAM.initializeAllocateDescription(env)) {
		objectPtr = OMR_GC_AllocateObject(vmThread->omrVMThread, &indexableOAM);
		if (NULL != objectPtr) {
			uintptr_t allocatedBytes = env->getExtensions()->objectModel.getConsumedSizeInBytesWithHeader(objectPtr);
			Assert_MM_true(allocatedBytes == indexableOAM.getAllocateDescription()->getContiguousBytes());
		}
	}
	
	if (env->_failAllocOnExcessiveGC && (NULL != objectPtr)) {
		/* If we have garbage collected too much, return NULL as if we had failed to allocate the object (effectively triggering an OOM).
		 * TODO: The ordering of this call wrt/ allocation really needs to change - this is just a temporary solution until
		 * we reorganize the rest of the code base.
		 */
		objectPtr = NULL;	
		/* we stop failing subsequent allocations, to give some room for the Java program
		 * to recover (release some resources) after OutOfMemoryError until next GC occurs
		 */
		env->_failAllocOnExcessiveGC = false;
		extensions->excessiveGCLevel = excessive_gc_fatal_consumed;
		/* sync storage since we have memset the object to NULL and
		 * filled in the header information even though we will be returning NULL
		 */
		MM_AtomicOperations::storeSync();
		Trc_MM_ArrayObjectAllocationFailedDueToExcessiveGC(vmThread);
	}

	sizeInBytesRequired = indexableOAM.getAllocateDescription()->getBytesRequested();
	if (NULL != objectPtr) {
		/* The hook could release access and so the object address could change (the value is preserved).  Since this
		 * means the hook could write back a different value to the variable, it must be a valid lvalue (ie: not cast).
		 */
		if (OMR_GC_ALLOCATE_OBJECT_INSTRUMENTABLE == (OMR_GC_ALLOCATE_OBJECT_INSTRUMENTABLE & allocateFlags)) {
			TRIGGER_J9HOOK_VM_OBJECT_ALLOCATE_INSTRUMENTABLE(
				vmThread->javaVM->hookInterface, 
				vmThread, 
				objectPtr, 
				sizeInBytesRequired);
		} else {
			TRIGGER_J9HOOK_VM_OBJECT_ALLOCATE(
				vmThread->javaVM->hookInterface, 
				vmThread, 
				objectPtr, 
				sizeInBytesRequired);
		}
	
		/* If this was a non-TLH allocation, trigger the hook */
		if( !indexableOAM.getAllocateDescription()->isCompletedFromTlh()) {
			TRIGGER_J9HOOK_MM_PRIVATE_NON_TLH_ALLOCATION(
				extensions->privateHookInterface,
				vmThread->omrVMThread,
				objectPtr);
		}
		
		uintptr_t lowThreshold = extensions->lowAllocationThreshold;
		uintptr_t highThreshold = extensions->highAllocationThreshold;
		if ( (sizeInBytesRequired >= lowThreshold) && (sizeInBytesRequired <= highThreshold) ) {
			Trc_MM_AllocationThreshold_triggerAllocationThresholdEventIndexable(vmThread,sizeInBytesRequired,lowThreshold,highThreshold);
			TRIGGER_J9HOOK_VM_OBJECT_ALLOCATE_WITHIN_THRESHOLD(
				vmThread->javaVM->hookInterface,
				vmThread,
				objectPtr,
				sizeInBytesRequired,
				lowThreshold,
				highThreshold);
		}
		
		objectPtr = traceAllocateObject(vmThread, objectPtr, clazz, sizeInBytesRequired, (uintptr_t)numberOfIndexedFields);
		if (extensions->isStandardGC()) {
			if (OMR_GC_ALLOCATE_OBJECT_TENURED == (allocateFlags & OMR_GC_ALLOCATE_OBJECT_TENURED)) {
				/* Object must be allocated in Tenure if it is requested */
				Assert_MM_true(extensions->isOld(objectPtr));
			}
#if defined(J9VM_GC_REALTIME)
		} else if (extensions->isMetronomeGC()) {
			if (env->saveObjects((omrobjectptr_t)objectPtr)) {
				j9gc_startGCIfTimeExpired(vmThread->omrVMThread);
				env->restoreObjects((omrobjectptr_t*)&objectPtr);
			}
#endif /* defined(J9VM_GC_REALTIME) */
		}
	} else {
		/* we're going to return NULL, trace this */
		PORT_ACCESS_FROM_ENVIRONMENT(env);
		MM_MemorySpace *memorySpace = indexableOAM.getAllocateDescription()->getMemorySpace();
		Trc_MM_ArrayObjectAllocationFailed(vmThread, sizeInBytesRequired, clazz, memorySpace->getName(), memorySpace);
		dumpStackFrames(vmThread);
		TRIGGER_J9HOOK_MM_PRIVATE_OUT_OF_MEMORY(extensions->privateHookInterface, vmThread->omrVMThread, j9time_hires_clock(), J9HOOK_MM_PRIVATE_OUT_OF_MEMORY, memorySpace, memorySpace->getName());
	}

#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
	if (extensions->fvtest_disableInlineAllocation || extensions->instrumentableAllocateHookEnabled || extensions->disableInlineCacheForAllocationThreshold ) {
		env->disableInlineTLHAllocate();
	}	
#endif /* J9VM_GC_THREAD_LOCAL_HEAP */	

	return objectPtr;
}

/** 
 * Async message callback routine called whenever J9HOOK_VM_OBJECT_ALLOCATE_INSTRUMENTABLE
 * or J9HOOK_VM_OBJECT_ALLOCATE_WITHIN_THRESHOLD is registered or unregistered. Each time called 
 * we check to see if at least one user is still registered. If so we disable inline 
 * TLH allocate to force JIT/Interpreter to go out of line, ie call J9AllocateObject et al,
 * for allocates so that the calls to the required calls to the hook routine(s) can be made.
 *
 * @param vmThread - thread whose inline allocates need enabling/disabling
 */ 
void
memoryManagerTLHAsyncCallbackHandler(J9VMThread *vmThread, IDATA handlerKey, void *userData)
{
	J9JavaVM * vm = (J9JavaVM*)userData;
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	MM_ObjectAllocationInterface* allocationInterface = env->_objectAllocationInterface;

	extensions->instrumentableAllocateHookEnabled = (0 != J9_EVENT_IS_HOOKED(vm->hookInterface,J9HOOK_VM_OBJECT_ALLOCATE_INSTRUMENTABLE));
	
	if ( J9_EVENT_IS_HOOKED(vm->hookInterface,J9HOOK_VM_OBJECT_ALLOCATE_WITHIN_THRESHOLD) ) {
		Trc_MM_memoryManagerTLHAsyncCallbackHandler_eventIsHooked(vmThread);
		if (extensions->isStandardGC() || extensions->isVLHGC()) {
#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
			extensions->disableInlineCacheForAllocationThreshold = (extensions->lowAllocationThreshold < (extensions->tlhMaximumSize + extensions->tlhMinimumSize));
#endif /* defined(J9VM_GC_THREAD_LOCAL_HEAP) */
		} else if (extensions->isSegregatedHeap()) {
#if defined(J9VM_GC_SEGREGATED_HEAP)
			extensions->disableInlineCacheForAllocationThreshold = (extensions->lowAllocationThreshold <= J9VMGC_SIZECLASSES_MAX_SMALL_SIZE_BYTES);
#endif /* defined(J9VM_GC_SEGREGATED_HEAP) */
		}
	} else {
		Trc_MM_memoryManagerTLHAsyncCallbackHandler_eventNotHooked(vmThread);
		extensions->disableInlineCacheForAllocationThreshold = false;
	}
	
	if (extensions->isStandardGC() || extensions->isVLHGC()) {
#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
		if (extensions->fvtest_disableInlineAllocation || extensions->instrumentableAllocateHookEnabled || extensions->disableInlineCacheForAllocationThreshold) {
			Trc_MM_memoryManagerTLHAsyncCallbackHandler_disableInlineTLHAllocates(vmThread,extensions->lowAllocationThreshold,extensions->highAllocationThreshold,extensions->tlhMinimumSize,extensions->tlhMaximumSize);
			if (env->isInlineTLHAllocateEnabled()) {
				/* BEN TODO: Collapse the env->enable/disableInlineTLHAllocate with these enable/disableCachedAllocations */
				env->disableInlineTLHAllocate();
				allocationInterface->disableCachedAllocations(env);
			}
		} else {
			Trc_MM_memoryManagerTLHAsyncCallbackHandler_enableInlineTLHAllocates(vmThread,extensions->lowAllocationThreshold,extensions->highAllocationThreshold,extensions->tlhMinimumSize,extensions->tlhMaximumSize);
			if (!env->isInlineTLHAllocateEnabled()) {
				/* BEN TODO: Collapse the env->enable/disableInlineTLHAllocate with these enable/disableCachedAllocations */
				env->enableInlineTLHAllocate();
				allocationInterface->enableCachedAllocations(env);
			}
		}
#endif /* defined(J9VM_GC_THREAD_LOCAL_HEAP) */
	} else if (extensions->isSegregatedHeap()) {
#if defined(J9VM_GC_SEGREGATED_HEAP)
		if (extensions->fvtest_disableInlineAllocation || extensions->instrumentableAllocateHookEnabled || extensions->disableInlineCacheForAllocationThreshold) {
			Trc_MM_memoryManagerTLHAsyncCallbackHandler_disableAllocationCache(vmThread,extensions->lowAllocationThreshold,extensions->highAllocationThreshold);
			if (allocationInterface->cachedAllocationsEnabled(env)) {
				allocationInterface->disableCachedAllocations(env);
			}
		} else {
			Trc_MM_memoryManagerTLHAsyncCallbackHandler_enableAllocationCache(vmThread,extensions->lowAllocationThreshold,extensions->highAllocationThreshold);
			if (!allocationInterface->cachedAllocationsEnabled(env)) {
				allocationInterface->enableCachedAllocations(env);
			}
		}
#endif /* defined(J9VM_GC_SEGREGATED_HEAP) */
	}
}
} /* extern "C" */

