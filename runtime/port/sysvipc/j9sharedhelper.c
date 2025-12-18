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

#include "j9sharedhelper.h"
#include "j9port.h"
#include "portpriv.h"
#include "ut_j9prt.h"
#include "portnls.h"
#include "j9portpg.h"
#include <errno.h>

#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <string.h>
#include <memory.h>
#include <fcntl.h>

#if defined(J9ZOS390)
#include "atoe.h"
#include "spawn.h"
#include "sys/wait.h"
#endif

#if defined(AIXPPC)
#include "protect_helpers.h"
#endif

#if defined(J9OS_I5)
#include "as400_types.h"
#include "as400_protos.h"
#endif

#define J9VERSION_NOT_IN_DEFAULT_DIR_MASK 0x10000000

typedef struct Dummy_OSCache_header {
	char eyecatcher[J9PORT_SHMEM_EYECATCHER_LENGTH+1]; /* "J9SC" */
	uintptr_t version;
} Dummy_OSCache_header;

#if defined(AIXPPC)
typedef struct protectedWriteInfo {
	char *writeAddress;
	uintptr_t length;
} protectedWriteInfo;
#endif

#if !defined(J9ZOS390)
#define __errno2() 0
#endif

static BOOLEAN IsFileReadWrite(struct J9FileStat * statbuf);

void
clearPortableError(J9PortLibrary *portLibrary) {
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	omrerror_set_last_error(0, 0);
}

intptr_t
changeDirectoryPermission(struct J9PortLibrary *portLibrary, const char* pathname, uintptr_t permission)
{
	struct stat statbuf;
	Trc_PRT_shared_changeDirectoryPermission_Entry( pathname, permission);
	if (J9SH_DIRPERM_DEFAULT != permission) {
		/* check if sticky bit is to be enabled */
		if (J9SH_DIRPERM_DEFAULT_WITH_STICKYBIT == permission) {
			if (stat(pathname, &statbuf) == -1) {
#if defined(ENOENT) && defined(ENOTDIR)
				if (ENOENT == errno || ENOTDIR == errno) {
					Trc_PRT_shared_changeDirectoryPermission_Exit3(errno);
					return J9SH_FILE_DOES_NOT_EXIST;
				} else {
#endif
					Trc_PRT_shared_changeDirectoryPermission_Exit4(errno);
					return J9SH_FAILED;
#if defined(ENOENT) && defined(ENOTDIR)
				}
#endif
			} else {
				permission |= statbuf.st_mode;
			}
		}
		if(-1 == chmod(pathname, permission)) {
#if defined(ENOENT) && defined(ENOTDIR)
			if (ENOENT == errno || ENOTDIR == errno) {
				Trc_PRT_shared_changeDirectoryPermission_Exit2(errno);
				return J9SH_FILE_DOES_NOT_EXIST;
			} else {
#endif
				Trc_PRT_shared_changeDirectoryPermission_Exit1(errno);
				return J9SH_FAILED;
#if defined(ENOENT) && defined(ENOTDIR)
			}
#endif
		} else {
			Trc_PRT_shared_changeDirectoryPermission_Exit();
			return J9SH_SUCCESS;
		}
	} else {
		Trc_PRT_shared_changeDirectoryPermission_Exit();
		return J9SH_SUCCESS;
	}
}

