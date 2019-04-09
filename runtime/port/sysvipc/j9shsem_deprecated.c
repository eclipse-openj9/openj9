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

#include <errno.h>
#include <stddef.h>
#include "portpriv.h"
#include "j9port.h"
#include "portnls.h"
#include "ut_j9prt.h"
#include "j9sharedhelper.h"
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "j9shsem.h"
#include "j9shsemun.h"
#include "j9SysvIPCWrappers.h"

/*These flags are only used internally*/
#define CREATE_ERROR -10
#define OPEN_ERROR  -11
#define WRITE_ERROR -12
#define READ_ERROR -13
#define MALLOC_ERROR -14

#define J9SH_NO_DATA -21
#define J9SH_BAD_DATA -22

#define RETRY_COUNT 10
#define SLEEP_TIME 5

#define SEMMARKER_INITIALIZED 769

#define SHSEM_FTOK_PROJID 0x81

static intptr_t checkMarker(J9PortLibrary *portLibrary, int semid, int semsetsize);
static intptr_t checkSize(J9PortLibrary *portLibrary, int semid, int32_t numsems);
static intptr_t createSemaphore(struct J9PortLibrary *portLibrary, intptr_t fd, BOOLEAN isReadOnlyFD, char *baseFile, int32_t setSize, j9shsem_baseFileFormat* controlinfo, uintptr_t groupPerm);
static intptr_t openSemaphore(struct J9PortLibrary *portLibrary, intptr_t fd, char *baseFile, j9shsem_baseFileFormat* controlinfo, uintptr_t groupPerm, uintptr_t cacheFileType);
static void createsemHandle(struct J9PortLibrary *portLibrary, int semid, int nsems, char* baseFile, j9shsem_handle * handle);
static intptr_t readOlderNonZeroByteControlFile(J9PortLibrary *portLibrary, intptr_t fd, j9shsem_baseFileFormat * info);
static void initShsemStatsBuffer(J9PortLibrary *portLibrary, J9PortShsemStatistic *statbuf);

intptr_t
j9shsem_deprecated_openDeprecated (struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct j9shsem_handle** handle, const char* semname, uintptr_t cacheFileType)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t retval = J9PORT_ERROR_SHSEM_OPFAILED;
	j9shsem_baseFileFormat controlinfo;
	char baseFile[J9SH_MAXPATH];
	struct j9shsem_handle * tmphandle = NULL;
	intptr_t baseFileLength = 0;
	intptr_t handleStructLength = sizeof(struct j9shsem_handle);
	intptr_t fd = -1;
	intptr_t fileIsLocked = J9SH_SUCCESS;
	BOOLEAN isReadOnlyFD = FALSE;
	union semun semctlArg;
	struct semid_ds buf;
	semctlArg.buf = &buf;
	*handle = NULL;
	Trc_PRT_shsem_j9shsem_openDeprecated_Entry();

	clearPortableError(portLibrary);

	if (cacheDirName == NULL) {
		Trc_PRT_shsem_j9shsem_deprecated_openDeprecated_ExitNullCacheDirName();
		goto error;
	}

	omrstr_printf(baseFile, J9SH_MAXPATH, "%s%s", cacheDirName, semname);

	baseFileLength = strlen(baseFile) + 1;
	tmphandle = omrmem_allocate_memory((handleStructLength + baseFileLength), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == tmphandle) {
		Trc_PRT_shsem_j9shsem_openDeprecated_Message("Error: could not alloc handle.");
		goto error;
	}

	tmphandle->baseFile = (char *) (((char *) tmphandle) + handleStructLength);

	fileIsLocked = ControlFileOpenWithWriteLock(portLibrary, &fd, &isReadOnlyFD, FALSE, baseFile, 0);
	
	if (fileIsLocked == J9SH_FILE_DOES_NOT_EXIST) {
		/* This will occur if the shared memory control files, exists but the semaphore file does not.
		 *
		 * In this case J9PORT_INFO_SHSEM_PARTIAL is returned to suggest the
		 * caller try doing there work without a lock (if it can).
		 */
		Trc_PRT_shsem_j9shsem_openDeprecated_Message("Error: control file does not exist.");
		retval = J9PORT_INFO_SHSEM_PARTIAL;
		goto error;
	} else if ( fileIsLocked != J9SH_SUCCESS) {
		Trc_PRT_shsem_j9shsem_openDeprecated_Message("Error: could not lock semaphore control file.");
		goto error;
	}

	if (cacheFileType == J9SH_SYSV_OLDER_EMPTY_CONTROL_FILE) {
		int semid = -1;
		int projid = 0xad;
		key_t fkey;
		int semflags_open = groupPerm ? J9SHSEM_SEMFLAGS_GROUP_OPEN : J9SHSEM_SEMFLAGS_OPEN;
		intptr_t rc;

		fkey = ftokWrapper(portLibrary, baseFile, projid);
		if (fkey == -1) {
			Trc_PRT_shsem_j9shsem_openDeprecated_Message("Error: ftok failed.");
			goto error;
		}
		
		/*Check that the semaphore is still on the system.*/
		semid = semgetWrapper(portLibrary, fkey, 0, semflags_open);
		if (-1 == semid) {
			int32_t lasterrno = omrerror_last_error_number() | J9PORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
			const char * errormsg = omrerror_last_error_message();
			Trc_PRT_shsem_j9shsem_openDeprecated_semgetFailed(fkey, lasterrno, errormsg);			
			if (lasterrno == J9PORT_ERROR_SYSV_IPC_ERRNO_EINVAL) {
				Trc_PRT_shsem_j9shsem_openDeprecated_semgetEINVAL(fkey);
				retval = J9PORT_INFO_SHSEM_PARTIAL;
				/*Clean up the control file since its SysV object is deleted.*/
				omrfile_unlink(baseFile);
			}
			goto error;
		}

		/*get buf.sem_nsems size for createsemHandle()*/
		if (semctlWrapper(portLibrary, TRUE, semid, 0, IPC_STAT, semctlArg) == -1) {
			int32_t lasterrno = omrerror_last_error_number() | J9PORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
			const char * errormsg = omrerror_last_error_message();
			Trc_PRT_shsem_j9shsem_openDeprecated_semctlFailed(semid, lasterrno, errormsg);
			goto error;
		}

		createsemHandle(portLibrary, semid, buf.sem_nsems, baseFile, tmphandle);
		tmphandle->timestamp = omrfile_lastmod(baseFile);

		retval = J9PORT_INFO_SHSEM_OPENED;

	} else if (cacheFileType == J9SH_SYSV_OLDER_CONTROL_FILE) {

		if (readOlderNonZeroByteControlFile(portLibrary, fd, &controlinfo) != J9SH_SUCCESS) {
			Trc_PRT_shsem_j9shsem_openDeprecated_Message("Error: could not read deprecated control file.");
			goto error;
		}

		retval = openSemaphore(portLibrary, fd, baseFile, &controlinfo, groupPerm, cacheFileType);
		if (J9PORT_INFO_SHSEM_OPENED != retval) {
			if (J9PORT_ERROR_SHSEM_OPFAILED_DONT_UNLINK == retval) {
				Trc_PRT_shsem_j9shsem_openDeprecated_Message("Control file was not unlinked.");
				retval = J9PORT_ERROR_SHSEM_OPFAILED;
			} else {
				/* Clean up the control file */
				if (-1 == omrfile_unlink(baseFile)) {
					Trc_PRT_shsem_j9shsem_openDeprecated_Message("Control file could not be unlinked.");
				} else {
					Trc_PRT_shsem_j9shsem_openDeprecated_Message("Control file was unlinked.");
				}
				retval = J9PORT_INFO_SHSEM_PARTIAL;
			}
			goto error;
		}
		
		createsemHandle(portLibrary, controlinfo.semid, controlinfo.semsetSize, baseFile, tmphandle);
		tmphandle->timestamp = omrfile_lastmod(baseFile);

		retval = J9PORT_INFO_SHSEM_OPENED;
	} else {
		Trc_PRT_shsem_j9shsem_openDeprecated_BadCacheType(cacheFileType);
	}

