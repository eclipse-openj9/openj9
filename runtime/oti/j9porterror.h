/*******************************************************************************
 * Copyright (c) 1998, 2017 IBM Corp. and others
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
 * @brief Port Library Error Codes
 *
 * When an error is reported by the operating system the port library must translate this OS specific error code to a 
 * portable error code.  All portable error codes are negative numbers.  Not every module of the port library will have
 * error codes dedicated to it's own use, some will use the generic portable error code values.
 *
 * Errors reported by the OS may be recorded by calling the port library functions @ref j9error.c::j9error_set_last_error "j9error_set_last_error()"
 * or @ref j9error.c::j9error_set_last_error_with_message "j9error_set_last_error_with_message()".  The mapping of the OS specific error
 * code to a portable error code is the responsibility of the calling module.
 *
 * Note that omr port uses range -1 to -499.
 */
#ifndef j9porterror_h
#define j9porterror_h

#include "omrporterror.h"

/** 
 * @name Port Library startup failure code
 * Failures related to the initialization and startup of the port library.
 *
 * @internal J9PORT_ERROR_STARTUP_EXTENDED* range from -500 to -549 to avoid overlap
 * @{
 */

#define J9PORT_ERROR_STARTUP_EXTENDED -500
#define J9PORT_ERROR_STARTUP_IPCMUTEX (J9PORT_ERROR_STARTUP_EXTENDED)
#define J9PORT_ERROR_STARTUP_SOCK (J9PORT_ERROR_STARTUP_EXTENDED -1)
#define J9PORT_ERROR_STARTUP_GP (J9PORT_ERROR_STARTUP_EXTENDED -2)
#define J9PORT_ERROR_STARTUP_SHSEM (J9PORT_ERROR_STARTUP_EXTENDED -3)
#define J9PORT_ERROR_STARTUP_SHMEM (J9PORT_ERROR_STARTUP_EXTENDED -4)
#define J9PORT_ERROR_STARTUP_SHMEM_MOSERVICE (J9PORT_ERROR_STARTUP_EXTENDED -5)

/** @} */

/**
 * @name Shared Semaphore Errors
 * Error codes for shared semaphore operations.
 *
 * @internal J9PORT_ERROR_SHSEM* range from at -150 to -169.  to match hyporterror.h codes.
 * Overlaps with omrporterror.h
 * @{
 */
#define J9PORT_ERROR_SHSEM_BASE -150
#define J9PORT_ERROR_SHSEM_OPFAILED (J9PORT_ERROR_SHSEM_BASE)
#define J9PORT_ERROR_SHSEM_HANDLE_INVALID (J9PORT_ERROR_SHSEM_BASE-1)
#define J9PORT_ERROR_SHSEM_SEMSET_INVALID (J9PORT_ERROR_SHSEM_BASE-2)
#define J9PORT_ERROR_SHSEM_OPFAILED_DONT_UNLINK (J9PORT_ERROR_SHSEM_BASE-3)
#define J9PORT_ERROR_SHSEM_OPFAILED_CONTROL_FILE_CORRUPT (J9PORT_ERROR_SHSEM_BASE-4)
#define J9PORT_ERROR_SHSEM_OPFAILED_SEMID_MISMATCH (J9PORT_ERROR_SHSEM_BASE-5)
#define J9PORT_ERROR_SHSEM_OPFAILED_SEM_KEY_MISMATCH (J9PORT_ERROR_SHSEM_BASE-6)
#define J9PORT_ERROR_SHSEM_OPFAILED_SEM_SIZE_CHECK_FAILED (J9PORT_ERROR_SHSEM_BASE-7)
#define J9PORT_ERROR_SHSEM_OPFAILED_SEM_MARKER_CHECK_FAILED (J9PORT_ERROR_SHSEM_BASE-8)
#define J9PORT_ERROR_SHSEM_DATA_DIRECTORY_FAILED (J9PORT_ERROR_SHSEM_BASE-9)
#define J9PORT_ERROR_SHSEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT (J9PORT_ERROR_SHSEM_BASE-10)
#define J9PORT_ERROR_SHSEM_STAT_BUFFER_INVALID (J9PORT_ERROR_SHSEM_BASE-11)
#define J9PORT_ERROR_SHSEM_STAT_FAILED (J9PORT_ERROR_SHSEM_BASE-12)
#define J9PORT_ERROR_SHSEM_OPFAILED_SEMAPHORE_NOT_FOUND (J9PORT_ERROR_SHSEM_BASE-13)
#define J9PORT_ERROR_SHSEM_OPFAILED_CONTROL_FILE_LOCK_FAILED (J9PORT_ERROR_SHSEM_BASE-14)

