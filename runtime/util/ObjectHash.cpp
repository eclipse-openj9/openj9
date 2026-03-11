/*******************************************************************************
 * Copyright IBM Corp. and others 2001
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "util_api.h"
#include "ObjectHash.hpp"

extern "C" {


I_32
computeObjectAddressToHash(J9JavaVM *vm, j9object_t objectPointer)
{
	return VM_ObjectHash::inlineComputeObjectAddressToHash(vm, objectPointer);
}

I_32
convertValueToHash(J9JavaVM *vm, UDATA value)
{
	return VM_ObjectHash::inlineConvertValueToHash(vm, value);
}

I_32
objectHashCode(J9JavaVM *vm, j9object_t objectPointer
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
		, jboolean *oomOccurred
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
)
{
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
	bool oomOccurredBool = false;
	I_32 result = VM_ObjectHash::inlineObjectHashCode(vm, objectPointer, &oomOccurredBool);
	if (NULL != oomOccurred) {
		*oomOccurred = oomOccurredBool ? JNI_TRUE : JNI_FALSE;
	}
	return result;
#else /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
	return VM_ObjectHash::inlineObjectHashCode(vm, objectPointer);
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
}

#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
I_32
convertObjectToHash(J9JavaVM *vm, J9VMThread *currentThread, j9object_t objectPointer, J9Class *clazz)
{
	bool oomOccurred = false;
	I_32 result = VM_ObjectHash::inlineConvertObjectToHash(vm, currentThread, objectPointer, &oomOccurred);
	/* Return 0 if OOM occurs, skipping OOM handling from GC code. */
	return oomOccurred ? 0 : result;
}
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */

} /* extern "C" */
