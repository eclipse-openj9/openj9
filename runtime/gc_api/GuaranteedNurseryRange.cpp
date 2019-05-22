/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
#include "j9.h"
#include "j9cfg.h"
#include "modron.h"
#include "ModronAssertions.h"

#include "GCExtensions.hpp"

extern "C" {

/**
 * Answer the region of address space which is guaranteed to only ever contain objects in the nursery.
 * The GC will attempt to answer the largest range it can. The range may extend beyond the end of the
 * heap (to the end of the address range) if the GC can guarantee that no other heap objects will appear
 * in that area. Note that stack allocated objects may appear in the nursery range.
 *
 * If there is no nursery, or no part of its range can be guaranteed, start and end are both set to NULL.
 *
 * If the guaranteed nursery range extends to the bottom of the address range, start will be NULL and end 
 * will be the highest nursery address.
 *
 * If the guaranteed nursery range extends to the top of the address range, start will be the lowest nursery
 * address and end will be (void*)UDATA_MAX.
 *
 * Since the nursery may grow and contract during the JVM's lifetime, this range might cover only a very
 * small portion of the heap (typically about 4MB).
 *
 * Various features may cause the nursery range to be bounded on both ends. These include:
 *  - shared objects in Zero
 *  - non-flat memory model (e.g. some Vich configurations)
 *  - RTJ scopes
 *  - resman regions
 *
 * @param[in] javaVM the J9JavaVM* instance
 * @param[out] start the start address is returned in this pointer
 * @param[out] end the end address is returned in this pointer
 * 
 * @note this function is used by the JIT for barrier omission optimizations
 */
void
j9mm_get_guaranteed_nursery_range(J9JavaVM* javaVM, void** start, void** end)
{
#if defined (J9VM_GC_GENERATIONAL)
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM->omrVM);
	extensions->getGuaranteedNurseryRange(start, end);
#else /* J9VM_GC_GENERATIONAL */
	*start = NULL;
	*end = NULL;
#endif /* J9VM_GC_GENERATIONAL */
}

}
