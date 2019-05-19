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
 * @brief Functions common to both the z/VM and PR/SM hypervisors
 */

/* Needed for Dl_info structure and dladdr call */
#define _GNU_SOURCE
#define __USE_GNU 1
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <mntent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "j9hypervisor_common.h"
#include "j9port.h"
#include "portnls.h"
#include "portpriv.h"
#include "ut_j9prt.h"

#define MAX_LINE_LENGTH	128

/* Hold structure for reading in various static attributes */
typedef struct LinuxStaticInfo {
	/* hypfs mount point */
	char *hypfsDir;

	/* Processor speed */
	uint64_t cpuClockSpeedMHz;

	/* Name of the partition */
	char VMName[MAX_LINE_LENGTH];

	/* system-z machine type */
	char machType[MAX_LINE_LENGTH];

	/* Used to synchronize access to hypervisor implementation specific details */
	omrthread_monitor_t hyperMonitor;
} LinuxStaticInfo;

/* Hold structure for reading in various dynamic attributes from hypfs */
typedef struct LinuxzVMInfo {
	/* Total cpu time for this partition since last IPL in microseconds */
	uint64_t cpuTime;

	/* current mem used for this partition */
	uint64_t memUsed;

	/* cpu dedicated or shared */
	uint32_t dedicatedCpu;

	/* vcpus for this partition */
	uint32_t vcpuCount;

	/* Total time since last boot in microseconds */
	uint64_t timestamp;

	/* max memory assigned for this partition */
	uint64_t maxMemLimit;

	/* max weight for this partition */
	uint32_t cpuWeightMax;

	/* entitled processor capacity for this partition */
	uint64_t entitledCapacity;

	/* time when the "update" file was last written to */
	time_t updateLastStat;
} LinuxzVMInfo;

/* Compute the path of the zlinux-machine-speeds.txt file. */
static intptr_t
get_speed_file_path(struct J9PortLibrary *portLibrary, char *speedFilePath, const size_t bufSize)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);

	const char *speedFileName = "zlinux-machine-speeds.txt";
	int32_t ret = 0;
	int dladdrRet = 0;
	char *lastSlashPos = NULL;
	const char *format = "%s/%s";
	uintptr_t combinedLength = 0;
	size_t dliFnameSize = 0;

	/* The location of the zos-machine-speeds file is the same as where
	 * the libj9prt*.so file is located
	 */
	Dl_info libraryInfo;
	dladdrRet = dladdr((void *)j9port_startup_library, &libraryInfo);
	if (0 == dladdrRet) {
		/* Can't find where the port library is loaded. */
		ret = J9PORT_ERROR_HYPERVISOR_MACHINE_SPEED_FILE_READ_FAILED;
		Trc_PRT_process_machine_speed_file_path_not_found(ret);
		return ret;
	}

	/* The path for the libj9prti*.so library must not exceed the buffer size. */
	dliFnameSize = strlen(libraryInfo.dli_fname);
	if (dliFnameSize >= bufSize) {
		ret = J9PORT_ERROR_HYPERVISOR_MACHINE_SPEED_FILE_READ_FAILED;
		Trc_PRT_process_machine_speed_file_path_size_exceeds_limit(dliFnameSize, bufSize, ret);
		return ret;
	}
	strncpy(speedFilePath, libraryInfo.dli_fname, bufSize);

	/* Remove libj9prt....so. */
	lastSlashPos = strrchr(speedFilePath,'/');
	/* Ensure that there is a '/' in speedFilePath */
	if (NULL == lastSlashPos) {
		ret = J9PORT_ERROR_HYPERVISOR_MACHINE_SPEED_FILE_READ_FAILED;
		Trc_PRT_process_machine_speed_file_path_is_malformed(speedFilePath, ret);
		return ret;
	}
	*lastSlashPos = '\0';

	/* Check that the resulting path does not exceed bufSize. */
	combinedLength = omrstr_printf(NULL, 0, format, speedFilePath, speedFileName);
	if (combinedLength >= bufSize) {
		ret = J9PORT_ERROR_HYPERVISOR_MACHINE_SPEED_FILE_READ_FAILED;
		Trc_PRT_process_machine_speed_file_path_size_exceeds_limit(combinedLength, bufSize, ret);
		return ret;
	}

	omrstr_printf(speedFilePath, bufSize, format, speedFilePath, speedFileName);

	return 0;
}

