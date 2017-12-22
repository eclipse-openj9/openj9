
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

#if !defined(VLHGC_INCREMENT_STATS_HPP_)
#define VLHGC_INCREMENT_STATS_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "j9port.h"
#include "j9consts.h"
#include "modronopt.h"

#if defined(J9VM_GC_VLHGC)

#include "Base.hpp"
#include "ClassUnloadStats.hpp"
#if defined(J9VM_GC_MODRON_COMPACTION)
#include "CompactVLHGCStats.hpp"
#endif /* J9VM_GC_MODRON_COMPACTION */
#include "CopyForwardStats.hpp"
#include "InterRegionRememberedSetStats.hpp"
#include "MarkVLHGCStats.hpp"
#include "SweepVLHGCStats.hpp"
#include "WorkPacketStats.hpp"

/**
 * Storage for statistics relevant to the sweep phase of global collection
 * @ingroup GC_Stats
 */
class MM_VLHGCIncrementStats : public MM_Base
{
public:
	class MM_MarkVLHGCStats _markStats;  /**< Stats for mark phase of increment */
	class MM_SweepVLHGCStats _sweepStats;  /**< Stats for sweep phase of increment */
#if defined(J9VM_GC_MODRON_COMPACTION)
	class MM_CompactVLHGCStats _compactStats;  /**< Stats for compact phase of increment */
#endif /* J9VM_GC_MODRON_COMPACTION */
	class MM_WorkPacketStats _workPacketStats;  /**< Stats for work packet activity of increment */
	class MM_CopyForwardStats _copyForwardStats;  /**< Stats for copy forward phase of increment */
	class MM_ClassUnloadStats _classUnloadStats;  /**< Stats for class unload operations of the increment */
	class MM_InterRegionRememberedSetStats _irrsStats; /**< Stats for Inter Region Remembered Set processing */

	enum GlobalMarkIncrementType {
		mark_idle = 0, /**< No Global marking in progress */
		mark_concurrent = 1, /**< Global marking is being performed concurrently with mutator */
		mark_incremental = 2, /**< Global marking is being performed non-concurrently in stop-the-world increments */
		mark_global_collection = 3, /** Global marking is being performed as part of a global collection */
	} _globalMarkIncrementType;

public:
	MM_VLHGCIncrementStats() :
		_markStats()
		,_sweepStats()
#if defined(J9VM_GC_MODRON_COMPACTION)
		,_compactStats()
#endif /* J9VM_GC_MODRON_COMPACTION */
		,_workPacketStats()
		,_copyForwardStats()
		,_classUnloadStats()
		,_irrsStats()
		,_globalMarkIncrementType(MM_VLHGCIncrementStats::mark_idle)
		{};

	/**
	 * Reset the statistics of the receiver for a new round.
	 */
	MMINLINE void clear()
	{
		_markStats.clear();
		_sweepStats.clear();
#if defined(J9VM_GC_MODRON_COMPACTION)
		_compactStats.clear();
#endif /* J9VM_GC_MODRON_COMPACTION */
		_workPacketStats.clear();
		_copyForwardStats.clear();
		_classUnloadStats.clear();
		_irrsStats.clear();
		_globalMarkIncrementType = MM_VLHGCIncrementStats::mark_idle;
	}

	/**
	 * Add / combine the statistics from the parameter to the receiver.
	 */
	MMINLINE void merge(MM_VLHGCIncrementStats *stats)
	{
		_markStats.merge(&stats->_markStats);
		_sweepStats.merge(&stats->_sweepStats);
#if defined(J9VM_GC_MODRON_COMPACTION)
		_compactStats.merge(&stats->_compactStats);
#endif /* J9VM_GC_MODRON_COMPACTION */
		_workPacketStats.merge(&stats->_workPacketStats);
		_copyForwardStats.merge(&stats->_copyForwardStats);
		_irrsStats.merge(&stats->_irrsStats);
	}
};

#endif /* J9VM_GC_VLHGC */
#endif /* VLHGC_INCREMENT_STATS_HPP_ */
