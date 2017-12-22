
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/


#include <stdlib.h>
#include <stdio.h>
#include <limits.h> // or <climits> for CHAR_BIT
#include <string.h> // memcpy

#include "j9cfg.h"
#include "j9.h"
#if defined(J9VM_OPT_JVMTI)
#include "jvmtiInternal.h"
#endif /* defined(J9VM_OPT_JVMTI) */

#include "HeapRootScanner.hpp"

#include "ClassIterator.hpp"
#include "ClassHeapIterator.hpp"
#include "ClassLoaderIterator.hpp"
#include "Debug.hpp"
#if defined(J9VM_GC_FINALIZATION)
#include "FinalizeListManager.hpp"
#endif /* J9VM_GC_FINALIZATION*/
#include "MixedObjectIterator.hpp"
#include "ModronTypes.hpp"
#include "ObjectAccessBarrier.hpp"
#include "ObjectHeapIterator.hpp"
#include "ObjectModel.hpp"
#include "OwnableSynchronizerObjectList.hpp"
#include "PointerArrayIterator.hpp"
#include "SegmentIterator.hpp"
#include "StringTable.hpp"
#include "UnfinalizedObjectList.hpp"
#include "VMClassSlotIterator.hpp"
#include "VMThreadListIterator.hpp"
#include "VMThreadIterator.hpp"

/**
 * @todo Provide function documentation
 */
void
MM_HeapRootScanner::doClassLoader(J9ClassLoader *classLoader)
{
	if (J9_GC_CLASS_LOADER_DEAD != (classLoader->gcFlags & J9_GC_CLASS_LOADER_DEAD) ) {
		doSlot(J9GC_J9CLASSLOADER_CLASSLOADEROBJECT_EA(classLoader));
	}
}

void
MM_HeapRootScanner::doOwnableSynchronizerObject(J9Object *objectPtr)
{
	doObject(objectPtr);
}

#if defined(J9VM_GC_FINALIZATION)
/**
 * @todo Provide function documentation
 */
void
MM_HeapRootScanner::doUnfinalizedObject(J9Object *objectPtr)
{
	doObject(objectPtr);
}
#endif /* J9VM_GC_FINALIZATION */

#if defined(J9VM_GC_FINALIZATION)
/**
 * @todo Provide function documentation
 */
void
MM_HeapRootScanner::doFinalizableObject(J9Object *objectPtr)
{
	doObject(objectPtr);
}
#endif /* J9VM_GC_FINALIZATION */

/**
 * @todo Provide function documentation
 */
void
MM_HeapRootScanner::doMonitorReference(J9ObjectMonitor *objectMonitor, GC_HashTableIterator *monitorReferenceIterator)
{
	J9ThreadAbstractMonitor * monitor = (J9ThreadAbstractMonitor *)objectMonitor->monitor;
	
	void *userData = (void *)monitor->userData;
	
	doObject((J9Object *)userData);
}

/**
 * @todo Provide function documentation
 */
void
MM_HeapRootScanner::doJNIWeakGlobalReference(J9Object **slotPtr)
{
	doSlot(slotPtr);
}

#if defined(J9VM_GC_MODRON_SCAVENGER)
/**
 * @todo Provide function documentation
 */
void
MM_HeapRootScanner::doRememberedSetSlot(J9Object **slotPtr, GC_RememberedSetSlotIterator *rememberedSetSlotIterator)
{
	doSlot(slotPtr);
}
#endif /* defined(J9VM_GC_MODRON_SCAVENGER) */

/**
 * @todo Provide function documentation
 */
void
MM_HeapRootScanner::doStringTableSlot(J9Object **slotPtr, GC_StringTableIterator *stringTableIterator)
{
	doSlot(slotPtr);
}

/**
 * @todo Provide function documentation
 */
void
MM_HeapRootScanner::doVMClassSlot(J9Class **slotPtr, GC_VMClassSlotIterator *vmClassSlotIterator)
{
	doClassSlot(slotPtr);
}

