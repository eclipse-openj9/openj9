/*******************************************************************************
 * Copyright (c) 2001, 2015 IBM Corp. and others
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
import java.util.HashMap;
import java.util.List;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.Pool;
import com.ibm.j9ddr.vm29.j9.SlotIterator;
import com.ibm.j9ddr.vm29.j9.SystemMonitor;
import com.ibm.j9ddr.vm29.j9.walkers.MonitorIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9ContendedLoadTableEntryPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9HashTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ThreadHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9VMThreadHelper;
import com.ibm.j9ddr.vm29.structure.J9ContendedLoadTableEntry;

public class DumpContendedLoadTable extends Command 
{
	private static final String nl = System.getProperty("line.separator");

	static HashMap<Long, String> loadingStatusValues;

	public DumpContendedLoadTable()
	{
		addCommand("dumpcontload", "cmd|help", "dump contended class load table");
		loadingStatusValues = new HashMap<Long, String>(5);
		loadingStatusValues.put(J9ContendedLoadTableEntry.CLASSLOADING_DONT_CARE, "dont_care");
		loadingStatusValues.put(J9ContendedLoadTableEntry.CLASSLOADING_DUMMY, "dummy");
		loadingStatusValues.put(J9ContendedLoadTableEntry.CLASSLOADING_LOAD_FAILED, "failed");
		loadingStatusValues.put(J9ContendedLoadTableEntry.CLASSLOADING_LOAD_IN_PROGRESS, "in-progress");
		loadingStatusValues.put(J9ContendedLoadTableEntry.CLASSLOADING_LOAD_SUCCEEDED, "succeeded");
	}

	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{

		if (0 != args.length) {
			String argument = args[0];

			if(argument.equalsIgnoreCase("help")) {
				help(out);
				return;
			}
		}

		try {
			J9JavaVMPointer javaVM = J9RASHelper.getVM(DataType.getJ9RASPointer());
			J9HashTablePointer contTable = javaVM.contendedLoadTable();
			J9PoolPointer poolPtr = contTable.listNodePool();
			Pool<J9ContendedLoadTableEntryPointer> pool = Pool.fromJ9Pool(poolPtr, J9ContendedLoadTableEntryPointer.class);		
			SlotIterator<J9ContendedLoadTableEntryPointer> poolIterator = pool.iterator();
			if (poolIterator.hasNext()) {
				out.println("Active class loads:");
				while (poolIterator.hasNext()) {
					J9ContendedLoadTableEntryPointer entryPointer = poolIterator.next();
					String ldStatus = loadingStatusValues.get(entryPointer.status().longValue());
					if (null == ldStatus) {
						ldStatus = "ILLEGAL VALUE: " + entryPointer.status();
					}
					J9VMThreadPointer loadingThread = entryPointer.thread();
					out.print(
							String.format("\tClassname: %s;\n\t\tLoader:  %s; Loading thread: %s %s\n\t\tStatus: %s; Table entry hash 0x%X\n",
									entryPointer.className().getCStringAtOffset(0, entryPointer.classNameLength().longValue()), 
									entryPointer.classLoader().formatShortInteractive(), 
									J9VMThreadHelper.getName(loadingThread), loadingThread.formatShortInteractive(),
									ldStatus, 
									entryPointer.hashValue().longValue())
							);
				}

				MonitorIterator iterator = new MonitorIterator(javaVM);

				while (iterator.hasNext()) {
					Object current = iterator.next();	
					if (current instanceof J9ThreadMonitorPointer) { // System Monitor
						SystemMonitor monitor  = SystemMonitor.fromJ9ThreadMonitor((J9ThreadMonitorPointer) current);
						final String monitorName = monitor.getName();
						if (monitorName.matches(".*VM class table.*")) {
							List<J9ThreadPointer> waitingThreads = monitor.getWaitingThreads();
							if (!waitingThreads.isEmpty()) {
								out.print("Waiting threads:\n");
								for (J9ThreadPointer tp: waitingThreads) {
									J9VMThreadPointer vmThread = J9ThreadHelper.getVMThread(tp);
									if (vmThread.notNull()) {
										out.print(String.format("\t%s\t%s\n", vmThread.formatShortInteractive(), J9VMThreadHelper.getName(vmThread)));
									} else {
										out.print(String.format("\t%s\t%s\n", tp.formatShortInteractive(), "[osthread]"));
									}
								}
							}
							break;
						}
					} 
				}
			} else {
				out.println("No active class loads");
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}

	}



	private void help(PrintStream out) {
		out.append("!dumpcontload       -- the contents of the VM contended class load table");
		out.append(nl);
	}
}