intptr_t
createDirectory(struct J9PortLibrary *portLibrary, const char *pathname, uintptr_t permission)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	char tempPath[J9SH_MAXPATH];
	char *current = NULL;
	intptr_t rc = 0;
	BOOLEAN usingUserHome = FALSE;

	Trc_PRT_shared_createDirectory_Entry(pathname);

	if (0 == omrfile_mkdir(pathname)) {
		Trc_PRT_shared_createDirectory_Exit();
		return J9SH_SUCCESS;
	} else if (J9PORT_ERROR_FILE_EXIST == omrerror_last_error_number()) {
		Trc_PRT_shared_createDirectory_Exit1();
		return J9SH_SUCCESS;
	}

	omrstr_printf(tempPath, J9SH_MAXPATH, "%s", pathname);

	/* If pathname begins with the home directory, mark usingUserHome as true. */
	if ((NULL != pathname) && (J9SH_DIRPERM_ABSENT == permission)) {
		char homeDir[J9SH_MAXPATH];
		homeDir[0] = '\0';
		if (0 == j9shmem_getDir(
			portLibrary,
			NULL,
			J9SHMEM_GETDIR_USE_USERHOME | J9SHMEM_GETDIR_DO_NOT_APPEND_HIDDENDIR,
			homeDir,
			J9SH_MAXPATH)
		) {
			if (0 == strncmp(pathname, homeDir, strlen(homeDir))) {
				usingUserHome = TRUE;
			}
		}
	}

	if ((J9SH_DIRPERM_ABSENT == permission)
		|| (J9SH_DIRPERM_ABSENT_GROUPACCESS == permission)
	) {
		if ((J9SH_DIRPERM_ABSENT == permission) && usingUserHome) {
			permission = J9SH_DIRPERM_HOME;
		} else {
			permission = J9SH_PARENTDIRPERM;
		}
	}

	current = strchr(tempPath + 1, DIR_SEPARATOR); /* skip the first '/' */

	while ((NULL != current) && (EsIsDir != omrfile_attr(pathname))) {
		char *previous = NULL;

		*current = '\0';

#if defined(J9SHSEM_DEBUG)
		portLibrary->tty_printf(portLibrary, "mkdir %s\n", tempPath);
#endif

		if (0 == omrfile_mkdir(tempPath)) {
			Trc_PRT_shared_createDirectory_Event1(tempPath);
			/* Change permission on new dir, note that for last dir in path
			 * this will be changed in ensureDirectory().
			 */
			if (J9SH_SUCCESS != changeDirectoryPermission(portLibrary, tempPath, permission)) {
				Trc_PRT_shared_createDirectory_Exit4(tempPath);
				return J9SH_FAILED;
			}
		} else if (EsIsDir != omrfile_attr(tempPath)) {
			Trc_PRT_shared_createDirectory_Exit3(tempPath);
			return J9SH_FAILED;
		}
		previous = current;
		current = strchr(current + 1, DIR_SEPARATOR);
		*previous = DIR_SEPARATOR;
	}

	Trc_PRT_shared_createDirectory_Exit2();
	return J9SH_SUCCESS;
}

/*
 * Note that this auto-clean function walks all shared memory segments on the system looking
 * for J9 shared caches. If it finds any, it attempts to destroy them. This was introduced to mitigate
 * against /tmp being wiped and the control files being lost, which causes shared memory to be leaked.
 * It is important to understand that this only attempts to clean shared memory areas which were created
 * in the default control file dir (/tmp) and therefore any VM which is currently using a custom control file dir
 * will exit this function without attempting any cleanup. Caches not created in the default control file dir are
 * distinguished by having an extra bit in the "version" field of the cache header, which is checked for below.
 */

void
cleanSharedMemorySegments(struct J9PortLibrary* portLibrary)
{
#if defined(LINUX) || defined(OSX)
#define IPCS_NO_FIELDS 7
#define IPCS_USERID_FIELD_NO 2
#define IPCS_SHMID_FIELD_NO 1
#define IPCS_NO_HEADER_LINES 3
#elif defined(AIXPPC)
#define IPCS_NO_FIELDS 6
#define IPCS_USERID_FIELD_NO 4
#define IPCS_SHMID_FIELD_NO 1
#define IPCS_NO_HEADER_LINES 3
#elif defined(J9ZOS390)
#define IPCS_NO_FIELDS 6
#define IPCS_USERID_FIELD_NO 4
#define IPCS_SHMID_FIELD_NO 1
#define IPCS_NO_HEADER_LINES 4
#else
#error cleanSharedMemorySegments: Platform not recognised
#endif
#define OUTPUTBUFSIZE 256

	OMRPortLibrary *omrPortLibrary = OMRPORT_FROM_J9PORT(portLibrary);
	char *ipcsField[IPCS_NO_FIELDS] = {0};
	char outputBuf[OUTPUTBUFSIZE] = {0};
	char *bufferPtr = NULL;
	char *bufferEndPtr = NULL;
	FILE *ipcsOutput = NULL;
	int noHeaderLines = IPCS_NO_HEADER_LINES;
	char processOwner[OUTPUTBUFSIZE] = "";
	int i = 0;
	IDATA result = 0;

	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);

#if defined(J9ZOS390)
	pid_t pid = -1;
	int childStatus = 0;
