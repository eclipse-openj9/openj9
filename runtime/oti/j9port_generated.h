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
 * @brief Port Library Header
 */

#ifndef portlibrarydefines_h
#define portlibrarydefines_h
/* @ddr_namespace: map_to_type=J9PortLibrary*/

#include "j9cfg.h"
#include "omrport.h"

typedef struct J9PortLibraryVersion {
	uint16_t majorVersionNumber;
	uint16_t minorVersionNumber;
	uint32_t padding;
	uint64_t capabilities;
} J9PortLibraryVersion;

typedef struct J9PortShmemStatistic {
	uintptr_t shmid;
	uintptr_t nattach;
	uintptr_t key;
	uintptr_t ouid;
	uintptr_t ogid;
	uintptr_t cuid;
	uintptr_t cgid;
	char* file;
	uintptr_t size;
	int64_t lastAttachTime;
	int64_t lastDetachTime;
	int64_t lastChangeTime;
	char* controlDir;
	J9Permission perm;
} J9PortShmemStatistic;

typedef struct J9PortShsemStatistic {
	uintptr_t semid;
	uintptr_t ouid;
	uintptr_t ogid;
	uintptr_t cuid;
	uintptr_t cgid;
	int64_t lastOpTime;
	int64_t lastChangeTime;
	int32_t nsems;
	J9Permission perm;
} J9PortShsemStatistic;

typedef struct J9RIParameters {
	uint32_t flags;
	void *controlBlock;
} J9RIParameters;

typedef struct J9GSParameters {
#if defined(S390) || defined(J9ZOS390)
	/**
	 * The following 48-bytes represents a z-specific Guarded Storage Event Parameter List (GSEPL),
	 * whose fields are populated by the hardware upon a guarded storage event.
	 */

	/* Word 0  - Flags and Reserved */
	uint32_t reserved8:8;

	/* Mode - 8 bits unsigned int:
	 *  1 - Basic-Addressing Mode
	 *  2 - Extended-Addressing Mode
	 */
	uint32_t reservedMode6:6;
	uint32_t addressingMode:2;

	/* Byte 2 - Event Cause Indications */
	/* Mode - 2 bits unsigned int Transactional-Execution-Mode
	 *  0 - CPU was not in the transactional-execution mode
	 *  1 - CPU was in the transactional-execution but NOT in constrained-transactional-execution
	 *  3 - CPU was in the transactional-execution AND in constrained-transactional-execution
	 */

	uint32_t transactionalExecutionMode:2;
	uint32_t reservedMode5:5;

	/* Instruction Cause Bit:
	 * 0 - LGG
	 * 1 - LLGFSG
	 */
	uint32_t instructionCause:1;

	/* Byte 3 - Access Information */
	uint32_t reservedBit1:1;
	/* Translation Mode 1 bit - copy of PSW bit 5 */
	uint32_t translationMode:1;
	/* Address-Space Indication Bits - copy of bits 16-17 of the PSW */
	uint32_t addressSpaceIndication:2;
	/* Access-Register Number */
	uint32_t accessRegNum:4;

	/* Bytes 4-7 - Not currently used */
	uint32_t notUsed2;
	/* Bytes 8-15  - Guarded-Storage-Event Handler Address */
	uint64_t handlerAddr;
	/* Bytes 16-23  - Guarded-Storage-Event Instruction Address */
	uint64_t instructionAddr;
	/* Bytes 24-31 - Guarded-Storage-Event Operand Address */
	uint64_t operandAddr;
	/* Bytes 32-39  - Guarded-Storage-Event Intermediate Result */
	uint64_t intermediateResult;
	/* Bytes 40-49  - Guarded-Storage-Event Return Address */
	uint64_t returnAddr;
#endif
	uint32_t flags;
	void *controlBlock;
} J9GSParameters;

/**
 * @struct J9PortLibrary
 * The port library function table
 */
struct J9Heap; /* Forward struct declaration */
struct j9shsem_handle; /* Forward struct declaration */
struct j9shmem_handle; /* Forward struct declaration */
struct j9NetworkInterfaceArray_struct; /* Forward struct declaration */
struct J9ControlFileStatus; /* Forward struct declaration */
struct J9PortLibrary;

typedef uintptr_t (*j9sig_protected_fn)(struct J9PortLibrary *portLib, void *handler_arg);
typedef uintptr_t (*j9sig_handler_fn)(struct J9PortLibrary *portLib, uint32_t gpType, void *gpInfo, void *handler_arg);

