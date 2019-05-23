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

/**
 * @file
 * @ingroup Port
 * @brief Shared Semaphores
 */


#include <Windows.h>
#include "j9port.h"
#include "omrportptb.h"
#include "ut_j9prt.h"
#include "j9shmem.h"
#include "j9shsem.h"

#define J9PORT_SHSEM_CREATIONMUTEX "j9shsemcreationMutex"
#define J9PORT_SHSEM_WAITTIME (2000) /* wait time in ms for creationMutex */
#define J9PORT_SHSEM_NAME_PREFIX "javasemaphore"
#define GLOBAL_NAMESPACE_PREFIX "Global\\"

/* Used Internally */
#define DIR_SEP DIR_SEPARATOR

#define OK 0
#define SUCCESS 0
#define SEMAPHORE_MAX I_32_MAX
static j9shsem_handle* createsemHandle(struct J9PortLibrary *portLibrary, int nsems, char* baseName);
static intptr_t createSemaphore(struct J9PortLibrary* portLibrary, struct j9shsem_handle* shsem_handle, BOOLEAN global);
static intptr_t openSemaphore(struct J9PortLibrary* portLibrary, struct j9shsem_handle* shsem_handle, BOOLEAN global);
static char*
getSemaphoreFullName(struct J9PortLibrary* portLibrary, const struct J9PortShSemParameters *params);
static wchar_t *convertToWindowsFormat(OMRPortLibrary *portLibrary, const char *source, wchar_t *destination, uintptr_t destinationSize, BOOLEAN global);

int32_t
j9shsem_params_init(struct J9PortLibrary *portLibrary, struct J9PortShSemParameters *params) 
{
 	params->semName = NULL; /* Unique identifier of the semaphore. */
 	params->setSize = 1; /* number of semaphores to be created in this set */
 	params->permission = 0; /* No applicable in Windows */
 	params->controlFileDir = NULL; /* Directory in which to create control files (SysV semaphores only */
 	params->proj_id = 1; /* parameter used with semName to generate semaphore key */
 	params->global = 0; /* by default use the session namespace for the semaphore */
 	return 0;
}

/*
 * Convert a UTF-8 string to UTF-16 and prepend a global name space prefix.
 * This allows a user with multiple sessions to share the semaphore.
 * @param portLibrary port library
 * @param source original UTF-8 string
 * @param global prepend global namespace prefix
 * @param destination buffer to receive the UTF-16 string
 * @param destinationSize size of destination buffer in UTF-16 characters
 * @return the destination buffer or NULL on failure.
 */
static wchar_t*
convertToWindowsFormat(OMRPortLibrary *portLibrary, const char *source, wchar_t *destination, uintptr_t destinationSize, BOOLEAN global) {
	wchar_t *destinationCursor = destination;
	wchar_t *result  = destination;

	if (global) {
		size_t prefixLength = strlen(GLOBAL_NAMESPACE_PREFIX);
		wchar_t *result = port_convertFromUTF8(portLibrary, GLOBAL_NAMESPACE_PREFIX, destinationCursor, destinationSize);
		destinationCursor += prefixLength;
		destinationSize -= prefixLength;
	}
	if (NULL != result) {
		result = port_convertFromUTF8(portLibrary, source, destinationCursor, destinationSize);
	}
	if (NULL != result) {
		result = destination;
	}
	return result;
}