/** @} */

/**
 * @name Shared Memory Errors
 * Error codes for shared memory semaphore operations.
 *
 * @internal J9PORT_ERROR_SHMEM* range from at -170 to -189 to match hyporterror.h codes.
 * Overlaps with omrporterror.h
 * @{
 */
#define J9PORT_ERROR_SHMEM_BASE -170
#define J9PORT_ERROR_SHMEM_OPFAILED (J9PORT_ERROR_SHMEM_BASE)
#define J9PORT_ERROR_SHMEM_OPEN_ATTACHED_FAILED (J9PORT_ERROR_SHMEM_BASE-1)
#define J9PORT_ERROR_SHMEM_CREATE_ATTACHED_FAILED (J9PORT_ERROR_SHMEM_BASE-2)
#define J9PORT_ERROR_SHMEM_NOSPACE (J9PORT_ERROR_SHMEM_BASE-3)
#define J9PORT_ERROR_SHMEM_TOOBIG (J9PORT_ERROR_SHMEM_BASE-4)
#define J9PORT_ERROR_SHMEM_DATA_DIRECTORY_FAILED (J9PORT_ERROR_SHMEM_BASE-5)
#define J9PORT_ERROR_SHMEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT (J9PORT_ERROR_SHMEM_BASE-6)
#define J9PORT_ERROR_SHMEM_ZOS_STORAGE_KEY_READONLY (J9PORT_ERROR_SHMEM_BASE-7)
#define J9PORT_ERROR_SHMEM_OPFAILED_CONTROL_FILE_CORRUPT (J9PORT_ERROR_SHMEM_BASE-8)
#define J9PORT_ERROR_SHMEM_OPFAILED_SHMID_MISMATCH (J9PORT_ERROR_SHMEM_BASE-9)
#define J9PORT_ERROR_SHMEM_OPFAILED_SHM_KEY_MISMATCH (J9PORT_ERROR_SHMEM_BASE-10)
#define J9PORT_ERROR_SHMEM_OPFAILED_SHM_GROUPID_CHECK_FAILED (J9PORT_ERROR_SHMEM_BASE-11)
#define J9PORT_ERROR_SHMEM_OPFAILED_SHM_USERID_CHECK_FAILED (J9PORT_ERROR_SHMEM_BASE-12)
#define J9PORT_ERROR_SHMEM_OPFAILED_SHM_SIZE_CHECK_FAILED (J9PORT_ERROR_SHMEM_BASE-13)
#define J9PORT_ERROR_SHMEM_HANDLE_INVALID (J9PORT_ERROR_SHMEM_BASE-14)
#define J9PORT_ERROR_SHMEM_STAT_BUFFER_INVALID (J9PORT_ERROR_SHMEM_BASE-15)
#define J9PORT_ERROR_SHMEM_STAT_FAILED (J9PORT_ERROR_SHMEM_BASE-16)
#define J9PORT_ERROR_SHMEM_OPFAILED_SHARED_MEMORY_NOT_FOUND (J9PORT_ERROR_SHMEM_BASE-17)
#define J9PORT_ERROR_SHMEM_OPFAILED_CONTROL_FILE_LOCK_FAILED (J9PORT_ERROR_SHMEM_BASE-18)

/** @} */

/**
 * @name Socket Errors
 * Error codes for socket operations
 *
 * @internal J9PORT_ERROR_SOCKET* range from -200 to -299 to match hyporterror.h codes.
 * Overlaps with omrporterror.h
 * @{
 */
