
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

#if !defined(COMPACTGROUPMANAGER_HPP_)
#define COMPACTGROUPMANAGER_HPP_

#include "j9.h"
#include "j9cfg.h"

#include "AllocationContextTarok.hpp"
#include "BaseNonVirtual.hpp"
#include "GCExtensions.hpp"
#include "GlobalAllocationManagerTarok.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"


class MM_CompactGroupManager : public MM_BaseNonVirtual
{
	/* Data Members */
public:
protected:
private:

	/* Methods */
public:
	/**
	 * Computes the 0-indexed compact group index for the receiver
	 * @param env[in] The thread
	 * @param age region age to compute compact group number for
	 * @param migrationDestination The context to consider for compact group number
	 * @return A 0-indexed value which can be used to look up the compact group if it were owned by migrationDestination
	 */
	MMINLINE static UDATA getCompactGroupNumberForAge(MM_EnvironmentVLHGC *env, UDATA age, MM_AllocationContextTarok *migrationDestination)
	{
		UDATA maxAge = MM_GCExtensions::getExtensions(env)->tarokRegionMaxAge;
		Assert_MM_true(age <= maxAge);
		UDATA contextNumber = migrationDestination->getAllocationContextNumber();
		return (contextNumber * (maxAge + 1)) + age;
	}
	
	/**
	 * Computes the 0-indexed compact group index for the receiver
	 * @param env[in] The thread
	 * @param region Region to compute the compact group number for.
	 * @param migrationDestination The context to consider for compact group number instead of the owning context of the given region
	 * @return A 0-indexed value which can be used to look up the region's compact group if it were owned by migrationDestination
	 */
	MMINLINE static UDATA getCompactGroupNumberInContext(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *region, MM_AllocationContextTarok *migrationDestination)
	{
		return getCompactGroupNumberForAge(env, region->getLogicalAge(), migrationDestination);
	}

	/**
	 * Computes the 0-indexed compact group index for the receiver
	 * @param env[in] The thread
	 * @param region Region to compute the compact group number for.
	 * @return A 0-indexed value which can be used to look up the regions compact group
	 */
	MMINLINE static UDATA getCompactGroupNumber(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *region)
	{
		return getCompactGroupNumberInContext(env, region, region->_allocateData._owningContext);
	}

	/**
	 * Calculate the maximum index that can be returned for a compact group.
	 * @param env GC thread.
	 * @return a 0-indexed value which represents the maximum that can be returned as a compact group (duration of the system).
	 */
	MMINLINE static UDATA getCompactGroupMaxCount(MM_EnvironmentVLHGC *env)
	{
		MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);