typedef struct J9PortLibrary {
	/** omrPortLibrary, must be the first member of J9PortLibrary */
	OMRPortLibrary omrPortLibrary;
	/** portVersion*/
	struct J9PortLibraryVersion portVersion;
	/** portGlobals*/
	struct J9PortLibraryGlobalData *portGlobals;
	/** see @ref j9port.c::j9port_shutdown_library "j9port_shutdown_library"*/
	int32_t  ( *port_shutdown_library)(struct J9PortLibrary *portLibrary ) ;
	/** see @ref j9port.c::j9port_isFunctionOverridden "j9port_isFunctionOverridden"*/
	int32_t  ( *port_isFunctionOverridden)(struct J9PortLibrary *portLibrary, uintptr_t offset) ;
	/** see @ref j9sysinfo.c::j9sysinfo_startup "j9sysinfo_startup"*/
	int32_t  ( *sysinfo_startup)(struct J9PortLibrary *portLibrary) ;
	/** see @ref j9sysinfo.c::j9sysinfo_shutdown "j9sysinfo_shutdown"*/
	void  ( *sysinfo_shutdown)(struct J9PortLibrary *portLibrary) ;
	/** see @ref j9sysinfo.c::j9sysinfo_get_classpathSeparator "j9sysinfo_get_classpathSeparator"*/
	uint16_t  ( *sysinfo_get_classpathSeparator)(struct J9PortLibrary *portLibrary ) ;
	/** see @ref j9sysinfo.c::j9sysinfo_get_processor_description "j9sysinfo_get_processor_description"*/
	intptr_t  ( *sysinfo_get_processor_description)(struct J9PortLibrary *portLibrary, J9ProcessorDesc *desc) ;
	/** see @ref j9sysinfo.c::j9sysinfo_processor_has_feature "j9sysinfo_processor_has_feature"*/
	BOOLEAN  ( *sysinfo_processor_has_feature)(struct J9PortLibrary *portLibrary, J9ProcessorDesc *desc, uint32_t feature) ;
	/** see @ref j9sysinfo.c::j9sysinfo_get_hw_info "j9sysinfo_get_hw_info"*/
	int32_t  ( *sysinfo_get_hw_info)(struct J9PortLibrary *portLibrary, uint32_t infoType, char * buf, uint32_t bufLen);
	/** see @ref j9sysinfo.c::j9sysinfo_get_cache_info "j9sysinfo_get_cache_info"*/
	int32_t ( *sysinfo_get_cache_info)(struct J9PortLibrary *portLibrary, const J9CacheInfoQuery * query);
	/** see @ref j9sock.c::j9sock_startup "j9sock_startup"*/
	int32_t  ( *sock_startup)(struct J9PortLibrary *portLibrary) ;
	/** see @ref j9sock.c::j9sock_shutdown "j9sock_shutdown"*/
	int32_t  ( *sock_shutdown)(struct J9PortLibrary *portLibrary) ;
	/** see @ref j9sock.c::j9sock_inetaddr "j9sock_inetaddr"*/
	int32_t  ( *sock_inetaddr)(struct J9PortLibrary *portLibrary, const char *addrStr, uint32_t *addr) ;
	/** see @ref j9sock.c::j9sock_gethostbyname "j9sock_gethostbyname"*/
	int32_t  ( *sock_gethostbyname)(struct J9PortLibrary *portLibrary, const char *name, j9hostent_t handle) ;
	/** see @ref j9gp.c::j9gp_startup "j9gp_startup"*/
	int32_t  ( *gp_startup)(struct J9PortLibrary *portLibrary) ;
	/** see @ref j9gp.c::j9gp_shutdown "j9gp_shutdown"*/
	void  ( *gp_shutdown)(struct J9PortLibrary *portLibrary) ;
	/** see @ref j9gp.c::j9gp_protect "j9gp_protect"*/
	uintptr_t  ( *gp_protect)(struct J9PortLibrary *portLibrary,  protected_fn fn, void* arg ) ;
	/** see @ref j9gp.c::j9gp_register_handler "j9gp_register_handler"*/
	void  ( *gp_register_handler)(struct J9PortLibrary *portLibrary, handler_fn fn, void *aUserData ) ;
	/** see @ref j9gp.c::j9gp_info "j9gp_info"*/
	uint32_t  ( *gp_info)(struct J9PortLibrary *portLibrary, void *info, uint32_t category, int32_t index, const char **name, void **value) ;
	/** see @ref j9gp.c::j9gp_info_count "j9gp_info_count"*/
	uint32_t  ( *gp_info_count)(struct J9PortLibrary *portLibrary, void *info, uint32_t category) ;
	/** see @ref j9gp.c::j9gp_handler_function "j9gp_handler_function"*/
	void  ( *gp_handler_function)(void *unknown) ;
	/** self_handle*/
	void* self_handle;
	/** see @ref j9ipcmutex.c::j9ipcmutex_startup "j9ipcmutex_startup"*/
	int32_t  ( *ipcmutex_startup)(struct J9PortLibrary *portLibrary) ;
	/** see @ref j9ipcmutex.c::j9ipcmutex_shutdown "j9ipcmutex_shutdown"*/
	void  ( *ipcmutex_shutdown)(struct J9PortLibrary *portLibrary) ;
	/** see @ref j9ipcmutex.c::j9ipcmutex_acquire "j9ipcmutex_acquire"*/
	int32_t  ( *ipcmutex_acquire)(struct J9PortLibrary *portLibrary, const char *name) ;
	/** see @ref j9ipcmutex.c::j9ipcmutex_release "j9ipcmutex_release"*/
	int32_t  ( *ipcmutex_release)(struct J9PortLibrary *portLibrary, const char *name) ;
	/** see @ref j9sysinfo.c::j9sysinfo_DLPAR_enabled "j9sysinfo_DLPAR_enabled"*/
	uintptr_t  ( *sysinfo_DLPAR_enabled)(struct J9PortLibrary *portLibrary) ;
	/** see @ref j9sysinfo.c::j9sysinfo_DLPAR_max_CPUs "j9sysinfo_DLPAR_max_CPUs"*/
	uintptr_t  ( *sysinfo_DLPAR_max_CPUs)(struct J9PortLibrary *portLibrary) ;
	/** see @ref j9sysinfo.c::j9sysinfo_weak_memory_consistency "j9sysinfo_weak_memory_consistency"*/
	uintptr_t  ( *sysinfo_weak_memory_consistency)(struct J9PortLibrary *portLibrary) ;
	/** see @ref j9sock.c::j9sock_gethostbyaddr "j9sock_gethostbyaddr"*/
	int32_t  ( *sock_gethostbyaddr)(struct J9PortLibrary *portLibrary, char *addr, int32_t length, int32_t type, j9hostent_t handle) ;
	/** see @ref j9sock.c::j9sock_freeaddrinfo "j9sock_freeaddrinfo"*/
	int32_t  ( *sock_freeaddrinfo)(struct J9PortLibrary *portLibrary, j9addrinfo_t handle) ;
	/** see @ref j9sock.c::j9sock_getaddrinfo "j9sock_getaddrinfo"*/
	int32_t  ( *sock_getaddrinfo)(struct J9PortLibrary *portLibrary, char *name, j9addrinfo_t hints, j9addrinfo_t result) ;
	/** see @ref j9sock.c::j9sock_getaddrinfo_address "j9sock_getaddrinfo_address"*/
	int32_t  ( *sock_getaddrinfo_address)(struct J9PortLibrary *portLibrary, j9addrinfo_t handle, uint8_t *address, int index, uint32_t* scope_id) ;
	/** see @ref j9sock.c::j9sock_getaddrinfo_create_hints "j9sock_getaddrinfo_create_hints"*/
	int32_t  ( *sock_getaddrinfo_create_hints)(struct J9PortLibrary *portLibrary, j9addrinfo_t *result, int16_t family, int32_t socktype, int32_t protocol, int32_t flags) ;
	/** see @ref j9sock.c::j9sock_getaddrinfo_family "j9sock_getaddrinfo_family"*/
	int32_t  ( *sock_getaddrinfo_family)(struct J9PortLibrary *portLibrary, j9addrinfo_t handle, int32_t *family, int index) ;
	/** see @ref j9sock.c::j9sock_getaddrinfo_length "j9sock_getaddrinfo_length"*/
	int32_t  ( *sock_getaddrinfo_length)(struct J9PortLibrary *portLibrary, j9addrinfo_t handle, int32_t *length) ;
	/** see @ref j9sock.c::j9sock_getaddrinfo_name "j9sock_getaddrinfo_name"*/
	int32_t  ( *sock_getaddrinfo_name)(struct J9PortLibrary *portLibrary, j9addrinfo_t handle, char *name, int index) ;
	/** see @ref j9sock.c::j9sock_error_message "j9sock_error_message"*/
	const char*  ( *sock_error_message)(struct J9PortLibrary *portLibrary) ;
	/** see @ref j9shsem.c::j9shsem_params_init "j9shsem_params_init"*/
	int32_t  ( *shsem_params_init)(struct J9PortLibrary *portLibrary, struct J9PortShSemParameters *params) ;
	/** see @ref j9shsem.c::j9shsem_startup "j9shsem_startup"*/
	int32_t  ( *shsem_startup)(struct J9PortLibrary *portLibrary) ;
	/** see @ref j9shsem.c::j9shsem_shutdown "j9shsem_shutdown"*/
	void  ( *shsem_shutdown)(struct J9PortLibrary *portLibrary) ;
	/** see @ref j9shsem.c::j9shsem_open "j9shsem_open"*/
	intptr_t  ( *shsem_open)(struct J9PortLibrary *portLibrary, struct j9shsem_handle **handle, const struct J9PortShSemParameters *params) ;
	/** see @ref j9shsem.c::j9shsem_post "j9shsem_post"*/
	intptr_t  ( *shsem_post)(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset, uintptr_t flag) ;
	/** see @ref j9shsem.c::j9shsem_wait "j9shsem_wait"*/
	intptr_t  ( *shsem_wait)(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset, uintptr_t flag) ;
	/** see @ref j9shsem.c::j9shsem_getVal "j9shsem_getVal"*/
	intptr_t  ( *shsem_getVal)(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset) ;
	/** see @ref j9shsem.c::j9shsem_setVal "j9shsem_setVal"*/
	intptr_t  ( *shsem_setVal)(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset, intptr_t value) ;
	/** see @ref j9shsem.c::j9shsem_close "j9shsem_close"*/
	void  ( *shsem_close)(struct J9PortLibrary *portLibrary, struct j9shsem_handle **handle) ;
	/** see @ref j9shsem.c::j9shsem_destroy "j9shsem_destroy"*/
	intptr_t  ( *shsem_destroy)(struct J9PortLibrary *portLibrary, struct j9shsem_handle **handle) ;
	/** see @ref j9shsem.c::j9shsem_deprecated_startup "j9shsem_deprecated_startup"*/
	int32_t  ( *shsem_deprecated_startup)(struct J9PortLibrary *portLibrary) ;
	/** see @ref j9shsem.c::j9shsem_deprecated_shutdown "j9shsem_deprecated_shutdown"*/
	void  ( *shsem_deprecated_shutdown)(struct J9PortLibrary *portLibrary) ;
	/** see @ref j9shsem.c::j9shsem_deprecated_open "j9shsem_deprecated_open"*/
	intptr_t  ( *shsem_deprecated_open)(struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct j9shsem_handle** handle, const char* semname, int setSize, int permission, uintptr_t flags, J9ControlFileStatus *controlFileStatus) ;
	/** see @ref j9shsem.c::j9shsem_deprecated_openDeprecated "j9shsem_deprecated_openDeprecated"*/
	intptr_t  ( *shsem_deprecated_openDeprecated)(struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct j9shsem_handle** handle, const char* semname, uintptr_t cacheFileType) ;
	/** see @ref j9shsem.c::j9shsem_deprecated_post "j9shsem_deprecated_post"*/
	intptr_t  ( *shsem_deprecated_post)(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset, uintptr_t flag) ;
	/** see @ref j9shsem.c::j9shsem_deprecated_wait "j9shsem_deprecated_wait"*/
	intptr_t  ( *shsem_deprecated_wait)(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset, uintptr_t flag) ;
	/** see @ref j9shsem.c::j9shsem_deprecated_getVal "j9shsem_deprecated_getVal"*/
	intptr_t  ( *shsem_deprecated_getVal)(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset) ;
	/** see @ref j9shsem.c::j9shsem_deprecated_setVal "j9shsem_deprecated_setVal"*/
	intptr_t  ( *shsem_deprecated_setVal)(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset, intptr_t value) ;
	/** see @ref j9shsem.c::j9shsem_deprecated_handle_stat "j9shsem_deprecated_handle_stat"*/
	intptr_t  ( *shsem_deprecated_handle_stat)(struct J9PortLibrary *portLibrary, struct j9shsem_handle *handle, struct J9PortShsemStatistic *statbuf);
	/** see @ref j9shsem.c::j9shsem_deprecated_close "j9shsem_deprecated_close"*/
	void  ( *shsem_deprecated_close)(struct J9PortLibrary *portLibrary, struct j9shsem_handle **handle) ;
	/** see @ref j9shsem.c::j9shsem_deprecated_destroy "j9shsem_deprecated_destroy"*/
	intptr_t  ( *shsem_deprecated_destroy)(struct J9PortLibrary *portLibrary, struct j9shsem_handle **handle) ;
	/** see @ref j9shsem.c::j9shsem_deprecated_destroyDeprecated "j9shsem_deprecated_destroyDeprecated"*/
	intptr_t  ( *shsem_deprecated_destroyDeprecated)(struct J9PortLibrary *portLibrary, struct j9shsem_handle **handle, uintptr_t cacheFileType) ;
	/** see @ref j9shsem.c::j9shsem_deprecated_getid "j9shsem_deprecated_getid"*/
	int32_t  ( *shsem_deprecated_getid)(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle) ;
	/** see @ref j9shmem.c::j9shmem_startup "j9shmem_startup"*/
	int32_t  ( *shmem_startup)(struct J9PortLibrary *portLibrary) ;
	/** see @ref j9shmem.c::j9shmem_shutdown "j9shmem_shutdown"*/
	void  ( *shmem_shutdown)(struct J9PortLibrary *portLibrary) ;
	/** see @ref j9shmem.c::j9shmem_open "j9shmem_open"*/
	intptr_t  ( *shmem_open)(struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct j9shmem_handle **handle, const char* rootname, uintptr_t size, uint32_t perm, uint32_t category, uintptr_t flags, J9ControlFileStatus *controlFileStatus) ;
	/** see @ref j9shmem.c::j9shmem_openDeprecated "j9shmem_openDeprecated"*/
	intptr_t  ( *shmem_openDeprecated)(struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct j9shmem_handle **handle, const char* rootname, uint32_t perm, uintptr_t cacheFileType, uint32_t category) ;
	/** see @ref j9shmem.c::j9shmem_attach "j9shmem_attach"*/
	void*  ( *shmem_attach)(struct J9PortLibrary *portLibrary, struct j9shmem_handle* handle,  uint32_t category) ;
	/** see @ref j9shmem.c::j9shmem_detach "j9shmem_detach"*/
	intptr_t  ( *shmem_detach)(struct J9PortLibrary *portLibrary, struct j9shmem_handle **handle) ;
	/** see @ref j9shmem.c::j9shmem_close "j9shmem_close"*/
	void  ( *shmem_close)(struct J9PortLibrary *portLibrary, struct j9shmem_handle **handle) ;
	/** see @ref j9shmem.c::j9shmem_destroy "j9shmem_destroy"*/
	intptr_t  ( *shmem_destroy)(struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct j9shmem_handle **handle) ;
	/** see @ref j9shmem.c::j9shmem_destroyDeprecated "j9shmem_destroyDeprecated"*/
	intptr_t  ( *shmem_destroyDeprecated)(struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct j9shmem_handle **handle, uintptr_t cacheFileType) ;
	/** see @ref j9shmem.c::j9shmem_findfirst "j9shmem_findfirst"*/
	uintptr_t  ( *shmem_findfirst)(struct J9PortLibrary *portLibrary, char *cacheDirName, char *resultbuf) ;
	/** see @ref j9shmem.c::j9shmem_findnext "j9shmem_findnext"*/
	int32_t  ( *shmem_findnext)(struct J9PortLibrary *portLibrary, uintptr_t findhandle, char *resultbuf) ;
	/** see @ref j9shmem.c::j9shmem_findclose "j9shmem_findclose"*/
	void  ( *shmem_findclose)(struct J9PortLibrary *portLibrary, uintptr_t findhandle) ;
	/** see @ref j9shmem.c::j9shmem_stat "j9shmem_stat"*/
	uintptr_t  ( *shmem_stat)(struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, const char* name, struct J9PortShmemStatistic* statbuf) ;
	/** see @ref j9shmem.c::j9shmem_statDeprecated "j9shmem_statDeprecated"*/
	uintptr_t  ( *shmem_statDeprecated)(struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, const char* name, struct J9PortShmemStatistic* statbuf, uintptr_t cacheFileType) ;
	/** see @ref j9shmem.c::j9shmem_handle_stat "j9shmem_handle_stat"*/
	intptr_t  ( *shmem_handle_stat)(struct J9PortLibrary *portLibrary, struct j9shmem_handle *handle, struct J9PortShmemStatistic *statbuf);
	/** see @ref j9shmem.c::j9shmem_getDir "j9shmem_getDir"*/
	intptr_t  ( *shmem_getDir)(struct J9PortLibrary* portLibrary, const char* ctrlDirName, uint32_t flags, char* buffer, uintptr_t length) ;
	/** see @ref j9shmem.c::j9shmem_createDir "j9shmem_createDir"*/
	intptr_t  ( *shmem_createDir)(struct J9PortLibrary *portLibrary, char* cacheDirName, uintptr_t cacheDirPerm, BOOLEAN cleanMemorySegments) ;
	/** see @ref j9shmem.c::j9shmem_getFilepath "j9shmem_getFilepath"*/
	intptr_t  ( *shmem_getFilepath)(struct J9PortLibrary* portLibrary, char* cacheDirName, char* buffer, uintptr_t length, const char* cachename) ;
	/** see @ref j9shmem.c::j9shmem_protect "j9shmem_protect"*/
	intptr_t  ( *shmem_protect)(struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, void* address, uintptr_t length, uintptr_t flags) ;
	/** see @ref j9shmem.c::j9shmem_get_region_granularity "j9shmem_get_region_granularity"*/
	uintptr_t  ( *shmem_get_region_granularity)(struct J9PortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, void *address) ;
	/** see @ref j9shmem.c::j9shmem_getid "j9shmem_getid"*/
	int32_t  ( *shmem_getid)(struct J9PortLibrary *portLibrary, struct j9shmem_handle* handle);
	/** see @ref j9sysinfo.c::j9sysinfo_get_processing_capacity "j9sysinfo_get_processing_capacity"*/
	uintptr_t  ( *sysinfo_get_processing_capacity)(struct J9PortLibrary *portLibrary) ;
	/** see @ref j9port.c::j9port_init_library "j9port_init_library"*/
	int32_t  ( *port_init_library)(struct J9PortLibrary *portLibrary, struct J9PortLibraryVersion *version, uintptr_t size) ;
	/** see @ref j9port.c::j9port_startup_library "j9port_startup_library"*/
	int32_t  ( *port_startup_library)(struct J9PortLibrary *portLibrary) ;
	/** see @ref j9port.c::j9port_create_library "j9port_create_library"*/
	int32_t  ( *port_create_library)(struct J9PortLibrary *portLibrary, struct J9PortLibraryVersion *version, uintptr_t size) ;
	/** see @ref j9hypervisor.c::j9hypervisor_startup "j9hypervisor_startup"*/
	int32_t  ( *hypervisor_startup)(struct J9PortLibrary *portLibrary) ;
	/** see @ref j9hypervisor.c::j9hypervisor_shutdown "j9hypervisor_shutdown"*/
	void  ( *hypervisor_shutdown)(struct J9PortLibrary *portLibrary) ;
	/** see @ref j9hypervisor.c::j9hypervisor_hypervisor_present "j9hypervisor_hypervisor_present"*/
	intptr_t  ( *hypervisor_hypervisor_present)(struct J9PortLibrary *portLibrary) ;
	/** see @ref j9hypervisor.c::j9hypervisor_get_hypervisor_info "j9hypervisor_get_hypervisor_info"*/
	intptr_t  ( *hypervisor_get_hypervisor_info)(struct J9PortLibrary *portLibrary , J9HypervisorVendorDetails *vendorDetails) ;
	/** see @ref j9hypervisor.c::j9hypervisor_get_guest_processor_usage "j9hypervisor_get_guest_processor_usage"*/
	intptr_t  ( *hypervisor_get_guest_processor_usage)(struct J9PortLibrary *portLibrary, J9GuestProcessorUsage *gpUsage) ;
	/** see @ref j9hypervisor.c::j9hypervisor_get_guest_memory_usage "j9hypervisor_get_guest_memory_usage"*/
	intptr_t  ( *hypervisor_get_guest_memory_usage)(struct J9PortLibrary *portLibrary, J9GuestMemoryUsage *gmUsage) ;
	/** see @ref j9process.c::j9process_create "j9process_create"*/
	intptr_t  ( *process_create)(struct J9PortLibrary *portLibrary, const char *command[], uintptr_t commandLength, char *env[], uintptr_t envSize, const char *dir, uint32_t options, intptr_t fdInput, intptr_t fdOutput, intptr_t fdError, J9ProcessHandle *processHandle) ;
	/** see @ref j9process.c::j9process_waitfor "j9process_waitfor"*/
	intptr_t  ( *process_waitfor)(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle) ;
	/** see @ref j9process.c::j9process_terminate "j9process_terminate"*/
	intptr_t  ( *process_terminate)(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle) ;
	/** see @ref j9process.c::j9process_write "j9process_write"*/
	intptr_t  ( *process_write)(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle, void *buffer, uintptr_t numBytes) ;
	/** see @ref j9process.c::j9process_read "j9process_read"*/
	intptr_t  ( *process_read)(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle, uintptr_t flags, void *buffer, uintptr_t numBytes) ;
	/** see @ref j9process.c::j9process_get_available "j9process_get_available"*/
	intptr_t  ( *process_get_available)(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle, uintptr_t flags) ;
	/** see @ref j9process.c::j9process_close "j9process_close"*/
	intptr_t  ( *process_close)(struct J9PortLibrary *portLibrary, J9ProcessHandle *processHandle, uint32_t options) ;
	/** see @ref j9process.c::j9process_getStream "j9process_getStream"*/
	intptr_t  ( *process_getStream)(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle, uintptr_t streamFlag, intptr_t *stream) ;
	/** see @ref j9process.c::j9process_isComplete "j9process_isComplete"*/
	intptr_t  ( *process_isComplete)(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle) ;
	/** see @ref j9process.c::j9process_get_exitCode "j9process_get_exitCode"*/
	intptr_t  ( *process_get_exitCode)(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle) ;
#if defined(J9VM_PORT_RUNTIME_INSTRUMENTATION)
	void  ( *ri_params_init)(struct J9PortLibrary *portLibrary, struct J9RIParameters *riParams, void *riControlBlock) ;
	/** see @ref j9ri.c::j9ri_initialize "j9ri_initialize"*/
	void  ( *ri_initialize)(struct J9PortLibrary *portLibrary, struct J9RIParameters *riParams) ;
	/** see @ref j9ri.c::j9ri_deinitialize "j9ri_deinitialize"*/
	void  ( *ri_deinitialize)(struct J9PortLibrary *portLibrary, struct J9RIParameters *riParams) ;
	/** see @ref j9ri.c::j9ri_enable "j9ri_enable"*/
	void  ( *ri_enable)(struct J9PortLibrary *portLibrary, struct J9RIParameters *riParams) ;
	/** see @ref j9ri.c::j9ri_disable "j9ri_disable"*/
	void  ( *ri_disable)(struct J9PortLibrary *portLibrary, struct J9RIParameters *riParams) ;
	/** see @ref j9ri.c::j9ri_enableRISupport "j9ri_enableRISupport"*/
	int32_t  ( *ri_enableRISupport)(struct J9PortLibrary *portLibrary) ;
	/** see @ref j9ri.c::j9ri_disableRISupport "j9ri_disableRISupport"*/
	int32_t  ( *ri_disableRISupport)(struct J9PortLibrary *portLibrary) ;
#endif /* defined(J9VM_PORT_RUNTIME_INSTRUMENTATION) */
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	/** see @ref j9gs.c::j9gs_params_init "j9gs_params_init"*/
	void ( *gs_params_init)(struct J9PortLibrary *portLibrary, struct J9GSParameters *gsParams, void *gsControlBlock);
	/** see @ref j9gs.c::j9gs_enable "j9gs_enable"*/
	void ( *gs_enable)(struct J9PortLibrary *portLibrary, struct J9GSParameters *gsParams, void* baseAddress, uint64_t perBitSectionSize, uint64_t bitMask);
	/** see @ref j9gs.c::j9gs_disable "j9gs_disable"*/
	void ( *gs_disable)(struct J9PortLibrary *portLibrary, struct J9GSParameters *gsParams);
	/** see @ref j9gs.c::j9gs_initialize "j9gs_initialize"*/
	int32_t ( *gs_initialize)(struct J9PortLibrary *portLibrary,struct J9GSParameters *gsParams, int32_t shiftAmount);
	/** see @ref j9gs.c::j9gs_deinitialize "j9gs_deinitialize"*/
	int32_t ( *gs_deinitialize)(struct J9PortLibrary *portLibrary, struct J9GSParameters *gsParams);
	/** see @ref j9gs.c::j9gs_isEnabled "j9gs_isEnabled"*/
	int32_t ( *gs_isEnabled)(struct J9PortLibrary *portLibrary, struct J9GSParameters *gsParams, void** baseAddress, uint64_t* perBitSectionSize, uint64_t* bitMask);
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
	/** see @ref j9portcontrol.c::j9port_control "j9port_control"*/
	int32_t (*port_control)(struct J9PortLibrary *portLibrary, const char *key, uintptr_t value) ;
#if defined(J9VM_OPT_CRIU_SUPPORT)
	/* The delta between Checkpoint and Restore of j9time_current_time_nanos() return values.
	 * It is initialized to 0 before Checkpoint, and set after restore.
	 * Only supports one Checkpoint, could be restored multiple times.
	 */
	int64_t nanoTimeMonotonicClockDelta;
	/* Invoking j9sysinfo_get_username()/getpwuid() with SSSD enabled can cause checkpoint failure.
	 * It is safe to call those methods if checkpoint is disallowed after a final restore.
	 * This is equivalent to isCheckpointAllowed(), just for portlibrary access.
	 * https://github.com/eclipse-openj9/openj9/issues/15800
	 */
	BOOLEAN isCheckPointAllowed;
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
} J9PortLibrary;

