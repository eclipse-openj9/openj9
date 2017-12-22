
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

#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "mmhook_internal.h"
#include "modronopt.h"

#include "VMInterface.hpp"

#include "ClassHeapIterator.hpp"
#include "ClassStaticsIterator.hpp"
#include "Dispatcher.hpp"
#if defined(J9VM_GC_FINALIZATION)
#include "FinalizeListManager.hpp"
#endif /* J9VM_GC_FINALIZATION*/
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "GlobalCollector.hpp"
#include "MemoryPool.hpp"
#include "ModronTypes.hpp"
#include "ObjectHeapIterator.hpp"
#include "ObjectModel.hpp"
#include "SublistPool.hpp"
#include "SublistPuddle.hpp"
#include "SublistIterator.hpp"
#include "SublistSlotIterator.hpp"
#include "Task.hpp"
#include "VMClassSlotIterator.hpp"
#include "VMThreadListIterator.hpp"
#include "VMThreadInterface.hpp"
#include "ObjectAllocationInterface.hpp"

/**
 * Return the hook interface for the Memory Manager
 */
J9HookInterface** 
GC_VMInterface::getHookInterface(MM_GCExtensions *extensions)
{
	return J9_HOOK_INTERFACE(extensions->hookInterface);
}

/**
 * Acquire exclusive access to the class table.
 */
void
GC_VMInterface::lockClassTable(MM_GCExtensions *extensions)
{
#if defined(J9VM_THR_PREEMPTIVE)
	omrthread_monitor_enter(((J9JavaVM *)extensions->getOmrVM()->_language_vm)->classTableMutex);
#endif /* J9VM_THR_PREEMPTIVE */
}

/**
 * Release exclusive access to the class table.
 */
void
GC_VMInterface::unlockClassTable(MM_GCExtensions *extensions)
{
#if defined(J9VM_THR_PREEMPTIVE)
	omrthread_monitor_exit(((J9JavaVM *)extensions->getOmrVM()->_language_vm)->classTableMutex);
#endif /* J9VM_THR_PREEMPTIVE */ 	
}

/**
 * Acquire exclusive access to the class memory segment list.
 */
void
GC_VMInterface::lockClassMemorySegmentList(MM_GCExtensions *extensions)
{
#if defined(J9VM_THR_PREEMPTIVE)
	omrthread_monitor_enter(((J9JavaVM *)extensions->getOmrVM()->_language_vm)->classMemorySegments->segmentMutex);
#endif /* J9VM_THR_PREEMPTIVE */	
}

/**
 * Release exclusive access to the class memory segment list.
 */
void
GC_VMInterface::unlockClassMemorySegmentList(MM_GCExtensions *extensions)
{
#if defined(J9VM_THR_PREEMPTIVE)
	omrthread_monitor_exit(((J9JavaVM *)extensions->getOmrVM()->_language_vm)->classMemorySegments->segmentMutex);
#endif /* J9VM_THR_PREEMPTIVE */		
}

/**
 * Acquire exclusive access to the class table and the class 
 * memory segment list. Acquiring this lock is sufficient to safely 
 * walk the class heap concurrently with other threads that have VM access.
 */
void
GC_VMInterface::lockClasses(MM_GCExtensions *extensions)
{
	/* Must lock and unlock in this specific order to avoid deadlock */
	lockClassTable(extensions);
	lockClassMemorySegmentList(extensions);	
}

/**
 * Release exclusive access to class table and class memory segment list.
 */
void
GC_VMInterface::unlockClasses(MM_GCExtensions *extensions)
{
	/* Must lock and unlock in this specific order to avoid deadlock */
	unlockClassMemorySegmentList(extensions);	
	unlockClassTable(extensions);
}

/**
 * Acquire exclusive access to JNI global references.
 */
void
GC_VMInterface::lockJNIGlobalReferences(MM_GCExtensions *extensions)
{
#if defined(J9VM_THR_PREEMPTIVE)
	omrthread_monitor_enter(((J9JavaVM *)extensions->getOmrVM()->_language_vm)->jniFrameMutex);
#endif /* J9VM_THR_PREEMPTIVE */
}

/**
 * Release exclusive access to JNI global references.
 */
void
GC_VMInterface::unlockJNIGlobalReferences(MM_GCExtensions *extensions)
{
#if defined(J9VM_THR_PREEMPTIVE)
	omrthread_monitor_exit(((J9JavaVM *)extensions->getOmrVM()->_language_vm)->jniFrameMutex);
#endif /* J9VM_THR_PREEMPTIVE */
}

/**
 * Acquire exclusive access to the VM thread list.
 * @note The caller must not try to acquire exclusive VM access
 * while holding this lock.
 */
void
GC_VMInterface::lockVMThreadList(MM_GCExtensions *extensions)
{
#if defined(J9VM_THR_PREEMPTIVE)
	omrthread_monitor_enter(((J9JavaVM *)extensions->getOmrVM()->_language_vm)->vmThreadListMutex);
#endif /* J9VM_THR_PREEMPTIVE */
}

/**
 * Release exclusive access to the VM thread list.
 */
void
GC_VMInterface::unlockVMThreadList(MM_GCExtensions *extensions)
{
#if defined(J9VM_THR_PREEMPTIVE)
	omrthread_monitor_exit(((J9JavaVM *)extensions->getOmrVM()->_language_vm)->vmThreadListMutex);
#endif /* J9VM_THR_PREEMPTIVE */
}

/**
 * Acquire exclusive access to the finalize list.
 */
void
GC_VMInterface::lockFinalizeList(MM_GCExtensions *extensions)
{
#if defined(J9VM_THR_PREEMPTIVE) && defined(J9VM_GC_FINALIZATION)
	extensions->finalizeListManager->lock();
#endif /* J9VM_THR_PREEMPTIVE && J9VM_GC_FINALIZATION */
}

/**
 * Release exclusive access to the finalize list.
 */
void
GC_VMInterface::unlockFinalizeList(MM_GCExtensions *extensions)
{
#if defined(J9VM_THR_PREEMPTIVE) && defined(J9VM_GC_FINALIZATION)
	extensions->finalizeListManager->unlock();
#endif /* J9VM_THR_PREEMPTIVE && J9VM_GC_FINALIZATION */
}

/**
 * Acquire exclusive access to the class loader.
 */
void
GC_VMInterface::lockClassLoaders(MM_GCExtensions *extensions)
{
#if defined(J9VM_THR_PREEMPTIVE)
	omrthread_monitor_enter(((J9JavaVM *)extensions->getOmrVM()->_language_vm)->classLoaderBlocksMutex);
#endif /* J9VM_THR_PREEMPTIVE */
}

/**
 * Release exclusive access to the class loader.
 */
void
GC_VMInterface::unlockClassLoaders(MM_GCExtensions *extensions)
{
#if defined(J9VM_THR_PREEMPTIVE)
	omrthread_monitor_exit(((J9JavaVM *)extensions->getOmrVM()->_language_vm)->classLoaderBlocksMutex);
#endif /* J9VM_THR_PREEMPTIVE */
}

