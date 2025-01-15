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
 * @brief System information
 */

#if 0
#define ENV_DEBUG
#endif

#if (defined(LINUX) || defined(OSX)) && !defined(J9ZTPF)
#define _GNU_SOURCE
#endif /* (defined(LINUX) || defined(OSX)) && !defined(J9ZTPF) */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <limits.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#if defined(J9OS_I5)
#include "Xj9I5OSInterface.H"
#endif /* defined(J9OS_I5) */
#if !defined(J9ZOS390)
#include <sys/param.h>
#endif /* !defined(J9ZOS390) */
#include <sys/time.h>
#include <sys/resource.h>
#include <nl_types.h>
#include <langinfo.h>
#if !defined(USER_HZ) && !defined(J9ZTPF)
#define USER_HZ HZ
#endif /* !defined(USER_HZ) && !defined(J9ZTPF) */

#if defined(J9X86) || defined(J9HAMMER) || defined(S390) || defined(J9ZOS390)
#include "j9sysinfo_helpers.h"
#endif /* defined(J9X86) || defined(J9HAMMER) || defined(S390) || defined(J9ZOS390) */

#if defined(J9ZOS390)
#include "j9csrsi.h"
#endif /* defined(J9ZOS390) */

#if defined(LINUXPPC) || (defined(S390) && defined(LINUX) && !defined(J9ZTPF))
#include "auxv.h"
#include <strings.h>
#endif /* defined(LINUXPPC) || (defined(S390) && defined(LINUX) && !defined(J9ZTPF)) */

#if defined(AIXPPC)
#include <fcntl.h>
#include <sys/procfs.h>
#include <sys/systemcfg.h>
#endif /* defined(AIXPPC) */

/* Start copy from j9filetext.c */
/* __STDC_ISO_10646__ indicates that the platform wchar_t encoding is Unicode */
/* but older versions of libc fail to set the flag, even though they are Unicode */
#if defined(__STDC_ISO_10646__) || defined(LINUX)
#define J9VM_USE_MBTOWC
#else /* defined(__STDC_ISO_10646__) || defined(LINUX) */
#include "omriconvhelpers.h"
#endif /* defined(__STDC_ISO_10646__) || defined(LINUX) */

/* a2e overrides nl_langinfo to return ASCII strings. We need the native EBCDIC string */
#if defined(J9ZOS390) && defined(nl_langinfo)
#undef nl_langinfo
#endif /* defined(J9ZOS390) && defined(nl_langinfo) */

/* End copy from j9filetext.c */

#if defined(AIXPPC)
#if defined(J9OS_I5)
/* The PASE compiler does not support libperfstat.h */
#else /* defined(J9OS_I5) */
#include <libperfstat.h>
#endif /* defined(J9OS_I5) */

#include <sys/proc.h>

/* j9sysinfo_get_number_CPUs_by_type */
#include <sys/thread.h>
#include <sys/rset.h>
#endif /* defined(AIXPPC) */

#if defined(RS6000)
#include <sys/systemcfg.h>
#include <sys/sysconfig.h>
#include <assert.h>

#if defined(OMR_ENV_DATA64)
#define LIBC_NAME "/usr/lib/libc.a(shr_64.o)"
#else /* defined(OMR_ENV_DATA64) */
#define LIBC_NAME "/usr/lib/libc.a(shr.o)"
#endif /* defined(OMR_ENV_DATA64) */

/* **********
These definitions were copied from sys/dr.h in AIX 5.3, because this functionality does not
exist on AIX5.2. The code that uses these definitions does a runtime lookup to see if the
lpar_get_info function exists.
********* */

