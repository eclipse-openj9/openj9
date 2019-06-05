
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
#include "j9cfg.h"
#include "j9port.h"
#include "Tgc.hpp"
#include "mmhook.h"

#if defined(J9VM_GC_MODRON_SCAVENGER)
#include "GCExtensions.hpp"
#include "MemorySubSpace.hpp"
#include "MemorySubSpaceRegionIterator.hpp"
#include "TgcExtensions.hpp"
#include "HeapRegionDescriptor.hpp"
#include "MemorySpace.hpp"
#include "ScavengerStats.hpp"
#include "ObjectHeapBufferedIterator.hpp"

#define MAX_AGE (OBJECT_HEADER_AGE_MAX+1)

/**
 * Structure to hold details of occurrences
 * of a class within the survivor space
 */
typedef struct ClassEntry {
	struct ClassEntry *next;
	
	J9Class *clazz;
	UDATA count[MAX_AGE];
} ClassEntry;

/**
 * Find the occurrence of clazz inside classList
 * (if one exists)
 */
static ClassEntry *
findClassInList(ClassEntry *classList, J9Class *clazz)
{
	ClassEntry *mover;
	
	for(mover = classList; mover; mover = mover->next) {
		if(mover->clazz == clazz) {
			return mover;
		}
	}
	
	/* we haven't found the class in the list */
	return NULL;
}

/**
 * Add a new ClassEntry object to the head of
 * classList, setting its class to clazz
 */
static ClassEntry *
addClassEntry(J9VMThread *vmThread, ClassEntry *classList, J9Class *clazz, UDATA age)
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(vmThread->javaVM)->getForge();
	
	ClassEntry *newEntry = (ClassEntry *) forge->allocate((UDATA) sizeof(ClassEntry), MM_AllocationCategory::DIAGNOSTIC, J9_GET_CALLSITE());
	if (newEntry) {
		memset(newEntry, 0, sizeof(ClassEntry));
	} else {
		/* we have failed to allocate */
		return NULL;
	}
	
	/* add to the head of the list */
	newEntry->next = classList;
	
	/* setup the contents */
	newEntry->clazz = clazz;
	newEntry->count[age] = 1;
	
	return newEntry;
}

/**
 * Count the number of objects within a classEntry
 */
static UDATA
countObjects(ClassEntry *classEntry)
{
	UDATA count = 0, i;
	
	for(i=0; i<MAX_AGE; i++) {
		count += classEntry->count[i];
	}
	return count;
}

/**
 * Print a histogram of the objects using classList
 */
static void
printHistogram(J9VMThread *vmThread, ClassEntry *classList)
{
	ClassEntry *mover;
	UDATA objectCount = 0, i;
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(vmThread);
	
	tgcExtensions->printf("\n{SCAV: tgcScavenger OBJECT HISTOGRAM}\n");
	tgcExtensions->printf("\n{SCAV: | class | instances of age 0-%zu in semi-space |\n", (UDATA)OBJECT_HEADER_AGE_MAX);
	mover = classList;
	
	while(mover) {
		/* print the results for that class */
		tgcExtensions->printf("{SCAV: ");
		tgcPrintClass(vmThread->javaVM, mover->clazz);
		for(i=0; i<MAX_AGE; i++) {
			tgcExtensions->printf(" %zu", mover->count[i]);
		}
		tgcExtensions->printf("\n");
		/* update the total object count */
		objectCount += countObjects(mover);
		
		/* get the next class */
		mover = mover->next;
	}
	tgcExtensions->printf("{SCAV: Total objects in semispace = \"%zu\"\n\n", objectCount);
	
}

/**
 * Free all the memory associated with classList
 */
static void
deleteClassList(J9VMThread *vmThread, ClassEntry *classList)
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(vmThread->javaVM)->getForge();
	ClassEntry *entry, *mover;
	
	if(!classList) {
		return;
	}
	
	entry = classList;
	
	while(entry) {
		mover = entry->next;
		forge->free(entry);
		entry = mover;
	}
}

/**
 * Report object histogram following scavenger collection.
 * When tgcScavenger is enabled, this function is called following each
 * scavenger collection. The function walks the survivor space keeping tally
 * of each class of object it encounters, and the relative age of each
 * instance of that object (how many times it has been copied). The information
 * is output as a simple histogram suitable for loading into a spreadsheet
 * or similar for further analysis.
 */
