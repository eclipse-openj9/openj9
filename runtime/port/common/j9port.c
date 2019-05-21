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
 * @brief Port Library
 */
#include <string.h>
#include "j9cfg.h"
#include "j9port.h"
#include "portpriv.h"
#include "j9portpg.h"
#include "ut_j9prt.h"

/*
 * Create the port library initialization structure
 * All minor versions append to the end of this table
 */

static J9PortLibrary MasterPortLibraryTable = {
	{NULL}, /* omrPortLibrary */
	{J9PORT_MAJOR_VERSION_NUMBER, J9PORT_MINOR_VERSION_NUMBER, 0, J9PORT_CAPABILITY_MASK}, /* portVersion */
	NULL, /* portGlobals */
	j9port_shutdown_library, /* port_shutdown_library */
	j9port_isFunctionOverridden, /* port_isFunctionOverridden */
	j9sysinfo_startup,
	j9sysinfo_shutdown,
	j9sysinfo_get_classpathSeparator, /* sysinfo_get_classpathSeparator */
	j9sysinfo_get_processor_description, /* sysinfo_get_processor_description */
	j9sysinfo_processor_has_feature, /* sysinfo_processor_has_feature */
	j9sysinfo_get_hw_info, /* sysinfo_get_hw_info */
	j9sysinfo_get_cache_info, /* sysinfo_get_cache_info */
	j9sock_startup, /* sock_startup */
	j9sock_shutdown, /* sock_shutdown */
	j9sock_htons, /* sock_htons */
	j9sock_write, /* sock_write */
	j9sock_sockaddr, /* sock_sockaddr */
	j9sock_read, /* sock_read */
	j9sock_socket, /* sock_socket */
	j9sock_close, /* sock_close */
	j9sock_connect, /* sock_connect */
	j9sock_inetaddr, /* sock_inetaddr */
	j9sock_gethostbyname, /* sock_gethostbyname */
	j9sock_hostent_addrlist, /* sock_hostent_addrlist */
	j9sock_sockaddr_init, /* sock_sockaddr_init */
	j9sock_linger_init, /* sock_linger_init */
	j9sock_setopt_linger, /* sock_setopt_linger */
	j9gp_startup, /* gp_startup */
	j9gp_shutdown, /* gp_shutdown */
	j9gp_protect, /* gp_protect */
	j9gp_register_handler, /* gp_register_handler */
	j9gp_info, /* gp_info */
	j9gp_info_count, /* gp_info_count */
	NULL, /* gp_handler_function DANGER: only initialized on SEH platforms, and is done in j9gp.c */
	NULL, /* self_handle */
	j9ipcmutex_startup, /* ipcmutex_startup */
	j9ipcmutex_shutdown, /* ipcmutex_shutdown */
	j9ipcmutex_acquire, /* ipcmutex_acquire */
	j9ipcmutex_release, /* ipcmutex_release */
	j9sysinfo_DLPAR_enabled, /* sysinfo_DLPAR_enabled */
	j9sysinfo_DLPAR_max_CPUs, /* sysinfo_DLPAR_max_CPUs */
	j9sysinfo_weak_memory_consistency, /* sysinfo_weak_memory_consistency */
	j9sock_htonl, /* sock_htonl */
	j9sock_bind, /* sock_bind */
	j9sock_accept, /* sock_accept */
	j9sock_shutdown_input, /* sock_shutdown_input */
	j9sock_shutdown_output, /* sock_shutdown_output */
	j9sock_listen, /* sock_listen */
	j9sock_ntohl, /* sock_ntohl */
	j9sock_ntohs, /* sock_ntohs */
	j9sock_getpeername, /* sock_getpeername */
	j9sock_getsockname, /* sock_getsockname */
	j9sock_readfrom, /* sock_readfrom */
	j9sock_select, /* sock_select */
	j9sock_writeto, /* sock_writeto */
	j9sock_inetntoa, /* sock_inetntoa */
	j9sock_gethostbyaddr, /* sock_gethostbyaddr */
	j9sock_gethostname, /* sock_gethostname */
	j9sock_hostent_aliaslist, /* sock_hostent_aliaslist */
	j9sock_hostent_hostname, /* sock_hostent_hostname */
	j9sock_sockaddr_port, /* sock_sockaddr_port */
	j9sock_sockaddr_address, /* sock_sockaddr_address */
	j9sock_fdset_init, /* sock_fdset_init */
	j9sock_fdset_size, /* sock_fdset_size */
	j9sock_timeval_init, /* sock_timeval_init */
	j9sock_getopt_int, /* sock_getopt_int */
	j9sock_setopt_int, /* sock_setopt_int */
	j9sock_getopt_bool, /* sock_getopt_bool */
	j9sock_setopt_bool, /* sock_setopt_bool */
	j9sock_getopt_byte, /* sock_getopt_byte */
	j9sock_setopt_byte, /* sock_setopt_byte */
	j9sock_getopt_linger, /* sock_getopt_linger */
	j9sock_getopt_sockaddr, /* sock_getopt_sockaddr */
	j9sock_setopt_sockaddr, /* sock_setopt_sockaddr */
	j9sock_setopt_ipmreq, /* sock_setopt_ipmreq */
	j9sock_linger_enabled, /* sock_linger_enabled */
	j9sock_linger_linger, /* sock_linger_linger */
	j9sock_ipmreq_init, /* sock_ipmreq_init */
	j9sock_setflag, /* sock_setflag */
	j9sock_freeaddrinfo, /* sock_freeaddrinfo */
	j9sock_getaddrinfo, /* sock_getaddrinfo */
	j9sock_getaddrinfo_address, /* sock_getaddrinfo_address */
	j9sock_getaddrinfo_create_hints, /* sock_getaddrinfo_create_hints */
	j9sock_getaddrinfo_family, /* sock_getaddrinfo_family */
	j9sock_getaddrinfo_length, /* sock_getaddrinfo_length */
	j9sock_getaddrinfo_name, /* sock_getaddrinfo_name */
	j9sock_getnameinfo, /* sock_getnameinfo */
	j9sock_ipv6_mreq_init, /* sock_ipv6_mreq_init */
	j9sock_setopt_ipv6_mreq, /* sock_setopt_ipv6_mreq */
	j9sock_sockaddr_address6, /* sock_sockaddr_address6 */
	j9sock_sockaddr_family, /* sock_sockaddr_family */
	j9sock_sockaddr_init6, /* sock_sockaddr_init6 */
	j9sock_socketIsValid, /* sock_socketIsValid */
	j9sock_select_read, /* sock_select_read */
	j9sock_set_nonblocking, /* sock_set_nonblocking */
	j9sock_error_message, /* sock_error_message */
	j9sock_get_network_interfaces, /* sock_get_network_interfaces */
	j9sock_free_network_interface_struct, /* sock_free_network_interface_struct */
	j9sock_connect_with_timeout,  /* sock_connect_with_timeout */
	j9sock_fdset_zero, /* sock_fdset_zero */
	j9sock_fdset_set, /* sock_fdset_set */
	j9sock_fdset_clr, /* sock_fdset_clr */
	j9sock_fdset_isset, /* sock_fdset_isset */
	j9shsem_params_init, /* shsem_parameters_init */
	j9shsem_startup, /* shsem_startup */
	j9shsem_shutdown, /* shsem_shutdown */
	j9shsem_open, /* shsem_open */
	j9shsem_post, /* shsem_post */
	j9shsem_wait, /* shsem_wait */
	j9shsem_getVal, /* shsem_getVal */
	j9shsem_setVal, /* shsem_setVal */
	j9shsem_close, /* shsem_close */
	j9shsem_destroy, /* shsem_destroy */
	j9shsem_deprecated_startup, /* shsem_deprecated_startup */
	j9shsem_deprecated_shutdown, /* shsem_deprecated_shutdown */
	j9shsem_deprecated_open, /* shsem_deprecated_open */
	j9shsem_deprecated_openDeprecated, /* shsem_deprecated_openDeprecated */
	j9shsem_deprecated_post, /* shsem_deprecated_post */
	j9shsem_deprecated_wait, /* shsem_deprecated_wait */
	j9shsem_deprecated_getVal, /* shsem_deprecated_getVal */
	j9shsem_deprecated_setVal, /* shsem_deprecated_setVal */
	j9shsem_deprecated_handle_stat, /* shsem_deprecated_handle_stat */
	j9shsem_deprecated_close, /* shsem_deprecated_close */
	j9shsem_deprecated_destroy, /* shsem_deprecated_destroy */
	j9shsem_deprecated_destroyDeprecated, /* shsem_deprecated_destroyDeprecated */
	j9shsem_deprecated_getid, /* shsem_deprecated_getid */
	j9shmem_startup, /* shmem_startup */
	j9shmem_shutdown, /* shmem_shutdown */
	j9shmem_open, /*shmem_open*/
	j9shmem_openDeprecated, /*shmem_openDeprecated*/
	j9shmem_attach, /*shmem_attach*/
	j9shmem_detach, /*shmem_detach*/
	j9shmem_close, /*shmem_close*/
	j9shmem_destroy, /*shmem_destroy */
	j9shmem_destroyDeprecated, /* shmem_destroyDeprecated */
	j9shmem_findfirst, /*shmem_findfirst*/
	j9shmem_findnext, /* shmem_findnext*/
	j9shmem_findclose, /* shmem_findclose*/
	j9shmem_stat, /* shmem_stat */
	j9shmem_statDeprecated, /* shmem_statDeprecated */
	j9shmem_handle_stat, /* shmem_handle_stat */
	j9shmem_getDir, /* shmem_getDir */
	j9shmem_createDir, /* shmem_createDir */
	j9shmem_getFilepath, /* shmem_getFilepath */
	j9shmem_protect, /* shmem_protect */
	j9shmem_get_region_granularity, /* shmem_get_region_granularity */
	j9shmem_getid, /* shmem_getid */
	j9sysinfo_get_processing_capacity, /* sysinfo_get_processing_capacity */
	j9port_init_library, /* port_init_library */
	j9port_startup_library, /* port_startup_library */
	j9port_create_library, /* port_create_library */
	j9hypervisor_startup, /* hypervisor_startup */
	j9hypervisor_shutdown, /* hypervisor_shutdown */
	j9hypervisor_hypervisor_present, /* hyperevisor_present */
	j9hypervisor_get_hypervisor_info,/* hypervisor_get_hypervisor_info */
	j9hypervisor_get_guest_processor_usage, /* hypervisor_get_guest_processor_usage */
	j9hypervisor_get_guest_memory_usage, /* hypervisor_get_guest_memory_usage */
	j9process_create, /* process_create */
	j9process_waitfor, /* process_waitfor */
	j9process_terminate, /* process_terminate */
	j9process_write, /* process_write */
	j9process_read, /* process_read */
	j9process_get_available, /* process_get_available */
	j9process_close, /* process_close */
	j9process_getStream, /* process_getStream */
	j9process_isComplete, /* process_isComplete */
	j9process_get_exitCode, /* process_get_exitCode */
#if defined(J9VM_PORT_RUNTIME_INSTRUMENTATION)
	j9ri_params_init,
	j9ri_initialize,
	j9ri_deinitialize,
	j9ri_enable,
	j9ri_disable,
	j9ri_enableRISupport,
	j9ri_disableRISupport,
#endif /* J9VM_PORT_RUNTIME_INSTRUMENTATION */
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	j9gs_params_init,
	j9gs_enable,
	j9gs_disable,
	j9gs_initialize,
	j9gs_deinitialize,
	j9gs_isEnabled,
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
	j9port_control
};