#endif

	Trc_PRT_shared_cleanSharedMemorySegments_entry();
#if defined(J9VM_OPT_CRIU_SUPPORT)
	/* isCheckPointAllowed is equivalent to isCheckpointAllowed().
	 * https://github.com/eclipse-openj9/openj9/issues/15800
	 */
	if (!portLibrary->isCheckPointAllowed)
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
	{
		result = omrsysinfo_get_username(processOwner, OUTPUTBUFSIZE);
		if (0 == result) {
			/* succeeded */
		} else if (result > 0) {
			Trc_PRT_shared_cleanSharedMemorySegments_getUserNameTooLong(result, OUTPUTBUFSIZE);
		} else {
			Trc_PRT_shared_cleanSharedMemorySegments_getUserName_Failed();
		}
	}
	if (0 != result) {
		result = omrsysinfo_get_env("USER", processOwner, OUTPUTBUFSIZE);
		if (0 == result) {
			/* succeeded */
		} else if (result > 0) {
			Trc_PRT_shared_cleanSharedMemorySegments_getEnvUserNameTooLong(result, OUTPUTBUFSIZE);
		} else {
			Trc_PRT_shared_cleanSharedMemorySegments_getEnvUserName_Failed();
		}
	}
	/* Keep existing behaviour regardless errors, processOwner = "" by default. */

	/* Run ipcs command and get FILE * to read output */
#if defined(J9ZOS390)
	{
		int pipeFd[2] = {0};
		char *fdopenArg = NULL;
		struct inheritance inherit = {0};
		int fdCnt = 0;
		int fdMap[3] = {0};
		char *argv[3] = {0};
		char **envp = NULL;

		/* Open a pipe for use with the ipcs command - we will use the read end */
		if (pipe(pipeFd) < 0) {
			Trc_PRT_shared_cleanSharedMemorySegments_pipeFailed(errno);
			return;
		}

		/* Set up file descriptors to be used by ipcs - stdin same as us; stdout and stderr to be the write end of the pipe */
		fdCnt = 3;
		fdMap[0] = 0;
		fdMap[1] = pipeFd[1];
		fdMap[2] = pipeFd[1];

		/* We don't need the ipcs process to inherit anything */
		inherit.flags = 0;

		/* Set up argv array */
#pragma convlit(suspend)
		fdopenArg = "r";
		argv[0] = "/bin/ipcs";
		argv[1] = "-m"; /* Only show details of shared memory */
		argv[2] = NULL;
#pragma convlit(resume)

		/* spawn a process to run the ipcs command */
		pid = spawn( argv[0],
								fdCnt,
								fdMap,
								&inherit,
								(const unsigned char**)argv,
								(const unsigned char**)envp);
		if (-1 == pid) {
			Trc_PRT_shared_cleanSharedMemorySegments_spawnFailed(errno);
			return;
		} else {
			Trc_PRT_shared_cleanSharedMemorySegments_spawnSuccess(pid);
		}

		/* Now new process has been created we close the write end of the pipe in this process */
		close(pipeFd[1]);

		/* get a FILE * from the file descriptor for the read end of the pipe */
		ipcsOutput = fdopen(pipeFd[0], fdopenArg);
		if (NULL == ipcsOutput) {
			Trc_PRT_shared_cleanSharedMemorySegments_fdopenFailed(errno);
			return;
		}
	}
#elif defined(J9ZTPF) /* defined(J9ZOS390) */
	/* The z/TPF platform does not support the ipcs command */
	Trc_PRT_shared_cleanSharedMemorySegments_exit();
	return;
#else /* defined(J9ZTPF) */
	/* Run the ipcs command and create a FILE * for us to read its output */
	ipcsOutput = popen( "ipcs -m", "r");

	if (NULL == ipcsOutput) {
		Trc_PRT_shared_cleanSharedMemorySegments_popenFailed(errno);
		return;
	}