#define J9PORT_ERROR_SOCKET_BASE -200
#define J9PORT_ERROR_SOCKET_BADSOCKET J9PORT_ERROR_SOCKET_BASE						/* generic error */
#define J9PORT_ERROR_SOCKET_NOTINITIALIZED (J9PORT_ERROR_SOCKET_BASE-1)				/* socket library uninitialized */
#define J9PORT_ERROR_SOCKET_BADAF (J9PORT_ERROR_SOCKET_BASE-2)						/* bad address family */
#define J9PORT_ERROR_SOCKET_BADPROTO (J9PORT_ERROR_SOCKET_BASE-3)					/* bad protocol */
#define J9PORT_ERROR_SOCKET_BADTYPE (J9PORT_ERROR_SOCKET_BASE-4)					/* bad type */
#define J9PORT_ERROR_SOCKET_SYSTEMBUSY (J9PORT_ERROR_SOCKET_BASE-5)					/* system busy handling requests */
#define J9PORT_ERROR_SOCKET_SYSTEMFULL (J9PORT_ERROR_SOCKET_BASE-6)					/* too many sockets */
#define J9PORT_ERROR_SOCKET_NOTCONNECTED (J9PORT_ERROR_SOCKET_BASE-7)				/* socket is not connected */
#define J9PORT_ERROR_SOCKET_INTERRUPTED	(J9PORT_ERROR_SOCKET_BASE-8)				/* the call was cancelled */
#define J9PORT_ERROR_SOCKET_TIMEOUT	(J9PORT_ERROR_SOCKET_BASE-9)					/* the operation timed out */
#define J9PORT_ERROR_SOCKET_CONNRESET (J9PORT_ERROR_SOCKET_BASE-10)					/* the connection was reset */
#define J9PORT_ERROR_SOCKET_WOULDBLOCK	( J9PORT_ERROR_SOCKET_BASE-11)				/* the socket is marked as nonblocking operation would block */
#define J9PORT_ERROR_SOCKET_ADDRNOTAVAIL (J9PORT_ERROR_SOCKET_BASE-12)				/* address not available */
#define J9PORT_ERROR_SOCKET_ADDRINUSE (J9PORT_ERROR_SOCKET_BASE-13)					/* address already in use */
#define J9PORT_ERROR_SOCKET_NOTBOUND (J9PORT_ERROR_SOCKET_BASE-14)					/* the socket is not bound */
#define J9PORT_ERROR_SOCKET_UNKNOWNSOCKET (J9PORT_ERROR_SOCKET_BASE-15)				/* resolution of fileDescriptor to socket failed */
#define J9PORT_ERROR_SOCKET_INVALIDTIMEOUT (J9PORT_ERROR_SOCKET_BASE-16)			/* the specified timeout is invalid */
#define J9PORT_ERROR_SOCKET_FDSETFULL (J9PORT_ERROR_SOCKET_BASE-17)					/* Unable to create an FDSET */
#define J9PORT_ERROR_SOCKET_TIMEVALFULL (J9PORT_ERROR_SOCKET_BASE-18)				/* Unable to create a TIMEVAL */
#define J9PORT_ERROR_SOCKET_REMSOCKSHUTDOWN (J9PORT_ERROR_SOCKET_BASE-19)			/* The remote socket has shutdown gracefully */
#define J9PORT_ERROR_SOCKET_NOTLISTENING (J9PORT_ERROR_SOCKET_BASE-20)				/* listen() was not invoked prior to accept() */
#define J9PORT_ERROR_SOCKET_NOTSTREAMSOCK (J9PORT_ERROR_SOCKET_BASE-21)				/* The socket does not support connection-oriented service */
#define J9PORT_ERROR_SOCKET_ALREADYBOUND (J9PORT_ERROR_SOCKET_BASE-22)				/* The socket is already bound to an address */
#define J9PORT_ERROR_SOCKET_NBWITHLINGER (J9PORT_ERROR_SOCKET_BASE-23)				/* The socket is marked non-blocking & SO_LINGER is non-zero */
#define J9PORT_ERROR_SOCKET_ISCONNECTED (J9PORT_ERROR_SOCKET_BASE-24)				/* The socket is already connected */
#define J9PORT_ERROR_SOCKET_NOBUFFERS (J9PORT_ERROR_SOCKET_BASE-25)					/* No buffer space is available */
#define J9PORT_ERROR_SOCKET_HOSTNOTFOUND (J9PORT_ERROR_SOCKET_BASE-26)				/* Authoritative Answer Host not found */
#define J9PORT_ERROR_SOCKET_NODATA (J9PORT_ERROR_SOCKET_BASE-27)					/* Valid name, no data record of requested type */
#define J9PORT_ERROR_SOCKET_BOUNDORCONN (J9PORT_ERROR_SOCKET_BASE-28)				/* The socket has not been bound or is already connected */
#define J9PORT_ERROR_SOCKET_OPNOTSUPP (J9PORT_ERROR_SOCKET_BASE-29)					/* The socket does not support the operation */
#define J9PORT_ERROR_SOCKET_OPTUNSUPP (J9PORT_ERROR_SOCKET_BASE-30)					/* The socket option is not supported */
#define J9PORT_ERROR_SOCKET_OPTARGSINVALID (J9PORT_ERROR_SOCKET_BASE-31)			/* The socket option arguments are invalid */
#define J9PORT_ERROR_SOCKET_SOCKLEVELINVALID (J9PORT_ERROR_SOCKET_BASE-32)			/* The socket level is invalid */
#define J9PORT_ERROR_SOCKET_TIMEOUTFAILURE (J9PORT_ERROR_SOCKET_BASE-33)			
#define J9PORT_ERROR_SOCKET_SOCKADDRALLOCFAIL (J9PORT_ERROR_SOCKET_BASE-34)			/* Unable to allocate the sockaddr structure */
#define J9PORT_ERROR_SOCKET_FDSET_SIZEBAD (J9PORT_ERROR_SOCKET_BASE-35)				/* The calculated maximum size of the file descriptor set is bad */
#define J9PORT_ERROR_SOCKET_UNKNOWNFLAG (J9PORT_ERROR_SOCKET_BASE-36)				/* The flag is unknown */
#define J9PORT_ERROR_SOCKET_MSGSIZE (J9PORT_ERROR_SOCKET_BASE-37)					/* The datagram was too big to fit the specified buffer & was truncated. */
#define J9PORT_ERROR_SOCKET_NORECOVERY (J9PORT_ERROR_SOCKET_BASE-38)				/* The operation failed with no recovery possible */
#define J9PORT_ERROR_SOCKET_ARGSINVALID (J9PORT_ERROR_SOCKET_BASE-39)				/* The arguments are invalid */
#define J9PORT_ERROR_SOCKET_BADDESC (J9PORT_ERROR_SOCKET_BASE-40)					/* The socket argument is not a valid file descriptor */
#define J9PORT_ERROR_SOCKET_NOTSOCK (J9PORT_ERROR_SOCKET_BASE-41)					/* The socket argument is not a socket */
#define J9PORT_ERROR_SOCKET_HOSTENTALLOCFAIL (J9PORT_ERROR_SOCKET_BASE-42)			/* Unable to allocate the hostent structure */
#define J9PORT_ERROR_SOCKET_TIMEVALALLOCFAIL (J9PORT_ERROR_SOCKET_BASE-43)			/* Unable to allocate the timeval structure */
#define J9PORT_ERROR_SOCKET_LINGERALLOCFAIL (J9PORT_ERROR_SOCKET_BASE-44)			/* Unable to allocate the linger structure */
#define J9PORT_ERROR_SOCKET_IPMREQALLOCFAIL (J9PORT_ERROR_SOCKET_BASE-45)			/* Unable to allocate the ipmreq structure */
#define J9PORT_ERROR_SOCKET_FDSETALLOCFAIL (J9PORT_ERROR_SOCKET_BASE-46)			/* Unable to allocate the fdset structure */
#define J9PORT_ERROR_SOCKET_OPFAILED (J9PORT_ERROR_SOCKET_BASE-47)
#define J9PORT_ERROR_SOCKET_VALUE_NULL (J9PORT_ERROR_SOCKET_BASE-48) 				/* The value indexed was NULL */
#define J9PORT_ERROR_SOCKET_CONNECTION_REFUSED	( J9PORT_ERROR_SOCKET_BASE-49)		/* connection was refused */
#define J9PORT_ERROR_SOCKET_ENETUNREACH (J9PORT_ERROR_SOCKET_BASE-50)				/* network is not reachable */
#define J9PORT_ERROR_SOCKET_EACCES (J9PORT_ERROR_SOCKET_BASE-51)					/* permissions do not allow action on socket */
#define J9PORT_ERROR_SOCKET_WAS_CLOSED (J9PORT_ERROR_SOCKET_BASE-52)				/* the socket was closed (set to NULL or INVALID) */
#define J9PORT_ERROR_SOCKET_EINPROGRESS (J9PORT_ERROR_SOCKET_BASE-53)				/* Non-blocking socket could not connect immediately, connect is happening asynchronously */
#define J9PORT_ERROR_SOCKET_ALREADYINPROGRESS (J9PORT_ERROR_SOCKET_BASE-54)			/* A connection attempt is already in progress on the socket */
#define J9PORT_ERROR_SOCKET_NOSR (J9PORT_ERROR_SOCKET_BASE-55)						/* Insufficient streams resources for the operation to complete */
#define J9PORT_ERROR_SOCKET_IO  (J9PORT_ERROR_SOCKET_BASE-56)						/* I/O error occurred either in making dir entry or allocation inode. */
#define J9PORT_ERROR_SOCKET_ISDIR  (J9PORT_ERROR_SOCKET_BASE-57)					/* Null pathname is specified. */
#define J9PORT_ERROR_SOCKET_LOOP  (J9PORT_ERROR_SOCKET_BASE-58)						/* Too many symbolic names were encountered in translating the pathname. */
#define J9PORT_ERROR_SOCKET_NOENT (J9PORT_ERROR_SOCKET_BASE-59)						/* Component of the path prefix of the pathname does not exist */
#define J9PORT_ERROR_SOCKET_NOTDIR (J9PORT_ERROR_SOCKET_BASE-60)					/* Component of the path prefix of the pathname is not a directory */
#define J9PORT_ERROR_SOCKET_ROFS (J9PORT_ERROR_SOCKET_BASE-61)						/* The inode would reside on a read-only file system */
#define J9PORT_ERROR_SOCKET_NOMEM (J9PORT_ERROR_SOCKET_BASE-62)						/* Insufficient kernel memory was available. */
#define J9PORT_ERROR_SOCKET_NAMETOOLONG (J9PORT_ERROR_SOCKET_BASE-63)				/* Addr name is too long */

