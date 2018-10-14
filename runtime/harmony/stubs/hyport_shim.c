/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
/* create a level of indirection from the hyport to the j9 port library */
#include <stdarg.h>

#include "hyport.h"
/* To prevent clashes between hyport.h and j9port.h on certain defines undef them here. */
#undef PORT_ACCESS_FROM_ENV
#undef PORT_ACCESS_FROM_JAVAVM
#undef PORT_ACCESS_FROM_VMI
#undef PORT_ACCESS_FROM_PORT
#undef PORTLIB 

#include "hyport_shim.h"
#include "j9port.h"
#include "j9.h"


#define J9SH_SHMEM_CONTROL_DIR_FLAG_TRY_CREATE_DIR 0x1
#define J9SH_SHMEM_CONTROL_DIR_FLAG_DONT_CREATE_BASEDIR 0x2

static omrthread_monitor_t getnameinfo_monitor;
static char *ctrlDirName = NULL;
static char *cacheDirName = NULL;
static UDATA groupPerm;
static UDATA cacheDirFlags;
#define J9PORT_CTLDATA_SHMEM_GROUP_PERM "SHMEM_GROUP_PERM"
#define J9PORT_CTLDATA_SHMEM_CONTROL_DIR_FLAGS "SHMEM_CONTROL_DIR_FLAGS"
#define J9PORT_CTLDATA_SHMEM_CONTROL_DIR "SHMEM_CONTROL_DIR"

static void ensureDirectory(struct HyPortLibrary *portLibrary ) {
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);

	if (cacheDirName == NULL){
		BOOLEAN appendBaseDir = (cacheDirFlags & J9SH_SHMEM_CONTROL_DIR_FLAG_DONT_CREATE_BASEDIR) == 0;
		cacheDirName = j9mem_allocate_memory(EsMaxPath+1, J9MEM_CATEGORY_HARMONY);
		j9shmem_getDir(ctrlDirName, appendBaseDir ? J9SHMEM_GETDIR_APPEND_BASEDIR : 0, cacheDirName, EsMaxPath);
		if (cacheDirFlags & J9SH_SHMEM_CONTROL_DIR_FLAG_TRY_CREATE_DIR) {
			j9shmem_createDir(cacheDirName, J9SH_DIRPERM_ABSENT, ctrlDirName == NULL);
		}
	}
}


I_32 
hystub_port_shutdown_library(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9port_shutdown_library();
}

I_32
hystub_port_isFunctionOverridden(struct HyPortLibrary *portLibrary, UDATA offset)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9port_isFunctionOverridden(offset);
}

void
hystub_port_tls_free(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9port_tls_free();
}

I_32
hystub_error_startup(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9error_startup();
}

void
hystub_error_shutdown(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9error_shutdown();
}

I_32
hystub_error_last_error_number(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9error_last_error_number();
}

const char *
hystub_error_last_error_message(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9error_last_error_message();
}

I_32
hystub_error_set_last_error(struct HyPortLibrary *portLibrary, I_32 platformCode, I_32 portableCode  )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9error_set_last_error(platformCode, portableCode);
}

I_32
hystub_error_set_last_error_with_message(struct HyPortLibrary *portLibrary, I_32 portableCode, const char *errorMessage )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9error_set_last_error_with_message(portableCode, errorMessage);
}

I_32
hystub_time_startup(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9time_startup();
}

void
hystub_time_shutdown(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9time_shutdown();
}

UDATA
hystub_time_msec_clock(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9time_msec_clock();
}

UDATA
hystub_time_usec_clock(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9time_usec_clock();
}

I_64
hystub_time_current_time_millis(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9time_current_time_millis();
}

U_64
hystub_time_hires_clock(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9time_hires_clock();
}

U_64
hystub_time_hires_frequency(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9time_hires_frequency();
}

U_64
hystub_time_hires_delta(struct HyPortLibrary *portLibrary, U_64 startTime, U_64 endTime, UDATA requiredResolution )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9time_hires_delta(startTime, endTime, requiredResolution);
}

I_32
hystub_sysinfo_startup(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sysinfo_startup();
}
void
hystub_sysinfo_shutdown(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9sysinfo_shutdown();
}

UDATA
hystub_sysinfo_get_pid(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sysinfo_get_pid();
}

U_64
hystub_sysinfo_get_physical_memory(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sysinfo_get_physical_memory();
}
const char*
hystub_sysinfo_get_OS_version(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sysinfo_get_OS_version();
}

IDATA
hystub_sysinfo_get_env(struct HyPortLibrary *portLibrary, char *envVar, char *infoString, UDATA bufSize )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sysinfo_get_env(envVar, infoString, bufSize);
}


const char *
hystub_sysinfo_get_CPU_architecture(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sysinfo_get_CPU_architecture();
}

const char *
hystub_sysinfo_get_OS_type(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sysinfo_get_OS_type();
}

U_16
hystub_sysinfo_get_classpathSeparator(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sysinfo_get_classpathSeparator();
}

IDATA
hystub_sysinfo_get_executable_name(struct HyPortLibrary *portLibrary, char *argv0, char **result )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sysinfo_get_executable_name(argv0, result);
}

UDATA
hystub_sysinfo_get_number_CPUs(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE);
}

IDATA
hystub_sysinfo_get_username(struct HyPortLibrary *portLibrary, char *buffer, UDATA length )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sysinfo_get_username(buffer, length);
}

I_32
hystub_file_startup(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9file_startup();
}

void
hystub_file_shutdown(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9file_shutdown();
}

IDATA
hystub_file_write(struct HyPortLibrary *portLibrary, IDATA fd, const void *buf, IDATA nbytes)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9file_write(fd, buf, nbytes);
}

IDATA
hystub_file_write_text(struct HyPortLibrary *portLibrary, IDATA fd, const char *buf, IDATA nbytes )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9file_write_text(fd, buf, nbytes);
}

void
hystub_file_vprintf(struct HyPortLibrary *portLibrary, IDATA fd, const char *format, va_list args )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9file_vprintf(fd, format, args);
}

void
hystub_file_printf(struct HyPortLibrary *portLibrary, IDATA fd, const char *format, ... )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	va_list args;
	
	va_start( args, format );
	j9file_vprintf(fd, format, args);
	va_end(args);
}

IDATA
hystub_file_open(struct HyPortLibrary *portLibrary, const char *path, I_32 flags, I_32 mode )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9file_open(path, flags, mode);
}

I_32
hystub_file_close(struct HyPortLibrary *portLibrary, IDATA fd )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9file_close(fd);
}

I_64
hystub_file_seek(struct HyPortLibrary *portLibrary, IDATA fd, I_64 offset, I_32 whence)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9file_seek(fd, offset, whence);
}

IDATA
hystub_file_read(struct HyPortLibrary *portLibrary, IDATA fd, void *buf, IDATA nbytes)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9file_read(fd, buf, nbytes);
}

I_32
hystub_file_unlink(struct HyPortLibrary *portLibrary, const char *path )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9file_unlink(path);
}

I_32
hystub_file_attr(struct HyPortLibrary *portLibrary, const char *path)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9file_attr(path);
}

I_64
hystub_file_lastmod(struct HyPortLibrary *portLibrary, const char *path )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	I_64 rc;
	
	/* Harmony Port Library returns last modified time in milliseconds, multiply by 1000 for now. */
	rc = j9file_lastmod(path);
	
	if (-1 != rc) {
		rc = rc * 1000;
	}
	
	return rc;
}

I_64
hystub_file_length(struct HyPortLibrary *portLibrary, const char *path)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9file_length(path);
}

I_32
hystub_file_set_length(struct HyPortLibrary *portLibrary, IDATA fd, I_64 newLength)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9file_set_length(fd, newLength);
}

I_32
hystub_file_sync(struct HyPortLibrary *portLibrary, IDATA fd)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9file_sync(fd);
}

I_32
hystub_sl_startup(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sl_startup();
}

void
hystub_sl_shutdown(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9sl_shutdown();
}

UDATA
hystub_sl_close_shared_library(struct HyPortLibrary *portLibrary, UDATA descriptor)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sl_close_shared_library(descriptor);
}


UDATA
hystub_sl_open_shared_library(struct HyPortLibrary *portLibrary, char *name, UDATA *descriptor, BOOLEAN decorate)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sl_open_shared_library(name, descriptor, decorate);
}

UDATA
hystub_sl_lookup_name(struct HyPortLibrary *portLibrary, UDATA descriptor, char *name, UDATA *func, const char *argSignature)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sl_lookup_name(descriptor, name, func, argSignature);
}

I_32
hystub_tty_startup(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9tty_startup();
}

void
hystub_tty_shutdown(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9tty_shutdown();
}

void
hystub_tty_printf(struct HyPortLibrary *portLibrary, const char *format, ...)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	va_list args;
	
	va_start( args, format );
	j9tty_vprintf( format, args);
	va_end(args);
}

void
hystub_tty_vprintf(struct HyPortLibrary *portLibrary, const char * format, va_list args )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9tty_vprintf(format, args);
}