#endif
	/* By this point we have a FILE *, ipcsOutput, we can read ipcs output from, whatever platform we are running on */

	/* Skip header lines in output */
	for (; noHeaderLines > 0; noHeaderLines--) {
		if (NULL == fgets(outputBuf, OUTPUTBUFSIZE, ipcsOutput)) {
			Trc_PRT_shared_cleanSharedMemorySegments_eofDuringHeaderReads(errno);
			goto errorRet;
		} else {
			Trc_PRT_shared_cleanSharedMemorySegments_lineSkipped(outputBuf);
		}
	}

	/* Read next output line */
	while (NULL != fgets( outputBuf, OUTPUTBUFSIZE, ipcsOutput)) {
		Trc_PRT_shared_cleanSharedMemorySegments_lineRead(outputBuf);

		/* Split into fields */

		/* NULL all pointers in case we come to the end of the line before processing all expected fields */
		for ( i = 0; i < IPCS_NO_FIELDS; i++ ) {
			ipcsField[i] = NULL;
		}

		/* Process expected number of fields */
		bufferPtr = &outputBuf[0];
		bufferEndPtr = &outputBuf[strlen(outputBuf)];
		for ( i = 0; (i < IPCS_NO_FIELDS) && (bufferPtr < bufferEndPtr); i++) {
			/* Skip initial spaces */
			for (; *bufferPtr == ' ' && bufferPtr < bufferEndPtr; bufferPtr++ );
			if (bufferPtr >= bufferEndPtr) {
				break;
			}
			/* Find end of field */
			ipcsField[i] = bufferPtr;
			for (; *bufferPtr != ' ' && *bufferPtr != '\n' && *bufferPtr != '\0'; bufferPtr++ );
			/* NUL terminate field and advance pointer */
			*bufferPtr = '\0';
			bufferPtr++;
			Trc_PRT_shared_cleanSharedMemorySegments_ipcsField(i, ipcsField[i]);
		}
		Trc_PRT_shared_cleanSharedMemorySegments_endOfExtraction();

		/* If owner, attach shared memory, check eyecatcher and remove if shared memory classes cache */
		if (ipcsField[IPCS_USERID_FIELD_NO] &&
			ipcsField[IPCS_SHMID_FIELD_NO] &&
			(!strcmp(ipcsField[IPCS_USERID_FIELD_NO], processOwner) ||
			!strcmp(processOwner, "root"))) {
			uintptr_t shmid = atol(ipcsField[IPCS_SHMID_FIELD_NO]);
			char *region = NULL;
			char *sharedClassesEyecatcher = J9PORT_SHMEM_EYECATCHER;
			int sharedClassesEyecatcherLength = J9PORT_SHMEM_EYECATCHER_LENGTH;

			Trc_PRT_shared_cleanSharedMemorySegments_ownerMatch(processOwner, shmid);
			region = shmat( shmid, NULL, 0);
			if ((void *)-1 == region) {
				Trc_PRT_shared_cleanSharedMemorySegments_attachFailed(errno);
			} else {
				Dummy_OSCache_header* dummy = (Dummy_OSCache_header*)region;

				if (!memcmp( dummy->eyecatcher, sharedClassesEyecatcher, sharedClassesEyecatcherLength )) {
					/* Only clean shared memory areas that were created in the default control file dir */
					if ((dummy->version & J9VERSION_NOT_IN_DEFAULT_DIR_MASK) == 0) {
						Trc_PRT_shared_cleanSharedMemorySegments_eyecatcherMatched();
						if (-1 == shmctl( shmid, IPC_RMID, 0)) {
							Trc_PRT_shared_cleanSharedMemorySegments_deleteFailed(errno);
						} else {
							Trc_PRT_shared_cleanSharedMemorySegments_deleted();
							/* Add NLS message here */
						}
					}
				} else {
					Trc_PRT_shared_cleanSharedMemorySegments_eyecatcherMismatched();
				}
				if (-1 == shmdt(region)) {
					Trc_PRT_shared_cleanSharedMemorySegments_detachFailed(errno);
				}
			}
		} else {
			Trc_PRT_shared_cleanSharedMemorySegments_noFieldsOrUseridMismatch(ipcsField[IPCS_USERID_FIELD_NO], ipcsField[IPCS_SHMID_FIELD_NO]);
		}
	}
	Trc_PRT_shared_cleanSharedMemorySegments_exit();

errorRet:
#if defined(J9ZOS390)
	fclose(ipcsOutput);
	if (-1 == waitpid( pid, &childStatus, 0)) {
		Trc_PRT_shared_cleanSharedMemorySegments_waitpidFailed(errno);
	} else {
		if (WIFEXITED(childStatus)) {
			Trc_PRT_shared_cleanSharedMemorySegments_waitpidSuccess(WEXITSTATUS(childStatus));
		} else {
			Trc_PRT_shared_cleanSharedMemorySegments_childFailed(childStatus);
		}
	}
