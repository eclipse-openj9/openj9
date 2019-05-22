/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
#ifdef __cplusplus
extern "C"
{
#endif
#include "shrinit.h"
#include "verbose_api.h"
#include "j2sever.h"
#include "ut_j9shr.h"
#ifdef __cplusplus
}
#endif

#include "hookhelpers.hpp"
#include "CacheMap.hpp"
#include "CompositeCacheImpl.hpp"
#include "SCImplementedAPI.hpp"

#define TSTATE_STOPPED 0
#define TSTATE_STARTED 1
#define TSTATE_ENTER_WRITEMUTEX 2
#define TSTATE_ENTER_SEGMENTMUTEX 3
#define TSTATE_ENTER_READWRITEMUTEX 4
#define TSTATE_ENTER_READWRITEMUTEX_WITHOUT_WRITEMUTEX 5

typedef char* BlockPtr;

/**
 * Start a transaction to allow string intern tree manipulation
 *
 * @param [in/out] tobj transaction object memory
 * @param [in] tobj thread using this transaction
 *
 * @return 0 if ok, otherwise something is wrong
 *
 * THREADING: On success this function will acquire 3 locks:
 *  - shared classes: 	string table mutex (if not readonly)
 */
IDATA
j9shr_stringTransaction_start(void * tobj, J9VMThread* currentThread)
{
	IDATA retval = 0;
	J9SharedStringTransaction * obj = (J9SharedStringTransaction *) tobj;
	J9SharedClassConfig * sconfig = currentThread->javaVM->sharedClassConfig;
	U_64 localRuntimeFlags = 0;
	BOOLEAN readOnly = false;
	SH_CacheMap* cachemap = (SH_CacheMap*) (sconfig->sharedClassCache);
	UDATA doRebuildLocalData = 0;
	UDATA doRebuildCacheData = 0;

	J9SharedInvariantInternTable* table = currentThread->javaVM->sharedInvariantInternTable;
	const char * fname = "j9shr_stringTransaction_start";
	Trc_SHR_API_j9shr_stringTransaction_start_Entry(currentThread);

	if (NULL == tobj) {
		Trc_SHR_API_j9shr_stringTransaction_start_tobj_Event(currentThread);
		retval = -1;
		goto done;
	}

	/*NOTE:
	 * We set the state as we take the locks so that during the call to stop we can clean up correctly.
	 * There may be a better way to do this.
	 */
	obj->transactionState = TSTATE_STARTED;
	obj->ownerThread = currentThread;
	obj->isOK = 0;

	cachemap->updateRuntimeFullFlags(currentThread);
	
	localRuntimeFlags = sconfig->runtimeFlags;
	readOnly = J9_ARE_ANY_BITS_SET(localRuntimeFlags, J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL | J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL);

	if (!(localRuntimeFlags & J9SHR_RUNTIMEFLAG_CACHE_INITIALIZATION_COMPLETE)) {
		Trc_SHR_API_j9shr_stringTransaction_start_NotInit_Event(currentThread, localRuntimeFlags);
		retval = -1;
		goto done;
	}

	Trc_SHR_API_Assert_mustHaveVMAccess(currentThread);

	/*NOTE:
	 * Skip enterStringTableMutex() if invariantInternSharedPool is NULL
	 */
	if (table != NULL) {
		if (0 != (localRuntimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_ALL)) {
			if (omrthread_monitor_enter(currentThread->javaVM->classMemorySegments->segmentMutex) != 0) {
				Trc_SHR_API_j9shr_stringTransaction_start_SegMutex_Event(currentThread);
				retval = -1;
				goto done;
			}

			obj->transactionState = TSTATE_ENTER_SEGMENTMUTEX;

			/*Take a write mutex on the shared cache. This will be released in when the transaction is stopped.*/
			if (cachemap->startClassTransaction(currentThread, false, fname) != 0) {
				Trc_SHR_API_j9shr_stringTransaction_start_WriteLock_Event(currentThread);
				retval = -1;
				goto done;
			}

			obj->transactionState = TSTATE_ENTER_WRITEMUTEX;
		}

		/*NOTE:
		 * - If cache is opened with -Xshareclasses:readonly, then enterStringTableMutex() will fail to acquire the lock
		 */
		if (cachemap->enterStringTableMutex(currentThread, readOnly, &doRebuildLocalData, &doRebuildCacheData) != 0) {
			Trc_SHR_API_j9shr_stringTransaction_start_STLock_Event(currentThread, localRuntimeFlags, cachemap->getStringTableBytes(),
				readOnly, doRebuildLocalData, doRebuildCacheData);
			retval = -1;
			goto done;
		}
	
		if (doRebuildCacheData) {
			j9shr_resetSharedStringTable(currentThread->javaVM);
		}

		/* State can be TSTATE_ENTER_WRITEMUTEX here, only if mprotect=all option is specified */
		if (TSTATE_ENTER_WRITEMUTEX == obj->transactionState) {
			obj->transactionState = TSTATE_ENTER_READWRITEMUTEX;
		} else {
			obj->transactionState = TSTATE_ENTER_READWRITEMUTEX_WITHOUT_WRITEMUTEX;
		}
	} else {
		Trc_SHR_API_j9shr_stringTransaction_start_NoStringTable(currentThread);
		retval = -1;
		goto done;
	}

	done:
	if ((table != NULL) && ((table->flags & J9AVLTREE_DO_VERIFY_TREE_STRUCT_AND_ACCESS) != 0)) {
		/* If the JVM is started with -Xshareclasses:verifyInternTree the local and shared tree should be verified 
		 * before any "string intern" work occurs.
		 * 
		 * The shared tree can only be verified if the string table lock was entered.
		 */
		UDATA action = STRINGINTERNTABLES_ACTION_VERIFY_LOCAL_TABLE_ONLY;
		if (obj->transactionState == TSTATE_ENTER_READWRITEMUTEX) {
			action = STRINGINTERNTABLES_ACTION_VERIFY_BOTH_TABLES;
		}
		table->performNodeAction(table, NULL, action, NULL);
	}
	
	if (retval == -1) {
		obj->isOK = -1;
	}
	Trc_SHR_API_j9shr_stringTransaction_start_Exit(currentThread);
	return retval;
}

/**
 * End a transaction to allow string intern tree manipulation
 *
 * @param [in/out] tobj transaction object memory
 *
 * @return 0 if ok, otherwise something is wrong
 *
 * THREADING: On success this function will acquire 3 locks:
 *  - shared classes: 	string table mutex (if not readonly)
 */