#define LPAR_INFO_FORMAT2       2 /* command for retrieving LPAR format2 info */
typedef struct lpar_info_format2_t {

        int      version;               /* version for this structure */
        int      pad0;

        uint64_t online_memory;         /* MB of currently online memory */
        uint64_t tot_dispatch_time;     /* Total lpar dispatch time in nsecs */
        uint64_t pool_idle_time;        /* Idle time of shared CPU pool nsecs*/
        uint64_t dispatch_latency;      /* Max latency inbetween dispatches */
                                        /* of this LPAR on physCPUS in nsecs */
        uint     lpar_flags;
#define LPAR_INFO2_CAPPED       0x01    /* Partition Capped */
#define LPAR_INFO2_AUTH_PIC     0x02    /* Authority granted for poolidle*/
#define LPAR_INFO2_SMT_ENABLED  0x04    /* SMT Enabled */

        uint     pcpus_in_sys;          /* # of active licensed physical CPUs
                                         * in system
                                         */
        uint     online_vcpus;          /* # of current online virtual CPUs */
        uint     online_lcpus;          /* # of current online logical CPUs */
        uint     pcpus_in_pool;         /* # physical CPUs in shared pool */
        uint     unalloc_capacity;      /* Unallocated Capacity available
                                         * in shared pool
                                         */
        uint     entitled_capacity;     /* Entitled Processor Capacity for this
                                         * partition
                                         */
        uint     variable_weight;       /* Variable Processor Capacity Weight */
        uint     unalloc_weight;        /* Unallocated Variable Weight available
                                         * for this partition
                                         */
        uint     min_req_vcpu_capacity; /* OS minimum required virtual processor
                                         * capacity.
                                         */

        ushort   group_id;              /* ID of a LPAR group/aggregation    */
        ushort   pool_id;               /* ID of a shared pool */
        char     pad1[36];              /* reserved for future */

} lpar_info_format2_t;

/* **********
These definitions were copied from sys/dr.h in AIX 6.1, because this functionality does not
exist on AIX5.2. The code that uses these definitions does a runtime lookup to see if the
lpar_get_info function exists.
********* */
typedef unsigned int ckey_t;
typedef unsigned short cid_t;

#define WPAR_INFO_FORMAT	3 /* command for retrieving WPAR format1 info */
typedef struct wpar_info_format_t {

        int   version;                  /* version for this structure */
        ckey_t   wpar_ckey;             /* WPAR static identifier */
        cid_t    wpar_cid;              /* WPAR dynamic identifier */
        uint     wpar_flags;
#define WPAR_INFO_CPU_RSET       0x01   /* WPAR restricted to CPU resource set */
#define WPAR_INFO_PROCESS        0x02   /* Denotes process runs inside WPAR */
#define WPAR_INFO_MOBILE         0x04   /* Denotes WPAR is checkpoint/restartable */
#define WPAR_INFO_APP            0x08   /* Denotes WPAR is application WPAR */
        uint     partition_cpu_limit;   /* Number of CPUs in partition rset or 0 */
        int      percent_cpu_limit;     /* CPU limit in 100ths of % - 1..10000 */
        char     pad1[32];              /* Reserved for future use */
} wpar_info_format_t;

#endif /* defined(RS6000) */

#if defined(LINUX) && !defined(J9ZTPF)
#include <sys/sysinfo.h>
#include <sched.h>
#endif /* defined(LINUX) && !defined(J9ZTPF) */

#include <unistd.h>

#include "portpriv.h"
#include "j9portpg.h"
#include "omrportptb.h"
#include "ut_j9prt.h"

#if defined(J9ZOS390)
#include <sys/ps.h>
#include <sys/types.h>
#include "atoe.h"

#if !defined(PATH_MAX)
/* This is a somewhat arbitrarily selected fixed buffer size. */
#define PATH_MAX 1024
#endif /* !defined(PATH_MAX) */
#endif /* defined(J9ZOS390) */

#define JIFFIES			100
#define USECS_PER_SEC	1000000
#define TICKS_TO_USEC	((uint64_t)(USECS_PER_SEC/JIFFIES))

static int32_t getCacheLevels(struct J9PortLibrary *portLibrary, const int32_t cpu);
static int32_t getCacheTypes(struct J9PortLibrary *portLibrary, const int32_t cpu, const int32_t level);
static int32_t getCacheSize(struct J9PortLibrary *portLibrary,
	const int32_t cpu, const int32_t level, const int32_t cacheType, const J9CacheQueryCommand query);

uint16_t
j9sysinfo_get_classpathSeparator(struct J9PortLibrary *portLibrary)
{
	return ':';
}