intptr_t 
j9shsem_open(struct J9PortLibrary *portLibrary, struct j9shsem_handle **handle, const struct J9PortShSemParameters *params)
{
	/* TODO: what happens if setSize == 0? We used to allow the setSize to be 0 so that when the user trying to open
		an existing semaphore they won't need to specify that. However because semaphore set is not part of Windows API
		so we are emulating it using separate name - we need code to find out how large a semaphore set is */
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	char baseFile[J9SH_MAXPATH], semaphoreName[J9SH_MAXPATH];
	j9shsem_handle* shsem_handle;
	intptr_t rc;
	DWORD waitResult;
	DWORD windowsLastError;
	char *semaphoreFullName;
	int i = 0;
	wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE], *unicodeSemaphoreName;
	BOOLEAN global = (1 == params->global);
	
	Trc_PRT_shsem_j9shsem_open_Entry(params->semName, params->setSize, params->permission);
	/* If global creation mutex doesn't exist, create it */
	if (NULL == PPG_shsem_creationMutex) {
		DWORD lastError = 0;

		/* Security attributes for the global creationMutex is set to public accessible */
		SECURITY_DESCRIPTOR secdes;
		SECURITY_ATTRIBUTES secattr;

		InitializeSecurityDescriptor (&secdes, SECURITY_DESCRIPTOR_REVISION);
		SetSecurityDescriptorDacl(	&secdes, TRUE, NULL, TRUE); 
	
		secattr.nLength=sizeof(SECURITY_ATTRIBUTES);
		secattr.lpSecurityDescriptor=&secdes;
		secattr.bInheritHandle = FALSE; 

		/* Initialize the creationMutex */
		Trc_PRT_shsem_j9shsem_open_globalMutexCreate();
		PPG_shsem_creationMutex = CreateMutex(&secattr, FALSE, J9PORT_SHSEM_CREATIONMUTEX);
		lastError = GetLastError();
		if(NULL == PPG_shsem_creationMutex) {
			if(lastError == ERROR_ALREADY_EXISTS) {
				Trc_PRT_shsem_j9shsem_open_globalMutexOpen(lastError);
				PPG_shsem_creationMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, J9PORT_SHSEM_CREATIONMUTEX);
				lastError = GetLastError();
				if(NULL == PPG_shsem_creationMutex) {
					Trc_PRT_shsem_j9shsem_open_globalMutexOpenFailed(lastError);
					return J9PORT_ERROR_SHSEM_OPFAILED;
				} else {
					Trc_PRT_shsem_j9shsem_open_globalMutexOpenSuccess(lastError);
				}
			} else {
				Trc_PRT_shsem_j9shsem_open_globalMutexCreateFailed(lastError);
				return J9PORT_ERROR_SHSEM_OPFAILED;
			}
		} else {
			Trc_PRT_shsem_j9shsem_open_globalMutexCreateSuccess(lastError);
		}
	} else {
		Trc_PRT_shsem_j9shsem_open_globalMutexExists();
	}

	semaphoreFullName = getSemaphoreFullName(portLibrary, params);
	if (NULL == semaphoreFullName) {
		Trc_PRT_shsem_j9shsem_open_nullSemaphoreFullName();
		return J9PORT_ERROR_SHSEM_OPFAILED;
	}
	omrstr_printf(baseFile, J9SH_MAXPATH, "%s_semaphore", semaphoreFullName);
	omrmem_free_memory(semaphoreFullName);
	Trc_PRT_shsem_j9shsem_open_builtsemname(baseFile);
	for (i = 0; baseFile[i] != '\0' && i < J9SH_MAXPATH; i++) {
		if (('\\' == baseFile[i]) || (':' == baseFile[i]) || (' ' == baseFile[i])) {
			baseFile[i] = '_';
		}
	}
	Trc_PRT_shsem_j9shsem_open_translatedsemname(baseFile);

	shsem_handle = (*handle) = createsemHandle(portLibrary, params->setSize, baseFile);
	if (!shsem_handle) {
		Trc_PRT_shsem_j9shsem_open_nullSemaphoreHandle();
		return J9PORT_ERROR_SHSEM_OPFAILED;
	}

	Trc_PRT_shsem_j9shsem_open_Debug1(baseFile);

	/* Lock the creation mutex */
	waitResult = WaitForSingleObject(PPG_shsem_creationMutex, J9PORT_SHSEM_WAITTIME); 
	windowsLastError = GetLastError();

	if (WAIT_FAILED == waitResult) {
		omrmem_free_memory((*handle)->rootName);
		omrmem_free_memory((*handle)->semHandles);
		omrmem_free_memory(*handle);
		*handle = NULL;
		Trc_PRT_shsem_j9shsem_open_waitglobalmutexfailed(waitResult, windowsLastError);
		return J9PORT_ERROR_SHSEM_OPFAILED;
	}
	if (WAIT_TIMEOUT == waitResult) {
		omrmem_free_memory((*handle)->rootName);
		omrmem_free_memory((*handle)->semHandles);
		omrmem_free_memory(*handle);
		*handle = NULL;
		Trc_PRT_shsem_j9shsem_open_waitglobalmutextimeout(waitResult, windowsLastError);
		return J9PORT_ERROR_SHSEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT;
	}

	Trc_PRT_shsem_j9shsem_open_waitglobalmutex(waitResult, windowsLastError);
	omrstr_printf(semaphoreName, J9SH_MAXPATH, "%s", baseFile);
	/* Convert the filename from UTF8 to Unicode and prepend 'Global\'*/
	unicodeSemaphoreName = convertToWindowsFormat(OMRPORTLIB,
			semaphoreName, unicodeBuffer, UNICODE_BUFFER_SIZE, global);
	if (NULL == unicodeSemaphoreName) {
		rc = J9PORT_ERROR_SHSEM_OPFAILED;
	} else {
		shsem_handle->mainLock = OpenSemaphoreW(SEMAPHORE_ALL_ACCESS, 0, unicodeSemaphoreName);
		if (unicodeBuffer != unicodeSemaphoreName) {
			omrmem_free_memory(unicodeSemaphoreName);
		}
  
		if (shsem_handle->mainLock == NULL) {
			Trc_PRT_shsem_j9shsem_open_Event2(semaphoreName);
			rc = createSemaphore(portLibrary, shsem_handle, global);
		} else {
			Trc_PRT_shsem_j9shsem_open_Event1(semaphoreName);
			rc = openSemaphore(portLibrary, shsem_handle, global);
		}
	}
	
	/* release the creation mutex */
	ReleaseMutex(PPG_shsem_creationMutex);
	
	if (J9PORT_ERROR_SHSEM_OPFAILED == rc) {
		Trc_PRT_shsem_j9shsem_open_Exit1();
		omrfile_error_message();
		return rc;
	}

	Trc_PRT_shsem_j9shsem_open_Exit(rc, (*handle));
	return rc; 	
}

