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
 * @brief Shared Memory
 */


#include "j9port.h"
#include "ut_j9prt.h"

#include <sys/types.h>
#include <sys/stat.h>
#if !defined(J9ZTPF)
#include <sys/ipc.h>
#include <sys/shm.h>
#else /* !defined(J9ZTPF) */
#include <tpf/i_shm.h>
#endif /* !defined(J9ZTPF) */
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>

#include "portnls.h"
#include "portpriv.h"
#include "j9sharedhelper.h"
#include "j9shmem.h"
#include "omrutil.h"
#include "protect_helpers.h"
#include "j9SysvIPCWrappers.h"
#include "shchelp.h"

#define CREATE_ERROR -10
#define OPEN_ERROR  -11
#define WRITE_ERROR -12
#define READ_ERROR -13
#define MALLOC_ERROR -14

#define J9SH_NO_DATA -21
#define J9SH_BAD_DATA -22

#define RETRY_COUNT 10

#define SHMEM_FTOK_PROJID 0x61
#define J9SH_DEFAULT_CTRL_ROOT "/tmp"

/*
 * The following set of defines construct the flags required for shmget
 * for the various sysvipc shared memory platforms.
 * The settings are determined by:
 *    o  whether we are running on zOS
 *    o  if on zOS whether APAR OA11519 is installed 
 *    o  if zOS whether we are building a 31 or 64 bit JVM
 *  There are default values for all other platforms.
 * 
 * Note in particular the inclusion of __IPC_MEGA on zOS systems.  If we 
 * are building a 31 bit JVM this is required.  However, it must NOT be 
 * included for the 64 bit zOS JVM.  This is because __IPC_MEGA allocates 
 * the shared memory below the 2GB line.  This is OK for processes running
 * in key 8 (such as Websphere server regions), but key 2 processes, such as
 * the Websphere control region, can only share memory ABOVE the 2GB line.
 * Therefore __IPC_MEGA must not be specified for the zOS 64 bit JVM.
 */


#define SHMFLAGS_BASE_COMMON (IPC_CREAT | IPC_EXCL)

#if defined(J9ZOS390)
#if (__EDC_TARGET >= __EDC_LE4106) /* For zOS system with APAR OA11519 */
#if defined(J9ZOS39064) /* For 64 bit systems must not specify __IPC_MEGA */
#define SHMFLAGS_BASE (SHMFLAGS_BASE_COMMON | __IPC_SHAREAS)
#else  /* defined(J9ZOS39064) */
#define SHMFLAGS_BASE (SHMFLAGS_BASE_COMMON | __IPC_SHAREAS | __IPC_MEGA)
#endif /* defined(J9ZOS39064) */
#else  /* (__EDC_TARGET >= __EDC_LE4106) */
#if defined(J9ZOS39064) /* For 64 bit systems must not specify __IPC_MEGA */
#define SHMFLAGS_BASE SHMFLAGS_BASE_COMMON
#else  /* defined(J9ZOS39064) */
#define SHMFLAGS_BASE (SHMFLAGS_BASE_COMMON | __IPC_MEGA)
#endif /* defined(J9ZOS39064) */
#endif /* (__EDC_TARGET >= __EDC_LE4106) */
#else  /* defined(J9ZOS390) */
#define SHMFLAGS_BASE SHMFLAGS_BASE_COMMON
#endif /* defined(J9ZOS390) */

#if defined(J9ZOS390)
#if (__EDC_TARGET >= __EDC_LE4106) /* For zOS system with APAR OA11519 */
#if defined(J9ZOS39064) /* For 64 bit systems must not specify __IPC_MEGA */
#define SHMFLAGS (IPC_CREAT | IPC_EXCL | __IPC_SHAREAS | S_IRUSR | S_IWUSR)
#define SHMFLAGS_NOSHAREAS (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)
#define SHMFLAGS_GROUP (IPC_CREAT | IPC_EXCL | __IPC_SHAREAS | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#define SHMFLAGS_GROUP_NOSHAREAS (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#else
#define SHMFLAGS (IPC_CREAT | IPC_EXCL | __IPC_MEGA | __IPC_SHAREAS | S_IRUSR | S_IWUSR)
#define SHMFLAGS_NOSHAREAS (IPC_CREAT | IPC_EXCL | __IPC_MEGA | S_IRUSR | S_IWUSR)
#define SHMFLAGS_GROUP (IPC_CREAT | IPC_EXCL | __IPC_MEGA | __IPC_SHAREAS | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#define SHMFLAGS_GROUP_NOSHAREAS (IPC_CREAT | IPC_EXCL | __IPC_MEGA | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#endif
#else /* For zOS systems without APAR OA11519 */
#if defined(J9ZOS39064) /* For 64 bit systems must not specify __IPC_MEGA */
#define SHMFLAGS (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)
#define SHMFLAGS_GROUP (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#else
#define SHMFLAGS (IPC_CREAT | IPC_EXCL | __IPC_MEGA | S_IRUSR | S_IWUSR)
#define SHMFLAGS_GROUP (IPC_CREAT | IPC_EXCL | __IPC_MEGA | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#endif
#endif
#elif defined(J9ZTPF)
#define SHMFLAGS (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | TPF_IPC64)
#define SHMFLAGS_GROUP (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | TPF_IPC64 )
#else /* For non-zOS systems */
#define SHMFLAGS (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)
#define SHMFLAGS_GROUP (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#endif

#define SHMFLAGS_READONLY (S_IRUSR | S_IRGRP | S_IROTH)
#define SHMFLAGS_USERRW (SHMFLAGS_READONLY | S_IWUSR)
#define SHMFLAGS_GROUPRW (SHMFLAGS_USERRW | S_IWGRP)

#if !defined(J9ZOS390)
#define __errno2() 0
#endif

#define SHM_STRING_ENDS_WITH_CHAR(str, ch) ((ch) == *((str) + strlen(str) - 1))

static int32_t createSharedMemory(J9PortLibrary *portLibrary, intptr_t fd, BOOLEAN isReadOnlyFD, const char *baseFile, int32_t size, int32_t perm, struct j9shmem_controlFileFormat * controlinfo, uintptr_t groupPerm, struct j9shmem_handle *handle);
static intptr_t checkSize(J9PortLibrary *portLibrary, int shmid, int64_t size);
static intptr_t openSharedMemory (J9PortLibrary *portLibrary, intptr_t fd, const char *baseFile, int32_t perm, struct j9shmem_controlFileFormat * controlinfo, uintptr_t groupPerm, struct j9shmem_handle *handle, BOOLEAN *canUnlink, uintptr_t cacheFileType);
static void getControlFilePath(struct J9PortLibrary* portLibrary, const char* cacheDirName, char* buffer, uintptr_t size, const char* name);
static void createshmHandle(J9PortLibrary *portLibrary, int32_t shmid, const char *controlFile, OMRMemCategory * category, uintptr_t size, j9shmem_handle * handle, uint32_t perm);
static intptr_t checkGid(J9PortLibrary *portLibrary, int shmid, int32_t gid);
static intptr_t checkUid(J9PortLibrary *portLibrary, int shmid, int32_t uid);
static uintptr_t isControlFileName(struct J9PortLibrary* portLibrary, char* buffer);
static intptr_t readControlFile(J9PortLibrary *portLibrary, intptr_t fd, j9shmem_controlFileFormat * info);
static intptr_t readOlderNonZeroByteControlFile(J9PortLibrary *portLibrary, intptr_t fd, j9shmem_controlBaseFileFormat * info);
static void initShmemStatsBuffer(J9PortLibrary *portLibrary, J9PortShmemStatistic *statbuf);
static intptr_t getShmStats(J9PortLibrary *portLibrary, int shmid, struct J9PortShmemStatistic* statbuf);

/* In j9shchelp.c */
extern uintptr_t isCacheFileName(J9PortLibrary *, const char *, uintptr_t, const char *);

intptr_t
j9shmem_openDeprecated (struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct j9shmem_handle **handle, const char* rootname, uint32_t perm, uintptr_t cacheFileType, uint32_t categoryCode)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t retval = J9PORT_ERROR_SHMEM_OPFAILED;
	intptr_t handleStructLength = sizeof(struct j9shmem_handle);
	char baseFile[J9SH_MAXPATH];
	struct j9shmem_handle * tmphandle = NULL;
	intptr_t baseFileLength = 0;
	intptr_t fd = -1;
	BOOLEAN fileIsLocked = FALSE;
	BOOLEAN isReadOnlyFD = FALSE;
	OMRMemCategory * category = omrmem_get_category(categoryCode);
	struct shmid_ds shminfo;

	Trc_PRT_shmem_j9shmem_openDeprecated_Entry();

	clearPortableError(portLibrary);

	if (cacheDirName == NULL) {
		Trc_PRT_shmem_j9shmem_openDeprecated_ExitNullCacheDirName();
		return J9PORT_ERROR_SHMEM_OPFAILED;
	}

	getControlFilePath(portLibrary, cacheDirName, baseFile, J9SH_MAXPATH, rootname);

	baseFileLength = strlen(baseFile) + 1;
	tmphandle = omrmem_allocate_memory((handleStructLength + baseFileLength), categoryCode);
	if (NULL == tmphandle) {
		Trc_PRT_shmem_j9shmem_openDeprecated_Message("Error: could not alloc handle.");
		goto error;
	}
	tmphandle->baseFileName = (char *) (((char *) tmphandle) + handleStructLength);
	tmphandle->controlStorageProtectKey = 0;
	tmphandle->currentStorageProtectKey = 0;

	if (ControlFileOpenWithWriteLock(portLibrary, &fd, &isReadOnlyFD, FALSE, baseFile, 0) != J9SH_SUCCESS) {
		Trc_PRT_shmem_j9shmem_openDeprecated_Message("Error: could not lock shared memory control file.");
		goto error;
	} else {
		fileIsLocked = TRUE;
	}
	if (cacheFileType == J9SH_SYSV_OLDER_EMPTY_CONTROL_FILE) {
		int shmid = -1;
		int32_t shmflags = groupPerm ? SHMFLAGS_GROUP : SHMFLAGS;
		int projid = 0xde;
		key_t fkey;
		j9shmem_controlFileFormat controlinfo;
		uintptr_t size = 0;

		fkey = ftokWrapper(portLibrary, baseFile, projid);
		if (fkey == -1) {
			Trc_PRT_shmem_j9shmem_openDeprecated_Message("Error: ftok failed.");
			goto error;
		}
		shmflags &= ~IPC_CREAT;

		/*size of zero means use current size of existing object*/
		shmid = shmgetWrapper(portLibrary, fkey, 0, shmflags);

#if defined (J9ZOS390)
		if(shmid == -1) {
			/* zOS doesn't ignore shmget flags that is doesn't understand. Doh. So for share classes to work */
			/* on zOS system without APAR OA11519, we need to retry without __IPC_SHAREAS flags */
			shmflags = groupPerm ? SHMFLAGS_GROUP_NOSHAREAS : SHMFLAGS_NOSHAREAS;
			shmflags &= ~IPC_CREAT;
			shmid = shmgetWrapper(portLibrary, fkey, 0, shmflags);
		}
#endif
		if (-1 == shmid) {
			int32_t lasterrno = omrerror_last_error_number() | J9PORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
			const char * errormsg = omrerror_last_error_message();
			Trc_PRT_shmem_j9shmem_openDeprecated_shmgetFailed(fkey,lasterrno,errormsg);
			if (lasterrno == J9PORT_ERROR_SYSV_IPC_ERRNO_EINVAL) {
				Trc_PRT_shmem_j9shmem_openDeprecated_shmgetEINVAL(fkey);
				retval = J9PORT_INFO_SHMEM_PARTIAL;
				/*Clean up the control file since its SysV object is deleted.*/
				omrfile_unlink(baseFile);
			}
			goto error;
		}

		if (shmctlWrapper(portLibrary, TRUE, shmid, IPC_STAT, &shminfo) == -1) {
			Trc_PRT_shmem_j9shmem_openDeprecated_Message("Error: shmctl failed.");
			goto error;
		}
		size = shminfo.shm_segsz;

		createshmHandle(portLibrary, shmid, baseFile, category, size, tmphandle, perm);
		tmphandle->timestamp = omrfile_lastmod(baseFile);
		retval = J9PORT_INFO_SHMEM_OPENED;
	} else if (cacheFileType == J9SH_SYSV_OLDER_CONTROL_FILE) {
		BOOLEAN canUnlink = FALSE;
		j9shmem_controlBaseFileFormat controlinfo;
		uintptr_t size = 0;
		struct shmid_ds shminfo;

		if (readOlderNonZeroByteControlFile(portLibrary, fd, &controlinfo) != J9SH_SUCCESS) {
			Trc_PRT_shmem_j9shmem_openDeprecated_Message("Error: could not read deprecated control file.");
			goto error;
		}

		retval = openSharedMemory(portLibrary, fd, baseFile, perm, (j9shmem_controlFileFormat *)&controlinfo, groupPerm, tmphandle, &canUnlink, cacheFileType);
		if (J9PORT_INFO_SHMEM_OPENED != retval) {
			if (FALSE == canUnlink) {
				Trc_PRT_shmem_j9shmem_openDeprecated_Message("Control file was not unlinked.");
				retval = J9PORT_ERROR_SHMEM_OPFAILED;
			} else {
				/*Clean up the control file */
				if (-1 == omrfile_unlink(baseFile)) {
					Trc_PRT_shmem_j9shmem_openDeprecated_Message("Control file could not be unlinked.");
				} else {
					Trc_PRT_shmem_j9shmem_openDeprecated_Message("Control file was unlinked.");
				}
				retval = J9PORT_INFO_SHMEM_PARTIAL;
			}
			goto error;
		}

		size = shminfo.shm_segsz;

		createshmHandle(portLibrary, controlinfo.shmid, baseFile, category, size, tmphandle, perm);
		tmphandle->timestamp = omrfile_lastmod(baseFile);
		retval = J9PORT_INFO_SHMEM_OPENED;
	} else {
		Trc_PRT_shmem_j9shmem_openDeprecated_BadCacheType(cacheFileType);
	}
	
