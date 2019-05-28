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

/* Used Internally */
#define DIR_SEP DIR_SEPARATOR

#define OK 0
#define SUCCESS 0

static intptr_t createMutex(struct J9PortLibrary* portLibrary, struct j9shsem_handle* shsem_handle);
static j9shsem_handle* createsemHandle(struct J9PortLibrary *portLibrary, int nsems, char* baseName);
static intptr_t openMutex(struct J9PortLibrary* portLibrary, struct j9shsem_handle* shsem_handle);

intptr_t
j9shsem_deprecated_openDeprecated (struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct j9shsem_handle** handle, const char* semname, uintptr_t cacheFileType)
{
	return J9PORT_ERROR_SHSEM_OPFAILED;
}

intptr_t 
j9shsem_deprecated_open (struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct j9shsem_handle **handle, const char *semname, int setSize, int permission, uintptr_t flags, J9ControlFileStatus *controlFileStatus)
{
	/* TODO: what happens if setSize == 0? We used to allow the setSize to be 0 so that when the user trying to open
		an existing semaphore they won't need to specify that. However because semaphore set is not part of Windows API
		so we are emulating it using separate name - we need code to find out how large a semaphore set is */
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	char baseFile[J9SH_MAXPATH], mutexName[J9SH_MAXPATH];
	j9shsem_handle* shsem_handle;
	intptr_t rc;
	DWORD waitResult;
	DWORD windowsLastError;
	char *sharedMemoryPathAndFileName;
	int i = 0;
	wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE], *unicodeMutexName;

	Trc_PRT_shsem_j9shsem_open_Entry(semname, setSize, permission);

	/* clear portable error number */
	omrerror_set_last_error(0, 0);

	if (cacheDirName == NULL) {
		Trc_PRT_shsem_j9shsem_deprecated_open_ExitNullCacheDirName();
		return J9PORT_ERROR_SHSEM_OPFAILED;
	}

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

	sharedMemoryPathAndFileName = getSharedMemoryPathandFileName(portLibrary, cacheDirName, semname);
	omrstr_printf(baseFile, J9SH_MAXPATH, "%s_mutex", sharedMemoryPathAndFileName);
	omrmem_free_memory(sharedMemoryPathAndFileName);
	Trc_PRT_shsem_j9shsem_open_builtsemname(baseFile);
	for (i = 0; baseFile[i] != '\0' && i < J9SH_MAXPATH; i++) {
		if (('\\' == baseFile[i]) || (':' == baseFile[i]) || (' ' == baseFile[i])) {
			baseFile[i] = '_';
		}
	}
	Trc_PRT_shsem_j9shsem_open_translatedsemname(baseFile);

	shsem_handle = (*handle) = createsemHandle(portLibrary, setSize, baseFile);
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

	/*First try and see whether the main mutex exists*/
	omrstr_printf(mutexName, J9SH_MAXPATH, "%s", baseFile);
	/* Convert the filename from UTF8 to Unicode */
	unicodeMutexName = port_convertFromUTF8(OMRPORTLIB, mutexName, unicodeBuffer, UNICODE_BUFFER_SIZE);
	if (NULL == unicodeMutexName) {
		rc = J9PORT_ERROR_SHSEM_OPFAILED;
	} else {
		shsem_handle->mainLock = OpenMutexW(MUTEX_ALL_ACCESS, FALSE, unicodeMutexName);
		if (unicodeBuffer != unicodeMutexName) {
			omrmem_free_memory(unicodeMutexName);
		}
  
		if (NULL == shsem_handle->mainLock) {
			if (J9_ARE_NO_BITS_SET(flags, J9SHSEM_OPEN_FOR_DESTROY)) {
				Trc_PRT_shsem_j9shsem_open_Event2(mutexName);
				rc = createMutex(portLibrary, shsem_handle);
			} else {
				Trc_PRT_shsem_j9shsem_open_Event4(mutexName);
				rc = J9PORT_ERROR_SHSEM_OPFAILED_SEMAPHORE_NOT_FOUND;
			}
		} else {
			Trc_PRT_shsem_j9shsem_open_Event1(mutexName);
			rc = openMutex(portLibrary, shsem_handle);
		}
	}
	
	/* release the creation mutex */
	ReleaseMutex(PPG_shsem_creationMutex);
	
	if ((J9PORT_ERROR_SHSEM_OPFAILED == rc) || (J9PORT_ERROR_SHSEM_OPFAILED_SEMAPHORE_NOT_FOUND == rc)) {
		Trc_PRT_shsem_j9shsem_open_Exit1();
		omrfile_error_message();
		return rc;
	}

	Trc_PRT_shsem_j9shsem_open_Exit(rc, (*handle));
	return rc; 	
}

