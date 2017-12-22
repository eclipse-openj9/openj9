/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
import java.util.HashSet;
import java.util.Set;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.DataUnavailableException;
import com.ibm.j9ddr.corereaders.osthread.IOSStackFrame;
import com.ibm.j9ddr.corereaders.osthread.IOSThread;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;

/**
 * Debug extension to print the native stacks for all or selected threads.
 * 
 * @author Howard Hellyer
 *
 */
public class NativeStacksCommand extends Command {
		
	public NativeStacksCommand() {
		addCommand("nativestacks","[tid] [tid] ...", "Display the native stacks for all or the specified thread ids.");
		addCommand("nstack","[tid] [tid] ...", "Display the native stacks for all or the specified thread ids.");
	}

	public void run(String cmd, String[] args, Context ctx, PrintStream out) throws DDRInteractiveCommandException {
		Set<Long> threadIds = null;
		for( String arg: args) {
			if( threadIds == null ) {
				threadIds = new HashSet<Long>();
			}
			Long tid = CommandUtils.parseNumber(arg).longValue();
			threadIds.add(tid);
		}	
	
		try {
			Collection<? extends IOSThread> threads = ctx.process.getThreads();
			for( IOSThread t : threads ) {
				if( threadIds == null || threadIds.contains(t.getThreadId()) ) {
					try {
						out.printf("Thread id: %d (0x%X)\n", t.getThreadId(), t.getThreadId());
						int frameId = 0;
						if( threadIds != null ) {
							threadIds.remove(t.getThreadId());
						}
						for( IOSStackFrame f: t.getStackFrames() ) {
							String symbol = ctx.process.getProcedureNameForAddress(f.getInstructionPointer()); 
							if( ctx.process.bytesPerPointer() == 8 ) {
								out.printf("#%d bp:0x%016X ip:0x%016X %s\n", frameId++, f.getBasePointer(), f.getInstructionPointer(), symbol );
							} else {
								out.printf("#%d bp:0x%08X ip:0x%08X %s\n", frameId++, f.getBasePointer(), f.getInstructionPointer(), symbol );
							}
						}
					} catch (DataUnavailableException e) {
						out.printf("DataUnavailableException reading stack for thread %d\n", t.getThreadId());
					} catch (CorruptDataException e) {
						out.printf("CorruptDataException reading stack for thread %d\n", t.getThreadId());
					}
					out.println("----------");

				}
			}
			if( threadIds != null && !threadIds.isEmpty() ) {
				out.print("Unable to find native thread for id(s): ");
				for( long tid: threadIds ) {
					out.print(tid);
				}
				out.println();
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
	
	
}
