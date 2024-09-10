
/*******************************************************************************
 * Copyright IBM Corp. and others 2017
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef ENVIRONMENTDELEGATE_HPP_
#define ENVIRONMENTDELEGATE_HPP_

#include "objectdescription.h"

#include "MarkJavaStats.hpp"
#include "ScavengerJavaStats.hpp"
#include "ContinuationStats.hpp"
#include "GCExtensionsBase.hpp"
#include "ObjectModel.hpp"

struct OMR_VMThread;

typedef struct GCmovedObjectHashCode {
	uint32_t originalHashCode;
	bool hasBeenMoved;
	bool hasBeenHashed;
} GCmovedObjectHashCode;

class MM_EnvironmentBase;
class MM_OwnableSynchronizerObjectBuffer;
class MM_ContinuationObjectBuffer;
class MM_ReferenceObjectBuffer;
class MM_UnfinalizedObjectBuffer;

class GC_Environment
{
	/* Data members */
private:

protected:

public:
	MM_MarkJavaStats _markJavaStats;
#if defined(OMR_GC_MODRON_SCAVENGER)
	MM_ScavengerJavaStats _scavengerJavaStats;
#endif /* OMR_GC_MODRON_SCAVENGER */
	MM_ContinuationStats _continuationStats;
	MM_ReferenceObjectBuffer *_referenceObjectBuffer; /**< The thread-specific buffer of recently discovered reference objects */
	MM_UnfinalizedObjectBuffer *_unfinalizedObjectBuffer; /**< The thread-specific buffer of recently allocated unfinalized objects */
	MM_OwnableSynchronizerObjectBuffer *_ownableSynchronizerObjectBuffer; /**< The thread-specific buffer of recently allocated ownable synchronizer objects */
	MM_ContinuationObjectBuffer *_continuationObjectBuffer; /**< The thread-specific buffer of recently allocated continuation objects */

	struct GCmovedObjectHashCode movedObjectHashCodeCache; /**< Structure to aid on object movement and hashing */
#if defined(J9VM_ENV_DATA64)
	bool shouldFixupDataAddrForContiguous; /**< Boolean to check if dataAddr fixup is needed on contiguous indexable object movement */
#endif /* defined(J9VM_ENV_DATA64) */

	/* Function members */
private:
protected:
public:
	GC_Environment()
		:_referenceObjectBuffer(NULL)
		,_unfinalizedObjectBuffer(NULL)
		,_ownableSynchronizerObjectBuffer(NULL)
		,_continuationObjectBuffer(NULL)
#if defined(J9VM_ENV_DATA64)
		,shouldFixupDataAddrForContiguous(false)
#endif /* defined(J9VM_ENV_DATA64) */
	{}
};

class MM_EnvironmentDelegate
{
	/* Data members */
private:
	MM_EnvironmentBase *_env;
	MM_GCExtensionsBase *_extensions;
	J9VMThread *_vmThread;
	GC_Environment _gcEnv;
protected:
public:

	/* Function members */
private:
protected:
public:
	static OMR_VMThread *attachVMThread(OMR_VM *omrVM, const char *threadName, uintptr_t reason);
	static void detachVMThread(OMR_VM *omrVM, OMR_VMThread *omrVMThread, uintptr_t reason);

	/**
	 * Initialize the delegate's internal structures and values.
	 * @return true if initialization completed, false otherwise
	 */
	bool initialize(MM_EnvironmentBase *env);

	/**
	 * Free any internal structures associated to the receiver.
	 */
	void tearDown();

	GC_Environment * getGCEnvironment() { return &_gcEnv; }

	void flushNonAllocationCaches();

	void setGCMainThread(bool isMainThread);

	/**
	 * This will be called for every allocated object.  Note this is not necessarily done when the object is allocated.  You are however
	 * guaranteed by the start of the next gc, you will be notified for all objects allocated since the last gc.
	 * hooktool is actually functionality better for this but is probably too heavy-weight for what we want for performant code.
	 */
	bool objectAllocationNotify(omrobjectptr_t omrObject) { return true; }