error:
	if (fileIsLocked == TRUE) {
		if (ControlFileCloseAndUnLock(portLibrary, fd) != J9SH_SUCCESS) {
			Trc_PRT_shmem_j9shmem_openDeprecated_Message("Error: could not unlock shared memory control file.");
			retval = J9PORT_ERROR_SHMEM_OPFAILED;
		}
	}
	if (retval != J9PORT_INFO_SHMEM_OPENED){
		if (NULL != tmphandle) {
			omrmem_free_memory(tmphandle);
		}
		*handle = NULL;
		Trc_PRT_shmem_j9shmem_openDeprecated_Exit("Exit: failed to open older shared memory");
	} else {
		*handle = tmphandle;
		Trc_PRT_shmem_j9shmem_openDeprecated_Exit("Open old generation of shared memory successfully.");
	}
	return retval;
}

intptr_t
j9shmem_open (J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct j9shmem_handle **handle, const char *rootname, uintptr_t size, uint32_t perm, uint32_t categoryCode, uintptr_t flags, J9ControlFileStatus *controlFileStatus)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	/* TODO: needs to be longer? dynamic?*/
	char baseFile[J9SH_MAXPATH];
	intptr_t fd;
	struct j9shmem_controlFileFormat controlinfo;
	int32_t rc;
	BOOLEAN isReadOnlyFD;
	intptr_t baseFileLength = 0;
	intptr_t handleStructLength = sizeof(struct j9shmem_handle);
	char * exitmsg = 0;
	intptr_t retryIfReadOnlyCount = 10;
	struct j9shmem_handle * tmphandle = NULL;
	OMRMemCategory * category = omrmem_get_category(categoryCode);
	BOOLEAN freeHandleOnError = TRUE;

	Trc_PRT_shmem_j9shmem_open_Entry(rootname, size, perm);

	clearPortableError(portLibrary);
	
	if (NULL != controlFileStatus) {
		memset(controlFileStatus, 0, sizeof(J9ControlFileStatus));
	}

	if (cacheDirName == NULL) {
		Trc_PRT_shmem_j9shmem_open_ExitNullCacheDirName();
		return J9PORT_ERROR_SHMEM_OPFAILED;
	}

	getControlFilePath(portLibrary, cacheDirName, baseFile, J9SH_MAXPATH, rootname);

	if (handle == NULL) {
		Trc_PRT_shmem_j9shmem_open_ExitWithMsg("Error: handle is NULL.");
		return J9PORT_ERROR_SHMEM_OPFAILED;
	}
	baseFileLength = strlen(baseFile) + 1;
	*handle = NULL;
	tmphandle = omrmem_allocate_memory((handleStructLength + baseFileLength), categoryCode);
	if (NULL == tmphandle) {
		Trc_PRT_shmem_j9shmem_open_ExitWithMsg("Error: could not alloc handle.");
		return J9PORT_ERROR_SHMEM_OPFAILED;
	}
	tmphandle->baseFileName = (char *) (((char *) tmphandle) + handleStructLength);
	tmphandle->controlStorageProtectKey = 0;
	tmphandle->currentStorageProtectKey = 0;
	tmphandle->flags = flags;
	
#if defined(J9ZOS390)
	if ((flags & J9SHMEM_STORAGE_KEY_TESTING) != 0) {
		tmphandle->currentStorageProtectKey = (flags >> J9SHMEM_STORAGE_KEY_TESTING_SHIFT) & J9SHMEM_STORAGE_KEY_TESTING_MASK;			
	} else {
		tmphandle->currentStorageProtectKey = getStorageKey();
	}	
#endif

	for (retryIfReadOnlyCount = 10; retryIfReadOnlyCount > 0; retryIfReadOnlyCount -= 1) {
		/*Open control file with write lock*/
		BOOLEAN canCreateFile = TRUE;
		if ((perm == J9SH_SHMEM_PERM_READ) || J9_ARE_ANY_BITS_SET(flags, J9SHMEM_OPEN_FOR_STATS | J9SHMEM_OPEN_FOR_DESTROY | J9SHMEM_OPEN_DO_NOT_CREATE)) {
			canCreateFile = FALSE;
		}
		if (ControlFileOpenWithWriteLock(portLibrary, &fd, &isReadOnlyFD, canCreateFile, baseFile, groupPerm) != J9SH_SUCCESS) {
			Trc_PRT_shmem_j9shmem_open_ExitWithMsg("Error: could not lock memory control file.");
			omrmem_free_memory(tmphandle);
			return J9PORT_ERROR_SHMEM_OPFAILED_CONTROL_FILE_LOCK_FAILED;
		}
		/*Try to read data from the file*/
		rc = readControlFile(portLibrary, fd, &controlinfo);
		if (rc == J9SH_ERROR) {
			/* The file is empty.*/
			if (J9_ARE_NO_BITS_SET(flags, J9SHMEM_OPEN_FOR_STATS | J9SHMEM_OPEN_FOR_DESTROY | J9SHMEM_OPEN_DO_NOT_CREATE)) {
				rc = createSharedMemory(portLibrary, fd, isReadOnlyFD, baseFile, size, perm, &controlinfo, groupPerm, tmphandle);
				if (rc == J9PORT_ERROR_SHMEM_OPFAILED) {
					exitmsg = "Error: create of the memory has failed.";
					if (perm == J9SH_SHMEM_PERM_READ) {
						Trc_PRT_shmem_j9shmem_open_Msg("Info: Control file was not unlinked (J9SH_SHMEM_PERM_READ).");
					} else if (isReadOnlyFD == FALSE) {
						if (unlinkControlFile(portLibrary, baseFile, controlFileStatus)) {
							Trc_PRT_shmem_j9shmem_open_Msg("Info: Control file is unlinked after call to createSharedMemory.");
						} else {
							Trc_PRT_shmem_j9shmem_open_Msg("Info: Failed to unlink control file after call to createSharedMemory.");
						}
					} else {
						Trc_PRT_shmem_j9shmem_open_Msg("Info: Control file was not unlinked (isReadOnlyFD == TRUE).");
					}
				}
			} else {
				Trc_PRT_shmem_j9shmem_open_Msg("Info: Control file cannot be read. Do not attempt to create shared memory as J9SHMEM_OPEN_FOR_STATS, J9SHMEM_OPEN_FOR_DESTROY or J9SHMEM_OPEN_DO_NOT_CREATE is set");
				rc = J9PORT_ERROR_SHMEM_OPFAILED;
			}
		} else if (rc == J9SH_SUCCESS) {
			BOOLEAN canUnlink = FALSE;
			
			/*The control file contains data, and we have successfully read in our structure*/
			rc = openSharedMemory(portLibrary, fd, baseFile, perm, &controlinfo, groupPerm, tmphandle, &canUnlink, J9SH_SYSV_REGULAR_CONTROL_FILE);
			if (J9PORT_INFO_SHMEM_OPENED != rc) {
				exitmsg = "Error: open of the memory has failed.";

				if ((TRUE == canUnlink)
					&& (J9PORT_ERROR_SHMEM_ZOS_STORAGE_KEY_READONLY != rc)
					&& J9_ARE_NO_BITS_SET(flags, J9SHMEM_OPEN_FOR_STATS | J9SHMEM_OPEN_DO_NOT_CREATE)
				) {
					if (perm == J9SH_SHMEM_PERM_READ) {
						Trc_PRT_shmem_j9shmem_open_Msg("Info: Control file was not unlinked (J9SH_SHMEM_PERM_READ is set).");
					} else {
						/* PR 74306: If the control file is read-only, do not unlink it unless the control file was created by an older JVM without this fix,
						 * which is identified by the check for major and minor level in the control file.
						 * Reason being if the control file is read-only, JVM acquires a shared lock in ControlFileOpenWithWriteLock().
						 * This implies in multiple JVM scenario it is possible that when one JVM has unlinked and created a new control file,
						 * another JVM unlinks the newly created control file, and creates another control file.
						 * For shared class cache, this may result into situations where the two JVMs will have their own shared memory regions
						 * but would be using same semaphore set for synchronization, thereby impacting each other's performance.
						 */
						if ((FALSE == isReadOnlyFD)
							|| ((J9SH_GET_MOD_MAJOR_LEVEL(controlinfo.common.modlevel) == J9SH_MOD_MAJOR_LEVEL_ALLOW_READONLY_UNLINK)
							&& (J9SH_SHM_GET_MOD_MINOR_LEVEL(controlinfo.common.modlevel) <= J9SH_MOD_MINOR_LEVEL_ALLOW_READONLY_UNLINK)
							&& (J9PORT_ERROR_SHMEM_OPFAILED_SHARED_MEMORY_NOT_FOUND == rc))
						) {
							if (unlinkControlFile(portLibrary, baseFile, controlFileStatus)) {
								Trc_PRT_shmem_j9shmem_open_Msg("Info: Control file was unlinked after call to openSharedMemory.");
								if (J9_ARE_ALL_BITS_SET(flags, J9SHMEM_OPEN_FOR_DESTROY)) {
									if (J9PORT_ERROR_SHMEM_OPFAILED_SHARED_MEMORY_NOT_FOUND != rc) {
										tmphandle->shmid = controlinfo.common.shmid;
										freeHandleOnError = FALSE;
									}
								} else if (retryIfReadOnlyCount > 1) {
									rc = J9PORT_INFO_SHMEM_OPEN_UNLINKED;
								}
							} else {
								Trc_PRT_shmem_j9shmem_open_Msg("Info: Failed to unlink control file after call to openSharedMemory.");
							}
						} else {
							Trc_PRT_shmem_j9shmem_open_Msg("Info: Control file was not unlinked after openSharedMemory because it is read-only and (its modLevel does not allow unlinking or rc != SHARED_MEMORY_NOT_FOUND).");
						}
					}
				} else {
					Trc_PRT_shmem_j9shmem_open_Msg("Info: Control file was not unlinked after call to openSharedMemory.");
				}
			}
		} else {
			/*Should never get here ... this means the control file was corrupted*/
			exitmsg = "Error: Memory control file is corrupted.";
			rc = J9PORT_ERROR_SHMEM_OPFAILED_CONTROL_FILE_CORRUPT;

			if (J9_ARE_NO_BITS_SET(flags, J9SHMEM_OPEN_FOR_STATS | J9SHMEM_OPEN_DO_NOT_CREATE)) {
				if (perm == J9SH_SHMEM_PERM_READ) {
					Trc_PRT_shmem_j9shmem_open_Msg("Info: Corrupt control file was not unlinked (J9SH_SHMEM_PERM_READ is set).");
				} else {
					if (isReadOnlyFD == FALSE) {
						if (unlinkControlFile(portLibrary, baseFile, controlFileStatus)) {
							Trc_PRT_shmem_j9shmem_open_Msg("Info: The corrupt control file was unlinked.");
							if (retryIfReadOnlyCount > 1) {
								rc = J9PORT_INFO_SHMEM_OPEN_UNLINKED;
							}
						} else {
							Trc_PRT_shmem_j9shmem_open_Msg("Info: Failed to remove the corrupt control file.");
						}
					} else {
						Trc_PRT_shmem_j9shmem_open_Msg("Info: Control file was not unlinked(isReadOnlyFD == TRUE).");
					}
				}
			} else {
				Trc_PRT_shmem_j9shmem_open_Msg("Info: Control file was found to be corrupt but was not unlinked (J9SHMEM_OPEN_FOR_STATS or J9SHMEM_OPEN_DO_NOT_CREATE is set).");
			}
		}
		if ((J9PORT_INFO_SHMEM_CREATED != rc) && (J9PORT_INFO_SHMEM_OPENED != rc)) {
			if (ControlFileCloseAndUnLock(portLibrary, fd) != J9SH_SUCCESS) {
				Trc_PRT_shmem_j9shmem_open_ExitWithMsg("Error: could not unlock memory control file.");
				omrmem_free_memory(tmphandle);
				return J9PORT_ERROR_SHMEM_OPFAILED;
			}
			if (J9_ARE_ANY_BITS_SET(flags, J9SHMEM_OPEN_FOR_STATS | J9SHMEM_OPEN_FOR_DESTROY | J9SHMEM_OPEN_DO_NOT_CREATE)
				|| (J9PORT_ERROR_SHMEM_ZOS_STORAGE_KEY_READONLY == rc)
			) {
				Trc_PRT_shmem_j9shmem_open_ExitWithMsg(exitmsg);
				/* If we get an error while opening shared memory and thereafter unlinked control file successfully,
				 * then we may have stored the shmid in tmphandle to be used by caller for error reporting.
				 * Do not free tmphandle in such case. Caller is responsible for freeing it.
				 */
				if (TRUE == freeHandleOnError) {
					omrmem_free_memory(tmphandle);
				}
				return rc;
			}
			if (((isReadOnlyFD == TRUE) || (rc == J9PORT_INFO_SHMEM_OPEN_UNLINKED)) && (retryIfReadOnlyCount > 1)) {
				/*Try loop again ... we sleep first to mimic old retry loop*/
				if (rc != J9PORT_INFO_SHMEM_OPEN_UNLINKED) {
					Trc_PRT_shmem_j9shmem_open_Msg("Retry with usleep(100).");
					usleep(100);
				} else {
					Trc_PRT_shmem_j9shmem_open_Msg("Retry.");
				}
				continue;
			} else {
				if (retryIfReadOnlyCount <= 1) {
					Trc_PRT_shmem_j9shmem_open_Msg("Info no more retries.");
				}
				Trc_PRT_shmem_j9shmem_open_ExitWithMsg(exitmsg);
				omrmem_free_memory(tmphandle);
				/* Here 'rc' can be:
				 * 	- J9PORT_ERROR_SHMEM_OPFAILED (returned either by createSharedMemory or openSharedMemory)
				 * 	- J9PORT_ERROR_SHMEM_OPFAILED_CONTROL_FILE_CORRUPT
				 * 	- J9PORT_ERROR_SHMEM_OPFAILED_SHMID_MISMATCH (returned by openSharedMemory)
				 * 	- J9PORT_ERROR_SHMEM_OPFAILED_SHM_KEY_MISMATCH (returned by openSharedMemory)
				 * 	- J9PORT_ERROR_SHMEM_OPFAILED_SHM_GROUPID_CHECK_FAILED (returned by openSharedMemory)
				 * 	- J9PORT_ERROR_SHMEM_OPFAILED_SHM_USERID_CHECK_FAILED (returned by openSharedMemory)
				 * 	- J9PORT_ERROR_SHMEM_OPFAILED_SHM_SIZE_CHECK_FAILED (returned by openSharedMemory)
				 * 	- J9PORT_ERROR_SHMEM_OPFAILED_SHARED_MEMORY_NOT_FOUND (returned by openSharedMemory)
				 */
				return rc;
			}
		}
		/*If we hit here we break out of the loop*/
		break;
	}
	createshmHandle(portLibrary, controlinfo.common.shmid, baseFile, category, size, tmphandle, perm);
	if (rc == J9PORT_INFO_SHMEM_CREATED) {
		tmphandle->timestamp = omrfile_lastmod(baseFile);
	}
	if (j9shmem_attach(portLibrary, tmphandle, categoryCode) == NULL) {
		/* The call must clean up the handle in this case by calling j9shxxx_destroy */
		if (ControlFileCloseAndUnLock(portLibrary, fd) != J9SH_SUCCESS) {
			Trc_PRT_shmem_j9shmem_open_Msg("Error: could not unlock memory control file.");
		}
		if (rc == J9PORT_INFO_SHMEM_CREATED) {
			rc = J9PORT_ERROR_SHMEM_CREATE_ATTACHED_FAILED;
			Trc_PRT_shmem_j9shmem_open_ExitWithMsg("Error: Created shared memory, but could not attach.");
		} else {
			rc = J9PORT_ERROR_SHMEM_OPEN_ATTACHED_FAILED;
			Trc_PRT_shmem_j9shmem_open_ExitWithMsg("Error: Opened shared memory, but could not attach.");
		}
		*handle = tmphandle;
		return rc;
	} else {
		Trc_PRT_shmem_j9shmem_open_Msg("Attached to shared memory");
	}
	if (ControlFileCloseAndUnLock(portLibrary, fd) != J9SH_SUCCESS) {
		Trc_PRT_shmem_j9shmem_open_ExitWithMsg("Error: could not unlock memory control file.");
		omrmem_free_memory(tmphandle);
		return J9PORT_ERROR_SHMEM_OPFAILED;
	}
	if (rc == J9PORT_INFO_SHMEM_CREATED) {
		Trc_PRT_shmem_j9shmem_open_ExitWithMsg("Successfully created a new memory.");
	} else {
		Trc_PRT_shmem_j9shmem_open_ExitWithMsg("Successfully opened an existing memory.");
	}
	*handle = tmphandle;
	return rc;
}