error:
	if (fileIsLocked == J9SH_SUCCESS) {
		if (ControlFileCloseAndUnLock(portLibrary, fd) != J9SH_SUCCESS) {
			Trc_PRT_shsem_j9shsem_openDeprecated_Message("Error: could not unlock semaphore control file.");
			retval = J9PORT_ERROR_SHSEM_OPFAILED;
		}
	}
	if (J9PORT_INFO_SHSEM_OPENED != retval){
		if (NULL != tmphandle) {
			omrmem_free_memory(tmphandle);
		}
		*handle = NULL;
		Trc_PRT_shsem_j9shsem_openDeprecated_Exit("Exit: failed to open older semaphore");
	} else {
		*handle = tmphandle;
		Trc_PRT_shsem_j9shsem_openDeprecated_Exit("Opened shared semaphore.");
	}

	return retval;
}

intptr_t 
j9shsem_deprecated_open (struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct j9shsem_handle **handle, const char *semname, int setSize, int permission, uintptr_t flags, J9ControlFileStatus *controlFileStatus)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	/* TODO: needs to be longer? dynamic?*/
	char baseFile[J9SH_MAXPATH];
	char versionStr[32];
	intptr_t fd;
	j9shsem_baseFileFormat controlinfo;
	int32_t rc;
	BOOLEAN isReadOnlyFD;
	intptr_t baseFileLength = 0;
	intptr_t handleStructLength = sizeof(struct j9shsem_handle);
	char * exitmsg = 0;
	intptr_t retryIfReadOnlyCount = 10;
	struct j9shsem_handle * tmphandle = NULL;
	BOOLEAN freeHandleOnError = TRUE;

	Trc_PRT_shsem_j9shsem_open_Entry(semname, setSize, permission);

	clearPortableError(portLibrary);

	if (cacheDirName == NULL) {
		Trc_PRT_shsem_j9shsem_deprecated_open_ExitNullCacheDirName();
		return J9PORT_ERROR_SHSEM_OPFAILED;
	}

	omrstr_printf(baseFile, J9SH_MAXPATH, "%s%s", cacheDirName, semname);

	Trc_PRT_shsem_j9shsem_open_Debug1(baseFile);

	baseFileLength = strlen(baseFile) + 1;
	*handle = NULL;
	tmphandle = omrmem_allocate_memory((handleStructLength + baseFileLength), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == tmphandle) {
		Trc_PRT_shsem_j9shsem_open_ExitWithMsg("Error: could not alloc handle.");
		return J9PORT_ERROR_SHSEM_OPFAILED;
	}
	tmphandle->baseFile = (char *) (((char *) tmphandle) + handleStructLength);

	if (NULL != controlFileStatus) {
		memset(controlFileStatus, 0, sizeof(J9ControlFileStatus));
	}

	for (retryIfReadOnlyCount = 10; retryIfReadOnlyCount > 0; retryIfReadOnlyCount -= 1) {
		/*Open control file with write lock*/
		BOOLEAN canCreateFile = TRUE;
		if (J9_ARE_ANY_BITS_SET(flags, J9SHSEM_OPEN_FOR_STATS | J9SHSEM_OPEN_FOR_DESTROY | J9SHSEM_OPEN_DO_NOT_CREATE)) {
			canCreateFile = FALSE;
		}
		if (ControlFileOpenWithWriteLock(portLibrary, &fd, &isReadOnlyFD, canCreateFile, baseFile, groupPerm) != J9SH_SUCCESS) {
			omrmem_free_memory(tmphandle);
			Trc_PRT_shsem_j9shsem_open_ExitWithMsg("Error: could not lock semaphore control file.");
			if (J9_ARE_ANY_BITS_SET(flags, J9SHSEM_OPEN_FOR_STATS | J9SHSEM_OPEN_FOR_DESTROY | J9SHSEM_OPEN_DO_NOT_CREATE)) {
				return J9PORT_INFO_SHSEM_PARTIAL;
			}
			return J9PORT_ERROR_SHSEM_OPFAILED_CONTROL_FILE_LOCK_FAILED;
		}

		/*Try to read data from the file*/
		rc = omrfile_read(fd, &controlinfo, sizeof(j9shsem_baseFileFormat));

		if (rc == -1) {
			/* The file is empty.*/
			if (J9_ARE_NO_BITS_SET(flags, J9SHSEM_OPEN_FOR_STATS | J9SHSEM_OPEN_FOR_DESTROY | J9SHSEM_OPEN_DO_NOT_CREATE)) {
				rc = createSemaphore(portLibrary, fd, isReadOnlyFD, baseFile, setSize, &controlinfo, groupPerm);
				if (rc == J9PORT_ERROR_SHSEM_OPFAILED) {
					exitmsg = "Error: create of the semaphore has failed.";
					if (isReadOnlyFD == FALSE) {
						if (unlinkControlFile(portLibrary, baseFile, controlFileStatus)) {
							Trc_PRT_shsem_j9shsem_open_Msg("Info: Control file is unlinked after call to createSemaphore.");
						} else {
							Trc_PRT_shsem_j9shsem_open_Msg("Info: Failed to unlink control file after call to createSemaphore.");
						}
					} else {
						Trc_PRT_shsem_j9shsem_open_Msg("Info: Control file is not unlinked after call to createSemaphore(isReadOnlyFD == TRUE).");
					}
				}
			} else {
				Trc_PRT_shsem_j9shsem_open_Msg("Info: Control file cannot be read. Do not attempt to create semaphore as J9SHSEM_OPEN_FOR_STATS, J9SHSEM_OPEN_FOR_DESTROY or J9SHSEM_OPEN_DO_NOT_CREATE is set");
				rc = J9PORT_ERROR_SHSEM_OPFAILED;
			}
		} else if (rc == sizeof(j9shsem_baseFileFormat)) {
			/*The control file contains data, and we have successfully read in our structure*/
			/*Note: there is no unlink here if the file is read-only ... the next function call manages this behaviour*/
			rc = openSemaphore(portLibrary, fd, baseFile, &controlinfo, groupPerm, J9SH_SYSV_REGULAR_CONTROL_FILE);
			if (J9PORT_INFO_SHSEM_OPENED != rc) {
				exitmsg = "Error: open of the semaphore has failed.";

				if ((J9PORT_ERROR_SHSEM_OPFAILED_DONT_UNLINK != rc)
					&& J9_ARE_NO_BITS_SET(flags, J9SHSEM_OPEN_FOR_STATS | J9SHSEM_OPEN_DO_NOT_CREATE)
				) {
					/* PR 74306: If the control file is read-only, do not unlink it unless the control file was created by an older JVM without this fix,
					 * which is identified by the check for major and minor level in the control file.
					 * Reason being if the control file is read-only, JVM acquires a shared lock in ControlFileOpenWithWriteLock().
					 * This implies in multiple JVM scenario it is possible that when one JVM has unlinked and created a new control file,
					 * another JVM unlinks the newly created control file, and creates another control file.
					 * For shared class cache, this may result into situations where the two JVMs, accessing the same cache,
					 * would create two different semaphore sets for same shared memory region causing one of them to detect cache as corrupt
					 * due to semid mismatch.
					 */
					if ((FALSE == isReadOnlyFD)
						|| ((J9SH_GET_MOD_MAJOR_LEVEL(controlinfo.modlevel) == J9SH_MOD_MAJOR_LEVEL_ALLOW_READONLY_UNLINK)
						&& (J9SH_SEM_GET_MOD_MINOR_LEVEL(controlinfo.modlevel) <= J9SH_MOD_MINOR_LEVEL_ALLOW_READONLY_UNLINK)
						&& (J9PORT_ERROR_SHSEM_OPFAILED_SEMAPHORE_NOT_FOUND == rc))
					) {
						if (unlinkControlFile(portLibrary, baseFile, controlFileStatus)) {
							Trc_PRT_shsem_j9shsem_open_Msg("Info: Control file is unlinked after call to openSemaphore.");
							if (J9_ARE_ALL_BITS_SET(flags, J9SHSEM_OPEN_FOR_DESTROY)) {
								if (J9PORT_ERROR_SHSEM_OPFAILED_SEMAPHORE_NOT_FOUND != rc) {
									tmphandle->semid = controlinfo.semid;
									freeHandleOnError = FALSE;
								}
							} else if (retryIfReadOnlyCount > 1) {
								rc = J9PORT_INFO_SHSEM_OPEN_UNLINKED;
							}
						} else {
							Trc_PRT_shsem_j9shsem_open_Msg("Info: Failed to unlink control file after call to openSemaphoe.");
						}
					} else {
						Trc_PRT_shsem_j9shsem_open_Msg("Info: Control file is not unlinked after openSemaphore because it is read-only and (its modLevel does not allow unlinking or rc != SEMAPHORE_NOT_FOUND).");
					}
				} else {
					Trc_PRT_shsem_j9shsem_open_Msg("Info: Control file is not unlinked after call to openSemaphore.");
				}
			}
		} else {
			/*Should never get here ... this means the control file was corrupted*/
			exitmsg = "Error: Semaphore control file is corrupted.";
			rc = J9PORT_ERROR_SHSEM_OPFAILED_CONTROL_FILE_CORRUPT;

			if (J9_ARE_NO_BITS_SET(flags, J9SHSEM_OPEN_FOR_STATS | J9SHSEM_OPEN_DO_NOT_CREATE)) {
				if (isReadOnlyFD == FALSE) {
					if (unlinkControlFile(portLibrary, baseFile, controlFileStatus)) {
						Trc_PRT_shsem_j9shsem_open_Msg("Info: The corrupt control file was unlinked.");
						if (retryIfReadOnlyCount > 1) {
							rc = J9PORT_INFO_SHSEM_OPEN_UNLINKED;
						}
					} else {
						Trc_PRT_shsem_j9shsem_open_Msg("Info: Failed to remove the corrupt control file.");
					}
				} else {
					Trc_PRT_shsem_j9shsem_open_Msg("Info: Corrupt control file is not unlinked (isReadOnlyFD == TRUE).");
				}
			} else {
				Trc_PRT_shsem_j9shsem_open_Msg("Info: Control file is found to be corrupt but is not unlinked (J9SHSEM_OPEN_FOR_STATS or J9SHSEM_OPEN_DO_NOT_CREATE is set).");
			}
		}

		if ((J9PORT_INFO_SHSEM_CREATED != rc) && (J9PORT_INFO_SHSEM_OPENED != rc)) {
			if (ControlFileCloseAndUnLock(portLibrary, fd) != J9SH_SUCCESS) {
				Trc_PRT_shsem_j9shsem_open_ExitWithMsg("Error: could not unlock semaphore control file.");
				omrmem_free_memory(tmphandle);
				return J9PORT_ERROR_SHSEM_OPFAILED;
			}
			if (J9_ARE_ANY_BITS_SET(flags, J9SHSEM_OPEN_FOR_STATS | J9SHSEM_OPEN_DO_NOT_CREATE)) {
				Trc_PRT_shsem_j9shsem_open_ExitWithMsg(exitmsg);
				omrmem_free_memory(tmphandle);
				return J9PORT_INFO_SHSEM_PARTIAL;
			}
			if (J9_ARE_ALL_BITS_SET(flags, J9SHSEM_OPEN_FOR_DESTROY)) {
				Trc_PRT_shsem_j9shsem_open_ExitWithMsg(exitmsg);
				/* If we get an error while opening semaphore and thereafter unlinked control file successfully,
				 * then we may have stored the semid in tmphandle to be used by caller for error reporting.
				 * Do not free tmphandle in such case. Caller is responsible for freeing it.
				 */
				if (TRUE == freeHandleOnError) {
					omrmem_free_memory(tmphandle);
				}
				if (J9PORT_ERROR_SHSEM_OPFAILED_DONT_UNLINK == rc) {
					rc = J9PORT_ERROR_SHSEM_OPFAILED;
				}
				return rc;
			}
			if (((isReadOnlyFD == TRUE) || (rc == J9PORT_INFO_SHSEM_OPEN_UNLINKED)) && (retryIfReadOnlyCount > 1)) {
				/*Try loop again ... we sleep first to mimic old retry loop*/
				if (rc != J9PORT_INFO_SHSEM_OPEN_UNLINKED) {
					Trc_PRT_shsem_j9shsem_open_Msg("Retry with usleep(100).");
					usleep(100);
				} else {
					Trc_PRT_shsem_j9shsem_open_Msg("Retry.");
				}
				continue;
			} else {
				if (retryIfReadOnlyCount <= 1) {
					Trc_PRT_shsem_j9shsem_open_Msg("Info no more retries.");
				}
				Trc_PRT_shsem_j9shsem_open_ExitWithMsg(exitmsg);
				omrmem_free_memory(tmphandle);
				if (J9PORT_ERROR_SHSEM_OPFAILED_DONT_UNLINK == rc) {
					rc = J9PORT_ERROR_SHSEM_OPFAILED;
				}
				/* Here 'rc' can be:
				 * 	- J9PORT_ERROR_SHSEM_OPFAILED (returned either by createSemaphore or openSemaphore)
				 * 	- J9PORT_ERROR_SHSEM_OPFAILED_CONTROL_FILE_CORRUPT
				 * 	- J9PORT_ERROR_SHSEM_OPFAILED_SEMID_MISMATCH (returned by openSemaphore)
				 * 	- J9PORT_ERROR_SHSEM_OPFAILED_SEM_KEY_MISMATCH (returned by openSemaphore)
				 * 	- J9PORT_ERROR_SHSEM_OPFAILED_SEM_SIZE_CHECK_FAILED (returned by openSemaphore)
				 * 	- J9PORT_ERROR_SHSEM_OPFAILED_SEM_MARKER_CHECK_FAILED (returned by openSemaphore)
				 *  - J9PORT_ERROR_SHSEM_OPFAILED_SEMAPHORE_NOT_FOUND ( returned by openSemaphore)
				 */
				return rc;
			}
		}

		/*If we hit here we break out of the loop*/
		break;
	}

	createsemHandle(portLibrary, controlinfo.semid, controlinfo.semsetSize, baseFile, tmphandle);

	if (rc == J9PORT_INFO_SHSEM_CREATED) {
		tmphandle->timestamp = omrfile_lastmod(baseFile);
	}
	if (ControlFileCloseAndUnLock(portLibrary, fd) != J9SH_SUCCESS) {
		Trc_PRT_shsem_j9shsem_open_ExitWithMsg("Error: could not unlock semaphore control file.");
		omrmem_free_memory(tmphandle);
		return J9PORT_ERROR_SHSEM_OPFAILED;
	}

	if (rc == J9PORT_INFO_SHSEM_CREATED) {
		Trc_PRT_shsem_j9shsem_open_ExitWithMsg("Successfully created a new semaphore.");
	} else {
		Trc_PRT_shsem_j9shsem_open_ExitWithMsg("Successfully opened an existing semaphore.");
	}
	*handle = tmphandle;
	return rc;
}