#if defined(J9VM_OPT_JVMTI)
/**
 * @todo Provide function documentation
 */
void
MM_HeapRootScanner::doJVMTIObjectTagSlot(J9Object **slotPtr, GC_JVMTIObjectTagTableIterator *objectTagTableIterator)
{
	doSlot(slotPtr);
}
#endif /* J9VM_OPT_JVMTI */

/**
 * @todo Provide function documentation
 */
void
MM_HeapRootScanner::scanClasses()
{
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	J9ClassLoader *sysClassLoader = _javaVM->systemClassLoader;
	J9ClassLoader *appClassLoader = _javaVM->applicationClassLoader;
	MM_GCExtensions::DynamicClassUnloading dynamicClassUnloadingFlag = (MM_GCExtensions::DynamicClassUnloading )_extensions->dynamicClassUnloading;
#endif
	
	reportScanningStarted(RootScannerEntity_Classes);
	
	GC_SegmentIterator segmentIterator(_javaVM->classMemorySegments, MEMORY_TYPE_RAM_CLASS);
	
	while(J9MemorySegment *segment = segmentIterator.nextSegment()) {
		GC_ClassHeapIterator classHeapIterator(_javaVM, segment);
		J9Class *clazz = NULL;
		while(NULL != (clazz = classHeapIterator.nextClass())) {
			RootScannerEntityReachability reachability;
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
			if (MM_GCExtensions::DYNAMIC_CLASS_UNLOADING_NEVER == dynamicClassUnloadingFlag) {
				/* if -Xnoclassgc, all classes are strong */
				reachability = RootScannerEntityReachability_Strong;
			} else {
				if ((sysClassLoader == clazz->classLoader) || (appClassLoader == clazz->classLoader)) {
					reachability = RootScannerEntityReachability_Strong;
				} else {
					reachability = RootScannerEntityReachability_Weak;
				}
			}
#else
			reachability = RootScannerEntityReachability_Strong;
#endif
			setReachability(reachability);
			doClass(clazz);
		}
	}

	reportScanningEnded(RootScannerEntity_Classes);
}

/**
 * Scan through the slots in the VM containing known classes
 */
void
MM_HeapRootScanner::scanVMClassSlots()
{
	reportScanningStarted(RootScannerEntity_VMClassSlots);
	setReachability(RootScannerEntityReachability_Strong);

	GC_VMClassSlotIterator classSlotIterator(_javaVM);
	J9Class **slotPtr;
	
	while((slotPtr = classSlotIterator.nextSlot()) != NULL) {
		doVMClassSlot(slotPtr, &classSlotIterator);
	}
	
	reportScanningEnded(RootScannerEntity_VMClassSlots);	
}

/** 
 * General class slot handler to be reimplemented by specializing class. 
 * This handler is called for every reference to a J9Class. 
 */
void 
MM_HeapRootScanner::doClassSlot(J9Class** slotPtr)
{
	doClass(*slotPtr);
}

/**
 * @todo Provide function documentation
 */
void
MM_HeapRootScanner::doVMThreadSlot(J9Object **slotPtr, GC_VMThreadIterator *vmThreadIterator)
{
	doSlot(slotPtr);
}


/**
 * @todo Provide function documentation
 */
void
MM_HeapRootScanner::doJNIGlobalReferenceSlot(J9Object **slotPtr, GC_JNIGlobalReferenceIterator *jniGlobalReferenceIterator)
{
	doSlot(slotPtr);
}

/**
 * @todo Provide function documentation
 */
