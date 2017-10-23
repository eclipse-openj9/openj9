/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

/**
 * @file
 * @ingroup Port
 * @brief Sockets
 */

/* For the Z/OS 1.9 compiler */
#define _OPEN_SYS_IF_EXT

#include "j9sock.h"
#include "omrmutex.h"
#include "portpriv.h"
#include "omrportptb.h"
#include "ut_j9prt.h"
#include <fcntl.h>
#include <string.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> /* for struct in_addr */
#include <sys/ioctl.h>
#include <net/if.h> /* for struct ifconf */
#if defined(LINUX)
#include <arpa/inet.h>
#endif

#if defined(J9ZTPF)
#undef GLIBC_R
#endif /* defined(J9ZTPF) */

#if defined(J9ZOS390)
#include "atoe.h"
#endif

#if defined(LINUX)
#include <poll.h>
#define IPV6_FLOWINFO_SEND      33
#define HAS_RTNETLINK 1
#endif

#if defined(HAS_RTNETLINK)
#include <asm/types.h>
#include <linux/netlink.h> 
#include <linux/rtnetlink.h> 
typedef struct linkReq_struct{
               struct nlmsghdr 	netlinkHeader;
               struct ifinfomsg	msg;
	} linkReq_struct ;

typedef struct addrReq_struct{
               struct nlmsghdr 	netlinkHeader;
               struct ifaddrmsg   msg;
	} addrReq_struct ;

#define NETLINK_DATA_BUFFER_SIZE 4096
#define NETLINK_READTIMEOUT_SECS 20

typedef struct netlinkContext_struct {
			int 						netlinkSocketHandle;
			char 						buffer[NETLINK_DATA_BUFFER_SIZE];
			struct nlmsghdr*    	netlinkHeader;
			uint32_t 					remainingLength;
			uint32_t						done;
	} netlinkContext_struct;

#else /* HAS_RTNETLINK */

/* need something so that functions still compile */
typedef struct netlinkContext_struct {
			int 						netlinkSocketHandle;
	} netlinkContext_struct;

typedef struct nlmsghdr {
			int length;
	} nlmsghdr;
#endif

#define INVALID_SOCKET (j9socket_t) -1

/*the number of times to retry a gethostby* call if the return code is TRYAGAIN*/
#define MAX_RETRIES 50

/* needed for connect_with_timeout */
typedef struct selectFDSet_struct {
	int 	  			nfds;
	int			 	sock;
	fd_set 			writeSet;
	fd_set 			readSet;
	fd_set			exceptionSet;
} selectFDSet_strut;


static int32_t platformSocketOption(int32_t portableSocketOption);
static int32_t platformSocketLevel(int32_t portableSocketLevel);
static int32_t findError (int32_t errorCode);
static int32_t map_protocol_family_J9_to_OS( int32_t addr_family);
static int32_t map_addr_family_J9_to_OS( int32_t addr_family);
static int32_t map_sockettype_J9_to_OS( int32_t socket_type);
static int32_t findHostError (int herr);

#if defined(IPv6_FUNCTION_SUPPORT)
static int32_t getAddrLength(j9sockaddr_t addrHandle);
#endif

static int32_t disconnectSocket(struct J9PortLibrary *portLibrary, j9socket_t sock, socklen_t fromlen);
static int32_t connectSocket(struct J9PortLibrary *portLibrary, j9socket_t sock, j9sockaddr_t addr, socklen_t fromlen);

static int32_t lookupIPv6AddressFromIndex(struct J9PortLibrary *portLibrary, uint8_t index, j9sockaddr_t handle);

/**
 * @internal
 * Given a pointer to a sockaddr_struct, look at the address family, and
 * return the length of sockaddr to be passed to bind/connect/writeto methods
 *
 * @param[in] pointer to j9sockaddr_t structure
 *
 * @return	the length of the structure
 */
static int32_t getAddrLength(j9sockaddr_t addrHandle){

	OSSOCKADDR * sockAddr = ((OSSOCKADDR *)&addrHandle->addr);
	int32_t rc = 0;

	if (sockAddr->sin_family == OS_AF_INET4) {
		rc = sizeof(OSSOCKADDR);
	}
#if defined(IPv6_FUNCTION_SUPPORT)
	else {
		rc = sizeof(OSSOCKADDR_IN6);
	}
#endif /* defined(IPv6_FUNCTION_SUPPORT) */
	return rc;

}

/**
 * @internal Given a pointer to a j9socket_struct, determine the address family of the socket.
 *
 * @param[in] pointer to j9socket_t structure
 *
 * @return	the portable address family of the socket or the (negative) portable error code
 */
static int32_t
getSocketFamily(struct J9PortLibrary *portLibrary, j9socket_t handle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = J9ADDR_FAMILY_UNSPEC;

	if ((handle->family == J9ADDR_FAMILY_AFINET4) || (handle->family == J9ADDR_FAMILY_AFINET6)) {
		rc = handle->family;
	} else {
		j9sockaddr_struct addr;
		socklen_t addrlen = sizeof(addr.addr);

		if (getsockname(SOCKET_CAST(handle), (struct sockaddr *) &addr.addr, &addrlen) != 0) {
			int32_t err = errno;
			J9SOCKDEBUG( "<getsockname failed, err=%d>\n", err);
			rc = omrerror_set_last_error(err, findError(err));
		} else {
			int32_t family = ((struct sockaddr_in *) &addr.addr)->sin_family;
			if (family == AF_INET) {
				rc = J9ADDR_FAMILY_AFINET4;
			}
#if defined(IPv6_FUNCTION_SUPPORT)
			else if (family == AF_INET6) {
				rc = J9ADDR_FAMILY_AFINET6;
			}
#endif /* defined(IPv6_FUNCTION_SUPPORT) */
		}
	}

	return rc;
}

/**
 * @internal
 * Determines the proper portable error code to return given a native error code
 *
 * @param[in] errorCode The error code reported by the OS
 *
 * @return	the (negative) portable error code
 */
static int32_t
findError (int32_t errorCode)
{
	switch (errorCode)
	{
		case J9PORT_ERROR_SOCKET_UNIX_EBADF:
			return J9PORT_ERROR_SOCKET_BADDESC;
		case J9PORT_ERROR_SOCKET_UNIX_ENOBUFS:
			return J9PORT_ERROR_SOCKET_NOBUFFERS;
		case J9PORT_ERROR_SOCKET_UNIX_EOPNOTSUP:
			return J9PORT_ERROR_SOCKET_OPNOTSUPP;
		case J9PORT_ERROR_SOCKET_UNIX_ENOPROTOOPT:
			return J9PORT_ERROR_SOCKET_OPTUNSUPP;
		case J9PORT_ERROR_SOCKET_UNIX_EINVAL:
			return J9PORT_ERROR_SOCKET_SOCKLEVELINVALID;
		case J9PORT_ERROR_SOCKET_UNIX_ENOTSOCK:
			return J9PORT_ERROR_SOCKET_NOTSOCK;
		case J9PORT_ERROR_SOCKET_UNIX_EINTR:
			return J9PORT_ERROR_SOCKET_INTERRUPTED;
		case J9PORT_ERROR_SOCKET_UNIX_ENOTCONN:
			return J9PORT_ERROR_SOCKET_NOTCONNECTED;
		case J9PORT_ERROR_SOCKET_UNIX_EAFNOSUPPORT:
			return J9PORT_ERROR_SOCKET_BADAF;
		/* note: J9PORT_ERROR_SOCKET_UNIX_ECONNRESET not included because it has the same
		 * value as J9PORT_ERROR_SOCKET_UNIX_CONNRESET and they both map to J9PORT_ERROR_SOCKET_CONNRESET */
		case J9PORT_ERROR_SOCKET_UNIX_CONNRESET:
			return J9PORT_ERROR_SOCKET_CONNRESET;
		case J9PORT_ERROR_SOCKET_UNIX_EAGAIN:
#if defined(J9ZOS390)
		case EWOULDBLOCK:	
#endif
			return J9PORT_ERROR_SOCKET_WOULDBLOCK;
		case J9PORT_ERROR_SOCKET_UNIX_EPROTONOSUPPORT:
			return J9PORT_ERROR_SOCKET_BADPROTO;
		case J9PORT_ERROR_SOCKET_UNIX_EFAULT:
			return J9PORT_ERROR_SOCKET_ARGSINVALID;
		case J9PORT_ERROR_SOCKET_UNIX_ETIMEDOUT:
			return J9PORT_ERROR_SOCKET_TIMEOUT;
		case J9PORT_ERROR_SOCKET_UNIX_CONNREFUSED:
			return J9PORT_ERROR_SOCKET_CONNECTION_REFUSED;
		case J9PORT_ERROR_SOCKET_UNIX_ENETUNREACH:
			return J9PORT_ERROR_SOCKET_ENETUNREACH;
		case J9PORT_ERROR_SOCKET_UNIX_EACCES:
			return J9PORT_ERROR_SOCKET_EACCES;
		case J9PORT_ERROR_SOCKET_UNIX_EINPROGRESS:
			return J9PORT_ERROR_SOCKET_EINPROGRESS;
		case J9PORT_ERROR_SOCKET_UNIX_EALREADY:	
			return J9PORT_ERROR_SOCKET_ALREADYINPROGRESS;
		case J9PORT_ERROR_SOCKET_UNIX_EISCONN:	
			return J9PORT_ERROR_SOCKET_ISCONNECTED;
		case J9PORT_ERROR_SOCKET_UNIX_EADDRINUSE:
			return J9PORT_ERROR_SOCKET_ADDRINUSE;
		case J9PORT_ERROR_SOCKET_UNIX_EADDRNOTAVAIL:
			return J9PORT_ERROR_SOCKET_ADDRNOTAVAIL;
#if !defined(J9ZTPF)
		case J9PORT_ERROR_SOCKET_UNIX_ENOSR:
			return J9PORT_ERROR_SOCKET_NOSR;
#endif /* !defined(J9ZTPF) */
		case J9PORT_ERROR_SOCKET_UNIX_EIO:
			return J9PORT_ERROR_SOCKET_IO;
		case J9PORT_ERROR_SOCKET_UNIX_EISDIR:
			return J9PORT_ERROR_SOCKET_ISDIR;
		case J9PORT_ERROR_SOCKET_UNIX_ELOOP:
			return J9PORT_ERROR_SOCKET_LOOP;
		case J9PORT_ERROR_SOCKET_UNIX_ENOENT:
			return J9PORT_ERROR_SOCKET_NOENT;
		case J9PORT_ERROR_SOCKET_UNIX_ENOTDIR:
			return J9PORT_ERROR_SOCKET_NOTDIR;
		case J9PORT_ERROR_SOCKET_UNIX_EROFS:
			return J9PORT_ERROR_SOCKET_ROFS;
		case J9PORT_ERROR_SOCKET_UNIX_ENOMEM:
			return J9PORT_ERROR_SOCKET_NOMEM;
		case J9PORT_ERROR_SOCKET_UNIX_ENAMETOOLONG:
			return J9PORT_ERROR_SOCKET_NAMETOOLONG;
		default:
			return J9PORT_ERROR_SOCKET_OPFAILED;
	}
}





/**
 * @internal Determines the proper portable error code to return given a native host error code
 *
 * @return	the (negative) error code
 */
static int32_t
findHostError (int herr)
{
	switch (herr)
	{
		case J9PORT_ERROR_SOCKET_UNIX_NORECOVERY:
			return J9PORT_ERROR_SOCKET_NORECOVERY;
		case J9PORT_ERROR_SOCKET_UNIX_HOSTNOTFOUND:
			return J9PORT_ERROR_SOCKET_HOSTNOTFOUND;
		case J9PORT_ERROR_SOCKET_UNIX_NODATA:
			return J9PORT_ERROR_SOCKET_NODATA;
		case J9PORT_ERROR_SOCKET_UNIX_TRYAGAIN:
			return J9PORT_ERROR_SOCKET_INTERRUPTED;
		default:
			return J9PORT_ERROR_SOCKET_BADSOCKET;
	}
}





/**
 * @internal Map the portable address family to the platform address family. 
 *
 * @param	addr_family	the portable address family to convert
 *
 * @return	the platform family, or OS_AF_UNSPEC if none exists
 */
static int32_t
map_addr_family_J9_to_OS( int32_t addr_family)
{
	switch( addr_family )
	{
		case J9ADDR_FAMILY_AFINET4:
			return OS_AF_INET4;
		case J9ADDR_FAMILY_AFINET6:
			return OS_AF_INET6;
	}
	return OS_AF_UNSPEC;
}





/**
 * @internal Map the portable address protocol to the platform protocol
 *
 * @param	addr_protocol  the portable address protocol to convert
 *
 * @return	the platform family, or OS_PF_UNSPEC if none exists
 */
static int32_t
map_protocol_family_J9_to_OS( int32_t addr_family)
{
	switch( addr_family )
	{
		case J9PROTOCOL_FAMILY_INET4:
			return OS_PF_INET4;
		case J9PROTOCOL_FAMILY_INET6:
			return OS_PF_INET6;
	}
	return OS_PF_UNSPEC;
}





/**
 * @internal Map the portable socket type to the platform type. 
 *
 * @param	addr_family	the portable socket type to convert
 *
 * @return	the platform family, or OSSOCK_ANY if none exists
 */
static int32_t
map_sockettype_J9_to_OS( int32_t socket_type)
{
	switch( socket_type )
	{
		case J9SOCKET_STREAM:
			return OSSOCK_STREAM;
		case J9SOCKET_DGRAM:
			return OSSOCK_DGRAM;
		case J9SOCKET_RAW:
			return OSSOCK_RAW;
#if !defined(J9ZTPF)
		case J9SOCKET_RDM:
			return OSSOCK_RDM;
		case J9SOCKET_SEQPACKET:
			return OSSOCK_SEQPACKET;
#endif /*!defined(J9ZTPF) */
	}
	return OSSOCK_ANY;
}





/**
 * @internal
 * Map the portable to the platform socket level.  Used to resolve the arguments of socket option functions.
 * Levels currently in use are:
 * \arg SOL_SOCKET, for most options
 * \arg IPPROTO_TCP, for the TCP noDelay option
 * \arg IPPROTO_IP, for the option operations associated with multicast (join, drop, interface)
 *
 * @param[in] portableSocketLevel The portable socket level to convert.
 *
 * @return	the platform socket level or a (negative) error code if no equivalent level exists.
 */
static int32_t
platformSocketLevel(int32_t portableSocketLevel)
{
	switch (portableSocketLevel)
	{
		case J9_SOL_SOCKET: 
			return OS_SOL_SOCKET;
		case J9_IPPROTO_TCP: 
			return OS_IPPROTO_TCP;
		case J9_IPPROTO_IP: 
			return OS_IPPROTO_IP;
#ifdef IPv6_FUNCTION_SUPPORT
		case J9_IPPROTO_IPV6: 
			return OS_IPPROTO_IPV6;
#endif
	}
	return J9PORT_ERROR_SOCKET_SOCKLEVELINVALID;
}





/**
 * @internal
 * Map the portable to the platform socket options.  Used to resolve the arguments of socket option functions.
 * Options currently in supported are:
 * \arg SOL_LINGER, the linger timeout
 * \arg TCP_NODELAY, the buffering scheme implementing Nagle's algorithm
 * \arg IP_MULTICAST_TTL, the packet Time-To-Live
 * \arg IP_ADD_MEMBERSHIP, to join a multicast group
 * \arg  IP_DROP_MEMBERSHIP, to leave a multicast group
 * \arg IP_MULTICAST_IF, the multicast interface
 *
 * @param[in] portableSocketOption The portable socket option to convert.
 *
 * @return	the platform socket option or a (negative) error code if no equivalent option exists.
 */
/**
 * @internal Map the portable to the platform socket options.  Used to resolve the arguments of socket option functions.
 * Options currently in supported are:
 *		SOL_LINGER, the linger timeout
 *		TCP_NODELAY, the buffering scheme implementing Nagle's algorithm
 *		IP_MULTICAST_TTL, the packet Time-To-Live
 *		IP_ADD_MEMBERSHIP, to join a multicast group
 *		IP_DROP_MEMBERSHIP, to leave a multicast group
 *		IP_MULTICAST_IF, the multicast interface
 *
 * @param	portlib		pointer to the VM port library
 * @param	portableSocketOption	the portable socket option to convert
 *
 * @return	the platform socket option or a J9 error if no equivalent option exists
 */
/*[PR1FLSKTU] Support datagram broadcasts */
static int32_t
platformSocketOption(int32_t portableSocketOption)
{
	switch (portableSocketOption)
	{
		case J9_SO_LINGER:
			return OS_SO_LINGER;
		case J9_SO_KEEPALIVE:
			return OS_SO_KEEPALIVE;
		case J9_TCP_NODELAY:
			return OS_TCP_NODELAY;
		case J9_MCAST_TTL:
			return OS_MCAST_TTL;
		case J9_MCAST_ADD_MEMBERSHIP:
			return OS_MCAST_ADD_MEMBERSHIP;
		case J9_MCAST_DROP_MEMBERSHIP:
			return OS_MCAST_DROP_MEMBERSHIP;
		case J9_MCAST_INTERFACE:
			return OS_MCAST_INTERFACE;
		case J9_SO_REUSEADDR:
			return OS_SO_REUSEADDR;
		case J9_SO_SNDBUF:
			return OS_SO_SNDBUF;
		case J9_SO_RCVBUF:
			return OS_SO_RCVBUF;
		case J9_SO_BROADCAST:
			return OS_SO_BROADCAST;
#if defined(AIXPPC) || defined(J9ZOS390)
		case J9_SO_REUSEPORT:
			return OS_SO_REUSEPORT;
#endif
		case J9_SO_OOBINLINE:
			return OS_SO_OOBINLINE;
		case J9_IP_MULTICAST_LOOP:
			return OS_MCAST_LOOP;
		case J9_IP_TOS:
			return OS_IP_TOS;
#ifdef IPv6_FUNCTION_SUPPORT
		case J9_MCAST_INTERFACE_2:
			return OS_MCAST_INTERFACE_2;
		case J9_IPV6_ADD_MEMBERSHIP:
			return OS_IPV6_ADD_MEMBERSHIP;
		case J9_IPV6_DROP_MEMBERSHIP:
			return OS_IPV6_DROP_MEMBERSHIP;		
#endif
	}
	return J9PORT_ERROR_SOCKET_OPTUNSUPP;
}






