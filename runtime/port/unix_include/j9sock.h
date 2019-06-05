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

#ifndef j9sock_h
#define j9sock_h
/******************************************************\
		Portable socket library implementation.
\******************************************************/

/* include common types from portsock.h */
#include "portsock.h"

#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>

#if defined(J9ZOS390)
#include <netinet/tcp_var.h>
#include <arpa/inet.h>
#include <xti.h>
#elif !defined(J9ZTPF)
#include <netinet/tcp.h>
#if defined(OSX)
#include <stdlib.h>
#else /* OSX */
#include <malloc.h>
#endif /* !OSX */
#include <sys/socket.h>
#endif /* defined(J9ZOS390) */

#if defined(AIXPPC)
#include <arpa/inet.h>
#endif

#include "j9socket.h"
#include "j9comp.h"
#include "j9port.h"

#ifdef DEBUG
#define J9SOCKDEBUG(x, err) printf(x, err)
#define J9SOCKDEBUGH(x, err) printf(x, err)
#define J9SOCKDEBUGPRINT(x) printf(x)
#else
#define J9SOCKDEBUG(x, err) 
#define J9SOCKDEBUGH(x, err) 
#define J9SOCKDEBUGPRINT(x)
#endif

/* PR 94621 - the following defines are used to determine how gethostby*_r calls should be handled.*/
/* HOSTENT_DATA_R: if the HOSTENT_DATA structure is used */
#if defined(AIXPPC)
#define HOSTENT_DATA_R 1
#else
#define HOSTENT_DATA_R 0
#endif
/* GLIBC_R: uses the GLIBC versions */
#if defined(LINUX)
#define GLIBC_R 1
#else
#define GLIBC_R 0
#endif
/* OTHER_R: everything else */
#define OTHER_R ((!HOSTENT_DATA_R)&&(!GLIBC_R))

/* os types */
typedef struct sockaddr OSADDR;
typedef struct sockaddr_in6 OSSOCKADDR_IN6;  /* IPv6 */
typedef struct addrinfo OSADDRINFO;  /* IPv6 */

/*
 * Socket Types
 */
#define OSSOCK_ANY			 0                                     /* for getaddrinfo hints */
#define OSSOCK_STREAM     SOCK_STREAM              /* stream socket */
#define OSSOCK_DGRAM      SOCK_DGRAM                /* datagram socket */
#define OSSOCK_RAW        SOCK_RAW                      /* raw-protocol interface */
#define OSSOCK_RDM        SOCK_RDM                       /* reliably-delivered message */
#define OSSOCK_SEQPACKET  SOCK_SEQPACKET    /* sequenced packet stream */


#if HOSTENT_DATA_R
typedef struct hostent_data OSHOSTENT_DATA;
#endif

#define OSSOMAXCONN SOMAXCONN

/* defines for socket levels */
#define OS_SOL_SOCKET SOL_SOCKET
#define OS_IPPROTO_TCP IPPROTO_TCP
#define OS_IPPROTO_IP IPPROTO_IP
#ifdef IPv6_FUNCTION_SUPPORT
#define OS_IPPROTO_IPV6 IPPROTO_IPV6 
#endif

/* defines for socket options */
#define OS_SO_LINGER SO_LINGER
#define OS_SO_KEEPALIVE SO_KEEPALIVE
#define OS_TCP_NODELAY TCP_NODELAY
#define OS_SO_REUSEADDR SO_REUSEADDR
#define OS_SO_SNDBUF SO_SNDBUF
#define OS_SO_RCVBUF SO_RCVBUF
#define OS_SO_BROADCAST SO_BROADCAST /*[PR1FLSKTU] Support datagram broadcasts */
#if defined(AIXPPC) || defined(J9ZOS390)
#define OS_SO_REUSEPORT SO_REUSEPORT
#endif
#define OS_SO_OOBINLINE SO_OOBINLINE
#define OS_IP_TOS	 IP_TOS

/* defines for socket options, multicast */
#define OS_MCAST_TTL IP_MULTICAST_TTL
#define OS_MCAST_ADD_MEMBERSHIP IP_ADD_MEMBERSHIP
#define OS_MCAST_DROP_MEMBERSHIP IP_DROP_MEMBERSHIP
#define OS_MCAST_INTERFACE IP_MULTICAST_IF
#define OS_MCAST_LOOP IP_MULTICAST_LOOP

#ifdef IPv6_FUNCTION_SUPPORT
#define OS_MCAST_INTERFACE_2 IPV6_MULTICAST_IF

#if defined(J9ZOS390)
/* TODO: determine the correct defines for z/OS, leave it broken in the meantime */
#define OS_IPV6_ADD_MEMBERSHIP (-1)
#define OS_IPV6_DROP_MEMBERSHIP (-1)
#else
#define OS_IPV6_ADD_MEMBERSHIP IPV6_ADD_MEMBERSHIP
#define OS_IPV6_DROP_MEMBERSHIP IPV6_DROP_MEMBERSHIP
#endif /* J9ZOS390 */
#endif /* IPv6_FUNCTION_SUPPORT */

