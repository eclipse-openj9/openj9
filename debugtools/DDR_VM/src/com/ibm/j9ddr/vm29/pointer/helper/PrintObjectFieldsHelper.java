/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.J9FieldFlagObject;
import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.J9FieldSizeDouble;
import static com.ibm.j9ddr.vm29.structure.J9ROMFieldOffsetWalkState.J9VM_FIELD_OFFSET_WALK_INCLUDE_HIDDEN;
import static com.ibm.j9ddr.vm29.structure.J9ROMFieldOffsetWalkState.J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE;

import java.io.PrintStream;
import java.util.Arrays;
import java.util.Iterator;
import java.util.regex.Pattern;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffset;
import com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffsetIterator;
import com.ibm.j9ddr.vm29.j9.ObjectModel;
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.I32Pointer;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.U64Pointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ArrayClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMFieldShapePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9UTF8Pointer;
import com.ibm.j9ddr.vm29.structure.J9Object;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.UDATA;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;


/**
 * Helper class to print j9object fields. Also contains helper functions
 * for formatting output.
 */
public class PrintObjectFieldsHelper {
	private static ValueTypeHelper valueTypeHelper = ValueTypeHelper.getValueTypeHelper();
	private static final String PADDING = "\t";

	/**
	 * Prints all the j9object fields
	 *
	 * @param out output stream
	 * @param tabLevel indicates the starting tab level
	 * @param localClazz the class of the j9object
	 * @param dataStart start location of the j9object
	 * @param localObject pointer to the j9object
	 * @param address address of the object being printed
	 * @throws CorruptDataException
	 */
	public static void printJ9ObjectFields(PrintStream out, int tabLevel, J9ClassPointer localClazz, U8Pointer dataStart, J9ObjectPointer localObject, long address) throws CorruptDataException
	{
		printJ9ObjectFields(out, tabLevel, localClazz, dataStart, localObject, address, null, false);
	}