/**
 * The accept function extracts the first connection on the queue of pending connections 
 * on socket sock. It then creates a new socket and returns a handle to the new socket. 
 * The newly created socket is the socket that will handle the actual the connection and 
 * has the same properties as socket sock.  
 *
 * The accept function can block the caller until a connection is present if no pending 
 * connections are present on the queue.
 *
 * @param[in] portLibrary The port library.
 * @param[in] serverSock  A j9socket_t  from which data will be read.
 * @param[in] addrHandle An optional pointer to a buffer that receives the address of the connecting 
 * entity, as known to the communications layer. The exact format of the addr parameter 
 * is determined by the address family established when the socket was created. 
 * @param[in] sockHandle A pointer to a j9socket_t  which will point to the newly created 
 * socket once accept returns succesfully
 *
 * @return 
 * \arg 0 on success
 * \arg J9PORT_ERROR_SOCKET_BADSOCKET, on generic error
 * \arg J9PORT_ERROR_SOCKET_NOTINITIALIZED, if socket library uninitialized
 * \arg J9PORT_ERROR_SOCKET_INTERRUPTED, the call was cancelled
 * \arg J9PORT_ERROR_SOCKET_ADDRNOTAVAIL, the addr parameter is not valid
 * \arg J9PORT_ERROR_SOCKET_SYSTEMBUSY, if system busy handling other requests
 * \arg J9PORT_ERROR_SOCKET_SYSTEMFULL, is too many sockets are active
 * \arg J9PORT_ERROR_SOCKET_WOULDBLOCK, the socket is marked as nonblocking and no connections are present to be accepted., 
 */
int32_t
j9sock_accept(struct J9PortLibrary *portLibrary, j9socket_t serverSock, j9sockaddr_t addrHandle, j9socket_t *sockHandle)
{
#if defined(LINUX)
#define ACCEPTCAST (socklen_t *)
#else
#define ACCEPTCAST
#endif

	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	int sc;
	socklen_t fromlen = sizeof(addrHandle->addr);

	Trc_PRT_sock_j9sock_accept_Entry(serverSock);
	
	*sockHandle = INVALID_SOCKET;

	sc = accept(SOCKET_CAST(serverSock), (struct sockaddr *) &addrHandle->addr, ACCEPTCAST &fromlen);
	if (sc < 0) {
		rc = errno;
		Trc_PRT_sock_j9sock_accept_failure(errno);
		J9SOCKDEBUG( "<accept failed, err=%d>\n",  rc);
		rc = omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_ADDRNOTAVAIL);
	}

	if (0 == rc) {
		*sockHandle = omrmem_allocate_memory(sizeof(struct j9socket_struct), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (*sockHandle == NULL) {
			close(sc);
			*sockHandle = INVALID_SOCKET ;
			rc = J9PORT_ERROR_SOCKET_NOBUFFERS;
			Trc_PRT_sock_j9sock_accept_failure_oom();
			Trc_PRT_sock_j9sock_accept_Exit(rc);
			return rc; 
		}

		Trc_PRT_sock_j9sock_accept_socket_created(*sockHandle);
		SOCKET_CAST(*sockHandle) = sc;
		(*sockHandle)->family = serverSock->family;
	}
	Trc_PRT_sock_j9sock_accept_Exit(rc);
	return rc;
}

#undef ACCEPTCAST





/**
 * The bind function is used on an unconnected socket before subsequent 
 * calls to the connect or listen functions. When a socket is created with a 
 * call to the socket function, it exists in a name space (address family), but 
 * it has no name assigned to it. Use bind to establish the local association 
 * of the socket by assigning a local name to an unnamed socket.
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock j9socket_t which will be be associated with the specified name.
 * @param[in] addr Address to bind to socket.
 *
 * @return
 * \arg 0, on success
 * \arg J9PORT_ERROR_SOCKET_BADSOCKET, on generic error
 * \arg J9PORT_ERROR_SOCKET_NOTINITIALIZED, if socket library uninitialized
 * \arg J9PORT_ERROR_SOCKET_ADDRINUSE  A process on the machine is already bound to the same fully-qualified address 
 * and the socket has not been marked to allow address re-use with SO_REUSEADDR. 
 * \arg J9PORT_ERROR_SOCKET_ADDRNOTAVAIL The specified address is not a valid address for this machine 
 * \arg J9PORT_ERROR_SOCKET_SYSTEMBUSY, if system busy handling other requests
 * \arg J9PORT_ERROR_SOCKET_SYSTEMFULL, is too many sockets are active
 * \arg J9PORT_ERROR_SOCKET_BADADDR, the addr parameter is not a valid part of the user address space, 
 */
int32_t
j9sock_bind(struct J9PortLibrary *portLibrary, j9socket_t sock, j9sockaddr_t addr)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
#if defined(IPv6_FUNCTION_SUPPORT)
	socklen_t length = getAddrLength(addr);
#else
	socklen_t length = sizeof(addr->addr);
#endif

	Trc_PRT_sock_j9sock_bind_Entry(sock, SOCKET_CAST(sock), addr);

	if (bind(SOCKET_CAST(sock), (struct sockaddr *) &addr->addr,  length) < 0) {
		int addrLength = 0;
		int port = 0;
		char ipstr[INET6_ADDRSTRLEN];
		rc = errno;
		port = j9sock_ntohs(portLibrary, j9sock_sockaddr_port(portLibrary, addr));

		J9SOCKDEBUG("<bind failed, err=%d>\n", rc);

	if (((OSSOCKADDR *)&addr->addr)->sin_family == OS_AF_INET4) {
		addrLength = INET_ADDRSTRLEN;
	}
#if defined(IPv6_FUNCTION_SUPPORT)
	else {
		addrLength = INET6_ADDRSTRLEN;
	}
#endif
#if !defined(J9ZTPF)
		if (0 == j9sock_getnameinfo(portLibrary, addr, length, ipstr, addrLength, NI_NUMERICHOST)) {
			Trc_PRT_sock_j9sock_bind_failed2(rc, ipstr, port);
		} else {
			Trc_PRT_sock_j9sock_bind_failed(rc);
		}
#else /* !defined(J9ZTPF) */
		Trc_PRT_sock_j9sock_bind_failed(rc);
#endif /* !defined(J9ZTPF) */		
		rc = omrerror_set_last_error(rc, findError(rc));
	}

	Trc_PRT_sock_j9sock_bind_Exit(rc);
	return rc;
}


/**
 * The close function closes a socket. Use it to release the socket descriptor socket so 
 * further references to socket will fail.
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock j9socket_t  which will be closed.
 *
 * @return
 * \arg 0, on success
 * \arg J9PORT_ERROR_SOCKET_BADSOCKET, on generic error
 * \arg J9PORT_ERROR_SOCKET_SYSTEMBUSY, if system busy handling other requests
 * \arg J9PORT_ERROR_SOCKET_WOULDBLOCK,  the socket is marked as nonblocking and SO_LINGER is set to a nonzero time-out value.
 */
int32_t
j9sock_close(struct J9PortLibrary *portLibrary, j9socket_t *sock)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	int32_t platformErrorCode = 0;

	Trc_PRT_sock_j9sock_close_Entry(*sock);

	if (*sock == INVALID_SOCKET) {
		platformErrorCode = J9PORT_ERROR_SOCKET_UNIX_EBADF;
	} else {
		if( 0 != close(SOCKET_CAST(*sock)) ) {
			platformErrorCode = errno;
		}

		omrmem_free_memory(*sock);
		*sock = INVALID_SOCKET;
	}

	if (0 != platformErrorCode) {
		Trc_PRT_sock_j9sock_close_failed(platformErrorCode);
		J9SOCKDEBUG( "<closesocket failed, err=%d>\n", platformErrorCode);
		rc = omrerror_set_last_error(platformErrorCode, J9PORT_ERROR_SOCKET_BADSOCKET);
	}

	Trc_PRT_sock_j9sock_close_Exit(rc);
	return rc;
}





int32_t
j9sock_connect(struct J9PortLibrary *portLibrary, j9socket_t sock, j9sockaddr_t addr)
{
	int32_t rc = 0;

#if defined(IPv6_FUNCTION_SUPPORT)
	socklen_t fromlen = getAddrLength(addr);
#else
	socklen_t fromlen = sizeof(addr->addr);
#endif
	OSSOCKADDR * sockAddr = ((OSSOCKADDR *)&addr->addr);
	Trc_PRT_sock_j9sock_connect_Entry(sock, addr);

    /* if AF_UNSPEC, then its a request to disconnect */
	if (sockAddr->sin_family == AF_UNSPEC) {
		rc = disconnectSocket(portLibrary, sock, fromlen);
	} else {
		rc = connectSocket(portLibrary, sock, addr, fromlen);
	}
 	Trc_PRT_sock_j9sock_connect_Exit(rc);
	return rc;
}





/**
 * Return an error message describing the last OS error that occurred.  The last
 * error returned is not thread safe, it may not be related to the operation that
 * failed for this thread.
 *
 * @param[in] portLibrary The port library
 *
 * @return	error message describing the last OS error, may return NULL.
 *
 * @internal
 * @note  This function gets the last error code from the OS and then returns
 * the corresponding string.  It is here as a helper function for JCL.  Once j9error
 * is integrated into the port library this function should probably disappear.
 */
const char *
j9sock_error_message(struct J9PortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	return omrerror_last_error_message();
}






/**
 * Create a file descriptor (FD) set of one element.  The call may not be generally useful,
 * as it currently only supports a single FD and is assumed to be used in conjunction with the 
 * j9sock_select function.
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP pointer to the socket to be added to the FD set.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_fdset_init(struct J9PortLibrary *portLibrary, j9socket_t socketP)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	j9fdset_t fdset;
	J9SocketPTB *ptBuffers = j9sock_ptb_get(portLibrary);

	if (NULL == ptBuffers) {
		return J9PORT_ERROR_SOCKET_SYSTEMFULL;
	}

	if (NULL == ptBuffers->fdset) {
		ptBuffers->fdset = omrmem_allocate_memory(sizeof(struct j9fdset_struct), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == ptBuffers->fdset) {
			return J9PORT_ERROR_SOCKET_SYSTEMFULL;
		}
	}
	fdset = ptBuffers->fdset;
	memset(fdset, 0, sizeof(struct j9fdset_struct));

#if defined(LINUX)
	portLibrary->sock_fdset_zero(portLibrary, ptBuffers->fdset);
	portLibrary->sock_fdset_set(portLibrary, socketP, ptBuffers->fdset);
#else
	FD_ZERO(&fdset->handle);
	FD_SET(SOCKET_CAST(socketP), &fdset->handle);
#endif

	return 0;
}





/**
 * Answer the size information, as required by @ref j9sock_select(), to act on a j9fdset_t that has "handle" set.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle
 *
 * @return	the maximum size of the fdset, otherwise the (negative) error code.
 *
 * @note On Unix, the value was the maximum file descriptor plus one, although
 * on many flavors, the value is ignored in the select function.
 * On Windows, the value is ignored by the select function.
 */
int32_t
j9sock_fdset_size(struct J9PortLibrary *portLibrary, j9socket_t handle)
{
	return SOCKET_CAST(handle) + 1;
}





/**
 * Frees the memory created by the call to @ref j9sock_getaddrinfo().
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Hints on what results are returned and how the response if formed .
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 *
 * @note Added for IPv6 support.
 */
int32_t
j9sock_freeaddrinfo(struct J9PortLibrary *portLibrary, j9addrinfo_t handle)
{

#ifdef IPv6_FUNCTION_SUPPORT
	freeaddrinfo(  (OSADDRINFO *)handle->addr_info );
#endif 

	handle->addr_info = NULL;
	handle->length = 0;		

	return 0;	
}





/**
 * Answers a list of addresses as an opaque pointer in "result".
 * 
 * Use the following functions to extract the details:
 * \arg \ref j9sock_getaddrinfo_length
 * \arg \ref j9sock_getaddrinfo_name
 * \arg \ref j9sock_getaddrinfo_address
 * \arg \ref j9sock_getaddrinfo_family
 *
 * If the machine type supports IPv6 you can specify how you want the results returned with the following function:
 * \arg \ref j9sock_create_getaddrinfo_hints.
 * Passing the structure into a machine with only IPv4 support will have no effect.
 *
 * @param[in] portLibrary The port library.
 * @param[in] name The name of the host in either host name format or in IPv4 or IPv6 accepted notations.
 * @param[in] hints Hints on what results are returned and how the response if formed (can be NULL for default action).
 * @param[out] result An opaque pointer to a list of results (j9addrinfo_struct must be preallocated).
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 *
 * @note you must free the "result" structure with @ref j9sock_freeaddrinfo to free up memory.  This must be done
 * before you make a subsequent call in the same thread to this function. 
 * @note Added for IPv6 support.
 */
int32_t
j9sock_getaddrinfo(struct J9PortLibrary *portLibrary, char *name, j9addrinfo_t hints, j9addrinfo_t result)
{
	/* If we have the IPv6 functions available we will call them, otherwise we'll call the IPv4 function */
#ifdef IPv6_FUNCTION_SUPPORT
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	OSADDRINFO *ipv6_result;
	OSADDRINFO *addr_info_hints = NULL;
	int count = 0;

	if( hints != NULL ) {
		addr_info_hints = (OSADDRINFO *) hints->addr_info;
	}

	if (0 != getaddrinfo(name, NULL, addr_info_hints, &ipv6_result)) {
		int32_t errorCode = errno;
		J9SOCKDEBUG( "<getaddrinfo failed, err=%d>\n", errorCode);
		return omrerror_set_last_error(errorCode, findError(errorCode));
	}

	memset(result, 0, sizeof(struct j9addrinfo_struct));
	result->addr_info = (void *) ipv6_result;
	while (NULL != ipv6_result->ai_next) {
		count++;
		ipv6_result = ipv6_result->ai_next;
	}
	result->length = ++count; /* Have to add an extra, because we didn't count the first entry */
	
	return 0;

#else /* IPv6_FUNCTION_SUPPORT */

	int32_t rc    = 0;
	uint32_t addr  = 0;
	uint32_t count = 0;
	struct j9hostent_struct j9hostent;

	if(0 != portLibrary->sock_inetaddr(portLibrary, name, &addr)) {
		if(0 != (rc = portLibrary->sock_gethostbyname(portLibrary, name, &j9hostent))) {
			return rc;
		}
	} else {
		if(0 != (rc = portLibrary->sock_gethostbyaddr(portLibrary, (char *)&addr, sizeof(addr), J9SOCK_AFINET, &j9hostent))) {
			return rc;
		}
	}

	memset(result, 0, sizeof(struct j9addrinfo_struct));
	result->addr_info = (void *) j9hostent.entity;

	/* count the host names and the addresses */
	while( j9hostent.entity->h_addr_list[ count ] != 0 ) {
		count ++;
	}	
	result->length = count;

	return 0;
#endif /* IPv6_FUNCTION_SUPPORT */
}





/**
 * Answers a uint8_t array representing the address at "index" in the structure returned from @ref j9sock_getaddrinfo, indexed starting at 0.
 * The address is in network byte order. 
 *
 * The address will either be 4 or 16 bytes depending on whether it is an OS_AF_INET  address or an OS_AF_INET6  address.  You can check
 * this will a call to @ref j9sock_getaddrinfo_family.  Therefore you should either check the family type before preallocating the "address"
 * or define it as 16 bytes.
 *
 * @param[in]   portLibrary The port library.
 * @param[in]   handle The result structure returned by @ref j9sock_getaddrinfo.
 * @param[out] address The address at "index" in a preallocated buffer.
 * @param[in]   index The address index into the structure returned by @ref j9sock_getaddrinfo.
 * @param[out] scope_id  The scope id associated with the address if applicable
 *
 * @return	
 * \arg 0, if no errors occurred, otherwise the (negative) error code
 * \arg J9PORT_ERROR_SOCKET_VALUE_NULL when we have have the old IPv4 gethostbyname call and the address indexed is out
 * of range.  This is because the address list and the host alias list are not the same length.  Just skip this entry.
 *
 * @note Added for IPv6 support.
 */
int32_t
j9sock_getaddrinfo_address(struct J9PortLibrary *portLibrary, j9addrinfo_t handle, uint8_t *address, int index, uint32_t* scope_id)
{
	int32_t rc = 0;
	OSADDRINFO *addr;
	void *sock_addr;
#ifndef IPv6_FUNCTION_SUPPORT
	char ** addr_list;
#endif /* IPv6_FUNCTION_SUPPORT */
	int i;

	/* If we have the IPv6 functions available we cast to an OSADDRINFO structure otherwise a OSHOSTENET structure */
#ifdef IPv6_FUNCTION_SUPPORT
	addr = (OSADDRINFO *) handle->addr_info;
	for( i=0; i<index; i++ ) {
		addr = addr->ai_next;
	}
	if( addr->ai_family == OS_AF_INET6 ) {
		sock_addr = ((OSSOCKADDR_IN6 *)addr->ai_addr)->sin6_addr.s6_addr;
		memcpy( address, sock_addr, 16 );
		*scope_id =  ((OSSOCKADDR_IN6 *)addr->ai_addr)->sin6_scope_id;
	} else {
		sock_addr = &((OSSOCKADDR *)addr->ai_addr)->sin_addr.s_addr;
		memcpy( address, sock_addr, 4 );
	}
#else
	addr_list = ((OSHOSTENT *) handle->addr_info)->h_addr_list;
	for( i=0; i<index; i++ ) {
		if( addr_list[i] == NULL ) {
			return J9PORT_ERROR_SOCKET_VALUE_NULL;
		}
	}
	memcpy( address, addr_list[index], 4 );
#endif

	return rc;
}