/* Palm OS6 uses a hybrid error reporting mechanism.  Need to be able to distinguish socket
 * errors from all other errors (yuck this is really awful).
 */
#define J9PORT_ERROR_SOCKET_FIRST_ERROR_NUMBER J9PORT_ERROR_SOCKET_BASE
#define J9PORT_ERROR_SOCKET_LAST_ERROR_NUMBER J9PORT_ERROR_SOCKET_VALUE_NULL /* Equals last used error code */

/** @} */

/**
 * @name process Errors 
 * Error codes for process operations.
 *
 * @internal J9PORT_ERROR_PROCESS* range from -700 to -749 avoid overlap
 * @{
 */
#define J9PORT_ERROR_PROCESS_BASE -700
#define J9PORT_ERROR_PROCESS_OPFAILED (J9PORT_ERROR_PROCESS_BASE)
#define J9PORT_ERROR_PROCESS_CLOSE_PROCHANDLE (J9PORT_ERROR_PROCESS_BASE-1)
#define J9PORT_ERROR_PROCESS_CLOSE_INHANDLE (J9PORT_ERROR_PROCESS_BASE-2)
#define J9PORT_ERROR_PROCESS_CLOSE_OUTHANDLE (J9PORT_ERROR_PROCESS_BASE-3)
#define J9PORT_ERROR_PROCESS_CLOSE_ERRHANDLE (J9PORT_ERROR_PROCESS_BASE-4)
#define J9PORT_ERROR_PROCESS_FREEMEMORY (J9PORT_ERROR_PROCESS_BASE-5)
#define J9PORT_ERROR_PROCESS_INVALID_STREAMFLAG (J9PORT_ERROR_PROCESS_BASE-6)
#define J9PORT_ERROR_PROCESS_INVALID_STREAMHANDLE (J9PORT_ERROR_PROCESS_BASE-7)
#define J9PORT_ERROR_PROCESS_INVALID_PARAMS (J9PORT_ERROR_PROCESS_BASE-8)
#define J9PORT_ERROR_PROCESS_NOPERMISSION (J9PORT_ERROR_PROCESS_BASE-9)
#define J9PORT_ERROR_PROCESS_COMMAND_LENGTH_ZERO (J9PORT_ERROR_PROCESS_BASE-10)