void 
j9shmem_close(struct J9PortLibrary *portLibrary, struct j9shmem_handle **handle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	Trc_PRT_shmem_j9shmem_close_Entry1(*handle, (*handle)->shmid);
	portLibrary->shmem_detach(portLibrary, handle);
	omrmem_free_memory(*handle);
	
	*handle=NULL;
	Trc_PRT_shmem_j9shmem_close_Exit();
}

void*
j9shmem_attach(struct J9PortLibrary *portLibrary, struct j9shmem_handle *handle, uint32_t categoryCode)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	void* region;
	int32_t shmatPerm = 0;
	OMRMemCategory * category = omrmem_get_category(categoryCode);
	Trc_PRT_shmem_j9shmem_attach_Entry1(handle, handle ? handle->shmid : -1);

	if (NULL == handle) {
		Trc_PRT_shmem_j9shmem_attach_Exit1();
		return NULL;
	}

	Trc_PRT_shmem_j9shmem_attach_Debug1(handle->shmid);

	if(NULL != handle->regionStart) {
		Trc_PRT_shmem_j9shmem_attach_Exit(handle->regionStart);
		return handle->regionStart;
	}
	if (handle->perm == J9SH_SHMEM_PERM_READ) {
		/* shmat perms should be 0 for read/write and SHM_RDONLY for read-only */
		shmatPerm = SHM_RDONLY;
	}

#if defined(J9ZOS390)
	if (((handle->flags & J9SHMEM_STORAGE_KEY_TESTING) != 0) &&
		(handle->currentStorageProtectKey != handle->controlStorageProtectKey) &&
		(!((8 == handle->currentStorageProtectKey) && (9 == handle->controlStorageProtectKey)))
	) {
		omrerror_set_last_error(EACCES, J9PORT_ERROR_SYSV_IPC_SHMAT_ERROR + J9PORT_ERROR_SYSV_IPC_ERRNO_EACCES);
		region = (void*)-1;
	} else {
#endif
		region = shmatWrapper(portLibrary, handle->shmid, 0, shmatPerm);
#if defined(J9ZOS390)
	}
	if ((void*)-1 == region) {
		if ((handle->flags & J9SHMEM_PRINT_STORAGE_KEY_WARNING) != 0) {
			if (handle->currentStorageProtectKey != handle->controlStorageProtectKey) {
				int32_t myerrno = omrerror_last_error_number();
				if ((J9PORT_ERROR_SYSV_IPC_SHMAT_ERROR + J9PORT_ERROR_SYSV_IPC_ERRNO_EACCES) == myerrno) {
					omrnls_printf(J9NLS_WARNING, J9NLS_PORT_ZOS_SHMEM_STORAGE_KEY_MISMATCH, handle->controlStorageProtectKey, handle->currentStorageProtectKey);
					/*
					 * omrnls_printf() can change the last error, this occurs when libj9ifa29.so is APF authorized (extattr +a).
					 * The last error may be set to -108 EDC5129I No such file or directory.
					 * This causes cmdLineTester_shareClassesStorageKey to fail with an unexpected error code. It also shows
					 * the incorrect last error as the reason the shared cache could not be opened, which is confusing.
					 * Reset the last error to what it was before omrnls_printf().
					 */
					omrerror_set_last_error(EACCES, J9PORT_ERROR_SYSV_IPC_SHMAT_ERROR + J9PORT_ERROR_SYSV_IPC_ERRNO_EACCES);
				}
			}
		}
	}
#endif

	/* Do not use shmctl to release the shared memory here, just in case of ^C or crash */
	if((void *)-1 == region) {
		int32_t myerrno = omrerror_last_error_number();
		Trc_PRT_shmem_j9shmem_attach_Exit2(myerrno);
		return NULL;
	} else {
		handle->regionStart = region;
		omrmem_categories_increment_counters(category, handle->size);
		Trc_PRT_shmem_j9shmem_attach_Exit(region);
		return region;
	} 
}

intptr_t 
j9shmem_destroy (struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct j9shmem_handle **handle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t rc;
	intptr_t lockFile;
	intptr_t fd;
	BOOLEAN isReadOnlyFD;
	j9shmem_controlBaseFileFormat controlinfo;

	Trc_PRT_shmem_j9shmem_destroy_Entry1(*handle, (*handle) ? (*handle)->shmid : -1);

	if (NULL == *handle) {
		Trc_PRT_shmem_j9shmem_destroy_Exit();
		return 0;
	}

	lockFile = ControlFileOpenWithWriteLock(portLibrary, &fd, &isReadOnlyFD, FALSE, (*handle)->baseFileName, 0);
	if (lockFile == J9SH_FILE_DOES_NOT_EXIST) {
		Trc_PRT_shmem_j9shmem_destroy_Msg("Error: control file not found");
		if (shmctlWrapper(portLibrary, TRUE, (*handle)->shmid, IPC_RMID, 0) == -1) {
			int32_t lastError = omrerror_last_error_number() | J9PORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
			if ((J9PORT_ERROR_SYSV_IPC_ERRNO_EINVAL == lastError) || (J9PORT_ERROR_SYSV_IPC_ERRNO_EIDRM == lastError)) {
				Trc_PRT_shmem_j9shmem_destroy_Msg("SysV obj is already deleted");
				rc = 0;
				goto j9shmem_destroy_exitWithSuccess;
			} else {
				Trc_PRT_shmem_j9shmem_destroy_Msg("Error: could not delete SysV obj");
				rc = -1;
				goto j9shmem_destroy_exitWithError;
			}
		} else {
			Trc_PRT_shmem_j9shmem_destroy_Msg("Deleted SysV obj");
			rc = 0;
			goto j9shmem_destroy_exitWithSuccess;
		}
	} else if (lockFile != J9SH_SUCCESS) {
		Trc_PRT_shmem_j9shmem_destroy_Msg("Error: could not open and lock control file.");
		goto j9shmem_destroy_exitWithError;
	}

	/*Try to read data from the file*/
	rc = omrfile_read(fd, &controlinfo, sizeof(j9shmem_controlBaseFileFormat));
	if (rc != sizeof(j9shmem_controlBaseFileFormat)) {
		Trc_PRT_shmem_j9shmem_destroy_Msg("Error: can not read control file");
		goto j9shmem_destroy_exitWithErrorAndUnlock;
	}

	/* check that the modlevel and version is okay */
	if (J9SH_GET_MOD_MAJOR_LEVEL(controlinfo.modlevel) != J9SH_MOD_MAJOR_LEVEL) {
		Trc_PRT_shmem_j9shmem_destroy_BadMajorModlevel(controlinfo.modlevel, J9SH_MODLEVEL);
		goto j9shmem_destroy_exitWithErrorAndUnlock;
	}

	if (controlinfo.shmid != (*handle)->shmid) {
		Trc_PRT_shmem_j9shmem_destroy_Msg("Error: mem id does not match contents of the control file");
		goto j9shmem_destroy_exitWithErrorAndUnlock;
	}

	portLibrary->shmem_detach(portLibrary, handle);

	if (shmctlWrapper(portLibrary, TRUE, (*handle)->shmid, IPC_RMID, NULL) == -1) {
		Trc_PRT_shmem_j9shmem_destroy_Msg("Error: shmctl(IPC_RMID) returned -1 ");
		goto j9shmem_destroy_exitWithErrorAndUnlock;
	}

	/* PR 74306: Allow unlinking of readonly control file if it was created by an older JVM without this fix
	 * which is identified by different modelevel.
	 */
	if ((FALSE == isReadOnlyFD)
		|| ((J9SH_GET_MOD_MAJOR_LEVEL(controlinfo.modlevel) == J9SH_MOD_MAJOR_LEVEL_ALLOW_READONLY_UNLINK)
		&& (J9SH_SHM_GET_MOD_MINOR_LEVEL(controlinfo.modlevel) <= J9SH_MOD_MINOR_LEVEL_ALLOW_READONLY_UNLINK))
	) {
		rc = omrfile_unlink((*handle)->baseFileName);
		Trc_PRT_shmem_j9shmem_destroy_Debug1((*handle)->baseFileName, rc, omrerror_last_error_number());
	}
	
	portLibrary->shmem_close(portLibrary, handle);

	if (ControlFileCloseAndUnLock(portLibrary, fd) != J9SH_SUCCESS) {
		Trc_PRT_shmem_j9shmem_destroy_Msg("Error: failed to unlock control file.");
		goto j9shmem_destroy_exitWithError;
	}

j9shmem_destroy_exitWithSuccess:
	Trc_PRT_shmem_j9shmem_destroy_Exit();
	return 0;

j9shmem_destroy_exitWithErrorAndUnlock:
	if (ControlFileCloseAndUnLock(portLibrary, fd) != J9SH_SUCCESS) {
		Trc_PRT_shmem_j9shmem_destroy_Msg("Error: failed to unlock control file");
	}
j9shmem_destroy_exitWithError:
	Trc_PRT_shmem_j9shmem_destroy_Exit1();
	return -1;

}