#else
	pclose(ipcsOutput);
#endif

	return;
}

/**
 * @internal
 * @brief This function opens a file and takes a writer lock on it. If the file has been removed we repeat the process.
 *
 * This function is used to help manage files used to control System V objects (semaphores and memory)
 *
 * @param[in] portLibrary The port library
 * @param[out] fd the file descriptor we locked on (if we are successful)
 * @param[out] isReadOnlyFD is the file opened read only (we do this only if we can not obtain write access)
 * @param[out] canCreateNewFile if true this function may create the file if it does not exist.
 * @param[in] filename the file we are opening
 * @param[in] groupPerm 1 if the file needs to have group read-write permission, 0 otherwise; used only when creating new control file
 *
 * @return 0 on success or less than 0 in the case of an error
 */
intptr_t
ControlFileOpenWithWriteLock(struct J9PortLibrary* portLibrary, intptr_t * fd, BOOLEAN * isReadOnlyFD, BOOLEAN canCreateNewFile, const char * filename, uintptr_t groupPerm)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	BOOLEAN weAreDone = FALSE;
	struct stat statafter;
	int32_t openflags =  EsOpenWrite | EsOpenRead;
	int32_t exclcreateflags =  EsOpenCreate | EsOpenWrite | EsOpenRead | EsOpenCreateNew;
	int32_t lockType = J9PORT_FILE_WRITE_LOCK;

	Trc_PRT_shared_ControlFileFDWithWriteLock_EnterWithMessage("Start");

	if (fd == NULL) {
		Trc_PRT_shared_ControlFileFDWithWriteLock_ExitWithMessage("Error: fd is null");
		return J9SH_FAILED;
	}

	while (weAreDone == FALSE) {
		BOOLEAN fileExists = TRUE;
		struct J9FileStat statbuf;
		BOOLEAN tryOpenReadOnly = FALSE;

		*fd = 0;
		lockType = J9PORT_FILE_WRITE_LOCK;

		if (0 != omrfile_stat(filename, 0, &statbuf)) {
			int32_t porterrno = omrerror_last_error_number();
			switch (porterrno) {
				case J9PORT_ERROR_FILE_NOENT:
					if (TRUE == canCreateNewFile) {
						fileExists = FALSE;
					} else {
						Trc_PRT_shared_ControlFileFDWithWriteLock_ExitWithMessage("Error: j9file_stat() has failed because the file does not exist, and 'FALSE == canCreateNewFile'");
						return J9SH_FILE_DOES_NOT_EXIST;
					}
					break;
				default:
					Trc_PRT_shared_ControlFileFDWithWriteLock_ExitWithMessage("Error: j9file_stat() has failed");
					return J9SH_FAILED;
			}
		}

		/*Only create a new file if it does not exist, and if we are allowed to.*/
		if ((FALSE == fileExists) && (TRUE == canCreateNewFile)) {
			int32_t mode = (1 == groupPerm) ? J9SH_BASEFILEPERM_GROUP_RW_ACCESS : J9SH_BASEFILEPERM;

			*fd = omrfile_open(filename, exclcreateflags, mode);
			if (*fd != -1) {
				if (omrfile_chown(filename, OMRPORT_FILE_IGNORE_ID, getegid()) == -1) {
					/*If this fails it is not fatal ... but we may have problems later ...*/
					Trc_PRT_shared_ControlFileFDWithWriteLock_Message("Info: could not chown file.");
				}
				if (-1 == omrfile_close(*fd)) {
					Trc_PRT_shared_ControlFileFDWithWriteLock_ExitWithMessage("Error: could not close exclusively created file");
					return J9SH_FAILED;
				}
			} else {
				int32_t porterrno = omrerror_last_error_number();
				if (J9PORT_ERROR_FILE_EXIST != porterrno) {
					Trc_PRT_shared_ControlFileFDWithWriteLock_ExitWithMessage("Error: j9file_open() failed to create new control file");
					return J9SH_FAILED;
				}
			}
			if (0 != omrfile_stat(filename, 0, &statbuf)) {
				int32_t porterrno = omrerror_last_error_number();
				switch (porterrno) {
					case J9PORT_ERROR_FILE_NOENT:
						/*Our unlinking scheme may cause the file to be deleted ... we go back to the beginning of the loop in this case*/
						continue;
					default:
						Trc_PRT_shared_ControlFileFDWithWriteLock_Message("Error: j9file_stat() failed after successfully creating file.");
						return J9SH_FAILED;
				}
			}
		}

		/*Now get a file descriptor for caller ... and obtain a writer lock ... */
		*fd = 0;
		*isReadOnlyFD=FALSE;

		if (TRUE == IsFileReadWrite(&statbuf)) {
			/*Only open file rw if stat says it is not likely to fail*/
			*fd = omrfile_open(filename, openflags, 0);
			if (-1 == *fd) {
				int32_t porterrno = omrerror_last_error_number();
				if (porterrno == J9PORT_ERROR_FILE_NOPERMISSION) {
					tryOpenReadOnly = TRUE;
				}
			} else {
				*isReadOnlyFD=FALSE;
			}
		} else {
			tryOpenReadOnly = TRUE;
		}

		if (TRUE == tryOpenReadOnly) {
			/*If opening the file rw will fail then default to r*/
			*fd = omrfile_open(filename, EsOpenRead, 0);
			if (-1 != *fd) {
				*isReadOnlyFD=TRUE;
			}
		}

		if (-1 == *fd) {
			int32_t porterrno = omrerror_last_error_number();
			if ((porterrno == J9PORT_ERROR_FILE_NOENT) || (porterrno == J9PORT_ERROR_FILE_NOTFOUND)) {
				/*Our unlinking scheme may cause the file to be deleted ... we go back to the beginning of the loop in this case*/
				if (canCreateNewFile == TRUE) {
					continue;
				} else {
					Trc_PRT_shared_ControlFileFDWithWriteLock_ExitWithMessage("Error: file does not exist");
					return J9SH_FILE_DOES_NOT_EXIST;
				}
			}
			Trc_PRT_shared_ControlFileFDWithWriteLock_ExitWithMessage("Error: failed to open control file");
			return J9SH_FAILED;
		}

		/*Take a reader lock if we open the file read only*/
		if (*isReadOnlyFD == TRUE) {
			lockType = J9PORT_FILE_READ_LOCK;
		}
		if (omrfile_lock_bytes(*fd, lockType | J9PORT_FILE_WAIT_FOR_LOCK, 0, 0) == -1) {
			if (-1 == omrfile_close(*fd)) {
				Trc_PRT_shared_ControlFileFDWithWriteLock_ExitWithMessage("Error: closing file after fcntl error has failed");
				return J9SH_FAILED;
			}
			Trc_PRT_shared_ControlFileFDWithWriteLock_ExitWithMessage("Error: failed to take a lock on the control file");
			return J9SH_FAILED;
		} else {
			/*CMVC 99667: there is no stat function in the port lib ... so we are careful to handle bias used by z/OS*/
			if (fstat( (((int)*fd) - FD_BIAS), &statafter) == -1) {
				omrerror_set_last_error(errno, J9PORT_ERROR_FILE_FSTAT_FAILED);
				if (-1 == omrfile_close(*fd)) {
					Trc_PRT_shared_ControlFileFDWithWriteLock_ExitWithMessage("Error: closing file after fstat error has failed");
					return J9SH_FAILED;
				}
				Trc_PRT_shared_ControlFileFDWithWriteLock_ExitWithMessage("Error: failed to stat file descriptor");
				return J9SH_FAILED;
			}
			if (0 == statafter.st_nlink) {
				/* We restart this process if the file was deleted */
				if (-1 == omrfile_close(*fd)) {
					Trc_PRT_shared_ControlFileFDWithWriteLock_ExitWithMessage("Error: closing file after checking link count");
					return J9SH_FAILED;
				}
				continue;
			}

			/*weAreDone=TRUE;*/
			Trc_PRT_shared_ControlFileFDWithWriteLock_ExitWithMessage("Success");
			return J9SH_SUCCESS;
		}
	}
	/*we should never reach here*/
	Trc_PRT_shared_ControlFileFDWithWriteLock_ExitWithMessage("Failed to open file");
	return J9SH_FAILED;
}
/**
 * @internal
 * @brief This function closes a file and releases a writer lock on it.
 *
 * This function is used to help manage files used to control System V objects (semaphores and memory)
 *
 * @param[in] portLibrary The port library
 * @param[in] fd the file descriptor we wish to close
 *
 * @return 0 on success or less than 0 in the case of an error
 */

