
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
 * @ingroup GC_Structs
 */

#if !defined(CLASSITERATOR_HPP_)
#define CLASSITERATOR_HPP_

#include "j9.h"
#include "j9cfg.h"

#include "CallSitesIterator.hpp"
#include "ClassStaticsIterator.hpp"
#include "ConstantPoolObjectSlotIterator.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "MethodTypesIterator.hpp"

/**
 * State constants representing the current stage of the iteration process
 * @anchor ClassIteratorState
 */
enum {
	classiterator_state_start = 0,
	classiterator_state_statics,
	classiterator_state_constant_pool,
	classiterator_state_slots,
	classiterator_state_callsites,
	classiterator_state_methodtypes,
	classiterator_state_varhandlemethodtypes,
	classiterator_state_end
};

/**
 * Iterate over all slots in a class which contain an object reference
 * 
 * @see GC_ClassIteratorClassSlots
 *  
 * @ingroup GC_Structs
 */
class GC_ClassIterator {
protected:
	J9Class *_clazzPtr;
	int _state;
	UDATA _scanIndex;

	GC_ClassStaticsIterator _classStaticsIterator;
	GC_ConstantPoolObjectSlotIterator _constantPoolObjectSlotIterator;
	GC_CallSitesIterator _callSitesIterator;
	GC_MethodTypesIterator _methodTypesIterator;
	GC_MethodTypesIterator _varHandlesMethodTypesIterator;
	const bool _shouldScanClassObject; /**< Boolean needed for balanced GC to prevent ClassObject from being scanned twice  */

public:
	GC_ClassIterator(MM_EnvironmentBase *env, J9Class *clazz, bool shouldScanClassObject = true)
		: _clazzPtr(clazz)
		, _state(classiterator_state_start)
		, _scanIndex(0)
		, _classStaticsIterator(env, clazz)
		, _constantPoolObjectSlotIterator((J9JavaVM *)env->getLanguageVM(), clazz)
		, _callSitesIterator(clazz)
		, _methodTypesIterator(clazz->romClass->methodTypeCount, clazz->methodTypes)
		, _varHandlesMethodTypesIterator(clazz->romClass->varHandleMethodTypeCount, clazz->varHandleMethodTypes)
		, _shouldScanClassObject(shouldScanClassObject)
	{}

	GC_ClassIterator(MM_GCExtensionsBase *extensions, J9Class *clazz)
		: _clazzPtr(clazz)
		, _state(classiterator_state_start)
		, _scanIndex(0)
		, _classStaticsIterator(extensions, clazz)
		, _constantPoolObjectSlotIterator((J9JavaVM *)extensions->getOmrVM()->_language_vm, clazz)
		, _callSitesIterator(clazz)
		, _methodTypesIterator(clazz->romClass->methodTypeCount, clazz->methodTypes)
		, _varHandlesMethodTypesIterator(clazz->romClass->varHandleMethodTypeCount, clazz->varHandleMethodTypes)
		, _shouldScanClassObject(true)
	{}

	MMINLINE int getState() 
	{ 
		return _state; 
	}

	/**
	 * Fetch the next slot in the class.
	 * Note that the pointer is volatile. In concurrent applications the mutator may 
	 * change the value in the slot while iteration is in progress.
	 * @return the next static slot in the class containing an object reference
	 * @return NULL if there are no more such slots
	 */
	virtual volatile j9object_t *nextSlot();
};

#endif /* CLASSITERATOR_HPP_ */