intptr_t 
j9shmem_destroyDeprecated (struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct j9shmem_handle **handle, uintptr_t cacheFileType)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t retval = J9SH_SUCCESS;
	intptr_t rcunlink;
	intptr_t fd;
	BOOLEAN isReadOnlyFD = FALSE;
	BOOLEAN fileIsLocked = FALSE;
	
	Trc_PRT_shmem_destroyDeprecated_Entry(*handle, (*handle)->shmid);
	
	if (cacheFileType == J9SH_SYSV_OLDER_EMPTY_CONTROL_FILE) {
		Trc_PRT_shmem_destroyDeprecated_Message("Info: cacheFileType == J9SH_SYSV_OLDER_EMPTY_CONTROL_FILE.");

		if (ControlFileOpenWithWriteLock(portLibrary, &fd, &isReadOnlyFD, FALSE, (*handle)->baseFileName, 0) != J9SH_SUCCESS) {
			Trc_PRT_shmem_destroyDeprecated_Message("Error: could not lock shared memory control file.");
			retval = J9SH_FAILED;
			goto error;
		} else {
			fileIsLocked = TRUE;
		}
		portLibrary->shmem_detach(portLibrary, handle);

		if (shmctlWrapper(portLibrary, TRUE, (*handle)->shmid, IPC_RMID, NULL) == -1) {
			Trc_PRT_shmem_destroyDeprecated_Message("Error: failed to remove SysV object.");
			retval = J9SH_FAILED;
			goto error;
		}

		if (0 == omrfile_unlink((*handle)->baseFileName)) {
			Trc_PRT_shmem_destroyDeprecated_Message("Unlinked control file");
		} else {
			Trc_PRT_shmem_destroyDeprecated_Message("Failed to unlink control file");
		}
		portLibrary->shmem_close(portLibrary, handle);
	error:
		if (fileIsLocked == TRUE) {
			if (ControlFileCloseAndUnLock(portLibrary, fd) != J9SH_SUCCESS) {
				Trc_PRT_shmem_destroyDeprecated_Message("Error: could not unlock shared memory control file.");
				retval = J9SH_FAILED;
			}
		}
	} else if (cacheFileType == J9SH_SYSV_OLDER_CONTROL_FILE) {
		Trc_PRT_shmem_destroyDeprecated_Message("Info: cacheFileType == J9SH_SYSV_OLDER_CONTROL_FILE.");
		retval = j9shmem_destroy(portLibrary, cacheDirName, groupPerm, handle);
	} else {
		Trc_PRT_j9shmem_destroyDeprecated_BadCacheType(cacheFileType);
		retval = J9SH_FAILED;
	}

	if (retval == J9SH_SUCCESS) {
		Trc_PRT_shmem_destroyDeprecated_ExitWithMessage("Exit successfully");
	} else {
		Trc_PRT_shmem_destroyDeprecated_ExitWithMessage("Exit with failure");
	}
	return retval;
}

intptr_t
j9shmem_detach(struct J9PortLibrary* portLibrary, struct j9shmem_handle **handle) 
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	Trc_PRT_shmem_j9shmem_detach_Entry1(*handle, (*handle)->shmid);
	if((*handle)->regionStart == NULL) {
		Trc_PRT_shmem_j9shmem_detach_Exit();
		return J9SH_SUCCESS;
	}

	if (-1 == shmdtWrapper(portLibrary, ((*handle)->regionStart))) {
		Trc_PRT_shmem_j9shmem_detach_Exit1();
		return J9SH_FAILED;
	}

	omrmem_categories_decrement_counters((*handle)->category, (*handle)->size);

	(*handle)->regionStart = NULL;
	Trc_PRT_shmem_j9shmem_detach_Exit();
	return J9SH_SUCCESS;
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
	char file[EsMaxPath];
	
	Trc_PRT_shmem_j9shmem_findfirst_Entry();
	
	if (cacheDirName == NULL) {
		Trc_PRT_shmem_j9shmem_findfirst_Exit3();
		return -1;
	}
	
	findHandle = omrfile_findfirst(cacheDirName, file);
	
	if(findHandle == -1) {
		Trc_PRT_shmem_j9shmem_findfirst_Exit1();
		return -1;
	}
	
	while (!isControlFileName(portLibrary, file)) {
		if (-1 == omrfile_findnext(findHandle, file)) {
			omrfile_findclose(findHandle);
			Trc_PRT_shmem_j9shmem_findfirst_Exit2();
			return -1;
		}		
	}
	
	strcpy(resultbuf, file);
	Trc_PRT_shmem_j9shmem_findfirst_file(resultbuf);
	Trc_PRT_shmem_j9shmem_findfirst_Exit();
	return findHandle;
}

int32_t
j9shmem_findnext(struct J9PortLibrary *portLibrary, uintptr_t findHandle, char *resultbuf)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	char file[EsMaxPath];

	Trc_PRT_shmem_j9shmem_findnext_Entry(findHandle);

	if (-1 == omrfile_findnext(findHandle, file)) {
		Trc_PRT_shmem_j9shmem_findnext_Exit1();
		return -1;
	}
	
	while (!isControlFileName(portLibrary, file)) {
		if (-1 == omrfile_findnext(findHandle, file)) {
			Trc_PRT_shmem_j9shmem_findnext_Exit2();
			return -1;
		}		
	}

	strcpy(resultbuf, file);
	Trc_PRT_shmem_j9shmem_findnext_file(resultbuf);
	Trc_PRT_shmem_j9shmem_findnext_Exit();
	return 0;
}

uintptr_t
j9shmem_stat(struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, const char* name, struct J9PortShmemStatistic* statbuf)
{
	int32_t rc;
	intptr_t fd;
	char controlFile[J9SH_MAXPATH];
	struct j9shmem_controlFileFormat controlinfo;
	BOOLEAN isReadOnlyFD;
	int shmflags = groupPerm ? SHMFLAGS_GROUP : SHMFLAGS;
	int shmid;
	char * exitMsg = "Exiting";
	intptr_t lockFile;

	shmflags &= ~IPC_CREAT;

	Trc_PRT_shmem_j9shmem_stat_Entry(name);
	if (cacheDirName == NULL) {
		Trc_PRT_shmem_j9shmem_stat_ExitNullCacheDirName();
		return -1;
	}
	if (statbuf == NULL) {
		Trc_PRT_shmem_j9shmem_stat_ExitNullStat();
		return -1;
	} else {
		initShmemStatsBuffer(portLibrary, statbuf);
	}

	getControlFilePath(portLibrary, cacheDirName, controlFile, J9SH_MAXPATH, name);

	/*Open control file with write lock*/
	lockFile = ControlFileOpenWithWriteLock(portLibrary, &fd, &isReadOnlyFD, FALSE, controlFile, 0);
	if (lockFile == J9SH_FILE_DOES_NOT_EXIST) {
		Trc_PRT_shmem_j9shmem_stat_Exit1(controlFile);
		return -1;
	} else if (lockFile != J9SH_SUCCESS) {
		exitMsg = "Error: can not open & lock control file";
		goto j9shmem_stat_exitWithError;
	}

	/*Try to read data from the file*/
	if (J9SH_SUCCESS != readControlFile(portLibrary, fd, &controlinfo)) {
		exitMsg = "Error: can not read control file";
		goto j9shmem_stat_exitWithErrorAndUnlock;
	}

	shmid = shmgetWrapper(portLibrary, controlinfo.common.ftok_key, controlinfo.size, shmflags);

#if defined (J9ZOS390)
	if(-1 == shmid) {
		/* zOS doesn't ignore shmget flags that is doesn't understand. So for share classes to work */
		/* on zOS system without APAR OA11519, we need to retry without __IPC_SHAREAS flags */
		shmflags = groupPerm ? SHMFLAGS_GROUP_NOSHAREAS : SHMFLAGS_NOSHAREAS;
		shmflags &= ~IPC_CREAT;
		shmid = shmgetWrapper(portLibrary, controlinfo.common.ftok_key, controlinfo.size, shmflags);
	}
#endif

	if (controlinfo.common.shmid != shmid) {
		exitMsg = "Error: mem id does not match contents of the control file";
		goto j9shmem_stat_exitWithErrorAndUnlock;
	}

	if (checkGid(portLibrary, controlinfo.common.shmid, controlinfo.gid) != 1) {
		/*If no match then this is someone elses sysv obj. The one our filed referred to was destroy and recreated by someone/thing else*/
		exitMsg = "Error: checkGid failed";
		goto j9shmem_stat_exitWithErrorAndUnlock;
	}

	if (checkUid(portLibrary, controlinfo.common.shmid, controlinfo.uid) != 1) {
		/*If no match then this is someone elses sysv obj. The one our filed referred to was destroy and recreated by someone/thing else*/
		exitMsg = "Error: checkUid failed";
		goto j9shmem_stat_exitWithErrorAndUnlock;
	}

	rc = checkSize(portLibrary, controlinfo.common.shmid, controlinfo.size);
	if (1 != rc) {
#if defined(J9ZOS390)
		if (-2 != rc)
#endif /* defined(J9ZOS390) */
		{
			/*If no match then this is someone elses sysv obj. The one our filed referred to was destroy and recreated by someone/thing else*/
			exitMsg = "Error: checkSize failed";
			goto j9shmem_stat_exitWithErrorAndUnlock;
		}
#if defined(J9ZOS390)
		else {
			/* JAZZ103 PR 79909 + PR 88930: shmctl(..., IPC_STAT, ...) returned 0 as the shared memory size.
			 * This can happen if shared memory segment is created by a 64-bit JVM and the caller is 31-bit JVM.
			 * Do not treat it as failure. Get the shared memory stats (which includes the size = 0) and let the caller handle
			 * this case as appropriate.
			 */
		}
#endif /* defined(J9ZOS390) */
	}

	statbuf->shmid = controlinfo.common.shmid;

	rc = getShmStats(portLibrary, statbuf->shmid, statbuf);
	if (J9PORT_INFO_SHMEM_STAT_PASSED != rc) {
		exitMsg = "Error: getShmStats failed";
		goto j9shmem_stat_exitWithErrorAndUnlock;
	}

	if (ControlFileCloseAndUnLock(portLibrary, fd) != J9SH_SUCCESS) {
		exitMsg = "Error: can not close & unlock control file (we were successful other than this)";
		goto j9shmem_stat_exitWithError;
	}

	Trc_PRT_shmem_j9shmem_stat_Exit();
	return 0;

j9shmem_stat_exitWithErrorAndUnlock:
	if (ControlFileCloseAndUnLock(portLibrary, fd) != J9SH_SUCCESS) {
		Trc_PRT_shmem_j9shmem_stat_Msg(exitMsg);
		exitMsg = "Error: can not close & unlock control file";
	}
j9shmem_stat_exitWithError:
	Trc_PRT_shmem_j9shmem_stat_ExitWithMsg(exitMsg);
	return -1;
}