intptr_t 
j9shsem_post(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset, uintptr_t flag)
{
	Trc_PRT_shsem_j9shsem_post_Entry(handle, semset, flag);
	/* flag is ignored on Win32 for now - there is no Undo for semaphore */	
	if(handle == NULL) {
		Trc_PRT_shsem_j9shsem_post_Exit1();
		return J9PORT_ERROR_SHSEM_HANDLE_INVALID;
	}

	if(semset < 0 || semset >= handle->setSize) {
		Trc_PRT_shsem_j9shsem_post_Exit2();
		return J9PORT_ERROR_SHSEM_SEMSET_INVALID;
	}
  
	if(0 != ReleaseSemaphore(handle->semHandles[semset], 1, NULL)) {
		Trc_PRT_shsem_j9shsem_post_Exit(0);
		return 0;
	} else {
		Trc_PRT_shsem_j9shsem_post_Exit3(0, GetLastError());
		return -1;
	}
}

intptr_t
j9shsem_wait(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset, uintptr_t flag)
{
	DWORD timeout,rc;

	Trc_PRT_shsem_j9shsem_wait_Entry(handle, semset, flag);

	if(handle == NULL) {
		Trc_PRT_shsem_j9shsem_wait_Exit1();
		return J9PORT_ERROR_SHSEM_HANDLE_INVALID;
	}
	if(semset < 0 || semset >= handle->setSize) {
		Trc_PRT_shsem_j9shsem_wait_Exit2();
		return J9PORT_ERROR_SHSEM_SEMSET_INVALID;
	}

	if(flag & J9PORT_SHSEM_MODE_NOWAIT) {
		timeout = 0;
	} else {
		timeout = INFINITE;
	} 
   
	rc = WaitForSingleObject(handle->semHandles[semset],timeout);
	
	switch(rc) {
	case WAIT_ABANDONED: /* This means someone has crashed but hasn't released the semaphore, we are okay with this */
	case WAIT_OBJECT_0:
		Trc_PRT_shsem_j9shsem_wait_Exit(0);
		return 0;
	case WAIT_TIMEOUT: /* Falls through */
	case WAIT_FAILED:
		Trc_PRT_shsem_j9shsem_wait_Exit3(-1, rc);
		return -1;
	default:
		Trc_PRT_shsem_j9shsem_wait_Exit3(-1, rc);
		return -1;
	}

}

intptr_t 
j9shsem_getVal(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset)
{
	Trc_PRT_shsem_j9shsem_getVal_Entry(*handle, semset);
	Trc_PRT_shsem_j9shsem_getVal_Exit(0);
	return -1;
}

intptr_t 
j9shsem_setVal(struct J9PortLibrary *portLibrary, struct j9shsem_handle *handle, uintptr_t semset, intptr_t value)
{
	Trc_PRT_shsem_j9shsem_setVal_Entry(handle, semset, value);
	Trc_PRT_shsem_j9shsem_setVal_Exit(-1);
	return -1;
}

