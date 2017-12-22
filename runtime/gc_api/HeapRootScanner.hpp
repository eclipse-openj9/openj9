/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
#if !defined(HEAPROOTSCANNER_HPP_)
#define HEAPROOTSCANNER_HPP_

#include "j9.h"
#include "j9cfg.h"

#include "BaseVirtual.hpp"

#include "Debug.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "HeapIteratorAPI.h"
#include "ModronTypes.hpp"

class MM_HeapRootScanner : public MM_BaseVirtual
{
protected:
	MM_GCExtensions *_extensions;
	J9JavaVM *_javaVM;

	bool _stringTableAsRoot;  /**< Treat the string table as a hard root */
	bool _singleThread;  /**< Should the iterator operate in single threaded mode */

	bool _nurseryReferencesOnly;  /**< Should the iterator only scan structures that currently contain nursery references */
	bool _nurseryReferencesPossibly;  /**< Should the iterator only scan structures that may contain nursery references */
	bool _includeStackFrameClassReferences;  /**< Should the iterator include Classes which have a method running on the stack */
#if defined(J9VM_GC_MODRON_SCAVENGER)		 
	bool _includeRememberedSetReferences;  /**< Should the iterator include references in the Remembered Set (if applicable) */
#endif /* J9VM_GC_MODRON_SCAVENGER */	 	
	bool _classDataAsRoots; /**< Should all classes (and class loaders) be treated as roots. Default true, should set to false when class unloading */
	bool _includeJVMTIObjectTagTables; /**< Should the iterator include the JVMTIObjectTagTables. Default true, should set to false when doing JVMTI object walks */
	bool _trackVisibleStackFrameDepth; /**< Should the stack walker be told to track the visible frame depth. Default false, should set to true when doing JVMTI walks that report stack slots */
	
	RootScannerEntity _scanningEntity; /**< The root scanner entity that is currently being scanned. Defaults to RootScannerEntity_None. */ 
	RootScannerEntity _lastScannedEntity; /**< The root scanner entity that was last scanned. Defaults to RootScannerEntity_None. */
	
	RootScannerEntityReachability _scanningEntityReachability;/**< Reachability of the root scanner entity that is currently being scanned. */
	
	/**
	 * Sets the currently scanned root entity to scanningEntity. This is mainly for
	 * debug purposes.
	 * @param scanningEntity The entity for which scanning has begun.
	 */
	MMINLINE void
	reportScanningStarted(RootScannerEntity scanningEntity)
	{
		/* Ensures reportScanningEnded was called previously. */
		assume0(RootScannerEntity_None == _scanningEntity);
		_scanningEntity = scanningEntity;
	}
	
	/**
	 * Sets the currently scanned root entity to scanningEntity. This is mainly for
	 * debug purposes.
	 * @param scanningEntity The entity for which scanning has begun.
	 */
	MMINLINE void
	setReachability(RootScannerEntityReachability reachability)
	{
		_scanningEntityReachability = reachability;
	}
	
	/**
	 * Sets the currently scanned root entity to None and sets the last scanned root
	 * entity to scannedEntity. This is mainly for debug purposes.
	 * @param scannedEntity The entity for which scanning has ended.
	 */
	MMINLINE void
	reportScanningEnded(RootScannerEntity scannedEntity)
	{
		/* Ensures scanning ended for the currently scanned entity. */
		assume0(_scanningEntity == scannedEntity);
		_lastScannedEntity = _scanningEntity;
		_scanningEntity = RootScannerEntity_None;
		_scanningEntityReachability = RootScannerEntityReachability_None;
	}

public:
	MM_HeapRootScanner(J9JavaVM* javaVM, bool singleThread = false) :
		_extensions(MM_GCExtensions::getExtensions(javaVM)),
		_javaVM(javaVM),
		_stringTableAsRoot(true),
		_singleThread(singleThread),
		_nurseryReferencesOnly(false),
		_nurseryReferencesPossibly(false),
		_includeStackFrameClassReferences(true),
#if defined(J9VM_GC_MODRON_SCAVENGER)
		_includeRememberedSetReferences(false),
#endif /* J9VM_GC_MODRON_SCAVENGER */	 
		_classDataAsRoots(true),
		_includeJVMTIObjectTagTables(true),
		_trackVisibleStackFrameDepth(false),
		_scanningEntity(RootScannerEntity_None),
		_lastScannedEntity(RootScannerEntity_None)
	{
		_typeId = __FUNCTION__;
	}
	
#if defined(J9VM_GC_MODRON_SCAVENGER)
	/** Set whether the iterator will only scan structures which contain nursery references */
	void setNurseryReferencesOnly(bool nurseryReferencesOnly) {
		_nurseryReferencesOnly = nurseryReferencesOnly;
	}

	/** Set whether the iterator will only scan structures which may contain nursery references */
	void setNurseryReferencesPossibly(bool nurseryReferencesPossibly) {
		_nurseryReferencesPossibly = nurseryReferencesPossibly;
	}

