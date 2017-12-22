
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
 * @ingroup GC_Modron_Base
 */

#ifndef REGIONVALIDATOR_HPP_
#define REGIONVALIDATOR_HPP_

#include "j9.h"
#include "HeapRegionDescriptorVLHGC.hpp"
#include "Validator.hpp"

class MM_EnvironmentBase;

class MM_RegionValidator : public MM_Validator {
/* data members */
private:
	MM_HeapRegionDescriptorVLHGC *_region; /**< The region being validated */
protected:
public:
	
/* function members */
private:
	/**
	 * Report detailed information about the current region.
	 * @param env[in] the current thread
	 * @param message[in] a message to include with the report
	 */
	void reportRegion(MM_EnvironmentBase* env, const char* message);

protected:
public:
	virtual void threadCrash(MM_EnvironmentBase* env);

	/**
	 * Construct a new RegionValidator
	 * @param region[in] the region to be validated
	 */
	MM_RegionValidator(MM_HeapRegionDescriptorVLHGC *region)
		: MM_Validator()
		, _region(region)
	{
		_typeId = __FUNCTION__;
	}

	/**
	 * Performs light-weight verification on the region given to check for common symptoms of problems left behind
	 * by the VM.  For example:  ensures that the first object in a region is a valid object and that arraylet leaf
	 * meta-data is consistent.  Fails an assertion if something is wrong.
	 * @param env[in] The current GC thread
	 * @return true if the region is valid, false on error
	 */
	bool validate(MM_EnvironmentBase *env);
};

#endif /* REGIONVALIDATOR_HPP_ */