/**
 * Initialize the port library.
 * 
 * Given a pointer to a port library and the required version,
 * populate the port library table with the appropriate functions
 * and then call the startup function for the port library.
 * 
 * @param[in] portLibrary The port library.
 * @param[in] version The required version of the port library.
 * @param[in] size Size of the port library.
 *
 * @return 0 on success, negative return value on failure
 */
int32_t
j9port_init_library(struct J9PortLibrary *portLibrary, struct J9PortLibraryVersion *version, uintptr_t size)
{
	/* return value of 0 is success */
	int32_t rc;

	rc = j9port_create_library(portLibrary, version, size);
	if ( rc == 0 ) {
		return j9port_startup_library(portLibrary);
	}
	return rc;
}

/**
 * PortLibrary shutdown.
 *
 * Shutdown the port library, de-allocate resources required by the components of the portlibrary.
 * Any resources that were created by @ref j9port_startup_library should be destroyed here.
 *
 * @param[in] portLibrary The portlibrary.
 *
 * @return 0 on success, negative return code on failure
 */
int32_t
j9port_shutdown_library(struct J9PortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	/* Ensure that the current thread is attached.
	 * If it is not, there will be a crash trying to using
	 * thread functions in the following shutdown functions. 
	 */
	omrthread_t attached_thread;
	intptr_t rc = omrthread_attach_ex(&attached_thread, J9THREAD_ATTR_DEFAULT);
	if ( 0 != rc ) {
		return (int32_t) rc;
	}

	/* If portGlobals failed to allocate, then other submodules can't be
	 * safely shutdown.
	 */
	if (NULL != portLibrary->portGlobals) {
		portLibrary->shmem_shutdown(portLibrary);
		portLibrary->shsem_shutdown(portLibrary);
		portLibrary->shsem_deprecated_shutdown(portLibrary);
		portLibrary->gp_shutdown(portLibrary);

		/* Shutdown the socket library before the tty, so it can write to the tty if required */
		portLibrary->sock_shutdown(portLibrary);
		portLibrary->ipcmutex_shutdown(portLibrary);

		portLibrary->hypervisor_shutdown(portLibrary);

		portLibrary->sysinfo_shutdown(portLibrary);

		omrmem_free_memory(portLibrary->portGlobals);
		portLibrary->portGlobals = NULL;
	}
	
	omrport_shutdown_library();

	omrthread_detach(attached_thread);

	/* Last thing to do.  If this port library was self allocated free this memory */
	if (NULL != portLibrary->self_handle) {
		j9mem_deallocate_portLibrary(portLibrary);
	}

	return (int32_t) 0;
}

