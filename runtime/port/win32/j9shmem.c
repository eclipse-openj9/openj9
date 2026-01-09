/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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

/**
 * @file
 * @ingroup Port
 * @brief Shared Memory Semaphores
 */


#include <Windows.h>
#include <shlobj.h>
#include "j9port.h"
#include "omrportptb.h"
#include "portpriv.h"
#include "portnls.h"
#include "ut_j9prt.h"
#include "j9shmem.h"
#include "protect_helpers.h"
#include "fltconst.h"
#include "shchelp.h"

#define ENV_LOCALAPPDATA "LOCALAPPDATA"
#define ENV_APPDATA "APPDATA"
#define ENV_TEMP "TEMP"
#define DIR_TEMP "C:\\TEMP"
#define DIR_CROOT "C:\\"

#define J9SH_SUCCESS 0
#define J9SH_FAILED 1
#define J9SH_ERROR 2

#define J9PORT_SHMEM_CREATIONMUTEX "j9shmemcreationMutex"
#define J9PORT_SHMEM_WAITTIME (5000) /* wait time in ms for creationMutex */
static int32_t convertFileMapPerm (int32_t perm);
static int32_t createMappedFile(struct J9PortLibrary* portLibrary, const char* cacheDirName, struct j9shmem_handle* handle, uintptr_t flags);
static uintptr_t isSharedMemoryFileName(struct J9PortLibrary* portLibrary, const char* filename);
static int32_t findError (int32_t errorCode, int32_t errorCode2);
char* getModifiedSharedMemoryPathandFileName(struct J9PortLibrary* portLibrary, const char* cacheDirName, const char* sharedMemoryFileName);
static void convertSlash (char *pathname);
static int64_t convertFileTimeToUnixEpoch(const FILETIME* time);
static int32_t convertPerm (int32_t perm);
static intptr_t createDirectory(struct J9PortLibrary *portLibrary, const char *pathname);

/* In j9shchelp.c */
extern uintptr_t isCacheFileName(J9PortLibrary *, const char *, uintptr_t, const char *);

intptr_t
j9shmem_openDeprecated (struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct j9shmem_handle **handle, const char* rootname, uint32_t perm, uintptr_t cacheFileType, uint32_t category)
{
	return J9PORT_ERROR_SHMEM_OPFAILED;
}

intptr_t
j9shmem_open(struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct j9shmem_handle **handle, const char *rootname, uintptr_t size, uint32_t perm, uint32_t category, uintptr_t flags, J9ControlFileStatus *controlFileStatus)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t rc = J9PORT_ERROR_SHMEM_OPFAILED;
	uintptr_t rootNameLength=0;
	DWORD waitResult;
	struct j9shmem_handle *myHandle;
	char *modifiedSharedMemoryFileName;
	char *tempName;

	Trc_PRT_shmem_j9shmem_open_Entry(rootname, size, perm);

	/* clear portable error number */
	omrerror_set_last_error(0, 0);

	if (NULL != controlFileStatus) {
		memset(controlFileStatus, 0, sizeof(J9ControlFileStatus));
	}
	
	if (NULL == cacheDirName) {
		Trc_PRT_shmem_j9shmem_open_ExitNullCacheDirName();
		return J9PORT_ERROR_SHMEM_OPFAILED;
	}

	if (NULL == PPG_shmem_creationMutex) {
		DWORD lastError = 0;

		/* Security attributes for the creationMutex is set to public accessible */
		SECURITY_DESCRIPTOR secdes;
		SECURITY_ATTRIBUTES secattr;
	
		InitializeSecurityDescriptor (&secdes, SECURITY_DESCRIPTOR_REVISION);
		SetSecurityDescriptorDacl(	&secdes, TRUE, NULL, TRUE); 
	
		secattr.nLength=sizeof(SECURITY_ATTRIBUTES);
		secattr.lpSecurityDescriptor=&secdes;
		secattr.bInheritHandle = FALSE; 

		/* Initialize the creationMutex */
		Trc_PRT_shmem_j9shmem_open_globalMutexCreate();
		PPG_shmem_creationMutex = CreateMutex(&secattr, FALSE, J9PORT_SHMEM_CREATIONMUTEX);
		lastError = GetLastError();
		if(NULL == PPG_shmem_creationMutex) {
			if(lastError == ERROR_ALREADY_EXISTS) {
				Trc_PRT_shmem_j9shmem_open_globalMutexOpen(lastError);
				PPG_shmem_creationMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, J9PORT_SHMEM_CREATIONMUTEX);
				lastError = GetLastError();
				if(NULL == PPG_shmem_creationMutex) {
					Trc_PRT_shmem_j9shmem_open_globalMutexOpenFailed(lastError);
					return J9PORT_ERROR_SHMEM_OPFAILED;
				} else {
					Trc_PRT_shmem_j9shmem_open_globalMutexOpenSuccess(lastError);
				}
			} else {
				Trc_PRT_shmem_j9shmem_open_globalMutexCreateFailed(lastError);
				return J9PORT_ERROR_SHMEM_OPFAILED;
			}
		} else {
			Trc_PRT_shmem_j9shmem_open_globalMutexCreateSuccess(lastError);
		}	
	} else {
			Trc_PRT_shmem_j9shmem_open_globalMutexExists();
	}

	myHandle = omrmem_allocate_memory(sizeof(struct j9shmem_handle) + strlen(rootname) + 1, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == myHandle) {
		Trc_PRT_shmem_j9shmem_open_Exit(J9PORT_ERROR_SHMEM_OPFAILED, myHandle);	
		return J9PORT_ERROR_SHMEM_OPFAILED;
	}
	
	tempName = (char*)((uintptr_t)myHandle + sizeof(struct j9shmem_handle));
	strcpy(tempName, rootname);
	myHandle->rootName = (const char*)tempName;
	
	/* Lock the creation mutex */
	waitResult = WaitForSingleObject(PPG_shmem_creationMutex, J9PORT_SHMEM_WAITTIME); 
	if (WAIT_TIMEOUT == waitResult) {
		omrmem_free_memory(myHandle);
		Trc_PRT_shmem_j9shmem_open_Exit(J9PORT_ERROR_SHMEM_OPFAILED, myHandle);	
		return J9PORT_ERROR_SHMEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT;
	}

