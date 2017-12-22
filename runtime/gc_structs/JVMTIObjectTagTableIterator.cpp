
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
 * @ingroup GC_Structs
 */

#include "JVMTIObjectTagTableIterator.hpp"

#include "Debug.hpp"

#if defined(J9VM_OPT_JVMTI)
#include "jvmtiInternal.h"

/**
 * Get the next object reference in the JVMTI Object Tag table.
 * @return A slot pointer (NULL when no more slots)
 */
void **
GC_JVMTIObjectTagTableIterator::nextSlot()
{
	J9JVMTIObjectTag *objTag = (J9JVMTIObjectTag *)_hashTableIterator.nextSlot();
	_lastTag = objTag;
	return (void **)&objTag->ref;
}

/**
 * NULLs out the current object reference slot
 */
void 
GC_JVMTIObjectTagTableIterator::removeSlot()
{
	J9JVMTIObjectTag *objTag = (J9JVMTIObjectTag *)_lastTag;
	if (NULL != objTag) {
		objTag->ref = NULL;
	}
}

#endif /* J9VM_OPT_JVMTI */
