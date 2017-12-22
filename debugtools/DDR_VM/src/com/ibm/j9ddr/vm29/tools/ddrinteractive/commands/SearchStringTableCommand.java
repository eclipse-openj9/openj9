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

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.StringTable;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;

public class SearchStringTableCommand extends Command{

	public SearchStringTableCommand() {
		addCommand("searchstringtable", "<address>", "Search for J9Object* in string table");
	}
	
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException {
		try {
			if (1 == args.length) {
				J9ObjectPointer objectPointer = J9ObjectPointer.cast(CommandUtils.parsePointer(args[0], J9BuildFlags.env_data64));
				J9ObjectPointer data = StringTable.from().search(objectPointer);
				
				if (data.notNull()) {
					String stringValue = J9ObjectHelper.stringValue(data);					
					String hexAddr = data.formatShortInteractive();
					out.println(String.format("%s <%s>", hexAddr, stringValue));
				} else {
					out.println("Not found");
				}
			} else {
				throw new DDRInteractiveCommandException("This debug extension needs an address argument !searchstringtable <addr>");
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
}
