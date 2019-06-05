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
 * @file j9hypervisor_systemp.c
 * @ingroup Port
 * @brief  Functions common to PowerVM and PowerKVM Hypervisor
 */
 
/* _GNU_SOURCE forces GLIBC_2.0 sscanf/vsscanf/fscanf for RHEL5 compatibility */
#if defined(LINUX)
#define _GNU_SOURCE
#endif /* defined(__GNUC__) */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef AIXPPC
#if defined(J9OS_I5)
/* The PASE compiler does not support libperfstat.h */
#else
#include <libperfstat.h>
#endif
#include <sys/proc.h>
#endif

#include "j9comp.h"
#include "j9port.h"
#include "portpriv.h"
#include "omrutil.h"
#include "omrutilbase.h"
#include "ut_j9prt.h"

#define BYTES_IN_MB		(1024 * 1024)
#define MICROSEC_IN_SEC		1000000
#define PROCESSOR_CAPACITY	100

#if defined(LINUXPPC)

#define PROC_CPUINFO		"/proc/cpuinfo"
#define LINUX_PPC_CONFIG	"/proc/ppc64/lparcfg"

#define COMPUTE_BASE		10

/* Individual values of this structure are updated by various helper functions */
typedef struct linuxPowerVMInfo {
	/* Total purr value in timebase units */
	uint64_t purr;
	/* 1 if the current LPAR is capped, 0 if not */
	uint32_t capped;
	/* Total memory currently in use by this partition */
	uint64_t memUsed;
	/* Maximum memory allocated to this partition */
	uint64_t maxMemLimit;
	/* timebase tick frequency in microseconds */
	uint64_t timebaseFreq;
	/* Current timebase register value */
	uint64_t timebaseLast;
	/* MHz value */
	uint64_t cpuClockSpeedMHz;
	/* entitled processor capacity for this partition */
	uint64_t entitledCapacity;
	/* maximum entitled processor capacity for this partition */
	uint64_t maxEntitledCapacity;
} linuxPowerVMInfo;

#define GET_ATTRIBUTE(att_source, string, val_source, dest, func, ret) \
{ 									\
	char *endPtr = NULL;						\
									\
	if (0 == strcmp(att_source, string)) {				\
		errno = 0;						\
		dest = func(val_source, &endPtr, COMPUTE_BASE);		\
		if (errno > 0) {					\
			ret = errno;					\
			break;						\
		}							\
		continue;						\
	}								\
}

#define MAX_LINE_LENGTH		128

/**
 * @internal	Helper function that updates linuxPowerVMInfo structure with the cpu MHz
 * 		and timebase frequency values after reading /proc/cpuinfo
 *
 * @param [in] portLibrary The port Library
 * @param [out] linuxInfo  Structure to be updated
 * 
 * @return 0 on success, negative value on failure
 */
static intptr_t
read_linux_cpuinfo(struct J9PortLibrary *portLibrary, linuxPowerVMInfo *linuxInfo)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t ret = 0;
	FILE *procFP = NULL;
	char *linePtr = NULL;
	char lineStr[MAX_LINE_LENGTH] = {0};
	char tokenName[MAX_LINE_LENGTH] = {0};
	char tokenValue[MAX_LINE_LENGTH] = {0};

	procFP = fopen(PROC_CPUINFO, "r");
	if (NULL == procFP) {
		Trc_PRT_read_linux_cpuinfo_open_failed(errno);
		omrerror_set_last_error(errno, J9PORT_ERROR_HYPERVISOR_CPUINFO_READ_FAILED);
		return J9PORT_ERROR_HYPERVISOR_CPUINFO_READ_FAILED;
	}

	while (NULL != fgets ((char *) lineStr, MAX_LINE_LENGTH, procFP)) {

		linePtr = (char *) lineStr;
		/* Read one less than MAX_LINE_LENGTH */
		int32_t tokens = sscanf(linePtr, "%127s %*c %s %*s", tokenName, tokenValue);
		if (2 == tokens) {
			GET_ATTRIBUTE(tokenName, "clock", tokenValue, linuxInfo->cpuClockSpeedMHz, strtoll, ret);
			GET_ATTRIBUTE(tokenName, "timebase", tokenValue, linuxInfo->timebaseFreq, strtoll, ret);
		}
	}
	fclose(procFP);

	/* All places that use timebase freq, needs micro second precision */
	linuxInfo->timebaseFreq /= MICROSEC_IN_SEC;

	/* timebase and clock cannot be zero */
	if (ret || 0 == linuxInfo->timebaseFreq || 0 == linuxInfo->cpuClockSpeedMHz) {
		Trc_PRT_read_linux_cpuinfo_failed(ret, linuxInfo->timebaseFreq, linuxInfo->cpuClockSpeedMHz);
		omrerror_set_last_error(ret, J9PORT_ERROR_HYPERVISOR_CPUINFO_READ_FAILED);
		return J9PORT_ERROR_HYPERVISOR_CPUINFO_READ_FAILED;
	}
	return 0;
}

