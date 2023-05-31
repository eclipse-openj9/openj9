/*******************************************************************************
 * Copyright IBM Corp. and others 2023
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef HEAPREGIONDESCRIPTORSTANDARDEXTENSION_HPP_
#define HEAPREGIONDESCRIPTORSTANDARDEXTENSION_HPP_

#include "EnvironmentBase.hpp"

#include "OwnableSynchronizerObjectList.hpp"
#include "ContinuationObjectList.hpp"
#include "ReferenceObjectList.hpp"
#include "UnfinalizedObjectList.hpp"

class MM_HeapRegionDescriptorStandardExtension : public MM_BaseNonVirtual
{
/*
 * Member data and types
 */
private:
protected:
public:
	uintptr_t _maxListIndex; /**< Max index for _*ObjectLists[index] */
	MM_UnfinalizedObjectList *_unfinalizedObjectLists; /**< An array of lists of unfinalized objects in this region */
	MM_OwnableSynchronizerObjectList *_ownableSynchronizerObjectLists; /**< An array of lists of ownable synchronizer objects in this region */
	MM_ContinuationObjectList *_continuationObjectLists; /**< An array of lists of continuation objects in this region */
	MM_ReferenceObjectList *_referenceObjectLists; /**< An array of lists of reference objects (i.e. weak/soft/phantom) in this region */

/*
 * Member functions
 */
private:
protected:
public:
	static MM_HeapRegionDescriptorStandardExtension *
	newInstance(MM_EnvironmentBase *env, uintptr_t listCount)
	{
		MM_HeapRegionDescriptorStandardExtension *heapRegionDescriptorStandardExtension = NULL;

		heapRegionDescriptorStandardExtension = (MM_HeapRegionDescriptorStandardExtension *)env->getForge()->allocate(sizeof(MM_HeapRegionDescriptorStandardExtension), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
		if (NULL != heapRegionDescriptorStandardExtension) {
			new(heapRegionDescriptorStandardExtension) MM_HeapRegionDescriptorStandardExtension(listCount);
			if (!heapRegionDescriptorStandardExtension->initialize(env)) {
				heapRegionDescriptorStandardExtension->kill(env);
				heapRegionDescriptorStandardExtension = NULL;
			}
		}
		return heapRegionDescriptorStandardExtension;
	}

	bool
	initialize(MM_EnvironmentBase *env)
	{
		if (NULL == (_unfinalizedObjectLists = MM_UnfinalizedObjectList::newInstanceArray(env, _maxListIndex))) {
			return false;
		}

		if (NULL == (_ownableSynchronizerObjectLists = MM_OwnableSynchronizerObjectList::newInstanceArray(env, _maxListIndex))) {
			return false;
		}

		if (NULL == (_continuationObjectLists = MM_ContinuationObjectList::newInstanceArray(env, _maxListIndex))) {
			return false;
		}

		if (NULL == (_referenceObjectLists = MM_ReferenceObjectList::newInstanceArray(env, _maxListIndex))) {
			return false;
		}

		return true;
	}

	void
	kill(MM_EnvironmentBase *env)
	{
		tearDown(env);
		env->getForge()->free(this);
	}

	void
	tearDown(MM_EnvironmentBase *env)
	{
		if (NULL != _unfinalizedObjectLists) {
			env->getForge()->free(_unfinalizedObjectLists);
			_unfinalizedObjectLists = NULL;
		}

		if (NULL != _ownableSynchronizerObjectLists) {
			env->getForge()->free(_ownableSynchronizerObjectLists);
			_ownableSynchronizerObjectLists = NULL;
		}

		if (NULL != _continuationObjectLists) {
			env->getForge()->free(_continuationObjectLists);
			_continuationObjectLists = NULL;
		}

		if (NULL != _referenceObjectLists) {
			env->getForge()->free(_referenceObjectLists);
			_referenceObjectLists = NULL;
		}
	}

	MM_HeapRegionDescriptorStandardExtension(uintptr_t listCount) :
		MM_BaseNonVirtual()
		, _maxListIndex(listCount)
		, _unfinalizedObjectLists(NULL)
		, _ownableSynchronizerObjectLists(NULL)
		, _continuationObjectLists(NULL)
		, _referenceObjectLists(NULL)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* HEAPREGIONDESCRIPTORSTANDARDEXTENSION_HPP_ */
