
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
 * @ingroup GC_Modron_Standard
 */

#if !defined(REGIONBASEDOVERFLOW_HPP_)
#define REGIONBASEDOVERFLOW_HPP_

#include "j9.h"
#include "ModronAssertions.h"

#include "WorkPacketOverflow.hpp"

#include "CycleState.hpp"
#include "EnvironmentBase.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"

class MM_EnvironmentVLHGC;
class MM_HeapRegionManager;
class MM_Packet;
class MM_WorkPackets;

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_RegionBasedOverflowVLHGC : public MM_WorkPacketOverflow
{
/* Data members */
public:
protected:
private:
	MM_GCExtensions *_extensions;
	MM_HeapRegionManager * const _heapRegionManager;	/**< The region manager used to translate object addresses into region addresses */
	const U_8 _overflowFlag;	/**< The overflow flag to set in the regions where we trigger overflow */
	
	enum {
		OVERFLOW_PARTIAL_COLLECT = 0x1,	/**< Overflow flag set if we overflowed a region when marking for a PGC */
		OVERFLOW_GLOBAL_COLLECT = 0x2,	/**< Overflow flag set if we overflowed a region when marking for a GMP or global collect */
	};
/* Methods */
public:
	static MM_RegionBasedOverflowVLHGC *newInstance(MM_EnvironmentBase *env, MM_WorkPackets *workPackets, U_8 overflowFlag);

	virtual void reset(MM_EnvironmentBase *env);
	virtual bool isEmpty();
	
	virtual void emptyToOverflow(MM_EnvironmentBase *env, MM_Packet *packet, MM_OverflowType type);
	virtual void fillFromOverflow(MM_EnvironmentBase *env, MM_Packet *packet);
	virtual void overflowItem(MM_EnvironmentBase *env, void *item, MM_OverflowType type);

	/**
	 * Create a Card Based Overflow object
	 */	
	MM_RegionBasedOverflowVLHGC(MM_EnvironmentBase *env, MM_WorkPackets *workPackets, U_8 overflowFlag);

	/**
	 * A helper to look up the appropriate overflow flag which corresponds to the given collectionType.
	 * @param env[in] A GC thread
	 * @param collectionType The collection type to look up
	 * @return The overflow flag which must be set on any region which overflows for this kind of collection
	 */
	static U_8 overflowFlagForCollectionType(MM_EnvironmentBase *env, MM_CycleState::CollectionType collectionType);

protected:
	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);
private:
	void overflowItemInternal(MM_EnvironmentBase *env, void *item, MM_OverflowType type);
	bool isEvacuateRegion(MM_HeapRegionDescriptorVLHGC *region)
	{
		return region->_markData._shouldMark;
	}
};

#endif /* CARDBASEDOVERFLOW_HPP_ */
