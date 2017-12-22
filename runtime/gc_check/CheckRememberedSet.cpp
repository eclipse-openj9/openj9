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
#include "CheckRememberedSet.hpp"
#include "ModronTypes.hpp"
#include "ScanFormatter.hpp"

#if defined(J9VM_GC_GENERATIONAL)

GC_Check *
GC_CheckRememberedSet::newInstance(J9JavaVM *javaVM, GC_CheckEngine *engine)
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(javaVM)->getForge();
	
	GC_CheckRememberedSet *check = (GC_CheckRememberedSet *) forge->allocate(sizeof(GC_CheckRememberedSet), MM_AllocationCategory::DIAGNOSTIC, J9_GET_CALLSITE());
	if(check) {
		new(check) GC_CheckRememberedSet(javaVM, engine);
	}
	return check;
}

void
GC_CheckRememberedSet::kill()
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(_javaVM)->getForge();
	forge->free(this);
}

void
GC_CheckRememberedSet::check()
{
	J9Object **slotPtr;
	MM_SublistPuddle *puddle;	
	GC_RememberedSetIterator remSetIterator(&_extensions->rememberedSet);
	
	/* no point checking if the scavenger wasn't turned on */
	if(!_extensions->scavengerEnabled) {
		return;
	}

	while((puddle = remSetIterator.nextList()) != NULL) {
		GC_RememberedSetSlotIterator remSetSlotIterator(puddle);

		while((slotPtr = (J9Object **)remSetSlotIterator.nextSlot()) != NULL) {
			if (_engine->checkSlotRememberedSet(_javaVM, slotPtr, puddle) != J9MODRON_SLOT_ITERATOR_OK ){
				return;
			}
		}
	}
}

void
GC_CheckRememberedSet::print()
{
	GC_RememberedSetIterator remSetIterator(&_extensions->rememberedSet);
	J9Object **slotPtr;
	MM_SublistPuddle *puddle;
	GC_ScanFormatter formatter(_portLibrary, "RememberedSet Sublist", (void *)&_extensions->rememberedSet);
	while ((puddle = remSetIterator.nextList()) != NULL) {
		GC_RememberedSetSlotIterator remSetSlotIterator(puddle);
		formatter.section("puddle", (void *) puddle);

		while((slotPtr = (J9Object **)remSetSlotIterator.nextSlot()) != NULL) {
			formatter.entry((void *)*slotPtr);
		}
		formatter.endSection();
	}
	formatter.end("RememberedSet Sublist", (void *)&_extensions->rememberedSet);
}

#endif /* J9VM_GC_GENERATIONAL */
