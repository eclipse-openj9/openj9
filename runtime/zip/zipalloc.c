/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
#include "zip_internal.h"
#include "j9memcategories.h"

#ifdef J9VM_OPT_ZLIB_SUPPORT

#ifdef AIXPPC	/* hack for zlib/AIX problem */
#define STDC
#endif

#include "zlib.h"

#endif

#if (defined(J9VM_OPT_ZLIB_SUPPORT)) 
void* zalloc (void* opaque, U_32 items, U_32 size);
#endif /* J9VM_OPT_ZLIB_SUPPORT */
#if (defined(J9VM_OPT_ZLIB_SUPPORT)) 
void zfree (void* opaque, void* address);
#endif /* J9VM_OPT_ZLIB_SUPPORT */

#if (defined(J9VM_OPT_ZLIB_SUPPORT)) 
/*
	ZLib interface to j9mem_allocate_memory.
*/
void* zalloc(void* opaque, U_32 items, U_32 size)
{
	PORT_ACCESS_FROM_PORT(((J9PortLibrary*)opaque));
	
	return j9mem_allocate_memory(items * size, J9MEM_CATEGORY_VM_JCL);
}
#endif /* J9VM_OPT_ZLIB_SUPPORT */


#if (defined(J9VM_OPT_ZLIB_SUPPORT)) 
/*
	ZLib interface to j9mem_free_memory.
*/
void zfree(void* opaque, void* address)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary*)opaque);

	j9mem_free_memory(address);
}
#endif /* J9VM_OPT_ZLIB_SUPPORT */




