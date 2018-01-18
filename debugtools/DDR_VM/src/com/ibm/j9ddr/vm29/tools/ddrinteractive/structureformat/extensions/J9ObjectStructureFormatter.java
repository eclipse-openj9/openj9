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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.extensions;

import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.J9FieldFlagObject;
import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.J9FieldSizeDouble;
import static com.ibm.j9ddr.vm29.structure.J9ROMFieldOffsetWalkState.J9VM_FIELD_OFFSET_WALK_INCLUDE_HIDDEN;
import static com.ibm.j9ddr.vm29.structure.J9ROMFieldOffsetWalkState.J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE;

import java.io.PrintStream;
import java.util.Iterator;
import java.util.List;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.tools.ddrinteractive.BaseStructureFormatter;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.FormatWalkResult;
import com.ibm.j9ddr.tools.ddrinteractive.IFieldFormatter;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffset;
import com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffsetIterator;
import com.ibm.j9ddr.vm29.j9.ObjectModel;
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.DoublePointer;
import com.ibm.j9ddr.vm29.pointer.FloatPointer;
import com.ibm.j9ddr.vm29.pointer.I16Pointer;
import com.ibm.j9ddr.vm29.pointer.I32Pointer;
import com.ibm.j9ddr.vm29.pointer.I64Pointer;
import com.ibm.j9ddr.vm29.pointer.ObjectReferencePointer;
import com.ibm.j9ddr.vm29.pointer.U16Pointer;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.U64Pointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9IndexableObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMFieldShapePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9UTF8Pointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9IndexableObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.structure.J9Object;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * Custom structure formatter for J9Object and J9Indexable object
 */
public class J9ObjectStructureFormatter extends BaseStructureFormatter 
{
	public static final int DEFAULT_ARRAY_FORMAT_BEGIN = 0;
	public static final int DEFAULT_ARRAY_FORMAT_END = 16;
	public static final String PADDING = "      ";
	
	@Override
	public FormatWalkResult format(String type, long address, PrintStream out,
			Context context, List<IFieldFormatter> fieldFormatters, String[] extraArgs) 
	{
		if (type.equalsIgnoreCase("j9object") || type.equalsIgnoreCase("j9indexableobject")) {
			J9ClassPointer clazz = null;
			J9ObjectPointer object = null;
			
			try {
				boolean isArray;
				String className;
				object = J9ObjectPointer.cast(address);
				clazz = J9ObjectHelper.clazz(object);
				
				if (clazz.isNull()) {
					out.println("<can not read RAM class address>");
					return FormatWalkResult.STOP_WALKING;
				}
	
				isArray = J9ClassHelper.isArrayClass(clazz);
				className = J9UTF8Helper.stringValue(clazz.romClass().className());
				
				U8Pointer dataStart =  U8Pointer.cast(object).add(ObjectModel.getHeaderSize(object));
				
				if (className.equals("java/lang/String")) {
					formatStringObject(out, 0, clazz, dataStart, object);
				} else if (isArray) {
					int begin = DEFAULT_ARRAY_FORMAT_BEGIN;
					int end = DEFAULT_ARRAY_FORMAT_END;
					if (extraArgs.length > 0) {
						begin = Integer.parseInt(extraArgs[0]);
					}
					
					if (extraArgs.length > 1) {
						end = Integer.parseInt(extraArgs[1]);
					}

					formatArrayObject(out, clazz, dataStart, J9IndexableObjectPointer.cast(object), begin, end);
				} else {
					formatObject(out, clazz, dataStart, object);
				}
			} catch (MemoryFault ex2) {
				out.println("Unable to read object clazz at " + object.getHexAddress() + " (clazz = "  + clazz.getHexAddress() + ")");
			} catch (CorruptDataException ex) {
				out.println("Error for ");
				ex.printStackTrace(out);
			}
			return FormatWalkResult.STOP_WALKING;
		} else {
			return FormatWalkResult.KEEP_WALKING;
		}
	}
	
	private void formatObject(PrintStream out, J9ClassPointer localClazz, U8Pointer dataStart, J9ObjectPointer localObject) throws CorruptDataException
	{
		out.print(String.format("!J9Object %s {", localObject.getHexAddress()));
		out.println();

		printJ9ObjectFields(out, 1, localClazz, dataStart, localObject);
		out.println("}");
	}
	
	private void formatArrayObject(PrintStream out, J9ClassPointer localClazz, U8Pointer dataStart, J9IndexableObjectPointer localObject, int begin, int end) throws CorruptDataException 
	{
		String className = J9IndexableObjectHelper.getClassName(localObject);
		
		out.print(String.format("!J9IndexableObject %s {", localObject.getHexAddress()));
		out.println();
		
		/* print individual fields */
		out.println(String.format("    struct J9Class* clazz = !j9arrayclass 0x%X   // %s", localClazz.getAddress(), className));
		out.println(String.format("    Object flags = %s;", J9IndexableObjectHelper.flags(localObject).getHexValue()));
		
		U32 size = J9IndexableObjectHelper.size(localObject);
		
		if (!J9BuildFlags.thr_lockNursery) {
			out.println(String.format("    j9objectmonitor_t monitor = %s;", J9IndexableObjectHelper.monitor(localObject).getHexValue()));
		}
		
		if (size.anyBitsIn(0x80000000)) {
			out.println(String.format("    U_32 size = %s; // Size exceeds Integer.MAX_VALUE!", size.getHexValue()));
		} else {
			out.println(String.format("    U_32 size = %s;", size.getHexValue()));
		}
		
		printSubArrayType(out, 1, localClazz, dataStart, begin, end, localObject);
		out.println("}");
	}
	