/**
 * @internal	Helper function that reads /proc/ppc64/lparcfg file and updates the
 * 		linuxPowerVMInfo structure with purr, capped, partition_entitled_capacity,
 * 		partition_max_entitled_capacity values, entitled_memory & backing_memory.
 * 		entitled_memory & backing_memory is not available in lparcfg versions earlier
 * 		than 1.8. The caller needs to take care of checking that.
 *
 * @param [in] portLibrary The port Library
 * @param [out] linuxInfo The structure to be updated
 * 
 * @return 0 on success, negative value on failure
 */
static intptr_t
read_linux_lparcfg(struct J9PortLibrary *portLibrary, linuxPowerVMInfo *linuxInfo)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t ret = 0;
	char *linePtr = NULL;
	FILE *lparcfgFP = NULL;
	char lineStr[MAX_LINE_LENGTH] = {0};
	char tokenName[MAX_LINE_LENGTH] = {0};
	char tokenValue[MAX_LINE_LENGTH] = {0};

	lparcfgFP = fopen(LINUX_PPC_CONFIG, "r");
	if (NULL == lparcfgFP) {
		Trc_PRT_read_linux_lparcfg_file_open_failed(errno);
		omrerror_set_last_error(errno, J9PORT_ERROR_HYPERVISOR_LPARCFG_READ_FAILED);
		return J9PORT_ERROR_HYPERVISOR_LPARCFG_READ_FAILED;
	}

	while (NULL != fgets ((char *) lineStr, MAX_LINE_LENGTH, lparcfgFP)) {

		linePtr = (char *) lineStr;
		int32_t tokens = sscanf(linePtr, "%[^=]=%s", tokenName, tokenValue);
		if (2 == tokens) {
			GET_ATTRIBUTE(tokenName, "partition_max_entitled_capacity", 
					tokenValue, linuxInfo->maxEntitledCapacity, strtoll, ret);
			GET_ATTRIBUTE(tokenName, "partition_entitled_capacity", tokenValue, 
					linuxInfo->entitledCapacity, strtoll, ret);
			GET_ATTRIBUTE(tokenName, "capped", tokenValue, linuxInfo->capped, strtol, ret);
			GET_ATTRIBUTE(tokenName, "entitled_memory", tokenValue, linuxInfo->maxMemLimit, strtoll, ret);
			GET_ATTRIBUTE(tokenName, "backing_memory", tokenValue, linuxInfo->memUsed, strtoll, ret);
			GET_ATTRIBUTE(tokenName, "purr", tokenValue, linuxInfo->purr, strtoll, ret);
		}
	}
	fclose(lparcfgFP);

	/* We do /not/ validate the counters here; simply pass these up.  The caller shall
	 * check as appropriate, for the target platform, which all counters are worth
	 * validating.
	 */
	return ret;
}

/**
 * Get Linux usage info from /proc/ppc64/lparcfg and constants from /proc/cpuinfo
 *
 * @param [in] portLibrary The port Library
 * @param [out] linuxinfo Caller supplied linuxPowerVMInfo that gets updated
 *
 * @return 0 on success, a negative value on failure
 */
