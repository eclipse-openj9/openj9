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
 * @file sysinfo_helpers.c
 * @ingroup Port
 * @brief Contains implementation of methods to retrieve memory and processor info details.
 */

#include <errno.h>
#include <sys/__wlm.h>

#include "atoe.h"
#include "j9port.h"
#include "omrsimap.h"
#include "omrzfs.h"
#include "j9sysinfo_helpers.h"
#include "ut_j9prt.h"

/* Forward declarations. */
static int32_t
computeCpuTime(struct J9PortLibrary *portLibrary, int64_t *cpuTime);

static int32_t
getProcessorSpeed(struct J9PortLibrary *portLibrary, int64_t *cpuSpeed);

intptr_t
retrieveZGuestMemoryStats(struct J9PortLibrary *portLibrary, struct J9GuestMemoryUsage *gmUsage)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	J9RMCT * __ptr32 rmctp = ((J9PSA * __ptr32)0)->flccvt->cvtopctp;
	J9RCE * __ptr32 rcep = ((J9PSA * __ptr32)0)->flccvt->cvtrcep;
	J9PVT * __ptr32 pvtp = ((J9PSA * __ptr32)0)->flccvt->cvtpvtp;
	J9RIT * __ptr32 ritp = pvtp->pvtritp;

	Trc_PRT_retrieveZGuestMemoryStats_Entered();

	/* Total number of frames currently on all available frame queues. Also, each frame
	 * is page sized.
	 */
	gmUsage->memUsed = ((uint64_t)(rcep->rcepool - rcep->rceafc) * J9BYTES_PER_PAGE);

	/* This is in bytes already - no conversion required. */
	gmUsage->maxMemLimit = (uint64_t)ritp->rittos;

	/* The timestamp when the above data was collected. */
	gmUsage->timestamp = omrtime_nano_time() / NANOSECS_PER_USEC;

	Trc_PRT_retrieveZGuestMemoryStats_Exit(0);
	return 0;
}

intptr_t
retrieveZGuestProcessorStats(struct J9PortLibrary *portLibrary, struct J9GuestProcessorUsage *gpUsage)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;

	Trc_PRT_retrieveZGuestProcessorStats_Entered();

	/* This is not available on Z/OS yet. When available, populate the field. */
	gpUsage->cpuEntitlement = -1;

	/* Obtain CPU time and the CPU speed. */
	rc = computeCpuTime(portLibrary, &gpUsage->cpuTime);
	if (0 != rc) {
		goto _leave_routine;
	}

	rc = getProcessorSpeed(portLibrary, &gpUsage->hostCpuClockSpeed);
	if (0 != rc) {
		goto _leave_routine;
	}

	/* The timestamp when the above data was collected. */
	gpUsage->timestamp = omrtime_nano_time() / NANOSECS_PER_USEC;

_leave_routine:
	Trc_PRT_retrieveZGuestProcessorStats_Exit(rc);
	return rc;
}

/**
 * Compute the CPU time on Z/OS (over PR/SM and z/VM) since boot.
 *
 * @param [in] portLibrary The port library.
 * @param [out] cpuTime The CPU time, as computed by the function.
 *
 * @return error code. 0, on success; negative value, in case of  an error.
 */
