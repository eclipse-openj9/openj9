/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
 * @brief System information
 */
#include "j9port.h"
#include <string.h>


/**
 * Determine if DLPAR (i.e. the ability to change number of CPUs and amount of memory dynamically)
 * is enabled on this platform.
 *
 * @param[in] portLibrary The port library.
 *
 * @return 1 if DLPAR is supported, otherwise 0.
 */
uintptr_t
j9sysinfo_DLPAR_enabled(struct J9PortLibrary *portLibrary)
{
	return FALSE;
}

/**
 * Determine the character used to separate entries on the classpath.
 *
 * @param[in] portLibrary The port library.
 *
 * @return the classpathSeparator character.
 */
uint16_t
j9sysinfo_get_classpathSeparator(struct J9PortLibrary *portLibrary )
{
	return ';';
}

/**
 * PortLibrary shutdown.
 *
 * This function is called during shutdown of the portLibrary.  Any resources that were created by @ref j9sysinfo_startup
 * should be destroyed here.
 *
 * @param[in] portLibrary The port library.
 *
 * @note Most implementations will be empty.
 */
void
j9sysinfo_shutdown(struct J9PortLibrary *portLibrary)
{
}

/**
 * PortLibrary startup.
 *
 * This function is called during startup of the portLibrary.  Any resources that are required for
 * the system information operations may be created here.  All resources created here should be destroyed
 * in @ref j9sysinfo_shutdown.
 *
 * @param[in] portLibrary The port library.
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg J9PORT_ERROR_STARTUP_SYSINFO
 *
 * @note Most implementations will simply return success.
 */
int32_t
j9sysinfo_startup(struct J9PortLibrary *portLibrary)
{
	return 0;
}

/**
 * Determine if the platform has weak memory consistency behaviour.
 * 
 * @param[in] portLibrary The port library.
 *
 * @return 1 if weak memory consistency, 0 otherwise.
 */
uintptr_t
j9sysinfo_weak_memory_consistency(struct J9PortLibrary *portLibrary)
{
	return FALSE;
}
/**
 * Determine the maximum number of CPUs on this platform
 *
 * @param[in] portLibrary The port library.
 *
 * @return The maximum number of supported CPUs..
 */
uintptr_t
j9sysinfo_DLPAR_max_CPUs(struct J9PortLibrary *portLibrary)
{
	return portLibrary->sysinfo_get_number_CPUs_by_type(portLibrary, J9PORT_CPU_ONLINE);
}

/**
 * Determine the collective processing capacity available to the VM
 * in units of 1% of a physical processor. In environments without
 * some kind of virtual partitioning, this will simply be the number
 * of CPUs * 100.
 *
 * @param[in] portLibrary The port library.
 *
 * @return The processing capacity available to the VM.
 */
uintptr_t
j9sysinfo_get_processing_capacity(struct J9PortLibrary *portLibrary)
{
	return portLibrary->sysinfo_get_number_CPUs_by_type(portLibrary, J9PORT_CPU_ONLINE) * 100;
}

/**
 * Determine CPU type and features.
 *
 * @param[in] portLibrary The port library.
 * @param[out] desc pointer to the struct that will contain the CPU type and features.
 *              - desc will still be initialized if there is a failure.
 *
 * @return 0 on success, -1 on failure
 */
intptr_t
j9sysinfo_get_processor_description(struct J9PortLibrary *portLibrary, J9ProcessorDesc *desc)
{
	return -1;
}

/**
 * Determine if a CPU feature is present.
 *
 * @param[in] portLibrary The port library.
 * @param[in] desc The struct that will contain the CPU type and features.
 * @param[in] feature The feature to check (see j9port.h for list of features J9PORT_{PPC,S390,PPC}_FEATURE_*)
 *
 * @return TRUE if feature is present, FALSE otherwise.
 */
BOOLEAN
j9sysinfo_processor_has_feature(struct J9PortLibrary *portLibrary, J9ProcessorDesc *desc, uint32_t feature)
{
	return FALSE;
}

/**
 * Retrieve hardware information such as model. The information is returned in 
 * an ASCII string.
 *
 * @param portLibrary instance of port library
 * @param infoType A number representing the information that is being 
 * requested (e.g., J9PORT_SYSINFO_GET_HW_INFO_MODEL for hw model)
 * @param buf buffer where string containing the information requested will 
 * be written to
 * @param bufLen Length of buf
 *
 * @return J9PORT_SYSINFO_GET_HW_INFO_SUCCESS in case of success. Any other 
 * value represents a failure.
 */
int32_t
j9sysinfo_get_hw_info(struct J9PortLibrary *portLibrary, uint32_t infoType,
	char * buf, uint32_t bufLen)
{
	return J9PORT_SYSINFO_GET_HW_INFO_NOT_AVAILABLE;
}


/**
 * Return information about the CPU caches, such as number of levels, cache sizes and types, lines size, etc.
 * @param[in] portLibrary The Port Library instance
 * @param[in] query Contains parameters for the query. Contents are not changed. Unused fields should be set to 0.
 * @return requested information: may be integer or bit-map data.
 */
IDATA
j9sysinfo_get_cache_info(struct J9PortLibrary *portLibrary, struct const J9CacheInfoQuery * query) {
	return J9PORT_ERROR_SYSINFO_NOT_SUPPORTED;
}
