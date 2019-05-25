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

#include <j9port.h>
#include <stddef.h>
#include "j9shsem.h"

/** 
 * Initialize a J9PortShSemParameters struct with default values 
 * @param[in] portLibrary The port library.
 * @param[out] J9PortShSemParameters containing information necessary for creating and opening a semaphore.
 * The caller must allocate storage for this struct.
  * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9shsem_params_init(struct J9PortLibrary *portLibrary, struct J9PortShSemParameters *params) 
{
 	return -1;
}

/**
 * Open an existing semaphore set, or create a new one if it does not exist
 * 
 * @param[in] portLibrary The port library.
 * @param[out] handle A semaphore handle is allocated and initialized for use with further calls, NULL on failure.
 * @param[in] params struct containing semaphore name, permissions, etc.
 *
 * @return
 * \arg J9PORT_ERROR_SHSEM_OPFAILED   Failure - Error opening the semaphore
 * \arg J9PORT_INFO_SHSEM_CREATED Success - Semaphore has been created
 * \arg J9PORT_INFO_SHSEM_OPENED  Success - Existing semaphore has been opened
 * \arg J9PORT_INFO_SHSEM_SEMID_DIFF Success - Existing semaphore opened, but OS Semaphore key is different
 */
intptr_t 
j9shsem_open (struct J9PortLibrary *portLibrary, struct j9shsem_handle **handle, const struct J9PortShSemParameters *params) 
{
	return J9PORT_ERROR_SHSEM_OPFAILED;
}

/**
 * post operation increments the counter in the semaphore by 1 if there is no one in wait for the semaphore. 
 * if there are other processes suspended by wait then one of them will become runnable and 
 * the counter remains the same. 
 * 
 * @param[in] portLibrary The port library.
 * @param[in] handle Semaphore set handle.
 * @param[in] semset The no of semaphore in the semaphore set that you want to post.
 * @param[in] flag The semaphore operation flag:
 * \arg J9PORT_SHSEM_MODE_DEFAULT The default operation flag, same as 0
 * \arg J9PORT_SHSEM_MODE_UNDO The changes made to the semaphore will be undone when this process finishes.
 *
 * @return 0 on success, -1 on failure.
 */
intptr_t 
j9shsem_post(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset, uintptr_t flag)
{
	return -1;
}
/**
 * Wait operation decrements the counter in the semaphore set if the counter > 0
 * if counter == 0 then the caller will be suspended.
 * 
 * @param[in] portLibrary The port library.
 * @param[in] handle Semaphore set handle.
 * @param[in] semset The no of semaphore in the semaphore set that you want to post.
 * @param[in] flag The semaphore operation flag:
 * \arg J9PORT_SHSEM_MODE_DEFAULT The default operation flag, same as 0
 * \arg J9PORT_SHSEM_MODE_UNDO The changes made to the semaphore will be undone when this process finishes.
 * \arg J9PORT_SHSEM_MODE_NOWAIT The caller will not be suspended if sem == 0, a -1 is returned instead.
 * 
 * @return 0 on success, -1 on failure.
 */
intptr_t
j9shsem_wait(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset, uintptr_t flag)
{
	return -1;
}
/**
 * reading the value of the semaphore in the set. This function
 * uses no synchronisation primitives
  * 
 * @pre caller has to deal with synchronisation issue.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Semaphore set handle.
 * @param[in] semset The number of semaphore in the semaphore set that you want to post.
 * 
 * @return -1 on failure, the value of the semaphore on success
 * 
 * @warning: The user will need to make sure locking is done correctly when
 * accessing semaphore values. This is because getValue simply reads the semaphore
 * value without stopping the access to the semaphore. Therefore the value of the semaphore
 * can change before the function returns. 
 */
intptr_t 
j9shsem_getVal(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset) 
{
	return -1;
}

/**
 * 
 * setting the value of the semaphore specified in semset. This function
 * uses no synchronisation primitives
 * 
 * @pre Caller has to deal with synchronisation issue.
 * 
 * @param[in] portLibrary The port Library.
 * @param[in] handle Semaphore set handle.
 * @param[in] semset The no of semaphore in the semaphore set that you want to post.
 * @param[in] value The value that you want to set the semaphore to
 * 
 * @warning The user will need to make sure locking is done correctly when
 * accessing semaphore values. This is because setValue simply set the semaphore
 * value without stopping the access to the semaphore. Therefore the value of the semaphore
 * can change before the function returns. 
 *
 * @return 0 on success, -1 on failure.
 */
intptr_t 
j9shsem_setVal(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset, intptr_t value)
{
	return -1;
}


/**
 * Release the resources allocated for the semaphore handles.
 * 
 * @param[in] portLibrary The port library.
 * @param[in] handle Semaphore set handle.
 * 
 * @note The actual semaphore is not destroyed.  Once the close operation has been performed 
 * on the semaphore handle, it is no longer valid and user needs to reissue @ref j9shsem_open call.
 */
void 
j9shsem_close (struct J9PortLibrary *portLibrary, struct j9shsem_handle **handle)
{
}



/**
 * Destroy the semaphore and release the resources allocated for the semaphore handles.
 * 
 * @param[in] portLibrary The port library.
 * @param[in] handle Semaphore set handle.
 * 
 * @return 0 on success, -1 on failure.
 * @note Due to operating system restriction we may not be able to destroy the semaphore
 */
intptr_t 
j9shsem_destroy (struct J9PortLibrary *portLibrary, struct j9shsem_handle **handle)
{
	return -1;
}


/**
 * PortLibrary startup.
 *
 * This function is called during startup of the portLibrary.  Any resources that are required for
 * the file operations may be created here.  All resources created here should be destroyed
 * in @ref j9shsem_shutdown.
 *
 * @param[in] portLibrary The port library.
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg J9PORT_ERROR_STARTUP_SHSEM
 *
 * @note Most implementations will simply return success.
 */
int32_t
j9shsem_startup(struct J9PortLibrary *portLibrary)
{
	return 0;
}
/**
 * PortLibrary shutdown.
 *
 * This function is called during shutdown of the portLibrary.  Any resources that were created by @ref j9shsem_startup
 * should be destroyed here.
 *
 * @param[in] portLibrary The port library.
 *
 * @note Most implementations will be empty.
 */
void
j9shsem_shutdown(struct J9PortLibrary *portLibrary)
{
}

