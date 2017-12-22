
/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
#include "j9port.h"
#include "Tgc.hpp"
#include "mmhook.h"

#include "TgcInterRegionReferences.hpp"

#include "ClassLoaderRememberedSet.hpp"
#include "CompactGroupManager.hpp"
#include "CycleState.hpp"
#include "EnvironmentVLHGC.hpp"
#include "GCExtensions.hpp"
#include "HeapMapIterator.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"
#include "HeapRegionIteratorVLHGC.hpp"
#include "HeapRegionManager.hpp"
#include "MarkMap.hpp"
#include "MixedObjectIterator.hpp"
#include "PointerArrayIterator.hpp"
#include "TgcExtensions.hpp"


/**
 * Walk the heap to count inter-region references and then output some statistics
 */
static void
tgcHookReportInterRegionReferenceCounting(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData)
{
	MM_CopyForwardStartEvent* event = (MM_CopyForwardStartEvent *)eventData;
	J9VMThread *vmThread = static_cast<J9VMThread*>(event->currentThread->_language_vmthread);
	J9JavaVM *javaVM = vmThread->javaVM;
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(vmThread);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);

	UDATA heapObjectCount = 0;
	UDATA heapInSlotCount = 0;
	UDATA heapOutSlotCount = 0;
	UDATA heapOutObjectCount = 0;
	UDATA heapBeyondSlotCount = 0;
	UDATA heapBeyondObjectCount = 0;
	UDATA heapCardCount = 0;

	MM_MarkMap *markMap = env->_cycleState->_markMap;
	MM_HeapRegionManager *regionManager = extensions->heapRegionManager;
	GC_HeapRegionIteratorVLHGC regionIterator(regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->containsObjects()) {
			UDATA objectCount = 0;
			UDATA inSlotCount = 0;
			UDATA outSlotCount = 0;
			UDATA outObjectCount = 0;
			UDATA beyondSlotCount = 0;
			UDATA beyondObjectCount = 0;
			UDATA cardCount = 0;

			UDATA regionCompactGroup = MM_CompactGroupManager::getCompactGroupNumber(env, region);
			UDATA *regionBase = (UDATA *)region->getLowAddress();
			UDATA *regionTop = (UDATA *)region->getHighAddress();

			MM_HeapMapIterator iterator = MM_HeapMapIterator(extensions, markMap, regionBase, regionTop, false);
			J9Object *object = NULL;
			void *lastRSCLObject = NULL;
			while (NULL != (object = iterator.nextObject())) {
				bool cardDoesPointOut = false;
				objectCount += 1;
				/* first, check the validity of the object's class */
				J9Class *clazz = J9GC_J9OBJECT_CLAZZ(object);
				Assert_MM_true((UDATA)0x99669966 == clazz->eyecatcher);
				/* second, verify that it is an instance of a marked class */
				J9Object *classObject = (J9Object *)clazz->classObject;
				Assert_MM_true(markMap->isBitSet(classObject));

				/* now that we know the class is valid, verify that this object only refers to other marked objects */
				switch(extensions->objectModel.getScanType(object)) {
				case GC_ObjectModel::SCAN_REFERENCE_MIXED_OBJECT:
					Assert_MM_true(GC_ObjectModel::REF_STATE_REMEMBERED != J9GC_J9VMJAVALANGREFERENCE_STATE(env, object));
					/* fall through */
				case GC_ObjectModel::SCAN_ATOMIC_MARKABLE_REFERENCE_OBJECT:
				case GC_ObjectModel::SCAN_MIXED_OBJECT:
				case GC_ObjectModel::SCAN_OWNABLESYNCHRONIZER_OBJECT:
				case GC_ObjectModel::SCAN_CLASS_OBJECT:
				case GC_ObjectModel::SCAN_CLASSLOADER_OBJECT:
				{
					Assert_MM_true(extensions->classLoaderRememberedSet->isInstanceRemembered(env, object));

					GC_MixedObjectIterator mixedObjectIterator(javaVM->omrVM, object);
					GC_SlotObject *slotObject = NULL;
					UDATA thisObjectIn = 0;
					UDATA thisObjectOut = 0;
					UDATA thisObjectBeyond = 0;
					bool doesPointOut = false;
					bool doesPointBeyond = false;

					while (NULL != (slotObject = mixedObjectIterator.nextSlot())) {
						J9Object *target = slotObject->readReferenceFromSlot();
						if (NULL != target) {
							MM_HeapRegionDescriptorVLHGC *dest = (MM_HeapRegionDescriptorVLHGC *)regionManager->tableDescriptorForAddress(target);
							if (dest == region) {
								inSlotCount += 1;
								thisObjectIn += 1;
							} else if (MM_CompactGroupManager::getCompactGroupNumber(env, dest) == regionCompactGroup) {
								doesPointOut = true;
								cardDoesPointOut = true;
								outSlotCount += 1;
								thisObjectOut += 1;
							} else {
								doesPointBeyond = true;
								cardDoesPointOut = true;
								beyondSlotCount += 1;
								thisObjectBeyond += 1;
							}
						}
					}
					/* note that this will double-count objects which point both out _and_ beyond but that data is considered more valuable than only counting it as one type of external reference */
					if (doesPointOut) {
						outObjectCount += 1;
					}
					if (doesPointBeyond) {
						beyondObjectCount += 1;
					}
				}
				break;
				case GC_ObjectModel::SCAN_POINTER_ARRAY_OBJECT:
				{
					Assert_MM_true(extensions->classLoaderRememberedSet->isInstanceRemembered(env, object));

					GC_PointerArrayIterator pointerArrayIterator(javaVM, object);
					GC_SlotObject *slotObject = NULL;
					UDATA thisObjectIn = 0;
					UDATA thisObjectOut = 0;
					UDATA thisObjectBeyond = 0;
					bool doesPointOut = false;
					bool doesPointBeyond = false;

					while (NULL != (slotObject = pointerArrayIterator.nextSlot())) {
						J9Object *target = slotObject->readReferenceFromSlot();
						if (NULL != target) {
							MM_HeapRegionDescriptorVLHGC *dest = (MM_HeapRegionDescriptorVLHGC *)regionManager->tableDescriptorForAddress(target);
							if (dest == region) {
								inSlotCount += 1;
								thisObjectIn += 1;
							} else if (MM_CompactGroupManager::getCompactGroupNumber(env, dest) == regionCompactGroup) {
								doesPointOut = true;
								cardDoesPointOut = true;
								outSlotCount += 1;
								thisObjectOut += 1;
							} else {
								doesPointBeyond = true;
								cardDoesPointOut = true;
								beyondSlotCount += 1;
								thisObjectBeyond += 1;
							}
						}
					}
					/* note that this will double-count objects which point both out _and_ beyond but that data is considered more valuable than only counting it as one type of external reference */
					if (doesPointOut) {
						outObjectCount += 1;
					}
					if (doesPointBeyond) {
						beyondObjectCount += 1;
					}
				}
				break;
				case GC_ObjectModel::SCAN_PRIMITIVE_ARRAY_OBJECT:
					/* nothing to do */
					break;
				default:
					Assert_MM_unreachable();
				}
				if (cardDoesPointOut) {
					if (NULL != lastRSCLObject) {
						if (((UDATA)lastRSCLObject >> CARD_SIZE_SHIFT) != ((UDATA)object >> CARD_SIZE_SHIFT)) {
							cardCount += 1;
						}
					}
					lastRSCLObject = object;
				}
			}
			if (NULL != lastRSCLObject) {
				cardCount += 1;
			}
			j9tty_printf(PORTLIB, "Region based %p (group %zu) has %zu objects.  %zu \"in\" slots, %zu \"out\" slots (%zu objects), %zu \"beyond\" slots (%zu objects).  Creates %zu out/beyond cards\n", regionBase, regionCompactGroup, objectCount, inSlotCount, outSlotCount, outObjectCount, beyondSlotCount, beyondObjectCount, cardCount);
			heapObjectCount += objectCount;
			heapInSlotCount += inSlotCount;
			heapOutSlotCount += outSlotCount;
			heapOutObjectCount += outObjectCount;
			heapBeyondSlotCount += beyondSlotCount;
			heapBeyondObjectCount += beyondObjectCount;
			heapCardCount += cardCount;
		}
	}
	j9tty_printf(PORTLIB, "Total heap has %zu objects.  %zu \"in\" slots, %zu \"out\" slots (%zu objects), %zu \"beyond\" slots (%zu objects).  Creates %zu out/beyond cards\n", heapObjectCount, heapInSlotCount, heapOutSlotCount, heapOutObjectCount, heapBeyondSlotCount, heapBeyondObjectCount, heapCardCount);
}


/**
 * Initialize inter region reference tgc tracing.
 */
bool
tgcInterRegionReferencesInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	bool result = true;

	J9HookInterface** privateHooks = J9_HOOK_INTERFACE(extensions->privateHookInterface);
	(*privateHooks)->J9HookRegisterWithCallSite(privateHooks, J9HOOK_MM_PRIVATE_COPY_FORWARD_END, tgcHookReportInterRegionReferenceCounting, OMR_GET_CALLSITE(), javaVM);
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	j9tty_printf(PORTLIB, "TGC inter-region references initialized.\nLegend:\n"
			"\t\"in\" slot refers to a slot which points into the same region which contains its object\n"
			"\t\"out\" slot refers to a slot which points into a different region from that which contains its object, yet is in the same compact group\n"
			"\t\"beyond\" slot refers to a slot which points into a different region from that which contains its object which is also in a different compact group\n"
			"\t\"out\" or \"beyond\" objects contain \"out\" or \"beyond\" slots, respectively (an object which contains both will be in both totals)\n"
			);

	return result;
}

void
tgcInterRegionReferencesTearDown(J9JavaVM *javaVM)
{
}