/**
 * Create the port library.
 * 
 * Given a pointer to a port library and the required version,
 * populate the port library table with the appropriate functions
 * 
 * @param[in] portLibrary The port library.
 * @param[in] version The required version of the port library.
 * @param[in] size Size of the port library.
 *
 * @return 0 on success, negative return value on failure
 * @note The portlibrary version must be compatible with the that which we are compiled against
 */

int32_t
j9port_create_library(struct J9PortLibrary *portLibrary, struct J9PortLibraryVersion *version, uintptr_t size)
{
	uintptr_t versionSize = j9port_getSize(version);

	if (J9PORT_MAJOR_VERSION_NUMBER != version->majorVersionNumber) {
		/* set this so diagnostic code can query it */
		portLibrary->portVersion.majorVersionNumber = J9PORT_MAJOR_VERSION_NUMBER;
		return J9PORT_ERROR_INIT_WRONG_MAJOR_VERSION;
	}

	if (versionSize > size) {
		return J9PORT_ERROR_INIT_WRONG_SIZE;
	}

	/* Ensure required functionality is there */
	if ((version->capabilities & J9PORT_CAPABILITY_MASK) != version->capabilities) {
		return J9PORT_ERROR_INIT_WRONG_CAPABILITIES;
	}

	/* Null and initialize the table passed in */
	memset(portLibrary, 0, size);
	memcpy(portLibrary, &MasterPortLibraryTable, versionSize);

	/* Reset capabilities to be what is actually there, not what was requested */
	portLibrary->portVersion.majorVersionNumber = version->majorVersionNumber;
	portLibrary->portVersion.minorVersionNumber = version->minorVersionNumber;
	portLibrary->portVersion.capabilities = J9PORT_CAPABILITY_MASK;

	if (0 != omrport_create_library(OMRPORT_FROM_J9PORT(portLibrary), sizeof(OMRPortLibrary))) {
		return J9PORT_ERROR_INIT_WRONG_SIZE;
	}

	return 0;
}

