/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.tools.ddrinteractive.commands;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.DataUnavailableException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;

/**
 * Debug extension to resolve symbols from addresses.
 * 
 * @author Howard Hellyer
 *
 */
public class LookupSymbolCommand extends Command {
	protected String hexformat = null;
		
	public LookupSymbolCommand() {
		addCommand("sym","<address>", "Display the symbol, or closest symbol to <address>");
	}

	public void run(String cmd, String[] args, Context ctx, PrintStream out) throws DDRInteractiveCommandException {
		if( args.length != 1) {
			out.println("Please supply a function pointer to lookup.");
			return;
		}
		
		boolean is64BitPlatform = (ctx.process.bytesPerPointer() == 8) ? true : false;
		long address = CommandUtils.parsePointer(args[0], is64BitPlatform);

		try {
			String symbol = ctx.process.getProcedureNameForAddress(address); 
			out.println("Closest match:");
			out.println(symbol);
		} catch (DataUnavailableException e) {
			throw new DDRInteractiveCommandException(e);
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
	
	
}