#if defined(J9SHMEM_DEBUG)
	omrtty_printf("j9shmem_open: calling OpenFileMapping: %s\n",myHandle->rootName);
#endif /* J9SHMEM_DEBUG */

	/* copy all the flags into the handle, so that createMappedFile knows what to do*/	
	myHandle -> region = NULL;
	myHandle -> perm = perm;
	myHandle -> size = size;

	modifiedSharedMemoryFileName = getModifiedSharedMemoryPathandFileName(portLibrary, cacheDirName, myHandle->rootName);
	if (NULL == modifiedSharedMemoryFileName) {
		Trc_PRT_shmem_j9shmem_open_Exit2();
		return J9PORT_ERROR_SHMEM_OPFAILED;
	}

	/* First we would try to see whether we can open an existing File mapping, if we can use it */	
	myHandle->shmHandle = OpenFileMapping( convertFileMapPerm(perm), FALSE, modifiedSharedMemoryFileName); 									 
	if (NULL == myHandle->shmHandle) {
		Trc_PRT_shmem_j9shmem_open_Event1(modifiedSharedMemoryFileName);
		if (J9PORT_ERROR_SHMEM_OPFAILED == (rc = createMappedFile(portLibrary, cacheDirName, myHandle, flags))) {
			omrmem_free_memory(myHandle);
			Trc_PRT_shmem_j9shmem_open_Event4();
		} 
	} else {
		Trc_PRT_shmem_j9shmem_open_Event2(modifiedSharedMemoryFileName);
		myHandle->mappedFile = 0;
		rc = J9PORT_INFO_SHMEM_OPENED;
	}

	omrmem_free_memory(modifiedSharedMemoryFileName);

	/* release the creation mutex */
	ReleaseMutex(PPG_shmem_creationMutex);
	
	if (J9PORT_ERROR_SHMEM_OPFAILED == rc) {
		omrfile_error_message();
		Trc_PRT_shmem_j9shmem_open_Exit1();
	} else {
		*handle=myHandle;
		if (j9shmem_attach(portLibrary, myHandle, category) == NULL) {
			/*The call must clean up the handle in this case by calling j9shxxx_destroy */
			if (rc == J9PORT_INFO_SHMEM_CREATED) {
				rc = J9PORT_ERROR_SHMEM_CREATE_ATTACHED_FAILED;
				Trc_PRT_shmem_j9shmem_open_CreateAndAttachFailed();
			} else {
				rc = J9PORT_ERROR_SHMEM_OPEN_ATTACHED_FAILED;
				Trc_PRT_shmem_j9shmem_open_OpenAndAttachFailed();
			}
			return rc;
		} else {
			Trc_PRT_shmem_j9shmem_open_Attached();
		}
	 	Trc_PRT_shmem_j9shmem_open_Exit(rc, myHandle);	
	}
	return rc;
}

