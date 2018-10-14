/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.*;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMFieldShapePointer;
import com.ibm.j9ddr.vm29.structure.J9CfrClassFile;
import com.ibm.j9ddr.vm29.structure.J9ROMFieldShape;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.U64;
import com.ibm.j9ddr.vm29.types.UDATA;

public class J9ROMFieldShapeHelper {
	
	public static String getName(J9ROMFieldShapePointer fieldShapePointer) throws CorruptDataException 
	{
		return J9UTF8Helper.stringValue(fieldShapePointer.nameAndSignature().name());
	}
	
	public static String getSignature(J9ROMFieldShapePointer fieldShapePointer) throws CorruptDataException 
	{
		return J9UTF8Helper.stringValue(fieldShapePointer.nameAndSignature().signature());
	}
	
	public static String toString(J9ROMFieldShapePointer fieldShapePointer) throws CorruptDataException
	{
		return String.format("%s(%s, %s)", fieldShapePointer.getClass().getSimpleName(), getName(fieldShapePointer), getSignature(fieldShapePointer));
	}
	
	public static J9ROMFieldShapePointer next(J9ROMFieldShapePointer fieldShapePointer) throws CorruptDataException 
	{
		return J9ROMFieldShapePointer.cast(fieldShapePointer.addOffset(romFieldSizeOf(fieldShapePointer)));
	}
	
	public static UDATA romFieldSizeOf(J9ROMFieldShapePointer fieldShapePointer) throws CorruptDataException 
	{
		long size = J9ROMFieldShape.SIZEOF;
		UDATA modifiers = fieldShapePointer.modifiers();
		
		if ((modifiers.anyBitsIn(J9FieldFlagConstant))) {	
			size += ((modifiers.allBitsIn(J9FieldSizeDouble)) ? U64.SIZEOF : U32.SIZEOF);
		}

		if (modifiers.anyBitsIn(J9FieldFlagHasGenericSignature)) {
			size += U32.SIZEOF;
		}

		if (modifiers.anyBitsIn(J9FieldFlagHasFieldAnnotations)) {
			long bytesToPad = 0;
			//long annotationSize = U32Pointer.cast(address + size).at(0).longValue();
			long annotationSize = U32Pointer.cast(fieldShapePointer.addOffset(size)).at(0).longValue();
			/* add the length of annotation data */
			size += annotationSize;
			/* add the size of the size of the annotation data */
			size += U32.SIZEOF;

			bytesToPad = U32.SIZEOF - (annotationSize % U32.SIZEOF);
			if (U32.SIZEOF == bytesToPad) {
				bytesToPad = 0;
			}
			/* add the padding */
			size += bytesToPad;
		}

		if (modifiers.anyBitsIn(J9FieldFlagHasTypeAnnotations)) {
			long bytesToPad = 0;
			long annotationSize = U32Pointer.cast(fieldShapePointer.addOffset(size)).at(0).longValue();
			/* add the length of annotation data */
			size += annotationSize;
			/* add the size of the size of the annotation data */
			size += U32.SIZEOF;

			bytesToPad = U32.SIZEOF - (annotationSize % U32.SIZEOF);
			if (U32.SIZEOF == bytesToPad) {
				bytesToPad = 0;
			}
			/* add the padding */
			size += bytesToPad;
		}
		
		return new UDATA(size);
	}
	
	
	private static U32Pointer getFieldAnnotationLocation(J9ROMFieldShapePointer fieldShapePointer)
			throws CorruptDataException {
		long size = 0;
		
		if ((fieldShapePointer.modifiers().allBitsIn(J9FieldFlagConstant))) {	
			size += ((fieldShapePointer.modifiers().allBitsIn(J9FieldSizeDouble)) ? U64.SIZEOF : U32.SIZEOF);
		}

		if (fieldShapePointer.modifiers().allBitsIn(J9FieldFlagHasGenericSignature)) {
			size += U32.SIZEOF;
		}
		
		// move past J9ROMFieldShape struct and then add size in bytes
		U32Pointer fieldAnnotationOffset = U32Pointer.cast(fieldShapePointer.add(1)).addOffset(size);
		return fieldAnnotationOffset;
	}
	
	public static U32Pointer getFieldAnnotationsDataFromROMField(J9ROMFieldShapePointer fieldShapePointer) throws CorruptDataException 
	{
		if (!fieldShapePointer.modifiers().allBitsIn(J9FieldFlagHasFieldAnnotations)) {
			return U32Pointer.NULL;
		} else {
			return getFieldAnnotationLocation(fieldShapePointer);
		}
	}

	public static U32Pointer getFieldTypeAnnotationsDataFromROMField(J9ROMFieldShapePointer fieldShapePointer) throws CorruptDataException 
	{
		if (!fieldShapePointer.modifiers().allBitsIn(J9FieldFlagHasTypeAnnotations)) {
			return U32Pointer.NULL;
		}
		
		U32Pointer fieldAnnotations = getFieldAnnotationsDataFromROMField(fieldShapePointer);
		if (fieldAnnotations.isNull()) { /* type annotations are where the field annotations would have been */
			return getFieldAnnotationLocation(fieldShapePointer);
		} else { /* skip over the field annotation */
			return fieldAnnotations.add(fieldAnnotations.at(0));
		}
	}
	
	/**
	 * Gets the modifiers as returned from java.lang.reflect.Field.getModifiers(); (masking out all internal
	 * JVM modifiers)
	 * @param fieldShapePointer Field
	 * @return Modifier codes
	 * @throws CorruptDataException
	 */
	public static int getReflectModifiers(J9ROMFieldShapePointer fieldShapePointer) throws CorruptDataException
	{
		return fieldShapePointer.modifiers().bitAnd(J9CfrClassFile.CFR_FIELD_ACCESS_MASK).intValue();
	}
}
