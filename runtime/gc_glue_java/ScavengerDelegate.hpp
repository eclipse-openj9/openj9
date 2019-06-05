/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#if !defined(SCAVENGERDELEGATEJAVA_HPP_)
#define SCAVENGERDELEGATEJAVA_HPP_

#include "j9.h"
#include "omrcfg.h"

#if defined(OMR_GC_MODRON_SCAVENGER)

#if defined(J9VM_GC_MODRON_CONCURRENT_MARK) && !defined(OMR_GC_MODRON_CONCURRENT_MARK)
#error "J9VM_GC_MODRON_CONCURRENT_MARK in j9cfg.h requires OMR_GC_MODRON_CONCURRENT_MARK in omrcfg.h"
#endif /* defined(J9VM_GC_MODRON_CONCURRENT_MARK) && !defined(OMR_GC_MODRON_CONCURRENT_MARK) */

#include "j9.h"
#include "CollectorLanguageInterface.hpp"
#include "CompactScheme.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "HeapRegionDescriptorStandard.hpp"
#include "HeapWalker.hpp"
#include "MarkingScheme.hpp"
#include "ScanClassesMode.hpp"

class GC_ObjectScanner;
class GC_VMThreadIterator;
class MM_CompactScheme;
class MM_EnvironmentStandard;
class MM_ForwardedHeader;
class MM_MemorySubSpaceSemiSpace;
class MM_Scavenger;
class MM_ParallelSweepScheme;

#if defined(J9VM_GC_MODRON_CONCURRENT_MARK) && !defined(OMR_GC_MODRON_CONCURRENT_MARK)
#error "J9VM_GC_MODRON_CONCURRENT_MARK in j9cfg.h requires OMR_GC_MODRON_CONCURRENT_MARK in omrcfg.h"
#endif /* defined(J9VM_GC_MODRON_CONCURRENT_MARK) && !defined(OMR_GC_MODRON_CONCURRENT_MARK) */

/**
 * Class implementing project-specific scavenger functionality.
 * @ingroup GC_Base
 */
class MM_ScavengerDelegate : public MM_BaseNonVirtual {
private:
	OMR_VM *_omrVM;
	J9JavaVM *_javaVM;
	MM_GCExtensions *_extensions;
	volatile bool _shouldScavengeFinalizableObjects; /**< Set to true at the beginning of a collection if there are any pending finalizable objects */
	volatile bool _shouldScavengeUnfinalizedObjects; /**< Set to true at the beginning of a collection if there are any unfinalized objects */
	volatile bool _shouldScavengeSoftReferenceObjects; /**< Set to true if there are any SoftReference objects discovered */
	volatile bool _shouldScavengeWeakReferenceObjects; /**< Set to true if there are any WeakReference objects discovered */
	volatile bool _shouldScavengePhantomReferenceObjects; /**< Set to true if there are any PhantomReference objects discovered */
#if defined(J9VM_GC_FINALIZATION)
	bool _finalizationRequired; /**< Scavenger variable used to determine if finalization should be triggered */
#endif /* J9VM_GC_FINALIZATION */

protected:
public:

private:

	/*
	 * ParallelScavenger, Private
	 */

	void private_addOwnableSynchronizerObjectInList(MM_EnvironmentStandard *env, omrobjectptr_t object);
	void private_setupForOwnableSynchronizerProcessing(MM_EnvironmentStandard *env);

	/*
	 * Scavenger Collector, Private
	 */

	/**
	 * Decide if GC percolation should occur for class unloading.
	 * The class unloading occurs after a user defined number of cycles.
	 */
	bool private_shouldPercolateGarbageCollect_classUnloading(MM_EnvironmentBase *envBase);

	/**
	 * Decide if GC percolation should occur due to active JNI critical
	 * regions.  Active regions require that objects do not move, which
	 * prevents scavenging. Percolate collect instead.
	 */
	bool private_shouldPercolateGarbageCollect_activeJNICriticalRegions(MM_EnvironmentBase *envBase);

protected:
public:
	void masterSetupForGC(MM_EnvironmentBase *env);
	void workerSetupForGC_clearEnvironmentLangStats(MM_EnvironmentBase *env);
	void reportScavengeEnd(MM_EnvironmentBase * envBase, bool scavengeSuccessful);
	void mergeGCStats_mergeLangStats(MM_EnvironmentBase *envBase);
	void masterThreadGarbageCollect_scavengeComplete(MM_EnvironmentBase *envBase);
	void masterThreadGarbageCollect_scavengeSuccess(MM_EnvironmentBase *envBase);
	bool internalGarbageCollect_shouldPercolateGarbageCollect(MM_EnvironmentBase *envBase, PercolateReason *reason, U_32 *gcCode);
	GC_ObjectScanner *getObjectScanner(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr, void *allocSpace, uintptr_t flags);
	void flushReferenceObjects(MM_EnvironmentStandard *env);
	bool hasIndirectReferentsInNewSpace(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);
	bool scavengeIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);
	void backOutIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);
	void backOutIndirectObjects(MM_EnvironmentStandard *env);
	void reverseForwardedObject(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedObject);
#if defined (OMR_GC_COMPRESSED_POINTERS)
	void fixupDestroyedSlot(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedObject, MM_MemorySubSpaceSemiSpace *subSpaceNew);
#endif /* OMR_GC_COMPRESSED_POINTERS */

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	void switchConcurrentForThread(MM_EnvironmentBase *env);
	void fixupIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);
	bool shouldYield();
#endif /* OMR_GC_CONCURRENT_SCAVENGER */

	void setShouldScavengeUnfinalizedObjects(bool shouldScavenge) { _shouldScavengeUnfinalizedObjects = shouldScavenge; }

	volatile bool getShouldScavengeFinalizableObjects() { return _shouldScavengeFinalizableObjects; }
	volatile bool getShouldScavengeUnfinalizedObjects() { return _shouldScavengeUnfinalizedObjects; }
	volatile bool getShouldScavengeSoftReferenceObjects() { return _shouldScavengeSoftReferenceObjects; }
	volatile bool getShouldScavengeWeakReferenceObjects() { return _shouldScavengeWeakReferenceObjects; }
	volatile bool getShouldScavengePhantomReferenceObjects() { return _shouldScavengePhantomReferenceObjects; }

#if defined(J9VM_GC_FINALIZATION)
	void setFinalizationRequired(bool req) { _finalizationRequired = req; }
	bool getFinalizationRequired(void) { return _finalizationRequired; }
#endif /* J9VM_GC_FINALIZATION */

#if defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS)
	void poisonSlots(MM_EnvironmentBase *env);
	void healSlots(MM_EnvironmentBase *env);
#endif /* defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS) */

	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);

	MM_ScavengerDelegate(MM_EnvironmentBase* env);
};

#endif /* OMR_GC_MODRON_SCAVENGER */
#endif /* SCAVENGERDELEGATEJAVA_HPP_ */