static intptr_t
get_linux_powervm_info(struct J9PortLibrary *portLibrary, linuxPowerVMInfo *linuxInfo)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t ret = 0;

	/* Fill in the timebase frequency and cpu MHz values from /proc/cpuinfo */
	ret = read_linux_cpuinfo(portLibrary, linuxInfo);
	if (ret < 0) {
		Trc_PRT_get_linux_powervm_info_read_cpuinfo_failed(ret);
		return ret;
	}

	/* Get CPU usage details from /proc/ppc64/lparcfg */
	ret = read_linux_lparcfg(portLibrary, linuxInfo);
	/* Check validity of counters on PowerVM: purr and entitled capacity cannot be zero */
	if ((0 != ret) || (0 == linuxInfo->purr) || (0 == linuxInfo->entitledCapacity)) {
		Trc_PRT_get_linux_powervm_info_values_failed(ret, linuxInfo->purr, linuxInfo->entitledCapacity);
		omrerror_set_last_error(ret, J9PORT_ERROR_HYPERVISOR_LPARCFG_READ_FAILED);
		return J9PORT_ERROR_HYPERVISOR_LPARCFG_READ_FAILED;
	}

	/* Get the current timebase value from the tb register */
	linuxInfo->timebaseLast = getTimebase();

	return ret;
}

/**
 * Returns processor usage statistics of the Guest VM as seen by the hypervisor
 *
 * @param [in] portLibrary The port Library
 * @param [out] gpUsage Caller supplied J9GuestProcessorUsage structure
 * 			that gets filled in with usage statistics
 *
 * @return 0 on success, a negative value on failure
 */
static intptr_t
get_linux_powervm_processor_usage(struct J9PortLibrary *portLibrary, J9GuestProcessorUsage *gpUsage)
{
	int32_t ret = 0;
	linuxPowerVMInfo linuxInfo;

	memset(&linuxInfo, 0, sizeof(linuxPowerVMInfo));

	/* Fill in the timebase frequency and cpu MHz values from /proc/cpuinfo */
	ret = get_linux_powervm_info(portLibrary, &linuxInfo);
	if (ret < 0) {
		Trc_PRT_get_linux_powervm_processor_usage_get_linux_info_failed(ret);
		return ret;
	}

	/* Purr value on linux is the total utilization value for all of the partition's
	 * virtual processors. timebaseFreq is the real time clock frequency with which
	 * purr is incremented. purr over timebaseFreq gives us the number of microseconds
	 * of utilization. purr is obtained from /proc/ppc64/lparcfg and is summation of the
	 * user, system and wait times of all vcpus of the LPAR
	 */
	gpUsage->cpuTime = linuxInfo.purr / linuxInfo.timebaseFreq;

	/* Cpu speed (in MHz) as found in /proc/cpuinfo */
	gpUsage->hostCpuClockSpeed = linuxInfo.cpuClockSpeedMHz;

	/* On PowerVM, the effective number of physical CPUs allocated to a partition is
	 * determined by three entities, capping, maxEntitledCapacity and entitled
	 * capacity. If there is no capping, then based on load, the CPU can go upto
	 * maxEntitledCapacity. If partition is capped, then CPU can only go upto
	 * entitled capacity. Processing units can be incremented in 1/100 of a unit
	 * with a minimum capacity of 1/10
	 */
	gpUsage->cpuEntitlement = (SYS_FLOAT) linuxInfo.entitledCapacity / (SYS_FLOAT) PROCESSOR_CAPACITY;
	gpUsage->timestamp = linuxInfo.timebaseLast / linuxInfo.timebaseFreq;

	return 0;
}

/**
 * Returns memory usage statistics of the Guest LPAR as seen by the hypervisor
 *
 * @param [in] portLibrary The port Library
 * @param [out] gmUsage Caller supplied J9GuestMemoryUsage structure
 * 			that gets filled in with usage statistics
 *
 * @return 0 on success, a negative value on failure
 */
