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
 
 
/**
 * @file
 * @ingroup GC_Modron_Base
 */

#include "j9.h"
#include "j9protos.h"
#include "j9consts.h"
#include "j9cfg.h"
#include "modronopt.h"
#include "j9modron.h"
#include "arrayCopyInterface.h"
#include "ModronAssertions.h"

#include <string.h>

#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "ObjectAccessBarrier.hpp"
#include "ObjectModel.hpp"
#include "SlotObject.hpp"

extern "C" {

/**
 * Called from JIT to determine how reference array copy should be handled.
 * @return true if jitted code should always call the C helper for a reference array copy
 * @return false otherwise
 */
uintptr_t
alwaysCallReferenceArrayCopyHelper(J9JavaVM *javaVM)
{
	return MM_GCExtensions::getExtensions(javaVM)->isMetronomeGC() ? 1 : 0;
}

/**
 * Internal helper for performing a type check before an array store.
 * @return true if the store is legal, false otherwise
 * @note Translated from builder: J9VM>>typeCheckArrayStoreOf:into:ifFail:nullCheck:
 */
static bool
typeCheckArrayStore(J9VMThread *vmThread, J9Object *object, J9IndexableObject *arrayObj)
{
	bool result = true;

	if (NULL != object) {
		J9Class *componentType = ((J9ArrayClass *)J9OBJECT_CLAZZ(vmThread, arrayObj))->componentType;
		/* Check if we are storing the object into an array of the same type */
		J9Class *storedClazz = J9OBJECT_CLAZZ(vmThread, object);
		if (storedClazz != componentType) {
			/* Check if we are storing the object into an array of Object[] */
			uintptr_t classDepth = J9CLASS_DEPTH(componentType);
			if (classDepth != 0) {
				result = 0 != instanceOfOrCheckCast(storedClazz, componentType);
			}
		}
	}
	return result;
}

/**
 * Internal helper to see whether any type checks need to be performed during arraycopy.
 * @return true if type checks should be performed, false otherwise.
 */
static MMINLINE bool
arrayStoreCheckRequired(J9VMThread *vmThread, J9IndexableObject *destObject, J9IndexableObject *srcObject)
{
	J9Class *srcClazz = J9OBJECT_CLAZZ(vmThread, srcObject);
	J9Class *destClazz = J9OBJECT_CLAZZ(vmThread, destObject);
	bool result = false;

	/* Type checks are not required only if both classes are the same, or
	 * the source class is a subclass of the dest class
	 */
	if (srcClazz != destClazz) {
		uintptr_t srcDepth = J9CLASS_DEPTH(srcClazz);
		uintptr_t destDepth = J9CLASS_DEPTH(destClazz);
		result = (srcDepth <= destDepth) || (destClazz->superclasses[srcDepth] != srcClazz);
	}

	return result;
}

/**
 * VM helper for performing a reference array copy.
 * This function corresponds to the function of the same name in the memory manager
 * function table. This helper is called by the System.arraycopy native when performing
 * a reference array copy, after bounds checks have been made.
 * This specific API takes starting positions in a form of address of a slot. It would just calculate
 * corresponding starting indices and pass the request to  referenceArrayCopyIndex() API.
 * This API must be used only with contiguous arrays.
 * @return -1 if the copy succeeded
 * @return the index where the ArrayStoreException occurred, if the copy failed
 */
int32_t
referenceArrayCopy(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, fj9object_t *srcAddress, fj9object_t *destAddress, int32_t lengthInSlots)
{
	int32_t result = -1;

	if (lengthInSlots > 0) {
		MM_GCExtensions *ext = MM_GCExtensions::getExtensions(vmThread->javaVM);
		MM_ObjectAccessBarrier *barrier = ext->accessBarrier;

		Assert_MM_true(ext->indexableObjectModel.isInlineContiguousArraylet(srcObject) && ext->indexableObjectModel.isInlineContiguousArraylet(destObject));

		uintptr_t const referenceSize = J9VMTHREAD_REFERENCE_SIZE(vmThread);
		uintptr_t srcDataAddr = (uintptr_t) barrier->getArrayObjectDataAddress(vmThread, srcObject);
		uintptr_t dstDataAddr = (uintptr_t) barrier->getArrayObjectDataAddress(vmThread, destObject);
		int32_t srcIndex = (int32_t)(((uintptr_t)srcAddress - srcDataAddr) / referenceSize);
		int32_t destIndex = (int32_t)(((uintptr_t)destAddress - dstDataAddr) / referenceSize);

		result = referenceArrayCopyIndex(vmThread, srcObject, destObject, srcIndex, destIndex, lengthInSlots);
	}
	return result;
}

/**
 * VM helper for performing a reference array copy.
 * This function corresponds to the function of the same name in the memory manager
 * function table. This helper is called by the System.arraycopy native when performing
 * a reference array copy, after bounds checks have been made.
 * @return -1 if the copy succeeded
 * @return if the copy failed, return the index where the exception occurred. It
 * may be an ArrayStoreException
 */
int32_t
referenceArrayCopyIndex(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, int32_t srcIndex, int32_t destIndex, int32_t lengthInSlots)
{
	int32_t result = -1;

	if (lengthInSlots > 0) {
		J9WriteBarrierType wrtbarType = (J9WriteBarrierType)j9gc_modron_getWriteBarrierType(vmThread->javaVM);
		J9ReferenceArrayCopyTable *table = &MM_GCExtensions::getExtensions(vmThread->javaVM)->referenceArrayCopyTable;

		if ((srcObject == destObject) && (srcIndex < destIndex) && ((srcIndex + lengthInSlots) > destIndex)) {
			result = table->backwardReferenceArrayCopyIndex[wrtbarType](vmThread, srcObject, destObject, srcIndex, destIndex, lengthInSlots);
		} else if (arrayStoreCheckRequired(vmThread, destObject, srcObject)) {
			result = table->forwardReferenceArrayCopyWithCheckIndex[wrtbarType](vmThread, srcObject, destObject, srcIndex, destIndex, lengthInSlots);
		} else {
			result = table->forwardReferenceArrayCopyWithoutCheckIndex[wrtbarType](vmThread, srcObject, destObject, srcIndex, destIndex, lengthInSlots);
		}
	}
	return result;
}

int32_t
backwardReferenceArrayCopyAndAlwaysWrtbarIndex(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, int32_t srcIndex, int32_t destIndex, int32_t lengthInSlots)
{
	MM_ObjectAccessBarrier *barrier = MM_GCExtensions::getExtensions(vmThread->javaVM)->accessBarrier;
	
	/* Let access barrier specific code try doing an optimized version of the copy (if such exists) */
	/* -1 copy successful, -2 no copy done, >=0 copy was attempted but and exception was raised (index returned) */	
	int32_t result = barrier->backwardReferenceArrayCopyIndex(vmThread, srcObject, destObject, srcIndex, destIndex, lengthInSlots);

	if (-1 > result) {
		int32_t endSrcIndex = srcIndex;
		J9Object *copyObject = NULL;

		srcIndex += lengthInSlots;
		destIndex += lengthInSlots;
		while (srcIndex > endSrcIndex) {
			srcIndex -= 1;
			destIndex -= 1;
			copyObject = J9JAVAARRAYOFOBJECT_LOAD(vmThread, srcObject, srcIndex);
			J9JAVAARRAYOFOBJECT_STORE(vmThread, destObject, destIndex, copyObject);
		}
		result = -1;
	}
	return result;
}

int32_t
forwardReferenceArrayCopyWithCheckAndAlwaysWrtbarIndex(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, int32_t srcIndex, int32_t destIndex, int32_t lengthInSlots)
{
	int32_t srcEndIndex = srcIndex + lengthInSlots;
	int32_t result = -1;
	
	while (srcIndex < srcEndIndex) {
		J9Object *copyObject = J9JAVAARRAYOFOBJECT_LOAD(vmThread, srcObject, srcIndex);
		if (typeCheckArrayStore(vmThread, copyObject, destObject)) {
			J9JAVAARRAYOFOBJECT_STORE(vmThread, destObject, destIndex, copyObject);
			srcIndex += 1;
			destIndex += 1;
		} else {
			/* An error has been returned */
			result = srcIndex;
			break;
		}
	}

	return result;
}

int32_t
forwardReferenceArrayCopyWithoutCheckAndAlwaysWrtbarIndex(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, int32_t srcIndex, int32_t destIndex, int32_t lengthInSlots)
{
	MM_ObjectAccessBarrier *barrier = MM_GCExtensions::getExtensions(vmThread->javaVM)->accessBarrier;
	
	/* Let access barrier specific code try doing an optimized version of the copy (if such exists) */
	/* -1 copy successful, -2 no copy done, >=0 copy was attempted but and exception was raised (index returned) */
	int32_t result = barrier->forwardReferenceArrayCopyIndex(vmThread, srcObject, destObject, srcIndex, destIndex, lengthInSlots);

	if (-1 > result) {
		int32_t srcEndIndex = srcIndex + lengthInSlots;
	
		while (srcIndex < srcEndIndex) {
			J9Object *copyObject = J9JAVAARRAYOFOBJECT_LOAD(vmThread, srcObject, srcIndex);
			J9JAVAARRAYOFOBJECT_STORE(vmThread, destObject, destIndex, copyObject);
			srcIndex += 1;
			destIndex += 1;
		}
		result = -1;
	}
	return result;
}

/**
 * Error stub for function table entries that aren't needed by Metronome. 
 */
int32_t
copyVariantUndefinedIndex(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, int32_t srcIndex, int32_t destIndex, int32_t lengthInSlots)
{	
	Assert_MM_unreachable();
	return srcIndex;
}

/**
 * Initialize the reference array copy function table with the appropriate function pointers
 * @note This table must be updated if @ref J9WriteBarrierType is changed
 */
void
initializeReferenceArrayCopyTable(J9ReferenceArrayCopyTable *table)
{
	/* The generic reference array copy */
	table->referenceArrayCopyIndex = referenceArrayCopyIndex;
	
	/* Backward copy doesn't require a type check, but source and dest object are the same */
	table->backwardReferenceArrayCopyIndex[j9gc_modron_wrtbar_illegal] = copyVariantUndefinedIndex;
	table->backwardReferenceArrayCopyIndex[j9gc_modron_wrtbar_none] = backwardReferenceArrayCopyAndAlwaysWrtbarIndex;
	table->backwardReferenceArrayCopyIndex[j9gc_modron_wrtbar_always] = backwardReferenceArrayCopyAndAlwaysWrtbarIndex;
	table->backwardReferenceArrayCopyIndex[j9gc_modron_wrtbar_oldcheck] = backwardReferenceArrayCopyAndAlwaysWrtbarIndex;
	table->backwardReferenceArrayCopyIndex[j9gc_modron_wrtbar_cardmark] = backwardReferenceArrayCopyAndAlwaysWrtbarIndex;
	table->backwardReferenceArrayCopyIndex[j9gc_modron_wrtbar_cardmark_incremental] = backwardReferenceArrayCopyAndAlwaysWrtbarIndex;
	table->backwardReferenceArrayCopyIndex[j9gc_modron_wrtbar_cardmark_and_oldcheck] = backwardReferenceArrayCopyAndAlwaysWrtbarIndex;
	table->backwardReferenceArrayCopyIndex[j9gc_modron_wrtbar_satb] = backwardReferenceArrayCopyAndAlwaysWrtbarIndex;

	/* Forward copies with type check on each element (check for ArrayStoreException) */
	table->forwardReferenceArrayCopyWithCheckIndex[j9gc_modron_wrtbar_illegal] = copyVariantUndefinedIndex;
	table->forwardReferenceArrayCopyWithCheckIndex[j9gc_modron_wrtbar_none] = forwardReferenceArrayCopyWithCheckAndAlwaysWrtbarIndex;
	table->forwardReferenceArrayCopyWithCheckIndex[j9gc_modron_wrtbar_always] = forwardReferenceArrayCopyWithCheckAndAlwaysWrtbarIndex;
	table->forwardReferenceArrayCopyWithCheckIndex[j9gc_modron_wrtbar_oldcheck] = forwardReferenceArrayCopyWithCheckAndAlwaysWrtbarIndex;
	table->forwardReferenceArrayCopyWithCheckIndex[j9gc_modron_wrtbar_cardmark] = forwardReferenceArrayCopyWithCheckAndAlwaysWrtbarIndex;
	table->forwardReferenceArrayCopyWithCheckIndex[j9gc_modron_wrtbar_cardmark_incremental] = forwardReferenceArrayCopyWithCheckAndAlwaysWrtbarIndex;
	table->forwardReferenceArrayCopyWithCheckIndex[j9gc_modron_wrtbar_cardmark_and_oldcheck] = forwardReferenceArrayCopyWithCheckAndAlwaysWrtbarIndex;
	table->forwardReferenceArrayCopyWithCheckIndex[j9gc_modron_wrtbar_satb] = forwardReferenceArrayCopyWithCheckAndAlwaysWrtbarIndex;
	
	/* Forward copies with no type check */
	table->forwardReferenceArrayCopyWithoutCheckIndex[j9gc_modron_wrtbar_illegal] = copyVariantUndefinedIndex;
	table->forwardReferenceArrayCopyWithoutCheckIndex[j9gc_modron_wrtbar_none] = forwardReferenceArrayCopyWithoutCheckAndAlwaysWrtbarIndex;
	table->forwardReferenceArrayCopyWithoutCheckIndex[j9gc_modron_wrtbar_always] = forwardReferenceArrayCopyWithoutCheckAndAlwaysWrtbarIndex;
	table->forwardReferenceArrayCopyWithoutCheckIndex[j9gc_modron_wrtbar_oldcheck] = forwardReferenceArrayCopyWithoutCheckAndAlwaysWrtbarIndex;
	table->forwardReferenceArrayCopyWithoutCheckIndex[j9gc_modron_wrtbar_cardmark] = forwardReferenceArrayCopyWithoutCheckAndAlwaysWrtbarIndex;
	table->forwardReferenceArrayCopyWithoutCheckIndex[j9gc_modron_wrtbar_cardmark_incremental] = forwardReferenceArrayCopyWithoutCheckAndAlwaysWrtbarIndex;
	table->forwardReferenceArrayCopyWithoutCheckIndex[j9gc_modron_wrtbar_cardmark_and_oldcheck] = forwardReferenceArrayCopyWithoutCheckAndAlwaysWrtbarIndex;
	table->forwardReferenceArrayCopyWithoutCheckIndex[j9gc_modron_wrtbar_satb] = forwardReferenceArrayCopyWithoutCheckAndAlwaysWrtbarIndex;
}

} /* extern "C" */
