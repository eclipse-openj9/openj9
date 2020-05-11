/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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
#endif
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

#if (defined(J9X86) || defined(J9HAMMER) || defined(S390) || defined(J9ZOS390))
#include "j9sysinfo_helpers.h"
#endif
#if defined(J9ZOS390)
#include "j9csrsi.h"
#endif /* defined(J9ZOS390) */

#if (defined(LINUXPPC) || (defined(S390) && defined(LINUX) && !defined(J9ZTPF)))
#include "auxv.h"
#include <strings.h>
#endif /* (defined(LINUXPPC) || (defined(S390) && defined(LINUX) && !defined(J9ZTPF))) */

#if defined(AIXPPC)
#include <fcntl.h>
#include <sys/procfs.h>
#include <sys/systemcfg.h>
#endif /* defined(AIXPPC) */

/* Start copy from j9filetext.c */
/* __STDC_ISO_10646__ indicates that the platform wchar_t encoding is Unicode */
/* but older versions of libc fail to set the flag, even though they are Unicode */
#if defined(__STDC_ISO_10646__) || defined (LINUX)
#define J9VM_USE_MBTOWC
#else
#include "omriconvhelpers.h"
#endif

/* a2e overrides nl_langinfo to return ASCII strings. We need the native EBCDIC string */
#if defined(J9ZOS390) && defined (nl_langinfo)
#undef nl_langinfo
#endif

/* End copy from j9filetext.c */

#ifdef AIXPPC
#if defined(J9OS_I5)
/* The PASE compiler does not support libperfstat.h */
#else
#include <libperfstat.h>
#endif
#include <sys/proc.h>

/* j9sysinfo_get_number_CPUs_by_type */
#include <sys/thread.h>
#include <sys/rset.h>
#endif

#ifdef RS6000
#include <sys/systemcfg.h>
#include <sys/sysconfig.h>
#include <assert.h>

#if defined( OMR_ENV_DATA64 )
#define LIBC_NAME "/usr/lib/libc.a(shr_64.o)"
#else
#define LIBC_NAME "/usr/lib/libc.a(shr.o)"
#endif

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


#endif

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
#endif
#endif

#define JIFFIES			100
#define USECS_PER_SEC	1000000
#define TICKS_TO_USEC	((uint64_t)(USECS_PER_SEC/JIFFIES))

static int32_t getCacheSize(J9PortLibrary *portLibrary, const int32_t cpu, const int32_t level,
	const int32_t cacheType, const J9CacheQueryCommand query);

#if defined(LINUXPPC)
static intptr_t getLinuxPPCDescription(struct J9PortLibrary *portLibrary, J9ProcessorDesc *desc);

#if !defined(AT_HWCAP2)
#define AT_HWCAP2 26 /* needed until glibc 2.17 */
#endif /* !defined(AT_HWCAP2) */

#elif defined(AIXPPC)
static intptr_t getAIXPPCDescription(struct J9PortLibrary *portLibrary, J9ProcessorDesc *desc);

#if !defined(__power_8)
#define POWER_8 0x10000 /* Power 8 class CPU */
#define __power_8() (_system_configuration.implementation == POWER_8)
#if !defined(J9OS_I5_V6R1)
#define PPI8_1 0x4B
#define PPI8_2 0x4D
#define __phy_proc_imp_8() (_system_configuration.phys_implementation == PPI8_1 || _system_configuration.phys_implementation == PPI8_2)
#endif /* !defined(J9OS_I5_V6R1) */
#endif /* !defined(__power_8) */

#if !defined(__power_9)
#define POWER_9 0x20000 /* Power 9 class CPU */
#define __power_9() (_system_configuration.implementation == POWER_9)
#if defined(J9OS_I5) && !defined(J9OS_I5_V6R1)
#define PPI9 0x4E
#define __phy_proc_imp_9() (_system_configuration.phys_implementation == PPI9)
#endif /* defined(J9OS_I5) && !defined(J9OS_I5_V6R1) */
#endif /* !defined(__power_9) */

#if !defined(__power_10)
#define POWER_10 0x40000 /* Power 10 class CPU */
#define __power_10() (_system_configuration.implementation == POWER_10)
#endif /* !defined(__power_10) */

#if defined(J9OS_I5_V6R1) /* vmx_version id only available since TL4 */
#define __power_vsx() (_system_configuration.vmx_version > 1)
#endif
#if !defined(J9OS_I5_V7R2) && !defined(J9OS_I5_V6R1)
/* both i 7.1 and i 7.2 do not support this function */
#if !defined(SC_TM_VER)
#define SC_TM_VER 59
#endif  /* !defined(SC_TM_VER) */

#if !defined(__power_tm)
#define __power_tm() ((long)getsystemcfg(SC_TM_VER) > 0) /* taken from AIX 7.1 sys/systemcfg.h */
#endif  /* !defined(__power_tm) */
#endif /* !defined(J9OS_I5_V7R2) && !defined(J9OS_I5_V6R1) */

#elif (defined(S390) || defined(J9ZOS390) || defined(J9ZTPF))
static BOOLEAN testSTFLE(struct J9PortLibrary *portLibrary, uint64_t stfleBit);
static intptr_t getS390Description(struct J9PortLibrary *portLibrary, J9ProcessorDesc *desc);
#endif /* defined(S390) || defined(J9ZOS390) || defined(J9ZTPF) */

#if defined(RISCV64)
static intptr_t getRISCV64Description(struct J9PortLibrary *portLibrary, J9ProcessorDesc *desc);
#endif /* defined(RISCV64) */

#if (defined(LINUXPPC) || defined(AIXPPC))
static J9ProcessorArchitecture mapPPCProcessor(const char *processorName);
static void setFeature(J9ProcessorDesc *desc, uint32_t feature);
#endif /* (defined(LINUXPPC) || defined(AIXPPC)) */

static int32_t getCacheLevels(struct J9PortLibrary *portLibrary, const int32_t cpu);
static int32_t getCacheTypes(struct J9PortLibrary *portLibrary, const int32_t cpu, const int32_t level);
static int32_t getCacheSize(struct J9PortLibrary *portLibrary,
	const int32_t cpu, const int32_t level, const int32_t cacheType, const J9CacheQueryCommand query);