	/**
	 * Prints all the j9object fields
	 *
	 * @param out The output stream
	 * @param tabLevel Indicates the starting tab level
	 * @param localClazz The class of the j9object
	 * @param dataStart The start location of the object. If printing a nested
	 * type this is the effect address of the nested type
	 * @param localObject The pointer to the j9object
	 * @param address The address of the object being printed. If printing a
	 * nested type this is the address of the top level container
	 * @param nestingHierarchy This is an array of field names that specifies
	 * which nested type contained in the top level container to be printed
	 * The first array element is the name of a nested field in the top level
	 * container. The second array element is the name of a nested field that
	 * is a member of the of the nested field specified  in the first array
	 * element. And so on...
	 * @param showNestedFields Specifies if the entire structure of nested
	 * fields should be outputted
	 * @throws CorruptDataException
	 */
	public static void printJ9ObjectFields(PrintStream out, int tabLevel, J9ClassPointer localClazz, U8Pointer dataStart, J9ObjectPointer localObject, long address, String[] nestingHierarchy, boolean showNestedFields) throws CorruptDataException
	{
		long superclassIndex;
		long depth;
		J9ClassPointer previousSuperclass = J9ClassPointer.NULL;
		boolean lockwordPrinted = false;
		UDATA nestedFieldOffset = new UDATA(0);
		boolean flatObject = false;
		J9ClassPointer nestedClassHierarchy[] = null;
		boolean showObjectHeader = tabLevel <= 1; /* This is true when !flatobject is specified are currently printing a nested structure */

		if ((null != nestingHierarchy) && (nestingHierarchy.length > 0)) {
			flatObject = true;
		}

		/* Given we have a flatObject, conditionally increment ahead to the correct object index if we have a flattened array, 
		 * and skip ahead to the specified field by its offset.
		 */
		if (flatObject) {
			nestedClassHierarchy = valueTypeHelper.findNestedClassHierarchy(localClazz, nestingHierarchy);
			J9ClassPointer clazz = null;

			U32 flags = new U32(J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE | J9VM_FIELD_OFFSET_WALK_INCLUDE_HIDDEN);
			for (int i = 0; i < nestedClassHierarchy.length - 1; i++) {
				clazz = nestedClassHierarchy[i];
				if ((i > 0) && valueTypeHelper.classRequires4BytePrePadding(clazz)) {
					/* decrement by 4bytes since the nested type is pre-padded */
					nestedFieldOffset = nestedFieldOffset.sub(4);
				}
				
				depth = J9ClassHelper.classDepth(clazz).longValue();
				boolean found = false;

				if (J9ClassHelper.isArrayClass(clazz)) {
					int index = Integer.parseInt(nestingHierarchy[0].substring(1, nestingHierarchy[0].length() - 1));
					int stride = J9ArrayClassPointer.cast(clazz).flattenedElementSize().intValue();
					dataStart = dataStart.add(index * stride);
				} else {
					for (superclassIndex = 0; (superclassIndex <= depth) && !found; superclassIndex++) {
						J9ClassPointer superclass;
						if (superclassIndex == depth) {
							superclass = clazz;
						} else {
							superclass = J9ClassPointer.cast(clazz.superclasses().at(superclassIndex));
						}
						Iterator<J9ObjectFieldOffset> iterator = J9ObjectFieldOffsetIterator.J9ObjectFieldOffsetIteratorFor(superclass.romClass(), clazz, previousSuperclass, flags);

						while (iterator.hasNext()) {
							J9ObjectFieldOffset result = iterator.next();
							if (result.getName().equals(nestingHierarchy[i])) {
								nestedFieldOffset = nestedFieldOffset.add(result.getOffsetOrAddress());
								found = true;
								break;
							}
						}
						previousSuperclass = superclass;
					}
				}
			}

			localClazz = nestedClassHierarchy[nestedClassHierarchy.length - 1];
			dataStart = dataStart.add(nestedFieldOffset);
			previousSuperclass = J9ClassPointer.NULL;
		}
		if (showObjectHeader) {
			if (flatObject) {
				padding(out, tabLevel);
				out.format("// EA = %s, offset in top level container = %d;%n", dataStart.getHexAddress(), dataStart.sub(address).getAddress());
			}

			/* print individual fields */
			J9UTF8Pointer classNameUTF = localClazz.romClass().className();
			padding(out, tabLevel);
			out.format("struct J9Class* clazz = !j9class 0x%X // %s%n", localClazz.getAddress(), J9UTF8Helper.stringValue(classNameUTF));
			padding(out, tabLevel);
			out.format("Object flags = %s;%n", J9ObjectHelper.flags(localObject).getHexValue());
		}

		depth = J9ClassHelper.classDepth(localClazz).longValue();
		for (superclassIndex = 0; superclassIndex <= depth; superclassIndex++) {
			J9ClassPointer superclass;
			if (superclassIndex == depth) {
				superclass = localClazz;
			} else {
				superclass = J9ClassPointer.cast(localClazz.superclasses().at(superclassIndex));
			}

			U32 flags = new U32(J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE | J9VM_FIELD_OFFSET_WALK_INCLUDE_HIDDEN);
			Iterator<J9ObjectFieldOffset> iterator = J9ObjectFieldOffsetIterator.J9ObjectFieldOffsetIteratorFor(superclass.romClass(), localClazz, previousSuperclass, flags);

			while (iterator.hasNext()) {
				J9ObjectFieldOffset result = iterator.next();
				boolean printField = true;
				boolean isHiddenField = result.isHidden();
				boolean isLockword = isHiddenField && result.getOffsetOrAddress().add(J9ObjectHelper.headerSize()).eq(superclass.lockOffset());

				if (isLockword) {
					/* Print the lockword field if it is indeed the lockword for this instanceClass and we haven't printed it yet. */
					printField = !lockwordPrinted && localClazz.lockOffset().eq(superclass.lockOffset());
					if (printField) {
						lockwordPrinted = true;
					}
				}


				if (printField) {
					if (showNestedFields) {
						J9ClassPointer containerClazz = null;
						if (null == nestedClassHierarchy) {
							/* if nestedClassHierarchy is null then this class is the container class */
							containerClazz = localClazz;
						} else {
							int containerClassIndex = Pattern.matches("\\[\\d+\\]", nestingHierarchy[0]) ? 1 : 0;
							containerClazz = nestedClassHierarchy[containerClassIndex];
						}
						printNestedObjectField(out, tabLevel, localClazz, dataStart, superclass, result, address, nestingHierarchy, containerClazz);
					} else {
						printObjectField(out, tabLevel, localClazz, dataStart, superclass, result, address, nestingHierarchy);
					}

					out.println();
				}
			}
			previousSuperclass = superclass;
		}
	}

