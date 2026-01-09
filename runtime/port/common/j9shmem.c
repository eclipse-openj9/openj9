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
#include <j9port.h>
#include "ut_j9prt.h"


/**
 * Open an older shared memory using the deprecated control file format.
 * 
 * @param[in] portLibrary The port Library
 * @param[in] cacheDirName The name of the cache directory
 * @param[in] groupPerm Group permissions to open the cache directory
 * @param[out] handle This handle is required for further attach/destroy of the memory region
 * @param[in] rootname Shared name for the region, which used to identify the region. 
 * @param[in] perm permission for the region.
 * @param[in] cacheFileType a uintptr_t indicating the type of cache file being used
 * @param[in] category Memory category code
 * 
 * @return
 * \arg J9PORT_ERROR_SHMEM_OPFAILED Failure - Cannot open the shared memory region
 * \arg J9PORT_INFO_SHMEM_OPENED Success - Existing memory region has been opened
 * \arg J9PORT_INFO_SHMEM_PARTIAL Failure - indicates that the SysV IPC object is missing.
 * 
 */
intptr_t
j9shmem_openDeprecated (struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct j9shmem_handle **handle, const char* rootname, uint32_t perm, uintptr_t cacheFileType, uint32_t category)
{
	return J9PORT_ERROR_SHMEM_OPFAILED;
}

/**
 * Creates/open a shared memory region
 * 
 * The rootname will uniquely identify the shared memory region, 
 * and is valid across different JVM instance. 
 * 
 * The shared memory region should persist across process, until OS reboots 
 * or destroy call is being made.
 * 
 * @param[in] portLibrary The port Library
 * @param[in] cacheDirName The name of the cache directory
 * @param[in] groupPerm Group permissions to open the cache directory
 * @param[out] handle This handle is required for further attach/destroy of the memory region
 * @param[in] rootname Shared name for the region, which used to identify the region. 
 * @param[in] size Size of the region in bytes
 * @param[in] perm permission for the region.
 * @param[in] flags used when we do not want to recreate shared memory.
 * @param[out] controlFileStatus indicates the status of control file if unlink was attempted. Valid values are:
 * \arg J9PORT_INFO_CONTROL_FILE_NOT_UNLINKED
 * \arg J9PORT_INFO_CONTROL_FILE_UNLINK_FAILED
 * \arg J9PORT_INFO_CONTROL_FILE_UNLINKED
 * 
 * @return
 * \arg J9PORT_ERROR_SHMEM_OPFAILED Failure - Cannot open the shared memory region
 * \arg J9PORT_INFO_SHMEM_OPENED Success - Existing memory region has been opened
 * \arg J9PORT_INFO_SHMEM_CREATED Success - A new shared memory region has been created
 * 
 */
intptr_t
j9shmem_open (struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct j9shmem_handle **handle, const char* rootname, uintptr_t size, uint32_t perm, uint32_t category, uintptr_t flags, J9ControlFileStatus *controlFileStatus)
{
	Trc_PRT_shmem_j9shmem_open_Entry(rootname, size, perm);
	Trc_PRT_shmem_j9shmem_open_Exit(J9PORT_ERROR_SHMEM_OPFAILED, *handle);
	return J9PORT_ERROR_SHMEM_OPFAILED;
}

/**
 * Attaches the shared memory represented by the handle
 * 
 * @param[in] portLibrary The port Library
 * @param[in] handle A valid shared memory handle
 * @param[in] category Memory allocation category code
 * 
 * @return: A pointer to the shared memory region, NULL on failure
 */
void*
j9shmem_attach(struct J9PortLibrary *portLibrary, struct j9shmem_handle* handle, uint32_t category)
{
	Trc_PRT_shmem_j9shmem_attach_Entry(handle);
	Trc_PRT_shmem_j9shmem_attach_Exit(NULL);
	return NULL;
}

/**
 * Detaches the shared memory region from the caller's process address space
 * Use @ref j9shmem_destroy to actually remove the memory region from the Operating system
 *
 * @param[in] portLibrary the Port Library.
 * @param[in] handle Pointer to the shared memory region.
 * 
 * @return 0 on success, -1 on failure.
 */
