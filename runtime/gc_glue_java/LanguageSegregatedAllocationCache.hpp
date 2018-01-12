/*******************************************************************************
 * Copyright (c) 2014, 2014 IBM Corp. and others
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

#ifndef LANGUAGESEGREGATEDALLOCATIONCACHE_HPP_
#define LANGUAGESEGREGATEDALLOCATIONCACHE_HPP_

#include "j9generated.h"
#include "modronbase.h"
#include "omr.h"
#include "sizeclasses.h"

typedef J9VMGCSegregatedAllocationCacheEntry LanguageSegregatedAllocationCacheEntryStruct;

typedef LanguageSegregatedAllocationCacheEntryStruct LanguageSegregatedAllocationCache[OMR_SIZECLASSES_NUM_SMALL + 1];

class MM_LanguageSegregatedAllocationCache {

public:
	MMINLINE LanguageSegregatedAllocationCacheEntryStruct *
	getLanguageSegregatedAllocationCacheStruct(MM_EnvironmentBase *env)
	{
		return reinterpret_cast<LanguageSegregatedAllocationCacheEntryStruct*>((UDATA)env->getLanguageVMThread() + J9_VMTHREAD_SEGREGATED_ALLOCATION_CACHE_OFFSET);
	}
};

#endif /* LANGUAGESEGREGATEDALLOCATIONCACHE_HPP_ */
