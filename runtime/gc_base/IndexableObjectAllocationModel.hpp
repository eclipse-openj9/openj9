
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#if !defined(INDEXABLEOBJECTALLOCATIONMODEL_HPP_)
#define INDEXABLEOBJECTALLOCATIONMODEL_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"
#include "objectdescription.h"
#include "ModronAssertions.h"

#include "ArrayletObjectModel.hpp"
#include "JavaObjectAllocationModel.hpp"
#include "MemorySpace.hpp"

class MM_HeapRegionDescriptorVLHGC;

/**
 * Class definition for the array object allocation model.
 */
class MM_IndexableObjectAllocationModel : public MM_JavaObjectAllocationModel
{
	/*
	 * Member data and types
	 */
private:
	const uint32_t _numberOfIndexedFields;
	const uintptr_t _dataSize;
	const GC_ArrayletObjectModel::ArrayLayout _layout;
	const bool _alignSpineDataSection;
	const uintptr_t _numberOfArraylets;

protected:

public:

	/*
	 * Member functions
	 */
private:
	/**
	 * For contiguous arraylet all data is subsumed into the spine.
	 *
	 * @param env thread GC Environment
	 * @param spine indexable object spine
	 *
	 * @return initialized arraylet spine with its arraylet pointers initialized.
	 */
	MMINLINE J9IndexableObject *layoutContiguousArraylet(MM_EnvironmentBase *env, J9IndexableObject *spine);

	/**
	 * For non-contiguous arraylet (i.e. discontiguous and hybrid), perform separate allocations
	 * for spine and leaf data. The spine and attached leaves may move as each leaf is allocated
	 * is GC is allowed. The final location of the spine is returned.
	 *
	 * @param env thread GC Environment
	 * @param spine indexable object spine
	 *
	 * @return initialized arraylet spine with its arraylet pointers initialized.
	 */
	MMINLINE J9IndexableObject *layoutDiscontiguousArraylet(MM_EnvironmentBase *env, J9IndexableObject *spine);

#if defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION)
	/**
	 * For discontiguous arraylet that will be allocated to sparse heap (off-heap). Even though this arraylet is large enough to be
	 * discontiguous, its true layout is InlineContiguous. Arraylet leaves still need to be created and initialized,
	 * even though they won't be used.
	 *
	 * @param env thread GC Environment
	 * @param spine indexable object spine
	 *
	 * @return initialized arraylet spine with its arraylet pointers initialized
	 */
	MMINLINE J9IndexableObject *getSparseAddressAndDecommitLeaves(MM_EnvironmentBase *env, J9IndexableObject *spine);
#endif /* defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION) */

protected:

public:
	MM_IndexableObjectAllocationModel(MM_EnvironmentBase *env,
			J9Class *clazz,
			uint32_t numberOfIndexedFields,
			uintptr_t allocateObjectFlags = 0
	)
		: MM_JavaObjectAllocationModel(env, clazz, allocation_category_indexable,
				0, allocateObjectFlags | OMR_GC_ALLOCATE_OBJECT_INDEXABLE)
		, _numberOfIndexedFields(numberOfIndexedFields)
		, _dataSize(env->getExtensions()->indexableObjectModel.getDataSizeInBytes(_class, _numberOfIndexedFields))
		, _layout(env->getExtensions()->indexableObjectModel.getArrayletLayout(_class, _numberOfIndexedFields,
				_allocateDescription.getMemorySpace()->getDefaultMemorySubSpace()->largestDesirableArraySpine()))
		, _alignSpineDataSection(env->getExtensions()->indexableObjectModel.shouldAlignSpineDataSection(_class))
		, _numberOfArraylets(env->getExtensions()->indexableObjectModel.numArraylets(_dataSize))
	{
		/* check for overflow of _dataSize in indexableObjectModel.getDataSizeInBytes() */
		if (J9_MAXIMUM_INDEXABLE_DATA_SIZE < _dataSize) {
			J9VMThread *vmThread = (J9VMThread *)env->getLanguageVMThread();
			switch (J9GC_CLASS_SHAPE(_class)) {
			case OBJECT_HEADER_SHAPE_BYTES:
				break;
			case OBJECT_HEADER_SHAPE_WORDS:
				Trc_MM_ShortArrayAllocationFailedDueToOverflow(vmThread, _numberOfIndexedFields);
				break;
			case OBJECT_HEADER_SHAPE_LONGS:
				Trc_MM_IntArrayAllocationFailedDueToOverflow(vmThread, _numberOfIndexedFields);
				break;
			case OBJECT_HEADER_SHAPE_DOUBLES:
				Trc_MM_DoubleArrayAllocationFailedDueToOverflow(vmThread, _numberOfIndexedFields);
				break;
			case OBJECT_HEADER_SHAPE_POINTERS:
				Trc_MM_ObjectArrayAllocationFailedDueToOverflow(vmThread, _numberOfIndexedFields);
				break;
			default:
				Assert_MM_unreachable();
				break;
			}
			setAllocatable(false);
		}
	}


	MMINLINE uint32_t getNumberOfIndexedFields() { return _numberOfIndexedFields; }

	MMINLINE uintptr_t getNumberOfArraylets() { return _numberOfArraylets; }

	/**
	 * Allocation description and layout initialization.
	 */
	bool initializeAllocateDescription(MM_EnvironmentBase *env);

	/**
	 * Initializer.
	 */
	omrobjectptr_t initializeIndexableObject(MM_EnvironmentBase *env, void *allocatedBytes);
};

#endif /* INDEXABLEOBJECTALLOCATIONMODEL_HPP_ */