intptr_t 
j9shsem_deprecated_post(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset, uintptr_t flag)
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
   
	if(ReleaseMutex(handle->semHandles[semset])) {
		Trc_PRT_shsem_j9shsem_post_Exit(0);
		return 0;
	} else {
		Trc_PRT_shsem_j9shsem_post_Exit3(0, GetLastError());
		return -1;
	}
}

intptr_t
j9shsem_deprecated_wait(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset, uintptr_t flag)
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
	case WAIT_ABANDONED: /* This means someone has crashed but hasn't released the mutex, we are okay with this */
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
j9shsem_deprecated_getVal(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset)
{
	Trc_PRT_shsem_j9shsem_getVal_Entry(*handle, semset);
	Trc_PRT_shsem_j9shsem_getVal_Exit(0);
	return -1;
}

intptr_t 
j9shsem_deprecated_setVal(struct J9PortLibrary *portLibrary, struct j9shsem_handle *handle, uintptr_t semset, intptr_t value)
{
	Trc_PRT_shsem_j9shsem_setVal_Entry(handle, semset, value);
	Trc_PRT_shsem_j9shsem_setVal_Exit(-1);
	return -1;
}

intptr_t
j9shsem_deprecated_handle_stat(struct J9PortLibrary *portLibrary, struct j9shsem_handle *handle, struct J9PortShsemStatistic *statbuf)
{
	return -1;
}

void 
j9shsem_deprecated_close (struct J9PortLibrary *portLibrary, struct j9shsem_handle **handle)
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
	*handle=NULL;

	Trc_PRT_shsem_j9shsem_close_Exit();
}

intptr_t 
j9shsem_deprecated_destroy (struct J9PortLibrary *portLibrary, struct j9shsem_handle **handle)
{
	Trc_PRT_shsem_j9shsem_destroy_Entry(*handle);
	/*On Windows this just maps to j9shsem_close */
	if((*handle) == NULL) {
		Trc_PRT_shsem_j9shsem_destroy_NullHandle();
		return 0;
	}

	j9shsem_deprecated_close(portLibrary, handle);
	Trc_PRT_shsem_j9shsem_close_Exit();
	return 0;
}

intptr_t 
j9shsem_deprecated_destroyDeprecated (struct J9PortLibrary *portLibrary, struct j9shsem_handle **handle, uintptr_t cacheFileType)
{
	return -1;
}

int32_t
j9shsem_deprecated_startup(struct J9PortLibrary *portLibrary)
{
	PPG_shsem_creationMutex = NULL;
	return 0;
}

void
j9shsem_deprecated_shutdown(struct J9PortLibrary *portLibrary)
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
	if (NULL == result->semHandles) {
		omrmem_free_memory(result->rootName);
		omrmem_free_memory(result);
		return NULL;
	}

	result->setSize = nsems;
            			  
	/* TODO: need to check whether baseName is too long - what should we do if it is?! */
	return result;
}


