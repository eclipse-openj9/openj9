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

#include "CheckEngine.hpp"
#include "CheckVMThreadStacks.hpp"
#include "ModronTypes.hpp"
#include "ScanFormatter.hpp"

/**
 * Wrapper for GC_CheckEngine::checkSlot being used by GC_VMThreadStackSlotIterator::scanSlots
 * 
 * @param objectIndirect the slot to be verified
 * @param localData the same value that was passed to GC_VMThreadStackSlotIterator::scanSlots
 * in the <code>userData</code> parameter 
 * @param isDerivedPointer legacy - not used
 * @param objectIndirectBase the object that contains the slot
 * 
 * @see GC_VMThreadStackSlotIterator::scanSlots
 */
void
checkStackSlotIterator(J9JavaVM *javaVM, J9Object **objectIndirect, void *localData, J9StackWalkState *walkState, const void *stackLocation)
{
	GC_CheckVMThreadStacks::checkStackIteratorData *checkStackIteratorData = (GC_CheckVMThreadStacks::checkStackIteratorData *)localData;
	
	if (J9MODRON_SLOT_ITERATOR_RECOVERABLE_ERROR == checkStackIteratorData->gcCheck->checkSlotStack(checkStackIteratorData->gcCheck->getJavaVM(), objectIndirect, checkStackIteratorData->walkThread, stackLocation)) {
		checkStackIteratorData->numberOfErrors += 1;
	}
}

/**
 * Wrapper for GC_CheckEngine::checkSlot being used by GC_VMThreadStackSlotIterator::scanSlots
 * 
 * @param objectIndirect the slot to be printed
 * @param localData the same value that was passed to GC_VMThreadStackSlotIterator::scanSlots
 * in the <code>userData</code> parameter 
 * @param isDerivedPointer legacy - not used
 * @param objectIndirectBase the object that contains the slot
 * 
 * @see GC_VMThreadStackSlotIterator::scanSlots
 */
void
printStackSlotIterator(J9JavaVM *javaVM, J9Object **objectIndirect, void *localData, J9StackWalkState *walkState, const void *stackLocation)
{
	GC_CheckVMThreadStacks::printStackIteratorData *printStackIteratorData = (GC_CheckVMThreadStacks::printStackIteratorData *)localData;
	
	printStackIteratorData->scanFormatter->entry((void *) *objectIndirect);
}

GC_Check *
GC_CheckVMThreadStacks::newInstance(J9JavaVM *javaVM, GC_CheckEngine *engine)
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(javaVM)->getForge();
	
	GC_CheckVMThreadStacks *check = (GC_CheckVMThreadStacks *) forge->allocate(sizeof(GC_CheckVMThreadStacks), MM_AllocationCategory::DIAGNOSTIC, J9_GET_CALLSITE());
	if(check) {
		new(check) GC_CheckVMThreadStacks(javaVM, engine);
	}
	return check;
}

void
GC_CheckVMThreadStacks::kill()
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(_javaVM)->getForge();
	forge->free(this);
}

void
GC_CheckVMThreadStacks::check()
{
	GC_VMThreadListIterator vmThreadListIterator(_javaVM);			
	J9VMThread *walkThread;
#if defined(J9VM_INTERP_VERBOSE)
	bool doStackDump = _engine->isStackDumpAlwaysDisplayed();
#endif /* J9VM_INTERP_VERBOSE */
		
	while((walkThread = vmThreadListIterator.nextVMThread()) != NULL) {
		J9VMThread *toScanWalkThread;
		checkStackIteratorData localData = { _engine, walkThread, 0 };
		toScanWalkThread = walkThread;
		if (NULL != toScanWalkThread) {
			GC_VMThreadStackSlotIterator::scanSlots(toScanWalkThread, toScanWalkThread, (void *)&localData, checkStackSlotIterator, false, false);	

#if defined(J9VM_INTERP_VERBOSE)
			if (_javaVM->verboseStackDump && (doStackDump || (localData.numberOfErrors > 0))) {
				_javaVM->verboseStackDump(toScanWalkThread, "bad object detected on stack");
			}
#endif /* J9VM_INTERP_VERBOSE */
		}
	}
}

void
GC_CheckVMThreadStacks::print()
{
	GC_VMThreadListIterator newVMThreadListIterator(_javaVM);
	J9VMThread *walkThread;

	/* call into the VM to print slots and stacks for each thread. */
	GC_ScanFormatter formatter(_portLibrary, "thread stacks");
	while((walkThread = newVMThreadListIterator.nextVMThread()) != NULL) {

		/* first we print the stack slots */
		formatter.section("thread slots", (void *) walkThread);
		J9VMThread *toScanWalkThread;
		printStackIteratorData localData = { &formatter, walkThread};
		toScanWalkThread = walkThread;
		GC_VMThreadStackSlotIterator::scanSlots(toScanWalkThread, toScanWalkThread, (void *)&localData, printStackSlotIterator, false, false);
		formatter.endSection();
		
		/* then we print the stack trace itself */
		formatter.section("thread stack", (void *) walkThread);		
		_javaVM->internalVMFunctions->dumpStackTrace(walkThread);
		formatter.endSection();
	}
	formatter.end("thread stacks");
}

