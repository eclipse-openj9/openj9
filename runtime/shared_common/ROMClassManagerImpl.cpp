/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

/**
 * @file
 * @ingroup Shared_Common
 */

#include "ROMClassManagerImpl.hpp"
#include "ClasspathManager.hpp"
#include "ScopeManager.hpp"
#include "SharedCache.hpp"
#include "ut_j9shr.h"
#include "j9shrnls.h"
#include "j9consts.h"
#include <string.h>

SH_ROMClassManagerImpl::SH_ROMClassManagerImpl()
 : _tsm(0),
   _linkedListImplPool(0)
{
}

SH_ROMClassManagerImpl::~SH_ROMClassManagerImpl()
{
}

/* Creates a new hashtable. Pre-req: portLibrary must be initialized */
J9HashTable* 
SH_ROMClassManagerImpl::localHashTableCreate(J9VMThread* currentThread, U_32 initialEntries)
{
	J9HashTable* returnVal;

	Trc_SHR_RMI_localHashTableCreate_Entry(currentThread, initialEntries);
	returnVal = hashTableNew(OMRPORT_FROM_J9PORT(_portlib), J9_GET_CALLSITE(), initialEntries, sizeof(SH_Manager::HashLinkedListImpl*), sizeof(char *), 0, J9MEM_CATEGORY_CLASSES, SH_Manager::hllHashFn, SH_Manager::hllHashEqualFn, NULL, (void*)currentThread->javaVM->internalVMFunctions);
	_hashTableGetNumItemsDoFn = SH_ROMClassManagerImpl::customCountItemsInList;
	Trc_SHR_RMI_localHashTableCreate_Exit(currentThread, returnVal);
	return returnVal;
}

/**
 * Create a new instance of SH_ROMClassManagerImpl
 *
 * @param [in] vm A Java VM
 * @param [in] cache_ The SH_SharedCache that will use this ROMClassManager
 * @param [in] tsm_ The SH_TimestampManager in use by the cache
 * @param [in] memForConstructor Memory in which to build the instance
 *
 * @return new SH_ROMClassManager
 */	
SH_ROMClassManagerImpl*
SH_ROMClassManagerImpl::newInstance(J9JavaVM* vm, SH_SharedCache* cache_, SH_TimestampManager* tsm_, SH_ROMClassManagerImpl* memForConstructor)
{
	SH_ROMClassManagerImpl* newRCM = (SH_ROMClassManagerImpl*)memForConstructor;

	Trc_SHR_RMI_newInstance_Entry(vm, cache_, tsm_);

	new(newRCM) SH_ROMClassManagerImpl();
	newRCM->initialize(vm, cache_, tsm_, ((BlockPtr)memForConstructor + sizeof(SH_ROMClassManagerImpl)));

	Trc_SHR_RMI_newInstance_Exit(newRCM);

	return newRCM;
}

/* Initialize the SH_ROMClassManager - should be called before startup */
void
SH_ROMClassManagerImpl::initialize(J9JavaVM* vm, SH_SharedCache* cache_, SH_TimestampManager* tsm_, BlockPtr memForConstructor)
{
	Trc_SHR_RMI_initialize_Entry();

	_cache = cache_;
	_tsm = tsm_;
	_portlib = vm->portLibrary;
	_htMutex = NULL;
	_dataTypesRepresented[0] = TYPE_ROMCLASS;
	_dataTypesRepresented[1] = TYPE_ORPHAN;
	_dataTypesRepresented[2] = TYPE_SCOPED_ROMCLASS;

	notifyManagerInitialized(_cache->managers(), "TYPE_ROMCLASS");

	Trc_SHR_RMI_initialize_Exit();
}

/**
 * Returns the number of bytes required to construct this SH_ROMClassManager
 *
 * @return size in bytes
 */
UDATA
SH_ROMClassManagerImpl::getRequiredConstrBytes(void)
{
	UDATA reqBytes = 0;

	reqBytes += sizeof(SH_ROMClassManagerImpl);
	return reqBytes;
}

