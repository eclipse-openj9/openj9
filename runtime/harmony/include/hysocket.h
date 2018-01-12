/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
#if !defined(hysocket_h)
#define hysocket_h

#include <stddef.h>
#include "hyporterror.h"
#include "j9socket.h"

/* Socket types, stream & datagram */
#define HYSOCK_STREAM 0
#define HYSOCK_DGRAM 1
#define HYSOCK_AFINET 2
#define HYSOCK_ANY 3
#define HYSOCK_DEFPROTOCOL 0
#define HYSOCK_INADDR_ANY (U_32)0 
#define HYSOCK_NOFLAGS (U_32)0       /* The default flag argument value, as in a recv */
#define HYSOCK_INADDR_LEN 4          /* The length in bytes of a binary IPv4 internet address */
#define HYSOCK_INADDR6_LEN 16        /* The length in bytes of a binary IPv6 internet address */

/* For getaddrinfo (IPv6) -- socket types */
#define HYSOCKET_ANY 0        /* for getaddrinfo hints */
#define HYSOCKET_STREAM 1     /* stream socket */
#define HYSOCKET_DGRAM 2      /* datagram socket */
#define HYSOCKET_RAW 3        /* raw-protocol interface */
#define HYSOCKET_RDM 4        /* reliably-delivered message */
#define HYSOCKET_SEQPACKET 5  /* sequenced packet stream */

/** address family */
#define HYADDR_FAMILY_UNSPEC 0     /* IPv6 */
#define HYADDR_FAMILY_AFINET4 2    /* IPv6 */
#define HYADDR_FAMILY_AFINET6 23   /* IPv6 */

/** protocol family */
#define HYPROTOCOL_FAMILY_UNSPEC  HYADDR_FAMILY_UNSPEC     /* IPv6 */
#define HYPROTOCOL_FAMILY_INET4   HYADDR_FAMILY_AFINET4    /* IPv6 */
#define HYPROTOCOL_FAMILY_INET6   HYADDR_FAMILY_AFINET6    /* IPv6 */

/* Portable defines for socket levels */
#define HY_SOL_SOCKET 1
#define HY_IPPROTO_TCP 2
#define HY_IPPROTO_IP 3
#define HY_IPPROTO_IPV6 4

/* Portable defines for socket options */
#define HY_SO_LINGER 1
#define HY_SO_KEEPALIVE 2
#define HY_TCP_NODELAY 3
#define HY_MCAST_TTL 4
#define HY_MCAST_ADD_MEMBERSHIP 5
#define HY_MCAST_DROP_MEMBERSHIP 6
#define HY_MCAST_INTERFACE 7
#define HY_SO_REUSEADDR 8
#define HY_SO_REUSEPORT 9
#define HY_SO_SNDBUF 11
#define HY_SO_RCVBUF 12
#define HY_SO_BROADCAST 13
#define HY_SO_OOBINLINE 14
#define HY_IP_MULTICAST_LOOP 15
#define HY_IP_TOS 16
#define HY_MCAST_INTERFACE_2 17
#define HY_IPV6_ADD_MEMBERSHIP 18
#define HY_IPV6_DROP_MEMBERSHIP 19

/* Portable defines for socket read/write options */
#define HYSOCK_MSG_PEEK 1
#define HYSOCK_MSG_OOB 2

typedef struct j9socket_struct  hysocket_struct;
typedef struct j9sockaddr_struct  hysockaddr_struct;
typedef struct j9hostent_struct  hyhostent_struct;
typedef struct j9fdset_struct  hyfdset_struct;
typedef struct j9timeval_struct  hytimeval_struct;
typedef struct j9linger_struct  hylinger_struct;
typedef struct j9ipmreq_struct  hyipmreq_struct;
typedef struct j9addrinfo_struct  hyaddrinfo_struct;    /* IPv6 */
typedef struct j9ipv6_mreq_struct  hyipv6_mreq_struct;  /* IPv6 */

/* Platform Constants */
typedef hysocket_struct *hysocket_t;
typedef hysockaddr_struct *hysockaddr_t;
typedef hyhostent_struct *hyhostent_t;
typedef hyfdset_struct *hyfdset_t;
typedef hytimeval_struct *hytimeval_t;
typedef hylinger_struct *hylinger_t;
typedef hyipmreq_struct *hyipmreq_t;
typedef hyaddrinfo_struct *hyaddrinfo_t;    /* IPv6 */
typedef hyipv6_mreq_struct *hyipv6_mreq_t;  /* IPv6 */

/* constants for calling multi-call functions */
#define HY_PORT_SOCKET_STEP_START   10
#define HY_PORT_SOCKET_STEP_CHECK 20
#define HY_PORT_SOCKET_STEP_DONE  30

#endif     /* hysocket_h */