intptr_t
ControlFileCloseAndUnLock(struct J9PortLibrary* portLibrary, intptr_t fd)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	Trc_PRT_shared_ControlFileFDUnLock_EnterWithMessage("Start");
	if (-1 == omrfile_close(fd)) {
		Trc_PRT_shared_ControlFileFDUnLock_ExitWithMessage("Error: failed to close control file.");
		return J9SH_FAILED;
	}
	Trc_PRT_shared_ControlFileFDUnLock_ExitWithMessage("Success");
	return J9SH_SUCCESS;
}


#if defined(J9OS_I5)
/**
 * @internal
 * @brief This function validates if the current user has *ALLOBJ and *SECADM special authorities.
 *
 * This function is used to help manage files used to control System V objects (semaphores and memory)
 * @return TRUE if current user has ALLOBJ and *SECADM special authorities.
 */
static BOOLEAN isAllObjAndSecAdmUser()
{
 	const char* objname = "QSYRUSRI";
 	const char* libname = "QSYS";

 	/*
 	 * ilepContainer is the field into which a 16-byte tagged system pointer to the specified object is place by _RSLOBJ2.
 	 * It is extra large to allow the address for the system pointer to be placed on a
 	 * 16 byte boundary.  PASE (AIX) compilers do not know about tagged pointers and thus do not place
 	 * variables on 16 byte boundaries required for tagged pointers. *
 	 */
 	char ilepContainer[sizeof(ILEpointer) + 16];
 	ILEpointer* qsyrusri_pointer= (ILEpointer*)(((size_t)(ilepContainer) + 0xf) & ~0xf);

 	char rcvr[104];
 	int rcvrlen = sizeof(rcvr);

 	/*Text 'USRI0200' in EBCDIC*/
 	char format[] = {0xe4, 0xe2, 0xd9, 0xc9, 0xf0, 0xf2, 0xf0, 0xf0};

 	/*Text 'CURRENT' in EBCDIC*/
 	char profname[] = {0x5c, 0xc3, 0xe4, 0xd9, 0xd9, 0xc5, 0xd5, 0xe3, 0x40, 0x40};

 	struct {
 		int bytes_provided;
 		int bytes_available;
 		char msgid[7];
 		char reserved;
 		char exception_data[64];
 	} errcode;

 	char hasAuthority[] = {0xe8,0xe8}; /*Text 'YY' in EBCDIC*/
 	void *qsyrusri_argv[6];

 	memset(ilepContainer, 0 , sizeof(ilepContainer));

	/*Set the IBM i pointer to the QSYS/QSYRUSRI *PGM object*/
	if (0 != _RSLOBJ2(qsyrusri_pointer, RSLOBJ_TS_PGM, objname, libname)) {
		return FALSE;
	}
 	/* initialize the QSYRUSRI returned info structure and error code structure  */
 	memset(rcvr, 0, sizeof(rcvr));
 	memset(&errcode, 0, sizeof(errcode));
 	errcode.bytes_provided = sizeof(errcode);

 	/* initialize the array of argument pointers for the QSYRUSRI API */
 	qsyrusri_argv[0] = &rcvr;
 	qsyrusri_argv[1] = &rcvrlen;
 	qsyrusri_argv[2] = &format;
 	qsyrusri_argv[3] = &profname;
 	qsyrusri_argv[4] = &errcode;
 	qsyrusri_argv[5] = NULL;

 	/* Call the IBM i QSYRUSRI API from PASE for i */
 	if (0 != _PGMCALL((const ILEpointer*)qsyrusri_pointer, (void*)&qsyrusri_argv, 0)) {
 		return FALSE;
 	}

 	/* Check the contents of bytes 28-29 of the returned information.
       If they are 'YY', the current user has *ALLOBJ and *SECADM special authorities*/
	if (0 == memcmp(&rcvr[28], &hasAuthority, sizeof(hasAuthority))) {
 		return TRUE;
 	} else {
 		return FALSE;
 	}
 }