void*
j9shmem_attach(struct J9PortLibrary *portLibrary, struct j9shmem_handle* handle, uint32_t categoryCode)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	Trc_PRT_shmem_j9shmem_attach_Entry(handle);

	if (NULL != handle) {
		if (NULL != handle->region) {
			Trc_PRT_shmem_j9shmem_attach_Exit(handle->region);
			return handle->region;
		}
		if(NULL != handle->shmHandle) {
			int32_t permission = convertFileMapPerm(handle->perm);
			OMRMemCategory * category = omrmem_get_category(categoryCode);
			handle->region = MapViewOfFile(handle->shmHandle, permission, 0, 0, 0);
			handle->category = category;
			if(NULL != handle->region) {
				Trc_PRT_shmem_j9shmem_attach_Exit(handle->region);
				omrmem_categories_increment_counters(category, handle->size);
				return handle->region;
			} else {
				Trc_PRT_shmem_j9shmem_attach_Exit2(GetLastError());
			}
		}
	} 

	Trc_PRT_shmem_j9shmem_attach_Exit1();
	return NULL;
}

intptr_t
j9shmem_detach(struct J9PortLibrary* portLibrary, struct j9shmem_handle **handle) 
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	Trc_PRT_shmem_j9shmem_detach_Entry(*handle);
	if(NULL == (*handle)->region) {
		Trc_PRT_shmem_j9shmem_detach_Exit();
		return 0;
	}

	if(UnmapViewOfFile((*handle)->region)) {
		(*handle)->region = NULL;
		omrmem_categories_decrement_counters((*handle)->category, (*handle)->size);
		Trc_PRT_shmem_j9shmem_detach_Exit();
		return 0;
	}

	Trc_PRT_shmem_j9shmem_detach_Exit1();
	return -1;
}

intptr_t 
j9shmem_destroy (struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct j9shmem_handle **handle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t rc = 0;
	char *sharedMemoryMappedFile;

	Trc_PRT_shmem_j9shmem_destroy_Entry(*handle);

	if (cacheDirName == NULL) {
		Trc_PRT_shmem_j9shmem_destroy_ExitNullCacheDirName();
		return -1;
	}

	sharedMemoryMappedFile = getSharedMemoryPathandFileName(portLibrary, cacheDirName, (*handle)->rootName);
	if (NULL == sharedMemoryMappedFile) {
		Trc_PRT_shmem_j9shmem_destroy_NullMappedFile();
		return rc;
	}
	
	j9shmem_close(portLibrary, handle);

	rc = omrfile_unlink(sharedMemoryMappedFile);
	
	Trc_PRT_shmem_j9shmem_destroy_Debug1(sharedMemoryMappedFile, rc, GetLastError());

	omrmem_free_memory(sharedMemoryMappedFile);

	if (-1 == rc) {
		Trc_PRT_shmem_j9shmem_destroy_Exit1();
	} else {
		Trc_PRT_shmem_j9shmem_destroy_Exit();
	}

	return rc;
}

intptr_t 
j9shmem_destroyDeprecated (struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct j9shmem_handle **handle, uintptr_t cacheFileType)
{
	return j9shmem_destroy (portLibrary, cacheDirName, groupPerm, handle);
}

void
j9shmem_shutdown(struct J9PortLibrary *portLibrary)
{
	if (NULL != portLibrary->portGlobals) {
		if (NULL != PPG_shmem_creationMutex) {
			CloseHandle(PPG_shmem_creationMutex);
		}
	}
}

int32_t
j9shmem_startup(struct J9PortLibrary *portLibrary)
{
	/* make sure PPG_shmem_creationMutex is null */
	if (NULL != portLibrary->portGlobals) {
		PPG_shmem_creationMutex = NULL;
	}
	
	return 0;
}

void 
j9shmem_close(struct J9PortLibrary *portLibrary, struct j9shmem_handle **handle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	Trc_PRT_shmem_j9shmem_close_Entry(*handle);
	j9shmem_detach(portLibrary, handle);
	if (0 != (*handle)->mappedFile) {
		omrfile_close((*handle)->mappedFile);
	}
	CloseHandle((*handle)->shmHandle);
	omrmem_free_memory(*handle);
	*handle=NULL;
	Trc_PRT_shmem_j9shmem_close_Exit();
}

void
j9shmem_findclose(struct J9PortLibrary *portLibrary, uintptr_t findhandle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	Trc_PRT_shmem_j9shmem_findclose_Entry(findhandle);
	omrfile_findclose(findhandle);
	Trc_PRT_shmem_j9shmem_findclose_Exit();
}

uintptr_t
j9shmem_findfirst(struct J9PortLibrary *portLibrary, char *cacheDirName, char *resultbuf)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	uintptr_t findHandle;

	Trc_PRT_shmem_j9shmem_findfirst_Entry();

	if (cacheDirName == NULL) {
		Trc_PRT_shmem_j9shmem_findfirst_Exit3();
		return (uintptr_t)-1;
	}

	findHandle = omrfile_findfirst(cacheDirName, resultbuf);

	if(findHandle == (uintptr_t)-1) {
		Trc_PRT_shmem_j9shmem_findfirst_Exit1();
		return (uintptr_t)-1;
	}

	while (!isSharedMemoryFileName(portLibrary, resultbuf)) {
		if (-1 == omrfile_findnext(findHandle, resultbuf)) {
			omrfile_findclose(findHandle);
			Trc_PRT_shmem_j9shmem_findfirst_Exit2();
			return (uintptr_t)-1;
		}
	}

	Trc_PRT_shmem_j9shmem_findfirst_Exit();
	return findHandle;
}

