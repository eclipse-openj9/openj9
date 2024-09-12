/*
 * Copyright IBM Corp. and others 2010
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.extensions;

import java.io.PrintStream;
import java.util.List;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.tools.ddrinteractive.BaseStructureFormatter;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.FormatWalkResult;
import com.ibm.j9ddr.tools.ddrinteractive.IFieldFormatter;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.ObjectModel;
import com.ibm.j9ddr.vm29.pointer.DoublePointer;
import com.ibm.j9ddr.vm29.pointer.FloatPointer;
import com.ibm.j9ddr.vm29.pointer.I16Pointer;
import com.ibm.j9ddr.vm29.pointer.I32Pointer;
import com.ibm.j9ddr.vm29.pointer.I64Pointer;
import com.ibm.j9ddr.vm29.pointer.ObjectReferencePointer;
import com.ibm.j9ddr.vm29.pointer.U16Pointer;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ArrayClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9IndexableObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9IndexableObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.pointer.helper.PrintObjectFieldsHelper;
import com.ibm.j9ddr.vm29.pointer.helper.ValueTypeHelper;
import com.ibm.j9ddr.vm29.types.U32;

/**
 * Custom structure formatter for J9Object and J9Indexable object
 */
public class J9ObjectStructureFormatter extends BaseStructureFormatter
{
	public static final int DEFAULT_ARRAY_FORMAT_BEGIN = 0;
	public static final int DEFAULT_ARRAY_FORMAT_END = 16;

	@Override
	public FormatWalkResult format(String type, long address, PrintStream out,
			Context context, List<IFieldFormatter> fieldFormatters, String[] extraArgs)
	{
		if (type.equalsIgnoreCase("j9object") || type.equalsIgnoreCase("j9indexableobject")) {
			J9ClassPointer clazz = null;
			J9ObjectPointer object = null;

			try {
				object = J9ObjectPointer.cast(address);
				clazz = J9ObjectHelper.clazz(object);

				if (clazz.isNull()) {
					out.println("<can not read RAM class address>");
					return FormatWalkResult.STOP_WALKING;
				}

				boolean isArray = J9ClassHelper.isArrayClass(clazz);
				String className = J9UTF8Helper.stringValue(clazz.romClass().className());

				U8Pointer dataStart =  U8Pointer.cast(object).add(ObjectModel.getHeaderSize(object));

				if (className.equals("java/lang/String")) {
					formatStringObject(out, 0, clazz, dataStart, object, address);
				} else if (isArray) {
					if (ValueTypeHelper.getValueTypeHelper().isJ9ClassIsFlattened(clazz)
						&& (extraArgs.length > 0)
						&& (extraArgs[0].startsWith("["))
					) {
						formatObject(out, J9ArrayClassPointer.cast(clazz).componentType(), dataStart, object, address, extraArgs);
					} else {
						int begin = DEFAULT_ARRAY_FORMAT_BEGIN;
						int end = DEFAULT_ARRAY_FORMAT_END;
						if (extraArgs.length > 0) {
							begin = Integer.parseInt(extraArgs[0]);
						}
	
						if (extraArgs.length > 1) {
							end = Integer.parseInt(extraArgs[1]);
						}
	
						formatArrayObject(out, clazz, dataStart, J9IndexableObjectPointer.cast(object), begin, end);
					}
				} else {
					formatObject(out, clazz, dataStart, object, address, extraArgs);
				}
			} catch (MemoryFault ex2) {
				out.println("Unable to read object clazz at "
						+ (object == null ? "(null)" : object.getHexAddress())
						+ " (clazz = "
						+ (clazz == null ? "(null)" : clazz.getHexAddress())
						+ ")");
			} catch (CorruptDataException ex) {
				out.println("Error for ");
				ex.printStackTrace(out);
			}
			return FormatWalkResult.STOP_WALKING;
		} else {
			return FormatWalkResult.KEEP_WALKING;
		}
	}

	private void formatObject(PrintStream out, J9ClassPointer localClazz, U8Pointer dataStart, J9ObjectPointer localObject, long address, String[] extraArgs) throws CorruptDataException
	{
		String[] nestHierarchy = null;

		if (ValueTypeHelper.getValueTypeHelper().areValueTypesSupported()) {
			nestHierarchy = extraArgs.length == 0 ? null : extraArgs[0].split("\\.");
		}

		if (null != nestHierarchy) {
			out.format("!J9Object %s %s {%n", localObject.getHexAddress(), extraArgs[0]);
		} else {
			out.format("!J9Object %s {%n", localObject.getHexAddress());
		}

		PrintObjectFieldsHelper.printJ9ObjectFields(out, 1, localClazz, dataStart, localObject, address, nestHierarchy, false);
		out.println("}");
	}