#if defined(OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS)
#define J9PORT_CAPABILITY_MASK_CAN_RESERVE J9PORT_CAPABILITY_CAN_RESERVE_SPECIFIC_ADDRESS
#else /* OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS */
#define J9PORT_CAPABILITY_MASK_CAN_RESERVE 0
#endif /* OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS */

#if defined(OMR_PORT_ALLOCATE_TOP_DOWN)
#define J9PORT_CAPABILITY_MASK_TOP_DOWN J9PORT_CAPABILITY_ALLOCATE_TOP_DOWN
#else /* OMR_PORT_ALLOCATE_TOP_DOWN */
#define J9PORT_CAPABILITY_MASK_TOP_DOWN 0
#endif /* OMR_PORT_ALLOCATE_TOP_DOWN */

#define J9PORT_CAPABILITY_MASK ((uint64_t)(J9PORT_CAPABILITY_STANDARD | J9PORT_CAPABILITY_MASK_CAN_RESERVE | J9PORT_CAPABILITY_MASK_TOP_DOWN))


#define J9PORT_SET_VERSION(portLibraryVersion, capabilityMask) \
	(portLibraryVersion)->majorVersionNumber = J9PORT_MAJOR_VERSION_NUMBER; \
	(portLibraryVersion)->minorVersionNumber = J9PORT_MINOR_VERSION_NUMBER; \
	(portLibraryVersion)->capabilities = (capabilityMask)