int32_t
j9shmem_findnext(struct J9PortLibrary *portLibrary, uintptr_t findHandle, char *resultbuf)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	Trc_PRT_shmem_j9shmem_findnext_Entry(findHandle);
	if (-1 == omrfile_findnext(findHandle, resultbuf)) {
		Trc_PRT_shmem_j9shmem_findnext_Exit1();
		return -1;
	}
	
	while (!isSharedMemoryFileName(portLibrary, resultbuf)) {
		if (-1 == omrfile_findnext(findHandle, resultbuf)) {
			Trc_PRT_shmem_j9shmem_findnext_Exit2();
			return -1;
		}
	}

	Trc_PRT_shmem_j9shmem_findnext_Exit();
	return 0;
}

uintptr_t
j9shmem_stat(struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, const char* name, struct J9PortShmemStatistic* statbuf)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int64_t size;
	HANDLE memHandle;
	BY_HANDLE_FILE_INFORMATION FileInformation;
	char * sharedMemoryFullPath;
	wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE], *unicodePath;
	
	Trc_PRT_shmem_j9shmem_stat_Entry(name);
	if(statbuf == NULL) {
		Trc_PRT_shmem_j9shmem_stat_statbufIsNull();
		return -1;
	}
	if (cacheDirName == NULL) {
		Trc_PRT_shmem_j9shmem_stat_ExitNullCacheDirName();
		return -1;
	}

	sharedMemoryFullPath = getSharedMemoryPathandFileName(portLibrary, cacheDirName, name);

	/* Convert the filename from UTF8 to Unicode */
	unicodePath = port_file_get_unicode_path(OMRPORTLIB, sharedMemoryFullPath, unicodeBuffer, UNICODE_BUFFER_SIZE);
	if (NULL == unicodePath) {
		omrmem_free_memory(sharedMemoryFullPath);
		Trc_PRT_shmem_j9shmem_stat_unicodePathNull();
		return -1;
	}
	
	/* The port library function j9file_open() can't be used because 'GetFileInformationByHandle()' is used below.
	 * Also, because the share classes cache file is created with (FILE_SHARE_READ | FILE_SHARE_WRITE) on windows
	 * we must be sure we call CreateFileW with exactly the same options. Otherwise the file will fail to be opened.
	 */
	memHandle = CreateFileW(unicodePath, GENERIC_READ, (FILE_SHARE_READ | FILE_SHARE_WRITE), NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	
	if (unicodeBuffer != unicodePath) {
		omrmem_free_memory(unicodePath);
	}
		
	if ((NULL == memHandle) || (INVALID_HANDLE_VALUE==memHandle)) {
		Trc_PRT_shmem_j9shmem_stat_Exit2_V2(sharedMemoryFullPath, GetLastError());
		omrmem_free_memory(sharedMemoryFullPath);
		return -1;
	}

	omrmem_free_memory(sharedMemoryFullPath);

	statbuf->controlDir=(char *)cacheDirName;

	if(GetFileInformationByHandle(memHandle, &FileInformation) == 0) {
		/* GetFileInformationByHandle() should not fail if we opened a file successfully with
		 * CreateFileW(). On the off chance it does make sure statbuf members are set before exiting.
		 */
		uintptr_t lasterr = (uintptr_t)GetLastError();
		CloseHandle(memHandle);
		statbuf->file=NULL;
		statbuf->nattach=-1;
		statbuf->shmid=-1;
		statbuf->lastAttachTime=-1;
		statbuf->lastDetachTime=-1;
		statbuf->lastChangeTime=-1;
		memset(&statbuf->perm, 0, sizeof(J9Permission));
		statbuf->size=0;
		Trc_PRT_shmem_j9shmem_stat_GetFileInformationByHandleFailed_Exit(sharedMemoryFullPath, lasterr);
		return 0;
	}

	statbuf->file=NULL;
	statbuf->nattach=-1;
	statbuf->shmid=-1;
	statbuf->lastAttachTime=-1;
	statbuf->lastDetachTime=convertFileTimeToUnixEpoch(&FileInformation.ftLastAccessTime);
	statbuf->lastChangeTime=convertFileTimeToUnixEpoch(&FileInformation.ftLastWriteTime);
	memset(&statbuf->perm, 0, sizeof(J9Permission));
	size = ((int64_t)FileInformation.nFileSizeHigh) << 32;
	size += (int64_t)FileInformation.nFileSizeLow;
	statbuf->size = (uintptr_t)size;		/* Cast should be safe as cache cannot be bigger than 2gig */
	
	CloseHandle(memHandle);
	Trc_PRT_shmem_j9shmem_stat_Exit();
	return 0;
}