/**
 * Answers a hints structure as an opaque pointer in "result".
 * 
 * This hints structure is used to modify the results returned by a call to @ref j9sock_getaddrinfo.  There is one of
 * these structures per thread, so subsequent calls to this function will overwrite the same structure in memory.
 * The structure is cached in ptBuffers and is cleared when a call to @ref j9port_free_ptBuffer is made.
 *
 * This function is only works on IPv6 supported OS's.  If it is called on an OS that does not support IPv6 then
 * it essentially returns a NULL pointer, meaning it will have no effect on the call to @ref j9sock_getaddrinfo.
 *
 * See man pages, or MSDN doc on getaddrinfo for information on how the hints structure works.
 *
 * @param[in] portLibrary The port library.
 * @param[out] result The filled in (per thread) hints structure
 * @param[in] family A address family type
 * @param[in] socktype A socket type
 * @param[in] protocol A protocol family
 * @param[in] flags Flags for modifying the result
 *
 * @return
 * \arg 0, if no errors occurred, otherwise the (negative) error code
 * \arg J9PORT_ERROR_SOCKET_SYSTEMFULL -- when we can't allocate memory for the ptBuffers, or the hints structure
 *
 * @note current supported family types are:
 * \arg J9ADDR_FAMILY_UNSPEC
 * \arg J9ADDR_FAMILY_AFINET4
 * \arg J9ADDR_FAMILY_AFINET6
 *
 * @note Added for IPv6 support.
 */
int32_t
j9sock_getaddrinfo_create_hints(struct J9PortLibrary *portLibrary, j9addrinfo_t *result, int16_t family, int32_t socktype, int32_t protocol, int32_t flags)
{
	int32_t rc = 0;
#if defined(IPv6_FUNCTION_SUPPORT)
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	OSADDRINFO *addrinfo;
	J9SocketPTB *ptBuffers = NULL;
#endif /* IPv6_FUNCTION_SUPPORT */

	*result = NULL;

	/* If we have the IPv6 functions available we fill in the structure, otherwise it is null */
#if defined(IPv6_FUNCTION_SUPPORT)
#define addrinfohints (ptBuffers->addr_info_hints).addr_info

	/* Initialized the pt buffers if necessary */
	ptBuffers = j9sock_ptb_get(portLibrary);
	if (NULL == ptBuffers) {
		return J9PORT_ERROR_SOCKET_SYSTEMFULL;
	}

	if (NULL == addrinfohints) {
		addrinfohints = omrmem_allocate_memory(sizeof(OSADDRINFO), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == addrinfohints) {
			return J9PORT_ERROR_SOCKET_SYSTEMFULL;
		}
	}
	memset(addrinfohints, 0, sizeof(OSADDRINFO));

	addrinfo = (OSADDRINFO*) addrinfohints;
	addrinfo->ai_flags = flags;
	addrinfo->ai_family = map_addr_family_J9_to_OS( family );
	addrinfo->ai_socktype = map_sockettype_J9_to_OS( socktype );
	addrinfo->ai_protocol = map_protocol_family_J9_to_OS( protocol );
	*result = &ptBuffers->addr_info_hints;

#undef addrinfohints
#endif  /*IPv6_FUNCTION_SUPPORT*/

	return rc;
}





/**
 * Answers the family type of the address at "index" in the structure returned from @ref j9sock_getaddrinfo, indexed starting at 0.
 *
 * Currently the family types we support are J9SOCK_AFINET and J9SOCK_AFINET6.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle The result structure returned by @ref j9sock_getaddrinfo.
 * @param[out] family The family at "index".
 * @param[in] index The address index into the structure returned by @ref j9sock_getaddrinfo.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 *
 * @note Added for IPv6 support.
 */
int32_t
j9sock_getaddrinfo_family(struct J9PortLibrary *portLibrary, j9addrinfo_t handle, int32_t *family, int index)
{
	int32_t rc = 0;
	OSADDRINFO *addr;
    	int i;

	/* If we have the IPv6 functions then we'll cast to a OSADDRINFO othewise we have a hostent */
#ifdef IPv6_FUNCTION_SUPPORT
	addr = (OSADDRINFO *) handle->addr_info;
	for( i=0; i<index; i++ ) {
		addr = addr->ai_next;
	}

	if( addr->ai_family == OS_AF_INET4 ) {
		*family = J9ADDR_FAMILY_AFINET4;
	} else {
		*family = J9ADDR_FAMILY_AFINET6;
	}
#else
	*family = J9ADDR_FAMILY_AFINET4;
#endif

	return rc;
}





/**
 * Answers the number of results returned from @ref j9sock_getaddrinfo.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle The result structure returned by @ref j9sock_getaddrinfo.
 * @param[out] length The number of results.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 *
 * @note Added for IPv6 support.
 */
int32_t
j9sock_getaddrinfo_length(struct J9PortLibrary *portLibrary, j9addrinfo_t handle, int32_t *length)
{
	*length = handle->length;
	return 0;
}





/**
 * Answers the cannon name of the address at "index" in the structure returned from @ref j9sock_getaddrinfo, indexed starting at 0.
 * 
 * The preallocated buffer for "name" should be the size of the maximum host name: OSNIMAXHOST.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle The result structure returned by @ref j9sock_getaddrinfo.
 * @param[out] name The name of the address at "index" in a preallocated buffer.
 * @param[in] index The address index into the structure returned by @ref j9sock_getaddrinfo.
 *
 * @return
 * \arg 0, if no errors occurred, otherwise the (negative) error code.
 * \arg J9PORT_ERROR_SOCKET_VALUE_NULL when we have have the old IPv4 gethostbyname call and the name indexed is out
 * of range.  This is because the address list and the host alias list are not the same length.  Just skip this entry.
 *
 * @note Added for IPv6 support.
 */
int32_t
j9sock_getaddrinfo_name(struct J9PortLibrary *portLibrary, j9addrinfo_t handle, char *name, int index)
{
	int32_t rc = 0;
#ifndef IPv6_FUNCTION_SUPPORT
	char ** alias_list;
#endif /* IPv6_FUNCTION_SUPPORT */
	int i;
	OSADDRINFO *addr;

	/* If we have the IPv6 functions available we cast to an OSADDRINFO structure otherwise a OSHOSTENET structure */
#ifdef IPv6_FUNCTION_SUPPORT

	addr = (OSADDRINFO *) handle->addr_info;
	for( i=0; i<index; i++ ) {
		addr = addr->ai_next;
	}
	if( addr->ai_canonname == NULL ) {
		name[0] = 0;
	} else {
		strcpy( name, addr->ai_canonname );
	}
#else
	alias_list = ((OSHOSTENT *) handle->addr_info)->h_aliases;
	for( i=0; i<index; i++ ) {
		if( alias_list[i] == NULL ) {
			return J9PORT_ERROR_SOCKET_VALUE_NULL;
		}
	}
	if( alias_list[index] == NULL ) {
		name[0] = 0;
	} else {
		strcpy( name, alias_list[index] );
	}
#endif

	return rc;
}





/**
 * Answer information on the host referred to by the address.  The information includes name, aliases and
 * addresses for the nominated host (the latter being relevant on multi-homed hosts).
 * This call has only been tested for addresses of type AF_INET.
 *
 * @param[in] portLibrary The port library.
 * @param[in] addr Pointer to the binary-format (not null-terminated) address, in network byte order.
 * @param[in] length Length of the addr.
 * @param[in] type The type of the addr.
 * @param[out] handle Pointer to the j9hostent_struct, to be linked to the per thread platform hostent struct.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_gethostbyaddr(struct J9PortLibrary *portLibrary, char *addr, int32_t length, int32_t type, j9hostent_t handle)
{
#if defined(J9ZTPF)
    int herr = NO_RECOVERY;
    OMRPORT_ACCESS_FROM_J9PORT(portLibrary);

    J9SOCKDEBUGH("<gethostbyaddr failed, err=%d>\n", herr);
    return omrerror_set_last_error(herr, findHostError(herr));

#else /* defined(J9ZTPF) */

#if !HOSTENT_DATA_R
	OSHOSTENT *result;
#endif
#if GLIBC_R || OTHER_R
	BOOLEAN allocBuffer = FALSE;
#endif
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int herr = 0;
	int i = 0;

	J9SocketPTB *ptBuffers = j9sock_ptb_get(portLibrary);
	if (NULL == ptBuffers) {
		return J9PORT_ERROR_SOCKET_SYSTEMFULL;
	}
#define hostentBuffer (&ptBuffers->hostent)

	/* PR 94621 - one of several threadsafe gethostbyaddr calls must be made depending on the current platform */
	/* if there is a transient error (J9PORT_ERROR_SOCKET_UNIX_TRYAGAIN), try making the call again */
	for (i = 0; i < MAX_RETRIES; i ++) {
#if HOSTENT_DATA_R
#define dataBuffer (ptBuffers->hostent_data)
		if (NULL == dataBuffer) {
			dataBuffer = omrmem_allocate_memory(sizeof(OSHOSTENT_DATA), OMRMEM_CATEGORY_PORT_LIBRARY);
			if (NULL == dataBuffer) {
				return J9PORT_ERROR_SOCKET_SYSTEMFULL;
			}
		}
		herr = gethostbyaddr_r(addr,length,type, hostentBuffer, dataBuffer);
#undef dataBuffer
#elif GLIBC_R || OTHER_R
#define buffer (ptBuffers->gethostBuffer)
#define bufferSize (ptBuffers->gethostBufferSize)
		if (NULL == buffer) {
			bufferSize = GET_HOST_BUFFER_SIZE;
		}

		while (TRUE) {
			if (allocBuffer == TRUE || (NULL == buffer)) {
				/* The buffer is allocated bufferSize + EXTRA_SPACE, while gethostby*_r is only aware of bufferSize
				 * because there seems to be a bug on Linux 386 where gethostbyname_r writes past the end of the
				 * buffer.  This bug has not been observed on other platforms, but EXTRA_SPACE is added anyway as a precaution.*/
				buffer = omrmem_allocate_memory(bufferSize + EXTRA_SPACE, OMRMEM_CATEGORY_PORT_LIBRARY);
				if (NULL == buffer) {
					return J9PORT_ERROR_SOCKET_SYSTEMFULL;
				}
				allocBuffer = FALSE;
			}
#if GLIBC_R
			gethostbyaddr_r(addr,length,type, hostentBuffer, buffer, bufferSize, &result, &herr);
#elif OTHER_R
			result = gethostbyaddr_r(addr,length,type, hostentBuffer, buffer, bufferSize, &herr);
#endif /* GLIBC_R */
			/* allocate more space if the buffer is too small */
			if (ERANGE == herr) {
				omrmem_free_memory(buffer);
				bufferSize *= 2;
				allocBuffer = TRUE;
			} else {
				break;
			}
		}
#undef buffer
#undef bufferSize
#endif
		if (herr != J9PORT_ERROR_SOCKET_UNIX_TRYAGAIN) {
			break;
		}
	}

#if HOSTENT_DATA_R
	if (0 != herr) {
		herr = h_errno;
		J9SOCKDEBUGH( "<gethostbyaddr failed, err=%d>\n", herr);
		return omrerror_set_last_error(herr, findHostError(herr));
	}
#else
	if (NULL == result) {
		J9SOCKDEBUGH( "<gethostbyaddr failed, err=%d>\n", herr );
		return omrerror_set_last_error(herr, findHostError(herr));
	}
#endif
	else {
		memset(handle, 0, sizeof(struct j9hostent_struct));
#if HOSTENT_DATA_R
		handle->entity = hostentBuffer;
#else
		handle->entity = result;
#endif
	}

#undef hostentBuffer

	return 0;
#endif /* defined(J9ZTPF) */
}





/**
 * Answer information on the host, specified by name.  The information includes host name,
 * aliases and addresses.
 *
 * @param[in] portLibrary The port library.
 * @param[in] name The host name string.
 * @param[out] handle Pointer to the j9hostent_struct (to be linked to the per thread platform hostent struct).
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_gethostbyname(struct J9PortLibrary *portLibrary, const char *name, j9hostent_t handle)
{
#if !HOSTENT_DATA_R
	OSHOSTENT *result;
#endif
#if GLIBC_R || OTHER_R
	BOOLEAN allocBuffer = FALSE;
#endif
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int herr = 0;
	int i = 0;

	J9SocketPTB *ptBuffers = j9sock_ptb_get(portLibrary);
	if (NULL == ptBuffers) {
		return J9PORT_ERROR_SOCKET_SYSTEMFULL;
	}
#define hostentBuffer (&ptBuffers->hostent)

	/* PR 94621 - one of several threadsafe gethostbyname calls must be made depending on the current platform */
	/* if there is a transient error (J9PORT_ERROR_SOCKET_UNIX_TRYAGAIN), try making the call again */
	for (i = 0; i < MAX_RETRIES; i ++) {
#if HOSTENT_DATA_R
#define dataBuffer (ptBuffers->hostent_data)
		if (NULL == dataBuffer) {
			dataBuffer = omrmem_allocate_memory(sizeof(OSHOSTENT_DATA), OMRMEM_CATEGORY_PORT_LIBRARY);
			if (NULL == dataBuffer) {
				return J9PORT_ERROR_SOCKET_SYSTEMFULL;
			}
		}
		herr = gethostbyname_r(name, hostentBuffer, dataBuffer);
#undef dataBuffer
#elif GLIBC_R || OTHER_R
#define buffer (ptBuffers->gethostBuffer)
#define bufferSize (ptBuffers->gethostBufferSize)
		if (!buffer) {
			bufferSize = GET_HOST_BUFFER_SIZE;
		}
		
		while (TRUE) {
			if (allocBuffer == TRUE || !buffer) {
				/* The buffer is allocated bufferSize + EXTRA_SPACE, while gethostby*_r is only aware of bufferSize
				 * because there seems to be a bug on Linux 386 where gethostbyname_r writes past the end of the
				 * buffer.  This bug has not been observed on other platforms, but EXTRA_SPACE is added anyway as a precaution.*/
				buffer = omrmem_allocate_memory(bufferSize + EXTRA_SPACE, OMRMEM_CATEGORY_PORT_LIBRARY);
				if (!buffer) {
					return J9PORT_ERROR_SOCKET_SYSTEMFULL;
				}
				allocBuffer = FALSE;
			}
#if GLIBC_R
			gethostbyname_r(name, hostentBuffer, buffer, bufferSize, &result, &herr);
#elif OTHER_R
			result = gethostbyname_r((char *)name, hostentBuffer, buffer, bufferSize, &herr);
#endif /* GLIBC_R */
			/* allocate more space if the buffer is too small */
			if (ERANGE == herr) {
				omrmem_free_memory(buffer);
				bufferSize *= 2;
				allocBuffer = TRUE;
			} else {
				break;
			}
		}
#undef buffer
#undef bufferSize
#endif
		if (herr != J9PORT_ERROR_SOCKET_UNIX_TRYAGAIN) {
			break;
		}
	}

#if HOSTENT_DATA_R
	if (0 != herr) {
		herr = h_errno;
		J9SOCKDEBUGH( "<gethostbyaddr failed, err=%d>\n", herr );
		return omrerror_set_last_error(herr, findHostError(herr));
	}
#else
	if (NULL == result) {
		J9SOCKDEBUGH( "<gethostbyaddr failed, err=%d>\n", herr );
		return omrerror_set_last_error(herr, findHostError(herr));
	}
#endif
	else {
		memset(handle, 0, sizeof(struct j9hostent_struct));
#if HOSTENT_DATA_R
		handle->entity = hostentBuffer;
#else
		handle->entity = result;
#endif
	}

#undef hostentBuffer

	return 0;
}





/**
 * Answer the name of the local host machine.
 *
 * @param[in] portLibrary The port library.
 * @param[in,out] buffer The string buffer to receive the name
 * @param[in] length The length of the buffer
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code
 */
int32_t
j9sock_gethostname(struct J9PortLibrary *portLibrary, char *buffer, int32_t length)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	if (0 != gethostname(buffer, length)) {
		int32_t err = errno;
		J9SOCKDEBUG( "<gethostname failed, err=%d>\n", err);
		return omrerror_set_last_error(err, findError(err));
	}
	return 0;
}





/**
 * Answers the host name of the "in_addr" in a preallocated buffer.
 *
 * The preallocated buffer for "name" should be the size of the maximum host name: OSNIMAXHOST.
 * Currently only AF_INET and AF_INET6 are supported.
 *
 * @param[in] portLibrary The port library.
 * @param[in] in_addr The address we want to do a name lookup on
 * @param[in] sockaddr_size The size of "in_addr"
 * @param[out] name The hostname of the passed address in a preallocated buffer.
 * @param[in] name_length The length of the buffer pointed to by name
 * @param[in] flags Flags on how to form the repsonse (see man pages or doc for getnameinfo)
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code
 *
 * @note Added for IPv6 support.
 * @note "flags" do not affect results on OS's that do not support the IPv6 calls.
 */
int32_t
j9sock_getnameinfo(struct J9PortLibrary *portLibrary, j9sockaddr_t in_addr, int32_t sockaddr_size, char *name, int32_t name_length, int flags)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	/* On z/TPF we don't support this option of returning the host name from the in_addr */
#if defined(J9ZTPF)
	int herr = NO_RECOVERY;
	J9SOCKDEBUGH( "<gethostbyaddr failed, err=%d>\n", herr);
	return omrerror_set_last_error(herr, findHostError(herr));
#else /* defined(J9ZTPF) */
/* If we have the IPv6 functions available we will call them, otherwise we'll call the IPv4 function */
#ifdef IPv6_FUNCTION_SUPPORT
	int rc = 0;

	rc = getnameinfo(  (OSADDR *) &in_addr->addr, sockaddr_size, name, name_length, NULL, 0, flags );
	if (0 != rc) {
		rc = errno;
		J9SOCKDEBUG( "<getnameinfo failed, err=%d>\n", rc);
		return omrerror_set_last_error(rc, findError(rc));
	} 		

	return 0;

#else /* IPv6_FUNCTION_SUPPORT */

#if !HOSTENT_DATA_R
	OSHOSTENT *result;
#endif
#if GLIBC_R || OTHER_R
	BOOLEAN allocBuffer = FALSE;
#endif
	int herr = 0;
	int i = 0;
	int rc = 0;
	int length;
	OSSOCKADDR *addr;
	J9SocketPTB *ptBuffers = NULL;

	addr = (OSSOCKADDR *) &in_addr->addr;
	if (addr->sin_family == OS_AF_INET4) {
		length = 4;
	} else {
		length = 16;
	}

	ptBuffers = j9sock_ptb_get(portLibrary);
	if (NULL == ptBuffers) {
		return J9PORT_ERROR_SOCKET_SYSTEMFULL;
	}
