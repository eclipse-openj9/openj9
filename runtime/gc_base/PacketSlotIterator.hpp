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

#if !defined(PacketSlotIterator_HPP_)
#define PacketSlotIterator_HPP_

#include "BaseNonVirtual.hpp"
#include "Packet.hpp"

/**
 * @todo Provide class documentation
 * @ingroup GC_Base
 */
class MM_PacketSlotIterator: public MM_BaseNonVirtual
{
private:
	MM_Packet *_packet;                          /**< Packet being iterated */
	J9Object **_nextSlot;                        /**< Next slot in the packet to be returned */

public:
	/**
	 * @return The next slot in the packet or NULL if the end of the packet has been reached (no slots pointing at NULL or array split tags will be returned)
	 */
	J9Object **nextSlot();

	/**
	 * Resets the split tag for correspondingObject to refer to a zero index which will force mark to completely rescan this array when
	 * it next sees it.
	 * Can only be called immediately after a slot containing correspondingObject is returned and allows updating of the corresponding
	 * array split tag.
	 * Note that this has no effect if the split tag would have been outside of the packet (since that implies that there isn't one)
	 * or if there was no split tag for this object.  An assertion failure will be generated if correspondingObject does not match.
	 * @param correspondingObject[in] The object which will be sanity checked against the previous returned object to ensure a match
	 * @param newValue the data to store in the tag slot. It is the caller's responsibility to ensure that this is appropriately tagged.
	 */
	void resetSplitTagIndexForObject(J9Object *correspondingObject, UDATA newValue);
	
	/**
	 * Create a PacketSlotIterator object.
	 */
	MM_PacketSlotIterator(MM_Packet *packet)
		: MM_BaseNonVirtual()
		, _packet(packet)
		, _nextSlot((J9Object **)packet->_basePtr)
	{
		_typeId = __FUNCTION__;
	}
};
#endif /* PacketSlotIterator_HPP_*/