#define COMPUTE_BASE		10

/**
 * @internal	Read machine type and corresponding MHz tuples from the 
 * 		zlinux-machine-speeds.txt file. This file is in the default dir.
 * 		Put the valid tuples into a linked list
 *
 * @param [in]  portLibrary The port Library
 * @param [out] staticInfo  Structure to be updated
 *
 * @return	0 on success, negative value on failure
 */
static intptr_t
process_machine_speed_file(struct J9PortLibrary *portLibrary, LinuxStaticInfo *staticInfo)
{
	int32_t ret = 0;
	FILE *speedFP = NULL;
	char speedFile[PATH_MAX] = {0};
	char lineStr[MAX_LINE_LENGTH] = {0};

	ret = get_speed_file_path(portLibrary, speedFile, PATH_MAX);
	if (0 != ret) {
		return ret;
	}

	speedFP = fopen(speedFile, "r");
	if (NULL == speedFP) {
		Trc_PRT_get_processor_speed_fopen_failed(speedFile, errno);
		return J9PORT_ERROR_HYPERVISOR_MACHINE_SPEED_FILE_READ_FAILED;
	}

	while (NULL != fgets (lineStr, MAX_LINE_LENGTH, speedFP)) {

		uint64_t speed = 0;
		char *linePtr = NULL;
		char token[MAX_LINE_LENGTH] = {0};
		char tokenName[MAX_LINE_LENGTH] = {0};
		char tokenValue[MAX_LINE_LENGTH] = {0};

		linePtr = lineStr;

		int32_t num_tokens = sscanf(linePtr, "%127s", token);
		if (1 != num_tokens) {
			continue;
		} 

		if ('#' == token[0]) {
			/* ignore comment lines */
			continue;
		}

		char *endPtr = NULL;
		/* Expect the string to be of the form "machType=mHz" without the quotes */
		num_tokens = sscanf(token, "%[^=]=%s", tokenName, tokenValue);
		if (2 != num_tokens) {
			continue;
		}
		/* Check if the machine type that we read matches with the one that is present
		 * in the staticInfo struct. If so store the speed and we are done.
		 */
		if (0 == strcmp(staticInfo->machType, tokenName)) {
			errno = 0;
			speed = strtoll(tokenValue, &endPtr, COMPUTE_BASE);
			if (errno > 0) {
				ret = J9PORT_ERROR_HYPERVISOR_MACHINE_SPEED_FILE_READ_FAILED;
			} else {
				staticInfo->cpuClockSpeedMHz = speed;
			}
			break;
		}
	}
	fclose(speedFP);

	return ret;
}

/**
 * @internal	Map the machine type obtained from /proc/sysinfo to clock speed MHz
 *		values from a list read from zlinux-machine-speeds.txt file
 *		If the MHz value is not found, it is set to -1
 *
 * @param [in]  portLibrary The port Library
 * @param [out] staticInfo  Structure to be updated
 *
 * @return	0 on success, negative value on failure
 */
static intptr_t
get_processor_speed(struct J9PortLibrary *portLibrary, LinuxStaticInfo *staticInfo)
{
	int32_t ret = 0;

	/* Incase the machine type is not found, MHz info will not be available */
	staticInfo->cpuClockSpeedMHz = -1;

	ret = process_machine_speed_file(portLibrary, staticInfo);
	if (0 != ret) {
		Trc_PRT_get_processor_speed_process_machine_speed_file_failed(ret);
	}

	return ret;
}

/**
 * @internal	Return the mount point for hypfs file system
 *
 * @param [in]  portLibrary The port library
 * @param [out] staticInfo   Structure to be updated
 *
 * @return	0 on success, negative value on failure
 */
