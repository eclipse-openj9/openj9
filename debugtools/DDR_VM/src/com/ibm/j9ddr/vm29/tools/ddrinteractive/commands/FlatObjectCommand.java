/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;
import java.util.StringJoiner;

import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.FormatWalkResult;
import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffset;
import com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffsetIterator;
import com.ibm.j9ddr.vm29.j9.ObjectAccessBarrier;
import com.ibm.j9ddr.vm29.j9.ObjectModel;
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.I32Pointer;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.U64Pointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.pointer.helper.ValueTypeHelper;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMFieldShapePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9UTF8Pointer;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.UDATA;
import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.J9FieldFlagObject;
import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.J9FieldSizeDouble;
import static com.ibm.j9ddr.vm29.structure.J9ROMFieldOffsetWalkState.J9VM_FIELD_OFFSET_WALK_INCLUDE_HIDDEN;
import static com.ibm.j9ddr.vm29.structure.J9ROMFieldOffsetWalkState.J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE;
import com.ibm.j9ddr.vm29.structure.J9Object;

public class FlatObjectCommand extends Command {

	public FlatObjectCommand() {
		addCommand("flatobject", "<addressOfContainer><,fieldNum,...>",
				"Display the flattenedj9object given a container address.");
	}

	private void printHelp(PrintStream out) {
		out.append("Usage: \n");
		out.append("  !flatobject <addressOfContainer>,<,fieldNum,...>\n");
	}

	public void run(String command, String[] args, Context context, PrintStream out)
			throws DDRInteractiveCommandException {
		if (args.length == 0) {
			printHelp(out);
			return;
		}

		String[] queries = args[0].split(",");
		startWalk(command, queries, context, out);
	}

	private void startWalk(String command, String[] args, Context context, PrintStream out)
			throws DDRInteractiveCommandException {
		long address = CommandUtils.parsePointer(args[0], J9BuildFlags.env_data64);
		J9ClassPointer clazz = null;
		J9ObjectPointer object = null;
		try {
			boolean isArray;
			String className;
			object = J9ObjectPointer.cast(address);
			clazz = J9ObjectHelper.clazz(object);

			if (clazz.isNull()) {
				out.println("<can not read RAM class address>");
				return;
			}

			className = J9UTF8Helper.stringValue(clazz.romClass().className());

			U8Pointer dataStart = U8Pointer.cast(object).add(ObjectModel.getHeaderSize(object));

			formatObject(out, clazz, dataStart, object, args);
		} catch (MemoryFault ex2) {
			out.println("Unable to read object clazz at " + object.getHexAddress() + " (clazz = "
					+ clazz.getHexAddress() + ")");
		} catch (CorruptDataException ex) {
			out.println("Error for ");
			ex.printStackTrace(out);
		}
	}

	private void formatObject(PrintStream out, J9ClassPointer localClazz, U8Pointer dataStart,
			J9ObjectPointer localObject, String[] args)
			throws CorruptDataException {
		String indexArgs = "";
		for (int i = 1; i < args.length; i++) {
			indexArgs += ("," + args[i]);
		}
		out.print(String.format("!flatobject 0x%x%s {", localObject.getAddress(), indexArgs));
		out.println();
		

		printFlatObjectFields(out, 1, localClazz, dataStart, args);
		out.println("}");
	}