#define hostentBuffer (&ptBuffers->hostent)

	/* PR 94621 - one of several threadsafe gethostbyaddr calls must be made depending on the current platform */
	/* if there is a transient error (J9PORT_ERROR_SOCKET_UNIX_TRYAGAIN), try making the call again */
	for (i = 0; i < MAX_RETRIES; i ++) {
#if HOSTENT_DATA_R
#define dataBuffer (ptBuffers->hostent_data)
		if (NULL == dataBuffer) {
			dataBuffer = omrmem_allocate_memory(sizeof(OSHOSTENT_DATA), OMRMEM_CATEGORY_PORT_LIBRARY);
			if (NULL == dataBuffer) {
				return J9PORT_ERROR_SOCKET_SYSTEMFULL;
			}
		}
#if defined (RS6000)
        herr = gethostbyaddr_r((const char*)&addr->sin_addr,length,addr->sin_family, hostentBuffer, dataBuffer);
#else
        herr = gethostbyaddr_r(&addr->sin_addr,length,addr->sin_family, hostentBuffer, dataBuffer);
#endif
#undef dataBuffer
#elif GLIBC_R || OTHER_R
#define buffer (ptBuffers->gethostBuffer)
#define bufferSize (ptBuffers->gethostBufferSize)
		if (!buffer) {
			bufferSize = GET_HOST_BUFFER_SIZE;
		}
		
		while (TRUE) {
			if (allocBuffer == TRUE || !buffer) {
				/* The buffer is allocated bufferSize + EXTRA_SPACE, while gethostby*_r is only aware of bufferSize
				 * because there seems to be a bug on Linux 386 where gethostbyname_r writes past the end of the
				 * buffer.  This bug has not been observed on other platforms, but EXTRA_SPACE is added anyway as a precaution.*/
				buffer = omrmem_allocate_memory(bufferSize + EXTRA_SPACE, OMRMEM_CATEGORY_PORT_LIBRARY);
				if (NULL == buffer) {
					return J9PORT_ERROR_SOCKET_SYSTEMFULL;
				}
				allocBuffer = FALSE;
			}
#if GLIBC_R
			gethostbyaddr_r((char *)&addr->sin_addr,length,addr->sin_family, hostentBuffer, buffer, bufferSize, &result, &herr);
#elif OTHER_R
			result = gethostbyaddr_r((char *)&addr->sin_addr,length,addr->sin_family, hostentBuffer, buffer, bufferSize, &herr);
#endif /* GLIBC_R */
			/* allocate more space if the buffer is too small */
			if (ERANGE == herr) {
				omrmem_free_memory(buffer);
				bufferSize *= 2;
				allocBuffer = TRUE;
			} else {
				break;
			}
		}
#undef buffer
#undef bufferSize
#endif
		if (herr != J9PORT_ERROR_SOCKET_UNIX_TRYAGAIN) {
			break;
		}
	}

#if HOSTENT_DATA_R
	if (0 != herr) {
		herr = h_errno;
		J9SOCKDEBUGH( "<gethostbyaddr failed, err=%d>\n", herr);
		return omrerror_set_last_error(herr, findHostError(herr));
	}
#else
	if (NULL == result) {
		J9SOCKDEBUGH( "<gethostbyaddr failed, err=%d>\n", herr);
		return omrerror_set_last_error(herr, findHostError(herr));
	}
#endif
	else {
		memset( name, 0, sizeof(char) * name_length );
#if HOSTENT_DATA_R
		strcpy( name, hostentBuffer->h_name );
#else
		strcpy( name, result->h_name );
#endif
	}
#undef hostentBuffer

	return 0;
#endif  /* IPv6_FUNCTION_SUPPORT */

#endif /* defined(J9ZTPF) */
}


#define MAP_LEVEL_OPTION_AND_FAMILY(optlevel, optname, socketP) \
	int32_t platformLevel = platformSocketLevel((optlevel)); \
	int32_t platformOption = platformSocketOption((optname)); \
	int32_t portableFamily = getSocketFamily(portLibrary, (socketP))

#define CHECK_LEVEL_OPTION_AND_FAMILY(variant) \
	if (0 > platformLevel) { \
		Trc_PRT_sock_j9sock_##variant##_invalid_level_Exit(); \
		return platformLevel; \
	} \
	if (0 > platformOption) { \
		Trc_PRT_sock_j9sock_##variant##_invalid_option_Exit(); \
		return platformOption; \
	} \
	if (0 > portableFamily) { \
		Trc_PRT_sock_j9sock_##variant##_invalid_family_Exit(); \
		return portableFamily; \
	} \
	do {} while (0)


/**
 * Answer the value of the nominated boolean socket option.
 * Refer to the private platformSocketLevel & platformSocketOption functions for details of the options
 * supported.
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to query for the option value.
 * @param[in] optlevel	 The level within the IP stack at which the option is defined.
 * @param[in] optname The name of the option to retrieve.
 * @param[out] optval Pointer to the boolean to update with the option value.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_getopt_bool(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  BOOLEAN *optval)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	socklen_t optlen = sizeof(*optval);
	MAP_LEVEL_OPTION_AND_FAMILY(optlevel, optname, socketP);

	Trc_PRT_sock_j9sock_getopt_bool_Entry(socketP, optlevel, optname);

	CHECK_LEVEL_OPTION_AND_FAMILY(getopt_bool);

	if ((IP_MULTICAST_LOOP == platformOption) && (IPPROTO_IP == platformLevel)) {
		if (J9ADDR_FAMILY_AFINET6 == portableFamily) {
			/* Attempt to get an IPv4 option on an IPv6 socket - map the option to the IPv6 variant */
			platformLevel = IPPROTO_IPV6;
			platformOption = IPV6_MULTICAST_LOOP;
		} else if (J9ADDR_FAMILY_AFINET4 == portableFamily) {
			/* Some platforms, such as AIX and QNX, return the IP_MULTICAST_LOOP value as a byte */
			optlen = sizeof(uint8_t);
		} else {
			Trc_PRT_sock_j9sock_getopt_bool_invalid_socket_family_Exit(portableFamily);
			return J9PORT_ERROR_SOCKET_BADAF;
		}
	}

	if (0 != getsockopt(SOCKET_CAST(socketP), platformLevel, platformOption, (void*)optval, &optlen)) {
		int32_t err = errno;
		J9SOCKDEBUG( "<getsockopt (for bool) failed, err=%d>\n", err);
		Trc_PRT_sock_j9sock_getopt_bool_getsockopt_failed_Exit(err);
		return omrerror_set_last_error(err, findError(err));
	}

	/*
	 * Some platforms return values other than 1 or 0 for some options, such as AIX for SO_OOBINLINE, and
	 * the IP_MULTICAST_LOOP return value is stored at the lowest address of optval. Map non-zero values to 1.
	 */
	if (*optval != 0) {
		*optval = 1;
	}

	Trc_PRT_sock_j9sock_getopt_bool_Exit(*optval);
	return 0;
}





/**
 * Answer the value of the nominated byte socket option.
 * Refer to the private platformSocketLevel & platformSocketOption functions for details of the options
 * supported.
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to query for the option value.
 * @param[in] optlevel	 The level within the IP stack at which the option is defined.
 * @param[in] optname The name of the option to retrieve.
 * @param[out] optval Pointer to the byte to update with the option value.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_getopt_byte(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  uint8_t *optval)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	socklen_t optlen = sizeof(*optval);
	MAP_LEVEL_OPTION_AND_FAMILY(optlevel, optname, socketP);

	Trc_PRT_sock_j9sock_getopt_byte_Entry(socketP, SOCKET_CAST(socketP), optlevel, optname);

	CHECK_LEVEL_OPTION_AND_FAMILY(getopt_byte);

 	if (0 != getsockopt(SOCKET_CAST(socketP), platformLevel, platformOption, (void*)optval, &optlen)) {
		int32_t err = errno;
		J9SOCKDEBUG( "<getsockopt (for byte) failed, err=%d>\n", err);
		Trc_PRT_sock_j9sock_getopt_byte_getsockopt_failed_Exit(err);
		return omrerror_set_last_error(err, findError(err));
	}
 	Trc_PRT_sock_j9sock_getopt_byte_Exit();
	return 0;
}





/**
 * Answer the value of the nominated integer socket option.
 * Refer to the private platformSocketLevel & platformSocketOption functions for details of the options
 * supported.
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to query for the option value.
 * @param[in] optlevel The level within the IP stack at which the option is defined.
 * @param[in] optname The name of the option to retrieve.
 * @param[out] optval Pointer to the integer to update with the option value.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_getopt_int(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  int32_t *optval)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	MAP_LEVEL_OPTION_AND_FAMILY(optlevel, optname, socketP);
	socklen_t optlen = sizeof(*optval);
	Trc_PRT_sock_j9sock_getopt_int_Entry(socketP, SOCKET_CAST(socketP), optlevel, optname);

	CHECK_LEVEL_OPTION_AND_FAMILY(getopt_int);

#if defined(AIXPPC) || defined(J9ZOS390)
	if((IPPROTO_IP == platformLevel) && (IP_MULTICAST_TTL == platformOption) && (J9ADDR_FAMILY_AFINET6 == portableFamily)){
		platformLevel = IPPROTO_IPV6;
		platformOption = IPV6_MULTICAST_HOPS;
	}
#endif

 	if (0 != getsockopt(SOCKET_CAST(socketP), platformLevel, platformOption, (void*)optval, &optlen)) {
		int32_t err = errno;
		J9SOCKDEBUG( "<getsockopt (for int) failed, err=%d>\n", err);
		Trc_PRT_sock_j9sock_getopt_int_getsockopt_failed_Exit(err);
		return omrerror_set_last_error(err, findError(err));
	}
 	Trc_PRT_sock_j9sock_getopt_int_Exit();
	return 0;
}





/**
 * Answer the value of the socket linger option.
 * See the @ref j9sock_linger_init for details of the linger behavior.
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to query for the option value
 * @param[in] optlevel The level within the IP stack at which the option is defined
 * @param[in] optname The name of the option to retrieve
 * @param[out] optval Pointer to the linger struct to update with the option value
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code
 */
int32_t
j9sock_getopt_linger(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  j9linger_t optval)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t platformLevel = platformSocketLevel(optlevel);
	int32_t platformOption = platformSocketOption(optname);
	socklen_t optlen = sizeof(optval->linger);

	if (0 > platformLevel) {
		return platformLevel;
	}
	if (0 > platformOption) {
		return platformOption;
	}

	if (0 != getsockopt(SOCKET_CAST(socketP), platformLevel, platformOption, (char*)(&optval->linger), &optlen)) {
		int32_t err = errno;
		J9SOCKDEBUG( "<getsockopt (for linger) failed, err=%d>\n", err);
		return omrerror_set_last_error(err, findError(err));
	}
	return 0;
}





/**
 * Answer the value of the socket option, an address struct.
 * Currently only used to retrieve the interface of multicast sockets, 
 * but the more general call style has been used.
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to query for the option value.
 * @param[in] optlevel	 The level within the IP stack at which the option is defined.
 * @param[in] optname The name of the option to retrieve.
 * @param[out] optval Pointer to the sockaddr struct to update with the option value.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_getopt_sockaddr(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname, j9sockaddr_t optval)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	MAP_LEVEL_OPTION_AND_FAMILY(optlevel, optname, socketP);

	/* if IPv4 the OS returns in_addr, if IPv6, value of interface index is returned */
	typedef union byte_or_int {
		uint8_t byte;
		uint32_t integer;
	} value;
	value val;
	socklen_t optlen = sizeof(val);
	OSSOCKADDR *sockaddr;

	Trc_PRT_sock_j9sock_getopt_sockaddr_Entry(socketP, SOCKET_CAST(socketP), platformLevel, platformOption, optval);

	CHECK_LEVEL_OPTION_AND_FAMILY(getopt_sockaddr);

#if defined(AIXPPC) || defined(J9ZOS390)
	if ((IPPROTO_IP == platformLevel) && (IP_MULTICAST_IF == platformOption) && (J9ADDR_FAMILY_AFINET6 == portableFamily)){
		platformLevel = IPPROTO_IPV6;
		platformOption = IPV6_MULTICAST_IF;
	}
#endif

	if(0 != getsockopt(SOCKET_CAST(socketP), platformLevel, platformOption, (char*)&val, &optlen)) {
		int32_t err = errno;
		J9SOCKDEBUG( "<getsockopt (for sockaddr) failed, err=%d>\n", err);
		Trc_PRT_sock_j9sock_getopt_sockaddr_getsockopt_failed_Exit(err);
		return omrerror_set_last_error(err, findError(err));
	}
	sockaddr = (OSSOCKADDR*) &optval->addr;
	/* if socket family is IPv6, and getsockopt can return an interface index (length 1),
	 * which means an IPv6 address needs to be looked up, or getsockopt can return an in_addr structure,
	 * for the case of an IPv4 address */
	if (J9ADDR_FAMILY_AFINET6 == portableFamily) {
		if (optlen == sizeof(uint8_t)) {
			/* lookup the address with the interface index */
			int32_t result = lookupIPv6AddressFromIndex(portLibrary, val.byte, (j9sockaddr_t) sockaddr);
			if (0 != result){
				return result;
			}
		} else if (optlen == sizeof(struct in_addr)){
			/* if optlen is 4, address is IPv4 */
			sockaddr->sin_addr.s_addr = val.integer;
		} else {
			Trc_PRT_sock_j9sock_getopt_sockaddr_address_length_invalid_Exit(portableFamily);
			return J9PORT_ERROR_SOCKET_OPTARGSINVALID;
		}
	} else if (J9ADDR_FAMILY_AFINET4 == portableFamily) {
		/* portableFamily is AFINET4 when preferIPv4Stack=true */
		if (optlen == sizeof(struct in_addr)) {
			sockaddr->sin_addr.s_addr = val.integer;
		} else {
			Trc_PRT_sock_j9sock_getopt_sockaddr_address_length_invalid_Exit(portableFamily);
			return J9PORT_ERROR_SOCKET_OPTARGSINVALID;
		}
		/* A temporary fix as Linux ARM (maybe others) returns 0 for the family. */
		sockaddr->sin_family = AF_INET;
	} else {
		Trc_PRT_sock_j9sock_getopt_sockaddr_socket_family_invalid_Exit(portableFamily);
		return J9PORT_ERROR_SOCKET_BADAF;
	}

	Trc_PRT_sock_j9sock_getopt_sockaddr_Exit();
	return 0;
}





/**
 * Answer the remote name for the socket.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Pointer to the socket to get the address of.
 * @param[out] addrHandle Pointer to the sockaddr struct to update with the address.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_getpeername(struct J9PortLibrary *portLibrary, j9socket_t handle, j9sockaddr_t addrHandle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	socklen_t addrlen = sizeof(addrHandle->addr);

	if (getpeername(SOCKET_CAST(handle), (struct sockaddr*) &addrHandle->addr, &addrlen) != 0) {
		int32_t err = errno;
		J9SOCKDEBUG( "<getpeername failed, err=%d>\n", err);
		return omrerror_set_last_error(err, findError(err));
	}
	return 0;
}





/**
 * Answer the local name for the socket.  Note, the stack getsockname function
 * actually answers a sockaddr structure, not a string name as the function name
 * might imply.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Pointer to the socket to get the address of.
 * @param[out] addrHandle Pointer to the sockaddr struct to update with the address.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_getsockname(struct J9PortLibrary *portLibrary, j9socket_t handle, j9sockaddr_t addrHandle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	socklen_t addrlen = sizeof(addrHandle->addr);
	int32_t rc = 0;

	Trc_PRT_sock_j9sock_getsockname_Entry(handle);

	if (getsockname(SOCKET_CAST(handle), (struct sockaddr*) &addrHandle->addr, &addrlen) != 0) {
		int32_t err = errno;
		J9SOCKDEBUG( "<getsockname failed, err=%d>\n", err);
		rc = omrerror_set_last_error(err, findError(err));
		Trc_PRT_sock_j9sock_getsockname_failure(err);
	}
	Trc_PRT_sock_j9sock_getsockname_Exit(addrHandle, rc);

	return rc;
}





/**
 * Answer the nominated element of the address list within the argument hostent struct.
 * The address is in network order.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Pointer to the hostent struct, in which to access the addr_list.
 * @param[in] index The index of the element within the addr_list to retrieve.
 *
 * @return	the address, in network order.
 */
int32_t
j9sock_hostent_addrlist(struct J9PortLibrary *portLibrary, j9hostent_t handle, uint32_t index)
{
	return *((int32_t*)handle->entity->h_addr_list[index]);
}





/**
 * Answer a reference to the list of alternative names for the host within the argument hostent struct.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Pointer to the hostent struct, in which to access the addr_list
 * @param[out] aliasList Pointer to the list of alternative names, to be updated 
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code
 */
int32_t
j9sock_hostent_aliaslist(struct J9PortLibrary *portLibrary, j9hostent_t handle, char ***aliasList)
{
	*aliasList = handle->entity->h_addr_list;
	return 0;
}





/**
 * Answer the host name (string) within the argument hostent struct.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Pointer to the hostent struct, in which to access the hostName.
 * @param[out] hostName Host name string.
 *
 * @return	0, the function does not validate the name access.
 */
int32_t
j9sock_hostent_hostname(struct J9PortLibrary *portLibrary, j9hostent_t handle, char** hostName)
{
	*hostName = handle->entity->h_name;
	return 0;
}





/**
 * Answer the 32 bit host ordered argument, in network byte order.
 *
 * @param[in] portLibrary The port library.
 * @param[in] val The 32 bit host ordered number.
 *
 * @return	the 32 bit network ordered number.
 */
int32_t
j9sock_htonl(struct J9PortLibrary *portLibrary, int32_t val)
{
	return htonl(val);
}





/**
 * Answer the 16 bit host ordered argument, in network byte order.
 *
 * @param[in] portLibrary The port library.
 * @param[in] val The 16 bit host ordered number.
 *
 * @return	the 16 bit network ordered number.
 */
uint16_t
j9sock_htons(struct J9PortLibrary *portLibrary, uint16_t val)
{
	return htons(val);
}





