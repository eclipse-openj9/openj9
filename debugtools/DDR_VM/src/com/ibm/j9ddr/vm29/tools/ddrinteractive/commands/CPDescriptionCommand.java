/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMClassHelper;
import com.ibm.j9ddr.vm29.structure.J9ConstantPool;

public class CPDescriptionCommand extends Command 
{
	public CPDescriptionCommand()
	{
		addCommand("cpdescription", "<address>", "Dump the cpdescription for the J9ROMClass");
	}
	
	
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		try {
			long address = CommandUtils.parsePointer(args[0], J9BuildFlags.env_data64);
			J9ROMClassPointer romClass = J9ROMClassPointer.cast(address);
			U32Pointer cpDescription = J9ROMClassHelper.cpShapeDescription(romClass);
			final long cpCount = romClass.romConstantPoolCount().longValue();
			
			final long numberOfLongs = (cpCount + J9ConstantPool.J9_CP_DESCRIPTIONS_PER_U32 - 1) / J9ConstantPool.J9_CP_DESCRIPTIONS_PER_U32;

			out.append("CP Shape Description:" + nl);
			long k = 0;
			for (long i = 0; i < numberOfLongs; i++) {
				long descriptionLong = cpDescription.at(i).longValue();
				for (long j = 0; j < J9ConstantPool.J9_CP_DESCRIPTIONS_PER_U32; j++, k++) {
					if (k == cpCount) {
						break;
					}
					out.append("[" + k + "] = " + (int) (descriptionLong & J9ConstantPool.J9_CP_DESCRIPTION_MASK)+ nl);
					descriptionLong >>= J9ConstantPool.J9_CP_BITS_PER_DESCRIPTION;
				}
			}
			out.append(nl);
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
}
