/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

#if !defined(hyport_h)
#define hyport_h

/* @ddr_namespace: default */
#include <stdarg.h> /* for va_list */
#include "j9comp.h"
#include "gp.h"                 /* for typedefs of function arguments to gp functions */
#if (defined(LINUX) || defined(DECUNIX))
#include <unistd.h>
#endif
#include "hythread.h"
#include "hysocket.h"

struct HyPortLibrary;

#define HyMaxPath 1024

#if defined(SEEK_SET)
#define HySeekSet SEEK_SET        /* Values for HyFileSeek */
#else
#define HySeekSet 0
#endif

#if defined(SEEK_CUR)
#define HySeekCur SEEK_CUR
#else
#define HySeekCur 1
#endif

#if defined(SEEK_END)
#define HySeekEnd SEEK_END
#else
#define HySeekEnd 2
#endif

#define HyPORT_PROCESS_IGNORE_OUTPUT 				1
#define HyPORT_PROCESS_NONBLOCKING_IO 				2
#define HyPORT_PROCESS_PIPE_TO_PARENT 				4
#define HyPORT_PROCESS_INHERIT_STDIN 				8
#define HyPORT_PROCESS_INHERIT_STDOUT				16
#define HyPORT_PROCESS_INHERIT_STDERR 				32
#define HyPORT_PROCESS_REDIRECT_STDERR_TO_STDOUT	64

#define HyPORT_PROCESS_STDIN 						1
#define HyPORT_PROCESS_STDOUT 						2
#define HyPORT_PROCESS_STDERR 						4

#define	HyPORT_PROCESS_DO_NOT_CLOSE_STREAMS			1

#define HyPORT_INVALID_FD	-1

#define HyOpenRead    1       /* Values for HyFileOpen */
#define HyOpenWrite   2
#define HyOpenCreate  4
#define HyOpenTruncate  8
#define HyOpenAppend  16
#define HyOpenText    32
#define HyOpenCreateNew 64      /* Use this flag with HyOpenCreate, if this flag is specified then trying to create an existing file will fail */
#define HyOpenSync  128
#define	HyOpenForInherit 512	/* Use this flag such that returned handle can be inherited by child processes */
#define	HyOpenCreateAlways 1024	/* Always creates a new file, an existing file will be overwritten */
#define HyIsDir   0       /* Return values for HyFileAttr */
#define HyIsFile  1

#define HYPORT_TTY_IN  0
#define HYPORT_TTY_OUT  1
#define HYPORT_TTY_ERR  2

#define HY_STR_(x) #x
#define HY_STR(x) HY_STR_(x)
#define HY_GET_CALLSITE() __FILE__ ":" HY_STR(__LINE__)

typedef struct HyPortLibraryVersion
{
  U_16 majorVersionNumber;
  U_16 minorVersionNumber;
  U_32 padding;
  U_64 capabilities;
} HyPortLibraryVersion;
typedef struct HyPortVmemIdentifier
{
  void *address;
  void *handle;
  UDATA size;
  UDATA pageSize;
  UDATA mode;
} HyPortVmemIdentifier;
typedef struct HyPortShmemStatistic
{
  UDATA shmid;
  UDATA nattach;
  UDATA key;
  UDATA perm;
  char *file;
  UDATA pad;
  I_64 atime;
  I_64 dtime;
  I_64 chtime;
} HyPortShmemStatistic;

typedef struct HyProcessHandleStruct *HyProcessHandle;

/** 
 * @struct HyPortLibrary 
 * The port library function table
 */
typedef UDATA (* hysig_protected_fn) (struct HyPortLibrary * portLib, void *handler_arg);        /* Forward struct declaration */
typedef UDATA (* hysig_handler_fn) (struct HyPortLibrary * portLib, U_32 gpType, void *gpInfo, void *handler_arg);       /* Forward struct declaration */
struct hyshmem_handle;          /* Forward struct declaration */
struct hyshsem_handle;          /* Forward struct declaration */
struct HyPortShmemStatistic;    /* Forward struct declaration */
struct HyPortLibrary;           /* Forward struct declaration */
struct hyNetworkInterfaceArray_struct;  /* Forward struct declaration */
struct HyPortVmemIdentifier;    /* Forward struct declaration */