uint16_t
j9sysinfo_get_classpathSeparator(struct J9PortLibrary *portLibrary )
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
	uintptr_t handle;
	int (*func)(int command, void *buffer, size_t bufsize);

	if (getenv( "NO_LPAR_RECONFIGURATION" ) == NULL) {
		struct getlpar_info lpar_info;
		char local_lpar_name;
		lpar_info.lpar_flags = 0;
		lpar_info.lpar_namesz = 0;
		lpar_info.lpar_name = &local_lpar_name;
		if (sysconfig(SYS_GETLPAR_INFO, &lpar_info, sizeof(struct getlpar_info)) == 0) {
			if ((lpar_info.lpar_flags & LPAR_ENABLED) && __DR_CPU_ADD()) {
				return TRUE;
			}
		}
	}

	/* Now check is WPAR is enabled and treat it as if DLPAR was. */

	/* look for lpar_get_info in libc */
	if (0 == omrsl_open_shared_library(LIBC_NAME, &handle, 0)) {
		if (0 == omrsl_lookup_name(handle, "lpar_get_info", (uintptr_t *)&func, "IIPi")) {
			rc = func( WPAR_INFO_FORMAT, &info, sizeof( info ));
		}
		omrsl_close_shared_library(handle);
	}

	/* function only exists on aix 5.3 and above */
	if (0 == rc) {
		if (info.wpar_flags & WPAR_INFO_MOBILE) {
			return TRUE;
		}
	}
#endif
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
#if (defined(S390) || defined(J9ZOS390))
	PPG_stfleCache.lastDoubleWord = -1;
#endif
}


int32_t
j9sysinfo_startup(struct J9PortLibrary *portLibrary)
{
#if (defined(S390) || defined(J9ZOS390))
	PPG_stfleCache.lastDoubleWord = -1;
#endif
#if !(defined(RS6000) || defined (LINUXPPC) || defined (PPC) || defined(S390) || defined(J9ZOS390))
	PPG_sysL1DCacheLineSize = -1;
#endif
	return 0;
}

uintptr_t
j9sysinfo_DLPAR_max_CPUs(struct J9PortLibrary *portLibrary)
{
#ifdef RS6000
	/* Ensure that when DLAR or mobility feature of WPAR is enabled that we return at least 2 CPUs for the max.
	 * Why 2?
	 *   On a DLPAR system the max_ncpus will in fact be correct.  However, on a WPAR system there
	 *   is no way to tell how many CPUs there could be on the system we will be going to as that
	 *   system may not even exist yet.  So we choose 2 to make sure that any users of this function
	 *   understand that they have the potential of eventually being in an SMP environment.  We could
	 *   return 10 to accomplish the same thing, but 2 was the agreed upon value. */
	if (_system_configuration.max_ncpus > 1) {
		return _system_configuration.max_ncpus;
	} else if(portLibrary->sysinfo_DLPAR_enabled(portLibrary)) {
		return 2;
	}
	return _system_configuration.max_ncpus;
#else
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	return omrsysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE);
#endif
}

uintptr_t
j9sysinfo_get_processing_capacity(struct J9PortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
#if defined( RS6000 )

	lpar_info_format2_t info;
	int rc = -1;
	uintptr_t handle;
	int (*func)(int command, void *buffer, size_t bufsize);

#if defined(J9OS_I5)
	/* Temp code due to unsupported syscall lpar_get_info for J9OS_I5 */
	return Xj9GetEntitledProcessorCapacity();
#else
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
#endif
#else
	return omrsysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE) * 100;
#endif
}

intptr_t
j9sysinfo_get_processor_description(struct J9PortLibrary *portLibrary, J9ProcessorDesc *desc)
{
	intptr_t rc = -1;
	Trc_PRT_sysinfo_get_processor_description_Entered(desc);

	if (NULL != desc) {
		memset(desc, 0, sizeof(J9ProcessorDesc));

#if (defined(J9X86) || defined(J9HAMMER))
		rc = getX86Description(portLibrary, desc);
#elif defined(LINUXPPC)
		rc = getLinuxPPCDescription(portLibrary, desc);
#elif defined(AIXPPC)
		rc = getAIXPPCDescription(portLibrary, desc);
#elif (defined(S390) || defined(J9ZOS390))
		rc = getS390Description(portLibrary, desc);
#elif defined(RISCV64)
		rc = getRISCV64Description(portLibrary, desc);
#endif
	}

	Trc_PRT_sysinfo_get_processor_description_Exit(rc);
	return rc;
}

/**
 * @internal
 * Helper to set appropriate feature field in a J9ProcessorDesc struct.
 *
 * @param[in] desc pointer to the struct that contains the CPU type and features.
 * @param[in] feature to set
 *
 */
static void
setFeature(J9ProcessorDesc *desc, uint32_t feature)
{
	if ((NULL != desc)
	&& (feature < (J9PORT_SYSINFO_FEATURES_SIZE * 32))
	) {
		uint32_t featureIndex = feature / 32;
		uint32_t featureShift = feature % 32;

		desc->features[featureIndex] = (desc->features[featureIndex] | (1 << (featureShift)));
	}
}

#if defined(LINUXPPC)
/**
 * @internal
 * Populates J9ProcessorDesc *desc on Linux PPC
 *
 * @param[in] desc pointer to the struct that will contain the CPU type and features.
 *
 * @return 0 on success, -1 on failure
 */
static intptr_t
getLinuxPPCDescription(struct J9PortLibrary *portLibrary, J9ProcessorDesc *desc)
{
	char* platform = NULL;
	char* base_platform = NULL;

	/* initialize auxv prior to querying the auxv */
	if (prefetch_auxv()) {
_error:
		desc->processor = PROCESSOR_PPC_UNKNOWN;
		desc->physicalProcessor = PROCESSOR_PPC_UNKNOWN;
		desc->features[0] = 0;
		desc->features[1] = 0;
		return -1;
	}

	/* Linux PPC processor */
	platform = (char *) query_auxv(AT_PLATFORM);
	if ((NULL == platform) || (((char *) -1) == platform)) {
		goto _error;
	}
	desc->processor = mapPPCProcessor(platform);

	/* Linux PPC physical processor */
	base_platform = (char *) query_auxv(AT_BASE_PLATFORM);
	if ((NULL == base_platform) || (((char *) -1) == base_platform)) {
		/* AT_PLATFORM is known from call above.  Default BASE to unknown */
		desc->physicalProcessor = PROCESSOR_PPC_UNKNOWN;
	} else {
		desc->physicalProcessor = mapPPCProcessor(base_platform);
	}

	/* Linux PPC features:
	 * Can't error check these calls as both 0 & -1 are valid
	 * bit fields that could be returned by this query.
	 */
	desc->features[0] = query_auxv(AT_HWCAP);
	desc->features[1] = query_auxv(AT_HWCAP2);

	return 0;
}

