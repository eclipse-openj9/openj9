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

#include "omr.h"
#include "omrcfg.h"

#include "EnvironmentRealtime.hpp"
#include "GCExtensionsBase.hpp"
#include "HeapRegionQueue.hpp"
#include "OSInterface.hpp"
#include "RealtimeGC.hpp"
#include "RealtimeRootScanner.hpp"
#include "SegregatedAllocationTracker.hpp"
#include "Timer.hpp"

MM_EnvironmentRealtime *
MM_EnvironmentRealtime::newInstance(MM_GCExtensionsBase *extensions, OMR_VMThread *omrVMThread)
{
	void *envPtr;
	MM_EnvironmentRealtime *env = NULL;
	
	envPtr = (void *)pool_newElement(extensions->environments);
	if (envPtr) {
		if (omrVMThread) {
			env = new(envPtr) MM_EnvironmentRealtime(omrVMThread);
		} else {
			env = new(envPtr) MM_EnvironmentRealtime(extensions->getOmrVM());
		}
		if (!env->initialize(extensions)) {
			env->kill();
			env = NULL;	
		}
	}	

	return env;
}

void
MM_EnvironmentRealtime::kill()
{
	MM_EnvironmentBase::kill();
}

bool
MM_EnvironmentRealtime::initialize(MM_GCExtensionsBase *extensions)
{
	/* initialize base class */
	if(!MM_EnvironmentBase::initialize(extensions)) {
		return false;
	}
	
	if (NULL == (_timer = MM_Timer::newInstance(this, _osInterface))) {
		return false;
	}
	
	_monitorCacheCleared = FALSE;
	
	_distanceToYieldTimeCheck = extensions->distanceToYieldTimeCheck;

	_overflowCache = (MM_HeapRegionDescriptorRealtime**)getForge()->allocate(sizeof(MM_HeapRegionDescriptorRealtime *) * extensions->overflowCacheCount, MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL == _overflowCache) {
		return false;
	}

	_yieldDisableDepth = 0;

	return true;
}

void
MM_EnvironmentRealtime::tearDown(MM_GCExtensionsBase *extensions)
{
	if (_overflowCache != NULL) {
		getForge()->free(_overflowCache);
		_overflowCache = NULL;
	}
	if (NULL != _timer) {
		_timer->kill(this);
		_timer = NULL;
	}
	
	/* tearDown base class */
	MM_EnvironmentBase::tearDown(extensions);
}

void MM_EnvironmentRealtime::disableYield()
{
	_yieldDisableDepth++;
}

void MM_EnvironmentRealtime::enableYield()
{
	_yieldDisableDepth--;
	assert1(_yieldDisableDepth >= 0);
}

void
MM_EnvironmentRealtime::reportScanningSuspended() {
	if (NULL != _rootScanner) {
		_rootScanner->reportScanningSuspended();
	}
}

void
MM_EnvironmentRealtime::reportScanningResumed() {
	if (NULL != _rootScanner) {
		_rootScanner->reportScanningResumed();
	}
}
