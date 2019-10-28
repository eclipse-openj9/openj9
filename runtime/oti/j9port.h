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

#if !defined(OTI_J9PORT_H_)
#define OTI_J9PORT_H_
/* @ddr_namespace: map_to_type=J9PortLibrary */

/* NOTE:  j9port_generated.h include is at the bottom of this file until its dependencies on this file can be relaxed */

/* fix for linux s390 32bit stdint vs unistd.h definition of intptr_t (see CMVC 73850) */
#if defined(LINUX) && defined(S390)
#include <stdint.h>
#endif

#include <stdarg.h>	/* for va_list */
#include "omrcfg.h"
#include "j9comp.h"
#include "omrthread.h"
#include "omrport.h"
#include "j9socket.h"
#include "gp.h"	/* for typedefs of function arguments to gp functions */

#if (defined(LINUX) || defined(RS6000) || defined(SOLARIS) || defined(DECUNIX) || defined(IRIX))
#include <unistd.h>
#endif

#if defined(LINUX) && !defined(J9ZTPF)
#include <pthread.h>
#endif /* defined(LINUX) && !defined(J9ZTPF) */

/**
 * @name Port library access
 * @anchor PortAccess
 * Macros for accessing port library.
 * @{
 */
#define PORTLIB (privatePortLibrary)
#define PORT_ACCESS_FROM_PORT(_portLibrary) J9PortLibrary *privatePortLibrary = (_portLibrary)

/* OMRTODO JAZZ 94696
 * Change OMRPORT_FROM_J9PORT() to ((OMRPortLibrary *)(_j9portLib)) when JAZZ 94696 is complete.
 * This definition makes it easier to catch mixups between OMRPortLibrary* and J9PortLibrary*.
 */
#define OMRPORT_FROM_J9PORT(_j9portLib) (&(_j9portLib)->omrPortLibrary)
#define OMRPORT_ACCESS_FROM_J9PORT(_j9portLib) OMRPORT_ACCESS_FROM_OMRPORT(OMRPORT_FROM_J9PORT(_j9portLib))
/** @} */

#define J9PORT_MAJOR_VERSION_NUMBER 89
#define J9PORT_MINOR_VERSION_NUMBER 0

#define J9PORT_CAPABILITY_BASE 0
#define J9PORT_CAPABILITY_STANDARD 1
#define J9PORT_CAPABILITY_CAN_RESERVE_SPECIFIC_ADDRESS 2
#define J9PORT_CAPABILITY_ALLOCATE_TOP_DOWN 4

#if defined(WIN32)
#define J9PORT_LIBRARY_SUFFIX ".dll"
#elif defined(OSX)
#define J9PORT_LIBRARY_SUFFIX ".dylib"
#elif defined(LINUX) || defined(AIXPPC) || defined(J9ZOS390)
#define J9PORT_LIBRARY_SUFFIX ".so"
#else
#error "J9PORT_LIBRARY_SUFFIX must be defined"
#endif


/**
 * @name Shared Semaphore Success flags
 * @anchor PortSharedSemaphoreSuccessFlags
 * Success codes related to shared semaphore  operations.
 * @{
 * @internal J9PORT_INFO_SHSEM* range from at 100 to 109 to avoid overlap 
 */
#define J9PORT_INFO_SHSEM_BASE 100
#define J9PORT_INFO_SHSEM_CREATED (J9PORT_INFO_SHSEM_BASE)
#define J9PORT_INFO_SHSEM_OPENED (J9PORT_INFO_SHSEM_BASE+1)
#define J9PORT_INFO_SHSEM_OPEN_UNLINKED (J9PORT_INFO_SHSEM_BASE+2)
#define J9PORT_INFO_SHSEM_OPENED_STALE (J9PORT_INFO_SHSEM_BASE+3)
#define J9PORT_INFO_SHSEM_PARTIAL (J9PORT_INFO_SHSEM_BASE+4)
#define J9PORT_INFO_SHSEM_STAT_PASSED (J9PORT_INFO_SHSEM_BASE+5)

/** @} */

/**
 * @name Shared Memory Success flags
 * @anchor PortSharedMemorySuccessFlags
 * Success codes related to shared memory semaphore operations.
 * @{
 * @internal J9PORT_INFO_SHMEM* range from at 110 to 119 to avoid overlap
 */
#define J9PORT_INFO_SHMEM_BASE 110
#define J9PORT_INFO_SHMEM_CREATED (J9PORT_INFO_SHMEM_BASE)
#define J9PORT_INFO_SHMEM_OPENED (J9PORT_INFO_SHMEM_BASE+1)
#define J9PORT_INFO_SHMEM_OPEN_UNLINKED (J9PORT_INFO_SHMEM_BASE+2)
#define J9PORT_INFO_SHMEM_OPENED_STALE (J9PORT_INFO_SHMEM_BASE+3)
#define J9PORT_INFO_SHMEM_PARTIAL (J9PORT_INFO_SHMEM_BASE+4)
#define J9PORT_INFO_SHMEM_STAT_PASSED (J9PORT_INFO_SHMEM_BASE+5)

/** @} */

/**
 * @name Shared Memory Eyecatcher
 * @anchor PortSharedMemoryEyecatcher
 * Eyecatcher written to start of a shared classes cache to identify the shared memory segment as such a cache
 * @{
 */
#define J9PORT_SHMEM_EYECATCHER "J9SC"
#define J9PORT_SHMEM_EYECATCHER_LENGTH 4
/** @} */

/**
 * @name Control file unlink status
 * Flags used to indicate unlink status of control files used by semaphore set or shared memory
 * These flags are used to store value in J9ControlFileStatus.status
 * @{
 */
#define J9PORT_INFO_CONTROL_FILE_NOT_UNLINKED			0
#define J9PORT_INFO_CONTROL_FILE_UNLINK_FAILED			1
#define J9PORT_INFO_CONTROL_FILE_UNLINKED				2
/** @} */

/**
 * @name OS Exception Handling
 * OS Exceptions
 * @{
 */
#define MAX_SIZE_TOTAL_GPINFO 2048
#define J9GP_VALUE_UNDEFINED 0
#define J9GP_VALUE_32 1 
#define J9GP_VALUE_64 2
#define J9GP_VALUE_STRING 3
#define J9GP_VALUE_ADDRESS 4
#define J9GP_VALUE_FLOAT_64 5
#define J9GP_VALUE_16 6

#define J9GP_SIGNAL 0 	/* information about the signal */
#define J9GP_GPR 1 /* general purpose registers */
#define J9GP_OTHER 2  /* other information */
#define J9GP_CONTROL 3 	/* control registers */
#define J9GP_FPR 4 		/* floating point registers */
#define J9GP_MODULE 5 	/* module information */	
#define J9GP_NUM_CATEGORIES 6

#define J9GP_CONTROL_PC (-1)
#define J9GP_MODULE_NAME (-1)
/** @} */

/**
 * @name Shared Semaphore
 * Flags used to indicate type of operation for j9shsem_post/j9shsem_wait
 * @{
 */
#define J9PORT_SHSEM_MODE_DEFAULT ((uintptr_t) 0)
#define J9PORT_SHSEM_MODE_UNDO ((uintptr_t) 1)
#define J9PORT_SHSEM_MODE_NOWAIT ((uintptr_t) 2)
/** @} */

/* The following defines are used by j9shmem and j9shsem */
#define J9SH_MAXPATH EsMaxPath

#define J9SH_MEMORY_ID "_memory_"
#define J9SH_SEMAPHORE_ID "_semaphore_"

#define J9SH_DIRPERM_ABSENT ((uintptr_t)-2)
#define J9SH_DIRPERM_ABSENT_GROUPACCESS ((uintptr_t)-3)

#define J9SH_DIRPERM (0700)
#define J9SH_DIRPERM_GROUPACCESS (0770)
#define J9SH_DIRPERM_DEFAULT_TMP (0777)

#define J9SH_PARENTDIRPERM (01777)
#define J9SH_DIRPERM_DEFAULT (0000)
#define J9SH_DIRPERM_DEFAULT_WITH_STICKYBIT (01000)
#define J9SH_BASEFILEPERM (0644)
#define J9SH_BASEFILEPERM_GROUP_RW_ACCESS (0664)

#define J9SH_SHMEM_PERM_READ (0444)
#define J9SH_SHMEM_PERM_READ_WRITE (0644)

#define J9SH_SYSV_REGULAR_CONTROL_FILE 0
#define J9SH_SYSV_OLDER_CONTROL_FILE 1
#define J9SH_SYSV_OLDER_EMPTY_CONTROL_FILE 2

/* 
 * Flags passed to "flag" argument of j9shmem_open(). Should be of type uintptr_t.
 * High order 4 bits are reserved for passing the storage key testing value to j9shmem.
 */