IDATA
j9shr_stringTransaction_stop(void * tobj)
{
	IDATA retval = 0;
	J9SharedStringTransaction * obj = (J9SharedStringTransaction *) tobj;
	J9VMThread* currentThread = obj->ownerThread;
	J9SharedInvariantInternTable* table = currentThread->javaVM->sharedInvariantInternTable;
	J9SharedClassConfig * sconfig = currentThread->javaVM->sharedClassConfig;
	SH_CacheMap* cachemap = (SH_CacheMap*) (sconfig->sharedClassCache);
	const char * fname = "j9shr_stringTransaction_stop";
	Trc_SHR_API_j9shr_stringTransaction_stop_Entry(currentThread, obj->transactionState);

	if ((obj->transactionState != TSTATE_ENTER_READWRITEMUTEX_WITHOUT_WRITEMUTEX) &&
			(obj->transactionState != TSTATE_ENTER_READWRITEMUTEX) &&
			(obj->transactionState != TSTATE_ENTER_WRITEMUTEX) &&
			(obj->transactionState != TSTATE_ENTER_SEGMENTMUTEX) &&
			(obj->transactionState != TSTATE_STARTED)) {
		Trc_SHR_API_j9shr_stringTransaction_stop_NotStarted_Event(currentThread);
		retval = SCSTRING_TRANSACTION_STOP_ERROR;
		goto done;
	}

	if ((table != NULL) && ((table->flags & J9AVLTREE_DO_VERIFY_TREE_STRUCT_AND_ACCESS) != 0)) {
		/* If the JVM is started with -Xshareclasses:verifyInternTree the local and shared tree should be verified 
		 * after any "string intern" work occurs.
		 * 
		 * The shared tree can only be verified if the string table lock was entered.
		 */
		UDATA action = STRINGINTERNTABLES_ACTION_VERIFY_LOCAL_TABLE_ONLY;
		if (obj->transactionState == TSTATE_ENTER_READWRITEMUTEX_WITHOUT_WRITEMUTEX) {
			action = STRINGINTERNTABLES_ACTION_VERIFY_BOTH_TABLES;
		}
		table->performNodeAction(table, NULL, action, NULL);
	}

	/*NOTE:
	 * If cache is readonly or string table is 0 size ... we will fail to acquire the lock
	 */
	if ((TSTATE_ENTER_READWRITEMUTEX_WITHOUT_WRITEMUTEX == obj->transactionState) ||
			(TSTATE_ENTER_READWRITEMUTEX == obj->transactionState)) {
		/* No new elements are allocated in the string pool during a string transaction.
		 * Elements can be promoted, but promoted elements use an existing shared node.
		 */
		if (((SH_CacheMap*)currentThread->javaVM->sharedClassConfig->sharedClassCache)->exitStringTableMutex(currentThread, J9SHR_STRING_POOL_OK) != 0) {
			Trc_SHR_API_j9shr_stringTransaction_stop_ExitSTMutex_Event(currentThread);
			retval = SCSTRING_TRANSACTION_STOP_ERROR;
		}
	}

	if ((TSTATE_ENTER_READWRITEMUTEX == obj->transactionState) ||
			(TSTATE_ENTER_WRITEMUTEX == obj->transactionState)) {
		if (cachemap->exitClassTransaction(currentThread, fname) != 0) {
			Trc_SHR_API_j9shr_stringTransaction_stop_ExitWriteMutex_Event(currentThread);
			retval = SCSTRING_TRANSACTION_STOP_ERROR;
		}
	}

	if ((TSTATE_ENTER_READWRITEMUTEX == obj->transactionState) ||
			(TSTATE_ENTER_WRITEMUTEX == obj->transactionState) ||
			(TSTATE_ENTER_SEGMENTMUTEX == obj->transactionState)) {
		if (omrthread_monitor_exit(currentThread->javaVM->classMemorySegments->segmentMutex) != 0) {
			Trc_SHR_API_j9shr_stringTransaction_stop_ExitSegMutex_Event(currentThread);
			retval = SCSTRING_TRANSACTION_STOP_ERROR;
		}
	}


	done:
	if (retval == SCSTRING_TRANSACTION_STOP_ERROR) {
		obj->isOK = -1;
	}
	Trc_SHR_API_j9shr_stringTransaction_stop_Exit(currentThread);
	return retval;
}

/**
 * Query the current state, and check if it is good.
 *
 * @param [in] tobj the current transaction
 *
 * @return TRUE if ok, otherwise something is wrong
 *
 */
BOOLEAN
j9shr_stringTransaction_IsOK(void * tobj)
{
	J9SharedStringTransaction * obj = (J9SharedStringTransaction *) tobj;
	if (obj->isOK != 0) {
		return FALSE;
	} else {
		return TRUE;
	}
}

/**
 * Query the current state, and check if it is good.
 *
 * @param [in] tobj the current transaction
 *
 * @return TRUE if ok, otherwise something is wrong
 *
 */
BOOLEAN
j9shr_classStoreTransaction_isOK(void * tobj)
{
	J9SharedClassTransaction * obj = (J9SharedClassTransaction *) tobj;
	if (obj->isOK != 0) {
		return FALSE;
	} else {
		return TRUE;
	}
}

/**
 * Query if the string table lock is owned by the current transaction.
 *
 * @param [in] tobj the current transaction
 *
 * @return TRUE if the transaction owns the string table mutex,
 * otherwise the transaction does not own the lock
 *
 */
BOOLEAN
j9shr_classStoreTransaction_hasSharedStringTableLock(void * tobj)
{
	J9SharedClassTransaction * obj = (J9SharedClassTransaction *) tobj;
	if (obj->transactionState == TSTATE_ENTER_READWRITEMUTEX) {
		return TRUE;
	}
	return FALSE;
}

/**
 * Start a transaction to store a shared class
 *
 * @param [in] tobj transaction object memory
 * @param [in] currentThread thread using this transaction
 * @param [in] classloader classloader being used for this new class
 * @param [in] entryIndex classpath index
 * @param [in] loadType load type for the new class
 * @param [in] classnameLength class name length
 * @param [in] classnameData class name data
 * @param [in] isModifiedClassfile true if the class to be stored is modified
 *
 * @return 0 if ok, otherwise something is wrong
 *
 * THREADING: On success this function will acquire 3 locks:
 * 	- JVM: 				segment mutex
 *  - shared classes: 	string table mutex (if not readonly)
 *  - shared classes: 	write mutex (if not readonly)
 */