static intptr_t
check_hypfs_mount(struct J9PortLibrary *portLibrary, LinuxStaticInfo *staticInfo)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t ret = 0;
	int32_t hypfsFound = 0;
	FILE *mountPts = NULL;
	struct mntent *mntbuf = NULL;
	const char *hypfsType = "s390_hypfs";
	
	mountPts = setmntent(_PATH_MOUNTED, "r");
	if (NULL == mountPts) {
		ret = J9PORT_ERROR_HYPERVISOR_HYPFS_NOT_MOUNTED;
		Trc_PRT_check_hypfs_mount_setmntent_failed(ret);
		return ret;
	}
	while ((mntbuf = getmntent(mountPts)) != NULL) {
		if (0 == strcmp(mntbuf->mnt_type, hypfsType)) {
			hypfsFound = 1;
			break;
		}
	}
	endmntent(mountPts);

	if (0 == hypfsFound) {
		ret = J9PORT_ERROR_HYPERVISOR_HYPFS_NOT_MOUNTED;
		Trc_PRT_check_hypfs_mount_failed(ret);
		return ret;
	}

	staticInfo->hypfsDir = omrmem_allocate_memory(strlen(mntbuf->mnt_dir) + 1, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == staticInfo->hypfsDir) {
		ret = J9PORT_ERROR_HYPERVISOR_MEMORY_ALLOC_FAILED;
		Trc_PRT_check_hypfs_mount_failed(ret);
		return ret;
	}
	/* get the mount path */
	strcpy(staticInfo->hypfsDir, mntbuf->mnt_dir);
	return 0;
}

/**
 * @internal	Read /proc/sysinfo to get the VMName and system-z machine type
 * 		We are looking at the name of the topmost level guest (the one that
 * 		we are currently running in) in the hypervisor hierarchy.
 * 		VM00 always points to the top level guest in sysinfo
 *
 * @param [in]  portLibrary The port library
 * @param [out] staticInfo   Structure to be updated
 *
 * @return	0 on success, negative on failure
 */
static intptr_t
get_guest_info(struct J9PortLibrary *portLibrary, LinuxStaticInfo *staticInfo)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	FILE *sysinfoFP = NULL;
	char lineStr[MAX_LINE_LENGTH] = {0};
	const char *procSysinfo = "/proc/sysinfo";

	sysinfoFP = fopen(procSysinfo, "r");
	if (NULL == sysinfoFP) {
		Trc_PRT_get_guest_info_fopen_failed(errno);
		omrerror_set_last_error(errno, J9PORT_ERROR_HYPERVISOR_SYSINFO_READ_FAILED);
		Trc_PRT_get_guest_info_fopen_failed(J9PORT_ERROR_HYPERVISOR_SYSINFO_READ_FAILED);
		return J9PORT_ERROR_HYPERVISOR_SYSINFO_READ_FAILED;
	}

	/* Read in the file and check for the tokens */
	while (NULL != fgets(lineStr, MAX_LINE_LENGTH, sysinfoFP)) {
		char *linePtr = NULL;
		char tokenName[MAX_LINE_LENGTH] = {0};
		char tokenValue1[MAX_LINE_LENGTH] = {0};
		char tokenValue2[MAX_LINE_LENGTH] = {0};
		const char *sysinfoType = "Type:";
		const char *sysinfoVmName = "Name:";
		const char *sysinfoVmNumber = "VM00";

		linePtr = lineStr;
		int32_t tokens = sscanf(linePtr, "%127s %127s %s %*s", tokenName, tokenValue1, tokenValue2);
		if (tokens >= 2) {
			if ((0 == strcmp(tokenName, sysinfoVmNumber)) &&
			    (0 == strcmp(tokenValue1, sysinfoVmName))) {
				strncpy(staticInfo->VMName, tokenValue2, MAX_LINE_LENGTH-1);
			} else if (0 == strcmp(tokenName, sysinfoType)) {
				strncpy(staticInfo->machType, tokenValue1, MAX_LINE_LENGTH-1);
			}
		}
	}
	fclose(sysinfoFP);

	if (('\0' == *staticInfo->VMName) || ('\0' == *staticInfo->machType)) {
		Trc_PRT_get_guest_info_failed(J9PORT_ERROR_HYPERVISOR_SYSINFO_READ_FAILED);
		return J9PORT_ERROR_HYPERVISOR_SYSINFO_READ_FAILED;
	}

	return 0;
}

/**
 * @internal	Read in the data from the hypfs attribute file specified
 *
 * @param [in]  portLibrary The port library
 * @param [in]  filepath    Absolute file path
 * @param [in]  attribName  Name of the attribute file
 * @param [out] attribVal   Value to be read in from the attribute file and returned
 *
 * @return	0 on success, negative value on failure
 */