#define J9SHMEM_NO_FLAGS					0x0
#define J9SHMEM_OPEN_FOR_STATS				0x1
#define J9SHMEM_OPEN_FOR_DESTROY			0x2
#define J9SHMEM_PRINT_STORAGE_KEY_WARNING	0x4
#define J9SHMEM_STORAGE_KEY_TESTING			0x8
#define J9SHMEM_OPEN_DO_NOT_CREATE			0x10

#define J9SHMEM_STORAGE_KEY_TESTING_SHIFT	((sizeof(uintptr_t)*8)-4)
#define J9SHMEM_STORAGE_KEY_TESTING_MASK	0xF



/* Flags passed to "flag" argument of j9shsem_deprecated_open(). */
#define J9SHSEM_NO_FLAGS			0x0
#define J9SHSEM_OPEN_FOR_STATS		0x1
#define J9SHSEM_OPEN_FOR_DESTROY	0x2
#define J9SHSEM_OPEN_DO_NOT_CREATE	0x4

/* Flags passed to "flags" argument of j9shmem_getDir(). */
#define J9SHMEM_GETDIR_APPEND_BASEDIR		0x1
#define J9SHMEM_GETDIR_USE_USERHOME			0x2

/* Maximum id we should try when we do ftok */
#define J9SH_MAX_PROJ_ID 20 

#ifdef WIN32
#define J9SH_BASEDIR "javasharedresources\\"
#else
#define J9SH_BASEDIR "javasharedresources/"
#endif

/**
 * Stores information about status of control file used by j9shmem_open() or j9shsem_deprecated_open().
 */
typedef struct J9ControlFileStatus {
	uintptr_t status;
	int32_t errorCode;
	char *errorMsg;
} J9ControlFileStatus;

/**
 * @name J9PortShSemParameters
 * The caller is responsible creating storage for J9PortShSemParameters. 
 * The structure is only needed for the lifetime of the call to @ref j9shsem_open
 * This structure must be initialized using @ref j9shsem_params_init
 */
typedef struct  J9PortShSemParameters {
 	const char *semName; /* Unique identifier of the semaphore. */
 	uint32_t setSize; /* number of semaphores to be created in this set */
 	uint32_t permission; /* Posix-style file permissions */
 	const char* controlFileDir; /* Directory in which to create control files (SysV semaphores only) */
 	uint8_t proj_id; /* parameter used with semName to generate semaphore key */
 	uint32_t deleteBasefile : 1; /* delete the base file (used to generate the semaphore key) when destroying the semaphore */
 	uint32_t global : 1; /* Windows only: use the global namespace for the sempahore */
} J9PortShSemParameters;
/**
 * @name Process Handle
 * J9ProcessHandle is a pointer to the opaque structure J9ProcessHandleStruct.
 * 
 * J9ProcessHandle represents a J9Port Library process.
 */
typedef struct J9ProcessHandleStruct *J9ProcessHandle;


/**
 * @name Process Streams
 * Flags used to define the streams of a process.
 */
#define J9PORT_PROCESS_STDIN ((uintptr_t) 0x00000001)
#define J9PORT_PROCESS_STDOUT ((uintptr_t) 0x00000002)
#define J9PORT_PROCESS_STDERR ((uintptr_t) 0x00000004)

#define J9PORT_PROCESS_IGNORE_OUTPUT 				1
#define J9PORT_PROCESS_NONBLOCKING_IO 				2
#define J9PORT_PROCESS_PIPE_TO_PARENT 				4
#define J9PORT_PROCESS_INHERIT_STDIN 				8
#define J9PORT_PROCESS_INHERIT_STDOUT				16
#define J9PORT_PROCESS_INHERIT_STDERR 				32
#define J9PORT_PROCESS_REDIRECT_STDERR_TO_STDOUT	64
#define J9PORT_PROCESS_CREATE_NEW_PROCESS_GROUP		128

#define J9PORT_PROCESS_DO_NOT_CLOSE_STREAMS			1

#define J9PORT_INVALID_FD	-1

#if defined(LINUX) && !defined(J9ZTPF)
typedef pthread_spinlock_t spinlock_t;
#else /* defined(LINUX) && !defined(J9ZTPF) */
typedef int32_t spinlock_t;
#endif /* defined(LINUX) && !defined(J9ZTPF) */

/* Structure to hold the Hypervisor vendor details.
 * Currently we only provide the name of the vendor
 */
typedef struct J9HypervisorVendorDetails {
	const char* hypervisorName;
} J9HypervisorVendorDetails;

/* Currently Supported Hypervisor Vendor Names */
#define HYPE_NAME_HYPERV	"Hyper-V"
#define HYPE_NAME_KVM		"KVM"
#define HYPE_NAME_POWERVM	"PowerVM"
#define HYPE_NAME_PRSM		"PR/SM"
#define HYPE_NAME_VMWARE	"VMWare"
#define HYPE_NAME_ZVM		"z/VM"
#define HYPE_NAME_POWERKVM	"PowerKVM"

/* Structure to hold the Processor usage details of the Guest OS
 * as seen by the hypervisor
 */
typedef struct J9GuestProcessorUsage {
	/* The time during which the virtual machine has used the CPU, in microseconds */
	int64_t cpuTime;
	/* The time stamp of the system, in microseconds */
	int64_t timestamp;
	/* cpu entitlement for this VM */
	SYS_FLOAT cpuEntitlement;
	/* host CPU speed (in platform-dependent units) */
	int64_t hostCpuClockSpeed;
} J9GuestProcessorUsage;

/* Structure to hold the Memory usage details of the Guest OS
 * as seen by the hypervisor
 */
typedef struct J9GuestMemoryUsage {
	/* Current memory used by the Guest OS in MB */
	int64_t memUsed;
	/* The time stamp of the system, in microseconds */
	int64_t timestamp;
	/* Maximum memory that has been allocated to the guest OS in MB */
	int64_t maxMemLimit;
} J9GuestMemoryUsage;

/* j9sysinfo_get_processor_description
 */
typedef enum J9ProcessorArchitecture {

	PROCESSOR_UNDEFINED,

	PROCESSOR_S390_UNKNOWN,
	PROCESSOR_S390_GP6,
	PROCESSOR_S390_GP7,
	PROCESSOR_S390_GP8,
	PROCESSOR_S390_GP9,
	PROCESSOR_S390_GP10,
	PROCESSOR_S390_GP11,
	PROCESSOR_S390_GP12,
	PROCESSOR_S390_GP13,
	PROCESSOR_S390_GP14,

	PROCESSOR_PPC_UNKNOWN,
	PROCESSOR_PPC_7XX,
	PROCESSOR_PPC_GP,
	PROCESSOR_PPC_GR,
	PROCESSOR_PPC_NSTAR,
	PROCESSOR_PPC_PULSAR,
	PROCESSOR_PPC_PWR403,
	PROCESSOR_PPC_PWR405,
	PROCESSOR_PPC_PWR440,
	PROCESSOR_PPC_PWR601,
	PROCESSOR_PPC_PWR602,
	PROCESSOR_PPC_PWR603,
	PROCESSOR_PPC_PWR604,
	PROCESSOR_PPC_PWR620,
	PROCESSOR_PPC_PWR630,
	PROCESSOR_PPC_RIOS1,
	PROCESSOR_PPC_RIOS2,
	PROCESSOR_PPC_P6,
	PROCESSOR_PPC_P7,
	PROCESSOR_PPC_P8,
	PROCESSOR_PPC_P9,

	PROCESSOR_X86_UNKNOWN,
	PROCESSOR_X86_INTELPENTIUM,
	PROCESSOR_X86_INTELP6,
	PROCESSOR_X86_INTELPENTIUM4,
	PROCESSOR_X86_INTELCORE2,
	PROCESSOR_X86_INTELTULSA,
	PROCESSOR_X86_INTELNEHALEM,
	PROCESSOR_X86_INTELWESTMERE,
	PROCESSOR_X86_INTELSANDYBRIDGE,
	PROCESSOR_X86_INTELHASWELL,
	PROCESSOR_X86_AMDK5,
	PROCESSOR_X86_AMDK6,
	PROCESSOR_X86_AMDATHLONDURON,
	PROCESSOR_X86_AMDOPTERON,

	PROCESSOR_DUMMY = 0x40000000 /* force wide enums */

} J9ProcessorArchitecture;

/* Holds processor type and features used with j9sysinfo_get_processor_description
 * and j9sysinfo_processor_has_feature
 */
#define J9PORT_SYSINFO_FEATURES_SIZE 5
typedef struct J9ProcessorDesc {
	J9ProcessorArchitecture processor;
	J9ProcessorArchitecture physicalProcessor;
	uint32_t features[J9PORT_SYSINFO_FEATURES_SIZE];
} J9ProcessorDesc;

/* PowerPC features
 * Auxiliary Vector Hardware Capability (AT_HWCAP) features for PowerPC.
 */
