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

#if !defined(REALTIMEMARKTASK_HPP_)
#define REALTIMEMARKTASK_HPP_

#include "omr.h"
#include "omrcfg.h"

#include "CycleState.hpp"
#include "IncrementalParallelTask.hpp"

class MM_Dispatcher;
class MM_EnvironmentBase;
class MM_RealtimeGC;
class MM_RealtimeMarkingScheme;

/**
 * Task representing a the mark phase of Metronome global collection.
 */
class MM_RealtimeMarkTask : public MM_IncrementalParallelTask
{
private:
	MM_RealtimeMarkingScheme *_markingScheme;
	MM_CycleState *_cycleState;  /**< Collection cycle state active for the task */

public:
	virtual uintptr_t getVMStateID() { return OMRVMSTATE_GC_MARK; };
	
	virtual void run(MM_EnvironmentBase *env);
	virtual void setup(MM_EnvironmentBase *env);
	virtual void cleanup(MM_EnvironmentBase *env);
	
	MM_RealtimeMarkTask(MM_EnvironmentBase *env, MM_Dispatcher *dispatcher, MM_RealtimeGC *realtimeGC, MM_RealtimeMarkingScheme *markingScheme, MM_CycleState *cycleState) :
		MM_IncrementalParallelTask(env, dispatcher)
		,_markingScheme(markingScheme)
		,_cycleState(cycleState)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* REALTIMEMARKTASK_HPP_ */