/** @} */

/**
 * @name System V IPC Errors
 * Unix System V IPC function call errno mappings
 * @internal
 * System V errno's for 8 sysv functions:
 * 1 General Function: ftok
 * 3 Semaphore Functions: semget, semctl, semop
 * 4 Memory Functions: shmget, shmctl, shmat, shmdt
 * 
 * The errors are meant to be a 32 bit integer.
 * The most significant 16 bits will identify the failing function 
 * The least significant 16 bits will identify the errno that occured while calling the function
 * 
 * 9 ranges will be used to represent errors starting at the below addresses:
 * 	0xFFFDxxxx for ftok
 * 	0xFFFCxxxx for semget
 * 	0xFFFBxxxx for semctl
 * 	0xFFFAxxxx for semop
 * 	0xFFF9xxxx for shmget
 * 	0xFFF8xxxx for shmctl
 * 	0xFFF7xxxx for shmat
 * 	0xFFF6xxxx for shmdt
 * 	0xFFF5xxxx for __getipc (zOS)
 * @{
 */
#define J9PORT_ERROR_SYSV_IPC_FTOK_ERROR ((I_32)((U_32)-2 << J9PORT_ERROR_SYSTEM_CALL_CODE_SHIFT))
#define J9PORT_ERROR_SYSV_IPC_SEMGET_ERROR ((I_32)((U_32)-3 << J9PORT_ERROR_SYSTEM_CALL_CODE_SHIFT))
#define J9PORT_ERROR_SYSV_IPC_SEMCTL_ERROR ((I_32)((U_32)-4 << J9PORT_ERROR_SYSTEM_CALL_CODE_SHIFT))
#define J9PORT_ERROR_SYSV_IPC_SEMOP_ERROR ((I_32)((U_32)-5 << J9PORT_ERROR_SYSTEM_CALL_CODE_SHIFT))
#define J9PORT_ERROR_SYSV_IPC_SHMGET_ERROR ((I_32)((U_32)-6 << J9PORT_ERROR_SYSTEM_CALL_CODE_SHIFT))
#define J9PORT_ERROR_SYSV_IPC_SHMCTL_ERROR ((I_32)((U_32)-7 << J9PORT_ERROR_SYSTEM_CALL_CODE_SHIFT))
#define J9PORT_ERROR_SYSV_IPC_SHMAT_ERROR ((I_32)((U_32)-8 << J9PORT_ERROR_SYSTEM_CALL_CODE_SHIFT))
#define J9PORT_ERROR_SYSV_IPC_SHMDT_ERROR ((I_32)((U_32)-9 << J9PORT_ERROR_SYSTEM_CALL_CODE_SHIFT))
#define J9PORT_ERROR_SYSV_IPC_GETIPC_ERROR ((I_32)((U_32)-10 << J9PORT_ERROR_SYSTEM_CALL_CODE_SHIFT))

