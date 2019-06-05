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
 * @brief Per-thread buffer for platform-dependent socket information.
 *
 * Per thread buffers are used to store information that is not shareable among the threads.  
 * For example when an OS system call fails the error code associated with that error is
 * relevant to the thread that called the OS function; it has no meaning to any other thread.
 *
 * This file contains the functions supported by the port library for creating, accessing and
 * destroying per thread buffers. @see j9sock.h for details on the per thread buffer structure.
 *
 */
#include <string.h>
#include "j9port.h"
#include "j9sock.h"
#include "omrport.h"
#include "portpriv.h"
#include "thread_api.h"

static void J9THREAD_PROC j9sock_ptb_free(void *socketPTBVoidP);

/**
 * @internal
 * @brief j9sock Per Thread Buffer Support
 *
 * Get a per thread buffer.
 *
 * @param[in] portLibrary The port library.
 *
 * @return The per thread buffer on success, NULL on failure.
 */
J9SocketPTB *
j9sock_ptb_get(J9PortLibrary *portLibrary)
{
	omrthread_t self = omrthread_self();
	J9SocketPTB *ptBuffers = omrthread_tls_get(self, portLibrary->portGlobals->socketTlsKey);

	if (NULL == ptBuffers) {
		OMRPORT_ACCESS_FROM_J9PORT(portLibrary);

		ptBuffers = omrmem_allocate_memory(sizeof(J9SocketPTB), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL != ptBuffers) {
			if (0 == omrthread_tls_set(self, portLibrary->portGlobals->socketTlsKey, ptBuffers)) {
				memset(ptBuffers, 0, sizeof(J9SocketPTB));
				ptBuffers->portLibrary = portLibrary;
			} else {
				omrmem_free_memory(ptBuffers);
				ptBuffers = NULL;
			}
		}
	}
	return ptBuffers;
}


/**
 * @internal
 * @brief Free the current thread's J9SocketPTB.
 * @param[in] socketPTBVoidP The current thread's J9SocketPTB
 */
static void J9THREAD_PROC
j9sock_ptb_free(void *socketPTBVoidP)
{
	J9SocketPTB *socketPTB = (J9SocketPTB *)socketPTBVoidP;
	OMRPORT_ACCESS_FROM_J9PORT(socketPTB->portLibrary);

	/* Free the socketPTB */
	if (NULL != socketPTB->fdset) {
		omrmem_free_memory(socketPTB->fdset);
	}
#if defined(WIN32)
    if (NULL != socketPTB->addr_info_hints.addr_info) {
    	omrmem_free_memory(socketPTB->addr_info_hints.addr_info);
	}

#else /* defined(WIN32) */
#if defined(IPv6_FUNCTION_SUPPORT)
    if (NULL != socketPTB->addr_info_hints.addr_info) {
    	omrmem_free_memory(socketPTB->addr_info_hints.addr_info);
	}
#endif
#if HOSTENT_DATA_R
	if (NULL != socketPTB->hostent_data) {
		omrmem_free_memory(socketPTB->hostent_data);
	}
#elif GLIBC_R || OTHER_R
	if (NULL != socketPTB->gethostBuffer) {
		omrmem_free_memory(socketPTB->gethostBuffer);
	}
#endif
#endif /* defined(WIN32) */
	omrmem_free_memory(socketPTB);
}

/**
 * @internal
 * @brief Initialize j9sock_ptb.
 * 
 * This function is called during startup of j9sock. All resources created here should be destroyed
 * in @ref j9sock_ptb_shutdown.
 *
 * @param[in] portLibrary The port library
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg J9PORT_ERROR_STARTUP_TLS
 * \arg J9PORT_ERROR_STARTUP_TLS_ALLOC
 * \arg J9PORT_ERROR_STARTUP_TLS_MUTEX
 */
int32_t
j9sock_ptb_init(J9PortLibrary *portLibrary)
{
	if (omrthread_tls_alloc_with_finalizer(&portLibrary->portGlobals->socketTlsKey, j9sock_ptb_free)) {
		return J9PORT_ERROR_STARTUP_TLS_ALLOC;
	}
	return 0;
}

/**
 * @internal
 * @brief PortLibrary shutdown.
 *
 * This function is called during shutdown of j9sock.  Any resources that were created by 
 * @ref j9sock_ptb_init should be destroyed here.
 *
 * @param[in] OMRPortLibrary The port library
 */
void
j9sock_ptb_shutdown(J9PortLibrary *portLibrary)
{
	if (NULL != portLibrary->portGlobals) {
		/* Release the TLS key */
		omrthread_tls_free(portLibrary->portGlobals->socketTlsKey);
	}
}