#define J9PORT_PPC_FEATURE_32                    31 /* 32-bit mode.  */
#define J9PORT_PPC_FEATURE_64                    30 /* 64-bit mode.  */
#define J9PORT_PPC_FEATURE_601_INSTR             29 /* 601 chip, Old POWER ISA.  */
#define J9PORT_PPC_FEATURE_HAS_ALTIVEC           28 /* SIMD/Vector Unit.  */
#define J9PORT_PPC_FEATURE_HAS_FPU               27 /* Floating Point Unit.  */
#define J9PORT_PPC_FEATURE_HAS_MMU               26 /* Memory Management Unit.  */
#define J9PORT_PPC_FEATURE_HAS_4xxMAC            25 /* 4xx Multiply Accumulator.  */
#define J9PORT_PPC_FEATURE_UNIFIED_CACHE         24 /* Unified I/D cache.  */
#define J9PORT_PPC_FEATURE_HAS_SPE               23 /* Signal Processing ext.  */
#define J9PORT_PPC_FEATURE_HAS_EFP_SINGLE        22 /* SPE Float.  */
#define J9PORT_PPC_FEATURE_HAS_EFP_DOUBLE        21 /* SPE Double.  */
#define J9PORT_PPC_FEATURE_NO_TB                 20 /* 601/403gx have no timebase.  */
#define J9PORT_PPC_FEATURE_POWER4                19 /* POWER4 ISA 2.01.  */
#define J9PORT_PPC_FEATURE_POWER5                18 /* POWER5 ISA 2.02.  */
#define J9PORT_PPC_FEATURE_POWER5_PLUS           17 /* POWER5+ ISA 2.03.  */
#define J9PORT_PPC_FEATURE_CELL_BE               16 /* CELL Broadband Engine */
#define J9PORT_PPC_FEATURE_BOOKE                 15 /* ISA Embedded Category.  */
#define J9PORT_PPC_FEATURE_SMT                   14 /* Simultaneous Multi-Threading.  */
#define J9PORT_PPC_FEATURE_ICACHE_SNOOP          13
#define J9PORT_PPC_FEATURE_ARCH_2_05             12 /* ISA 2.05.  */
#define J9PORT_PPC_FEATURE_PA6T                  11 /* PA Semi 6T Core.  */
#define J9PORT_PPC_FEATURE_HAS_DFP               10 /* Decimal FP Unit.  */
#define J9PORT_PPC_FEATURE_POWER6_EXT             9 /* P6 + mffgpr/mftgpr.  */
#define J9PORT_PPC_FEATURE_ARCH_2_06              8 /* ISA 2.06.  */
#define J9PORT_PPC_FEATURE_HAS_VSX                7 /* P7 Vector Scalar Extension.  */
#define J9PORT_PPC_FEATURE_PSERIES_PERFMON_COMPAT 6 /* Has ISA >= 2.05 PMU basic subset support.  */
#define J9PORT_PPC_FEATURE_TRUE_LE                1 /* Processor in true Little Endian mode.  */
#define J9PORT_PPC_FEATURE_PPC_LE                 0 /* Processor emulates Little Endian Mode.  */

#define J9PORT_PPC_FEATURE_ARCH_2_07             32 + 31
#define J9PORT_PPC_FEATURE_HTM                   32 + 30
#define J9PORT_PPC_FEATURE_DSCR                  32 + 29
#define J9PORT_PPC_FEATURE_EBB                   32 + 28
#define J9PORT_PPC_FEATURE_ISEL                  32 + 27
#define J9PORT_PPC_FEATURE_TAR                   32 + 26

/* s390 features
 * z/Architecture Principles of Operation 4-69
 * STORE FACILITY LIST EXTENDED (STFLE)
 */
#define J9PORT_S390_FEATURE_ESAN3      0 /* STFLE bit 0 */
#define J9PORT_S390_FEATURE_ZARCH      1 /* STFLE bit 2 */
#define J9PORT_S390_FEATURE_STFLE      2 /* STFLE bit 7 */
#define J9PORT_S390_FEATURE_MSA        3 /* STFLE bit 17 */
#define J9PORT_S390_FEATURE_DFP        6 /* STFLE bit 42 & 44 */
#define J9PORT_S390_FEATURE_HPAGE      7
#define J9PORT_S390_FEATURE_TE        10 /* STFLE bit 50 & 73 */
#define J9PORT_S390_FEATURE_MSA_EXTENSION3                      11 /* STFLE bit 76 */
#define J9PORT_S390_FEATURE_MSA_EXTENSION4                      12 /* STFLE bit 77 */

#define J9PORT_S390_FEATURE_COMPARE_AND_SWAP_AND_STORE          32 + 0  /* STFLE bit 32 */
#define J9PORT_S390_FEATURE_COMPARE_AND_SWAP_AND_STORE2         32 + 1  /* STFLE bit 33 */
#define J9PORT_S390_FEATURE_EXECUTE_EXTENSIONS                  32 + 3  /* STFLE bit 35 */
#define J9PORT_S390_FEATURE_FPE                                 32 + 9  /* STFLE bit 41 */

#define J9PORT_S390_FEATURE_RI            64 + 0 /* STFLE bit 64 */

/* z990 facilities */

/* STFLE bit 19 - Long-displacement facility */
#define J9PORT_S390_FEATURE_LONG_DISPLACEMENT 19

/* z9 facilities */

/* STFLE bit 21 - Extended-immediate facility */
#define J9PORT_S390_FEATURE_EXTENDED_IMMEDIATE 21

/* STFLE bit 22 - Extended-translation facility 3 */
#define J9PORT_S390_FEATURE_EXTENDED_TRANSLATION_3 22

/* STFLE bit 30 - ETF3-enhancement facility */
#define J9PORT_S390_FEATURE_ETF3_ENHANCEMENT 30

/* z10 facilities */

/* STFLE bit 34 - General-instructions-extension facility */
#define J9PORT_S390_FEATURE_GENERAL_INSTRUCTIONS_EXTENSIONS 34

/* z196 facilities */

/* STFLE bit 45 - High-word facility */
#define J9PORT_S390_FEATURE_HIGH_WORD 45

/* STFLE bit 45 - Load/store-on-condition facility 1 */
#define J9PORT_S390_FEATURE_LOAD_STORE_ON_CONDITION_1 45

/* zEC12 facilities */

/* STFLE bit 49 - Miscellaneous-instruction-extension facility */
#define J9PORT_S390_FEATURE_MISCELLANEOUS_INSTRUCTION_EXTENSION 49

/* z13 facilities */

/* STFLE bit 53 - Load/store-on-condition facility 2 */
#define J9PORT_S390_FEATURE_LOAD_STORE_ON_CONDITION_2 53

/* STFLE bit 53 - Load-and-zero-rightmost-byte facility */
#define J9PORT_S390_FEATURE_LOAD_AND_ZERO_RIGHTMOST_BYTE 53

/* STFLE bit 129 - Vector facility */
#define J9PORT_S390_FEATURE_VECTOR_FACILITY 129

/* z14 facilities */

/* STFLE bit 58 - Miscellaneous-instruction-extensions facility 2 */
#define J9PORT_S390_FEATURE_MISCELLANEOUS_INSTRUCTION_EXTENSION_2 58

/* STFLE bit 59 - Semaphore-assist facility */
#define J9PORT_S390_FEATURE_SEMAPHORE_ASSIST 59

/* STFLE bit 131 - Side-effect-access facility */
#define J9PORT_S390_FEATURE_SIDE_EFFECT_ACCESS 131

/* STFLE bit 133 - Guarded-storage facility */
#define J9PORT_S390_FEATURE_GUARDED_STORAGE 133

/* STFLE bit 134 - Vector packed decimal facility */
#define J9PORT_S390_FEATURE_VECTOR_PACKED_DECIMAL 134

/* STFLE bit 135 - Vector enhancements facility 1 */
#define J9PORT_S390_FEATURE_VECTOR_FACILITY_ENHANCEMENT_1 135

/* STFLE bit 146 - Message-security-assist-extension-8 facility */
#define J9PORT_S390_FEATURE_MSA_EXTENSION_8 146

/* STFLE bit 57 - Message-security-assist-extension-5 facility */
#define J9PORT_S390_FEATURE_MSA_EXTENSION_5 57

/* z15 facilities */

/* STFLE bit 61 - Miscellaneous-instruction-extensions facility 3 */ 
#define J9PORT_S390_FEATURE_MISCELLANEOUS_INSTRUCTION_EXTENSION_3 61

/* STFLE bit 148 - Vector enhancements facility 2 */
#define J9PORT_S390_FEATURE_VECTOR_FACILITY_ENHANCEMENT_2 148

/* STFLE bit 152 - Vector packed decimal enhancement facility */
#define J9PORT_S390_FEATURE_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY 152