#endif

/**
 * @internal
 * @brief This function validates if the current uid has rw access to a file.
 *
 * This function is used to help manage files used to control System V objects (semaphores and memory)
 *
 * @param[in] statbuf stat infor the file in question
 *
 * @return TRUE if current uuid has rw access to the file
 */
static BOOLEAN
IsFileReadWrite(struct J9FileStat * statbuf)
{
	if (statbuf->ownerUid == geteuid()) {
		if (statbuf->perm.isUserWriteable == 1 && statbuf->perm.isUserReadable == 1) {
			return TRUE;
		} else {
			return FALSE;
		}
	} else {
		if (statbuf->perm.isGroupWriteable == 1 && statbuf->perm.isGroupReadable == 1) {
			return TRUE;
		} else {
#if defined(J9OS_I5)
 			if (isAllObjAndSecAdmUser()) {
 				return TRUE;
 			}
#endif
			return FALSE;
		}
	}
}

/**
 * @internal
 * @brief This function is used to unlink a control file in an attempt to recover from an error condition.
 * To avoid overwriting the original error code/message, it creates a copy of existing error code/message,
 * unlinks the file and restores the error code/message.
 * Error code/message about an error that occurs during unlinking is stored in J9ControlFileStatus parameter.
 *
 * @param[in] portLibrary pointer to J9PortLibrary
 * @param[in] controlFile file to be unlinked
 * @param[out] controlFileStatus pointer to J9ControlFileStatus. Stores error code and message if any error occurs during unlinking
 *
 * @return TRUE if successfully unlinked the control file, or control file does not exist, FALSE otherwise
 */
