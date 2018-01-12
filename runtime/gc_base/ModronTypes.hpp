
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

/**
 * @file
 * @ingroup GC_Base
 */

#if !defined(MODRONTYPES_HPP_)
#define MODRONTYPES_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modronopt.h"
 
/**
 * @ingroup GC_Base
 * @name Iterators
 * @{
 */

#include "ClassArrayClassSlotIterator.hpp"
#include "ClassHeapIterator.hpp"
#include "ClassIterator.hpp"
#include "ClassIteratorClassSlots.hpp"
#include "ClassLoaderSegmentIterator.hpp"
#include "ClassLocalInterfaceIterator.hpp"
#include "ClassStaticsIterator.hpp"
#include "ClassSuperclassesIterator.hpp"
#include "ConstantPoolObjectSlotIterator.hpp"
#include "ConstantPoolClassSlotIterator.hpp"
#include "HashTableIterator.hpp"

#if defined(J9VM_OPT_JVMTI)
#include "JVMTIObjectTagTableIterator.hpp"
#endif /* J9VM_OPT_JVMTI */

#include "MixedObjectIterator.hpp"
#include "ObjectHeapIterator.hpp"
#include "PointerArrayIterator.hpp"
#include "PoolIterator.hpp"
#include "SegmentIterator.hpp"
#include "SublistIterator.hpp"
#include "SublistSlotIterator.hpp"
#include "VMClassSlotIterator.hpp"
#include "VMThreadIterator.hpp"
#include "VMThreadJNISlotIterator.hpp"
#include "VMThreadListIterator.hpp"

#if defined(J9VM_INTERP_HOT_CODE_REPLACEMENT)
#include "VMThreadMonitorRecordSlotIterator.hpp"
#endif /* J9VM_INTERP_HOT_CODE_REPLACEMENT */

#include "VMThreadSlotIterator.hpp"
#include "VMThreadStackSlotIterator.hpp"

#if defined(J9VM_OPT_JVMTI)
typedef GC_PoolIterator GC_JVMTIObjectTagTableListIterator;
#endif /* J9VM_OPT_JVMTI */

typedef GC_HashTableIterator GC_StringTableIterator;

#if defined(J9VM_GC_MODRON_SCAVENGER)
typedef GC_SublistIterator GC_RememberedSetIterator;
typedef GC_SublistSlotIterator GC_RememberedSetSlotIterator;
#endif /* J9VM_GC_MODRON_SCAVENGER */

typedef GC_PoolIterator GC_JNIGlobalReferenceIterator;
typedef GC_PoolIterator GC_JNIWeakGlobalReferenceIterator;

#if defined(J9VM_INTERP_DEBUG_SUPPORT)
typedef GC_PoolIterator GC_DebuggerReferenceIterator;
typedef GC_PoolIterator GC_DebuggerClassReferenceIterator;
#endif /* J9VM_INTERP_DEBUG_SUPPORT */
/**
 * @} Iterators
 */
#endif /* MODRONTYPES_HPP_ */