uintptr_t
j9shmem_statDeprecated (struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, const char* name, struct J9PortShmemStatistic* statbuf, uintptr_t cacheFileType)
{
	intptr_t rc;
	uintptr_t retval = (uintptr_t)J9SH_SUCCESS;
	char controlFile[J9SH_MAXPATH];
	intptr_t fd = -1;
	BOOLEAN fileIsLocked = FALSE;
	BOOLEAN isReadOnlyFD = FALSE;

	Trc_PRT_shmem_j9shmem_statDeprecated_Entry();

	if (cacheDirName == NULL) {
		Trc_PRT_shmem_j9shmem_statDeprecated_ExitNullCacheDirName();
		return -1;
	}
	initShmemStatsBuffer(portLibrary, statbuf);

	getControlFilePath(portLibrary, cacheDirName, controlFile, J9SH_MAXPATH, name);
	if (ControlFileOpenWithWriteLock(portLibrary, &fd, &isReadOnlyFD, FALSE, controlFile, 0) != J9SH_SUCCESS) {
		Trc_PRT_shmem_j9shmem_statDeprecated_Message("Error: could not lock shared memory control file.");
		retval = J9SH_FAILED;
		goto error;
	} else {
		fileIsLocked = TRUE;
	}

	if (cacheFileType == J9SH_SYSV_OLDER_EMPTY_CONTROL_FILE) {
		int shmid = -1;
		int32_t shmflags = groupPerm ? SHMFLAGS_GROUP : SHMFLAGS;
		int projid = 0xde;
		key_t fkey;
		j9shmem_controlFileFormat info;

		Trc_PRT_shmem_j9shmem_statDeprecated_Message("Info: cacheFileType == J9SH_SYSV_OLDER_EMPTY_CONTROL_FILE.");

		fkey = ftokWrapper(portLibrary, controlFile, projid);
		if (fkey == -1) {
			Trc_PRT_shmem_j9shmem_statDeprecated_Message("Error: ftokWrapper failed.");
			retval = J9SH_FAILED;
			goto error;
		}
		shmflags &= ~IPC_CREAT;

		 /*size of zero means use current size of existing object*/
		shmid = shmgetWrapper(portLibrary, fkey, 0, shmflags);

#if defined (J9ZOS390)
		if(shmid == -1) {
			/* zOS doesn't ignore shmget flags that is doesn't understand. So for share classes to work */
			/* on zOS system without APAR OA11519, we need to retry without __IPC_SHAREAS flags */
			shmflags = groupPerm ? SHMFLAGS_GROUP_NOSHAREAS : SHMFLAGS_NOSHAREAS;
			shmflags &= ~IPC_CREAT;
			shmid = shmgetWrapper(portLibrary, fkey, 0, shmflags);
		}
#endif
		if (shmid == -1) {
			Trc_PRT_shmem_j9shmem_statDeprecated_Message("Error: shmgetWrapper failed.");
			retval = J9SH_FAILED;
			goto error;
		}
		statbuf->shmid = shmid;
		
	} else if (cacheFileType == J9SH_SYSV_OLDER_CONTROL_FILE) {
		j9shmem_controlBaseFileFormat info;
		
		Trc_PRT_shmem_j9shmem_statDeprecated_Message("Info: cacheFileType == J9SH_SYSV_OLDER_CONTROL_FILE.");

		if (readOlderNonZeroByteControlFile(portLibrary, fd, &info) != J9SH_SUCCESS) {
			Trc_PRT_shmem_j9shmem_statDeprecated_Message("Error: can not read control file.");
			retval = J9SH_FAILED;
			goto error;
		}
		statbuf->shmid = info.shmid;
		
	} else {
		Trc_PRT_shmem_j9shmem_statDeprecated_BadCacheType(cacheFileType);
		retval = J9SH_FAILED;
		goto error;			
	}

	rc = getShmStats(portLibrary, statbuf->shmid, statbuf);
	if (J9PORT_INFO_SHMEM_STAT_PASSED != rc) {
		Trc_PRT_shmem_j9shmem_statDeprecated_Message("Error: getShmStats failed");
		retval = J9SH_FAILED;
		goto error;
	}

error:
	if (fileIsLocked == TRUE) {
		if (ControlFileCloseAndUnLock(portLibrary, fd) != J9SH_SUCCESS) {
			Trc_PRT_shmem_j9shmem_statDeprecated_Message("Error: could not unlock shared memory control file.");
			retval = J9SH_FAILED;
		}
	}
	if (retval == J9SH_SUCCESS) {
		Trc_PRT_shmem_j9shmem_statDeprecated_Exit("Successful exit.");
	} else {
		Trc_PRT_shmem_j9shmem_statDeprecated_Exit("Exit with Error.");
	}
	return retval;
}

intptr_t
j9shmem_handle_stat(struct J9PortLibrary *portLibrary, struct j9shmem_handle *handle, struct J9PortShmemStatistic *statbuf)
{
	intptr_t rc = J9PORT_ERROR_SHMEM_STAT_FAILED;

	Trc_PRT_shmem_j9shmem_handle_stat_Entry1(handle, handle ? handle->shmid : -1);

	clearPortableError(portLibrary);

	if (NULL == handle) {
		Trc_PRT_shmem_j9shmem_handle_stat_ErrorNullHandle();
		rc = J9PORT_ERROR_SHMEM_HANDLE_INVALID;
		goto _end;
	}
	if (NULL == statbuf) {
		Trc_PRT_shmem_j9shmem_handle_stat_ErrorNullBuffer();
		rc = J9PORT_ERROR_SHMEM_STAT_BUFFER_INVALID;
		goto _end;
	} else {
		initShmemStatsBuffer(portLibrary, statbuf);
	}

	rc = getShmStats(portLibrary, handle->shmid, statbuf);
	if (J9PORT_INFO_SHMEM_STAT_PASSED != rc) {
		Trc_PRT_shmem_j9shmem_handle_stat_ErrorGetShmStatsFailed(handle->shmid);
	}

_end:
	Trc_PRT_shmem_j9shmem_handle_stat_Exit(rc);
	return rc;
}

void
j9shmem_shutdown(struct J9PortLibrary *portLibrary)
{
}

int32_t
j9shmem_startup(struct J9PortLibrary *portLibrary)
{
#if defined(J9ZOS39064)
#if (__EDC_TARGET >= __EDC_LE410A)
	__mopl_t mymopl;
	int  mos_rv;
	void * mymoptr;
#endif
#endif

	if (NULL != portLibrary->portGlobals) {
#if defined(AIXPPC)
		PPG_pageProtectionPossible = PAGE_PROTECTION_NOTCHECKED;
#endif
#if defined(J9ZOS39064)
#if (__EDC_TARGET >= __EDC_LE410A)
		memset(&mymopl, 0, sizeof(__mopl_t));

		/* Set a shared memory dump priority for subsequent shmget()
		 *  setting dump priority to __MO_DUMP_PRIORITY_STACK ensures that shared memory regions
		 *  gets included in the first tdump (On 64 bit, tdumps gets divided into chunks < 2G, this
		 *  makes sure shared memory gets into the first chunk).
		 */
		mymopl.__mopldumppriority = __MO_DUMP_PRIORITY_STACK;
		mos_rv = __moservices(__MO_SHMDUMPPRIORITY, sizeof(mymopl),
		                      &mymopl , &mymoptr);
		if (mos_rv != 0) {
			return (int32_t)J9PORT_ERROR_STARTUP_SHMEM_MOSERVICE;
		}
#endif
#endif
		return 0;
	}
	return (int32_t)J9PORT_ERROR_STARTUP_SHMEM;
}

/**
 * @internal
 * Create a sharedmemory handle for j9shmem_open() caller ...
 *
 * @param[in] portLibrary
 * @param[in] shmid id of SysV object
 * @param[in] controlFile file being used for control file
 * @param[in] category Memory category structure for allocation
 * @param[in] size size of shared memory region
 * @param[out] handle the handle obj
 *
 * @return	void
 */
static void
createshmHandle(J9PortLibrary *portLibrary, int32_t shmid, const char *controlFile, OMRMemCategory * category, uintptr_t size, j9shmem_handle * handle, uint32_t perm)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t cfstrLength = strlen(controlFile);

	Trc_PRT_shmem_createshmHandle_Entry(controlFile, shmid);
	    
	handle->shmid = shmid;
	omrstr_printf(handle->baseFileName, cfstrLength+1, "%s", controlFile);
	handle->regionStart = NULL;
	handle->size = size;
	handle->category = category;
	handle->perm = (int32_t)perm;

	Trc_PRT_shmem_createshmHandle_Exit(J9SH_SUCCESS);
}

/* Note that, although it's the responsibility of the caller to generate the
 * version prefix and generation postfix, the function this calls strictly checks
 * the format used by shared classes. It would be much better to have a more flexible
 * API that takes a call-back function, but we decided not to change the API this release */ 
static uintptr_t
isControlFileName(struct J9PortLibrary* portLibrary, char* nameToTest)
{
	return isCacheFileName(portLibrary, nameToTest, J9PORT_SHR_CACHE_TYPE_NONPERSISTENT, J9SH_MEMORY_ID);
}

/**
 * @internal
 * Get the full path for a shared memory area
 *
 * @param[in] portLibrary The port library
 * @param[in] cacheDirName path to directory where control files are present
 * @param[out] buffer where the path will be stored
 * @param[in] size size of the buffer
 * @param[in] name of the shared memory area
 *
 */
static void 
getControlFilePath(struct J9PortLibrary* portLibrary, const char* cacheDirName, char* buffer, uintptr_t size, const char* name)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	omrstr_printf(buffer, size, "%s%s", cacheDirName, name);

#if defined(J9SHMEM_DEBUG)
	omrtty_printf("getControlFileName returns: %s\n",buffer);
#endif
}

intptr_t
j9shmem_getDir(struct J9PortLibrary* portLibrary, const char* ctrlDirName, uint32_t flags, char* buffer, uintptr_t bufLength)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	const char* rootDir = NULL;
	char homeDirBuf[J9SH_MAXPATH];
	const char* homeDir = NULL;
	intptr_t rc = 0;
	BOOLEAN appendBaseDir = J9_ARE_ALL_BITS_SET(flags, J9SHMEM_GETDIR_APPEND_BASEDIR);

	Trc_PRT_j9shmem_getDir_Entry();

	memset(homeDirBuf, 0, sizeof(homeDirBuf));

	if (ctrlDirName != NULL) {
		rootDir = ctrlDirName;
	} else {
		if (J9_ARE_ALL_BITS_SET(flags, J9SHMEM_GETDIR_USE_USERHOME)) {
			/* If J9SHMEM_GETDIR_USE_USERHOME is set, try setting cache directory to $HOME/.cache/javasharedresources/ */
			Assert_PRT_true(TRUE == appendBaseDir);
			uintptr_t baseDirLen = sizeof(J9SH_HIDDENDIR) - 1 + sizeof(J9SH_BASEDIR) - 1;
			uintptr_t minBufLen = bufLength < J9SH_MAXPATH ? bufLength : J9SH_MAXPATH;
			/*
			 *  User could define/change their notion of the home directory by setting environment variable "HOME".
			 *  So check "HOME" first, if failed, then check getpwuid(getuid())->pw_dir.
			 */
			if (0 == omrsysinfo_get_env("HOME", homeDirBuf, J9SH_MAXPATH)) {
				uintptr_t dirLen = strlen((const char*)homeDirBuf);
				if (0 < dirLen
					&& ((dirLen + baseDirLen) < minBufLen))
				{
					homeDir = homeDirBuf;
				} else {
					Trc_PRT_j9shmem_getDir_tryHomeDirFailed_homeDirTooLong(dirLen, minBufLen - baseDirLen);
				}
			} else {
				Trc_PRT_j9shmem_getDir_tryHomeDirFailed_getEnvHomeFailed();
			}
			if (NULL == homeDir) {
				struct passwd *pwent = NULL;
#if defined(J9VM_OPT_CRIU_SUPPORT)
				/* isCheckPointAllowed is equivalent to isCheckpointAllowed().
				 * https://github.com/eclipse-openj9/openj9/issues/15800
				 */
				if (portLibrary->isCheckPointAllowed) {
					Trc_PRT_j9shmem_getDir_tryHomeDirFailed_notFinalRestore();
				} else
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
				{
					pwent = getpwuid(getuid());
					if (NULL == pwent) {
						Trc_PRT_j9shmem_getDir_tryHomeDirFailed_getpwuidFailed();
					}
				}
				if (NULL != pwent) {
					uintptr_t dirLen = strlen((const char*)pwent->pw_dir);
					if ((0 < dirLen)
						&& ((dirLen + baseDirLen) < minBufLen)
					) {
						homeDir = pwent->pw_dir;
					} else {
						rc = J9PORT_ERROR_SHMEM_GET_DIR_HOME_BUF_OVERFLOW;
						Trc_PRT_j9shmem_getDir_tryHomeDirFailed_pw_dirDirTooLong(dirLen, minBufLen - baseDirLen);
					}
				} else {
					rc = J9PORT_ERROR_SHMEM_GET_DIR_FAILED_TO_GET_HOME;
				}
			}
			if (NULL != homeDir) {
				struct J9FileStat statBuf = {0};
				if (0 == omrfile_stat(homeDir, 0, &statBuf)) {
					if (!statBuf.isRemote) {
						uintptr_t charWritten = omrstr_printf(homeDirBuf, J9SH_MAXPATH, SHM_STRING_ENDS_WITH_CHAR(homeDir, '/') ? "%s%s" : "%s/%s", homeDir, J9SH_HIDDENDIR);
						Assert_PRT_true(charWritten < J9SH_MAXPATH);
						rootDir = homeDirBuf;
					} else {
						rc = J9PORT_ERROR_SHMEM_GET_DIR_HOME_ON_NFS;
						Trc_PRT_j9shmem_getDir_tryHomeDirFailed_homeOnNFS(homeDir);
					}
				} else {
					rc = J9PORT_ERROR_SHMEM_GET_DIR_CANNOT_STAT_HOME;
					Trc_PRT_j9shmem_getDir_tryHomeDirFailed_cannotStat(homeDir);
				}
			}
		} else {
			rootDir = J9SH_DEFAULT_CTRL_ROOT;
		}
	}
	if (NULL == rootDir) {
		Assert_PRT_true(J9_ARE_ALL_BITS_SET(flags, J9SHMEM_GETDIR_USE_USERHOME));
		Assert_PRT_true(rc < 0);
	} else {
		if (appendBaseDir) {
			if (omrstr_printf(buffer, bufLength, SHM_STRING_ENDS_WITH_CHAR(rootDir, '/') ? "%s%s" : "%s/%s", rootDir, J9SH_BASEDIR) >= bufLength) {
				Trc_PRT_j9shmem_getDir_ExitFailedOverflow();
				rc = J9PORT_ERROR_SHMEM_GET_DIR_BUF_OVERFLOW;
			}
		} else {
			/* Avoid appending two slashes; this leads to problems in matching full file names. */
			if (omrstr_printf(buffer,
								bufLength,
								SHM_STRING_ENDS_WITH_CHAR(rootDir, '/') ? "%s" : "%s/",
								rootDir) >= bufLength) {
				Trc_PRT_j9shmem_getDir_ExitFailedOverflow();
				rc = J9PORT_ERROR_SHMEM_GET_DIR_BUF_OVERFLOW;
			}
		}
	}

	Trc_PRT_j9shmem_getDir_Exit(buffer);
	return rc;
}