	/**
	 * Acquire shared VM access. Threads must acquire VM access before accessing any OMR internal
	 * structures such as the heap. Requests for VM access will be blocked if any other thread is
	 * requesting or has obtained exclusive VM access until exclusive VM access is released.
	 *
	 * This implementation is not preemptive. Threads that have obtained shared VM access must
	 * check frequently whether any other thread is requesting exclusive VM access and release
	 * shared VM access as quickly as possible in that event.
	 */
	void acquireVMAccess();

	/**
	 * Release shared VM access.
	 */
	void releaseVMAccess();
	
	/**
	 * Returns true if a mutator threads entered native code without releasing VM access
	 */
	bool inNative();	

	/**
	 * Check whether another thread is requesting exclusive VM access. This method must be
	 * called frequently by all threads that are holding shared VM access if the VM access framework
	 * is not preemptive. If this method returns true, the calling thread should release shared
	 * VM access as quickly as possible and reacquire it if necessary.
	 *
	 * @return true if another thread is waiting to acquire exclusive VM access
	 */
	bool isExclusiveAccessRequestWaiting();

	/**
	 * Acquire exclusive VM access. This method should only be called by the OMR runtime to
	 * perform stop-the-world operations such as garbage collection. Calling thread will be
	 * blocked until all other threads holding shared VM access have release VM access.
	 */
	void acquireExclusiveVMAccess();

	/**
	 * Release exclusive VM access. If no other thread is waiting for exclusive VM access
	 * this method will notify all threads waiting for shared VM access to continue and
	 * acquire shared VM access.
	 */
	void releaseExclusiveVMAccess();

	uintptr_t relinquishExclusiveVMAccess();

	void assumeExclusiveVMAccess(uintptr_t exclusiveCount);

	void releaseCriticalHeapAccess(uintptr_t *data);

	void reacquireCriticalHeapAccess(uintptr_t data);

	void forceOutOfLineVMAccess();

	/**
	 * This method is responsible for remembering object information before object is moved. Differently than
	 * evacuation, we're sliding the object; therefore, we need to remember object's original information
	 * before object moves and could potentially grow
	 *
	 * @param[in] objectPtr points to the object that is about to be moved
	 * @see postObjectMoveForCompact(omrobjectptr_t)
	 */
	MMINLINE void
	preObjectMoveForCompact(omrobjectptr_t objectPtr)
	{
		GC_ObjectModel *objectModel = &_extensions->objectModel;
		bool hashed = objectModel->hasBeenHashed(objectPtr);
		bool moved = objectModel->hasBeenMoved(objectPtr);

		_gcEnv.movedObjectHashCodeCache.hasBeenHashed = hashed;
		_gcEnv.movedObjectHashCodeCache.hasBeenMoved = moved;

		if (hashed && !moved) {
			/* calculate this BEFORE we (potentially) destroy the object */
			_gcEnv.movedObjectHashCodeCache.originalHashCode = computeObjectAddressToHash((J9JavaVM *)_extensions->getOmrVM()->_language_vm, objectPtr);
		}

#if defined(J9VM_ENV_DATA64)
		if (objectModel->isIndexable(objectPtr)) {
			GC_ArrayObjectModel *indexableObjectModel = &_extensions->indexableObjectModel;
			if (_vmThread->isVirtualLargeObjectHeapEnabled && indexableObjectModel->isInlineContiguousArraylet((J9IndexableObject *)objectPtr)) {
				_gcEnv.shouldFixupDataAddrForContiguous = indexableObjectModel->shouldFixupDataAddrForContiguous((J9IndexableObject *)objectPtr);
			} else {
				_gcEnv.shouldFixupDataAddrForContiguous = false;
			}
		}
#endif /* defined(J9VM_ENV_DATA64) */
	}

