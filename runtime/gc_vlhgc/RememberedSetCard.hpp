/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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
 * @ingroup gc_vlhgc
 */

#if !defined(REMEMBEREDSETCARD_HPP)
#define REMEMBEREDSETCARD_HPP

#include "modron.h"

class MM_RememberedSetCard
{
public:
	MMINLINE static uintptr_t cardSize(bool compressed) { return compressed ? sizeof(uint32_t) : sizeof(uintptr_t); }

	/**
	 * Read the value of a card.
	 *
	 * @param[in] addr the card address
	 * @param[in] compressed true if cards are compressed, false if not
	 * @return the card value
	 */
	MMINLINE static UDATA readCard(MM_RememberedSetCard *addr, bool compressed)
	{
		return compressed ? *(uint32_t*)addr : *(uintptr_t*)addr;
	}

	/**
	 * Write the value of a card.
	 *
	 * @param[in] addr the card address
	 * @param[in] compressed true if cards are compressed, false if not
	 * @param[in] card the card value
	 */
	MMINLINE static void writeCard(MM_RememberedSetCard *addr, UDATA card, bool compressed)
	{
		if (compressed) {
			*(uint32_t*)addr = (uint32_t)card;
		} else {
			*(uintptr_t*)addr = (uintptr_t)card;
		}
	}

	/**
	 * Calculate the addition of an integer to a card address
	 *
	 * @param[in] base the base card address
	 * @param[in] index the index to add
	 * @param[in] compressed true if cards are compressed, false if not
	 * @return the adjusted address
	 */
	MMINLINE static MM_RememberedSetCard* addToCardAddress(MM_RememberedSetCard *base, intptr_t index, bool compressed)
	{
		if (compressed) {
			return (MM_RememberedSetCard*)((uint32_t*)base + index);
		} else {
			return (MM_RememberedSetCard*)((uintptr_t*)base + index);
		}
	}

	/**
	 * Calculate the subtraction of an integer from a card address
	 *
	 * @param[in] base the base card address
	 * @param[in] index the index to subtract
	 * @param[in] compressed true if cards are compressed, false if not
	 * @return the adjusted address
	 */
	MMINLINE static MM_RememberedSetCard* subtractFromCardAddress(MM_RememberedSetCard *base, intptr_t index, bool compressed)
	{
		if (compressed) {
			return (MM_RememberedSetCard*)((uint32_t*)base - index);
		} else {
			return (MM_RememberedSetCard*)((uintptr_t*)base - index);
		}
	}

	/**
	 * Calculate the difference between two card addresses, in slots
	 *
	 * @param[in] p1 the value to be subtracted from
	 * @param[in] p2 the value to be subtracted
	 * @param[in] compressed true if cards are compressed, false if not
	 * @return p1 - p2 in slots
	 */
	MMINLINE static intptr_t subtractCardAddresses(MM_RememberedSetCard *p1, MM_RememberedSetCard *p2, bool compressed)
	{
		if (compressed) {
			return (uint32_t*)p1 - (uint32_t*)p2;
		} else {
			return (uintptr_t*)p1 - (uintptr_t*)p2;
		}
	}
};

#endif /* REMEMBEREDSETCARD_HPP */