/**
 * Answer the dotted IP string as an Internet address.
 *
 * @param[in] portLibrary The port library.
 * @param[in] addrStr The dotted IP string.
 * @param[out] addr Pointer to the Internet address.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_inetaddr(struct J9PortLibrary *portLibrary, const char *addrStr, uint32_t *addr)
{
	int32_t rc = 0;
	uint32_t val;

	val = inet_addr((char *)addrStr);
	if (-1 == val) {
		J9SOCKDEBUGPRINT( "<inet_addr failed>\n" );
		rc = J9PORT_ERROR_SOCKET_ADDRNOTAVAIL;
	} else {
		*addr = val;
	}
	return rc;
}





/**
 * Answer the Internet address as a dotted IP string.
 *
 * @param[in] portLibrary The port library.
 * @param[out] addrStr The dotted IP string.
 * @param[in] nipAddr The Internet address.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_inetntoa(struct J9PortLibrary *portLibrary, char **addrStr, uint32_t nipAddr)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	uint8_t *addr = (uint8_t *)&nipAddr;
	J9SocketPTB *ptBuffers = j9sock_ptb_get(portLibrary);
	if (NULL == ptBuffers) {
		return J9PORT_ERROR_SOCKET_SYSTEMFULL;
	}
	omrstr_printf(ptBuffers->ntoa, NTOA_SIZE, "%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);
	*addrStr = ptBuffers->ntoa;
	return 0;
}





/**
 * Initializes a new multicast membership structure.  The membership structure is used to join & leave
 * multicast groups @see j9sock_setopt_ipmreq.  The group may be joined using 0 (J9SOCK_INADDR_ANY)
 * as the local interface, in which case the default local address will be used.
 *
 * @param[in] portLibrary The port library.
 * @param[out] handle Pointer to the multicast membership struct.
 * @param[in] nipmcast The address, in network order, of the multicast group to join.
 * @param[in] nipinterface The address, in network order, of the local machine interface to join on.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_ipmreq_init(struct J9PortLibrary *portLibrary, j9ipmreq_t handle, uint32_t nipmcast, uint32_t nipinterface)
{
	memset(handle, 0, sizeof(struct j9ipmreq_struct));

	handle->addrpair.imr_multiaddr.s_addr = nipmcast;
	handle->addrpair.imr_interface.s_addr = nipinterface;
	return 0;

}





/**
 * Fills in a preallocated j9ipv6_mreq_struct
 *
 * @param[in] portLibrary The port library.
 * @param[out] handle A pointer to the j9ipv6_mreq_struct to populate.
 * @param[in] ipmcast_addr The ip mulitcast address.
 * @param[in] ipv6mr_interface The ip mulitcast inteface.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 *
 * @note Added for IPv6 support.
 */
int32_t
j9sock_ipv6_mreq_init(struct J9PortLibrary *portLibrary, j9ipv6_mreq_t handle, uint8_t *ipmcast_addr, uint32_t ipv6mr_interface)
{
#ifdef IPv6_FUNCTION_SUPPORT
	memset(handle, 0, sizeof(struct j9ipmreq_struct));
	memcpy( handle->mreq.ipv6mr_multiaddr.s6_addr, ipmcast_addr, 16 );
	handle->mreq.ipv6mr_interface = ipv6mr_interface;
#endif
	return 0;
}





/**
 * Answer true if the linger is enabled in the argument linger struct.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Pointer to the linger struct to be accessed.
 * @param[out] enabled Pointer to the boolean to be updated with the linger status.
 *
 * @return	0, the function does not validate the access.
 */
int32_t
j9sock_linger_enabled(struct J9PortLibrary *portLibrary, j9linger_t handle, BOOLEAN *enabled)
{
	*enabled = (BOOLEAN)(handle->linger.l_onoff);
	return 0;
}





/**
 * Initializes a new linger structure, enabled or disabled, with the timeout as specified.
 * Linger defines the behavior when unsent messages exist for a socket that has been sent close.
 * If linger is disabled, the default, close returns immediately and the stack attempts to deliver unsent messages.
 * If linger is enabled:
 * \arg if the timeout is 0, the close will block indefinitely until the messages are sent
 * \arg if the timeout is set, the close will return after the messages are sent or the timeout period expired
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Pointer to the linger struct to be accessed.
 * @param[in] enabled Aero to disable, a non-zero value to enable linger.
 * @param[in] timeout	 0 to linger indefinitely or a positive timeout value (in seconds).
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_linger_init(struct J9PortLibrary *portLibrary, j9linger_t handle, int32_t enabled, uint16_t timeout)
{
	memset(handle, 0, sizeof (struct j9linger_struct));
	handle->linger.l_onoff = enabled;
	handle->linger.l_linger = (int32_t) timeout;
	return 0;
}





/**
 * Answer the linger timeout value in the argument linger struct.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Pointer to the linger struct to be accessed.
 * @param[out] linger Pointer to the integer, to be updated with the linger value (in seconds).
 *
 * @return	0, the function does not validate the access.
 */
int32_t
j9sock_linger_linger(struct J9PortLibrary *portLibrary, j9linger_t handle, uint16_t *linger)
{
	*linger = (uint16_t)(handle->linger.l_linger);
	return 0;
}





/**
 * Set the socket to listen for incoming connection requests.  This call is made prior to accepting requests,
 * via the @ref j9sock_accept function.  The backlog specifies the maximum length of the queue of pending connections,
 * after which further requests are rejected.
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock Pointer to the socket to modify.
 * @param[in] backlog The maximum number of queued requests.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_listen(struct J9PortLibrary *portLibrary, j9socket_t sock, int32_t backlog)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;

 	if (listen(SOCKET_CAST(sock), backlog) < 0) {
		rc = errno;
		J9SOCKDEBUG( "<listen failed, err=%d>\n", rc);
		rc = omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_OPFAILED);
	}
	return rc;
}





/**
 * Answer the 32 bit network ordered argument, in host byte order.
 *
 * @param[in] portLibrary The port library.
 * @param[in] val The 32 bit network ordered number.
 *
 * @return	the 32 bit host ordered number.
 */
int32_t
j9sock_ntohl(struct J9PortLibrary *portLibrary, int32_t val)
{
	return ntohl(val);
}





/**
 * Answer the 16-bit network ordered argument, in host byte order.
 *
 * @param[in] portLibrary The port library.
 * @param[in] val The 16-bit network ordered number.
 *
 * @return	the 16-bit host ordered number.
 */
uint16_t
j9sock_ntohs(struct J9PortLibrary *portLibrary, uint16_t val)
{
	uint16_t rc;

	Trc_PRT_sock_j9sock_ntohs_Entry(val);

	rc = ntohs(val);

	Trc_PRT_sock_j9sock_ntohs_Exit(rc);

	return rc;
}

int32_t
j9sock_read(struct J9PortLibrary *portLibrary, j9socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t bytesRec = 0;

	Trc_PRT_sock_j9sock_read_Entry(sock, nbyte, flags);
	
	bytesRec = recv(SOCKET_CAST(sock), buf, nbyte, flags );
	if (-1 == bytesRec) {
		int32_t err = errno;
		Trc_PRT_sock_j9sock_read_failure(errno);
		J9SOCKDEBUG( "<recv failed, err=%d>\n", err);
		err = omrerror_set_last_error(err, findError(err));
		Trc_PRT_sock_j9sock_read_Exit(err);
		return err;
	} else {
		Trc_PRT_sock_j9sock_read_Exit(bytesRec);
		return bytesRec;
	}
}

/**
 * The read function receives data from a possibly connected socket.  Calling read will return as much 
 * information as is currently available up to the size of the buffer supplied.  If the information is too large
 * for the buffer, the excess will be discarded.  If no incoming  data is available at the socket, the read call 
 * blocks and waits for data to arrive.  It the address argument is not null, the address will be updated with
 * address of the message sender.
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock Pointer to the socket to read on.
 * @param[out] buf Pointer to the buffer where input bytes are written.
 * @param[in] nbyte The length of buf.
 * @param[in] flags Tthe flags, to influence this read.
 * @param[out] addrHandle	if provided, the address to be updated with the sender information.
 *
 * @return
 * \arg If no error occurs, return the number of bytes received.
 * \arg If the connection has been gracefully closed, return 0.
 * \arg Otherwise return the (negative) error code.
 */
int32_t
j9sock_readfrom(struct J9PortLibrary *portLibrary, j9socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags, j9sockaddr_t addrHandle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t bytesRec = 0;
	socklen_t addrlen;

	Trc_PRT_sock_j9sock_readfrom_Entry(sock, nbyte, flags, addrHandle);
	
	if (NULL == addrHandle) {
		addrlen = sizeof(*addrHandle);
		bytesRec = recvfrom(SOCKET_CAST(sock), buf, nbyte, flags, NULL, &addrlen);
	} else {
		addrlen = sizeof(addrHandle->addr);
		bytesRec = recvfrom(SOCKET_CAST(sock), buf, nbyte, flags, (struct sockaddr*) &addrHandle->addr, &addrlen);
	}
	if (-1 == bytesRec) {
		int32_t err = errno;
		Trc_PRT_sock_j9sock_readfrom_failure(errno);
		J9SOCKDEBUG( "<recvfrom failed, err=%d>\n", err);
		err = omrerror_set_last_error(err, findError(err));
		Trc_PRT_sock_j9sock_readfrom_Exit(err);
		return err;
	} else {
		Trc_PRT_sock_j9sock_readfrom_Exit(bytesRec);
		return bytesRec;
	}
}


/**
 * The select function allows the state of sockets for read & write operations and exceptional conditions to be tested.
 * The function is used prior to a j9sock_read/readfrom, to control the period the operation may block for.
 * Depending upon the timeout specified:
 * \arg 0, return immediately with the status of the descriptors
 * \arg timeout, return when one of the descriptors is ready or after the timeout period has expired
 * \arg null, block indefinitely for a ready descriptor
 *
 * Note 1: j9fdset_t does not support multiple entries.
 * Note 2: one of readfd or writefd must contain a valid entry.
 *
 * @param[in] portLibrary The port library.
 * @param[in] nfds For all the sockets that are set in readfds, writefds and exceptfds, this is the maximum value returned
 *            by @ref j9sock_fdset_size().
 * @param[in] readfd The j9fdset_t representing the descriptor to be checked to see if data can be read without blocking.
 * @param[in] writefd The j9fdset_t representing the descriptor to be checked to see if data can be written without blocking.
 * @param[in] exceptfd_notSupported Not supported, must either by NULL or empty
 * @param[in] timeout Pointer to the timeout (a j9timeval struct).
 *
 * @return	Number of socket descriptors that are ready, otherwise return the (negative) error code.
 */
int32_t
j9sock_select(struct J9PortLibrary *portLibrary, int32_t nfds, j9fdset_t readfd, j9fdset_t writefd, j9fdset_t exceptfd_notSupported, j9timeval_t timeout)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
#if !defined(LINUX)	
	int32_t result = 0;
#endif /* !defined(LINUX) */

	Trc_PRT_sock_j9sock_select_Entry(nfds, readfd, writefd, exceptfd_notSupported, timeout == NULL ? 0 : timeout->time.tv_sec, timeout == NULL ? 0 : timeout->time.tv_usec);

	/**
	 * CMVC 201051
	 * Checking whether nfds >= FD_SETSIZE(1024) on Linux is unnecessary in that
	 * 1) it is using poll(..) which doesn't use fdset at all.
	 * 2) nfds also gets ignored when using poll(..) on Linux.
	 * The checking code should be kept active on other platforms (zOS/AIX) since they still use select(..)
	 * and the default value of FD_SETSIZE varies with platforms.
	 */
#if defined(LINUX)
	if (NULL == timeout) {
#else
	if ((nfds >= FD_SETSIZE) || (NULL == timeout)) {
#endif
		rc = J9PORT_ERROR_SOCKET_ARGSINVALID;
	} else {

		if (NULL != exceptfd_notSupported) {
#if defined(LINUX)
			if (-1 != exceptfd_notSupported->fd) {
#else
			if (NULL != &exceptfd_notSupported->handle) {
#endif
				rc = omrerror_set_last_error_with_message(J9PORT_ERROR_SOCKET_ARGSINVALID, "exceptfd_notSupported cannot contain a valid fd");
				Trc_PRT_sock_j9sock_select_Exit(rc);
				return rc;
			}
		}

#if defined(LINUX)
		int timeoutms;
		int pollrc = 0;
		struct pollfd pfds[2];
		uintptr_t numEntriesInPfd = 0;
		BOOLEAN readfdHasValidFD = FALSE;
		BOOLEAN writefdHasValidFD = FALSE;

		memset(pfds, 0, sizeof(pfds));

		timeoutms = timeout->time.tv_usec/1000 + timeout->time.tv_sec*1000;

		if (NULL != readfd) {
			if (-1 != readfd->fd) {
				readfdHasValidFD = TRUE;
			}
		}

		if (NULL != writefd) {
			if (-1 != writefd->fd) {
				writefdHasValidFD = TRUE;
			}
		}

		/* at least one of the fds must be valid */
		if (TRUE != (readfdHasValidFD || writefdHasValidFD)) {
			rc = omrerror_set_last_error_with_message(J9PORT_ERROR_SOCKET_ARGSINVALID, "One of readfd and writefd must contain a valid socket");
			Trc_PRT_sock_j9sock_select_Exit(rc);
			return rc;
		}

		/* so, we have at least one valid fd, fill out the poll pfds array */

		if (readfdHasValidFD) {
			pfds[0].fd = readfd->fd;
			pfds[0].events |= POLLIN | POLLPRI;
			portLibrary->sock_fdset_zero(portLibrary, readfd);
			numEntriesInPfd++;

			if (writefdHasValidFD) {
				if (writefd->fd == readfd->fd) {
					/* same fd, can store both read/write events in the first pollfd entry */
					pfds[0].events |= POLLOUT;
					portLibrary->sock_fdset_zero(portLibrary, writefd);
				} else {
					pfds[1].fd = writefd->fd;
					pfds[1].events |= POLLOUT;
					portLibrary->sock_fdset_zero(portLibrary, writefd);
					numEntriesInPfd++;
				}
			}

		} else	{
			pfds[0].fd = writefd->fd;
			pfds[0].events |= POLLOUT;
			portLibrary->sock_fdset_zero(portLibrary, writefd);
			numEntriesInPfd++;
		}

		/**
		 * JAZZ103 80030: PMR 46171,001,866 : JDWP module failed to initialize due to socket accept failed with J9PORT_ERROR_SOCKET_OPFAILED
		 *
		 * In the case of poll fails with EINTR, continue trying to poll since EINTR happens randomly for no reason.
		 */
		do {
			/* Make sure revents is initilized to 0 before poll call. revents might not be initilized to 0 if this is not the first try of calling poll */
			pfds[0].revents = 0;
			pfds[1].revents = 0;
			pollrc = poll(pfds, numEntriesInPfd, timeoutms);
		} while ((-1 == pollrc) && (EINTR == errno));

		if (0 < pollrc) {
			/* an event occurred */

			uintptr_t i;
			for (i = 0; i < numEntriesInPfd; i++){

				if (0 != (pfds[i].revents & (POLLIN | POLLPRI))) {
					/* there's data to read */
					readfd->fd = pfds[i].fd;
					rc++;
				} else if (0 != (POLLOUT & pfds[i].revents)) {
					/* writing won't block */
					writefd->fd = pfds[i].fd;
					rc++;
				}
			}
		} else if (0 == pollrc) {
			/* timeout */
			Trc_PRT_sock_j9sock_select_timeout();
			rc = J9PORT_ERROR_SOCKET_TIMEOUT;
		} else {
			/* something went wrong in the call to poll */
			Trc_PRT_sock_j9sock_select_failure(errno);
			rc = omrerror_set_last_error(pollrc, findError(pollrc));
		}
	}
#else
		result = select( nfds, readfd == NULL ? NULL : &readfd->handle, writefd == NULL ? NULL : &writefd->handle, NULL,
				timeout == NULL ? NULL : &timeout->time);
		if (-1 == result) {
			rc = errno;
			Trc_PRT_sock_j9sock_select_failure(errno);
			J9SOCKDEBUG( "<select failed, err=%d>\n", rc);
			rc = omrerror_set_last_error(rc, findError(rc));
		} else {
			if (result) {
				rc = result;
			} else {
				rc = J9PORT_ERROR_SOCKET_TIMEOUT;
				Trc_PRT_sock_j9sock_select_timeout();
			}
		}
	}

#endif

	Trc_PRT_sock_j9sock_select_Exit(rc);
	return rc;

}






int32_t
j9sock_select_read(struct J9PortLibrary *portLibrary, j9socket_t j9socketP, int32_t secTime, int32_t uSecTime, BOOLEAN isNonBlocking)
{
	j9timeval_struct timeP;
	int32_t result = 0;
	int32_t size = 0;
	J9SocketPTB *ptBuffers = NULL;

	Trc_PRT_sock_j9sock_select_read_Entry(j9socketP, secTime, uSecTime, isNonBlocking);
	ptBuffers = j9sock_ptb_get(portLibrary);
	if (NULL == ptBuffers) {
		Trc_PRT_sock_j9sock_select_read_failure("NULL == ptBuffers");
		Trc_PRT_sock_j9sock_select_read_Exit(J9PORT_ERROR_SOCKET_SYSTEMFULL);
		return J9PORT_ERROR_SOCKET_SYSTEMFULL;
	}

/* The max fdset size per process is always expected to be less than a 32bit integer value.
 * Is this valid on a 64bit platform?
 */

	/*[PR 99074] Removed check for zero timeout */

	result = j9sock_fdset_init( portLibrary, j9socketP );
	if ( 0 != result) {
		Trc_PRT_sock_j9sock_select_read_failure("0 != j9sock_fdset_init( portLibrary, j9socketP )");
		Trc_PRT_sock_j9sock_select_read_Exit(result);
		return result;
	}

	j9sock_timeval_init( portLibrary, secTime, uSecTime, &timeP );
	size = j9sock_fdset_size( portLibrary, j9socketP );
	if ( 0 > size) {
		Trc_PRT_sock_j9sock_select_read_failure("0 > j9sock_fdset_size( portLibrary, j9socketP )");
		result = J9PORT_ERROR_SOCKET_FDSET_SIZEBAD;
	} else {
		result = j9sock_select(portLibrary, size, ptBuffers->fdset, NULL, NULL, &timeP);
	}
	Trc_PRT_sock_j9sock_select_read_Exit(result);
	return result;
}