IDATA
SH_ROMClassManagerImpl::localInitializePools(J9VMThread* currentThread)
{
	Trc_SHR_RMI_localInitializePools_Entry(currentThread);

	_linkedListImplPool = pool_new(sizeof(SH_Manager::HashLinkedListImpl),  0, 0, 0, J9_GET_CALLSITE(), J9MEM_CATEGORY_CLASSES, POOL_FOR_PORT(_portlib));
	if (!_linkedListImplPool) {
		PORT_ACCESS_FROM_PORT(_portlib);
		M_ERR_TRACE(J9NLS_SHRC_RMI_FAILED_CREATE_POOL);
		Trc_SHR_RMI_localInitializePools_ExitFailed(currentThread);
		return -1;
	}

	Trc_SHR_RMI_localInitializePools_ExitOK(currentThread);
	return 0;
}

void
SH_ROMClassManagerImpl::localTearDownPools(J9VMThread* currentThread)
{
	Trc_SHR_RMI_localTearDownPools_Entry(currentThread);

	if (_linkedListImplPool) {
		pool_kill(_linkedListImplPool);
		_linkedListImplPool = NULL;
	}

	Trc_SHR_RMI_localTearDownPools_Exit(currentThread);
}
	
U_32
SH_ROMClassManagerImpl::getHashTableEntriesFromCacheSize(UDATA cacheSizeBytes)
{
	return (U_32)((cacheSizeBytes / 2000) + 100);
}

/**
 * Registers a new cached ROMClass with the SH_ROMClassManager
 *
 * Called when a new ROMClass has been identified in the cache
 *
 * @see Manager.hpp
 * @param[in] currentThread The current thread
 * @param[in] itemInCache The address of the item found in the cache
 * 
 * @return true if successful, false otherwise
 */
bool 
SH_ROMClassManagerImpl::storeNew(J9VMThread* currentThread, const ShcItem* itemInCache, SH_CompositeCache* cachelet)
{
	HashLinkedListImpl* result = NULL;
	bool orphanReunited = false;
	J9ROMClass* romClass = NULL;
	J9UTF8* utf8Name = NULL;

	if (getState() != MANAGER_STATE_STARTED) {
		return false;
	}
	Trc_SHR_RMI_storeNew_Entry(currentThread, itemInCache);

	if (ITEMTYPE(itemInCache) == TYPE_ORPHAN) {
		romClass = (J9ROMClass*)OWROMCLASS(((OrphanWrapper*)ITEMDATA(itemInCache)));
	} else {
		romClass = (J9ROMClass*)RCWROMCLASS(((ROMClassWrapper*)ITEMDATA(itemInCache)));
	}

	utf8Name = J9ROMCLASS_CLASSNAME(romClass);

	if (ITEMTYPE(itemInCache) == TYPE_ORPHAN) {
		Trc_SHR_RMI_storeNew_Event1(currentThread, J9UTF8_LENGTH(utf8Name), J9UTF8_DATA(utf8Name), romClass);
	} else {
		Trc_SHR_RMI_storeNew_Event2(currentThread, J9UTF8_LENGTH(utf8Name), J9UTF8_DATA(utf8Name), romClass);
	}

	if (ITEMTYPE(itemInCache) == TYPE_ROMCLASS) {
		orphanReunited = reuniteOrphan(currentThread, (const char*)J9UTF8_DATA(utf8Name), J9UTF8_LENGTH(utf8Name), itemInCache, romClass);
	}
	if (!orphanReunited) {
		result = hllTableUpdate(currentThread, _linkedListImplPool, utf8Name, itemInCache, cachelet);
		if (!result) {
			Trc_SHR_RMI_storeNew_ExitFalse(currentThread);
			return false;
		}
	}
	Trc_SHR_RMI_storeNew_ExitTrue(currentThread);
 	return true;
}

/* When an orphan is encountered in the cache, this is added to the hashtable with isOrphan==true. 
 * If a ROMClass entry is found which points to the same ROMClass as the orphan,
 * the hashtable entry should be re-used: The fact that we have an orphan is no longer relevant.
 * This function looks for an orphan entry which points to the ROMClass in romClassPtr and then 
 * "reunites" the entry with the data in item, making it no longer orphaned */