BOOLEAN
unlinkControlFile(struct J9PortLibrary* portLibrary, const char *controlFile, J9ControlFileStatus *controlFileStatus)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	char originalErrMsg[J9ERROR_DEFAULT_BUFFER_SIZE];
	int32_t originalErrCode = omrerror_last_error_number();
	const char *currentErrMsg = omrerror_last_error_message();
	int32_t msgLen = strlen(currentErrMsg);
	BOOLEAN rc = FALSE;

	if (msgLen + 1 > sizeof(originalErrMsg)) {
		msgLen = sizeof(originalErrMsg) - 1;
	}
	strncpy(originalErrMsg, currentErrMsg, msgLen);
	originalErrMsg[msgLen] = '\0';
	if (-1 == omrfile_unlink(controlFile)) {
		/* If an error occurred during unlinking, store the unlink error code in 'controlFileStatus' if available,
		 * and restore previous error.
		 */
		int32_t unlinkErrCode = omrerror_last_error_number();

		if (J9PORT_ERROR_FILE_NOENT != unlinkErrCode) {
			if (NULL != controlFileStatus) {
				const char *unlinkErrMsg = omrerror_last_error_message();
				msgLen = strlen(unlinkErrMsg);

				/* clear any stale status */
				memset(controlFileStatus, 0, sizeof(*controlFileStatus));
				controlFileStatus->status = J9PORT_INFO_CONTROL_FILE_UNLINK_FAILED;
				controlFileStatus->errorCode = unlinkErrCode;
				controlFileStatus->errorMsg = omrmem_allocate_memory(msgLen+1, OMRMEM_CATEGORY_PORT_LIBRARY);
				if (NULL != controlFileStatus->errorMsg) {
					strncpy(controlFileStatus->errorMsg, unlinkErrMsg, msgLen);
					controlFileStatus->errorMsg[msgLen] = '\0';
				}
			}
			rc = FALSE;
		} else {
			/* Control file is already deleted; treat it as success. */
			if (NULL != controlFileStatus) {
				/* clear any stale status */
				memset(controlFileStatus, 0, sizeof(*controlFileStatus));
				controlFileStatus->status = J9PORT_INFO_CONTROL_FILE_UNLINKED;
			}
			rc = TRUE;
		}
	} else {
		if (NULL != controlFileStatus) {
			/* clear any stale status */
			memset(controlFileStatus, 0, sizeof(*controlFileStatus));
			controlFileStatus->status = J9PORT_INFO_CONTROL_FILE_UNLINKED;
		}
		rc = TRUE;
	}
	/* restore error code and message as before */
	omrerror_set_last_error_with_message(originalErrCode, originalErrMsg);
	return rc;
}