void
MM_HeapRootScanner::scanClassLoaders()
{	
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	J9ClassLoader *sysClassLoader = _javaVM->systemClassLoader;
	J9ClassLoader *appClassLoader = _javaVM->applicationClassLoader;
	MM_GCExtensions::DynamicClassUnloading dynamicClassUnloadingFlag = (MM_GCExtensions::DynamicClassUnloading )_extensions->dynamicClassUnloading;
#endif
	J9ClassLoader *classLoader;	
	GC_ClassLoaderIterator classLoaderIterator(_javaVM->classLoaderBlocks);
	
	reportScanningStarted(RootScannerEntity_ClassLoaders);
	
	while((classLoader = classLoaderIterator.nextSlot()) != NULL) {
		RootScannerEntityReachability reachability;
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		if (MM_GCExtensions::DYNAMIC_CLASS_UNLOADING_NEVER == dynamicClassUnloadingFlag) {
			/* if -Xnoclassgc, all class loader refs are strong */
			reachability = RootScannerEntityReachability_Strong;
		} else {
			if (classLoader == appClassLoader || classLoader == sysClassLoader) {
				reachability = RootScannerEntityReachability_Strong;
			} else {
				reachability = RootScannerEntityReachability_Weak;	
			}
		}
#else
		reachability = RootScannerEntityReachability_Strong;
#endif
		setReachability(reachability);
		doClassLoader(classLoader);
		
	}
	reportScanningEnded(RootScannerEntity_ClassLoaders);
}

/**
 * @todo Provide function documentation
 *
 * This function iterates through all the threads, calling scanOneThread on each one that
 * should be scanned.  The scanOneThread function scans exactly one thread and returns
 * either true (if it took an action that requires the thread list iterator to return to
 * the beginning) or false (if the thread list iterator should just continue with the next
 * thread).
 */
void
MM_HeapRootScanner::scanThreads()
{
	reportScanningStarted(RootScannerEntity_Threads);
	setReachability(RootScannerEntityReachability_Strong);
	
	GC_VMThreadListIterator vmThreadListIterator(_javaVM);
	
	while(J9VMThread *walkThread = vmThreadListIterator.nextVMThread()) {
		if (scanOneThread(walkThread)) {
			vmThreadListIterator.reset(_javaVM->mainThread);
		}
	}

	reportScanningEnded(RootScannerEntity_Threads);
}

/**
 * This function scans exactly one thread for potential roots.
 * @param walkThead the thread to be scanned
 * @param localData opaque data to be passed to the stack walker callback function.
 *   The root scanner fixes that callback function to the stackSlotIterator function
 *   defined above
 * @return true if the thread scan included an action that requires the thread list
 *   iterator to begin over again at the beginning, false otherwise.  This implementation
 *   always returns false but an overriding implementation may take actions that require
 *   a true return (for example, RealtimeRootScanner returns true if it yielded after the
 *   scan, allowing other threads to run and perhaps be created or terminated).
 **/
bool
MM_HeapRootScanner::scanOneThread(J9VMThread* walkThread)
{
	GC_VMThreadIterator vmThreadIterator(walkThread);
	
	while(J9Object **slot = vmThreadIterator.nextSlot()) {
		doVMThreadSlot(slot, &vmThreadIterator);
	}
	
	return false;
}

#if defined(J9VM_GC_FINALIZATION)
/**
 * @todo Provide function documentation
 */
void
MM_HeapRootScanner::scanFinalizableObjects()
{
	reportScanningStarted(RootScannerEntity_FinalizableObjects);
	setReachability(RootScannerEntityReachability_Strong);
	
	GC_FinalizeListManager * finalizeListManager = _extensions->finalizeListManager;
	{
		/* walk finalizable objects created by the system class loader */
		j9object_t systemObject = finalizeListManager->peekSystemFinalizableObject();
		while (NULL != systemObject) {
			doFinalizableObject(systemObject);
			systemObject = finalizeListManager->peekNextSystemFinalizableObject(systemObject);
		}
	}

	{
		/* walk finalizable objects created by all other class loaders*/
		j9object_t defaultObject = finalizeListManager->peekDefaultFinalizableObject();
		while (NULL != defaultObject) {
			doFinalizableObject(defaultObject);
			defaultObject = finalizeListManager->peekNextDefaultFinalizableObject(defaultObject);
		}
	}

	{
		/* walk reference objects */
		j9object_t referenceObject = finalizeListManager->peekReferenceObject();
		while (NULL != referenceObject) {
			doFinalizableObject(referenceObject);
			referenceObject = finalizeListManager->peekNextReferenceObject(referenceObject);
		}
	}

	reportScanningEnded(RootScannerEntity_FinalizableObjects);
}
#endif /* J9VM_GC_FINALIZATION */