uintptr_t
j9sysinfo_DLPAR_enabled(struct J9PortLibrary *portLibrary)
{
#if defined(RS6000) && !defined(J9OS_I5)
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	/* variables for use with WPAR Mobility query */
	wpar_info_format_t info;
	int rc = -1;
	uintptr_t handle = 0;
	int (*func)(int command, void *buffer, size_t bufsize);

	if (NULL == getenv("NO_LPAR_RECONFIGURATION")) {
		struct getlpar_info lpar_info;
		char local_lpar_name = '\0';
		lpar_info.lpar_flags = 0;
		lpar_info.lpar_namesz = 0;
		lpar_info.lpar_name = &local_lpar_name;
		if (0 == sysconfig(SYS_GETLPAR_INFO, &lpar_info, sizeof(lpar_info))) {
			if ((lpar_info.lpar_flags & LPAR_ENABLED) && __DR_CPU_ADD()) {
				return TRUE;
			}
		}
	}

	/* Now check is WPAR is enabled and treat it as if DLPAR was. */

	/* look for lpar_get_info in libc */
	if (0 == omrsl_open_shared_library(LIBC_NAME, &handle, 0)) {
		if (0 == omrsl_lookup_name(handle, "lpar_get_info", (uintptr_t *)&func, "IIPi")) {
			rc = func(WPAR_INFO_FORMAT, &info, sizeof(info));
		}
		omrsl_close_shared_library(handle);
	}

	/* function only exists on aix 5.3 and above */
	if (0 == rc) {
		if (info.wpar_flags & WPAR_INFO_MOBILE) {
			return TRUE;
		}
	}
#endif /* defined(RS6000) && !defined(J9OS_I5) */
	return FALSE;
}

uintptr_t
j9sysinfo_weak_memory_consistency(struct J9PortLibrary *portLibrary)
{
	return FALSE;
}

void
j9sysinfo_shutdown(struct J9PortLibrary *portLibrary)
{
#if defined(S390) || defined(J9ZOS390)
	PPG_stfleCache.lastDoubleWord = -1;
#endif
}

int32_t
j9sysinfo_startup(struct J9PortLibrary *portLibrary)
{
#if defined(S390) || defined(J9ZOS390)
	PPG_stfleCache.lastDoubleWord = -1;
#elif !(defined(RS6000) || defined(LINUXPPC) || defined(PPC)) /* defined(S390) || defined(J9ZOS390) */
	PPG_sysL1DCacheLineSize = -1;
#endif /* defined(S390) || defined(J9ZOS390) */
	return 0;
}

uintptr_t
j9sysinfo_DLPAR_max_CPUs(struct J9PortLibrary *portLibrary)
{
#if defined(RS6000)
	/* Ensure that when DLAR or mobility feature of WPAR is enabled that we return at least 2 CPUs for the max.
	 * Why 2?
	 *   On a DLPAR system the max_ncpus will in fact be correct.  However, on a WPAR system there
	 *   is no way to tell how many CPUs there could be on the system we will be going to as that
	 *   system may not even exist yet.  So we choose 2 to make sure that any users of this function
	 *   understand that they have the potential of eventually being in an SMP environment.  We could
	 *   return 10 to accomplish the same thing, but 2 was the agreed upon value.
	 */
	if (_system_configuration.max_ncpus > 1) {
		return _system_configuration.max_ncpus;
	} else if (portLibrary->sysinfo_DLPAR_enabled(portLibrary)) {
		return 2;
	}
	return _system_configuration.max_ncpus;
#else /* defined(RS6000) */
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	return omrsysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE);
#endif /* defined(RS6000) */
}

uintptr_t
j9sysinfo_get_processing_capacity(struct J9PortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
#if defined(RS6000)
	lpar_info_format2_t info;
	int rc = -1;
	uintptr_t handle = 0;
	int (*func)(int command, void *buffer, size_t bufsize);

#if defined(J9OS_I5)
	/* Temp code due to unsupported syscall lpar_get_info for J9OS_I5 */
	return Xj9GetEntitledProcessorCapacity();
#else /* defined(J9OS_I5) */
	/* look for lpar_get_info in libc */
	if (0 == omrsl_open_shared_library(LIBC_NAME, &handle, 0)) {
		if (0 == omrsl_lookup_name(handle, "lpar_get_info", (uintptr_t *)&func, "IIPi")) {
			rc = func(LPAR_INFO_FORMAT2, &info, sizeof(info));
		}
		omrsl_close_shared_library(handle);
	}

	/* if the function was not found (only exists on aix 5.3 and above) or it failed, fall back to #cpus*100 */
	if (0 != rc) {
		return omrsysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE) * 100;
	}

	return info.entitled_capacity;
#endif /* defined(J9OS_I5) */
#else /* defined(RS6000) */
	return omrsysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE) * 100;