IDATA
j9shr_classStoreTransaction_start(void * tobj, J9VMThread* currentThread, J9ClassLoader* classloader, J9ClassPathEntry* classPathEntries, UDATA cpEntryCount, UDATA entryIndex, UDATA loadType, const J9UTF8* partition, U_16 classnameLength, U_8 * classnameData, BOOLEAN isModifiedClassfile, BOOLEAN takeReadWriteLock)
{
	const char * fname = "j9shr_classStoreTransaction_start";
	IDATA retval = 0;
	J9SharedClassTransaction * obj = (J9SharedClassTransaction *) tobj;
	J9SharedClassConfig * sconfig = currentThread->javaVM->sharedClassConfig;
	U_64 localRuntimeFlags = sconfig->runtimeFlags;
	SH_CacheMap* cachemap = (SH_CacheMap*) (sconfig->sharedClassCache);
	ClasspathItem * classpath = NULL;
	const J9UTF8* modContext = NULL;
	J9SharedInvariantInternTable* table = currentThread->javaVM->sharedInvariantInternTable;
	J9JavaVM* vm = currentThread->javaVM;

	Trc_SHR_API_j9shr_classStoreTransaction_start_Entry((UDATA)currentThread, (UDATA)classloader, (UDATA)classPathEntries, cpEntryCount, entryIndex, loadType, (UDATA)partition, (UDATA)classnameLength, classnameData, (UDATA)isModifiedClassfile, (UDATA)takeReadWriteLock);

	if (NULL == tobj) {
		Trc_SHR_API_j9shr_classStoreTransaction_start_tobj_Event(currentThread);
		retval = -1;
		goto done;
	}

	/*Note:
	 * - transactionState, oldVMState, & ownerThread must be set as early as possible (b/c they are used in j9shr_classStoreTransaction_stop())
	 */
	obj->transactionState = TSTATE_STARTED;
	obj->ownerThread = currentThread;

	/*Set the VM state to indicate we are potentially storing a class*/
	if (currentThread->omrVMThread->vmState != J9VMSTATE_SHAREDCLASS_STORE) {
		obj->oldVMState = currentThread->omrVMThread->vmState;
		currentThread->omrVMThread->vmState = J9VMSTATE_SHAREDCLASS_STORE;
	} else {
		obj->oldVMState = (UDATA)-1;
	}

	if ((loadType == J9SHR_LOADTYPE_REDEFINED) || ((loadType == J9SHR_LOADTYPE_RETRANSFORMED) && (localRuntimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_CACHERETRANSFORMED) == 0)) {
		Trc_SHR_API_j9shr_classStoreTransaction_start_Loadtype_Event(currentThread, loadType, (UDATA)classnameLength, classnameData);
		retval = -1;
		goto done;
	}

	if (localRuntimeFlags & J9SHR_RUNTIMEFLAG_DENY_CACHE_ACCESS) {
		Trc_SHR_API_j9shr_classStoreTransaction_start_DenyAccess_Event(currentThread, localRuntimeFlags, classnameLength, classnameData);
		retval = -1;
		goto done;
	}

	/* The entryIndex will be -1 for generated classes */ 
	if ((entryIndex > MAX_CLASS_PATH_ENTRIES) && (entryIndex != UDATA_MAX)) {
		Trc_SHR_API_j9shr_classStoreTransaction_start_CPIndex_Event(currentThread, entryIndex, (UDATA)classnameLength, classnameData);
		retval = -1;
		goto done;
	}

	if (NULL == cachemap->getROMClassManager(currentThread)) {
		Trc_SHR_API_j9shr_classStoreTransaction_start_RCMNotStarted_Event(currentThread, (UDATA)classnameLength, classnameData);
		retval = -1;
		goto done;
	}

	if (!(localRuntimeFlags & J9SHR_RUNTIMEFLAG_CACHE_INITIALIZATION_COMPLETE)) {
		Trc_SHR_API_j9shr_classStoreTransaction_start_NotInit_Event(currentThread, localRuntimeFlags, (UDATA)classnameLength, classnameData);
		retval = -1;
		goto done;
	}
	

	if (partition != NULL) {
		Trc_SHR_API_j9shr_classStoreTransaction_start_NonNullPartition_Event(currentThread, J9UTF8_LENGTH(partition),J9UTF8_DATA(partition));
	}

	obj->classloader = classloader;
	obj->entryIndex = (I_16)entryIndex;
	obj->loadType = loadType;
	obj->classnameLength = classnameLength;
	obj->classnameData = classnameData;
	obj->isOK = 0;
	obj->helperID = 0;
	obj->partitionInCache = NULL;
	obj->modContextInCache = NULL;
	obj->allocatedMem = NULL;
	obj->allocatedLineNumberTableSize = 0;
	obj->allocatedLocalVariableTableSize = 0;
	obj->allocatedLineNumberTable = NULL;
	obj->allocatedLocalVariableTable = NULL;
	obj->ClasspathWrapper = NULL;
	obj->cacheAreaForAllocate = NULL;
	obj->newItemInCache = NULL;
	obj->firstFound = NULL;
	obj->findNextIterator = NULL;
	obj->findNextRomClass = NULL;
	obj->isModifiedClassfile = ((isModifiedClassfile == TRUE)?1:0);
	obj->takeReadWriteLock = ((takeReadWriteLock==TRUE)?1:0);

	/*NOTE:
	 * Must omrthread_monitor_enter(segmentMutex) before enterWriteMutex(). Otherwise we will deadlock with hookFindSharedClass()
	 */
	if (omrthread_monitor_enter(vm->classMemorySegments->segmentMutex) != 0) {
		Trc_SHR_API_j9shr_classStoreTransaction_start_SegMutex_Event(currentThread, (UDATA)classnameLength, classnameData);
		retval = -1;
		goto done;
	}

	obj->transactionState = TSTATE_ENTER_SEGMENTMUTEX;


	if (sconfig->classnameFilterPool) {
		if (checkForStoreFilter(vm, classloader, (const char*) classnameData, (UDATA)classnameLength, sconfig->classnameFilterPool)) {
			Trc_SHR_API_j9shr_classStoreTransaction_start_StoreFilt_Event(currentThread, (UDATA)classnameLength, classnameData);
			retval = -1;
			goto done;
		}
	}

	if (takeReadWriteLock == TRUE) {
		/* When this function is called from the j9shr_jclUpdateROMClassMetaData()
		 *  - takeReadWriteLock == FALSE,
		 *  - and we do not have vm access.
		 * In this case we skip this check. Calls from the JCL do not require vm access
		 * because structures like 'class segments' are not updated when only meta-data
		 * is added to the shared cache.
		 */
		Trc_SHR_API_Assert_mustHaveVMAccess(currentThread);
	}

	/*
	 * Do work to initialize the below:
	 * 		obj->ClasspathWrapper
	 * 		obj->partitionInCache
	 * 		obj->modContextInCache
	 * 		obj->helperID
	 *
	 * The are all used during ::allocate and ::commit.
	 */

	modContext = sconfig->modContext;

	if (classloader != NULL) {
		/* default values for bootstrap: */
		IDATA helperID = 0;
		U_16 cpType = CP_TYPE_CLASSPATH;

		if ((J2SE_VERSION(vm) >= J2SE_V11)
			|| ((NULL != classPathEntries) && (-1 != obj->entryIndex))
		) {
			/* For class loaded from modules that entryIndex is -1. classPathEntries can be NULL. */
			void* cpExtraInfo = NULL;
			UDATA infoFound = 0;

			if (NULL != classPathEntries) {
				cpExtraInfo= classPathEntries->extraInfo;
				infoFound = translateExtraInfo(cpExtraInfo, &helperID, &cpType, &classpath);
			}

			/* Bootstrap loader does not provide meaningful extraInfo */
			if (!classpath && !infoFound) {
				UDATA pathEntryCount = cpEntryCount;

				if (J2SE_VERSION(vm) >= J2SE_V11) {
					if (classloader == vm->systemClassLoader) {
						if (J9_ARE_NO_BITS_SET(localRuntimeFlags, J9SHR_RUNTIMEFLAG_ENABLE_CACHEBOOTCLASSES)) {
							/* User specified noBootclasspath - do not continue */
							retval = -1;
							goto done;
						}
						/* bootstrap loader searched on path: modulePath:classPathEntries, entryIndex is the index of classPathEntries if loaded from classPathEntries
						 * or -1 if loaded from modules, so add 1 here */
						obj->entryIndex += 1;
						pathEntryCount += 1;
						classpath = getBootstrapClasspathItem(currentThread, vm->modulesPathEntry, pathEntryCount);
					}
				} else {
					if (J9_ARE_NO_BITS_SET(localRuntimeFlags, J9SHR_RUNTIMEFLAG_ENABLE_CACHEBOOTCLASSES)) {
						/* User specified noBootclasspath - do not continue */
						retval = -1;
						goto done;
					}
					classpath = getBootstrapClasspathItem(currentThread, classPathEntries, pathEntryCount);
				}
			}

			if (!classpath) {
				/* No cached classpath found. Need to create a new one. */
				if ((NULL != classPathEntries) || (classloader == vm->systemClassLoader)) {
					classpath = createClasspath(currentThread, classPathEntries, cpEntryCount, helperID, cpType, infoFound);
					if (NULL == classpath) {
						retval = -1;
						goto done;
					}
				}
			}
		}
	}

	/*Take a write mutex on the shared cache. This will be released in when the transaction is stopped.*/
	if (cachemap->startClassTransaction(currentThread, false, fname) != 0) {
		Trc_SHR_API_j9shr_classStoreTransaction_start_WriteLock_Event(currentThread, (UDATA)classnameLength, classnameData);
		retval = -1;
		goto done;
	}

	obj->transactionState = TSTATE_ENTER_WRITEMUTEX;

	if (NULL != classpath) {
		ClasspathWrapper* cpw = NULL;
		const J9UTF8* cachedModContext = NULL;
		const J9UTF8* cachedPartition = NULL;
		bool readOnly = (localRuntimeFlags & (J9SHR_RUNTIMEFLAG_ENABLE_READONLY | J9SHR_RUNTIMEFLAG_DENY_CACHE_UPDATES )) ? true : false;

		/* CMVC 182368 : When running in read-only mode, J9SharedClassTransaction.ClasspathWrapper is not used.
		 * We can keep it as NULL to prevent any spurious calls to commit data in j9shr_classStoreTransaction_stop().
		 */
		if (true == readOnly) {
			/* We've already identified that an element of this classpath is stale. Sharing is therefore disabled for this classpath. */
			if (classpath->flags & MARKED_STALE_FLAG) {
				Trc_SHR_API_j9shr_classStoreTransaction_start_CPIsStale_Event(currentThread, classpath->flags, (UDATA)classnameLength, classnameData);
				retval = -1;
				goto done;
			}
		} else {
			if ((cpw = cachemap->updateClasspathInfo(currentThread, classpath, obj->entryIndex, partition, &cachedPartition, modContext, &cachedModContext, true /*we own the write area lock*/)) == NULL) {
				/* Likely cache is full or read failure updating hashtables */
				Trc_SHR_API_j9shr_classStoreTransaction_start_updateCPInfo_Event(currentThread, obj->entryIndex, (UDATA)classnameLength, classnameData);
				retval = -1;
				goto done;
			}
			obj->ClasspathWrapper = (void *) cpw;
			obj->partitionInCache = (J9UTF8*)cachedPartition;
			obj->modContextInCache = (J9UTF8*)cachedModContext;
			obj->helperID = ((NULL == classpath) ? -1 : classpath->getHelperID());
		}
	}

	done:
	if ((table != NULL) && ((table->flags & J9AVLTREE_DO_VERIFY_TREE_STRUCT_AND_ACCESS) != 0)) {
		/* If the JVM is started with -Xshareclasses:verifyInternTree the local and shared tree should be verified 
		 * before any "string intern" work occurs.
		 * 
		 * The shared tree can only be verified if the string table lock was entered.
		 */
		UDATA action = STRINGINTERNTABLES_ACTION_VERIFY_LOCAL_TABLE_ONLY;
		if (obj->transactionState == TSTATE_ENTER_WRITEMUTEX || obj->transactionState == TSTATE_ENTER_READWRITEMUTEX) {
			action = STRINGINTERNTABLES_ACTION_VERIFY_BOTH_TABLES;
		}
		table->performNodeAction(table, NULL, action, NULL);
	}
	if (retval == -1) {
		obj->isOK = -1;
	}
	Trc_SHR_API_j9shr_classStoreTransaction_start_Exit(currentThread);
	return retval;
}

