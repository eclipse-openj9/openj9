/*******************************************************************************
 * Copyright IBM Corp. and others 2001
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/
#include "hookhelpers.hpp"
#include "CacheMap.hpp"
#include "sharedconsts.h"
#include "vmzipcachehook.h"
#include "j2sever.h"
#include "ut_j9shr.h"
#include "j9shrnls.h"

#define HOOKHELPERS_ERR_TRACE(verbose, var) if (verbose) j9nls_printf(PORTLIB, J9NLS_ERROR, var)
#define HOOKHELPERS_TRACE5_NOTAG(verbose, var, p1, p2, p3, p4, p5) if (verbose) j9nls_printf(PORTLIB, J9NLS_DO_NOT_PRINT_MESSAGE_TAG, var, p1, p2, p3, p4, p5)
#define HOOKHELPERS_TRACE6_NOTAG(verbose, var, p1, p2, p3, p4, p5, p6) if (verbose) j9nls_printf(PORTLIB, J9NLS_DO_NOT_PRINT_MESSAGE_TAG, var, p1, p2, p3, p4, p5, p6)
#define HOOKHELPERS_TRACE_NOTAG(verbose, var) if (verbose) j9nls_printf(PORTLIB, J9NLS_DO_NOT_PRINT_MESSAGE_TAG, var)

static bool makeClasspathItems(J9JavaVM* vm, J9ClassLoader* classloader, J9ClassPathEntry** cpePtrArray, I_16 entryCount, ClasspathItem* classpath);

UDATA
translateExtraInfo(void* extraInfo, IDATA* helperID, U_16* cpType, ClasspathItem** cachedCPI)
{
	J9GenericByID* generic = (J9GenericByID*) extraInfo;
	if (!generic || generic->magic != CP_MAGIC) {
		return 0;
	}
	*cpType = generic->type;
	*helperID = generic->id;
	*cachedCPI = (ClasspathItem*) generic->cpData;
	return 1;
}

/**
 * THREADING: callers hold the VM classMemorySegments->segmentMutex.
 */
ClasspathItem*
getBootstrapClasspathItem(J9VMThread* currentThread, J9ClassPathEntry* bootstrapCPE, UDATA entryCount)
{
	J9JavaVM* vm = currentThread->javaVM;
	Trc_SHR_Assert_ShouldHaveLocalMutex(vm->classMemorySegments->segmentMutex);

	if (bootstrapCPE == vm->sharedClassConfig->lastBootstrapCPE) {
		ClasspathItem* cpi = (ClasspathItem*) vm->sharedClassConfig->bootstrapCPI;
		if ((NULL != cpi) && ((UDATA)cpi->getMaxItems() == entryCount)) {
			return cpi;
		}
	}
	return NULL;
}

static bool
makeClasspathItems(J9JavaVM* vm, J9ClassLoader* classloader, J9ClassPathEntry** cpePtrArray, I_16 entryCount, ClasspathItem* classpath)
{
	IDATA prototype = PROTO_UNKNOWN;
	bool ret = true;
	bool ignoreStateChange = false;
	bool bootstrap = classloader == vm->systemClassLoader;

	if (bootstrap && (J2SE_VERSION(vm) >= J2SE_V11)) {
		J9ClassPathEntry* modulePath = vm->modulesPathEntry;
		if (CPE_TYPE_JIMAGE == modulePath->type) {
			prototype = PROTO_JIMAGE;
			ignoreStateChange = true;
		} else if (CPE_TYPE_DIRECTORY == modulePath->type) {
			prototype = PROTO_DIR;
		} else {
			Trc_SHR_Assert_ShouldNeverHappen();
		}
		/* add module path first */
		if ((classpath)->addItem(vm->internalVMFunctions, (const char*)modulePath->path, modulePath->pathLength, prototype) < 0) {
			ret = false;
		}
		if ((NULL != vm->sharedClassConfig) 
			&& (ignoreStateChange)
		) {
			J9VMThread* currentThread = vm->internalVMFunctions->currentVMThread(vm);

			((SH_CacheMap*) (vm->sharedClassConfig->sharedClassCache))->notifyClasspathEntryStateChange(currentThread, (const char*) modulePath->path, J9ZIP_STATE_IGNORE_STATE_CHANGES);
		}
	}
	if (bootstrap) {
		if (NULL == classloader->classPathEntries) {
			goto done;
		}
	} else {
		if (NULL == cpePtrArray) {
			goto done;
		}
	}
	for (I_16 i = 0; i < entryCount; i++) {
		J9ClassPathEntry* classPathEntry = NULL;
		if (bootstrap) {
			omrthread_rwmutex_enter_read(classloader->cpEntriesMutex);
			classPathEntry = classloader->classPathEntries[i];
			omrthread_rwmutex_exit_read(classloader->cpEntriesMutex);
		} else {
			classPathEntry = cpePtrArray[i];
		}
		char* path = (char*) classPathEntry->path;
		IDATA pathLen = classPathEntry->pathLength;
		prototype = PROTO_UNKNOWN;
		ignoreStateChange = false;

		if (CPE_TYPE_JAR == classPathEntry->type) {
			prototype = PROTO_JAR;
		} else if (CPE_TYPE_JIMAGE == classPathEntry->type) {
			prototype = PROTO_JIMAGE;
			ignoreStateChange = true;
		} else if (CPE_TYPE_DIRECTORY == classPathEntry->type) {
			prototype = PROTO_DIR;
		} else if (CPE_TYPE_UNUSABLE == classPathEntry->type) {
			prototype = PROTO_TOKEN;
		} else if (0 == classPathEntry->type) {
			char* endsWith = path + (pathLen - 4);
			if ((strcmp(endsWith, ".jar") == 0) || (strcmp(endsWith, ".zip") == 0)) {
				prototype = PROTO_JAR;
			} else {
				prototype = PROTO_DIR;
			}
		}
		if ((classpath)->addItem(vm->internalVMFunctions, (const char*)path, (U_16)pathLen, prototype) < 0) {
			ret = false; /* Serious problem */
			goto done;
		}
		if ((vm->sharedClassConfig) && (classPathEntry->status & CPE_STATUS_IGNORE_ZIP_LOAD_STATE)) {
			ignoreStateChange = true;
		}
		if (ignoreStateChange) {
			J9VMThread* currentThread = vm->internalVMFunctions->currentVMThread(vm);

			((SH_CacheMap*) (vm->sharedClassConfig->sharedClassCache))->notifyClasspathEntryStateChange(currentThread, (const char*)path, J9ZIP_STATE_IGNORE_STATE_CHANGES);
		}
	}
done:
	return ret;
}

