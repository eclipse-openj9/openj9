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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

import static com.ibm.j9ddr.vm29.structure.J9BCTranslationData.BCT_DumpMaps;
import static com.ibm.j9ddr.vm29.structure.J9BCTranslationData.BCT_LittleEndianOutput;
import static com.ibm.j9ddr.vm29.structure.J9BCTranslationData.BCT_BigEndianOutput;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.ConstantPoolHelpers;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9MethodHelper;

public class BytecodesCommand extends Command 
{
	public BytecodesCommand()
	{
		addCommand("bytecodes", "<address>[,maps]", "cfdump the specified J9Method. (maps is optional)");
	}
	
	
	// dbgext_bytecodes
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		try {
			long address = CommandUtils.parsePointer(args[0], J9BuildFlags.env_data64);
			
			J9MethodPointer ramMethod = J9MethodPointer.cast(address);
			
			long maps = 0;
			if (ramMethod.isNull()) {
				CommandUtils.dbgPrint(out, "bad or missing ram method addr\n");
				return;
			}

			J9ClassPointer ramClass = ConstantPoolHelpers.J9_CLASS_FROM_METHOD(ramMethod);

			if (args.length == 2 && args[1].equals("maps")) {
				maps |= BCT_DumpMaps;
			}

			if (J9BuildFlags.env_littleEndian) {
				maps |= BCT_LittleEndianOutput;
			} else {
				maps |= BCT_BigEndianOutput;
			}

			J9ROMMethodPointer romMethod = J9MethodHelper.romMethod(ramMethod);
			J9ROMClassPointer romClass = ramClass.romClass();

			J9BCUtil.j9bcutil_dumpRomMethod(out, romMethod, romClass, maps, 0);
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
}