bool
SH_ROMClassManagerImpl::reuniteOrphan(J9VMThread* currentThread, const char* romClassName, UDATA nameLen, const ShcItem* item, const J9ROMClass* romClassPtr)
{
	HashLinkedListImpl* found, *walk;

	if (getState() != MANAGER_STATE_STARTED) {
		return false;
	}
	Trc_SHR_RMI_reuniteOrphan_Entry(currentThread, nameLen, romClassName);

	found = hllTableLookup(currentThread, romClassName, (U_16)nameLen, false);
	walk = found;

	if (!found) {
		Trc_SHR_RMI_reuniteOrphan_ExitFalse(currentThread);
		return false;
	}
	do {
		const ShcItem* currentItem = walk->_item;

		if (TYPE_ORPHAN == ITEMTYPE(currentItem)) {
			J9ROMClass* orphanRc = (J9ROMClass*)OWROMCLASS(((OrphanWrapper*)ITEMDATA(currentItem)));

			if (orphanRc == romClassPtr) {
				Trc_SHR_RMI_reuniteOrphan_Event1(currentThread, nameLen, romClassName, romClassPtr, item);
				walk->_item = item;			/* Fix up to real item */
				Trc_SHR_RMI_reuniteOrphan_ExitTrue(currentThread);
				return true;
			}
		}
		walk = (HashLinkedListImpl*)walk->_next;
	} while (found!=walk);
	Trc_SHR_RMI_reuniteOrphan_ExitFalse(currentThread);
	return false;
}

/**
 * Iterate thru classes currently stored in the cache
 *
 * @see ROMClassManager.hpp
 * @param[in] currentThread thread calling this function
 * @param[in] findNextIterator the last class we looked at, NULL if we should start at beginning
 * @param[in] firstFound first element we found (used to detect start of list)
 * @param[in] classnameLength length of the class name
 * @param[in] classnameData class name data
 *
 * @return A matching cached ROMClass if one exists, otherwise NULL
 */
const J9ROMClass*
SH_ROMClassManagerImpl::findNextExisting(J9VMThread* currentThread, void * &findNextIterator, void * &firstFound, U_16 classnameLength, const char* classnameData)
{
	HashLinkedListImpl * walk = NULL;
	HashLinkedListImpl * prev = NULL;
	const ShcItem* currentItem = NULL;
	const ShcItem* prevItem = NULL;
	const J9ROMClass* returnVal = NULL;
	const J9ROMClass* prevVal = NULL;

	Trc_SHR_RMI_findNextROMClass_Entry(currentThread);

	if (getState() != MANAGER_STATE_STARTED) {
		Trc_SHR_RMI_findNextROMClass_NotStarted_Event(currentThread, (UDATA)classnameLength, classnameData);
		Trc_SHR_RMI_findNextROMClass_Exit(currentThread);
		return NULL;
	}

	if (findNextIterator == NULL) {
		Trc_SHR_RMI_findNextROMClass_FirstElem_Event(currentThread);
		walk = hllTableLookup(currentThread, classnameData, classnameLength, true);
		firstFound = (void *)walk;
		findNextIterator = (void *)walk;
	} else {
		Trc_SHR_RMI_findNextROMClass_NextElem_Event(currentThread);
		walk = (HashLinkedListImpl*) findNextIterator;
		prev = walk;
		walk = (HashLinkedListImpl*) walk->_next;
		findNextIterator = (void *)walk;

		if (firstFound == walk) {
			/*We have circled back to the beginning of the list*/
			firstFound = NULL;
			findNextIterator = NULL;
			Trc_SHR_RMI_findNextROMClass_NoMore_Event(currentThread);
			Trc_SHR_RMI_findNextROMClass_Exit(currentThread);
			return NULL;
		}
	}

	if (walk == NULL) {
		findNextIterator = NULL;
		Trc_SHR_RMI_findNextROMClass_NoElems_Event(currentThread);
		Trc_SHR_RMI_findNextROMClass_Exit(currentThread);
		return NULL;
	}

	currentItem = walk->_item;

	if (TYPE_ORPHAN == ITEMTYPE(currentItem)) {
		Trc_SHR_RMI_findNextROMClass_FoundOrphan_Event(currentThread);
		returnVal = (J9ROMClass*) OWROMCLASS(((OrphanWrapper*) ITEMDATA(currentItem)));
	} else {
		Trc_SHR_RMI_findNextROMClass_FoundClass_Event(currentThread);
		returnVal = (J9ROMClass*) RCWROMCLASS(((ROMClassWrapper*) ITEMDATA(currentItem)));
	}
	
	if (prev != NULL) {
		/*If returnVal is the same as the last one returned then move on.*/
		prevItem = prev->_item;
		if (TYPE_ORPHAN == ITEMTYPE(prevItem)) {
			prevVal = (J9ROMClass*) OWROMCLASS(((OrphanWrapper*) ITEMDATA(prevItem)));
		} else {
			prevVal = (J9ROMClass*) RCWROMCLASS(((ROMClassWrapper*) ITEMDATA(prevItem)));
		}
		if (prevVal == returnVal) {
			Trc_SHR_RMI_findNextROMClass_MatchPrev_Event(currentThread);
			returnVal = this->findNextExisting(currentThread, findNextIterator, firstFound, classnameLength, classnameData);
		}
	}

	Trc_SHR_RMI_findNextROMClass_Exit(currentThread);
	return returnVal;
}

