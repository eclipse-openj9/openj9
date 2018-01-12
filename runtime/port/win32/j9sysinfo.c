/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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

#include <pdh.h>
#include <pdhmsg.h>
#include <stdio.h>
#include <windows.h>

#include "portpriv.h"
#include "j9portpg.h"
#include "omrportptb.h"
#include "j9sysinfo_helpers.h"
#include "ut_j9prt.h"

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
	PPG_sysL1DCacheLineSize = -1;
	return 0;
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
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	return omrsysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE);
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
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	return omrsysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE) * 100;
}

intptr_t
j9sysinfo_get_processor_description(struct J9PortLibrary *portLibrary, J9ProcessorDesc *desc)
{
	intptr_t rc = -1;
	Trc_PRT_sysinfo_get_processor_description_Entered(desc);
	
	if (NULL != desc) {
		memset(desc, 0, sizeof(J9ProcessorDesc));
		rc = getX86Description(portLibrary, desc);
	}
	
	Trc_PRT_sysinfo_get_processor_description_Exit(rc);
	return rc;
}

BOOLEAN
j9sysinfo_processor_has_feature(struct J9PortLibrary *portLibrary, J9ProcessorDesc *desc, uint32_t feature)
{
	BOOLEAN rc = FALSE;
	Trc_PRT_sysinfo_processor_has_feature_Entered(desc, feature);

	if ((NULL != desc)
	&& (feature < (J9PORT_SYSINFO_FEATURES_SIZE * 32))
	) {
		uint32_t featureIndex = feature / 32;
		uint32_t featureShift = feature % 32;

		rc = J9_ARE_ALL_BITS_SET(desc->features[featureIndex], 1 << featureShift);
	}

	Trc_PRT_sysinfo_processor_has_feature_Exit((uintptr_t)rc);
	return rc;
}

int32_t
j9sysinfo_get_hw_info(struct J9PortLibrary *portLibrary, uint32_t infoType,
	char * buf, uint32_t bufLen)
{
	return J9PORT_SYSINFO_GET_HW_INFO_NOT_AVAILABLE;
}

/**
 * Query the operating system for a list of cache descriptors.
 * Scan the list and create a bit mask of the types of caches at that level
 * @param [in]  portLibrary port library
 * @param [in]  procInfoPtr buffer containing descriptors
 * @param [in]  procInfoLength buffer length in bytes
 * @param [in]  cacheLevel which cache level to query. Must be >= 1
 * @param [in]  cacheType which type of cache level to query.
 * @return cache descriptor
 */
static CACHE_DESCRIPTOR const*
findCacheForTypeAndLevel(struct J9PortLibrary *portLibrary,
	SYSTEM_LOGICAL_PROCESSOR_INFORMATION *procInfoPtr,
	uint32_t procInfoLength, int32_t const cacheLevel,  const int32_t cacheType)
{
	SYSTEM_LOGICAL_PROCESSOR_INFORMATION *cursor = procInfoPtr;
	CACHE_DESCRIPTOR *cacheDesc = NULL;
	SYSTEM_LOGICAL_PROCESSOR_INFORMATION *endInfo = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION *) (((U_8 *) procInfoPtr) + procInfoLength);

	while ((NULL == cacheDesc)  && (cursor < endInfo)) {
		CACHE_DESCRIPTOR *temp = &(cursor->Cache);
		if ((temp->Level == cacheLevel) && (temp->LineSize > 0)) {
			switch (temp->Type)  {
			case CacheUnified: {
				if (J9_ARE_ANY_BITS_SET(cacheType,
					J9PORT_CACHEINFO_DCACHE | J9PORT_CACHEINFO_ICACHE | J9PORT_CACHEINFO_UCACHE)) {
					cacheDesc = temp;
				}
				break;
			case  CacheInstruction:
				if (J9_ARE_ANY_BITS_SET(cacheType, J9PORT_CACHEINFO_ICACHE)) {
					cacheDesc = temp;
				}
				break;
			case  CacheData:
				if (J9_ARE_ANY_BITS_SET(cacheType, J9PORT_CACHEINFO_DCACHE)) {
					cacheDesc = temp;
				}
				break;
			case  CacheTrace:
				if (J9_ARE_ANY_BITS_SET(cacheType, J9PORT_CACHEINFO_TCACHE)) {
					cacheDesc = temp;
				}
				break;
			default:
				break;
			}
			break;
			}
		}
		++cursor;
	}
	return cacheDesc;
}

/**
 * Query the operating system for a list of cache descriptors.
 * Scan the list and create a bit mask of the types of caches at that level
 * @param [in]  portLibrary port library
 * @param [in]  procInfoPtr buffer containing descriptors
 * @param [in]  procInfoLength buffer length in bytes
 * @param [in]  cacheLevel which cache level to query. Must be >= 1
 * @return bit mask of types in bytes
 */