#endif /* defined(RS6000) */
}

int32_t
j9sysinfo_get_hw_info(struct J9PortLibrary *portLibrary, uint32_t infoType,
	char * buf, uint32_t bufLen)
{
	int32_t ret = J9PORT_SYSINFO_GET_HW_INFO_NOT_AVAILABLE;

#if defined(J9ZOS390)
	if (0 != PPG_j9csrsi_ret_code) {
		Trc_PRT_sysinfo_j9csrsi_init_failed(PPG_j9csrsi_ret_code);
		if (j9csrsi_is_hw_info_available(PPG_j9csrsi_ret_code)) {
			ret = J9PORT_SYSINFO_GET_HW_INFO_ERROR;
		}
	} else {
		switch (infoType) {
		case J9PORT_SYSINFO_GET_HW_INFO_MODEL:
			if (0 < j9csrsi_get_cpctype(PPG_j9csrsi_session, buf, bufLen)) {
				ret = J9PORT_SYSINFO_GET_HW_INFO_SUCCESS;
			} else {
				ret = J9PORT_SYSINFO_GET_HW_INFO_ERROR;
			}
			break;
		default:
			break;
		}
	}
#endif /* defined(J9ZOS390) */

	return ret;
}

#define PATH_ELEMENT_LENGTH 32

/**
 * Concatenate a filename onto a directory path, open the file, read it into a buffer,
 * null terminate the contents, and close the file.
 * pathBuffer must contain the directory part of the path including a terminal '/'.
 * The filename will be appended at the fileNameInsertionPoint. This must be at least PATH_ELEMENT_LENGTH
 * bytes before the end of the buffer.
 * At most readBufferLength-1 bytes are read from the file.
 * The file must not be empty.
 * @param [in]  portLibrary port library
 * @param [in]  pathBuffer buffer large enough to hold the entire file path. Contents are modified.
 * @param [in]  pathBufferLength Length of pathBuffer bytes
 * @param [in]  fileNameInsertionPoint position in pathBuffer immediately after the final '/'
 * @param [in]  fileName name to be appended to the path
 * @param [out] readBuffer buffer into which to read the file contents
 * @param [in]  readBufferLength size of the buffer.
 * @return 0 if the operation succeeds
 * @return J9PORT_ERROR_STRING_BUFFER_TOO_SMALL if the string operations overflow a buffer
 * @return J9PORT_ERROR_FILE_OPFAILED if the file could not be opened or read
 */
static int32_t
openAndReadInfo(struct J9PortLibrary *portLibrary, char *pathBuffer, size_t pathBufferLength, char *fileNameInsertionPoint,
	const char *fileName, char *readBuffer, size_t readBufferLength)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t fd = -1;
	intptr_t nRead = 0;
	int32_t status = 0;
	char *pathBufferEnd = pathBuffer + pathBufferLength - 1;

	if (fileNameInsertionPoint + strlen(fileName) >= pathBufferEnd) {
		status = J9PORT_ERROR_STRING_BUFFER_TOO_SMALL;
	} else {
		strcpy(fileNameInsertionPoint, fileName); /* length already checked */
		fd = omrfile_open(pathBuffer, EsOpenRead, 0);
		if (-1 == fd) {
			status = J9PORT_ERROR_FILE_OPFAILED;
		} else {
			nRead = omrfile_read(fd, readBuffer, readBufferLength - 1);
			omrfile_close(fd);
			if (nRead < 0) {
				status = J9PORT_ERROR_FILE_OPFAILED;
			} else {
				readBuffer[nRead] = '\0';
			}
		}
	}
	return status;
}

#if defined(J9X86) || defined(J9HAMMER) || defined(RISCV64)

char const *cpuPathPattern = "/sys/devices/system/cpu/cpu%d/cache/";
char const *indexPattern = "index%d/";
#define CPU_PATTERN_SIZE (strlen(cpuPathPattern) + (2 * PATH_ELEMENT_LENGTH) + 1)
/* leave room for terminating null */
#define READ_BUFFER_SIZE 32 /*
 * must be large enough to hold the content of the "size", "coherency_line_size"
 * "level" or "type" files
 */