uintptr_t
j9shmem_statDeprecated(struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, const char* name, struct J9PortShmemStatistic* statbuf, uintptr_t cacheFileType)
{
	return -1;
}

intptr_t
j9shmem_handle_stat(struct J9PortLibrary *portLibrary, struct j9shmem_handle *handle, struct J9PortShmemStatistic *statbuf)
{
	return -1;
}

static int32_t 
convertPerm (int32_t perm) 
{
	switch(perm) {
	case J9SH_SHMEM_PERM_READ_WRITE :
		return PAGE_READWRITE;
		break;
	case J9SH_SHMEM_PERM_READ :
		return PAGE_READONLY;
		break;
	}
	return 0;
}

static int32_t
convertFileMapPerm (int32_t perm) {
	switch(perm) {
	case J9SH_SHMEM_PERM_READ :
		return FILE_MAP_READ;
		break;
	case J9SH_SHMEM_PERM_READ_WRITE :
		return FILE_MAP_ALL_ACCESS;
		break;
	}
	return 0;
}

static void 
convertSlash (char *pathname) 
{
	uintptr_t i;
	uintptr_t length = strlen(pathname);

	for(i=0; i < length; i++) {
		if(pathname[i] == '\\') {
		 pathname[i] = '/';
		}
	}
}

static intptr_t
createDirectory(struct J9PortLibrary *portLibrary, const char *pathname)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	char tempPath[J9SH_MAXPATH];
	int32_t currentIndex;
	int32_t pathlength;

	/* Port library function can assumes only the final directory on a path needs creating,
	 * and that the other higher level ones already exist.
	 */
	if (0 == omrfile_mkdir(pathname)) {
		return 0; /* successfully created */
	} else {
		if (omrerror_last_error_number() == J9PORT_ERROR_FILE_EXIST) {
			return 0; /* already exists */
		}
	}
		
	/* Dropped to here, so need to build each piece of the path */
		
	/* Skip any 'C:' type drive spec on the front of the path */
	if (strlen(pathname) > 1 && *(pathname+1)==':') {
		currentIndex = 2;
	} else {
		currentIndex = 0;
	}
	pathlength = (int32_t)strlen(pathname);

	/* Go along the path, something like "C:\dir1\dir2\dir3" */
	while (currentIndex<pathlength) {
		currentIndex++;
		/* skip to the next separator or end of the path */
		while (*(pathname+currentIndex)!=0 && *(pathname+currentIndex)!=DIR_SEPARATOR) currentIndex++;
		
		/* create the partial path we are now interested in, eg. "C:\dir1" */
		strncpy(tempPath,pathname,currentIndex);
		*(tempPath+currentIndex)=0;
		/*printf("Mkdir on %s\n",tempPath);*/
		/* build it, cope with it already existing */
		if (0 != omrfile_mkdir(tempPath)) {
			if (omrerror_last_error_number() != J9PORT_ERROR_FILE_EXIST) {
				/* there is a genuine error, exit! */
				return -1;
			}
		}
	}
	return 0;
}
/**
 * @internal
 * Creates a file in mapped memory
 *
 * @param[in] portLibrary The port library
 * @param[in] handle the shared memory semaphore to be created
 *
 * @return J9PORT_ERROR_SHMEM_OPFAILED on failure, J9PORT_INFO_SHMEM_OPENED or J9PORT_INFO_SHMEM_CREATED on success
 */
static int32_t
createMappedFile(struct J9PortLibrary* portLibrary, const char* cacheDirName , struct j9shmem_handle* handle, uintptr_t flags)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc;
	char* shareMemoryFileName;
	char* modifiedSharedMemoryFileName;
	DWORD maxSizeHigh32, maxSizeLow32;

	Trc_PRT_shmem_createMappedFile_entry();
	shareMemoryFileName = getSharedMemoryPathandFileName(portLibrary, cacheDirName, handle->rootName);
	if (NULL == shareMemoryFileName) {
		Trc_PRT_shmem_createMappedFile_getFileNameFailed();
		return J9PORT_ERROR_SHMEM_OPFAILED;
	}

	Trc_PRT_shmem_createMappedFile_fileName(shareMemoryFileName);

#if defined(J9SHMEM_DEBUG)
	omrtty_printf("createMappedFile - trying to create Memory Mapped file = %s!\n", shareMemoryFileName);