typedef struct HyPortLibrary
{
  /** portVersion*/
  struct HyPortLibraryVersion portVersion;
  /** portGlobals*/
  struct HyPortLibraryGlobalData *portGlobals;
  /** see @ref hyport.c::hyport_shutdown_library "hyport_shutdown_library"*/
  I_32 (*port_shutdown_library) (struct HyPortLibrary * portLibrary);
  /** see @ref hyport.c::hyport_isFunctionOverridden "hyport_isFunctionOverridden"*/
  I_32 (*port_isFunctionOverridden) (struct HyPortLibrary *
                                            portLibrary, UDATA offset);
  /** see @ref hyport.c::hyport_tls_free "hyport_tls_free"*/
  void (*port_tls_free) (struct HyPortLibrary * portLibrary);
  /** see @ref hyerror.c::hyerror_startup "hyerror_startup"*/
  I_32 (*error_startup) (struct HyPortLibrary * portLibrary);
  /** see @ref hyerror.c::hyerror_shutdown "hyerror_shutdown"*/
  void (*error_shutdown) (struct HyPortLibrary * portLibrary);
  /** see @ref hyerror.c::hyerror_last_error_number "hyerror_last_error_number"*/
  I_32 (*error_last_error_number) (struct HyPortLibrary *
                                          portLibrary);
  /** see @ref hyerror.c::hyerror_last_error_message "hyerror_last_error_message"*/
  const char *(*error_last_error_message) (struct HyPortLibrary *
                                                  portLibrary);
  /** see @ref hyerror.c::hyerror_set_last_error "hyerror_set_last_error"*/
  I_32 (*error_set_last_error) (struct HyPortLibrary * portLibrary,
                                      I_32 platformCode,
                                      I_32 portableCode);
  /** see @ref hyerror.c::hyerror_set_last_error_with_message "hyerror_set_last_error_with_message"*/
  I_32 (*error_set_last_error_with_message) (struct HyPortLibrary *
                                                    portLibrary,
                                                    I_32 portableCode,
                                                    const char
                                                    *errorMessage);
  /** see @ref hytime.c::hytime_startup "hytime_startup"*/
  I_32 (*time_startup) (struct HyPortLibrary * portLibrary);
  /** see @ref hytime.c::hytime_shutdown "hytime_shutdown"*/
  void (*time_shutdown) (struct HyPortLibrary * portLibrary);
  /** see @ref hytime.c::hytime_msec_clock "hytime_msec_clock"*/
  UDATA (*time_msec_clock) (struct HyPortLibrary * portLibrary);
  /** see @ref hytime.c::hytime_usec_clock "hytime_usec_clock"*/
  UDATA (*time_usec_clock) (struct HyPortLibrary * portLibrary);
  /** see @ref hytime.c::hytime_current_time_millis "hytime_current_time_millis"*/
  I_64 (*time_current_time_millis) (struct HyPortLibrary *
                                          portLibrary);
  /** see @ref hytime.c::hytime_hires_clock "hytime_hires_clock"*/
  U_64 (*time_hires_clock) (struct HyPortLibrary * portLibrary);
  /** see @ref hytime.c::hytime_hires_frequency "hytime_hires_frequency"*/
  U_64 (*time_hires_frequency) (struct HyPortLibrary * portLibrary);
  /** see @ref hytime.c::hytime_hires_delta "hytime_hires_delta"*/
  U_64 (*time_hires_delta) (struct HyPortLibrary * portLibrary,
                                  U_64 startTime, U_64 endTime,
                                  UDATA requiredResolution);
  /** see @ref hysysinfo.c::hysysinfo_startup "hysysinfo_startup"*/
  I_32 (*sysinfo_startup) (struct HyPortLibrary * portLibrary);
  /** see @ref hysysinfo.c::hysysinfo_shutdown "hysysinfo_shutdown"*/
  void (*sysinfo_shutdown) (struct HyPortLibrary * portLibrary);
  /** see @ref hysysinfo.c::hysysinfo_get_pid "hysysinfo_get_pid"*/
  UDATA (*sysinfo_get_pid) (struct HyPortLibrary * portLibrary);
  /** see @ref hysysinfo.c::hysysinfo_get_physical_memory "hysysinfo_get_physical_memory"*/
  U_64 (*sysinfo_get_physical_memory) (struct HyPortLibrary *
                                              portLibrary);
  /** see @ref hysysinfo.c::hysysinfo_get_OS_version "hysysinfo_get_OS_version"*/
  const char *(*sysinfo_get_OS_version) (struct HyPortLibrary *
                                                portLibrary);
  /** see @ref hysysinfo.c::hysysinfo_get_env "hysysinfo_get_env"*/
  IDATA (*sysinfo_get_env) (struct HyPortLibrary * portLibrary,
                                  char *envVar, char *infoString,
                                  UDATA bufSize);
  /** see @ref hysysinfo.c::hysysinfo_get_CPU_architecture "hysysinfo_get_CPU_architecture"*/
  const char *(*sysinfo_get_CPU_architecture) (struct HyPortLibrary *
                                                      portLibrary);
  /** see @ref hysysinfo.c::hysysinfo_get_OS_type "hysysinfo_get_OS_type"*/
  const char *(*sysinfo_get_OS_type) (struct HyPortLibrary *
                                            portLibrary);
  /** see @ref hysysinfo.c::hysysinfo_get_classpathSeparator "hysysinfo_get_classpathSeparator"*/
  U_16 (*sysinfo_get_classpathSeparator) (struct HyPortLibrary *
                                                portLibrary);
  /** see @ref hysysinfo.c::hysysinfo_get_executable_name "hysysinfo_get_executable_name"*/
  IDATA (*sysinfo_get_executable_name) (struct HyPortLibrary *
                                              portLibrary, char *argv0,
                                              char **result);
  /** see @ref hysysinfo.c::hysysinfo_get_number_CPUs "hysysinfo_get_number_CPUs"*/
  UDATA (*sysinfo_get_number_CPUs) (struct HyPortLibrary *
                                          portLibrary);
  /** see @ref hysysinfo.c::hysysinfo_get_username "hysysinfo_get_username"*/
  IDATA (*sysinfo_get_username) (struct HyPortLibrary * portLibrary,
                                        char *buffer, UDATA length);
  /** see @ref hyfile.c::hyfile_startup "hyfile_startup"*/
  I_32 (*file_startup) (struct HyPortLibrary * portLibrary);
  /** see @ref hyfile.c::hyfile_shutdown "hyfile_shutdown"*/
  void (*file_shutdown) (struct HyPortLibrary * portLibrary);
  /** see @ref hyfile.c::hyfile_write "hyfile_write"*/
  IDATA (*file_write) (struct HyPortLibrary * portLibrary, IDATA fd,
                              const void *buf, IDATA nbytes);
  /** see @ref hyfile.c::hyfile_write_text "hyfile_write_text"*/
  IDATA (*file_write_text) (struct HyPortLibrary * portLibrary,
                                  IDATA fd, const char *buf, IDATA nbytes);
  /** see @ref hyfile.c::hyfile_vprintf "hyfile_vprintf"*/
  void (*file_vprintf) (struct HyPortLibrary * portLibrary, IDATA fd,
                              const char *format, va_list args);
  /** see @ref hyfile.c::hyfile_printf "hyfile_printf"*/
  void (*file_printf) (struct HyPortLibrary * portLibrary, IDATA fd,
                              const char *format, ...);
  /** see @ref hyfile.c::hyfile_open "hyfile_open"*/
  IDATA (*file_open) (struct HyPortLibrary * portLibrary,
                            const char *path, I_32 flags, I_32 mode);
  /** see @ref hyfile.c::hyfile_close "hyfile_close"*/
  I_32 (*file_close) (struct HyPortLibrary * portLibrary, IDATA fd);
  /** see @ref hyfile.c::hyfile_seek "hyfile_seek"*/
  I_64 (*file_seek) (struct HyPortLibrary * portLibrary, IDATA fd,
                            I_64 offset, I_32 whence);
  /** see @ref hyfile.c::hyfile_read "hyfile_read"*/
  IDATA (*file_read) (struct HyPortLibrary * portLibrary, IDATA fd,
                            void *buf, IDATA nbytes);
  /** see @ref hyfile.c::hyfile_unlink "hyfile_unlink"*/
  I_32 (*file_unlink) (struct HyPortLibrary * portLibrary,
                              const char *path);
  /** see @ref hyfile.c::hyfile_attr "hyfile_attr"*/
  I_32 (*file_attr) (struct HyPortLibrary * portLibrary,
                            const char *path);
  /** see @ref hyfile.c::hyfile_lastmod "hyfile_lastmod"*/
  I_64 (*file_lastmod) (struct HyPortLibrary * portLibrary,
                              const char *path);
  /** see @ref hyfile.c::hyfile_length "hyfile_length"*/
  I_64 (*file_length) (struct HyPortLibrary * portLibrary,
                              const char *path);
  /** see @ref hyfile.c::hyfile_set_length "hyfile_set_length"*/
  I_32 (*file_set_length) (struct HyPortLibrary * portLibrary,
                                  IDATA fd, I_64 newLength);
  /** see @ref hyfile.c::hyfile_sync "hyfile_sync"*/
  I_32 (*file_sync) (struct HyPortLibrary * portLibrary, IDATA fd);
  /** see @ref hysl.c::hysl_startup "hysl_startup"*/
  I_32 (*sl_startup) (struct HyPortLibrary * portLibrary);
  /** see @ref hysl.c::hysl_shutdown "hysl_shutdown"*/
  void (*sl_shutdown) (struct HyPortLibrary * portLibrary);
  /** see @ref hysl.c::hysl_close_shared_library "hysl_close_shared_library"*/
  UDATA (*sl_close_shared_library) (struct HyPortLibrary *
                                          portLibrary, UDATA descriptor);
  /** see @ref hysl.c::hysl_open_shared_library "hysl_open_shared_library"*/
  UDATA (*sl_open_shared_library) (struct HyPortLibrary *
                                          portLibrary, char *name,
                                          UDATA * descriptor,
                                          BOOLEAN decorate);
  /** see @ref hysl.c::hysl_lookup_name "hysl_lookup_name"*/
  UDATA (*sl_lookup_name) (struct HyPortLibrary * portLibrary,
                                  UDATA descriptor, char *name,
                                  UDATA * func, const char *argSignature);
  /** see @ref hytty.c::hytty_startup "hytty_startup"*/
  I_32 (*tty_startup) (struct HyPortLibrary * portLibrary);
  /** see @ref hytty.c::hytty_shutdown "hytty_shutdown"*/
  void (*tty_shutdown) (struct HyPortLibrary * portLibrary);
  /** see @ref hytty.c::hytty_printf "hytty_printf"*/
  void (*tty_printf) (struct HyPortLibrary * portLibrary,
                            const char *format, ...);
  /** see @ref hytty.c::hytty_vprintf "hytty_vprintf"*/
  void (*tty_vprintf) (struct HyPortLibrary * portLibrary,
                              const char *format, va_list args);
  /** see @ref hytty.c::hytty_get_chars "hytty_get_chars"*/
  IDATA (*tty_get_chars) (struct HyPortLibrary * portLibrary,
                                char *s, UDATA length);
  /** see @ref hytty.c::hytty_err_printf "hytty_err_printf"*/
  void (*tty_err_printf) (struct HyPortLibrary * portLibrary,
                                const char *format, ...);
  /** see @ref hytty.c::hytty_err_vprintf "hytty_err_vprintf"*/
  void (*tty_err_vprintf) (struct HyPortLibrary * portLibrary,
                                  const char *format, va_list args);
  /** see @ref hytty.c::hytty_available "hytty_available"*/
  IDATA (*tty_available) (struct HyPortLibrary * portLibrary);
  /** see @ref hymem.c::hymem_startup "hymem_startup"*/
  I_32 (*mem_startup) (struct HyPortLibrary * portLibrary,
                              UDATA portGlobalSize);
  /** see @ref hymem.c::hymem_shutdown "hymem_shutdown"*/
  void (*mem_shutdown) (struct HyPortLibrary * portLibrary);
  /** see @ref hymem.c::hymem_allocate_memory "hymem_allocate_memory"*/
  void *(*mem_allocate_memory) (struct HyPortLibrary * portLibrary,
                                      UDATA byteAmount);
  /** see @ref hymem.c::hymem_allocate_memory_callSite "hymem_allocate_memory_callSite"*/
  void *(*mem_allocate_memory_callSite) (struct HyPortLibrary *
                                                portLibrary, UDATA byteAmount,
                                                char *callSite);
  /** see @ref hymem.c::hymem_free_memory "hymem_free_memory"*/
  void (*mem_free_memory) (struct HyPortLibrary * portLibrary,
                                  void *memoryPointer);
  /** see @ref hymem.c::hymem_reallocate_memory "hymem_reallocate_memory"*/
  void *(*mem_reallocate_memory) (struct HyPortLibrary * portLibrary,
                                        void *memoryPointer,
                                        UDATA byteAmount);
  /** see @ref hycpu.c::hycpu_startup "hycpu_startup"*/
  I_32 (*cpu_startup) (struct HyPortLibrary * portLibrary);
  /** see @ref hycpu.c::hycpu_shutdown "hycpu_shutdown"*/
  void (*cpu_shutdown) (struct HyPortLibrary * portLibrary);
  /** see @ref hycpu.c::hycpu_flush_icache "hycpu_flush_icache"*/
  void (*cpu_flush_icache) (struct HyPortLibrary * portLibrary,
                                  void *memoryPointer, UDATA byteAmount);
  /** see @ref hyvmem.c::hyvmem_startup "hyvmem_startup"*/
  I_32 (*vmem_startup) (struct HyPortLibrary * portLibrary);
  /** see @ref hyvmem.c::hyvmem_shutdown "hyvmem_shutdown"*/
  void (*vmem_shutdown) (struct HyPortLibrary * portLibrary);
  /** see @ref hyvmem.c::hyvmem_commit_memory "hyvmem_commit_memory"*/
  void *(*vmem_commit_memory) (struct HyPortLibrary * portLibrary,
                                      void *address, UDATA byteAmount,
  struct HyPortVmemIdentifier *
    identifier);
  /** see @ref hyvmem.c::hyvmem_decommit_memory "hyvmem_decommit_memory"*/
  IDATA (*vmem_decommit_memory) (struct HyPortLibrary * portLibrary,
                                        void *address, UDATA byteAmount,
  struct HyPortVmemIdentifier *
    identifier);
  /** see @ref hyvmem.c::hyvmem_free_memory "hyvmem_free_memory"*/
  I_32 (*vmem_free_memory) (struct HyPortLibrary * portLibrary,
                                  void *userAddress, UDATA byteAmount,
  struct HyPortVmemIdentifier *
    identifier);
  /** see @ref hyvmem.c::hyvmem_reserve_memory "hyvmem_reserve_memory"*/
  void *(*vmem_reserve_memory) (struct HyPortLibrary * portLibrary,
                                      void *address, UDATA byteAmount,
  struct HyPortVmemIdentifier *
    identifier, UDATA mode,
    UDATA pageSize);
  /** see @ref hyvmem.c::hyvmem_supported_page_sizes "hyvmem_supported_page_sizes"*/
  UDATA *(*vmem_supported_page_sizes) (struct HyPortLibrary *
                                              portLibrary);
  /** see @ref hysock.c::hysock_startup "hysock_startup"*/
  I_32 (*sock_startup) (struct HyPortLibrary * portLibrary);
  /** see @ref hysock.c::hysock_shutdown "hysock_shutdown"*/
  I_32 (*sock_shutdown) (struct HyPortLibrary * portLibrary);
  /** see @ref hysock.c::hysock_htons "hysock_htons"*/
  U_16 (*sock_htons) (struct HyPortLibrary * portLibrary, U_16 val);
  /** see @ref hysock.c::hysock_write "hysock_write"*/
  I_32 (*sock_write) (struct HyPortLibrary * portLibrary,
                            hysocket_t sock, U_8 * buf, I_32 nbyte,
                            I_32 flags);
  /** see @ref hysock.c::hysock_sockaddr "hysock_sockaddr"*/
  I_32 (*sock_sockaddr) (struct HyPortLibrary * portLibrary,
                                hysockaddr_t handle, char *addrStr,
                                U_16 port);
  /** see @ref hysock.c::hysock_read "hysock_read"*/
  I_32 (*sock_read) (struct HyPortLibrary * portLibrary,
                            hysocket_t sock, U_8 * buf, I_32 nbyte,
                            I_32 flags);
  /** see @ref hysock.c::hysock_socket "hysock_socket"*/
  I_32 (*sock_socket) (struct HyPortLibrary * portLibrary,
                              hysocket_t * handle, I_32 family,
                              I_32 socktype, I_32 protocol);
  /** see @ref hysock.c::hysock_close "hysock_close"*/
  I_32 (*sock_close) (struct HyPortLibrary * portLibrary,
                            hysocket_t * sock);
  /** see @ref hysock.c::hysock_connect "hysock_connect"*/
  I_32 (*sock_connect) (struct HyPortLibrary * portLibrary,
                              hysocket_t sock, hysockaddr_t addr);
  /** see @ref hysock.c::hysock_inetaddr "hysock_inetaddr"*/
  I_32 (*sock_inetaddr) (struct HyPortLibrary * portLibrary,
                                char *addrStr, U_32 * addr);
  /** see @ref hysock.c::hysock_gethostbyname "hysock_gethostbyname"*/
  I_32 (*sock_gethostbyname) (struct HyPortLibrary * portLibrary,
                                    char *name, hyhostent_t handle);
  /** see @ref hysock.c::hysock_hostent_addrlist "hysock_hostent_addrlist"*/
  I_32 (*sock_hostent_addrlist) (struct HyPortLibrary * portLibrary,
                                        hyhostent_t handle, U_32 index);
  /** see @ref hysock.c::hysock_sockaddr_init "hysock_sockaddr_init"*/
  I_32 (*sock_sockaddr_init) (struct HyPortLibrary * portLibrary,
                                    hysockaddr_t handle, I_16 family,
                                    U_32 nipAddr, U_16 nPort);
  /** see @ref hysock.c::hysock_linger_init "hysock_linger_init"*/
  I_32 (*sock_linger_init) (struct HyPortLibrary * portLibrary,
                                  hylinger_t handle, I_32 enabled,
                                  U_16 timeout);
  /** see @ref hysock.c::hysock_setopt_linger "hysock_setopt_linger"*/
  I_32 (*sock_setopt_linger) (struct HyPortLibrary * portLibrary,
                                    hysocket_t socketP, I_32 optlevel,
                                    I_32 optname, hylinger_t optval);
  /** see @ref hygp.c::hygp_startup "hygp_startup"*/
  I_32 (*gp_startup) (struct HyPortLibrary * portLibrary);
  /** see @ref hygp.c::hygp_shutdown "hygp_shutdown"*/
  void (*gp_shutdown) (struct HyPortLibrary * portLibrary);
  /** see @ref hygp.c::hygp_protect "hygp_protect"*/
  UDATA (*gp_protect) (struct HyPortLibrary * portLibrary,
                              protected_fn fn, void *arg);
  /** see @ref hygp.c::hygp_register_handler "hygp_register_handler"*/
  void (*gp_register_handler) (struct HyPortLibrary * portLibrary,
                                      handler_fn fn, void *aUserData);
  /** see @ref hygp.c::hygp_info "hygp_info"*/
  U_32 (*gp_info) (struct HyPortLibrary * portLibrary, void *info,
                          U_32 category, I_32 index, const char **name,
                          void **value);
  /** see @ref hygp.c::hygp_info_count "hygp_info_count"*/
  U_32 (*gp_info_count) (struct HyPortLibrary * portLibrary,
                                void *info, U_32 category);
  /** see @ref hygp.c::hygp_handler_function "hygp_handler_function"*/
  void (*gp_handler_function) (void *unknown);
  /** see @ref hystr.c::hystr_startup "hystr_startup"*/
  I_32 (*str_startup) (struct HyPortLibrary * portLibrary);
  /** see @ref hystr.c::hystr_shutdown "hystr_shutdown"*/
  void (*str_shutdown) (struct HyPortLibrary * portLibrary);
  /** see @ref hystr.c::hystr_printf "hystr_printf"*/
  U_32 (*str_printf) (struct HyPortLibrary * portLibrary, char *buf,
                            U_32 bufLen, const char *format, ...);
  /** see @ref hystr.c::hystr_vprintf "hystr_vprintf"*/
  U_32 (*str_vprintf) (struct HyPortLibrary * portLibrary, char *buf,
                              U_32 bufLen, const char *format,
                              va_list args);
  /** see @ref hyexit.c::hyexit_startup "hyexit_startup"*/
  I_32 (*exit_startup) (struct HyPortLibrary * portLibrary);
  /** see @ref hyexit.c::hyexit_shutdown "hyexit_shutdown"*/
  void (*exit_shutdown) (struct HyPortLibrary * portLibrary);
  /** see @ref hyexit.c::hyexit_get_exit_code "hyexit_get_exit_code"*/
  I_32 (*exit_get_exit_code) (struct HyPortLibrary * portLibrary);
  /** see @ref hyexit.c::hyexit_shutdown_and_exit "hyexit_shutdown_and_exit"*/
  void (*exit_shutdown_and_exit) (struct HyPortLibrary * portLibrary,
                                        I_32 exitCode);
  /** self_handle*/
  void *self_handle;
  /** see @ref hydump.c::hydump_create "hydump_create"*/
  UDATA (*dump_create) (struct HyPortLibrary * portLibrary,
                              char *filename, char *dumpType,
                              void *userData);
  /** see @ref hynls.c::hynls_startup "hynls_startup"*/
  I_32 (*nls_startup) (struct HyPortLibrary * portLibrary);
  /** see @ref hynls.c::hynls_shutdown "hynls_shutdown"*/
  void (*nls_shutdown) (struct HyPortLibrary * portLibrary);
  /** see @ref hynls.c::hynls_set_catalog "hynls_set_catalog"*/
  void (*nls_set_catalog) (struct HyPortLibrary * portLibrary,
                                  const char **paths, const int nPaths,
                                  const char *baseName,
                                  const char *extension);
  /** see @ref hynls.c::hynls_set_locale "hynls_set_locale"*/
  void (*nls_set_locale) (struct HyPortLibrary * portLibrary,
                                const char *lang, const char *region,
                                const char *variant);
  /** see @ref hynls.c::hynls_get_language "hynls_get_language"*/
  const char *(*nls_get_language) (struct HyPortLibrary * portLibrary);
  /** see @ref hynls.c::hynls_get_region "hynls_get_region"*/
  const char *(*nls_get_region) (struct HyPortLibrary * portLibrary);
  /** see @ref hynls.c::hynls_get_variant "hynls_get_variant"*/
  const char *(*nls_get_variant) (struct HyPortLibrary * portLibrary);
  /** see @ref hynls.c::hynls_printf "hynls_printf"*/
  void (*nls_printf) (struct HyPortLibrary * portLibrary, UDATA flags,
                            U_32 module_name, U_32 message_num, ...);
  /** see @ref hynls.c::hynls_vprintf "hynls_vprintf"*/
  void (*nls_vprintf) (struct HyPortLibrary * portLibrary, UDATA flags,
                              U_32 module_name, U_32 message_num,
                              va_list args);
  /** see @ref hynls.c::hynls_lookup_message "hynls_lookup_message"*/
  const char *(*nls_lookup_message) (struct HyPortLibrary *
                                            portLibrary, UDATA flags,
                                            U_32 module_name,
                                            U_32 message_num,
                                            const char *default_string);
  /** see @ref hyipcmutex.c::hyipcmutex_startup "hyipcmutex_startup"*/
  I_32 (*ipcmutex_startup) (struct HyPortLibrary * portLibrary);
  /** see @ref hyipcmutex.c::hyipcmutex_shutdown "hyipcmutex_shutdown"*/
  void (*ipcmutex_shutdown) (struct HyPortLibrary * portLibrary);
  /** see @ref hyipcmutex.c::hyipcmutex_acquire "hyipcmutex_acquire"*/
  I_32 (*ipcmutex_acquire) (struct HyPortLibrary * portLibrary,
                                  const char *name);
  /** see @ref hyipcmutex.c::hyipcmutex_release "hyipcmutex_release"*/
  I_32 (*ipcmutex_release) (struct HyPortLibrary * portLibrary,
                                  const char *name);
  /** see @ref hyportcontrol.c::hyport_control "hyport_control"*/
  I_32 (*port_control) (struct HyPortLibrary * portLibrary,
                              char *key, UDATA value);
  /** see @ref hysig.c::hysig_startup "hysig_startup"*/
  I_32 (*sig_startup) (struct HyPortLibrary * portLibrary);
  /** see @ref hysig.c::hysig_shutdown "hysig_shutdown"*/
  void (*sig_shutdown) (struct HyPortLibrary * portLibrary);
  /** see @ref hysig.c::hysig_protect "hysig_protect"*/
  I_32 (*sig_protect) (struct HyPortLibrary * portLibrary,
                              hysig_protected_fn fn, void *fn_arg,
                              hysig_handler_fn handler, void *handler_arg,
                              U_32 flags, UDATA * result);
  /** see @ref hysig.c::hysig_can_protect "hysig_can_protect"*/
  I_32 (*sig_can_protect) (struct HyPortLibrary * portLibrary,
                                  U_32 flags);
  /** see @ref hysig.c::hysig_set_async_signal_handler "hysig_set_async_signal_handler"*/
  I_32 (*sig_set_async_signal_handler) (struct HyPortLibrary *
                                              portLibrary,
                                              hysig_handler_fn handler,
                                              void *handler_arg,
                                              U_32 flags);
  /** see @ref hysig.c::hysig_info "hysig_info"*/
  U_32 (*sig_info) (struct HyPortLibrary * portLibrary, void *info,
                          U_32 category, I_32 index, const char **name,
                          void **value);
  /** see @ref hysig.c::hysig_info_count "hysig_info_count"*/
  U_32 (*sig_info_count) (struct HyPortLibrary * portLibrary,
                                void *info, U_32 category);
  /** see @ref hysig.c::hysig_set_options "hysig_set_options"*/
  I_32 (*sig_set_options) (struct HyPortLibrary * portLibrary,
                                  U_32 options);
  /** see @ref hysig.c::hysig_get_options "hysig_get_options"*/
  U_32 (*sig_get_options) (struct HyPortLibrary * portLibrary);
  /** attached_thread*/
  hythread_t attached_thread;
  /** see @ref hysysinfo.c::hysysinfo_DLPAR_enabled "hysysinfo_DLPAR_enabled"*/
  UDATA (*sysinfo_DLPAR_enabled) (struct HyPortLibrary *
                                        portLibrary);
  /** see @ref hysysinfo.c::hysysinfo_DLPAR_max_CPUs "hysysinfo_DLPAR_max_CPUs"*/
  UDATA (*sysinfo_DLPAR_max_CPUs) (struct HyPortLibrary *
                                          portLibrary);
  /** see @ref hysysinfo.c::hysysinfo_weak_memory_consistency "hysysinfo_weak_memory_consistency"*/
  UDATA (*sysinfo_weak_memory_consistency) (struct HyPortLibrary *
                                                  portLibrary);
  /** see @ref hyfile.c::hyfile_read_text "hyfile_read_text"*/
  char *(*file_read_text) (struct HyPortLibrary * portLibrary,
                                  IDATA fd, char *buf, IDATA nbytes);
  /** see @ref hyfile.c::hyfile_mkdir "hyfile_mkdir"*/
  I_32 (*file_mkdir) (struct HyPortLibrary * portLibrary,
                            const char *path);
  /** see @ref hyfile.c::hyfile_move "hyfile_move"*/
  I_32 (*file_move) (struct HyPortLibrary * portLibrary,
                            const char *pathExist, const char *pathNew);
  /** see @ref hyfile.c::hyfile_unlinkdir "hyfile_unlinkdir"*/
  I_32 (*file_unlinkdir) (struct HyPortLibrary * portLibrary,
                                const char *path);
  /** see @ref hyfile.c::hyfile_findfirst "hyfile_findfirst"*/
  UDATA (*file_findfirst) (struct HyPortLibrary * portLibrary,
                                  const char *path, char *resultbuf);
  /** see @ref hyfile.c::hyfile_findnext "hyfile_findnext"*/
  I_32 (*file_findnext) (struct HyPortLibrary * portLibrary,
                                UDATA findhandle, char *resultbuf);
  /** see @ref hyfile.c::hyfile_findclose "hyfile_findclose"*/
  void (*file_findclose) (struct HyPortLibrary * portLibrary,
                                UDATA findhandle);
  /** see @ref hyfile.c::hyfile_error_message "hyfile_error_message"*/
  const char *(*file_error_message) (struct HyPortLibrary *
                                            portLibrary);
  /** see @ref hysock.c::hysock_htonl "hysock_htonl"*/
  I_32 (*sock_htonl) (struct HyPortLibrary * portLibrary, I_32 val);
  /** see @ref hysock.c::hysock_bind "hysock_bind"*/
  I_32 (*sock_bind) (struct HyPortLibrary * portLibrary,
                            hysocket_t sock, hysockaddr_t addr);
  /** see @ref hysock.c::hysock_accept "hysock_accept"*/
  I_32 (*sock_accept) (struct HyPortLibrary * portLibrary,
                              hysocket_t serverSock,
                              hysockaddr_t addrHandle,
                              hysocket_t * sockHandle);
  /** see @ref hysock.c::hysock_shutdown_input "hysock_shutdown_input"*/
  I_32 (*sock_shutdown_input) (struct HyPortLibrary * portLibrary,
                                      hysocket_t sock);
  /** see @ref hysock.c::hysock_shutdown_output "hysock_shutdown_output"*/
  I_32 (*sock_shutdown_output) (struct HyPortLibrary * portLibrary,
                                      hysocket_t sock);
  /** see @ref hysock.c::hysock_listen "hysock_listen"*/
  I_32 (*sock_listen) (struct HyPortLibrary * portLibrary,
                              hysocket_t sock, I_32 backlog);
  /** see @ref hysock.c::hysock_ntohl "hysock_ntohl"*/
  I_32 (*sock_ntohl) (struct HyPortLibrary * portLibrary, I_32 val);
  /** see @ref hysock.c::hysock_ntohs "hysock_ntohs"*/
  U_16 (*sock_ntohs) (struct HyPortLibrary * portLibrary, U_16 val);
  /** see @ref hysock.c::hysock_getpeername "hysock_getpeername"*/
  I_32 (*sock_getpeername) (struct HyPortLibrary * portLibrary,
                                  hysocket_t handle,
                                  hysockaddr_t addrHandle);
  /** see @ref hysock.c::hysock_getsockname "hysock_getsockname"*/
  I_32 (*sock_getsockname) (struct HyPortLibrary * portLibrary,
                                  hysocket_t handle,
                                  hysockaddr_t addrHandle);
  /** see @ref hysock.c::hysock_readfrom "hysock_readfrom"*/
  I_32 (*sock_readfrom) (struct HyPortLibrary * portLibrary,
                                hysocket_t sock, U_8 * buf, I_32 nbyte,
                                I_32 flags, hysockaddr_t addrHandle);
  /** see @ref hysock.c::hysock_select "hysock_select"*/
  I_32 (*sock_select) (struct HyPortLibrary * portLibrary, I_32 nfds,
                              hyfdset_t readfds, hyfdset_t writefds,
                              hyfdset_t exceptfds, hytimeval_t timeout);
  /** see @ref hysock.c::hysock_writeto "hysock_writeto"*/
  I_32 (*sock_writeto) (struct HyPortLibrary * portLibrary,
                              hysocket_t sock, U_8 * buf, I_32 nbyte,
                              I_32 flags, hysockaddr_t addrHandle);
  /** see @ref hysock.c::hysock_inetntoa "hysock_inetntoa"*/
  I_32 (*sock_inetntoa) (struct HyPortLibrary * portLibrary,
                                char **addrStr, U_32 nipAddr);
  /** see @ref hysock.c::hysock_gethostbyaddr "hysock_gethostbyaddr"*/
  I_32 (*sock_gethostbyaddr) (struct HyPortLibrary * portLibrary,
                                    char *addr, I_32 length, I_32 type,
                                    hyhostent_t handle);
  /** see @ref hysock.c::hysock_gethostname "hysock_gethostname"*/
  I_32 (*sock_gethostname) (struct HyPortLibrary * portLibrary,
                                  char *buffer, I_32 length);
  /** see @ref hysock.c::hysock_hostent_aliaslist "hysock_hostent_aliaslist"*/
  I_32 (*sock_hostent_aliaslist) (struct HyPortLibrary * portLibrary,
                                        hyhostent_t handle,
                                        char ***aliasList);
  /** see @ref hysock.c::hysock_hostent_hostname "hysock_hostent_hostname"*/
  I_32 (*sock_hostent_hostname) (struct HyPortLibrary * portLibrary,
                                        hyhostent_t handle,
                                        char **hostName);
  /** see @ref hysock.c::hysock_sockaddr_port "hysock_sockaddr_port"*/
  U_16 (*sock_sockaddr_port) (struct HyPortLibrary * portLibrary,
                                    hysockaddr_t handle);
  /** see @ref hysock.c::hysock_sockaddr_address "hysock_sockaddr_address"*/
  I_32 (*sock_sockaddr_address) (struct HyPortLibrary * portLibrary,
                                        hysockaddr_t handle);
  /** see @ref hysock.c::hysock_fdset_init "hysock_fdset_init"*/
  I_32 (*sock_fdset_init) (struct HyPortLibrary * portLibrary,
                                  hysocket_t socketP);
  /** see @ref hysock.c::hysock_fdset_size "hysock_fdset_size"*/
  I_32 (*sock_fdset_size) (struct HyPortLibrary * portLibrary,
                                  hysocket_t handle);
  /** see @ref hysock.c::hysock_timeval_init "hysock_timeval_init"*/
  I_32 (*sock_timeval_init) (struct HyPortLibrary * portLibrary,
                                    U_32 secTime, U_32 uSecTime,
                                    hytimeval_t timeP);
  /** see @ref hysock.c::hysock_getopt_int "hysock_getopt_int"*/
  I_32 (*sock_getopt_int) (struct HyPortLibrary * portLibrary,
                                  hysocket_t socketP, I_32 optlevel,
                                  I_32 optname, I_32 * optval);
  /** see @ref hysock.c::hysock_setopt_int "hysock_setopt_int"*/
  I_32 (*sock_setopt_int) (struct HyPortLibrary * portLibrary,
                                  hysocket_t socketP, I_32 optlevel,
                                  I_32 optname, I_32 * optval);
  /** see @ref hysock.c::hysock_getopt_bool "hysock_getopt_bool"*/
  I_32 (*sock_getopt_bool) (struct HyPortLibrary * portLibrary,
                                  hysocket_t socketP, I_32 optlevel,
                                  I_32 optname, BOOLEAN * optval);
  /** see @ref hysock.c::hysock_setopt_bool "hysock_setopt_bool"*/
  I_32 (*sock_setopt_bool) (struct HyPortLibrary * portLibrary,
                                  hysocket_t socketP, I_32 optlevel,
                                  I_32 optname, BOOLEAN * optval);
  /** see @ref hysock.c::hysock_getopt_byte "hysock_getopt_byte"*/
  I_32 (*sock_getopt_byte) (struct HyPortLibrary * portLibrary,
                                  hysocket_t socketP, I_32 optlevel,
                                  I_32 optname, U_8 * optval);
  /** see @ref hysock.c::hysock_setopt_byte "hysock_setopt_byte"*/
  I_32 (*sock_setopt_byte) (struct HyPortLibrary * portLibrary,
                                  hysocket_t socketP, I_32 optlevel,
                                  I_32 optname, U_8 * optval);
  /** see @ref hysock.c::hysock_getopt_linger "hysock_getopt_linger"*/
  I_32 (*sock_getopt_linger) (struct HyPortLibrary * portLibrary,
                                    hysocket_t socketP, I_32 optlevel,
                                    I_32 optname, hylinger_t optval);
  /** see @ref hysock.c::hysock_getopt_sockaddr "hysock_getopt_sockaddr"*/
  I_32 (*sock_getopt_sockaddr) (struct HyPortLibrary * portLibrary,
                                      hysocket_t socketP, I_32 optlevel,
                                      I_32 optname, hysockaddr_t optval);
  /** see @ref hysock.c::hysock_setopt_sockaddr "hysock_setopt_sockaddr"*/
  I_32 (*sock_setopt_sockaddr) (struct HyPortLibrary * portLibrary,
                                      hysocket_t socketP, I_32 optlevel,
                                      I_32 optname, hysockaddr_t optval);
  /** see @ref hysock.c::hysock_setopt_ipmreq "hysock_setopt_ipmreq"*/
  I_32 (*sock_setopt_ipmreq) (struct HyPortLibrary * portLibrary,
                                    hysocket_t socketP, I_32 optlevel,
                                    I_32 optname, hyipmreq_t optval);
  /** see @ref hysock.c::hysock_linger_enabled "hysock_linger_enabled"*/
  I_32 (*sock_linger_enabled) (struct HyPortLibrary * portLibrary,
                                      hylinger_t handle, BOOLEAN * enabled);
  /** see @ref hysock.c::hysock_linger_linger "hysock_linger_linger"*/
  I_32 (*sock_linger_linger) (struct HyPortLibrary * portLibrary,
                                    hylinger_t handle, U_16 * linger);
  /** see @ref hysock.c::hysock_ipmreq_init "hysock_ipmreq_init"*/
  I_32 (*sock_ipmreq_init) (struct HyPortLibrary * portLibrary,
                                  hyipmreq_t handle, U_32 nipmcast,
                                  U_32 nipinterface);
  /** see @ref hysock.c::hysock_setflag "hysock_setflag"*/
  I_32 (*sock_setflag) (struct HyPortLibrary * portLibrary,
                              I_32 flag, I_32 * arg);
  /** see @ref hysock.c::hysock_freeaddrinfo "hysock_freeaddrinfo"*/
  I_32 (*sock_freeaddrinfo) (struct HyPortLibrary * portLibrary,
                                    hyaddrinfo_t handle);
  /** see @ref hysock.c::hysock_getaddrinfo "hysock_getaddrinfo"*/
  I_32 (*sock_getaddrinfo) (struct HyPortLibrary * portLibrary,
                                  char *name, hyaddrinfo_t hints,
                                  hyaddrinfo_t result);
  /** see @ref hysock.c::hysock_getaddrinfo_address "hysock_getaddrinfo_address"*/
  I_32 (*sock_getaddrinfo_address) (struct HyPortLibrary *
                                          portLibrary, hyaddrinfo_t handle,
                                          U_8 * address, int index,
                                          U_32 * scope_id);
  /** see @ref hysock.c::hysock_getaddrinfo_create_hints "hysock_getaddrinfo_create_hints"*/
  I_32 (*sock_getaddrinfo_create_hints) (struct HyPortLibrary *
                                                portLibrary,
                                                hyaddrinfo_t * result,
                                                I_16 family, I_32 socktype,
                                                I_32 protocol, I_32 flags);
  /** see @ref hysock.c::hysock_getaddrinfo_family "hysock_getaddrinfo_family"*/
  I_32 (*sock_getaddrinfo_family) (struct HyPortLibrary *
                                          portLibrary, hyaddrinfo_t handle,
                                          I_32 * family, int index);
  /** see @ref hysock.c::hysock_getaddrinfo_length "hysock_getaddrinfo_length"*/
  I_32 (*sock_getaddrinfo_length) (struct HyPortLibrary *
                                          portLibrary, hyaddrinfo_t handle,
                                          I_32 * length);
  /** see @ref hysock.c::hysock_getaddrinfo_name "hysock_getaddrinfo_name"*/
  I_32 (*sock_getaddrinfo_name) (struct HyPortLibrary * portLibrary,
                                        hyaddrinfo_t handle, char *name,
                                        int index);
  /** see @ref hysock.c::hysock_getnameinfo "hysock_getnameinfo"*/
  I_32 (*sock_getnameinfo) (struct HyPortLibrary * portLibrary,
                                  hysockaddr_t in_addr, I_32 sockaddr_size,
                                  char *name, I_32 name_length, int flags);
  /** see @ref hysock.c::hysock_ipv6_mreq_init "hysock_ipv6_mreq_init"*/
  I_32 (*sock_ipv6_mreq_init) (struct HyPortLibrary * portLibrary,
                                      hyipv6_mreq_t handle,
                                      U_8 * ipmcast_addr,
                                      U_32 ipv6mr_interface);
  /** see @ref hysock.c::hysock_setopt_ipv6_mreq "hysock_setopt_ipv6_mreq"*/
  I_32 (*sock_setopt_ipv6_mreq) (struct HyPortLibrary * portLibrary,
                                        hysocket_t socketP, I_32 optlevel,
                                        I_32 optname, hyipv6_mreq_t optval);
  /** see @ref hysock.c::hysock_sockaddr_address6 "hysock_sockaddr_address6"*/
  I_32 (*sock_sockaddr_address6) (struct HyPortLibrary * portLibrary,
                                        hysockaddr_t handle, U_8 * address,
                                        U_32 * length, U_32 * scope_id);
  /** see @ref hysock.c::hysock_sockaddr_family "hysock_sockaddr_family"*/
  I_32 (*sock_sockaddr_family) (struct HyPortLibrary * portLibrary,
                                      I_16 * family, hysockaddr_t handle);
  /** see @ref hysock.c::hysock_sockaddr_init6 "hysock_sockaddr_init6"*/
  I_32 (*sock_sockaddr_init6) (struct HyPortLibrary * portLibrary,
                                      hysockaddr_t handle, U_8 * addr,
                                      I_32 addrlength, I_16 family,
                                      U_16 nPort, U_32 flowinfo,
                                      U_32 scope_id, hysocket_t sock);
  /** see @ref hysock.c::hysock_socketIsValid "hysock_socketIsValid"*/
  I_32 (*sock_socketIsValid) (struct HyPortLibrary * portLibrary,
                                    hysocket_t handle);
  /** see @ref hysock.c::hysock_select_read "hysock_select_read"*/
  I_32 (*sock_select_read) (struct HyPortLibrary * portLibrary,
                                  hysocket_t hysocketP, I_32 secTime,
                                  I_32 uSecTime, BOOLEAN accept);
  /** see @ref hysock.c::hysock_set_nonblocking "hysock_set_nonblocking"*/
  I_32 (*sock_set_nonblocking) (struct HyPortLibrary * portLibrary,
                                      hysocket_t socketP,
                                      BOOLEAN nonblocking);
  /** see @ref hysock.c::hysock_error_message "hysock_error_message"*/
  const char *(*sock_error_message) (struct HyPortLibrary *
                                            portLibrary);
  /** see @ref hysock.c::hysock_get_network_interfaces "hysock_get_network_interfaces"*/
  I_32 (*sock_get_network_interfaces) (struct HyPortLibrary *
                                              portLibrary,
  struct
    hyNetworkInterfaceArray_struct
    * array,
    BOOLEAN preferIPv4Stack);
  /** see @ref hysock.c::hysock_free_network_interface_struct "hysock_free_network_interface_struct"*/
  I_32 (*sock_free_network_interface_struct) (struct HyPortLibrary *
                                                    portLibrary,
  struct
    hyNetworkInterfaceArray_struct
    * array);
  /** see @ref hysock.c::hysock_connect_with_timeout "hysock_connect_with_timeout"*/
  I_32 (*sock_connect_with_timeout) (struct HyPortLibrary *
                                            portLibrary, hysocket_t sock,
                                            hysockaddr_t addr, U_32 timeout,
                                            U_32 step, U_8 ** context);
  /** see @ref hystr.c::hystr_ftime "hystr_ftime"*/
  U_32 (*str_ftime) (struct HyPortLibrary * portLibrary, char *buf,
                            U_32 bufLen, const char *format);
  /** see @ref hymmap.c::hymmap_startup "hymmap_startup"*/
  I_32 (*mmap_startup) (struct HyPortLibrary * portLibrary);
  /** see @ref hymmap.c::hymmap_shutdown "hymmap_shutdown"*/
  void (*mmap_shutdown) (struct HyPortLibrary * portLibrary);
  /** see @ref hymmap.c::hymmap_capabilities "hymmap_capabilities"*/
  I_32 (*mmap_capabilities) (struct HyPortLibrary * portLibrary);
  /** see @ref hymmap.c::hymmap_map_file "hymmap_map_file"*/
  void *(*mmap_map_file) (struct HyPortLibrary * portLibrary,
                                const char *path, void **handle);
  /** see @ref hymmap.c::hymmap_unmap_file "hymmap_unmap_file"*/
  void (*mmap_unmap_file) (struct HyPortLibrary * portLibrary,
                                  void *handle);
  /** see @ref hyshsem.c::hyshsem_startup "hyshsem_startup"*/
  I_32 (*shsem_startup) (struct HyPortLibrary * portLibrary);
  /** see @ref hyshsem.c::hyshsem_shutdown "hyshsem_shutdown"*/
  void (*shsem_shutdown) (struct HyPortLibrary * portLibrary);
  /** see @ref hyshsem.c::hyshsem_open "hyshsem_open"*/
  IDATA (*shsem_open) (struct HyPortLibrary * portLibrary,
  struct hyshsem_handle ** handle,
    const char *semname, int setSize,
    int permission, UDATA flags);
  /** see @ref hyshsem.c::hyshsem_post "hyshsem_post"*/
  IDATA (*shsem_post) (struct HyPortLibrary * portLibrary,
  struct hyshsem_handle * handle, UDATA semset,
    UDATA flag);
  /** see @ref hyshsem.c::hyshsem_wait "hyshsem_wait"*/
  IDATA (*shsem_wait) (struct HyPortLibrary * portLibrary,
  struct hyshsem_handle * handle, UDATA semset,
    UDATA flag);
  /** see @ref hyshsem.c::hyshsem_getVal "hyshsem_getVal"*/
  IDATA (*shsem_getVal) (struct HyPortLibrary * portLibrary,
  struct hyshsem_handle * handle,
    UDATA semset);
  /** see @ref hyshsem.c::hyshsem_setVal "hyshsem_setVal"*/
  IDATA (*shsem_setVal) (struct HyPortLibrary * portLibrary,
  struct hyshsem_handle * handle,
    UDATA semset, IDATA value);
  /** see @ref hyshsem.c::hyshsem_close "hyshsem_close"*/
  void (*shsem_close) (struct HyPortLibrary * portLibrary,
  struct hyshsem_handle ** handle);
  /** see @ref hyshsem.c::hyshsem_destroy "hyshsem_destroy"*/
  IDATA (*shsem_destroy) (struct HyPortLibrary * portLibrary,
  struct hyshsem_handle ** handle);
  /** see @ref hyshmem.c::hyshmem_startup "hyshmem_startup"*/
  I_32 (*shmem_startup) (struct HyPortLibrary * portLibrary);
  /** see @ref hyshmem.c::hyshmem_shutdown "hyshmem_shutdown"*/
  void (*shmem_shutdown) (struct HyPortLibrary * portLibrary);
  /** see @ref hyshmem.c::hyshmem_open "hyshmem_open"*/
  IDATA (*shmem_open) (struct HyPortLibrary * portLibrary,
  struct hyshmem_handle ** handle,
    const char *rootname, I_32 size, I_32 perm, UDATA flags);
  /** see @ref hyshmem.c::hyshmem_attach "hyshmem_attach"*/
  void *(*shmem_attach) (struct HyPortLibrary * portLibrary,
  struct hyshmem_handle * handle);
  /** see @ref hyshmem.c::hyshmem_detach "hyshmem_detach"*/
  IDATA (*shmem_detach) (struct HyPortLibrary * portLibrary,
  struct hyshmem_handle ** handle);
  /** see @ref hyshmem.c::hyshmem_close "hyshmem_close"*/
  void (*shmem_close) (struct HyPortLibrary * portLibrary,
  struct hyshmem_handle ** handle);
  /** see @ref hyshmem.c::hyshmem_destroy "hyshmem_destroy"*/
  IDATA (*shmem_destroy) (struct HyPortLibrary * portLibrary,
  struct hyshmem_handle ** handle);
  /** see @ref hyshmem.c::hyshmem_findfirst "hyshmem_findfirst"*/
  UDATA (*shmem_findfirst) (struct HyPortLibrary * portLibrary,
                                  char *resultbuf);
  /** see @ref hyshmem.c::hyshmem_findnext "hyshmem_findnext"*/
  I_32 (*shmem_findnext) (struct HyPortLibrary * portLibrary,
                                UDATA findhandle, char *resultbuf);
  /** see @ref hyshmem.c::hyshmem_findclose "hyshmem_findclose"*/
  void (*shmem_findclose) (struct HyPortLibrary * portLibrary,
                                  UDATA findhandle);
  /** see @ref hyshmem.c::hyshmem_stat "hyshmem_stat"*/
  UDATA (*shmem_stat) (struct HyPortLibrary * portLibrary,
                              const char *name,
  struct HyPortShmemStatistic * statbuf);
  /** see @ref hysysinfo.c::hysysinfo_get_processing_capacity "hysysinfo_get_processing_capacity"*/
  UDATA (*sysinfo_get_processing_capacity) (struct HyPortLibrary *
                                                  portLibrary);
  /** see @ref hyfile.c::hybuf_write_text "hybuf_write_text"*/
  char *(*buf_write_text) (struct HyPortLibrary * portLibrary,
                                 const char *buf, IDATA nbytes);
  /** see @ref hyport.c::hyport_get_thread_library "hyport_get_thread_library" */
  HyThreadLibrary * (*port_get_thread_library) (struct HyPortLibrary * portLibrary);
  void  (*sock_fdset_zero)(struct HyPortLibrary *portLibrary, hyfdset_t hyfdset) ;
  void  (*sock_fdset_set)(struct HyPortLibrary *portLibrary, hysocket_t aSocket, hyfdset_t hyfdset) ;
  /** see @ref hymem.c::hyprocess_create "hyprocess_create"*/
  IDATA (*process_create) (struct HyPortLibrary * portLibrary,
		  char *command[], UDATA commandLength, char *env[], UDATA envSize,
		  char *dir, U_32 options, IDATA fdInput, IDATA fdOutput, IDATA fdError,
		  struct HyProcessHandleStruct *processHandle);
  /** see @ref hymem.c::hyprocess_getStream "hyprocess_getStream"*/
  IDATA (*process_getStream) (struct HyPortLibrary * portLibrary,
		  HyProcessHandle processHandle, UDATA streamFlag, IDATA *stream);
  /** see @ref hymem.c::hyprocess_waitfor "hyprocess_waitfor"*/
  IDATA (*process_waitfor) (struct HyPortLibrary * portLibrary,
		  HyProcessHandle processHandle);
  /** see @ref hymem.c::hyprocess_close "hyprocess_close"*/
  IDATA (*process_close) (struct HyPortLibrary * portLibrary,
		  HyProcessHandle processHandle, U_32 options);
  /** see @ref hymem.c::hyprocess_isComplete "hyprocess_isComplete"*/
  IDATA (*process_isComplete) (struct HyPortLibrary * portLibrary,
		  HyProcessHandle processHandle);
  /** see @ref hymem.c::hyprocess_terminate "hyprocess_terminate"*/
  IDATA (*process_terminate) (struct HyPortLibrary * portLibrary,
		  HyProcessHandle processHandle);
  /** see @ref hymem.c::hyprocess_get_exitCode "hyprocess_get_exitCode"*/
  IDATA (*process_get_exitCode) (struct HyPortLibrary * portLibrary,
		  HyProcessHandle processHandle);
  /** see @ref hymem.c::hyprocess_get_available "hyprocess_get_available"*/
  IDATA (*process_get_available) (struct HyPortLibrary * portLibrary,
		  HyProcessHandle processHandle, UDATA flag);
  char _hypadding039C[4];       /* 4 bytes of automatic padding */
} HyPortLibrary;