IDATA
hystub_tty_get_chars(struct HyPortLibrary *portLibrary, char *s, UDATA length)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9tty_get_chars(s, length);
}

void
hystub_tty_err_printf(struct HyPortLibrary *portLibrary, const char *format, ...)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	va_list args;
	
	va_start(args, format);
	j9tty_err_vprintf(format, args);
	va_end(args);
}

void
hystub_tty_err_vprintf(struct HyPortLibrary *portLibrary, const char *format, va_list args)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9tty_err_vprintf(format, args);
}

IDATA
hystub_tty_available(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9tty_available();
}

I_32
hystub_mem_startup(struct HyPortLibrary *portLibrary, UDATA portGlobalSize )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9mem_startup(portGlobalSize);
}

void
hystub_mem_shutdown(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9mem_shutdown();
}

void*
hystub_mem_allocate_memory(struct HyPortLibrary *portLibrary, UDATA byteAmount)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9mem_allocate_memory(byteAmount, J9MEM_CATEGORY_HARMONY);
}

void *
hystub_mem_allocate_memory_callSite(struct HyPortLibrary *portLibrary, UDATA byteAmount, char *callSite)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return OMRPORT_FROM_J9PORT(PORTLIB)->mem_allocate_memory(OMRPORT_FROM_J9PORT(PORTLIB), byteAmount, callSite, J9MEM_CATEGORY_HARMONY);
}

void
hystub_mem_free_memory(struct HyPortLibrary *portLibrary, void *memoryPointer)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9mem_free_memory(memoryPointer);
}

void *
hystub_mem_reallocate_memory(struct HyPortLibrary *portLibrary, void *memoryPointer, UDATA byteAmount )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return OMRPORT_FROM_J9PORT(PORTLIB)->mem_reallocate_memory(OMRPORT_FROM_J9PORT(PORTLIB), memoryPointer, byteAmount, NULL, J9MEM_CATEGORY_HARMONY);
}

I_32
hystub_cpu_startup(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9cpu_startup();
}

void
hystub_cpu_shutdown(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9cpu_shutdown();
}

void
hystub_cpu_flush_icache(struct HyPortLibrary *portLibrary, void *memoryPointer, UDATA byteAmount )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9cpu_flush_icache(memoryPointer, byteAmount);
}

I_32
hystub_vmem_startup(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9vmem_startup();
}

void
hystub_vmem_shutdown(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9vmem_shutdown();
}

void *
hystub_vmem_commit_memory(struct HyPortLibrary *portLibrary, void *address, UDATA byteAmount, struct HyPortVmemIdentifier *identifier)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9vmem_commit_memory(address, byteAmount, (struct J9PortVmemIdentifier *)identifier);
}

IDATA
hystub_vmem_decommit_memory(struct HyPortLibrary *portLibrary, void *address, UDATA byteAmount, struct HyPortVmemIdentifier *identifier)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9vmem_decommit_memory(address, byteAmount, (struct J9PortVmemIdentifier *)identifier);
}

I_32
hystub_vmem_free_memory(struct HyPortLibrary *portLibrary, void *address, UDATA byteAmount, struct HyPortVmemIdentifier *identifier)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9vmem_free_memory(address, byteAmount, (struct J9PortVmemIdentifier *)identifier);
}

void *
hystub_vmem_reserve_memory(struct HyPortLibrary *portLibrary, void *address, UDATA byteAmount, struct HyPortVmemIdentifier *identifier, UDATA mode, UDATA pageSize)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9vmem_reserve_memory(address, byteAmount, (struct J9PortVmemIdentifier *)identifier, mode, pageSize, J9MEM_CATEGORY_HARMONY);
}

UDATA *
hystub_vmem_supported_page_sizes(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9vmem_supported_page_sizes();
}

I_32
hystub_sock_startup(struct HyPortLibrary *portLibrary )
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_startup();
}

I_32 
hystub_sock_shutdown(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_shutdown();
}

U_16
hystub_sock_htons(struct HyPortLibrary *portLibrary, U_16 val)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_htons(val);
}

I_32 
hystub_sock_write(struct HyPortLibrary *portLibrary, hysocket_t sock, U_8 * buf, I_32 nbyte, I_32 flags)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_write((j9socket_t)sock, buf, nbyte, flags);
}

I_32
hystub_sock_sockaddr(struct HyPortLibrary *portLibrary, hysockaddr_t handle, char *addrStr, U_16 port)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_sockaddr((j9sockaddr_t)handle, addrStr, port);
}

I_32
hystub_sock_read(struct HyPortLibrary *portLibrary, hysocket_t sock, U_8 *buf, I_32 nbyte, I_32 flags)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_read((j9socket_t)sock, buf, nbyte, flags);
}

I_32
hystub_sock_socket(struct HyPortLibrary *portLibrary, hysocket_t *handle, I_32 family, I_32 socktype, I_32 protocol)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_socket((j9socket_t *)handle, family, socktype, protocol);
}

I_32
hystub_sock_close(struct HyPortLibrary *portLibrary, hysocket_t *sock)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_close((j9socket_t *)sock);
}

I_32
hystub_sock_connect(struct HyPortLibrary *portLibrary, hysocket_t sock, hysockaddr_t addr)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_connect((j9socket_t)sock, (j9sockaddr_t)addr);
}

I_32
hystub_sock_inetaddr(struct HyPortLibrary *portLibrary, char *addrStr, U_32 *addr)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_inetaddr(addrStr, addr);
}

I_32
hystub_sock_gethostbyname(struct HyPortLibrary *portLibrary, char *name, hyhostent_t handle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_gethostbyname(name, (j9hostent_t)handle);
}

I_32
hystub_sock_hostent_addrlist(struct HyPortLibrary *portLibrary, hyhostent_t handle, U_32 index)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_hostent_addrlist((j9hostent_t)handle, index);
}

I_32
hystub_sock_sockaddr_init(struct HyPortLibrary *portLibrary, hysockaddr_t handle, I_16 family, U_32 nipAddr, U_16 port)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_sockaddr_init((j9sockaddr_t)handle, family, nipAddr, port);
}

I_32
hystub_sock_linger_init(struct HyPortLibrary *portLibrary, hylinger_t handle, I_32 enabled, U_16 timeout)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_linger_init((j9linger_t)handle, enabled, timeout);
}

I_32
hystub_sock_setopt_linger(struct HyPortLibrary *portLibrary, hysocket_t socketP, I_32 optlevel, I_32 optname, hylinger_t optval)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_setopt_linger((j9socket_t)socketP, optlevel, optname, (j9linger_t)optval);
}

I_32
hystub_gp_startup(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9gp_startup();
}

void
hystub_gp_shutdown(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9gp_shutdown();
}

UDATA
hystub_gp_protect(struct HyPortLibrary *portLibrary, protected_fn fn, void *arg)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9gp_protect(fn, arg);
}

void
hystub_gp_register_handler(struct HyPortLibrary *portLibrary, handler_fn fn, void *aUserData)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9gp_register_handler(fn, aUserData);
}

U_32
hystub_gp_info(struct HyPortLibrary *portLibrary, void *info, U_32 category, I_32 index, const char **name, void **value)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9gp_info(info, category, index, name, value);
}

U_32
hystub_gp_info_count(struct HyPortLibrary *portLibrary, void *info, U_32 category)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9gp_info_count(info,category);
}

void 
hystub_gp_handler_function(void *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)((HyPortLibrary*)portLibrary)->self_handle);
	j9gp_handler_function();
}

I_32
hystub_str_startup(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9str_startup();
}

void
hystub_str_shutdown(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9str_shutdown();
}

U_32
hystub_str_printf(struct HyPortLibrary *portLibrary, char * buf, U_32 bufLen, const char *format, ...)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	U_32 rc;
	va_list args;
	va_start( args, format );
	/* cast is safe because bufLen is U_32 */
	rc = (U_32) j9str_vprintf(buf, bufLen, format, args);
	va_end( args );
	return rc;
}

U_32
hystub_str_vprintf(struct HyPortLibrary *portLibrary, char *buf, U_32 bufLen, const char *format, va_list args)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	IDATA actualLen;

	actualLen = j9str_vprintf(buf, bufLen, format, args);
	if (U_32_MAX < actualLen) {
		/* bufLen == 0 indicates a query for the length */
		return -1;
	} else {
		return (U_32) actualLen;
	}
}

I_32
hystub_exit_startup(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9exit_startup();
}

void
hystub_exit_shutdown(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9exit_shutdown();
}

I_32
hystub_exit_get_exit_code(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9exit_get_exit_code();
}

void
hystub_exit_shutdown_and_exit(struct HyPortLibrary *portLibrary, I_32 exitCode)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9exit_shutdown_and_exit(exitCode);
}

UDATA
hystub_dump_create(struct HyPortLibrary *portLibrary, char *filename, char *dumpType, void *userData)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9dump_create(filename, dumpType, userData);
}

I_32
hystub_nls_startup(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9nls_startup();
}

void 
hystub_nls_shutdown(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9nls_shutdown();
}