/**
 * @todo Provide function documentation
 */
void
MM_HeapRootScanner::scanJNIGlobalReferences()
{
	reportScanningStarted(RootScannerEntity_JNIGlobalReferences);
	setReachability(RootScannerEntityReachability_Strong);

	GC_JNIGlobalReferenceIterator jniGlobalReferenceIterator(_javaVM->jniGlobalReferences);
	J9Object **slot;

	while((slot = (J9Object **)jniGlobalReferenceIterator.nextSlot()) != NULL) {
		doJNIGlobalReferenceSlot(slot, &jniGlobalReferenceIterator);
	}

	reportScanningEnded(RootScannerEntity_JNIGlobalReferences);
}

/**
 * @todo Provide function documentation
 */
void
MM_HeapRootScanner::scanStringTable()
{
	reportScanningStarted(RootScannerEntity_StringTable);
	RootScannerEntityReachability reachability;
	
	if (_extensions->collectStringConstants) {
		reachability = RootScannerEntityReachability_Weak;
	} else {
		reachability = RootScannerEntityReachability_Strong;
	}
	setReachability(reachability);
	
	MM_StringTable *stringTable = MM_GCExtensions::getExtensions(_javaVM->omrVM)->getStringTable();
	for (UDATA tableIndex = 0; tableIndex < stringTable->getTableCount(); tableIndex++) {
		GC_HashTableIterator stringTableIterator(stringTable->getTable(tableIndex));
		J9Object **slot;

		while((slot = (J9Object **)stringTableIterator.nextSlot()) != NULL) {
			doStringTableSlot(slot, NULL);
		}
	}
	reportScanningEnded(RootScannerEntity_StringTable);
}

#if defined(J9VM_GC_FINALIZATION)
/**
 * @todo Provide function documentation
 *
 * @NOTE this code is not thread safe and assumes it is called in a single threaded
 * manner.
 *
 * @NOTE this can only be used as a READ-ONLY version of the unfinalizedObjectList.
 * If you need to modify elements with-in the list you will need to provide your own functionality.
 * See MM_MarkingScheme::scanUnfinalizedObjects(MM_EnvironmentStandard *env) as an example
 * which modifies elements with-in the list.
 */
void
MM_HeapRootScanner::scanUnfinalizedObjects()
{
	reportScanningStarted(RootScannerEntity_UnfinalizedObjects);
	setReachability(RootScannerEntityReachability_Weak);
	
	MM_ObjectAccessBarrier *barrier = _extensions->accessBarrier;
	MM_UnfinalizedObjectList *unfinalizedObjectList = _extensions->unfinalizedObjectLists;

	while(NULL != unfinalizedObjectList) {
		J9Object *objectPtr = unfinalizedObjectList->getHeadOfList();
		while (NULL != objectPtr) {
			doUnfinalizedObject(objectPtr);
			objectPtr = barrier->getFinalizeLink(objectPtr);
		}
		unfinalizedObjectList = unfinalizedObjectList->getNextList();
	}
	reportScanningEnded(RootScannerEntity_UnfinalizedObjects);
}
#endif /* J9VM_GC_FINALIZATION */

void
MM_HeapRootScanner::scanOwnableSynchronizerObjects()
{
	reportScanningStarted(RootScannerEntity_OwnableSynchronizerObjects);
	setReachability(RootScannerEntityReachability_Weak);

	MM_ObjectAccessBarrier *barrier = _extensions->accessBarrier;
	MM_OwnableSynchronizerObjectList *ownableSynchronizerObjectList = _extensions->ownableSynchronizerObjectLists;

	while(NULL != ownableSynchronizerObjectList) {
		J9Object *objectPtr = ownableSynchronizerObjectList->getHeadOfList();
		while (NULL != objectPtr) {
			doOwnableSynchronizerObject(objectPtr);
			objectPtr = barrier->getOwnableSynchronizerLink(objectPtr);
		}
		ownableSynchronizerObjectList = ownableSynchronizerObjectList->getNextList();
	}
	reportScanningEnded(RootScannerEntity_OwnableSynchronizerObjects);
}

