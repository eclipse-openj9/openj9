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
#include "CheckFinalizableList.hpp"
#include "FinalizeListManager.hpp"
#include "ModronTypes.hpp"
#include "ScanFormatter.hpp"

#if defined(J9VM_GC_FINALIZATION)

GC_Check *
GC_CheckFinalizableList::newInstance(J9JavaVM *javaVM, GC_CheckEngine *engine)
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(javaVM)->getForge();
	
	GC_CheckFinalizableList *check = (GC_CheckFinalizableList *) forge->allocate(sizeof(GC_CheckFinalizableList), MM_AllocationCategory::DIAGNOSTIC, J9_GET_CALLSITE());
	if(check) {
		new(check) GC_CheckFinalizableList(javaVM, engine);
	}
	return check;
}

void
GC_CheckFinalizableList::kill()
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(_javaVM)->getForge();
	forge->free(this);
}

void
GC_CheckFinalizableList::check()
{
	GC_FinalizeListManager *finalizeListManager = _extensions->finalizeListManager;
	{
		/* walk finalizable objects created by the system class loader */
		j9object_t systemObject = peekSystemFinalizableObject(finalizeListManager);
		while (NULL != systemObject) {
			if (_engine->checkSlotFinalizableList(_javaVM, &systemObject, finalizeListManager) != J9MODRON_SLOT_ITERATOR_OK ){
				return ;
			}
			systemObject = peekNextSystemFinalizableObject(finalizeListManager, systemObject);
		}
	}

	{
		/* walk finalizable objects created by all other class loaders*/
		j9object_t defaultObject = peekDefaultFinalizableObject(finalizeListManager);
		while (NULL != defaultObject) {
			if (_engine->checkSlotFinalizableList(_javaVM, &defaultObject, finalizeListManager) != J9MODRON_SLOT_ITERATOR_OK ){
				return ;
			}
			defaultObject = peekNextDefaultFinalizableObject(finalizeListManager, defaultObject);
		}
	}

	{
		/* walk reference objects */
		j9object_t referenceObject = finalizeListManager->peekReferenceObject();
		while (NULL != referenceObject) {
			if (_engine->checkSlotFinalizableList(_javaVM, &referenceObject, finalizeListManager) != J9MODRON_SLOT_ITERATOR_OK ){
				return ;
			}
			referenceObject = finalizeListManager->peekNextReferenceObject(referenceObject);
		}
	}
}

void
GC_CheckFinalizableList::print()
{
	GC_FinalizeListManager *finalizeListManager = _extensions->finalizeListManager;
	GC_ScanFormatter formatter(_portLibrary, "finalizableList");
	{
		/* walk finalizable objects created by the system class loader */
		formatter.section("finalizable objects created by the system classloader");
		j9object_t systemObject = peekSystemFinalizableObject(finalizeListManager);
		while (NULL != systemObject) {
			formatter.entry((void *) systemObject);
			systemObject = peekNextSystemFinalizableObject(finalizeListManager, systemObject);
		}
		formatter.endSection();
	}

	{
		formatter.section("finalizable objects created by application class loaders");
		/* walk finalizable objects created by all other class loaders*/
		j9object_t defaultObject = peekDefaultFinalizableObject(finalizeListManager);
		while (NULL != defaultObject) {
			formatter.entry((void *) defaultObject);
			defaultObject = peekNextDefaultFinalizableObject(finalizeListManager, defaultObject);
		}
		formatter.endSection();
	}

	{
		formatter.section("reference objects");
		/* walk reference objects */
		j9object_t referenceObject = finalizeListManager->peekReferenceObject();
		while (NULL != referenceObject) {
			formatter.entry((void *) referenceObject);
			referenceObject = finalizeListManager->peekNextReferenceObject(referenceObject);
		}
		formatter.endSection();
	}

	formatter.end("finalizableList");
}

j9object_t
GC_CheckFinalizableList::peekSystemFinalizableObject(GC_FinalizeListManager *finalizeListManager)
{
	return finalizeListManager->_systemFinalizableObjects;
}

j9object_t
GC_CheckFinalizableList::peekNextSystemFinalizableObject(GC_FinalizeListManager *finalizeListManager, j9object_t current)
{
	MM_GCExtensions *extensions = finalizeListManager->_extensions;
	MM_ObjectAccessBarrier *barrier = extensions->accessBarrier;
	return barrier->getFinalizeLink(current);
}

j9object_t
GC_CheckFinalizableList::peekDefaultFinalizableObject(GC_FinalizeListManager *finalizeListManager)
{
	return finalizeListManager->_defaultFinalizableObjects;
}

j9object_t
GC_CheckFinalizableList::peekNextDefaultFinalizableObject(GC_FinalizeListManager *finalizeListManager, j9object_t current)
{
	MM_GCExtensions *extensions = finalizeListManager->_extensions;
	MM_ObjectAccessBarrier *barrier = extensions->accessBarrier;
	return barrier->getFinalizeLink(current);
}

#endif /* J9VM_GC_FINALIZATION */