/*  Linux on Z features
 *  Auxiliary Vector Hardware Capability (AT_HWCAP) features for Linux on Z.
 *  Obtained from: https://github.com/torvalds/linux/blob/050cdc6c9501abcd64720b8cc3e7941efee9547d/arch/s390/include/asm/elf.h#L94-L109.
 *  If new facility support is required, then it must be defined there (and here), before we can check for it consistently.
 *
 *  The linux kernel will use the defines in the above link to set HWCAP features. This is done inside "setup_hwcaps(void)" routine found
 *  in arch/s390/kernel/setup.c in the linux kernel source tree.
 */
#define J9PORT_HWCAP_S390_ESAN3     0x1
#define J9PORT_HWCAP_S390_ZARCH     0x2
#define J9PORT_HWCAP_S390_STFLE     0x4
#define J9PORT_HWCAP_S390_MSA       0x8
#define J9PORT_HWCAP_S390_LDISP     0x10
#define J9PORT_HWCAP_S390_EIMM      0x20
#define J9PORT_HWCAP_S390_DFP       0x40
#define J9PORT_HWCAP_S390_HPAGE     0x80
#define J9PORT_HWCAP_S390_ETF3EH    0x100
#define J9PORT_HWCAP_S390_HIGH_GPRS 0x200
#define J9PORT_HWCAP_S390_TE        0x400
#define J9PORT_HWCAP_S390_VXRS      0x800
#define J9PORT_HWCAP_S390_VXRS_BCD  0x1000
#define J9PORT_HWCAP_S390_VXRS_EXT  0x2000
#define J9PORT_HWCAP_S390_GS        0x4000

/* x86 features
 * INTEL INSTRUCTION SET REFERENCE, A-M
 * 3-170 Vol. 2A Table 3-21. More on Feature Information Returned in the EDX Register
 */
#define J9PORT_X86_FEATURE_FPU     0 /* Floating Point Unit On-Chip. */
#define J9PORT_X86_FEATURE_VME     1 /* Virtual 8086 Mode Enhancements. */
#define J9PORT_X86_FEATURE_DE      2 /* DE Debugging Extensions. */
#define J9PORT_X86_FEATURE_PSE     3 /* Page Size Extension. */
#define J9PORT_X86_FEATURE_TSC     4 /* Time Stamp Counter. */
#define J9PORT_X86_FEATURE_MSR     5 /* Model Specific Registers RDMSR and WRMSR Instructions. */
#define J9PORT_X86_FEATURE_PAE     6 /* Physical Address Extension. */
#define J9PORT_X86_FEATURE_MCE     7 /* Machine Check Exception. */
#define J9PORT_X86_FEATURE_CX8     8 /* Compare-and-exchange 8 bytes (64 bits) instruction */
#define J9PORT_X86_FEATURE_APIC    9 /* APIC On-Chip. */
#define J9PORT_X86_FEATURE_10     10 /* Reserved */
#define J9PORT_X86_FEATURE_SEP    11 /* SYSENTER and SYSEXIT Instructions. */
#define J9PORT_X86_FEATURE_MTRR   12 /* Memory Type Range Registers. */
#define J9PORT_X86_FEATURE_PGE    13 /* Page Global Bit. */
#define J9PORT_X86_FEATURE_MCA    14 /* Machine Check Architecture. */
#define J9PORT_X86_FEATURE_CMOV   15 /* Conditional Move Instructions. */
#define J9PORT_X86_FEATURE_PAT    16 /* Page Attribute Table. */
#define J9PORT_X86_FEATURE_PSE_36 17 /* 36-Bit Page Size Extension. */
#define J9PORT_X86_FEATURE_PSN    18 /* Processor Serial Number. */
#define J9PORT_X86_FEATURE_CLFSH  19 /* CLFLUSH Instruction. */
#define J9PORT_X86_FEATURE_20     20 /* Reserved */
#define J9PORT_X86_FEATURE_DS     21 /* Debug Store. */
#define J9PORT_X86_FEATURE_ACPI   22 /* Thermal Monitor and Software Controlled Clock Facilities. */
#define J9PORT_X86_FEATURE_MMX    23 /* Intel MMX Technology. */
#define J9PORT_X86_FEATURE_FXSR   24 /* FXSAVE and FXRSTOR Instructions. */
#define J9PORT_X86_FEATURE_SSE    25 /* The processor supports the SSE extensions. */
#define J9PORT_X86_FEATURE_SSE2   26 /* The processor supports the SSE2 extensions. */
#define J9PORT_X86_FEATURE_SS     27 /* Self Snoop. */
#define J9PORT_X86_FEATURE_HTT    28 /* Hyper Threading. */
#define J9PORT_X86_FEATURE_TM     29 /* Thermal Monitor. */
#define J9PORT_X86_FEATURE_30     30 /* Reserved */
#define J9PORT_X86_FEATURE_PBE    31 /* Pending Break Enable. */

/* INTEL INSTRUCTION SET REFERENCE, A-M
 * Vol. 2A 3-167 Table 3-20. Feature Information Returned in the ECX Register
 */
#define J9PORT_X86_FEATURE_SSE3         32 + 0 /* Streaming SIMD Extensions 3 */
#define J9PORT_X86_FEATURE_PCLMULQDQ    32 + 1 /* PCLMULQDQ. */
#define J9PORT_X86_FEATURE_DTES64       32 + 2 /* 64-bit DS Area. */
#define J9PORT_X86_FEATURE_MONITOR      32 + 3 /* MONITOR/MWAIT. */
#define J9PORT_X86_FEATURE_DS_CPL       32 + 4 /* CPL Qualified Debug Store. */
#define J9PORT_X86_FEATURE_VMX          32 + 5 /* Virtual Machine Extensions. */
#define J9PORT_X86_FEATURE_SMX          32 + 6 /* Safer Mode Extensions. */
#define J9PORT_X86_FEATURE_EIST         32 + 7 /* Enhanced Intel SpeedStep technology. */
#define J9PORT_X86_FEATURE_TM2          32 + 8 /* Thermal Monitor 2. */
#define J9PORT_X86_FEATURE_SSSE3        32 + 9 /* Supplemental Streaming SIMD Extensions 3 */
#define J9PORT_X86_FEATURE_CNXT_ID      32 + 10 /* L1 Context ID. */
#define J9PORT_X86_FEATURE_11           32 + 11 /* Reserved */
#define J9PORT_X86_FEATURE_FMA          32 + 12 /* FMA extensions using YMM state. */
#define J9PORT_X86_FEATURE_CMPXCHG16B   32 + 13 /* CMPXCHG16B Available. */
#define J9PORT_X86_FEATURE_XTPR         32 + 14 /* xTPR Update Control. */
#define J9PORT_X86_FEATURE_PDCM         32 + 15 /* Perfmon and Debug Capability. */
#define J9PORT_X86_FEATURE_16           32 + 16 /* Reserved. */
#define J9PORT_X86_FEATURE_PCID         32 + 17 /* Process-context identifiers. */
#define J9PORT_X86_FEATURE_DCA          32 + 18 /* Processor supports the ability to prefetch data from a memory mapped device. */
#define J9PORT_X86_FEATURE_SSE4_1       32 + 19 /* Processor supports SSE4.1. */
#define J9PORT_X86_FEATURE_SSE4_2       32 + 20 /* Processor supports SSE4.2. */
#define J9PORT_X86_FEATURE_X2APIC       32 + 21 /* Processor supports x2APIC feature. */
#define J9PORT_X86_FEATURE_MOVBE        32 + 22 /* Processor supports MOVBE instruction. */
#define J9PORT_X86_FEATURE_POPCNT       32 + 23 /* Processor supports the POPCNT instruction. */
#define J9PORT_X86_FEATURE_TSC_DEADLINE 32 + 24 /* Processor's local APIC timer supports one-shot operation using a TSC deadline value. */
#define J9PORT_X86_FEATURE_AESNI        32 + 25 /* Processor supports the AESNI instruction extensions. */
#define J9PORT_X86_FEATURE_XSAVE        32 + 26 /* Processor supports the XSAVE/XRSTOR processor extended states. */
#define J9PORT_X86_FEATURE_OSXSAVE      32 + 27 /* OS has enabled XSETBV/XGETBV instructions to access XCR0, and support for processor extended state management using XSAVE/XRSTOR. */
#define J9PORT_X86_FEATURE_AVX          32 + 28 /* Processor supports the AVX instruction extensions. */
#define J9PORT_X86_FEATURE_F16C         32 + 29 /* 16-bit floating-point conversion instructions. */
#define J9PORT_X86_FEATURE_RDRAND       32 + 30 /* Processor supports RDRAND instruction. */


/* INTEL INSTRUCTION SET REFERENCE, A-L May 2019
 * Vol. 2 3-197 Table 3-8. Structured Feature Information Returned in the EBX Register by CPUID instruction
 */
