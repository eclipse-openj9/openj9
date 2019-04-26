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

#ifndef GLOBALCOLLECTORDELEGATE_HPP_
#define GLOBALCOLLECTORDELEGATE_HPP_

#include "omrcfg.h"

struct J9JavaVM;

class MM_EnvironmentBase;
class MM_GCExtensions;
class MM_GlobalCollector;
class MM_MarkingScheme;

class MM_GlobalCollectorDelegate
{
	/*
	 * Data members
	 */
private:

protected:
	J9JavaVM *_javaVM;
	MM_GCExtensions *_extensions;
	MM_MarkingScheme *_markingScheme;
	MM_GlobalCollector *_globalCollector;
#if defined(J9VM_GC_MODRON_COMPACTION)
	uintptr_t _criticalSectionCount;
#endif /* defined(J9VM_GC_MODRON_COMPACTION) */
#if defined(J9VM_GC_FINALIZATION)
	bool _finalizationRequired;
#endif /* defined(J9VM_GC_FINALIZATION) */

public:

	/*
	 * Function members
	 */
private:
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	bool enterClassUnloadMutex(MM_EnvironmentBase *env, bool force);
	void exitClassUnloadMutex(MM_EnvironmentBase *env);
	void unloadDeadClassLoaders(MM_EnvironmentBase *env);
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

protected:

public:
	/**
	 * Initialize the delegate. For standard GC policies, the global collector and marking scheme
	 * instances must be provided.
	 *
	 * @param env environment for calling thread
	 * @param globalCollector the MM_GlobalCollector instance that the delegate is bound to
	 * @param markingScheme the MM_MarkingScheme instance (may be null if not standard GC)
	 * @return true if delegate initialized successfully
	 */
	bool initialize(MM_EnvironmentBase *env, MM_GlobalCollector *globalCollector, MM_MarkingScheme *markingScheme);
	void tearDown(MM_EnvironmentBase *env);

	void masterThreadGarbageCollectStarted(MM_EnvironmentBase *env);
	void postMarkProcessing(MM_EnvironmentBase *env);
	void masterThreadGarbageCollectFinished(MM_EnvironmentBase *env, bool compactedThisCycle);
	void postCollect(MM_EnvironmentBase* env, MM_MemorySubSpace* subSpace);

	bool isAllowUserHeapWalk();
	void prepareHeapForWalk(MM_EnvironmentBase *env);

#if defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS)
	void poisonSlots(MM_EnvironmentBase *env);
	void healSlots(MM_EnvironmentBase *env);
#endif /* defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS) */

	bool heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress);
	bool heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress);

	bool isTimeForGlobalGCKickoff();

#if defined(J9VM_GC_MODRON_COMPACTION)
	CompactPreventedReason checkIfCompactionShouldBePrevented(MM_EnvironmentBase *env);
#endif /* J9VM_GC_MODRON_COMPACTION */

#if defined(J9VM_GC_FINALIZATION)
	void setFinalizationRequired() { _finalizationRequired = true; }
	bool isFinalizationRequired() { return _finalizationRequired; }
#endif /* defined(J9VM_GC_FINALIZATION) */

	MM_GlobalCollectorDelegate()
		: _javaVM(NULL)
		, _extensions(NULL)
		, _markingScheme(NULL)
		, _globalCollector(NULL)
#if defined(J9VM_GC_MODRON_COMPACTION)
		, _criticalSectionCount(0)
#endif /* defined(J9VM_GC_MODRON_COMPACTION) */
#if defined(J9VM_GC_FINALIZATION)
		, _finalizationRequired(false)
#endif /* defined(J9VM_GC_FINALIZATION) */
	{}
};
#endif /* GLOBALCOLLECTORDELEGATE_HPP_ */