intptr_t 
j9shsem_deprecated_post(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset, uintptr_t flag)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	struct sembuf buffer;
	intptr_t rc;

	Trc_PRT_shsem_j9shsem_post_Entry1(handle, semset, flag, handle ? handle->semid : -1);
	if(handle == NULL) {
		Trc_PRT_shsem_j9shsem_post_Exit1();
		return J9PORT_ERROR_SHSEM_HANDLE_INVALID;
	}

	if(semset >= handle->nsems) {
		Trc_PRT_shsem_j9shsem_post_Exit2();
		return J9PORT_ERROR_SHSEM_SEMSET_INVALID;
	}

	buffer.sem_num = semset;
	buffer.sem_op = 1; /* post */
	if(flag & J9PORT_SHSEM_MODE_UNDO) {
		buffer.sem_flg = SEM_UNDO;
	} else {
		buffer.sem_flg = 0;
	}

	rc = semopWrapper(portLibrary,handle->semid, &buffer, 1);
	
	if (-1 == rc) {
		int32_t myerrno = omrerror_last_error_number();
		Trc_PRT_shsem_j9shsem_post_Exit3(rc, myerrno);
	} else {
		Trc_PRT_shsem_j9shsem_post_Exit(rc);
	}

	return rc;
}

intptr_t
j9shsem_deprecated_wait(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset, uintptr_t flag)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	struct sembuf buffer;
	intptr_t rc;

	Trc_PRT_shsem_j9shsem_wait_Entry1(handle, semset, flag, handle ? handle->semid : -1);
	if(handle == NULL) {
		Trc_PRT_shsem_j9shsem_wait_Exit1();
		return J9PORT_ERROR_SHSEM_HANDLE_INVALID;
	}

	if(semset >= handle->nsems) {
		Trc_PRT_shsem_j9shsem_wait_Exit2();
		return J9PORT_ERROR_SHSEM_SEMSET_INVALID;
	}

	buffer.sem_num = semset;
	buffer.sem_op = -1; /* wait */

	if( flag & J9PORT_SHSEM_MODE_UNDO ) {
		buffer.sem_flg = SEM_UNDO;
	} else {
		buffer.sem_flg = 0;
	}

	if( flag & J9PORT_SHSEM_MODE_NOWAIT ) {
		buffer.sem_flg = buffer.sem_flg | IPC_NOWAIT;
	}
    
	rc = semopWrapper(portLibrary,handle->semid, &buffer, 1);
	if (-1 == rc) {
		int32_t myerrno = omrerror_last_error_number();
		Trc_PRT_shsem_j9shsem_wait_Exit3(rc, myerrno);
	} else {
		Trc_PRT_shsem_j9shsem_wait_Exit(rc);
	}
	
	return rc;
}

