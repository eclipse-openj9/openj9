/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#if !defined(METRONOMEARRAYOBJECTSCANNER_HPP_)
#define METRONOMEARRAYOBJECTSCANNER_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"

#include "ArrayObjectModel.hpp"
#include "GCExtensionsBase.hpp"
#include "IndexableObjectScanner.hpp"

/**
 * This class is used to iterate over the slots of a Java pointer array.
 */
class GC_MetronomeArrayObjectScanner : public GC_IndexableObjectScanner
{
	/* Data Members */
private:
	fomrobject_t *_mapPtr;	/**< pointer to first array element in current scan segment */

protected:

public:

	/* Methods */
private:

protected:
	/**
	 * @param env The scanning thread environment
	 * @param[in] arrayPtr pointer to the array to be processed
	 * @param[in] basePtr pointer to the first contiguous array cell
	 * @param[in] limitPtr pointer to end of last contiguous array cell
	 * @param[in] scanPtr pointer to the array cell where scanning will start
	 * @param[in] endPtr pointer to the array cell where scanning will stop
	 * @param[in] flags Scanning context flags
	 */
	MMINLINE GC_MetronomeArrayObjectScanner(MM_EnvironmentBase *env, omrobjectptr_t arrayPtr, fomrobject_t* endPtr, fomrobject_t *scanPtr, uintptr_t flags)
		: GC_IndexableObjectScanner(env, arrayPtr, NULL, endPtr, scanPtr, endPtr, 0, sizeof(fomrobject_t), flags)
		, _mapPtr(_scanPtr)
	{
		_typeId = __FUNCTION__;
	}

	/**
	 * Set up the scanner.
	 * @param[in] env Current environment
	 */
	MMINLINE void
	initialize(MM_EnvironmentBase *env)
	{
		GC_IndexableObjectScanner::initialize(env);
	}

public:
	/**
	 * @param[in] env The scanning thread environment
	 * @param[in] objectPtr pointer to the array to be processed
	 * @param[in] allocSpace pointer to space within which the scanner should be instantiated (in-place)
	 * @param[in] flags Scanning context flags
	 * @param[in] splitAmount If >0, the number of elements to include for this scanner instance; if 0, include all elements
	 * @param[in] startIndex The index of the first element to scan
	 */
	MMINLINE static GC_MetronomeArrayObjectScanner *
	newInstance(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, void *allocSpace, uintptr_t flags)
	{
		GC_MetronomeArrayObjectScanner *objectScanner = (GC_MetronomeArrayObjectScanner *)allocSpace;
		if (NULL != objectScanner) {
			MM_GCExtensionsBase *extensions = env->getExtensions();
			J9IndexableObject *indexablePtr = (J9IndexableObject *)objectPtr;
			fomrobject_t *basePtr = 0;
			fomrobject_t *endPtr = 0;

			/* if NUA is enabled, separate path for contiguous arrays */
			UDATA sizeInElements = extensions->indexableObjectModel.getSizeInElements(indexablePtr);
			bool isContiguous = extensions->indexableObjectModel.isInlineContiguousArraylet(indexablePtr);
			if (isContiguous || (0 == sizeInElements)) {
				basePtr = (fj9object_t *)extensions->indexableObjectModel.getDataPointerForContiguous(indexablePtr);
				endPtr = basePtr + sizeInElements;
			} else {
				UDATA numArraylets = extensions->indexableObjectModel.numArraylets(indexablePtr);
				basePtr = extensions->indexableObjectModel.getArrayoidPointer(indexablePtr);
				endPtr = basePtr + numArraylets;
			}

			new(objectScanner) GC_MetronomeArrayObjectScanner(env, objectPtr, endPtr, basePtr, flags);
			objectScanner->initialize(env);
		}
		return objectScanner;
	}

	MMINLINE uintptr_t getBytesRemaining() { return sizeof(fomrobject_t) * (_endPtr - _scanPtr); }
	fomrobject_t *getNextSlotMap(uintptr_t *scanMap, bool *hasNextSlotMap) { return NULL; }
	fomrobject_t *getNextSlotMap(uintptr_t *scanMap, uintptr_t *leafMap, bool *hasNextSlotMap) { return NULL; }
	GC_IndexableObjectScanner *splitTo(MM_EnvironmentBase *env, void *allocSpace, uintptr_t splitAmount) { return NULL; }
};

#endif /* METRONOMEARRAYOBJECTSCANNER_HPP_ */
