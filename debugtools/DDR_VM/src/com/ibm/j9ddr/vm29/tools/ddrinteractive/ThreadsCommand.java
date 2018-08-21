/*******************************************************************************
 * Copyright (c) 2009, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive;

import java.io.PrintStream;
import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.MonitorTable;
import com.ibm.j9ddr.vm29.j9.MonitorTableListIterator;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.StackWalkCommand;
import com.ibm.j9ddr.vm29.types.U8;
import com.ibm.j9ddr.vm29.types.UDATA;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.MonitorsCommand;


public class ThreadsCommand extends  Command 
{
	private static final String nl = System.getProperty("line.separator");

	public ThreadsCommand() 
	{
		addCommand("threads", "cmd|help", "Lists VM threads");
	}
	
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException {
		if (args.length == 0) {
			displayThreads(out);
		} else {
			String argument = args[0];

			// dbgext_threads
			if (argument.equalsIgnoreCase("help")) {
				help(out);
			} else {
				out.append("Attached Threads List. For more options, run !threads help\n");
				out.append(nl);
				if (argument.equalsIgnoreCase("stack")) {
					stack(out, context, "");
				} else if (argument.equalsIgnoreCase("stackslots")) {
					stack(out, context, "!stackslots");
				} else if (argument.equalsIgnoreCase("flags")) {
					flags(out);
				} else if (argument.equalsIgnoreCase("debugEventData")) {
					if (J9BuildFlags.interp_debugSupport) {
						debugEventData(out);
					}
				} else if (argument.equalsIgnoreCase("search")) {
					search(out, new UDATA(Long.decode(args[1])));
				} else if (argument.equalsIgnoreCase("monitors")) {
					monitors(out);
				} else if (argument.equalsIgnoreCase("trace")) {
					trace(out);
				}
			}
		}
	}

	public static String getThreadName(J9VMThreadPointer thread) throws CorruptDataException
	{
		U8Pointer threadName = thread.omrVMThread().threadName();
		StringBuffer sb = new StringBuffer();

		if (threadName.isNull()) {
			sb.append("<NULL>");
		} else {
			while (!threadName.at(0).eq(0)) {
				U8 at = threadName.at(0);
				sb.append((char) at.intValue());
				threadName = threadName.add(1);
			}
		}
		
		return sb.toString();
	}
	
	private void trace(PrintStream out) throws DDRInteractiveCommandException {
		// 					

		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			J9VMThreadPointer mainThread = vm.mainThread();
			if (mainThread.notNull()) {
				J9VMThreadPointer threadCursor = vm.mainThread();

				do {		
					out.append(String.format("    !stack 0x%s  !j9vmthread 0x%s  !j9thread 0x%s  tid 0x%s (%d) !utthreaddata 0x%s // %s", 
							Long.toHexString(threadCursor.getAddress()), 
							Long.toHexString(threadCursor.getAddress()), 
							Long.toHexString(threadCursor.osThread().getAddress()), 
							Long.toHexString(threadCursor.osThread().tid().longValue()), 
							threadCursor.osThread().tid().longValue(),
							Long.toHexString(threadCursor.omrVMThread()._trace$uteThread().getAddress()),
							getThreadName(threadCursor)));
					out.append(nl);
					threadCursor = threadCursor.linkNext();
				} while (!threadCursor.eq(mainThread));
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}

	}

	/**
	 * Prints all of the J9ObjectMonitors in the list of monitor tables.
	 *  
	 * NOTE: It does not dump system monitors found in the thread lib monitor_pool
	 * 
	 * @param out the PrintStream to write output to
	 * 
	 * @see {@link MonitorsCommand}
	 */
	private void monitors(PrintStream out) throws DDRInteractiveCommandException {
		try {
			MonitorTableListIterator iterator = new MonitorTableListIterator();
			MonitorTable previousMonitorTable = null;
			
			while(iterator.hasNext()) {
				J9ObjectMonitorPointer objectMonitorPointer = iterator.next();
				MonitorTable currentMonitorTable = iterator.currentMonitorTable();
				
				if (!currentMonitorTable.equals(previousMonitorTable)) {
					/* Print header for new monitor table */
					out.append("Table = " + currentMonitorTable.getTableName() + ", itemCount=" + currentMonitorTable.getCount());
					out.append(nl);
				}
				out.append(String.format("\n    !j9thread 0x%s    !j9threadmonitor 0x%s", Long.toHexString(objectMonitorPointer.monitor().owner().getAddress()), Long.toHexString(objectMonitorPointer.monitor().getAddress())));
				out.append(nl);
			
				previousMonitorTable = currentMonitorTable;
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}

	private void search(PrintStream out, UDATA tid) throws DDRInteractiveCommandException {
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			J9VMThreadPointer mainThread = vm.mainThread();
			if (mainThread.notNull()) {
				J9VMThreadPointer threadCursor = vm.mainThread();

				do {
					if (threadCursor.osThread().tid().eq(tid)) {
						out.println(String.format("\t!stack 0x%08x\t!j9vmthread 0x%08x\t!j9thread 0x%08x\ttid 0x%x (%d) // %s", 
								threadCursor.getAddress(), 
								threadCursor.getAddress(), 
								threadCursor.osThread().getAddress(), 
								threadCursor.osThread().tid().longValue(),
								threadCursor.osThread().tid().longValue(),
								getThreadName(threadCursor)));
						break;
					}
					threadCursor = threadCursor.linkNext();
				} while (!threadCursor.eq(mainThread));
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}

	private void debugEventData(PrintStream out) throws DDRInteractiveCommandException {
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			J9VMThreadPointer mainThread = vm.mainThread();
			if (mainThread.notNull()) {
				J9VMThreadPointer threadCursor = vm.mainThread();

				do {//					
					out.append(String.format("    !j9vmthread 0x%s %s %s %s %s %s %s %s %s", Long.toHexString(threadCursor.getAddress()), Long.toHexString(threadCursor.debugEventData1().longValue()), Long.toHexString(threadCursor.debugEventData2().longValue()), Long.toHexString(threadCursor.debugEventData3().longValue()), Long.toHexString(threadCursor.debugEventData4()
							.longValue()), Long.toHexString(threadCursor.debugEventData5().longValue()), Long.toHexString(threadCursor.debugEventData6().longValue()), Long.toHexString(threadCursor.debugEventData7().longValue()), Long.toHexString(threadCursor.debugEventData8().longValue())));
					out.append(nl);
					threadCursor = threadCursor.linkNext();
				} while (!threadCursor.eq(mainThread));
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}

	}

	private void flags(PrintStream out) throws DDRInteractiveCommandException {
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			J9VMThreadPointer mainThread = vm.mainThread();
			if (mainThread.notNull()) {
				J9VMThreadPointer threadCursor = vm.mainThread();

				do {
					out.append(String.format("    !j9vmthread 0x%s publicFlags=%s privateFlags=%s inNative=%s // %s", 
							Long.toHexString(threadCursor.getAddress()),
							Long.toHexString(threadCursor.publicFlags().longValue()),
							Long.toHexString(threadCursor.privateFlags().longValue()),
							Long.toHexString(threadCursor.inNative().longValue()),
							getThreadName(threadCursor)));
					out.append(nl);
					threadCursor = threadCursor.linkNext();
				} while (!threadCursor.eq(mainThread));
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}

	private void stack(PrintStream out, Context context, String command) throws DDRInteractiveCommandException {
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			J9VMThreadPointer mainThread = vm.mainThread();
			StackWalkCommand walkCommand = new StackWalkCommand();

			if (mainThread.notNull()) {
				J9VMThreadPointer threadCursor = vm.mainThread();

				do {
					out.println(String.format("\t!stack 0x%08x\t!j9vmthread 0x%08x\t!j9thread 0x%08x\ttid 0x%x (%d) // %s", 
							threadCursor.getAddress(), 
							threadCursor.getAddress(), 
							threadCursor.osThread().getAddress(),
							threadCursor.osThread().tid().longValue(),
							threadCursor.osThread().tid().longValue(),
							getThreadName(threadCursor)));
					out.append(nl);
					walkCommand.run(command, new String[] { Long.toString(threadCursor.getAddress()) }, context, out);
					out.append(nl);

					threadCursor = threadCursor.linkNext();
				} while (!threadCursor.eq(mainThread));
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}

	private void help(PrintStream out) {
		out.append("!threads            -- list all threads in the VM");
		out.append(nl);
		out.append("!threads stack      -- list stacks for all threads in the VM");
		out.append(nl);
		out.append("!threads stackslots -- list stackslots for all threads in the VM");
		out.append(nl);
		out.append("!threads flags      -- print the public and private flags field for each thread");
		out.append(nl);
		if (J9BuildFlags.interp_debugSupport) {
			out.append("!threads debugEventData -- print the debugEventData fields for each thread");
			out.append(nl);
		}
		out.append("!threads search     -- find a thread by thread id");
		out.append(nl);
		out.append("!threads monitors     -- list all monitors owned by each thread");
		out.append(nl);
		out.append("!threads trace     -- show UTE thread information");
		out.append(nl);
	}

	private void displayThreads(PrintStream out) throws DDRInteractiveCommandException {
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			J9VMThreadPointer mainThread = vm.mainThread();
			if (mainThread.notNull()) {
				J9VMThreadPointer threadCursor = vm.mainThread();

				do {
					out.println(String.format("\t!stack 0x%08x\t!j9vmthread 0x%08x\t!j9thread 0x%08x\ttid 0x%x (%d) // (%s)", 
							threadCursor.getAddress(), 
							threadCursor.getAddress(), 
							threadCursor.osThread().getAddress(),
							threadCursor.osThread().tid().longValue(),
							threadCursor.osThread().tid().longValue(),
							getThreadName(threadCursor)));

					threadCursor = threadCursor.linkNext();
				} while (!threadCursor.eq(mainThread));
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}

}