#define J9PORT_SET_VERSION_DEFAULT(portLibraryVersion) \
	(portLibraryVersion)->majorVersionNumber = J9PORT_MAJOR_VERSION_NUMBER; \
	(portLibraryVersion)->minorVersionNumber = J9PORT_MINOR_VERSION_NUMBER; \
	(portLibraryVersion)->capabilities = J9PORT_CAPABILITY_MASK


/**
 * @name Port library startup and shutdown functions
 * @anchor PortStartup
 * Create, initialize, startup and shutdow the port library
 * @{
 */
/** Standard startup and shutdown (port library allocated on stack or by application)  */
extern J9_CFUNC int32_t j9port_create_library(struct J9PortLibrary *portLibrary, struct J9PortLibraryVersion *version, uintptr_t size);
extern J9_CFUNC int32_t j9port_init_library(struct J9PortLibrary* portLibrary, struct J9PortLibraryVersion *version, uintptr_t size);
extern J9_CFUNC int32_t j9port_shutdown_library(struct J9PortLibrary *portLibrary);
extern J9_CFUNC int32_t j9port_startup_library(struct J9PortLibrary *portLibrary);

/** Port library self allocation routines */
extern J9_CFUNC int32_t j9port_allocate_library(struct J9PortLibraryVersion *expectedVersion, struct J9PortLibrary **portLibrary);
/** @} */

/**
 * @name Port library version and compatibility queries
 * @anchor PortVersionControl
 * Determine port library compatibility and version.
 * @{
 */
extern J9_CFUNC uintptr_t j9port_getSize(struct J9PortLibraryVersion *version);
extern J9_CFUNC int32_t j9port_getVersion(struct J9PortLibrary *portLibrary, struct J9PortLibraryVersion *version);
extern J9_CFUNC int32_t j9port_isCompatible(struct J9PortLibraryVersion *expectedVersion);
/** @} */


/**
 * @name PortLibrary Access functions
 * Users should call port library functions through these macros.
 * @code
 * PORT_ACCESS_FROM_ENV(jniEnv);
 * memoryPointer = j9mem_allocate_memory(1024);
 * @endcode
 * @{
 */