/**
 * PortLibrary startup.
 *
 * Start the port library, allocate resources required by the components of the portlibrary.
 * All resources created here should be destroyed in @ref j9port_shutdown_library.
 *
 * @param[in] portLibrary The portlibrary.
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg J9PORT_ERROR_STARTUP_THREAD
 * \arg J9PORT_ERROR_STARTUP_MEM
 * \arg J9PORT_ERROR_STARTUP_TLS
 * \arg J9PORT_ERROR_STARTUP_TLS_ALLOC
 * \arg J9PORT_ERROR_STARTUP_TLS_MUTEX
 * \arg J9PORT_ERROR_STARTUP_ERROR
 * \arg J9PORT_ERROR_STARTUP_CPU
 * \arg J9PORT_ERROR_STARTUP_VMEM
 * \arg J9PORT_ERROR_STARTUP_FILE
 * \arg J9PORT_ERROR_STARTUP_TTY
 * \arg J9PORT_ERROR_STARTUP_TTY_HANDLE
 * \arg J9PORT_ERROR_STARTUP_TTY_CONSOLE
 * \arg J9PORT_ERROR_STARTUP_MMAP
 * \arg J9PORT_ERROR_STARTUP_IPCMUTEX
 * \arg J9PORT_ERROR_STARTUP_NLS
 * \arg J9PORT_ERROR_STARTUP_SOCK
 * \arg J9PORT_ERROR_STARTUP_TIME
 * \arg J9PORT_ERROR_STARTUP_GP
 * \arg J9PORT_ERROR_STARTUP_EXIT
 * \arg J9PORT_ERROR_STARTUP_SYSINFO
 * \arg J9PORT_ERROR_STARTUP_SL
 * \arg J9PORT_ERROR_STARTUP_STR
 * \arg J9PORT_ERROR_STARTUP_SHSEM
 * \arg J9PORT_ERROR_STARTUP_SHMEM
 * \arg J9PORT_ERROR_STARTUP_SIGNAL
 *
 * @note The port library memory is deallocated if it was created by @ref j9port_allocate_library
 */