	void 
	printSubArrayType(PrintStream out, int tabLevel, J9ClassPointer localClazz, U8Pointer dataStart, int begin, int end, J9IndexableObjectPointer array) throws CorruptDataException
	{
		U32 arraySize = J9IndexableObjectHelper.size(array);
		if (arraySize.anyBitsIn(0x80000000)) {			
			arraySize = new U32(Integer.MAX_VALUE);
		}
		int finish = Math.min(arraySize.intValue(), end);
		
		if (begin < finish) {
			String className = J9IndexableObjectHelper.getClassName(array);
			char signature = className.charAt(1);
			switch (signature) {

			case 'B':
			case 'Z':
				for (int index = begin; index < finish; index++) {
					padding(out, tabLevel);
					VoidPointer slot = J9IndexableObjectHelper.getElementEA(array, index, 1);
					out.println(String.format("[%d] = %3d, 0x%02x", index, U8Pointer.cast(slot).at(0).longValue(), U8Pointer.cast(slot).at(0).longValue()));
				}
				break;
			case 'C':
				for (int index = begin; index < finish; index++) {
					padding(out, tabLevel);
					VoidPointer slot = J9IndexableObjectHelper.getElementEA(array, index, 2);
					long value = U16Pointer.cast(slot).at(0).longValue();
					out.println(String.format("[%d] = %5d, 0x%2$04x, '%c'", index, value, (char)value));
				}
				break;
			case 'S':
				for (int index = begin; index < finish; index++) {
					padding(out, tabLevel);
					VoidPointer slot = J9IndexableObjectHelper.getElementEA(array, index, 2);
					out.println(String.format("[%d] = %6d, 0x%04x", index, I16Pointer.cast(slot).at(0).longValue(), U16Pointer.cast(slot).at(0).longValue()));
				}
				break;
			case 'I':
			case 'F':
				for (int index = begin; index < finish; index++) {
					padding(out, tabLevel);
					VoidPointer slot = J9IndexableObjectHelper.getElementEA(array, index, 4);
					out.println(String.format("[%d] = %10d, 0x%08x, %8.8fF", index, I32Pointer.cast(slot).at(0).longValue(), U32Pointer.cast(slot).at(0).longValue(), FloatPointer.cast(slot).floatAt(0)));
				}
				break;

			case 'J':
			case 'D':
				for (int index = begin; index < finish; index++) {
					padding(out, tabLevel);
					VoidPointer slot = J9IndexableObjectHelper.getElementEA(array, index, 8);
					out.println(String.format("[%d] = %2d, 0x%016x, %8.8fF", index, I64Pointer.cast(slot).at(0).longValue(), I64Pointer.cast(slot).at(0).longValue(), DoublePointer.cast(slot).doubleAt(0)));
				}
				break;
			case 'L':
			case '[':
				for (int index = begin; index < finish; index++) {
					VoidPointer slot = J9IndexableObjectHelper.getElementEA(array, index, (int)ObjectReferencePointer.SIZEOF);
					if (slot.notNull()) {
						long compressedPtrValue;
						if (J9BuildFlags.gc_compressedPointers) {
							compressedPtrValue = I32Pointer.cast(slot).at(0).longValue();
						} else {
							compressedPtrValue = DataType.getProcess().getPointerAt(slot.getAddress());
						}
						padding(out, tabLevel);
						out.println(String.format("[%d] = !fj9object 0x%x = !j9object 0x%x", index, compressedPtrValue, ObjectReferencePointer.cast(slot).at(0).longValue()));
					} else {
						padding(out, tabLevel);
						out.println(String.format("[%d] = null", slot));
					}
				}
				break;
			}
		}
		
		/* check if it just printed a range; if so, give cmd to see all data */
		arraySize = J9IndexableObjectHelper.size(array);
		if (begin > 0 || arraySize.longValue() > finish ) {
			out.println(String.format("To print entire range: !j9indexableobject %s %d %d\n", array.getHexAddress(), 0, arraySize.longValue()));
		}
	}
	
	private void formatStringObject(PrintStream out, int tabLevel, J9ClassPointer localClazz, U8Pointer dataStart, J9ObjectPointer localObject) throws CorruptDataException 
	{
		out.println(String.format("J9VMJavaLangString at %s {\n", localObject.getHexAddress()));
		printJ9ObjectFields(out, tabLevel, localClazz, dataStart, localObject);
		padding(out, tabLevel);
		out.println(String.format("\"%s\"\n", J9ObjectHelper.stringValue(localObject)));
		out.println("}");		
	}
	