/**
 * Scan the /sys filesystem for the correct descriptor and read it.
 * Stop when a descriptor matching the CPU, level, and one of the types in cacheType matches.
 * Note that if the cache is unified, it will match a query for any type.
 * @param [in]  portLibrary port library
 * @param [in]  cpu which CPU to query
 * @param [in]  level which cache level to query. Must be >= 1
 * @param [in]  cacheType bit mask of the type(s) of cache to query.
 * @param [in]  query indicates whether to get cache size or line size
 * @return cache size or line size in bytes
 */
static int32_t
getCacheSize(struct J9PortLibrary *portLibrary,
	const int32_t cpu, const int32_t level, const int32_t cacheType, const J9CacheQueryCommand query)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	char pathBuffer[CPU_PATTERN_SIZE];
	int32_t result = J9PORT_ERROR_SYSINFO_NOT_SUPPORTED;
	int32_t indexCursor = 0;
	int32_t cpuPathLength = 0;
	char *indexBuffer = NULL;

	if ((1 == level)
			&& J9_ARE_ANY_BITS_SET(cacheType, J9PORT_CACHEINFO_DCACHE)
			&& (J9PORT_CACHEINFO_QUERY_LINESIZE == query)
			&& (PPG_sysL1DCacheLineSize > 0)
	) {
		return PPG_sysL1DCacheLineSize;
	}
	cpuPathLength = omrstr_printf(pathBuffer, sizeof(pathBuffer), cpuPathPattern, cpu);
	indexBuffer = pathBuffer + cpuPathLength;
	Assert_PRT_true(cpuPathLength > 0);
	while (J9PORT_ERROR_SYSINFO_NOT_SUPPORTED == result) {
		int32_t indexLen = omrstr_printf(indexBuffer, PATH_ELEMENT_LENGTH, indexPattern, indexCursor);
		char *infoBuffer = indexBuffer + indexLen;
		char readBuffer[READ_BUFFER_SIZE];
		int32_t status = 0;

		Assert_PRT_true(indexLen > 0);

		status = openAndReadInfo(
				portLibrary, pathBuffer, sizeof(pathBuffer), infoBuffer,
				"level", readBuffer, sizeof(readBuffer)
		);
		if (0 != status) {
			result = status;
			break;
		}
		if (atoi(readBuffer) == level) {
			BOOLEAN found = FALSE;

			status = openAndReadInfo(
					portLibrary, pathBuffer, sizeof(pathBuffer), infoBuffer,
					"type", readBuffer, sizeof(readBuffer));
			if (0 != status) {
				result = status;
				break;
			}
			switch (readBuffer[0]) {
			case 'U':
				found = TRUE;
				break;
			case 'D':
				if (J9_ARE_ANY_BITS_SET(cacheType, J9PORT_CACHEINFO_DCACHE)) {
					found = TRUE;
				}
				break;
			case 'I':
				if (J9_ARE_ANY_BITS_SET(cacheType, J9PORT_CACHEINFO_ICACHE)) {
					found = TRUE;
				}
				break;
			}

			if (found) {
				size_t numericLength = 0;
				char *infoFile = (J9PORT_CACHEINFO_QUERY_LINESIZE == query) ?
						"coherency_line_size" : "size";
				status = openAndReadInfo(
						portLibrary, pathBuffer, sizeof(pathBuffer), infoBuffer,
						infoFile, readBuffer, sizeof(readBuffer)
				);
				if (0 != status) {
					result = status;
					break;
				}
				Trc_PRT_sysinfo_cacheSizeFileContent(pathBuffer, readBuffer);
				result = atoi(readBuffer);
				numericLength = strspn(readBuffer, "0123456789"); /* find the start of the scale character if any */
				switch (readBuffer[numericLength]) {
				case '\0':
				case '\r':
				case '\n': break;
				case 'k':
				case 'K':
					result *= 0x400;
					break;
				case 'm':
				case 'M':
					result *= 0x100000;
					break;
				default:
					result = J9PORT_ERROR_SYSINFO_OPFAILED;
					break;
				}
			}
		}
		++indexCursor;
	}
	if ((1 == level)
		&& J9_ARE_ANY_BITS_SET(cacheType, J9PORT_CACHEINFO_DCACHE)
		&& (J9PORT_CACHEINFO_QUERY_LINESIZE == query)
		&& (result > 0)
	) {
		PPG_sysL1DCacheLineSize = result;
	}
	return result;
}