void
hystub_nls_set_catalog(struct HyPortLibrary *portLibrary, const char **paths, const int nPaths, const char *baseName, const char *extension)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9nls_set_catalog(paths, nPaths, baseName, extension);
}

void
hystub_nls_set_locale(struct HyPortLibrary *portLibrary, const char *lang, const char *region, const char *variant)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9nls_set_locale(lang, region, variant);
}

const char *
hystub_nls_get_language(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9nls_get_language();
}

const char *
hystub_nls_get_region(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9nls_get_region();
}

const char *
hystub_nls_get_variant(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9nls_get_variant();
}

void
hystub_nls_printf(struct HyPortLibrary *portLibrary, UDATA flags, U_32 module_name, U_32 message_num, ...)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	va_list( args );
	va_start( args, message_num );
	j9nls_vprintf(flags, module_name, message_num, args);
	va_end( args );
}

void
hystub_nls_vprintf(struct HyPortLibrary *portLibrary, UDATA flags, U_32 module_name, U_32 message_num, va_list args)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9nls_vprintf(flags, module_name, message_num, args);
}

const char *
hystub_nls_lookup_message(struct HyPortLibrary *portLibrary, UDATA flags, U_32 module_name, U_32 message_num, const char *default_string)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return OMRPORT_FROM_J9PORT(PORTLIB)->nls_lookup_message(OMRPORT_FROM_J9PORT(PORTLIB), flags, module_name, message_num, default_string);
}

I_32
hystub_ipcmutex_startup(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9ipcmutex_startup();
}

void
hystub_ipcmutex_shutdown(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9ipcmutex_shutdown();
}

I_32
hystub_ipcmutex_acquire(struct HyPortLibrary *portLibrary, const char *name)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9ipcmutex_acquire(name);
}

I_32
hystub_ipcmutex_release(struct HyPortLibrary *portLibrary, const char *name)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9ipcmutex_release(name);
}

I_32
hystub_port_control(struct HyPortLibrary *portLibrary, char *key, UDATA value)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	/*TODO check if the key is the static name, then change the static getDir*/

	if(0 == strcmp(J9PORT_CTLDATA_SHMEM_GROUP_PERM, key)) {
		groupPerm = value;
		return 0;
	}
	if (0 == strcmp(J9PORT_CTLDATA_SHMEM_CONTROL_DIR_FLAGS, key)) {
		cacheDirFlags = value;
		return 0;
	}
#if defined(PPG_shmem_controlDir)
	if (0 == strcmp(J9PORT_CTLDATA_SHMEM_CONTROL_DIR, key)) {
		if ((value != 0) && (ctrlDirName == NULL)) {
			/* ctrlDirName is set to NULL in shim portlibrary startup and conditionally freed in shim portlibrary shutdown */
			ctrlDirName = j9mem_allocate_memory(EsMaxPath+1, J9MEM_CATEGORY_HARMONY);
			if (ctrlDirName != NULL) {
				j9str_printf(PORTLIB, ctrlDirName, EsMaxPath, "%s", (const char*)value);
				return 0;
			}
		}
	}
#endif

	return j9port_control(key, value);
}

I_32
hystub_sig_startup(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sig_startup();
}

void
hystub_sig_shutdown(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9sig_shutdown();
}

I_32
hystub_sig_protect(struct HyPortLibrary *portLibrary, hysig_protected_fn fn, void *fn_arg, hysig_handler_fn handler, void * handler_arg, U_32 flags, UDATA *result)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sig_protect((j9sig_protected_fn)fn, fn_arg, (j9sig_handler_fn)handler, handler_arg, flags, result);
}

I_32
hystub_sig_can_protect(struct HyPortLibrary *portLibrary, U_32 flags)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sig_can_protect(flags);
}

I_32
hystub_sig_set_async_signal_handler(struct HyPortLibrary *portLibrary, hysig_handler_fn handler, void *handler_arg, U_32 flags)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sig_set_async_signal_handler((j9sig_handler_fn)handler, handler_arg, flags);
}

U_32
hystub_sig_info(struct HyPortLibrary *portLibrary, void *info, U_32 category, I_32 index, const char **name, void **value)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sig_info(info, category, index, name, value);
}

U_32
hystub_sig_info_count(struct HyPortLibrary *portLibrary, void *info, U_32 category)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sig_info_count(info, category);
}

I_32
hystub_sig_set_options(struct HyPortLibrary *portLibrary, U_32 options)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sig_set_options(options);
}

U_32
hystub_sig_get_options(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sig_get_options();
}

UDATA
hystub_sysinfo_DLPAR_enabled(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sysinfo_DLPAR_enabled();
}

UDATA
hystub_sysinfo_DLPAR_max_CPUs(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sysinfo_DLPAR_max_CPUs();
}

UDATA
hystub_sysinfo_weak_memory_consistency(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sysinfo_weak_memory_consistency();
}
char *
hystub_file_read_text(struct HyPortLibrary *portLibrary, IDATA fd, char *buf, IDATA nbytes)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9file_read_text(fd, buf, nbytes);
}

I_32
hystub_file_mkdir(struct HyPortLibrary *portLibrary, const char *path)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9file_mkdir(path);
}

I_32
hystub_file_move(struct HyPortLibrary *portLibrary, const char *pathExist, const char *pathNew)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9file_move(pathExist, pathNew);
}

I_32
hystub_file_unlinkdir(struct HyPortLibrary *portLibrary, const char *path)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9file_unlinkdir(path);
}

UDATA
hystub_file_findfirst(struct HyPortLibrary *portLibrary, const char *path, char *resultbuf)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9file_findfirst(path, resultbuf);
}

I_32
hystub_file_findnext(struct HyPortLibrary *portLibrary, UDATA findhandle, char *resultbuf)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9file_findnext(findhandle, resultbuf);
}

void
hystub_file_findclose(struct HyPortLibrary *portLibrary, UDATA findhandle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9file_findclose(findhandle);
}

const char *
hystub_file_error_message(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9file_error_message();
}

I_32
hystub_sock_htonl(struct HyPortLibrary *portLibrary, I_32 val)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_htonl(val);
}

I_32
hystub_sock_bind(struct HyPortLibrary *portLibrary, hysocket_t sock, hysockaddr_t addr)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_bind((j9socket_t)sock, (j9sockaddr_t) addr);
}

I_32
hystub_sock_accept(struct HyPortLibrary *portLibrary, hysocket_t serverSock, hysockaddr_t addrHandle, hysocket_t *sockHandle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_accept((j9socket_t)serverSock, (j9sockaddr_t)addrHandle, (j9socket_t*)sockHandle);
}

I_32
hystub_sock_shutdown_input(struct HyPortLibrary *portLibrary, hysocket_t sock)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_shutdown_input((j9socket_t)sock);
}

I_32
hystub_sock_shutdown_output(struct HyPortLibrary *portLibrary, hysocket_t sock)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_shutdown_output((j9socket_t)sock);
}

I_32
hystub_sock_listen(struct HyPortLibrary *portLibrary, hysocket_t sock, I_32 backlog)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_listen((j9socket_t)sock, backlog);
}

I_32
hystub_sock_ntohl(struct HyPortLibrary *portLibrary, I_32 val)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_ntohl(val);
}

U_16
hystub_sock_ntohs(struct HyPortLibrary *portLibrary, U_16 val)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_ntohs(val);
}

I_32
hystub_sock_getpeername(struct HyPortLibrary *portLibrary, hysocket_t handle, hysockaddr_t addrHandle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_getpeername((j9socket_t)handle, (j9sockaddr_t)addrHandle);
}

I_32
hystub_sock_getsockname(struct HyPortLibrary *portLibrary, hysocket_t handle, hysockaddr_t addrHandle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_getsockname((j9socket_t)handle, (j9sockaddr_t)addrHandle);
}

I_32
hystub_sock_readfrom(struct HyPortLibrary *portLibrary, hysocket_t sock, U_8 *buf, I_32 nbyte, I_32 flags, hysockaddr_t addrHandle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_readfrom((j9socket_t)sock, buf, nbyte, flags, (j9sockaddr_t)addrHandle);
}

I_32
hystub_sock_select(struct HyPortLibrary *portLibrary, I_32 nfds, hyfdset_t readfds, hyfdset_t writefds, hyfdset_t exceptfds, hytimeval_t timeout)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_select(nfds, (j9fdset_t)readfds, (j9fdset_t)writefds, (j9fdset_t)exceptfds, (j9timeval_t)timeout);
}

I_32
hystub_sock_writeto(struct HyPortLibrary *portLibrary, hysocket_t sock, U_8 *buf, I_32 nbyte, I_32 flags, hysockaddr_t addrHandle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_writeto((j9socket_t)sock, buf, nbyte, flags, (j9sockaddr_t)addrHandle);
}

I_32
hystub_sock_inetntoa(struct HyPortLibrary *portLibrary, char **addrStr, U_32 nipAddr)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_inetntoa(addrStr, nipAddr);
}

I_32
hystub_sock_gethostbyaddr(struct HyPortLibrary *portLibrary, char *addr, I_32 length, I_32 type, hyhostent_t handle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_gethostbyaddr(addr, length, type, (j9hostent_t)handle);
}


