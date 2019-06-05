/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
#include "j9accessbarrier.h"
#include "j9protos.h"
#include "lockNurseryUtil.h"
#include "mmhook.h"
#include "ut_j9vm.h"
#include "vm_api.h"
#include "vm_internal.h"
#include "j9modron.h"

/* uncomment this to enable monitor table verbosity */
/* #define MONTABLE_TRACING
*/

#ifdef MONTABLE_TRACING
static UDATA hits, misses;
#define HIT() hits++
#define MISS() misses++
#define TRACE(message) j9tty_printf(PORTLIB, "%s in monitorTableAt in %p. Monitor=%p. Monitor-count=%d. Cache=%p (%d/%d)\n", (message), vmStruct, monitor, hashTableGetCount(vm->monitorTable), vmStruct->eventReportData1, hits, hits + misses);
#else
#define HIT()
#define MISS()
#define TRACE(message)
#endif

#ifdef J9VM_THR_LOCK_NURSERY
#define J9_OBJECT_MONITOR_LOOKUP_SLOT(object,vm) ( (((UDATA)object) >> vm->omrVM->_objectAlignmentShift) & (J9VMTHREAD_OBJECT_MONITOR_CACHE_SIZE-1))
#endif

static UDATA hashMonitorCompare (void *leftKey, void *rightKey, void *userData);
static UDATA hashMonitorDestroyDo (void *entry, void *opaque);
static UDATA hashMonitorHash (void *key, void *userData);
static J9HashTable* createMonitorTable(J9JavaVM *vm, char *tableName);


static UDATA
hashMonitorHash(void *key, void *userData)
{
	J9JavaVM *javaVM = (J9JavaVM *)userData;
	UDATA hash = ((J9ObjectMonitor *) key)->hash;
	if (0 == hash) {
		J9ThreadAbstractMonitor *monitor = (J9ThreadAbstractMonitor *) (((J9ObjectMonitor *) key)->monitor);
		j9object_t object = (j9object_t)monitor->userData;
		hash = (UDATA)(U_32)objectHashCode(javaVM, object);
		((J9ObjectMonitor *) key)->hash = (U_32)hash;
	}

	return hash;
}


static UDATA
hashMonitorCompare(void *tableEntryKey, void *userKey, void *userData)
{
	J9ThreadAbstractMonitor *tableEntryMonitor = (J9ThreadAbstractMonitor *) (((J9ObjectMonitor *) tableEntryKey)->monitor);
	J9ThreadAbstractMonitor *userMonitor = (J9ThreadAbstractMonitor *) (((J9ObjectMonitor *) userKey)->monitor);

	/* In a Concurrent GC where monitor object can *move* in a middle of GC cycle,
	 * we need a proper barrier to get an up-to-date location of the monitor object
	 * Only access to the table entry needs the barrier. The user provided key should already have an updated location of the objects,
	 * since a read barrier had to be executed some time prior to the construction of the key, wherever the value is read from */
	j9object_t tableEntryObject = J9MONITORTABLE_OBJECT_LOAD_VM((J9JavaVM *)userData, &(tableEntryMonitor->userData));

	return tableEntryObject == (j9object_t)userMonitor->userData;
}

#ifdef J9VM_THR_LOCK_NURSERY
void
cacheObjectMonitorForLookup(J9JavaVM* vm, J9VMThread* vmStruct, J9ObjectMonitor* objectMonitor)
{
	j9object_t object = J9MONITORTABLE_OBJECT_LOAD(vmStruct, &((J9ThreadAbstractMonitor*)(objectMonitor->monitor))->userData);

	vmStruct->objectMonitorLookupCache[J9_OBJECT_MONITOR_LOOKUP_SLOT(object,vm)] = (j9objectmonitor_t) ((UDATA) objectMonitor);
}
#endif



/**
 * Creates the monitor hashtable
 *
 * @param vm		the vm
 * @param tableName	the name to assign to the hashtable
 *
 * @return an initialized J9HashTable on success, otherwise NULL
 */
static J9HashTable*
createMonitorTable(J9JavaVM *vm, char *tableName)
{
	U_32 flags = 0;
	if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm)) {
		flags = J9HASH_TABLE_ALLOCATE_ELEMENTS_USING_MALLOC32;
	}
	Assert_VM_false(NULL == tableName);
	return hashTableNew(OMRPORT_FROM_J9PORT(vm->portLibrary), tableName, 64, sizeof(J9ObjectMonitor), 0, flags, OMRMEM_CATEGORY_VM, hashMonitorHash, hashMonitorCompare, NULL, vm);
}