int32_t
j9sock_set_nonblocking(struct J9PortLibrary *portLibrary, j9socket_t socketP, BOOLEAN nonblocking)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc;
	uint32_t param = nonblocking;

	Trc_PRT_sock_j9sock_setnonblocking_Entry(socketP, nonblocking);
	/* set the socket to non blocking or block as requested */	
	rc = ioctl (SOCKET_CAST(socketP), FIONBIO, &param);
	
	if (rc < 0)	{
		rc = errno;
		Trc_PRT_sock_j9sock_setnonblocking_failure(errno);
		switch(rc) {
			case J9PORT_ERROR_SOCKET_UNIX_EINVAL:
				rc = J9PORT_ERROR_SOCKET_OPTARGSINVALID;
			default:
				rc = omrerror_set_last_error(rc, findError(rc));
		}
	}
	
	Trc_PRT_sock_j9sock_setnonblocking_Exit(rc);
	return rc;
}

/**
 * Ensure the flag designated is set in the argument.  This is used to construct arguments for the 
 * j9sock_read/readfrom/write/writeto calls with optional flags, such as J9SOCK_MSG_PEEK.
 *
 * @param[in] portLibrary The port library.
 * @param[in] flag The operation flag to set in the argument.
 * @param[in] arg Pointer to the argument to set the flag bit in.
 *
 * @return	0 if no error occurs, otherwise return the (negative) error code.
 */
int32_t
j9sock_setflag(struct J9PortLibrary *portLibrary, int32_t flag, int32_t *arg)
{
	int32_t rc = 0;
	
	if(flag == J9SOCK_MSG_PEEK) {
		*arg |= MSG_PEEK;
	} else if (flag == J9SOCK_MSG_OOB) {
		*arg |= MSG_OOB;
	} else {
		rc = J9PORT_ERROR_SOCKET_UNKNOWNFLAG;
	}
	return rc;
}





/**
 * Set the value of the nominated boolean socket option.
 * Refer to the private platformSocketLevel & platformSocketOption functions for details of the options
 * supported.
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to set the option in.
 * @param[in] optlevel The level within the IP stack at which the option is defined.
 * @param[in] optname The name of the option to set.
 * @param[in] optval Pointer to the boolean to update the socket option with.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_setopt_bool(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  BOOLEAN *optval)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	socklen_t optlen = sizeof(*optval);
	uint32_t val = ((NULL == optval) || (0 != *optval)) ? 1 : 0;
	MAP_LEVEL_OPTION_AND_FAMILY(optlevel, optname, socketP);

	Trc_PRT_sock_j9sock_setopt_bool_Entry(socketP, optlevel, optname, val);

	CHECK_LEVEL_OPTION_AND_FAMILY(setopt_bool);

	if ((IP_MULTICAST_LOOP == platformOption) && (IPPROTO_IP == platformLevel)) {
		if (J9ADDR_FAMILY_AFINET6 == portableFamily) {
			/* Attempt to set an IPv4 option on an IPv6 socket - map the option to the IPv6 variant */
			platformLevel = IPPROTO_IPV6;
			platformOption = IPV6_MULTICAST_LOOP;
		} else if (J9ADDR_FAMILY_AFINET4 == portableFamily) {
			/* Some platforms, such as AIX and QNX, expect the IP_MULTICAST_LOOP value to be a byte */
			optlen = sizeof(uint8_t);
			*((uint8_t *) &val) = (uint8_t) val;
		} else {
			Trc_PRT_sock_j9sock_setopt_bool_invalid_socket_family_Exit(portableFamily);
			return J9PORT_ERROR_SOCKET_BADAF;
		}
	}

	if (0 != setsockopt(SOCKET_CAST(socketP), platformLevel, platformOption, (void *) &val, optlen)) {
		int32_t err = errno;
		J9SOCKDEBUG( "<setsockopt (for bool) failed, err=%d>\n", err);
		Trc_PRT_sock_j9sock_setopt_bool_setsockopt_failed_Exit(err);
		return omrerror_set_last_error(err, findError(err));
	}

	Trc_PRT_sock_j9sock_setopt_bool_Exit(0);
	return 0;
}





/**
 * Set the value of the nominated byte socket option.
 * Refer to the private platformSocketLevel & platformSocketOption functions for details of the options
 * supported.
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to set the option in.
 * @param[in] optlevel The level within the IP stack at which the option is defined.
 * @param[in] optname The name of the option to set.
 * @param[in] optval Pointer to the byte to update the socket option with.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_setopt_byte(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  uint8_t *optval)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	MAP_LEVEL_OPTION_AND_FAMILY(optlevel, optname, socketP);
	socklen_t optlen = sizeof(*optval);

	Trc_PRT_sock_j9sock_setopt_byte_Entry(socketP, optlevel, optname, optval == NULL ? 0 : *optval);
	
	CHECK_LEVEL_OPTION_AND_FAMILY(setopt_byte);

 	if (0 != setsockopt(SOCKET_CAST(socketP), platformLevel, platformOption, (void*)optval, optlen)) {
		int32_t err = errno;
		Trc_PRT_sock_j9sock_setopt_failure_code("byte", errno);
		J9SOCKDEBUG( "<setsockopt (for byte) failed, err=%d>\n", err);
		err = omrerror_set_last_error(err, findError(err));
		Trc_PRT_sock_j9sock_setopt_byte_Exit(err);
		return err;
	}

 	Trc_PRT_sock_j9sock_setopt_byte_Exit(0);
	return 0;
}





/**
 * Set the value of the nominated integer socket option.
 * Refer to the private platformSocketLevel & platformSocketOption functions for details of the options
 * supported.
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to set the option in.
 * @param[in] optlevel The level within the IP stack at which the option is defined.
 * @param[in] optname The name of the option to set.
 * @param[in] optval Pointer to the integer to update the socket option with.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_setopt_int(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  int32_t *optval)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	MAP_LEVEL_OPTION_AND_FAMILY(optlevel, optname, socketP);
	socklen_t optlen = sizeof(*optval);
	int32_t optvalueUsed = *optval;
	
	Trc_PRT_sock_j9sock_setopt_int_Entry(socketP, optlevel, optname, optval == NULL ? 0 : *optval);

	CHECK_LEVEL_OPTION_AND_FAMILY(setopt_int);

#if defined(AIXPPC) || defined(J9ZOS390)
	if ((SOL_SOCKET == platformLevel) && (SO_RCVBUF == platformOption)) {
		/* get current rcvbuf value and store in optval,
		 * use the lesser of the 2 values, as AIX returns error if set to greater than max
		 */
	 	if (0 != getsockopt(SOCKET_CAST(socketP), platformLevel, platformOption, (void*)optval, &optlen)) {
			int32_t err = errno;
			J9SOCKDEBUG( "<getsockopt (for int) failed, err=%d>\n", err);
			return omrerror_set_last_error(err, findError(err));
		}
		if (optvalueUsed > *optval){
			optvalueUsed = *optval;
		}
	} else if((IPPROTO_IP == platformLevel) && (IP_MULTICAST_TTL == platformOption) && (J9ADDR_FAMILY_AFINET6 == portableFamily)){
		platformLevel = IPPROTO_IPV6;
		platformOption = IPV6_MULTICAST_HOPS;
	}
#endif

	/* for LINUX in order to enable support sending the ipv6_flowinfo field we have to set the SEND_FLOWINFO option on the socket
		In java one value is set that is used for both the traffic class in the flowinfo field and the IP_TOS option.  Therefore  if the caller is setting traffic class
		then this indicates that we should also be setting the flowinfo field so we need to 
		set this option.  Howerver it can only be set on IPv6 sockets */
#if defined(IPv6_FUNCTION_SUPPORT)
#if defined(LINUX)
	if(( OS_IPPROTO_IP == platformLevel)&&(OS_IP_TOS==platformOption)&&(socketP->family == J9ADDR_FAMILY_AFINET6 )){
		uint32_t on = 1;
		uint32_t result = 0;
		int32_t err = 0;
		result = setsockopt(SOCKET_CAST(socketP), SOL_IPV6, IPV6_FLOWINFO_SEND, (void*)&on, sizeof(on));
		if (0 != result) {
			err = errno;
			Trc_PRT_sock_j9sock_setopt_failure_code("int", errno);
			err = omrerror_set_last_error(err, findError(err));
			Trc_PRT_sock_j9sock_setopt_int_Exit(err);
			return err;
		}
	}
#endif
#endif

	/* for some platforms if the lower bit of IP_TOS is set we get an error.  While this is correct because the
	   value is used for several purposes in java the bit may not be 0 and we don't want th error.  So just
	   mask the value so that we don't get the error */
	if(( OS_IPPROTO_IP == platformLevel)&&(OS_IP_TOS==platformOption)){
		optvalueUsed = optvalueUsed & 0xFE;
	}

	/* If this option is IP_TOS then don't call it on an IPv6 socket as it is not supported */
#if defined(IPv6_FUNCTION_SUPPORT)	
	if(!(( OS_IPPROTO_IP == platformLevel)&&(OS_IP_TOS==platformOption)&&(socketP->family == J9ADDR_FAMILY_AFINET6 ) ) ) 
	{
#endif
 		if (0 != setsockopt(SOCKET_CAST(socketP), platformLevel, platformOption, (void*) &optvalueUsed, optlen)) {
			int32_t err = errno;
			Trc_PRT_sock_j9sock_setopt_failure_code("int", errno);
			J9SOCKDEBUG( "<setsockopt (for int) failed, err=%d>\n", err);
			err = omrerror_set_last_error(err, findError(err));
			Trc_PRT_sock_j9sock_setopt_int_Exit(err);
			return err;
		}
#if defined(IPv6_FUNCTION_SUPPORT)		
	}
#endif
	Trc_PRT_sock_j9sock_setopt_int_Exit(0);
	return 0;
}

#ifdef IPv6_FUNCTION_SUPPORT
/**
 * @internal map an IPv4 address to an IPv4-mapped IPv6 address
 *
 * @param[in]	ipv4Addr An IPv4 address
 * @param[out]	ipv6Addr An IPv6 address
 */
static void
mapIPv4AddressToIPv6Address(struct J9PortLibrary *portLibrary, struct in_addr *ipv4Addr, struct in6_addr *ipv6Addr)
{
	/* Zero the IPv6 address */
	memset(ipv6Addr, 0, sizeof(*ipv6Addr));

	/* If the IPv4 address is INADDR_ANY, then the IPv6 address is already cleared to in6addr_any */
	if (INADDR_ANY != ipv4Addr->s_addr) {
		/* It isn't INADDR_ANY, so map the IPv4 address into the IPv6 address */
		memcpy(&(ipv6Addr->s6_addr[12]), ipv4Addr, sizeof(*ipv4Addr));

		/* Set the IPv4-Mapped Embedded IPv6 marker bits */
		ipv6Addr->s6_addr[10] = 0xFF;
		ipv6Addr->s6_addr[11] = 0xFF;
	}
}

/**
 * @internal map an IPv4 address representing an interface to an IPv6 interface index
 *
 * @param[in]	ipv4Addr An IPv4 address representing an interface
 * @param[out]	ipv6Interface An IPv6 interface index
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
static int32_t
mapIPv4InterfaceAddressToIPv6InterfaceIndex(struct J9PortLibrary *portLibrary, struct in_addr *ipv4Addr, uint32_t *ipv6Interface)
{
	struct j9NetworkInterfaceArray_struct array;
	int32_t rc = j9sock_get_network_interfaces(portLibrary, &array, FALSE);

	if (0 == rc) {
		uint32_t i, j;
		struct in6_addr ipv6Addr;

		/*
		 * Walk all the network interfaces looking for an interface with either the given ipv4Addr or its equivalent IPv4-mapped IPv6 address.
		 */
		mapIPv4AddressToIPv6Address(portLibrary, ipv4Addr, &ipv6Addr);
		rc = J9PORT_ERROR_SOCKET_OPTARGSINVALID;
		for (i = 0; (rc != 0) && (i < array.length); i++) {
			for (j = 0; j < array.elements[i].numberAddresses; j++) {
				if (((sizeof(*ipv4Addr) == array.elements[i].addresses[j].length) && (memcmp(ipv4Addr, &array.elements[i].addresses[j].addr, sizeof(*ipv4Addr)) == 0))
				|| ((sizeof(ipv6Addr) == array.elements[i].addresses[j].length) && (memcmp(&ipv6Addr, &array.elements[i].addresses[j].addr, sizeof(ipv6Addr)) == 0))) {
					*ipv6Interface = array.elements[i].index;
					rc = 0;
					break;
				}
			}
		}
		(void) j9sock_free_network_interface_struct(portLibrary, &array);
	}

	return rc;
}

/**
 * @internal map an IPv4 multicast request to an IPv6 multicast request
 *
 * @param[in]	ipmreq An IPv4 multicast request
 * @param[out]	ipmreq6 An IPv6 multicast request
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
static int32_t
convertIPMulticastRequestFromV4ToV6(struct J9PortLibrary *portLibrary, struct ip_mreq *ipmreq, struct ipv6_mreq *ipmreq6)
{
	mapIPv4AddressToIPv6Address(portLibrary, &ipmreq->imr_multiaddr, &ipmreq6->ipv6mr_multiaddr);

	/* If the interface is INADDR_ANY, then the interface index is 0 (use the system's default interface) */
	if (INADDR_ANY == ipmreq->imr_interface.s_addr) {
		ipmreq6->ipv6mr_interface = 0;
		return 0;
	}

	/* Otherwise map the IPv4 interface address to an IPv6 interface index */
	return mapIPv4InterfaceAddressToIPv6InterfaceIndex(portLibrary, &ipmreq->imr_interface, &ipmreq6->ipv6mr_interface);
}
#endif

/**
 * @internal set the multicast request on this socket
 * This handles both IPv4 and IPv6 multicast requests because the caller may be mistaken about the socket family.
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to set the option in.
 * @param[in] optlevel The level within the IP stack at which the option is defined.
 * @param[in] optname The name of the option to set.
 * @param[in] optval Pointer to the ipmreq/ipv6_mreq struct to update the socket option with.
 * @param[in] optlen The size of the optval struct.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
static int32_t
helperSetoptIpmreq(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname, void * optval, socklen_t optlen)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	void *platformOptval = optval;
	socklen_t platformOptlen = optlen;
#ifdef IPv6_FUNCTION_SUPPORT
	struct ipv6_mreq ipmreq6;
#endif
	MAP_LEVEL_OPTION_AND_FAMILY(optlevel, optname, socketP);

	Trc_PRT_sock_j9sock_setopt_ipmreq_Entry(socketP, optlevel, optname);

	CHECK_LEVEL_OPTION_AND_FAMILY(setopt_ipmreq);

	/* Adjust optval for socket family, if necessary */
	if (J9ADDR_FAMILY_AFINET4 == portableFamily) {
#ifdef IPv6_FUNCTION_SUPPORT
		/* Bail if the socket is an IPv4 socket, but the caller thinks it is an IPv6 socket - this is unsupported */
		if (IPPROTO_IPV6 == platformLevel) {
			Trc_PRT_sock_j9sock_setopt_ipmreq_invalid_socket_family_Exit();
			return J9PORT_ERROR_SOCKET_BADAF;
		}
	} else if (J9ADDR_FAMILY_AFINET6 == portableFamily) {
		/*
		 * If the socket is an IPv6 socket, but the caller thinks it is an IPv4 socket, map the multicast request from IPv4 to IPv6.
		 * This can happen with a dual-stack sockets implementation if the caller asked for a socket with AF_UNSPEC, then bound it to
		 * an IPv4 address and port.
		 */
		if (IPPROTO_IP == platformLevel) {
			int32_t rc;

#if defined(J9ZOS390)
			/* z/OS (and BSD-derived Unix variants) define different options - map them for convenience */
#define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
#define IPV6_DROP_MEMBERSHIP IPV6_LEAVE_GROUP
#endif

			platformLevel = IPPROTO_IPV6;
			if (IP_ADD_MEMBERSHIP == platformOption) {
				platformOption = IPV6_ADD_MEMBERSHIP;
			} else if (IP_DROP_MEMBERSHIP == platformOption) {
				platformOption = IPV6_DROP_MEMBERSHIP;
			} else {
				Trc_PRT_sock_j9sock_setopt_ipmreq_unsupported_option_Exit();
				return J9PORT_ERROR_SOCKET_OPTUNSUPP;
			}
			rc = convertIPMulticastRequestFromV4ToV6(portLibrary, (struct ip_mreq *) optval, &ipmreq6);
			if (0 != rc) {
				Trc_PRT_sock_j9sock_setopt_ipmreq_unable_to_convert_request_Exit(rc);
				return rc;
			}
			platformOptval = &ipmreq6;
			platformOptlen = sizeof(ipmreq6);
		}
#endif
	} else if (J9ADDR_FAMILY_UNSPEC == portableFamily) {
		Trc_PRT_sock_j9sock_setopt_ipmreq_unspecified_socket_family_Exit();
		return J9PORT_ERROR_SOCKET_BADAF;
	} else {
		Trc_PRT_sock_j9sock_setopt_ipmreq_unknown_socket_family_Exit(portableFamily);
		return J9PORT_ERROR_SOCKET_BADAF;
	}

	/* Call native OS function */
	if (0 != setsockopt(SOCKET_CAST(socketP), platformLevel, platformOption, platformOptval, platformOptlen)) {
		int32_t err = errno;
		J9SOCKDEBUG( "<setsockopt (for ipmreq) failed, err=%d>\n", err);
		Trc_PRT_sock_j9sock_setopt_ipmreq_setsockopt_failed_Exit(err);
#ifdef IPv6_FUNCTION_SUPPORT
		if ((J9ADDR_FAMILY_AFINET6 == portableFamily) && (J9PORT_ERROR_SOCKET_UNIX_EINTR == err)) {
			return J9PORT_ERROR_SOCKET_OPTARGSINVALID;
		}
#endif
		return omrerror_set_last_error(err, findError(err));
	}

 	Trc_PRT_sock_j9sock_setopt_ipmreq_Exit(0);
	return 0;
}

/**
 * Set the multicast request on this socket. 
 * Currently this is used to join or leave the nominated multicast group on the local interface.
 * 	It may be more generally useful, so a generic 'setop' function has been defined.
 * 
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to set the option in.
 * @param[in] optlevel The level within the IP stack at which the option is defined.
 * @param[in] optname The name of the option to set.
 * @param[in] optval Pointer to the ipmreq struct to update the socket option with.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_setopt_ipmreq(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  j9ipmreq_t optval)
{
	/* Note that tracing is done in the helper */
	return helperSetoptIpmreq(portLibrary, socketP, optlevel, optname, (void*)(&optval->addrpair), sizeof(optval->addrpair));
}