/**
 * @internal
 * Maps a PPC processor string to the J9ProcessorArchitecture enum.
 *
 * @param[in] processorName
 *
 * @return A J9ProcessorArchitecture PROCESSOR_PPC_* found otherwise PROCESSOR_PPC_UNKNOWN.
 */
static J9ProcessorArchitecture
mapPPCProcessor(const char *processorName)
{
	J9ProcessorArchitecture rc = PROCESSOR_PPC_UNKNOWN;

	if (0 == strncasecmp(processorName, "ppc403", 6)) {
		rc = PROCESSOR_PPC_PWR403;
	} else if (0 == strncasecmp(processorName, "ppc405", 6)) {
		rc = PROCESSOR_PPC_PWR405;
	} else if (0 == strncasecmp(processorName, "ppc440gp", 8)) {
		rc = PROCESSOR_PPC_PWR440;
	} else if (0 == strncasecmp(processorName, "ppc601", 6)) {
		rc = PROCESSOR_PPC_PWR601;
	} else if (0 == strncasecmp(processorName, "ppc603", 6)) {
		rc = PROCESSOR_PPC_PWR603;
	} else if (0 == strncasecmp(processorName, "ppc604", 6)) {
		rc = PROCESSOR_PPC_PWR604;
	} else if (0 == strncasecmp(processorName, "ppc7400", 7)) {
		rc = PROCESSOR_PPC_PWR603;
	} else if (0 == strncasecmp(processorName, "ppc750", 6)) {
		rc = PROCESSOR_PPC_7XX;
	} else if (0 == strncasecmp(processorName, "rs64", 4)) {
		rc = PROCESSOR_PPC_PULSAR;
	} else if (0 == strncasecmp(processorName, "ppc970", 6)) {
		rc = PROCESSOR_PPC_GP;
	} else if (0 == strncasecmp(processorName, "power3", 6)) {
		rc = PROCESSOR_PPC_PWR630;
	} else if (0 == strncasecmp(processorName, "power4", 6)) {
		rc = PROCESSOR_PPC_GP;
	} else if (0 == strncasecmp(processorName, "power5", 6)) {
		rc = PROCESSOR_PPC_GR;
	} else if (0 == strncasecmp(processorName, "power6", 6)) {
		rc = PROCESSOR_PPC_P6;
	} else if (0 == strncasecmp(processorName, "power7", 6)) {
		rc = PROCESSOR_PPC_P7;
	} else if (0 == strncasecmp(processorName, "power8", 6)) {
		rc = PROCESSOR_PPC_P8;
	} else if (0 == strncasecmp(processorName, "power9", 6)) {
		rc = PROCESSOR_PPC_P9;
	} else if (0 == strncasecmp(processorName, "power10", 7)) {
                rc = PROCESSOR_PPC_P10;
	}

	return rc;
}
#endif /* defined(LINUXPPC) */

#if defined(AIXPPC)
/**
 * @internal
 * Populates J9ProcessorDesc *desc on AIX
 *
 * @param[in] desc pointer to the struct that will contain the CPU type and features.
 *
 * @return 0 on success, -1 on failure
 */
static intptr_t
getAIXPPCDescription(struct J9PortLibrary *portLibrary, J9ProcessorDesc *desc)
{
	/* AIX processor */
	if (__power_rs1() || __power_rsc()) {
		desc->processor = PROCESSOR_PPC_RIOS1;
	} else if (__power_rs2()) {
		desc->processor = PROCESSOR_PPC_RIOS2;
	} else if (__power_601()) {
		desc->processor = PROCESSOR_PPC_PWR601;
	} else if (__power_603()) {
		desc->processor = PROCESSOR_PPC_PWR603;
	} else if (__power_604()) {
		desc->processor = PROCESSOR_PPC_PWR604;
	} else if (__power_620()) {
		desc->processor = PROCESSOR_PPC_PWR620;
	} else if (__power_630()) {
		desc->processor = PROCESSOR_PPC_PWR630;
	} else if (__power_A35()) {
		desc->processor = PROCESSOR_PPC_NSTAR;
	} else if (__power_RS64II()) {
		desc->processor = PROCESSOR_PPC_NSTAR;
	} else if (__power_RS64III()) {
		desc->processor = PROCESSOR_PPC_PULSAR;
	} else if (__power_4()) {
		desc->processor = PROCESSOR_PPC_GP;
	} else if (__power_5()) {
		desc->processor = PROCESSOR_PPC_GR;
	} else if (__power_6()) {
		desc->processor = PROCESSOR_PPC_P6;
	} else if (__power_7()) {
		desc->processor = PROCESSOR_PPC_P7;
	} else if (__power_8()) {
		desc->processor = PROCESSOR_PPC_P8;
	} else if (__power_9()) {
		desc->processor = PROCESSOR_PPC_P9;
	} else if (__power_10()) {
                desc->processor = PROCESSOR_PPC_P10;
	} else {
		desc->processor = PROCESSOR_PPC_UNKNOWN;
	}
#if !defined(J9OS_I5_V6R1)
	/* AIX physical processor */
	if (__phy_proc_imp_4()) {
		desc->physicalProcessor = PROCESSOR_PPC_GP;
	} else if (__phy_proc_imp_5()) {
		desc->physicalProcessor = PROCESSOR_PPC_GR;
	} else if (__phy_proc_imp_6()) {
		desc->physicalProcessor = PROCESSOR_PPC_P6;
	} else if (__phy_proc_imp_7()) {
		desc->physicalProcessor = PROCESSOR_PPC_P7;
	} else if (__phy_proc_imp_8()) {
		desc->physicalProcessor = PROCESSOR_PPC_P8;
	}
#if defined(J9OS_I5)
	else if (__phy_proc_imp_9()) {
		desc->physicalProcessor = PROCESSOR_PPC_P9;
	}
#endif
	else {
		desc->physicalProcessor = desc->processor;
	}
#else
		desc->physicalProcessor = desc->processor;
#endif /* !defined(J9OS_I5_V6R1) */
	/* AIX Features */
	if (__power_64()) {
		setFeature(desc, J9PORT_PPC_FEATURE_64);
	}
	if (__power_vmx()) {
		setFeature(desc, J9PORT_PPC_FEATURE_HAS_ALTIVEC);
	}
	if (__power_dfp()) {
		setFeature(desc, J9PORT_PPC_FEATURE_HAS_DFP);
	}
	if (__power_vsx()) {
		setFeature(desc, J9PORT_PPC_FEATURE_HAS_VSX);
	}
#if !defined(J9OS_I5_V6R1)
	if (__phy_proc_imp_6()) {
		setFeature(desc, J9PORT_PPC_FEATURE_ARCH_2_05);
	}
	if (__phy_proc_imp_4()) {
		setFeature(desc, J9PORT_PPC_FEATURE_POWER4);
	}
#endif /* !defined(J9OS_I5_V6R1) */
#if !defined(J9OS_I5_V7R2) && !defined(J9OS_I5_V6R1)
	if (__power_tm()) {
		setFeature(desc, J9PORT_PPC_FEATURE_HTM);
	}
#endif /* !defined(J9OS_I5_V7R2) && !defined(J9OS_I5_V6R1) */

	return 0;
}
#endif /* defined(AIXPPC) */