intptr_t
j9shmem_createDir(struct J9PortLibrary* portLibrary, char* cacheDirName, uintptr_t cacheDirPerm, BOOLEAN cleanMemorySegments)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t rc, rc2;
	char pathBuffer[J9SH_MAXPATH];
	BOOLEAN usingDefaultTmp = FALSE;

	Trc_PRT_j9shmem_createDir_Entry();
	if (0 == j9shmem_getDir(portLibrary, NULL, J9SHMEM_GETDIR_APPEND_BASEDIR, pathBuffer, J9SH_MAXPATH)) {
		if (0 == strcmp(cacheDirName, (const char*)pathBuffer)) {
			usingDefaultTmp = TRUE;
		}
	}

	rc = omrfile_attr(cacheDirName);
	switch(rc) {
	case EsIsFile:
		Trc_PRT_j9shmem_createDir_ExitFailedFile();
		break;
	case EsIsDir:
#if !defined(J9ZOS390)
		if ((J9SH_DIRPERM_ABSENT == cacheDirPerm) || (J9SH_DIRPERM_ABSENT_GROUPACCESS == cacheDirPerm)) {
			if (usingDefaultTmp) {
				/* cacheDirPerm option is not specified. Change permission to J9SH_DIRPERM_DEFAULT_TMP if it is the default dir under tmp. */
				rc2 = changeDirectoryPermission(portLibrary, cacheDirName, J9SH_DIRPERM_DEFAULT_TMP);
				if (J9SH_FILE_DOES_NOT_EXIST == rc2) {
					Trc_PRT_shared_createDir_Exit3(errno);
					return -1;
				} else {
					Trc_PRT_shared_createDir_Exit4(errno);
					return 0;
				}
			} else {
				Trc_PRT_shared_createDir_Exit8();
				return 0;
			}
		} else
#endif
		{
			Trc_PRT_shared_createDir_Exit7();
			return 0;
		}
	default: /* Directory is not there */
		strncpy(pathBuffer, cacheDirName, J9SH_MAXPATH);

		if (cleanMemorySegments) {
			cleanSharedMemorySegments(portLibrary);
		}
		/* Note that the createDirectory call may change pathBuffer */
		rc = createDirectory(portLibrary, (char*)pathBuffer, cacheDirPerm);
		if (J9SH_FAILED != rc) {
			if (J9SH_DIRPERM_ABSENT == cacheDirPerm) {
				if (usingDefaultTmp) {
					rc2 = changeDirectoryPermission(portLibrary, cacheDirName, J9SH_DIRPERM_DEFAULT_TMP);
				} else {
					rc2 = changeDirectoryPermission(portLibrary, cacheDirName, J9SH_DIRPERM);
				}
			} else if (J9SH_DIRPERM_ABSENT_GROUPACCESS == cacheDirPerm) {
				if (usingDefaultTmp) {
					rc2 = changeDirectoryPermission(portLibrary, cacheDirName, J9SH_DIRPERM_DEFAULT_TMP);
				} else {
					rc2 = changeDirectoryPermission(portLibrary, cacheDirName, J9SH_DIRPERM_GROUPACCESS);
				}
			} else {
				rc2 = changeDirectoryPermission(portLibrary, cacheDirName, cacheDirPerm);
			}
			if (J9SH_FILE_DOES_NOT_EXIST == rc2) {
				Trc_PRT_shared_createDir_Exit5(errno);
				return -1;
			} else {
				Trc_PRT_shared_createDir_Exit6(errno);
				return 0;
			}
		}
		Trc_PRT_j9shmem_createDir_ExitFailed();
		return -1;
	}

	Trc_PRT_j9shmem_createDir_ExitFailed();
	return -1;
}

intptr_t
j9shmem_getFilepath(struct J9PortLibrary* portLibrary, char* cacheDirName, char* buffer, uintptr_t length, const char* cachename)
{
	if (cacheDirName == NULL) {
		Trc_PRT_shmem_j9shmem_getFilepath_ExitNullCacheDirName();
		return -1;
	}
	getControlFilePath(portLibrary, cacheDirName, buffer, length, cachename);
	return 0;
}

intptr_t
j9shmem_protect(struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, void *address, uintptr_t length, uintptr_t flags)
{
	intptr_t rc = J9PORT_PAGE_PROTECT_NOT_SUPPORTED;

#if defined(J9ZTPF)
	return rc;
#endif /* defined(J9ZTPF) */

#if defined(J9ZOS390) && !defined(J9ZOS39064)
	Trc_PRT_shmem_j9shmem_protect(address, length);
	rc = protect_memory(portLibrary, address, length, flags);

#elif defined(AIXPPC) /* defined(J9ZOS390) && !defined(J9ZOS39064) */
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);

	if (PAGE_PROTECTION_NOTCHECKED == PPG_pageProtectionPossible) {
		if (NULL == cacheDirName) {
			Trc_PRT_shmem_j9shmem_protect_ExitNullCacheDirName();
			return -1;
		}
		portLibrary->shmem_get_region_granularity(portLibrary, cacheDirName, groupPerm, address);
	}
	if (PAGE_PROTECTION_NOTAVAILABLE == PPG_pageProtectionPossible) {
		return rc;
	}
	Trc_PRT_shmem_j9shmem_protect(address, length);
	rc = omrmmap_protect(address, length, flags);

#elif defined(LINUX) /* defined(AIXPPC) */
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);

	Trc_PRT_shmem_j9shmem_protect(address, length);
	rc = omrmmap_protect(address, length, flags);
#endif /* defined(J9ZOS390) && !defined(J9ZOS39064) */

	return rc;
}
 
uintptr_t 
j9shmem_get_region_granularity(struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, void *address)
{
	uintptr_t rc = 0;
	
#if defined(J9ZOS390) && !defined(J9ZOS39064)
	rc = protect_region_granularity(portLibrary, address);
	
#elif defined(AIXPPC) /* defined(J9ZOS390) && !defined(J9ZOS39064) */
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	
	if (PAGE_PROTECTION_NOTCHECKED == PPG_pageProtectionPossible) {
		intptr_t envrc = 0;
		char buffer[J9SH_MAXPATH];
		envrc = omrsysinfo_get_env("MPROTECT_SHM", buffer, J9SH_MAXPATH);
	
	  	if ((0 != envrc) || (0 != strcmp(buffer,"ON"))) {
			Trc_PRT_shmem_get_region_granularity_MPROTECT_SHM_Not_Set();
			PPG_pageProtectionPossible = PAGE_PROTECTION_NOTAVAILABLE;
		}
    }

	if (PAGE_PROTECTION_NOTAVAILABLE == PPG_pageProtectionPossible) {
		return rc;
	}
	rc = omrmmap_get_region_granularity(address);

#elif defined(LINUX) /* defined(AIXPPC) */
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	
	rc = omrmmap_get_region_granularity(address);
#endif /* defined(J9ZOS390) && !defined(J9ZOS39064) */

	return rc;
}

int32_t
j9shmem_getid (struct J9PortLibrary *portLibrary, struct j9shmem_handle* handle)
{
	if (NULL != handle) {
		return handle->shmid;
	} else {
		return 0;
	}
}

static intptr_t
readControlFile(J9PortLibrary *portLibrary, intptr_t fd, j9shmem_controlFileFormat * info)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t rc;
	/* Ensure the buffer is big enough to hold an optional padding word */
	uint8_t buffer[sizeof(j9shmem_controlFileFormat) + sizeof(uint32_t)];
	BOOLEAN hasPadding = sizeof(j9shmem_controlBaseFileFormat) != ((uintptr_t)&info->size - (uintptr_t)info);

	rc = omrfile_read(fd, buffer, sizeof(buffer));
	if (rc < 0) {
		return J9SH_ERROR;
	}

	if (rc == sizeof(j9shmem_controlFileFormat)) {
		/* Control file size is an exact match */
		memcpy(info, buffer, sizeof(j9shmem_controlFileFormat));
		return J9SH_SUCCESS;
	}

	if (hasPadding) {
		if (rc == (sizeof(j9shmem_controlFileFormat) - sizeof(uint32_t))) {
			/* Control file does not have the padding, but this JVM uses padding */
			memcpy(info, buffer, sizeof(j9shmem_controlBaseFileFormat));
			memcpy(&info->size, buffer + sizeof(j9shmem_controlBaseFileFormat), sizeof(j9shmem_controlFileFormat) - ((uintptr_t)&info->size - (uintptr_t)info));
			return J9SH_SUCCESS;
		}
	} else {
		if (rc == (sizeof(j9shmem_controlFileFormat) + sizeof(uint32_t))) {
			/* Control file has the padding, but this JVM doesn't use padding */
			memcpy(info, buffer, sizeof(j9shmem_controlBaseFileFormat));
			memcpy(&info->size, buffer + sizeof(j9shmem_controlBaseFileFormat) + sizeof(uint32_t), sizeof(j9shmem_controlFileFormat) - ((uintptr_t)&info->size - (uintptr_t)info));
			return J9SH_SUCCESS;
		}
	}
	return J9SH_FAILED;
}

static intptr_t 
readOlderNonZeroByteControlFile(J9PortLibrary *portLibrary, intptr_t fd, j9shmem_controlBaseFileFormat * info)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t rc;

	rc = omrfile_read(fd, info, sizeof(j9shmem_controlBaseFileFormat));

	if (rc < 0) {
		return J9SH_FAILED;/*READ_ERROR;*/
	} else if (rc == 0) {
		return J9SH_FAILED;/*J9SH_NO_DATA;*/
	} else if (rc != sizeof(struct j9shmem_controlBaseFileFormat)) {
		return J9SH_FAILED;/*J9SH_BAD_DATA;*/
	}
	return J9SH_SUCCESS;
}

/**
 * @internal
 * Open a SysV shared memory based on information in control file.
 *
 * @param[in] portLibrary
 * @param[in] fd of file to write info to ...
 * @param[in] baseFile file being used for control file
 * @param[in] perm permission for the region.
 * @param[in] controlinfo control file info (copy of data last written to control file)
 * @param[in] groupPerm group permission of the shared memory
 * @param[in] handle used on z/OS to check storage key
 * @param[out] canUnlink on failure it is set to TRUE if caller can unlink the control file, otherwise set to FALSE
 * @param[in] cacheFileType type of control file (regular or older or older-empty)
 *
 * @return J9PORT_INFO_SHMEM_OPENED on success
 */
static intptr_t 
openSharedMemory (J9PortLibrary *portLibrary, intptr_t fd, const char *baseFile, int32_t perm, struct j9shmem_controlFileFormat * controlinfo, uintptr_t groupPerm, struct j9shmem_handle *handle, BOOLEAN *canUnlink, uintptr_t cacheFileType)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	/*The control file contains data, and we have successfully reading our structure*/
	int shmid = -1;
	int shmflags = groupPerm ? SHMFLAGS_GROUP : SHMFLAGS;
	intptr_t rc = -1;
	struct shmid_ds buf;
#if defined (J9ZOS390)
	IPCQPROC info;