/* THREADING: Should be protected by the class segment mutex */
UDATA
checkForStoreFilter(J9JavaVM* vm, J9ClassLoader* classloader, const char* classname, UDATA classnameLen, J9Pool* filterPool)
{
	struct ClassNameFilterData* anElement;
	struct ClassNameFilterData* theElement = NULL;
	pool_state aState;
	UDATA result = 0;

	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_SHR_Assert_ShouldHaveLocalMutex(vm->classMemorySegments->segmentMutex);

	/* Look for an entry matching the classloader and classname */
	anElement = (struct ClassNameFilterData*)pool_startDo(filterPool, &aState);
	while (anElement) {
		if (anElement->classloader == classloader) {
			if ((anElement->classnameLen == classnameLen) && (memcmp(anElement->classname, classname, classnameLen) == 0)) {
				theElement = anElement;
				break;
			}
		}
		anElement = (struct ClassNameFilterData*)pool_nextDo(&aState);
	}

	/* Remove the element from the pool now that we've used it */
	if (theElement) {
		result = 1;
		if (theElement->classname != (char*)&(theElement->buffer)) {
			j9mem_free_memory(theElement->classname);
		}
		pool_removeElement(filterPool, anElement);
	}

	return result;
}

void
storeClassVerboseIO( J9VMThread* currentThread, ClasspathItem * classpath, I_16 entryIndex, U_16 classnameLength, const U_8 * classnameData, UDATA helperID, BOOLEAN didWeStore)
{
	J9SharedClassConfig * sconfig = currentThread->javaVM->sharedClassConfig;
	PORT_ACCESS_FROM_JAVAVM(currentThread->javaVM);

	if (classpath != NULL && (sconfig->verboseFlags & J9SHR_VERBOSEFLAG_ENABLE_VERBOSE_IO)) {
		U_16 vPathLen;
		const char* vPath;
		UDATA classpathItemType = classpath->getType();

		if( classpathItemType == CP_TYPE_CLASSPATH ) {
			vPath = classpath->itemAt(entryIndex)->getPath(&vPathLen);

			if( didWeStore ) {
				HOOKHELPERS_TRACE6_NOTAG(sconfig->verboseFlags, J9NLS_SHRC_SHRINIT_STORED_VERBOSE_MSG, classnameLength, classnameData, helperID, vPathLen, vPath, entryIndex);
			}else {
				HOOKHELPERS_TRACE6_NOTAG(sconfig->verboseFlags, J9NLS_SHRC_SHRINIT_STORE_FAILED_VERBOSE_MSG, classnameLength, classnameData, helperID, vPathLen, vPath, entryIndex);
			}
		} else {
			vPath = classpath->itemAt(0)->getPath(&vPathLen);
				
			if( classpathItemType == CP_TYPE_URL) {
				if(didWeStore ) {
					HOOKHELPERS_TRACE5_NOTAG(sconfig->verboseFlags, J9NLS_SHRC_SHRINIT_STORED_VERBOSE_URL_MSG, classnameLength, classnameData, helperID, vPathLen, vPath);
				} else {
					HOOKHELPERS_TRACE5_NOTAG(sconfig->verboseFlags, J9NLS_SHRC_SHRINIT_STORE_FAILED_VERBOSE_URL_MSG, classnameLength, classnameData, helperID, vPathLen, vPath);
				}
			} else if( classpathItemType == CP_TYPE_TOKEN) {
				if(didWeStore) {
					HOOKHELPERS_TRACE5_NOTAG(sconfig->verboseFlags, J9NLS_SHRC_SHRINIT_STORED_VERBOSE_TOKEN_MSG, classnameLength, classnameData, helperID, vPathLen, vPath);
				} else {
					HOOKHELPERS_TRACE5_NOTAG(sconfig->verboseFlags, J9NLS_SHRC_SHRINIT_STORE_FAILED_VERBOSE_TOKEN_MSG, classnameLength, classnameData, helperID, vPathLen, vPath);
				}
			}
		}
	}
	
}