#if (defined(S390) || defined(J9ZOS390))

#define LAST_DOUBLE_WORD	2
/**
 * @internal
 * Check if a specific bit is set from STFLE instruction on z/OS and zLinux.
 * STORE FACILITY LIST EXTENDED stores a variable number of doublewords containing facility bits.
 *  see z/Architecture Principles of Operation 4-69
 *
 * @param[in] stfleBit bit to check
 *
 * @return TRUE if bit is 1, FALSE otherwise.
 */
static BOOLEAN
testSTFLE(struct J9PortLibrary *portLibrary, uint64_t stfleBit)
{
	BOOLEAN rc = FALSE;

	STFLEFacilities *mem = &(PPG_stfleCache.facilities);
	uintptr_t *stfleRead = &(PPG_stfleCache.lastDoubleWord);

	/* If it is the first time, read stfle and cache it */
	if (-1 == *stfleRead) {
		*stfleRead = getstfle(LAST_DOUBLE_WORD, (uint64_t*)mem);
	}

	if (stfleBit < 64 && *stfleRead >= 0) {
		rc = (0 != (mem->dw1 & ((J9CONST_U64(1)) << (63 - stfleBit))));
	} else if (stfleBit < 128 && *stfleRead >= 1) {
		rc = (0 != (mem->dw2 & ((J9CONST_U64(1)) << (127 - stfleBit))));
	} else if (stfleBit < 192 && *stfleRead >= 2) {
		rc = (0 != (mem->dw3 & ((J9CONST_U64(1)) << (191 - stfleBit))));
	}

	return rc;
}

#ifdef J9ZOS390
#ifdef _LP64
typedef struct pcb_t
{
	char pcbeye[8];					/* pcbeye = "CEEPCB" */
	char dummy[336];				/* Ignore the rest to get to flag6 field */
	unsigned char ceepcb_flags6;
} pcb_t;
typedef struct ceecaa_t
{
	char dummy[912];				/* pcb is at offset 912 in 64bit */
	pcb_t *pcb_addr;
} ceecaa_t;
#else
typedef struct pcb_t
{
	char pcbeye[8];					/* pcbeye = "CEEPCB" */
	char dummy[76];					/* Ignore the rest to get to flag6 field */
	unsigned char ceepcb_flags6;
} pcb_t;
typedef struct ceecaa_t
{
	char dummy[756];				/* pcb is at offset 756 in 32bit */
	pcb_t *pcb_addr;
} ceecaa_t;
#endif /* ifdef _LP64 */

/** @internal
 *  Check if z/OS supports the Vector Extension Facility (SIMD) by checking whether both the OS and LE support vector
 *  registers. We use the CVTVEF (0x80) bit in the CVT structure for the OS check and bit 0x08 of CEEPCB_FLAG6 field in
 *  the PCB for the LE check.
 *
 *  @return TRUE if VEF is supported; FALSE otherwise.
 */
static BOOLEAN
getS390zOS_supportsVectorExtensionFacility(void)
{
	/* FLCCVT is an ADDRESS off the PSA structure
	 * https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.3.0/com.ibm.zos.v2r3.iead300/PSA-map.htm */
	uint8_t* CVT = (uint8_t*)(*(uint32_t*)0x10);

	/* CVTFLAG5 is a BITSTRING off the CVT structure containing the CVTVEF (0x80) bit
	 * https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.3.0/com.ibm.zos.v2r3.iead100/CVT-map.htm */
	uint8_t CVTFLAG5 = *(CVT + 0x0F4);

	ceecaa_t* CAA = (ceecaa_t *)_gtca();

	if (J9_ARE_ALL_BITS_SET(CVTFLAG5, 0x80)) {
		if (NULL != CAA) {
			return J9_ARE_ALL_BITS_SET(CAA->pcb_addr->ceepcb_flags6, 0x08);
		}
	}

	return FALSE;
}

/** @internal
 *  Check if z/OS supports the Transactional Execution Facility (TX). We use the CVTTX (0x08) and CVTTXC (0x04) bits in
 *  the CVT structure for the OS check.
 *
 *  @return TRUE if TX is supported; FALSE otherwise.
 */
static BOOLEAN
getS390zOS_supportsTransactionalExecutionFacility(void)
{
	/* FLCCVT is an ADDRESS off the PSA structure
	 * https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.3.0/com.ibm.zos.v2r3.iead300/PSA-map.htm */
	uint8_t* CVT = (uint8_t*)(*(uint32_t*)0x10);

	/* CVTFLAG4 is a BITSTRING off the CVT structure containing the CVTTX (0x08), CVTTXC (0x04), and CVTRI (0x02) bits
	 * https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.3.0/com.ibm.zos.v2r3.iead100/CVT-map.htm */
	uint8_t CVTFLAG4 = *(CVT + 0x17B);

	/* Note we check for both constrained and non-constrained transaction support */
	return J9_ARE_ALL_BITS_SET(CVTFLAG4, 0x0C);
}

/** @internal
 *  Check if z/OS supports the Runtime Instrumentation Facility (RI). We use the CVTRI (0x02) bit in the CVT structure
 *  for the OS check.
 *
 *  @return TRUE if RI is supported; FALSE otherwise.
 */