#endif
	Trc_PRT_shmem_j9shmem_openSharedMemory_EnterWithMessage("Start");

	/* openSharedMemory can only open sysv memory with the same major level */
	if ((J9SH_SYSV_REGULAR_CONTROL_FILE == cacheFileType) &&
		(J9SH_GET_MOD_MAJOR_LEVEL(controlinfo->common.modlevel) != J9SH_MOD_MAJOR_LEVEL)
	) {
		Trc_PRT_shmem_j9shmem_openSharedMemory_ExitWithMessage("Error: major modlevel mismatch.");
		/* Clear any stale portlibrary error code */
		clearPortableError(portLibrary);
		rc = J9PORT_ERROR_SHMEM_OPFAILED;
		goto failDontUnlink;
	}

#if defined (J9ZOS390)
	handle->controlStorageProtectKey = J9SH_SHM_GET_MODLEVEL_STORAGEKEY(controlinfo->common.modlevel);
	Trc_PRT_shmem_j9shmem_openSharedMemory_storageKey(handle->controlStorageProtectKey, handle->currentStorageProtectKey);
	/* Switch to readonly mode if memory was created in storage protection key 9 */
	if ((9 == handle->controlStorageProtectKey) &&
	    (8 == handle->currentStorageProtectKey) &&
	    (perm != J9SH_SHMEM_PERM_READ)
	) {
		/* Clear any stale portlibrary error code */
		clearPortableError(portLibrary);
		rc = J9PORT_ERROR_SHMEM_ZOS_STORAGE_KEY_READONLY;
		goto failDontUnlink;
	}
#endif

	shmflags &= ~IPC_CREAT;
	if (perm == J9SH_SHMEM_PERM_READ) {
		shmflags &= ~(S_IWUSR | S_IWGRP);
	}

	/*size of zero means use current size of existing object*/
	shmid = shmgetWrapper(portLibrary, controlinfo->common.ftok_key, 0, shmflags);

#if defined (J9ZOS390)
	if(-1 == shmid) {
		/* zOS doesn't ignore shmget flags that is doesn't understand. So for share classes to work */
		/* on zOS system without APAR OA11519, we need to retry without __IPC_SHAREAS flags */
		shmflags = groupPerm ? SHMFLAGS_GROUP_NOSHAREAS : SHMFLAGS_NOSHAREAS;
		shmflags &= ~IPC_CREAT;
		if (perm == J9SH_SHMEM_PERM_READ) {
			shmflags &= ~(S_IWUSR | S_IWGRP);
		}
		Trc_PRT_shmem_call_shmget_zos(controlinfo->common.ftok_key, controlinfo->size, shmflags);
		/*size of zero means use current size of existing object*/
		shmid = shmgetWrapper(portLibrary, controlinfo->common.ftok_key, 0, shmflags);
	}
#endif

	if (-1 == shmid) {
		int32_t lastError = omrerror_last_error_number() | J9PORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
		if ((lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_ENOENT) || (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EIDRM)) {
			Trc_PRT_shmem_j9shmem_openSharedMemory_Msg("The shared SysV obj was deleted, but the control file still exists.");
			rc = J9PORT_ERROR_SHMEM_OPFAILED_SHARED_MEMORY_NOT_FOUND;
			goto fail;
		} else if (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EACCES) {
			Trc_PRT_shmem_j9shmem_openSharedMemory_Msg("Info: EACCES occurred.");
			/*Continue looking for opportunity to re-create*/
		} else {
			/*Any other errno will not result is an error*/
			Trc_PRT_shmem_j9shmem_openSharedMemory_MsgWithError("Error: can not open shared memory (shmget failed), portable errorCode = ", lastError);
			rc = J9PORT_ERROR_SHMEM_OPFAILED;
			goto failDontUnlink;
		}
	}

	do {

		if (shmid != -1 && shmid != controlinfo->common.shmid) {
			Trc_PRT_shmem_j9shmem_openSharedMemory_Msg("The SysV id does not match our control file.");
			/* Clear any stale portlibrary error code */
			clearPortableError(portLibrary);
			rc = J9PORT_ERROR_SHMEM_OPFAILED_SHMID_MISMATCH;
			goto fail;
		}

		/*If our sysv obj id is gone, or our <key, id> pair is invalid, then we can also recover*/
		rc = shmctlWrapper(portLibrary, TRUE, controlinfo->common.shmid, IPC_STAT, &buf);
		if (-1 == rc) {
			int32_t lastError = omrerror_last_error_number() | J9PORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
			rc = J9PORT_ERROR_SHMEM_OPFAILED;
			if ((lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EINVAL) || (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EIDRM)) {
				Trc_PRT_shmem_j9shmem_openSharedMemory_Msg("The SysV obj may have been removed just before, or during, our sXmctl call.");
				rc = J9PORT_ERROR_SHMEM_OPFAILED_SHARED_MEMORY_NOT_FOUND;
				goto fail;
			} else {
				if ((lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EACCES) && (shmid != -1)) {
					Trc_PRT_shmem_j9shmem_openSharedMemory_Msg("The SysV obj may have been modified since the call to sXmget.");
					goto fail;
				}/*Any other error and we will terminate*/

				/*If sXmctl fails our checks below will also fail (they use the same function) ... so we terminate with an error*/
				Trc_PRT_shmem_j9shmem_openSharedMemory_MsgWithError("Error: shmctl failed. Can not open shared memory, portable errorCode = ", lastError);
				goto failDontUnlink;
			}
		} else {
#if (defined(__GNUC__) && !defined(J9ZOS390)) || defined(AIXPPC)
#if defined(OSX)
			/*Use ._key for OSX*/
			if (buf.shm_perm._key != controlinfo->common.ftok_key)
#elif defined(AIXPPC)
			/*Use .key for AIXPPC*/
			if (buf.shm_perm.key != controlinfo->common.ftok_key)
#elif defined(__GNUC__)
			/*Use .__key for __GNUC__*/
			if (buf.shm_perm.__key != controlinfo->common.ftok_key)
#endif
			{
#if defined (J9OS_I5)
                                /* The statement 'buf.shm_perm.key != controlinfo->common.ftok_key' will never fail on IBM i platform,
                                 * as the definition of structure ipc_perm is different:
                                 *  'key' field doesn't exists in structure ipc_perm on IBM i and the value of 'key' always zero.
                                 *  Simply log the error here, and then ignore it to avoid more incorrect messages
                                 */
                                Trc_PRT_shmem_j9shmem_openSharedMemory_Msg("The <key,id> pair in our control file is not valid, but we ignore it here to avoid more incorrect messages."); 
#else /* defined (J9OS_I5) */
				Trc_PRT_shmem_j9shmem_openSharedMemory_Msg("The <key,id> pair in our control file is no longer valid.");
				/* Clear any stale portlibrary error code */
				clearPortableError(portLibrary);
				rc = J9PORT_ERROR_SHMEM_OPFAILED_SHM_KEY_MISMATCH;
				goto fail;
#endif /* defined (J9OS_I5) */
			}
#endif
#if defined (J9ZOS390)
			if (getipcWrapper(portLibrary, controlinfo->common.shmid, &info, sizeof(IPCQPROC), IPCQALL) == -1) {
				int32_t lastError = omrerror_last_error_number() | J9PORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
				rc = J9PORT_ERROR_SHMEM_OPFAILED;
				if (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EINVAL) {
					Trc_PRT_shmem_j9shmem_openSharedMemory_Msg("__getipc(): The shared SysV obj was deleted, but the control file still exists.");
					rc = J9PORT_ERROR_SHMEM_OPFAILED_SHARED_MEMORY_NOT_FOUND;
					goto fail;
				} else {
					if ((lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EACCES) && (shmid != -1)) {
						Trc_PRT_shmem_j9shmem_openSharedMemory_Msg("__getipc(): has returned EACCES.");
						goto fail;
					}/*Any other error and we will terminate*/

					/*If sXmctl fails our checks below will also fail (they use the same function) ... so we terminate with an error*/
					Trc_PRT_shmem_j9shmem_openSharedMemory_MsgWithError("Error: __getipc(): failed. Can not open shared memory, portable errorCode = ", lastError);
					goto failDontUnlink;
				}
			} else {
				if (info.shm.ipcqkey != controlinfo->common.ftok_key) {
					Trc_PRT_shmem_j9shmem_openSharedMemory_Msg("The <key,id> pair in our control file is no longer valid.");
					rc = J9PORT_ERROR_SHMEM_OPFAILED_SHM_KEY_MISMATCH;
					goto fail;
				}
			}
#endif
			/*Things are looking good so far ...  continue on and do other checks*/
		}

		/* Check for gid, uid and size only for regular control files.
		 * These fields in older control files cannot be read.
		 * See comments in j9shmem.h
		 */
		if (J9SH_SYSV_REGULAR_CONTROL_FILE == cacheFileType) {
			rc = checkGid(portLibrary, controlinfo->common.shmid, controlinfo->gid);
			if (rc == 0) {
				Trc_PRT_shmem_j9shmem_openSharedMemory_Msg("The creator gid does not match our control file.");
				/* Clear any stale portlibrary error code */
				clearPortableError(portLibrary);
				rc = J9PORT_ERROR_SHMEM_OPFAILED_SHM_GROUPID_CHECK_FAILED;
				goto fail;
			} else if (rc == -1) {
				Trc_PRT_shmem_j9shmem_openSharedMemory_ExitWithMessage("Error: checkGid failed during shmctl.");
				rc = J9PORT_ERROR_SHMEM_OPFAILED;
				goto failDontUnlink;
			} /*Continue looking for opportunity to re-create*/

			rc = checkUid(portLibrary, controlinfo->common.shmid, controlinfo->uid);
			if (rc == 0) {
				Trc_PRT_shmem_j9shmem_openSharedMemory_Msg("The creator uid does not match our control file.");
				/* Clear any stale portlibrary error code */
				clearPortableError(portLibrary);
				rc = J9PORT_ERROR_SHMEM_OPFAILED_SHM_USERID_CHECK_FAILED;
				goto fail;
			} else if (rc == -1) {
				Trc_PRT_shmem_j9shmem_openSharedMemory_ExitWithMessage("Error: checkUid failed during shmctl.");
				rc = J9PORT_ERROR_SHMEM_OPFAILED;
				goto failDontUnlink;
			} /*Continue looking for opportunity to re-create*/

			rc = checkSize(portLibrary, controlinfo->common.shmid, controlinfo->size);
			if (rc == 0) {
				Trc_PRT_shmem_j9shmem_openSharedMemory_Msg("The size does not match our control file.");
				rc = J9PORT_ERROR_SHMEM_OPFAILED_SHM_SIZE_CHECK_FAILED;
				/* Clear any stale portlibrary error code */
				clearPortableError(portLibrary);
				goto fail;
			} else if (rc == -1) {
				Trc_PRT_shmem_j9shmem_openSharedMemory_ExitWithMessage("Error: checkSize failed during shmctl.");
				rc = J9PORT_ERROR_SHMEM_OPFAILED;
				goto failDontUnlink;
			} /*Continue looking for opportunity to re-create*/
		}

		/*No more checks ... We want this loop to exec only once ... so we break ...*/
	} while (FALSE);

	if (-1 == shmid) {
		rc = J9PORT_ERROR_SHMEM_OPFAILED;
		goto failDontUnlink;
	}
pass:
	Trc_PRT_shmem_j9shmem_openSharedMemory_ExitWithMessage("Successfully opened shared memory.");
	return J9PORT_INFO_SHMEM_OPENED;
fail:
	*canUnlink = TRUE;
	Trc_PRT_shmem_j9shmem_openSharedMemory_ExitWithMessage("Error: can not open shared memory (shmget failed). Caller MAY unlink control file.");
	return rc;
failDontUnlink:
	*canUnlink = FALSE;
	Trc_PRT_shmem_j9shmem_openSharedMemory_ExitWithMessage("Error: can not open shared memory (shmget failed). Caller should not unlink control file");
	return rc;
}

/**
 * @internal
 * Create a SysV shared memory 
 *
 * @param[in] portLibrary
 * @param[in] fd of file to write info to ...
 * @param[in] isReadOnlyFD set to TRUE if control file is read-only, FALSE otherwise
 * @param[in] baseFile file being used for control file
 * @param[in] size size of the shared memory
 * @param[in] perm permission for the region
 * @param[in] controlinfo control file info (copy of data last written to control file)
 * @param[in] groupPerm group permission of the shared memory
 * @param[out] handle used on z/OS to store current storage key
 *
 * @return	J9PORT_INFO_SHMEM_CREATED on success
 */
