
/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#include <string.h>

#include "GCExtensions.hpp"
#include "TgcExtensions.hpp"

#include "Dispatcher.hpp"
#include "EnvironmentBase.hpp"
#include "HashTableIterator.hpp"
#include "HeapMapIterator.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"
#include "HeapRegionIteratorVLHGC.hpp"
#include "HeapRegionManager.hpp"
#include "ParallelTask.hpp"

struct ClassTableEntry {
	J9Class *clazz; /**< The class represented by this entry */
	UDATA rememberedInstances; /**< The number of instances of the class with references out of their own region */
	UDATA totalInstances; /**< The total number of instances of the class encountered */

	static UDATA hash(void *key, void *userData) { return (UDATA)((ClassTableEntry*)key)->clazz; }
	static UDATA equal(void *leftKey, void *rightKey, void *userData) { return ((ClassTableEntry*)leftKey)->clazz == ((ClassTableEntry*)rightKey)->clazz; }  
};

static void reportInterRegionRememberedSetDemographics(MM_EnvironmentBase *env);
static void tgcHookIncrementStart(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData);
static void insertIntoSortedList(ClassTableEntry* entry, ClassTableEntry* list, UDATA listCount);

class TgcParallelHeapWalkTask : public MM_ParallelTask
{
	/* Data Members */
private:
protected:
public:

	/* Member Functions */
private:
protected:
public:
	virtual UDATA getVMStateID(void) { return OMRVMSTATE_GC_TGC; }
	virtual void run(MM_EnvironmentBase *env);

	TgcParallelHeapWalkTask(MM_EnvironmentBase *env, MM_Dispatcher *dispatcher)
		: MM_ParallelTask(env, dispatcher)
	{
		_typeId = __FUNCTION__;
	}
};

void
TgcParallelHeapWalkTask::run(MM_EnvironmentBase *env)
{
	UDATA errorCount = 0;
	UDATA totalRememberedObjects = 0;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	MM_HeapMap *markMap = extensions->previousMarkMap;
	J9HashTable *hashTable = hashTableNew(env->getPortLibrary(), J9_GET_CALLSITE(), 8*1024, sizeof(ClassTableEntry), sizeof(char *), 0, OMRMEM_CATEGORY_MM, ClassTableEntry::hash, ClassTableEntry::equal, NULL, NULL);
	
	if (NULL == hashTable) {
		errorCount += 1;
	} else {
		GC_HeapRegionIteratorVLHGC regionIterator(extensions->heapRegionManager);
		MM_HeapRegionDescriptorVLHGC *region = NULL;
		while(NULL != (region = regionIterator.nextRegion())) {
			if (region->hasValidMarkMap()) {
				if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
					MM_HeapMapIterator mapIterator(extensions, markMap, (UDATA *)region->getLowAddress(), (UDATA *)region->getHighAddress(), false);
					J9Object *objectPtr = NULL;
					while(NULL != (objectPtr = mapIterator.nextObject())) {
						ClassTableEntry exemplar;
						exemplar.clazz = J9GC_J9OBJECT_CLAZZ(objectPtr);
						exemplar.rememberedInstances = 0;
						exemplar.totalInstances = 0;
						ClassTableEntry *entry = (ClassTableEntry *)hashTableAdd(hashTable, &exemplar);
			
						if (NULL == entry) {
							errorCount += 1;
						} else {
							entry->totalInstances += 1;
							if (extensions->objectModel.isRemembered(objectPtr)) {
								entry->rememberedInstances += 1;
								totalRememberedObjects += 1;
							}
						}
					}
				}
			}
		}
	}
	
	/*
	 * Merge data
	 */
	omrthread_monitor_enter(tgcExtensions->_interRegionRememberedSetDemographics.mutex);
	if (NULL != hashTable) {
		J9HashTable *sharedHashTable = tgcExtensions->_interRegionRememberedSetDemographics.classHashTable;
		GC_HashTableIterator hashTableIterator(hashTable);
		ClassTableEntry *entry = NULL;
		while(NULL != (entry = (ClassTableEntry*)hashTableIterator.nextSlot())) {
			if (entry->rememberedInstances > 0) {
				ClassTableEntry exemplar;
				exemplar.clazz = entry->clazz;
				exemplar.rememberedInstances = 0;
				exemplar.totalInstances = 0;
				ClassTableEntry *sharedEntry = (ClassTableEntry *)hashTableAdd(sharedHashTable, &exemplar);
				if (NULL == sharedEntry) {
					errorCount += 1;
				} else {
					sharedEntry->rememberedInstances += entry->rememberedInstances;
					sharedEntry->totalInstances += entry->totalInstances;
				}
			}
		}
	}
	tgcExtensions->_interRegionRememberedSetDemographics.errorCount += errorCount;
	tgcExtensions->_interRegionRememberedSetDemographics.totalRememberedObjects += totalRememberedObjects;
	omrthread_monitor_exit(tgcExtensions->_interRegionRememberedSetDemographics.mutex);

	if (NULL != hashTable) {
		hashTableFree(hashTable);
	}
}


