
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

#include "j9.h"
#include "j9cfg.h"

#include "ClassHeapIterator.hpp"
#include "ClassModel.hpp"

/**
 * @return the next class in the segment being iterated
 * @return NULL if there are no more classes in the segment
 */
J9Class *
GC_ClassHeapIterator::nextClass() 
{
	J9Class *currentScanPtr;
#if defined(J9VM_OPT_FRAGMENT_RAM_CLASSES)
	if(_scanPtr != NULL) {
		currentScanPtr = _scanPtr;
		_scanPtr = _scanPtr->nextClassInSegment;
		return currentScanPtr;
	}
#else
	if(_scanPtr < (J9Class *)_memorySegment->heapAlloc) {
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
		/* Skip the JIT VTable if there is one */
		if(_vm->jitConfig) {
			_scanPtr = (J9Class *)( ((UDATA)_scanPtr) + *((UDATA *)_scanPtr) );
		}
#endif
		currentScanPtr = _scanPtr;
		_scanPtr = (J9Class *)( ((UDATA)_scanPtr) + GC_ClassModel::getSizeInBytes(_scanPtr) );
		return currentScanPtr;
	}
#endif
	return NULL;
}