	/** Set whether the iterator will scan the remembered set references (if applicable to the scan type) */
	void setIncludeRememberedSetReferences(bool includeRememberedSetReferences) {
		_includeRememberedSetReferences = includeRememberedSetReferences;
	}
#endif /* J9VM_GC_MODRON_SCAVENGER */

	/** Set whether the iterator will scan stack frames for the classes of running methods */
	void setIncludeStackFrameClassReferences(bool includeStackFrameClassReferences) {
		_includeStackFrameClassReferences = includeStackFrameClassReferences;
	}

#if defined(J9VM_OPT_JVMTI)
	/** Set whether the iterator will scan the JVMTIObjectTagTables (if applicable to the scan type) */
	void setIncludeJVMTIObjectTagTables(bool includeJVMTIObjectTagTables) {
		_includeJVMTIObjectTagTables = includeJVMTIObjectTagTables;
	}
#endif /* J9VM_OPT_JVMTI */

	/** Set whether the iterator will scan the JVMTIObjectTagTables (if applicable to the scan type) */
	void setTrackVisibleStackFrameDepth(bool trackVisibleStackFrameDepth) {
		 _trackVisibleStackFrameDepth = trackVisibleStackFrameDepth;
	}

	/** General object slot handler to be reimplemented by specializing class. This handler is called for every reference to a J9Object. */
	virtual void doSlot(J9Object** slotPtr) = 0;

	/** General class slot handler to be reimplemented by specializing class. This handler is called for every reference to a J9Class. */
	virtual void doClassSlot(J9Class** slotPtr);

	/** General class handler to be reimplemented by specializing class. This handler is called once per class. */
	virtual void doClass(J9Class *clazz) = 0;
	
	virtual void doObject(J9Object* slotPtr) = 0;

#if defined(J9VM_GC_MODRON_SCAVENGER)
	virtual void scanRememberedSet();
#endif /* J9VM_GC_MODRON_SCAVENGER */

	virtual void scanClasses();
	virtual void scanVMClassSlots();

 	virtual bool scanOneThread(J9VMThread* walkThread);
	
	virtual void scanClassLoaders();
	virtual void scanThreads();
#if defined(J9VM_GC_FINALIZATION)
	void scanFinalizableObjects();
	virtual void scanUnfinalizedObjects();
#endif /* J9VM_GC_FINALIZATION */
	/**
	 * @todo Provide function documentation
	 *
	 * @NOTE this code is not thread safe and assumes it is called in a single threaded
	 * manner.
	 *
	 * @NOTE this can only be used as a READ-ONLY version of the ownableSynchronizerObjectList.
	 * If you need to modify elements with-in the list you will need to provide your own functionality.
	 * See MM_MarkingScheme::scanOwnableSynchronizerObjects(MM_EnvironmentStandard *env) as an example
	 * which modifies elements with-in the list.
	 */
	virtual void scanOwnableSynchronizerObjects();
	void scanStringTable();
	void scanJNIGlobalReferences();
	void scanJNIWeakGlobalReferences();

    virtual void scanMonitorReferences();

#if defined(J9VM_OPT_JVMTI)
	void scanJVMTIObjectTagTables();
#endif /* J9VM_OPT_JVMTI */ 

	virtual void doClassLoader(J9ClassLoader *classLoader);

#if defined(J9VM_GC_FINALIZATION)
	virtual void doFinalizableObject(J9Object *objectPtr);
	virtual void doUnfinalizedObject(J9Object *objectPtr);
#endif /* J9VM_GC_FINALIZATION */
	/**
	 * @todo Provide function documentation
	 */
	virtual void doOwnableSynchronizerObject(J9Object *objectPtr);

	virtual void doMonitorReference(J9ObjectMonitor *objectMonitor, GC_HashTableIterator *monitorReferenceIterator);

	virtual void doJNIWeakGlobalReference(J9Object **slotPtr);
	virtual void doJNIGlobalReferenceSlot(J9Object **slotPtr, GC_JNIGlobalReferenceIterator *jniGlobalReferenceIterator);

#if defined(J9VM_GC_MODRON_SCAVENGER)
	virtual void doRememberedSetSlot(J9Object **slotPtr, GC_RememberedSetSlotIterator *rememberedSetSlotIterator);
#endif /* defined(J9VM_GC_MODRON_SCAVENGER) */

#if defined(J9VM_OPT_JVMTI)
	virtual void doJVMTIObjectTagSlot(J9Object **slotPtr, GC_JVMTIObjectTagTableIterator *objectTagTableIterator);
#endif /* J9VM_OPT_JVMTI */

	virtual void doStringTableSlot(J9Object **slotPtr, GC_StringTableIterator *stringTableIterator);
	virtual void doVMClassSlot(J9Class **slotPtr, GC_VMClassSlotIterator *vmClassSlotIterator);
	virtual void doVMThreadSlot(J9Object **slotPtr, GC_VMThreadIterator *vmThreadIterator);
};


#endif /* HEAPROOTSCANNER_HPP_ */