void 
j9shsem_close (struct J9PortLibrary *portLibrary, struct j9shsem_handle **handle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	uint32_t i;
	j9shsem_handle* sem_handle = (*handle);
	
	Trc_PRT_shsem_j9shsem_close_Entry(*handle);

	if(*handle == NULL) {
		Trc_PRT_shsem_j9shsem_close_NullHandle();
		return;
	}
   
	for(i=0; i<sem_handle->setSize; i++) {        
		CloseHandle(sem_handle->semHandles[i]);
	}
  
	CloseHandle(sem_handle->mainLock);

	omrmem_free_memory((*handle)->rootName);
	omrmem_free_memory((*handle)->semHandles);
	omrmem_free_memory(*handle);
	*handle = NULL;

	Trc_PRT_shsem_j9shsem_close_Exit();
}

intptr_t 
j9shsem_destroy (struct J9PortLibrary *portLibrary, struct j9shsem_handle **handle)
{
	Trc_PRT_shsem_j9shsem_destroy_Entry(*handle);
	/*On Windows this just maps to j9shsem_close */
	if((*handle) == NULL) {
		Trc_PRT_shsem_j9shsem_destroy_NullHandle();
		return 0;
	}

	j9shsem_close(portLibrary, handle);
	Trc_PRT_shsem_j9shsem_close_Exit();
	return 0;
}


int32_t
j9shsem_startup(struct J9PortLibrary *portLibrary)
{
	PPG_shsem_creationMutex = NULL;
	return 0;
}

void
j9shsem_shutdown(struct J9PortLibrary *portLibrary)
{
	if (NULL != PPG_shsem_creationMutex) {
		CloseHandle(PPG_shsem_creationMutex);
		PPG_shsem_creationMutex = NULL;
	}
}
static j9shsem_handle*
createsemHandle(struct J9PortLibrary *portLibrary, int nsems, char* baseName) 
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	j9shsem_handle* result;

	result = omrmem_allocate_memory(sizeof(j9shsem_handle), OMRMEM_CATEGORY_PORT_LIBRARY);
	if(result == NULL) {
		return NULL;
	}

	result->rootName = omrmem_allocate_memory(strlen(baseName)+1, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == result->rootName) {
		omrmem_free_memory(result);
		return NULL;
	}
	omrstr_printf(result->rootName, J9SH_MAXPATH, "%s", baseName);

	/*Allocating semHandle array*/
	result->semHandles = omrmem_allocate_memory(nsems*sizeof(HANDLE), OMRMEM_CATEGORY_PORT_LIBRARY);
	if(NULL == result->semHandles) {
		omrmem_free_memory(result->rootName);
		omrmem_free_memory(result);
		return NULL;
	}

	result->setSize = nsems;

	/* TODO: need to check whether baseName is too long - what should we do if it is?! */
	return result;
}

