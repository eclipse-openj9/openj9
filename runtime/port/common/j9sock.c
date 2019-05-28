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

/**
 * @file
 * @ingroup Port
 * @brief Sockets
 */
#include "j9sock.h"
#include "portpriv.h"

int32_t platformSocketLevel( int32_t portableSocketLevel );
int32_t platformSocketOption( int32_t portableSocketOption );


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
int32_t
platformSocketLevel( int32_t portableSocketLevel )
{
	switch (portableSocketLevel) {
		case J9_SOL_SOCKET: 
			return OS_SOL_SOCKET;
		case J9_IPPROTO_TCP: 
			return OS_IPPROTO_TCP;
		case J9_IPPROTO_IP: 
			return OS_IPPROTO_IP;
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
/*[PR1FLSKTU] Support datagram broadcasts */
int32_t
platformSocketOption( int32_t portableSocketOption )
{
	switch (portableSocketOption) {
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
		case J9_SO_OOBINLINE:
			return OS_SO_OOBINLINE;
		case J9_IP_MULTICAST_LOOP:
			return OS_MCAST_LOOP;
		case J9_IP_TOS:
			return OS_IP_TOS;
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
 * socket once accept returns successfully
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
	return J9PORT_ERROR_SOCKET_BADSOCKET;
}
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
	return J9PORT_ERROR_SOCKET_BADSOCKET;
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
	return J9PORT_ERROR_SOCKET_BADSOCKET;
}
/**
 * Establish a connection to a peer.
 *
 * For non-blocking sockets: If a connection cannot be immediately established 
 * this function will return J9PORT_ERROR_SOCK_EINPROGRESS and the connection 
 * attempt will continue to take place asynchronously. Subsequent calls to this 
 * function while a connection attempt is taking place will return
 * J9PORT_ERROR_SOCKET_ALREADYINPROGRESS. Once the asynchronous connection attempt 
 * completes this function will return with J9PORT_ERROR_SOCKET_ISCONNECTED or 
 * J9PORT_ERROR_SOCKET_ALREADYBOUND. It is not recommended that users call this
 * function multiple times to determine when the connection attempt has succeeded.
 * Instead, the j9sock_select() function can be used to determine when the socket 
 * is ready for reading or writing.
 * 
 * @param[in] portLibrary The port library.
 * @param[in] sock pointer to the unconnected local socket.
 * @param[in] addr	pointer to the sockaddr, specifying remote host/port.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_connect(struct J9PortLibrary *portLibrary, j9socket_t sock, j9sockaddr_t addr)
{
	return J9PORT_ERROR_SOCKET_BADSOCKET;
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
const char*
j9sock_error_message(struct J9PortLibrary *portLibrary)
{
	return "Operation Failed";
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
	memset(ptBuffers->fdset, 0, sizeof(struct j9fdset_struct));

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
	return 0;
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
	return 0;
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
	return 0;
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
	return 0;
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
j9sock_getaddrinfo_family(struct J9PortLibrary *portLibrary, j9addrinfo_t handle, int32_t *family, int index )
{
	return 0;
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
	return 0;
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
	return 0;
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
j9sock_gethostname(struct J9PortLibrary *portLibrary, char *buffer, int32_t length )
{
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
 * @param[in] flags Flags on how to form the response (see man pages or doc for getnameinfo)
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code
 *
 * @note Added for IPv6 support.
 * @note "flags" do not affect results on OS's that do not support the IPv6 calls.
 */
int32_t
j9sock_getnameinfo(struct J9PortLibrary *portLibrary, j9sockaddr_t in_addr, int32_t sockaddr_size, char *name, int32_t name_length, int flags)
{
	return 0;
}
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
	int32_t rc = 0;
	int32_t platformLevel = platformSocketLevel( optlevel );
	int32_t platformOption = platformSocketOption( optname );

	if (0 > platformLevel) {
		return platformLevel;
	}
	if (0 > platformOption) {
		return platformOption;
	}
	return J9PORT_ERROR_SOCKET_BADSOCKET;
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
	int32_t rc = 0;
	int32_t platformLevel = platformSocketLevel( optlevel );
	int32_t platformOption = platformSocketOption( optname );

	if (0 > platformLevel) {
		return platformLevel;
	}
	if (0 > platformOption) {
		return platformOption;
	}
	return J9PORT_ERROR_SOCKET_BADSOCKET;
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
	int32_t rc = 0;
	int32_t platformLevel = platformSocketLevel( optlevel );
	int32_t platformOption = platformSocketOption( optname );

	if (0 > platformLevel) {
		return platformLevel;
	}
	if (0 > platformOption) {
		return platformOption;
	}
	return J9PORT_ERROR_SOCKET_BADSOCKET;
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
	int32_t rc = 0;
	int32_t platformLevel = platformSocketLevel( optlevel );
	int32_t platformOption = platformSocketOption( optname );

	if (0 > platformLevel) {
		return platformLevel;
	}
	if (0 > platformOption) {
		return platformOption;
	}
	return J9PORT_ERROR_SOCKET_BADSOCKET;
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
	int32_t rc = 0;
	int32_t platformLevel = platformSocketLevel( optlevel );
	int32_t platformOption = platformSocketOption( optname );

	if (0 > platformLevel) {
		return platformLevel;
	}
	if (0 > platformOption) {
		return platformOption;
	}
	return J9PORT_ERROR_SOCKET_BADSOCKET;
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
	return J9PORT_ERROR_SOCKET_BADSOCKET;
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
	return J9PORT_ERROR_SOCKET_BADSOCKET;
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
	return 0;
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
	return val;
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
	return val;
}
/**
 * Answer the dotted IP string as an Internet address.
 *
 * @param[in] portLibrary The port library.
 * @param[out] addrStr The dotted IP string.
 * @param[in] addr Pointer to the Internet address.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_inetaddr(struct J9PortLibrary *portLibrary, const char *addrStr, uint32_t *addr)
{
	return 0;
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
 * @param[in] ipv6mr_interface The ip mulitcast interface.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 *
 * @note Added for IPv6 support.
 */
int32_t
j9sock_ipv6_mreq_init(struct J9PortLibrary *portLibrary, j9ipv6_mreq_t handle, uint8_t *ipmcast_addr, uint32_t ipv6mr_interface)
{
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
	handle->linger.l_linger = timeout;
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
	return J9PORT_ERROR_SOCKET_BADSOCKET;
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
	return val;
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
	return val;
}
/**
 * Receives data from a connected socket.  
 * 
 * This will return as much information as is currently available up to the size of the buffer supplied. 
 * 
 * Behaviour depending on the blocking characteristic of the socket, as specified by  @ref j9sock_set_nonblocking().
 *
 *	Blocking socket:
 * 		- If no incoming data is available, the call blocks and waits for data to arrive. 
 * 
 * 	Non-blocking socket:
 * 		- If no incoming data is immediately available, the negative portable error code J9PORT_ERROR_SOCKET_WOULDBLOCK is returned.
 * 		- J9PORT_ERROR_SOCKET_WOULDBLOCK is non-fatal and it is normal practice for the caller to retry @ref j9sock_read() upon receiving it.
 * 
 * @param[in]	portLibrary The port library.
 * @param[in] 	sock Pointer to the socket to read on
 * @param[out] 	buf Pointer to the buffer where input bytes are written
 * @param[in] 	nbyte The length of buf
 * @param[in] 	flags The flags, to influence this read (in addition to the socket options)
 *
 * @return
 * \arg If no error occurs, return the number of bytes received.
 * \arg If the connection has been gracefully closed, return 0.
 * \arg Otherwise return the (negative) error code.
 */
int32_t
j9sock_read(struct J9PortLibrary *portLibrary, j9socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags)
{
	return J9PORT_ERROR_SOCKET_BADSOCKET;
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
	return J9PORT_ERROR_SOCKET_BADSOCKET;
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
	return J9PORT_ERROR_SOCKET_BADSOCKET;
}
/**
 * A helper method, to ensure a read operation can be performed without blocking.
 * The portable version of a read operation is a blocking call (will wait indefinitely for data).
 * This function should be called prior to a read operation, to provide a read timeout.
 * If the result is 1, the caller is guaranteed to be able to complete a read on the socket without blocking.
 * The actual contents of the fdset are not available for inspection (as provided in the more general 'select' function).
 * The timeout is specified in seconds and microseconds.
 *
 * @param[in] portLibrary The port library.
 * @param[in] j9socketP Pointer to the j9socket to query for available read data.
 * @param[in] secTime The integer component of the timeout period, in seconds.
 * @param[in] uSecTime The fractional component of the timeout period, in microSeconds.
 * @param[in] isNonBlocking Only used when timeout is zero.  If TRUE, then method 
 *            will not block, if FALSE then may block if waiting for data
 *
 * @return
 * \arg 1, 	If a read operation can be performed on the socket without blocking
 * 			Note that this does not necessarily mean there is data available to be read. 
 * 			For instance, a read operation on a closed socket will not block therefore 
 * 			j9sock_select_read() will return 1 even though the read will fail.
 * \arg J9PORT_ERROR_SOCKET_TIMEOUT if the call timed out
 * \arg otherwise return the (negative) error code.
 */
int32_t
j9sock_select_read(struct J9PortLibrary *portLibrary, j9socket_t j9socketP, int32_t secTime, int32_t uSecTime, BOOLEAN isNonBlocking)
{
	return J9PORT_ERROR_SOCKET_BADSOCKET;
}
/**
 * Set the nonblocking state of the socket.
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to read on
 * @param[in] nonblocking Set true for nonblocking, false for blocking
 *
 * @return	0 if no error occurs, otherwise return the (negative) error code.
 */
int32_t
j9sock_set_nonblocking(struct J9PortLibrary *portLibrary, j9socket_t socketP, BOOLEAN nonblocking)
{
	return J9PORT_ERROR_SOCKET_BADSOCKET;
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
	return 0;
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
 * @param[out] optval Pointer to the boolean to update the socket option with.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_setopt_bool(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  BOOLEAN *optval)
{
	int32_t rc = 0;
	int32_t platformLevel = platformSocketLevel( optlevel );
	int32_t platformOption = platformSocketOption( optname );

	if (0 > platformLevel) {
		return platformLevel;
	}
	if (0 > platformOption) {
		return platformOption;
	}
	return J9PORT_ERROR_SOCKET_BADSOCKET;
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
 * @param[out] optval Pointer to the byte to update the socket option with.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_setopt_byte(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  uint8_t *optval)
{
	int32_t rc = 0;
	int32_t platformLevel = platformSocketLevel( optlevel );
	int32_t platformOption = platformSocketOption( optname );

	if (0 > platformLevel) {
		return platformLevel;
	}
	if (0 > platformOption) {
		return platformOption;
	}
	return J9PORT_ERROR_SOCKET_BADSOCKET;
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
 * @param[out] optval Pointer to the integer to update the socket option with.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_setopt_int(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  int32_t *optval)
{
	int32_t rc = 0;
	int32_t platformLevel = platformSocketLevel( optlevel );
	int32_t platformOption = platformSocketOption( optname );

	if (0 > platformLevel) {
		return platformLevel;
	}
	if (0 > platformOption) {
		return platformOption;
	}
	return J9PORT_ERROR_SOCKET_BADSOCKET;
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
 * @param[out] optval Pointer to the ipmreq struct to update the socket option with.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_setopt_ipmreq(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  j9ipmreq_t optval)
{
	int32_t rc = 0;
	int32_t platformLevel = platformSocketLevel( optlevel );
	int32_t platformOption = platformSocketOption( optname );

	if (0 > platformLevel) {
		return platformLevel;
	}
	if (0 > platformOption) {
		return platformOption;
	}
	return J9PORT_ERROR_SOCKET_BADSOCKET;
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
 * @param[out] optval Pointer to the ipmreq struct to update the socket option with.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 *
 * @note Added for IPv6 support.
 */
int32_t
j9sock_setopt_ipv6_mreq(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  j9ipv6_mreq_t optval)
{
	return J9PORT_ERROR_SOCKET_BADSOCKET;
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
	int32_t rc = 0;
	int32_t platformLevel = platformSocketLevel( optlevel );
	int32_t platformOption = platformSocketOption( optname );

	if (0 > platformLevel) {
		return platformLevel;
	}
	if (0 > platformOption) {
		return platformOption;
	}
	return J9PORT_ERROR_SOCKET_BADSOCKET;
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
 * @param[out] optval Pointer to the j9sockaddr struct to update the socket option with.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_setopt_sockaddr(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  j9sockaddr_t optval)
{
	int32_t rc = 0;
	int32_t platformLevel = platformSocketLevel( optlevel );
	int32_t platformOption = platformSocketOption( optname );

	if (0 > platformLevel) {
		return platformLevel;
	}
	if (0 > platformOption) {
		return platformOption;
	}
	return J9PORT_ERROR_SOCKET_BADSOCKET;
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
j9sock_shutdown(struct J9PortLibrary *portLibrary )
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
	return J9PORT_ERROR_SOCKET_OPFAILED;
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
	return J9PORT_ERROR_SOCKET_OPFAILED;
}
/**
 * Creates a new j9sockaddr, referring to the specified port and address.  The only address family currently supported
 * is AF_INET.
 *
 * @param[in] portLibrary The port library.
 * @param[out] handle Pointer to the j9sockaddr struct, to be allocated.
 * @param[in] addrStr The target host, as either a name or dotted ip string.
 * @param[in] portNetworkOrder The target port, in network byte order. Use @ref j9sock_htons() to convert from to host to network order.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
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
	return handle->addr.sin_addr.s_addr;
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
	*family = handle->addr.sin_family;
	return 0;
}

/**
 * Creates a new j9sockaddr, referring to the specified port and address.  The only address family currently supported
 * is AF_INET.
 *
 * @param[in] portLibrary The port library.
 * @param[out] handle Pointer pointer to the j9sockaddr struct, to be allocated.
 * @param[in] family The address family.
 * @param[in] ipAddrNetworkOrder The target host address, in network order. Use @ref j9sock_htonl() to convert the ip address from host to network byte order.
 * @param[in] portNetworkOrder The target port, in network byte order. Use @ref j9sock_htons() to convert the port from host to network byte order.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_sockaddr_init(struct J9PortLibrary *portLibrary, j9sockaddr_t handle, int16_t family, uint32_t ipAddrNetworkOrder, uint16_t portNetworkOrder)
{
	memset(handle, 0, sizeof (struct j9sockaddr_struct));
	handle->addr.sin_family = family;
	handle->addr.sin_addr.s_addr = ipAddrNetworkOrder;
	handle->addr.sin_port = portNetworkOrder;

	return 0;
}

/**
 * Answers an initialized j9sockaddr_struct structure.
 *
 * Pass in a j9sockaddr_struct with some initial parameters to initialize it appropriately.
 * Currently the only address families supported are OS_AF_INET6 and OS_AF_INET, which
 * will be determined by addrlength.  (4 bytes for IPv4 addresses and 16 bytes for IPv6 addresses).
 *
 * @param[in] portLibrary The port library.
 * @param[out] handle Pointer pointer to the j9sockaddr struct, to be allocated.
 * @param[in] addr The IPv4 or IPv6 address in network byte order.
 * @param[in] addrlength The number of bytes of the address (4 or 16).
 * @param[in] family The address family.
 * @param[in] portNetworkOrder The target port, in network order. Use @ref j9sock_htons() to convert the port from host to network byte order.
 * @param[in] flowinfo The flowinfo value for IPv6 addresses in HOST order.  Set to 0 for IPv4 addresses or if no flowinfo needs to be set for IPv6 address
 * @param[in] scope_id The scope id for an IPv6 address in HOST order.  Set to 0 for IPv4 addresses and for non-scoped IPv6 addresses
 * @param[in] sock The socket that this address will be used with.  
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 *
 * @note Added for IPv6 support.
 */
int32_t
j9sock_sockaddr_init6(struct J9PortLibrary *portLibrary, j9sockaddr_t handle, uint8_t *addr, int32_t addrlength, int16_t family, uint16_t portNetworkOrder,uint32_t flowinfo, uint32_t scope_id, j9socket_t sock)
{
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
	return handle->addr.sin_port;
}
/**
 * Creates a new socket descriptor and any related resources.
 *
 * @param[in] portLibrary The port library.
 * @param[out]	handle Pointer pointer to the j9socket struct, to be allocated
 * @param[in] family The address family (currently, only J9SOCK_AFINET is supported)
 * @param[in] socktype Specifies what type of socket is created
 * \arg J9SOCK_STREAM, for a stream socket
 * \arg J9SOCK_DGRAM, for a datagram socket
 * @param[in] protocol Type/family specific creation parameter (currently, only J9SOCK_DEFPROTOCOL supported).
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_socket(struct J9PortLibrary *portLibrary, j9socket_t *handle, int32_t family, int32_t socktype,  int32_t protocol)
{
	return J9PORT_ERROR_SOCKET_OPFAILED;
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
	return 0;
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
	return J9PORT_ERROR_SOCKET_BADSOCKET;
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
	return J9PORT_ERROR_SOCKET_BADSOCKET;
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
	array->elements =NULL;
	array->length = 0;
	return 0;
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
	return 0;
}
/**
 * Establish a connection to a peer with a timeout.  This function is called repeatedly in order to carry out the connect and to allow
 * other tasks to proceed on certain platforms.  The caller must first call with step = J9_SOCK_STEP_START, if the result is J9_ERROR_SOCKET_NOTCONNECTED
 * it will then call it with step = CHECK until either another error or 0 is returned to indicate the connect is complete.  Each time the function should sleep for no more than
 * timeout milliseconds.  If the connect succeeds or an error occurs, the caller must always end the process by calling the function with step = J9_SOCK_STEP_DONE
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock pointer to the unconnected local socket.
 * @param[in] addr pointer to the sockaddr, specifying remote host/port.
 * @param[in] timeout  timeout in milliseconds 
 * @param[in,out] pointer to context pointer.  Filled in on first call and then to be passed into each subsequent call
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_connect_with_timeout(struct J9PortLibrary *portLibrary, j9socket_t sock, j9sockaddr_t addr, uint32_t timeout, uint32_t step, uint8_t** context)
{
	return J9PORT_ERROR_SOCKET_BADSOCKET;
}

/**
 * Zero a j9 file descriptor set
 *  
 * Note: j9fdset_t does not support multiple entries.
 *
 * @param[in] portLibrary The port library.
 * @param[in] j9fdset a j9 file descriptor set.
 */
void
j9sock_fdset_zero(struct J9PortLibrary *portLibrary, j9fdset_t j9fdset) 
{
	return;
}

/**
 * Add a j9 socket to a j9 file descriptor set.
 *  
 * Note: j9fdset_t does not support multiple entries.
 *
 * @param[in] portLibrary The port library.
 * @param[in] aSocket j9 socket to be added to j9fdset
 * @param[in] j9fdset a j9 file descriptor set.
 */
void
j9sock_fdset_set(struct J9PortLibrary *portLibrary, j9socket_t aSocket, j9fdset_t j9fdset) 
{
	return;
}

/**
 * Remove a j9 socket from a j9 file descriptor set
 *
 * Note: j9fdset_t does not support multiple entries.
 *  
 * @param[in] portLibrary The port library.
 * @param[in] aSocket j9 socket to be removed from j9fdset
 * @param[in] j9fdset a j9 file descriptor set.
 */
void
j9sock_fdset_clr(struct J9PortLibrary *portLibrary, j9socket_t aSocket, j9fdset_t j9fdset) 
{
	return;
}

/**
 * Returns TRUE if a j9 socket is part of the j9 file descriptor set.
 * This is designed to be used after a call to @ref j9socket_select to determine if the j9 socket is ready for a read/write.  
 *  
 * Note: j9fdset_t does not support multiple entries.
 *
 * @param[in] portLibrary the port library.
 * @param[in] aSocket j9 socket to test for in j9fdset
 * @param[in] j9fdset a j9 file descriptor set.
 * 
 * @return TRUE if fd is part of j9fdset.
 */
BOOLEAN
j9sock_fdset_isset(struct J9PortLibrary *portLibrary, j9socket_t aSocket, j9fdset_t j9fdset) 
{
	return FALSE;
}