intptr_t
j9shmem_detach(struct J9PortLibrary *portLibrary, struct j9shmem_handle **handle) 
{
	Trc_PRT_shmem_j9shmem_detach_Entry(*handle);
	Trc_PRT_shmem_j9shmem_detach_Exit1();
	return -1;
}
/**
 * Destroy and removes the shared memory region from OS.
 * 
 * The timing of which OS removes the memory is OS dependent. However when 
 * you make a call you can considered that you can no longer access the region through
 * the handle. Memory allocated for handle structure is freed as well.
 * 
 * @param[in] portLibrary The port Library
 * @param[in] cacheDirName The name of the cache directory
 * @param[in] groupPerm Group permissions to open the cache directory
 * @param[in] handle Pointer to a valid shared memory handle
 * 
 * @return 0 on success, -1 on failure.
 */
intptr_t 
j9shmem_destroy (struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct j9shmem_handle **handle)
{
	return -1;
}

/**
 * Destroy and removes the shared memory region from OS.
 * 
 * The timing of which OS removes the memory is OS dependent. However when 
 * you make a call you can considered that you can no longer access the region through
 * the handle. Memory allocated for handle structure is freed as well.
 * 
 * @param[in] portLibrary The port Library
 * @param[in] cacheDirName The name of the cache directory
 * @param[in] groupPerm Group permissions to open the cache directory
 * @param[in] handle Pointer to a valid shared memory handle
 * @param[in] cacheFileType a uintptr_t indicating the type of cache file being used
 * 
 * @return 0 on success, -1 on failure.
 */
intptr_t 
j9shmem_destroyDeprecated (struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct j9shmem_handle **handle, uintptr_t cacheFileType)
{
	return j9shmem_destroy (portLibrary, cacheDirName, groupPerm, handle);
}

/**
 * PortLibrary shutdown.
 *
 * This function is called during shutdown of the portLibrary.  Any resources that were created by @ref j9shmem_startup
 * should be destroyed here.
 *
 * @param[in] portLibrary The port library.
 *
 * @note Most implementations will be empty.
 */
void
j9shmem_shutdown(struct J9PortLibrary *portLibrary)
{
}
/**
 * PortLibrary startup.
 *
 * This function is called during startup of the portLibrary.  Any resources that are required for
 * the file operations may be created here.  All resources created here should be destroyed
 * in @ref j9shmem_shutdown.
 *
 * @param[in] portLibrary The port library.
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg J9PORT_ERROR_STARTUP_SHMEM
 *
 * @note Most implementations will simply return success.
 */
int32_t
j9shmem_startup(struct J9PortLibrary *portLibrary)
{
	return 0;
}
/**
 * Detach, Close and remove the shared memory handle.
 * 
 * @note This method does not remove the shared memory region from the OS
 *	 use @ref j9shmem_destroy instead. However this will free all the memory
 * resources used by the handle, and detach the region specified by the handle
 *
 * @param[in] portLibrary The port Library
 * @param[in] handle Pointer to a valid shared memory handle
 * 
 */
void 
j9shmem_close(struct J9PortLibrary *portLibrary, struct j9shmem_handle **handle)
{
	Trc_PRT_shmem_j9shmem_close_Entry(*handle);
	Trc_PRT_shmem_j9shmem_close_Exit();
}
/**
 * Find the name of a shared memory region on the system. Answers a handle
 * to be used in subsequent calls to @ref j9shmem_findnext and @ref j9shmem_findclose. 
 *
 * @param[in] portLibrary The port library
 * @param[in] cacheDirName The name of the cache directory
 * @param[out] resultbuf filename and path matching path.
 *
 * @return valid handle on success, -1 on failure.
 */
uintptr_t
j9shmem_findfirst(struct J9PortLibrary *portLibrary, char *cacheDirName, char *resultbuf)
{
	return -1;
}
/**
 * Find the name of the next shared memory region.
 *
 * @param[in] portLibrary The port library
 * @param[in] findhandle handle returned from @ref j9shmem_findfirst.
 * @param[out] resultbuf next filename and path matching findhandle.
 *
 * @return 0 on success, -1 on failure or if no matching entries.
 */
int32_t
j9shmem_findnext(struct J9PortLibrary *portLibrary, uintptr_t findhandle, char *resultbuf)
{
	return -1;
}

/**
 * Close the handle returned from @ref j9shmem_findfirst.
 *
 * @param[in] portLibrary The port library
 * @param[in] findhandle  Handle returned from @ref j9shmem_findfirst.
 */
