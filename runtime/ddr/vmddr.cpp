/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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
#include "j9port_generated.h"
#include "j9generated.h"
#include "j9nongenerated.h"
#include "../rasdump/rasdump_internal.h"
#include "shcdatatypes.h"
#include "srphashtable_api.h"
#include "stackwalk.h"
#include "ddrhelp.h"

#define VM_DdrDebugLink(type) DdrDebugLink(vm, type)

VM_DdrDebugLink(CacheletHints)
VM_DdrDebugLink(CacheletWrapper)
VM_DdrDebugLink(CharArrayWrapper)
VM_DdrDebugLink(J9DbgROMClassBuilder)
VM_DdrDebugLink(J9DbgStringInternTable)
VM_DdrDebugLink(J9JITDataCacheHeader)
VM_DdrDebugLink(J9JITFrame)
VM_DdrDebugLink(J9JITHashTable)
VM_DdrDebugLink(J9JITStackAtlas)
VM_DdrDebugLink(J9Object)
VM_DdrDebugLink(J9ObjectCompressed)
VM_DdrDebugLink(J9ObjectFull)
VM_DdrDebugLink(J9Object)
VM_DdrDebugLink(J9ObjectCompressed)
VM_DdrDebugLink(J9ObjectFull)
VM_DdrDebugLink(J9IndexableObject)
VM_DdrDebugLink(J9IndexableObjectContiguous)
VM_DdrDebugLink(J9IndexableObjectContiguousCompressed)
VM_DdrDebugLink(J9IndexableObjectContiguousFull)
VM_DdrDebugLink(J9IndexableObjectDiscontiguous)
VM_DdrDebugLink(J9IndexableObjectDiscontiguousCompressed)
VM_DdrDebugLink(J9IndexableObjectDiscontiguousFull)
VM_DdrDebugLink(J9OSRFrame)
VM_DdrDebugLink(J9RAMMethodRef)
VM_DdrDebugLink(J9RASdumpQueue)
VM_DdrDebugLink(J9ROMClassTOCEntry)
VM_DdrDebugLink(J9ROMConstantDynamicRef)
VM_DdrDebugLink(J9SFStackFrame)
VM_DdrDebugLink(J9SRPHashTable)
VM_DdrDebugLink(J9VMPopFramesInterruptEvent)


/* Declare these for compatibility with the old DTFJ plugin, see https://github.com/eclipse/openj9/issues/6316 */
/* @ddr_namespace: map_to_type=J9Consts */

#define J9_JAVA_CLASS_ARRAY J9AccClassArray
#define J9_JAVA_CLASS_REFERENCE_MASK J9AccClassReferenceMask
#define J9_JAVA_CLASS_GC_SPECIAL J9AccClassGCSpecial
#define J9_JAVA_CLASS_RAM_SHAPE_SHIFT J9AccClassRAMShapeShift
#define J9_JAVA_STATIC J9AccStatic
#define J9_JAVA_CLASS_RAM_ARRAY J9AccClassRAMArray
#define J9_JAVA_INTERFACE J9AccInterface
#define J9_JAVA_CLASS_DEPTH_MASK J9AccClassDepthMask
#define J9_JAVA_CLASS_HOT_SWAPPED_OUT J9AccClassHotSwappedOut
#define J9_JAVA_INTERFACE J9AccInterface
#define J9_JAVA_CLASS_DYING J9AccClassDying
#define J9_JAVA_NATIVE J9AccNative