static BOOLEAN
getS390zOS_supportsRuntimeInstrumentationFacility(void)
{
	/* FLCCVT is an ADDRESS off the PSA structure
	 * https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.3.0/com.ibm.zos.v2r3.iead300/PSA-map.htm */
	uint8_t* CVT = (uint8_t*)(*(uint32_t*)0x10);

	/* CVTFLAG4 is a BITSTRING off the CVT structure containing the CVTTX (0x08), CVTTXC (0x04), and CVTRI (0x02) bits
	 * https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.3.0/com.ibm.zos.v2r3.iead100/CVT-map.htm */
	uint8_t CVTFLAG4 = *(CVT + 0x17B);

	return J9_ARE_ALL_BITS_SET(CVTFLAG4, 0x02);
}

/** @internal
 *  Check if z/OS supports the Guarded Storage Facility (GS). We use the CVTGSF (0x01) bit in the CVT structure
 *  for the OS check.
 *
 *  @return TRUE if GS is supported; FALSE otherwise.
 */
static BOOLEAN
getS390zOS_supportsGuardedStorageFacility(void)
{
	/* FLCCVT is an ADDRESS off the PSA structure
	 * https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.3.0/com.ibm.zos.v2r3.iead300/PSA-map.htm */
	uint8_t* CVT = (uint8_t*)(*(uint32_t*)0x10);

	/* CVTFLAG3 is a BITSTRING off the CVT structure containing the CVTGSF (0x01) bit
	 * https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.3.0/com.ibm.zos.v2r3.iead100/CVT-map.htm */
	uint8_t CVTFLAG3 = *(CVT + 0x17A);

	return J9_ARE_ALL_BITS_SET(CVTFLAG3, 0x01);
}
#endif /* ifdef J9ZOS390 */

/**
 * @internal
 * Populates J9ProcessorDesc *desc on z/OS and zLinux
 *
 * @param[in] desc pointer to the struct that will contain the CPU type and features.
 *
 * @return 0 on success, -1 on failure
 */