intptr_t 
j9shsem_deprecated_getVal(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset) 
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t rc;
	Trc_PRT_shsem_j9shsem_getVal_Entry1(handle, semset, handle ? handle->semid : -1);
	if(handle == NULL) {
		Trc_PRT_shsem_j9shsem_getVal_Exit1();
		return J9PORT_ERROR_SHSEM_HANDLE_INVALID;
	}

	if(semset >= handle->nsems) {
		Trc_PRT_shsem_j9shsem_getVal_Exit2();
		return J9PORT_ERROR_SHSEM_SEMSET_INVALID; 
	}

	rc = semctlWrapper(portLibrary, TRUE, handle->semid, semset, GETVAL);
	if (-1 == rc) {
		int32_t myerrno = omrerror_last_error_number();
		Trc_PRT_shsem_j9shsem_getVal_Exit3(rc, myerrno);
	} else {
		Trc_PRT_shsem_j9shsem_getVal_Exit(rc);
	}
	return rc;
}

intptr_t 
j9shsem_deprecated_setVal(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset, intptr_t value)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	semctlUnion sem_union;
	
	intptr_t rc;
	
	Trc_PRT_shsem_j9shsem_setVal_Entry1(handle, semset, value, handle ? handle->semid : -1);

	if(handle == NULL) {
		Trc_PRT_shsem_j9shsem_setVal_Exit1();
		return J9PORT_ERROR_SHSEM_HANDLE_INVALID;
	}
	if(semset >= handle->nsems) {
		Trc_PRT_shsem_j9shsem_setVal_Exit2();
		return J9PORT_ERROR_SHSEM_SEMSET_INVALID; 
	}
    
	sem_union.val = value;
	
	rc = semctlWrapper(portLibrary, TRUE, handle->semid, semset, SETVAL, sem_union);

	if (-1 == rc) {
		int32_t myerrno = omrerror_last_error_number();
		Trc_PRT_shsem_j9shsem_setVal_Exit3(rc, myerrno);
	} else {
		Trc_PRT_shsem_j9shsem_setVal_Exit(rc);
	}
	return rc;
}

intptr_t
j9shsem_deprecated_handle_stat(struct J9PortLibrary *portLibrary, struct j9shsem_handle *handle, struct J9PortShsemStatistic *statbuf)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t rc = J9PORT_ERROR_SHSEM_STAT_FAILED;
	struct semid_ds seminfo;
	union semun semctlArg;

	Trc_PRT_shsem_j9shsem_deprecated_handle_stat_Entry1(handle, handle ? handle->semid : -1);

	clearPortableError(portLibrary);

	if (NULL == handle) {
		Trc_PRT_shsem_j9shsem_deprecated_handle_stat_ErrorNullHandle();
		rc = J9PORT_ERROR_SHSEM_HANDLE_INVALID;
		goto _end;
	}

	if (NULL == statbuf) {
		Trc_PRT_shsem_j9shsem_deprecated_handle_stat_ErrorNullBuffer();
		rc = J9PORT_ERROR_SHSEM_STAT_BUFFER_INVALID;
		goto _end;
	} else {
		initShsemStatsBuffer(portLibrary, statbuf);
	}

	semctlArg.buf = &seminfo;
	rc = semctlWrapper(portLibrary, TRUE, handle->semid, 0, IPC_STAT, semctlArg);
	if (J9SH_FAILED == rc) {
		int32_t lasterrno = omrerror_last_error_number() | J9PORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
		const char *errormsg = omrerror_last_error_message();
		Trc_PRT_shsem_j9shsem_deprecated_handle_stat_semctlFailed(handle->semid, lasterrno, errormsg);
		rc = J9PORT_ERROR_SHSEM_STAT_FAILED;
		goto _end;
	}

	statbuf->semid = handle->semid;
	statbuf->ouid = seminfo.sem_perm.uid;
	statbuf->ogid = seminfo.sem_perm.gid;
	statbuf->cuid = seminfo.sem_perm.cuid;
	statbuf->cgid = seminfo.sem_perm.cgid;
	statbuf->lastOpTime = seminfo.sem_otime;
	statbuf->lastChangeTime = seminfo.sem_ctime;
	statbuf->nsems = seminfo.sem_nsems;

	/* Same code as in j9file_stat(). Probably a new function to translate operating system modes to J9 modes is required. */
	if (seminfo.sem_perm.mode & S_IWUSR) {
		statbuf->perm.isUserWriteable = 1;
	}
	if (seminfo.sem_perm.mode & S_IRUSR) {
		statbuf->perm.isUserReadable = 1;
	}
	if (seminfo.sem_perm.mode & S_IWGRP) {
		statbuf->perm.isGroupWriteable = 1;
	}
	if (seminfo.sem_perm.mode & S_IRGRP) {
		statbuf->perm.isGroupReadable = 1;
	}
	if (seminfo.sem_perm.mode & S_IWOTH) {
		statbuf->perm.isOtherWriteable = 1;
	}
	if (seminfo.sem_perm.mode & S_IROTH) {
		statbuf->perm.isOtherReadable = 1;
	}

	rc = J9PORT_INFO_SHSEM_STAT_PASSED;

