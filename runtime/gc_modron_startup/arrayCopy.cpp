
/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
UDATA
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
	if (object) {
		J9Class *componentType = ((J9ArrayClass *)J9OBJECT_CLAZZ(vmThread, arrayObj))->componentType;
		/* Check if we are storing the object into an array of the same type */
		J9Class *storedClazz = J9OBJECT_CLAZZ(vmThread, object);
		if (storedClazz != componentType) {
			/* Check if we are storing the object into an array of Object[] */
			UDATA classDepth = J9CLASS_DEPTH(componentType);
			if (classDepth != 0) {
				return (0 != instanceOfOrCheckCast(storedClazz, componentType));
			}
		}
	}
	return true;
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

	/* Type checks are not required only if both classes are the same, or
	 * the source class is a subclass of the dest class
	 */
	if (srcClazz != destClazz) {
		UDATA srcDepth = J9CLASS_DEPTH(srcClazz);
		UDATA destDepth = J9CLASS_DEPTH(destClazz);
		if ((srcDepth <= destDepth) || (destClazz->superclasses[srcDepth] != srcClazz)) {
			return true;
		}
	}

	return false;
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
I_32
referenceArrayCopy(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, fj9object_t *srcAddress, fj9object_t *destAddress, I_32 lengthInSlots)
{
	if (lengthInSlots > 0) {
		MM_GCExtensions *ext = MM_GCExtensions::getExtensions(vmThread->javaVM);

#if defined(J9VM_GC_ARRAYLETS)
		Assert_MM_true(ext->indexableObjectModel.isInlineContiguousArraylet(srcObject) && ext->indexableObjectModel.isInlineContiguousArraylet(destObject));
#endif

		uintptr_t srcHeaderSize = ext->indexableObjectModel.getHeaderSize(srcObject);
		uintptr_t destHeaderSize = ext->indexableObjectModel.getHeaderSize(destObject);
		I_32 srcIndex = (I_32)(((uintptr_t)srcAddress - (srcHeaderSize + (uintptr_t)srcObject)) / sizeof(fj9object_t));
		I_32 destIndex = (I_32)(((uintptr_t)destAddress - (destHeaderSize + (uintptr_t)destObject)) / sizeof(fj9object_t));

		return referenceArrayCopyIndex(vmThread, srcObject, destObject, srcIndex, destIndex, lengthInSlots);
	}
	return -1;

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
I_32
referenceArrayCopyIndex(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots)
{
	if (lengthInSlots > 0) {

		J9WriteBarrierType wrtbarType = (J9WriteBarrierType)j9gc_modron_getWriteBarrierType(vmThread->javaVM);
		J9ReferenceArrayCopyTable *table = &MM_GCExtensions::getExtensions(vmThread->javaVM)->referenceArrayCopyTable;

		if ((srcObject == destObject) && (srcIndex < destIndex) && ((srcIndex + lengthInSlots) > destIndex)) {
			return table->backwardReferenceArrayCopyIndex[wrtbarType](vmThread, srcObject, destObject, srcIndex, destIndex, lengthInSlots);
		}
		if (arrayStoreCheckRequired(vmThread, destObject, srcObject)) {
			return table->forwardReferenceArrayCopyWithCheckIndex[wrtbarType](vmThread, srcObject, destObject, srcIndex, destIndex, lengthInSlots);
		}
		return table->forwardReferenceArrayCopyWithoutCheckIndex[wrtbarType](vmThread, srcObject, destObject, srcIndex, destIndex, lengthInSlots);	
	}
	return -1;
}

I_32 
backwardReferenceArrayCopyAndAlwaysWrtbarIndex(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots)
{
	MM_ObjectAccessBarrier *barrier = MM_GCExtensions::getExtensions(vmThread->javaVM)->accessBarrier;
	I_32 result;	
	
	/* Let access barrier specific code try doing an optimized version of the copy (if such exists) */
	/* -1 copy successful, -2 no copy done, >=0 copy was attempted but and exception was raised (index returned) */	
	if (-1 <= (result = barrier->backwardReferenceArrayCopyIndex(vmThread, srcObject, destObject, srcIndex, destIndex, lengthInSlots))) {
		return result;
	}

	I_32 endSrcIndex = srcIndex;
	J9Object *copyObject = NULL;

	srcIndex += lengthInSlots;
	destIndex += lengthInSlots;
	while(srcIndex > endSrcIndex) {
		srcIndex--;
		destIndex--;
		copyObject = J9JAVAARRAYOFOBJECT_LOAD(vmThread, srcObject, srcIndex);
		J9JAVAARRAYOFOBJECT_STORE(vmThread, destObject, destIndex, copyObject);
	}
	
	return -1;
}

I_32 
forwardReferenceArrayCopyWithCheckAndAlwaysWrtbarIndex(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots)
{
	I_32 srcEndIndex = srcIndex + lengthInSlots;
	
	while (srcIndex < srcEndIndex) {
		J9Object *copyObject = J9JAVAARRAYOFOBJECT_LOAD(vmThread, srcObject, srcIndex);
		if (!typeCheckArrayStore(vmThread, copyObject, destObject)) {
			goto error;
		}
		J9JAVAARRAYOFOBJECT_STORE(vmThread, destObject, destIndex, copyObject);
		srcIndex++;
		destIndex++;
	}

	return -1;

error:
	return srcIndex;
}

I_32 
forwardReferenceArrayCopyWithoutCheckAndAlwaysWrtbarIndex(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots)
{
	MM_ObjectAccessBarrier *barrier = MM_GCExtensions::getExtensions(vmThread->javaVM)->accessBarrier;
	I_32 result;
	
	/* Let access barrier specific code try doing an optimized version of the copy (if such exists) */
	/* -1 copy successful, -2 no copy done, >=0 copy was attempted but and exception was raised (index returned) */
	if (-1 <= (result = barrier->forwardReferenceArrayCopyIndex(vmThread, srcObject, destObject, srcIndex, destIndex, lengthInSlots))) {
		return result;
	}

	I_32 srcEndIndex = srcIndex + lengthInSlots;
	
	while (srcIndex < srcEndIndex) {
		J9Object *copyObject = J9JAVAARRAYOFOBJECT_LOAD(vmThread, srcObject, srcIndex);
		J9JAVAARRAYOFOBJECT_STORE(vmThread, destObject, destIndex, copyObject);
		srcIndex++;
		destIndex++;
	}

	return -1;
}

/**
 * Error stub for function table entries that aren't needed by Metronome. 
 */
I_32
copyVariantUndefinedIndex(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots)
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