UDATA
initializeMonitorTable(J9JavaVM* vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9MonitorTableListEntry *monitorTableListEntry = NULL;
	UDATA tableCount = 0;
	UDATA tableIndex = 0;

	if (FALSE == vm->memoryManagerFunctions->j9gc_modron_getConfigurationValueForKey(vm, j9gc_modron_configuration_gcThreadCount, (void *)&tableCount)) {
		return -1;
	}

	if (0 == tableCount) {
		/* Maybe pick another value but we should always have more than 0 GC threads */
		return -1;
	}

	if (omrthread_monitor_init_with_name(&vm->monitorTableMutex, 0, "VM monitor table")) {
		return -1;
	}

	vm->monitorTableListPool = pool_new(sizeof(J9MonitorTableListEntry), 0, 0, 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(vm->portLibrary));
	if (NULL == vm->monitorTableListPool) {
		return -1;
	}

	vm->monitorTables = (J9HashTable **)j9mem_allocate_memory(sizeof(J9HashTable *) * tableCount, OMRMEM_CATEGORY_VM);
	if (NULL == vm->monitorTables) {
		return -1;
	}
	memset(vm->monitorTables, 0, sizeof(J9HashTable *) * tableCount);
	
	vm->monitorTableList = NULL;

	for (tableIndex = 0; tableIndex < tableCount; tableIndex++) {
		J9HashTable *table = createMonitorTable(vm, J9_GET_CALLSITE());
		if (NULL == table) {
			return -1;
		}
		monitorTableListEntry = pool_newElement(vm->monitorTableListPool);
		if (NULL == monitorTableListEntry) {
			return -1;
		}

		/* Link this entry into the list at the head */
		monitorTableListEntry->next = vm->monitorTableList;
		vm->monitorTableList = monitorTableListEntry;

		/* Store the table into the array */
		vm->monitorTables[tableIndex] = table;
		/* Store the table into the list entry */
		monitorTableListEntry->monitorTable = table;
	}

	vm->monitorTableCount = tableCount;
	
	return 0;
}

void
destroyMonitorTable(J9JavaVM* vm)
{
	if (NULL != vm->monitorTables) {
		PORT_ACCESS_FROM_JAVAVM(vm);
		UDATA tableIndex = 0;
		for (tableIndex = 0; tableIndex < vm->monitorTableCount; tableIndex++) {
			J9HashTable* table = vm->monitorTables[tableIndex];
			if (NULL != table) {
				/* free all of the individual monitors */
				hashTableForEachDo(table, hashMonitorDestroyDo, NULL);

				/* free the table */
				hashTableFree(table);
				vm->monitorTables[tableIndex] = NULL;
			}
		}

		j9mem_free_memory(vm->monitorTables);
		vm->monitorTables = NULL;
	}


	/* free the monitorTableListPool */
	if (NULL != vm->monitorTableListPool) {
		pool_kill(vm->monitorTableListPool);
		vm->monitorTableListPool = NULL;
	}

	if (NULL != vm->monitorTableMutex) {
		omrthread_monitor_destroy(vm->monitorTableMutex);
		vm->monitorTableMutex = NULL;
	}

	/* Note: destroyMonitorTable is called after the GC hook interface has shut down,
	 * so we cannot unbook the events.
	 */
}


/*
 * The name of this routine is misleading, as it does NOT behave like the other
 * xxTableAt functions.  It should be called LookupAndAdd or something like that.
 *
 * @pre: The caller must have VM access.
 */
