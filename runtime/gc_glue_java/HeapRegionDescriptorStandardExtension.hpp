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
		if ((NULL == (_unfinalizedObjectLists = MM_UnfinalizedObjectList::newInstanceArray(env, _maxListIndex, NULL, 0)))
			|| (NULL == (_ownableSynchronizerObjectLists = MM_OwnableSynchronizerObjectList::newInstanceArray(env, _maxListIndex, NULL, 0)))
			|| (NULL == (_continuationObjectLists = MM_ContinuationObjectList::newInstanceArray(env, _maxListIndex, NULL, 0)))
			|| (NULL == (_referenceObjectLists = MM_ReferenceObjectList::newInstanceArray(env, _maxListIndex, NULL, 0)))
		) {
			tearDown(env);
			return false;
		}

		return true;
	}

#if defined(J9VM_OPT_CRIU_SUPPORT)
	bool
	reinitializeForRestore(MM_EnvironmentBase* env)
	{
		MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);

		MM_UnfinalizedObjectList *newUnfinalizedObjectLists = NULL;
		MM_OwnableSynchronizerObjectList *newOwnableSynchronizerObjectLists = NULL;
		MM_ContinuationObjectList *newContinuationObjectLists = NULL;
		MM_ReferenceObjectList *newReferenceObjectLists = NULL;

		uintptr_t newListCount = extensions->gcThreadCount;

		Assert_MM_true(_maxListIndex > 0);

		if (newListCount > _maxListIndex) {
			if ((NULL == (newUnfinalizedObjectLists = MM_UnfinalizedObjectList::newInstanceArray(env, newListCount, _unfinalizedObjectLists, _maxListIndex)))
				|| (NULL == (newOwnableSynchronizerObjectLists = MM_OwnableSynchronizerObjectList::newInstanceArray(env, newListCount, _ownableSynchronizerObjectLists, _maxListIndex)))
				|| (NULL == (newContinuationObjectLists = MM_ContinuationObjectList::newInstanceArray(env, newListCount, _continuationObjectLists, _maxListIndex)))
				|| (NULL == (newReferenceObjectLists = MM_ReferenceObjectList::newInstanceArray(env, newListCount, _referenceObjectLists, _maxListIndex)))
			) {
				goto failed;
			}

			tearDown(env);

			_unfinalizedObjectLists = newUnfinalizedObjectLists;
			_ownableSynchronizerObjectLists = newOwnableSynchronizerObjectLists;
			_continuationObjectLists = newContinuationObjectLists;
			_referenceObjectLists = newReferenceObjectLists;

			_maxListIndex = newListCount;
		}

		return true;

failed:
		releaseLists(env, &newUnfinalizedObjectLists, &newOwnableSynchronizerObjectLists, &newContinuationObjectLists, &newReferenceObjectLists);

		return false;
	}
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

	void
	kill(MM_EnvironmentBase *env)
	{
		tearDown(env);
		env->getForge()->free(this);
	}

	void
	tearDown(MM_EnvironmentBase *env)
	{
		releaseLists(env, &_unfinalizedObjectLists, &_ownableSynchronizerObjectLists, &_continuationObjectLists, &_referenceObjectLists);
	}

	void
	releaseLists(MM_EnvironmentBase *env, MM_UnfinalizedObjectList **unfinalizedObjectLists, MM_OwnableSynchronizerObjectList **ownableSynchronizerObjectLists, MM_ContinuationObjectList **continuationObjectLists, MM_ReferenceObjectList **referenceObjectLists)
	{
		if (NULL != *unfinalizedObjectLists) {
			env->getForge()->free(*unfinalizedObjectLists);
			*unfinalizedObjectLists = NULL;
		}

		if (NULL != *ownableSynchronizerObjectLists) {
			env->getForge()->free(*ownableSynchronizerObjectLists);
			*ownableSynchronizerObjectLists = NULL;
		}

		if (NULL != *continuationObjectLists) {
			env->getForge()->free(*continuationObjectLists);
			*continuationObjectLists = NULL;
		}

		if (NULL != *referenceObjectLists) {
			env->getForge()->free(*referenceObjectLists);
			*referenceObjectLists = NULL;
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