_end:
	Trc_PRT_shsem_j9shsem_deprecated_handle_stat_Exit(rc);
	return rc;
}

void 
j9shsem_deprecated_close (struct J9PortLibrary *portLibrary, struct j9shsem_handle **handle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	Trc_PRT_shsem_j9shsem_close_Entry1(*handle, (*handle) ? (*handle)->semid : -1);

	/* On Unix you don't need to close the semaphore handles*/
	if (NULL == *handle) {
		Trc_PRT_shsem_j9shsem_close_ExitNullHandle();
		return;
	}
	omrmem_free_memory(*handle);
	*handle=NULL;

	Trc_PRT_shsem_j9shsem_close_Exit();
}

intptr_t 
j9shsem_deprecated_destroy (struct J9PortLibrary *portLibrary, struct j9shsem_handle **handle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	/*pre: user has not closed the handle, and assume user has perm to remove */
	intptr_t rc = -1;
	intptr_t lockFile, rcunlink;
	intptr_t fd;
	BOOLEAN isReadOnlyFD;
	j9shsem_baseFileFormat controlinfo;

	Trc_PRT_shsem_j9shsem_destroy_Entry1(*handle, (*handle) ? (*handle)->semid : -1);

	if (*handle == NULL) {
		Trc_PRT_shsem_j9shsem_destroy_ExitNullHandle();
		return 0;
	}

	lockFile = ControlFileOpenWithWriteLock(portLibrary, &fd, &isReadOnlyFD, FALSE, (*handle)->baseFile, 0);
	if (lockFile == J9SH_FILE_DOES_NOT_EXIST) {
		Trc_PRT_shsem_j9shsem_destroy_Msg("Error: control file not found");
		if (semctlWrapper(portLibrary, TRUE, (*handle)->semid, 0, IPC_RMID, 0) == -1) {
			int32_t lastError = omrerror_last_error_number() | J9PORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
			if ((J9PORT_ERROR_SYSV_IPC_ERRNO_EINVAL == lastError) || (J9PORT_ERROR_SYSV_IPC_ERRNO_EIDRM == lastError)) {
				Trc_PRT_shsem_j9shsem_destroy_Msg("SysV obj is already deleted");
				rc = 0;
			} else {
				Trc_PRT_shsem_j9shsem_destroy_Msg("Error: could not delete SysV obj");
				rc = -1;
			}
		} else {
			Trc_PRT_shsem_j9shsem_destroy_Msg("Deleted SysV obj");
			rc = 0;
		}
		goto j9shsem_deprecated_destroy_exitWithRC;
	} else if (lockFile != J9SH_SUCCESS) {
		Trc_PRT_shsem_j9shsem_destroy_Msg("Error: could not open and lock control file");
		goto j9shsem_deprecated_destroy_exitWithError;
	}

	/*Try to read data from the file*/
	rc = omrfile_read(fd, &controlinfo, sizeof(j9shsem_baseFileFormat));
	if (rc != sizeof(j9shsem_baseFileFormat)) {
		Trc_PRT_shsem_j9shsem_destroy_Msg("Error: can not read control file");
		goto j9shsem_deprecated_destroy_exitWithErrorAndUnlock;
	}

	/* check that the modlevel and version is okay */
	if (J9SH_GET_MOD_MAJOR_LEVEL(controlinfo.modlevel) != J9SH_MOD_MAJOR_LEVEL) {
		Trc_PRT_shsem_j9shsem_destroy_BadMajorModlevel(controlinfo.modlevel, J9SH_MODLEVEL);
		goto j9shsem_deprecated_destroy_exitWithErrorAndUnlock;
	}

	if (controlinfo.semid == (*handle)->semid) {
		if (semctlWrapper(portLibrary, TRUE, (*handle)->semid, 0, IPC_RMID, 0) == -1) {
			Trc_PRT_shsem_j9shsem_destroy_Debug2((*handle)->semid, omrerror_last_error_number());
			rc = -1;
		} else {
			rc = 0;
		}
	}

	if (0 == rc) {
		/* PR 74306: Allow unlinking of readonly control file if it was created by an older JVM without this fix 
		 * which is identified by different modlevel.
		 */
		if ((FALSE == isReadOnlyFD)
			|| ((J9SH_GET_MOD_MAJOR_LEVEL(controlinfo.modlevel) == J9SH_MOD_MAJOR_LEVEL_ALLOW_READONLY_UNLINK)
			&& (J9SH_SEM_GET_MOD_MINOR_LEVEL(controlinfo.modlevel) <= J9SH_MOD_MINOR_LEVEL_ALLOW_READONLY_UNLINK))
		) {
			rcunlink = omrfile_unlink((*handle)->baseFile);
			Trc_PRT_shsem_j9shsem_destroy_Debug1((*handle)->baseFile, rcunlink, omrerror_last_error_number());
		}
	}

	j9shsem_deprecated_close(portLibrary, handle);

	if (ControlFileCloseAndUnLock(portLibrary, fd) != J9SH_SUCCESS) {
		Trc_PRT_shsem_j9shsem_destroy_Msg("Error: failed to unlock control file.");
		goto j9shsem_deprecated_destroy_exitWithError;
	}

	if (0 == rc) {
		Trc_PRT_shsem_j9shsem_destroy_Exit();
	} else {
		Trc_PRT_shsem_j9shsem_destroy_Exit1();
	}
	
j9shsem_deprecated_destroy_exitWithRC:
	return rc;

j9shsem_deprecated_destroy_exitWithErrorAndUnlock:
	if (ControlFileCloseAndUnLock(portLibrary, fd) != J9SH_SUCCESS) {
		Trc_PRT_shsem_j9shsem_destroy_Msg("Error: failed to unlock control file (after version check fail).");
	}
j9shsem_deprecated_destroy_exitWithError:
	Trc_PRT_shsem_j9shsem_destroy_Exit1();
	return -1;

}