static void
tgcHookScavengerReportObjectHistogram(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_LocalGCEndEvent* event = (MM_LocalGCEndEvent*)eventData;
	J9VMThread *vmThread = (J9VMThread*)event->currentThread->_language_vmthread;
	J9JavaVM *javaVM = vmThread->javaVM;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	bool shouldReport = false;

	ClassEntry *classList = NULL;
	MM_MemorySubSpace *subspace = ((MM_MemorySubSpace *)event->subSpace)->getDefaultMemorySubSpace();
	GC_MemorySubSpaceRegionIterator regionIterator(subspace);
	MM_HeapRegionDescriptor *region = NULL;

	while(NULL != (region = regionIterator.nextRegion())) {
		GC_ObjectHeapBufferedIterator objectHeapIterator(extensions, region); 
		J9Object *object = NULL;
		
		shouldReport = true;
		while(NULL != (object = objectHeapIterator.nextObject())) {
			/* iterate over objects in the segment */
			UDATA objectAge = extensions->objectModel.getObjectAge(object);
			ClassEntry *entry = findClassInList(classList, J9GC_J9OBJECT_CLAZZ(object));
			if(NULL != entry) {
				/* increment the appropriate struct */
				entry->count[objectAge] += 1;
			} else {
				/* add it to the list */
				entry = addClassEntry(vmThread, classList, J9GC_J9OBJECT_CLAZZ(object), objectAge);
				if(!entry) {
					/* whoops! */
					tgcExtensions->printf("Failed to allocate for histogram!\n");
					deleteClassList(vmThread, classList);
					return;
				}
				/* reset the classList to point at the new head */
				classList = entry;
			}
		}
	}
	
	if (shouldReport) {
		/* done iterating - print out the results */
		printHistogram(vmThread, classList);
	}
	/* free all the memory we allocated earlier */
	deleteClassList(vmThread, classList);
}


static void
tgcHookScavengerFlipSizeHistogram(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_ScavengeEndEvent* event = (MM_ScavengeEndEvent*)eventData;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(event->currentThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);

	tgcExtensions->printf("Scavenger Copy Bytes by Object Age:\n ");
	for (IDATA i = -1; i <= OBJECT_HEADER_AGE_MAX; ++i) {
		tgcExtensions->printf(" %9zi ", i);
	}
	tgcExtensions->printf("\n_");
	for (UDATA i = 0; i <= OBJECT_HEADER_AGE_MAX+1; ++i) {
		tgcExtensions->printf("___________");
	}
	tgcExtensions->printf("_\n");

	MM_ScavengerStats *stats = &extensions->scavengerStats;
	for (UDATA j = 0; j < SCAVENGER_FLIP_HISTORY_SIZE; ++j) {
		tgcExtensions->printf(" ");

		/* Flipped bytes */
		for (UDATA i = 0; i <= OBJECT_HEADER_AGE_MAX+1; ++i) {
			/* Blank out the top left table elements because data will not be populated for this cell until the next scavenge. */
			if ((0 == j) && (0 == i)) {
				tgcExtensions->printf("           ");
			} else {
				char tenureChar = ' ';
				if (i > 0 && (0 != (((UDATA)1 << (i-1)) & stats->getFlipHistory(j)->_tenureMask))) {
					tenureChar = '*';
				}
				tgcExtensions->printf(" %9zu%c", stats->getFlipHistory(j)->_flipBytes[i], tenureChar);
			}
		}
		tgcExtensions->printf(" \n ");

		/* Tenured bytes */
		for (UDATA i = 0; i <= OBJECT_HEADER_AGE_MAX+1; ++i) {
			/* Blank out the top left table elements because data will not be populated for this cell until the next scavenge. */
			if ((0 == j) && (0 == i)) {
				tgcExtensions->printf("           ");
			} else {
				tgcExtensions->printf(" %9zu ", stats->getFlipHistory(j)->_tenureBytes[i]);
			}
		}
		tgcExtensions->printf(" \n____________");

		/* Survival rate */
		for (UDATA i = 1; i <= OBJECT_HEADER_AGE_MAX+1; ++i) {
			if (j < SCAVENGER_FLIP_HISTORY_SIZE - 1) {
				UDATA currentFlipBytes = stats->getFlipHistory(j)->_flipBytes[i];
				UDATA currentTenureBytes = stats->getFlipHistory(j)->_tenureBytes[i];
				UDATA previousFlipBytes = stats->getFlipHistory(j+1)->_flipBytes[i-1];
	
				double survivalPercentage = 0.0;
				if (previousFlipBytes != 0) {
					survivalPercentage = ((double)(currentFlipBytes + currentTenureBytes) / (double)previousFlipBytes) * 100.0;
				}
	
				const char* underscorePadding = "";
				if (99.9995 <= survivalPercentage) {
					underscorePadding = "_";
				} else if (9.9995 <= survivalPercentage) {
					underscorePadding = "__";
				} else {
					underscorePadding = "___";
				}
	
				tgcExtensions->printf("__%s%.3lf%%", underscorePadding, survivalPercentage);
			} else {
				tgcExtensions->printf("___________");
			}
		}
		tgcExtensions->printf("_\n");
	}
}