static intptr_t
get_hypfs_attribute(struct J9PortLibrary *portLibrary, const char *filepath, const char *attribName, char* attribVal)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);

	char *val = NULL;
	FILE *hypfsFP = NULL;
	char filename[PATH_MAX] = {0};
	const char *format = "%s%s";
	uintptr_t combinedLength = 0;

	/* Get the complete attribute path name */
	combinedLength = omrstr_printf(NULL, 0, format, filepath, attribName);
	if (combinedLength >= PATH_MAX) {
		Trc_PRT_get_hypfs_attribute_file_path_size_exceeds_limit(combinedLength, PATH_MAX, J9PORT_ERROR_HYPERVISOR_HYPFS_READ_FAILED);
		return J9PORT_ERROR_HYPERVISOR_HYPFS_READ_FAILED;
	}
	omrstr_printf(filename, PATH_MAX, format, filepath, attribName);

	hypfsFP = fopen(filename, "r");
	if (NULL == hypfsFP) {
		Trc_PRT_get_hypfs_attribute_fopen_failed(filename, errno);
		omrerror_set_last_error(errno, J9PORT_ERROR_HYPERVISOR_HYPFS_READ_FAILED);
		Trc_PRT_get_hypfs_attribute_fopen_failed(filename, J9PORT_ERROR_HYPERVISOR_HYPFS_READ_FAILED);
		return J9PORT_ERROR_HYPERVISOR_HYPFS_READ_FAILED;
	}
	val = fgets(attribVal, PATH_MAX, hypfsFP);
	fclose(hypfsFP);

	if ('\0' == *attribVal) {
		Trc_PRT_get_hypfs_attribute_failed(filename, J9PORT_ERROR_HYPERVISOR_HYPFS_READ_FAILED);
		return J9PORT_ERROR_HYPERVISOR_HYPFS_READ_FAILED;
	}

	return 0;
}

#define GET_ATTRIB(path, attribute, buf, dest, ret, func, errlabel) \
do {									\
	char *endPtr = NULL;						\
									\
	ret = get_hypfs_attribute(portLibrary, path, attribute, buf);	\
	if (ret < 0) {							\
		goto errlabel;						\
	}								\
	errno = 0;							\
	dest = func(buf, &endPtr, COMPUTE_BASE);			\
	if (0 != errno) {						\
		ret = errno;						\
		goto errlabel;						\
	}								\
} while(0)

/**
 * @internal	Read in the various attributes required from hypfs
 *
 * @param [in]  portLibrary The port library
 * @param [in]  staticInfo  Constants
 * @param [out] linuxInfo   Structure to be updated
 *
 * @return	0 on success, 1 if the timestamps didn't match, negative value on error
 */
static intptr_t
read_hypfs_data(struct J9PortLibrary *portLibrary, LinuxStaticInfo *staticInfo, LinuxzVMInfo *linuxInfo)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t ret = 0;
	char *hypName = NULL;
	char buf[PATH_MAX] = {0};
	char filepath[PATH_MAX] = {0};
	const char *format = "%s%s%s";
	uintptr_t combinedLength = 0;
	/* z/VM attribute files on hypfs */
	const char *hypfsSystem = "/systems/";
	const char *zvmTimestamp = "/onlinetime_us";
	const char *zvmCputime = "/cpus/cputime_us";
	const char *zvmDedicatedCpu = "/cpus/dedicated";
	const char *zvmCpuWeightMax = "/cpus/weight_max";
	const char *zvmVcpuCount = "/cpus/count";
	const char *zvmMaxMem = "/mem/max_KiB";
	const char *zvmUsedMem = "/mem/used_KiB";

	hypName = (char *) PHD_vendorDetails.hypervisorName;

	combinedLength = omrstr_printf(NULL, 0, format, staticInfo->hypfsDir, hypfsSystem, staticInfo->VMName);
	if (combinedLength >= PATH_MAX) {
		ret = J9PORT_ERROR_HYPERVISOR_HYPFS_READ_FAILED;
		Trc_PRT_read_hypfs_data_file_path_size_exceeds_limit(combinedLength, PATH_MAX, ret);
		return ret;
	}
	omrstr_printf(filepath, PATH_MAX, format, staticInfo->hypfsDir, hypfsSystem, staticInfo->VMName);

	if (0 == strcmp(hypName, HYPE_NAME_ZVM)) {
		/* If it is z/VM, read the data as seen by the top level hypervisor */
		GET_ATTRIB(filepath, zvmTimestamp, buf, linuxInfo->timestamp, ret, strtoll, errorout);
		GET_ATTRIB(filepath, zvmCputime, buf, linuxInfo->cpuTime, ret, strtoll, errorout);
		GET_ATTRIB(filepath, zvmDedicatedCpu, buf, linuxInfo->dedicatedCpu, ret, strtol, errorout);
		GET_ATTRIB(filepath, zvmCpuWeightMax, buf, linuxInfo->cpuWeightMax, ret, strtol, errorout);
		GET_ATTRIB(filepath, zvmVcpuCount, buf, linuxInfo->vcpuCount, ret, strtol, errorout);
		GET_ATTRIB(filepath, zvmMaxMem, buf, linuxInfo->maxMemLimit, ret, strtoll, errorout);
		GET_ATTRIB(filepath, zvmUsedMem, buf, linuxInfo->memUsed, ret, strtoll, errorout);
	} else if (0 == strcmp(hypName, HYPE_NAME_PRSM) || 0 == strcmp(hypName, HYPE_NAME_KVM)) {
		/* We do not support PR/SM and KVM on Z at this time */
		linuxInfo->cpuTime = -1;
		linuxInfo->memUsed = -1;
		linuxInfo->vcpuCount = -1;
		linuxInfo->timestamp = -1;
		linuxInfo->maxMemLimit = -1;
	}