UDATA
SH_ROMClassManagerImpl::existsClassForName(J9VMThread* currentThread, const char* path, UDATA pathLen)
{
	return (hllTableLookup(currentThread, path, (U_16)pathLen, true) != NULL);	
}


/**
 * Locates and validates a ROMClass in the cache by fully qualified classname.
 * 
 * Searches all cached ROMClasses matching the fully qualified name and returns the one which is valid WRT to the caller's classpath.
 * A number of hints can be provided to the function to optimize the search, depending on the information available.
 * cpeIndex should be provided if it is known what classpath entry in the caller's classpath the ROMClass should exist in
 * cachedROMClass should be provided if it is already known that the ROMClass exists in the cache - it just needs to be validated
 *
 * @note parameter cp here could be local or cached classpath. If it is local, callerHelperID should be provided for tracing purposes
 *
 * @param[in] currentThread The current thread
 * @param[in] path Fully-qualified classname of ROMClass
 * @param[in] pathLen Length of path
 * @param[in] cp Classpath of caller's classloader
 * @param[in] cpeIndex Optional. Should be provided if known to limit scope (see above), otherwise should be -1.
 * @param[in] confirmedEntries The number of entries which have been confirmed in the classpath. Only applies to TYPE_CLASSPATH, otherwise should be -1.
 * @param[in] callerHelperID Helper ID of the caller classloader if the classpath is local (not cached)
 * @param[in] cachedROMClass If the ROMClass already exists in the cache and this is known, this should be provided
 * @param[in] partition The partition if one is in use
 * @param[in] modContext The modification context if one is in use
 * @param[out] result A LocateROMClassResult should be provided into which the results are written
 *
 * Returns a number of potential flags indicating results
 * @return LOCATE_ROMCLASS_RETURN_FOUND if a ROMClass was found and validated
 * @return LOCATE_ROMCLASS_RETURN_NOTFOUND if a ROMClass was not found or not valid WRT the caller's classpath
 * @return LOCATE_ROMCLASS_RETURN_DO_MARK_CPEI_STALE indicates that the caller should perform a stale mark immediately. Value to use is staleCPEI in LocateROMClassResult
 * @return LOCATE_ROMCLASS_RETURN_DO_TRY_WAIT 4 indicates that the caller should try waiting for an update as another JVM is likely in the middle of adding the class
 * @return LOCATE_ROMCLASS_RETURN_MARKED_ITEM_STALE indicates an out of date class was found and it was marked stale. If the read mutex was held, it is released.
 * @return LOCATE_ROMCLASS_RETURN_FOUND_SHADOW indicates that the class was found, but a shadow has appeared earlier in the classpath which has made it invalid
 */