	private void formatArrayObject(PrintStream out, J9ClassPointer localClazz, U8Pointer dataStart, J9IndexableObjectPointer localObject, int begin, int end) throws CorruptDataException
	{
		boolean isIndexableDataAddrPresent;
		try {
			J9JavaVMPointer javaVM = J9RASHelper.getVM(DataType.getJ9RASPointer());
			isIndexableDataAddrPresent = (J9BuildFlags.J9VM_ENV_DATA64 && !javaVM.isIndexableDataAddrPresent().isZero());
		} catch (CorruptDataException | NoSuchFieldException e) {
			isIndexableDataAddrPresent = false;
		}

		String className = J9IndexableObjectHelper.getClassName(localObject);

		out.format("!J9IndexableObject %s {%n", localObject.getHexAddress());

		/* print individual fields */
		out.format("    struct J9Class* clazz = !j9arrayclass 0x%X   // %s%n", localClazz.getAddress(), className);
		out.format("    Object flags = %s;%n", J9IndexableObjectHelper.flags(localObject).getHexValue());

		U32 size = J9IndexableObjectHelper.size(localObject);

		if (size.anyBitsIn(0x80000000)) {
			out.format("    U_32 size = %s; // Size exceeds Integer.MAX_VALUE!%n", size.getHexValue());
		} else {
			out.format("    U_32 size = %s;%n", size.getHexValue());
		}

		/* if IndexableDataAddrPresent in header of ArrayObject, output DataAddr */
		if (isIndexableDataAddrPresent) {
			VoidPointer dataAddr;
			try {
				dataAddr = J9IndexableObjectHelper.getDataAddrForIndexable(localObject);
				if (dataAddr.isNull()) {
					dataAddr = null;
				}
			} catch (NoSuchFieldException e) {
				dataAddr = null;
			}
			out.format("    U_64 DataAddr = %s;%n", (dataAddr == null) ? "NULL" : dataAddr.getHexAddress());
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
					PrintObjectFieldsHelper.padding(out, tabLevel);
					VoidPointer slot = J9IndexableObjectHelper.getElementEA(array, index, 1);
					out.format("[%d] = %3d, 0x%02x%n", index, U8Pointer.cast(slot).at(0).longValue(), U8Pointer.cast(slot).at(0).longValue());
				}
				break;
			case 'C':
				for (int index = begin; index < finish; index++) {
					PrintObjectFieldsHelper.padding(out, tabLevel);
					VoidPointer slot = J9IndexableObjectHelper.getElementEA(array, index, 2);
					long value = U16Pointer.cast(slot).at(0).longValue();
					out.format("[%d] = %5d, 0x%2$04x, '%c'%n", index, value, (char)value);
				}
				break;
			case 'S':
				for (int index = begin; index < finish; index++) {
					PrintObjectFieldsHelper.padding(out, tabLevel);
					VoidPointer slot = J9IndexableObjectHelper.getElementEA(array, index, 2);
					out.format("[%d] = %6d, 0x%04x%n", index, I16Pointer.cast(slot).at(0).longValue(), U16Pointer.cast(slot).at(0).longValue());
				}
				break;
			case 'I':
			case 'F':
				for (int index = begin; index < finish; index++) {
					PrintObjectFieldsHelper.padding(out, tabLevel);
					VoidPointer slot = J9IndexableObjectHelper.getElementEA(array, index, 4);
					out.format("[%d] = %10d, 0x%08x, %8.8fF%n", index, I32Pointer.cast(slot).at(0).longValue(), U32Pointer.cast(slot).at(0).longValue(), FloatPointer.cast(slot).floatAt(0));
				}
				break;

			case 'J':
			case 'D':
				for (int index = begin; index < finish; index++) {
					PrintObjectFieldsHelper.padding(out, tabLevel);
					VoidPointer slot = J9IndexableObjectHelper.getElementEA(array, index, 8);
					out.format("[%d] = %2d, 0x%016x, %8.8fF%n", index, I64Pointer.cast(slot).at(0).longValue(), I64Pointer.cast(slot).at(0).longValue(), DoublePointer.cast(slot).doubleAt(0));
				}
				break;
			case 'L':
			case '[':
				if (ValueTypeHelper.getValueTypeHelper().isJ9ClassIsFlattened(localClazz)) {
					formatFlattenedObjectArray(out, tabLevel, begin, finish, array);
				} else {
					formatReferenceObjectArray(out, tabLevel, localClazz, begin, finish, array);
				}
				break;
			}
		}

		/* check if it just printed a range; if so, give cmd to see all data */
		arraySize = J9IndexableObjectHelper.size(array);
		if (begin > 0 || arraySize.longValue() > finish ) {
			out.format("To print entire range: !j9indexableobject %s %d %d%n", array.getHexAddress(), 0, arraySize.longValue());
		}
	}

	private void formatReferenceObjectArray(PrintStream out, int tabLevel, J9ClassPointer localClazz, int begin, int finish, J9IndexableObjectPointer array) throws CorruptDataException
	{
		for (int index = begin; index < finish; index++) {
			VoidPointer slot = J9IndexableObjectHelper.getElementEA(array, index, (int) ObjectReferencePointer.SIZEOF);
			long pointer;
			if (J9ObjectHelper.compressObjectReferences) {
				pointer = U32Pointer.cast(slot).at(0).longValue();
			} else {
				pointer = DataType.getProcess().getPointerAt(slot.getAddress());
			}
			PrintObjectFieldsHelper.padding(out, tabLevel);
			if (pointer != 0) {
				out.format("[%d] = !fj9object 0x%x = !j9object 0x%x%n", index, pointer, ObjectReferencePointer.cast(slot).at(0).longValue());
			} else {
				out.format("[%d] = null%n", index);
			}
		}
	}

	private void formatFlattenedObjectArray(PrintStream out, int tabLevel, int begin, int finish, J9IndexableObjectPointer array) throws CorruptDataException
	{
		for (int index = begin; index < finish; index++) {
			PrintObjectFieldsHelper.padding(out, tabLevel);
			out.format("[%d] = !j9object 0x%x [%d]%n", index, array.getAddress(), index);
		}
	}

	private void formatStringObject(PrintStream out, int tabLevel, J9ClassPointer localClazz, U8Pointer dataStart, J9ObjectPointer localObject, long address) throws CorruptDataException
	{
		out.format("J9VMJavaLangString at %s {%n", localObject.getHexAddress());
		PrintObjectFieldsHelper.printJ9ObjectFields(out, tabLevel, localClazz, dataStart, localObject, address);
		PrintObjectFieldsHelper.padding(out, tabLevel);
		out.format("\"%s\"%n", J9ObjectHelper.stringValue(localObject));
		out.println("}");
	}
}