/**
 * Scan the /sys filesystem reading the "type" files for a given level.
 * Create a bit mask of the types of caches at that level
 * @param [in]  portLibrary port library
 * @param [in]  cpu which CPU to query
 * @param [in]  level which cache level to query. Must be >= 1
 * @return bit mask of types in bytes
 */
static int32_t
getCacheTypes(struct J9PortLibrary *portLibrary, const int32_t cpu, const int32_t level)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t result = J9PORT_ERROR_SYSINFO_NOT_SUPPORTED;
	int32_t indexCursor = 0;
	int32_t cpuPathLength = 0;
	char *indexBuffer = NULL;
	BOOLEAN finish = FALSE;

	char pathBuffer[CPU_PATTERN_SIZE];
	cpuPathLength = omrstr_printf(pathBuffer, sizeof(pathBuffer), cpuPathPattern, cpu);
	indexBuffer = pathBuffer+cpuPathLength;
	Assert_PRT_true(cpuPathLength > 0);
	do {
		int32_t indexLen = omrstr_printf(indexBuffer, PATH_ELEMENT_LENGTH, indexPattern, indexCursor);
		char *infoBuffer = indexBuffer + indexLen;
		char readBuffer[READ_BUFFER_SIZE];
		int32_t status = 0;

		Assert_PRT_true(indexLen > 0);

		status = openAndReadInfo(
				portLibrary, pathBuffer, sizeof(pathBuffer), infoBuffer,
				"level", readBuffer, sizeof(readBuffer)
		);
		if (0 != status) {
			finish = TRUE;
		} else if (atoi(readBuffer) == level) {
			if (J9PORT_ERROR_SYSINFO_NOT_SUPPORTED == result) {
				result = 0;
			}

			status = openAndReadInfo(
					portLibrary, pathBuffer, sizeof(pathBuffer), infoBuffer,
					"type", readBuffer, sizeof(readBuffer)
			);
			if (0 != status) {
				result = status;
				break;
			}
			switch (readBuffer[0]) {
			case 'U':
				result |= J9PORT_CACHEINFO_UCACHE;
				break;
			case 'D':
				result |= J9PORT_CACHEINFO_DCACHE;
				break;
			case 'I':
				result |= J9PORT_CACHEINFO_ICACHE;
				break;
			}
		}
		++indexCursor;
	} while (!finish);
	return result;
}

/**
 * Scan the /sys filesystem reading the "level" files.
 * Find the largest value.
 * @param [in]  portLibrary port library
 * @param [in]  cpu which CPU to query
 * @return Highest level found, or negative value on error.
 */
static int32_t
getCacheLevels(struct J9PortLibrary *portLibrary, const int32_t cpu)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	char pathBuffer[CPU_PATTERN_SIZE];
	int32_t result = J9PORT_ERROR_SYSINFO_NOT_SUPPORTED;
	int32_t indexCursor = 0;
	int32_t cpuPathLength = 0;
	char *indexBuffer = NULL;
	BOOLEAN finish = FALSE;

	cpuPathLength = omrstr_printf(pathBuffer, sizeof(pathBuffer), cpuPathPattern, cpu);
	indexBuffer = pathBuffer+cpuPathLength;
	Assert_PRT_true(cpuPathLength > 0);
	do {
		int32_t indexLen = omrstr_printf(indexBuffer, PATH_ELEMENT_LENGTH, indexPattern, indexCursor);
		char *infoBuffer = indexBuffer + indexLen;
		char readBuffer[READ_BUFFER_SIZE];
		int32_t status = 0;

		Assert_PRT_true(indexLen > 0);

		status = openAndReadInfo(
				portLibrary, pathBuffer, sizeof(pathBuffer), infoBuffer,
				"level", readBuffer, sizeof(readBuffer)
		);
		if (J9PORT_ERROR_FILE_OPFAILED == status) {
			/* came to the end of the list of cache entries.  This error is expected. */
			finish = TRUE;
		} else if (status < 0) { /* unexpected error */
			result = status;
			finish = TRUE;
		} else {
			int32_t level = atoi(readBuffer);
			result = OMR_MAX(result, level);
		}
		++indexCursor;

	} while (!finish);
	return result;
}
#elif defined(AIXPPC) /* defined(J9X86) || defined(J9HAMMER) || defined(RISCV64) */
static int32_t
getCacheLevels(struct J9PortLibrary *portLibrary,
	const int32_t cpu)
{
	return 2;
}