intptr_t 
j9shsem_deprecated_destroyDeprecated (struct J9PortLibrary *portLibrary, struct j9shsem_handle **handle, uintptr_t cacheFileType)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t retval = -1;
	intptr_t rcunlink;
	intptr_t fd;
	BOOLEAN isReadOnlyFD = FALSE;
	BOOLEAN fileIsLocked = FALSE;
	
	Trc_PRT_shsem_deprecated_destroyDeprecated_Entry(*handle, (*handle)->semid);
	
	if (cacheFileType == J9SH_SYSV_OLDER_EMPTY_CONTROL_FILE) {
		Trc_PRT_shsem_deprecated_destroyDeprecated_Message("Info: cacheFileType == J9SH_SYSV_OLDER_EMPTY_CONTROL_FILE.");
		if (ControlFileOpenWithWriteLock(portLibrary, &fd, &isReadOnlyFD, FALSE, (*handle)->baseFile, 0) != J9SH_SUCCESS) {
			Trc_PRT_shsem_deprecated_destroyDeprecated_ExitWithMessage("Error: could not lock semaphore control file.");
			goto error;
		} else {
			fileIsLocked = TRUE;
		}
		if (semctlWrapper(portLibrary, TRUE, (*handle)->semid, 0, IPC_RMID, 0) == -1) {
			Trc_PRT_shsem_deprecated_destroyDeprecated_ExitWithMessage("Error: failed to remove SysV object.");
			goto error;
		}

		if (0 == omrfile_unlink((*handle)->baseFile)) {
			Trc_PRT_shsem_deprecated_destroyDeprecated_Message("Unlinked control file");
		} else {
			Trc_PRT_shsem_deprecated_destroyDeprecated_Message("Failed to unlink control file");
		}

		j9shsem_deprecated_close(portLibrary, handle);
		retval = 0;



	error:
		if (fileIsLocked == TRUE) {
			if (ControlFileCloseAndUnLock(portLibrary, fd) != J9SH_SUCCESS) {
				Trc_PRT_shsem_deprecated_destroyDeprecated_Message("Error: could not unlock semaphore control file.");
				retval = -1;
			}
		}
	} else if (cacheFileType == J9SH_SYSV_OLDER_CONTROL_FILE) {
		Trc_PRT_shsem_deprecated_destroyDeprecated_Message("Info: cacheFileType == J9SH_SYSV_OLDER_CONTROL_FILE.");
		retval = j9shsem_deprecated_destroy(portLibrary, handle);
	} else {
		Trc_PRT_shsem_deprecated_destroyDeprecated_BadCacheType(cacheFileType);
	}
	
	Trc_PRT_shsem_deprecated_destroyDeprecated_ExitWithMessage("Exit");
	return retval;
}

int32_t
j9shsem_deprecated_startup(struct J9PortLibrary *portLibrary)
{
	return 0;
}

void
j9shsem_deprecated_shutdown(struct J9PortLibrary *portLibrary)
{
	/* Don't need to do anything for now, but maybe we will need to clean up
	 * some directories, etc over here.
	 */
}

static intptr_t 
readOlderNonZeroByteControlFile(J9PortLibrary *portLibrary, intptr_t fd, j9shsem_baseFileFormat * info)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t rc;
	rc = omrfile_read(fd, info, sizeof(j9shsem_baseFileFormat));
	if (rc < 0) {
		return J9SH_FAILED;/*READ_ERROR;*/
	} else if (rc == 0) {
		return J9SH_FAILED;/*J9SH_NO_DATA;*/
	} else if (rc < sizeof(j9shsem_baseFileFormat)) {
		return J9SH_FAILED;/*J9SH_BAD_DATA;*/
	}
	return J9SH_SUCCESS;
}

/**
 * @internal
 * Create a new SysV semaphore
 *
 * @param[in] portLibrary
 * @param[in] fd read/write file descriptor 
 * @param[in] isReadOnlyFD set to TRUE if control file is read-only, FALSE otherwise
 * @param[in] baseFile filename matching fd
 * @param[in] setSize size of semaphore set
 * @param[out] controlinfo information about the new sysv object
 * @param[in] groupPerm group permission of the semaphore
 *
 * @return	J9PORT_INFO_SHSEM_CREATED on success
 */
static intptr_t
createSemaphore(struct J9PortLibrary *portLibrary, intptr_t fd, BOOLEAN isReadOnlyFD, char *baseFile, int32_t setSize, j9shsem_baseFileFormat* controlinfo, uintptr_t groupPerm)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	union semun sem_union;
	intptr_t i = 0;
	intptr_t j = 0;
	intptr_t projId = SHSEM_FTOK_PROJID;
	intptr_t maxProjIdRetries = 20;
	int semflags = groupPerm ? J9SHSEM_SEMFLAGS_GROUP : J9SHSEM_SEMFLAGS;

	Trc_PRT_shsem_j9shsem_createsemaphore_EnterWithMessage("start");

	if (isReadOnlyFD == TRUE) {
		Trc_PRT_shsem_j9shsem_createsemaphore_ExitWithMessage("Error: can not write control file b/c file descriptor is opened read only.");
		return J9PORT_ERROR_SHSEM_OPFAILED;
	}

	if (controlinfo == NULL) {
		Trc_PRT_shsem_j9shsem_createsemaphore_ExitWithMessage("Error: controlinfo == NULL");
		return J9PORT_ERROR_SHSEM_OPFAILED;
	}

	for (i = 0; i < maxProjIdRetries; i += 1, projId += 1) {
		int semid = -1;
		key_t fkey = -1;
		intptr_t rc = 0;

		fkey = ftokWrapper(portLibrary, baseFile, projId);
		if (-1 == fkey) {
			int32_t lastError = omrerror_last_error_number() | J9PORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
			if (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_ENOENT) {
				Trc_PRT_shsem_j9shsem_createsemaphore_Msg("ftok: ENOENT");
			} else if (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EACCES) {
				Trc_PRT_shsem_j9shsem_createsemaphore_Msg("ftok: EACCES");
			} else {
				/*By this point other errno's should not happen*/
				Trc_PRT_shsem_j9shsem_createsemaphore_MsgWithError("ftok: unexpected errno, portable errorCode = ", lastError);
			}
			Trc_PRT_shsem_j9shsem_createsemaphore_ExitWithMessage("Error: fkey == -1");
			return J9PORT_ERROR_SHSEM_OPFAILED;
		}

		semid = semgetWrapper(portLibrary, fkey, setSize + 1, semflags);
		if (-1 == semid) {
			int32_t lastError = omrerror_last_error_number() | J9PORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
			if (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EEXIST || lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EACCES) {
				Trc_PRT_shsem_j9shsem_createsemaphore_Msg("Retry (errno == EEXIST || errno == EACCES) was found.");
				continue;
			}
			Trc_PRT_shsem_j9shsem_createsemaphore_ExitWithMessageAndError("Error: semid = -1 with portable errorCode = ", lastError);
			return J9PORT_ERROR_SHSEM_OPFAILED;
		}

		controlinfo->version = J9SH_VERSION;
		controlinfo->modlevel = J9SH_MODLEVEL;
		controlinfo->proj_id = projId;
		controlinfo->ftok_key = fkey;
		controlinfo->semid = semid;
		controlinfo->creator_pid = omrsysinfo_get_pid();
		controlinfo->semsetSize = setSize;

		if (-1 == omrfile_seek(fd, 0, EsSeekSet)) {
			Trc_PRT_shsem_j9shsem_createsemaphore_ExitWithMessage("Error: could not seek to start of control file");
			/* Do not set error code if semctl fails, else j9file_seek() error code will be lost. */
			if (semctlWrapper(portLibrary, FALSE, semid, 0, IPC_RMID, 0) == -1) {
				/*Already in an error condition ... don't worry about failure here*/
				return J9PORT_ERROR_SHSEM_OPFAILED;
			}
			return J9PORT_ERROR_SHSEM_OPFAILED;
		}

		rc = omrfile_write(fd, controlinfo, sizeof(j9shsem_baseFileFormat));
		if (rc != sizeof(j9shsem_baseFileFormat)) {
			Trc_PRT_shsem_j9shsem_createsemaphore_ExitWithMessage("Error: write to control file failed");
			/* Do not set error code if semctl fails, else j9file_write() error code will be lost. */
			if (semctlWrapper(portLibrary, FALSE, semid, 0, IPC_RMID, 0) == -1) {
				/*Already in an error condition ... don't worry about failure here*/
				return J9PORT_ERROR_SHSEM_OPFAILED;
			}
			return J9PORT_ERROR_SHSEM_OPFAILED;
		}

		for (j=0; j<setSize; j++) {
			struct j9shsem_handle tmphandle;
			tmphandle.semid=semid;
			tmphandle.nsems = setSize;
			tmphandle.baseFile = baseFile;
			if (portLibrary->shsem_deprecated_post(portLibrary, &tmphandle, j, J9PORT_SHSEM_MODE_DEFAULT) != 0) {
				/* The code should never ever fail here b/c we just created, and obviously own the new semaphore.
				 * If semctl call below fails we are already in an error condition, so 
				 * don't worry about failure here.
				 * Do not set error code if semctl fails, else j9shsem_deprecated_post() error code will be lost.
				 */
				semctlWrapper(portLibrary, FALSE, semid, 0, IPC_RMID, 0);
				return J9PORT_ERROR_SHSEM_OPFAILED;
			}
		}
			
		sem_union.val = SEMMARKER_INITIALIZED;
		if (semctlWrapper(portLibrary, TRUE, semid, setSize, SETVAL, sem_union) == -1) {
			Trc_PRT_shsem_j9shsem_createsemaphore_ExitWithMessage("Could not mark semaphore as initialized initialized.");
			/* The code should never ever fail here b/c we just created, and obviously own the new semaphore.
			 * If semctl call below fails we are already in an error condition, so 
			 * don't worry about failure here.
			 * Do not set error code if the next semctl fails, else the previous semctl error code will be lost.
			 */
			semctlWrapper(portLibrary, FALSE, semid, 0, IPC_RMID, 0);
			return J9PORT_ERROR_SHSEM_OPFAILED;
		}
		Trc_PRT_shsem_j9shsem_createsemaphore_ExitWithMessage("Successfully created semaphore");
		return J9PORT_INFO_SHSEM_CREATED;
	}

	Trc_PRT_shsem_j9shsem_createsemaphore_ExitWithMessage("Error: end of retries. Clean up SysV using 'ipcs'");
	return J9PORT_ERROR_SHSEM_OPFAILED;
}

