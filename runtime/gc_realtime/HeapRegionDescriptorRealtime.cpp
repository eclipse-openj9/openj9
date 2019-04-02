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

#include "EnvironmentBase.hpp"

#include "HeapRegionDescriptorRealtime.hpp"

#if defined(OMR_GC_REALTIME)

MM_HeapRegionDescriptorRealtime::MM_HeapRegionDescriptorRealtime(MM_EnvironmentBase *env, void *lowAddress, void *highAddress)
	: MM_HeapRegionDescriptorSegregated(env, lowAddress, highAddress)
{
	_arrayletBackPointers = ((uintptr_t **)(this + 1));
	_typeId = __FUNCTION__;
}

bool
MM_HeapRegionDescriptorRealtime::initialize(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager)
{
	if (!MM_HeapRegionDescriptorSegregated::initialize(env, regionManager)) {
		return false;
	}
	
	_nextOverflowedRegion = NULL;

	return true;
}

bool 
MM_HeapRegionDescriptorRealtime::initializer(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager, MM_HeapRegionDescriptor *descriptor, void *lowAddress, void *highAddress)
{
	new((MM_HeapRegionDescriptorRealtime*)descriptor) MM_HeapRegionDescriptorRealtime(env, lowAddress, highAddress);
	return ((MM_HeapRegionDescriptorRealtime*)descriptor)->initialize(env, regionManager);
}

void 
MM_HeapRegionDescriptorRealtime::destructor(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager, MM_HeapRegionDescriptor *descriptor)
{
	((MM_HeapRegionDescriptorRealtime*)descriptor)->tearDown(env);
}

#endif /* OMR_GC_REALTIME */
