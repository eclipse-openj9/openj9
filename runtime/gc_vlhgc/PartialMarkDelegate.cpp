
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
 * @ingroup GC_Modron_Tarok
 */

#include "j9.h"
#include "j9cfg.h"
#include "j9port.h"
#include "ModronAssertions.h"

#include "PartialMarkDelegate.hpp"

#include "CardTable.hpp"
#include "ClassLoaderIterator.hpp"
#include "ClassLoaderManager.hpp"
#include "CycleState.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentVLHGC.hpp"
#include "FinalizeListManager.hpp"
#include "FinalizerSupport.hpp"
#include "GCExtensions.hpp"
#include "GlobalMarkCardScrubber.hpp"
#include "HeapMapIterator.hpp"
#include "InterRegionRememberedSet.hpp"
#include "MarkMap.hpp"
#include "PartialMarkingScheme.hpp"
#include "WorkPacketsVLHGC.hpp"


bool
MM_PartialMarkDelegate::initialize(MM_EnvironmentVLHGC *env)
{
	_javaVM = (J9JavaVM *)env->getLanguageVM();
	_extensions = MM_GCExtensions::getExtensions(env);

	if(NULL == (_markingScheme = MM_PartialMarkingScheme::newInstance(env))) {
		goto error_no_memory;
	}

	_dispatcher = _extensions->dispatcher;

	return true;

error_no_memory:
	return false;
}

void
MM_PartialMarkDelegate::tearDown(MM_EnvironmentVLHGC *env)
{
	_dispatcher = NULL;

	if(NULL != _markingScheme) {
		_markingScheme->kill(env);
		_markingScheme = NULL;
	}
}

bool
MM_PartialMarkDelegate::heapAddRange(MM_EnvironmentVLHGC *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress)
{
	return _markingScheme->heapAddRange(env, subspace, size, lowAddress, highAddress);
}

bool
MM_PartialMarkDelegate::heapRemoveRange(MM_EnvironmentVLHGC *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress)
{
	return _markingScheme->heapRemoveRange(env, subspace, size, lowAddress, highAddress, lowValidAddress, highValidAddress);
}

void 
MM_PartialMarkDelegate::performMarkForPartialGC(MM_EnvironmentVLHGC *env)
{
	Assert_MM_true(MM_CycleState::state_mark_idle == env->_cycleState->_markDelegateState);

	/* provide complete mark operation */
	markAll(env);

	env->_cycleState->_markDelegateState = MM_CycleState::state_mark_idle;
}

void
MM_PartialMarkDelegate::markAll(MM_EnvironmentVLHGC *env)
{
	_markingScheme->masterSetupForGC(env);

	/* run the mark */
	MM_ParallelPartialMarkTask markTask(env, _dispatcher, _markingScheme, env->_cycleState);
	_dispatcher->run(env, &markTask);

	/* Do any post mark checks */
	_markingScheme->masterCleanupAfterGC(env);
}

void 
MM_PartialMarkDelegate::postMarkCleanup(MM_EnvironmentVLHGC *env)
{
}