/**
 * Set the multicast request on this socket for IPv6 sockets. 
 * Currently this is used to join or leave the nominated multicast group on the local interface.
 * 	It may be more generally useful, so a generic 'setop' function has been defined.t.
 *
 * Supported families are OS_AF_INET and OS_AF_INET6 
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to set the option in.
 * @param[in] optlevel The level within the IP stack at which the option is defined.
 * @param[in] optname The name of the option to set.
 * @param[in] optval Pointer to the ipmreq struct to update the socket option with.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 *
 * @note Added for IPv6 support.
 */
int32_t
j9sock_setopt_ipv6_mreq(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  j9ipv6_mreq_t optval)
{
	int32_t rc = 0;
	Trc_PRT_sock_j9sock_setopt_ipv6_mreq_Entry(socketP, optlevel, optname);
#ifdef IPv6_FUNCTION_SUPPORT
	rc = helperSetoptIpmreq(portLibrary, socketP, optlevel, optname, (void*)(&optval->mreq), sizeof(optval->mreq));
#endif
	Trc_PRT_sock_j9sock_setopt_ipv6_mreq_Exit(rc);
	return rc;
}





/**
 * Set the linger value on the socket. 
 * See the @ref j9sock_linger_init for details of the linger behavior.
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to set the option in.
 * @param[in] optlevel The level within the IP stack at which the option is defined.
 * @param[in] optname The name of the option to set.
 * @param[out] optval Pointer to the linger struct to update the socket option with.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_setopt_linger(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  j9linger_t optval)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t platformLevel = platformSocketLevel(optlevel);
	int32_t platformOption = platformSocketOption(optname);
	socklen_t optlen;

	Trc_PRT_sock_j9sock_setopt_linger_Entry(socketP, optlevel, optname);
	
	optlen = sizeof(optval->linger);
	
	if (0 > platformLevel) {
		Trc_PRT_sock_j9sock_setopt_failure_cause("linger","0 > platformLevel");
		Trc_PRT_sock_j9sock_setopt_linger_Exit(platformLevel);
		return platformLevel;
	}
	if (0 > platformOption) {
		Trc_PRT_sock_j9sock_setopt_failure_cause("linger","0 > platformOption");
		Trc_PRT_sock_j9sock_setopt_linger_Exit(platformOption);
		return platformOption;
	}

 	if (0 != setsockopt(SOCKET_CAST(socketP), platformLevel, platformOption, (void*)(&optval->linger), optlen)) {
		int32_t err = errno;
		Trc_PRT_sock_j9sock_setopt_failure_code("linger", errno);
		J9SOCKDEBUG( "<setsockopt (for linger) failed, err=%d>\n", err);
		err = omrerror_set_last_error(err, findError(err));
		Trc_PRT_sock_j9sock_setopt_linger_Exit(err);
		return err;
	}
 	
 	Trc_PRT_sock_j9sock_setopt_linger_Exit(0);
	return 0;
}





/**
 * Set the sockaddr for the socket.
 * Currently used to set the interface of multicast sockets, but the more general call style is used,
 * in case it is more generally useful.
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to set the option in.
 * @param[in] optlevel The level within the IP stack at which the option is defined.
 * @param[in] optname The name of the option to set.
 * @param[in] optval Pointer to the j9sockaddr struct to update the socket option with.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_setopt_sockaddr(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname, j9sockaddr_t optval)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	MAP_LEVEL_OPTION_AND_FAMILY(optlevel, optname, socketP);
	OSSOCKADDR *sockaddr = (OSSOCKADDR*)&optval->addr;
	socklen_t optlen = sizeof(sockaddr->sin_addr);

	Trc_PRT_sock_j9sock_setopt_sockaddr_Entry(socketP, optlevel, optname);

	CHECK_LEVEL_OPTION_AND_FAMILY(setopt_sockaddr);

#if defined(AIXPPC) || defined(J9ZOS390)
	if ((IPPROTO_IP == platformLevel) && (IP_MULTICAST_IF == platformOption) && (J9ADDR_FAMILY_AFINET6 == portableFamily)){
		platformLevel = IPPROTO_IPV6;
		platformOption = IPV6_MULTICAST_IF;
	}
#endif

 	if (0 != setsockopt(SOCKET_CAST(socketP), platformLevel, platformOption, (void*)(&sockaddr->sin_addr), optlen)) {
		int32_t err = errno;
		Trc_PRT_sock_j9sock_setopt_failure_code("sockaddr", errno);
		J9SOCKDEBUG( "<setsockopt (for sockaddr) failed, err=%d>\n", err);
		err = omrerror_set_last_error(err, findError(err));
		Trc_PRT_sock_j9sock_setopt_sockaddr_Exit(err);
		return err;
	}
 	Trc_PRT_sock_j9sock_setopt_sockaddr_Exit(0);
	return 0;
}





/**
 * Terminates use of the socket library.  No sockets should be in use or the results
 * of this operation are unpredictable.  Frees any resources held by the socket library.
 *
 * @param[in] portLibrary The port library.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code
 */
int32_t
j9sock_shutdown(struct J9PortLibrary *portLibrary)
{
	j9sock_ptb_shutdown(portLibrary);
	return 0;
}





/**
 * The shutdown_input function disables the input stream on a socket. Any subsequent reads from the socket
 * will fail.
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock Socket to close input stream on.
 *
 * @return
 * \arg  0, on success
 * \arg J9PORT_ERROR_SOCKET_OPFAILED, on generic error
 * \arg J9PORT_ERROR_SOCKET_NOTINITIALIZED, if the library is not initialized
*/
int32_t
j9sock_shutdown_input(struct J9PortLibrary *portLibrary, j9socket_t sock)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	Trc_PRT_sock_j9sock_shutdown_input_Entry(sock);

 	if (shutdown(SOCKET_CAST(sock), 0) < 0) {
		rc = errno;
		if (ENOTCONN == errno) {
			/* Workaround for linux bug/nuance that occurs if the output end of the socket has already been terminated */
			Trc_PRT_sock_j9sock_shutdown_input_enotconn();
			rc = 0;
		} else {
			Trc_PRT_sock_j9sock_shutdown_input_failed(errno, strerror(errno));
			J9SOCKDEBUG( "<closesocket failed, err=%d>\n", rc);
			return omrerror_set_last_error(rc, findError(rc));
		}
	}

 	Trc_PRT_sock_j9sock_shutdown_input_Exit(rc);
	return rc;
}

/**
 * The shutdown_output function disables the output stream on a socket. Any subsequent writes to the socket
 * will fail.
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock Socket to close output stream on.
 *
 * @return
 * \arg  0, on success
 * \arg J9PORT_ERROR_SOCKET_OPFAILED, on generic error
 * \arg J9PORT_ERROR_SOCKET_NOTINITIALIZED, if the library is not initialized
 */
int32_t
j9sock_shutdown_output(struct J9PortLibrary *portLibrary, j9socket_t sock)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;

	Trc_PRT_sock_j9sock_shutdown_output_Entry(sock);
 	if (shutdown(SOCKET_CAST(sock), 1) < 0) {
 		Trc_PRT_j9sock_j9sock_shutdown_output_failed(errno, strerror(errno));
		rc = errno;
		J9SOCKDEBUG( "<closesocket failed, err=%d>\n", rc);
		return omrerror_set_last_error(rc, findError(rc));
	}

 	Trc_PRT_sock_j9sock_shutdown_output_Exit(rc);
	return rc;
}

int32_t
j9sock_sockaddr(struct J9PortLibrary *portLibrary, j9sockaddr_t handle, const char *addrStr, uint16_t portNetworkOrder)
{
	int32_t rc = 0;
	uint32_t addr = 0;
	j9hostent_struct host_t;

	if(0 != portLibrary->sock_inetaddr(portLibrary, addrStr, &addr)) {
		memset(&host_t, 0, sizeof (struct j9hostent_struct));
		if (0 != (rc = portLibrary->sock_gethostbyname(portLibrary, addrStr, &host_t))) {
			return rc;
		} else {
			addr = portLibrary->sock_hostent_addrlist(portLibrary, &host_t, 0);
		}
	} 
	rc = portLibrary->sock_sockaddr_init(portLibrary, handle, J9SOCK_AFINET, addr, portNetworkOrder);
	return rc;
}





/**
 * Answer the address, in network order, of the j9sockaddr argument.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Pointer to the j9sockaddr struct to access.
 *
 * @return	the address (there is no validation on the access).
 */
int32_t
j9sock_sockaddr_address(struct J9PortLibrary *portLibrary, j9sockaddr_t handle)
{
	return ((OSSOCKADDR*)&handle->addr)->sin_addr.s_addr;
}





/**
 * Answers the IP address of a structure and its length, in a preallocated buffer.
 *
 * Preallocated buffer "address" should be 16 bytes.  "length" tells you how many bytes were used 4 or 16.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle A populated j9sockaddr_t.
 * @param[out] address The IPv4 or IPv6 address in network byte order.
 * @param[out] length The number of bytes of the address (4 or 16).
 * @param[out] scope_id the scope id for the address if appropriate
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 *
 * @note Added for IPv6 support.
 */
int32_t
j9sock_sockaddr_address6(struct J9PortLibrary *portLibrary, j9sockaddr_t handle, uint8_t *address, uint32_t *length, uint32_t* scope_id)
{
	OSSOCKADDR *ipv4;
#ifdef IPv6_FUNCTION_SUPPORT
	OSSOCKADDR_IN6 *ipv6;
#endif
	
	ipv4 = (OSSOCKADDR *) &handle->addr;
	if( ipv4->sin_family == OS_AF_INET4 ) {
		memcpy( address, &ipv4->sin_addr, 4 );
		*length = 4;
	}
#ifdef IPv6_FUNCTION_SUPPORT
	else {
		ipv6 = (OSSOCKADDR_IN6 *) &handle->addr;
		memcpy( address, &ipv6->sin6_addr, 16 );
		*length = 16;
		*scope_id = ipv6->sin6_scope_id;
	} 
#endif

	return 0;
}





/**
 * Answers the family name of a j9sockaddr_struct.
 *
 * Supported families are OS_AF_INET and OS_AF_INET6 
 *
 * @param[in] portLibrary The port library.
 * @param[out] family The family name of the address.
 * @param[in] handle A populated j9sockaddr_t.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 *
 * @note Added for IPv6 support.
 */
int32_t
j9sock_sockaddr_family(struct J9PortLibrary *portLibrary, int16_t *family, j9sockaddr_t handle)
{
	OSSOCKADDR *ipv4;
	
	ipv4 = (OSSOCKADDR *) &handle->addr;
	if( ipv4->sin_family == OS_AF_INET4 ) {
		*family = J9ADDR_FAMILY_AFINET4;
	} else {
		*family = J9ADDR_FAMILY_AFINET6;
	} 

	return 0;
}

int32_t
j9sock_sockaddr_init(struct J9PortLibrary *portLibrary, j9sockaddr_t handle, int16_t family, uint32_t ipAddrNetworkOrder, uint16_t portNetworkOrder)
{
	OSSOCKADDR *sockaddr = (OSSOCKADDR*) &handle->addr;
	memset(handle, 0, sizeof (struct j9sockaddr_struct));
	sockaddr->sin_family = family;
	sockaddr->sin_addr.s_addr = ipAddrNetworkOrder ;
	sockaddr->sin_port = portNetworkOrder;

	return 0;
}

int32_t
j9sock_sockaddr_init6(struct J9PortLibrary *portLibrary, j9sockaddr_t handle, uint8_t *addr, int32_t addrlength, int16_t family, uint16_t portNetworkOrder,uint32_t flowinfo, uint32_t scope_id, j9socket_t sock)
{
    	OSSOCKADDR *sockaddr;
#ifdef IPv6_FUNCTION_SUPPORT
	OSSOCKADDR_IN6 *sockaddr_6;
#endif
	memset(handle, 0, sizeof (struct j9sockaddr_struct));

	if( family == J9ADDR_FAMILY_AFINET4 ) {
#ifdef IPv6_FUNCTION_SUPPORT
		if (j9sock_socketIsValid(portLibrary, sock)&&
					(getSocketFamily(portLibrary, sock) == J9ADDR_FAMILY_AFINET6 )) {

			/* to talk IPv4 on an IPv6 socket we need to map the IPv4 address to an IPv6 format.  If mapAddress is true then we do this */
			sockaddr_6 = (OSSOCKADDR_IN6*)&handle->addr;
			memset(sockaddr_6->sin6_addr.s6_addr, 0,16);
			memcpy( &(sockaddr_6->sin6_addr.s6_addr[12]), addr, addrlength) ;
			/* do a check if it is the any address.  we know the top 4 bytes of sockaddr_6->sin6_addr.s6_addr are 0's as we just cleared the,
                so we use them to do the check */
			if (memcmp(sockaddr_6->sin6_addr.s6_addr, addr, addrlength)!=0){
				/* if it is the any address then use the IPv6 any address */
				sockaddr_6->sin6_addr.s6_addr[10] = 0xFF;
				sockaddr_6->sin6_addr.s6_addr[11] = 0xFF;
			}
			sockaddr_6->sin6_port = portNetworkOrder;
			sockaddr_6->sin6_family = OS_AF_INET6;	
			sockaddr_6->sin6_scope_id = scope_id;
			sockaddr_6->sin6_flowinfo   = htonl(flowinfo);
		} else {
#endif
			/* just initialize the IPv4 address as is as it will be used with an IPv4 Socket */
			sockaddr = (OSSOCKADDR*)&handle->addr;
			memcpy( &sockaddr->sin_addr.s_addr, addr, addrlength) ;
			sockaddr->sin_port = portNetworkOrder;
			sockaddr->sin_family = OS_AF_INET4;
#ifdef IPv6_FUNCTION_SUPPORT
		}
#endif
	} 
#ifdef IPv6_FUNCTION_SUPPORT
	else if ( family == J9ADDR_FAMILY_AFINET6 ) {
		sockaddr_6 = (OSSOCKADDR_IN6*)&handle->addr;
		memcpy( &sockaddr_6->sin6_addr.s6_addr, addr, addrlength) ;
		sockaddr_6->sin6_port = portNetworkOrder;
		sockaddr_6->sin6_family = OS_AF_INET6;	
		sockaddr_6->sin6_scope_id = scope_id;
		sockaddr_6->sin6_flowinfo   = htonl(flowinfo);
#ifdef SIN6_LEN
		sockaddr_6->sin6_len = sizeof(OSSOCKADDR_IN6);
#endif
	}
#endif
	else {
		sockaddr = (OSSOCKADDR*)&handle->addr;
		memcpy( &sockaddr->sin_addr.s_addr, addr, J9SOCK_INADDR_LEN) ;
		sockaddr->sin_port = portNetworkOrder;
		sockaddr->sin_family = map_addr_family_J9_to_OS(family);
	}

	return 0;
}





/**
 * Answer the port, in network order, of the j9sockaddr argument.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Pointer to the j9sockaddr struct to access.
 *
 * @return	the port (there is no validation on the access).
 */
uint16_t
j9sock_sockaddr_port(struct J9PortLibrary *portLibrary, j9sockaddr_t handle)
{

	uint16_t rc = 0;

	Trc_PRT_sock_j9sock_sockaddr_port_Entry(handle);

	if( ((OSSOCKADDR*)&handle->addr)->sin_family == OS_AF_INET4 ) {
		rc = ((OSSOCKADDR*)&handle->addr)->sin_port;
	}
#ifdef IPv6_FUNCTION_SUPPORT
	else {
		rc = ((OSSOCKADDR_IN6*)&handle->addr)->sin6_port;
	}
#endif

	Trc_PRT_sock_j9sock_sockaddr_port_Exit(rc);

	return rc;

}





/**
 * Creates a new socket descriptor and any related resources.
 *
 * @param[in] portLibrary The port library.
 * @param[out]	handle Pointer pointer to the j9socket struct, to be allocated
 * @param[in] family The address family (currently, only J9SOCK_AFINET is supported)
 * @param[in] socktype Secifies what type of socket is created
 * \arg J9SOCK_STREAM, for a stream socket
 * \arg J9SOCK_DGRAM, for a datagram socket
 * @param[in] protocol Type/family specific creation parameter (currently, only J9SOCK_DEFPROTOCOL supported).
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_socket(struct J9PortLibrary *portLibrary, j9socket_t *handle, int32_t family, int32_t socktype,  int32_t protocol)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	int sock;

	Trc_PRT_sock_j9sock_socket_Entry(handle, family, socktype, protocol);
	
 	/* initialize return j9socket_t to invalid socket */
	*handle =INVALID_SOCKET ;
	
	if(protocol != J9SOCK_DEFPROTOCOL ) {
		Trc_PRT_sock_j9sock_socket_failure_cause("protocol != J9SOCK_DEFPROTOCOL");
		rc = J9PORT_ERROR_SOCKET_BADPROTO;
	} else if((socktype != J9SOCK_STREAM) && (socktype != J9SOCK_DGRAM) ) {
		Trc_PRT_sock_j9sock_socket_failure_cause("(socktype != J9SOCK_STREAM) && (socktype != J9SOCK_DGRAM)");
		rc = J9PORT_ERROR_SOCKET_BADTYPE;
	} else if(family != J9ADDR_FAMILY_AFINET6 && family != J9ADDR_FAMILY_AFINET4 && family != J9ADDR_FAMILY_UNSPEC) {
		Trc_PRT_sock_j9sock_socket_failure_cause("family != J9ADDR_FAMILY_AFINET6 && family != J9ADDR_FAMILY_AFINET4 && family != J9ADDR_FAMILY_UNSPEC");
		rc = J9PORT_ERROR_SOCKET_BADAF;
	}
	if(rc == 0) {

#ifdef IPv6_FUNCTION_SUPPORT
		if (family !=  J9ADDR_FAMILY_AFINET4)  { 
			family =  J9ADDR_FAMILY_AFINET6;
			sock= socket(AF_INET6, ((socktype == J9SOCK_STREAM) ? SOCK_STREAM : SOCK_DGRAM),  0);
			if (sock < 0) {
				rc = errno;
				Trc_PRT_sock_j9sock_socket_creation_failure(errno);
				J9SOCKDEBUG( "<socket failed, err=%d>\n", rc);
				rc = omrerror_set_last_error(rc, findError(rc));
				Trc_PRT_sock_j9sock_socket_Exit(rc);
				return rc;
			}
	   	} else {
#endif
			sock = socket(AF_INET, ((socktype == J9SOCK_STREAM) ? SOCK_STREAM : SOCK_DGRAM),  0);
			if (sock < 0) {
				rc = errno;
				Trc_PRT_sock_j9sock_socket_creation_failure(errno);
				J9SOCKDEBUG( "<socket failed, err=%d>\n", rc);
				rc = omrerror_set_last_error(rc, findError(rc));
				Trc_PRT_sock_j9sock_socket_Exit(rc);
				return rc;
				}
#ifdef IPv6_FUNCTION_SUPPORT
		}
#endif

	}
	if (rc == 0)	{
		/* PR 95345 - Tag this descriptor as being non-inheritable */
		int32_t fdflags = fcntl(sock, F_GETFD, 0);
		fcntl(sock, F_SETFD, fdflags | FD_CLOEXEC);

		/* set up the socket structure */
		*handle = omrmem_allocate_memory(sizeof( struct j9socket_struct ), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == *handle) {
			Trc_PRT_sock_j9sock_socket_failure_cause("*handle == NULL");
			close(sock);
			*handle = INVALID_SOCKET ;
			Trc_PRT_sock_j9sock_socket_Exit(J9PORT_ERROR_SOCKET_NOBUFFERS);
			return J9PORT_ERROR_SOCKET_NOBUFFERS; 
		}

		SOCKET_CAST(*handle) = sock;
		(*handle)->family = family;

		Trc_PRT_sock_j9sock_socket_created(*handle);
	}
	
	Trc_PRT_sock_j9sock_socket_Exit(rc);
	
	return rc;
}