/**
 * End a transaction to store a shared class, and if required commit a new class (or meta-data) to the shared cache
 *
 * @param [in] tobj initialized transaction object memory
 *
 * @return the below values
 *  SCCLASS_STORE_STOP_ERROR if nothing was stored
 *  SCCLASS_STORE_STOP_NOTHING_STORED if data was stored
 *  SCCLASS_STORE_STOP_DATA_STORED something is wrong (an error will always override one of the above)
 *
 * THREADING: On success this function will releases 3 locks:
 * 	- JVM: 				segment mutex
 *  - shared classes: 	string table mutex (if not readonly)
 *  - shared classes: 	write mutex (if not readonly)
 */
IDATA
j9shr_classStoreTransaction_stop(void * tobj)
{
	IDATA retval = SCCLASS_STORE_STOP_NOTHING_STORED;
	const char * fname = "j9shr_classStoreTransaction_stop";
	J9SharedClassTransaction * obj = (J9SharedClassTransaction *) tobj;
	J9VMThread* currentThread = obj->ownerThread;
	J9JavaVM *vm = currentThread->javaVM;
	J9SharedClassConfig * sconfig = vm->sharedClassConfig;
	SH_CacheMap* cachemap = (SH_CacheMap*) (sconfig->sharedClassCache);
	bool releaseWriteMutex;
	bool releaseReadWriteMutex;
	bool releaseSegmentMutex;
	UDATA oldVMState = obj->oldVMState;
	ClasspathWrapper* cpw = (ClasspathWrapper*) obj->ClasspathWrapper;
	IDATA didWeStore = 0;
	bool modifiedNoContext = ((obj->isModifiedClassfile == 1) && (NULL == obj->modContextInCache));
	J9ROMClass * storedClass = NULL;
	J9UTF8 * storedClassName = NULL;
	J9SharedInvariantInternTable* table = currentThread->javaVM->sharedInvariantInternTable;

	Trc_SHR_API_j9shr_classStoreTransaction_stop_Entry(currentThread, obj->transactionState);

	/*if started go to done ...*/
	if (obj->transactionState == TSTATE_STARTED) {	
		Trc_SHR_API_j9shr_classStoreTransaction_stop_isOnlyStarted_Event(currentThread);
		retval = SCCLASS_STORE_STOP_ERROR;
		goto done;
	}

	if (obj->transactionState != TSTATE_ENTER_SEGMENTMUTEX && obj->transactionState != TSTATE_ENTER_READWRITEMUTEX && obj->transactionState != TSTATE_ENTER_WRITEMUTEX) {
		Trc_SHR_API_j9shr_classStoreTransaction_stop_NotStarted_Event(currentThread, obj->transactionState);
		retval = SCCLASS_STORE_STOP_ERROR;
		goto done;
	}

	releaseReadWriteMutex = (obj->transactionState == TSTATE_ENTER_READWRITEMUTEX);
	releaseWriteMutex = (obj->transactionState == TSTATE_ENTER_WRITEMUTEX || obj->transactionState == TSTATE_ENTER_READWRITEMUTEX);
	releaseSegmentMutex = (obj->transactionState == TSTATE_ENTER_SEGMENTMUTEX || obj->transactionState == TSTATE_ENTER_READWRITEMUTEX || obj->transactionState == TSTATE_ENTER_WRITEMUTEX);

	if ((table != NULL) && ((table->flags & J9AVLTREE_DO_VERIFY_TREE_STRUCT_AND_ACCESS) != 0)) {
		/* If the JVM is started with -Xshareclasses:verifyInternTree the local and shared tree should be verified 
		 * after any "string intern" work occurs.
		 * 
		 * The shared tree can only be verified if the string table lock was entered.
		 */
		UDATA action = STRINGINTERNTABLES_ACTION_VERIFY_LOCAL_TABLE_ONLY;
		if (releaseReadWriteMutex == true) {
			action = STRINGINTERNTABLES_ACTION_VERIFY_BOTH_TABLES;
		}
		table->performNodeAction(table, NULL, action, NULL);
	}


	if (releaseWriteMutex == true) {
		if ((NULL == obj->newItemInCache) && (NULL == obj->findNextRomClass)) {
			Trc_SHR_API_j9shr_classStoreTransaction_stop_NoWork_Event(currentThread, (UDATA) obj->classnameLength, obj->classnameData);
			/* The below assert ensures no debug data is ever commited if there is no 
			 * rom class to commit. This assert should never ever trigger.
			 */
			Trc_SHR_Assert_False( ((NULL != obj->allocatedLineNumberTable) || (NULL != obj->allocatedLocalVariableTable)) );
			retval = SCCLASS_STORE_STOP_NOTHING_STORED;
		} else {
			if (obj->newItemInCache != NULL && cpw != NULL && modifiedNoContext == false) {
				storedClass = (J9ROMClass *)obj->allocatedMem;
				didWeStore = cachemap->commitROMClass(obj->ownerThread, (ShcItem*)obj->newItemInCache, (SH_CompositeCacheImpl*) obj->cacheAreaForAllocate, (ClasspathWrapper*) cpw, obj->entryIndex, obj->partitionInCache, obj->modContextInCache, (BlockPtr) obj->allocatedMem, true);
			} else if (obj->newItemInCache != NULL) {
				if (modifiedNoContext == true) {
					Trc_SHR_API_j9shr_classStoreTransaction_stop_StoreModifed_Event(currentThread, (UDATA) obj->classnameLength, obj->classnameData, (UDATA)storedClass);
				}
				storedClass = (J9ROMClass *)obj->allocatedMem;
				didWeStore = cachemap->commitOrphanROMClass(obj->ownerThread, (ShcItem*)obj->newItemInCache, (SH_CompositeCacheImpl*) obj->cacheAreaForAllocate, (ClasspathWrapper*) cpw, (BlockPtr) obj->allocatedMem);
			} else {
				storedClass = (J9ROMClass *)obj->findNextRomClass;
				if (modifiedNoContext == true) {
					Trc_SHR_API_j9shr_classStoreTransaction_stop_NoNewMetaDataForModBytes_Event(currentThread, (UDATA) obj->classnameLength, obj->classnameData, (UDATA)storedClass);
				} else if (NULL == cpw) {
					Trc_SHR_API_j9shr_classStoreTransaction_stop_NoNewMetaDataNoCPInfo_Event(currentThread, (UDATA) obj->classnameLength, obj->classnameData, (UDATA)storedClass);
				} else {
					didWeStore = cachemap->commitMetaDataROMClassIfRequired(obj->ownerThread, (ClasspathWrapper*) cpw, obj->entryIndex, obj->helperID, obj->partitionInCache, obj->modContextInCache, (J9ROMClass *)obj->findNextRomClass);
				}
			}
			storedClassName = J9ROMCLASS_CLASSNAME(storedClass);

			/* Verify intermediateClassData in the ROMClass, if available, is within cache boundary */
			if (NULL != storedClass) {
				U_8 *intermediateClassData = J9ROMCLASS_INTERMEDIATECLASSDATA(storedClass);
				if (NULL != intermediateClassData) {
					Trc_SHR_Assert_True(j9shr_isAddressInCache(vm, intermediateClassData, storedClass->intermediateClassDataLength));
				}
			}
		}
	}

	/* Display verbose store messages.*/
	if (cpw != NULL) {
		ClasspathItem * classpath = ((ClasspathItem*) CPWDATA(cpw));
		storeClassVerboseIO(obj->ownerThread, classpath, obj->entryIndex, obj->classnameLength, obj->classnameData, obj->helperID, (didWeStore == 1));
	}

	/*Clear the transaction object*/
	memset(obj, 0, sizeof(J9SharedClassTransaction));
	obj->transactionState = TSTATE_STOPPED;

	if (releaseReadWriteMutex) {
		if (cachemap->exitStringTableMutex(currentThread, J9SHR_STRING_POOL_OK) != 0) {
			Trc_SHR_API_j9shr_classStoreTransaction_stop_ExitSTMutex_Event(currentThread);
			retval = SCCLASS_STORE_STOP_ERROR;
		}
	}


	if (releaseWriteMutex) {
		if (cachemap->exitClassTransaction(currentThread, fname) != 0) {
			Trc_SHR_API_j9shr_classStoreTransaction_stop_ExitWriteMutex_Event(currentThread);
			retval = SCCLASS_STORE_STOP_ERROR;
		}
	}

	if (releaseSegmentMutex) {
		if (omrthread_monitor_exit(vm->classMemorySegments->segmentMutex) != 0) {
			Trc_SHR_API_j9shr_classStoreTransaction_stop_ExitSegMutex_Event(currentThread);
			retval = SCCLASS_STORE_STOP_ERROR;
		}
	}

	done:
	if (oldVMState != (UDATA) - 1) {
		currentThread->omrVMThread->vmState = oldVMState;
	}
	if (retval == SCCLASS_STORE_STOP_ERROR) {
		obj->isOK = -1;
	}
	if (retval != SCCLASS_STORE_STOP_ERROR && didWeStore == SCCLASS_STORE_STOP_DATA_STORED) {
		Trc_SHR_API_j9shr_classStoreTransaction_stop_StoredData_Event(currentThread, (UDATA)J9UTF8_LENGTH(storedClassName), J9UTF8_DATA(storedClassName), storedClass);
		retval = SCCLASS_STORE_STOP_DATA_STORED;
	}
	Trc_SHR_API_j9shr_classStoreTransaction_stop_Exit(currentThread);
	return retval;
}