errorout:
	if (0 != ret || 0 == linuxInfo->vcpuCount) {
		Trc_PRT_read_hypfs_data_failed(ret);
		omrerror_set_last_error(ret, J9PORT_ERROR_HYPERVISOR_HYPFS_READ_FAILED);
		return J9PORT_ERROR_HYPERVISOR_HYPFS_READ_FAILED;
	}
	return 0;
}

#define UPDATE_VAL		1	/* Value to write to update file */
#define UPDATE_INTERVAL		1	/* update interval in seconds */
#define UPDATE_LOOP_PAUSE	1	/* Pause loop for 1 ms */
#define ITERATION_MAX		10000	/* Dont iterate the update loop forever */

#define DATA_UNREAD		0	/* hypfs data still unread */
#define DATA_READ		1	/* post hypfs data read */

/**
 * @internal	Check the hypfs file /$HYPFS_MOUNT/update for when it was written to
 * 		If updated within the last second, we will use the existing
 *		stats. If not write to the file to get a new reading
 *		hypfs data is only updated when we specifically write to the file /$HYPFS_MOUNT/update
 *		On writing a "1" to this file, hypfs then proceeds to delete all existing hypfs
 *		files and recreates them with the latest data.
 *		The above operation is very dicey when multiple processes are writing to the update file
 *		and reading from the hypfs files. hypfs also does not allow two writes to the "update"
 *		file within the span of a second. To prevent issues with multiple processes accessing hypfs
 *		at the same time, the following protocol is followed
 *		1. check the file write time of the "update" file using stat.
 *		2. If the write has happened within the last second, go to step 5
 *		3. Write "1" to the "update" file
 *		4. Go back to step 1
 *		5. Read in the required data from hypfs
 *		6. Check the file write time of the "update" file again
 *		7. If the time from 6 is not matching 1, update file write timestamp,?go back to 5
 *
 * @param [in]  portLibrary The port library
 * @param [in]  staticInfo  Constants
 * @param [out] linuxInfo   Caller supplied LinuxzVMInfo that gets updated
 * @param [out] dataCycle   Indicates if the data has already been read
 *
 * @return	0 if successful, negative value on error
 */