static intptr_t
get_linux_powervm_memory_usage(struct J9PortLibrary *portLibrary, J9GuestMemoryUsage *gmUsage)
{
	int32_t ret = 0;
	linuxPowerVMInfo linuxInfo;

	memset(&linuxInfo, 0, sizeof(linuxPowerVMInfo));

	/* Fill in the timebase frequency and cpu MHz values from /proc/cpuinfo */
	ret = get_linux_powervm_info(portLibrary, &linuxInfo);
	if (ret < 0) {
		Trc_PRT_get_linux_powervm_memory_usage_get_linux_info_failed(ret);
		return ret;
	}

	/* If both are not available return error. These values are only present from lparcfg
	 * level 1.8 onwards. lparcfg 1.8 (or greater) is part of SLES 11 and RHEL 6
	 * Earlier versions of these distros will report the following error
	 */
	if (0 == linuxInfo.memUsed && 0 == linuxInfo.maxMemLimit) {
		ret = J9PORT_ERROR_HYPERVISOR_LPARCFG_MEM_UNSUPPORTED;
		Trc_PRT_get_linux_powervm_memory_usage_values_read_failed(ret);
		return ret;
	}

	/* Value is in bytes, convert to MB */
	gmUsage->memUsed = linuxInfo.memUsed / BYTES_IN_MB;
	gmUsage->maxMemLimit = linuxInfo.maxMemLimit / BYTES_IN_MB;
	gmUsage->timestamp = linuxInfo.timebaseLast / linuxInfo.timebaseFreq;

	return 0;
}
/**
 * Cleanup function called at java shutdown time.
 * Close all open handles and free any memory allocated at startup
 *
 * @param [in] portLibrary The port Library
 */
static void
linux_powervm_shutdown(struct J9PortLibrary *portLibrary)
{
	PHD_hypFunc.get_guest_processor_usage = NULL;
	PHD_hypFunc.get_guest_memory_usage = NULL;
	PHD_hypFunc.hypervisor_impl_shutdown = NULL;
	PHD_vendorPrivateData = NULL;
}

/**
 * This is called when the j9hypervisor_get_guest_processor_usage or
 * j9hypervisor_get_guest_memory_usage API are invoked for the first time
 * and we are running on top of PowerVM
 *
 * @param [in] portLibrary The port Library
 *
 * @result 0 on success and negative on failure
 */
intptr_t
linux_powervm_startup(struct J9PortLibrary *portLibrary)
{
	/* Setup pointers to the getter and shutdown functions */
	PHD_hypFunc.get_guest_processor_usage = get_linux_powervm_processor_usage;
	PHD_hypFunc.get_guest_memory_usage = get_linux_powervm_memory_usage;
	PHD_hypFunc.hypervisor_impl_shutdown = linux_powervm_shutdown;
	PHD_vendorStatus = HYPERVISOR_VENDOR_INIT_SUCCESS;

	return 0;
}

/** 
 * PowerKVM routine to obtain hypervisor specific information; reuses code in helpers 
 * read_linux_cpuinfo() and read_linux_lparcfg() for obtaining PURR 
 * and timebase.  However, it does its own PowerKVM-specific checks that may not be 
 * applicable to PowerVM or vice versa.
 *
 * @param [in] portLibrary The port Library
 * @param [out] linuxinfo Caller supplied linuxPowerVMInfo that gets updated
 *
 * @return 0 on success, a negative value on failure
 */
static intptr_t
get_linux_powerkvm_info(struct J9PortLibrary *portLibrary, linuxPowerVMInfo *linuxInfo)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t ret = 0;

	/* Fill in the timebase frequency and cpu MHz values from /proc/cpuinfo */
	ret = read_linux_cpuinfo(portLibrary, linuxInfo);
	if (ret < 0) {
		Trc_PRT_get_linux_powerkvm_info_read_cpuinfo_failed(ret);
		return ret;
	}

	/* Get CPU usage details from /proc/ppc64/lparcfg */
	ret = read_linux_lparcfg(portLibrary, linuxInfo);
	/* Check validity of counters on PowerKVM: purr cannot be zero */
	if ((0 != ret) || (0 == linuxInfo->purr)) {
		Trc_PRT_get_linux_powerkvm_info_values_failed(ret, linuxInfo->purr);
		omrerror_set_last_error(ret, J9PORT_ERROR_HYPERVISOR_LPARCFG_READ_FAILED);
		return J9PORT_ERROR_HYPERVISOR_LPARCFG_READ_FAILED;
	}

	/* Get the current timebase value from the tb register */
	linuxInfo->timebaseLast = getTimebase();

	return ret;
}