/**
 * Iterate thru class with the same ROMClass name that we would like to store.
 * - This function is called multiple times to iterate over all the potential matches.
 * - This function should be called after starting a transaction.
 * - The caller must iterate over all potential matches unless a match is found.
 *
 * @param [in] tobj initialized transaction object memory
 *
 * @return NULL if there is no more ROMClasses matching your class name
 * in the cache, or NON-NULL if a class is found to exist in the cache.
 *
 * THREADING: Must own (in atleast read only form):
 * 	- JVM: 				segment mutex
 *  - shared classes: 	write mutex (if not readonly)
 */
J9ROMClass *
j9shr_classStoreTransaction_nextSharedClassForCompare(void * tobj)
{
	J9SharedClassTransaction * obj = (J9SharedClassTransaction *) tobj;
	J9VMThread* currentThread = obj->ownerThread;
	J9SharedClassConfig * sconfig = currentThread->javaVM->sharedClassConfig;
	SH_CacheMap* cachemap = (SH_CacheMap*) (sconfig->sharedClassCache);

	Trc_SHR_API_j9shr_nextSharedClassForCompare_Entry(currentThread);

	if (obj->transactionState != TSTATE_ENTER_WRITEMUTEX) {
		Trc_SHR_API_j9shr_nextSharedClassForCompare_NotStarted_Event(currentThread, obj->transactionState);
		Trc_SHR_API_j9shr_nextSharedClassForCompare_Exit(currentThread);
		return NULL;
	}

	obj->findNextRomClass = (J9ROMClass *) cachemap->findNextROMClass(currentThread, obj->findNextIterator, obj->firstFound, obj->classnameLength, (const char*)obj->classnameData);

	Trc_SHR_API_j9shr_nextSharedClassForCompare_Exit(currentThread);
	return (J9ROMClass *)(obj->findNextRomClass);
}