/**
 * Create a ClasspathItem.
 *
 * @param [in] currentThread Current thread
 * @param [in] classloader The class loader from which the class path entries are from. This parameter is only used when cpePtrArray is NUUL.
 * @param [in] cpePtrArray The Array of class path entries pointer, which is used to create the ClasspathItem.
 * 				If this parameter is NULL, classloader->classPathEntries will be used.
 * @param [in] entryCount The number of entries in cpePtrArray.
 * @param [in] helperID The class loader helper ID.
 * @param [in] cpType The type of class path entries.
 * @param [in] infoFound Extra info of class path entries.
 *
 * @return pointer to ClasspathItem
 *
 * THREADING: callers hold the VM classMemorySegments->segmentMutex
 */
ClasspathItem *
createClasspath(J9VMThread* currentThread, J9ClassLoader* classloader, J9ClassPathEntry** cpePtrArray, UDATA entryCount, IDATA helperID, U_16 cpType, UDATA infoFound)
{
	ClasspathItem *classpath;
	PORT_ACCESS_FROM_VMC(currentThread);
	Trc_SHR_Assert_ShouldHaveLocalMutex(currentThread->javaVM->classMemorySegments->segmentMutex);

	I_16 cpeCount = (cpePtrArray == NULL) ? (I_16)classloader->classPathEntryCount : (I_16)entryCount;
	I_16 supportedEntries = (I_16)cpeCount;
	
	if (!infoFound && J2SE_VERSION(currentThread->javaVM) >= J2SE_V11) {
		/* Add module entry */
		supportedEntries += 1;
	}

	if (entryCount > MAX_CLASS_PATH_ENTRIES) {
		supportedEntries = MAX_CLASS_PATH_ENTRIES;
	}
	UDATA sizeNeeded = ClasspathItem::getRequiredConstrBytes(supportedEntries);
	ClasspathItem* cpPtr = (ClasspathItem*) j9mem_allocate_memory(sizeNeeded, J9MEM_CATEGORY_CLASSES);
	
	if (!cpPtr) {
		HOOKHELPERS_ERR_TRACE(currentThread->javaVM->sharedClassConfig->verboseFlags, J9NLS_SHRC_SHRINIT_FAILURE_CREATE_CPI_FIND);
		return NULL;
	}
	Trc_SHR_createClasspath(currentThread, sizeNeeded, cpPtr);
	memset(cpPtr, 0, sizeNeeded);
	/* Note: modContext does not change, so no need to enter config mutex */
	classpath = ClasspathItem::newInstance(currentThread->javaVM, supportedEntries, helperID, cpType, cpPtr);
	if (!makeClasspathItems(currentThread->javaVM, classloader, cpePtrArray, cpeCount, classpath)) {
		/* Serious problem - error msg already reported */
		j9mem_free_memory(cpPtr);
		return NULL;
	}
	if (infoFound) {
		J9GenericByID* generic = (J9GenericByID*)cpePtrArray[0]->extraInfo;
		generic->cpData = classpath;
	} else {
		J9JavaVM *vm = currentThread->javaVM;
		if (vm->sharedClassConfig->bootstrapCPI) {
			j9mem_free_memory(vm->sharedClassConfig->bootstrapCPI);
		}
		if (J2SE_VERSION(vm) >= J2SE_V11) {
			vm->sharedClassConfig->lastBootstrapCPE = vm->modulesPathEntry;
		} else {
			omrthread_rwmutex_enter_read(classloader->cpEntriesMutex);
			vm->sharedClassConfig->lastBootstrapCPE = classloader->classPathEntries[0];
			omrthread_rwmutex_exit_read(classloader->cpEntriesMutex);
		}
		vm->sharedClassConfig->bootstrapCPI = classpath;
	}
	return classpath;
}