/**
 * Report RSCL histogram prior to a collection
 */
static void
reportInterRegionRememberedSetDemographics(MM_EnvironmentBase *env)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	J9HashTable *hashTable = tgcExtensions->_interRegionRememberedSetDemographics.classHashTable;

	tgcExtensions->_interRegionRememberedSetDemographics.incrementCount += 1;
	tgcExtensions->_interRegionRememberedSetDemographics.errorCount = 0;
	tgcExtensions->_interRegionRememberedSetDemographics.totalRememberedObjects = 0;
	
	tgcExtensions->printf("<rememberedSetDemographics increment=\"%zu\">\n", tgcExtensions->_interRegionRememberedSetDemographics.incrementCount);

	MM_Dispatcher *dispatcher = extensions->dispatcher;
	TgcParallelHeapWalkTask heapWalkTask(env, dispatcher);
	dispatcher->run(env, &heapWalkTask);
	
	/* at this point, the helper threads have completed populating the shared hash table with the all classes with remembered instances */
	
	const UDATA listCount = 10;
	ClassTableEntry sortedResults[listCount];
	memset(sortedResults, 0, sizeof(sortedResults));

	GC_HashTableIterator hashTableIterator(hashTable);
	ClassTableEntry *entry = NULL;
	while(NULL != (entry = (ClassTableEntry*)hashTableIterator.nextSlot())) {
		Assert_MM_true(0 != entry->rememberedInstances);
		insertIntoSortedList(entry, sortedResults, listCount);
		hashTableIterator.removeSlot();
	}

	UDATA errorCount = tgcExtensions->_interRegionRememberedSetDemographics.errorCount;
	UDATA totalRememberedObjects = tgcExtensions->_interRegionRememberedSetDemographics.totalRememberedObjects;
	for (UDATA index = 0; index < listCount; index++) {
		entry = &sortedResults[index];
		J9Class *clazz = entry->clazz;
		if (NULL != clazz) {
			double fractionRemembered = (double)entry->rememberedInstances / (double)entry->totalInstances;
			double fractionOfAllRemembered = (double)entry->rememberedInstances / (double)totalRememberedObjects;
			tgcExtensions->printf("%8zu %.2f %.2f %p ", entry->rememberedInstances, fractionRemembered, fractionOfAllRemembered, clazz);
			if (J9GC_CLASS_IS_ARRAY(clazz)) {
				J9Class *leafComponentType = ((J9ArrayClass*)clazz)->leafComponentType;
				J9UTF8 *name = J9ROMCLASS_CLASSNAME(leafComponentType->romClass);
				UDATA arity = ((J9ArrayClass*)clazz)->arity;
				char brackets[256];
				Assert_MM_true(arity <= sizeof(brackets));
				memset(brackets, '[', sizeof(brackets));
				if (OBJECT_HEADER_SHAPE_MIXED == J9GC_CLASS_SHAPE(leafComponentType)) {
					tgcExtensions->printf("%.*sL%.*s;\n", arity, brackets, J9UTF8_LENGTH(name), J9UTF8_DATA(name));
				} else {
					tgcExtensions->printf("%.*s%.*s\n", arity, brackets, J9UTF8_LENGTH(name), J9UTF8_DATA(name));
				}
			} else {
				J9UTF8 *name = J9ROMCLASS_CLASSNAME(clazz->romClass);
				tgcExtensions->printf("%.*s\n", J9UTF8_LENGTH(name), J9UTF8_DATA(name));
			}
		}
	}
	
	if (0 != errorCount) {
		tgcExtensions->printf("WARNING: Failed to allocate records for %zu class(es).\n", errorCount);
	}

	tgcExtensions->printf("<rememberedSetDemographics/>\n");
}

