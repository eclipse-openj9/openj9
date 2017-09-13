
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#if !defined(HEAPREGIONDESCRIPTORREALTIME_HPP)
#define HEAPREGIONDESCRIPTORREALTIME_HPP

#include "j9.h"
#include "j9cfg.h"

#include "HeapRegionDescriptorSegregated.hpp"
#include "OwnableSynchronizerObjectList.hpp"
#include "ReferenceObjectList.hpp"
#include "UnfinalizedObjectList.hpp"

#if defined(J9VM_GC_REALTIME)

class MM_EnvironmentBase;

class MM_HeapRegionDescriptorRealtime : public MM_HeapRegionDescriptorSegregated
{

	/*
	 * Data Members
	 */
private:
	MM_HeapRegionDescriptorRealtime *_nextOverflowedRegion;

public:
	MM_HeapRegionDescriptorRealtime(MM_EnvironmentBase *env, void *lowAddress, void *highAddress);

	bool initialize(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager);
	
	static bool initializer(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager, MM_HeapRegionDescriptor *descriptor, void *lowAddress, void *highAddress);
	static void destructor(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager, MM_HeapRegionDescriptor *descriptor);
	static UDATA getUnfinalizedObjectListCount(MM_EnvironmentBase *env) {return MM_GCExtensions::getExtensions(env)->gcThreadCount;}
	static UDATA getOwnableSynchronizerObjectListCount(MM_EnvironmentBase *env) {return MM_GCExtensions::getExtensions(env)->gcThreadCount;}
	static UDATA getReferenceObjectListCount(MM_EnvironmentBase *env) {return MM_GCExtensions::getExtensions(env)->gcThreadCount;}

	MM_HeapRegionDescriptorRealtime *getNextOverflowRegion() {return _nextOverflowedRegion;}
	void setNextOverflowRegion(MM_HeapRegionDescriptorRealtime *region) {_nextOverflowedRegion = region;}
};

#endif /* J9VM_GC_REALTIME */

#endif /* HEAPREGIONDESCRIPTORREALTIME_HPP */
