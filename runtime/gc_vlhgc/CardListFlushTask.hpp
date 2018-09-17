
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
 * @ingroup GC_Modron_Standard
 */

#if !defined(CARDLISTFLUSHTASK_HPP_)
#define CARDLISTFLUSHTASK_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "j9modron.h"
#include "modronopt.h"

#include "EnvironmentBase.hpp"
#include "ParallelTask.hpp"

class MM_CycleState;
class MM_HeapRegionManager;
class MM_InterRegionRememberedSet;


/**
 * @}
 */


/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_CardListFlushTask : public MM_ParallelTask
{
	/* Data Members */
private:
	MM_HeapRegionManager * const _regionManager;
	MM_InterRegionRememberedSet * const _interRegionRememberedSet;
	MM_CycleState * const _cycleState;
protected:
public:

	/* Member Functions */
private:
protected:
public:
	virtual UDATA getVMStateID() { return OMRVMSTATE_GC_MARK; }
	
	virtual void run(MM_EnvironmentBase *env);
	virtual void setup(MM_EnvironmentBase *env);
	virtual void cleanup(MM_EnvironmentBase *env);
	
	void masterSetup(MM_EnvironmentBase *env);
	void masterCleanup(MM_EnvironmentBase *env);

	/**
	 * Checks the state of the given card and updates its content based on what state it should transition the old one from, given that we want
	 * the resultant state to also describe that a card from a card list was flushed to it.
	 * This is used in Marking and Compact scheme when we flush RSCL into CardTable.
	 * @param card[in/out] The card which will be used as the input and output of the state machine function
	 * @param gmpIsActive[in] True if there is currently a GMP in progress during this PGC
	 */
	static void writeFlushToCardState(Card *card, bool gmpIsActive);

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	virtual void synchronizeGCThreads(MM_EnvironmentBase *env, const char *id);
	virtual bool synchronizeGCThreadsAndReleaseMaster(MM_EnvironmentBase *env, const char *id);
	virtual bool synchronizeGCThreadsAndReleaseSingleThread(MM_EnvironmentBase *env, const char *id);
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

	MM_CardListFlushTask(MM_EnvironmentBase *env, MM_Dispatcher *dispatcher, MM_HeapRegionManager *manager, MM_InterRegionRememberedSet *remset)
		: MM_ParallelTask(env, dispatcher)
		, _regionManager(manager)
		, _interRegionRememberedSet(remset)
		, _cycleState(env->_cycleState)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* CARDLISTFLUSHTASK_HPP_ */