	/**
	 * This method may be called during heap compaction, after the object has been moved to a new location.
	 * The implementation may apply any information extracted and cached in the calling thread at this point.
	 * It also updates indexable dataAddr field through fixupDataAddr.
	 *
	 * @param[in] destinationObjectPtr points to the object that has just been moved (new location)
	 * @param[in] sourceObjectPtr points to the old object that has just been moved (old location)
	 * @see preObjectMoveForCompact(omrobjectptr_t)
	 */
	MMINLINE void
	postObjectMoveForCompact(omrobjectptr_t destinationObjectPtr, omrobjectptr_t sourceObjectPtr)
	{
		GC_ObjectModel *objectModel = &_extensions->objectModel;
		GC_ArrayObjectModel *indexableObjectModel = &_extensions->indexableObjectModel;
		if (_gcEnv.movedObjectHashCodeCache.hasBeenHashed && !_gcEnv.movedObjectHashCodeCache.hasBeenMoved) {
			*(uint32_t*)((uintptr_t)destinationObjectPtr + objectModel->getHashcodeOffset(destinationObjectPtr)) = _gcEnv.movedObjectHashCodeCache.originalHashCode;
			objectModel->setObjectHasBeenMoved(destinationObjectPtr);
		}

		if (objectModel->isIndexable(destinationObjectPtr)) {
#if defined(J9VM_ENV_DATA64)
			if (_gcEnv.shouldFixupDataAddrForContiguous) {
				/**
				 * Update the dataAddr internal field of the indexable object. The field being updated
				 * points to the array data. In the case of contiguous data, it will point to the data
				 * itself, and in case of discontiguous data, it will be NULL. How to
				 * update dataAddr is up to the target language that must override fixupDataAddr.
				 */
				indexableObjectModel->setDataAddrForContiguous((J9IndexableObject *)destinationObjectPtr);
			}
#endif /* defined(J9VM_ENV_DATA64) */

			if (UDATA_MAX != _extensions->getOmrVM()->_arrayletLeafSize) {
				indexableObjectModel->fixupInternalLeafPointersAfterCopy((J9IndexableObject *)destinationObjectPtr, (J9IndexableObject *)sourceObjectPtr);
			}
		}
	}

#if defined (OMR_GC_THREAD_LOCAL_HEAP)
	/**
	 * Disable inline TLH allocates by hiding the real heap top address from
	 * JIT/Interpreter in realHeapTop and setting HeapTop == heapALloc so TLH
	 * looks full.
	 *
	 */
	void disableInlineTLHAllocate();

	/**
	 * Re-enable inline TLH allocate by restoring heapTop from realHeapTop
	 */
	void enableInlineTLHAllocate();

	/**
	 * Determine if inline TLH allocate is enabled; its enabled if realheapTop is NULL.
	 * @return TRUE if inline TLH allocates currently enabled for this thread; FALSE otherwise
	 */
	bool isInlineTLHAllocateEnabled();

	/**
	 * Set TLH Sampling Top by hiding the real heap top address from
	 * JIT/Interpreter in realHeapTop and setting HeapTop = (HeapAlloc + size) if size < (HeapTop - HeapAlloc)
	 * so out of line allocate would happen at TLH Sampling Top.
	 * If size >= (HeapTop - HeapAlloc) resetTLHSamplingTop()
	 *
	 * @param size the number of bytes to next sampling point
	 */
	void setTLHSamplingTop(uintptr_t size);

	/**
	 * Restore heapTop from realHeapTop if realHeapTop != NULL
	 */
	void resetTLHSamplingTop();

	/**
	 * Retrieve allocation size inside TLH Cache.
	 * @return (heapAlloc - heapBase)
	 */
	uintptr_t getAllocatedSizeInsideTLH();

#endif /* OMR_GC_THREAD_LOCAL_HEAP */

#if defined(J9VM_OPT_CRIU_SUPPORT)
	/**
	 * Reinitialize the Env Delegate by updating the thread local obj buffers.
	 *
	 * @param[in] env the current environment.
	 * @return boolean indicating whether the Env Delegate was successfully updated.
	 */
	virtual bool reinitializeForRestore(MM_EnvironmentBase* env);
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

	MM_EnvironmentDelegate()
		
	
		: _env(NULL)
		, _extensions(NULL)
		, _vmThread(NULL)
	{ }
};

#endif /* ENVIRONMENTDELEGATE_HPP_ */
