/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.gccheck;

import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.ObjectReferencePointer;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_OwnableSynchronizerObjectListPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_SublistPuddlePointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_UnfinalizedObjectListPointer;

public class CheckError
{
	public static final int check_type_other = 0;
	public static final int check_type_object = 1;
	public static final int check_type_class = 2;
	public static final int check_type_thread = 3;
	public static final int check_type_puddle = 4;
	public static final int check_type_unfinalized = 5;
	public static final int check_type_finalizable = 6;
	public static final int check_type_ownable_synchronizer = 7;
	
	public VoidPointer _object;
	public VoidPointer _slot;
	public VoidPointer _stackLocation;
	public Check _check;
	public CheckCycle _cycle;
	public String _elementName;
	public int _errorCode;
	public int _errorNumber;
	public int _objectType;
	
	// TODO : Constructors!
	
	public CheckError(AbstractPointer object, VoidPointer slot, VoidPointer stackLocation, CheckCycle cycle, Check check, String elementName, int errorCode, int errorNumber, int objectType)
	{
		_object = VoidPointer.cast(object);
		_slot = slot;
		_stackLocation = stackLocation;
		_cycle = cycle;
		_check = check;
		_elementName = elementName;
		_errorCode = errorCode;
		_errorNumber = errorNumber;
		_objectType = objectType;
	}

	public CheckError(AbstractPointer object, PointerPointer slot, CheckCycle cycle, Check check, int errorCode, int errorNumber, int objectType)
	{
		this(object, VoidPointer.cast(slot), null, cycle, check, "", errorCode, errorNumber, objectType);
	}
	
	public CheckError(J9ClassPointer clazz, CheckCycle cycle, Check check, String elementName, int errorCode, int errorNumber)
	{
		this(clazz, null, null, cycle, check, elementName, errorCode, errorNumber, check_type_class);
	}
	
	public CheckError(J9ClassPointer clazz, PointerPointer slot, CheckCycle cycle, Check check, String elementName, int errorCode, int errorNumber)
	{
		this(clazz, VoidPointer.cast(slot), null, cycle, check, elementName, errorCode, errorNumber, check_type_class);
	}
	
	public CheckError(J9ObjectPointer object, CheckCycle cycle, Check check, String elementName, int errorCode, int errorNumber)
	{
		this(object, null, null, cycle, check, elementName, errorCode, errorNumber, check_type_object);
	}

	public CheckError(J9ObjectPointer object, ObjectReferencePointer slot, CheckCycle cycle, Check check, String elementName, int errorCode, int errorNumber)
	{
		this(object, VoidPointer.cast(slot), null, cycle, check, elementName, errorCode, errorNumber, check_type_object);
	}

	public CheckError(J9VMThreadPointer object, PointerPointer slot, VoidPointer stackLocation, CheckCycle cycle, Check check, int errorCode, int errorNumber)
	{
		this(object, VoidPointer.cast(slot), stackLocation, cycle, check, "", errorCode, errorNumber, check_type_thread);
	}

	public CheckError(MM_SublistPuddlePointer object, PointerPointer slot, CheckCycle cycle, Check check, int errorCode, int errorNumber)
	{
		this(object, VoidPointer.cast(slot), null, cycle, check, "", errorCode, errorNumber, check_type_puddle);
	}

	public CheckError(MM_UnfinalizedObjectListPointer object, J9ObjectPointer slot, CheckCycle cycle, Check check, int errorCode, int errorNumber)
	{
		this(object, VoidPointer.cast(slot), null, cycle, check, "", errorCode, errorNumber, check_type_unfinalized);
	}
	
	public CheckError(MM_OwnableSynchronizerObjectListPointer object, J9ObjectPointer slot, CheckCycle cycle, Check check, int errorCode, int errorNumber)
	{
		this(object, VoidPointer.cast(slot), null, cycle, check, "", errorCode, errorNumber, check_type_ownable_synchronizer);
	}
}