/**
 * @internal
 * Open a new SysV semaphore
 *
 * @param[in] portLibrary
 * @param[in] fd read/write file descriptor 
 * @param[in] baseFile filename matching fd
 * @param[in] controlinfo copy of information in control file
 * @param[in] groupPerm the group permission of the semaphore
 * @param[in] cacheFileType type of control file (regular or older or older-empty)
 *
 * @return	J9PORT_INFO_SHSEM_OPENED on success
 */
static intptr_t
openSemaphore(struct J9PortLibrary *portLibrary, intptr_t fd, char *baseFile, j9shsem_baseFileFormat* controlinfo, uintptr_t groupPerm, uintptr_t cacheFileType)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	/*The control file contains data, and we have successfully read in our structure*/
	int semid = -1;
	int semflags = groupPerm ? J9SHSEM_SEMFLAGS_GROUP_OPEN : J9SHSEM_SEMFLAGS_OPEN;
	intptr_t rc = -1;
#if defined (J9ZOS390)
	IPCQPROC info;
#endif
	union semun semctlArg;
	struct semid_ds buf;
	semctlArg.buf = &buf;

	Trc_PRT_shsem_j9shsem_opensemaphore_EnterWithMessage("Start");

	/* openSemaphore can only open semaphores with the same major level */
	if ((J9SH_SYSV_REGULAR_CONTROL_FILE == cacheFileType) &&
		(J9SH_GET_MOD_MAJOR_LEVEL(controlinfo->modlevel) != J9SH_MOD_MAJOR_LEVEL)
	) {
		Trc_PRT_shsem_j9shsem_opensemaphore_ExitWithMessage("Error: major modlevel mismatch.");
		/* Clear any stale portlibrary error code */
		clearPortableError(portLibrary);
		goto failDontUnlink;
	}

	semid = semgetWrapper(portLibrary, controlinfo->ftok_key, 0/*don't care*/, semflags);
	if (-1 == semid) {
		int32_t lastError = omrerror_last_error_number() | J9PORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
		if ((lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_ENOENT) || (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EIDRM)) {
			/*The semaphore set was deleted, but the control file still exists*/
			Trc_PRT_shsem_j9shsem_opensemaphore_ExitWithMessage("The shared SysV obj was deleted, but the control file still exists.");
			rc = J9PORT_ERROR_SHSEM_OPFAILED_SEMAPHORE_NOT_FOUND;
			goto fail;
		} else if (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EACCES) {
			Trc_PRT_shsem_j9shsem_opensemaphore_ExitWithMessage("Info: EACCES occurred.");
		} else {
			/*Any other errno will result is an error*/
			Trc_PRT_shsem_j9shsem_opensemaphore_MsgWithError("Error: can not open shared semaphore (semget failed), portable errorCode = ", lastError);
			goto failDontUnlink;
		}
	}

	do {

		if (semid != -1 && semid != controlinfo->semid) {
			/*semid does not match value in the control file.*/
			Trc_PRT_shsem_j9shsem_opensemaphore_Msg("The SysV id does not match our control file.");
			/* Clear any stale portlibrary error code */
			clearPortableError(portLibrary);
			rc = J9PORT_ERROR_SHSEM_OPFAILED_SEMID_MISMATCH;
			goto fail;
		}

		/*If our sysv obj id is gone, or our <key, id> pair is invalid, then we can also recover*/
		rc = semctlWrapper(portLibrary, TRUE, controlinfo->semid, 0, IPC_STAT, semctlArg);
		if (-1 == rc) {
			int32_t lastError = omrerror_last_error_number() | J9PORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
			if ((lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EINVAL) || (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EIDRM)) {
				/*the sysv obj may have been removed just before, or during, our sXmctl call. In this case our key is free to be reused.*/
				Trc_PRT_shsem_j9shsem_opensemaphore_Msg("The shared SysV obj was deleted, but the control file still exists.");
				rc = J9PORT_ERROR_SHSEM_OPFAILED_SEMAPHORE_NOT_FOUND;
				goto fail;
			} else {
				if ((lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EACCES) && (semid != -1)) {
					Trc_PRT_shsem_j9shsem_opensemaphore_Msg("The SysV obj may have been modified since the call to sXmget.");
					rc = J9PORT_ERROR_SHSEM_OPFAILED;
					goto fail;
				}

				/*If sXmctl fails our checks below will also fail (they use the same function) ... so we terminate with an error*/
				Trc_PRT_shsem_j9shsem_opensemaphore_MsgWithError("Error: semctl failed. Can not open shared semaphore, portable errorCode = ", lastError);
				goto failDontUnlink;
			}
		} else {
#if defined(__GNUC__) || defined(AIXPPC) || defined(J9ZTPF)
#if defined(OSX)
			/*Use _key for OSX*/
			if (buf.sem_perm._key != controlinfo->ftok_key)
#elif defined(AIXPPC)
			/*Use .key for AIXPPC*/
			if (buf.sem_perm.key != controlinfo->ftok_key)
#elif defined(J9ZTPF)
			/*Use .key for z/TPF */
			if (buf.key != controlinfo->ftok_key)
#elif defined(__GNUC__)
			/*Use .__key for __GNUC__*/
			if (buf.sem_perm.__key != controlinfo->ftok_key)
#endif
			{
#if defined (J9OS_I5)
 				/* The statement 'buf.sem_perm.key != controlinfo->common.ftok_key' will never fail on IBM i platform,
 				 * as the definition of structure ipc_perm is different:
 				 *  'key' field doesn't exists in structure ipc_perm on IBM i and the value of 'key' always zero.
 				 *  Simply log the error here, and then ignore it to avoid more incorrect messages
 				 */
 				Trc_PRT_shsem_j9shsem_opensemaphore_Msg("The <key,id> pair in our control file is not valid, but we ignore it here to avoid more incorrect messages.");
#else /* defined (J9OS_I5) */
				Trc_PRT_shsem_j9shsem_opensemaphore_Msg("The <key,id> pair in our control file is no longer valid.");
				/* Clear any stale portlibrary error code */
				clearPortableError(portLibrary);
				rc = J9PORT_ERROR_SHSEM_OPFAILED_SEM_KEY_MISMATCH;
				goto fail;
#endif /* defined (J9OS_I5) */
			}
#endif
#if defined (J9ZOS390)
			if (getipcWrapper(portLibrary, controlinfo->semid, &info, sizeof(IPCQPROC), IPCQALL) == -1) {
				int32_t lastError = omrerror_last_error_number() | J9PORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
				if (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EINVAL) {
					Trc_PRT_shsem_j9shsem_opensemaphore_Msg("__getipc(): The shared SysV obj was deleted, but the control file still exists.");
					rc = J9PORT_ERROR_SHSEM_OPFAILED_SEMAPHORE_NOT_FOUND;
					goto fail;
				} else {
					if ((lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EACCES) && (semid != -1)) {
						Trc_PRT_shsem_j9shsem_opensemaphore_Msg("__getipc(): has returned EACCES.");
						rc = J9PORT_ERROR_SHSEM_OPFAILED;
						goto fail;
					}/*Any other error and we will terminate*/

					/*If sXmctl fails our checks below will also fail (they use the same function) ... so we terminate with an error*/
					Trc_PRT_shsem_j9shsem_opensemaphore_MsgWithError("Error: __getipc() failed. Can not open shared shared semaphore, portable errorCode = ", lastError);
					goto failDontUnlink;
				}
			} else {
				if (info.sem.ipcqkey != controlinfo->ftok_key) {
					Trc_PRT_shsem_j9shsem_opensemaphore_Msg("The <key,id> pair in our control file is no longer valid.");
					rc = J9PORT_ERROR_SHSEM_OPFAILED_SEM_KEY_MISMATCH;
					goto fail;
				}
			}
#endif
			/*Things are looking good so far ...  continue on and do other checks*/
		}

		rc = checkSize(portLibrary, controlinfo->semid, controlinfo->semsetSize);
		if (rc == 0) {
			Trc_PRT_shsem_j9shsem_opensemaphore_Msg("The size does not match our control file.");
			/* Clear any stale portlibrary error code */
			clearPortableError(portLibrary);
			rc = J9PORT_ERROR_SHSEM_OPFAILED_SEM_SIZE_CHECK_FAILED;
			goto fail;
		} else if (rc == -1) {
			Trc_PRT_shsem_j9shsem_opensemaphore_ExitWithMessage("Error: checkSize failed during semctl.");
			goto failDontUnlink;
		} /*Continue looking for opportunity to re-create*/

		rc = checkMarker(portLibrary, controlinfo->semid, controlinfo->semsetSize);
		if (rc == 0) {
			Trc_PRT_shsem_j9shsem_opensemaphore_Msg("The marker semaphore does not match our expected value.");
			/* Clear any stale portlibrary error code */
			clearPortableError(portLibrary);
			rc = J9PORT_ERROR_SHSEM_OPFAILED_SEM_MARKER_CHECK_FAILED;
			goto fail;
		} else if (rc == -1) {
			Trc_PRT_shsem_j9shsem_opensemaphore_ExitWithMessage("Error: checkMarker failed during semctl.");
			goto failDontUnlink;
		}
		/*No more checks ... We want this loop to exec only once ... so we break ...*/
	} while (FALSE);

	if (-1 == semid) {
		goto failDontUnlink;
	}
	