static intptr_t
check_and_update_stat(struct J9PortLibrary *portLibrary, LinuxStaticInfo *staticInfo, LinuxzVMInfo *linuxInfo, int32_t *dataCycle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t ret = 0;
	int64_t iterations = ITERATION_MAX;
	char hypfsUpdate[PATH_MAX] = {0};
	const char *hypfsUpdateName = "/update";
	const char *format = "%s%s";
	uintptr_t combinedLength = 0;

	combinedLength = omrstr_printf(NULL, 0, format, staticInfo->hypfsDir, hypfsUpdateName);
	if (combinedLength >= PATH_MAX) {
		ret = J9PORT_ERROR_HYPERVISOR_HYPFS_STAT_FAILED;
		Trc_PRT_check_and_update_stat_file_path_size_exceeds_limit(combinedLength, PATH_MAX, ret);
		return ret;
	}
	omrstr_printf(hypfsUpdate, PATH_MAX, format, staticInfo->hypfsDir, hypfsUpdateName);

	do {	
		struct stat updateStat;
		
		/* get the timestamp of when the update file was written to */
		ret = stat(hypfsUpdate, &updateStat);
		if (ret < 0) {
			Trc_PRT_check_and_update_stat_stat_failed(errno);
			ret = J9PORT_ERROR_HYPERVISOR_HYPFS_STAT_FAILED;
			Trc_PRT_check_and_update_stat_stat_failed(ret);
			omrerror_set_last_error(errno, ret);
			break;
		}

		/* If attribute data has already been read, check if current update time matches stored time */
		if (DATA_READ == *dataCycle) {
			if (linuxInfo->updateLastStat != updateStat.st_mtime) {
				/* Data inconsistent, need to read again */
				*dataCycle = DATA_UNREAD;
			}	
			break;
		}

		/* If attribute data is yet to be read, check if update file was written to within the last second.
		 * Store the time and exit if so. Else write to the file and loop back
		 */
		if (DATA_UNREAD == *dataCycle) {
			time_t currentEpoch = time(NULL);
			/* Return if it was updated within the last second */
			if (currentEpoch <= (updateStat.st_mtime + UPDATE_INTERVAL)) {
				linuxInfo->updateLastStat = updateStat.st_mtime;
				break;
			}

			/* Data needs to be updated */
			int32_t updatefd = open(hypfsUpdate, O_WRONLY);
			if (updatefd < 0) {
				Trc_PRT_check_and_update_stat_open_failed(errno);
				ret = J9PORT_ERROR_HYPERVISOR_HYPFS_UPDATE_OPEN_FAILED;
				Trc_PRT_check_and_update_stat_open_failed(ret);
				omrerror_set_last_error(errno, ret);
				return ret;
			}
			char byte = UPDATE_VAL;
			int32_t numBytes = write(updatefd, &byte, UPDATE_VAL);
			close(updatefd);

			/* The update file cannot be written to more than once per second */
			if (numBytes < 0) {
				if (EBUSY == errno) {
					continue;
				}
				else {
					Trc_PRT_check_and_update_stat_write_failed(errno);
					ret = J9PORT_ERROR_HYPERVISOR_HYPFS_UPDATE_WRITE_FAILED;
					Trc_PRT_check_and_update_stat_write_failed(ret);
					omrerror_set_last_error(errno, ret);
					break;
				}
			}
		}
		omrthread_sleep(UPDATE_LOOP_PAUSE);
	} while (--iterations > 0);

	if (0 == iterations) {
		ret = J9PORT_ERROR_HYPERVISOR_HYPFS_UPDATE_LOOP_TIMEOUT;
		Trc_PRT_check_and_update_stat_update_loop_timeout(ret);
	}
	return ret;
}

/**
 * Lookup the error code to find the corresponding nls message and save it for later use
 *
 * @param [in]  portLibrary The port library
 * @param [in]  errCode	    The error code to be saved
 */
static void
log_hypervisor_error(struct J9PortLibrary *portLibrary, uint32_t module_name, uint32_t message_num)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	const char *errMsg = OMRPORTLIB->nls_lookup_message(OMRPORTLIB, J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
							     module_name,
							     message_num,
							     NULL);
	save_error_message(portLibrary, (char *) errMsg);
}

/**
 * Get Linux usage info from hypfs and constants from /proc/sysinfo
 *
 * @param [in]  portLibrary The port library
 * @param [in]  staticInfo  Constants
 * @param [out] linuxInfo   Caller supplied LinuxzVMInfo that gets updated
 *
 * @return	0 on success, negative value on failure
 */