#if !defined(J9PORT_LIBRARY_DEFINE)
#define j9port_shutdown_library() privatePortLibrary->port_shutdown_library(privatePortLibrary)
#define j9port_isFunctionOverridden(param1) privatePortLibrary->port_isFunctionOverridden(privatePortLibrary,param1)
#define j9port_tls_free() OMRPORT_FROM_J9PORT(privatePortLibrary)->port_tls_free(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9error_startup() OMRPORT_FROM_J9PORT(privatePortLibrary)->error_startup(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9error_shutdown() OMRPORT_FROM_J9PORT(privatePortLibrary)->error_shutdown(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9error_last_error_number() OMRPORT_FROM_J9PORT(privatePortLibrary)->error_last_error_number(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9error_last_error_message() OMRPORT_FROM_J9PORT(privatePortLibrary)->error_last_error_message(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9error_set_last_error(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->error_set_last_error(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9error_set_last_error_with_message(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->error_set_last_error_with_message(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9time_startup() OMRPORT_FROM_J9PORT(privatePortLibrary)->time_startup(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9time_shutdown() OMRPORT_FROM_J9PORT(privatePortLibrary)->time_shutdown(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9time_msec_clock() OMRPORT_FROM_J9PORT(privatePortLibrary)->time_msec_clock(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9time_usec_clock() OMRPORT_FROM_J9PORT(privatePortLibrary)->time_usec_clock(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9time_current_time_nanos(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->time_current_time_nanos(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9time_current_time_millis() OMRPORT_FROM_J9PORT(privatePortLibrary)->time_current_time_millis(OMRPORT_FROM_J9PORT(privatePortLibrary))
#if defined(J9VM_OPT_CRIU_SUPPORT)
#define NANO_TIME_ADJUSTMENT privatePortLibrary->nanoTimeMonotonicClockDelta
#else /* defined(J9VM_OPT_CRIU_SUPPORT) */
#define NANO_TIME_ADJUSTMENT 0
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
#define j9time_nano_time() (OMRPORT_FROM_J9PORT(privatePortLibrary)->time_nano_time(OMRPORT_FROM_J9PORT(privatePortLibrary)) - NANO_TIME_ADJUSTMENT)
#define j9time_hires_clock() OMRPORT_FROM_J9PORT(privatePortLibrary)->time_hires_clock(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9time_hires_frequency() OMRPORT_FROM_J9PORT(privatePortLibrary)->time_hires_frequency(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9time_hires_delta(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->time_hires_delta(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9sysinfo_startup() privatePortLibrary->sysinfo_startup(privatePortLibrary)
#define j9sysinfo_shutdown() privatePortLibrary->sysinfo_shutdown(privatePortLibrary)
#define j9sysinfo_process_exists(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_process_exists(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9sysinfo_get_egid() OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_egid(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9sysinfo_get_euid() OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_euid(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9sysinfo_get_groups(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_groups(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9sysinfo_get_pid() OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_pid(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9sysinfo_get_ppid() OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_ppid(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9sysinfo_get_memory_info(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_memory_info(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9sysinfo_get_memory_info_with_flags(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_memory_info(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9sysinfo_get_processor_info(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_processor_info(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9sysinfo_destroy_processor_info(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_destroy_processor_info(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9sysinfo_get_physical_memory() OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_physical_memory(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9sysinfo_get_OS_version() OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_OS_version(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9sysinfo_get_env(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_env(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9sysinfo_get_CPU_architecture() OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_CPU_architecture(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9sysinfo_get_OS_type() OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_OS_type(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9sysinfo_get_classpathSeparator() privatePortLibrary->sysinfo_get_classpathSeparator(privatePortLibrary)
#define j9sysinfo_get_executable_name(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_executable_name(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9sysinfo_get_username(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_username(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9sysinfo_get_groupname(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_groupname(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9sysinfo_get_load_average(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_load_average(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9sysinfo_get_CPU_utilization(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_CPU_utilization(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9sysinfo_limit_iterator_init(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_limit_iterator_init(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9sysinfo_limit_iterator_hasNext(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_limit_iterator_hasNext(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9sysinfo_limit_iterator_next(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_limit_iterator_next(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9sysinfo_env_iterator_init(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_env_iterator_init(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9sysinfo_env_iterator_hasNext(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_env_iterator_hasNext(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9sysinfo_env_iterator_next(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_env_iterator_next(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9sysinfo_get_processor_description(param1) privatePortLibrary->sysinfo_get_processor_description(privatePortLibrary,param1)
#define j9sysinfo_processor_has_feature(param1,param2) privatePortLibrary->sysinfo_processor_has_feature(privatePortLibrary,param1,param2)
#define j9sysinfo_get_hw_info(param1,param2,param3) privatePortLibrary->sysinfo_get_hw_info(privatePortLibrary,param1,param2,param3)
#define j9sysinfo_get_cache_info(param1) privatePortLibrary->sysinfo_get_cache_info(privatePortLibrary,param1)
#define j9file_startup() OMRPORT_FROM_J9PORT(privatePortLibrary)->file_startup(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9file_shutdown() OMRPORT_FROM_J9PORT(privatePortLibrary)->file_shutdown(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9file_write(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_write(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9file_write_text(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_write_text(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9file_get_text_encoding(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_get_text_encoding(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9file_vprintf(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_vprintf(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9file_printf(param1,...) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_printf(OMRPORT_FROM_J9PORT(param1), ## __VA_ARGS__)
#define j9file_open(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_open(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9file_close(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_close(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9file_seek(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_seek(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9file_read(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_read(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9file_unlink(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_unlink(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9file_attr(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_attr(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9file_chmod(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_chmod(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9file_chown(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_chown(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9file_lastmod(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_lastmod(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9file_length(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_length(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9file_flength(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_flength(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9file_set_length(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_set_length(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9file_sync(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_sync(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9file_fstat(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_fstat(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9file_stat(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_stat(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9file_stat_filesystem(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_stat_filesystem(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9file_blockingasync_open(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_blockingasync_open(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9file_blockingasync_close(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_blockingasync_close(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9file_blockingasync_read(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_blockingasync_read(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9file_blockingasync_write(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_blockingasync_write(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9file_blockingasync_unlock_bytes(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_blockingasync_unlock_bytes(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9file_blockingasync_lock_bytes(param1,param2,param3,param4) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_blockingasync_lock_bytes(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3,param4)
#define j9file_blockingasync_set_length(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_blockingasync_set_length(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9file_blockingasync_flength(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_blockingasync_flength(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9filestream_startup() OMRPORT_FROM_J9PORT(privatePortLibrary)->filestream_startup(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9filestream_shutdown() OMRPORT_FROM_J9PORT(privatePortLibrary)->filestream_shutdown(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9filestream_open(param1, param2, param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->filestream_open(OMRPORT_FROM_J9PORT(privatePortLibrary), param1, param2, param3)
#define j9filestream_close(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->filestream_close(OMRPORT_FROM_J9PORT(privatePortLibrary), param1)
#define j9filestream_write(param1, param2, param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->filestream_write(OMRPORT_FROM_J9PORT(privatePortLibrary),param1, param2, param3)
#define j9filestream_write_text(param1, param2, param3, param4) OMRPORT_FROM_J9PORT(privatePortLibrary)->filestream_write_text(OMRPORT_FROM_J9PORT(privatePortLibrary),param1, param2, param3, param4)
#define j9filestream_vprintf(param1, param2, param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->filestream_vprintf(OMRPORT_FROM_J9PORT(privatePortLibrary),param1, param2, param3)
#define j9filestream_printf(param1, ...) OMRPORT_FROM_J9PORT(privatePortLibrary)->filestream_printf(OMRPORT_FROM_J9PORT(param1), ## __VA_ARGS__)
#define j9filestream_sync(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->filestream_sync(OMRPORT_FROM_J9PORT(privatePortLibrary), param1)
#define j9filestream_setbuffer(param1, param2, param3, param4) OMRPORT_FROM_J9PORT(privatePortLibrary)->filestream_setbuffer(OMRPORT_FROM_J9PORT(privatePortLibrary), param1, param2, param3, param4)
#define j9filestream_fdopen(param1, param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->filestream_fdopen(OMRPORT_FROM_J9PORT(privatePortLibrary), param1, param2)
#define j9filestream_fileno(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->filestream_fileno(OMRPORT_FROM_J9PORT(privatePortLibrary), param1)
#define j9sl_startup() OMRPORT_FROM_J9PORT(privatePortLibrary)->sl_startup(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9sl_shutdown() OMRPORT_FROM_J9PORT(privatePortLibrary)->sl_shutdown(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9sl_close_shared_library(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->sl_close_shared_library(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9sl_open_shared_library(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->sl_open_shared_library(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9sl_lookup_name(param1,param2,param3,param4) OMRPORT_FROM_J9PORT(privatePortLibrary)->sl_lookup_name(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3,param4)
#define j9tty_startup() OMRPORT_FROM_J9PORT(privatePortLibrary)->tty_startup(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9tty_shutdown() OMRPORT_FROM_J9PORT(privatePortLibrary)->tty_shutdown(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9tty_printf(param1,...) OMRPORT_FROM_J9PORT(privatePortLibrary)->tty_printf(OMRPORT_FROM_J9PORT(param1), ## __VA_ARGS__)
#define j9tty_vprintf(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->tty_vprintf(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9tty_get_chars(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->tty_get_chars(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9tty_err_printf(param1,...) OMRPORT_FROM_J9PORT(privatePortLibrary)->tty_err_printf(OMRPORT_FROM_J9PORT(param1), ## __VA_ARGS__)
#define j9tty_err_vprintf(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->tty_err_vprintf(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9tty_available() OMRPORT_FROM_J9PORT(privatePortLibrary)->tty_available(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9tty_daemonize() OMRPORT_FROM_J9PORT(privatePortLibrary)->tty_daemonize(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9heap_create(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->heap_create(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9heap_allocate(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->heap_allocate(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9heap_free(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->heap_free(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9heap_reallocate(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->heap_reallocate(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9mem_startup(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->mem_startup(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9mem_shutdown() OMRPORT_FROM_J9PORT(privatePortLibrary)->mem_shutdown(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9mem_allocate_memory(param1, category) OMRPORT_FROM_J9PORT(privatePortLibrary)->mem_allocate_memory(OMRPORT_FROM_J9PORT(privatePortLibrary),param1, J9_GET_CALLSITE(), category)
#define j9mem_free_memory(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->mem_free_memory(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9mem_advise_and_free_memory(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->mem_advise_and_free_memory(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9mem_reallocate_memory(param1, param2, category) OMRPORT_FROM_J9PORT(privatePortLibrary)->mem_reallocate_memory(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,J9_GET_CALLSITE(), category)
#define j9mem_allocate_memory32(param1, category) OMRPORT_FROM_J9PORT(privatePortLibrary)->mem_allocate_memory32(OMRPORT_FROM_J9PORT(privatePortLibrary),param1, J9_GET_CALLSITE(), category)
#define j9mem_free_memory32(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->mem_free_memory32(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9mem_ensure_capacity32(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->mem_ensure_capacity32(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9cpu_startup() OMRPORT_FROM_J9PORT(privatePortLibrary)->cpu_startup(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9cpu_shutdown() OMRPORT_FROM_J9PORT(privatePortLibrary)->cpu_shutdown(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9cpu_flush_icache(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->cpu_flush_icache(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9vmem_startup() OMRPORT_FROM_J9PORT(privatePortLibrary)->vmem_startup(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9vmem_shutdown() OMRPORT_FROM_J9PORT(privatePortLibrary)->vmem_shutdown(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9vmem_commit_memory(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->vmem_commit_memory(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9vmem_decommit_memory(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->vmem_decommit_memory(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9vmem_free_memory(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->vmem_free_memory(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9vmem_vmem_params_init(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->vmem_vmem_params_init(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9vmem_reserve_memory(param1,param2,param3,param4,param5,param6) OMRPORT_FROM_J9PORT(privatePortLibrary)->vmem_reserve_memory(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3,param4,param5,param6)
#define j9vmem_reserve_memory_ex(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->vmem_reserve_memory_ex(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9vmem_get_page_size(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->vmem_get_page_size(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9vmem_get_page_flags(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->vmem_get_page_flags(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9vmem_supported_page_sizes() OMRPORT_FROM_J9PORT(privatePortLibrary)->vmem_supported_page_sizes(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9vmem_supported_page_flags() OMRPORT_FROM_J9PORT(privatePortLibrary)->vmem_supported_page_flags(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9vmem_default_large_page_size_ex(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->vmem_default_large_page_size_ex(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9vmem_find_valid_page_size(param1,param2,param3,param4) OMRPORT_FROM_J9PORT(privatePortLibrary)->vmem_find_valid_page_size(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3,param4)
#define j9vmem_numa_set_affinity(param1,param2,param3,param4) OMRPORT_FROM_J9PORT(privatePortLibrary)->vmem_numa_set_affinity(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3,param4)
#define j9vmem_numa_get_node_details(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->vmem_numa_get_node_details(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9vmem_get_available_physical_memory(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->vmem_get_available_physical_memory(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9vmem_get_process_memory_size(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->vmem_get_process_memory_size(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9sock_startup() privatePortLibrary->sock_startup(privatePortLibrary)
#define j9sock_shutdown() privatePortLibrary->sock_shutdown(privatePortLibrary)
#define j9sock_inetaddr(param1,param2) privatePortLibrary->sock_inetaddr(privatePortLibrary,param1,param2)
#define j9sock_gethostbyname(param1,param2) privatePortLibrary->sock_gethostbyname(privatePortLibrary,param1,param2)
#define j9gp_startup() privatePortLibrary->gp_startup(privatePortLibrary)
#define j9gp_shutdown() privatePortLibrary->gp_shutdown(privatePortLibrary)
#define j9gp_protect(param1,param2) privatePortLibrary->gp_protect(privatePortLibrary,param1,param2)
#define j9gp_register_handler(param1,param2) privatePortLibrary->gp_register_handler(privatePortLibrary,param1,param2)
#define j9gp_info(param1,param2,param3,param4,param5) privatePortLibrary->gp_info(privatePortLibrary,param1,param2,param3,param4,param5)
#define j9gp_info_count(param1,param2) privatePortLibrary->gp_info_count(privatePortLibrary,param1,param2)
#define j9gp_handler_function() privatePortLibrary->gp_handler_function(privatePortLibrary)
#define j9str_startup() OMRPORT_FROM_J9PORT(privatePortLibrary)->str_startup(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9str_shutdown() OMRPORT_FROM_J9PORT(privatePortLibrary)->str_shutdown(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9str_printf(param1,...) OMRPORT_FROM_J9PORT(privatePortLibrary)->str_printf(OMRPORT_FROM_J9PORT(param1), ## __VA_ARGS__)
#define j9str_vprintf(param1,param2,param3,param4) OMRPORT_FROM_J9PORT(privatePortLibrary)->str_vprintf(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3,param4)
#define j9str_create_tokens(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->str_create_tokens(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9str_set_token(param1,...) OMRPORT_FROM_J9PORT(privatePortLibrary)->str_set_token(OMRPORT_FROM_J9PORT(param1), ## __VA_ARGS__)
#define j9str_subst_tokens(param1,param2,param3,param4) OMRPORT_FROM_J9PORT(privatePortLibrary)->str_subst_tokens(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3,param4)
#define j9str_free_tokens(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->str_free_tokens(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9str_set_time_tokens(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->str_set_time_tokens(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9str_convert(param1,param2,param3,param4,param5,param6) OMRPORT_FROM_J9PORT(privatePortLibrary)->str_convert(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3,param4,param5,param6)
#define j9exit_startup() OMRPORT_FROM_J9PORT(privatePortLibrary)->exit_startup(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9exit_shutdown() OMRPORT_FROM_J9PORT(privatePortLibrary)->exit_shutdown(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9exit_get_exit_code() OMRPORT_FROM_J9PORT(privatePortLibrary)->exit_get_exit_code(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9exit_shutdown_and_exit(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->exit_shutdown_and_exit(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9dump_create(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->dump_create(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9dump_startup() OMRPORT_FROM_J9PORT(privatePortLibrary)->dump_startup(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9dump_shutdown() OMRPORT_FROM_J9PORT(privatePortLibrary)->dump_shutdown(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9nls_startup() OMRPORT_FROM_J9PORT(privatePortLibrary)->nls_startup(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9nls_free_cached_data() OMRPORT_FROM_J9PORT(privatePortLibrary)->nls_free_cached_data(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9nls_shutdown() OMRPORT_FROM_J9PORT(privatePortLibrary)->nls_shutdown(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9nls_set_catalog(param1,param2,param3,param4) OMRPORT_FROM_J9PORT(privatePortLibrary)->nls_set_catalog(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3,param4)
#define j9nls_set_locale(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->nls_set_locale(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9nls_get_language() OMRPORT_FROM_J9PORT(privatePortLibrary)->nls_get_language(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9nls_get_region() OMRPORT_FROM_J9PORT(privatePortLibrary)->nls_get_region(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9nls_get_variant() OMRPORT_FROM_J9PORT(privatePortLibrary)->nls_get_variant(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9nls_printf(param1,...) OMRPORT_FROM_J9PORT(privatePortLibrary)->nls_printf(OMRPORT_FROM_J9PORT(param1), ## __VA_ARGS__)
#define j9nls_vprintf(param1,param2,param3,param4) OMRPORT_FROM_J9PORT(privatePortLibrary)->nls_vprintf(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3,param4)
#define j9nls_lookup_message(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->nls_lookup_message(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9ipcmutex_startup() privatePortLibrary->ipcmutex_startup(privatePortLibrary)
#define j9ipcmutex_shutdown() privatePortLibrary->ipcmutex_shutdown(privatePortLibrary)
#define j9ipcmutex_acquire(param1) privatePortLibrary->ipcmutex_acquire(privatePortLibrary,param1)
#define j9ipcmutex_release(param1) privatePortLibrary->ipcmutex_release(privatePortLibrary,param1)
#define j9port_control(param1,param2) privatePortLibrary->port_control(privatePortLibrary,param1,param2)
#define j9sig_startup() OMRPORT_FROM_J9PORT(privatePortLibrary)->sig_startup(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9sig_shutdown() OMRPORT_FROM_J9PORT(privatePortLibrary)->sig_shutdown(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9sig_protect(param1,param2,param3,param4,param5,param6) OMRPORT_FROM_J9PORT(privatePortLibrary)->sig_protect((OMRPortLibrary*)privatePortLibrary,(omrsig_protected_fn)param1,param2,(omrsig_handler_fn)param3,param4,param5,param6)
#define j9sig_can_protect(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->sig_can_protect(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9sig_set_async_signal_handler(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->sig_set_async_signal_handler((OMRPortLibrary*)privatePortLibrary,(omrsig_handler_fn)param1,param2,param3)
#define j9sig_set_single_async_signal_handler(param1,param2,param3,param4) OMRPORT_FROM_J9PORT(privatePortLibrary)->sig_set_single_async_signal_handler((OMRPortLibrary*)privatePortLibrary,(omrsig_handler_fn)param1,param2,param3,param4)
#define j9sig_map_os_signal_to_portlib_signal(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->sig_map_os_signal_to_portlib_signal((OMRPortLibrary*)privatePortLibrary,param1)
#define j9sig_map_portlib_signal_to_os_signal(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->sig_map_portlib_signal_to_os_signal((OMRPortLibrary*)privatePortLibrary,param1)
#define j9sig_register_os_handler(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->sig_register_os_handler((OMRPortLibrary*)privatePortLibrary,param1,(void *)param2,param3)
#define j9sig_is_main_signal_handler(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->sig_is_main_signal_handler((OMRPortLibrary*)privatePortLibrary,(void *)param1)
#define j9sig_is_signal_ignored(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->sig_is_signal_ignored((OMRPortLibrary*)privatePortLibrary,param1,param2)
#define j9sig_info(param1,param2,param3,param4,param5) OMRPORT_FROM_J9PORT(privatePortLibrary)->sig_info(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3,param4,param5)
#define j9sig_info_count(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->sig_info_count(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9sig_set_options(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->sig_set_options(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9sig_get_options() OMRPORT_FROM_J9PORT(privatePortLibrary)->sig_get_options(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9sig_get_current_signal() OMRPORT_FROM_J9PORT(privatePortLibrary)->sig_get_current_signal(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9sig_set_reporter_priority(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->sig_set_reporter_priority(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9sysinfo_DLPAR_enabled() privatePortLibrary->sysinfo_DLPAR_enabled(privatePortLibrary)
#define j9sysinfo_DLPAR_max_CPUs() privatePortLibrary->sysinfo_DLPAR_max_CPUs(privatePortLibrary)
#define j9sysinfo_weak_memory_consistency() privatePortLibrary->sysinfo_weak_memory_consistency(privatePortLibrary)
#define j9file_read_text(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_read_text(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9file_mkdir(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_mkdir(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9file_move(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_move(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9file_unlinkdir(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_unlinkdir(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9file_findfirst(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_findfirst(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9file_findnext(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_findnext(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9file_findclose(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_findclose(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9file_error_message() OMRPORT_FROM_J9PORT(privatePortLibrary)->file_error_message(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9file_unlock_bytes(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_unlock_bytes(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9file_lock_bytes(param1,param2,param3,param4) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_lock_bytes(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3,param4)
#define j9file_convert_native_fd_to_j9file_fd(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_convert_native_fd_to_omrfile_fd(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9file_convert_j9file_fd_to_native_fd(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->file_convert_omrfile_fd_to_native_fd(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9sock_gethostbyaddr(param1,param2,param3,param4) privatePortLibrary->sock_gethostbyaddr(privatePortLibrary,param1,param2,param3,param4)
#define j9sock_freeaddrinfo(param1) privatePortLibrary->sock_freeaddrinfo(privatePortLibrary,param1)
#define j9sock_getaddrinfo(param1,param2,param3) privatePortLibrary->sock_getaddrinfo(privatePortLibrary,param1,param2,param3)
#define j9sock_getaddrinfo_address(param1,param2,param3,param4) privatePortLibrary->sock_getaddrinfo_address(privatePortLibrary,param1,param2,param3,param4)
#define j9sock_getaddrinfo_create_hints(param1,param2,param3,param4,param5) privatePortLibrary->sock_getaddrinfo_create_hints(privatePortLibrary,param1,param2,param3,param4,param5)
#define j9sock_getaddrinfo_family(param1,param2,param3) privatePortLibrary->sock_getaddrinfo_family(privatePortLibrary,param1,param2,param3)
#define j9sock_getaddrinfo_length(param1,param2) privatePortLibrary->sock_getaddrinfo_length(privatePortLibrary,param1,param2)
#define j9sock_getaddrinfo_name(param1,param2,param3) privatePortLibrary->sock_getaddrinfo_name(privatePortLibrary,param1,param2,param3)
#define j9sock_error_message() privatePortLibrary->sock_error_message(privatePortLibrary)
#define j9mmap_startup() OMRPORT_FROM_J9PORT(privatePortLibrary)->mmap_startup(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9mmap_shutdown() OMRPORT_FROM_J9PORT(privatePortLibrary)->mmap_shutdown(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9mmap_capabilities() OMRPORT_FROM_J9PORT(privatePortLibrary)->mmap_capabilities(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9mmap_map_file(param1,param2,param3,param4,param5,param6) OMRPORT_FROM_J9PORT(privatePortLibrary)->mmap_map_file(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3,param4,param5,param6)
#define j9mmap_unmap_file(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->mmap_unmap_file(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9mmap_msync(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->mmap_msync(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9mmap_protect(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->mmap_protect(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9mmap_get_region_granularity(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->mmap_get_region_granularity(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9mmap_dont_need(param1, param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->mmap_dont_need(OMRPORT_FROM_J9PORT(privatePortLibrary),param1, param2)
#define j9shsem_params_init(param1) privatePortLibrary->shsem_params_init(privatePortLibrary,param1)
#define j9shsem_startup() privatePortLibrary->shsem_startup(privatePortLibrary)
#define j9shsem_shutdown() privatePortLibrary->shsem_shutdown(privatePortLibrary)
#define j9shsem_open(param1,param2) privatePortLibrary->shsem_open(privatePortLibrary,param1,param2)
#define j9shsem_post(param1,param2,param3) privatePortLibrary->shsem_post(privatePortLibrary,param1,param2,param3)
#define j9shsem_wait(param1,param2,param3) privatePortLibrary->shsem_wait(privatePortLibrary,param1,param2,param3)
#define j9shsem_getVal(param1,param2) privatePortLibrary->shsem_getVal(privatePortLibrary,param1,param2)
#define j9shsem_setVal(param1,param2,param3) privatePortLibrary->shsem_setVal(privatePortLibrary,param1,param2,param3)
#define j9shsem_close(param1) privatePortLibrary->shsem_close(privatePortLibrary,param1)
#define j9shsem_destroy(param1) privatePortLibrary->shsem_destroy(privatePortLibrary,param1)
#define j9shsem_deprecated_startup() privatePortLibrary->shsem_deprecated_startup(privatePortLibrary)
#define j9shsem_deprecated_shutdown() privatePortLibrary->shsem_deprecated_shutdown(privatePortLibrary)
#define j9shsem_deprecated_open(param1,param2,param3,param4,param5,param6,param7,param8) privatePortLibrary->shsem_deprecated_open(privatePortLibrary,param1,param2,param3,param4,param5,param6,param7,param8)
#define j9shsem_deprecated_openDeprecated(param1,param2,param3,param4,param5) privatePortLibrary->shsem_deprecated_openDeprecated(privatePortLibrary,param1,param2,param3,param4,param5)
#define j9shsem_deprecated_post(param1,param2,param3) privatePortLibrary->shsem_deprecated_post(privatePortLibrary,param1,param2,param3)
#define j9shsem_deprecated_wait(param1,param2,param3) privatePortLibrary->shsem_deprecated_wait(privatePortLibrary,param1,param2,param3)
#define j9shsem_deprecated_getVal(param1,param2) privatePortLibrary->shsem_deprecated_getVal(privatePortLibrary,param1,param2)
#define j9shsem_deprecated_setVal(param1,param2,param3) privatePortLibrary->shsem_deprecated_setVal(privatePortLibrary,param1,param2,param3)
#define j9shsem_deprecated_handle_stat(param1,param2) privatePortLibrary->shsem_deprecated_handle_stat(privatePortLibrary,param1,param2)
#define j9shsem_deprecated_close(param1) privatePortLibrary->shsem_deprecated_close(privatePortLibrary,param1)
#define j9shsem_deprecated_destroy(param1) privatePortLibrary->shsem_deprecated_destroy(privatePortLibrary,param1)
#define j9shsem_deprecated_destroyDeprecated(param1,param2) privatePortLibrary->shsem_deprecated_destroyDeprecated(privatePortLibrary,param1,param2)
#define j9shsem_deprecated_getid(param1) privatePortLibrary->shsem_deprecated_getid(privatePortLibrary,param1)
#define j9shmem_startup() privatePortLibrary->shmem_startup(privatePortLibrary)
#define j9shmem_shutdown() privatePortLibrary->shmem_shutdown(privatePortLibrary)
#define j9shmem_open(param1,param2,param3,param4,param5,param6,param7,param8,param9) privatePortLibrary->shmem_open(privatePortLibrary,param1,param2,param3,param4,param5,param6,param7,param8,param9)
#define j9shmem_openDeprecated(param1,param2,param3,param4,param5,param6,param7) privatePortLibrary->shmem_openDeprecated(privatePortLibrary,param1,param2,param3,param4,param5,param6,param7)
#define j9shmem_attach(param1,param2) privatePortLibrary->shmem_attach(privatePortLibrary,param1,param2)
#define j9shmem_detach(param1) privatePortLibrary->shmem_detach(privatePortLibrary,param1)
#define j9shmem_close(param1) privatePortLibrary->shmem_close(privatePortLibrary,param1)
#define j9shmem_destroy(param1,param2,param3) privatePortLibrary->shmem_destroy(privatePortLibrary,param1,param2,param3)
#define j9shmem_destroyDeprecated(param1,param2,param3,param4) privatePortLibrary->shmem_destroyDeprecated(privatePortLibrary,param1,param2,param3,param4)
#define j9shmem_findfirst(param1,param2) privatePortLibrary->shmem_findfirst(privatePortLibrary,param1,param2)
#define j9shmem_findnext(param1,param2) privatePortLibrary->shmem_findnext(privatePortLibrary,param1,param2)
#define j9shmem_findclose(param1) privatePortLibrary->shmem_findclose(privatePortLibrary,param1)
#define j9shmem_stat(param1,param2,param3,param4) privatePortLibrary->shmem_stat(privatePortLibrary,param1,param2,param3,param4)
#define j9shmem_statDeprecated(param1,param2,param3,param4,param5) privatePortLibrary->shmem_statDeprecated(privatePortLibrary,param1,param2,param3,param4,param5)
#define j9shmem_handle_stat(param1,param2) privatePortLibrary->shmem_handle_stat(privatePortLibrary,param1,param2)
#define j9shmem_getDir(param1,param2,param3,param4) privatePortLibrary->shmem_getDir(privatePortLibrary,param1,param2,param3,param4)
#define j9shmem_createDir(param1,param2,param3) privatePortLibrary->shmem_createDir(privatePortLibrary,param1,param2,param3)
#define j9shmem_getFilepath(param1,param2,param3,param4) privatePortLibrary->shmem_getFilepath(privatePortLibrary,param1,param2,param3,param4)
#define j9shmem_protect(param1,param2,param3,param4,param5) privatePortLibrary->shmem_protect(privatePortLibrary,param1,param2,param3,param4,param5)
#define j9shmem_get_region_granularity(param1,param2,param3) privatePortLibrary->shmem_get_region_granularity(privatePortLibrary,param1,param2,param3)
#define j9shmem_getid(param1) privatePortLibrary->shmem_getid(privatePortLibrary,param1)
#define j9sysinfo_get_limit(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_limit(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9sysinfo_set_limit(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_set_limit(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9sysinfo_get_processing_capacity() privatePortLibrary->sysinfo_get_processing_capacity(privatePortLibrary)
#define j9sysinfo_get_number_CPUs_by_type(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_number_CPUs_by_type(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9sysinfo_set_number_user_specified_CPUs(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_set_number_user_specified_CPUs(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9sysinfo_get_cwd(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_cwd(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9sysinfo_get_tmp(param1,param2,param3) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_tmp(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2,param3)
#define j9sysinfo_get_open_file_count(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_open_file_count(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9sysinfo_get_os_description(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_os_description(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9sysinfo_os_has_feature(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_os_has_feature(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#if defined(J9VM_OPT_CRIU_SUPPORT)
#define j9sysinfo_get_process_start_time(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->sysinfo_get_process_start_time(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
#define j9syslog_write(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->syslog_write(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9hypervisor_startup() privatePortLibrary->hypervisor_startup(privatePortLibrary)
#define j9hypervisor_shutdown() privatePortLibrary->hypervisor_shutdown(privatePortLibrary)
#define j9hypervisor_hypervisor_present() privatePortLibrary->hypervisor_hypervisor_present(privatePortLibrary)
#define j9hypervisor_get_hypervisor_info(param1) privatePortLibrary->hypervisor_get_hypervisor_info(privatePortLibrary,param1)
#define j9hypervisor_get_guest_processor_usage(param1) privatePortLibrary->hypervisor_get_guest_processor_usage(privatePortLibrary,param1)
#define j9hypervisor_get_guest_memory_usage(param1) privatePortLibrary->hypervisor_get_guest_memory_usage(privatePortLibrary,param1)
#define j9process_create(param1,param2,param3,param4,param5,param6,param7,param8,param9,param10) privatePortLibrary->process_create(privatePortLibrary,param1,param2,param3,param4,param5,param6,param7,param8,param9,param10)
#define j9process_waitfor(param1) privatePortLibrary->process_waitfor(privatePortLibrary,param1)
#define j9process_terminate(param1) privatePortLibrary->process_terminate(privatePortLibrary,param1)
#define j9process_write(param1,param2,param3) privatePortLibrary->process_write(privatePortLibrary,param1,param2,param3)
#define j9process_read(param1,param2,param3,param4) privatePortLibrary->process_read(privatePortLibrary,param1,param2,param3,param4)
#define j9process_get_available(param1,param2) privatePortLibrary->process_get_available(privatePortLibrary,param1,param2)
#define j9process_close(param1,param2) privatePortLibrary->process_close(privatePortLibrary,param1,param2)
#define j9process_getStream(param1,param2,param3) privatePortLibrary->process_getStream(privatePortLibrary,param1,param2,param3)
#define j9process_isComplete(param1) privatePortLibrary->process_isComplete(privatePortLibrary,param1)
#define j9process_get_exitCode(param1) privatePortLibrary->process_get_exitCode(privatePortLibrary,param1)
#define j9syslog_query() OMRPORT_FROM_J9PORT(privatePortLibrary)->syslog_query(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9syslog_set(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->syslog_set(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9mem_walk_categories(param1) OMRPORT_FROM_J9PORT(privatePortLibrary)->mem_walk_categories(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)
#define j9heap_query_size(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->heap_query_size(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#define j9heap_grow(param1,param2) OMRPORT_FROM_J9PORT(privatePortLibrary)->heap_grow(OMRPORT_FROM_J9PORT(privatePortLibrary),param1,param2)
#if defined(J9VM_PORT_RUNTIME_INSTRUMENTATION)
#define j9ri_params_init(param1,param2) privatePortLibrary->ri_params_init(privatePortLibrary,param1,param2)
#define j9ri_initialize(param1) privatePortLibrary->ri_initialize(privatePortLibrary,param1)
#define j9ri_deinitialize(param1) privatePortLibrary->ri_deinitialize(privatePortLibrary,param1)
#define j9ri_enable(param1) privatePortLibrary->ri_enable(privatePortLibrary,param1)
#define j9ri_disable(param1) privatePortLibrary->ri_disable(privatePortLibrary,param1)
#define j9ri_enableRISupport() privatePortLibrary->ri_enableRISupport(privatePortLibrary)
#define j9ri_disableRISupport() privatePortLibrary->ri_disableRISupport(privatePortLibrary)
#endif /* J9VM_PORT_RUNTIME_INSTRUMENTATION */
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
#define j9gs_params_init(param1,param2) privatePortLibrary->gs_params_init(privatePortLibrary,param1,param2)
#define j9gs_enable(param1,param2,param3,param4) privatePortLibrary->gs_enable(privatePortLibrary,param1,param2,param3,param4)
#define j9gs_disable(param1) privatePortLibrary->gs_disable(privatePortLibrary,param1)
#define j9gs_initialize(param1,param2) privatePortLibrary->gs_initialize(privatePortLibrary,param1,param2)
#define j9gs_deinitialize(param1) privatePortLibrary->gs_deinitialize(privatePortLibrary,param1)
#define j9gs_isEnabled(param1,param2,param3,param4) privatePortLibrary->gs_isEnabled(privatePortLibrary,param1,param2,param3,param4)
#endif /* OMR_GC_CONCURRENT_SCAVENGER */

