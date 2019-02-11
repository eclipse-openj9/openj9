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
#if defined(J9VM_OUT_OF_PROCESS)
#include "j9dbgext.h"
#endif
#include "j9protos.h"
#include "j9consts.h"
#include "util_internal.h"

#if defined(J9VM_OUT_OF_PROCESS)
#define READU(field) dbgReadUDATA((UDATA *)&(field))
#define READP(field) ((void *)READU(field))
#else /* not J9VM_OUT_OF_PROCESS */
#define READU(field) (field)
#define READP(field) (field)
#endif

#define J9RAMCLASS_SUPERCLASSES(clazz) ((J9Class **)READP((clazz)->superclasses))
#define J9RAMCLASS_DEPTH(clazz) (READU(J9CLASS_FLAGS(clazz)) & J9AccClassDepthMask)

UDATA 
isSameOrSuperClassOf(J9Class *superClass, J9Class *baseClass)
{
	UDATA superClassDepth = J9RAMCLASS_DEPTH(superClass);
	
	return ((baseClass == superClass)
					|| ((J9RAMCLASS_DEPTH(baseClass) > superClassDepth)
					   && (READP(J9RAMCLASS_SUPERCLASSES(baseClass)[superClassDepth]) == superClass)));
}


BOOLEAN
isSameOrSuperInterfaceOf(J9Class *superInterface, J9Class *baseInterface)
{
	BOOLEAN isSameOrSuper = FALSE;

	if (superInterface == baseInterface) {
		isSameOrSuper = TRUE;
	} else {
		J9ITable *iTable = (J9ITable *)baseInterface->iTable;
		if (iTable->depth <= ((J9ITable *)superInterface->iTable)->depth) {
			/* Super interface must be at a shallower depth than baseInterface */
			isSameOrSuper = FALSE;
		} else {
			while (NULL != iTable) {
				if (iTable->interfaceClass == superInterface) {
					isSameOrSuper = TRUE;
					break;
				}
				iTable = iTable->next;
			}
		}
	}
	return isSameOrSuper;
}