/* THREADING: This function can be called multi-threaded */
UDATA
SH_ROMClassManagerImpl::locateROMClass(J9VMThread* currentThread, const char* path, U_16 pathLen, ClasspathItem* cp, I_16 cpeIndex, IDATA confirmedEntries, IDATA callerHelperID, 
						const J9ROMClass* cachedROMClass, const J9UTF8* partition, const J9UTF8* modContext, LocateROMClassResult* result) 
{
	HashLinkedListImpl* found = NULL;
	HashLinkedListImpl* walk = NULL;
	ROMClassWrapper* match = NULL;
	bool localFoundUnmodifiedOrphan = false;
	UDATA foundResult = LOCATE_ROMCLASS_RETURN_NOTFOUND;
	SH_ClasspathManager* localCPM = NULL;
	SH_ScopeManager* localSCM = NULL;

	Trc_SHR_RMI_locateROMClass_Entry(currentThread, pathLen, path, callerHelperID, cpeIndex);
	
	if (getState() != MANAGER_STATE_STARTED) {
		/* trace exception is at level 1 and trace exit message is at level 2 as per CMVC 155318/157683 */
		Trc_SHR_RMI_locateROMClass_ManagerNotInStartedState_Exception(currentThread, pathLen, path, callerHelperID, cpeIndex);
		Trc_SHR_RMI_locateROMClass_ManagerNotInStartedState(currentThread);
		return LOCATE_ROMCLASS_RETURN_NOTFOUND;
	}

	result->known = NULL;
	result->knownItem = NULL;
	result->foundAtIndex = -1;
	result->staleCPEI = NULL;

	found = hllTableLookup(currentThread, path, (U_16)pathLen, true);

	if (!found) {
		/*** NOTHING IS FOUND, TELL THE CALLER THAT IT MIGHT BE WORTH WAITING ***/
		Trc_SHR_RMI_locateROMClass_ExitNotFound1_Event(currentThread, pathLen, path, callerHelperID, cpeIndex);
		/* trace event is at level 1 and trace exit message is at level 2 as per CMVC 155318/157683  */
		Trc_SHR_RMI_locateROMClass_ExitNotFound1(currentThread);
		return (LOCATE_ROMCLASS_RETURN_DO_TRY_WAIT | LOCATE_ROMCLASS_RETURN_NOTFOUND);
	}

	walk = found;
	do {
		const ShcItem* currentItem = walk->_item;

		Trc_SHR_RMI_locateROMClass_TestItem(currentThread, currentItem);

		if (TYPE_ORPHAN == ITEMTYPE(currentItem)) {

			/*** IF AN UNMODIFIED ORPHAN IS FOUND, REMEMBER IT AND CARRY ON SEARCHING ***/

			if (!localFoundUnmodifiedOrphan) {
				J9ROMClass* orphanRC = (J9ROMClass*)OWROMCLASS(((OrphanWrapper*)ITEMDATA(currentItem)));
				bool romClassIsModified = J9ROMCLASS_HAS_MODIFIED_BYTECODES(orphanRC);

				if (!romClassIsModified) {
					localFoundUnmodifiedOrphan = true;
				}
			}
			Trc_SHR_RMI_locateROMClass_FoundOrphan(currentThread, localFoundUnmodifiedOrphan);
		} else {

			/*** IF AN ORPHAN IS NOT FOUND, THEN THIS IS A ROMCLASS ***/

			if (!(_cache->isStale(currentItem))) {

				/*** IF THE ROMCLASS IS NOT STALE, PROCEED TO VALIDATE THAT IT IS THE ONE WE WANT ***/

				ROMClassWrapper* wrapper = ((ROMClassWrapper*)ITEMDATA(walk->_item));
				ClasspathItem* cpInCache = (ClasspathItem*)CPWDATA(RCWCLASSPATH(wrapper));
				I_16 localFoundAtIndex = -1;				/* Important to initialize to -1 as this indicates "not found" */

				/* If we have a cachedROMClass (ie. the exact ROMClass we want is already in the cache) we can use that to eliminate non-matches */
				if ((cachedROMClass != NULL) && (cachedROMClass != (J9ROMClass*)RCWROMCLASS(wrapper))) {
					/* Only interested in an exact pointer-match, keep searching */
					Trc_SHR_RMI_locateROMClass_ElimatedWalkNext(currentThread);
					goto _continueNext;
				}

				/* If we have been given a cpeIndex, we can use that to eliminate non-matches. 
					Note that, even if we have found a cachedROMClass match, we still have to validate classpaths, so do not skip this step */
				if (cpeIndex >= 0) {
					ClasspathEntryItem* storedAt, *testCPEI; 

					storedAt = cpInCache->itemAt(wrapper->cpeIndex);
					testCPEI = cp->itemAt(cpeIndex);
					if (ClasspathItem::compare(currentThread->javaVM->internalVMFunctions, storedAt, testCPEI)) {
						/* If timestamps have changed, fail immediately. Note that timestamp comparisons are only valid between ClasspathEntryItems in cache. */
						if ((*_runtimeFlagsPtr & J9SHR_RUNTIMEFLAG_ENABLE_TIMESTAMP_CHECKS) && (storedAt->timestamp != testCPEI->timestamp)) {
							result->staleCPEI = storedAt;
							/* trace event is at level 1 and trace exit message is at level 2 as per CMVC 155318/157683  */
							Trc_SHR_RMI_locateROMClass_TimestampMismatch_Event(currentThread, storedAt->timestamp, testCPEI->timestamp,
									pathLen, path, callerHelperID, cpeIndex);
							Trc_SHR_RMI_locateROMClass_ExitTimestampMismatch(currentThread, storedAt->timestamp, testCPEI->timestamp);
							return (LOCATE_ROMCLASS_RETURN_DO_MARK_CPEI_STALE | LOCATE_ROMCLASS_RETURN_NOTFOUND);
						}
					} else {
						/* We haven't found the right ROMClass, keep searching */
						Trc_SHR_RMI_locateROMClass_ElimatedWalkNext(currentThread);
						goto _continueNext;
					}
					/* We have found correct ROMClass for this CPEI - proceed to match classpath */
				}

				/* If we've got here, we have a potential match to test */
				if ((partition!=NULL) || (modContext!=NULL) || ITEMTYPE(currentItem)==TYPE_SCOPED_ROMCLASS) {
					IDATA validateResult = 0;

					if (!localSCM) {
						if (_cache->getAndStartManagerForType(currentThread, TYPE_SCOPE, (SH_Manager**)&localSCM) != TYPE_SCOPE) {
							goto _notFound;
						}
					}
					validateResult = localSCM->validate(currentThread, partition, modContext, currentItem);

					if (validateResult == 0) {
						goto _continueNext;
					} else
					if (validateResult == -1) {
						/* Error from the ScopeManager - fail the find */
						goto _notFound;
					}
				}

				if (cp->isInCache()) {
					/* If cp is in the cache, we are only interested in exact pointer equality */
					if (cpInCache==cp) {
						Trc_SHR_RMI_locateROMClass_FoundCacheClasspath(currentThread, wrapper, cpeIndex, result->staleCPEI);
						match = wrapper;
						localFoundAtIndex = cpeIndex;
					}
				} else {
					/* If compareTo is from a ClassLoader, validate classpath and set localFoundAtIndex>=0 if valid */
					Trc_SHR_RMI_locateROMClass_ValidateClasspath(currentThread);
					if (!localCPM) {
						if (_cache->getAndStartManagerForType(currentThread, TYPE_CLASSPATH, (SH_Manager**)&localCPM) != TYPE_CLASSPATH) {
							goto _notFound;
						}
					}
					if (localCPM->validate(currentThread, wrapper, cp, confirmedEntries, &localFoundAtIndex, &(result->staleCPEI))) {
						Trc_SHR_RMI_locateROMClass_ValidateSucceeded(currentThread, wrapper, localFoundAtIndex, result->staleCPEI);
						match = wrapper;
					}
				}

				/* At this point, we have our match - just need to check timestamp if .class file and look for shadows */
				if (match) {
					if ((cp->getType() != CP_TYPE_TOKEN) && (*_runtimeFlagsPtr & J9SHR_RUNTIMEFLAG_ENABLE_TIMESTAMP_CHECKS)) {
						if (match->timestamp!=0 && checkTimestamp(currentThread, path, pathLen, match, walk->_item)) {
							/* At this point if the read mutex was held, it has been released. */
							/* trace event is at level 1 and trace exit message is at level 2 as per CMVC 155318/157683  */
							Trc_SHR_RMI_locateROMClass_TimestampChanged_Event(currentThread, pathLen, path, callerHelperID, cpeIndex);							
							Trc_SHR_RMI_locateROMClass_ExitRcTimestampChanged(currentThread);
							return (LOCATE_ROMCLASS_RETURN_MARKED_ITEM_STALE | LOCATE_ROMCLASS_RETURN_NOTFOUND);
						}
						if (!localCPM) {
							if (_cache->getAndStartManagerForType(currentThread, TYPE_CLASSPATH, (SH_Manager**)&localCPM) != TYPE_CLASSPATH) {
								goto _notFound;
							}
						}
						if (localCPM->touchForClassFiles(currentThread, path, pathLen, cp, localFoundAtIndex)) {
							/* trace event is at level 1 and trace exit message is at level 2 as per CMVC 155318/157683  */
							Trc_SHR_RMI_locateROMClass_FoundShadowClass_Event(currentThread, pathLen, path, callerHelperID, cpeIndex);
							Trc_SHR_RMI_locateROMClass_ExitFoundShadowClass(currentThread);
							return (LOCATE_ROMCLASS_RETURN_FOUND_SHADOW | LOCATE_ROMCLASS_RETURN_NOTFOUND);
						}
					}

					/*** SUCCESS - WE HAVE FOUND AND VALIDATED THE ROMCLASS ***/

					foundResult = LOCATE_ROMCLASS_RETURN_FOUND;
					result->foundAtIndex = localFoundAtIndex;
					result->known = match;
					result->knownItem = walk->_item;
					/* It is possible to return a stale CPEI *and* a successful ROMClass (see below) */
					if (!result->staleCPEI) {
						/* trace event is at level 1 and trace exit message is at level 2 as per CMVC 155318/157683  */						
						Trc_SHR_RMI_locateROMClass_ExitSuccess_Event(currentThread, match, localFoundAtIndex, 
								result->staleCPEI, pathLen, path, callerHelperID, cpeIndex);
						Trc_SHR_RMI_locateROMClass_ExitSuccess(currentThread, match, localFoundAtIndex, result->staleCPEI);
						return LOCATE_ROMCLASS_RETURN_FOUND;
					}
				}
			}
		}

		/* It is quite possible that we have a match to return as well as a stale mark to do */
		if (result->staleCPEI) {
			/* trace event is at level 1 and trace exit message is at level 2 as per CMVC 155318/157683  */
			Trc_SHR_RMI_locateROMClass_ExitTValidateFoundStale_Event(currentThread, pathLen, path, callerHelperID, cpeIndex);
			Trc_SHR_RMI_locateROMClass_ExitTValidateFoundStale(currentThread);
			return (LOCATE_ROMCLASS_RETURN_DO_MARK_CPEI_STALE | foundResult);
		}

_continueNext:
		walk = (HashLinkedListImpl*)walk->_next;
	} while (found!=walk);

	if (localFoundUnmodifiedOrphan) {
		/* trace event is at level 1 and trace exit message is at level 2 as per CMVC 155318/157683  */
		Trc_SHR_RMI_locateROMClass_ExitFoundOrphan_Event(currentThread, pathLen, path, callerHelperID, cpeIndex);
		Trc_SHR_RMI_locateROMClass_ExitFoundOrphan(currentThread);
		/* CMVC 93940 z/OS PERFORMANCE: Suggest waiting for entire update, not just load from disk */
		return (LOCATE_ROMCLASS_RETURN_DO_TRY_WAIT | LOCATE_ROMCLASS_RETURN_NOTFOUND);
	}

_notFound:
	/* no level 1 trace event here due to performance problem stated in CMVC 155318/157683 */
	Trc_SHR_RMI_locateROMClass_ExitNotFound2(currentThread, result->foundAtIndex, result->staleCPEI);
	return LOCATE_ROMCLASS_RETURN_NOTFOUND;
}