#define HYPORT_MAJOR_VERSION_NUMBER  5
#define HYPORT_MINOR_VERSION_NUMBER  1
#define HYPORT_CAPABILITY_BASE  0
#define HYPORT_CAPABILITY_STANDARD  1
#define HYPORT_CAPABILITY_FILESYSTEM  2
#define HYPORT_CAPABILITY_SOCKETS  4
#define HYPORT_CAPABILITY_LARGE_PAGE_SUPPORT  8
#define HYPORT_CAPABILITY_MASK ((U_64)(HYPORT_CAPABILITY_STANDARD | HYPORT_CAPABILITY_FILESYSTEM | HYPORT_CAPABILITY_SOCKETS | HYPORT_CAPABILITY_LARGE_PAGE_SUPPORT))
#define HYPORT_SET_VERSION(portLibraryVersion, capabilityMask) \
  (portLibraryVersion)->majorVersionNumber = HYPORT_MAJOR_VERSION_NUMBER; \
  (portLibraryVersion)->minorVersionNumber = HYPORT_MINOR_VERSION_NUMBER; \
  (portLibraryVersion)->capabilities = (capabilityMask)
#define HYPORT_SET_VERSION_DEFAULT(portLibraryVersion) \
  (portLibraryVersion)->majorVersionNumber = HYPORT_MAJOR_VERSION_NUMBER; \
  (portLibraryVersion)->minorVersionNumber = HYPORT_MINOR_VERSION_NUMBER; \
  (portLibraryVersion)->capabilities = HYPORT_CAPABILITY_MASK
