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


#include <windows.h>
#include "j9port.h"



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
	HANDLE 	hMutex;
	DWORD 	dwResult;

	/* check length of name */
	if (strlen(name) > MAX_PATH) {
		return -1;
	}

	/* Create a named mutex or get its handle if it already exists */
	hMutex = CreateMutex(
						NULL,
						FALSE,						/* Not owned on creation */
						(LPCTSTR) name);		/* Mutex name */

	if (hMutex == NULL) {
		return -1;
	} else {
		/* Wait until mutex is released by other process */
		dwResult = WaitForSingleObject(
								hMutex,					/* Handle to mutex */
								INFINITE);				/* Block indefinitely */

		/* if state of mutex was signaled within wait interval or mutex was abandoned, return true */
		/* otherwise function timeout, so return false */
		if (dwResult == WAIT_OBJECT_0 || dwResult == WAIT_ABANDONED) {
			return 0;
		} else {
			return -1;
		}
	}
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
	HANDLE 	hMutex;

	/* check length of name */
	if (strlen(name) > MAX_PATH) {
		return -1;
	}

	/* Open handle to mutex */
	hMutex = OpenMutex(
						MUTEX_ALL_ACCESS,
						FALSE,						/* Will be ignored for existing mutex */
						(LPCTSTR) name);		/* Mutex name */

	if (hMutex == NULL) {
		return -1;
	}

	/* release mutex */
	if (ReleaseMutex(hMutex) == 0) {
		return -1;
	} else {
		return 0;
	}
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