static int32_t 
createSharedMemory(J9PortLibrary *portLibrary, intptr_t fd, BOOLEAN isReadOnlyFD, const char *baseFile, int32_t size, int32_t perm, struct j9shmem_controlFileFormat * controlinfo, uintptr_t groupPerm, struct j9shmem_handle *handle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t i = 0;
	intptr_t projId = SHMEM_FTOK_PROJID;
	intptr_t maxProjIdRetries = 20;
	int shmflags = groupPerm ? SHMFLAGS_GROUP : SHMFLAGS;
#if defined (J9ZOS390)
	int32_t oldsize = size;
#endif

	Trc_PRT_shmem_j9shmem_createSharedMemory_EnterWithMessage("start");

	if ((TRUE == isReadOnlyFD) || (J9SH_SHMEM_PERM_READ == perm)) {
		Trc_PRT_shmem_j9shmem_createSharedMemory_ExitWithMessage("Error: can not write control file b/c either file descriptor is opened read only or shared memory permission is set to read-only.");
		return J9PORT_ERROR_SHMEM_OPFAILED;
	}

	if (controlinfo == NULL) {
		Trc_PRT_shmem_j9shmem_createSharedMemory_ExitWithMessage("Error: controlinfo == NULL");
		return J9PORT_ERROR_SHMEM_OPFAILED;
	}

#if defined (J9ZOS390)
	/* round size up to next megabyte */
	if ((0 == size) || ((size & 0xfff00000) != size)) {
		if ( size > 0x7ff00000) {
			size = 0x7ff00000;
		} else {
			size = (size + 0x100000) & 0xfff00000;
		}
	}
	Trc_PRT_shmem_createSharedMemory_round_size( oldsize, size);
#endif

	for (i = 0; i < maxProjIdRetries; i += 1, projId += 1) {
		int shmid = -1;
		key_t fkey = -1;
		intptr_t rc = 0;

		fkey = ftokWrapper(portLibrary, baseFile, projId);
		if (-1 == fkey) {
			int32_t lastError = omrerror_last_error_number() | J9PORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
			if (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_ENOENT) {
				Trc_PRT_shmem_j9shmem_createSharedMemory_Msg("ftok: ENOENT");
			} else if (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EACCES) {
				Trc_PRT_shmem_j9shmem_createSharedMemory_Msg("ftok: EACCES");
			} else {
				/*By this point other errno's should not happen*/
				Trc_PRT_shmem_j9shmem_createSharedMemory_MsgWithError("ftok: unexpected errno, portable errorCode = ", lastError);
			}
			Trc_PRT_shmem_j9shmem_createSharedMemory_ExitWithMessage("Error: fkey == -1");
			return J9PORT_ERROR_SHMEM_OPFAILED;
		}

		shmid = shmgetWrapper(portLibrary, fkey, size, shmflags);

#if defined (J9ZOS390)
		if(-1 == shmid) {
			/* zOS doesn't ignore shmget flags that is doesn't understand. So for share classes to work */
			/* on zOS system without APAR OA11519, we need to retry without __IPC_SHAREAS flags */
			shmflags = groupPerm ? SHMFLAGS_GROUP_NOSHAREAS : SHMFLAGS_NOSHAREAS;
			Trc_PRT_shmem_call_shmget_zos(fkey, size, shmflags);
			shmid = shmgetWrapper(portLibrary, fkey, size, shmflags);
		}
#endif

		if (-1 == shmid) {
			int32_t lastError = omrerror_last_error_number() | J9PORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
			if (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EEXIST || lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EACCES) {
				Trc_PRT_shmem_j9shmem_createSharedMemory_Msg("Retry (errno == EEXIST || errno == EACCES) was found.");
				continue;
			}
			if (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EINVAL) {
				omrerror_set_last_error(EINVAL, J9PORT_ERROR_SHMEM_TOOBIG);
			}
			Trc_PRT_shmem_j9shmem_createSharedMemory_ExitWithMessageAndError("Error: portable errorCode = ", lastError);
			return J9PORT_ERROR_SHMEM_OPFAILED;
		}

#if defined (J9ZOS390)
		handle->controlStorageProtectKey = handle->currentStorageProtectKey;
		Assert_PRT_true(0xF000 > J9SH_MODLEVEL);
		controlinfo->common.modlevel = J9SH_SHM_CREATE_MODLEVEL_WITH_STORAGEKEY(handle->controlStorageProtectKey);
		Trc_PRT_shmem_j9shmem_createSharedMemory_storageKey(handle->controlStorageProtectKey);
#else
		controlinfo->common.modlevel = J9SH_MODLEVEL;
#endif
		controlinfo->common.version = J9SH_VERSION;
		controlinfo->common.proj_id = projId;
		controlinfo->common.ftok_key = fkey;
		controlinfo->common.shmid = shmid;
		controlinfo->size = size;
		controlinfo->uid = geteuid();
		controlinfo->gid = getegid();

		if (-1 == omrfile_seek(fd, 0, EsSeekSet)) {
			Trc_PRT_shmem_j9shmem_createSharedMemory_ExitWithMessage("Error: could not seek to start of control file");
			/* Do not set error code if shmctl fails, else j9file_seek() error code will be lost. */
			if (shmctlWrapper(portLibrary, FALSE, shmid, IPC_RMID, NULL) == -1) {
				/*Already in an error condition ... don't worry about failure here*/
				return J9PORT_ERROR_SHMEM_OPFAILED;
			}
			return J9PORT_ERROR_SHMEM_OPFAILED;
		}

		rc = omrfile_write(fd, controlinfo, sizeof(j9shmem_controlFileFormat));
		if (rc != sizeof(j9shmem_controlFileFormat)) {
			Trc_PRT_shmem_j9shmem_createSharedMemory_ExitWithMessage("Error: write to control file failed");
			/* Do not set error code if shmctl fails, else j9file_write() error code will be lost. */
			if (shmctlWrapper(portLibrary, FALSE, shmid, IPC_RMID, NULL) == -1) {
				/*Already in an error condition ... don't worry about failure here*/
				return J9PORT_ERROR_SHMEM_OPFAILED;
			}
			return J9PORT_ERROR_SHMEM_OPFAILED;
		}

		Trc_PRT_shmem_j9shmem_createSharedMemory_ExitWithMessage("Successfully created memory");
		return J9PORT_INFO_SHMEM_CREATED;
	}

	Trc_PRT_shmem_j9shmem_createSharedMemory_ExitWithMessage("Error: end of retries. Clean up SysV using 'ipcs'");
	return J9PORT_ERROR_SHMEM_OPFAILED;
}

/**
 * @internal
 * Check the size of the shared memory
 *
 * @param[in] portLibrary The port library
 * @param[in] shmid shared memory set id
 * @param[in] size
 *
 * @return	-1 if we can't get info from shmid, 1 if it match is found, 0 if it does not match, -2 if shmctl() returned size as 0
 */
static intptr_t 
checkSize(J9PortLibrary *portLibrary, int shmid, int64_t size)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t rc;
	struct shmid_ds buf;

	rc = shmctlWrapper(portLibrary, TRUE, shmid, IPC_STAT, &buf);
	if (-1 == rc) {
		int32_t lastError = omrerror_last_error_number() | J9PORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
		if (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EINVAL) {
			return 0;
		} else if (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EIDRM) {
			return 0;
		}
		return -1;
	}

	/* Jazz103 88930 : On 64-bit z/OS the size of a 31-bit shared memory segment 
	 * is returned in shmid_ds.shm_segsz_31 when queried by a 64-bit process.
	 */
	if (((int64_t) buf.shm_segsz == size)
#if defined(J9ZOS39064)
		|| ((int64_t)*(int32_t *)buf.shm_segsz_31 == size)
#endif /* defined(J9ZOS39064) */
	) {
		return 1;
	} else {
#if defined(J9ZOS390)
		if (0 == buf.shm_segsz) {
			/* JAZZ103 PR 79909 + PR 88903 : shmctl(IPC_STAT) can return segment size as 0 on z/OS 
			 * if the shared memory segment was created by a 64-bit JVM and the caller is a 31-bit JVM.
			 */
			return -2;
		}
#endif /* defined(J9ZOS390) */
		return 0;
	}
}

/**
 * @internal
 * Check the creator uuid of the shared memory
 *
 * @param[in] portLibrary The port library
 * @param[in] shmid shared memory set id
 * @param[in] uid
 *
 * @return	-1 if we can't get info from shmid, 1 if it match is found, 0 if it does not match
 */
static intptr_t 
checkUid(J9PortLibrary *portLibrary, int shmid, int32_t uid)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t rc;
	struct shmid_ds buf;

	rc = shmctlWrapper(portLibrary, TRUE, shmid, IPC_STAT, &buf);
	if (-1 == rc) {
		int32_t lastError = omrerror_last_error_number() | J9PORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
		if (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EINVAL) {
			return 0;
		} else if (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EIDRM) {
			return 0;
		}
		return -1;
	}

	if (buf.shm_perm.cuid == uid) {
		return 1;
	} else {
		return 0;
	}
}

/**
 * @internal
 * Check the creator gid of the shared memory
 *
 * @param[in] portLibrary The port library
 * @param[in] shmid shared memory set id
 * @param[in] gid
 *
 * @return	-1 if we can't get info from shmid, 1 if it match is found, 0 if it does not match
 */
static intptr_t 
checkGid(J9PortLibrary *portLibrary, int shmid, int32_t gid)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t rc;
	struct shmid_ds buf;

	rc = shmctlWrapper(portLibrary, TRUE, shmid, IPC_STAT, &buf);
	if (-1 == rc) {
		int32_t lastError = omrerror_last_error_number() | J9PORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
		if (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EINVAL) {
			return 0;
		} else if (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EIDRM) {
			return 0;
		}
		return -1;
	}

	if (buf.shm_perm.cgid == gid) {
		return 1;
	} else {
		return 0;
	}
}

/**
 * @internal
 * Initialize J9PortShmemStatistic with default values.
 *
 * @param[in] portLibrary
 * @param[out] statbuf Pointer to J9PortShmemStatistic to be initialized
 *
 * @return void
 */
static void
initShmemStatsBuffer(J9PortLibrary *portLibrary, J9PortShmemStatistic *statbuf)
{
	if (NULL != statbuf) {
		memset(statbuf, 0, sizeof(J9PortShmemStatistic));
	}
}

/**
 * @internal
 * Retrieves stats for the shared memory region identified by shmid, and populates J9PortShmemStatistic.
 * Only those fields in J9PortShmemStatistic are touched which have corresponding fields in OS data structures.
 * Rest are left unchanged (eg file, controlDir).
 *
 * @param[in] portLibrary the Port Library.
 * @param[in] shmid shared memory id
 * @param[out] statbuf the statistics returns by the operating system
 *
 * @return	J9SH_FAILED if failed to get stats, J9SH_SUCCESS on success.
 */
static intptr_t
getShmStats(J9PortLibrary *portLibrary, int shmid, struct J9PortShmemStatistic* statbuf)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t rc = J9PORT_ERROR_SHMEM_STAT_FAILED;
	struct shmid_ds shminfo;

	rc = shmctlWrapper(portLibrary, TRUE, shmid, IPC_STAT, &shminfo);
	if (J9SH_FAILED == rc) {
		int32_t lasterrno = omrerror_last_error_number();
		const char *errormsg = omrerror_last_error_message();
		Trc_PRT_shmem_getShmStats_shmctlFailed(shmid, lasterrno, errormsg);
		rc = J9PORT_ERROR_SHMEM_STAT_FAILED;
		goto _end;
	}

	statbuf->shmid = shmid;
	statbuf->ouid = shminfo.shm_perm.uid;
	statbuf->ogid = shminfo.shm_perm.gid;
	statbuf->cuid = shminfo.shm_perm.cuid;
	statbuf->cgid = shminfo.shm_perm.cgid;
	statbuf->lastAttachTime = shminfo.shm_atime;
	statbuf->lastDetachTime = shminfo.shm_dtime;
	statbuf->lastChangeTime = shminfo.shm_ctime;
	statbuf->nattach = shminfo.shm_nattch;
	/* Jazz103 88930 : On z/OS a 64-bit process needs to access shminfo.shm_segsz_31 
	 * to get size of a 31-bit shared memory segment. 
	 */
#if defined(J9ZOS39064)
	if (0 != shminfo.shm_segsz) {
		statbuf->size = (uintptr_t) shminfo.shm_segsz;
	} else {
		statbuf->size = (uintptr_t)*(int32_t *) shminfo.shm_segsz_31;
	}
#else /* defined(J9ZOS39064) */
	statbuf->size = (uintptr_t) shminfo.shm_segsz;
#endif /* defined(J9ZOS39064) */

	/* Same code as in j9file_stat(). Probably a new function to convert OS modes to J9 modes is required. */
	if (shminfo.shm_perm.mode & S_IWUSR) {
		statbuf->perm.isUserWriteable = 1;
	}
	if (shminfo.shm_perm.mode & S_IRUSR) {
		statbuf->perm.isUserReadable = 1;
	}
	if (shminfo.shm_perm.mode & S_IWGRP) {
		statbuf->perm.isGroupWriteable = 1;
	}
	if (shminfo.shm_perm.mode & S_IRGRP) {
		statbuf->perm.isGroupReadable = 1;
	}
	if (shminfo.shm_perm.mode & S_IWOTH) {
		statbuf->perm.isOtherWriteable = 1;
	}
	if (shminfo.shm_perm.mode & S_IROTH) {
		statbuf->perm.isOtherReadable = 1;
	}

	rc = J9PORT_INFO_SHMEM_STAT_PASSED;

_end:
	return rc;
}
