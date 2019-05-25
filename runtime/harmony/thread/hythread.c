/*******************************************************************************
 * Copyright (c) 2007, 2019 IBM Corp. and others
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
 * @ingroup Port
 * @brief Port Library
 */
#if defined(WIN32)
#include <windows.h>
#endif
#include <stdlib.h>
#include <string.h>
#include "hythread.h"
#include "omrthread.h"

static intptr_t failedToSetAttr(intptr_t rc);

void *j9mem_allocate_threadLibrary(UDATA byteAmount)
{
#if defined(WIN32)
	HANDLE memHeap = GetProcessHeap();
	return HeapAlloc(memHeap, 0, byteAmount );
#else 
	return (void *) malloc(byteAmount);
#endif
}

void j9mem_deallocate_threadLibrary(void *memoryPointer)
{
#if defined(WIN32)
	HANDLE memHeap = GetProcessHeap();
	HeapFree(memHeap, 0, memoryPointer);
#else
	free(memoryPointer);
#endif
}

IDATA
hysem_destroy (HyThreadLibrary *threadLib, hysem_t s);

IDATA
hysem_init(HyThreadLibrary *threadLib, hysem_t* sp, I_32 initValue);

IDATA 
hysem_post(HyThreadLibrary *threadLib, hysem_t s);

IDATA 
hysem_wait(HyThreadLibrary *threadLib, hysem_t s);

IDATA 
hythread_attach(HyThreadLibrary *threadLib, hythread_t* handle);

IDATA 
hythread_create(HyThreadLibrary *threadLib, hythread_t* handle, UDATA stacksize, UDATA priority, UDATA suspend, hythread_entrypoint_t entrypoint, void* entryarg);

void
hythread_detach(HyThreadLibrary *threadLib, hythread_t thread);

void OMRNORETURN
hythread_exit(HyThreadLibrary *threadLib, hythread_monitor_t monitor);
	
UDATA* 
hythread_global(HyThreadLibrary *threadLib, char* name);

IDATA 
hythread_monitor_destroy(HyThreadLibrary *threadLib, hythread_monitor_t monitor);

IDATA 
hythread_monitor_enter(HyThreadLibrary *threadLib, hythread_monitor_t monitor);

IDATA 
hythread_monitor_exit(HyThreadLibrary *threadLib, hythread_monitor_t monitor);

IDATA 
hythread_monitor_init_with_name(HyThreadLibrary *threadLib, hythread_monitor_t* handle, UDATA flags, char* name);

IDATA 
hythread_monitor_notify(HyThreadLibrary *threadLib, hythread_monitor_t monitor);

IDATA
hythread_monitor_notify_all(HyThreadLibrary *threadLib, hythread_monitor_t monitor);

IDATA 
hythread_monitor_wait(HyThreadLibrary *threadLib, hythread_monitor_t monitor);

hythread_t 
hythread_self(HyThreadLibrary *threadLib) ;

IDATA 
hythread_sleep(HyThreadLibrary *threadLib, I_64 millis);

IDATA 
hythread_tls_alloc(HyThreadLibrary *threadLib, hythread_tls_key_t* handle);

IDATA 
hythread_tls_free(HyThreadLibrary *threadLib, hythread_tls_key_t key);

void*
hythread_tls_get(HyThreadLibrary *threadLib, hythread_t thread, hythread_tls_key_t key) ;

IDATA 
hythread_tls_set(HyThreadLibrary *threadLib, hythread_t thread, hythread_tls_key_t key, void* value);


/*
 * Create the port library initialization structure
 * All minor versions append to the end of this table
 */