static intptr_t
createMutex(struct J9PortLibrary* portLibrary, struct j9shsem_handle* shsem_handle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	uint32_t i;
	char mutexName[J9SH_MAXPATH];
	DWORD windowsLastError;
	wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE], *unicodeMutexName;

	Trc_PRT_shsem_j9shsem_createmutex_entered(shsem_handle->rootName);

	/* Convert the mutex name from UTF8 to Unicode */
	unicodeMutexName = port_convertFromUTF8(OMRPORTLIB, shsem_handle->rootName, unicodeBuffer, UNICODE_BUFFER_SIZE);
	if (NULL != unicodeMutexName) {
		shsem_handle->mainLock = CreateMutexW(NULL, TRUE, unicodeMutexName);
		if (unicodeBuffer != unicodeMutexName) {
			omrmem_free_memory(unicodeMutexName);
		}	
	}

	if(shsem_handle->mainLock == NULL) {
		windowsLastError = GetLastError();
		Trc_PRT_shsem_j9shsem_createmutex_exit2(windowsLastError);
		/* can't create mainlock, we can't do anything else :-( */
		return J9PORT_ERROR_SHSEM_OPFAILED;
	}

	Trc_PRT_shsem_j9shsem_createmutex_createdmain();	
	
	for (i = 0; i < shsem_handle->setSize; i++) {
		HANDLE debugHandle;          
		omrstr_printf(mutexName, J9SH_MAXPATH, "%s_set%d", shsem_handle->rootName, i);
		Trc_PRT_shsem_j9shsem_createmutex_creatingset(i, mutexName);   
		  
		unicodeMutexName = port_convertFromUTF8(OMRPORTLIB, mutexName, unicodeBuffer, UNICODE_BUFFER_SIZE);
		if (NULL != unicodeMutexName) {
			debugHandle = shsem_handle->semHandles[i] = CreateMutexW(NULL, FALSE, unicodeMutexName);
			if (unicodeBuffer != unicodeMutexName) {
				omrmem_free_memory(unicodeMutexName);
			}	
		}
		if(shsem_handle->semHandles[i] == NULL) {
			windowsLastError = GetLastError();
			Trc_PRT_shsem_j9shsem_createmutex_exit3(windowsLastError);
			return J9PORT_ERROR_SHSEM_OPFAILED;
		}
	}

	Trc_PRT_shsem_j9shsem_createmutex_exit();
	return J9PORT_INFO_SHSEM_CREATED;
}

static intptr_t
openMutex(struct J9PortLibrary* portLibrary, struct j9shsem_handle* shsem_handle)
{
	/*Open and setup the mutex arrays*/
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	uint32_t i;
	char mutexName[J9SH_MAXPATH];
	DWORD windowsLastError;
	wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE], *unicodeMutexName;

	Trc_PRT_shsem_j9shsem_openmutex_entered(shsem_handle->rootName);

	for (i = 0; i < shsem_handle->setSize; i++) {
		omrstr_printf(mutexName, J9SH_MAXPATH, "%s_set%d", shsem_handle->rootName, i);
		Trc_PRT_shsem_j9shsem_openmutex_openingset(i, mutexName);          

		/*convert the name to unicode*/
		memset(unicodeBuffer, 0, UNICODE_BUFFER_SIZE * sizeof(wchar_t));
		unicodeMutexName = port_convertFromUTF8(OMRPORTLIB, mutexName, unicodeBuffer, UNICODE_BUFFER_SIZE);
		if (NULL != unicodeMutexName) {
			/*CMVC 102996: We use CreateMutexW (NOT OpenMutexW) b/c it is possible CloseHandle() was called by a exiting vm. Which which could result in the mutex handle being removed.*/
			shsem_handle->semHandles[i] = CreateMutexW(NULL, FALSE, unicodeMutexName);
			if (unicodeBuffer != unicodeMutexName) {
				omrmem_free_memory(unicodeMutexName);
			}
		} else {
			Trc_PRT_shsem_j9shsem_openmutex_failedToBuildUnicodeString(
					mutexName,
					omrerror_last_error_number());
			shsem_handle->semHandles[i] = NULL;
			/*Continue to clean up code and return J9PORT_ERROR_SHSEM_OPFAILED*/
		}
		
		if(shsem_handle->semHandles[i] == NULL) {
			uint32_t j;
			windowsLastError = GetLastError();
			Trc_PRT_shsem_j9shsem_openmutex_exit2(windowsLastError);
			for(j=0; j<i; j++) {
				CloseHandle(shsem_handle->semHandles[i]);
			}
			return J9PORT_ERROR_SHSEM_OPFAILED;
		}
	}

	Trc_PRT_shsem_j9shsem_openmutex_exit();
	return J9PORT_INFO_SHSEM_OPENED;
}

int32_t 
j9shsem_deprecated_getid (struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle) 
{
	return 0;
}
