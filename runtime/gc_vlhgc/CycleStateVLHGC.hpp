/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#if !defined(CYCLESTATEVLHGC_HPP_)
#define CYCLESTATEVLHGC_HPP_

#include "CycleState.hpp"

#include "VLHGCIncrementStats.hpp"
#include "VLHGCCycleStats.hpp"

class MM_CycleState;
class MM_SchedulingDelegate;

/**
 * Per cycle state information
 */
class MM_CycleStateVLHGC : public MM_CycleState {
public:
	MM_VLHGCIncrementStats _vlhgcIncrementStats; /**< Stats for the various phases / operations within an increment */
	MM_VLHGCCycleStats _vlhgcCycleStats; /**< Stats for the various phases / operations within a cycle */
	MM_SchedulingDelegate *_schedulingDelegate;

	uintptr_t _desiredCompactWork; /**< Stats for desired amount of work to be compacted during a particular PGC cycle */
	bool _useSlidingCompactor; /**< Stats to indicate If we surpassed survivor free memory. If so it's set to true, false otherwise */
	bool _abortFlagRaisedDuringPGC; /**< Stats that indicate if PGC cycle aborted or not */

	MM_CycleStateVLHGC()
		: MM_CycleState()
		, _vlhgcIncrementStats()
		, _vlhgcCycleStats()
		, _schedulingDelegate(NULL)
		, _desiredCompactWork(0)
		, _useSlidingCompactor(false)
		, _abortFlagRaisedDuringPGC(false)
	{
	}
};

#endif /* CYCLESTATEVLHGC_HPP_ */
