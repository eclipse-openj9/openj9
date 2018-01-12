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
#include "HeapIteratorAPIRootIterator.hpp"
#include "HeapIteratorAPI.h"

void 
HeapIteratorAPI_RootIterator::scanAllSlots()
{	
	if(!_nurseryReferencesOnly && !_nurseryReferencesPossibly) {
		if( _flags & SCAN_CLASSES) {
			scanClasses();
		}
		if( _flags & SCAN_VM_CLASS_SLOTS) {
			scanVMClassSlots();
		}			
	}
	
	if(_flags & SCAN_CLASS_LOADERS) {
		scanClassLoaders();
	}
	
	if(_flags & SCAN_THREADS) {
		scanThreads();
	}

#if defined(J9VM_GC_FINALIZATION)
	if(_flags & SCAN_FINALIZABLE_OBJECTS) {
		/* TODO: MM_GCExtensionCore does not contain GC_FinalizeListManager, 
		 * which is required to walk finalizable objects.
		 */
		scanFinalizableObjects();
	}
#endif /* J9VM_GC_FINALIZATION */

	if(_flags & SCAN_JNI_GLOBAL) {
		scanJNIGlobalReferences();
	}
	if(!_nurseryReferencesOnly && !_nurseryReferencesPossibly) {
		if(_flags & SCAN_STRING_TABLE) {
			scanStringTable();
		}
	}

#if defined(J9VM_GC_FINALIZATION)
	if(_flags & SCAN_UNFINALIZABLE) {
		scanUnfinalizedObjects();
	}
#endif /* J9VM_GC_FINALIZATION */
		
	if(_flags & SCAN_MONITORS) {
		scanMonitorReferences();
	}

	if(_flags & SCAN_JNI_WEAK) {
		scanJNIWeakGlobalReferences();
	}

#if defined(J9VM_GC_MODRON_SCAVENGER)
	if(!_nurseryReferencesOnly && !_nurseryReferencesPossibly) {
		if(_flags & SCAN_REMEBERED_SET) {
			scanRememberedSet();
		}
	}
#endif /* J9VM_GC_MODRON_SCAVENGER */

#if defined(J9VM_OPT_JVMTI)
	if(_includeJVMTIObjectTagTables) {
		if(_flags & SCAN_JVMTI_OBJECT_TAG_TABLE) {
			/* TODO: The J9JVMTI_DATA_FROM_VM is OOP unsafe - convert code when it is. */
			scanJVMTIObjectTagTables();
		}
	}
#endif /* J9VM_OPT_JVMTI */

	if (_flags & SCAN_OWNABLE_SYNCHRONIZER) {
		scanOwnableSynchronizerObjects();
	}
}


void
HeapIteratorAPI_RootIterator::doSlot(J9Object** slotPtr)
{
	J9MM_HeapRootSlotDescriptor rootDesc;
	
	rootDesc.slotType = _scanningEntity;
	rootDesc.scanType = HEAP_ROOT_SLOT_DESCRIPTOR_OBJECT;
	rootDesc.slotReachability = _scanningEntityReachability;
			
	if( NULL != ((void *)*slotPtr) ) {
		_func((void *)*slotPtr, &rootDesc, _userData);
	}
}

void 
HeapIteratorAPI_RootIterator::doObject(J9Object* slotPtr)
{
	J9MM_HeapRootSlotDescriptor rootDesc;
	
	rootDesc.slotType = _scanningEntity;
	rootDesc.scanType = HEAP_ROOT_SLOT_DESCRIPTOR_OBJECT;
	rootDesc.slotReachability = _scanningEntityReachability;
	
	_func( (void *)slotPtr, &rootDesc, _userData);
	
}

void
HeapIteratorAPI_RootIterator::doClass(J9Class *clazz)
{
	J9MM_HeapRootSlotDescriptor rootDesc;
	
	rootDesc.slotType = _scanningEntity;
	rootDesc.scanType = HEAP_ROOT_SLOT_DESCRIPTOR_CLASS;
	rootDesc.slotReachability = _scanningEntityReachability;

	if( NULL != clazz ) {
		_func( (void *)clazz, &rootDesc, _userData);	
	}
}