/**
 * @name Port library startup and shutdown functions
 * @anchor PortStartup
 * Create, initialize, startup and shutdow the port library
 * @{
 */
/** Standard startup and shutdown (port library allocated on stack or by application)  */
extern HY_CFUNC I_32 hyport_create_library (struct HyPortLibrary
                                                   *portLibrary,
                                                   struct HyPortLibraryVersion
                                                   *version, UDATA size);
extern HY_CFUNC I_32 hyport_init_library (struct HyPortLibrary
                                                 *portLibrary,
                                                 struct HyPortLibraryVersion
                                                 *version, UDATA size);
extern HY_CFUNC I_32 hyport_shutdown_library (struct HyPortLibrary
                                                     *portLibrary);
extern HY_CFUNC I_32 hyport_startup_library (struct HyPortLibrary
                                                    *portLibrary);
/** Port library self allocation routines */
extern HY_CFUNC I_32 hyport_allocate_library (struct
                                                     HyPortLibraryVersion
                                                     *expectedVersion,
                                                     struct HyPortLibrary
                                                     **portLibrary);

/** @} */
/**
 * @name Port library version and compatibility queries
 * @anchor PortVersionControl
 * Determine port library compatibility and version.
 * @{
 */
extern HY_CFUNC UDATA hyport_getSize (struct HyPortLibraryVersion
                                             *version);