static HyThreadLibrary MasterThreadLibraryTable = {
	{HYTHREAD_MAJOR_VERSION_NUMBER, HYTHREAD_MINOR_VERSION_NUMBER, 0, HYTHREAD_CAPABILITY_MASK}, /* threadVersion */
	hysem_destroy, /* sem_destroy */
	hysem_init, /* sem_init */
	hysem_post, /* sem_post */
	hysem_wait, /* sem_wait */
	hythread_attach, /* thread_attach */
	hythread_create, /* thread_create */
	hythread_detach, /* thread_detach */
	hythread_exit, /* thread_exit */
	hythread_global, /* thread_global */
	hythread_monitor_destroy, /* thread_monitor_destroy */
	hythread_monitor_enter, /* thread_monitor_enter */
	hythread_monitor_exit, /* thread_monitor_exit */
	hythread_monitor_init_with_name, /* thread_monitor_init_with_name */
	hythread_monitor_notify, /* thread_monitor_notify */
	hythread_monitor_notify_all, /* thread_monitor_notify_all */
	hythread_monitor_wait, /* thread_monitor_wait */
	hythread_self, /* thread_self */
	hythread_sleep, /* thread_sleep */
	hythread_tls_alloc, /* thread_tls_alloc */
	hythread_tls_free, /* thread_tls_free */
	hythread_tls_get, /* thread_tls_get */
	hythread_tls_set, /* thread_tls_set */
	NULL, /* self_handle */
};

/**
 * Initialize the thread library.
 * 
 * Given a pointer to a thread library and the required version,
 * populate the thread library table with the appropriate functions
 * and then call the startup function for the thread library.
 * 
 * @param[in] threadLibrary The thread library.
 * @param[in] version The required version of the thread library.
 * @param[in] size Size of the thread library.
 *
 * @return 0 on success, negative return value on failure
 */
I_32
hythread_init_library(struct HyThreadLibrary *threadLibrary, struct HyThreadLibraryVersion *version, UDATA size)
{
	/* return value of 0 is success */
	I_32 rc;

	rc = hythread_create_library(threadLibrary, version, size);
	if ( rc == 0 ) {
		return hythread_startup_library(threadLibrary);
	}
	return rc;
}

/**
 * ThreadLibrary shutdown.
 *
 * Shutdown the thread library, de-allocate resources required by the components of the threadlibrary.
 * Any resources that werer created by @ref hythread_startup_library should be destroyed here.
 *
 * @param[in] threadLibrary The thread library.
 *
 * @return 0 on success, negative return code on failure
 */
I_32
hythread_shutdown_library(struct HyThreadLibrary *threadLibrary )
{
	if ( NULL != threadLibrary->self_handle ) {
		j9mem_deallocate_threadLibrary(threadLibrary);
	}
	return (I_32) 0;
}

/**
 * Create the thread library.
 * 
 * Given a pointer to a thread library and the required version,
 * populate the thread library table with the appropriate functions
 * 
 * @param[in] threadLibrary The thread library.
 * @param[in] version The required version of the thread library.
 * @param[in] size Size of the thread library.
 *
 * @return 0 on success, negative return value on failure
 * @note The threadlibrary version must be compatible with the that which we are compiled against
 */
I_32
hythread_create_library(struct HyThreadLibrary *threadLibrary, struct HyThreadLibraryVersion *version, UDATA size)
{
	UDATA versionSize = hythread_getSize(version);

	if (HYTHREAD_MAJOR_VERSION_NUMBER != version->majorVersionNumber) {
		return -1;
	}

	if (versionSize > size) {
		return -1;
	}

	/* Ensure required functionality is there */
	if ((version->capabilities & HYTHREAD_CAPABILITY_MASK) != version->capabilities) {
		return -1;
	}

	/* Null and initialize the table passed in */
	memset(threadLibrary, 0, size);
	memcpy(threadLibrary, &MasterThreadLibraryTable, versionSize);

	/* Reset capabilities to be what is actually there, not what was requested */
	threadLibrary->threadVersion.majorVersionNumber = version->majorVersionNumber;
	threadLibrary->threadVersion.minorVersionNumber = version->minorVersionNumber;
	threadLibrary->threadVersion.capabilities = HYTHREAD_CAPABILITY_MASK;

	return 0;
}

/**
 * ThreadLibrary startup.
 *
 * Start the thread library, allocate resources required by the components of the thread library.
 * All resources created here should be destroyed in @ref hythread_shutdown_library.
 *
 * @param[in] threadLibrary The thread library.
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 *
 * @note The thread library memory is deallocated if it was created by @ref hythread_allocate_library
 */