int32_t
j9port_startup_library(struct J9PortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;

	Assert_PRT_true(omrthread_self() != NULL);

	rc = omrport_startup_library(OMRPORTLIB);
	if (0 != rc) {
		goto cleanup;
	}

	portLibrary->portGlobals = omrmem_allocate_memory(sizeof(J9PortLibraryGlobalData), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == portLibrary->portGlobals) {
		rc = J9PORT_ERROR_STARTUP_MEM;
		goto cleanup;
	}
	memset(portLibrary->portGlobals, 0, sizeof(J9PortLibraryGlobalData));

	/* Check for omr port startup failure after allocating port globals. Globals must be allocated. */
	if (0 != rc) {
		goto cleanup;
	}

	rc = portLibrary->sysinfo_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}

	rc = portLibrary->ipcmutex_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}

	rc = portLibrary->sock_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}

	rc = portLibrary->gp_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}

	/* Moved the Startup of Hypervisor early to fill the hypervisor related details
	 * These details are required for decision making based on if JVM is running
	 * in a virtualized environment or not
	 */
	rc = portLibrary->hypervisor_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}

	rc = portLibrary->shsem_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}

	rc = portLibrary->shsem_deprecated_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}
	
	rc = portLibrary->shmem_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}

	return rc;

cleanup:
	/* TODO: should call shutdown, but need to make all shutdown functions
	 *  safe if the corresponding startup was not called.  No worse than the existing
	 * code 
	 */

	/* If this port library was self allocated free this memory */
	if (NULL != portLibrary->self_handle) {
		j9mem_deallocate_portLibrary(portLibrary);
	}
	return rc;
}

/**
 * Determine the size of the port library.
 * 
 * Given a port library version, return the size of the structure in bytes
 * required to be allocated.
 * 
 * @param[in] version The J9PortLibraryVersion structure.
 *
 * @return size of port library on success, zero on failure
 *
 * @note The portlibrary version must be compatible with the that which we are compiled against
 */
uintptr_t 
j9port_getSize(struct J9PortLibraryVersion *version)
{
	/* Can't initialize a structure that is not understood by this version of the port library	 */
	if (J9PORT_MAJOR_VERSION_NUMBER != version->majorVersionNumber) {
		return 0;
	}

	/* The size of the portLibrary table is determined by the majorVersion number
	 * and the presence/absence of the J9PORT_CAPABILITY_STANDARD capability
	 */
	if (0 != (version->capabilities & J9PORT_CAPABILITY_STANDARD)) {
		return sizeof(J9PortLibrary);
	} else {
		return offsetof(J9PortLibrary, ipcmutex_release)+sizeof(void *);
	}
}
/**
 * Determine the version of the port library.
 * 
 * Given a port library return the version of that instance.
 * 
 * @param[in] portLibrary The port library.
 * @param[in,out] version The J9PortLibraryVersion structure to be populated.
 *
 * @return 0 on success, negative return value on failure
 * @note If portLibrary is NULL, version is populated with the version in the linked DLL
 */
