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
#include "CheckVMThreads.hpp"
#include "ModronTypes.hpp"
#include "ScanFormatter.hpp"

GC_Check *
GC_CheckVMThreads::newInstance(J9JavaVM *javaVM, GC_CheckEngine *engine)
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(javaVM)->getForge();
	
	GC_CheckVMThreads *check = (GC_CheckVMThreads *) forge->allocate(sizeof(GC_CheckVMThreads), MM_AllocationCategory::DIAGNOSTIC, J9_GET_CALLSITE());
	if(check) {
		new(check) GC_CheckVMThreads(javaVM, engine);
	}
	return check;
}

void
GC_CheckVMThreads::kill()
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(_javaVM)->getForge();
	forge->free(this);
}

void
GC_CheckVMThreads::check()
{
	GC_VMThreadListIterator vmThreadListIterator(_javaVM);			
	J9Object **slotPtr;
	J9VMThread *walkThread;
		
	while((walkThread = vmThreadListIterator.nextVMThread()) != NULL) {
		GC_VMThreadIterator vmthreadIterator(walkThread);
		while ((slotPtr = vmthreadIterator.nextSlot()) != NULL ) {
			if (_engine->checkSlotVMThread(_javaVM, slotPtr, walkThread, check_type_other, &vmthreadIterator) != J9MODRON_SLOT_ITERATOR_OK ){
				return;
			}
		}
	}
}

void
GC_CheckVMThreads::print()
{
	GC_VMThreadListIterator vmThreadListIterator(_javaVM);
	J9Object **slotPtr;
	J9VMThread *walkThread;

	GC_ScanFormatter formatter(_portLibrary, "VMThread Slots");
	while((walkThread = vmThreadListIterator.nextVMThread()) != NULL) {
		GC_VMThreadIterator vmthreadIterator(walkThread);

		formatter.section("thread", (void *) walkThread);
		while((slotPtr = (J9Object **)vmthreadIterator.nextSlot()) != NULL) {
			formatter.entry((void *)*slotPtr);
		}
		formatter.endSection();
	}
	formatter.end("VMThread Slots");
}