/* getsystemcfg() isn't supported on i 7.1 so there's no need to define the 
 * functions `getCacheTypes` & `getCacheSize` on the i-series platforms
 */
#if !defined(J9OS_I5_V6R1)

static int32_t
getCacheTypes(struct J9PortLibrary *portLibrary,
	const int32_t cpu, const int32_t level)
{
	int32_t result = J9PORT_ERROR_SYSINFO_NOT_SUPPORTED;
	if (1 == level) {
		uint64_t info = getsystemcfg(SC_L1C_ATTR);
		if (J9_ARE_ANY_BITS_SET(info, 1 << 30)) {
			result = J9PORT_CACHEINFO_DCACHE | J9PORT_CACHEINFO_ICACHE;
		} else {
			result = J9PORT_CACHEINFO_UCACHE;
		}
	} else if (2 == level) {
		result = J9PORT_CACHEINFO_UCACHE;
	}
	return result;
}

static int32_t
getCacheSize(struct J9PortLibrary *portLibrary,
	const int32_t cpu, const int32_t level, const int32_t cacheType, const J9CacheQueryCommand query)
{
	int32_t result = J9PORT_ERROR_SYSINFO_NOT_SUPPORTED;
	switch (level) {
	case 1: {
		/* Note: AIX has split I/D level 1 cache.  Querying UCACHE is invalid. */
		switch (query) {
		case J9PORT_CACHEINFO_QUERY_CACHESIZE:
			if (J9_ARE_ANY_BITS_SET(cacheType, J9PORT_CACHEINFO_DCACHE)) {
				result = getsystemcfg(SC_L1C_DSZ);
			} else if (J9_ARE_ANY_BITS_SET(cacheType, J9PORT_CACHEINFO_ICACHE)) {
				result = getsystemcfg(SC_L1C_ISZ);
			}
			break;
		case J9PORT_CACHEINFO_QUERY_LINESIZE:
			if (J9_ARE_ANY_BITS_SET(cacheType, J9PORT_CACHEINFO_DCACHE)) {
				result = getsystemcfg(SC_L1C_DLS);
			} else if (J9_ARE_ANY_BITS_SET(cacheType, J9PORT_CACHEINFO_ICACHE)) {
				result = getsystemcfg(SC_L1C_ILS);
			}
			break;
		default:
			break;
		}
		break;
	}
	case 2:
		if (J9PORT_CACHEINFO_QUERY_CACHESIZE == query) {
			result = getsystemcfg(SC_L2C_SZ);
		} else if (J9PORT_CACHEINFO_QUERY_LINESIZE == query) {
			/* assumes the L1 and L2 sizes are the same */
			if (J9_ARE_ANY_BITS_SET(cacheType, J9PORT_CACHEINFO_DCACHE)) {
				result = getsystemcfg(SC_L1C_DLS);
			} else if (J9_ARE_ANY_BITS_SET(cacheType, J9PORT_CACHEINFO_ICACHE)) {
				result = getsystemcfg(SC_L1C_ILS);
			}
		}
		break;
	default:
		break;
	}
	return result;
}
#endif /* defined(J9OS_I5_V6R1) */
#endif /* defined(J9X86) || defined(J9HAMMER) || defined(RISCV64) */

/*
 * Cache information is organized as a set of "index" directories in /sys/devices/system/cpu/cpu<N>/cache/.
 * In each index directory is a file containing the cache level and another file containing the cache type.
 */