I_32
hythread_startup_library(struct HyThreadLibrary *threadLibrary)
{
	return 0;
}

/**
 * Determine the size of the thread library.
 * 
 * Given a thread library version, return the size of the structure in bytes
 * required to be allocated.
 * 
 * @param[in] version The HyThreadLibraryVersion structure.
 *
 * @return size of thread library on success, zero on failure
 *
 * @note The thread library version must be compatible with the that which we are compiled against
 */
UDATA 
hythread_getSize(struct HyThreadLibraryVersion *version)
{
	/* Can't initialize a structure that is not understood by this version of the thread library	 */
	if (HYTHREAD_MAJOR_VERSION_NUMBER != version->majorVersionNumber) {
		return 0;
	}

	/* The size of the portLibrary table is determined by the majorVersion number
	 * and the presence/absence of the HYTHREAD_CAPABILITY_STANDARD capability 
	 */
	if (0 != (version->capabilities & HYTHREAD_CAPABILITY_STANDARD)) {
		return sizeof(HyThreadLibrary);
	} else {
		return offsetof(HyThreadLibrary, self_handle)+sizeof(void *);
	}
}
/**
 * Determine the version of the thread library.
 * 
 * Given a thread library return the version of that instance.
 * 
 * @param[in]threadLibrary The thread library.
 * @param[in,out] version The HyThreadLibraryVersion structure to be populated.
 *
 * @return 0 on success, negative return value on failure
 * @note If threadLibrary is NULL, version is populated with the version in the linked DLL
 */
I_32 
hythread_getVersion(struct HyThreadLibrary *threadLibrary, struct HyThreadLibraryVersion *version)
{
	if (NULL == version) {
		return -1;
	}

	if (threadLibrary) {
		version->majorVersionNumber = threadLibrary->threadVersion.majorVersionNumber;
		version->minorVersionNumber = threadLibrary->threadVersion.minorVersionNumber;
		version->capabilities = threadLibrary->threadVersion.capabilities;
	} else {
		version->majorVersionNumber = HYTHREAD_MAJOR_VERSION_NUMBER;
		version->minorVersionNumber = HYTHREAD_MINOR_VERSION_NUMBER;
		version->capabilities = HYTHREAD_CAPABILITY_MASK;
	}

	return 0;
}
/**
 * Determine thread library compatibility.
 * 
 * Given the minimum version of the thread library that the application requires determine
 * if the current thread library meets that requirements.
 *
 * @param[in] expectedVersion The version the application requires as a minimum.
 *
 * @return 1 if compatible, 0 if not compatible
 */
I_32 
hythread_isCompatible(struct HyThreadLibraryVersion *expectedVersion)
{
	/* Position of functions, signature of functions.
	 * Major number incremented when existing functions change positions or signatures
	 */
	if (HYTHREAD_MAJOR_VERSION_NUMBER != expectedVersion->majorVersionNumber) {
		return 0;
	}

	/* Size of table, it's ok to have more functions at end of table that are not used.
	 * Minor number incremented when new functions added to the end of the table
	 */
	if (HYTHREAD_MINOR_VERSION_NUMBER < expectedVersion->minorVersionNumber) {
		return 0;
	}

	/* Functionality supported */
	return (HYTHREAD_CAPABILITY_MASK & expectedVersion->capabilities) == expectedVersion->capabilities;
}

/**
 * Query the thread library.
 * 
 * Given a pointer to the thread library and an offset into the table determine if
 * the function at that offset has been overridden from the default value expected by J9. 
 *
 * @param[in] threadLibrary The thread library.
 * @param[in] offset The offset of the function to be queried.
 * 
 * @return 1 if the function is overridden, else 0.
 *
 * hythread_isFunctionOverridden(threadLibrary, offsetof(HyThreadLibrary, sem_init));
 */