#define J9PORT_X86_FEATURE_FSGSBASE 		96 + 0	/* fsgsbase instructions support */
#define J9PORT_X86_FEATURE_IA32_TSC_ADJUST	96 + 1	/* IA32_TSC_ADJUST MSR support */
#define J9PORT_X86_FEATURE_SGX				96 + 2	/* Intel Software Guard Extensions */
#define J9PORT_X86_FEATURE_BMI1				96 + 3	/* Bit Manipulation Instructions 1 */
#define J9PORT_X86_FEATURE_HLE				96 + 4	/* Hardware Lock Elison */
#define J9PORT_X86_FEATURE_AVX2				96 + 5	/* AVX2 support */
#define J9PORT_X86_FEATURE_FDP_EXCPTN_ONLY	96 + 6	/* x87 FPU data pointer updated only on exceptions */
#define J9PORT_X86_FEATURE_SMEP				96 + 7	/* Supervsior-Mode Execution Prevention */
#define J9PORT_X86_FEATURE_BMI2				96 + 8	/* Bit Manipulation Instructions 2 */
#define J9PORT_X86_FEATURE_ERMSB			96 + 9	/* Enhanced REP MOVSB/STOSB */
#define J9PORT_X86_FEATURE_INVPCID			96 + 10	/* Invalidate Process-Context Identifier instruction */
#define J9PORT_X86_FEATURE_RTM				96 + 11	/* Restricted Transactional Memory */
#define J9PORT_X86_FEATURE_RDT_M			96 + 12	/* Intel RDT Monitoring */
#define J9PORT_X86_FEATURE_DEPRECATE_FPUCS	96 + 13	/* Deprecates FPU CS and FPU DS when set */
#define J9PORT_X86_FEATURE_MPX				96 + 14	/* Intel Memory Protextion Extensions */
#define J9PORT_X86_FEATURE_RDT_A			96 + 15	/* Intel RDT Allocation */
#define J9PORT_X86_FEATURE_AVX512F			96 + 16	/* AVX512 Foundation */
#define J9PORT_X86_FEATURE_AVX512DQ			96 + 17	/* AVX512 Doubleword & Quadword */
#define J9PORT_X86_FEATURE_RDSEED			96 + 18	/* RDSEED instruction support */
#define J9PORT_X86_FEATURE_ADX				96 + 19	/* Intel ADX (multi-precision arithmetic) */
#define J9PORT_X86_FEATURE_SMAP				96 + 20	/* Supervisor-Mode Access Prevention */
#define J9PORT_X86_FEATURE_AVX512_IFMA		96 + 21	/* AVX512 Integer Fused Multiply Add */
#define J9PORT_X86_FEATURE_22				96 + 22	/* reserved */
#define J9PORT_X86_FEATURE_CLFLUSHOPT		96 + 23	/* cache flush optimized */
#define J9PORT_X86_FEATURE_CLWB				96 + 24	/* cache line write back */
#define J9PORT_X86_FEATURE_IPT				96 + 25	/* Intel Processor Trace */
#define J9PORT_X86_FEATURE_AVX512PF			96 + 26	/* AVX512 Prefetch */
#define J9PORT_X86_FEATURE_AVX512ER			96 + 27	/* AVX512 Exponential and Reciprocal */
#define J9PORT_X86_FEATURE_AVX512CD			96 + 28	/* AVX512 Conflict Detection */
#define J9PORT_X86_FEATURE_SHA				96 + 29	/* Intel SHA Extensions */
#define J9PORT_X86_FEATURE_AVX512BW			96 + 30	/* AVX512 Byte and Word */
#define J9PORT_X86_FEATURE_AVX512VL			96 + 31	/* AVX512 Vector Length */

/* cache types */
#define J9PORT_CACHEINFO_ICACHE 0x01
#define J9PORT_CACHEINFO_DCACHE 0x02
#define J9PORT_CACHEINFO_UCACHE 0x04 /* unified */
#define J9PORT_CACHEINFO_TCACHE 0x08 /* trace cache, Windows only */

/* use this to indicate what information you want */
typedef enum J9CacheQueryCommand {
	J9PORT_CACHEINFO_QUERY_INVALID = 0,
	J9PORT_CACHEINFO_QUERY_TYPES, /* query cache types available at a given level & CPU.  Return value is a bitmap */
	J9PORT_CACHEINFO_QUERY_LEVELS, /* query number of cache levels */
	J9PORT_CACHEINFO_QUERY_LINESIZE, /* query line size for given level, CPU and type */
	J9PORT_CACHEINFO_QUERY_CACHESIZE, /* query cache size for given level, CPU and type */
} J9CacheQueryCommand;

typedef struct J9CacheInfoQuery {
	J9CacheQueryCommand cmd;
	int32_t cpu;
	int32_t level; /* 1 or greater */
	int32_t cacheType; /* bit map of cache types. If the cache is unified,
	* all other types, e.g. J9PORT_CACHEINFO_ICACHE and J9PORT_CACHEINFO_DCACHE, will return the same data
	*/
} J9CacheInfoQuery;

#define J9PORT_SYSINFO_GET_HW_INFO_MODEL          1
#define J9PORT_SYSINFO_HW_INFO_MODEL_BUF_LENGTH   128
#define J9PORT_SYSINFO_GET_HW_INFO_SUCCESS        0
#define J9PORT_SYSINFO_GET_HW_INFO_ERROR          -1
#define J9PORT_SYSINFO_GET_HW_INFO_NOT_AVAILABLE  -2

/* Real-time signal for Runtime instrumentation on zlinux use SIGRTMIN+1 */
#if defined(LINUX) && defined(S390)
#define SIG_RI_INTERRUPT_INDEX 1
#define SIG_RI_INTERRUPT (SIGRTMIN+SIG_RI_INTERRUPT_INDEX)
#endif

/* include the generated Port header here (at the end of the file since it relies on some defines from within this file) */
#include "j9port_generated.h"

#define j9memAlloc_fptr_t omrmemAlloc_fptr_t
#define j9memFree_fptr_t omrmemFree_fptr_t

#define J9PORT_TIME_DELTA_IN_MICROSECONDS OMRPORT_TIME_DELTA_IN_MICROSECONDS
#define J9PORT_TIME_DELTA_IN_MILLISECONDS OMRPORT_TIME_DELTA_IN_MILLISECONDS
#define J9PORT_TIME_NS_PER_MS OMRPORT_TIME_NS_PER_MS
#define J9PORT_TIME_US_PER_SEC OMRPORT_TIME_US_PER_SEC

#define J9PORT_VMEM_PROCESS_EnsureWideEnum OMRPORT_VMEM_PROCESS_EnsureWideEnum
#define J9PORT_VMEM_PROCESS_PHYSICAL OMRPORT_VMEM_PROCESS_PHYSICAL
#define J9PORT_VMEM_PROCESS_PRIVATE OMRPORT_VMEM_PROCESS_PRIVATE
#define J9PORT_VMEM_PROCESS_VIRTUAL OMRPORT_VMEM_PROCESS_VIRTUAL
#define J9PORT_VMEM_MEMORY_MODE_READ OMRPORT_VMEM_MEMORY_MODE_READ
#define J9PORT_VMEM_MEMORY_MODE_WRITE OMRPORT_VMEM_MEMORY_MODE_WRITE
#define J9PORT_VMEM_MEMORY_MODE_COMMIT OMRPORT_VMEM_MEMORY_MODE_COMMIT
#define J9PORT_VMEM_MEMORY_MODE_EXECUTE OMRPORT_VMEM_MEMORY_MODE_EXECUTE
#define J9PORT_VMEM_MEMORY_MODE_VIRTUAL OMRPORT_VMEM_MEMORY_MODE_VIRTUAL
#define J9PORT_VMEM_ALLOC_DIR_BOTTOM_UP OMRPORT_VMEM_ALLOC_DIR_BOTTOM_UP
#define J9PORT_VMEM_STRICT_ADDRESS OMRPORT_VMEM_STRICT_ADDRESS
#define J9PORT_VMEM_PAGE_FLAG_PAGEABLE OMRPORT_VMEM_PAGE_FLAG_PAGEABLE
#define J9PORT_VMEM_PAGE_FLAG_PAGEABLE_PREFERABLE OMRPORT_VMEM_PAGE_FLAG_PAGEABLE_PREFERABLE
#define J9PORT_VMEM_PAGE_FLAG_FIXED OMRPORT_VMEM_PAGE_FLAG_FIXED
#define J9PORT_VMEM_PAGE_FLAG_NOT_USED OMRPORT_VMEM_PAGE_FLAG_NOT_USED
#define J9PORT_VMEM_STRICT_PAGE_SIZE OMRPORT_VMEM_STRICT_PAGE_SIZE
#define J9PORT_VMEM_ALLOC_DIR_TOP_DOWN OMRPORT_VMEM_ALLOC_DIR_TOP_DOWN
#define J9PORT_VMEM_ALLOC_QUICK OMRPORT_VMEM_ALLOC_QUICK
#define J9PORT_VMEM_ZTPF_USE_31BIT_MALLOC OMRPORT_VMEM_ZTPF_USE_31BIT_MALLOC
#define J9PORT_VMEM_ZOS_USE_EXTENDED_PRIVATE_AREA OMRPORT_VMEM_ZOS_USE2TO32G_AREA
#define J9PORT_VMEM_ALLOCATE_TOP_DOWN OMRPORT_VMEM_ALLOCATE_TOP_DOWN
#define J9PORT_VMEM_ADDRESS_HINT OMRPORT_VMEM_ADDRESS_HINT
#define J9PORT_VMEM_NO_AFFINITY OMRPORT_VMEM_NO_AFFINITY