void
j9shmem_findclose(struct J9PortLibrary *portLibrary, uintptr_t findhandle)
{
	return;
}

/**
 * Return the statistic for a shared memory region
 *
 * @note the implementation can decide to put -1 in the fields of @ref statbuf
 * if it does not make sense on this platform, or it is impossible to obtain.
 * 
 * @param[in] portLibrary The port library
 * @param[in] cacheDirName The name of the cache directory
 * @param[in] groupPerm Group permissions to open the cache directory
 * @param[in] name The name of the shared memory area.
 * @param[out] statbuf the statistics returns by the operating system
 *
 * @return 0 on success, -1 on failure or if there is no matching entries.
 */
uintptr_t
j9shmem_stat(struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, const char* name, struct J9PortShmemStatistic* statbuf)
{
	return -1;
}

/**
 * Return the statistics for a shared memory using the deprecated control file format.
 *
 * @note the implementation can decide to put -1 in the fields of @ref statbuf
 * if it does not make sense on this platform, or it is impossible to obtain.
 * 
 * @param[in] portLibrary The port library
 * @param[in] cacheDirName The name of the cache directory
 * @param[in] groupPerm Group permissions to open the cache directory
 * @param[in] name The name of the shared memory area.
 * @param[out] statbuf the statistics returns by the operating system
 * @param[in] cacheFileType a uintptr_t indicating the type of cache file being used
 *
 * @return 0 on success, -1 on failure or if there is no matching entries.
 */
uintptr_t
j9shmem_statDeprecated(struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, const char* name, struct J9PortShmemStatistic* statbuf, uintptr_t cacheFileType)
{
	return -1;
}

/**
 * Return the statistics for a shared memory region associated with the given shared memory handle.
 * This API is used only on non-Windows systems. To get stats on Windows, see @ref j9shmem_stat.
 *
 * @note the implementation can decide to put -1 in the fields of @ref statbuf
 * if it does not make sense on this platform, or it is impossible to obtain.
 *
 * @param[in] portLibrary The port library
 * @param[in] handle A valid shared memory handle
 * @param[out] statbuf Pointer to J9PortShmemStatistic which is populated with statistics returned by the operating system.
 * 					   On return do not expect any value in statbuf to be preserved as it is explicitly cleared by this function before getting stats.
 *
 * @return
 * \arg J9PORT_INFO_SHMEM_STAT_PASSED			Success - Successfully retrieved stats of the shared memory
 * \arg J9PORT_ERROR_SHMEM_HANDLE_INVALID 		Failure - 'handle' is NULL
 * \arg J9PORT_ERROR_SHMEM_STAT_BUFFER_INVALID	Failure - 'statbuf' is NULL
 * \arg J9PORT_ERROR_SHMEM_STAT_FAILED			Failure - Error in retrieving stats of the shared memory
 */
intptr_t
j9shmem_handle_stat(struct J9PortLibrary *portLibrary, struct j9shmem_handle *handle, struct J9PortShmemStatistic *statbuf)
{
	return -1;
}

/**
 * This function will determine the control directory to be used for shared 
 * classes files and return the path to the caller via the buffer supplied
 * 
 * This function should be called first to obtain the directory used to store shared memory related files.
 * The directory will be passed to the other shmem or shmem_deprecated functions.
 *
 * If ctrlDirName is NULL, a platform specific directory will be chosen.
 * 
 * @param[in] portLibrary The port library
 * @param[in] ctrlDirName The name of the control directory, ignored if cacheDirName is set
 * @param[in] flags extra flags passed in
 * @param[out] buffer Pointer to a buffer to hold the returned path
 * @param[in] length the length of the buffer
 *
 * The returned path always ends with a DIR_SEPARATOR, ensuring it represents a directory.
 *
 * @return 0 for success
 * J9PORT_ERROR_SHMEM_GET_DIR_BUF_OVERFLOW The cache directory is too long
 * J9PORT_ERROR_SHMEM_GET_DIR_FAILED_TO_GET_HOME Cannot get the home directory
 * J9PORT_ERROR_SHMEM_GET_DIR_HOME_BUF_OVERFLOW The home directory is too long
 * J9PORT_ERROR_SHMEM_GET_DIR_HOME_ON_NFS The home directory is on network file system
 * J9PORT_ERROR_SHMEM_GET_DIR_CANNOT_STAT_HOME Failed to stat the home directory
 * J9PORT_ERROR_SHMEM_NOSPACE Cannot allocate native memory
 *  
 */
