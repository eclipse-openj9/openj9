
/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#if !defined(CALLSITESITERATOR_HPP_)
#define CALLSITESITERATOR_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"

/**
 * Iterate over all call site slots in a class which contain an object reference.
 * @ingroup GC_Structs
 */
class GC_CallSitesIterator
{
	U_32 _callSiteTotal;
	U_32 _callSiteCount;
	j9object_t *_callSitePtr;
	
public:
	GC_CallSitesIterator(J9Class *clazz) :
		_callSiteCount(clazz->romClass->callSiteCount),
		_callSitePtr(clazz->callSites)
	{
		/* check if the class's call sites have been abandoned due to hot
		 * code replace. If so, this class has no call sites of its own
		 */
		if (_callSitePtr == NULL) {
			_callSiteCount = 0;
		}
		_callSiteTotal = _callSiteCount;
	};

	/**
	 * @return the next call site slot in the class containing an object reference
	 * @return NULL if there are no more such slots
	 */
	j9object_t *
	nextSlot()
	{
		j9object_t *slotPtr;
		
		if(0 == _callSiteCount) {
			return NULL;
		}

		_callSiteCount -= 1;
		slotPtr = _callSitePtr++;

		return slotPtr;
	}

	/**
	 * Gets the current call site index.
	 * @return zero based call site index of the entry returned by the last call of nextSlot.
	 * @return -1 if nextSlot has yet to be called.
	 */
	MMINLINE IDATA getIndex() {
		return _callSiteTotal - _callSiteCount - 1;
	}
};

#endif /* CALLSITESITERATOR_HPP_ */