/* defines for the unix error constants.  These may be overridden for specific platforms. */
#define J9PORT_ERROR_SOCKET_UNIX_CONNRESET 		ECONNRESET
#define J9PORT_ERROR_SOCKET_UNIX_EAGAIN							EAGAIN
#define J9PORT_ERROR_SOCKET_UNIX_EAFNOSUPPORT			EAFNOSUPPORT
#define J9PORT_ERROR_SOCKET_UNIX_EBADF								EBADF
#define J9PORT_ERROR_SOCKET_UNIX_ECONNRESET					ECONNRESET
#define J9PORT_ERROR_SOCKET_UNIX_EINVAL							EINVAL
#define J9PORT_ERROR_SOCKET_UNIX_EINTR								EINTR
#define J9PORT_ERROR_SOCKET_UNIX_EFAULT							EFAULT
#define J9PORT_ERROR_SOCKET_UNIX_ENOPROTOOPT				ENOPROTOOPT
#define J9PORT_ERROR_SOCKET_UNIX_ENOTCONN						ENOTCONN
#define J9PORT_ERROR_SOCKET_UNIX_EPROTONOSUPPORT		EPROTONOSUPPORT
#define J9PORT_ERROR_SOCKET_UNIX_HOSTNOTFOUND			HOST_NOT_FOUND
#define J9PORT_ERROR_SOCKET_UNIX_ENOBUFS						ENOBUFS
#define J9PORT_ERROR_SOCKET_UNIX_NODATA							NO_DATA
#define J9PORT_ERROR_SOCKET_UNIX_NORECOVERY				NO_RECOVERY
#define J9PORT_ERROR_SOCKET_UNIX_ENOTSOCK					ENOTSOCK
#define J9PORT_ERROR_SOCKET_UNIX_TRYAGAIN						TRY_AGAIN
#define J9PORT_ERROR_SOCKET_UNIX_EOPNOTSUP				EOPNOTSUPP
#define J9PORT_ERROR_SOCKET_UNIX_ETIMEDOUT				ETIMEDOUT
#define J9PORT_ERROR_SOCKET_UNIX_CONNREFUSED				ECONNREFUSED
#define J9PORT_ERROR_SOCKET_UNIX_EINPROGRESS				EINPROGRESS
#define J9PORT_ERROR_SOCKET_UNIX_ENETUNREACH				ENETUNREACH	
#define J9PORT_ERROR_SOCKET_UNIX_EACCES						EACCES
#define J9PORT_ERROR_SOCKET_UNIX_EALREADY						EALREADY
#define J9PORT_ERROR_SOCKET_UNIX_EISCONN						EISCONN
#define J9PORT_ERROR_SOCKET_UNIX_EADDRINUSE					EADDRINUSE
#define J9PORT_ERROR_SOCKET_UNIX_EADDRNOTAVAIL				EADDRNOTAVAIL
#define J9PORT_ERROR_SOCKET_UNIX_ENOSR						ENOSR
#define J9PORT_ERROR_SOCKET_UNIX_EIO						EIO
#define J9PORT_ERROR_SOCKET_UNIX_EISDIR						EISDIR
#define J9PORT_ERROR_SOCKET_UNIX_ELOOP						ELOOP
#define J9PORT_ERROR_SOCKET_UNIX_ENOENT						ENOENT
#define J9PORT_ERROR_SOCKET_UNIX_ENOTDIR					ENOTDIR
#define J9PORT_ERROR_SOCKET_UNIX_EROFS						EROFS
#define J9PORT_ERROR_SOCKET_UNIX_ENOMEM						ENOMEM
#define J9PORT_ERROR_SOCKET_UNIX_ENAMETOOLONG				ENAMETOOLONG
/* platform constants */
#define J9SOCK_MAXCONN OSSOMAXCONN;

/* defines added for IPv6 */
#define OS_AF_INET4 AF_INET
#define OS_AF_UNSPEC AF_UNSPEC
#define OS_PF_UNSPEC PF_UNSPEC
#define OS_PF_INET4 PF_INET
#define OS_INET4_ADDRESS_LENGTH INET_ADDRSTRLEN

#ifdef AF_INET6
#define OS_AF_INET6 AF_INET6
#define OS_PF_INET6 PF_INET6
#define OS_INET6_ADDRESS_LENGTH INET6_ADDRSTRLEN
#else 
#define OS_AF_INET6 -1
#define OS_PF_INET6 -1
#define OS_INET6_ADDRESS_LENGTH 46
#endif

#define GET_HOST_BUFFER_SIZE 512
/* The gethostBuffer is allocated bufferSize + EXTRA_SPACE, while gethostby*_r is only aware of bufferSize
 * because there seems to be a bug on Linux 386 where gethostbyname_r writes past the end of the
 * buffer.  This bug has not been observed on other platforms, but EXTRA_SPACE is added anyway as a precaution.*/
#define EXTRA_SPACE 128
/*size is 16 because an IP string is "xxx.xxx.xxx.xxx" + 1 null character */
#define NTOA_SIZE 16

/* Per-thread buffer for platform-dependent socket information */
typedef struct J9SocketPTB {
	struct J9PortLibrary *portLibrary;
	j9fdset_t fdset;  /**< file descriptor set */
	char ntoa[NTOA_SIZE];
	j9addrinfo_struct addr_info_hints;
	OSHOSTENT hostent;
#if HOSTENT_DATA_R
	OSHOSTENT_DATA *hostent_data;
#elif GLIBC_R || OTHER_R
	int gethostBufferSize;
	char *gethostBuffer;
#endif /* HOSTENT_DATA_R */
} J9SocketPTB;
#endif

