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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

import static com.ibm.j9ddr.vm29.structure.J9ROMFieldOffsetWalkState.*;

import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Iterator;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.util.PatternString;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffset;
import com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffsetIterator;
import com.ibm.j9ddr.vm29.j9.walkers.ClassSegmentIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMFieldShapePointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMFieldShapeHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.structure.J9Object;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.UDATA;

public class J9ClassShapeCommand extends Command 
{

	public J9ClassShapeCommand()
	{
		addCommand("j9classshape", "<ramclass>", "view instance shape");
	}
		
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		if (args.length != 1) {
			CommandUtils.dbgPrint(out, "Usage: !j9classshape <classAddress>\n");
			return;
		}

		try {
			
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			J9ClassPointer instanceClass = J9ClassPointer.NULL;
			String classSelector = args[0];
			if (classSelector.matches("\\p{Digit}.*")) { /* addresses start with a decimal digit, possibly the '0' in "0x" */
				instanceClass = J9ClassPointer.cast(CommandUtils.parsePointer(classSelector, J9BuildFlags.env_data64));
			} else {
				J9ClassPointer candidates[] = findClassByName(vm, classSelector);
				if (candidates.length == 0) {
					CommandUtils.dbgPrint(out, "No classes matching \""+classSelector+"\" found\n");
					return;
				} else if (candidates.length > 1) {
					CommandUtils.dbgPrint(out, "Multiple classes matching \""+classSelector+"\" found\n");
					return;
				} else {
					instanceClass = candidates[0];
					String javaName = J9ClassHelper.getJavaName(instanceClass);
					String hexString = instanceClass.getHexAddress();
					CommandUtils.dbgPrint(out, String.format("!j9class %1$s\n", hexString));					
				}
			}
			
			J9ClassPointer previousSuperclass = J9ClassPointer.NULL;

			String className = J9ClassHelper.getName(instanceClass);
			J9VMThreadPointer mainThread = vm.mainThread();
			boolean lockwordPrinted = true;
			if (J9BuildFlags.thr_lockNursery) {
				lockwordPrinted = false;
			}
			CommandUtils.dbgPrint(out, "Instance fields in %s:\n", className);
			CommandUtils.dbgPrint(out, "\noffset     name\tsignature\t(declaring class)\n");

			if (mainThread.isNull()) {
				/* we cannot print the instance fields without this */
				return;
			}

			long depth = J9ClassHelper.classDepth(instanceClass).longValue();			
			for (long superclassIndex = 0; superclassIndex <= depth; superclassIndex++) {
				J9ClassPointer superclass;

				if (superclassIndex == depth) {
					superclass = instanceClass;
				} else {
					superclass = J9ClassPointer.cast(instanceClass.superclasses().at(superclassIndex));
				}
				

				U32 flags = new U32(J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE | J9VM_FIELD_OFFSET_WALK_INCLUDE_HIDDEN);
				Iterator<J9ObjectFieldOffset> iterator = J9ObjectFieldOffsetIterator.J9ObjectFieldOffsetIteratorFor(superclass.romClass(), instanceClass, previousSuperclass, flags);

				while (iterator.hasNext()) {
					J9ObjectFieldOffset result = (J9ObjectFieldOffset) iterator.next();

					boolean printField = true;
					boolean isHiddenField = result.isHidden();

					if (J9BuildFlags.thr_lockNursery) {
						boolean isLockword = (isHiddenField && (result.getOffsetOrAddress().add(J9Object.SIZEOF).eq(superclass.lockOffset())));

						if (isLockword) {
							/*
							 * Print the lockword field if it is indeed the
							 * lockword for this instanceClass and we haven't
							 * printed it yet.
							 */
							printField = !lockwordPrinted && instanceClass.lockOffset().eq(superclass.lockOffset());
							if (printField) {
								lockwordPrinted = true;
							}
						}
					}
					if (printField) {
						printShapeField(out, superclass, result.getField(), result.getOffsetOrAddress(), isHiddenField);
					}
				}
				previousSuperclass = superclass;
			}

			CommandUtils.dbgPrint(out, "\nTotal instance size: %d\n", instanceClass.totalInstanceSize().longValue());
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}

	private void printShapeField(PrintStream out, J9ClassPointer fromClass, J9ROMFieldShapePointer field, UDATA offset, boolean isHiddenField) throws CorruptDataException 
	{
		String name =  J9ROMFieldShapeHelper.getName(field);
		String signature = J9ROMFieldShapeHelper.getSignature(field);
		String className = J9UTF8Helper.stringValue(fromClass.romClass().className());

		CommandUtils.dbgPrint(out, "%d\t%s\t%s\t(%s)%s\n", offset.longValue(), name, signature, className, (isHiddenField ? " <hidden>" : ""));
	}
	
	public static J9ClassPointer[] findClassByName(J9JavaVMPointer vm, String searchClassName) throws DDRInteractiveCommandException 
	{
		ArrayList<J9ClassPointer> result = new ArrayList<J9ClassPointer>();
		try {
			PatternString pattern = new PatternString (searchClassName);

			ClassSegmentIterator classSegmentIterator = new ClassSegmentIterator(vm.classMemorySegments());
			while (classSegmentIterator.hasNext()) {
				J9ClassPointer classPointer = (J9ClassPointer) classSegmentIterator.next();
				String javaName = J9ClassHelper.getJavaName(classPointer);
				if (pattern.isMatch(javaName)) {
					result.add(classPointer);
				}
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
		return result.toArray(new J9ClassPointer[result.size()] );
	}

}