#endif /* J9SHMEM_DEBUG */

	if ((handle->perm != J9SH_SHMEM_PERM_READ) && (J9_ARE_NO_BITS_SET(flags, J9SHMEM_OPEN_FOR_DESTROY))) {
		handle->mappedFile = omrfile_open(shareMemoryFileName, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0);
	} else {
		handle->mappedFile = -1;
	}
	if (-1 == handle->mappedFile) {
#if defined(J9SHMEM_DEBUG)
		omrtty_printf("createMappedFile - createNew failed, so old file must be there!\n");
#endif /*J9SHMEM_DEBUG */
		Trc_PRT_shmem_createMappedFile_fileCreateFailed();
		/* CreateNew failed, it probably means that memory mapped file is there */
		if (J9SH_SHMEM_PERM_READ == handle->perm) {
			handle->mappedFile = omrfile_open(shareMemoryFileName, EsOpenRead, 0);
		} else {
			handle->mappedFile = omrfile_open(shareMemoryFileName, EsOpenRead | EsOpenWrite, 0);
		}
		if (-1 == handle->mappedFile) {
#if defined(J9SHMEM_DEBUG)
		omrtty_printf("createMappedFile - Opening existing file failed - weird!\n");
#endif /*J9SHMEM_DEBUG */
			Trc_PRT_shmem_createMappedFile_fileOpenFailed();
			omrmem_free_memory(shareMemoryFileName);
			return J9PORT_ERROR_SHMEM_OPFAILED;	
		}	
		Trc_PRT_shmem_createMappedFile_fileOpenSucceeded();	
		handle->size = 0;
		rc = J9PORT_INFO_SHMEM_OPENED;	
	} else {
		wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE], *unicodePath;

		Trc_PRT_shmem_createMappedFile_fileCreateSucceeded();

		/* Convert the filename from UTF8 to Unicode */
		unicodePath = port_file_get_unicode_path(OMRPORTLIB, shareMemoryFileName, unicodeBuffer, UNICODE_BUFFER_SIZE);
		if (NULL != unicodePath) {
			/* MoveFileEx allows us to delete the share cache after the next reboot */
			MoveFileExW(unicodePath, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
			if(unicodeBuffer != unicodePath) {
				omrmem_free_memory(unicodePath);
			}
		}

		rc = J9PORT_INFO_SHMEM_CREATED;		
	}

	modifiedSharedMemoryFileName = getModifiedSharedMemoryPathandFileName(portLibrary, cacheDirName, handle->rootName);
	if (NULL == modifiedSharedMemoryFileName) {
		omrmem_free_memory(shareMemoryFileName);
		Trc_PRT_shmem_createMappedFile_modifiedSharedMemoryFileNameNull();
		return J9PORT_ERROR_SHMEM_OPFAILED;
	}

#if defined(WIN64)
	maxSizeHigh32 = HIGH_U32_FROM_LONG64(handle->size);
	maxSizeLow32 = LOW_U32_FROM_LONG64(handle->size);
#else
	maxSizeHigh32 = 0;
	maxSizeLow32 = handle->size;
#endif

	handle->shmHandle = CreateFileMapping( (HANDLE) handle->mappedFile, NULL, convertPerm(handle->perm), maxSizeHigh32, maxSizeLow32, modifiedSharedMemoryFileName);
	if (NULL == handle->shmHandle) {
		/* Need to clean up the file */
		DWORD lastError = GetLastError();
		Trc_PRT_shmem_createMappedFile_createFileMappingFailed(handle->mappedFile, convertPerm(handle->perm), handle->size, modifiedSharedMemoryFileName, lastError);
		omrfile_close(handle->mappedFile);
		omrfile_unlink(shareMemoryFileName);
		
#if defined(J9SHMEM_DEBUG)
		omrtty_printf("Error create file mapping\n");
#endif /*J9SHMEM_DEBUG */
		rc=J9PORT_ERROR_SHMEM_OPFAILED;	
	} else {
		Trc_PRT_shmem_createMappedFile_createFileMappingSucceeded(handle->mappedFile, convertPerm(handle->perm), handle->size, modifiedSharedMemoryFileName);
	}

	Trc_PRT_shmem_createMappedFile_exit(rc);
	omrmem_free_memory(shareMemoryFileName);
	omrmem_free_memory(modifiedSharedMemoryFileName);
	return rc;
}

char*
getSharedMemoryPathandFileName(struct J9PortLibrary* portLibrary, const char*  cacheDirName, const char* sharedMemoryFileName)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	char* result;
	uintptr_t resultLen;

	/* the string is cacheDirName\sharedMemoryFileName\0 */
	resultLen = strlen(sharedMemoryFileName)+strlen(cacheDirName)+2;
	result = omrmem_allocate_memory(resultLen, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == result) {
		return NULL;
	}

	omrstr_printf(result, resultLen, "%s\\%s", cacheDirName, sharedMemoryFileName);

	return result;
}

/* Note that, although it's the responsibility of the caller to generate the
 * version prefix and generation postfix, the function this calls strictly checks
 * the format used by shared classes. It would be much better to have a more flexible
 * API that takes a call-back function, but we decided not to change the API this release */ 