I_32
hystub_sock_gethostname(struct HyPortLibrary *portLibrary, char *buffer, I_32 length)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_gethostname(buffer, length);
}

I_32
hystub_sock_hostent_aliaslist(struct HyPortLibrary *portLibrary, hyhostent_t handle, char ***aliasList)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_hostent_aliaslist((j9hostent_t)handle, aliasList);
}

I_32
hystub_sock_hostent_hostname(struct HyPortLibrary *portLibrary, hyhostent_t handle, char **hostName)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_hostent_hostname((j9hostent_t)handle, hostName);
}

U_16
hystub_sock_sockaddr_port(struct HyPortLibrary *portLibrary, hysockaddr_t handle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_sockaddr_port((j9sockaddr_t)handle);
}

I_32 
hystub_sock_sockaddr_address(struct HyPortLibrary *portLibrary, hysockaddr_t handle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_sockaddr_address(handle);
}


I_32
hystub_sock_fdset_init(struct HyPortLibrary *portLibrary, hysocket_t socketP)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_fdset_init((j9socket_t)socketP);
}

I_32
hystub_sock_fdset_size(struct HyPortLibrary *portLibrary, hysocket_t handle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_fdset_size((j9socket_t)handle);
}

I_32
hystub_sock_timeval_init(struct HyPortLibrary *portLibrary, U_32 secTime, U_32 uSecTime, hytimeval_t timeP)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_timeval_init(secTime, uSecTime, (j9timeval_t)timeP);
}

I_32
hystub_sock_getopt_int(struct HyPortLibrary *portLibrary, hysocket_t socketP, I_32 optlevel, I_32 optname, I_32 *optval)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_getopt_int((j9socket_t)socketP, optlevel, optname, optval);
}

I_32
hystub_sock_setopt_int(struct HyPortLibrary *portLibrary, hysocket_t socketP, I_32 optlevel, I_32 optname, I_32 *optval)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_setopt_int((j9socket_t) socketP, optlevel, optname, optval);
}

I_32
hystub_sock_getopt_bool(struct HyPortLibrary *portLibrary, hysocket_t socketP, I_32 optlevel, I_32 optname, BOOLEAN *optval)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_getopt_bool((j9socket_t)socketP, optlevel, optname, optval);
}

I_32
hystub_sock_setopt_bool(struct HyPortLibrary *portLibrary, hysocket_t socketP, I_32 optlevel, I_32 optname, BOOLEAN *optval)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_setopt_bool((j9socket_t)socketP, optlevel, optname, optval);
}

I_32
hystub_sock_getopt_byte(struct HyPortLibrary *portLibrary, hysocket_t socketP, I_32 optlevel, I_32 optname, U_8 *optval)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_getopt_byte((j9socket_t)socketP, optlevel, optname, optval);
}

I_32
hystub_sock_setopt_byte(struct HyPortLibrary *portLibrary, hysocket_t socketP, I_32 optlevel, I_32 optname, U_8 *optval)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_setopt_byte((j9socket_t)socketP, optlevel, optname, optval);
}

I_32
hystub_sock_getopt_linger(struct HyPortLibrary *portLibrary, hysocket_t socketP, I_32 optlevel, I_32 optname, hylinger_t optval)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_getopt_linger((j9socket_t)socketP, optlevel, optname, (j9linger_t)optval);
}

I_32
hystub_sock_getopt_sockaddr(struct HyPortLibrary *portLibrary, hysocket_t socketP, I_32 optlevel, I_32 optname, hysockaddr_t optval)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_getopt_sockaddr((j9socket_t)socketP, optlevel, optname, (j9sockaddr_t)optval);
}

I_32
hystub_sock_setopt_sockaddr(struct HyPortLibrary *portLibrary, hysocket_t socketP, I_32 optlevel, I_32 optname, hysockaddr_t optval)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_setopt_sockaddr((j9socket_t)socketP, optlevel, optname, (j9sockaddr_t)optval);
}

I_32
hystub_sock_setopt_ipmreq(struct HyPortLibrary *portLibrary, hysocket_t socketP, I_32 optlevel, I_32 optname, hyipmreq_t optval)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_setopt_ipmreq((j9socket_t)socketP, optlevel, optname, (j9ipmreq_t)optval);
}

I_32
hystub_sock_linger_enabled(struct HyPortLibrary *portLibrary, hylinger_t handle, BOOLEAN *enabled)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_linger_enabled((j9linger_t)handle, enabled);
}

I_32
hystub_sock_linger_linger(struct HyPortLibrary *portLibrary, hylinger_t handle, U_16 * linger)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_linger_linger((j9linger_t)handle, linger);
}

I_32
hystub_sock_ipmreq_init(struct HyPortLibrary *portLibrary, hyipmreq_t handle, U_32 nipmcast, U_32 nipinterface)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_ipmreq_init((j9ipmreq_t)handle, nipmcast, nipinterface);
}

I_32
hystub_sock_setflag(struct HyPortLibrary *portLibrary, I_32 flag, I_32 *arg)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_setflag(flag, arg);
}

I_32
hystub_sock_freeaddrinfo(struct HyPortLibrary *portLibrary, hyaddrinfo_t handle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_freeaddrinfo((j9addrinfo_t)handle);
}

I_32
hystub_sock_getaddrinfo(struct HyPortLibrary *portLibrary, char * name, hyaddrinfo_t hints, hyaddrinfo_t result)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_getaddrinfo(name, (j9addrinfo_t)hints, (j9addrinfo_t)result);
}

I_32
hystub_sock_getaddrinfo_address(struct HyPortLibrary *portLibrary, hyaddrinfo_t handle, U_8 *address, int index, U_32 *scope_id)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	I_32 rc;
	omrthread_monitor_enter(getnameinfo_monitor);
	rc = j9sock_getaddrinfo_address((j9addrinfo_t)handle, address, index, scope_id);
	omrthread_monitor_exit(getnameinfo_monitor);

	return rc;
}

I_32
hystub_sock_getaddrinfo_create_hints(struct HyPortLibrary *portLibrary, hyaddrinfo_t *handle, I_16 family, I_32 socktype, I_32 protocol, I_32 flags)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_getaddrinfo_create_hints((j9addrinfo_t *)handle, family, socktype, protocol, flags);
}

I_32
hystub_sock_getaddrinfo_family(struct HyPortLibrary *portLibrary, hyaddrinfo_t handle, I_32 *family, int index)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_getaddrinfo_family((j9addrinfo_t)handle, family, index);
}

I_32
hystub_sock_getaddrinfo_length(struct HyPortLibrary *portLibrary, hyaddrinfo_t handle, I_32 *length)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_getaddrinfo_length((j9addrinfo_t)handle, length);
}

I_32
hystub_sock_getaddrinfo_name(struct HyPortLibrary *portLibrary, hyaddrinfo_t handle, char *name, int index)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_getaddrinfo_name((j9addrinfo_t)handle, name, index);
}

I_32
hystub_sock_getnameinfo(struct HyPortLibrary *portLibrary, hysockaddr_t in_addr, I_32 sockaddr_size, char *name, I_32 name_length, int flags)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	I_32 rc;
	omrthread_monitor_enter(getnameinfo_monitor);
	rc = j9sock_getnameinfo((j9sockaddr_t)in_addr, sockaddr_size, name, name_length, flags);
	omrthread_monitor_exit(getnameinfo_monitor);
	return rc;
}

I_32
hystub_sock_ipv6_mreq_init(struct HyPortLibrary *portLibrary, hyipv6_mreq_t handle, U_8 *ipmcast_addr, U_32 ipv6mr_interface)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_ipv6_mreq_init((j9ipv6_mreq_t)handle, ipmcast_addr, ipv6mr_interface);
}

I_32
hystub_sock_setopt_ipv6_mreq(struct HyPortLibrary *portLibrary, hysocket_t socketP, I_32 optlevel, I_32 optname, hyipv6_mreq_t optval)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_setopt_ipv6_mreq((j9socket_t)socketP, optlevel, optname, (j9ipv6_mreq_t)optval);
}

I_32
hystub_sock_sockaddr_address6(struct HyPortLibrary *portLibrary, hysockaddr_t handle, U_8 *address, U_32 *length, U_32 *scope_id)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_sockaddr_address6((j9sockaddr_t)handle, address, length, scope_id);
}

I_32
hystub_sock_sockaddr_family(struct HyPortLibrary *portLibrary, I_16 *family, hysockaddr_t handle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_sockaddr_family(family, (hysockaddr_t)handle);
}

I_32
hystub_sock_sockaddr_init6(struct HyPortLibrary *portLibrary, hysockaddr_t handle, U_8 *addr, I_32 addrlength, I_16 family, U_16 nPort, U_32 flowinfo, U_32 scope_id, hysocket_t sock)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_sockaddr_init6((j9sockaddr_t)handle, addr, addrlength, family, nPort, flowinfo, scope_id, (j9socket_t)sock);
}

I_32
hystub_sock_socketIsValid(struct HyPortLibrary *portLibrary, hysocket_t handle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_socketIsValid((j9socket_t)handle);
}

