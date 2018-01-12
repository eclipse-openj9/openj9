/*******************************************************************************
 * Copyright (c) 1998, 2014 IBM Corp. and others
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
/******************************************************\
		Portable socket library header.
\******************************************************/

#ifndef j9socket_h
#define j9socket_h

#include <stddef.h>
#include "j9porterror.h"

/* Socket types, stream & datagram */
#define J9SOCK_STREAM 0
#define J9SOCK_DGRAM 1
#define J9SOCK_AFINET 2
#define J9SOCK_ANY 3
#define J9SOCK_DEFPROTOCOL 0
#define J9SOCK_INADDR_ANY (uint32_t)0	
#define J9SOCK_NOFLAGS (uint32_t)0			/* The default flag argument value, as in a recv */
#define J9SOCK_INADDR_LEN 4				/* The length in bytes of a binary IPv4 internet address */
#define J9SOCK_INADDR6_LEN 16            /* The length in bytes of a binary IPv6 internet address */

/* For getaddrinfo (IPv6) -- socket types */
#define J9SOCKET_ANY			 0       /* for getaddrinfo hints */
#define J9SOCKET_STREAM     1        /* stream socket */
#define J9SOCKET_DGRAM       2        /* datagram socket */
#define J9SOCKET_RAW           3        /* raw-protocol interface */
#define J9SOCKET_RDM           4         /* reliably-delivered message */
#define J9SOCKET_SEQPACKET  5     /* sequenced packet stream */

/** address family */
#define J9ADDR_FAMILY_UNSPEC 0                /* IPv6 */
#define J9ADDR_FAMILY_AFINET4 2                /* IPv6 */
#define J9ADDR_FAMILY_AFINET6 23              /* IPv6 */

/** protocol family */
#define J9PROTOCOL_FAMILY_UNSPEC   J9ADDR_FAMILY_UNSPEC                /* IPv6 */
#define J9PROTOCOL_FAMILY_INET4        J9ADDR_FAMILY_AFINET4                /* IPv6 */
#define J9PROTOCOL_FAMILY_INET6        J9ADDR_FAMILY_AFINET6                /* IPv6 */

/* Portable defines for socket levels */
#define J9_SOL_SOCKET 1
#define J9_IPPROTO_TCP 2
#define J9_IPPROTO_IP 3
#define J9_IPPROTO_IPV6 4

/* Portable defines for socket options */
#define J9_SO_LINGER 1
#define J9_SO_KEEPALIVE 2
#define J9_TCP_NODELAY 3
#define J9_MCAST_TTL 4
#define J9_MCAST_ADD_MEMBERSHIP 5
#define J9_MCAST_DROP_MEMBERSHIP 6
#define J9_MCAST_INTERFACE 7
#define J9_SO_REUSEADDR 8
#define J9_SO_REUSEPORT 9

#define J9_SO_SNDBUF 11
#define J9_SO_RCVBUF 12
#define J9_SO_BROADCAST 13 /*[PR1FLSKTU] Support datagram broadcasts */
#define J9_SO_OOBINLINE 14
#define J9_IP_MULTICAST_LOOP 15
#define J9_IP_TOS 16
#define J9_MCAST_INTERFACE_2 		17
#define J9_IPV6_ADD_MEMBERSHIP 	18
#define J9_IPV6_DROP_MEMBERSHIP	19

/* Portable defines for socket read/write options */
#define J9SOCK_MSG_PEEK 1
#define J9SOCK_MSG_OOB 2

/* Platform Constants */
typedef struct j9socket_struct *j9socket_t;
typedef struct j9sockaddr_struct *j9sockaddr_t;
typedef struct j9hostent_struct *j9hostent_t;
typedef struct j9fdset_struct *j9fdset_t;
typedef struct j9timeval_struct *j9timeval_t;
typedef struct j9linger_struct *j9linger_t;
typedef struct j9ipmreq_struct *j9ipmreq_t;
typedef struct j9addrinfo_struct *j9addrinfo_t;                               /* IPv6 */
typedef struct j9ipv6_mreq_struct *j9ipv6_mreq_t;      /* IPv6 */

/* constants for calling multi-call functions */
#define J9_PORT_SOCKET_STEP_START 		10
#define J9_PORT_SOCKET_STEP_CHECK	20
#define J9_PORT_SOCKET_STEP_DONE		30

#endif     /* j9socket_h */