/* Check timestamp of ROMClass. 
 * If the timestamp has changed, exits the read mutex and acquires the write mutex if necessary,
 * marks ROMClass stale, and returns true.
 */
bool 
SH_ROMClassManagerImpl::checkTimestamp(J9VMThread* currentThread, const char* path, UDATA pathLen, ROMClassWrapper* rcw, const ShcItem* item)
{
	ClasspathWrapper* cpw;
	ClasspathEntryItem* cpeiInCache;

	Trc_SHR_RMI_checkTimestamp_Entry(currentThread, pathLen, path);

	cpw = (ClasspathWrapper*)RCWCLASSPATH(rcw);
	cpeiInCache = ((ClasspathItem*)CPWDATA(cpw))->itemAt(rcw->cpeIndex);

	if (_tsm->checkROMClassTimeStamp(currentThread, path, pathLen, cpeiInCache, rcw) != TIMESTAMP_UNCHANGED) {
		_cache->markItemStaleCheckMutex(currentThread, item, false);
		Trc_SHR_RMI_checkTimestamp_ExitTrue(currentThread);
		return true;
	}
	Trc_SHR_RMI_checkTimestamp_ExitFalse(currentThread);
	return false;
}

UDATA
SH_ROMClassManagerImpl::customCountItemsInList(void* entry, void* opaque)
{
	SH_Manager::LinkedListImpl* node = *(SH_Manager::LinkedListImpl**)entry;
	SH_Manager::LinkedListImpl* walk = node;
	SH_Manager::CountData* countData = (SH_Manager::CountData*)opaque;
	
	do {
		if (countData->_cache->isStale(walk->_item)) {
			++countData->_staleItems;
		} else {
			++countData->_nonStaleItems;
		}
		walk = walk->_next;
	} while (node != walk);
	return 0;
}