I_32
hystub_sock_select_read(struct HyPortLibrary *portLibrary, hysocket_t socketP, I_32 secTime, I_32 uSecTime, BOOLEAN accept)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_select_read((j9socket_t)socketP, secTime, uSecTime, accept);
}

I_32
hystub_sock_set_nonblocking(struct HyPortLibrary *portLibrary, hysocket_t socketP, BOOLEAN nonblocking)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_set_nonblocking((j9socket_t)socketP, nonblocking);
}

const char *
hystub_sock_error_message(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_error_message();
}

I_32
hystub_sock_get_network_interfaces(struct HyPortLibrary *portLibrary, struct hyNetworkInterfaceArray_struct *array, BOOLEAN preferIPv4Stack)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_get_network_interfaces((struct j9NetworkInterfaceArray_struct *)array, preferIPv4Stack);
}

I_32
hystub_sock_free_network_interface_struct(struct HyPortLibrary *portLibrary, struct hyNetworkInterfaceArray_struct *array)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_free_network_interface_struct((struct j9NetworkInterfaceArray_struct *)array);
}

I_32
hystub_sock_connect_with_timeout(struct HyPortLibrary *portLibrary, hysocket_t sock, hysockaddr_t addr, U_32 timeout, U_32 step, U_8 ** context)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sock_connect_with_timeout((j9socket_t)sock, (j9sockaddr_t)addr, timeout, step, context);
}

U_32
hystub_strftime(struct HyPortLibrary *portLibrary, char *buf, U_32 bufLen, const char *format)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return (U_32)j9str_ftime(buf, bufLen, format, j9time_current_time_millis());
}

I_32
hystub_mmap_startup(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9mmap_startup();
}

void
hystub_mmap_shutdown(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9mmap_shutdown();
}

I_32
hystub_mmap_capabilities(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9mmap_capabilities();
}

void *
hystub_mmap_map_file(struct HyPortLibrary *portLibrary, const char *path, void **handle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);

	IDATA file;
	UDATA size;
	U_32 mmapFlags = J9PORT_MMAP_FLAG_COPYONWRITE;
	J9MmapHandle *mmapHandle;
	
	size = (UDATA)j9file_length( path );
	file = j9file_open( path, (I_32) EsOpenRead, (I_32) 0 );
	mmapHandle = j9mmap_map_file( file, 0, size, NULL, mmapFlags, J9MEM_CATEGORY_HARMONY );
	j9file_close(file);
	*handle = (void*)mmapHandle;
	if (mmapHandle == NULL) {
		return NULL;
	}
	
	return mmapHandle->pointer;
}

void
hystub_mmap_unmap_file(struct HyPortLibrary *portLibrary, void *handle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9mmap_unmap_file(handle);
}

I_32
hystub_shsem_startup(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9shsem_deprecated_startup();
}

void
hystub_shsem_shutdown(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9shsem_deprecated_shutdown();
}

IDATA
hystub_shsem_open(struct HyPortLibrary *portLibrary, struct hyshsem_handle **handle, const char *semname, int setSize, int permission, UDATA flags)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	ensureDirectory(portLibrary);
	return j9shsem_deprecated_open(cacheDirName, groupPerm, (struct j9shsem_handle**)handle, semname, setSize, permission, flags, NULL);
}

IDATA
hystub_shsem_post(struct HyPortLibrary *portLibrary, struct hyshsem_handle *handle, UDATA semset, UDATA flag)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9shsem_deprecated_post((struct j9shsem_handle*)handle, semset, flag);
}

IDATA
hystub_shsem_wait(struct HyPortLibrary *portLibrary, struct hyshsem_handle *handle, UDATA semset, UDATA flag)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9shsem_deprecated_wait((struct j9shsem_handle*)handle, semset, flag);
}

IDATA
hystub_shsem_getVal(struct HyPortLibrary *portLibrary, struct hyshsem_handle *handle, UDATA semset)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9shsem_deprecated_getVal((struct j9shsem_handle*)handle, semset);
}

IDATA
hystub_shsem_setVal(struct HyPortLibrary *portLibrary, struct hyshsem_handle *handle, UDATA semset, IDATA value)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9shsem_deprecated_setVal((struct j9shsem_handle *)handle, semset, value);
}

void
hystub_shsem_close(struct HyPortLibrary *portLibrary, struct hyshsem_handle **handle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9shsem_deprecated_close((struct j9shsem_handle **)handle);
}

IDATA
hystub_shsem_destroy(struct HyPortLibrary *portLibrary, struct hyshsem_handle **handle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9shsem_deprecated_destroy((struct j9shsem_handle **)handle);
}

I_32
hystub_shmem_startup(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	ctrlDirName = NULL;
	cacheDirName = NULL;
	return j9shmem_startup();
}

void
hystub_shmem_shutdown(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	if (ctrlDirName) {
		j9mem_free_memory(ctrlDirName);
	}
	if (cacheDirName) {
		j9mem_free_memory(cacheDirName);
	}
	j9shmem_shutdown();
}

IDATA
hystub_shmem_open(struct HyPortLibrary *portLibrary, struct hyshmem_handle **handle, const char *rootname, I_32 size, I_32 perm, UDATA flags)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	ensureDirectory(portLibrary);
	return j9shmem_open(cacheDirName, groupPerm, (struct j9shmem_handle**)handle, rootname, (UDATA)size, (U_32)perm, J9MEM_CATEGORY_HARMONY, flags, NULL);
}

void *
hystub_shmem_attach(struct HyPortLibrary *portLibrary, struct hyshmem_handle *handle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9shmem_attach((struct j9shmem_handle*) handle, J9MEM_CATEGORY_HARMONY);
}

IDATA
hystub_shmem_detach(struct HyPortLibrary *portLibrary, struct hyshmem_handle **handle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9shmem_detach((struct j9shmem_handle **)handle);
}

void
hystub_shmem_close(struct HyPortLibrary *portLibrary, struct hyshmem_handle **handle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9shmem_close((struct j9shmem_handle**)handle);
}

IDATA
hystub_shmem_destroy(struct HyPortLibrary *portLibrary, struct hyshmem_handle **handle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	ensureDirectory(portLibrary);
	return j9shmem_destroy(cacheDirName, groupPerm, (struct j9shmem_handle**) handle);
}

UDATA
hystub_shmem_findfirst(struct HyPortLibrary *portLibrary, char *resultbuf)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	ensureDirectory(portLibrary);
	return j9shmem_findfirst(cacheDirName, resultbuf);
}

I_32
hystub_shmem_findnext(struct HyPortLibrary *portLibrary, UDATA findhandle, char *resultbuf)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9shmem_findnext(findhandle, resultbuf);
}

void
hystub_shmem_findclose(struct HyPortLibrary *portLibrary, UDATA findhandle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9shmem_findclose(findhandle);
}

UDATA
hystub_shmem_stat(struct HyPortLibrary *portLibrary, const char * name, struct HyPortShmemStatistic *statbuf)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	ensureDirectory(portLibrary);
	return j9shmem_stat(cacheDirName, groupPerm, name, (struct J9PortShmemStatistic *)statbuf);
}

UDATA
hystub_sysinfo_get_processing_capacity(struct HyPortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9sysinfo_get_processing_capacity();
}

char *
hystub_buf_write_text(struct HyPortLibrary *portLibrary, const char *buf, IDATA nbytes)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9tty_printf(PORTLIB, "ERROR: call to unimplemented portlib->buf_write_text()\n");
	return NULL;
}

HyThreadLibrary *
hystub_port_get_thread_library(struct HyPortLibrary *portLibrary)
{
	return (HyThreadLibrary*)portLibrary->portGlobals;
}


void
hystub_sock_fdset_zero(struct HyPortLibrary *portLibrary, hyfdset_t hyfdset)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9sock_fdset_zero((j9fdset_t)hyfdset);
}
void
hystub_sock_fdset_set(struct HyPortLibrary *portLibrary, hysocket_t aSocket, hyfdset_t hyfdset)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	j9sock_fdset_set((j9socket_t)aSocket, (j9fdset_t)hyfdset);
}

IDATA
hystub_process_create(struct HyPortLibrary *portLibrary, char *command[], UDATA commandLength, char *env[], UDATA envSize, char *dir, U_32 options, IDATA fdInput, IDATA fdOutput, IDATA fdError, struct HyProcessHandleStruct *processHandle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9process_create((const char **)command, commandLength, env, envSize, dir, options, fdInput, fdOutput, fdError, (J9ProcessHandle *) processHandle);
}

IDATA
hystub_process_getStream(struct HyPortLibrary *portLibrary, HyProcessHandle processHandle, UDATA streamFlag, IDATA *stream)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9process_getStream((J9ProcessHandle)processHandle, streamFlag, (IDATA *) stream);
}

IDATA
hystub_process_waitfor(struct HyPortLibrary *portLibrary, HyProcessHandle processHandle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9process_waitfor((J9ProcessHandle) processHandle);
}

