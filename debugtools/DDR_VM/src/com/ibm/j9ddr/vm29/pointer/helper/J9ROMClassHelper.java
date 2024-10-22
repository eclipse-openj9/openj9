/*
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
 */
package com.ibm.j9ddr.vm29.pointer.helper;

import static com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags.*;
import static com.ibm.j9ddr.vm29.structure.J9NonbuilderConstants.J9_ROMCLASS_OPTINFO_IMPLICITCREATION_ATTRIBUTE;
import static com.ibm.j9ddr.vm29.structure.J9NonbuilderConstants.J9_ROMCLASS_OPTINFO_LOADABLEDESCRIPTORS_ATTRIBUTE;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMConstantPoolItemPointer;

public class J9ROMClassHelper {
	public static J9ROMConstantPoolItemPointer constantPool(J9ROMClassPointer romclass) {
		return J9ROMConstantPoolItemPointer.cast(romclass.add(1));
	}

	public static U32Pointer cpShapeDescription(J9ROMClassPointer romClass) throws CorruptDataException {
		return U32Pointer.cast(romClass.cpShapeDescription());
	}

	public static U32Pointer optionalInfo(J9ROMClassPointer romClass) throws CorruptDataException {
		return U32Pointer.cast(romClass.optionalInfo());
	}

	public static boolean isPublic(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass.modifiers().allBitsIn(J9AccPublic);
	}

	public static boolean isFinal(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass.modifiers().allBitsIn(J9AccFinal);
	}

	public static boolean isInterface(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass.modifiers().allBitsIn(J9AccInterface);
	}

	public static boolean isAbstract(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass.modifiers().allBitsIn(J9AccAbstract);
	}

	public static boolean isSynthetic(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass.modifiers().allBitsIn(J9AccSynthetic);
	}

	public static boolean isArray(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass.modifiers().allBitsIn(J9AccClassArray);
	}

	public static boolean isPrimitiveType(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass.modifiers().allBitsIn(J9AccClassInternalPrimitiveType);
	}

	public static boolean isUnsafe(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass.extraModifiers().allBitsIn(J9AccClassUnsafe);
	}

	public static boolean hasVerifyData(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass.extraModifiers().allBitsIn(J9AccClassHasVerifyData);
	}

	public static boolean hasModifiedByteCodes(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass.extraModifiers().allBitsIn(J9AccClassBytecodesModified);
	}

	public static boolean hasEmptyFinalize(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass.extraModifiers().allBitsIn(J9AccClassHasEmptyFinalize);
	}

	public static boolean hasJDBCNatives(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass.extraModifiers().allBitsIn(J9AccClassHasJDBCNatives);
	}

	public static boolean isGCSpecial(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass.extraModifiers().allBitsIn(J9AccClassGCSpecial);
	}

	public static boolean hasFinalFields(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass.extraModifiers().allBitsIn(J9AccClassHasFinalFields);
	}

	public static boolean isHotSwappedOut(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass.extraModifiers().allBitsIn(J9AccClassHotSwappedOut);
	}

	public static boolean isDying(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass.extraModifiers().allBitsIn(J9AccClassDying);
	}

	public static boolean referenceWeek(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass.extraModifiers().allBitsIn(J9AccClassReferenceWeak);
	}

	public static boolean referenceSoft(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass. extraModifiers().allBitsIn(J9AccClassReferenceSoft);
	}

	public static boolean referencePhantom(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass.extraModifiers().allBitsIn(J9AccClassReferencePhantom);
	}

	public static boolean finalizeNeeded(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass. extraModifiers().allBitsIn(J9AccClassFinalizeNeeded);
	}

	public static boolean isClonable(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass.extraModifiers().allBitsIn(J9AccClassCloneable);
	}
	
	public static boolean isAnonymousClass(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass.extraModifiers().allBitsIn(J9AccClassAnonClass);
	}

	public static boolean isSealed(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass.extraModifiers().allBitsIn(J9AccSealed);
	}

	public static boolean hasLoadableDescriptorsAttribute(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass.optionalFlags().allBitsIn(J9_ROMCLASS_OPTINFO_LOADABLEDESCRIPTORS_ATTRIBUTE);
	}

	public static boolean hasImplicitCreationAttribute(J9ROMClassPointer romclass) throws CorruptDataException {
		return romclass.optionalFlags().allBitsIn(J9_ROMCLASS_OPTINFO_IMPLICITCREATION_ATTRIBUTE);
	}
}
