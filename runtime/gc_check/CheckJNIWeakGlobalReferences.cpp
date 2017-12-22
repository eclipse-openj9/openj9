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
#include "CheckJNIWeakGlobalReferences.hpp"
#include "ModronTypes.hpp"
#include "ScanFormatter.hpp"

GC_Check *
GC_CheckJNIWeakGlobalReferences::newInstance(J9JavaVM *javaVM, GC_CheckEngine *engine)
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(javaVM)->getForge();
	
	GC_CheckJNIWeakGlobalReferences *check = (GC_CheckJNIWeakGlobalReferences *) forge->allocate(sizeof(GC_CheckJNIWeakGlobalReferences), MM_AllocationCategory::DIAGNOSTIC, J9_GET_CALLSITE());
	if(check) {
		new(check) GC_CheckJNIWeakGlobalReferences(javaVM, engine);
	}
	return check;
}

void
GC_CheckJNIWeakGlobalReferences::kill()
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(_javaVM)->getForge();
	forge->free(this);
}

void
GC_CheckJNIWeakGlobalReferences::check()
{
	GC_JNIWeakGlobalReferenceIterator jniWeakGlobalReferenceIterator(_javaVM->jniWeakGlobalReferences);
	J9Object **slotPtr;
	while((slotPtr = (J9Object **)jniWeakGlobalReferenceIterator.nextSlot()) != NULL) {
		if (_engine->checkSlotPool(_javaVM, slotPtr, _javaVM->jniWeakGlobalReferences) != J9MODRON_SLOT_ITERATOR_OK ){
			return;
		}
	}
}

void
GC_CheckJNIWeakGlobalReferences::print()
{
	J9Pool *pool = _javaVM->jniWeakGlobalReferences;
	GC_PoolIterator poolReferenceIterator(pool);
	J9Object **slotPtr;

	GC_ScanFormatter formatter(_portLibrary, "jniWeakGlobalReferences", (void *) pool);
	while((slotPtr = (J9Object **)poolReferenceIterator.nextSlot()) != NULL) {
		formatter.entry((void *)*slotPtr);
	}
	formatter.end("jniWeakGlobalReferences", (void *)pool);
}