static intptr_t
createSemaphore(struct J9PortLibrary* portLibrary, struct j9shsem_handle* shsem_handle, BOOLEAN global)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	uint32_t i;
	char semaphoreName[J9SH_MAXPATH];
	DWORD windowsLastError;
	wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE], *unicodeSemaphoreName;

	Trc_PRT_shsem_j9shsem_createsemaphore_entered(shsem_handle->rootName);

	/* Convert the semaphore name from UTF8 to Unicode */
	unicodeSemaphoreName = convertToWindowsFormat(OMRPORTLIB, shsem_handle->rootName, unicodeBuffer, UNICODE_BUFFER_SIZE, global);
	if (NULL != unicodeSemaphoreName) {
		shsem_handle->mainLock = CreateSemaphoreW(NULL, 0, SEMAPHORE_MAX, unicodeSemaphoreName);
		if(unicodeBuffer != unicodeSemaphoreName) {
			omrmem_free_memory(unicodeSemaphoreName);
		}	
	}

	if(shsem_handle->mainLock == NULL) {
		windowsLastError = GetLastError();
		Trc_PRT_shsem_j9shsem_createsemaphore_exiterror(windowsLastError);
		/* can't create mainlock, we can't do anything else :-( */
		return J9PORT_ERROR_SHSEM_OPFAILED;
	}

	Trc_PRT_shsem_j9shsem_createsemaphore_createdmain();	
	
	for (i = 0; i < shsem_handle->setSize; i++) {
		HANDLE debugHandle;
		omrstr_printf(semaphoreName, J9SH_MAXPATH, "%s_set%d", shsem_handle->rootName, i);
		Trc_PRT_shsem_j9shsem_createsemaphore_creatingset(i, semaphoreName);   
		  
		unicodeSemaphoreName = convertToWindowsFormat(OMRPORTLIB, semaphoreName, unicodeBuffer, UNICODE_BUFFER_SIZE, global);
		if (NULL != unicodeSemaphoreName) {
			debugHandle = shsem_handle->semHandles[i] = CreateSemaphoreW(NULL, 0, SEMAPHORE_MAX, unicodeSemaphoreName);
			if (unicodeBuffer != unicodeSemaphoreName) {
				omrmem_free_memory(unicodeSemaphoreName);
			}	
		}
		if(shsem_handle->semHandles[i] == NULL) {
			windowsLastError = GetLastError();
			Trc_PRT_shsem_j9shsem_createsemaphore_exiterror(windowsLastError);
			return J9PORT_ERROR_SHSEM_OPFAILED;
		}
	}

	Trc_PRT_shsem_j9shsem_createsemaphore_exit();
	return J9PORT_INFO_SHSEM_CREATED;
}
static intptr_t
openSemaphore(struct J9PortLibrary* portLibrary, struct j9shsem_handle* shsem_handle, BOOLEAN global)
{
	/*Open and setup the semaphore arrays*/
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	uint32_t i;
	char semaphoreName[J9SH_MAXPATH];
	DWORD windowsLastError;
	wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE], *unicodeSemaphoreName;

	Trc_PRT_shsem_j9shsem_opensemaphore_entered(shsem_handle->rootName);

	for (i = 0; i < shsem_handle->setSize; i++) {
		omrstr_printf(semaphoreName, J9SH_MAXPATH, "%s_set%d", shsem_handle->rootName, i);
		Trc_PRT_shsem_j9shsem_opensemaphore_openingset(i, semaphoreName);          

		/*convert the name to unicode*/
		memset(unicodeBuffer, 0, UNICODE_BUFFER_SIZE * sizeof(wchar_t));
		unicodeSemaphoreName = convertToWindowsFormat(OMRPORTLIB, semaphoreName, unicodeBuffer, UNICODE_BUFFER_SIZE, global);
		if (NULL != unicodeSemaphoreName) {
			shsem_handle->semHandles[i] = OpenSemaphoreW(SEMAPHORE_ALL_ACCESS, 0, unicodeSemaphoreName);
			if (unicodeBuffer != unicodeSemaphoreName) {
				omrmem_free_memory(unicodeSemaphoreName);
			}
		} else {
			Trc_PRT_shsem_j9shsem_opensemaphore_failedToBuildUnicodeString(
					semaphoreName,
					omrerror_last_error_number());
			shsem_handle->semHandles[i] = NULL;
			/*Continue to clean up code and return J9PORT_ERROR_SHSEM_OPFAILED*/
		}
		
		if(shsem_handle->semHandles[i] == NULL) {
			uint32_t j;
			windowsLastError = GetLastError();
			Trc_PRT_shsem_j9shsem_opensemaphore_exiterror(windowsLastError);
			for(j=0; j<i; j++) {
				CloseHandle(shsem_handle->semHandles[i]);
			}
			return J9PORT_ERROR_SHSEM_OPFAILED;
		}
	}

	Trc_PRT_shsem_j9shsem_opensemaphore_exit();
	return J9PORT_INFO_SHSEM_OPENED;
}

/* Create a semaphore name acceptable to the Windows semaphore open and create functions.
 * @param[in] portLibrary The port library.
 * @param[in] J9PortShSemParameters struct containing semaphore name and control directory.
 * @return Semaphore full name derived from semaphore name and control directory
 * The caller must release the storage allocated for the semaphore full name.
 * 
 * Filters out backslashes from the control directory path string and other non-numeric characters caused by path conversion.
 */
static char*
getSemaphoreFullName(struct J9PortLibrary* portLibrary, const struct J9PortShSemParameters *params)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	char *result, *readPtr, *writePtr;
	uintptr_t resultLen;

	resultLen = strlen(params->semName)
		+strlen(params->controlFileDir)
		+2; /* add space for the underscore between controlFileDir and semName and for the null terminator */
	result = omrmem_allocate_memory(resultLen, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == result) {
		return NULL;
	}

	omrstr_printf(result, resultLen, "%s_%s", params->controlFileDir, params->semName);
	readPtr = writePtr = result;
	while ('\0' != *readPtr) { /* get rid of non-alphanumeric characters */
		if (isalnum(*readPtr) || ('_' == *readPtr)) {
			*writePtr = *readPtr;
			++writePtr;
		}
		++readPtr;
	}
	*writePtr = '\0';
	return result;
}