/**
 * Returns processor usage statistics of the Guest VM as seen by the hypervisor
 *
 * @param [in] portLibrary The port Library
 * @param [out] gpUsage Caller supplied J9GuestProcessorUsage structure
 *                      that gets filled in with usage statistics
 *
 * @return 0 on success, a negative value on failure
 */
static intptr_t
get_linux_powerkvm_processor_usage(struct J9PortLibrary *portLibrary, J9GuestProcessorUsage *gpUsage)
{
	int32_t ret = 0;
	linuxPowerVMInfo linuxInfo;

	memset(&linuxInfo, 0, sizeof(linuxPowerVMInfo));

	/* Fill in the timebase frequency and cpu MHz values from /proc/cpuinfo */
	ret = get_linux_powerkvm_info(portLibrary, &linuxInfo);
	if (ret < 0) {
		Trc_PRT_get_linux_powerkvm_processor_usage_get_linux_info_failed(ret);
		return ret;
	}

	/* Purr value on linux is the total utilization value for all of the partition's
	 * virtual processors. timebaseFreq is the real time clock frequency with which
	 * purr is incremented. purr over timebaseFreq gives us the number of microseconds
	 * of utilization. purr is obtained from /proc/ppc64/lparcfg and is summation of the
	 * user, system and wait times of all vcpus of the LPAR
	 */
	gpUsage->cpuTime = linuxInfo.purr / linuxInfo.timebaseFreq;
	/* Cpu speed (in MHz) as found in /proc/cpuinfo */
	gpUsage->hostCpuClockSpeed = linuxInfo.cpuClockSpeedMHz;

	/* On PowerKVM, CPU entitlement is not available, as of today. */
	gpUsage->cpuEntitlement = (SYS_FLOAT) -1;
	gpUsage->timestamp = linuxInfo.timebaseLast / linuxInfo.timebaseFreq;

	return 0;
}

/**
 * Returns memory usage statistics of the Guest LPAR as seen by the hypervisor
 *
 * @param [in] portLibrary The port Library
 * @param [out] gmUsage Caller supplied J9GuestMemoryUsage structure
 *                      that gets filled in with usage statistics
 *
 * @return 0 on success, a negative value on failure
 */
static intptr_t
get_linux_powerkvm_memory_usage(struct J9PortLibrary *portLibrary, J9GuestMemoryUsage *gmUsage)
{
	/* Set to indicate non-availability of these counters on PowerKVM.  Will be
	 * implemented as and when each of these become available. 
	 */
	gmUsage->memUsed = -1;
	gmUsage->maxMemLimit = -1;
	gmUsage->timestamp = -1;
	
	return J9PORT_ERROR_HYPERVISOR_LPARCFG_MEM_UNSUPPORTED;
}

/**
 * Cleanup function called at java shutdown time.
 * Close all open handles and free any memory allocated at startup
 *
 * @param [in] portLibrary The port Library
 */
static void 
linux_powerkvm_shutdown(struct J9PortLibrary *portLibrary)
{
	PHD_hypFunc.get_guest_processor_usage = NULL;
	PHD_hypFunc.get_guest_memory_usage = NULL;
	PHD_hypFunc.hypervisor_impl_shutdown = NULL;
	PHD_vendorPrivateData = NULL;
}

/* The PowerKVM hypervisor specific startup code, which sets up global function
 * pointers (PHD_hypFunc) to point to PowerKVM specific functions for memory and
 * CPU usage retrieval API's.
 *
 * @param [in] portLibrary The port Library
 *
 * @result 0 on success and negative on failure
 */
intptr_t 
linux_powerkvm_startup(struct J9PortLibrary *portLibrary)
{
	/* Setup pointers to the getter and shutdown functions */
	PHD_hypFunc.get_guest_processor_usage = get_linux_powerkvm_processor_usage;
	PHD_hypFunc.get_guest_memory_usage = get_linux_powerkvm_memory_usage;
	PHD_hypFunc.hypervisor_impl_shutdown = linux_powerkvm_shutdown;
	PHD_vendorStatus = HYPERVISOR_VENDOR_INIT_SUCCESS;
	
	return 0;
}

