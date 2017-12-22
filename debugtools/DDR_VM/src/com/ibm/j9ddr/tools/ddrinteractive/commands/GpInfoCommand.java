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
package com.ibm.j9ddr.tools.ddrinteractive.commands;

import java.io.PrintStream;
import java.util.Collection;
import java.util.Collections;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;

/**
 * Basic debug extension to print out gpInfo for dumps triggered by a gpf. 
 * 
 * @author Howard Hellyer
 *
 */
public class GpInfoCommand extends Command {

	public GpInfoCommand()
	{
		addCommand("gpinfo", "", "print out info about a gpf if one occured");
	}
	
	public Collection<String> getCommandDescriptions() {
		return Collections.singleton("gpinfo                         - print out info about a gpf if one occured");
	}

	public void run(String command, String[] args, Context context,
			PrintStream out) throws DDRInteractiveCommandException {
		long vmAddress = context.vmAddress;
		try {
			long j9rasAddress = CommandUtils.followPointerFromStructure(context, "J9JavaVM", vmAddress, "j9ras");
			long crashInfoAddress = CommandUtils.followPointerFromStructure(context, "J9RAS", j9rasAddress, "crashInfo");
			if( crashInfoAddress != 0l ) {
				long failingThread = CommandUtils.followPointerFromStructure(context, "J9RASCrashInfo", crashInfoAddress, "failingThread");
				long failingThreadID = CommandUtils.followPointerFromStructure(context, "J9RASCrashInfo", crashInfoAddress, "failingThreadID");
				long gpinfo = CommandUtils.followPointerFromStructure(context, "J9RASCrashInfo", crashInfoAddress, "gpInfo");

				out.println("Failing Thread: !j9vmthread 0x" + Long.toHexString(failingThread));
				out.println("Failing Thread ID: 0x" + Long.toHexString(failingThreadID) + " ("+failingThreadID+")");
				out.println("gpInfo:");
				out.println(CommandUtils.getCStringAtAddress(context.process, gpinfo));
			} else {
				out.println("Core does not appear to have been triggered by a gpf. No J9RASCrashInfo found.");
			}

		} catch (MemoryFault e) {
			throw new DDRInteractiveCommandException(e);
		} catch (com.ibm.j9ddr.NoSuchFieldException e){
			throw new DDRInteractiveCommandException(e);
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}	
	}

}
