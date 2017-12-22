/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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

#include "PacketSlotIterator.hpp"

#include "ModronAssertions.h"


J9Object **
MM_PacketSlotIterator::nextSlot()
{
	/* we can't assume that _nextSlot points inside the packet or contains a value which we want to return so cue it up to the next valid slot */
	bool found = false;
	while ((!found) && (_nextSlot < (J9Object **)_packet->_currentPtr)) {
		if ((NULL != *_nextSlot) && (PACKET_ARRAY_SPLIT_TAG != (PACKET_ARRAY_SPLIT_TAG & (UDATA)*_nextSlot))) {
			/* we have found a valid slot which we want to return so fall out of the loop without advancing the cursor */
			found = true;
		} else {
			/* this slot was not acceptable so advance to the next - we will fall out of the loop if this advances us past the end */
			_nextSlot += 1;
		}
	}
	/* by this point, _nextSlot points either at the next valid slot or past the end of the packet */
	J9Object **slotToReturn = NULL;
	if (_nextSlot < (J9Object **)_packet->_currentPtr) {
		/* this is a valid slot so return it */
		slotToReturn = _nextSlot;
		/* advance the _nextSlot past the value we have read */
		_nextSlot += 1;
	}
	return slotToReturn;
}

void
MM_PacketSlotIterator::resetSplitTagIndexForObject(J9Object *correspondingObject, UDATA newValue)
{
	J9Object **tagSlot = _nextSlot - 2;
	if (tagSlot >= (J9Object **)_packet->_basePtr) {
		/* the next slot is within the range of the packet */
		if (PACKET_ARRAY_SPLIT_TAG == (PACKET_ARRAY_SPLIT_TAG & (UDATA)*tagSlot)) {
			/* the next slot is an array split */
			J9Object **objectSlot = _nextSlot - 1;
			Assert_MM_true(correspondingObject == *objectSlot);
			*tagSlot = (J9Object *)newValue;
		}
	}
}