static void
tgcHookScavengerDiscardedSpace(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_ScavengeEndEvent* event = (MM_ScavengeEndEvent*)eventData;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(event->currentThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);

	MM_ScavengerStats *stats = &extensions->scavengerStats;

	tgcExtensions->printf("\n");
	/* If _survivorTLHRemainderCount/_tenureTLHRemainderCount gets too high (in 1000s),
	 * it may indicate that discard thresholds are too low (and possibly causing contention while popping/pushing scan queue
	 */
	tgcExtensions->printf("Scavenger flipped=%zu discard=%zu TLHRemainderReuse=%zu\n", stats->_flipBytes, stats->_flipDiscardBytes, stats->_survivorTLHRemainderCount);
	tgcExtensions->printf("Scavenger tenured=%zu discard=%zu TLHRemainderReuse=%zu\n", stats->_tenureAggregateBytes, stats->_tenureDiscardBytes, stats->_tenureTLHRemainderCount);
}

static void
tgcHookScavengerAllocationPaths(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_ScavengeEndEvent* event = (MM_ScavengeEndEvent*)eventData;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(event->currentThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);

	MM_ScavengerStats *stats = &extensions->scavengerStats;
	tgcExtensions->printf("\n");
	tgcExtensions->printf("Scavenger semi space allocation path:   large=%zu, small=%zu\n", stats->_semiSpaceAllocationCountLarge, stats->_semiSpaceAllocationCountSmall);
	tgcExtensions->printf("Scavenger tenure space allocation path: large=%zu, small=%zu\n", stats->_tenureSpaceAllocationCountLarge, stats->_tenureSpaceAllocationCountSmall);
	tgcExtensions->printf("\n");
}

/**
 * Initialize scavenger tgc tracing.
 * Initializes the TgcScavengerExtensions object associated with scavenger tgc tracing. Attaches hooks
 * to the appropriate functions handling events used by scavenger tgc tracing.
 */
bool
tgcScavengerInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	bool result = true;

	J9HookInterface** omrHooks = J9_HOOK_INTERFACE(extensions->omrHookInterface);
	/*	Object histogram is expensive, but for legacy reasons we still support it. Other, cheaper, scavenger stats have been
	 * given special options, but also reported through this generic scavenge tgc options. */
	(*omrHooks)->J9HookRegisterWithCallSite(omrHooks, J9HOOK_MM_OMR_LOCAL_GC_END, tgcHookScavengerReportObjectHistogram, OMR_GET_CALLSITE(), NULL);

	return result;
}

/**
 * Initialize scavenger copied objects tgc tracing.
 * Attaches hooks to the appropriate functions handling events used by scavenger copied object tgc tracing.
 */
bool
tgcScavengerSurvivalStatsInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	bool result = true;

	J9HookInterface** privateHooks = J9_HOOK_INTERFACE(extensions->privateHookInterface);
	(*privateHooks)->J9HookRegisterWithCallSite(privateHooks, J9HOOK_MM_PRIVATE_SCAVENGE_END, tgcHookScavengerFlipSizeHistogram, OMR_GET_CALLSITE(), NULL);

	return result;
}

/**
 * Initialize scavenger allocate and discard stats
 * Attaches hooks to the appropriate functions handling events used by scavenger allocate and discard tgc tracing.
 */
bool
tgcScavengerMemoryStatsInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	bool result = true;

	J9HookInterface** privateHooks = J9_HOOK_INTERFACE(extensions->privateHookInterface);
	(*privateHooks)->J9HookRegisterWithCallSite(privateHooks, J9HOOK_MM_PRIVATE_SCAVENGE_END, tgcHookScavengerDiscardedSpace, OMR_GET_CALLSITE(), NULL);
	(*privateHooks)->J9HookRegisterWithCallSite(privateHooks, J9HOOK_MM_PRIVATE_SCAVENGE_END, tgcHookScavengerAllocationPaths, OMR_GET_CALLSITE(), NULL);

	return result;
}

#endif /* J9VM_GC_MODRON_SCAVENGER */
