
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
 * @ingroup GC_Structs
 */

#if !defined(CLASSITERATORDECLARATIONORDER_HPP_)
#define CLASSITERATORDECLARATIONORDER_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"
#include "j9modron.h"

#include "ClassIterator.hpp"
#include "ClassStaticsDeclarationOrderIterator.hpp"
#include "GCExtensionsBase.hpp"

/** 
 * Iterate through <i>all</i> slots in the class which contain an object reference 
 * in the order declared in the class where applicable.
 * 
 * @see GC_ClassIterator
 * @ingroup GC_Structs
 */
class GC_ClassIteratorDeclarationOrder : public GC_ClassIterator
{
protected:
	GC_ClassStaticsDeclarationOrderIterator _classStaticsDeclarationOrderIterator;

public:

	GC_ClassIteratorDeclarationOrder(J9JavaVM *jvm, J9Class *clazz, bool shouldPreindexInterfaceFields) 
		: GC_ClassIterator(MM_GCExtensionsBase::getExtensions(jvm->omrVM), clazz)
		, _classStaticsDeclarationOrderIterator(jvm, clazz, shouldPreindexInterfaceFields)
	{};


	/**
	 * Gets the current index corresponding to the current state.
	 * @return current index of the current state where appropriate.
	 * @return -1 if the current state is not indexed.
	 */
	MMINLINE IDATA getIndex()
	{
		switch (getState()) {
			case classiterator_state_statics:
				return _classStaticsDeclarationOrderIterator.getIndex();
				
			case classiterator_state_constant_pool:
				return _constantPoolObjectSlotIterator.getIndex();

			case classiterator_state_slots:
				return (IDATA)_scanIndex;
		
			case classiterator_state_callsites:
				return _callSitesIterator.getIndex();

			default:
				return -1;
		}			
	}

	/**
	 * Gets the reference type of the slot referred to for the current state
	 * @return 
	 */
	MMINLINE IDATA getSlotReferenceType()
	{
		IDATA refType = J9GC_REFERENCE_TYPE_UNKNOWN;
		
		switch (getState()) {
			case classiterator_state_statics:
				refType = J9GC_REFERENCE_TYPE_STATIC;
				break;
			case classiterator_state_constant_pool:
				refType = J9GC_REFERENCE_TYPE_CONSTANT_POOL;
				break;
			case classiterator_state_slots: 
				{
					/* NOTE: asking for index after nextSlot, so 1 larger than actual index in offset array */
					switch (_scanIndex) {
						case 1: 
							refType = J9GC_REFERENCE_TYPE_PROTECTION_DOMAIN;
							break;
						case 2:
							refType = J9GC_REFERENCE_TYPE_CLASS_NAME_STRING;
							break;
						default:		
							refType = J9GC_REFERENCE_TYPE_UNKNOWN;
							break;
					}
				}
				break;
			case classiterator_state_callsites:
				refType = J9GC_REFERENCE_TYPE_CALL_SITE;
				break;
			default:
				refType = J9GC_REFERENCE_TYPE_UNKNOWN;
				break;
		}
		return refType;
	}

	virtual volatile j9object_t *nextSlot();
};

#endif /* CLASSITERATORDECLARATIONORDER_HPP_ */