#define J9PORT_ERROR_SYSV_IPC_ERRNO_BASE -750
#define J9PORT_ERROR_SYSV_IPC_ERRNO_EACCES (J9PORT_ERROR_SYSV_IPC_ERRNO_BASE)
#define J9PORT_ERROR_SYSV_IPC_ERRNO_EEXIST (J9PORT_ERROR_SYSV_IPC_ERRNO_BASE-1)
#define J9PORT_ERROR_SYSV_IPC_ERRNO_ENOENT (J9PORT_ERROR_SYSV_IPC_ERRNO_BASE-2)
#define J9PORT_ERROR_SYSV_IPC_ERRNO_EINVAL (J9PORT_ERROR_SYSV_IPC_ERRNO_BASE-3)
#define J9PORT_ERROR_SYSV_IPC_ERRNO_ENOMEM (J9PORT_ERROR_SYSV_IPC_ERRNO_BASE-4)
#define J9PORT_ERROR_SYSV_IPC_ERRNO_ENOSPC (J9PORT_ERROR_SYSV_IPC_ERRNO_BASE-5)
#define J9PORT_ERROR_SYSV_IPC_ERRNO_ELOOP (J9PORT_ERROR_SYSV_IPC_ERRNO_BASE-6)
#define J9PORT_ERROR_SYSV_IPC_ERRNO_ENAMETOOLONG (J9PORT_ERROR_SYSV_IPC_ERRNO_BASE-7) 
#define J9PORT_ERROR_SYSV_IPC_ERRNO_ENOTDIR (J9PORT_ERROR_SYSV_IPC_ERRNO_BASE-8)
#define J9PORT_ERROR_SYSV_IPC_ERRNO_EPERM (J9PORT_ERROR_SYSV_IPC_ERRNO_BASE-9)
#define J9PORT_ERROR_SYSV_IPC_ERRNO_ERANGE (J9PORT_ERROR_SYSV_IPC_ERRNO_BASE-10)
#define J9PORT_ERROR_SYSV_IPC_ERRNO_E2BIG (J9PORT_ERROR_SYSV_IPC_ERRNO_BASE-11)
#define J9PORT_ERROR_SYSV_IPC_ERRNO_EAGAIN (J9PORT_ERROR_SYSV_IPC_ERRNO_BASE-12)
#define J9PORT_ERROR_SYSV_IPC_ERRNO_EFBIG (J9PORT_ERROR_SYSV_IPC_ERRNO_BASE-13)
#define J9PORT_ERROR_SYSV_IPC_ERRNO_EIDRM (J9PORT_ERROR_SYSV_IPC_ERRNO_BASE-14)
#define J9PORT_ERROR_SYSV_IPC_ERRNO_EINTR (J9PORT_ERROR_SYSV_IPC_ERRNO_BASE-15)
#define J9PORT_ERROR_SYSV_IPC_ERRNO_EMFILE (J9PORT_ERROR_SYSV_IPC_ERRNO_BASE-16)
#define J9PORT_ERROR_SYSV_IPC_ERRNO_UNMAPPED (J9PORT_ERROR_SYSV_IPC_ERRNO_BASE-17)
#define J9PORT_ERROR_SYSV_IPC_ERRNO_END J9PORT_ERROR_SYSV_IPC_ERRNO_UNMAPPED
/** @} */