#if defined(J9SHR_CACHELET_SUPPORT)

bool 
SH_ROMClassManagerImpl::canCreateHints()
{
	return true;
}

/**
 * Walk the managed items hashtable in this cachelet. Allocate and populate an array
 * of hints, one for each hash entry.
 *
 * @param[in] self a data type manager
 * @param[in] vmthread the current VMThread
 * @param[out] hints a CacheletHints structure. This function fills in its
 * contents.
 *
 * @retval 0 success
 * @retval -1 failure
 */
IDATA
SH_ROMClassManagerImpl::createHintsForCachelet(J9VMThread* vmthread, SH_CompositeCache* cachelet, CacheletHints* hints)
{
	Trc_SHR_Assert_True(hints != NULL);

	/* hints->dataType should have been set by the caller */
	Trc_SHR_Assert_True(hints->dataType == _dataTypesRepresented[0]);
	
	return hllCollectHashes(vmthread, cachelet, hints);
}

/**
 * add a (_hashValue, cachelet) entry to the hash table
 * only called with hints of the right data type 
 *
 * each hint is a UDATA-length hash of a string
 */
IDATA
SH_ROMClassManagerImpl::primeHashtables(J9VMThread* vmthread, SH_CompositeCache* cachelet, U_8* hintsData, UDATA dataLength)
{
	UDATA* hashSlot = (UDATA*)hintsData;
	UDATA hintCount = 0;

	if ((dataLength == 0) || (hintsData == NULL)) {
		return 0;
	}

	hintCount = dataLength / sizeof(UDATA);
	while (hintCount-- > 0) {
		Trc_SHR_RMI_primeHashtables_addingHint(vmthread, cachelet, *hashSlot);
		if (!_hints.addHint(vmthread, *hashSlot, cachelet)) {
			/* If we failed to finish priming the hints, just give up.
			 * Stuff should still work, just suboptimally.
			 */
			Trc_SHR_RMI_primeHashtables_failedToPrimeHint(vmthread, this, cachelet, *hashSlot);
			break;
		}
		++hashSlot;
	}
	
	return 0;
}

#endif /* J9SHR_CACHELET_SUPPORT */