/**
 * Determines whether or not the socket is valid.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Pointer to the j9socket struct, to be allocated.
 *
 * @return	0 if invalid, non-zero for valid.
 */
int32_t
j9sock_socketIsValid(struct J9PortLibrary *portLibrary, j9socket_t handle)
{
	if ((handle != NULL)&&(handle != INVALID_SOCKET)) {
		return TRUE;
	} else {
		return FALSE;
	}
}





/**
 * Initiate the use of sockets by a process.  This function must be called before any other socket calls.
 *
 * @param[in] portLibrary The port library.
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg J9PORT_ERROR_STARTUP_SOCK
 * \arg J9PORT_ERROR_SOCKET_OPFAILED
 * \arg J9PORT_ERROR_SOCKET_NOTINITIALIZED
 */
int32_t
j9sock_startup(struct J9PortLibrary *portLibrary)
{
	int32_t rc = 0;
	if (0 != j9sock_ptb_init(portLibrary)) {
		rc = J9PORT_ERROR_STARTUP_SOCK;
	}
	return rc;
}





/**
 * Create a time structure, representing the timeout period defined in seconds & microSeconds.
 * Timeval's are used as timeout arguments in the @ref j9sock_select function.
 *
 * @param[in] portLibrary The port library.
 * @param[in] secTime The integer component of the timeout value (in seconds).
 * @param[in] uSecTime The fractional component of the timeout value (in microseconds).
 * @param[out] timeP Pointer pointer to the j9timeval_struct to be allocated.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_timeval_init(struct J9PortLibrary *portLibrary, uint32_t secTime, uint32_t uSecTime, j9timeval_t timeP)
{
	memset(timeP, 0, sizeof (struct j9timeval_struct));
	timeP->time.tv_sec = secTime;
	timeP->time.tv_usec = uSecTime;

	return 0;
}





/**
 * The write function writes data to a connected socket.  The successful completion of a write 
 * does not indicate that the data was successfully delivered.  If no buffer space is available 
 * within the transport system to hold the data to be transmitted, send will block.
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock Pointer to the socket to send on
 * @param[in] buf The bytes to be sent
 * @param[in] nbyte The number of bytes to send
 * @param[in] flags The flags to modify the send behavior
 *
 * @return	If no error occur, return the total number of bytes sent, which can be less than the 
 * 'nbyte' for nonblocking sockets, otherwise the (negative) error code
 */
int32_t
j9sock_write(struct J9PortLibrary *portLibrary, j9socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t bytesSent = 0;

	Trc_PRT_sock_j9sock_write_Entry(sock, buf, nbyte, flags);
	
	bytesSent = send(SOCKET_CAST(sock), buf, nbyte, flags);

	if(-1 == bytesSent) {
		int32_t err = errno;
		Trc_PRT_sock_j9sock_write_failure(errno);
		J9SOCKDEBUG( "<send failed, err=%d>\n", err);
		err = omrerror_set_last_error(err, findError(err));
		Trc_PRT_sock_j9sock_write_Exit(err);
		return err;
	} else {
		Trc_PRT_sock_j9sock_write_Exit(bytesSent);
		return bytesSent;
	}
}





/**
 * The writeto function writes data to a datagram socket.  The successful completion of a writeto
 * does not indicate that the data was successfully delivered.  If no buffer space is available 
 * within the transport system to hold the data to be transmitted, writeto will block.
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock Pointer to the socket to send on
 * @param[in] buf The bytes to be sent
 * @param[in] nbyte The number of bytes to send
 * @param[in] flags The flags to modify the send behavior
 * @param [in] addrHandle The network address to send the datagram to
 *
 * @return	If no error occur, return the total number of bytes sent, otherwise the (negative) error code.
 */
int32_t
j9sock_writeto(struct J9PortLibrary *portLibrary, j9socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags, j9sockaddr_t addrHandle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t bytesSent = 0;

	socklen_t length = sizeof(addrHandle->addr);
#if defined(IPv6_FUNCTION_SUPPORT)
	length = getAddrLength(addrHandle);
#endif

	Trc_PRT_sock_j9sock_writeto_Entry(sock, buf, nbyte, flags, addrHandle);

	bytesSent = sendto(SOCKET_CAST(sock), buf , nbyte, flags, (struct sockaddr*) &(addrHandle->addr), length);

	if (-1 == bytesSent) {
		int32_t err = errno;
		Trc_PRT_sock_j9sock_writeto_failure(errno);
		J9SOCKDEBUG( "<sendto failed, err=%d>\n", err);
		err = omrerror_set_last_error(err, findError(err));
		Trc_PRT_sock_j9sock_writeto_Exit(err);
		return err;
	} else {
		Trc_PRT_sock_j9sock_writeto_Exit(bytesSent);
		return bytesSent;
	}
}





/**
 * Queries and returns the information for the network interfaces that are currently active within the system. 
 * Applications are responsible for freeing the memory returned via the handle.
 *
 * @param[in] portLibrary The port library.
 * @param[in,out] array Pointer to structure with array of network interface entries
 * @param[in] boolean which indicates if we should prefer the IPv4 stack or not
 *
 * @return	0 on success, negative portable error code on failure.
			J9PORT_ERROR_SOCKET_NORECOVERY if system calls required to get the info fail, J9PORT_ERROR_SOCKET_NOBUFFERS if memory allocation fails
 */
int32_t
j9sock_get_network_interfaces(struct J9PortLibrary *portLibrary, struct j9NetworkInterfaceArray_struct *array,BOOLEAN preferIPv4Stack)
{
	return J9PORT_ERROR_SOCKET_BADSOCKET;
}





/**
 * Frees the memory allocated for the j9NetworkInterface_struct array passed in
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Pointer to array of network interface structures to be freed
 *
 * @return 0 on success
*/
int32_t
j9sock_free_network_interface_struct(struct J9PortLibrary *portLibrary, struct j9NetworkInterfaceArray_struct* array)
{
	return J9PORT_ERROR_SOCKET_BADSOCKET;
}

/**
 * Establish a connection to a peer with a timeout.  This function is called repeatedly in order to carry out the connect and to allow
 * other tasks to proceed on certain platforms.  The caller must first call with step = J9_SOCK_STEP_START, if the result is J9_ERROR_SOCKET_NOTCONNECTED
 * it will then call it with step = CHECK until either another error or 0 is returned to indicate the connect is complete.  Each time the function should sleep for no more than
 * timeout milliseconds.  If the connect succeeds or an error occurs, the caller must always end the process by calling the function with step = J9_SOCK_STEP_DONE
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock pointer to the unconnected local socket.
 * @param[in] addr	pointer to the sockaddr, specifying remote host/port.
 * @param[in] timeout  timeout in milliseconds 
 * @param[in] step must be one of 3 states (J9_PORT_SOCKET_STEP_START, J9_PORT_SOCKET_STEP_CHECK, and J9_PORT_SOCKET_STEP_DONE)
 * @param[in,out] pointer to context pointer.  Filled in on first call and then to be passed into each subsequent call
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_connect_with_timeout(struct J9PortLibrary *portLibrary, j9socket_t sock, j9sockaddr_t addr, uint32_t timeout, uint32_t step, uint8_t** context)
{
	return J9PORT_ERROR_SOCKET_BADSOCKET;
}


/* Note: j9fdset_t does not support multiple entries. */
void
j9sock_fdset_zero(struct J9PortLibrary *portLibrary, j9fdset_t j9fdset) 
{
#if defined(LINUX)
	j9fdset->fd = -1;
#else
	FD_ZERO(&j9fdset->handle);
#endif
	return;
}

/* Note: j9fdset_t does not support multiple entries. */
void
j9sock_fdset_set(struct J9PortLibrary *portLibrary, j9socket_t aSocket, j9fdset_t j9fdset) 
{

#if defined(LINUX)
	/* check that the j9fdset is empty */
	Assert_PRT_true((-1 == j9fdset->fd) || (j9fdset->fd == SOCKET_CAST(aSocket)));

	j9fdset->fd = SOCKET_CAST(aSocket);
#else

	if (SOCKET_CAST(aSocket) > FD_SETSIZE) {
		return;
	} else {
		FD_SET(SOCKET_CAST(aSocket), &j9fdset->handle);
	}
#endif
	return;
}

/* Note: j9fdset_t does not support multiple entries. */
void
j9sock_fdset_clr(struct J9PortLibrary *portLibrary, j9socket_t aSocket, j9fdset_t j9fdset) 
{
#if defined(LINUX)
	j9fdset->fd = -1;
#else
	if (SOCKET_CAST(aSocket) > FD_SETSIZE) {
		return;
	} else {
		FD_CLR(SOCKET_CAST(aSocket), &j9fdset->handle);
	}
#endif
	return;
}

/* Note: j9fdset_t does not support multiple entries. */
BOOLEAN
j9sock_fdset_isset(struct J9PortLibrary *portLibrary, j9socket_t aSocket, j9fdset_t j9fdset) 
{
	BOOLEAN rc = FALSE;

#if defined(LINUX)
	rc = (j9fdset->fd == SOCKET_CAST(aSocket));
#else
	int fdIsSetRC;
	if (SOCKET_CAST(aSocket) > FD_SETSIZE) {
		return FALSE;
	} else {
		fdIsSetRC = FD_ISSET(SOCKET_CAST(aSocket), &j9fdset->handle);
	}
	
	if (fdIsSetRC!= 0) {
		rc = TRUE;
	}
#endif
	
	return rc;
}

/**
 * @internal	Disconnects stream and datagram sockets, performing platform specific initialization
 * 				of sockaddr to tell the platform that the subsequent connect call is a request to disconnect
 *
 * @param sock the socket to disconnect
 * @param fromlen the length of the j9sockaddr_t passed in
 *
 * @return	0 if success, a socket error if a failure occurred
 */
#ifdef LINUX
static int32_t
disconnectSocket(struct J9PortLibrary *portLibrary, j9socket_t sock, socklen_t fromlen)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	int32_t currentError = 0;
	int32_t uCharOptval = 0;
	socklen_t optlen = sizeof(uCharOptval);
	j9sockaddr_struct sockaddrP;
	BOOLEAN retrievedAddress = FALSE;
	uint8_t ipAddr[J9SOCK_INADDR6_LEN];
	memset (ipAddr, 0, J9SOCK_INADDR6_LEN);

	Trc_PRT_sock_disconnectSocket_Entry(sock, fromlen);

	/* Linux expects an address marked as unspecified as the cue to disconnect the socket */
	j9sock_sockaddr_init6(portLibrary, &sockaddrP, ipAddr, fromlen, J9ADDR_FAMILY_UNSPEC, 0, 0, 0, sock);

	/* determine if the socket is datagram */
	if(0 == getsockopt(SOCKET_CAST(sock), SOL_SOCKET, SO_TYPE, (void*)&uCharOptval, &optlen)) {
		if (uCharOptval == J9SOCKET_DGRAM){
			/* if socket is datagram, fetch address associated with socket (getsockname)
			 * for use later in bind call, if it fails, proceed with the disconnect, and do not bother to bind explicitly */
			if( j9sock_getsockname(portLibrary, sock, &sockaddrP) < 0) {
				currentError = errno;
				J9SOCKDEBUG( "<getsockname failed, err=%d>\n", currentError);
				Trc_PRT_sock_disconnectSocket_getsockname_failed(currentError);
			} else {
				retrievedAddress = TRUE;
			}
		}
	}

	rc = connectSocket(portLibrary, sock, &sockaddrP, fromlen);

	if (retrievedAddress) {
		/* now that we have disconnected the socket, we need to bind explicitly to the retained address
		 * since Linux zeros out address and port on disconnect */
		if (bind(SOCKET_CAST(sock), (struct sockaddr *)&sockaddrP.addr,  fromlen ) < 0) {
			currentError = errno;
			J9SOCKDEBUG( "<bind failed, err=%d>\n", currentError);
			rc = omrerror_set_last_error(currentError, findError(currentError));
			/* ignore EINVAL/SOCKLEVELINVALID error, as it indicates socket already bound */
			if (rc == J9PORT_ERROR_SOCKET_SOCKLEVELINVALID){
				Trc_PRT_sock_disconnectSocket_Event_socket_already_bound(currentError);
				rc = 0;
			} else {
				Trc_PRT_sock_disconnectSocket_Exit_bind_failed(currentError);
				return rc;
			}
		}
	}

	Trc_PRT_sock_disconnectSocket_Exit(rc);
	return rc;
}
#elif defined(AIXPPC) /* disconnect for AIX */
static int32_t
disconnectSocket(struct J9PortLibrary *portLibrary, j9socket_t sock, socklen_t fromlen)
{
	int32_t rc = 0;
	j9sockaddr_struct sockaddrP;

  	Trc_PRT_sock_disconnectSocket_Entry(sock, fromlen);

	/* must use AF_INET with an INADDR_ANY to disconnect a socket on AIX, and can not use port 0 */
	j9sock_sockaddr_init6(portLibrary, &sockaddrP, INADDR_ANY, fromlen, J9ADDR_FAMILY_AFINET4, 1, 0, 0, sock);

	rc = connectSocket(portLibrary, sock, &sockaddrP, fromlen);

	Trc_PRT_sock_disconnectSocket_Exit(rc);
	return rc;
}
#else
static int32_t
disconnectSocket(struct J9PortLibrary *portLibrary, j9socket_t sock, socklen_t fromlen)
{
	int32_t rc = 0;
	j9sockaddr_struct sockaddrP;
  	uint8_t ipAddr[J9SOCK_INADDR6_LEN];
  	memset (ipAddr, 0, J9SOCK_INADDR6_LEN);

  	Trc_PRT_sock_disconnectSocket_Entry(sock, fromlen);

  	/* Most platforms expect an address marked as unspecified as the cue to disconnect the socket */
	j9sock_sockaddr_init6(portLibrary, &sockaddrP, ipAddr, fromlen, J9ADDR_FAMILY_UNSPEC, 0, 0, 0, sock);

	rc = connectSocket(portLibrary, sock, &sockaddrP, fromlen);

	Trc_PRT_sock_disconnectSocket_Exit(rc);
	return rc;
}
#endif

/**
 * @internal	Connects both stream and datagram sockets
 *
 * @param sock the socket to connect
 * @param addr the address to associate to the socket
 * @param fromlen the length of the j9sockaddr_t passed in
 *
 * @return	0 if success, a socket error if a failure occurred
 */
static int32_t
connectSocket(struct J9PortLibrary *portLibrary, j9socket_t sock, j9sockaddr_t addr, socklen_t fromlen)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	int32_t currentError = 0;
	Trc_PRT_sock_connectSocket_Entry(sock, addr, fromlen);
	if (connect(SOCKET_CAST(sock), (struct sockaddr *) &addr->addr, fromlen ) < 0) {
		currentError = errno;
		Trc_PRT_sock_connectSocket_failure(currentError);
		J9SOCKDEBUG( "<connect failed, err=%d>\n", currentError);
		rc = omrerror_set_last_error(currentError, findError(currentError));
	}
	Trc_PRT_sock_connectSocket_Exit(rc);
	return rc;
}

static int32_t
lookupIPv6AddressFromIndex(struct J9PortLibrary *portLibrary, uint8_t index, j9sockaddr_t handle)
{
	j9NetworkInterfaceArray_struct netifArray;
	int32_t rc = j9sock_get_network_interfaces(portLibrary, &netifArray, FALSE);

	if (rc == 0) {
		uint32_t i, j;
#if defined(IPv6_FUNCTION_SUPPORT)
		struct in6_addr *ipv6;
#endif /* defined(IPv6_FUNCTION_SUPPORT) */

		/* no address found, index parameter was invalid */
		rc = J9PORT_ERROR_SOCKET_OPTARGSINVALID;

#if defined(IPv6_FUNCTION_SUPPORT)
		/* loop through the interfaces to look for matching indices */
		for (i = 0; (i < netifArray.length) && (rc != 0); i++) {
			if (netifArray.elements[i].index == index) {
				/* loop through the addresses, return the first IPv6 address */
				for (j = 0; j < netifArray.elements[i].numberAddresses; j++){
					if (netifArray.elements[i].addresses[j].length == sizeof(struct in6_addr)){
						ipv6 = (struct in6_addr *) &handle->addr;
						memcpy(ipv6, &netifArray.elements[i].addresses[j].addr.in6Addr, sizeof(struct in6_addr));
						rc = 0;
						break;
					}
				}
			}
		}
#endif /* defined(IPv6_FUNCTION_SUPPORT) */

		(void) j9sock_free_network_interface_struct(portLibrary, &netifArray);
	}

	return rc;
}