J9ObjectMonitor *
monitorTableAt(J9VMThread* vmStruct, j9object_t object)
{
	J9JavaVM* vm = vmStruct->javaVM;
	omrthread_monitor_t mutex = vm->monitorTableMutex;
	J9ObjectMonitor * objectMonitor = NULL;
	J9ObjectMonitor key_objectMonitor;
	J9ThreadAbstractMonitor key_monitor;
	struct J9HashTable* monitorTable = NULL;
	UDATA index = 0;
#if defined(J9VM_INTERP_CUSTOM_SPIN_OPTIONS)
	J9Class *ramClass = J9OBJECT_CLAZZ(vmStruct, object);
	J9VMCustomSpinOptions *option = ramClass->customSpinOption;
#endif /* J9VM_INTERP_CUSTOM_SPIN_OPTIONS */


#ifdef MONTABLE_TRACING
	PORT_ACCESS_FROM_VMC(vmStruct);
#endif

#ifdef J9VM_THR_LOCK_NURSERY
	Trc_VM_monitorTableAt_Entry(vmStruct, object, J9OBJECT_CLAZZ(vmStruct, object),J9OBJECT_MONITOR_OFFSET(vmStruct,object));

	if (TrcEnabled_Trc_VM_monitorTableAtObjectWithNoLockword){
		if (!LN_HAS_LOCKWORD(vmStruct,object)) {
			/* includes name of object */
			Trc_VM_monitorTableAtObjectWithNoLockword(vmStruct, J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(J9OBJECT_CLAZZ(vmStruct, object)->romClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(J9OBJECT_CLAZZ(vmStruct, object)->romClass)), object);
		}
	}

#else
	Trc_VM_monitorTableAt_Entry(vmStruct, object, J9OBJECT_CLAZZ(vmStruct, object),offsetof(J9Object,monitor));
#endif

#ifdef J9VM_THR_LOCK_NURSERY
	objectMonitor = (J9ObjectMonitor*) ((UDATA) vmStruct->objectMonitorLookupCache[J9_OBJECT_MONITOR_LOOKUP_SLOT(object,vm)]);
#else
	objectMonitor = vmStruct->cachedMonitor;
#endif
	if ((objectMonitor != NULL) && (((J9ThreadAbstractMonitor*)objectMonitor->monitor)->userData == (UDATA) object)) {
		HIT();
		TRACE("Cache hit");
		Trc_VM_monitorTableAt_CacheHit_Exit(vmStruct, objectMonitor);
		return objectMonitor;
	} else {
		Trc_VM_monitorTableAtCacheMiss(vmStruct, J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(J9OBJECT_CLAZZ(vmStruct, object)->romClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(J9OBJECT_CLAZZ(vmStruct, object)->romClass)), object);
		MISS();
	}

	/* Create a "fake" monitor just to probe the hash-table */
	key_monitor.userData = (UDATA) object;
	key_objectMonitor.monitor = (omrthread_monitor_t) &key_monitor;
	key_objectMonitor.hash = objectHashCode(vm, object);
	index = key_objectMonitor.hash % (U_32)vm->monitorTableCount;
	monitorTable = vm->monitorTables[index];


	omrthread_monitor_enter(mutex);

	if (NULL == monitorTable){
		TRACE("Out of memory creating tenant monitor table");
		objectMonitor = NULL;
	} else {
		objectMonitor = hashTableFind(monitorTable, &key_objectMonitor);
		if (objectMonitor == NULL) {
			omrthread_monitor_t monitor;
			UDATA monitorFlags = J9THREAD_MONITOR_OBJECT;

#ifdef J9VM_THR_LOCK_NURSERY
			key_objectMonitor.alternateLockword = 0;
#endif

			if (omrthread_monitor_init_with_name(&monitor, monitorFlags, NULL) == 0) {
				TRACE("Adding monitor");
				((J9ThreadAbstractMonitor*)monitor)->userData = (UDATA) object;

#if defined(J9VM_INTERP_CUSTOM_SPIN_OPTIONS)
#if defined(OMR_THR_CUSTOM_SPIN_OPTIONS)
				if (NULL != option) {
					const J9ThreadCustomSpinOptions *const j9threadOptions = &option->j9threadOptions;
#if defined(OMR_THR_THREE_TIER_LOCKING)
					((J9ThreadAbstractMonitor *)monitor)->customSpinOptions = j9threadOptions;
					((J9ThreadAbstractMonitor *)monitor)->spinCount1 = j9threadOptions->customThreeTierSpinCount1;
					((J9ThreadAbstractMonitor *)monitor)->spinCount2 = j9threadOptions->customThreeTierSpinCount2;
					((J9ThreadAbstractMonitor *)monitor)->spinCount3 = j9threadOptions->customThreeTierSpinCount3;
			
					Trc_VM_MonitorTableAt_CustomSpinOption(option->className,
														   object,
													   	   ((J9ThreadAbstractMonitor *)monitor)->spinCount1,
													   	   ((J9ThreadAbstractMonitor *)monitor)->spinCount2,
													       ((J9ThreadAbstractMonitor *)monitor)->spinCount3);
#endif /* OMR_THR_THREE_TIER_LOCKING */
#if defined(OMR_THR_ADAPTIVE_SPIN)													  
					Trc_VM_MonitorTableAt_CustomSpinOption2(option->className,
														    object,													       
														    j9threadOptions->customAdaptSpin);
#endif /* OMR_THR_ADAPTIVE_SPIN */							
				}
#endif /* OMR_THR_CUSTOM_SPIN_OPTIONS */				
#endif /* J9VM_INTERP_CUSTOM_SPIN_OPTIONS */

				key_objectMonitor.monitor = monitor;

#ifdef J9VM_THR_SMART_DEFLATION
				key_objectMonitor.proDeflationCount = 0;
				key_objectMonitor.antiDeflationCount = 0;
#endif

				objectMonitor = hashTableAdd(monitorTable, &key_objectMonitor);
				if (objectMonitor == NULL) {
					omrthread_monitor_destroy(monitor);
					TRACE("Out of memory adding to hash table");
				}
			} else {
				TRACE("Out of memory creating omrthread_monitor_t");
				objectMonitor = NULL;
			}
		} else {
			TRACE("Found monitor");
		}
	}

	if (NULL != objectMonitor) {
#ifdef J9VM_THR_LOCK_NURSERY
		cacheObjectMonitorForLookup(vm, vmStruct, objectMonitor);
#else
		vmStruct->cachedMonitor = objectMonitor;
#endif
	}

	omrthread_monitor_exit(mutex);

	Trc_VM_monitorTableAt_Exit(vmStruct, objectMonitor);

	return objectMonitor;
}




static UDATA
hashMonitorDestroyDo(void *entry, void *opaque)
{
	omrthread_monitor_t monitor = ((J9ObjectMonitor *) entry)->monitor;

	omrthread_monitor_destroy(monitor);
	return FALSE;
}