#define J9PORT_SLOPEN_DECORATE OMRPORT_SLOPEN_DECORATE
#define J9PORT_SLOPEN_LAZY OMRPORT_SLOPEN_LAZY
#define J9PORT_SLOPEN_NO_LOOKUP_MSG_FOR_NOT_FOUND OMRPORT_SLOPEN_NO_LOOKUP_MSG_FOR_NOT_FOUND
#define J9PORT_SLOPEN_OPEN_EXECUTABLE OMRPORT_SLOPEN_OPEN_EXECUTABLE

#define J9PORT_SL_FOUND OMRPORT_SL_FOUND
#define J9PORT_SL_NOT_FOUND OMRPORT_SL_NOT_FOUND

#define J9PORT_CTLDATA_SIG_FLAGS OMRPORT_CTLDATA_SIG_FLAGS
#define J9PORT_CTLDATA_TRACE_START OMRPORT_CTLDATA_TRACE_START
#define J9PORT_CTLDATA_TRACE_STOP OMRPORT_CTLDATA_TRACE_STOP
#define J9PORT_CTLDATA_VMEM_NUMA_IN_USE OMRPORT_CTLDATA_VMEM_NUMA_IN_USE
#define J9PORT_CTLDATA_VMEM_NUMA_ENABLE OMRPORT_CTLDATA_VMEM_NUMA_ENABLE
#define J9PORT_CTLDATA_VMEM_NUMA_INTERLEAVE_MEM OMRPORT_CTLDATA_VMEM_NUMA_INTERLEAVE_MEM
#define J9PORT_CTLDATA_SYSLOG_OPEN OMRPORT_CTLDATA_SYSLOG_OPEN
#define J9PORT_CTLDATA_SYSLOG_CLOSE OMRPORT_CTLDATA_SYSLOG_CLOSE
#define J9PORT_CTLDATA_NOIPT OMRPORT_CTLDATA_NOIPT
#define J9PORT_CTLDATA_TIME_CLEAR_TICK_TOCK OMRPORT_CTLDATA_TIME_CLEAR_TICK_TOCK
#define J9PORT_CTLDATA_MEM_CATEGORIES_SET OMRPORT_CTLDATA_MEM_CATEGORIES_SET
#define J9PORT_CTLDATA_AIX_PROC_ATTR OMRPORT_CTLDATA_AIX_PROC_ATTR
#define J9PORT_CTLDATA_ALLOCATE32_COMMIT_SIZE OMRPORT_CTLDATA_ALLOCATE32_COMMIT_SIZE
#define J9PORT_CTLDATA_NOSUBALLOC32BITMEM OMRPORT_CTLDATA_NOSUBALLOC32BITMEM
#define J9PORT_CTLDATA_VMEM_ADVISE_OS_ONFREE OMRPORT_CTLDATA_VMEM_ADVISE_OS_ONFREE
#define J9PORT_CTLDATA_VECTOR_REGS_SUPPORT_ON OMRPORT_CTLDATA_VECTOR_REGS_SUPPORT_ON
#define J9PORT_CTLDATA_VMEM_ADVISE_HUGEPAGE OMRPORT_CTLDATA_VMEM_ADVISE_HUGEPAGE

#define J9PORT_CPU_ONLINE OMRPORT_CPU_ONLINE
#define J9PORT_CPU_TARGET OMRPORT_CPU_TARGET
#define J9PORT_CPU_PHYSICAL OMRPORT_CPU_PHYSICAL
#define J9PORT_CPU_BOUND OMRPORT_CPU_BOUND

#define J9PORT_ERROR_FILE_EXIST OMRPORT_ERROR_FILE_EXIST
#define J9PORT_ERROR_FILE_NOENT OMRPORT_ERROR_FILE_NOENT
#define J9PORT_ERROR_FILE_NOPERMISSION OMRPORT_ERROR_FILE_NOPERMISSION
#define J9PORT_ERROR_FILE_NOTFOUND OMRPORT_ERROR_FILE_NOTFOUND
#define J9PORT_ERROR_FILE_FSTAT_FAILED OMRPORT_ERROR_FILE_FSTAT_FAILED
#define J9PORT_ERROR_FILE_LOCK_EDEADLK OMRPORT_ERROR_FILE_LOCK_EDEADLK
#define J9PORT_ERROR_FILE_NAMETOOLONG OMRPORT_ERROR_FILE_NAMETOOLONG
#define J9PORT_ERROR_FILE_OPFAILED OMRPORT_ERROR_FILE_OPFAILED
#define J9PORT_ERROR_FILE_LOCK_BADLOCK OMRPORT_ERROR_FILE_LOCK_BADLOCK
#define J9PORT_ERROR_SYSTEM_CALL_ERRNO_MASK OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK
#define J9PORT_ERROR_SYSTEM_CALL_CODE_SHIFT OMRPORT_ERROR_SYSTEM_CALL_CODE_SHIFT
#define J9PORT_ERROR_OPFAILED OMRPORT_ERROR_OPFAILED
#define J9PORT_ERROR_INVALID_HANDLE OMRPORT_ERROR_INVALID_HANDLE
#define J9PORT_ERROR_INVALID_ARGUMENTS OMRPORT_ERROR_INVALID_ARGUMENTS
#define J9PORT_ERROR_NOTFOUND OMRPORT_ERROR_NOTFOUND
#define J9PORT_ERROR_BADF OMRPORT_ERROR_BADF
#define J9PORT_ERROR_FILE_SYSTEMFULL OMRPORT_ERROR_FILE_SYSTEMFULL
#define J9PORT_PAGE_PROTECT_NOT_SUPPORTED OMRPORT_PAGE_PROTECT_NOT_SUPPORTED
#define J9PORT_ERROR_STRING_MEM_ALLOCATE_FAILED OMRPORT_ERROR_STRING_MEM_ALLOCATE_FAILED
#define J9PORT_ERROR_STRING_UNSUPPORTED_ENCODING OMRPORT_ERROR_STRING_UNSUPPORTED_ENCODING
#define J9PORT_ERROR_STRING_ICONV_OPEN_FAILED OMRPORT_ERROR_STRING_ICONV_OPEN_FAILED
#define J9PORT_ERROR_STRING_BUFFER_TOO_SMALL OMRPORT_ERROR_STRING_BUFFER_TOO_SMALL
#define J9PORT_ERROR_STRING_ILLEGAL_STRING OMRPORT_ERROR_STRING_ILLEGAL_STRING
#define J9PORT_ERROR_STARTUP_MEM OMRPORT_ERROR_STARTUP_MEM
#define J9PORT_ERROR_STARTUP_TLS_ALLOC OMRPORT_ERROR_STARTUP_TLS_ALLOC
#define J9PORT_ERROR_SYSINFO_NOT_SUPPORTED OMRPORT_ERROR_SYSINFO_NOT_SUPPORTED
#define J9PORT_ERROR_SYSINFO_OPFAILED OMRPORT_ERROR_SYSINFO_OPFAILED
#define J9PORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED OMRPORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED
#define J9PORT_ERROR_SYSINFO_INSUFFICIENT_PRIVILEGE OMRPORT_ERROR_SYSINFO_INSUFFICIENT_PRIVILEGE
#define J9PORT_ERROR_SYSINFO_ENV_INIT_CRASHED_COPYING_BUFFER OMRPORT_ERROR_SYSINFO_ENV_INIT_CRASHED_COPYING_BUFFER
#define J9PORT_ERROR_VMEM_INSUFFICENT_RESOURCES OMRPORT_ERROR_VMEM_INSUFFICENT_RESOURCES
#define J9PORT_ERROR_VMEM_INSUFFICENT_RESOURCES OMRPORT_ERROR_VMEM_INSUFFICENT_RESOURCES
#define J9PORT_ERROR_NOT_SUPPORTED_ON_THIS_PLATFORM OMRPORT_ERROR_NOT_SUPPORTED_ON_THIS_PLATFORM

