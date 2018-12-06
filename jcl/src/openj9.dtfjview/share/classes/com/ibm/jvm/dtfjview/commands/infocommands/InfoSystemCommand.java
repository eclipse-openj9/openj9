/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2018 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.commands.infocommands;

import java.io.PrintStream;
import java.net.InetAddress;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Iterator;
import java.util.Properties;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;
import com.ibm.jvm.dtfjview.commands.BaseJdmpviewCommand;

@DTFJPlugin(version="1.*", runtime=false)
public class InfoSystemCommand extends BaseJdmpviewCommand {
	{
		addCommand("info sys", "", "shortened form of info system");
		addCommand("info system", "", "displays information about the system the core dump is from");				
	}
	
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		if(args.length != 0) {
			out.println("\"info system\" command does not take any parameters");
			return;
		}
		doCommand();
	}
	
	public void doCommand(){
		try {
			out.println("\nMachine OS:\t" + ctx.getImage().getSystemType());
		} catch (DataUnavailable exc) {
			out.println("\nMachine OS:\t" + "data unavailable");
		} catch (CorruptDataException exc) {
			out.println("\nMachine OS:\t" + "data corrupted");
		}

		Properties imageProperties = ctx.getImage().getProperties();
		if (imageProperties.containsKey("Hypervisor")) {
			out.println("Hypervisor:\t" + imageProperties.getProperty("Hypervisor"));
		}

		try {
			out.println("Machine name:\t" + ctx.getImage().getHostName());
		} catch (DataUnavailable exc) {
			out.println("Machine name:\t" + "data unavailable");
		} catch (CorruptDataException exc) {
			out.println("Machine name:\t" + "data corrupted");
		}

		out.println("Machine IP address(es):");
		try {
			Iterator<?> itIPAddresses = ctx.getImage().getIPAddresses(); 
			while (itIPAddresses.hasNext()) {
				Object addr = itIPAddresses.next(); 
				if (addr instanceof InetAddress) {
					out.println("\t\t" + ((InetAddress)addr).getHostAddress());
				} else if (addr instanceof CorruptData) {
					out.println("\t\tdata corrupted");
				} 
			}
		} catch (DataUnavailable du) {
			out.println("\t\tdata unavailable");
		}		
		try {
			out.println("System memory:\t" + ctx.getImage().getInstalledMemory());
		} catch (DataUnavailable exc) {
			out.println("System memory:\t" + "data unavailable");
		}

		try {
			long createTimeMillis = ctx.getImage().getCreationTime();
			if (createTimeMillis != 0) {
				DateFormat fmt = new SimpleDateFormat("yyyy/MM/dd HH:mm:ss:SSS");
				String createTimeStr = fmt.format(new Date(createTimeMillis));
				out.println("\nDump creation time: " + createTimeStr);
			} else {
				out.println("\nDump creation time: data unavailable");
			}
		} catch (DataUnavailable d) {
			out.println("\nDump creation time: data unavailable");
		}
		
		// Dump creation time - nanotime - added in DTFJ 1.12
		try {
			long createTimeNanos = ctx.getImage().getCreationTimeNanos();
			if (createTimeNanos != 0) {
				out.println("Dump creation time (nanoseconds): " + createTimeNanos);
			} else {
				out.println("Dump creation time (nanoseconds): data unavailable");
			}
		} catch (DataUnavailable du) {
			out.println("Dump creation time (nanoseconds): data unavailable");
		} catch (CorruptDataException cde) {
			out.println("Dump creation time (nanoseconds): data corrupted");
		}

		final JavaRuntime runtime = ctx.getRuntime();
		out.println("\nJava version:");
		if (runtime == null) {
			out.println("\tmissing, unknown or unsupported JRE");
		} else {
			try {
				out.println(runtime.getVersion());
			} catch (CorruptDataException e) {
				out.println("version data corrupted");
			}

			// JVM start time - millisecond wall clock - added in DTFJ 1.12
			try {
				long startTimeMillis = runtime.getStartTime();
				if (startTimeMillis != 0) {
					DateFormat fmt = new SimpleDateFormat("yyyy/MM/dd HH:mm:ss:SSS");
					String createTimeStr = fmt.format(new Date(startTimeMillis));
					out.println("\nJVM start time: " + createTimeStr);
				} else {
					out.println("\nJVM start time: data unavailable");
				}
			} catch (DataUnavailable d) {
				out.println("\nJVM start time: data unavailable");
			} catch (CorruptDataException cde) {
				out.println("\nJVM start time (nanoseconds): data corrupted");
			}

			// JVM start time - nanotime - added in DTFJ 1.12
			try {
				long startTimeNanos = runtime.getStartTimeNanos();
				if (startTimeNanos != 0) {
					out.println("JVM start time (nanoseconds): " + startTimeNanos);
				} else {
					out.println("JVM start time (nanoseconds): data unavailable");
				}
			} catch (DataUnavailable du) {
				out.println("JVM start time (nanoseconds): data unavailable");
			} catch (CorruptDataException cde) {
				out.println("JVM start time (nanoseconds): data corrupted");
			}
		}

		boolean kernelSettingPrinted = false;
		for( String name: new String[] {"/proc/sys/kernel/sched_compat_yield", "/proc/sys/kernel/core_pattern", "/proc/sys/kernel/core_uses_pid"}) {
			if (imageProperties.containsKey(name)) {
				if( !kernelSettingPrinted ) {
					out.println("\nLinux Kernel Settings:");
				}
				out.println(name + " = " + imageProperties.getProperty(name));
				kernelSettingPrinted = true;
			}
		}
		
		out.println();
	}

	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println("displays information about the system the core dump is from\n\n" +
				"parameters: none\n\n" +
				"prints information about the system the core dump is from:\n" +
				"  - operating system\n" +
				"  - host name and IP addresses\n" +
				"  - amount of memory\n" +
				"  - virtual machine(s) present\n"
				);
	}
}
