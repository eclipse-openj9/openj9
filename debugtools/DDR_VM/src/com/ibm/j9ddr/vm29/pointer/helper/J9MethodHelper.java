/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.pointer.helper;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.ConstantPoolHelpers;
import com.ibm.j9ddr.vm29.j9.ROMHelp;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ConstantPoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMNameAndSignaturePointer;
import com.ibm.j9ddr.vm29.structure.J9Method;
import com.ibm.j9ddr.vm29.structure.J9ROMMethod;

public class J9MethodHelper {
	
	public static J9ROMMethodPointer romMethod(J9MethodPointer methodPointer) throws CorruptDataException
	{
		return J9ROMMethodPointer.cast(methodPointer.bytecodes().addOffset(-J9ROMMethod.SIZEOF));
	}
	
	public static J9MethodPointer nextMethod(J9MethodPointer methodPointer) throws CorruptDataException
	{
		return J9MethodPointer.cast(methodPointer.addOffset(J9Method.SIZEOF));
	}
	
	public static String getName(J9MethodPointer methodPointer) throws CorruptDataException 
	{
		J9ClassPointer className;
		J9ConstantPoolPointer constantPool;
	
		if (methodPointer.isNull())
			return "bad ramMethod";
	
		constantPool = ConstantPoolHelpers.J9_CP_FROM_METHOD(methodPointer);
		if (constantPool.isNull())  {
			return "error reading constant pool from ramMethod";
		}
		className = ConstantPoolHelpers.J9_CLASS_FROM_CP(constantPool);
		if (className.isNull())  {
			return "error reading class name from constant pool";
		}
	
	
		J9ROMNameAndSignaturePointer nameAndSignature = ROMHelp.J9_ROM_METHOD_FROM_RAM_METHOD(methodPointer).nameAndSignature();
		String name= J9UTF8Helper.stringValue(nameAndSignature.name());
		String signature = J9UTF8Helper.stringValue(nameAndSignature.signature());
		
		return J9ClassHelper.getName(className) + "." + name+ signature;
	}
}
