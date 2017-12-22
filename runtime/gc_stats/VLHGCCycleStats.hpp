
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
 * @ingroup GC_Stats
 */

#if !defined(VLHGCCYCLESTATS_HPP_)
#define VLHGCCYCLESTATS_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "j9port.h"
#include "j9consts.h"
#include "modronopt.h"

#if defined(J9VM_GC_VLHGC)

#include "Base.hpp"
#include "InterRegionRememberedSetStats.hpp"
#include "ModronAssertions.h"
#include "SweepVLHGCStats.hpp"
#include "WorkPacketStats.hpp"
#include "VLHGCIncrementStats.hpp"

/**
 * Storage for statistics relevant to the collection cycles
 * @ingroup GC_Stats
 */
class MM_VLHGCCycleStats : public MM_Base
{
public:
	class MM_MarkVLHGCStats _markStats;  /**< Stats for mark phase of cycle */
	class MM_MarkVLHGCStats _concurrentMarkStats;  /**< Stats for concurrent mark phase of cycle */
	class MM_MarkVLHGCStats _incrementalMarkStats;  /**< Stats for incremental (stop-the-world) mark phase of cycle */
	class MM_WorkPacketStats _workPacketStats;  /**< Stats for work packet activity of cycle */
	class MM_InterRegionRememberedSetStats _irrsStats; /**< Stats for Inter Region Remembered Set processing */

public:
	MM_VLHGCCycleStats() :
		_markStats()
		,_concurrentMarkStats()
		,_incrementalMarkStats()
		,_workPacketStats()
		,_irrsStats()
		{};

	/**
	 * Reset the statistics of the receiver for a new round.
	 */
	MMINLINE void clear()
	{
		_markStats.clear();
		_concurrentMarkStats.clear();
		_incrementalMarkStats.clear();
		_workPacketStats.clear();
		_irrsStats.clear();
	}

	/**
	 * Add / combine the statistics from the parameter to the receiver.
	 */
	MMINLINE void merge(MM_VLHGCIncrementStats *stats)
	{
		_markStats.merge(&stats->_markStats);
		_workPacketStats.merge(&stats->_workPacketStats);
		_irrsStats.merge(&stats->_irrsStats);

		switch (stats->_globalMarkIncrementType) {
		case MM_VLHGCIncrementStats::mark_concurrent:
			_concurrentMarkStats.merge(&stats->_markStats);
			break;

		case MM_VLHGCIncrementStats::mark_incremental:
			_incrementalMarkStats.merge(&stats->_markStats);
			break;
			
		case MM_VLHGCIncrementStats::mark_idle:
		case MM_VLHGCIncrementStats::mark_global_collection:
			break;

		default:
			Assert_MM_unreachable();
		}
	}

};

#endif /* J9VM_GC_VLHGC */
#endif /* VLHGCCYCLESTATS_HPP_ */