I_32
hythread_isFunctionOverridden(struct HyThreadLibrary *threadLibrary, UDATA offset)
{
	UDATA requiredSize;

	requiredSize = hythread_getSize(&(threadLibrary->threadVersion));
	if (requiredSize < offset)  {
		return 0;
	}

	return *((UDATA*) &(((U_8*) threadLibrary)[offset])) != *((UDATA*) &(((U_8*) &MasterThreadLibraryTable)[offset]));
}
/**
 * Allocate a thread library.
 * 
 * Given a pointer to the required version of the thread library allocate and initialize the structure.
 * The startup function is not called (@ref hythread_startup_library) allowing the application to override
 * any functions they desire.  In the event @ref hythread_startup_library fails when called by the application 
 * the thread library memory will be freed.  
 * 
 * @param[in] version The required version of the thread library.
 * @param[out] threadLibrary Pointer to the allocated thread library table.
 *
 * @return 0 on success, negative return value on failure
 *
 * @note threadLibrary will be NULL on failure
 * @note The threadlibrary version must be compatible with the that which we are compiled against
 * @note @ref hythread_shutdown_library will deallocate this memory as part of regular shutdown
 */
I_32
hythread_allocate_library(struct HyThreadLibraryVersion *version, struct HyThreadLibrary **threadLibrary )
{
	UDATA size = hythread_getSize(version);
	HyThreadLibrary *threadLib;
	I_32 rc;

	/* Allocate the memory */
	*threadLibrary = NULL;
	if (0 == size) {
		return -1;
	} else {
		threadLib = j9mem_allocate_threadLibrary(size);
		if (NULL == threadLib) {
			return -1;
		}
	}

	/* Initialize with default values */
	rc = hythread_create_library(threadLib, version, size);
	if (0 == rc) {
		/* Record this was self allocated */
		threadLib->self_handle = threadLib;
		*threadLibrary = threadLib;
	} else {
		j9mem_deallocate_threadLibrary(threadLib);
	}
	return rc;
}

                     

IDATA
hysem_destroy(HyThreadLibrary *threadLib, hysem_t s)
{
	return j9sem_destroy(s);
}

IDATA
hysem_init(HyThreadLibrary *threadLib, hysem_t* sp, I_32 initValue)
{
	return j9sem_init(sp, initValue);
}

IDATA 
hysem_post(HyThreadLibrary *threadLib, hysem_t s)
{
	return j9sem_post(s);
}

IDATA 
hysem_wait(HyThreadLibrary *threadLib, hysem_t s)
{
	return j9sem_wait(s);
}

IDATA 
hythread_attach(HyThreadLibrary *threadLib, hythread_t* handle)
{
	omrthread_attr_t attr;
	IDATA retVal = 0;

	retVal = omrthread_attr_init(&attr);
	if (J9THREAD_SUCCESS != retVal) {
		return -J9THREAD_ERR;
	}

	if (failedToSetAttr(omrthread_attr_set_category(&attr, J9THREAD_CATEGORY_APPLICATION_THREAD))) {
		retVal = J9THREAD_ERR;
		goto destroy_attr;
	}

	retVal = omrthread_attach_ex(handle, &attr);
	if (J9THREAD_SUCCESS != retVal) {
		retVal = J9THREAD_ERR;
	}

destroy_attr:
	omrthread_attr_destroy(&attr);
	return -retVal;
}