int32_t 
j9port_getVersion(struct J9PortLibrary *portLibrary, struct J9PortLibraryVersion *version)
{
	if (NULL == version) {
		return -1;
	}

	if (portLibrary) {
		version->majorVersionNumber = portLibrary->portVersion.majorVersionNumber;
		version->minorVersionNumber = portLibrary->portVersion.minorVersionNumber;
		version->capabilities = portLibrary->portVersion.capabilities;
	} else {
		version->majorVersionNumber = J9PORT_MAJOR_VERSION_NUMBER;
		version->minorVersionNumber = J9PORT_MINOR_VERSION_NUMBER;
		version->capabilities = J9PORT_CAPABILITY_MASK;
	}

	return 0;
}
/**
 * Determine port library compatibility.
 * 
 * Given the minimum version of the port library that the application requires determine
 * if the current port library meets that requirements.
 *
 * @param[in] expectedVersion The version the application requires as a minimum.
 *
 * @return 1 if compatible, 0 if not compatible
 */
int32_t 
j9port_isCompatible(struct J9PortLibraryVersion *expectedVersion)
{
	/* Position of functions, signature of functions.
	 * Major number incremented when existing functions change positions or signatures
	 */
	if (J9PORT_MAJOR_VERSION_NUMBER != expectedVersion->majorVersionNumber) {
		return 0;
	}

	/* Size of table, it's ok to have more functions at end of table that are not used.
	 * Minor number incremented when new functions added to the end of the table
	 */
	if (J9PORT_MINOR_VERSION_NUMBER < expectedVersion->minorVersionNumber) {
		return 0;
	}

	/* Functionality supported */
	return ((J9PORT_CAPABILITY_MASK & expectedVersion->capabilities) == expectedVersion->capabilities);
}

/**
 * Query the port library.
 * 
 * Given a pointer to the port library and an offset into the table determine if
 * the function at that offset has been overridden from the default value expected by J9. 
 *
 * @param[in] portLibrary The port library.
 * @param[in] offset The offset of the function to be queried.
 * 
 * @return 1 if the function is overridden, else 0.
 *
 * j9port_isFunctionOverridden(portLibrary, offsetof(J9PortLibrary, mem_allocate_memory));
 */
int32_t
j9port_isFunctionOverridden(struct J9PortLibrary *portLibrary, uintptr_t offset)
{
	uintptr_t requiredSize;

	requiredSize = j9port_getSize(&(portLibrary->portVersion));
	if (requiredSize < offset)  {
		return 0;
	}

	return *((uintptr_t*) &(((uint8_t*) portLibrary)[offset])) != *((uintptr_t*) &(((uint8_t*) &MasterPortLibraryTable)[offset]));
}
/**
 * Allocate a port library.
 * 
 * Given a pointer to the required version of the port library allocate and initialize the structure.
 * The startup function is not called (@ref j9port_startup_library) allowing the application to override
 * any functions they desire.  In the event @ref j9port_startup_library fails when called by the application 
 * the port library memory will be freed.  
 * 
 * @param[in] version The required version of the port library.
 * @param[out] portLibrary Pointer to the allocated port library table.
 *
 * @return 0 on success, negative return value on failure
 *
 * @note portLibrary will be NULL on failure
 * @note The portlibrary version must be compatible with the that which we are compiled against
 * @note @ref j9port_shutdown_library will deallocate this memory as part of regular shutdown
 */
int32_t
j9port_allocate_library(struct J9PortLibraryVersion *version, struct J9PortLibrary **portLibrary )
{
	uintptr_t size = j9port_getSize(version);
	J9PortLibrary *portLib;
	int32_t rc;

	/* Allocate the memory */
	*portLibrary = NULL;
	if (0 == size) {
		return -1;
	} else {
		portLib = j9mem_allocate_portLibrary(size);
		if (NULL == portLib) {
			return -1;
		}
	}

	/* Initialize with default values */
	rc = j9port_create_library(portLib, version, size);
	if (0 == rc) {
		/* Record this was self allocated */
		portLib->self_handle = portLib;
		*portLibrary = portLib;
	} else {
		j9mem_deallocate_portLibrary(portLib);
	}
	return rc;
}