IDATA
hystub_process_close(struct HyPortLibrary *portLibrary, HyProcessHandle processHandle, U_32 options)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9process_close((J9ProcessHandle *)&processHandle, options);
}

IDATA
hystub_process_isComplete(struct HyPortLibrary *portLibrary, HyProcessHandle processHandle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9process_isComplete((J9ProcessHandle)processHandle);
}

IDATA
hystub_process_terminate(struct HyPortLibrary *portLibrary, HyProcessHandle processHandle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9process_terminate((J9ProcessHandle)processHandle);
}

IDATA
hystub_process_get_exitCode(struct HyPortLibrary *portLibrary, HyProcessHandle processHandle)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9process_get_exitCode((J9ProcessHandle)processHandle);
}

IDATA
hystub_process_get_available(struct HyPortLibrary *portLibrary, HyProcessHandle processHandle, UDATA flags)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)portLibrary->self_handle);
	return j9process_get_available((J9ProcessHandle)processHandle, flags);
}

J9_CFUNC I_32 
hyport_allocate_library_maptoj9 (struct J9PortLibrary *j9PortLib, struct HyPortLibraryVersion *expectedVersion, struct HyPortLibrary **portLibrary)
{
	PORT_ACCESS_FROM_PORT(j9PortLib);
	struct HyPortLibrary *hyPortLib;
	
	if (omrthread_monitor_init_with_name(&getnameinfo_monitor, 0, "getnameinfo monitor")) {
		return -1;
	}

	*portLibrary = j9mem_allocate_memory(sizeof(struct HyPortLibrary),J9MEM_CATEGORY_HARMONY);
	if (NULL == *portLibrary) {
		omrthread_monitor_destroy(getnameinfo_monitor);
		getnameinfo_monitor = NULL;
		return -1;
	}
	memset(*portLibrary, 0, sizeof(struct HyPortLibrary));
	
	hyPortLib = *portLibrary;
	
	hyPortLib->portVersion.majorVersionNumber = HYPORT_MAJOR_VERSION_NUMBER;
	hyPortLib->portVersion.minorVersionNumber = HYPORT_MINOR_VERSION_NUMBER;
	hyPortLib->portVersion.padding = 0;
	hyPortLib->portVersion.capabilities = HYPORT_CAPABILITY_MASK;
	{
		/* this is an abuse of the portGlobals pointer on this table, if anyone actually uses portGlobals
		 * outside of the port library itself, this will cause big time issues. */
		HyThreadLibraryVersion hyThreadLibraryVersion;
		HYTHREAD_SET_VERSION(&hyThreadLibraryVersion, HYTHREAD_CAPABILITY_MASK);
		hythread_allocate_library(&hyThreadLibraryVersion, (struct HyThreadLibrary **)&(hyPortLib->portGlobals));
	}
	hyPortLib->port_shutdown_library = hystub_port_shutdown_library;      /* port_shutdown_library */
	hyPortLib->port_isFunctionOverridden = hystub_port_isFunctionOverridden;  /* port_isFunctionOverridden */
	hyPortLib->port_tls_free = hystub_port_tls_free;              /* port_tls_free */
	hyPortLib->error_startup = hystub_error_startup;              /* error_startup */
	hyPortLib->error_shutdown = hystub_error_shutdown;             /* error_shutdown */
	hyPortLib->error_last_error_number = hystub_error_last_error_number;    /* error_last_error_number */
	hyPortLib->error_last_error_message = hystub_error_last_error_message;   /* error_last_error_message */
	hyPortLib->error_set_last_error = hystub_error_set_last_error;       /* error_set_last_error */
	hyPortLib->error_set_last_error_with_message = hystub_error_set_last_error_with_message;  /* error_set_last_error_with_message */
	hyPortLib->time_startup = hystub_time_startup;               /* time_startup */
	hyPortLib->time_shutdown = hystub_time_shutdown;              /* time_shutdown */
	hyPortLib->time_msec_clock = hystub_time_msec_clock;            /* time_msec_clock */
	hyPortLib->time_usec_clock = hystub_time_usec_clock;            /* time_usec_clock */
	hyPortLib->time_current_time_millis = hystub_time_current_time_millis;   /* time_current_time_millis */
	hyPortLib->time_hires_clock = hystub_time_hires_clock;           /* time_hires_clock */
	hyPortLib->time_hires_frequency = hystub_time_hires_frequency;       /* time_hires_frequency */
	hyPortLib->time_hires_delta = hystub_time_hires_delta;           /* time_hires_delta */
	hyPortLib->sysinfo_startup = hystub_sysinfo_startup;            /* sysinfo_startup */
	hyPortLib->sysinfo_shutdown = hystub_sysinfo_shutdown;           /* sysinfo_shutdown */
	hyPortLib->sysinfo_get_pid = hystub_sysinfo_get_pid;            /* sysinfo_get_pid */
	hyPortLib->sysinfo_get_physical_memory = hystub_sysinfo_get_physical_memory;        /* sysinfo_get_physical_memory */
	hyPortLib->sysinfo_get_OS_version = hystub_sysinfo_get_OS_version;     /* sysinfo_get_OS_version */
	hyPortLib->sysinfo_get_env = hystub_sysinfo_get_env;            /* sysinfo_get_env */
	hyPortLib->sysinfo_get_CPU_architecture = hystub_sysinfo_get_CPU_architecture;       /* sysinfo_get_CPU_architecture */
	hyPortLib->sysinfo_get_OS_type = hystub_sysinfo_get_OS_type;        /* sysinfo_get_OS_type */
	hyPortLib->sysinfo_get_classpathSeparator = hystub_sysinfo_get_classpathSeparator;     /* sysinfo_get_classpathSeparator */
	hyPortLib->sysinfo_get_executable_name = hystub_sysinfo_get_executable_name;        /* sysinfo_get_executable_name */
	hyPortLib->sysinfo_get_number_CPUs = hystub_sysinfo_get_number_CPUs;    /* sysinfo_get_number_CPUs */
	hyPortLib->sysinfo_get_username = hystub_sysinfo_get_username;       /* sysinfo_get_username */
	hyPortLib->file_startup = hystub_file_startup;               /* file_startup */
	hyPortLib->file_shutdown = hystub_file_shutdown;              /* file_shutdown */
	hyPortLib->file_write = hystub_file_write;                 /* file_write */
	hyPortLib->file_write_text = hystub_file_write_text;            /* file_write_text */
	hyPortLib->file_vprintf = hystub_file_vprintf;               /* file_vprintf */
	hyPortLib->file_printf = hystub_file_printf;                /* file_printf */
	hyPortLib->file_open = hystub_file_open;                  /* file_open */
	hyPortLib->file_close = hystub_file_close;                 /* file_close */
	hyPortLib->file_seek = hystub_file_seek;                  /* file_seek */
	hyPortLib->file_read = hystub_file_read;                  /* file_read */
	hyPortLib->file_unlink = hystub_file_unlink;                /* file_unlink */
	hyPortLib->file_attr = hystub_file_attr;                  /* file_attr */
	hyPortLib->file_lastmod = hystub_file_lastmod;               /* file_lastmod */
	hyPortLib->file_length = hystub_file_length;                /* file_length */
	hyPortLib->file_set_length = hystub_file_set_length;            /* file_set_length */
	hyPortLib->file_sync = hystub_file_sync;                  /* file_sync */
	hyPortLib->sl_startup = hystub_sl_startup;                 /* sl_startup */
	hyPortLib->sl_shutdown = hystub_sl_shutdown;                /* sl_shutdown */
	hyPortLib->sl_close_shared_library = hystub_sl_close_shared_library;    /* sl_close_shared_library */
	hyPortLib->sl_open_shared_library = hystub_sl_open_shared_library;     /* sl_open_shared_library */
	hyPortLib->sl_lookup_name = hystub_sl_lookup_name;             /* sl_lookup_name */
	hyPortLib->tty_startup = hystub_tty_startup;                /* tty_startup */
	hyPortLib->tty_shutdown = hystub_tty_shutdown;               /* tty_shutdown */
	hyPortLib->tty_printf = hystub_tty_printf;                 /* tty_printf */
	hyPortLib->tty_vprintf = hystub_tty_vprintf;                /* tty_vprintf */
	hyPortLib->tty_get_chars = hystub_tty_get_chars;              /* tty_get_chars */
	hyPortLib->tty_err_printf = hystub_tty_err_printf;             /* tty_err_printf */
	hyPortLib->tty_err_vprintf = hystub_tty_err_vprintf;            /* tty_err_vprintf */
	hyPortLib->tty_available = hystub_tty_available;              /* tty_available */
	hyPortLib->mem_startup = hystub_mem_startup;                /* mem_startup */
	hyPortLib->mem_shutdown = hystub_mem_shutdown;               /* mem_shutdown */
	hyPortLib->mem_allocate_memory = hystub_mem_allocate_memory;        /* mem_allocate_memory */
	hyPortLib->mem_allocate_memory_callSite = hystub_mem_allocate_memory_callSite;       /* mem_allocate_memory_callSite */
	hyPortLib->mem_free_memory = hystub_mem_free_memory;            /* mem_free_memory */
	hyPortLib->mem_reallocate_memory = hystub_mem_reallocate_memory;      /* mem_reallocate_memory */
	hyPortLib->cpu_startup = hystub_cpu_startup;                /* cpu_startup */
	hyPortLib->cpu_shutdown = hystub_cpu_shutdown;               /* cpu_shutdown */
	hyPortLib->cpu_flush_icache = hystub_cpu_flush_icache;           /* cpu_flush_icache */
	hyPortLib->vmem_startup = hystub_vmem_startup;               /* vmem_startup */
	hyPortLib->vmem_shutdown = hystub_vmem_shutdown;              /* vmem_shutdown */
	hyPortLib->vmem_commit_memory = hystub_vmem_commit_memory;         /* vmem_commit_memory */
	hyPortLib->vmem_decommit_memory = hystub_vmem_decommit_memory;       /* vmem_decommit_memory */
	hyPortLib->vmem_free_memory = hystub_vmem_free_memory;           /* vmem_free_memory */
	hyPortLib->vmem_reserve_memory = hystub_vmem_reserve_memory;        /* vmem_reserve_memory */
	hyPortLib->vmem_supported_page_sizes = hystub_vmem_supported_page_sizes;  /* vmem_supported_page_sizes */
	hyPortLib->sock_startup = hystub_sock_startup;               /* sock_startup */
	hyPortLib->sock_shutdown = hystub_sock_shutdown;              /* sock_shutdown */
	hyPortLib->sock_htons = hystub_sock_htons;                 /* sock_htons */
	hyPortLib->sock_write = hystub_sock_write;                 /* sock_write */
	hyPortLib->sock_sockaddr = hystub_sock_sockaddr;              /* sock_sockaddr */
	hyPortLib->sock_read = hystub_sock_read;                  /* sock_read */
	hyPortLib->sock_socket = hystub_sock_socket;                /* sock_socket */
	hyPortLib->sock_close = hystub_sock_close;                 /* sock_close */
	hyPortLib->sock_connect = hystub_sock_connect;               /* sock_connect */
	hyPortLib->sock_inetaddr = hystub_sock_inetaddr;              /* sock_inetaddr */
	hyPortLib->sock_gethostbyname = hystub_sock_gethostbyname;         /* sock_gethostbyname */
	hyPortLib->sock_hostent_addrlist = hystub_sock_hostent_addrlist;      /* sock_hostent_addrlist */
	hyPortLib->sock_sockaddr_init = hystub_sock_sockaddr_init;         /* sock_sockaddr_init */
	hyPortLib->sock_linger_init = hystub_sock_linger_init;           /* sock_linger_init */
	hyPortLib->sock_setopt_linger = hystub_sock_setopt_linger;         /* sock_setopt_linger */
	hyPortLib->gp_startup = hystub_gp_startup;                 /* gp_startup */
	hyPortLib->gp_shutdown = hystub_gp_shutdown;                /* gp_shutdown */
	hyPortLib->gp_protect = hystub_gp_protect;                 /* gp_protect */
	hyPortLib->gp_register_handler = hystub_gp_register_handler;        /* gp_register_handler */
	hyPortLib->gp_info = hystub_gp_info;                    /* gp_info */
	hyPortLib->gp_info_count = hystub_gp_info_count;              /* gp_info_count */
	hyPortLib->gp_handler_function = hystub_gp_handler_function;
	hyPortLib->str_startup = hystub_str_startup;                /* str_startup */
	hyPortLib->str_shutdown = hystub_str_shutdown;               /* str_shutdown */
	hyPortLib->str_printf = hystub_str_printf;                 /* str_printf */
	hyPortLib->str_vprintf = hystub_str_vprintf;                /* str_vprintf */
	hyPortLib->exit_startup = hystub_exit_startup;               /* exit_startup */
	hyPortLib->exit_shutdown = hystub_exit_shutdown;              /* exit_shutdown */
	hyPortLib->exit_get_exit_code = hystub_exit_get_exit_code;         /* exit_get_exit_code */
	hyPortLib->exit_shutdown_and_exit = hystub_exit_shutdown_and_exit;     /* exit_shutdown_and_exit */
	hyPortLib->self_handle = PORTLIB;
	hyPortLib->dump_create = hystub_dump_create;                /* dump_create */
	hyPortLib->nls_startup = hystub_nls_startup;                /* nls_startup */
	hyPortLib->nls_shutdown = hystub_nls_shutdown;               /* nls_shutdown */
	hyPortLib->nls_set_catalog = hystub_nls_set_catalog;            /* nls_set_catalog */
	hyPortLib->nls_set_locale = hystub_nls_set_locale;             /* nls_set_locale */
	hyPortLib->nls_get_language = hystub_nls_get_language;           /* nls_get_language */
	hyPortLib->nls_get_region = hystub_nls_get_region;             /* nls_get_region */
	hyPortLib->nls_get_variant = hystub_nls_get_variant;            /* nls_get_variant */
	hyPortLib->nls_printf = hystub_nls_printf;                 /* nls_printf */
	hyPortLib->nls_vprintf = hystub_nls_vprintf;                /* nls_vprintf */
	hyPortLib->nls_lookup_message = hystub_nls_lookup_message;         /* nls_lookup_message */
	hyPortLib->ipcmutex_startup = hystub_ipcmutex_startup;           /* ipcmutex_startup */
	hyPortLib->ipcmutex_shutdown = hystub_ipcmutex_shutdown;          /* ipcmutex_shutdown */
	hyPortLib->ipcmutex_acquire = hystub_ipcmutex_acquire;           /* ipcmutex_acquire */
	hyPortLib->ipcmutex_release = hystub_ipcmutex_release;           /* ipcmutex_release */
	hyPortLib->port_control = hystub_port_control;               /* port_control */
	hyPortLib->sig_startup = hystub_sig_startup;                /* sig_startup */
	hyPortLib->sig_shutdown = hystub_sig_shutdown;               /* sig_shutdown */
	hyPortLib->sig_protect = hystub_sig_protect;                /* sig_protect */
	hyPortLib->sig_can_protect = hystub_sig_can_protect;            /* sig_can_protect */
	hyPortLib->sig_set_async_signal_handler = hystub_sig_set_async_signal_handler;       /* sig_set_async_signal_handler */
	hyPortLib->sig_info = hystub_sig_info;                   /* sig_info */
	hyPortLib->sig_info_count = hystub_sig_info_count;             /* sig_info_count */
	hyPortLib->sig_set_options = hystub_sig_set_options;            /* sig_set_options */
	hyPortLib->sig_get_options = hystub_sig_get_options;            /* sig_get_options */
	hyPortLib->sysinfo_DLPAR_enabled = hystub_sysinfo_DLPAR_enabled;      /* sysinfo_DLPAR_enabled */
	hyPortLib->sysinfo_DLPAR_max_CPUs = hystub_sysinfo_DLPAR_max_CPUs;     /* sysinfo_DLPAR_max_CPUs */
	hyPortLib->sysinfo_weak_memory_consistency = hystub_sysinfo_weak_memory_consistency;    /* sysinfo_weak_memory_consistency */
	hyPortLib->file_read_text = hystub_file_read_text;             /* file_read_text */
	hyPortLib->file_mkdir = hystub_file_mkdir;                 /* file_mkdir */
	hyPortLib->file_move = hystub_file_move;                  /* file_move */
	hyPortLib->file_unlinkdir = hystub_file_unlinkdir;             /* file_unlinkdir */
	hyPortLib->file_findfirst = hystub_file_findfirst;             /* file_findfirst */
	hyPortLib->file_findnext = hystub_file_findnext;              /* file_findnext */
	hyPortLib->file_findclose = hystub_file_findclose;             /* file_findclose */
	hyPortLib->file_error_message = hystub_file_error_message;         /* file_error_message */
	hyPortLib->sock_htonl = hystub_sock_htonl;                 /* sock_htonl */
	hyPortLib->sock_bind = hystub_sock_bind;                  /* sock_bind */
	hyPortLib->sock_accept = hystub_sock_accept;                /* sock_accept */
	hyPortLib->sock_shutdown_input = hystub_sock_shutdown_input;        /* sock_shutdown_input */
	hyPortLib->sock_shutdown_output = hystub_sock_shutdown_output;       /* sock_shutdown_output */
	hyPortLib->sock_listen = hystub_sock_listen;                /* sock_listen */
	hyPortLib->sock_ntohl = hystub_sock_ntohl;                 /* sock_ntohl */
	hyPortLib->sock_ntohs = hystub_sock_ntohs;                 /* sock_ntohs */
	hyPortLib->sock_getpeername = hystub_sock_getpeername;           /* sock_getpeername */
	hyPortLib->sock_getsockname = hystub_sock_getsockname;           /* sock_getsockname */
	hyPortLib->sock_readfrom = hystub_sock_readfrom;              /* sock_readfrom */
	hyPortLib->sock_select = hystub_sock_select;                /* sock_select */
	hyPortLib->sock_writeto = hystub_sock_writeto;               /* sock_writeto */
	hyPortLib->sock_inetntoa = hystub_sock_inetntoa;              /* sock_inetntoa */
	hyPortLib->sock_gethostbyaddr = hystub_sock_gethostbyaddr;         /* sock_gethostbyaddr */
	hyPortLib->sock_gethostname = hystub_sock_gethostname;           /* sock_gethostname */
	hyPortLib->sock_hostent_aliaslist = hystub_sock_hostent_aliaslist;     /* sock_hostent_aliaslist */
	hyPortLib->sock_hostent_hostname = hystub_sock_hostent_hostname;      /* sock_hostent_hostname */
	hyPortLib->sock_sockaddr_port = hystub_sock_sockaddr_port;         /* sock_sockaddr_port */
	hyPortLib->sock_sockaddr_address = hystub_sock_sockaddr_address;      /* sock_sockaddr_address */
	hyPortLib->sock_fdset_init = hystub_sock_fdset_init;            /* sock_fdset_init */
	hyPortLib->sock_fdset_size = hystub_sock_fdset_size;            /* sock_fdset_size */
	hyPortLib->sock_timeval_init = hystub_sock_timeval_init;          /* sock_timeval_init */
	hyPortLib->sock_getopt_int = hystub_sock_getopt_int;            /* sock_getopt_int */
	hyPortLib->sock_setopt_int = hystub_sock_setopt_int;            /* sock_setopt_int */
	hyPortLib->sock_getopt_bool = hystub_sock_getopt_bool;           /* sock_getopt_bool */
	hyPortLib->sock_setopt_bool = hystub_sock_setopt_bool;           /* sock_setopt_bool */
	hyPortLib->sock_getopt_byte = hystub_sock_getopt_byte;           /* sock_getopt_byte */
	hyPortLib->sock_setopt_byte = hystub_sock_setopt_byte;           /* sock_setopt_byte */
	hyPortLib->sock_getopt_linger = hystub_sock_getopt_linger;         /* sock_getopt_linger */
	hyPortLib->sock_getopt_sockaddr = hystub_sock_getopt_sockaddr;       /* sock_getopt_sockaddr */
	hyPortLib->sock_setopt_sockaddr = hystub_sock_setopt_sockaddr;       /* sock_setopt_sockaddr */
	hyPortLib->sock_setopt_ipmreq = hystub_sock_setopt_ipmreq;         /* sock_setopt_ipmreq */
	hyPortLib->sock_linger_enabled = hystub_sock_linger_enabled;        /* sock_linger_enabled */
	hyPortLib->sock_linger_linger = hystub_sock_linger_linger;         /* sock_linger_linger */
	hyPortLib->sock_ipmreq_init = hystub_sock_ipmreq_init;           /* sock_ipmreq_init */
	hyPortLib->sock_setflag = hystub_sock_setflag;               /* sock_setflag */
	hyPortLib->sock_freeaddrinfo = hystub_sock_freeaddrinfo;          /* sock_freeaddrinfo */
	hyPortLib->sock_getaddrinfo = hystub_sock_getaddrinfo;           /* sock_getaddrinfo */
	hyPortLib->sock_getaddrinfo_address = hystub_sock_getaddrinfo_address;   /* sock_getaddrinfo_address */
	hyPortLib->sock_getaddrinfo_create_hints = hystub_sock_getaddrinfo_create_hints;      /* sock_getaddrinfo_create_hints */
	hyPortLib->sock_getaddrinfo_family = hystub_sock_getaddrinfo_family;    /* sock_getaddrinfo_family */
	hyPortLib->sock_getaddrinfo_length = hystub_sock_getaddrinfo_length;    /* sock_getaddrinfo_length */
	hyPortLib->sock_getaddrinfo_name = hystub_sock_getaddrinfo_name;      /* sock_getaddrinfo_name */
	hyPortLib->sock_getnameinfo = hystub_sock_getnameinfo;           /* sock_getnameinfo */
	hyPortLib->sock_ipv6_mreq_init = hystub_sock_ipv6_mreq_init;        /* sock_ipv6_mreq_init */
	hyPortLib->sock_setopt_ipv6_mreq = hystub_sock_setopt_ipv6_mreq;      /* sock_setopt_ipv6_mreq */
	hyPortLib->sock_sockaddr_address6 = hystub_sock_sockaddr_address6;     /* sock_sockaddr_address6 */
	hyPortLib->sock_sockaddr_family = hystub_sock_sockaddr_family;       /* sock_sockaddr_family */
	hyPortLib->sock_sockaddr_init6 = hystub_sock_sockaddr_init6;        /* sock_sockaddr_init6 */
	hyPortLib->sock_socketIsValid = hystub_sock_socketIsValid;         /* sock_socketIsValid */
	hyPortLib->sock_select_read = hystub_sock_select_read;           /* sock_select_read */
	hyPortLib->sock_set_nonblocking = hystub_sock_set_nonblocking;       /* sock_set_nonblocking */
	hyPortLib->sock_error_message = hystub_sock_error_message;         /* sock_error_message */
	hyPortLib->sock_get_network_interfaces = hystub_sock_get_network_interfaces;        /* sock_get_network_interfaces */
	hyPortLib->sock_free_network_interface_struct = hystub_sock_free_network_interface_struct; /* sock_free_network_interface_struct */
	hyPortLib->sock_connect_with_timeout = hystub_sock_connect_with_timeout;  /* sock_connect_with_timeout */
	hyPortLib->str_ftime = hystub_strftime;                   /* str_ftime */
	hyPortLib->mmap_startup = hystub_mmap_startup;               /* mmap_startup */
	hyPortLib->mmap_shutdown = hystub_mmap_shutdown;              /* mmap_shutdown */
	hyPortLib->mmap_capabilities = hystub_mmap_capabilities;          /* mmap_capabilities */
	hyPortLib->mmap_map_file = hystub_mmap_map_file;              /* mmap_map_file */
	hyPortLib->mmap_unmap_file = hystub_mmap_unmap_file;            /* mmap_unmap_file */
	hyPortLib->shsem_startup = hystub_shsem_startup;              /* shsem_startup */
	hyPortLib->shsem_shutdown = hystub_shsem_shutdown;             /* shsem_shutdown */
	hyPortLib->shsem_open = hystub_shsem_open;                 /* shsem_open */
	hyPortLib->shsem_post = hystub_shsem_post;                 /* shsem_post */
	hyPortLib->shsem_wait = hystub_shsem_wait;                 /* shsem_wait */
	hyPortLib->shsem_getVal = hystub_shsem_getVal;               /* shsem_getVal */
	hyPortLib->shsem_setVal = hystub_shsem_setVal;               /* shsem_setVal */
	hyPortLib->shsem_close = hystub_shsem_close;                /* shsem_close */
	hyPortLib->shsem_destroy = hystub_shsem_destroy;              /* shsem_destroy */
	hyPortLib->shmem_startup = hystub_shmem_startup;              /* shmem_startup */
	hyPortLib->shmem_shutdown = hystub_shmem_shutdown;             /* shmem_shutdown */
	hyPortLib->shmem_open = hystub_shmem_open;                 /*shmem_open */
	hyPortLib->shmem_attach = hystub_shmem_attach;               /*shmem_attach */
	hyPortLib->shmem_detach = hystub_shmem_detach;               /*shmem_detach */
	hyPortLib->shmem_close = hystub_shmem_close;                /*shmem_close */
	hyPortLib->shmem_destroy = hystub_shmem_destroy;              /*shmem_destroy */
	hyPortLib->shmem_findfirst = hystub_shmem_findfirst;            /*shmem_findfirst */
	hyPortLib->shmem_findnext = hystub_shmem_findnext;             /* shmem_findnext */
	hyPortLib->shmem_findclose = hystub_shmem_findclose;            /* shmem_findclose */
	hyPortLib->shmem_stat = hystub_shmem_stat;                 /* shmem_stat */
	hyPortLib->sysinfo_get_processing_capacity = hystub_sysinfo_get_processing_capacity;    /* sysinfo_get_processing_capacity */
	hyPortLib->buf_write_text = hystub_buf_write_text;            /* buf_write_text */
	hyPortLib->port_get_thread_library = hystub_port_get_thread_library;    /* port_get_thread_library */
	hyPortLib->sock_fdset_zero = hystub_sock_fdset_zero;
	hyPortLib->sock_fdset_set = hystub_sock_fdset_set;
	hyPortLib->process_create = hystub_process_create;	/* process_create */
	hyPortLib->process_getStream = hystub_process_getStream;	/* process_getStream */
	hyPortLib->process_waitfor = hystub_process_waitfor;	/* process_waitfor */
	hyPortLib->process_close = hystub_process_close;	/* process_close */
	hyPortLib->process_isComplete = hystub_process_isComplete;	/* process_isComplete */
	hyPortLib->process_terminate = hystub_process_terminate;	/* process_terminate */		
	hyPortLib->process_get_exitCode = hystub_process_get_exitCode;	/* process_get_exitCode */
	hyPortLib->process_get_available = hystub_process_get_available;	/* process_get_available */

	return 0;
}