static intptr_t
get_linux_systemz_info(struct J9PortLibrary *portLibrary, LinuxStaticInfo *staticInfo, LinuxzVMInfo *linuxInfo)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t ret = 0;
	int32_t dataCycle = DATA_UNREAD;
	omrthread_monitor_t hyperMonitor = staticInfo->hyperMonitor;

	/*
	 * Need to synchronize the threads that are accessing the hypfs data as hypfs data
	 * update mechanism is not concurrent
	 */
	omrthread_monitor_enter(hyperMonitor);

	/* Check stat time of update file and write to it if not recent */
	ret = check_and_update_stat(portLibrary, staticInfo, linuxInfo, &dataCycle);
	if (ret < 0) {
		Trc_PRT_get_linux_systemz_info_check_and_update_stat_failed(ret);
		log_hypervisor_error(portLibrary, J9NLS_PORT_HYPFS_UPDATE_FAILED);
		omrerror_set_last_error_with_message(ret, PHD_vendorErrMsg);
		goto err_exit;
	}

	do {
		/* Get all the attributes */
		ret = read_hypfs_data(portLibrary, staticInfo, linuxInfo);
		if (ret < 0) {
			Trc_PRT_get_linux_systemz_info_read_hypfs_data_failed(ret);
			log_hypervisor_error(portLibrary, J9NLS_PORT_HYPFS_READ_FAILED);
			omrerror_set_last_error_with_message(ret, PHD_vendorErrMsg);
			break;
		}

		dataCycle = DATA_READ;	
		/* See if the update file has been written to since we last checked */
		ret = check_and_update_stat(portLibrary, staticInfo, linuxInfo, &dataCycle);
		if (ret < 0) {
			Trc_PRT_get_linux_systemz_info_check_and_update_stat_failed(ret);
			log_hypervisor_error(portLibrary, J9NLS_PORT_HYPFS_UPDATE_FAILED);
			omrerror_set_last_error_with_message(ret, PHD_vendorErrMsg);
			break;
		}
	} while (DATA_UNREAD == dataCycle);

err_exit:
	omrthread_monitor_exit(hyperMonitor);

	return ret;
}

/**
 * Returns processor usage statistics of the Guest VM as seen by the hypervisor
 * Uses hypfs to get the usage details
 *
 * @param [in]  portLibrary The port Library
 * @param [out] gpUsage     Caller supplied J9GuestProcessorUsage structure
 * 			    that gets filled in with usage statistics
 *
 * @return 0 on success, a negative value on failure
 */
static intptr_t
get_linux_systemz_processor_usage(struct J9PortLibrary *portLibrary, J9GuestProcessorUsage *gpUsage)
{
	int32_t ret = 0;
	LinuxzVMInfo linuxInfo;
	LinuxStaticInfo *staticInfo = (struct LinuxStaticInfo *) PHD_vendorPrivateData;

	memset(&linuxInfo, 0, sizeof(LinuxzVMInfo));

	/* get_linux_systemz_info fills in the linuxInfo structure */
	ret = get_linux_systemz_info(portLibrary, staticInfo, &linuxInfo);
	if (ret < 0) {
		Trc_PRT_get_linux_systemz_processor_usage_get_linux_systemz_info_failed(ret);
		return ret;
	}

	/* Clock speed is statically calculated as of now */
	gpUsage->hostCpuClockSpeed = staticInfo->cpuClockSpeedMHz;
	/* cpuTime info is already in microseconds */
	gpUsage->cpuTime = linuxInfo.cpuTime;
	/* Entitlement information is unavailable from z/VM currently. */
	gpUsage->cpuEntitlement = -1;
	/* timestamp is already in microseconds */
	gpUsage->timestamp = linuxInfo.timestamp;

	return 0;
}

#define KB_IN_MB	1024

/**
 * Returns memory usage statistics of the Guest VM as seen by the hypervisor
 * Uses hypfs to get the usage details
 *
 * @param [in]  portLibrary The port Library
 * @param [out] gmUsage     Caller supplied J9GuestMemoryUsage structure
 * 			    that gets filled in with usage statistics
 *
 * @return 0 on success, a negative value on failure
 */
static intptr_t
get_linux_systemz_memory_usage(struct J9PortLibrary *portLibrary, J9GuestMemoryUsage *gmUsage)
{
	int32_t ret = 0;
	LinuxzVMInfo linuxInfo;
	LinuxStaticInfo *staticInfo = (struct LinuxStaticInfo *) PHD_vendorPrivateData;

	memset(&linuxInfo, 0, sizeof(LinuxzVMInfo));

	/* get_linux_systemz_info fills in the linuxInfo structure */
	ret = get_linux_systemz_info(portLibrary, staticInfo, &linuxInfo);
	if (ret < 0) {
		Trc_PRT_get_linux_systemz_memory_usage_get_linux_systemz_info_failed(ret);
		return ret;
	}

	/* hypfs stores memory data in kbytes */
	gmUsage->memUsed = linuxInfo.memUsed/ KB_IN_MB;
	gmUsage->maxMemLimit = linuxInfo.maxMemLimit / KB_IN_MB;
	/* timestamp is already in microseconds */
	gmUsage->timestamp = linuxInfo.timestamp;

	return 0;
}