	private void printFlatObjectFields(PrintStream out, int tabLevel, J9ClassPointer localClazz, U8Pointer dataStart,
			String[] args)
			throws CorruptDataException {

		long superclassIndex;
		long depth;
		J9ClassPointer previousSuperclass = J9ClassPointer.NULL;

		/* print individual fields */
		boolean lockwordPrinted = false;

		if (J9BuildFlags.thr_lockNursery) {
			lockwordPrinted = false;
		}

		List<Integer> indexs = new ArrayList<>();
		List<Long> offsets = new ArrayList<>();

		parseIndexs(args, indexs);
		J9ClassPointer instanceClass = offsetEachLevel(localClazz, indexs, offsets);
		long sumOffsets = totalOffsets(offsets);
		dataStart = dataStart.add(sumOffsets);

		J9UTF8Pointer classNameUTF = instanceClass.romClass().className();
		padding(out, tabLevel);
		out.println(String.format("//continer offset %d", sumOffsets));
		
		padding(out, tabLevel);
		out.println(String.format("struct J9Class* clazz = !j9class 0x%X   // %s", instanceClass.getAddress(),
				J9UTF8Helper.stringValue(classNameUTF)));

		depth = J9ClassHelper.classDepth(instanceClass).longValue();
		for (superclassIndex = 0; superclassIndex <= depth; superclassIndex++) {
			J9ClassPointer superclass;
			if (superclassIndex == depth) {
				superclass = instanceClass;
			} else {
				superclass = J9ClassPointer.cast(instanceClass.superclasses().at(superclassIndex));
			}

			U32 flags = new U32(J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE | J9VM_FIELD_OFFSET_WALK_INCLUDE_HIDDEN);
			Iterator<J9ObjectFieldOffset> iterator = J9ObjectFieldOffsetIterator
					.J9ObjectFieldOffsetIteratorFor(superclass.romClass(), instanceClass, previousSuperclass, flags);

			int index = 0;
			while (iterator.hasNext()) {
				J9ObjectFieldOffset result = iterator.next();
				boolean printField = true;
				boolean isHiddenField = result.isHidden();
				
				if (J9BuildFlags.thr_lockNursery) {
					boolean isLockword = (isHiddenField
							&& ((result.getOffsetOrAddress().add(J9Object.SIZEOF).eq(superclass.lockOffset()))));
				
					if (isLockword) {
						printField = (!lockwordPrinted && (instanceClass.lockOffset().eq(superclass.lockOffset())));
						if (printField) {
							lockwordPrinted = true;
						}
					}
				}
				
				if (printField) {
					printFlatObjectField(out, tabLevel, localClazz, dataStart, superclass, result, args,
							index);
					out.println();
				}
				index++;
			}
			previousSuperclass = superclass;
		}
	}

	private void printFlatObjectField(PrintStream out, int tabLevel, J9ClassPointer localClazz, U8Pointer dataStart,
			J9ClassPointer fromClass, J9ObjectFieldOffset objectFieldOffset, String[] args,
			int index)
			throws CorruptDataException {
		J9ROMFieldShapePointer fieldShape = objectFieldOffset.getField();
		UDATA fieldOffset = objectFieldOffset.getOffsetOrAddress();
		boolean isHiddenField = objectFieldOffset.isHidden();

		String className = J9UTF8Helper.stringValue(fromClass.romClass().className());
		String fieldName = J9UTF8Helper.stringValue(fieldShape.nameAndSignature().name());
		String fieldSignature = J9UTF8Helper.stringValue(fieldShape.nameAndSignature().signature());
		String fieldClassName = getClassNameFromSig(fieldSignature);

		U8Pointer valuePtr = dataStart;
		valuePtr = valuePtr.add(fieldOffset);

		padding(out, tabLevel);
		out.print(String.format("%s %s = ", fieldSignature, fieldName));

		if (fieldShape.modifiers().anyBitsIn(J9FieldSizeDouble)) {
			out.print(U64Pointer.cast(valuePtr).at(0).getHexValue());
		} else if (fieldShape.modifiers().anyBitsIn(J9FieldFlagObject)) {
			if (ValueTypeHelper.ifClassIsFlattened(fromClass.flattenedClassCache(), fieldClassName)) {
				out.print(String.format("!flatobject %s,%d", stringFromArgs(args), index));
			} else {
				AbstractPointer ptr = J9BuildFlags.gc_compressedPointers ? U32Pointer.cast(valuePtr)
						: UDATAPointer.cast(valuePtr);
				out.print(String.format("!fj9object 0x%x", ptr.at(0).longValue()));
			}
		} else {
			out.print(I32Pointer.cast(valuePtr).at(0).getHexValue());
		}

		if (ValueTypeHelper.ifClassIsFlattened(fromClass.flattenedClassCache(), fieldClassName)) {
			out.print(String.format(" (offset=%d) (%s)", fieldOffset.longValue(), fieldClassName));
		} else {
			out.print(String.format(" (offset=%d) (%s)", fieldOffset.longValue(), className));
		}

		if (isHiddenField) {
			out.print(" <hidden>");
		}
	}