#if defined(OMR_OPT_CUDA)

#define j9cuda_startup() \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_startup(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9cuda_shutdown() \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_shutdown(OMRPORT_FROM_J9PORT(privatePortLibrary))
#define j9cuda_deviceAlloc(deviceId, size, deviceAddressOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_deviceAlloc(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, size, deviceAddressOut)
#define j9cuda_deviceCanAccessPeer(deviceId, peerDeviceId, canAccessPeerOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_deviceCanAccessPeer(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, peerDeviceId, canAccessPeerOut)
#define j9cuda_deviceDisablePeerAccess(deviceId, peerDeviceId) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_deviceDisablePeerAccess(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, peerDeviceId)
#define j9cuda_deviceEnablePeerAccess(deviceId, peerDeviceId) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_deviceEnablePeerAccess(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, peerDeviceId)
#define j9cuda_deviceFree(deviceId, devicePointer) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_deviceFree(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, devicePointer)
#define j9cuda_deviceGetAttribute(deviceId, attribute, valueOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_deviceGetAttribute(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, attribute, valueOut)
#define j9cuda_deviceGetCacheConfig(deviceId, configOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_deviceGetCacheConfig(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, configOut)
#define j9cuda_deviceGetCount(countOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_deviceGetCount(OMRPORT_FROM_J9PORT(privatePortLibrary), countOut)
#define j9cuda_deviceGetLimit(deviceId, limit, valueOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_deviceGetLimit(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, limit, valueOut)
#define j9cuda_deviceGetMemInfo(deviceId, freeOut, totalOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_deviceGetMemInfo(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, freeOut, totalOut)
#define j9cuda_deviceGetName(deviceId, nameSize, nameOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_deviceGetName(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, nameSize, nameOut)
#define j9cuda_deviceGetSharedMemConfig(deviceId, configOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_deviceGetSharedMemConfig(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, configOut)
#define j9cuda_deviceGetStreamPriorityRange(deviceId, leastPriorityOut, greatestPriorityOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_deviceGetStreamPriorityRange(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, leastPriorityOut, greatestPriorityOut)
#define j9cuda_deviceReset(deviceId) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_deviceReset(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId)
#define j9cuda_deviceSetCacheConfig(deviceId, config) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_deviceSetCacheConfig(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, config)
#define j9cuda_deviceSetLimit(deviceId, limit, value) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_deviceSetLimit(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, limit, value)
#define j9cuda_deviceSetSharedMemConfig(deviceId, config) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_deviceSetSharedMemConfig(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, config)
#define j9cuda_deviceSynchronize(deviceId) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_deviceSynchronize(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId)
#define j9cuda_driverGetVersion(versionOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_driverGetVersion(OMRPORT_FROM_J9PORT(privatePortLibrary), versionOut)
#define j9cuda_eventCreate(deviceId, flags, eventOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_eventCreate(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, flags, eventOut)
#define j9cuda_eventDestroy(deviceId, event) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_eventDestroy(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, event)
#define j9cuda_eventElapsedTime(start, end, elapsedMillisOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_eventElapsedTime(OMRPORT_FROM_J9PORT(privatePortLibrary), start, end, elapsedMillisOut)
#define j9cuda_eventQuery(event) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_eventQuery(OMRPORT_FROM_J9PORT(privatePortLibrary), event)
#define j9cuda_eventRecord(deviceId, event, stream) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_eventRecord(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, event, stream)
#define j9cuda_eventSynchronize(event) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_eventSynchronize(OMRPORT_FROM_J9PORT(privatePortLibrary), event)
#define j9cuda_funcGetAttribute(deviceId, function, attribute, valueOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_funcGetAttribute(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, function, attribute, valueOut)
#define j9cuda_funcMaxActiveBlocksPerMultiprocessor(deviceId, function, blockSize, dynamicSharedMemorySize, flags, valueOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_funcMaxActiveBlocksPerMultiprocessor(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, function, blockSize, dynamicSharedMemorySize, flags, valueOut)
#define j9cuda_funcMaxPotentialBlockSize(deviceId, function, dynamicSharedMemoryFunction, userData, blockSizeLimit, flags, minGridSizeOut, maxBlockSizeOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_funcMaxPotentialBlockSize(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, function, dynamicSharedMemoryFunction, userData, blockSizeLimit, flags, minGridSizeOut, maxBlockSizeOut)
#define j9cuda_funcSetCacheConfig(deviceId, function, config) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_funcSetCacheConfig(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, function, config)
#define j9cuda_funcSetSharedMemConfig(deviceId, function, config) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_funcSetSharedMemConfig(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, function, config)
#define j9cuda_getErrorString(error) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_getErrorString(OMRPORT_FROM_J9PORT(privatePortLibrary), error)
#define j9cuda_hostAlloc(size, flags, hostAddressOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_hostAlloc(OMRPORT_FROM_J9PORT(privatePortLibrary), size, flags, hostAddressOut)
#define j9cuda_hostFree(hostAddress) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_hostFree(OMRPORT_FROM_J9PORT(privatePortLibrary), hostAddress)
#define j9cuda_launchKernel(deviceId, function, gridDimX, gridDimY, gridDimZ, blockDimX, blockDimY, blockDimZ, sharedMemBytes, stream, kernelParms) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_launchKernel(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, function, gridDimX, gridDimY, gridDimZ, blockDimX, blockDimY, blockDimZ, sharedMemBytes, stream, kernelParms)
#define j9cuda_linkerAddData(deviceId, linker, type, data, size, name, options) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_linkerAddData(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, linker, type, data, size, name, options)
#define j9cuda_linkerComplete(deviceId, linker, cubinOut, sizeOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_linkerComplete(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, linker, cubinOut, sizeOut)
#define j9cuda_linkerCreate(deviceId, options, linkerOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_linkerCreate(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, options, linkerOut)
#define j9cuda_linkerDestroy(deviceId, linker) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_linkerDestroy(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, linker)
#define j9cuda_memcpy2DDeviceToDevice(deviceId, targetAddress, targetPitch, sourceAddress, sourcePitch, width, height) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_memcpy2DDeviceToDevice(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, targetAddress, targetPitch, sourceAddress, sourcePitch, width, height)
#define j9cuda_memcpy2DDeviceToDeviceAsync(deviceId, targetAddress, targetPitch, sourceAddress, sourcePitch, width, height, stream) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_memcpy2DDeviceToDeviceAsync(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, targetAddress, targetPitch, sourceAddress, sourcePitch, width, height, stream)
#define j9cuda_memcpy2DDeviceToHost(deviceId, targetAddress, targetPitch, sourceAddress, sourcePitch, width, height) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_memcpy2DDeviceToHost(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, targetAddress, targetPitch, sourceAddress, sourcePitch, width, height)
#define j9cuda_memcpy2DDeviceToHostAsync(deviceId, targetAddress, targetPitch, sourceAddress, sourcePitch, width, height, stream) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_memcpy2DDeviceToHostAsync(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, targetAddress, targetPitch, sourceAddress, sourcePitch, width, height, stream)
#define j9cuda_memcpy2DHostToDevice(deviceId, targetAddress, targetPitch, sourceAddress, sourcePitch, width, height) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_memcpy2DHostToDevice(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, targetAddress, targetPitch, sourceAddress, sourcePitch, width, height)
#define j9cuda_memcpy2DHostToDeviceAsync(deviceId, targetAddress, targetPitch, sourceAddress, sourcePitch, width, height, stream) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_memcpy2DHostToDeviceAsync(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, targetAddress, targetPitch, sourceAddress, sourcePitch, width, height, stream)
#define j9cuda_memcpyDeviceToDevice(deviceId, targetAddress, sourceAddress, byteCount) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_memcpyDeviceToDevice(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, targetAddress, sourceAddress, byteCount)
#define j9cuda_memcpyDeviceToDeviceAsync(deviceId, targetAddress, sourceAddress, byteCount, stream) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_memcpyDeviceToDeviceAsync(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, targetAddress, sourceAddress, byteCount, stream)
#define j9cuda_memcpyDeviceToHost(deviceId, targetAddress, sourceAddress, byteCount) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_memcpyDeviceToHost(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, targetAddress, sourceAddress, byteCount)
#define j9cuda_memcpyDeviceToHostAsync(deviceId, targetAddress, sourceAddress, byteCount, stream) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_memcpyDeviceToHostAsync(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, targetAddress, sourceAddress, byteCount, stream)
#define j9cuda_memcpyHostToDevice(deviceId, targetAddress, sourceAddress, byteCount) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_memcpyHostToDevice(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, targetAddress, sourceAddress, byteCount)
#define j9cuda_memcpyHostToDeviceAsync(deviceId, targetAddress, sourceAddress, byteCount, stream) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_memcpyHostToDeviceAsync(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, targetAddress, sourceAddress, byteCount, stream)
#define j9cuda_memcpyPeer(targetDeviceId, targetAddress, sourceDeviceId, sourceAddress, byteCount) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_memcpyPeer(OMRPORT_FROM_J9PORT(privatePortLibrary), targetDeviceId, targetAddress, sourceDeviceId, sourceAddress, byteCount)
#define j9cuda_memcpyPeerAsync(targetDeviceId, targetAddress, sourceDeviceId, sourceAddress, byteCount, stream) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_memcpyPeerAsync(OMRPORT_FROM_J9PORT(privatePortLibrary), targetDeviceId, targetAddress, sourceDeviceId, sourceAddress, byteCount, stream)
#define j9cuda_memset8(deviceId, deviceAddress, value, count) \
				j9cuda_memset8Async(deviceId, deviceAddress, value, count, NULL)