/**
 * Wrapper to call an appropriate (hypervisor-specific) initializer routine.
 *
 * @param [in] portLibrary The port Library
 *
 * @return 0 on success and negative on failure
 */
intptr_t
systemp_startup(struct J9PortLibrary *portLibrary)
{
	/* Depending on the specific hypervisor we are running, we select 
	 * an initialization routine.  That initializer will then set up 
	 * all function pointers appropriately for the platform.
	 */
	if (0 == strcmp(PHD_vendorDetails.hypervisorName, HYPE_NAME_POWERKVM)) {
		/* Hypervisor == "PowerKVM" */
		return linux_powerkvm_startup(portLibrary);
	} else {
		/* Hypervisor == "PowerVM" */
		return linux_powervm_startup(portLibrary);
	}
}

#elif defined(AIXPPC)

#define HTIC2MICROSEC(x) (((double)x * XINTFRAC)/(double)1000.0)
#define MHz_IN_Hz	(1000 * 1000)

/**
 * Returns processor usage statistics of the Guest LPAR as seen by the hypervisor
 * Uses libperfstat to get the usage details
 *
 * @param [in] portLibrary The port Library
 * @param [out]	gpUsage. Caller supplied J9GuestProcessorUsage structure
 * 							 that gets filled in with usage statistics
 *
 * @return 0 on success, a negative value on failure
 */
static intptr_t
get_aix_powervm_processor_usage(struct J9PortLibrary *portLibrary, J9GuestProcessorUsage *gpUsage)
{
#if !defined(J9OS_I5)
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t ret = 0;
	int64_t usedTimeinTicks = 0;
	perfstat_cpu_total_t cpuTotal;
	perfstat_partition_total_t partition_stats;

	/* perfstat_cpu_total has the processor clock speed value in Hz */
	ret = perfstat_cpu_total(NULL, &cpuTotal, sizeof(perfstat_cpu_total_t), 1);
	if (ret < 0) {
		Trc_PRT_get_aix_powervm_processor_usage_perfstat_cpu_failed(errno);
		ret = J9PORT_ERROR_HYPERVISOR_PERFSTAT_CPU_FAILED;
		Trc_PRT_get_aix_powervm_processor_usage_perfstat_cpu_failed(ret);
		omrerror_set_last_error(errno, ret);
		return ret;
	}

	gpUsage->hostCpuClockSpeed = cpuTotal.processorHZ / MHz_IN_Hz;

	ret = perfstat_partition_total(NULL, &partition_stats, sizeof(perfstat_partition_total_t), 1);
	if (ret < 0) {
		Trc_PRT_get_aix_powervm_processor_usage_perfstat_partition_failed(errno);
		ret = J9PORT_ERROR_HYPERVISOR_PERFSTAT_PARTITION_FAILED;
		Trc_PRT_get_aix_powervm_processor_usage_perfstat_partition_failed(ret);
		omrerror_set_last_error(errno, ret);
		return ret;
	}

	/* perstat_partition_total is the preferred structure as it incorporates purr based accounting.
	 * usedTime for this lpar is just the summation of user, system and wait times. AIX tools also
	 * take into consideration the idle times. However we ignore idle to keep it consistent across
	 * all platforms / hypervisors
	 */
	usedTimeinTicks = partition_stats.puser +  partition_stats.psys +  partition_stats.pwait;
	/* If the partition is a DLPAR with donate enabled, need to factor in the busy donated
	 * and the busy stolen ticks. These are ticks that this partition would have used but was
	 * taken away for use of other LPARs. AIX tools account for these ticks as well
	 */
	if (partition_stats.type.b.donate_enabled) {
		usedTimeinTicks += partition_stats.busy_donated_purr + partition_stats.busy_stolen_purr;
	}

	/* The above values are processor tics which get updated at timebase frequency.
	 * Use HTICTOMICROSEC macro to convert to micro seconds
	 */
	gpUsage->cpuTime = (int64_t) HTIC2MICROSEC((SYS_FLOAT) usedTimeinTicks);

	/* On PowerVM, the effective number of physical CPUs allocated to a partition is
	 * determined by three entities, capping, maxEntitledCapacity and entitled
	 * capacity. If there is no capping, then based on load, the CPU can go upto
	 * maxEntitledCapacity. If partition is capped, then CPU can only go upto
	 * entitled capacity. Processing units can be incremented in 1/100 of a unit
	 * with a minimum capacity of 1/10
	 */
	gpUsage->cpuEntitlement = (SYS_FLOAT) partition_stats.entitled_proc_capacity / (SYS_FLOAT) PROCESSOR_CAPACITY;
	gpUsage->timestamp = (int64_t) HTIC2MICROSEC((SYS_FLOAT) partition_stats.timebase_last);

	return 0;
#else
	return -1; /* not supported */
#endif
}