		UDATA managedContextCount = MM_GlobalAllocationManagerTarok::calculateIdealManagedContextCount(extensions);
		return (extensions->tarokRegionMaxAge + 1) * managedContextCount;
	}

	/**
	 * Derive the allocation context number from the compact group number.
	 * @param env GC thread.
	 * @param compactGroupNumber the compact group number serving as the base.
	 * @return the allocation context number which comprises part of the compact group number.
	 */
	MMINLINE static UDATA getAllocationContextNumberFromGroup(MM_EnvironmentVLHGC *env, UDATA compactGroupNumber)
	{
		MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
		return compactGroupNumber / (extensions->tarokRegionMaxAge + 1);
	}

	/**
	 * Derive the region age from the compact group number.
	 * @param env GC thread.
	 * @param compactGroupNumber the compact group number serving as the base.
	 * @return the region age which comprises part of the compact group number.
	 */
	MMINLINE static UDATA getRegionAgeFromGroup(MM_EnvironmentVLHGC *env, UDATA compactGroupNumber)
	{
		MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
		return compactGroupNumber % (extensions->tarokRegionMaxAge + 1);
	}

	/**
	 * Calculate Logical age for region based on Allocation age
	 *
	 * Each next ages interval growth exponentially (specified-based)
	 * To avoid exponent/logarithm operations and as far as maximum logical age is a small constant (order of tens)
	 * it is safe to do calculation incrementally.
	 * Also it is easy to control possible overflow at each step of calculation and explicitly limit amount of work
	 *
	 * 	AgesIntervalSize(n+1) = AgesIntervalSize(n) * exponentBase;
	 * 	ThresholdForAge(n+1) = ThresholdForAge(n) + AgesIntervalSize(n+1);
	 *
	 * @param env current thread environment
	 * @param allocationAge for region
	 * @return calculated logical region age
	 */
	MMINLINE static UDATA calculateLogicalAgeForRegion(MM_EnvironmentVLHGC *env, U_64 allocationAge)
	{
		MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);

		U_64 unit = (U_64)extensions->tarokAllocationAgeUnit;
		double exponentBase = extensions->tarokAllocationAgeExponentBase;

		Assert_MM_true(unit > 0);
		Assert_MM_true(allocationAge <= extensions->tarokMaximumAgeInBytes);

		UDATA logicalAge = 0;
		bool tooLarge = false;

		U_64 threshold = unit;

		while (!tooLarge && (threshold <= allocationAge)) {
			unit = (U_64)(unit * exponentBase);
			U_64 nextThreshold = threshold + unit;

			tooLarge = (nextThreshold < threshold)									/* overflow */
					|| (logicalAge >= extensions->tarokRegionMaxAge);				/* maximum logical age reached */

			if (tooLarge) {
				logicalAge = extensions->tarokRegionMaxAge;
			} else {
				threshold = nextThreshold;
				logicalAge += 1;
			}
		}

		return logicalAge;
	}

	/**
	 * Calculate initial value for maximum allocation age in bytes based on maximumLogicalAge
	 * @param env current thread environment
	 * @param maximumLogicalAge maximum logical age allocation age calculated for
	 */
	MMINLINE static U_64 calculateMaximumAllocationAge(MM_EnvironmentVLHGC *env, UDATA maximumLogicalAge)
	{
		MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
		U_64 unit = (U_64)extensions->tarokAllocationAgeUnit;
		double exponentBase = extensions->tarokAllocationAgeExponentBase;
		Assert_MM_true(unit > 0);
		Assert_MM_true(maximumLogicalAge > 0);
		UDATA logicalAge = 1;
		bool tooLarge = false;
		U_64 allocationAge = unit;

		while (!tooLarge && (logicalAge < maximumLogicalAge)) {
			unit = (U_64)(unit * exponentBase);
			U_64 nextThreshold = allocationAge + unit;

			tooLarge = (nextThreshold < allocationAge);	/* overflow */

			if (tooLarge) {
				allocationAge = U_64_MAX;
			} else {
				allocationAge = nextThreshold;
				logicalAge += 1;
			}
		}

		return allocationAge;
	}

	/**
	 * Check is given region is a member of Nursery Collection Set
	 * Eden region is mandatory part of Nursery Collection Set regardless it's age
	 * non-Eden region is part of Nursery Collection Set if it's age is lower or equal of nursery threshold
	 * @param env[in] The thread
	 * @param region given region
	 * @return true if region is a member of Nursery Collection Set
	 */
	MMINLINE static bool isRegionInNursery(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *region)
	{
		MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
		bool result = false;

		if (extensions->tarokAllocationAgeEnabled) {
			result = region->isEden() || (region->getAllocationAge() <= extensions->tarokMaximumNurseryAgeInBytes);
		} else {
			result = region->isEden() || (region->getLogicalAge() <= extensions->tarokNurseryMaxAge._valueSpecified);
		}

		return result;
	}

	/**
	 * Check is given region is a DCSS candidate
	 * It is true if it is not a member of Nursery Collection Set and it's age has not reached maximum yet
	 * @param env[in] The thread
	 * @param region given region
	 * @return true if region is a DCSS candidate
	 */
	MMINLINE static bool isRegionDCSSCandidate(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *region)
	{
		MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
		bool result = false;

		if (extensions->tarokAllocationAgeEnabled) {
			result = !isRegionInNursery(env, region) && (region->getAllocationAge() < extensions->tarokMaximumAgeInBytes);
		} else {
			result = !isRegionInNursery(env, region) && (region->getLogicalAge() < extensions->tarokRegionMaxAge);
		}

		return result;
	}

	/**
	 * Check is given Compact Group is a DCSS candidate
	 * It is true if it is not a member of Nursery Collection Set and it's age has not reached maximum yet
	 * @param env[in] The thread
	 * @param compactGroupNumber given compact group number
	 * @return true if region is a DCSS candidate
	 */
	MMINLINE static bool isCompactGroupDCSSCandidate(MM_EnvironmentVLHGC *env, UDATA compactGroupNumber)
	{
		MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
		bool result = false;
		UDATA ageOfGroup = getRegionAgeFromGroup(env, compactGroupNumber);

		/*
		 * It is no special case for allocation-based aging system because
		 * tarokNurseryMaxAge and tarokRegionMaxAge were set at startup
		 */
		result = (ageOfGroup > extensions->tarokNurseryMaxAge._valueSpecified) && (ageOfGroup < extensions->tarokRegionMaxAge);

		return result;
	}

protected:
private:
	MM_CompactGroupManager()
	{
		_typeId = __FUNCTION__;
	}

};

#endif /* COMPACTGROUPMANAGER_HPP_ */