#define j9cuda_memset8Async(deviceId, deviceAddress, value, count, stream) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_memset8Async(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, deviceAddress, value, count, stream)
#define j9cuda_memset16(deviceId, deviceAddress, value, count) \
				j9cuda_memset16Async(deviceId, deviceAddress, value, count, NULL)
#define j9cuda_memset16Async(deviceId, deviceAddress, value, count, stream) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_memset16Async(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, deviceAddress, value, count, stream)
#define j9cuda_memset32(deviceId, deviceAddress, value, count) \
		j9cuda_memset32Async(deviceId, deviceAddress, value, count, NULL)
#define j9cuda_memset32Async(deviceId, deviceAddress, value, count, stream) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_memset32Async(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, deviceAddress, value, count, stream)
#define j9cuda_moduleGetFunction(deviceId, module, name, functionOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_moduleGetFunction(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, module, name, functionOut)
#define j9cuda_moduleGetGlobal(deviceId, module, name, addressOut, sizeOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_moduleGetGlobal(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, module, name, addressOut, sizeOut)
#define j9cuda_moduleGetSurfaceRef(deviceId, module, name, surfRefOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_moduleGetSurfaceRef(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, module, name, surfRefOut)
#define j9cuda_moduleGetTextureRef(deviceId, module, name, texRefOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_moduleGetTextureRef(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, module, name, texRefOut)
#define j9cuda_moduleLoad(deviceId, image, options, moduleOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_moduleLoad(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, image, options, moduleOut)
#define j9cuda_moduleUnload(deviceId, module) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_moduleUnload(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, module)
#define j9cuda_runtimeGetVersion(versionOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_runtimeGetVersion(OMRPORT_FROM_J9PORT(privatePortLibrary), versionOut)
#define j9cuda_streamAddCallback(deviceId, stream, callback, userData) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_streamAddCallback(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, stream, callback, userData)
#define j9cuda_streamCreate(deviceId, streamOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_streamCreate(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, streamOut)
#define j9cuda_streamCreateWithPriority(deviceId, priority, flags, streamOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_streamCreateWithPriority(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, priority, flags, streamOut)
#define j9cuda_streamDestroy(deviceId, stream) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_streamDestroy(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, stream)
#define j9cuda_streamGetFlags(deviceId, stream, flagsOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_streamGetFlags(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, stream, flagsOut)
#define j9cuda_streamGetPriority(deviceId, stream, priorityOut) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_streamGetPriority(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, stream, priorityOut)
#define j9cuda_streamQuery(deviceId, stream) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_streamQuery(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, stream)
#define j9cuda_streamSynchronize(deviceId, stream) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_streamSynchronize(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, stream)
#define j9cuda_streamWaitEvent(deviceId, stream, event) \
				OMRPORT_FROM_J9PORT(privatePortLibrary)->cuda_streamWaitEvent(OMRPORT_FROM_J9PORT(privatePortLibrary), deviceId, stream, event)

#endif /* OMR_OPT_CUDA */

#endif /* !J9PORT_LIBRARY_DEFINE */

/** @} */

#endif /* portlibrarydefines_h */