static int32_t
computeCpuTime(struct J9PortLibrary *portLibrary, int64_t *cpuTime)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t anslen = 1048;
	int32_t i = 0;
	int32_t rc = 0;
	size_t lpdatlen = 0;
	uint64_t uncappedSu = 0;
	uint64_t cappedSu = 0;
	J9LPDat *lpdatp = NULL;
	J9LPDatServiceTableEntry *entryP = NULL;
	struct sysi *sysip = NULL;
	struct sysi_entry *sysi_entryp = NULL;

	Trc_PRT_computeCpuTime_Entered();

	/* First things first: Query size of the LPDAT structure. */
	lpdatlen = omrreq_lpdatlen();
	if (lpdatlen < 0) {
		/* The query failed!! */
		Trc_PRT_computeCpuTime_unexpectedLPDataBuffSz(lpdatlen);
		Trc_PRT_computeCpuTime_Exit(J9PORT_ERROR_HYPERVISOR_LPDAT_QUERY_FAILED);
		return J9PORT_ERROR_HYPERVISOR_LPDAT_QUERY_FAILED;
	}

	/* Using this length retrieved allocate buffer for an LDAP structure. */
	lpdatp = (J9LPDat *)omrmem_allocate_memory(lpdatlen, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == lpdatp) {
		Trc_PRT_computeCpuTime_memAllocFailed();
		Trc_PRT_computeCpuTime_Exit(J9PORT_ERROR_HYPERVISOR_MEMORY_ALLOC_FAILED);
		return J9PORT_ERROR_HYPERVISOR_MEMORY_ALLOC_FAILED;
	}

	/* Initialize chunk of memory and set the length field in the structure. */
	memset(lpdatp, 0, lpdatlen);
	lpdatp->length = lpdatlen;

	/* We now query for the LPAR Data to obtain the CPU service units. */
	rc = omrreq_lpdat((char*)lpdatp);
	if (0 != rc) {
		omrmem_free_memory(lpdatp);
		Trc_PRT_computeCpuTime_failedRetrievingLparData(rc);
		Trc_PRT_computeCpuTime_Exit(J9PORT_ERROR_HYPERVISOR_LPDAT_QUERY_FAILED);
		return J9PORT_ERROR_HYPERVISOR_LPDAT_QUERY_FAILED;
	}

	/* Sum up service units in capped and uncapped modes for all entries that are
	 * present - by default, there being 48 entries, unless configured otherwise.
	 */
	for (i = 0; i < lpdatp->serviceTableEntries; i++) {
		entryP = (J9LPDatServiceTableEntry*) ((char*) lpdatp +
				lpdatp->serviceTableOffset +
				(i * lpdatp->serviceTableEntryLength));

		uncappedSu += entryP->serviceUncapped,
		cappedSu += entryP->serviceCapped;
	}

	/* On a capped partition, CPU time is sum of both, capped as well as uncapped service
	 * units. For an uncapped partition, CPU time is just the uncapped service units.
	 */
	*cpuTime = J9LPAR_CAPPED(lpdatp) ? (uncappedSu + cappedSu) : uncappedSu;

	sysip = (struct sysi *)omrmem_allocate_memory(anslen, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == sysip) {
		omrmem_free_memory(lpdatp);
		Trc_PRT_computeCpuTime_memAllocFailed();
		Trc_PRT_computeCpuTime_Exit(J9PORT_ERROR_HYPERVISOR_MEMORY_ALLOC_FAILED);
		return J9PORT_ERROR_HYPERVISOR_MEMORY_ALLOC_FAILED;
	}

	/* Query the Work Load Manager (WLM) statistics. */
	rc = QueryMetrics(sysip, &anslen);
	if (-1 == rc) {
		rc = errno;
		omrmem_free_memory(lpdatp);
		omrmem_free_memory(sysip);
		Trc_PRT_computeCpuTime_queryMetricsFailed(rc);
		Trc_PRT_computeCpuTime_Exit(J9PORT_ERROR_HYPERVISOR_LPDAT_QUERY_FAILED);
		return J9PORT_ERROR_HYPERVISOR_LPDAT_QUERY_FAILED;
	}
	sysi_entryp = (struct sysi_entry*) ((char*)sysip + sysip->sysi_header_size);

	/* Convert the service units to microseconds. */
	*cpuTime = (((float)*cpuTime) / (float)sysi_entryp->sysi_cpu_up) * J9PORT_TIME_US_PER_SEC;

	omrmem_free_memory(lpdatp);
	omrmem_free_memory(sysip);
	Trc_PRT_computeCpuTime_Exit(0);
	return 0;
}

/**
 * @internal    Function detects the current machine type by querying the Virtual
 *              Server interface and based on that returns the cpu speed.
 *
 * @param [in]  portLibrary The port Library
 * @param [out] cpuSpeed    System type for which we want to find the CPU speed.
 *
 * @return      0 on success, negative value on failure
 */
static int32_t
getProcessorSpeed(struct J9PortLibrary *portLibrary, int64_t *cpuSpeed)
{
	int32_t rc = 0;
	struct qvs qvsInst;

	qvsInst.qvslen = sizeof(qvsInst);
	rc = iwmqvs(&qvsInst);
	if ((0 == rc) && (qvsInst.qvscecvalid)) {
		*cpuSpeed = ((int64_t)qvsInst.qvsceccapacity);
	} else {
		rc = J9PORT_ERROR_HYPERVISOR_CLOCKSPEED_NOT_FOUND;
		Trc_PRT_getProcessorSpeed_vsQueryFailed(rc);
	}

	return rc;
}
