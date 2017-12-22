
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
 * @ingroup GC_Base
 */

#ifndef STACKSLOTVALIDATOR_HPP_
#define STACKSLOTVALIDATOR_HPP_

#include "j9.h"

#include "EnvironmentBase.hpp"
#include "HeapRegionDescriptor.hpp"
#include "HeapRegionManager.hpp"
#include "Validator.hpp"

class MM_EnvironmentBase;

class MM_StackSlotValidator : public MM_Validator {
/* data members */
private:
	const UDATA _flags; /**< Flags for this validator: COULD_BE_FORWARDED / NOT_ON_HEAP */
	J9Object *const _slotValue; /**< The object in the stack slot being validated */
	const void* const _stackLocation; /**< The actual stack slot pointer */
	J9StackWalkState * const _walkState; /**< The current stack walk state */
protected:
public:
	enum {
		COULD_BE_FORWARDED = 1, /**< indicates that a scavenge or copy-forward operation is in progress and that the value in _slotPtr could point to a forwarded object */
		NOT_ON_HEAP = 2, /**< indicates that the value in slotPtr is not on the object heap */
		FAKE_ERROR = 4 /**< indicates that an error should be produced artificially */
	};
	
/* function members */
private:
	/**
	 * Report detailed information about the current stack slot.
	 * @param env[in] the current thread (not necessarily the thread whose stack is being inspected)
	 * @param message[in] a message to include with the report
	 */
	void reportStackSlot(MM_EnvironmentBase* env, const char* message);
	
protected:
public:
	virtual void threadCrash(MM_EnvironmentBase* env);

	/**
	 * Construct a new StackSlotValidator
	 * @param flags[in] Flags for this validator: COULD_BE_FORWARDED / NOT_ON_HEAP
	 * @param slotValue[in] The object in the stack slot being validated
	 * @param stackLocation[in] The actual stack slot pointer
	 * @param walkState[in] The current stack walk state 
	 */
	MM_StackSlotValidator(UDATA flags, J9Object *slotValue, const void* stackLocation, void *walkState)
		: MM_Validator()
		, _flags(flags)
		, _slotValue(slotValue)
		, _stackLocation(stackLocation)
		, _walkState((J9StackWalkState*)walkState)
	{
		_typeId = __FUNCTION__;
	}

	/**
	 * Validate the stack slot value.
	 * If an error is detected, print diagnostics to stderr and tracepoints and return false.
	 * Otherwise silently (and quickly) return true.
	 * @param env[in] the current thread (not necessarily the thread whose stack is being inspected)
	 * @return true for valid stack slot value, false for invalid stack slot
	 */
	bool validate(MM_EnvironmentBase *env) { 
		bool result = true;
		env->_activeValidator = this;

		const bool onHeap = (NOT_ON_HEAP != (NOT_ON_HEAP & _flags));
		const bool couldBeForwarded = (COULD_BE_FORWARDED == (COULD_BE_FORWARDED & _flags));
		const bool fakeError = (FAKE_ERROR == (FAKE_ERROR & _flags));
		MM_HeapRegionDescriptor *region = NULL;
		MM_GCExtensionsBase *extensions = env->getExtensions();
		bool shouldCheckRegionValidity = extensions->isVLHGC() || extensions->isSegregatedHeap();
		if (onHeap && shouldCheckRegionValidity) {
			region = extensions->heapRegionManager->regionDescriptorForAddress(_slotValue);
		}
		J9JavaStack *stack = _walkState->walkThread->stackObject;
		
		if (J9_INVALID_OBJECT == _slotValue) {
			reportStackSlot(env, "J9_INVALID_OBJECT");
			result = false;
		} else if ( onHeap && (0 != ((UDATA)_slotValue & (env->getObjectAlignmentInBytes() - 1)))) {
			reportStackSlot(env, "Misaligned object");
			result = false;
		} else if (shouldCheckRegionValidity && onHeap && ( (NULL == region) || !region->containsObjects())) {
			reportStackSlot(env, "Object not in valid region");
			result = false;
		} else if ( !onHeap && (((U_8*)_slotValue >= (U_8*)stack->end) || ((U_8*)_slotValue < (U_8*)(stack + 1)))) {
			reportStackSlot(env, "Object neither in heap nor stack-allocated");
			result = false;
		} else if ( !onHeap && (0 != ((UDATA)_slotValue & (sizeof(UDATA) - 1)))) {
			reportStackSlot(env, "Misaligned stack-allocated object"); /* stack allocated objects might only be pointer aligned, not necessarily 8-aligned */
			result = false;
		} else if (!couldBeForwarded && ((UDATA)0x99669966 != J9GC_J9OBJECT_CLAZZ(_slotValue)->eyecatcher)) {
			reportStackSlot(env, !onHeap ? "Invalid class pointer in stack allocated object" : "Invalid class pointer");
			result = false;
		} else if (fakeError) {
			reportStackSlot(env, "Artificial error");
			result = false;
		}
		
		env->_activeValidator = NULL;
		return result;
	}
};

#endif /* STACKSLOTVALIDATOR_HPP_ */
