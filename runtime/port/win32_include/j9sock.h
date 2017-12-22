/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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

#include "j9socket.h"
#include "j9comp.h"
#include "j9port.h"
#include "j9portpg.h"

#ifdef DEBUG
#define J9SOCKDEBUG(x, error) printf(x, error)
#define J9SOCKDEBUGPRINT(x) printf(x)
#else
#define J9SOCKDEBUG(x, error) 
#define J9SOCKDEBUGPRINT(x)
#endif

extern J9_CFUNC char * os_error_message (struct J9PortLibrary *portLibrary, int32_t errorNum );

/* os types */
typedef SOCKADDR OSADDR;

typedef struct ip_mreq OSIPMREQ;
#define OSSOMAXCONN SOMAXCONN
#define OS_BADSOCKET INVALID_SOCKET /*[PR 1FJBLB9] Provide bad socket constant */

typedef struct addrinfoW OSADDRINFO;  /* IPv6 - Unicode */

typedef SOCKADDR_IN6 OSSOCKADDR_IN6;  /* IPv6 */

/* constant for identifying the pseudo looback interface */
#define pseudoLoopbackGUID  "{6BD113CC-5EC2-7638-B953-0B889DA72014}"

/* defines for socket levels */
#define OS_SOL_SOCKET SOL_SOCKET
#define OS_IPPROTO_TCP IPPROTO_TCP
#define OS_IPPROTO_IP IPPROTO_IP
#define OS_IPPROTO_IPV6 IPPROTO_IPV6 

/* defines for socket options */
#define OS_SO_LINGER SO_LINGER
#define OS_SO_KEEPALIVE SO_KEEPALIVE
#define OS_TCP_NODELAY TCP_NODELAY
#define OS_SO_REUSEADDR SO_REUSEADDR
#define OS_SO_SNDBUF SO_SNDBUF
#define OS_SO_RCVBUF SO_RCVBUF
#define OS_SO_BROADCAST SO_BROADCAST /*[PR1FLSKTU] Support datagram broadcasts */
#define OS_SO_OOBINLINE SO_OOBINLINE
#define OS_IP_TOS	 IP_TOS

/* defines added for IPv6 */
#define OS_AF_INET4 AF_INET
#define OS_AF_UNSPEC AF_UNSPEC
#define OS_PF_UNSPEC PF_UNSPEC
#define OS_PF_INET4 PF_INET

#define OS_AF_INET6 AF_INET6
#define OS_PF_INET6 PF_INET6
#define OS_INET4_ADDRESS_LENGTH INET_ADDRSTRLEN
#define OS_INET6_ADDRESS_LENGTH INET6_ADDRSTRLEN
#define OSNIMAXSERV NI_MAXSERV

/* defines for socket options, multicast */
#define OS_MCAST_TTL IP_MULTICAST_TTL
#define OS_MCAST_ADD_MEMBERSHIP IP_ADD_MEMBERSHIP
#define OS_MCAST_DROP_MEMBERSHIP IP_DROP_MEMBERSHIP
#define OS_MCAST_INTERFACE IP_MULTICAST_IF
#define OS_MCAST_INTERFACE_2 IPV6_MULTICAST_IF
#define OS_IPV6_ADD_MEMBERSHIP IPV6_ADD_MEMBERSHIP
#define OS_IPV6_DROP_MEMBERSHIP IPV6_DROP_MEMBERSHIP
#define OS_MCAST_LOOP IP_MULTICAST_LOOP
 
/* platform constants */
#define J9SOCK_MAXCONN OSSOMAXCONN

/*
 * Socket Types
 */
#define OSSOCK_ANY			 0                                     /* for getaddrinfo hints */
#define OSSOCK_STREAM     SOCK_STREAM              /* stream socket */
#define OSSOCK_DGRAM      SOCK_DGRAM                /* datagram socket */
#define OSSOCK_RAW        SOCK_RAW                      /* raw-protocol interface */
#define OSSOCK_RDM        SOCK_RDM                       /* reliably-delivered message */
#define OSSOCK_SEQPACKET  SOCK_SEQPACKET    /* sequenced packet stream */


/* socket structure flags */
#define SOCKET_IPV4_OPEN_MASK '\x1'    /* 00000001 */
#define SOCKET_IPV6_OPEN_MASK '\x2'    /* 00000010 */
#define SOCKET_BOTH_OPEN_MASK '\x3'  /* 00000011 */

/* Per-thread buffer for platform-dependent socket information */
typedef struct J9SocketPTB {
	struct J9PortLibrary *portLibrary;
	j9fdset_t fdset;  /**< file descriptor set */
	j9addrinfo_struct addr_info_hints;
} J9SocketPTB;

#endif

