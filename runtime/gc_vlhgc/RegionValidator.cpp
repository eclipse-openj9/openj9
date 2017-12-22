
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

#include "j9cfg.h"
#include "j9.h"
#include "modron.h"
#include "rommeth.h"
#include "ModronAssertions.h"

#include "RegionValidator.hpp"

#include "CycleState.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "HeapMapWordIterator.hpp"
#include "HeapRegionManager.hpp"
#include "MarkMap.hpp"
#include "MemoryPoolBumpPointer.hpp"

void 
MM_RegionValidator::threadCrash(MM_EnvironmentBase* env)
{
	reportRegion(env, "Unhandled exception while validating region");
}

void
MM_RegionValidator::reportRegion(MM_EnvironmentBase* env, const char* message)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_RegionValidator_reportRegion_Entry(env->getLanguageVMThread(), _region);

	MM_HeapRegionDescriptor::RegionType regionType = _region->getRegionType();
	if (_region->isArrayletLeaf()) {
		j9tty_printf(PORTLIB, "ERROR: %s in region %p; type=%zu; range=%p-%p; spine=%p\n", message, _region, (UDATA)regionType, _region->getLowAddress(), _region->getHighAddress(), _region->_allocateData.getSpine());
		Trc_MM_RegionValidator_leafRegion(env->getLanguageVMThread(), message, _region, regionType, _region->getLowAddress(), _region->getHighAddress(), _region->_allocateData.getSpine());
	} else {
		j9tty_printf(PORTLIB, "ERROR: %s in region %p; type=%zu; range=%p-%p\n", message, _region, (UDATA)regionType, _region->getLowAddress(), _region->getHighAddress());
		Trc_MM_RegionValidator_objectRegion(env->getLanguageVMThread(), message, _region, regionType, _region->getLowAddress(), _region->getHighAddress());
	}
	
	MM_HeapRegionManager* regionManager = MM_GCExtensions::getExtensions(env)->getHeap()->getHeapRegionManager();
	UDATA thisIndex = regionManager->mapDescriptorToRegionTableIndex(_region);
	if (thisIndex > 0) {
		MM_HeapRegionDescriptorVLHGC *previousRegion = (MM_HeapRegionDescriptorVLHGC*)regionManager->tableDescriptorForIndex(thisIndex - 1);
		MM_HeapRegionDescriptor::RegionType previousRegionType = previousRegion->getRegionType();
		if (previousRegion->isArrayletLeaf()) {
			j9tty_printf(PORTLIB, "ERROR: (Previous region %p; type=%zu; range=%p-%p; spine=%p)\n", previousRegion, (UDATA)previousRegionType, previousRegion->getLowAddress(), previousRegion->getHighAddress(), previousRegion->_allocateData.getSpine());
			Trc_MM_RegionValidator_previousLeafRegion(env->getLanguageVMThread(), previousRegion, previousRegionType, previousRegion->getLowAddress(), previousRegion->getHighAddress(), previousRegion->_allocateData.getSpine());
		} else {
			j9tty_printf(PORTLIB, "ERROR: (Previous region %p; type=%zu; range=%p-%p)\n", previousRegion, (UDATA)previousRegionType, previousRegion->getLowAddress(), previousRegion->getHighAddress());
			Trc_MM_RegionValidator_previousObjectRegion(env->getLanguageVMThread(), previousRegion, previousRegionType, previousRegion->getLowAddress(), previousRegion->getHighAddress());
		}
	}
	
	Trc_MM_RegionValidator_reportRegion_Exit(env->getLanguageVMThread());
}

bool 
MM_RegionValidator::validate(MM_EnvironmentBase *env) 
{ 
	const UDATA EYECATCHER = (UDATA)0x99669966;
	bool result = true;
	env->_activeValidator = this;
	
	MM_HeapRegionDescriptor::RegionType regionType = _region->getRegionType();
	if (MM_HeapRegionDescriptor::BUMP_ALLOCATED == regionType) {
		/* verify that the region starts with either a valid hole, an object, or the allocate pointer hasn't advanced */
		MM_MemoryPoolBumpPointer *pool = (MM_MemoryPoolBumpPointer *)_region->getMemoryPool();
		void *lowAddress = _region->getLowAddress();
		if (pool->getAllocationPointer() > lowAddress) {
			/* something is here */
			J9Object *firstObject = (J9Object *)lowAddress;
			if (!MM_GCExtensions::getExtensions(env)->objectModel.isDeadObject(firstObject)) {
				J9Class *clazz = J9GC_J9OBJECT_CLAZZ(firstObject);
				if (NULL == clazz) {
					reportRegion(env, "NULL class in first object");
					result = false;
				} else if (EYECATCHER != clazz->eyecatcher) {
					reportRegion(env, "Invalid class in first object");
					result = false;
				}
			}
		}
	} else if (MM_HeapRegionDescriptor::BUMP_ALLOCATED_MARKED == regionType) {
		/* verify that the first object in this region is valid (checks against the previous region overflowing into this one) */
		MM_HeapMapWordIterator firstWordIterator(MM_GCExtensions::getExtensions(env)->previousMarkMap, _region->getLowAddress());
		J9Object *firstObject = firstWordIterator.nextObject();
		if (NULL != firstObject) {
			J9Class *clazz = J9GC_J9OBJECT_CLAZZ(firstObject);
			if (NULL == clazz) {
				reportRegion(env, "NULL class in first marked object");
				result = false;
			} else if (EYECATCHER != clazz->eyecatcher) {
				reportRegion(env, "Invalid class in first marked object");
				result = false;
			}
		}
	} else if (_region->isArrayletLeaf()) {
		/* Do a quick check to ensure that arraylets look reasonable before the collection to help debug problems like CMVC 174687 */
		if (NULL == _region->_allocateData.getSpine()) {
			reportRegion(env, "NULL spine object");
			result = false;
		} else if (EYECATCHER != J9GC_J9OBJECT_CLAZZ(_region->_allocateData.getSpine())->eyecatcher) {
			reportRegion(env, "Invalid spine object");
			result = false;
		}
	}
	
	env->_activeValidator = NULL;
	return result;
}