/**
 * @todo Provide function documentation
 */
void
MM_HeapRootScanner::scanMonitorReferences()
{
	reportScanningStarted(RootScannerEntity_MonitorReferences);
	setReachability(RootScannerEntityReachability_Weak);
	
	J9ObjectMonitor *objectMonitor = NULL;
	J9MonitorTableListEntry *monitorTableList = _javaVM->monitorTableList;
	while (NULL != monitorTableList) {
		J9HashTable *table = monitorTableList->monitorTable;
		if (NULL != table) {
			GC_HashTableIterator iterator(table);
			while (NULL != (objectMonitor = (J9ObjectMonitor *)iterator.nextSlot())) {
				doMonitorReference(objectMonitor, &iterator);
			}
		}
		monitorTableList = monitorTableList->next;
	}

	reportScanningEnded(RootScannerEntity_MonitorReferences);
}

/**
 * @todo Provide function documentation
 */
void
MM_HeapRootScanner::scanJNIWeakGlobalReferences()
{
	reportScanningStarted(RootScannerEntity_JNIWeakGlobalReferences);
	setReachability(RootScannerEntityReachability_Weak);
	
	GC_JNIWeakGlobalReferenceIterator jniWeakGlobalReferenceIterator(_javaVM->jniWeakGlobalReferences);
	J9Object **slot;

	while((slot = (J9Object **)jniWeakGlobalReferenceIterator.nextSlot()) != NULL) {
		doJNIWeakGlobalReference(slot);
	}

	reportScanningEnded(RootScannerEntity_JNIWeakGlobalReferences);
}

#if defined(J9VM_GC_MODRON_SCAVENGER)
/**
 * @todo Provide function documentation
 */
void
MM_HeapRootScanner::scanRememberedSet()
{	
	reportScanningStarted(RootScannerEntity_RememberedSet);
	setReachability(RootScannerEntityReachability_Weak);
	
	MM_SublistPuddle *puddle;
	J9Object **slotPtr;	
	
	GC_RememberedSetIterator rememberedSetIterator(&_extensions->rememberedSet);
	
	while((puddle = rememberedSetIterator.nextList()) != NULL) {
		GC_RememberedSetSlotIterator rememberedSetSlotIterator(puddle);
		while((slotPtr = (J9Object **)rememberedSetSlotIterator.nextSlot()) != NULL) {
			doRememberedSetSlot(slotPtr, &rememberedSetSlotIterator);
		}
	}
	
	reportScanningEnded(RootScannerEntity_RememberedSet);
}
#endif /* J9VM_GC_MODRON_SCAVENGER */

#if defined(J9VM_OPT_JVMTI)
/**
 * Scan the JVMTI tag tables for object references
 */
void
MM_HeapRootScanner::scanJVMTIObjectTagTables()
{
	reportScanningStarted(RootScannerEntity_JVMTIObjectTagTables);
	setReachability(RootScannerEntityReachability_Weak);
	
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(_javaVM);
	J9JVMTIEnv * jvmtiEnv;
	J9Object **slotPtr;	
	if (NULL != jvmtiData) {
		GC_JVMTIObjectTagTableListIterator objectTagTableList( jvmtiData->environments);
		while(NULL != (jvmtiEnv = (J9JVMTIEnv *)objectTagTableList.nextSlot())) {
			GC_JVMTIObjectTagTableIterator objectTagTableIterator(jvmtiEnv->objectTagTable);
			while(NULL != (slotPtr = (J9Object **)objectTagTableIterator.nextSlot())) {
				doJVMTIObjectTagSlot(slotPtr, &objectTagTableIterator);
			}
		}
	}
	
	reportScanningEnded(RootScannerEntity_JVMTIObjectTagTables);
}
#endif /* J9VM_OPT_JVMTI */