intptr_t
j9shmem_getDir(struct J9PortLibrary* portLibrary, const char* ctrlDirName, uint32_t flags, char* buffer, uintptr_t length)
{
	return -1;
}

/**
 * This function will create the control directory to be used for shared
 * classes files
 *
 * @param[in] portLibrary The port library
 * @param[in] cacheDirName The name of the cache directory
 * @param[in] cleanMemorySegments, set TRUE to call cleanSharedMemorySegments. It will clean sysv memory segments.
 * 				It should be set to TRUE if the ctrlDirName is NULL.
 *
 * Returns -1 for error, >=0 for success
 */
intptr_t
j9shmem_createDir(struct J9PortLibrary* portLibrary, char* cacheDirName,  uintptr_t cacheDirPerm, BOOLEAN cleanMemorySegments)
{
	return -1;
}
 
/**
 * This function will determine the file name path to be used for the given shared 
 * classes memory control file, ensure that it exists and return the path to the caller 
 * via the buffer supplied
 * 
 * @param[in] portLibrary The port library
 * @param[in] cacheDirName The name of the cache directory, usually the result of calling j9shmem_getDir
 * @param[in] buffer Pointer to a buffer to hold the returned path
 * @param[in] length the length of the buffer
 * @param[in] cachename the name of the shared classes cache
 *
 * @return 0 on success, -1 on failure
 */
intptr_t
j9shmem_getFilepath(struct J9PortLibrary* portLibrary, char* cacheDirName, char* buffer, uintptr_t length, const char* cachename)
{
	return -1;
}
 
/**
 * Sets the protection as specified by flags for the memory pages containing all or part of the interval address->(address+len).
 * The size of the memory page for a specified memory region can be requested via @ref j9shmem_get_region_granularity
 *
 * The memory region must have been acquired using j9shmap_file
 *
 * This call has no effect on the protection of other processes.
 *
 * The specified protection will overwrite any preexisting protection. This means that if the page had previously been marked
 * J9PORT_PAGE_PROTECT_READ and j9shmem_protect is called with J9PORT_PAGE_PROTECT_WRITE, it will no longer be readable.
 *
 * @param[in] portLibrary the Port Library.
 * @param[in] cacheDirName The name of the cache directory
 * @param[in] groupPerm Group permissions to open the cache directory
 * @param[in] address Pointer to the shared memory region.
 * @param[in] length The size of memory in bytes spanning the region in which we want to set protection
 * @param[in] flags The specified protection to apply to the pages in the specified interval
 * \arg J9PORT_PAGE_PROTECT_NONE accessing the memory will cause a segmentation violation
 * \arg J9PORT_PAGE_PROTECT_READ reading the memory is allowed
 * \arg J9PORT_PAGE_PROTECT_WRITE writing to the memory is allowed
 * \arg J9PORT_PAGE_PROTECT_EXEC executing code in the memory is allowed
 *
 * @return 0 on success, non-zero on failure.
 * 	\arg J9PORT_PAGE_PROTECT_NOT_SUPPORTED the functionality is not supported on this platform
 */
intptr_t
j9shmem_protect(struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, void *address, uintptr_t length, uintptr_t flags)
{
	return J9PORT_PAGE_PROTECT_NOT_SUPPORTED;
}
 
/**
 * Returns the minimum granularity in bytes on which permissions can be set for a memory region containing the address provided.
 *
 * @param[in] portLibrary the Port Library.
 * @param[in] cacheDirName The name of the cache directory
 * @param[in] groupPerm Group permissions to open the cache directory
 * @param[in] address  an address that lies within the region of memory the caller is interested in.
 * @return 0 on error, the minimum size of region on which we can control permissions size on success.
 */
uintptr_t
j9shmem_get_region_granularity(struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, void *address)
{
	return 0;
}

/**
 * Return the platform specific shmid value from the handle, or zero if there is no shmid available.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Shared memory handle.
 *
 * @return the shmid value from the handle, or zero if there is no shmid available
 */
int32_t
j9shmem_getid (struct J9PortLibrary *portLibrary, struct j9shmem_handle* handle)
{
	return 0;
}