/**
 * @name j9hypervisor Errors
 * Error code returned by the j9hypervisor API
 *
 * @internal J9PORT_ERROR_HYPERVISOR* range from -800 to -849 avoid overlap
 * @{
 */
#define J9PORT_ERROR_HYPERVISOR_BASE -800
#define J9PORT_ERROR_HYPERVISOR_CROSSGUEST_MEMORY_UNINIT (J9PORT_ERROR_HYPERVISOR_BASE)
#define J9PORT_ERROR_HYPERVISOR_BAD_HDR_MAGIC (J9PORT_ERROR_HYPERVISOR_BASE - 1)
#define J9PORT_ERROR_HYPERVISOR_BAD_HDR_SIZE (J9PORT_ERROR_HYPERVISOR_BASE - 2)
#define J9PORT_ERROR_HYPERVISOR_MAPPINGFAILED (J9PORT_ERROR_HYPERVISOR_BASE - 3)
#define J9PORT_ERROR_HYPERVISOR_MSYNC_FAILED (J9PORT_ERROR_HYPERVISOR_BASE - 4)
#define J9PORT_ERROR_HYPERVISOR_OPFAILED (J9PORT_ERROR_HYPERVISOR_BASE - 5)
#define J9PORT_ERROR_HYPERVISOR_UNSUPPORTED (J9PORT_ERROR_HYPERVISOR_BASE - 6)
#define J9PORT_ERROR_HYPERVISOR_NO_HYPERVISOR (J9PORT_ERROR_HYPERVISOR_BASE - 7)
#define J9PORT_ERROR_HYPERVISOR_API_UNAVAILABLE (J9PORT_ERROR_HYPERVISOR_BASE - 8)
#define J9PORT_ERROR_HYPERVISOR_CPUINFO_READ_FAILED (J9PORT_ERROR_HYPERVISOR_BASE - 9)
#define J9PORT_ERROR_HYPERVISOR_HOST_INFO_UNAVAILABLE (J9PORT_ERROR_HYPERVISOR_BASE - 10)
#define J9PORT_ERROR_HYPERVISOR_HOST_REQUEST_UNSUPPORTED (J9PORT_ERROR_HYPERVISOR_BASE - 11)
#define J9PORT_ERROR_HYPERVISOR_LPARCFG_MEM_UNSUPPORTED (J9PORT_ERROR_HYPERVISOR_BASE - 12)
#define J9PORT_ERROR_HYPERVISOR_LPARCFG_READ_FAILED (J9PORT_ERROR_HYPERVISOR_BASE - 13)
#define J9PORT_ERROR_HYPERVISOR_MEMORY_ALLOC_FAILED (J9PORT_ERROR_HYPERVISOR_BASE - 14)
#define J9PORT_ERROR_HYPERVISOR_PERFSTAT_CPU_FAILED (J9PORT_ERROR_HYPERVISOR_BASE - 15)
#define J9PORT_ERROR_HYPERVISOR_PERFSTAT_PARTITION_FAILED (J9PORT_ERROR_HYPERVISOR_BASE - 16)
#define J9PORT_ERROR_HYPERVISOR_VMWARE_GUEST_SDK_OP_FAILED (J9PORT_ERROR_HYPERVISOR_BASE - 17)
#define J9PORT_ERROR_HYPERVISOR_VMWARE_GUEST_SDK_OPEN_FAILED (J9PORT_ERROR_HYPERVISOR_BASE - 18)
#define J9PORT_ERROR_HYPERVISOR_VMWARE_HOST_GUESTLIB_NOT_ENABLED (J9PORT_ERROR_HYPERVISOR_BASE - 19)
#define J9PORT_ERROR_HYPERVISOR_VMWARE_UPDATEINFO_NOT_CALLED (J9PORT_ERROR_HYPERVISOR_BASE - 20)
#define J9PORT_ERROR_HYPERVISOR_VMWARE_BUFFER_TOO_SMALL (J9PORT_ERROR_HYPERVISOR_BASE - 21)
#define J9PORT_ERROR_HYPERVISOR_ENV_NOT_SET (J9PORT_ERROR_HYPERVISOR_BASE - 22)
#define J9PORT_ERROR_HYPERVISOR_ENV_VAR_MALFORMED (J9PORT_ERROR_HYPERVISOR_BASE - 23)
#define J9PORT_ERROR_HYPERVISOR_DEFAULTNAME_NOT_SET (J9PORT_ERROR_HYPERVISOR_BASE - 24)
#define J9PORT_ERROR_HYPERVISOR_HYPFS_NOT_MOUNTED (J9PORT_ERROR_HYPERVISOR_BASE - 25)
#define J9PORT_ERROR_HYPERVISOR_HYPFS_READ_FAILED (J9PORT_ERROR_HYPERVISOR_BASE - 26)
#define J9PORT_ERROR_HYPERVISOR_HYPFS_STAT_FAILED (J9PORT_ERROR_HYPERVISOR_BASE - 27)
#define J9PORT_ERROR_HYPERVISOR_HYPFS_UPDATE_OPEN_FAILED (J9PORT_ERROR_HYPERVISOR_BASE - 28)
#define J9PORT_ERROR_HYPERVISOR_HYPFS_UPDATE_WRITE_FAILED (J9PORT_ERROR_HYPERVISOR_BASE - 29)
#define J9PORT_ERROR_HYPERVISOR_HYPFS_UPDATE_LOOP_TIMEOUT (J9PORT_ERROR_HYPERVISOR_BASE - 30)
#define J9PORT_ERROR_HYPERVISOR_SYSINFO_READ_FAILED (J9PORT_ERROR_HYPERVISOR_BASE - 31)
#define J9PORT_ERROR_HYPERVISOR_MACHINE_SPEED_FILE_READ_FAILED (J9PORT_ERROR_HYPERVISOR_BASE - 32)
#define J9PORT_ERROR_HYPERVISOR_LPDAT_QUERY_FAILED (J9PORT_ERROR_HYPERVISOR_BASE - 33)
#define J9PORT_ERROR_HYPERVISOR_CLOCKSPEED_NOT_FOUND (J9PORT_ERROR_HYPERVISOR_BASE - 34)

/**
 * @name Port library initialization return codes
 * Reasons for failure to initialize port library
 *
 * @internal J9PORT_ERROR_STRING_* range from -850 to -899 avoid overlap
 */

/* default value */
#define J9PORT_ERROR_INIT_FAIL_BASE -850
#define J9PORT_ERROR_INIT_WRONG_MAJOR_VERSION (J9PORT_ERROR_INIT_FAIL_BASE)
#define J9PORT_ERROR_INIT_WRONG_SIZE (J9PORT_ERROR_INIT_FAIL_BASE - 1)
#define J9PORT_ERROR_INIT_WRONG_CAPABILITIES (J9PORT_ERROR_INIT_FAIL_BASE - 2)

#endif /* j9porterror_h */


