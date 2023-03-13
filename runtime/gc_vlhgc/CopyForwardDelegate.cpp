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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "j9.h"
#include "j9cfg.h"

#include "CopyForwardDelegate.hpp"

#include "ClassLoaderManager.hpp"
#include "CompactGroupManager.hpp"
#include "CompactGroupPersistentStats.hpp"
#include "CycleState.hpp"
#include "FinalizerSupport.hpp"
#include "HeapRegionIteratorVLHGC.hpp"
#include "HeapRegionManager.hpp"
#include "MarkMap.hpp"
#include "MemorySubSpace.hpp"
#include "ObjectAllocationInterface.hpp"

MM_CopyForwardDelegate::MM_CopyForwardDelegate(MM_EnvironmentVLHGC *env)
	: _javaVM((J9JavaVM *)env->getLanguageVM())
	, _extensions(MM_GCExtensions::getExtensions(env))
	, _breadthFirstCopyForwardScheme(NULL)
{
	_typeId = __FUNCTION__;
}

bool
MM_CopyForwardDelegate::initialize(MM_EnvironmentVLHGC *env)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	_breadthFirstCopyForwardScheme = MM_CopyForwardScheme::newInstance(env, extensions->heapRegionManager);

	return (NULL != _breadthFirstCopyForwardScheme);
}

void
MM_CopyForwardDelegate::tearDown(MM_EnvironmentVLHGC *env)
{
	if (NULL != _breadthFirstCopyForwardScheme) {
		_breadthFirstCopyForwardScheme->kill(env);
		_breadthFirstCopyForwardScheme = NULL;
	}
}

void
MM_CopyForwardDelegate::performCopyForwardForPartialGC(MM_EnvironmentVLHGC *env)
{
#if defined(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD)
	if (_extensions->isConcurrentCopyForwardEnabled())
	{
		_breadthFirstCopyForwardScheme->concurrentCopyForwardCollectionSet(env);
	} else
#endif /* defined(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD) */
	{
		_breadthFirstCopyForwardScheme->copyForwardCollectionSet(env);
	}
}

void
MM_CopyForwardDelegate::preCopyForwardSetup(MM_EnvironmentVLHGC *env)
{
}

void
MM_CopyForwardDelegate::postCopyForwardCleanup(MM_EnvironmentVLHGC *env)
{
	/* Restart the allocation caches associated to all threads */
	{
		GC_VMThreadListIterator vmThreadListIterator(_javaVM);
		J9VMThread *walkThread;
		while((walkThread = vmThreadListIterator.nextVMThread()) != NULL) {
			MM_EnvironmentBase *walkEnv = MM_EnvironmentBase::getEnvironment(walkThread->omrVMThread);
			walkEnv->_objectAllocationInterface->restartCache(env);
		}
	}
}


UDATA
MM_CopyForwardDelegate::estimateRequiredSurvivorBytes(MM_EnvironmentVLHGC *env)
{
	UDATA estimatedSurvivorRequired = 0;
	MM_HeapRegionManager *const regionManager = _extensions->heapRegionManager;
	MM_CompactGroupPersistentStats *const persistentStats = _extensions->compactGroupPersistentStats;
	GC_HeapRegionIteratorVLHGC regionIterator(regionManager, MM_HeapRegionDescriptor::MANAGED);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->_markData._shouldMark) {
			UDATA compactGroup = MM_CompactGroupManager::getCompactGroupNumber(env, region);
			double survivalRate = persistentStats[compactGroup]._historicalSurvivalRate;
			UDATA freeMemory =  region->getMemoryPool()->getFreeMemoryAndDarkMatterBytes();
			estimatedSurvivorRequired += (UDATA)((double)(region->getSize() - freeMemory) * survivalRate);
		}
	}
	return estimatedSurvivorRequired;
}
