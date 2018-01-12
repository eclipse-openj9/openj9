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

import java.io.PrintStream;
import java.util.Iterator;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffset;
import com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffsetIterator;
import com.ibm.j9ddr.vm29.pointer.BoolPointer;
import com.ibm.j9ddr.vm29.pointer.DoublePointer;
import com.ibm.j9ddr.vm29.pointer.FloatPointer;
import com.ibm.j9ddr.vm29.pointer.I16Pointer;
import com.ibm.j9ddr.vm29.pointer.I32Pointer;
import com.ibm.j9ddr.vm29.pointer.I64Pointer;
import com.ibm.j9ddr.vm29.pointer.I8Pointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMFieldShapePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9UTF8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMFieldShapeHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.structure.J9ROMFieldOffsetWalkState;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.UDATA;

public class J9StaticsCommand extends Command 
{
	public J9StaticsCommand()
	{
		addCommand("j9statics", "<ramclass>", "Display static fields of a ram class");
	}

	long staticFieldAddress(J9VMThreadPointer vmStruct, J9ROMClassPointer romClass, String fieldName, String signature, long options) 
	{
		return 0;
	}

	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		try {
			if (args.length != 1) {
				CommandUtils.dbgPrint(out, "Usage: !j9statics <classAddress>\n");
				return;
			}
			
			long address = CommandUtils.parsePointer(args[0], J9BuildFlags.env_data64);
			
			J9ClassPointer ramClass = J9ClassPointer.cast(address);
			J9ROMClassPointer romClass = ramClass.romClass();
			J9UTF8Pointer className = romClass.className();

			CommandUtils.dbgPrint(out, "Static fields in %s:\n", J9UTF8Helper.stringValue(className));
			Iterator<J9ObjectFieldOffset> ofoIterator = J9ObjectFieldOffsetIterator.J9ObjectFieldOffsetIteratorFor(ramClass, J9ClassHelper.superclass(ramClass), new U32(J9ROMFieldOffsetWalkState.J9VM_FIELD_OFFSET_WALK_INCLUDE_STATIC));

			while (ofoIterator.hasNext()) {
				J9ObjectFieldOffset fieldOffset = ofoIterator.next();
				J9ROMFieldShapePointer field = fieldOffset.getField();

				String name = J9ROMFieldShapeHelper.getName(field);
				String sig = J9ROMFieldShapeHelper.getSignature(field);
				UDATAPointer fieldAddress = ramClass.ramStatics().addOffset(fieldOffset.getOffsetOrAddress());

				switch (sig.charAt(0)) {
				case 'L':
				case '[':
					CommandUtils.dbgPrint(out, "\t%s %s %s (!j9romstaticstringfieldshape %s) = !j9object %s\n", fieldAddress.getHexAddress(), name, sig, field.getHexAddress(), fieldAddress.at(0).getHexValue());
					break;
				case 'D':
					DoublePointer  doublePointer = DoublePointer.cast(fieldAddress);
					CommandUtils.dbgPrint(out, "\t%s %s %s (!j9romstaticdoublefieldshape %s) = %s (%s)\n", fieldAddress.getHexAddress(), name, sig, field.getHexAddress(), doublePointer.getHexValue(), new Double(doublePointer.doubleAt(0)).toString());
					break;
				case 'F':
					FloatPointer floatPointer = FloatPointer.cast(fieldAddress);
					CommandUtils.dbgPrint(out, "\t%s %s %s (!j9romstaticsinglefieldshape %s) = %s (%s)\n", fieldAddress.getHexAddress(), name, sig, field.getHexAddress(), floatPointer.getHexValue(), new Float(floatPointer.floatAt(0)).toString());
					break;
				case 'J':
					I64Pointer longPointer = I64Pointer.cast(fieldAddress);
					CommandUtils.dbgPrint(out, "\t%s %s %s (!j9romstaticdoublefieldshape %s) = %s (%d)\n", fieldAddress.getHexAddress(), name, sig, field.getHexAddress(), longPointer.getHexValue(), longPointer.at(0).longValue());
					break;
				case 'I':
					I32Pointer intPointer = I32Pointer.cast(fieldAddress);
					CommandUtils.dbgPrint(out, "\t%s %s %s (!j9romstaticsinglefieldshape %s) = %s (%d)\n", fieldAddress.getHexAddress(), name, sig, field.getHexAddress(), intPointer.getHexValue(), intPointer.at(0).intValue());
					break;
				case 'B':
					I8Pointer bytePointer = I8Pointer.cast(fieldAddress);
					CommandUtils.dbgPrint(out, "\t%s %s %s (!j9romstaticsinglefieldshape %s) = %s (%s)\n", fieldAddress.getHexAddress(), name, sig, field.getHexAddress(), bytePointer.getHexValue(), bytePointer.at(0).byteValue());
					break;
				case 'S':	
					I16Pointer shortPointer = I16Pointer.cast(fieldAddress);
					CommandUtils.dbgPrint(out, "\t%s %s %s (!j9romstaticsinglefieldshape %s) = %s (%d)\n", fieldAddress.getHexAddress(), name, sig, field.getHexAddress(), shortPointer.getHexValue(), shortPointer.at(0).shortValue());
					break;
				case 'Z':
					BoolPointer booleanPointer = BoolPointer.cast(fieldAddress);
					CommandUtils.dbgPrint(out, "\t%s %s %s (!j9romstaticsinglefieldshape %s) = %s (%s)\n", fieldAddress.getHexAddress(), name, sig, field.getHexAddress(), booleanPointer.getHexValue(), booleanPointer.boolAt(0)? "true" : "false");
					break;
				default:
					CommandUtils.dbgPrint(out, "\t%s %s %s (!j9romstaticsinglefieldshape %s) = %s\n", fieldAddress.getHexAddress(), name, sig, field.getHexAddress(), fieldAddress.at(0).getHexValue());
					break;
				}				
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
}