IDATA
j9shr_classStoreTransaction_createSharedClass(void * tobj, const J9RomClassRequirements * sizes, J9SharedRomClassPieces * pieces)
{
	IDATA retval = 0;
	J9SharedClassTransaction * obj = (J9SharedClassTransaction *) tobj;
	J9SharedClassConfig * sconfig = obj->ownerThread->javaVM->sharedClassConfig;
	U_64 localRuntimeFlags = sconfig->runtimeFlags;
	SH_CacheMap* cachemap = (SH_CacheMap*) (sconfig->sharedClassCache);
	J9VMThread * currentThread = obj->ownerThread;
	bool modifiedNoContext = ((1 == obj->isModifiedClassfile) && (NULL == obj->modContextInCache));
	UDATA doRebuildLocalData = 0;
	UDATA doRebuildCacheData = 0;
	U_16 classnameLength = obj->classnameLength;
	const char* classnameData = (const char*)obj->classnameData;
	bool allocatedRomClass = false;

	Trc_SHR_API_j9shr_createSharedClass_Entry3(currentThread, classnameLength, classnameData, sizes->romClassSizeFullSize, sizes->romClassMinimalSize, sizes->lineNumberTableSize, sizes->localVariableTableSize);

	if (-1 == obj->isOK) {
		Trc_SHR_API_j9shr_createSharedClass_NotOk_Event(currentThread);
		retval = -1;
		goto done;
	}

	if (localRuntimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_READONLY) {
		Trc_SHR_API_j9shr_createSharedClass_ReadOnly_Event(currentThread);
		retval = -1;
		goto done;
	}
	if (J9_ARE_ALL_BITS_SET(localRuntimeFlags, J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
		Trc_SHR_API_j9shr_createSharedClass_Full_Event(currentThread);
		retval = -1;
		goto done;
	}
	if (localRuntimeFlags & J9SHR_RUNTIMEFLAG_DENY_CACHE_UPDATES) {
		Trc_SHR_API_j9shr_createSharedClass_DenyUpdates_Event(currentThread);
		retval = -1;
		goto done;
	}
	if (J9_ARE_ANY_BITS_SET(localRuntimeFlags, J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL)) {
		/* Put this block after the checks for J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL and J9SHR_RUNTIMEFLAG_DENY_CACHE_UPDATES, as there is
		 *  no point increasing the unstored bytes if these flags are set
		 */

		Trc_SHR_API_j9shr_createSharedClass_softFull_Event(currentThread);
		cachemap->increaseTransactionUnstoredBytes(sizes->romClassSizeFullSize, obj);
		retval = -1;
		goto done;
	}
	if (obj->transactionState != TSTATE_ENTER_WRITEMUTEX) {
		Trc_SHR_API_j9shr_createSharedClass_NotStarted_Event(currentThread);
		retval = -1;
		goto done;
	}

	if ((0 == sizes->romClassSizeFullSize) || (0 == sizes->romClassMinimalSize)) {
		/*The two ROM Class sizes must be non zero*/
		Trc_SHR_API_j9shr_createSharedClass_ZeroRomClassSize_Event(currentThread, sizes->romClassSizeFullSize, sizes->romClassMinimalSize);
		Trc_SHR_Assert_True(0 != sizes->romClassSizeFullSize);
		Trc_SHR_Assert_True(0 != sizes->romClassMinimalSize);
		retval = -1;
		goto done;
	}
	
	/*ROM Class size must be doubled aligned*/
	if (0 != (sizes->romClassSizeFullSize & (sizeof(U_64) - 1))) {
		Trc_SHR_API_j9shr_createSharedClass_DblAlign_Event(currentThread);
		Trc_SHR_Assert_ShouldNeverHappen();
		retval = -1;
		goto done;
	}

	/*ROM Class size must be doubled aligned*/
	if (0 != (sizes->romClassMinimalSize & (sizeof(U_64) - 1))) {
		Trc_SHR_API_j9shr_createSharedClass_DblAlign_Event(currentThread);
		Trc_SHR_Assert_ShouldNeverHappen();
		retval = -1;
		goto done;
	}
	
	/*We expect that j9shr_createSharedClass is never called more than once during a transaction*/
	if (obj->newItemInCache != NULL) {
		Trc_SHR_API_j9shr_createSharedClass_CalledTwice1_Event(currentThread);
		retval = -1;
		goto done;
	}
	if (obj->cacheAreaForAllocate != NULL) {
		Trc_SHR_API_j9shr_createSharedClass_CalledTwice2_Event(currentThread);
		retval = -1;
		goto done;
	}

	Trc_SHR_Assert_True(NULL == obj->allocatedMem);
	Trc_SHR_Assert_True(NULL == obj->allocatedLineNumberTable);
	Trc_SHR_Assert_True(NULL == obj->allocatedLocalVariableTable);
	
	memset(pieces, 0, sizeof(J9SharedRomClassPieces));


	allocatedRomClass = cachemap->allocateROMClass(currentThread,
			sizes,
			pieces,
			obj->classnameLength,
			(const char*)(obj->classnameData),
			(ClasspathWrapper*)obj->ClasspathWrapper,
			obj->partitionInCache,
			obj->modContextInCache,
			obj->helperID,
			modifiedNoContext,
			obj->newItemInCache,
			obj->cacheAreaForAllocate);

	if (false == allocatedRomClass) {
		/*Nothing could be allocated*/
		Trc_SHR_API_j9shr_createSharedClass_FailedToAllocRomclass_Event(currentThread);
		retval = -1;
		goto done;
	} 
	
	Trc_SHR_API_j9shr_createSharedClass_AllocRomclass_Event(currentThread, pieces->romClass);
	

	obj->allocatedLineNumberTableSize = sizes->lineNumberTableSize;
	obj->allocatedLocalVariableTableSize = sizes->localVariableTableSize;
	obj->allocatedLineNumberTable = pieces->lineNumberTable;
	obj->allocatedLocalVariableTable = pieces->localVariableTable;
	obj->allocatedMem = pieces->romClass;

	/*NOTE:
	 * Skip enterStringTableMutex() if:
	 * - the cache is readonly
	 * - the call to this function is from the JCL natives (obj->takeReadWriteLock == TRUE)
	 * - invariantInternSharedPool is NULL
	 */
	if (((localRuntimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_READONLY) == 0)
			&& (obj->takeReadWriteLock == 1)
			&& (currentThread->javaVM->sharedInvariantInternTable != NULL)) {
		/*NOTE:
		 * If cache is opened with -Xshareclasses:readonly then enterStringTableMutex() will fail to acquire the lock
		 */
		if (cachemap->enterStringTableMutex(currentThread, FALSE, &doRebuildLocalData, &doRebuildCacheData) != 0) {
			Trc_SHR_API_j9shr_createSharedClass_STLock_Event(currentThread, localRuntimeFlags, cachemap->getStringTableBytes(),
					FALSE, doRebuildLocalData, doRebuildCacheData, (UDATA)obj->classnameLength, (const char*)obj->classnameData);
		} else {
			if (doRebuildCacheData) {
				j9shr_resetSharedStringTable(currentThread->javaVM);
			}
			obj->transactionState = TSTATE_ENTER_READWRITEMUTEX;
		}
	}

	done:
	Trc_SHR_API_j9shr_createSharedClass_Exit(currentThread, retval);
	return retval;
}

/**
 * Update the size of a newly allocated ROMClass to something smaller.
 * - The call should have allocated a class.
 *
 * @param [in] tobj initialized transaction object memory
 *
 * @return NON-NULL if class is successfully allocated from the shared cache.
 *
 * THREADING: Must own:
 * 	- JVM: 				segment mutex
 *  - shared classes: 	string table mutex
 *  - shared classes: 	write mutex
 */
IDATA
j9shr_classStoreTransaction_updateSharedClassSize(void * tobj, U_32 sizeUsed)
{
	IDATA retval = 0;
	SH_CompositeCacheImpl* cacheAreaForAllocate = NULL;
	J9SharedClassTransaction * obj = (J9SharedClassTransaction *) tobj;
	J9VMThread * currentThread = obj->ownerThread;
	Trc_SHR_API_j9shr_updateSharedClassSize_Entry(currentThread);

	/*If the state is TSTATE_ENTER_READWRITEMUTEX then the write lock is also owned by this thread*/
	if (obj->transactionState != TSTATE_ENTER_WRITEMUTEX
		&& obj->transactionState != TSTATE_ENTER_READWRITEMUTEX) {
		Trc_SHR_API_j9shr_updateSharedClassSize_NotStarted_Event(currentThread, (UDATA)obj->classnameLength, obj->classnameData);
		retval = -1;
		goto done;
	}

	if ((sizeUsed % SHC_DOUBLEALIGN) != 0) {
		Trc_SHR_Assert_True((sizeUsed % SHC_DOUBLEALIGN) == 0);
		Trc_SHR_API_j9shr_updateSharedClassSize_DblAlign_Event(currentThread, (UDATA)obj->classnameLength, obj->classnameData);
		goto done;
	}

	/*We expect that something to be allocated*/
	if (NULL == obj->newItemInCache) {
		Trc_SHR_API_j9shr_updateSharedClassSize_NotCalled1_Event(currentThread, (UDATA)obj->classnameLength, obj->classnameData);
		retval = -1;
		goto done;
	}
	if (NULL == obj->cacheAreaForAllocate) {
		Trc_SHR_API_j9shr_updateSharedClassSize_NotCalled2_Event(currentThread, (UDATA)obj->classnameLength, obj->classnameData);
		retval = -1;
		goto done;
	}

	Trc_SHR_API_j9shr_updateSharedClassSize_Update_Event(currentThread, (UDATA)obj->classnameLength, obj->classnameData, sizeUsed);

	cacheAreaForAllocate = (SH_CompositeCacheImpl*)obj->cacheAreaForAllocate;
	cacheAreaForAllocate->updateStoredSegmentUsedBytes(sizeUsed);

	done:
	Trc_SHR_API_j9shr_updateSharedClassSize_Exit(currentThread);
	return retval;
}

/**
 * Called by JCL natives, this function updates the metadata (e.g. classpath info) for a shared ROMClass.
 *
 * @param [in] currentThread thread calling this function
 * @param [in] classloader
 * @param [in] classPathEntries
 * @param [in] cpEntryCount
 * @param [in] entryIndex
 * @param [in] existingClass
 *
 * @return void
 *
 * THREADING: Uses J9SharedClassTransaction functions.
 */
J9ROMClass *
j9shr_jclUpdateROMClassMetaData(J9VMThread* currentThread, J9ClassLoader* classloader, J9ClassPathEntry* classPathEntries, UDATA cpEntryCount, UDATA entryIndex, const J9UTF8* partition, const J9ROMClass * existingClass)
{
	J9SharedClassTransaction tobj;
	J9SharedClassConfig * sconfig = currentThread->javaVM->sharedClassConfig;
	SH_CacheMap* cachemap = (SH_CacheMap*) (sconfig->sharedClassCache);
	J9UTF8* romClassName = J9ROMCLASS_CLASSNAME(existingClass);
	U_16 classnameLength = J9UTF8_LENGTH(romClassName);
	U_8 * classnameData = J9UTF8_DATA(romClassName);
	/*Note: J9ROMCLASS_HAS_MODIFIED_BYTECODES means the classfile is modified, not just the bytecodes*/
	BOOLEAN isModifiedClassfile = J9ROMCLASS_HAS_MODIFIED_BYTECODES(existingClass);
	IDATA didWeStoreAnything = SCCLASS_STORE_STOP_ERROR;

	Trc_SHR_API_j9shr_jclUpdateROMClassMetaData_Entry((UDATA)currentThread, (UDATA)classloader, (UDATA)classPathEntries, cpEntryCount, entryIndex, (UDATA)partition, (UDATA)classnameLength, classnameData);

	if ((sconfig->runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_READONLY) != 0) {
		/* Note: 
		 * - When bcutil calls j9shr_classStoreTransaction_start() it may do so in 
		 *   order to find orphan rom classes in the cache, this operation should be allowed 
		 *   even when the cache is readonly. 
		 * - However when this function calls j9shr_classStoreTransaction_start()it does so only 
		 *   to write meta data to the cache. In this case when the cache is readonly there is 
		 *   no point in calling j9shr_classStoreTransaction_start(). 
		 */
		Trc_SHR_API_j9shr_jclUpdateROMClassMetaData_CacheIsReadonly_Event(currentThread, (UDATA)classnameLength, classnameData, (UDATA)sconfig->runtimeFlags);
		goto done;
	}

	if (cachemap->isAddressInROMClassSegment((BlockPtr) existingClass) == false) {
		Trc_SHR_API_j9shr_jclUpdateROMClassMetaData_NotInCache_Event(currentThread, (UDATA)classnameLength, classnameData, (UDATA)existingClass);
		goto done;
	}

	if ((NULL == sconfig->modContext) && (TRUE == isModifiedClassfile)) {
		/* Note:
		 * - A rom class with modified byte codes is written as an Orphan unless there is a modification context.
		 *   If this happens there is no point in calling j9shr_classStoreTransaction_start().
		 */
		Trc_SHR_API_j9shr_jclUpdateROMClassMetaData_ModifiedByteCodes_Event(currentThread, (UDATA)classnameLength, classnameData, (UDATA)sconfig->modContext, (UDATA)isModifiedClassfile);
		goto done;
	}

	if (j9shr_classStoreTransaction_start((void *) &tobj, currentThread, classloader, classPathEntries, cpEntryCount, entryIndex, J9SHR_LOADTYPE_NORMAL, partition, classnameLength, classnameData, isModifiedClassfile, FALSE) != 0) {
		Trc_SHR_API_j9shr_jclUpdateROMClassMetaData_StartFailed_Event(currentThread, (UDATA)classnameLength, classnameData);
		/* If j9shr_classStoreTransaction_start() fails we still
		 * need to release some of the locks that have been taken.
		 */
		goto callstopAndExit;
	}

	tobj.findNextRomClass = (void *) existingClass;

callstopAndExit:
	didWeStoreAnything = j9shr_classStoreTransaction_stop((void *) &tobj);
	if (didWeStoreAnything == SCCLASS_STORE_STOP_ERROR) {
		Trc_SHR_API_j9shr_jclUpdateROMClassMetaData_StopFailed_Event(currentThread, (UDATA)classnameLength, classnameData);
		goto done;
	}

done:
	if (didWeStoreAnything == SCCLASS_STORE_STOP_DATA_STORED) {
		/*If new meta was stored we return a pointer to the 
		 * ROMClass that was passed in. This matches the behaviour 
		 * of the old hookStoreSharedClass() function
		 */
		Trc_SHR_API_j9shr_jclUpdateROMClassMetaData_ExitStore(currentThread, (UDATA)classnameLength, classnameData);
		return (J9ROMClass *)existingClass;
	} else {
		if (didWeStoreAnything == SCCLASS_STORE_STOP_NOTHING_STORED) {
			Trc_SHR_API_j9shr_jclUpdateROMClassMetaData_ExitNoStore(currentThread, (UDATA)classnameLength, classnameData);
		} else {
			/*didWeStoreAnything == SCCLASS_STORE_STOP_ERROR*/
			Trc_SHR_API_j9shr_jclUpdateROMClassMetaData_ExitError(currentThread, (UDATA)classnameLength, classnameData);
		}
		/*If no meta data was stored we simply return NULL*/
		return NULL;
	}
}