extern HY_CFUNC I_32 hyport_getVersion (struct HyPortLibrary
                                               *portLibrary,
                                               struct HyPortLibraryVersion
                                               *version);
extern HY_CFUNC I_32 hyport_isCompatible (struct HyPortLibraryVersion
                                                 *expectedVersion);


#if !defined(HYPORT_LIBRARY_DEFINE)
#define hyport_get_thread_library() privatePortLibrary->port_get_thread_library(privatePortLibrary)
#define hyport_shutdown_library() privatePortLibrary->port_shutdown_library(privatePortLibrary)
#define hyport_isFunctionOverridden(param1) privatePortLibrary->port_isFunctionOverridden(privatePortLibrary,param1)
#define hyport_tls_free() privatePortLibrary->port_tls_free(privatePortLibrary)
#define hyerror_startup() privatePortLibrary->error_startup(privatePortLibrary)
#define hyerror_shutdown() privatePortLibrary->error_shutdown(privatePortLibrary)
#define hyerror_last_error_number() privatePortLibrary->error_last_error_number(privatePortLibrary)
#define hyerror_last_error_message() privatePortLibrary->error_last_error_message(privatePortLibrary)
#define hyerror_set_last_error(param1,param2) privatePortLibrary->error_set_last_error(privatePortLibrary,param1,param2)
#define hyerror_set_last_error_with_message(param1,param2) privatePortLibrary->error_set_last_error_with_message(privatePortLibrary,param1,param2)
#define hytime_startup() privatePortLibrary->time_startup(privatePortLibrary)
#define hytime_shutdown() privatePortLibrary->time_shutdown(privatePortLibrary)
#define hytime_msec_clock() privatePortLibrary->time_msec_clock(privatePortLibrary)
#define hytime_usec_clock() privatePortLibrary->time_usec_clock(privatePortLibrary)
#define hytime_current_time_millis() privatePortLibrary->time_current_time_millis(privatePortLibrary)
#define hytime_hires_clock() privatePortLibrary->time_hires_clock(privatePortLibrary)
#define hytime_hires_frequency() privatePortLibrary->time_hires_frequency(privatePortLibrary)
#define hytime_hires_delta(param1,param2,param3) privatePortLibrary->time_hires_delta(privatePortLibrary,param1,param2,param3)
#define hysysinfo_startup() privatePortLibrary->sysinfo_startup(privatePortLibrary)
#define hysysinfo_shutdown() privatePortLibrary->sysinfo_shutdown(privatePortLibrary)
#define hysysinfo_get_pid() privatePortLibrary->sysinfo_get_pid(privatePortLibrary)
#define hysysinfo_get_physical_memory() privatePortLibrary->sysinfo_get_physical_memory(privatePortLibrary)
#define hysysinfo_get_OS_version() privatePortLibrary->sysinfo_get_OS_version(privatePortLibrary)
#define hysysinfo_get_env(param1,param2,param3) privatePortLibrary->sysinfo_get_env(privatePortLibrary,param1,param2,param3)
#define hysysinfo_get_CPU_architecture() privatePortLibrary->sysinfo_get_CPU_architecture(privatePortLibrary)
#define hysysinfo_get_OS_type() privatePortLibrary->sysinfo_get_OS_type(privatePortLibrary)
#define hysysinfo_get_classpathSeparator() privatePortLibrary->sysinfo_get_classpathSeparator(privatePortLibrary)
#define hysysinfo_get_executable_name(param1,param2) privatePortLibrary->sysinfo_get_executable_name(privatePortLibrary,param1,param2)
#define hysysinfo_get_number_CPUs() privatePortLibrary->sysinfo_get_number_CPUs(privatePortLibrary)
#define hysysinfo_get_username(param1,param2) privatePortLibrary->sysinfo_get_username(privatePortLibrary,param1,param2)
#define hyfile_startup() privatePortLibrary->file_startup(privatePortLibrary)
#define hyfile_shutdown() privatePortLibrary->file_shutdown(privatePortLibrary)
#define hyfile_write(param1,param2,param3) privatePortLibrary->file_write(privatePortLibrary,param1,param2,param3)
#define hyfile_write_text(param1,param2,param3) privatePortLibrary->file_write_text(privatePortLibrary,param1,param2,param3)
#define hybuf_write_text(param1,param2) privatePortLibrary->buf_write_text(privatePortLibrary,param1,param2)
#define hyfile_vprintf(param1,param2,param3) privatePortLibrary->file_vprintf(privatePortLibrary,param1,param2,param3)
#define hyfile_printf privatePortLibrary->file_printf
#define hyfile_open(param1,param2,param3) privatePortLibrary->file_open(privatePortLibrary,param1,param2,param3)
#define hyfile_close(param1) privatePortLibrary->file_close(privatePortLibrary,param1)
#define hyfile_seek(param1,param2,param3) privatePortLibrary->file_seek(privatePortLibrary,param1,param2,param3)
#define hyfile_read(param1,param2,param3) privatePortLibrary->file_read(privatePortLibrary,param1,param2,param3)
#define hyfile_unlink(param1) privatePortLibrary->file_unlink(privatePortLibrary,param1)
#define hyfile_attr(param1) privatePortLibrary->file_attr(privatePortLibrary,param1)
#define hyfile_lastmod(param1) privatePortLibrary->file_lastmod(privatePortLibrary,param1)
#define hyfile_length(param1) privatePortLibrary->file_length(privatePortLibrary,param1)
#define hyfile_set_length(param1,param2) privatePortLibrary->file_set_length(privatePortLibrary,param1,param2)
#define hyfile_sync(param1) privatePortLibrary->file_sync(privatePortLibrary,param1)
#define hysl_startup() privatePortLibrary->sl_startup(privatePortLibrary)
#define hysl_shutdown() privatePortLibrary->sl_shutdown(privatePortLibrary)
#define hysl_close_shared_library(param1) privatePortLibrary->sl_close_shared_library(privatePortLibrary,param1)
#define hysl_open_shared_library(param1,param2,param3) privatePortLibrary->sl_open_shared_library(privatePortLibrary,param1,param2,param3)
#define hysl_lookup_name(param1,param2,param3,param4) privatePortLibrary->sl_lookup_name(privatePortLibrary,param1,param2,param3,param4)
#define hytty_startup() privatePortLibrary->tty_startup(privatePortLibrary)
#define hytty_shutdown() privatePortLibrary->tty_shutdown(privatePortLibrary)
#define hytty_printf privatePortLibrary->tty_printf
#define hytty_vprintf(param1,param2) privatePortLibrary->tty_vprintf(privatePortLibrary,param1,param2)
#define hytty_get_chars(param1,param2) privatePortLibrary->tty_get_chars(privatePortLibrary,param1,param2)
#define hytty_err_printf privatePortLibrary->tty_err_printf
#define hytty_err_vprintf(param1,param2) privatePortLibrary->tty_err_vprintf(privatePortLibrary,param1,param2)
#define hytty_available() privatePortLibrary->tty_available(privatePortLibrary)
#define hymem_startup(param1) privatePortLibrary->mem_startup(privatePortLibrary,param1)
#define hymem_shutdown() privatePortLibrary->mem_shutdown(privatePortLibrary)
#define hymem_allocate_memory(param1) privatePortLibrary->mem_allocate_memory(privatePortLibrary,param1)
#define hymem_allocate_memory_callSite(param1,param2) privatePortLibrary->mem_allocate_memory_callSite(privatePortLibrary,param1,param2)
#define hymem_free_memory(param1) privatePortLibrary->mem_free_memory(privatePortLibrary,param1)
#define hymem_reallocate_memory(param1,param2) privatePortLibrary->mem_reallocate_memory(privatePortLibrary,param1,param2)
#define hycpu_startup() privatePortLibrary->cpu_startup(privatePortLibrary)
#define hycpu_shutdown() privatePortLibrary->cpu_shutdown(privatePortLibrary)
#define hycpu_flush_icache(param1,param2) privatePortLibrary->cpu_flush_icache(privatePortLibrary,param1,param2)
#define hyvmem_startup() privatePortLibrary->vmem_startup(privatePortLibrary)
#define hyvmem_shutdown() privatePortLibrary->vmem_shutdown(privatePortLibrary)
#define hyvmem_commit_memory(param1,param2,param3) privatePortLibrary->vmem_commit_memory(privatePortLibrary,param1,param2,param3)
#define hyvmem_decommit_memory(param1,param2,param3) privatePortLibrary->vmem_decommit_memory(privatePortLibrary,param1,param2,param3)
#define hyvmem_free_memory(param1,param2,param3) privatePortLibrary->vmem_free_memory(privatePortLibrary,param1,param2,param3)
#define hyvmem_reserve_memory(param1,param2,param3,param4,param5) privatePortLibrary->vmem_reserve_memory(privatePortLibrary,param1,param2,param3,param4,param5)
#define hyvmem_supported_page_sizes() privatePortLibrary->vmem_supported_page_sizes(privatePortLibrary)
#define hysock_startup() privatePortLibrary->sock_startup(privatePortLibrary)
#define hysock_shutdown() privatePortLibrary->sock_shutdown(privatePortLibrary)
#define hysock_htons(param1) privatePortLibrary->sock_htons(privatePortLibrary,param1)
#define hysock_write(param1,param2,param3,param4) privatePortLibrary->sock_write(privatePortLibrary,param1,param2,param3,param4)
#define hysock_sockaddr(param1,param2,param3) privatePortLibrary->sock_sockaddr(privatePortLibrary,param1,param2,param3)
#define hysock_read(param1,param2,param3,param4) privatePortLibrary->sock_read(privatePortLibrary,param1,param2,param3,param4)
#define hysock_socket(param1,param2,param3,param4) privatePortLibrary->sock_socket(privatePortLibrary,param1,param2,param3,param4)
#define hysock_close(param1) privatePortLibrary->sock_close(privatePortLibrary,param1)
#define hysock_connect(param1,param2) privatePortLibrary->sock_connect(privatePortLibrary,param1,param2)
#define hysock_inetaddr(param1,param2) privatePortLibrary->sock_inetaddr(privatePortLibrary,param1,param2)
#define hysock_gethostbyname(param1,param2) privatePortLibrary->sock_gethostbyname(privatePortLibrary,param1,param2)
#define hysock_hostent_addrlist(param1,param2) privatePortLibrary->sock_hostent_addrlist(privatePortLibrary,param1,param2)
#define hysock_sockaddr_init(param1,param2,param3,param4) privatePortLibrary->sock_sockaddr_init(privatePortLibrary,param1,param2,param3,param4)
#define hysock_linger_init(param1,param2,param3) privatePortLibrary->sock_linger_init(privatePortLibrary,param1,param2,param3)
#define hysock_setopt_linger(param1,param2,param3,param4) privatePortLibrary->sock_setopt_linger(privatePortLibrary,param1,param2,param3,param4)
#define hygp_startup() privatePortLibrary->gp_startup(privatePortLibrary)
#define hygp_shutdown() privatePortLibrary->gp_shutdown(privatePortLibrary)
#define hygp_protect(param1,param2) privatePortLibrary->gp_protect(privatePortLibrary,param1,param2)
#define hygp_register_handler(param1,param2) privatePortLibrary->gp_register_handler(privatePortLibrary,param1,param2)
#define hygp_info(param1,param2,param3,param4,param5) privatePortLibrary->gp_info(privatePortLibrary,param1,param2,param3,param4,param5)
#define hygp_info_count(param1,param2) privatePortLibrary->gp_info_count(privatePortLibrary,param1,param2)
#define hygp_handler_function() privatePortLibrary->gp_handler_function(privatePortLibrary)
#define hystr_startup() privatePortLibrary->str_startup(privatePortLibrary)
#define hystr_shutdown() privatePortLibrary->str_shutdown(privatePortLibrary)
#define hystr_printf privatePortLibrary->str_printf
#define hystr_vprintf(param1,param2,param3,param4) privatePortLibrary->str_vprintf(privatePortLibrary,param1,param2,param3,param4)
#define hyexit_startup() privatePortLibrary->exit_startup(privatePortLibrary)
#define hyexit_shutdown() privatePortLibrary->exit_shutdown(privatePortLibrary)
#define hyexit_get_exit_code() privatePortLibrary->exit_get_exit_code(privatePortLibrary)
#define hyexit_shutdown_and_exit(param1) privatePortLibrary->exit_shutdown_and_exit(privatePortLibrary,param1)
#define hydump_create(param1,param2,param3) privatePortLibrary->dump_create(privatePortLibrary,param1,param2,param3)
#define hynls_startup() privatePortLibrary->nls_startup(privatePortLibrary)
#define hynls_shutdown() privatePortLibrary->nls_shutdown(privatePortLibrary)
#define hynls_set_catalog(param1,param2,param3,param4) privatePortLibrary->nls_set_catalog(privatePortLibrary,param1,param2,param3,param4)
#define hynls_set_locale(param1,param2,param3) privatePortLibrary->nls_set_locale(privatePortLibrary,param1,param2,param3)
#define hynls_get_language() privatePortLibrary->nls_get_language(privatePortLibrary)
#define hynls_get_region() privatePortLibrary->nls_get_region(privatePortLibrary)
#define hynls_get_variant() privatePortLibrary->nls_get_variant(privatePortLibrary)
#define hynls_printf privatePortLibrary->nls_printf
#define hynls_vprintf(param1,param2,param3,param4) privatePortLibrary->nls_vprintf(privatePortLibrary,param1,param2,param3,param4)
#define hynls_lookup_message(param1,param2,param3,param4) privatePortLibrary->nls_lookup_message(privatePortLibrary,param1,param2,param3,param4)
#define hyipcmutex_startup() privatePortLibrary->ipcmutex_startup(privatePortLibrary)
#define hyipcmutex_shutdown() privatePortLibrary->ipcmutex_shutdown(privatePortLibrary)
#define hyipcmutex_acquire(param1) privatePortLibrary->ipcmutex_acquire(privatePortLibrary,param1)
#define hyipcmutex_release(param1) privatePortLibrary->ipcmutex_release(privatePortLibrary,param1)
#define hyport_control(param1,param2) privatePortLibrary->port_control(privatePortLibrary,param1,param2)
#define hysig_startup() privatePortLibrary->sig_startup(privatePortLibrary)
#define hysig_shutdown() privatePortLibrary->sig_shutdown(privatePortLibrary)
#define hysig_protect(param1,param2,param3,param4,param5,param6) privatePortLibrary->sig_protect(privatePortLibrary,param1,param2,param3,param4,param5,param6)
#define hysig_can_protect(param1) privatePortLibrary->sig_can_protect(privatePortLibrary,param1)
#define hysig_set_async_signal_handler(param1,param2,param3) privatePortLibrary->sig_set_async_signal_handler(privatePortLibrary,param1,param2,param3)
#define hysig_info(param1,param2,param3,param4,param5) privatePortLibrary->sig_info(privatePortLibrary,param1,param2,param3,param4,param5)
#define hysig_info_count(param1,param2) privatePortLibrary->sig_info_count(privatePortLibrary,param1,param2)
#define hysig_set_options(param1) privatePortLibrary->sig_set_options(privatePortLibrary,param1)
#define hysig_get_options() privatePortLibrary->sig_get_options(privatePortLibrary)
#define hysysinfo_DLPAR_enabled() privatePortLibrary->sysinfo_DLPAR_enabled(privatePortLibrary)
#define hysysinfo_DLPAR_max_CPUs() privatePortLibrary->sysinfo_DLPAR_max_CPUs(privatePortLibrary)
#define hysysinfo_weak_memory_consistency() privatePortLibrary->sysinfo_weak_memory_consistency(privatePortLibrary)
#define hyfile_read_text(param1,param2,param3) privatePortLibrary->file_read_text(privatePortLibrary,param1,param2,param3)
#define hyfile_mkdir(param1) privatePortLibrary->file_mkdir(privatePortLibrary,param1)
#define hyfile_move(param1,param2) privatePortLibrary->file_move(privatePortLibrary,param1,param2)
#define hyfile_unlinkdir(param1) privatePortLibrary->file_unlinkdir(privatePortLibrary,param1)
#define hyfile_findfirst(param1,param2) privatePortLibrary->file_findfirst(privatePortLibrary,param1,param2)
#define hyfile_findnext(param1,param2) privatePortLibrary->file_findnext(privatePortLibrary,param1,param2)
#define hyfile_findclose(param1) privatePortLibrary->file_findclose(privatePortLibrary,param1)
#define hyfile_error_message() privatePortLibrary->file_error_message(privatePortLibrary)
#define hysock_htonl(param1) privatePortLibrary->sock_htonl(privatePortLibrary,param1)
#define hysock_bind(param1,param2) privatePortLibrary->sock_bind(privatePortLibrary,param1,param2)
#define hysock_accept(param1,param2,param3) privatePortLibrary->sock_accept(privatePortLibrary,param1,param2,param3)
#define hysock_shutdown_input(param1) privatePortLibrary->sock_shutdown_input(privatePortLibrary,param1)
#define hysock_shutdown_output(param1) privatePortLibrary->sock_shutdown_output(privatePortLibrary,param1)
#define hysock_listen(param1,param2) privatePortLibrary->sock_listen(privatePortLibrary,param1,param2)
#define hysock_ntohl(param1) privatePortLibrary->sock_ntohl(privatePortLibrary,param1)
#define hysock_ntohs(param1) privatePortLibrary->sock_ntohs(privatePortLibrary,param1)
#define hysock_getpeername(param1,param2) privatePortLibrary->sock_getpeername(privatePortLibrary,param1,param2)
#define hysock_getsockname(param1,param2) privatePortLibrary->sock_getsockname(privatePortLibrary,param1,param2)
#define hysock_readfrom(param1,param2,param3,param4,param5) privatePortLibrary->sock_readfrom(privatePortLibrary,param1,param2,param3,param4,param5)
#define hysock_select(param1,param2,param3,param4,param5) privatePortLibrary->sock_select(privatePortLibrary,param1,param2,param3,param4,param5)
#define hysock_writeto(param1,param2,param3,param4,param5) privatePortLibrary->sock_writeto(privatePortLibrary,param1,param2,param3,param4,param5)
#define hysock_inetntoa(param1,param2) privatePortLibrary->sock_inetntoa(privatePortLibrary,param1,param2)
#define hysock_gethostbyaddr(param1,param2,param3,param4) privatePortLibrary->sock_gethostbyaddr(privatePortLibrary,param1,param2,param3,param4)
#define hysock_gethostname(param1,param2) privatePortLibrary->sock_gethostname(privatePortLibrary,param1,param2)
#define hysock_hostent_aliaslist(param1,param2) privatePortLibrary->sock_hostent_aliaslist(privatePortLibrary,param1,param2)
#define hysock_hostent_hostname(param1,param2) privatePortLibrary->sock_hostent_hostname(privatePortLibrary,param1,param2)
#define hysock_sockaddr_port(param1) privatePortLibrary->sock_sockaddr_port(privatePortLibrary,param1)
#define hysock_sockaddr_address(param1) privatePortLibrary->sock_sockaddr_address(privatePortLibrary,param1)
#define hysock_fdset_init(param1) privatePortLibrary->sock_fdset_init(privatePortLibrary,param1)
#define hysock_fdset_size(param1) privatePortLibrary->sock_fdset_size(privatePortLibrary,param1)
#define hysock_timeval_init(param1,param2,param3) privatePortLibrary->sock_timeval_init(privatePortLibrary,param1,param2,param3)
#define hysock_getopt_int(param1,param2,param3,param4) privatePortLibrary->sock_getopt_int(privatePortLibrary,param1,param2,param3,param4)
#define hysock_setopt_int(param1,param2,param3,param4) privatePortLibrary->sock_setopt_int(privatePortLibrary,param1,param2,param3,param4)
#define hysock_getopt_bool(param1,param2,param3,param4) privatePortLibrary->sock_getopt_bool(privatePortLibrary,param1,param2,param3,param4)
#define hysock_setopt_bool(param1,param2,param3,param4) privatePortLibrary->sock_setopt_bool(privatePortLibrary,param1,param2,param3,param4)
#define hysock_getopt_byte(param1,param2,param3,param4) privatePortLibrary->sock_getopt_byte(privatePortLibrary,param1,param2,param3,param4)
#define hysock_setopt_byte(param1,param2,param3,param4) privatePortLibrary->sock_setopt_byte(privatePortLibrary,param1,param2,param3,param4)
#define hysock_getopt_linger(param1,param2,param3,param4) privatePortLibrary->sock_getopt_linger(privatePortLibrary,param1,param2,param3,param4)
#define hysock_getopt_sockaddr(param1,param2,param3,param4) privatePortLibrary->sock_getopt_sockaddr(privatePortLibrary,param1,param2,param3,param4)
#define hysock_setopt_sockaddr(param1,param2,param3,param4) privatePortLibrary->sock_setopt_sockaddr(privatePortLibrary,param1,param2,param3,param4)
#define hysock_setopt_ipmreq(param1,param2,param3,param4) privatePortLibrary->sock_setopt_ipmreq(privatePortLibrary,param1,param2,param3,param4)
#define hysock_linger_enabled(param1,param2) privatePortLibrary->sock_linger_enabled(privatePortLibrary,param1,param2)
#define hysock_linger_linger(param1,param2) privatePortLibrary->sock_linger_linger(privatePortLibrary,param1,param2)
#define hysock_ipmreq_init(param1,param2,param3) privatePortLibrary->sock_ipmreq_init(privatePortLibrary,param1,param2,param3)
#define hysock_setflag(param1,param2) privatePortLibrary->sock_setflag(privatePortLibrary,param1,param2)
#define hysock_freeaddrinfo(param1) privatePortLibrary->sock_freeaddrinfo(privatePortLibrary,param1)
#define hysock_getaddrinfo(param1,param2,param3) privatePortLibrary->sock_getaddrinfo(privatePortLibrary,param1,param2,param3)
#define hysock_getaddrinfo_address(param1,param2,param3,param4) privatePortLibrary->sock_getaddrinfo_address(privatePortLibrary,param1,param2,param3,param4)
#define hysock_getaddrinfo_create_hints(param1,param2,param3,param4,param5) privatePortLibrary->sock_getaddrinfo_create_hints(privatePortLibrary,param1,param2,param3,param4,param5)
#define hysock_getaddrinfo_family(param1,param2,param3) privatePortLibrary->sock_getaddrinfo_family(privatePortLibrary,param1,param2,param3)
#define hysock_getaddrinfo_length(param1,param2) privatePortLibrary->sock_getaddrinfo_length(privatePortLibrary,param1,param2)
#define hysock_getaddrinfo_name(param1,param2,param3) privatePortLibrary->sock_getaddrinfo_name(privatePortLibrary,param1,param2,param3)
#define hysock_getnameinfo(param1,param2,param3,param4,param5) privatePortLibrary->sock_getnameinfo(privatePortLibrary,param1,param2,param3,param4,param5)
#define hysock_ipv6_mreq_init(param1,param2,param3) privatePortLibrary->sock_ipv6_mreq_init(privatePortLibrary,param1,param2,param3)
#define hysock_setopt_ipv6_mreq(param1,param2,param3,param4) privatePortLibrary->sock_setopt_ipv6_mreq(privatePortLibrary,param1,param2,param3,param4)
#define hysock_sockaddr_address6(param1,param2,param3,param4) privatePortLibrary->sock_sockaddr_address6(privatePortLibrary,param1,param2,param3,param4)
#define hysock_sockaddr_family(param1,param2) privatePortLibrary->sock_sockaddr_family(privatePortLibrary,param1,param2)
#define hysock_sockaddr_init6(param1,param2,param3,param4,param5,param6,param7,param8) privatePortLibrary->sock_sockaddr_init6(privatePortLibrary,param1,param2,param3,param4,param5,param6,param7,param8)
#define hysock_socketIsValid(param1) privatePortLibrary->sock_socketIsValid(privatePortLibrary,param1)
#define hysock_select_read(param1,param2,param3,param4) privatePortLibrary->sock_select_read(privatePortLibrary,param1,param2,param3,param4)
#define hysock_set_nonblocking(param1,param2) privatePortLibrary->sock_set_nonblocking(privatePortLibrary,param1,param2)
#define hysock_error_message() privatePortLibrary->sock_error_message(privatePortLibrary)
#define hysock_get_network_interfaces(param1,param2) privatePortLibrary->sock_get_network_interfaces(privatePortLibrary,param1,param2)
#define hysock_free_network_interface_struct(param1) privatePortLibrary->sock_free_network_interface_struct(privatePortLibrary,param1)
#define hysock_connect_with_timeout(param1,param2,param3,param4,param5) privatePortLibrary->sock_connect_with_timeout(privatePortLibrary,param1,param2,param3,param4,param5)
#define hystr_ftime(param1,param2,param3) privatePortLibrary->str_ftime(privatePortLibrary,param1,param2,param3)
#define hymmap_startup() privatePortLibrary->mmap_startup(privatePortLibrary)
#define hymmap_shutdown() privatePortLibrary->mmap_shutdown(privatePortLibrary)
#define hymmap_capabilities() privatePortLibrary->mmap_capabilities(privatePortLibrary)
#define hymmap_map_file(param1,param2) privatePortLibrary->mmap_map_file(privatePortLibrary,param1,param2)
#define hymmap_unmap_file(param1) privatePortLibrary->mmap_unmap_file(privatePortLibrary,param1)
#define hyshsem_startup() privatePortLibrary->shsem_startup(privatePortLibrary)
#define hyshsem_shutdown() privatePortLibrary->shsem_shutdown(privatePortLibrary)
#define hyshsem_open(param1,param2,param3,param4,param5) privatePortLibrary->shsem_open(privatePortLibrary,param1,param2,param3,param4,param5)
#define hyshsem_post(param1,param2,param3) privatePortLibrary->shsem_post(privatePortLibrary,param1,param2,param3)
#define hyshsem_wait(param1,param2,param3) privatePortLibrary->shsem_wait(privatePortLibrary,param1,param2,param3)
#define hyshsem_getVal(param1,param2) privatePortLibrary->shsem_getVal(privatePortLibrary,param1,param2)
#define hyshsem_setVal(param1,param2,param3) privatePortLibrary->shsem_setVal(privatePortLibrary,param1,param2,param3)
#define hyshsem_close(param1) privatePortLibrary->shsem_close(privatePortLibrary,param1)
#define hyshsem_destroy(param1) privatePortLibrary->shsem_destroy(privatePortLibrary,param1)
#define hyshmem_startup() privatePortLibrary->shmem_startup(privatePortLibrary)
#define hyshmem_shutdown() privatePortLibrary->shmem_shutdown(privatePortLibrary)
#define hyshmem_open(param1,param2,param3,param4,param5) privatePortLibrary->shmem_open(privatePortLibrary,param1,param2,param3,param4,param5)
#define hyshmem_attach(param1) privatePortLibrary->shmem_attach(privatePortLibrary,param1)
#define hyshmem_detach(param1) privatePortLibrary->shmem_detach(privatePortLibrary,param1)
#define hyshmem_close(param1) privatePortLibrary->shmem_close(privatePortLibrary,param1)
#define hyshmem_destroy(param1) privatePortLibrary->shmem_destroy(privatePortLibrary,param1)
#define hyshmem_findfirst(param1) privatePortLibrary->shmem_findfirst(privatePortLibrary,param1)
#define hyshmem_findnext(param1,param2) privatePortLibrary->shmem_findnext(privatePortLibrary,param1,param2)
#define hyshmem_findclose(param1) privatePortLibrary->shmem_findclose(privatePortLibrary,param1)
#define hyshmem_stat(param1,param2) privatePortLibrary->shmem_stat(privatePortLibrary,param1,param2)
#define hysysinfo_get_processing_capacity() privatePortLibrary->sysinfo_get_processing_capacity(privatePortLibrary)
#undef hynls_lookup_message
#define hynls_lookup_message(param1,param2,param3) privatePortLibrary->nls_lookup_message(privatePortLibrary,param1,param2,param3)
#undef hynls_vprintf
#define hynls_vprintf(param1,param2,param3) privatePortLibrary->hynls_vprintf(privatePortLibrary,param1,param2,param3)
#undef hymem_allocate_memory
#define hymem_allocate_memory(param1) privatePortLibrary->mem_allocate_memory_callSite(privatePortLibrary,param1, HY_GET_CALLSITE())
#undef hymem_allocate_code_memory
#define hymem_allocate_code_memory(param1) privatePortLibrary->mem_allocate_code_memory_callSite(privatePortLibrary,param1, HY_GET_CALLSITE())
#define hysock_fdset_zero(param1) privatePortLibrary->sock_fdset_zero(privatePortLibrary,param1)
#define hysock_fdset_set(param1,param2) privatePortLibrary->sock_fdset_set(privatePortLibrary,param1,param2)
#define hyprocess_create(param1,param2,param3,param4,param5,param6,param7,param8,param9,param10) privatePortLibrary->process_create(privatePortLibrary,param1,param2,param3,param4,param5,param6,param7,param8,param9,param10)
#define hyprocess_getStream(param1,param2,param3) privatePortLibrary->process_getStream(privatePortLibrary,param1,param2,param3)
#define hyprocess_waitfor(param1) privatePortLibrary->process_waitfor(privatePortLibrary,param1)
#define hyprocess_close(param1, param2) privatePortLibrary->process_close(privatePortLibrary,param1, param2)
#define hyprocess_isComplete(param1) privatePortLibrary->process_isComplete(privatePortLibrary,param1)
#define hyprocess_terminate(param1) privatePortLibrary->process_terminate(privatePortLibrary,param1)
#define hyprocess_get_exitCode(param1) privatePortLibrary->process_get_exitCode(privatePortLibrary,param1)
#define hyprocess_get_available(param1) privatePortLibrary->process_get_available(privatePortLibrary,param1)

#endif /* !HYPORT_LIBRARY_DEFINE */

#endif /* hyport_h */
