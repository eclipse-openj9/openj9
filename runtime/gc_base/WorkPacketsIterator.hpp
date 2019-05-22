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

/**
 * @file
 * @ingroup GC_Base
 */

#if !defined(WORKPACKETSITERATOR_HPP_)
#define WORKPACKETSITERATOR_HPP_

#include "BaseNonVirtual.hpp"
#include "WorkPackets.hpp"

class MM_EnvironmentBase;
class MM_Packet;
class MM_PacketList;


/**
 * This class contains the knowledge of how to walk the packet lists in the MM_WorkPackets structure and
 * return each non-empty MM_PacketList instance it finds.
 * It is only intended to be used as part of the MM_WorkPacketsIterator's implementation.
 */
class MM_PacketListIterator : public MM_BaseNonVirtual
{
	/* Data Members */
private:
	enum {
		_numPacketLists = 5	/**< The definition of the constant for how many lists we will be able to walk */
	};
	MM_PacketList *_packetLists[_numPacketLists + 1];	/**< Cached pointers to the MM_PacketList instances in the MM_WorkPackets instance with which the receiver was initialized */
	I_32 _nextListIndex;	/**< The next packet list index in the _packetLists array */
protected:
public:

	/* Member Functions */
private:
protected:
public:
	/**
	 * @param env[in] A GC thread
	 * @return The next MM_PacketList or NULL if there are no more to be returned
	 */
	MM_PacketList *nextPacketList(MM_EnvironmentBase *env);

	/**
	 * Create a MM_PacketListIterator instance to walk the lists in the given workPackets.
	 * @param env[in] A GC thread
	 * @param workPackets[in] The MM_WorkPackets instance which the receiver will iterate in order to find MM_PacketList instances
	 */
	MM_PacketListIterator(MM_EnvironmentBase *env, MM_WorkPackets *workPackets);
};


/**
 * This class contains the knowledge of how to walk the sublists within a packet list.
 * It is only intended to be used as part of the MM_WorkPacketsIterator's implementation.
 */
class MM_PacketSublistIterator : public MM_BaseNonVirtual
{
	/* Data Members */
private:
	MM_PacketList *_listToWalk;	/**< The list from which we will extract the sublist (NULL creates an empty iterator) */
	UDATA _nextSublistIndex;	/**< The next sublist index from which to extract a sublist from _listToWalk when we are next requested */
protected:
public:

	/* Member Functions */
private:
protected:
public:
	/**
	 * @param env[in] A GC thread
	 * @return The next non-empty sublist in _listToWalk or NULL if there are no more non-empty sublists
	 */
	MM_PacketList::PacketSublist *nextSublist(MM_EnvironmentBase *env);
	
	/**
	 * @param env[in] A GC thread
	 * @param packetList[in] The packet to iterate for its sublists (passing NULL is permitted and creates an empty iterator)
	 */
	MM_PacketSublistIterator(MM_EnvironmentBase *env, MM_PacketList *packetList);
};


/**
 * This class contains the knowledge of how to walk a list of packets, starting at any packet in the list.
 * It is only intended to be used as part of the MM_WorkPacketsIterator's implementation.
 */
class MM_PacketIterator : public MM_BaseNonVirtual
{
	/* Data Members */
private:
	MM_Packet *_nextPacket;	/**< The next packet to be returned by the receiver */
protected:
public:

	/* Member Functions */
private:
protected:
public:
	/**
	 * @param env[in] A GC thread
	 * @return The next packet in the list or NULL if there are no more packets to iterate
	 */
	MM_Packet *nextPacket(MM_EnvironmentBase *env);
	
	/**
	 * Initializes an instance of the receiver for walking packets starting at startPacket.
	 * @param env A GC thread
	 * @param startPacket The first packet which will be returned by nextPacket() and the chain of packets
	 * which will be followed (if NULL is passed, the initialized instance will return no packets)
	 */
	MM_PacketIterator(MM_EnvironmentBase *env, MM_Packet *startPacket);
};


/**
 * @todo Provide class documentation
 * @ingroup GC_Base
 */
class MM_WorkPacketsIterator: public MM_BaseNonVirtual
{
	/* Data Members */
private:
	MM_PacketListIterator _packetListIterator;	/**< The iterator instance used to walk the packet lists inside the MM_WorkPackets instance with which the receiver was initialized */
	MM_PacketSublistIterator _packetSublistIterator;	/**< The iterator instance used to walk the packet sublists inside the packet list instance currently being iterated */
	MM_PacketIterator _packetIterator;	/**< The iterator instance used to walk chains of packets once the receiver has found the root of the next packet list */
protected:
public:

	/* Member Functions */
private:
protected:
public:
	MM_Packet *nextPacket(MM_EnvironmentBase *env);

	/**
	 * Create a WorkPacketsIterator object.
	 */
	MM_WorkPacketsIterator(MM_EnvironmentBase *env, MM_WorkPackets *workPackets);
};
#endif /* WORKPACKETSITERATOR_HPP_*/
