
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

#include "CheckEngine.hpp"
#include "CheckJVMTIObjectTagTables.hpp"
#include "ModronTypes.hpp"
#include "ScanFormatter.hpp"

#if defined(J9VM_OPT_JVMTI)

GC_Check *
GC_CheckJVMTIObjectTagTables::newInstance(J9JavaVM *javaVM, GC_CheckEngine *engine)
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(javaVM)->getForge();
	
	GC_CheckJVMTIObjectTagTables *check = (GC_CheckJVMTIObjectTagTables *) forge->allocate(sizeof(GC_CheckJVMTIObjectTagTables), MM_AllocationCategory::DIAGNOSTIC, J9_GET_CALLSITE());
	if(check) {
		new(check) GC_CheckJVMTIObjectTagTables(javaVM, engine);
	}
	return check;
}

void
GC_CheckJVMTIObjectTagTables::kill()
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(_javaVM)->getForge();
	forge->free(this);
}

void
GC_CheckJVMTIObjectTagTables::check()
{
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(_javaVM);
	J9JVMTIEnv * jvmtiEnv;
	J9Object **slotPtr;	
	if (NULL != jvmtiData) {
		GC_JVMTIObjectTagTableListIterator objectTagTableList(jvmtiData->environments);
		while(NULL != (jvmtiEnv = (J9JVMTIEnv *)objectTagTableList.nextSlot())) {
			GC_JVMTIObjectTagTableIterator objectTagTableIterator(jvmtiEnv->objectTagTable);
			while(NULL != (slotPtr = (J9Object **)objectTagTableIterator.nextSlot())) {
				if (_engine->checkSlotPool(_javaVM, slotPtr, jvmtiEnv->objectTagTable) != J9MODRON_SLOT_ITERATOR_OK ){
					return;
				}
			}
		}
	}
}

void
GC_CheckJVMTIObjectTagTables::print()
{
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(_javaVM);
	J9JVMTIEnv * jvmtiEnv;
	J9Object **slotPtr;	

	if (NULL != jvmtiData) {
		GC_ScanFormatter formatter(_portLibrary, "jvmtiObjectTagTables", (void *) jvmtiData);

		GC_JVMTIObjectTagTableListIterator objectTagTableList(jvmtiData->environments);
		while(NULL != (jvmtiEnv = (J9JVMTIEnv *)objectTagTableList.nextSlot())) {
			GC_JVMTIObjectTagTableIterator objectTagTableIterator(jvmtiEnv->objectTagTable);
			while(NULL != (slotPtr = (J9Object **)objectTagTableIterator.nextSlot())) {
				formatter.entry((void *)*slotPtr);
			}
		}

		formatter.end("jvmtiObjectTagTables", (void *)jvmtiData);
	}
}

#endif /* J9VM_OPT_JVMTI */
