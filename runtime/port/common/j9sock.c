/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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