#define J9PORT_TTY_OUT OMRPORT_TTY_OUT
#define J9PORT_TTY_ERR OMRPORT_TTY_ERR

#define J9PORT_SIG_SIGNAL OMRPORT_SIG_SIGNAL
#define J9PORT_SIG_SIGNAL_INACCESSIBLE_ADDRESS OMRPORT_SIG_SIGNAL_INACCESSIBLE_ADDRESS
#define J9PORT_SIG_ERROR OMRPORT_SIG_ERROR
#define J9PORT_SIG_OTHER OMRPORT_SIG_OTHER
#define J9PORT_SIG_FPR OMRPORT_SIG_FPR
#define J9PORT_SIG_VR OMRPORT_SIG_VR
#define J9PORT_SIG_NO_EXCEPTION OMRPORT_SIG_NO_EXCEPTION
#define J9PORT_SIG_EXCEPTION_OCCURRED OMRPORT_SIG_EXCEPTION_OCCURRED
#define J9PORT_SIG_ERROR OMRPORT_SIG_ERROR
#define J9PORT_SIG_OPTIONS_ZOS_USE_CEEHDLR OMRPORT_SIG_OPTIONS_ZOS_USE_CEEHDLR
#define J9PORT_SIG_OPTIONS_OMRSIG_NO_CHAIN OMRPORT_SIG_OPTIONS_OMRSIG_NO_CHAIN
#define J9PORT_SIG_OPTIONS_SIGXFSZ OMRPORT_SIG_OPTIONS_SIGXFSZ
#define J9PORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS
#define J9PORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS
#define J9PORT_SIG_OPTIONS_COOPERATIVE_SHUTDOWN OMRPORT_SIG_OPTIONS_COOPERATIVE_SHUTDOWN
#define J9PORT_SIG_NUM_CATEGORIES OMRPORT_SIG_NUM_CATEGORIES
#define J9PORT_SIG_VALUE_16 OMRPORT_SIG_VALUE_16
#define J9PORT_SIG_VALUE_32 OMRPORT_SIG_VALUE_32
#define J9PORT_SIG_VALUE_64 OMRPORT_SIG_VALUE_64
#define J9PORT_SIG_VALUE_128 OMRPORT_SIG_VALUE_128
#define J9PORT_SIG_VALUE_UNDEFINED OMRPORT_SIG_VALUE_UNDEFINED
#define J9PORT_SIG_VALUE_FLOAT_64 OMRPORT_SIG_VALUE_FLOAT_64
#define J9PORT_SIG_VALUE_STRING OMRPORT_SIG_VALUE_STRING
#define J9PORT_SIG_VALUE_ADDRESS OMRPORT_SIG_VALUE_ADDRESS
#define J9PORT_SIG_FLAG_SIGQUIT OMRPORT_SIG_FLAG_SIGQUIT
#define J9PORT_SIG_FLAG_SIGABRT OMRPORT_SIG_FLAG_SIGABRT
#define J9PORT_SIG_FLAG_SIGTERM OMRPORT_SIG_FLAG_SIGTERM
#define J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION
#define J9PORT_SIG_FLAG_MAY_RETURN OMRPORT_SIG_FLAG_MAY_RETURN
#define J9PORT_SIG_FLAG_SIGSEGV OMRPORT_SIG_FLAG_SIGSEGV
#define J9PORT_SIG_FLAG_SIGBUS OMRPORT_SIG_FLAG_SIGBUS
#define J9PORT_SIG_FLAG_SIGILL OMRPORT_SIG_FLAG_SIGILL
#define J9PORT_SIG_FLAG_SIGFPE OMRPORT_SIG_FLAG_SIGFPE
#define J9PORT_SIG_FLAG_SIGTRAP OMRPORT_SIG_FLAG_SIGTRAP
#define J9PORT_SIG_FLAG_SIGABEND OMRPORT_SIG_FLAG_SIGABEND
#define J9PORT_SIG_FLAG_SIGXFSZ OMRPORT_SIG_FLAG_SIGXFSZ
#define J9PORT_SIG_FLAG_SIGALLSYNC OMRPORT_SIG_FLAG_SIGALLSYNC
#define J9PORT_SIG_FLAG_SIGFPE_DIV_BY_ZERO OMRPORT_SIG_FLAG_SIGFPE_DIV_BY_ZERO
#define J9PORT_SIG_FLAG_SIGFPE_INT_DIV_BY_ZERO OMRPORT_SIG_FLAG_SIGFPE_INT_DIV_BY_ZERO
#define J9PORT_SIG_FLAG_SIGFPE_INT_OVERFLOW OMRPORT_SIG_FLAG_SIGFPE_INT_OVERFLOW
#define J9PORT_SIG_FLAG_SIGINT OMRPORT_SIG_FLAG_SIGINT
#define J9PORT_SIG_FLAG_SIGRECONFIG OMRPORT_SIG_FLAG_SIGRECONFIG
#define J9PORT_SIG_SIGNAL_TYPE OMRPORT_SIG_SIGNAL_TYPE
#define J9PORT_SIG_SIGNAL_CODE OMRPORT_SIG_SIGNAL_CODE
#define J9PORT_SIG_SIGNAL_ERROR_VALUE OMRPORT_SIG_SIGNAL_ERROR_VALUE
#define J9PORT_SIG_CONTROL OMRPORT_SIG_CONTROL
#define J9PORT_SIG_CONTROL_PC OMRPORT_SIG_CONTROL_PC
#define J9PORT_SIG_CONTROL_SP OMRPORT_SIG_CONTROL_SP
#define J9PORT_SIG_CONTROL_BP OMRPORT_SIG_CONTROL_BP
#define J9PORT_SIG_GPR OMRPORT_SIG_GPR
#define J9PORT_SIG_GPR_X86_EDI OMRPORT_SIG_GPR_X86_EDI
#define J9PORT_SIG_GPR_X86_ESI OMRPORT_SIG_GPR_X86_ESI
#define J9PORT_SIG_GPR_X86_EAX OMRPORT_SIG_GPR_X86_EAX
#define J9PORT_SIG_GPR_X86_EBX OMRPORT_SIG_GPR_X86_EBX
#define J9PORT_SIG_GPR_X86_ECX OMRPORT_SIG_GPR_X86_ECX
#define J9PORT_SIG_GPR_X86_EDX OMRPORT_SIG_GPR_X86_EDX
#define J9PORT_SIG_MODULE_NAME OMRPORT_SIG_MODULE_NAME
#define J9PORT_SIG_SIGNAL_ADDRESS OMRPORT_SIG_SIGNAL_ADDRESS
#define J9PORT_SIG_SIGNAL_HANDLER OMRPORT_SIG_SIGNAL_HANDLER
#define J9PORT_SIG_SIGNAL_PLATFORM_SIGNAL_TYPE OMRPORT_SIG_SIGNAL_PLATFORM_SIGNAL_TYPE
#define J9PORT_SIG_SIGNAL_INACCESSIBLE_ADDRESS OMRPORT_SIG_SIGNAL_INACCESSIBLE_ADDRESS
#define J9PORT_SIG_GPR_AMD64_RDI OMRPORT_SIG_GPR_AMD64_RDI
#define J9PORT_SIG_GPR_AMD64_RSI OMRPORT_SIG_GPR_AMD64_RSI
#define J9PORT_SIG_GPR_AMD64_RAX OMRPORT_SIG_GPR_AMD64_RAX
#define J9PORT_SIG_GPR_AMD64_RBX OMRPORT_SIG_GPR_AMD64_RBX
#define J9PORT_SIG_GPR_AMD64_RCX OMRPORT_SIG_GPR_AMD64_RCX
#define J9PORT_SIG_GPR_AMD64_RDX OMRPORT_SIG_GPR_AMD64_RDX
#define J9PORT_SIG_GPR_AMD64_R8 OMRPORT_SIG_GPR_AMD64_R8
#define J9PORT_SIG_GPR_AMD64_R9 OMRPORT_SIG_GPR_AMD64_R9
#define J9PORT_SIG_GPR_AMD64_R10 OMRPORT_SIG_GPR_AMD64_R10
#define J9PORT_SIG_GPR_AMD64_R11 OMRPORT_SIG_GPR_AMD64_R11
#define J9PORT_SIG_GPR_AMD64_R12 OMRPORT_SIG_GPR_AMD64_R12
#define J9PORT_SIG_GPR_AMD64_R13 OMRPORT_SIG_GPR_AMD64_R13
#define J9PORT_SIG_GPR_AMD64_R14 OMRPORT_SIG_GPR_AMD64_R14
#define J9PORT_SIG_GPR_AMD64_R15 OMRPORT_SIG_GPR_AMD64_R15
#define J9PORT_SIG_CONTROL_POWERPC_LR OMRPORT_SIG_CONTROL_POWERPC_LR
#define J9PORT_SIG_CONTROL_POWERPC_MSR OMRPORT_SIG_CONTROL_POWERPC_MSR
#define J9PORT_SIG_CONTROL_POWERPC_CTR OMRPORT_SIG_CONTROL_POWERPC_CTR
#define J9PORT_SIG_CONTROL_POWERPC_CR OMRPORT_SIG_CONTROL_POWERPC_CR
#define J9PORT_SIG_CONTROL_POWERPC_FPSCR OMRPORT_SIG_CONTROL_POWERPC_FPSCR
#define J9PORT_SIG_CONTROL_POWERPC_XER OMRPORT_SIG_CONTROL_POWERPC_XER
#define J9PORT_SIG_CONTROL_POWERPC_MQ OMRPORT_SIG_CONTROL_POWERPC_MQ
#define J9PORT_SIG_CONTROL_POWERPC_DAR OMRPORT_SIG_CONTROL_POWERPC_DAR
#define J9PORT_SIG_CONTROL_POWERPC_DSIR OMRPORT_SIG_CONTROL_POWERPC_DSIR
#define J9PORT_SIG_CONTROL_S390_FPC OMRPORT_SIG_CONTROL_S390_FPC
#define J9PORT_SIG_CONTROL_S390_GPR7 OMRPORT_SIG_CONTROL_S390_GPR7
#define J9PORT_SIG_CONTROL_X86_EFLAGS OMRPORT_SIG_CONTROL_X86_EFLAGS
#define J9PORT_SIG_SIGNAL_ZOS_CONDITION_INFORMATION_BLOCK OMRPORT_SIG_SIGNAL_ZOS_CONDITION_INFORMATION_BLOCK
#define J9PORT_SIG_SIGNAL_ZOS_CONDITION_FACILITY_ID OMRPORT_SIG_SIGNAL_ZOS_CONDITION_FACILITY_ID
#define J9PORT_SIG_SIGNAL_ZOS_CONDITION_MESSAGE_NUMBER OMRPORT_SIG_SIGNAL_ZOS_CONDITION_MESSAGE_NUMBER
#define J9PORT_SIG_SIGNAL_ZOS_CONDITION_FEEDBACK_TOKEN OMRPORT_SIG_SIGNAL_ZOS_CONDITION_FEEDBACK_TOKEN
#define J9PORT_SIG_SIGNAL_ZOS_CONDITION_SEVERITY OMRPORT_SIG_SIGNAL_ZOS_CONDITION_SEVERITY
#define J9PORT_SIG_MODULE_FUNCTION_NAM OMRPORT_SIG_MODULE_FUNCTION_NAM
#define J9PORT_SIG_WINDOWS_DEFER_TRY_EXCEPT_HANDLER OMRPORT_SIG_WINDOWS_DEFER_TRY_EXCEPT_HANDLER
#define J9PORT_SIG_CONTROL_S390_BEA OMRPORT_SIG_CONTROL_S390_BEA
#define J9PORT_SIG_GPR_ARM_R0 OMRPORT_SIG_GPR_ARM_R0
#define J9PORT_SIG_GPR_ARM_R1 OMRPORT_SIG_GPR_ARM_R1
#define J9PORT_SIG_GPR_ARM_R2 OMRPORT_SIG_GPR_ARM_R2
#define J9PORT_SIG_GPR_ARM_R3 OMRPORT_SIG_GPR_ARM_R3
#define J9PORT_SIG_GPR_ARM_R4 OMRPORT_SIG_GPR_ARM_R4
#define J9PORT_SIG_GPR_ARM_R5 OMRPORT_SIG_GPR_ARM_R5
#define J9PORT_SIG_GPR_ARM_R6 OMRPORT_SIG_GPR_ARM_R6
#define J9PORT_SIG_GPR_ARM_R7 OMRPORT_SIG_GPR_ARM_R7
#define J9PORT_SIG_GPR_ARM_R8 OMRPORT_SIG_GPR_ARM_R8
#define J9PORT_SIG_GPR_ARM_R9 OMRPORT_SIG_GPR_ARM_R9
#define J9PORT_SIG_GPR_ARM_R10 OMRPORT_SIG_GPR_ARM_R10
#define J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH
#define J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION OMRPORT_SIG_EXCEPTION_CONTINUE_EXECUTION
#define J9PORT_SIG_EXCEPTION_RETURN OMRPORT_SIG_EXCEPTION_RETURN
#define J9PORT_SIG_EXCEPTION_OCCURRED OMRPORT_SIG_EXCEPTION_OCCURRED
#define J9PORT_SIG_EXCEPTION_COOPERATIVE_SHUTDOWN OMRPORT_SIG_EXCEPTION_COOPERATIVE_SHUTDOWN
#define J9PORT_SIG_MODULE OMRPORT_SIG_MODULE