IDATA 
hythread_create(HyThreadLibrary *threadLib, hythread_t* handle, UDATA stacksize, UDATA priority, UDATA suspend, hythread_entrypoint_t entrypoint, void* entryarg)
{
	omrthread_attr_t attr;
	IDATA retVal = 0;

	retVal = omrthread_attr_init(&attr);
	if (J9THREAD_SUCCESS != retVal) {
		retVal = J9THREAD_ERR_CANT_ALLOC_CREATE_ATTR;
		return -retVal;
	}

	if (failedToSetAttr(omrthread_attr_set_schedpolicy(&attr, J9THREAD_SCHEDPOLICY_OTHER))) {
		retVal =  J9THREAD_ERR_INVALID_CREATE_ATTR;
		goto destroy_attr;
	}

	/* HACK: the priority must be set after the policy because RTJ might override the schedpolicy */
	if (failedToSetAttr(omrthread_attr_set_priority(&attr, priority))) {
		retVal = J9THREAD_ERR_INVALID_CREATE_ATTR;
		goto destroy_attr;
	}

	if (failedToSetAttr(omrthread_attr_set_stacksize(&attr, stacksize))) {
		retVal = J9THREAD_ERR_INVALID_CREATE_ATTR;
		goto destroy_attr;
	}

	if (failedToSetAttr(omrthread_attr_set_category(&attr, J9THREAD_CATEGORY_APPLICATION_THREAD))) {
		retVal = J9THREAD_ERR_INVALID_CREATE_ATTR;
		goto destroy_attr;
	}

	retVal = omrthread_create_ex(handle, &attr, suspend, entrypoint, entryarg);

destroy_attr:
	omrthread_attr_destroy(&attr);
	return -retVal;
}

void
hythread_detach(HyThreadLibrary *threadLib, hythread_t thread)
{
	omrthread_detach(thread);
}

void OMRNORETURN
hythread_exit(HyThreadLibrary *threadLib, hythread_monitor_t monitor)
{
	omrthread_exit(monitor);

dontreturn: goto dontreturn; /* avoid warnings */
}
	
UDATA* 
hythread_global(HyThreadLibrary *threadLib, char* name)
{
	return omrthread_global(name);
}

IDATA 
hythread_monitor_destroy(HyThreadLibrary *threadLib, hythread_monitor_t monitor)
{
	return omrthread_monitor_destroy(monitor);
}

IDATA 
hythread_monitor_enter(HyThreadLibrary *threadLib, hythread_monitor_t monitor)
{
	return omrthread_monitor_enter(monitor);
}

IDATA 
hythread_monitor_exit(HyThreadLibrary *threadLib, hythread_monitor_t monitor)
{
	return omrthread_monitor_exit(monitor);
}

IDATA 
hythread_monitor_init_with_name(HyThreadLibrary *threadLib, hythread_monitor_t* handle, UDATA flags, char* name)
{
	return omrthread_monitor_init_with_name(handle, flags, name);
}

IDATA 
hythread_monitor_notify(HyThreadLibrary *threadLib, hythread_monitor_t monitor)
{
	return omrthread_monitor_notify(monitor);
}

IDATA
hythread_monitor_notify_all(HyThreadLibrary *threadLib, hythread_monitor_t monitor)
{
	return omrthread_monitor_notify_all(monitor);
}

IDATA 
hythread_monitor_wait(HyThreadLibrary *threadLib, hythread_monitor_t monitor)
{
	return omrthread_monitor_wait(monitor);
}

hythread_t 
hythread_self(HyThreadLibrary *threadLib) 
{
	return omrthread_self();
}

IDATA 
hythread_sleep(HyThreadLibrary *threadLib, I_64 millis)
{
	return omrthread_sleep(millis);
}

IDATA 
hythread_tls_alloc(HyThreadLibrary *threadLib, hythread_tls_key_t* handle)
{
	return omrthread_tls_alloc(handle);
}

IDATA 
hythread_tls_free(HyThreadLibrary *threadLib, hythread_tls_key_t key)
{
	return omrthread_tls_free(key);
}

void*
hythread_tls_get(HyThreadLibrary *threadLib, hythread_t thread, hythread_tls_key_t key) 
{
	return omrthread_tls_get(thread, key);
}

IDATA 
hythread_tls_set(HyThreadLibrary *threadLib, hythread_t thread, hythread_tls_key_t key, void* value)
{
	return omrthread_tls_set(thread, key, value);
}

static intptr_t
failedToSetAttr(intptr_t rc)
{
	rc &= ~J9THREAD_ERR_OS_ERRNO_SET;
	return ((rc != J9THREAD_SUCCESS) && (rc != J9THREAD_ERR_UNSUPPORTED_ATTR));
}
