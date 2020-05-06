/*******************************************************************************
 * Copyright (c) 2001, 2020 IBM Corp. and others
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

#if !defined(J9SC_SCQUERYFUNCTIONS_H)
#define J9SC_SCQUERYFUNCTIONS_H

#include "j9.h"
#include "j9generated.h"
#include "SCAbstractAPI.h"


#ifdef __cplusplus
extern "C"
{
#endif

 #if defined(__GNUC__)
 /*
  * Source files which include (directly or indirectly) this file
  * but do not use the functions defined below may fail to compile unless the "unused" attribute
  * is applied.
  * Note that Microsoft compilers do not allow this attribute.
  */
#if !defined(J9VM_SHRTEST)
#if !defined(J9SHR_CACHELET_SUPPORT)
static VMINLINE BOOLEAN j9shr_Query_IsCacheFull(J9JavaVM *vm)  __attribute__ ((__unused__));
static VMINLINE BOOLEAN j9shr_Query_IsAddressInCache(J9JavaVM *vm, void *address, UDATA length)  __attribute__ ((__unused__));
static VMINLINE BOOLEAN j9shr_Query_IsAddressInReadWriteCache(J9JavaVM *vm, void *address, UDATA length) __attribute__ ((__unused__));
#endif /* !J9SHR_CACHELET_SUPPORT */
static VMINLINE void j9shr_Query_PopulatePreinitConfigDefaults(J9JavaVM *vm, J9SharedClassPreinitConfig *updatedWithDefaults)  __attribute__ ((__unused__));
#endif /* !J9VM_SHRTEST */
#endif /* __GNUC__ */

#if defined (J9VM_SHRTEST) && defined (J9SHR_CACHELET_SUPPORT)
/*This function is not used from shrtest -Xrealtime */
#else
static VMINLINE BOOLEAN
j9shr_Query_IsCacheFull(J9JavaVM *vm)
{
	BOOLEAN retval = TRUE;
	if (vm != NULL && vm->sharedClassConfig != NULL) {
		SCAbstractAPI * sharedapi = (SCAbstractAPI *)(vm->sharedClassConfig->sharedAPIObject);
		if (sharedapi != NULL && sharedapi->isCacheFull != NULL) {
			retval = sharedapi->isCacheFull(vm);
		}
	}
	return retval;
}
#endif

/**
 * 	Following comment is copied from
 * 	shrinit.cpp :: j9shr_isAddressInCache(J9JavaVM *vm, void *address, UDATA length, BOOLEAN checkReadWriteCacheOnly)
 *
 * 	This function checks whether the memory segment;
 *  which starts at the given address and with the given length,
 *  is in the range of any cache in the cacheDescriptorList.
 *
 * If it is in the range of any one of the caches in the cacheDescriptorList,
 * then this functions returns true,
 * otherwise it returns false.
 *
 * @param vm  Currently running VM.
 * @param address Beginning of the memory segment to be checked whether in any cache boundaries.
 * @param Length of the memory segment.
 * @return TRUE if memory segment is in any cache, FALSE otherwise.
 */
#if defined (J9VM_SHRTEST) && defined (J9SHR_CACHELET_SUPPORT)
/*This function is not used from shrtest -Xrealtime */
#else
static VMINLINE BOOLEAN
j9shr_Query_IsAddressInCache(J9JavaVM *vm, void *address, UDATA length)
{
	BOOLEAN retval = FALSE;
	if ((NULL != vm) && (NULL != vm->sharedClassConfig)) {
		SCAbstractAPI * sharedapi = (SCAbstractAPI *)(vm->sharedClassConfig->sharedAPIObject);
		if ((NULL != sharedapi) && (NULL != sharedapi->isAddressInCache)) {
			retval = sharedapi->isAddressInCache(vm, address, length, FALSE);
		}
	}
	return retval;
}

/**
 * Check if an address range in in the readWrite (top layer) shared cache
 *
 * @param[in] vm  The Java VM.
 * @param[in] address The start of the address range
 * @param[in] length The length of the address range
 *
 * @return TRUE if the the address range is in the readWrite cache. False otherwise.
 **/

static VMINLINE BOOLEAN
j9shr_Query_IsAddressInReadWriteCache(J9JavaVM *vm, void *address, UDATA length)
{
	BOOLEAN retval = FALSE;
	if ((NULL != vm) && (NULL != vm->sharedClassConfig)) {
		SCAbstractAPI * sharedapi = (SCAbstractAPI *)(vm->sharedClassConfig->sharedAPIObject);
		if ((NULL != sharedapi) && (NULL != sharedapi->isAddressInCache)) {
			retval = sharedapi->isAddressInCache(vm, address, length, TRUE);
		}
	}
	return retval;
}
#endif

#if !defined (J9VM_SHRTEST)
static VMINLINE void
j9shr_Query_PopulatePreinitConfigDefaults(J9JavaVM *vm, J9SharedClassPreinitConfig *updatedWithDefaults)
{
	if ((NULL != vm) && (NULL != vm->sharedClassConfig)) {
		SCAbstractAPI * sharedapi = (SCAbstractAPI *)(vm->sharedClassConfig->sharedAPIObject);
		if ((NULL != sharedapi) && (NULL != sharedapi->populatePreinitConfigDefaults)) {
			sharedapi->populatePreinitConfigDefaults(vm, updatedWithDefaults);
		}
	} else {
		/*If shared classes is disabled all the values in 'updatedWithDefaults' are 0 bytes*/
		memset(updatedWithDefaults,0,sizeof(J9SharedClassPreinitConfig));
	}
}
#endif

#ifdef __cplusplus
}/*extern "C"*/
#endif

#endif /* J9SC_SCQUERYFUNCTIONS_H */