#define J9PORT_FILE_WRITE_LOCK OMRPORT_FILE_WRITE_LOCK
#define J9PORT_FILE_READ_LOCK OMRPORT_FILE_READ_LOCK
#define J9PORT_FILE_WAIT_FOR_LOCK OMRPORT_FILE_WAIT_FOR_LOCK
#define J9PORT_FILE_NOWAIT_FOR_LOCK OMRPORT_FILE_NOWAIT_FOR_LOCK
#define J9PORT_FILE_IGNORE_ID OMRPORT_FILE_IGNORE_ID

#define J9PORT_MMAP_FLAG_READ OMRPORT_MMAP_FLAG_READ

#define J9PORT_DISABLE_ENSURE_CAP32 OMRPORT_DISABLE_ENSURE_CAP32
#define J9PORT_RESOURCE_CORE_FILE OMRPORT_RESOURCE_CORE_FILE

#define J9PORT_LIMIT_HARD OMRPORT_LIMIT_HARD
#define J9PORT_LIMIT_LIMITED OMRPORT_LIMIT_LIMITED
#define J9PORT_LIMIT_UNLIMITED OMRPORT_LIMIT_UNLIMITED

#define J9PORT_PAGE_PROTECT_READ OMRPORT_PAGE_PROTECT_READ
#define J9PORT_PAGE_PROTECT_WRITE OMRPORT_PAGE_PROTECT_WRITE

#define J9PORT_MMAP_CAPABILITY_WRITE OMRPORT_MMAP_CAPABILITY_WRITE
#define J9PORT_MMAP_CAPABILITY_MSYNC OMRPORT_MMAP_CAPABILITY_MSYNC
#define J9PORT_MMAP_CAPABILITY_PROTECT OMRPORT_MMAP_CAPABILITY_PROTECT
#define J9PORT_MMAP_CAPABILITY_COPYONWRITE OMRPORT_MMAP_CAPABILITY_COPYONWRITE
#define J9PORT_MMAP_CAPABILITY_READ OMRPORT_MMAP_CAPABILITY_READ
#define J9PORT_MMAP_FLAG_WRITE OMRPORT_MMAP_FLAG_WRITE
#define J9PORT_MMAP_FLAG_SHARED OMRPORT_MMAP_FLAG_SHARED
#define J9PORT_MMAP_FLAG_COPYONWRITE OMRPORT_MMAP_FLAG_COPYONWRITE
#define J9PORT_MMAP_SYNC_WAIT OMRPORT_MMAP_SYNC_WAIT

#define J9PORT_RESOURCE_SHARED_MEMORY OMRPORT_RESOURCE_SHARED_MEMORY
#define J9PORT_RESOURCE_ADDRESS_SPACE OMRPORT_RESOURCE_ADDRESS_SPACE

#define J9PORT_ARCH_X86 OMRPORT_ARCH_X86
#define J9PORT_ARCH_HAMMER OMRPORT_ARCH_HAMMER
#define J9PORT_ARCH_PPC OMRPORT_ARCH_PPC
#define J9PORT_ARCH_PPC64 OMRPORT_ARCH_PPC64
#define J9PORT_ARCH_PPC64LE OMRPORT_ARCH_PPC64LE
#define J9PORT_ARCH_S390 OMRPORT_ARCH_S390
#define J9PORT_ARCH_S390X OMRPORT_ARCH_S390X

#define J9PORT_RESOURCE_CORE_FLAGS OMRPORT_RESOURCE_CORE_FLAGS
#define J9PORT_RESOURCE_FILE_DESCRIPTORS OMRPORT_RESOURCE_FILE_DESCRIPTORS

#define J9PORT_MEMINFO_NOT_AVAILABLE OMRPORT_MEMINFO_NOT_AVAILABLE

#define J9PORT_PROCINFO_PROC_ONLINE OMRPORT_PROCINFO_PROC_ONLINE
#define J9PORT_PROCINFO_NOT_AVAILABLE OMRPORT_PROCINFO_NOT_AVAILABLE

#endif /* !defined(OTI_J9PORT_H_) */
