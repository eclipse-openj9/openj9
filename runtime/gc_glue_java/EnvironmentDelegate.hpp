
/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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

#ifndef ENVIRONMENTDELEGATE_HPP_
#define ENVIRONMENTDELEGATE_HPP_

#include "objectdescription.h"

#include "MarkJavaStats.hpp"
#include "ScavengerJavaStats.hpp"

struct OMR_VMThread;

class MM_EnvironmentBase;
class MM_OwnableSynchronizerObjectBuffer;
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
	MM_ReferenceObjectBuffer *_referenceObjectBuffer; /**< The thread-specific buffer of recently discovered reference objects */
	MM_UnfinalizedObjectBuffer *_unfinalizedObjectBuffer; /**< The thread-specific buffer of recently allocated unfinalized objects */
	MM_OwnableSynchronizerObjectBuffer *_ownableSynchronizerObjectBuffer; /**< The thread-specific buffer of recently allocated ownable synchronizer objects */

	/* Function members */
private:
protected:
public:
	GC_Environment()
		:_referenceObjectBuffer(NULL)
		,_unfinalizedObjectBuffer(NULL)
		,_ownableSynchronizerObjectBuffer(NULL)
	{}
};

class MM_EnvironmentDelegate
{
	/* Data members */
private:
	MM_EnvironmentBase *_env;
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

	void setGCMasterThread(bool isMasterThread);

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

#if defined (OMR_GC_THREAD_LOCAL_HEAP)
	/**
	 * Disable inline TLH allocates by hiding the real heap allocation address from
	 * JIT/Interpreter in realHeapAlloc and setting heapALloc == HeapTop so TLH
	 * looks full.
	 *
	 */
	void disableInlineTLHAllocate();

	/**
	 * Re-enable inline TLH allocate by restoring heapAlloc from realHeapAlloc
	 */
	void enableInlineTLHAllocate();

	/**
	 * Determine if inline TLH allocate is enabled; its enabled if realheapAlloc is NULL.
	 * @return TRUE if inline TLH allocates currently enabled for this thread; FALSE otherwise
	 */
	bool isInlineTLHAllocateEnabled();
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

	MM_EnvironmentDelegate()
		: _env(NULL)
	{ }
};

#endif /* ENVIRONMENTDELEGATE_HPP_ */