static intptr_t
getS390Description(struct J9PortLibrary *portLibrary, J9ProcessorDesc *desc)
{
/* Check hardware and OS (z/OS only) support for GS (guarded storage), RI (runtime instrumentation) and TE (transactional memory) */
#if defined(J9ZOS390)
#define S390_STFLE_BIT (0x80000000 >> 7)
	/* s390 feature detection requires the store-facility-list-extended (STFLE) instruction which was introduced in z9
	 * Location 200 is architected such that bit 7 is ON if STFLE instruction is installed */
	if (J9_ARE_NO_BITS_SET(*(int*) 200, S390_STFLE_BIT)) {
		return -1;
	}
#elif defined(J9ZTPF)  /* defined(J9ZOS390) */
	/*
	 * z/TPF requires OS support for some of the Hardware Capabilities.
	 * Setting the auxvFeatures capabilities flag directly to mimic the query_auxv call in Linux.
	 */
	unsigned long auxvFeatures = J9PORT_HWCAP_S390_HIGH_GPRS|J9PORT_S390_FEATURE_ESAN3|J9PORT_HWCAP_S390_ZARCH|
			J9PORT_HWCAP_S390_STFLE|J9PORT_HWCAP_S390_MSA|J9PORT_HWCAP_S390_DFP|
			J9PORT_HWCAP_S390_LDISP|J9PORT_HWCAP_S390_EIMM|J9PORT_HWCAP_S390_ETF3EH;

#elif defined(LINUX) /* defined(J9ZTPF) */
	/* Some s390 features require OS support on Linux, querying auxv for AT_HWCAP bit-mask of processor capabilities. */
	unsigned long auxvFeatures = query_auxv(AT_HWCAP);
#endif /* defined(LINUX) */

#if (defined(S390) && defined(LINUX))
	/* OS Support of HPAGE on Linux on Z */
	if (J9_ARE_ALL_BITS_SET(auxvFeatures, J9PORT_HWCAP_S390_HPAGE)){
		setFeature(desc, J9PORT_S390_FEATURE_HPAGE);
	}
#endif /* defined(S390) && defined(LINUX) */

	/* Miscellaneous facility detection */

	if (testSTFLE(portLibrary, 0)) {
#if (defined(S390) && defined(LINUX))
	/* OS Support for Linux on Z */
		if (J9_ARE_ALL_BITS_SET(auxvFeatures, J9PORT_HWCAP_S390_ESAN3))
#endif /* defined(S390) && defined(LINUX)*/
		{
			setFeature(desc, J9PORT_S390_FEATURE_ESAN3);
		}
	}

	if (testSTFLE(portLibrary, 2)) {
#if (defined(S390) && defined(LINUX))
	/* OS Support for Linux on Z */
		if (J9_ARE_ALL_BITS_SET(auxvFeatures, J9PORT_HWCAP_S390_ZARCH))
#endif /* defined(S390) && defined(LINUX)*/
		{
			setFeature(desc, J9PORT_S390_FEATURE_ZARCH);
		}
	}

	if (testSTFLE(portLibrary, 7)) {
#if (defined(S390) && defined(LINUX))
	/* OS Support for Linux on Z */
		if (J9_ARE_ALL_BITS_SET(auxvFeatures, J9PORT_HWCAP_S390_STFLE))
#endif /* defined(S390) && defined(LINUX)*/
		{
			setFeature(desc, J9PORT_S390_FEATURE_STFLE);
		}
	}

	if (testSTFLE(portLibrary, 17)) {
#if (defined(S390) && defined(LINUX))
	/* OS Support for Linux on Z */
		if (J9_ARE_ALL_BITS_SET(auxvFeatures, J9PORT_HWCAP_S390_MSA))
#endif /* defined(S390) && defined(LINUX)*/
		{
			setFeature(desc, J9PORT_S390_FEATURE_MSA);
		}
	}

	if (testSTFLE(portLibrary, 42) && testSTFLE(portLibrary, 44)) {
#if (defined(S390) && defined(LINUX))
	/* OS Support for Linux on Z */
		if (J9_ARE_ALL_BITS_SET(auxvFeatures, J9PORT_HWCAP_S390_DFP))
#endif /* defined(S390) && defined(LINUX) */
		{
			setFeature(desc, J9PORT_S390_FEATURE_DFP);
		}
	}

	if (testSTFLE(portLibrary, 32)) {
		setFeature(desc, J9PORT_S390_FEATURE_COMPARE_AND_SWAP_AND_STORE);
	}

	if (testSTFLE(portLibrary, 33)) {
		setFeature(desc, J9PORT_S390_FEATURE_COMPARE_AND_SWAP_AND_STORE2);
	}

	if (testSTFLE(portLibrary, 35)) {
		setFeature(desc, J9PORT_S390_FEATURE_EXECUTE_EXTENSIONS);
	}

	if (testSTFLE(portLibrary, 41)) {
		setFeature(desc, J9PORT_S390_FEATURE_FPE);
	}

	if (testSTFLE(portLibrary, 49)) {
		setFeature(desc, J9PORT_S390_FEATURE_MISCELLANEOUS_INSTRUCTION_EXTENSION);
	}

	if (testSTFLE(portLibrary, 76)) {
		setFeature(desc, J9PORT_S390_FEATURE_MSA_EXTENSION3);
	}

	if (testSTFLE(portLibrary, 77)) {
		setFeature(desc, J9PORT_S390_FEATURE_MSA_EXTENSION4);
	}

	if (testSTFLE(portLibrary, J9PORT_S390_FEATURE_MSA_EXTENSION_5)) {
		setFeature(desc, J9PORT_S390_FEATURE_MSA_EXTENSION_5);
	}

	/* Assume an unknown processor ID unless we determine otherwise */
	desc->processor = PROCESSOR_S390_UNKNOWN;

	/* z990 facility and processor detection */

	if (testSTFLE(portLibrary, J9PORT_S390_FEATURE_LONG_DISPLACEMENT)) {
#if (defined(S390) && defined(LINUX))
	/* OS Support for Linux on Z */
		if (J9_ARE_ALL_BITS_SET(auxvFeatures, J9PORT_HWCAP_S390_LDISP))
#endif /* defined(S390) && defined(LINUX) */
		{
			setFeature(desc, J9PORT_S390_FEATURE_LONG_DISPLACEMENT);

			desc->processor = PROCESSOR_S390_GP6;
		}
	}

	/* z9 facility and processor detection */

	if (testSTFLE(portLibrary, J9PORT_S390_FEATURE_EXTENDED_IMMEDIATE)) {
#if (defined(S390) && defined(LINUX))
	/* OS Support for Linux on Z */
		if (J9_ARE_ALL_BITS_SET(auxvFeatures, J9PORT_HWCAP_S390_EIMM))
#endif /* defined(S390) && defined(LINUX) */
		{
			setFeature(desc, J9PORT_S390_FEATURE_EXTENDED_IMMEDIATE);
		}
	}

	if (testSTFLE(portLibrary, J9PORT_S390_FEATURE_EXTENDED_TRANSLATION_3)) {
		setFeature(desc, J9PORT_S390_FEATURE_EXTENDED_TRANSLATION_3);
	}

	if (testSTFLE(portLibrary, J9PORT_S390_FEATURE_ETF3_ENHANCEMENT)) {
#if (defined(S390) && defined(LINUX))
	/* OS Support for Linux on Z */
		if (J9_ARE_ALL_BITS_SET(auxvFeatures, J9PORT_HWCAP_S390_ETF3EH))
#endif /* defined(S390) && defined(LINUX) */
		{
			setFeature(desc, J9PORT_S390_FEATURE_ETF3_ENHANCEMENT);
		}
	}

	if (testSTFLE(portLibrary, J9PORT_S390_FEATURE_EXTENDED_IMMEDIATE) &&
		 testSTFLE(portLibrary, J9PORT_S390_FEATURE_EXTENDED_TRANSLATION_3) &&
		 testSTFLE(portLibrary, J9PORT_S390_FEATURE_ETF3_ENHANCEMENT)) {
		desc->processor = PROCESSOR_S390_GP7;
	}

	/* z10 facility and processor detection */

	if (testSTFLE(portLibrary, J9PORT_S390_FEATURE_GENERAL_INSTRUCTIONS_EXTENSIONS)) {
		setFeature(desc, J9PORT_S390_FEATURE_GENERAL_INSTRUCTIONS_EXTENSIONS);

		desc->processor = PROCESSOR_S390_GP8;
	}

	/* z196 facility and processor detection */

	if (testSTFLE(portLibrary, J9PORT_S390_FEATURE_HIGH_WORD)) {
#if (defined(S390) && defined(LINUX))
		/* OS Support for Linux on Z */
		if (J9_ARE_ALL_BITS_SET(auxvFeatures, J9PORT_HWCAP_S390_HIGH_GPRS))
#endif /* defined(S390) && defined(LINUX)*/
		{
			setFeature(desc, J9PORT_S390_FEATURE_HIGH_WORD);
		}

		desc->processor = PROCESSOR_S390_GP9;
	}

	if (testSTFLE(portLibrary, J9PORT_S390_FEATURE_LOAD_STORE_ON_CONDITION_1)) {
		setFeature(desc, J9PORT_S390_FEATURE_LOAD_STORE_ON_CONDITION_1);

		desc->processor = PROCESSOR_S390_GP9;
	}

	/* zEC12 facility and processor detection */

	/* TE/TX hardware support */
	if (testSTFLE(portLibrary, 50) && testSTFLE(portLibrary, 73)) {
#if defined(J9ZOS390)
		if (getS390zOS_supportsTransactionalExecutionFacility())
#elif defined(LINUX) /* LINUX S390 */
		if (J9_ARE_ALL_BITS_SET(auxvFeatures, J9PORT_HWCAP_S390_TE))
#endif /* defined(J9ZOS390) */
		{
			setFeature(desc, J9PORT_S390_FEATURE_TE);
		}
	}

	/* RI hardware support */
	if (testSTFLE(portLibrary, 64)) {
#if defined(J9ZOS390)
		if (getS390zOS_supportsRuntimeInstrumentationFacility())
#endif /* defined(J9ZOS390) */
		{
#if !defined(J9ZTPF)
			setFeature(desc, J9PORT_S390_FEATURE_RI);
#endif /* !defined(J9ZTPF) */
		}
	}

	if (testSTFLE(portLibrary, J9PORT_S390_FEATURE_MISCELLANEOUS_INSTRUCTION_EXTENSION)) {
		setFeature(desc, J9PORT_S390_FEATURE_MISCELLANEOUS_INSTRUCTION_EXTENSION);

		desc->processor = PROCESSOR_S390_GP10;
	}

	/* z13 facility and processor detection */

	if (testSTFLE(portLibrary, 129)) {
#if defined(J9ZOS390)
		/* Vector facility requires hardware and OS support */
		if (getS390zOS_supportsVectorExtensionFacility())
#elif defined(LINUX) /* LINUX S390 */
		/* Vector facility requires hardware and OS support */
		if (J9_ARE_ALL_BITS_SET(auxvFeatures, J9PORT_HWCAP_S390_VXRS))
#endif
		{
			setFeature(desc, J9PORT_S390_FEATURE_VECTOR_FACILITY);
			desc->processor = PROCESSOR_S390_GP11;
		}
	}

	if (testSTFLE(portLibrary, J9PORT_S390_FEATURE_LOAD_STORE_ON_CONDITION_2)) {
		setFeature(desc, J9PORT_S390_FEATURE_LOAD_STORE_ON_CONDITION_2);

		desc->processor = PROCESSOR_S390_GP11;
	}

	if (testSTFLE(portLibrary, J9PORT_S390_FEATURE_LOAD_AND_ZERO_RIGHTMOST_BYTE)) {
		setFeature(desc, J9PORT_S390_FEATURE_LOAD_AND_ZERO_RIGHTMOST_BYTE);

		desc->processor = PROCESSOR_S390_GP11;
	}

	/* z14 facility and processor detection */

	/* GS hardware support */
	if (testSTFLE(portLibrary, J9PORT_S390_FEATURE_GUARDED_STORAGE)) {
#if defined(J9ZOS390)
		if (getS390zOS_supportsGuardedStorageFacility())
#elif defined(LINUX) /* defined(J9ZOS390) */
		if (J9_ARE_ALL_BITS_SET(auxvFeatures, J9PORT_HWCAP_S390_GS))
#endif /* defined(LINUX) */
		{
			setFeature(desc, J9PORT_S390_FEATURE_GUARDED_STORAGE);

			desc->processor = PROCESSOR_S390_GP12;
		}
	}

	if (testSTFLE(portLibrary, J9PORT_S390_FEATURE_MISCELLANEOUS_INSTRUCTION_EXTENSION_2)) {
		setFeature(desc, J9PORT_S390_FEATURE_MISCELLANEOUS_INSTRUCTION_EXTENSION_2);

		desc->processor = PROCESSOR_S390_GP12;
	}

	if (testSTFLE(portLibrary, J9PORT_S390_FEATURE_SEMAPHORE_ASSIST)) {
		setFeature(desc, J9PORT_S390_FEATURE_SEMAPHORE_ASSIST);

		desc->processor = PROCESSOR_S390_GP12;
	}

	if (testSTFLE(portLibrary, J9PORT_S390_FEATURE_VECTOR_PACKED_DECIMAL)) {
#if defined(J9ZOS390)
		/* Vector packed decimal requires hardware and OS support (for OS, checking for VEF is sufficient) */
		if (getS390zOS_supportsVectorExtensionFacility())
#elif (defined(S390) && defined(LINUX)) /* defined(J9ZOS390) */
	/* OS Support for Linux on Z */
		if (J9_ARE_ALL_BITS_SET(auxvFeatures, J9PORT_HWCAP_S390_VXRS_BCD))
#endif /* defined(S390) && defined(LINUX) */
		{
			setFeature(desc, J9PORT_S390_FEATURE_VECTOR_PACKED_DECIMAL);

			desc->processor = PROCESSOR_S390_GP12;
		}
	}

	if (testSTFLE(portLibrary, J9PORT_S390_FEATURE_VECTOR_FACILITY_ENHANCEMENT_1)) {
#if defined(J9ZOS390)
		/* Vector facility enhancement 1 requires hardware and OS support (for OS, checking for VEF is sufficient) */
		if (getS390zOS_supportsVectorExtensionFacility())
#elif (defined(S390) && defined(LINUX)) /* defined(J9ZOS390) */
	/* OS Support for Linux on Z */
		if (J9_ARE_ALL_BITS_SET(auxvFeatures, J9PORT_HWCAP_S390_VXRS_EXT))
#endif /* defined(S390) && defined(LINUX) */
		{
			setFeature(desc, J9PORT_S390_FEATURE_VECTOR_FACILITY_ENHANCEMENT_1);

			desc->processor = PROCESSOR_S390_GP12;
		}
	}

	if (testSTFLE(portLibrary, J9PORT_S390_FEATURE_MSA_EXTENSION_8)) {
		setFeature(desc, J9PORT_S390_FEATURE_MSA_EXTENSION_8);

		desc->processor = PROCESSOR_S390_GP12;
	}
	
    /* z15 facility and processor detection */

	if (testSTFLE(portLibrary, J9PORT_S390_FEATURE_MISCELLANEOUS_INSTRUCTION_EXTENSION_3)) {
		setFeature(desc, J9PORT_S390_FEATURE_MISCELLANEOUS_INSTRUCTION_EXTENSION_3);

		desc->processor = PROCESSOR_S390_GP13;
	}

	if (testSTFLE(portLibrary, J9PORT_S390_FEATURE_VECTOR_FACILITY_ENHANCEMENT_2)) {
#if defined(J9ZOS390)
		if (getS390zOS_supportsVectorExtensionFacility())
#elif defined(LINUX) && !defined(J9ZTPF) /* defined(J9ZOS390) */
		if (J9_ARE_ALL_BITS_SET(auxvFeatures, J9PORT_HWCAP_S390_VXRS))
#endif /* defined(LINUX) && !defined(J9ZTPF) */
		{
			setFeature(desc, J9PORT_S390_FEATURE_VECTOR_FACILITY_ENHANCEMENT_2);

			desc->processor = PROCESSOR_S390_GP13;
		}
	}

	if (testSTFLE(portLibrary, J9PORT_S390_FEATURE_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY)) {
#if defined(J9ZOS390)
		if (getS390zOS_supportsVectorExtensionFacility())
#elif defined(LINUX) && !defined(J9ZTPF) /* defined(J9ZOS390) */
		if (J9_ARE_ALL_BITS_SET(auxvFeatures, J9PORT_HWCAP_S390_VXRS))
#endif /* defined(LINUX) && !defined(J9ZTPF) */
		{
			setFeature(desc, J9PORT_S390_FEATURE_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY);

			desc->processor = PROCESSOR_S390_GP13;
		}
	}

	/* Set Side Effect Facility without setting GP12. This is because
	 * this GP12-only STFLE bit can also be enabled on zEC12 (GP10)
	 */
	if (testSTFLE(portLibrary, J9PORT_S390_FEATURE_SIDE_EFFECT_ACCESS)) {
		setFeature(desc, J9PORT_S390_FEATURE_SIDE_EFFECT_ACCESS);
	}

	desc->physicalProcessor = desc->processor;

	return 0;
}
#endif /* (defined(S390) || defined(J9ZOS390)) */

