/*******************************************************************************
 * Copyright (c) 2015, 2016 IBM Corp. and others
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

#include "j9port.h"
#include "portpriv.h"
#include <sys/mman.h>
#include <errno.h>


/**
 * @internal @file
 * @ingroup Port
 * @brief Helpers for protecting shared memory regions in the virtual address space (used by omrmmap and j9shmem).
 */

/* Helper for j9shmem and omrmmap.
 *
 * Adheres to the j9shmem_protect() and omrmmap() APIs
 */
intptr_t
protect_memory(struct J9PortLibrary *portLibrary, void *address, uintptr_t length, uintptr_t flags)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	uintptr_t index;
	intptr_t unixFlags = 0;
	intptr_t rc = -1;

	if ((flags & OMRPORT_PAGE_PROTECT_WRITE) == 0) {
		/*omrtty_printf(portLibrary,"Calling _MPROT addr=%d length=%d\n",address,length);*/
		rc = _MPROT((uintptr_t)address, ((uintptr_t)address) + length - 1); /*flags 0=prot 1=unprot */
	} else {
		/*omrtty_printf(portLibrary,"Calling _MUNPROT addr=%d length=%d\n",address,length);*/
		rc = _MUNPROT((uintptr_t)address, ((uintptr_t)address) + length - 1); /*flags 0=prot 1=unprot */
	}

	/* Code for zOS 64-bit will need to build a ranglist and pass it in */

	if (rc != 0) {
		omrerror_set_last_error(errno, OMRPORT_PAGE_PROTECT_FAILED);
	}

	return rc;
}

uintptr_t
protect_region_granularity(struct J9PortLibrary *portLibrary, void *address)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	uintptr_t granularity = 0;

	granularity = omrvmem_supported_page_sizes()[0];

	return granularity;
}
