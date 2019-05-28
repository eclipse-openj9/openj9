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
 * @brief Shared Resource Mutex
 *
 * The J9IPCMutex is used to protect a shared resource from simultaneous access by processes or threads executing in the same or different VMs.
 * Each process/thread must request and wait for the ownership of the shared resource before it can use that resource. It must also release the ownership
 * of the resource as soon as it has finished using it so that other processes competing for the same resource are not delayed.
 */


#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <string.h>
#include <errno.h>
#include "j9port.h"

#if !defined(OSX) /* OSX provides this union in it's sys/sem.h */
/* arg for semctl semaphore system calls. */
union semun {
	int val;
    struct semid_ds *buf;
    uint16_t *array;
};
#endif /* OSX */



/**
 * Acquires a named mutex for the calling process.
 *
 * If a Mutex with the same Name already exists, the function opens the existing Mutex and tries to lock it.
 * If another process already has the Mutex locked, the function will block indefinitely. 
 * If there is no Mutex with the same Name, the function will create it and lock it for the calling process of this function.
 *
 * @param[in] portLibrary The port library
 * @param[in] name Mutex to be acquired
 *
 * @return 0 on success, -1 on error.
 *
 * @note The Mutex must be explicitly released by calling the @ref j9ipcmutex_release function as 
 * soon as the lock is no longer required.
 */
int32_t
j9ipcmutex_acquire(struct J9PortLibrary *portLibrary, const char *name)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int sid;
	int nsops;									/* number of operations to perform on semaphore */
	int nsems = 1;							/* number of semaphores */
	key_t sKey;								/* semaphore identifier key */
	int nameLen;								/* length of semaphore name */
	char* sPath;								/* semaphore path (used in ftok) */
	int sPathLen;								/* length of semaphore path */
	union semun arg;						/* initialization options structure */
	struct sembuf sLock;					/* operations buffer */
	int32_t	mutexFD;							/* mutex file descriptor */

	nameLen = strlen(name);
	/* check if length of semaphore name is empty */
	if (nameLen == 0) {
		return -1;
	}

	/* get size required for semaphore path and name */
	sPathLen = nameLen + sizeof("/tmp/");

	sPath = omrmem_allocate_memory(sPathLen, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (!sPath) {
		return -1;
	}

	/* initialize semaphore path */
	strcpy(sPath, "/tmp/");
	strcat(sPath, name);

	/* create file to be used by semaphore */
	mutexFD = omrfile_open(sPath, EsOpenCreate | EsOpenRead | EsOpenWrite, 0666);
	if (mutexFD  == -1) {
		return -1;
	}
	
	/* close handle */
	omrfile_close(mutexFD);

	/* build unique semaphore key */
	sKey = ftok(sPath, 's');

	/* free allocated memory no longer needed */
	omrmem_free_memory(sPath);

	if (sKey == ((key_t) -1)) {
		return -1;
	}

	/* check if semaphore already exists */
	sid = semget(sKey, 0, 0666);
	if (sid == -1) {
		/* semaphore doesn't exist, create it */
		sid = semget(sKey, nsems, IPC_CREAT | 0666);
		if (sid == -1) {
			return -1;
		}

		/* semaphore created, set initial value */
		arg.val = 1;
		if (semctl(sid, 0, SETVAL, arg) == -1) {
			semctl(sid, 0, IPC_RMID, arg); /* cleanup semaphore from system */
			return -1;
		}
	}

	/* initialize operation structure to lock mutex */
	sLock.sem_num = 0;
	sLock.sem_op = -1;
	sLock.sem_flg = 0;

	/* set operation to acquire semaphore */
	nsops = 1;				/* one operation to be performed (acquire) */

	/* if semaphore was acquired successfully, return 0 */
	/* otherwise, return -1 */
	return (int32_t) semop(sid, &sLock, nsops);
}

/**
 * Releases a named Mutex from the calling process.
 *
 * If a Mutex with the same Name already exists, the function opens the existing Mutex and tries to unlock it.
 * This function will fail if a Mutex with the given Name is not found or if the Mutex cannot be unlocked.
 *
 * @param[in] portLibrary The port library
 * @param[in] name Mutex to be released.
 *
 * @return 0 on success, -1 on error.
 *
 * @note Callers of this function must have called the function @ref j9ipcmutex_acquire prior to calling this function.
 */
int32_t
j9ipcmutex_release(struct J9PortLibrary *portLibrary, const char *name)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int sid;
	int nsops;									/* number of operations to perform on semaphore */
	key_t sKey = 439; 						/* semaphore identifier key */
	int nameLen;								/* length of semaphore name */
	char* sPath;								/* semaphore path (used in ftok) */
	int sPathLen;								/* length of semaphore path */
	struct sembuf sUnlock;				/* operations buffer */

	nameLen = strlen(name);

	/* get size required for semaphore path and name */
	sPathLen = nameLen + sizeof("/tmp/");

	/* check if length of semaphore name is empty or if it exceeds max size for path */
	if (nameLen == 0) {
		return -1;
	}

	sPath = omrmem_allocate_memory(sPathLen, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (!sPath) {
		return -1;
	}

	/* build unique semaphore key */
	strcpy(sPath, "/tmp/");
	strcat(sPath, name);
	sKey = ftok(sPath, 's');

	/* free allocated memory no longer needed */
	omrmem_free_memory(sPath);

	if (sKey == (key_t) -1) {
		return -1;
	}

	/* open existing semaphore */
	sid = semget(sKey, 0, 0666);
	if (sid == -1) {
		return -1;
	}

	/* initialize operation structure to unlock mutex */
	sUnlock.sem_num = 0;
	sUnlock.sem_op = 1;
	sUnlock.sem_flg = 0;

	/* set operations to wait and acquire semaphore */
	nsops = 1;									/* one operation to be performed (release) */
	
	/* if semaphore was unlocked successfully, return 0 */
	/* otherwise, return -1 */
	return (int32_t) semop(sid, &sUnlock, nsops);
}

/**
 * PortLibrary shutdown.
 *
 * This function is called during shutdown of the portLibrary.  Any resources that were created by @ref j9ipcmutex_startup
 * should be destroyed here.
 *
 * @param[in] portLibrary The port library
 *
 * @note Most implementations will be empty.
 */
void
j9ipcmutex_shutdown(struct J9PortLibrary *portLibrary)
{
}

/**
 * PortLibrary startup.
 *
 * This function is called during startup of the portLibrary.  Any resources that are required for
 * the IPC mutex operations may be created here.  All resources created here should be destroyed
 * in @ref j9ipcmutex_shutdown.
 *
 * @param[in] portLibrary The port library
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg J9PORT_ERROR_STARTUP_IPCMUTEX
 *
 * @note Most implementations will simply return success.
 */
int32_t
j9ipcmutex_startup(struct J9PortLibrary *portLibrary)
{
	return 0;
}



