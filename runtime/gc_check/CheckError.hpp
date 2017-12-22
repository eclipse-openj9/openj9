
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
 * @ingroup GC_Check
 */

#if !defined(CHECKERROR_HPP_)
#define CHECKERROR_HPP_

#include "j9.h"
#include "j9cfg.h"

#include "Base.hpp"
#include "CheckBase.hpp"
#include "CheckCycle.hpp"

class GC_CheckReporter;
class MM_SublistPuddle;
class MM_UnfinalizedObjectList;
class MM_OwnableSynchronizerObjectList;
class GC_FinalizeListManager;

enum {
	check_type_other = 0,
	check_type_object,
	check_type_class,
	check_type_thread,
	check_type_puddle,
	check_type_unfinalized,
	check_type_finalizable,
	check_type_ownable_synchronizer
};


/**
 * Capture information related to an error.
 * 
 * While walking the heap various errors can be detected. This object
 * gathers the relevant error information and is passed to
 * GC_CheckReporter for displaying.
 * 
 * @todo _object and _slot are not always J9Objects, so we should do the right
 * thing for different types, and stop casting everything to J9Object when
 * we call this constructor
 * 
 * @see GC_CheckReporter
 * 
 * @ingroup GC_Check
 */
class GC_CheckError : public MM_Base
{
public:
	void *_object; /**< The object or structure that is reporting the error */
	void *_slot; /**< The slot that is reporting the error */
	const void *_stackLocaition; /**< The original location on the stack that reports the error */
	GC_Check *_check; /**< The check which triggered the error */
	GC_CheckCycle *_cycle; /**< Description of the cycle that triggered the error */
	const char *_elementName; /**< String describing the element reporting the error */
	UDATA _errorCode; /**< The error to be recorded, see @ref GCCheckWalkStageErrorCodes. */
	UDATA _errorNumber; /**< Number of the error encountered */
	UDATA _objectType; /**< The type of object */ 

private:

	void 
	initialize(void *object, void *slot, const void *stackLocation, GC_CheckCycle *cycle, GC_Check *check, const char *elementName, UDATA errorCode, UDATA errorNumber, UDATA objectType)
	{
		_object = object;
		_slot = slot;
		_stackLocaition = stackLocation;
		_cycle = cycle;
		_check = check;
		_elementName = elementName;
		_errorCode = errorCode;
		_errorNumber = errorNumber;
		_objectType = objectType;
	}

	void 
	initialize(void *object, void *slot, GC_CheckCycle *cycle, GC_Check *check, const char *elementName, UDATA errorCode, UDATA errorNumber, UDATA objectType)
	{
		initialize(object, slot, NULL, cycle, check, elementName, errorCode, errorNumber, objectType);
	}
	
	void 
	initialize(void *object, void *slot, GC_CheckCycle *cycle, GC_Check *check, UDATA errorCode, UDATA errorNumber, UDATA objectType)
	{
		initialize(object, slot, NULL, cycle, check, "", errorCode, errorNumber, objectType);
	}
	
	void 
	initialize(void *object, void *slot, const void *stackLocation, GC_CheckCycle *cycle, GC_Check *check, UDATA errorCode, UDATA errorNumber, UDATA objectType)
	{
		initialize(object, slot, stackLocation, cycle, check, "", errorCode, errorNumber, objectType);
	}

	void 
	initialize(void *object, GC_CheckCycle *cycle, GC_Check *check, const char *elementName, UDATA errorCode, UDATA errorNumber, UDATA objectType)
	{
		initialize(object, NULL, NULL, cycle, check, elementName, errorCode, errorNumber, objectType);
	}
	
	void 
	initialize(void *object, GC_CheckCycle *cycle, GC_Check *check, UDATA errorCode, UDATA errorNumber, UDATA objectType)
	{
		initialize(object, NULL, NULL, cycle, check, "", errorCode, errorNumber, objectType);
	}

public:

	/**
	 * Create a new CheckError object.
	 * 
	 * @param object the object (not necessarily a java.lang.Object) containing the bad slot
	 * @param slot the slot where the error was found, or the slot pointing to the object
	 * where the error was found
	 * @param invokedBy code indicating how the check was invoked (see @ref GCCheckInvokedBy)
	 * @param stage the current stage of the check, where the error was found
	 * @param manualCheckInvocation GCCheckInvokedBy#invocation_manual, if the check was invoked
	 * manually, and 0 otherwise
	 * @param elementName a string by which to refer to <code>object</code>, e.g. "Object ", "IObject ", etc.
	 * @param errorCode what kind of problem was detected (see @ref GCCheckWalkStageErrorCodes)
	 * @param errorNumber the current number of errors that have occurred, including this one
	 * @param objectType the type of the object param
	 */
	GC_CheckError(void *object, void *slot, GC_CheckCycle *cycle, GC_Check *check, const char *elementName, UDATA errorCode, UDATA errorNumber, UDATA objectType)
	{ 
		initialize(object, slot, cycle, check, elementName, errorCode, errorNumber, objectType);
	}

	/**
	 * Create a new CheckError object, with a blank elementName.
	 */
	GC_CheckError(void *object, void *slot, GC_CheckCycle *cycle, GC_Check *check, UDATA errorCode, UDATA errorNumber, UDATA objectType)
	{ 
		initialize(object, slot, cycle, check, errorCode, errorNumber, objectType);
	}

	/**
	 * Create a new CheckError object, when the error occurred while referencing
	 * an object directly on the heap (not through a slot).
	 */
	GC_CheckError(void *object, GC_CheckCycle *cycle, GC_Check *check, const char *elementName, UDATA errorCode, UDATA errorNumber, UDATA objectType)
	{ 
		initialize(object, cycle, check, elementName, errorCode, errorNumber, objectType);
	}


	/**
	 * Create a new CheckError object, with a blank element name, when the error 
	 * occurred while referencing an object directly on the heap (not through a slot).
	 */
	GC_CheckError(void *object, GC_CheckCycle *cycle, GC_Check *check, UDATA errorCode, UDATA errorNumber, UDATA objectType)
	{ 
		initialize(object, cycle, check, errorCode, errorNumber, objectType);
	}

	//////// Constructors for J9Class ////////

	/**
	 * Full constructor for errors in J9Class
	 */
	GC_CheckError(J9Class *object, void *slot, GC_CheckCycle *cycle, GC_Check *check, const char *elementName, UDATA errorCode, UDATA errorNumber)
	{
		initialize((void*)object, slot, cycle, check, elementName, errorCode, errorNumber, check_type_class);
	}

	/**
	 * Create a new CheckError object, with a blank elementName.
	 */
	GC_CheckError(J9Class *object, void *slot, GC_CheckCycle *cycle, GC_Check *check, UDATA errorCode, UDATA errorNumber)
	{
		initialize((void*)object, slot, cycle, check, errorCode, errorNumber, check_type_class);
	}

	/**
	 * Create a new CheckError object, when the error occurred while referencing
	 * an object directly on the heap (not through a slot).
	 */
	GC_CheckError(J9Class *object, GC_CheckCycle *cycle, GC_Check *check, const char *elementName, UDATA errorCode, UDATA errorNumber)
	{
		initialize((void*)object, cycle, check, elementName, errorCode, errorNumber, check_type_class);
	}

	/**
	 * Create a new CheckError object, with a blank element name, when the error 
	 * occurred while referencing an object directly on the heap (not through a slot).
	 */
	GC_CheckError(J9Class *object, GC_CheckCycle *cycle, GC_Check *check, UDATA errorCode, UDATA errorNumber)
	{
		initialize((void*)object, cycle, check, errorCode, errorNumber, check_type_class);
	}

	//////// Constructors for J9VMThread ////////
	/**
	 * Create a new CheckError object, with a blank elementName.
	 */
	GC_CheckError(J9VMThread *object, J9Object **slot, const void *stackLocation, GC_CheckCycle *cycle, GC_Check *check, UDATA errorCode, UDATA errorNumber)
	{
		initialize((void*)object, (void*)slot, stackLocation, cycle, check, errorCode, errorNumber, check_type_thread);
	}
	//////// Constructors for J9Object ////////

	/**
	 * Full constructor for errors in J9VMThread
	 */
	GC_CheckError(J9Object *object, fj9object_t *slot, GC_CheckCycle *cycle, GC_Check *check, const char *elementName, UDATA errorCode, UDATA errorNumber)
	{
		initialize((void*)object, (void*)slot, cycle, check, elementName, errorCode, errorNumber, check_type_object);
	}

