
/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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

#include "VMThreadSlotIterator.hpp"

/**
 * @return the next slot in the thread which contains an object reference
 * @return NULL if no more such slots exist
 */
j9object_t *
GC_VMThreadSlotIterator::nextSlot() 
{
	switch (_scanIndex++) {
	case 0:
		return &(_vmThread->jitException);
	case 1:
		return &(_vmThread->currentException);
	case 2:
		return &(_vmThread->threadObject);
	case 3:
		return &(_vmThread->stopThrowable);
	case 4:
		return &(_vmThread->outOfMemoryError);
	case 5:
		return &(_vmThread->blockingEnterObject);
	case 6:
		return &(_vmThread->forceEarlyReturnObjectSlot);
	case 7:
		return &(_vmThread->javaLangThreadLocalCache);
	case 8:
		return (j9object_t *)&(_vmThread->omrVMThread->_savedObject1);
	case 9:
		return (j9object_t *)&(_vmThread->omrVMThread->_savedObject2);
	default:
		break;
	}
	return NULL;
}