/**
 * Cleanup function called at java shutdown time.
 * Close all open handles and free any memory allocated at startup
 *
 * @param [in]  portLibrary The port Library
 */
static void
linux_systemz_shutdown(struct J9PortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	LinuxStaticInfo *staticInfo = (struct LinuxStaticInfo *) PHD_vendorPrivateData;

	Trc_Assert_PRT_linux_systemz_shutdown_NULL_staticInfo(staticInfo != NULL);

	omrthread_monitor_t hyperMonitor = staticInfo->hyperMonitor;
	omrthread_monitor_destroy(hyperMonitor);

	if (NULL != staticInfo->hypfsDir) {
		omrmem_free_memory(staticInfo->hypfsDir);
	}
	omrmem_free_memory(staticInfo);
	PHD_hypFunc.get_guest_processor_usage = NULL;
	PHD_hypFunc.get_guest_memory_usage = NULL;
	PHD_hypFunc.hypervisor_impl_shutdown = NULL;
	PHD_vendorPrivateData = NULL;
}

/**
 * Called when the j9hypervisor_get_guest_processor_usage or
 * j9hypervisor_get_guest_memory_usage API are called for the first time
 * and if we are running on z/VM or PR/SM.
 *
 * @param [in]  portLibrary The port Library
 *
 * @return	0 on success, negative value on failure
 */
intptr_t
linux_systemz_startup(struct J9PortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t ret = 0;
	LinuxStaticInfo *staticInfo = NULL;

	staticInfo = omrmem_allocate_memory(sizeof(LinuxStaticInfo), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == staticInfo) {
		ret = J9PORT_ERROR_HYPERVISOR_MEMORY_ALLOC_FAILED;
		Trc_PRT_linux_systemz_startup_allocate_memory_failed(ret);
		goto mem_fail;
	}

	memset(staticInfo, 0, sizeof(LinuxStaticInfo));

	/* Get the hypfs mount point */
	ret = check_hypfs_mount(portLibrary, staticInfo);
	if (0 != ret) {
		Trc_PRT_linux_systemz_startup_check_hypfs_mount_failed(ret);
		log_hypervisor_error(portLibrary, J9NLS_PORT_HYPFS_NOT_MOUNTED);
		goto op_fail;
	}

	/* Read info from /proc/sysinfo */
	ret = get_guest_info(portLibrary, staticInfo);
	if (0 != ret) {
		Trc_PRT_linux_systemz_startup_get_guest_info_failed(ret);
		log_hypervisor_error(portLibrary, J9NLS_PORT_SYSINFO_FILE_READ_FAILED);
		goto op_fail;
	}

	/* Convert machine type to MHz */
	ret = get_processor_speed(portLibrary, staticInfo);
	if (0 != ret) {
		/* Record the error but continue as not having MHz info is not fatal */
		Trc_PRT_linux_systemz_startup_get_processor_speed_failed(ret);
	}

	/* initialize the Hypervisor monitor. Needed for synchronization between multiple threads */
	ret = omrthread_monitor_init(&(staticInfo->hyperMonitor), 0);
	if (0 != ret) {
		Trc_PRT_linux_systemz_startup_monitor_init_failed(ret);
		goto op_fail;
	}

	/* Setup pointers to the getter and shutdown functions */
	PHD_hypFunc.get_guest_processor_usage = get_linux_systemz_processor_usage;
	PHD_hypFunc.get_guest_memory_usage = get_linux_systemz_memory_usage;
	PHD_hypFunc.hypervisor_impl_shutdown = linux_systemz_shutdown;
	PHD_vendorPrivateData = (void *) staticInfo;
	PHD_vendorStatus = HYPERVISOR_VENDOR_INIT_SUCCESS;
	Trc_PRT_linux_systemz_startup_exit(ret);

	return 0;

op_fail:
	if (NULL != staticInfo->hypfsDir) {
		omrmem_free_memory(staticInfo->hypfsDir);
	}
	omrmem_free_memory(staticInfo);
mem_fail:
	PHD_vendorStatus = (int32_t) ret;
	Trc_PRT_linux_systemz_startup_error_exit(ret);
	return ret;
}

/**
 * Common function for systemz startup.
 * Calls the appropriate architecture specific startup function
 *
 * @param [in] portLibrary The port Library
 *
 * @return 0 on success and negative on failure
 */
intptr_t
systemz_startup(struct J9PortLibrary *portLibrary)
{
        return linux_systemz_startup(portLibrary);
}