pass:
	Trc_PRT_shsem_j9shsem_opensemaphore_ExitWithMessage("Successfully opened semaphore.");
	return J9PORT_INFO_SHSEM_OPENED;
fail:
	Trc_PRT_shsem_j9shsem_opensemaphore_ExitWithMessage("Error: can not open shared semaphore (semget failed). Caller MAY unlink the control file");
	return rc;
failDontUnlink:
	Trc_PRT_shsem_j9shsem_opensemaphore_ExitWithMessage("Error: can not open shared semaphore (semget failed). Caller should not unlink the control file");
	return J9PORT_ERROR_SHSEM_OPFAILED_DONT_UNLINK;
}

/**
 * @internal
 * Create a semaphore handle for j9shsem_open() caller ...
 *
 * @param[in] portLibrary
 * @param[in] semid id of SysV object
 * @param[in] nsems number of semaphores in the semaphore set
 * @param[in] baseFile file being used for control file
 * @param[out] handle the handle obj
 *
 * @return	void
 */
static void
createsemHandle(struct J9PortLibrary *portLibrary, int semid, int nsems, char* baseFile, j9shsem_handle * handle) 
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t baseFileLength = strlen(baseFile) + 1;

	Trc_PRT_shsem_createsemHandle_Entry(baseFile, semid, nsems);

	handle->semid = semid;
	handle->nsems = nsems;

	omrstr_printf(handle->baseFile, baseFileLength, "%s", baseFile);

	Trc_PRT_shsem_createsemHandle_Exit(J9SH_SUCCESS);
}

/**
 * @internal
 * Check for SEMMARKER at the last semaphore in the set 
 *
 * @param[in] portLibrary The port library
 * @param[in] semid semaphore set id
 * @param[in] semsetsize last semaphore in set
 *
 * @return	-1 if we can not get the value of the semaphore, 1 if it matches SEMMARKER, 0 if it does not match
 */
static intptr_t 
checkMarker(J9PortLibrary *portLibrary, int semid, int semsetsize)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t rc;
	rc = semctlWrapper(portLibrary, TRUE, semid, semsetsize, GETVAL);
	if (-1 == rc) {
		int32_t lastError = omrerror_last_error_number() | J9PORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
		if (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EINVAL) {
			return 0;
		} else if (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EIDRM) {
			return 0;
		}
		return -1;
	}

	if (rc == SEMMARKER_INITIALIZED) {
		return 1;
	} else {
		return 0;
	}
}

/**
 * @internal
 * Check for size of the semaphore set
 *
 * @param[in] portLibrary The port library
 * @param[in] semid semaphore set id
 * @param[in] numsems
 *
 * @return	-1 if we can get info from semid, 1 if it match is found, 0 if it does not match
 */
static intptr_t 
checkSize(J9PortLibrary *portLibrary, int semid, int32_t numsems)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t rc;
	union semun semctlArg;
	struct semid_ds buf;

	semctlArg.buf = &buf;

	rc = semctlWrapper(portLibrary, TRUE, semid, 0, IPC_STAT, semctlArg);
	if (-1 == rc) {
		int32_t lastError = omrerror_last_error_number() | J9PORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
		if (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EINVAL) {
			return 0;
		} else if (lastError == J9PORT_ERROR_SYSV_IPC_ERRNO_EIDRM) {
			return 0;
		}
		return -1;
	}

	if (buf.sem_nsems == (numsems + 1)) {
		return 1;
	} else {
		return 0;
	}
}

/**
 * @internal
 * Initialize J9PortShsemStatistic with default values.
 *
 * @param[in] portLibrary
 * @param[out] statbuf Pointer to J9PortShsemStatistic to be initialized
 *
 * @return void
 */
static void
initShsemStatsBuffer(J9PortLibrary *portLibrary, J9PortShsemStatistic *statbuf)
{
	if (NULL != statbuf) {
		memset(statbuf, 0, sizeof(J9PortShsemStatistic));
	}
}

int32_t 
j9shsem_deprecated_getid (struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle) 
{
	return handle->semid;
}