static uintptr_t
isSharedMemoryFileName(struct J9PortLibrary* portLibrary, const char* filename)
{
	return isCacheFileName(portLibrary, filename, J9PORT_SHR_CACHE_TYPE_NONPERSISTENT, NULL);
}

static int64_t
convertFileTimeToUnixEpoch(const FILETIME* time)
{
	/*This function is copied from j9file_lastmod*/
	int64_t tempResult, result;
	tempResult = ((int64_t) time->dwHighDateTime << (int64_t) 32) | (int64_t) time->dwLowDateTime;
	result = (tempResult - 116444736000000000) / 10000000;
	return result;
}
/**
 * @internal
 * Determines the proper portable error code to return given a native error code
 *
 * @param[in] errorCode The error code reported by the OS
 *
 * @return	the (negative) portable error code
 */
static int32_t
findError (int32_t errorCode, int32_t errorCode2)
{
	switch (errorCode)
	{
		default:
			return J9PORT_ERROR_SHMEM_OPFAILED;
	}
}

char*
getModifiedSharedMemoryPathandFileName(struct J9PortLibrary* portLibrary, const char* cacheDirName, const char* sharedMemoryFileName)
{
	char* modifiedSharedMemoryFileName;
	char* p;

	/* Get the shared cache (memory mapped file) path and name to use as the mapping object name  */
	/* The mapping object name needs to have certain characters changed to be acceptable, so loop    */
	/* through the string changing them to underscores                                                                             */
	modifiedSharedMemoryFileName = getSharedMemoryPathandFileName(portLibrary, cacheDirName, sharedMemoryFileName);
	if (NULL == modifiedSharedMemoryFileName) {
		return NULL;
	}
	for(p = modifiedSharedMemoryFileName; *p != '\0'; p++) {
		if (*p == '\\' || *p == ':' || *p == ' ') {
			*p = '_';
		}
	}

	return modifiedSharedMemoryFileName;
}