static void 
insertIntoSortedList(ClassTableEntry* entry, ClassTableEntry* list, UDATA listCount)
{
	for (UDATA index = 0; index < listCount; index++) {
		if (entry->rememberedInstances > list[index].rememberedInstances) {
			memmove(&list[index + 1], &list[index], (listCount - index - 1) * sizeof(list[0]));
			list[index] = *entry;
			break;
		}
	}
}

static void
tgcHookIncrementStart(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData)
{
	OMR_VMThread *omrThread = ((MM_GCIncrementStartEvent*)eventData)->currentThread;
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrThread);
	reportInterRegionRememberedSetDemographics(env);
}


/**
 * Initialize inter region remembered set  tgc tracing.
 * Initializes the TgcInterRegionRememberedSetExtensions object associated with irrs tgc tracing. Attaches hooks
 * to the appropriate functions handling events used by irrs tgc tracing.
 */
bool
tgcInterRegionRememberedSetDemographicsInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	bool result = true;

	J9HashTable *hashTable = hashTableNew(OMRPORT_FROM_J9PORT(javaVM->portLibrary), J9_GET_CALLSITE(), 8*1024, sizeof(ClassTableEntry), sizeof(char *), 0, OMRMEM_CATEGORY_MM, ClassTableEntry::hash, ClassTableEntry::equal, NULL, NULL);
	tgcExtensions->_interRegionRememberedSetDemographics.classHashTable = hashTable;

	if (NULL != hashTable) {
		if (0 == omrthread_monitor_init_with_name(&tgcExtensions->_interRegionRememberedSetDemographics.mutex, 0, "InterRegionRememberedSetDemographics")) {
			J9HookInterface** privateHooks = J9_HOOK_INTERFACE(extensions->privateHookInterface);
			(*privateHooks)->J9HookRegisterWithCallSite(privateHooks, J9HOOK_MM_PRIVATE_GC_INCREMENT_START, tgcHookIncrementStart, OMR_GET_CALLSITE(), javaVM);
		} else {
			result = false;
		}
	} else {
		result = false;
	}

	tgcExtensions->_interRegionRememberedSetDemographics.incrementCount = 0;

	return result;
}

void
tgcInterRegionRememberedSetDemographicsTearDown(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);

	if (NULL != tgcExtensions->_interRegionRememberedSetDemographics.classHashTable) {
		hashTableFree(tgcExtensions->_interRegionRememberedSetDemographics.classHashTable);
		tgcExtensions->_interRegionRememberedSetDemographics.classHashTable = NULL;
	}
	
	if (NULL != tgcExtensions->_interRegionRememberedSetDemographics.mutex) {
		omrthread_monitor_destroy(tgcExtensions->_interRegionRememberedSetDemographics.mutex);
		tgcExtensions->_interRegionRememberedSetDemographics.mutex = NULL;

	}
}