	private void printJ9ObjectFields(PrintStream out, int tabLevel, J9ClassPointer localClazz, U8Pointer dataStart, J9ObjectPointer localObject) throws CorruptDataException 
	{
		J9ClassPointer instanceClass = localClazz;
		long superclassIndex;
		long depth;
		J9ClassPointer previousSuperclass = J9ClassPointer.NULL;
		boolean lockwordPrinted = false;
		
		if (J9BuildFlags.thr_lockNursery) {
			lockwordPrinted = false;
		}
		
		/* print individual fields */
		J9UTF8Pointer classNameUTF = instanceClass.romClass().className();
		padding(out, tabLevel);		
		out.println(String.format("struct J9Class* clazz = !j9class 0x%X   // %s", localClazz.getAddress(), J9UTF8Helper.stringValue(classNameUTF)));
		padding(out, tabLevel);
		out.println(String.format("Object flags = %s;", J9ObjectHelper.flags(localObject).getHexValue()));
		
		if (!J9BuildFlags.thr_lockNursery) {
			UDATA lockword = J9ObjectHelper.monitor(localObject);
			if (lockword != null) {
				padding(out, tabLevel);
				out.println(String.format("j9objectmonitor_t monitor = %s;", lockword.getHexValue()));
			}
		}
		
		depth = J9ClassHelper.classDepth(instanceClass).longValue();
		for (superclassIndex = 0; superclassIndex <= depth; superclassIndex++) {
			J9ClassPointer superclass;
			if (superclassIndex == depth) {
				superclass = instanceClass;
			} else {
				superclass = J9ClassPointer.cast(instanceClass.superclasses().at(superclassIndex));
			}

			U32 flags = new U32(J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE | J9VM_FIELD_OFFSET_WALK_INCLUDE_HIDDEN);
			Iterator<J9ObjectFieldOffset> iterator = J9ObjectFieldOffsetIterator.J9ObjectFieldOffsetIteratorFor(superclass.romClass(), instanceClass, previousSuperclass, flags);

			while (iterator.hasNext()) 
			{
				J9ObjectFieldOffset result = iterator.next();
				boolean printField = true;
				boolean isHiddenField = result.isHidden();
				if (J9BuildFlags.thr_lockNursery) {
					boolean isLockword = (isHiddenField && ((result.getOffsetOrAddress().add(J9Object.SIZEOF).eq(superclass.lockOffset()))));
					
					if (isLockword) {
						/* Print the lockword field if it is indeed the lockword for this instanceClass and we haven't printed it yet. */
						printField = (!lockwordPrinted && (instanceClass.lockOffset().eq(superclass.lockOffset())));
						if (printField) {
							lockwordPrinted = true;
						}
					}
				}
				
				if (printField) {
					printObjectField(out, tabLevel, localClazz, dataStart, superclass, result);
					out.println();
				}
			}
			previousSuperclass = superclass;
		}
	}
	
	public void printObjectField(PrintStream out, int tabLevel, J9ClassPointer localClazz, U8Pointer dataStart, J9ClassPointer fromClass, J9ObjectFieldOffset objectFieldOffset) throws CorruptDataException 
	{
		J9ROMFieldShapePointer fieldShape = objectFieldOffset.getField();
		UDATA fieldOffset = objectFieldOffset.getOffsetOrAddress();
		boolean isHiddenField = objectFieldOffset.isHidden();
		
		String className = J9UTF8Helper.stringValue(fromClass.romClass().className());
		String fieldName = J9UTF8Helper.stringValue(fieldShape.nameAndSignature().name());
		String fieldSignature = J9UTF8Helper.stringValue(fieldShape.nameAndSignature().signature());
		
		U8Pointer valuePtr = dataStart;
		valuePtr = valuePtr.add(fieldOffset);

		padding(out, tabLevel);
		out.print(String.format("%s %s = ", fieldSignature, fieldName));
		
		if (fieldShape.modifiers().anyBitsIn(J9FieldSizeDouble)) {
			out.print(U64Pointer.cast(valuePtr).at(0).getHexValue());
		} else if (fieldShape.modifiers().anyBitsIn(J9FieldFlagObject)) {
			AbstractPointer ptr = J9BuildFlags.gc_compressedPointers ? U32Pointer.cast(valuePtr) : UDATAPointer.cast(valuePtr);
			out.print(String.format("!fj9object 0x%x", ptr.at(0).longValue()));
		} else {
			out.print(I32Pointer.cast(valuePtr).at(0).getHexValue());
		}
		
		out.print(String.format(" (offset=%d) (%s)", fieldOffset.longValue(), className));
				
		if (isHiddenField) {
			out.print(" <hidden>");
		}
	}
	
	private void padding(PrintStream out, int level) 
	{
		for (int i = 0; i < level; i++) {
			out.print(PADDING);
		}
	}
}