intptr_t
j9shmem_getDir(struct J9PortLibrary* portLibrary, const char* ctrlDirName, uint32_t flags, char* shmemdir, uintptr_t bufLength)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc;
	BOOLEAN appendBaseDir = J9_ARE_ALL_BITS_SET(flags, J9SHMEM_GETDIR_APPEND_BASEDIR);

	Trc_PRT_j9shmem_getDir_Entry();

	if (NULL != ctrlDirName) {
		if (appendBaseDir) {
			if (omrstr_printf(
					shmemdir,
					bufLength,
					SHM_STRING_ENDS_WITH_CHAR(ctrlDirName, DIR_SEPARATOR)
						? "%s%s" DIR_SEPARATOR_STR
						: "%s" DIR_SEPARATOR_STR "%s" DIR_SEPARATOR_STR,
					ctrlDirName,
					J9SH_BASEDIR) >= bufLength) {
				Trc_PRT_j9shmem_getDir_ExitFailedOverflow();
				return J9PORT_ERROR_SHMEM_GET_DIR_BUF_OVERFLOW;
			}
		} else {
			if (omrstr_printf(
					shmemdir,
					bufLength,
					SHM_STRING_ENDS_WITH_CHAR(ctrlDirName, DIR_SEPARATOR)
						? "%s"
						: "%s" DIR_SEPARATOR_STR,
					ctrlDirName) >= bufLength) {
				Trc_PRT_j9shmem_getDir_ExitFailedOverflow();
				return J9PORT_ERROR_SHMEM_GET_DIR_BUF_OVERFLOW;
			}
		}
	} else {
		char* appdatadir = NULL;

		appdatadir = omrmem_allocate_memory((J9SH_MAXPATH+1) * 2, OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == appdatadir) {
			Trc_PRT_j9shmem_getDir_ExitNoMemory();
			return J9PORT_ERROR_SHMEM_NOSPACE;
		}

		/* The sequence for which we look for the shared memory directory:
		 * %APPDATA%, %TEMP%, c:\\temp, c:\\
		 */
		rc = SHGetFolderPathW( NULL, CSIDL_LOCAL_APPDATA, NULL, 0, (wchar_t*)appdatadir);
		if(SUCCEEDED(rc)) {
			/* Convert Unicode to UTF8 */
			if (appendBaseDir) {
				if (omrstr_printf(
						shmemdir,
						bufLength,
						SHM_STRING_ENDS_WITH_CHAR((char*)appdatadir, DIR_SEPARATOR)
							? "%ls%s" DIR_SEPARATOR_STR
							: "%ls" DIR_SEPARATOR_STR "%s" DIR_SEPARATOR_STR,
						appdatadir,
						J9SH_BASEDIR) >= bufLength) {
					Trc_PRT_j9shmem_getDir_ExitFailedOverflow();
					return J9PORT_ERROR_SHMEM_GET_DIR_BUF_OVERFLOW;
				}
			} else {
				if (omrstr_printf(
						shmemdir,
						bufLength,
						SHM_STRING_ENDS_WITH_CHAR((char*)appdatadir, DIR_SEPARATOR)
							? "%ls"
							: "%ls" DIR_SEPARATOR_STR,
						appdatadir) >= bufLength) {
					Trc_PRT_j9shmem_getDir_ExitFailedOverflow();
					return J9PORT_ERROR_SHMEM_GET_DIR_BUF_OVERFLOW;
				}
			}
		} else {
			/*	The preference of share class cache file storage location is from 
				ENV_LOCALAPPDATA, ENV_APPDATA, ENV_TEMP, DIR_TEMP, DIR_CROOT */				
			if (-1 == omrsysinfo_get_env(ENV_LOCALAPPDATA, appdatadir, J9SH_MAXPATH)) {
				if (-1 == omrsysinfo_get_env(ENV_APPDATA, appdatadir, J9SH_MAXPATH)) {
					if (-1 == omrsysinfo_get_env(ENV_TEMP, appdatadir, J9SH_MAXPATH)) {
						if (omrfile_attr(DIR_TEMP) == EsIsDir) {
							omrstr_printf(appdatadir, J9SH_MAXPATH, DIR_TEMP);
						} else {
							omrstr_printf(appdatadir, J9SH_MAXPATH, DIR_CROOT);
						}
					}
				}
			}
			if (appendBaseDir) {
				if (omrstr_printf(
						shmemdir,
						bufLength,
						SHM_STRING_ENDS_WITH_CHAR(appdatadir, DIR_SEPARATOR)
							? "%s%s" DIR_SEPARATOR_STR
							: "%s" DIR_SEPARATOR_STR "%s" DIR_SEPARATOR_STR,
						appdatadir,
						J9SH_BASEDIR) >= bufLength) {
					Trc_PRT_j9shmem_getDir_ExitFailedOverflow();
					return J9PORT_ERROR_SHMEM_GET_DIR_BUF_OVERFLOW;
				}
			} else {
				if (omrstr_printf(
						shmemdir,
						bufLength,
						SHM_STRING_ENDS_WITH_CHAR(appdatadir, DIR_SEPARATOR)
							? "%s"
							: "%s" DIR_SEPARATOR_STR,
						appdatadir) >= bufLength) {
					Trc_PRT_j9shmem_getDir_ExitFailedOverflow();
					return J9PORT_ERROR_SHMEM_GET_DIR_BUF_OVERFLOW;
				}
			}
		}
		omrmem_free_memory(appdatadir);
	}

	Trc_PRT_j9shmem_getDir_Exit(shmemdir);
	return 0;
}

intptr_t
j9shmem_createDir(struct J9PortLibrary *portLibrary, char *cacheDirName, uintptr_t cacheDirPerm, BOOLEAN cleanMemorySegments)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t rc = 0;
	char pathCopy[J9SH_MAXPATH];

	Trc_PRT_j9shmem_createDir_Entry();

	rc = omrfile_attr(cacheDirName);
	switch (rc) {
	case EsIsFile:
		Trc_PRT_j9shmem_createDir_ExitFailedFile();
		break;
	case EsIsDir:
		Trc_PRT_j9shmem_createDir_Exit();
		return 0;
	default:
		strncpy(pathCopy, cacheDirName, J9SH_MAXPATH);
		if (0 == createDirectory(portLibrary, pathCopy)) {
			Trc_PRT_j9shmem_createDir_Exit();
			return 0;
		}
	}
	Trc_PRT_j9shmem_createDir_ExitFailed();
	return -1;
}

intptr_t
j9shmem_getFilepath(struct J9PortLibrary *portLibrary, char *cacheDirName, char *buffer, uintptr_t length, const char *cacheName)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	char *filePath = NULL;

	if (NULL == cacheDirName) {
		Trc_PRT_shmem_j9shmem_getFilepath_ExitNullCacheDirName();
		return -1;
	}

	filePath = getSharedMemoryPathandFileName(portLibrary, cacheDirName, cacheName);
	if (NULL == filePath) {
		return -1;
	}

	omrstr_printf(buffer, length, "%s", filePath);
	omrmem_free_memory(filePath);
	return 0;
}

intptr_t
j9shmem_protect(struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, void *address, uintptr_t length, uintptr_t flags)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	return omrmmap_protect(address, length, flags);
}

uintptr_t
j9shmem_get_region_granularity(struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, void *address)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	return omrmmap_get_region_granularity(address);
}

int32_t
j9shmem_getid (struct J9PortLibrary *portLibrary, struct j9shmem_handle* handle)
{
	return 0;
}