	private static void printObjectField(PrintStream out, int tabLevel, J9ClassPointer localClazz, U8Pointer dataStart, J9ClassPointer fromClass, J9ObjectFieldOffset objectFieldOffset, long address, String[] nestingHierarchy) throws CorruptDataException
	{
		J9ROMFieldShapePointer fieldShape = objectFieldOffset.getField();
		UDATA fieldOffset = objectFieldOffset.getOffsetOrAddress();
		boolean isHiddenField = objectFieldOffset.isHidden();
		boolean containerIsFlatObject = (nestingHierarchy != null) && (nestingHierarchy.length > 0);
		String className = J9ClassHelper.getName(fromClass);
		String fieldName = J9UTF8Helper.stringValue(fieldShape.nameAndSignature().name());
		String fieldSignature = J9UTF8Helper.stringValue(fieldShape.nameAndSignature().signature());
		boolean fieldIsFlattened = valueTypeHelper.isFieldInClassFlattened(localClazz, fieldName);

		if (containerIsFlatObject && valueTypeHelper.classRequires4BytePrePadding(localClazz)) {
			/* If container has pre-padding the dataStart was adjusted to reflect this. 
			 * Make sure to print out the real offset (the one without pre-padding.
			 */
			fieldOffset = fieldOffset.sub(4);
		}
		
		padding(out, tabLevel);
		out.format("%s %s = ", fieldSignature, fieldName);

		U8Pointer valuePtr = dataStart.add(fieldOffset);
		
		if (fieldShape.modifiers().anyBitsIn(J9FieldSizeDouble)) {
			out.print(U64Pointer.cast(valuePtr).at(0).getHexValue());
		} else if (fieldShape.modifiers().anyBitsIn(J9FieldFlagObject)) {
			if (fieldIsFlattened) {
				J9ObjectPointer object = J9ObjectPointer.cast(address);
				StringBuilder hierarchy = new StringBuilder("");

				if (containerIsFlatObject) {
					for (int i = 0; i < nestingHierarchy.length; i++) {
						hierarchy.append(nestingHierarchy[i]);
						hierarchy.append(".");
					}
				}

				hierarchy.append(fieldName);
				out.format("!j9object 0x%x %s", object.getAddress(), hierarchy);
			} else {
				AbstractPointer ptr = J9ObjectHelper.compressObjectReferences ? U32Pointer.cast(valuePtr) : UDATAPointer.cast(valuePtr);
				out.format("!fj9object 0x%x", ptr.at(0).longValue());
			}
		} else {
			out.print(I32Pointer.cast(valuePtr).at(0).getHexValue());
		}
		if (fieldIsFlattened) {
			out.format(" (offset = %d) (%s)", fieldOffset.longValue(), fieldSignature.substring(1, fieldSignature.length() - 1));
		} else {
			out.format(" (offset = %d) (%s)", fieldOffset.longValue(), className);
		}

		if (isHiddenField) {
			out.print(" <hidden>");
		}
	}

	private static void printNestedObjectField(PrintStream out, int tabLevel, J9ClassPointer localClazz, U8Pointer dataStart, J9ClassPointer fromClass, J9ObjectFieldOffset objectFieldOffset, long address, String[] nestingHierarchy, J9ClassPointer containerClazz) throws CorruptDataException
	{
		J9ROMFieldShapePointer fieldShape = objectFieldOffset.getField();
		UDATA fieldOffset = objectFieldOffset.getOffsetOrAddress();
		boolean containerIsFlatObject = (nestingHierarchy != null) && (nestingHierarchy.length > 0);
		boolean isHiddenField = objectFieldOffset.isHidden();
		String fieldName = J9UTF8Helper.stringValue(fieldShape.nameAndSignature().name());
		String fieldSignature = J9UTF8Helper.stringValue(fieldShape.nameAndSignature().signature());
		boolean fieldIsFlattened = valueTypeHelper.isFieldInClassFlattened(localClazz, fieldName);
		
		padding(out, tabLevel);
		
		if (containerIsFlatObject && valueTypeHelper.classRequires4BytePrePadding(localClazz)) {
			/* If container has pre-padding the dataStart was adjusted to reflect this. 
			 * Make sure to print out the real offset (the one without pre-padding.
			 */
			fieldOffset = fieldOffset.sub(4);
		}
		U8Pointer valuePtr = dataStart.add(fieldOffset);
		
		if (fieldIsFlattened) {
			out.format("%s %s { // EA = %s (offset = %d)%n", fieldSignature.substring(1, fieldSignature.length() - 1), fieldName, valuePtr.getHexAddress(), fieldOffset.longValue());
		} else {
			out.format("%s %s = ", fieldSignature, fieldName);
		}

		if (fieldShape.modifiers().anyBitsIn(J9FieldSizeDouble)) {
			out.print(U64Pointer.cast(valuePtr).at(0).getHexValue());
		} else if (fieldShape.modifiers().anyBitsIn(J9FieldFlagObject)) {
			if (fieldIsFlattened) {
				String newNestingHierarchy[] = null;

				if (nestingHierarchy == null) {
					/* we are currently at the container level */
					newNestingHierarchy = new String[] {fieldName};
				} else {
					newNestingHierarchy = Arrays.copyOf(nestingHierarchy, nestingHierarchy.length + 1);
					newNestingHierarchy[nestingHierarchy.length] = fieldName;
				}

				J9ObjectPointer obj = J9ObjectPointer.cast(address);
				dataStart = U8Pointer.cast(obj).add(ObjectModel.getHeaderSize(obj));
				printJ9ObjectFields(out, tabLevel + 1, containerClazz, dataStart, null, address, newNestingHierarchy, true);

			} else {
				AbstractPointer ptr = J9ObjectHelper.compressObjectReferences ? U32Pointer.cast(valuePtr) : UDATAPointer.cast(valuePtr);
				out.format("!fj9object 0x%x%n", ptr.at(0).longValue());
			}
		} else {
			out.print(I32Pointer.cast(valuePtr).at(0).getHexValue());
		}

		if (fieldIsFlattened) {
			padding(out, tabLevel);
			out.print("}");
		}

		if (isHiddenField) {
			out.print(" <hidden>");
		}
	}

	public static void padding(PrintStream out, int level)
	{
		for (int i = 0; i < level; i++) {
			out.print(PADDING);
		}
	}
}