	private J9ClassPointer offsetEachLevel(J9ClassPointer upperClass, List<Integer> indexs, List<Long> offsets)
			throws CorruptDataException {
		boolean lockwordPrinted = false;

		if (J9BuildFlags.thr_lockNursery) {
			lockwordPrinted = false;
		}

		long superclassIndex;
		long depth;
		J9ClassPointer previousSuperclass = J9ClassPointer.NULL;
		J9ClassPointer fieldClass = J9ClassPointer.NULL;
		if (indexs.size() == 0) {
			return upperClass;
		}
		
		int currentIndex = indexs.get(0);
		indexs.remove(0);
		depth = J9ClassHelper.classDepth(upperClass).longValue();
		for (superclassIndex = 0; superclassIndex <= depth; superclassIndex++) {
			J9ClassPointer superclass;
			if (superclassIndex == depth) {
				superclass = upperClass;
			} else {
				superclass = J9ClassPointer.cast(upperClass.superclasses().at(superclassIndex));
			}

			U32 flags = new U32(J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE | J9VM_FIELD_OFFSET_WALK_INCLUDE_HIDDEN);
			Iterator<J9ObjectFieldOffset> iterator = J9ObjectFieldOffsetIterator
					.J9ObjectFieldOffsetIteratorFor(superclass.romClass(), upperClass, previousSuperclass, flags);

			int index = 0;
			while (iterator.hasNext() && index <= currentIndex) {
				J9ObjectFieldOffset result = iterator.next();
				boolean printField = true;
				boolean isHiddenField = result.isHidden();
				if (J9BuildFlags.thr_lockNursery) {
					boolean isLockword = (isHiddenField
							&& ((result.getOffsetOrAddress().add(J9Object.SIZEOF).eq(superclass.lockOffset()))));

					if (isLockword) {
						printField = (!lockwordPrinted && (upperClass.lockOffset().eq(superclass.lockOffset())));
					}
				}

				if (printField) {
					if (index == currentIndex) {
						J9ROMFieldShapePointer fieldShape = result.getField();
						UDATA fieldOffset = result.getOffsetOrAddress();
						long offset = fieldOffset.longValue();
						offsets.add(offset);

						String fieldName = J9UTF8Helper.stringValue(fieldShape.nameAndSignature().name());
						String fieldSignature = J9UTF8Helper.stringValue(fieldShape.nameAndSignature().signature());
						fieldClass = ValueTypeHelper.findJ9ClassInFlattenedClassCache(upperClass.flattenedClassCache(),
								getClassNameFromSig(fieldSignature));
						return offsetEachLevel(fieldClass, indexs, offsets);
					}
				}
				index++;
			}
			previousSuperclass = superclass;
		}

		return J9ClassPointer.NULL;
	}

	private void parseIndexs(String[] args, List<Integer> indexs) {
		for (int i = 1; i < args.length; i++) {
			indexs.add(Integer.parseInt(args[i]));
		}
	}

	private String stringFromArgs(String[] args) {
		StringJoiner result = new StringJoiner(",");
		for (int i = 0; i < args.length; i++) {
			result.add(args[i]);
		}
		return result.toString();
	}

	private long totalOffsets(List<Long> offsets) {
		long result = 0;
		for (int i = 0; i < offsets.size(); i++) {
			result += offsets.get(i);
		}
		return result;
	}

	private String getClassNameFromSig(String signature) {
		String name = "";
		if (signature.length() == 1) {
			name = signature;
		} else {
			name = signature.substring(1, signature.length() - 1);
		}
		return name;
	}

	private void padding(PrintStream out, int level) {
		for (int i = 0; i < level; i++) {
			out.print(" ");
		}
	}

}