int32_t
j9sysinfo_get_cache_info(struct J9PortLibrary *portLibrary, const J9CacheInfoQuery * query)
{
	int32_t result = J9PORT_ERROR_SYSINFO_NOT_SUPPORTED;
	Trc_PRT_sysinfo_get_cache_info_enter(query->cmd, query->cpu, query->level, query->cacheType);

#if defined(RISCV64)
	{
		/* We need to avoid checking the cache directory as the cache info doesn't exist
		 * on Linux booted via QEMU (the emulator) while is literally supported on the
		 * hardware (e.g. HiFive U540). In such case, there is no padding adjustment for
		 * class objects even if the -XX:-RestrictContended option is specified on the
		 * command line.
		 */
		DIR *cacheDir = opendir("/sys/devices/system/cpu/cpu0/cache");
		if (NULL != cacheDir) {
			closedir(cacheDir);
		} else {
			Trc_PRT_sysinfo_get_cache_info_exit(result);
			return result;
		}
	}
#endif /* defined(RISCV64) */

#if defined(OSX)
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	switch (query->cmd) {
	case J9PORT_CACHEINFO_QUERY_LINESIZE:
		/* ignore the cache type and level, since there is only one line size on MacOS */
		omrcpu_get_cache_line_size(&result);
		break;
	case J9PORT_CACHEINFO_QUERY_CACHESIZE:
	case J9PORT_CACHEINFO_QUERY_TYPES:
	case J9PORT_CACHEINFO_QUERY_LEVELS:
	default:
		result = J9PORT_ERROR_SYSINFO_NOT_SUPPORTED;
		break;
	}
#elif defined(J9OS_I5_V6R1) /* defined(OSX) */
	switch (query->cmd) {
	case J9PORT_CACHEINFO_QUERY_LEVELS:
		result =  getCacheLevels(portLibrary, query->cpu);
		break;
	case J9PORT_CACHEINFO_QUERY_LINESIZE:
	case J9PORT_CACHEINFO_QUERY_CACHESIZE:
	case J9PORT_CACHEINFO_QUERY_TYPES:
	default:
		result = J9PORT_ERROR_SYSINFO_NOT_SUPPORTED;
		break;
	}
#elif defined(J9X86) || defined(J9HAMMER) || defined(AIXPPC) || defined(RISCV64) /* defined(J9OS_I5_V6R1) */
	switch (query->cmd) {
	case J9PORT_CACHEINFO_QUERY_LINESIZE:
	case J9PORT_CACHEINFO_QUERY_CACHESIZE:
		result =  getCacheSize(portLibrary, query->cpu, query->level, query->cacheType, query->cmd);

#if defined(RISCV64)
		/* The L1 data cache at "cache/index1" is set up from "/sys/devices/system/cpu/cpu1" on some Linux distro
		 * (e.g. Debian_riscv) rather than "/sys/devices/system/cpu/cpu0" in which "cache/index1" doesn't exist.
		 * Note: this is a temporary solution specific to Debian_riscv which won't be used or simply
		 * removed once we confirm "cpu0/cache/index1" does exist on the latest version of Debian_riscv.
		 */
		if (result < 0) {
			result =  getCacheSize(portLibrary, query->cpu + 1, query->level, query->cacheType, query->cmd);
		}
#endif /* defined(RISCV64) */
		break;
	case J9PORT_CACHEINFO_QUERY_TYPES:
		result =  getCacheTypes(portLibrary, query->cpu, query->level);
		break;
	case J9PORT_CACHEINFO_QUERY_LEVELS:
		result =  getCacheLevels(portLibrary, query->cpu);
		break;
	default:
		result = J9PORT_ERROR_SYSINFO_NOT_SUPPORTED;
		break;
	}
#elif defined(LINUXPPC) || defined(PPC) || defined(S390) || defined(J9ZOS390) /* defined(J9X86) || defined(J9HAMMER) || defined(AIXPPC) || defined(RISCV64) */
	if ((1 == query->level)
		&& (J9_ARE_ANY_BITS_SET(query->cacheType, J9PORT_CACHEINFO_DCACHE | J9PORT_CACHEINFO_UCACHE))
	) {
#if defined(S390) || defined(J9ZOS390)
		result = 256;
#else /* defined(S390) || defined(J9ZOS390) */
		OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
		omrcpu_get_cache_line_size(&result);
#endif /* defined(S390) || defined(J9ZOS390) */
	}
#elif defined(LINUX) && defined(J9AARCH64) /* defined(LINUXPPC) || defined(PPC) || defined(S390) || defined(J9ZOS390) */
	if ((query->cmd == J9PORT_CACHEINFO_QUERY_LINESIZE)
		&& (query->cacheType == J9PORT_CACHEINFO_DCACHE)
		&& (query->level == 1)
	) {
		/* L1 data cache line size */
		int32_t rc = (int32_t)sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
		if (rc > 0) {
			result = rc;
		} else if (rc == 0) {
			/*
			 * Cache line size is unavailable on some systems
			 * Use 64 as the default value because Arm Cortex ARMv8-A cores
			 * have L1 data cache lines of that size
			 */
			result = 64;
		}
	}
#endif /* defined(OSX) */
	Trc_PRT_sysinfo_get_cache_info_exit(result);
	return result;
}