#if defined(RISCV64)
/**
 * @internal
 * Populates J9ProcessorDesc *desc on RISC-V
 *
 * @param[in] desc pointer to the struct that will contain the CPU type and features.
 *
 * @return 0 on success, -1 on failure
 */
static intptr_t
getRISCV64Description(struct J9PortLibrary *portLibrary, J9ProcessorDesc *desc)
{
	desc->processor = PROCESOR_RISCV64_UNKNOWN;
	desc->physicalProcessor = desc->processor;
	return 0;
}
#endif /* defined(RISCV64) */

BOOLEAN
j9sysinfo_processor_has_feature(struct J9PortLibrary *portLibrary, J9ProcessorDesc *desc, uint32_t feature)
{
	BOOLEAN rc = FALSE;
	Trc_PRT_sysinfo_processor_has_feature_Entered(desc, feature);

#if defined(J9OS_I5)
#if defined(J9OS_I5_V5R4)
	if ((J9PORT_PPC_FEATURE_HAS_VSX == feature) || (J9PORT_PPC_FEATURE_HAS_ALTIVEC == feature) || (J9PORT_PPC_FEATURE_HTM == feature)) {
		Trc_PRT_sysinfo_processor_has_feature_Exit((UDATA)rc);
		return rc;
	}
#elif defined(J9OS_I5_V6R1) || defined(J9OS_I5_V7R2)
	if (J9PORT_PPC_FEATURE_HTM == feature) {
		Trc_PRT_sysinfo_processor_has_feature_Exit((UDATA)rc);
		return rc;
	}
#endif
#endif

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
openAndReadInfo(struct J9PortLibrary *portLibrary, char* pathBuffer, size_t pathBufferLength, char* fileNameInsertionPoint,
	const char* fileName, char* readBuffer, size_t readBufferLength)
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

#if (defined(J9X86) || defined(J9HAMMER) || defined(RISCV64))

char const *cpuPathPattern = "/sys/devices/system/cpu/cpu%d/cache/";
char const *indexPattern = "index%d/";
#define CPU_PATTERN_SIZE (strlen(cpuPathPattern) + (2 * PATH_ELEMENT_LENGTH) + 1)
/* leave room for terminating null */
#define READ_BUFFER_SIZE 32 /*
 * must be large enough to hold the content of the "size", "coherency_line_size"
 * "level" or "type" files */

/**
 * Scan the /sys filesystem for the correct descriptor and read it.
 * Stop when a descriptor matching the CPU, level, and one of the types in cacheType matches.
 * Note that if if the cache is unified, it will match a query for any type.
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
				if (0 != status){
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
		if (J9PORT_ERROR_FILE_OPFAILED == status){
			/* came to the end of the list of cache entries.  This error is expected. */
			finish = TRUE;
		} else if (status < 0){ /* unexpected error */
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
/*  (defined(J9X86) || defined(J9HAMMER) || defined(RISCV64)) */
#elif  defined(AIXPPC)
static int32_t
getCacheLevels(struct J9PortLibrary *portLibrary,
	const int32_t cpu)
{
	return 2;
}

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
		case J9PORT_CACHEINFO_QUERY_CACHESIZE: {
			if (J9_ARE_ANY_BITS_SET(cacheType, J9PORT_CACHEINFO_DCACHE)) {
				result = getsystemcfg(SC_L1C_DSZ);
			} else if (J9_ARE_ANY_BITS_SET(cacheType, J9PORT_CACHEINFO_ICACHE)) {
				result = getsystemcfg(SC_L1C_ISZ);
			}
		}
		break;
		case J9PORT_CACHEINFO_QUERY_LINESIZE: {
			if (J9_ARE_ANY_BITS_SET(cacheType, J9PORT_CACHEINFO_DCACHE)) {
				result = getsystemcfg(SC_L1C_DLS);
			} else if (J9_ARE_ANY_BITS_SET(cacheType, J9PORT_CACHEINFO_ICACHE)) {
				result = getsystemcfg(SC_L1C_ILS);
			}
		}
		default: break;
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
	default: break;
	}
	return result;
}
#endif /* defined(AIXPPC) */

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
		DIR* cacheDir = opendir("/sys/devices/system/cpu/cpu0/cache");
		if (NULL != cacheDir) {
			closedir(cacheDir);
		} else {
			Trc_PRT_sysinfo_get_cache_info_exit(result);
			return result;
		}
	}
