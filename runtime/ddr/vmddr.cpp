/*******************************************************************************
 * Copyright IBM Corp. and others 2017
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "j9.h"
#include "j9port_generated.h"
#include "j9generated.h"
#include "j9nongenerated.h"
#include "../rasdump/rasdump_internal.h"
#include "shcdatatypes.h"
#include "srphashtable_api.h"
#include "stackwalk.h"
#include "ComparingCursor.hpp"
#include "ComparingCursorHelper.hpp"
#include "ddrhelp.h"

#define VM_DdrDebugLink(type) DdrDebugLink(vm, type)

VM_DdrDebugLink(CacheletHints)
VM_DdrDebugLink(CacheletWrapper)
VM_DdrDebugLink(CharArrayWrapper)
VM_DdrDebugLink(ComparingCursor)
VM_DdrDebugLink(ComparingCursorHelper)

const void *
DDR_ComparingCursorHelper(ComparingCursorHelper * helper)
{
	return helper->getBaseAddress();
}

VM_DdrDebugLink(J9DbgROMClassBuilder)
VM_DdrDebugLink(J9DbgStringInternTable)
VM_DdrDebugLink(J9IndexableObject)
VM_DdrDebugLink(J9IndexableObjectContiguous)
VM_DdrDebugLink(J9IndexableObjectContiguousCompressed)
VM_DdrDebugLink(J9IndexableObjectContiguousFull)
VM_DdrDebugLink(J9IndexableObjectDiscontiguous)
VM_DdrDebugLink(J9IndexableObjectDiscontiguousCompressed)
VM_DdrDebugLink(J9IndexableObjectDiscontiguousFull)
VM_DdrDebugLink(J9JITDataCacheHeader)
VM_DdrDebugLink(J9JITFrame)
VM_DdrDebugLink(J9JITHashTable)
VM_DdrDebugLink(J9JITStackAtlas)
VM_DdrDebugLink(J9Object)
VM_DdrDebugLink(J9ObjectCompressed)
VM_DdrDebugLink(J9ObjectFull)
VM_DdrDebugLink(J9OSRFrame)
VM_DdrDebugLink(J9RAMMethodRef)
VM_DdrDebugLink(J9RASdumpQueue)
VM_DdrDebugLink(J9ROMClassTOCEntry)
VM_DdrDebugLink(J9ROMConstantDynamicRef)
VM_DdrDebugLink(J9SFStackFrame)
VM_DdrDebugLink(J9SRPHashTable)
VM_DdrDebugLink(J9VMPopFramesInterruptEvent)

VM_DdrDebugLink(_jbooleanArray)
VM_DdrDebugLink(_jbyteArray)
VM_DdrDebugLink(_jcharArray)
VM_DdrDebugLink(_jclass)
VM_DdrDebugLink(_jdoubleArray)
VM_DdrDebugLink(_jfloatArray)
VM_DdrDebugLink(_jintArray)
VM_DdrDebugLink(_jlongArray)
VM_DdrDebugLink(_jobjectArray)
VM_DdrDebugLink(_jshortArray)
VM_DdrDebugLink(_jstring)
VM_DdrDebugLink(_jthrowable)
VM_DdrDebugLink(_jweak)

/*
 * All builds now validate that DTFJ code is compatible with generating pointer
 * classes dynamically. This will mark core files accordingly.
 *
 * @ddr_namespace: map_to_type=DDRAlgorithmVersions
 */
#define J9DDR_GENERATE_VERSION 1