	/**
	 * Create a new CheckError object, with a blank elementName.
	 */
	GC_CheckError(J9Object *object, fj9object_t *slot, GC_CheckCycle *cycle, GC_Check *check, UDATA errorCode, UDATA errorNumber)
	{
		initialize((void*)object, (void*)slot, cycle, check, errorCode, errorNumber, check_type_object);
	}

	/**
	 * Create a new CheckError object, when the error occurred while referencing
	 * an object directly on the heap (not through a slot).
	 */
	GC_CheckError(J9Object *object, GC_CheckCycle *cycle, GC_Check *check, const char *elementName, UDATA errorCode, UDATA errorNumber)
	{
		initialize((void*)object, cycle, check, elementName, errorCode, errorNumber, check_type_object);
	}

	/**
	 * Create a new CheckError object, with a blank element name, when the error 
	 * occurred while referencing an object directly on the heap (not through a slot).
	 */
	GC_CheckError(J9Object *object, GC_CheckCycle *cycle, GC_Check *check, UDATA errorCode, UDATA errorNumber)
	{
		initialize((void*)object, cycle, check, errorCode, errorNumber, check_type_object);
	}

	//////// Constructors for MM_SublistPuddle ////////

	/**
	 * Full constructor for errors in MM_SublistPuddle
	 */
	GC_CheckError(MM_SublistPuddle *object, J9Object **slot, GC_CheckCycle *cycle, GC_Check *check, const char *elementName, UDATA errorCode, UDATA errorNumber)
	{
		initialize((void*)object, (void*)slot, cycle, check, elementName, errorCode, errorNumber, check_type_puddle);
	}

	/**
	 * Create a new CheckError object, with a blank elementName.
	 */
	GC_CheckError(MM_SublistPuddle *object, J9Object **slot, GC_CheckCycle *cycle, GC_Check *check, UDATA errorCode, UDATA errorNumber)
	{
		initialize((void*)object, (void*)slot, cycle, check, errorCode, errorNumber, check_type_puddle);
	}

	/**
	 * Create a new CheckError object, when the error occurred while referencing
	 * an object directly on the heap (not through a slot).
	 */
	GC_CheckError(MM_SublistPuddle *object, GC_CheckCycle *cycle, GC_Check *check, const char *elementName, UDATA errorCode, UDATA errorNumber)
	{
		initialize((void*)object, cycle, check, elementName, errorCode, errorNumber, check_type_puddle);
	}

	/**
	 * Create a new CheckError object, with a blank element name, when the error 
	 * occurred while referencing an object directly on the heap (not through a slot).
	 */
	GC_CheckError(MM_SublistPuddle *object, GC_CheckCycle *cycle, GC_Check *check, UDATA errorCode, UDATA errorNumber)
	{
		initialize((void*)object, cycle, check, errorCode, errorNumber, check_type_puddle);
	}

	//////// Constructors for MM_UnfinalizedObjectList ////////

	/**
	 * Create a new CheckError object, with a blank elementName.
	 */
	GC_CheckError(MM_UnfinalizedObjectList *object, J9Object **slot, GC_CheckCycle *cycle, GC_Check *check, UDATA errorCode, UDATA errorNumber)
	{
		initialize((void*)object, (void*)slot, cycle, check, errorCode, errorNumber, check_type_unfinalized);
	}

	//////// Constructors for MM_FinalizeListManager ////////

	/**
	 * Create a new CheckError object, with a blank elementName.
	 */
	GC_CheckError(GC_FinalizeListManager *object, J9Object **slot, GC_CheckCycle *cycle, GC_Check *check, UDATA errorCode, UDATA errorNumber)
	{
		initialize((void*)object, (void*)slot, cycle, check, errorCode, errorNumber, check_type_finalizable);
	}

	//////// Constructors for MM_OwnableSynchronizerObjectList ////////

	/**
	 * Create a new CheckError object, with a blank elementName.
	 */
	GC_CheckError(MM_OwnableSynchronizerObjectList *object, J9Object **slot, GC_CheckCycle *cycle, GC_Check *check, UDATA errorCode, UDATA errorNumber)
	{
		initialize((void*)object, (void*)slot, cycle, check, errorCode, errorNumber, check_type_ownable_synchronizer);
	}
};

#endif /* CHECKERROR_HPP_ */