#endif

#if defined(OSX)
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	switch (query->cmd) {
	case J9PORT_CACHEINFO_QUERY_LINESIZE:
		/* ignore the cache type and level, since there is only one line size on MacOS */
		omrcpu_get_cache_line_size(&result);
		break;
	case J9PORT_CACHEINFO_QUERY_CACHESIZE: /* FALLTHROUGH */
	case J9PORT_CACHEINFO_QUERY_TYPES: /* FALLTHROUGH */
	case J9PORT_CACHEINFO_QUERY_LEVELS: /* FALLTHROUGH */
	default:
		result = J9PORT_ERROR_SYSINFO_NOT_SUPPORTED;
		break;
	}
#elif defined(J9OS_I5_V6R1)
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
#elif (defined(J9X86) || defined(J9HAMMER) || defined(AIXPPC) || defined(RISCV64))
	switch (query->cmd) {
	case J9PORT_CACHEINFO_QUERY_LINESIZE:
	case J9PORT_CACHEINFO_QUERY_CACHESIZE:
		result =  getCacheSize(portLibrary, query->cpu, query->level, query->cacheType, query->cmd);
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
#elif  defined (LINUXPPC) || defined (PPC) || defined(S390) || defined(J9ZOS390)
	if ((1 == query->level) &&
		(J9_ARE_ANY_BITS_SET(query->cacheType, J9PORT_CACHEINFO_DCACHE | J9PORT_CACHEINFO_UCACHE))) {
#if defined(S390) || defined(J9ZOS390)
			result = 256;
#else
			OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
			omrcpu_get_cache_line_size(&result);
#endif
		}
#elif defined(LINUX) && defined(J9AARCH64)
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
#endif
	Trc_PRT_sysinfo_get_cache_info_exit(result);
	return result;
}