/**
 * Returns memory usage statistics of the Guest LPAR as seen by the hypervisor
 * Uses libperfstat to get the usage details
 *
 * @param [in] portLibrary The port Library
 * @param [out]	gmUsage Caller supplied J9GuestMemoryUsage structure
 * 			that gets filled in with usage statistics
 *
 * @return 0 on success, a negative value on failure
 */
static intptr_t
get_aix_powervm_memory_usage(struct J9PortLibrary *portLibrary, J9GuestMemoryUsage *gmUsage)
{
#if !defined(J9OS_I5)
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t ret = 0;
	perfstat_partition_total_t partition_stats;

	ret = perfstat_partition_total(NULL, &partition_stats, sizeof(perfstat_partition_total_t), 1);
	if (ret < 0) {
		Trc_PRT_get_aix_powervm_memory_usage_perfstat_failed(errno);
		ret = J9PORT_ERROR_HYPERVISOR_PERFSTAT_PARTITION_FAILED;
		Trc_PRT_get_aix_powervm_memory_usage_perfstat_failed(ret);
		omrerror_set_last_error(errno, ret);
		return ret;
	}

	/* pmem, which is the backing physical memory is set to be the same as max_memory
	 * in case of a dedicated partition.
	 */
	gmUsage->maxMemLimit = partition_stats.max_memory;
	gmUsage->memUsed = partition_stats.pmem / BYTES_IN_MB;
	gmUsage->timestamp = (int64_t) HTIC2MICROSEC((SYS_FLOAT) partition_stats.timebase_last);

	return 0;
#else
	return -1; /* not supported */
#endif
}
/**
 * Cleanup function called at java shutdown time.
 * Close all open handles and free any memory allocated at startup
 *
 * @param [in] portLibrary The port Library
 */
static void
aix_powervm_shutdown(struct J9PortLibrary *portLibrary)
{
	PHD_hypFunc.get_guest_processor_usage = NULL;
	PHD_hypFunc.get_guest_memory_usage = NULL;
	PHD_hypFunc.hypervisor_impl_shutdown = NULL;
	PHD_vendorPrivateData = NULL;
}

/**
 * This is called when the j9hypervisor_get_guest_processor_usage or
 * j9hypervisor_get_guest_memory_usage API are invoked for the first time
 * and we are running on top of PowerVM
 *
 * @param [in] portLibrary The port Library
 *
 * @result 0 on success, negative value on failure
 */
intptr_t
aix_powervm_startup(struct J9PortLibrary *portLibrary)
{
	/* Setup pointers to the getter and shutdown functions */
	PHD_hypFunc.get_guest_processor_usage = get_aix_powervm_processor_usage;
	PHD_hypFunc.get_guest_memory_usage = get_aix_powervm_memory_usage;
	PHD_hypFunc.hypervisor_impl_shutdown = aix_powervm_shutdown;
	PHD_vendorStatus = HYPERVISOR_VENDOR_INIT_SUCCESS;

	return 0;
}

/**
 * Wrapper to call aix_powervm_startup
 *
 * @param [in] portLibrary The port Library
 *
 * @return 0 on success and negative on failure
 */
intptr_t
systemp_startup(struct J9PortLibrary *portLibrary)
{
	return aix_powervm_startup(portLibrary);
}
#endif