static uint32_t
getCacheTypes(struct J9PortLibrary *portLibrary, SYSTEM_LOGICAL_PROCESSOR_INFORMATION *procInfoPtr,
	uint32_t procInfoLength, const uint32_t cacheLevel)
{
	SYSTEM_LOGICAL_PROCESSOR_INFORMATION *cursor = procInfoPtr;
	uint32_t result = 0;
	SYSTEM_LOGICAL_PROCESSOR_INFORMATION *endInfo = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION *) (((U_8 *) procInfoPtr) + procInfoLength);

	while (cursor < endInfo) {
		CACHE_DESCRIPTOR *temp = &(cursor->Cache);
		if ((temp->Level == cacheLevel) && (temp->LineSize > 0)) {
			BOOLEAN found = FALSE;
			switch (temp->Type)  {
			case CacheUnified:
				result |= J9PORT_CACHEINFO_UCACHE;
				if (1 == temp->Level) {
					PPG_sysL1DCacheLineSize = temp->LineSize;
				}
				break;
			case  CacheInstruction:
				result |= J9PORT_CACHEINFO_ICACHE;
				break;
			case  CacheData:
				result |= J9PORT_CACHEINFO_DCACHE;
				if (1 == temp->Level) {
					PPG_sysL1DCacheLineSize = temp->LineSize;
				}
				break;
			case  CacheTrace:
				result |= J9PORT_CACHEINFO_TCACHE;
				break;
			}
		}
		++cursor;
	}
	return result;
}

/**
 * Query the operating system for a list of cache descriptors.
 * Scan the list for the highest implemented level.
 * @param [in]  portLibrary port library
 * @param [in]  procInfoPtr buffer containing descriptors
 * @param [in]  procInfoLength buffer length in bytes
 * @return Highest level found
 */
static uint32_t
getCacheLevels(struct J9PortLibrary *portLibrary, SYSTEM_LOGICAL_PROCESSOR_INFORMATION *procInfoPtr,
	uint32_t procInfoLength)
{
	SYSTEM_LOGICAL_PROCESSOR_INFORMATION *cursor = procInfoPtr;
	uint32_t result = 0;
	SYSTEM_LOGICAL_PROCESSOR_INFORMATION *endInfo = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION *) (((U_8 *) procInfoPtr) + procInfoLength);

	while (cursor < endInfo) {
		CACHE_DESCRIPTOR *temp = &(cursor->Cache);
		if  (temp->LineSize > 0) {
			result = OMR_MAX(result, temp->Level);
		}
		++cursor;
	}
	return result;
}

int32_t
j9sysinfo_get_cache_info(struct J9PortLibrary *portLibrary, const J9CacheInfoQuery * query)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t result = J9PORT_ERROR_SYSINFO_NOT_SUPPORTED;
	SYSTEM_LOGICAL_PROCESSOR_INFORMATION procInfoBuff[8];
	SYSTEM_LOGICAL_PROCESSOR_INFORMATION *procInfoPtr = procInfoBuff;
	DWORD procInfoLength = sizeof(procInfoBuff);

	Trc_PRT_sysinfo_get_cache_info_enter(query->cmd, query->cpu, query->level, query->cacheType);
	if ((1 == query->level) && J9_ARE_ANY_BITS_SET(query->cacheType, J9PORT_CACHEINFO_DCACHE) &&
		(J9PORT_CACHEINFO_QUERY_LINESIZE == query->cmd) && (PPG_sysL1DCacheLineSize > 0)) {
		return PPG_sysL1DCacheLineSize;
	}
	if (!GetLogicalProcessorInformation(
		procInfoPtr,
		&procInfoLength)
		&& (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		) {
		procInfoPtr = omrmem_allocate_memory(procInfoLength, OMRMEM_CATEGORY_PORT_LIBRARY);
		Trc_PRT_sysinfo_get_cache_info_allocate(procInfoLength);

		if (NULL == procInfoPtr) {
			return J9PORT_ERROR_SYSINFO_OPFAILED;
		}
		if (!GetLogicalProcessorInformation(
			procInfoPtr,
			&procInfoLength)
			) {
			if (procInfoBuff != procInfoPtr) {
				omrmem_free_memory(procInfoPtr);
			}
			return J9PORT_ERROR_SYSINFO_OPFAILED;
		}
	}

	switch (query->cmd) {
	case J9PORT_CACHEINFO_QUERY_LINESIZE: {
		CACHE_DESCRIPTOR const* cacheDesc = findCacheForTypeAndLevel(portLibrary,
			procInfoPtr,
			procInfoLength,
			query->level,
			query->cacheType);
		if (NULL != cacheDesc) {
			result = cacheDesc->LineSize;
		}
	}
	break;
	case J9PORT_CACHEINFO_QUERY_CACHESIZE: {
		CACHE_DESCRIPTOR const* cacheDesc = findCacheForTypeAndLevel(portLibrary, procInfoPtr, procInfoLength,
			query->level,
			query->cacheType);
		if (NULL != cacheDesc) {
			result = cacheDesc->Size;
		}
	}
	break;
	case J9PORT_CACHEINFO_QUERY_TYPES:
		result =  getCacheTypes(portLibrary, procInfoPtr, procInfoLength, query->level);
		break;
	case J9PORT_CACHEINFO_QUERY_LEVELS:
		result =  getCacheLevels(portLibrary, procInfoPtr, procInfoLength);
		break;
	default:
		result = J9PORT_ERROR_SYSINFO_NOT_SUPPORTED;
		break;
	}
	if (procInfoBuff != procInfoPtr) {
		omrmem_free_memory(procInfoPtr);
	}
	if ((1 == query->level) && J9_ARE_ANY_BITS_SET(query->cacheType, J9PORT_CACHEINFO_DCACHE) &&
		(J9PORT_CACHEINFO_QUERY_LINESIZE == query->cmd)) {
		PPG_sysL1DCacheLineSize = result;
	}
	Trc_PRT_sysinfo_get_cache_info_exit(result);
	return result;
}
