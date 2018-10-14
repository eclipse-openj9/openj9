/*******************************************************************************
 * Copyright (c) 2010, 2018 IBM Corp. and others
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
import com.ibm.j9ddr.vm29.j9.AlgorithmVersion;
import com.ibm.j9ddr.vm29.j9.ROMHelp;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.structure.J9CfrClassFile;
import com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags;
import com.ibm.j9ddr.vm29.structure.J9ROMMethod;
import com.ibm.j9ddr.vm29.types.UDATA;
import com.ibm.j9ddr.vm29.types.U32;

import static com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags.*;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;

/**
 * Static helper for ROM methods
 *
 */
public class J9ROMMethodHelper 
{
	static final int romHelpVersion = AlgorithmVersion.getVersionOf(AlgorithmVersion.ROM_HELP_VERSION).getAlgorithmVersion();
	/**
	 * Gets the modifiers as returned from java.lang.reflect.Method.getModifiers(); (masking out all internal
	 * JVM modifiers)
	 * @param fieldShapePointer Field
	 * @return Modifier codes
	 * @throws CorruptDataException
	 */
	public static int getReflectModifiers(J9ROMMethodPointer method) throws CorruptDataException
	{
		return method.modifiers().bitAnd(J9CfrClassFile.CFR_METHOD_ACCESS_MASK).intValue();
	}
	
	public static String getName(J9ROMMethodPointer method) throws CorruptDataException {
		return J9UTF8Helper.stringValue(method.nameAndSignature().name());
	}
	
	public static String getSignature(J9ROMMethodPointer method) throws CorruptDataException {
		return J9UTF8Helper.stringValue(method.nameAndSignature().signature());
	}
	
	// This method must have been lost in the auto-gen
	public static U8Pointer bytecodes(J9ROMMethodPointer method) throws CorruptDataException {
		return U8Pointer.cast(method).addOffset(J9ROMMethod.SIZEOF);
	}

	public static UDATA bytecodeSize(J9ROMMethodPointer method) throws CorruptDataException {
		return method.bytecodeSizeLow().add(new UDATA(method.bytecodeSizeHigh().leftShift(16)));
	}

	public static  U8Pointer bytecodeEnd(J9ROMMethodPointer method) throws CorruptDataException {
		return bytecodes(method).add(bytecodeSize(method));
	}
	
	public static U32 getExtendedModifiersDataFromROMMethod(J9ROMMethodPointer method) throws CorruptDataException
	{
		return ROMHelp.getExtendedModifiersDataFromROMMethod(method);
	}
	
	public static boolean isGetter(J9ROMMethodPointer method) throws CorruptDataException {
		return method.modifiers().allBitsIn(J9AccGetterMethod);
	}
		
	public static boolean isForwarder(J9ROMMethodPointer method) throws CorruptDataException {
		return method.modifiers().allBitsIn(J9AccForwarderMethod);
	}
	
	public static boolean isEmpty(J9ROMMethodPointer method) throws CorruptDataException {
		return method.modifiers().allBitsIn(J9AccEmptyMethod);
	}
	
	public static boolean hasVTable(J9ROMMethodPointer method) throws CorruptDataException {
		return method.modifiers().allBitsIn(J9AccMethodVTable);
	}

	public static boolean isStatic(J9ROMMethodPointer method) throws CorruptDataException {
		return method.modifiers().allBitsIn(J9AccStatic);
	}

	public static boolean hasExceptionInfo(J9ROMMethodPointer method) throws CorruptDataException {
		return method.modifiers().allBitsIn(J9AccMethodHasExceptionInfo);
	}

	public static boolean hasBackwardBranches(J9ROMMethodPointer method) throws CorruptDataException {
		return method.modifiers().allBitsIn(J9AccMethodHasBackwardBranches);
	}
	
	public static boolean hasGenericSignature(J9ROMMethodPointer method) throws CorruptDataException {
		return method.modifiers().allBitsIn(J9AccMethodHasGenericSignature);
	}
	
	public static boolean hasMethodAnnotations(J9ROMMethodPointer method) throws CorruptDataException {
		return method.modifiers().allBitsIn(J9JavaAccessFlags.J9AccMethodHasMethodAnnotations);
	}
	
	public static boolean hasParameterAnnotations(J9ROMMethodPointer method) throws CorruptDataException {
		return method.modifiers().allBitsIn(J9JavaAccessFlags.J9AccMethodHasParameterAnnotations);
	}
	
	public static boolean hasMethodTypeAnnotations(J9ROMMethodPointer method) throws CorruptDataException {
		boolean result = false;
		if (0 != romHelpVersion) {
			U32 extendedModifiers = getExtendedModifiersDataFromROMMethod(method);
			result =  extendedModifiers.allBitsIn(J9CfrClassFile.CFR_METHOD_EXT_HAS_METHOD_TYPE_ANNOTATIONS);
		} else {
			result = method.modifiers().allBitsIn(J9JavaAccessFlags.J9AccMethodHasTypeAnnotations);
			/* J9AccMethodHasExtendedModifiers was J9AccMethodHasTypeAnnotations */
		}
		return result;
	}
	
	public static boolean hasCodeTypeAnnotations(J9ROMMethodPointer method) throws CorruptDataException {
		boolean result = false;
		if (0 != romHelpVersion) {
			U32 extendedModifiers = getExtendedModifiersDataFromROMMethod(method);
			result = extendedModifiers.allBitsIn(J9CfrClassFile.CFR_METHOD_EXT_HAS_CODE_TYPE_ANNOTATIONS);
		}
		return result;
	}
	
	public static boolean hasExtendedModifiers(J9ROMMethodPointer method) throws CorruptDataException {
		return (0 != romHelpVersion) && method.modifiers().allBitsIn(J9JavaAccessFlags.J9AccMethodHasExtendedModifiers);
	}
	
	public static boolean hasDefaultAnnotation(J9ROMMethodPointer method) throws CorruptDataException {
		return method.modifiers().allBitsIn(J9JavaAccessFlags.J9AccMethodHasDefaultAnnotation);
	}
	
	public static boolean hasDebugInfo(J9ROMMethodPointer method) throws CorruptDataException {
		return method.modifiers().allBitsIn(J9JavaAccessFlags.J9AccMethodHasDebugInfo);
	}
	
	public static boolean hasStackMap(J9ROMMethodPointer method) throws CorruptDataException {
		return method.modifiers().allBitsIn(J9AccMethodHasStackMap);
	}

	public static boolean hasMethodParameters(J9ROMMethodPointer method) throws CorruptDataException {
		return method.modifiers().allBitsIn(J9AccMethodHasMethodParameters);
	}

	public static boolean isNonEmptyObjectConstructor(J9ROMMethodPointer method) throws CorruptDataException {
		return method.modifiers().maskAndCompare(J9AccMethodObjectConstructor + J9AccEmptyMethod, J9AccMethodObjectConstructor);
	}
}
