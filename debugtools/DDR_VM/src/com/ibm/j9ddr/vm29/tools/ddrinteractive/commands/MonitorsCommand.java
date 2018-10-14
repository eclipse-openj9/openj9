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

import java.io.PrintStream;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.MonitorTable;
import com.ibm.j9ddr.vm29.j9.MonitorTableListIterator;
import com.ibm.j9ddr.vm29.j9.ObjectAccessBarrier;
import com.ibm.j9ddr.vm29.j9.ObjectMonitor;
import com.ibm.j9ddr.vm29.j9.Pool;
import com.ibm.j9ddr.vm29.j9.SlotIterator;
import com.ibm.j9ddr.vm29.j9.SystemMonitor;
import com.ibm.j9ddr.vm29.j9.gc.GCVMThreadListIterator;
import com.ibm.j9ddr.vm29.j9.walkers.HeapWalker;
import com.ibm.j9ddr.vm29.j9.walkers.MonitorIterator;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadAbstractMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadLibraryPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ThreadHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9VMThreadHelper;
import com.ibm.j9ddr.vm29.structure.J9ThreadAbstractMonitor;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.monitors.DeadlockDetector;

/**
 * 
 * Various monitor related commands
 * 
 * See {@link #helpCommand(String[], PrintStream)} for command documentation
 */
public class MonitorsCommand extends Command
{
	/**
	 * Creates the command and registers is
	 * 
	 * @see @Link {@link Command#addCommand(String, String, String)}
	 */
	public MonitorsCommand()
	{
		addCommand("monitors", "<command> | help", "Monitor related commands");
	}

	/**
	 * @see @Link {@link #run(String, String[], Context, PrintStream)}
	 */
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException
	{
		try {
			if (args.length < 1) {
				out.println("No subcommand specified. \"!monitors help\" for usage");
				return;
			}

			/* the first arg is the command */
			String monitorCommand = args[0];

			if (monitorCommand.equalsIgnoreCase("table")) {
				tableCommand(args, out);
			} else if (monitorCommand.equalsIgnoreCase("tables")) {
				tablesCommand(args, out);
			} else if (monitorCommand.equalsIgnoreCase("system")) {
				systemCommand(args, out);
			} else if (monitorCommand.equalsIgnoreCase("object")) {
				objectCommand(args, out);
			} else if (monitorCommand.equalsIgnoreCase("objects")) {
				objectsCommand(args, out);
			} else if (monitorCommand.equalsIgnoreCase("thread")) {
				threadCommand(args, out);
			} else if (monitorCommand.equalsIgnoreCase("j9thread")) {
				j9threadCommand(args, out);
			} else if (monitorCommand.equalsIgnoreCase("j9vmthread")) {
				j9vmthreadCommand(args, out);
			} else if (monitorCommand.equalsIgnoreCase("deadlock")) {
				deadlockCommand(args, out);
			} else if (monitorCommand.equalsIgnoreCase("help")) {
				helpCommand(args, out);
			} else {	
				allCommand(args, out);
			}
		} catch (Exception/* CorruptDataException */e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
	
	static class FilterOptions 
	{
		boolean all;
		boolean owner;
		boolean blocked;
		boolean waiting;

		private FilterOptions() {};
		
		/* Defaults to active */
		public static FilterOptions defaultOption()
		{
			FilterOptions retVal = new FilterOptions();
			retVal.owner = true;
			retVal.blocked = true;
			retVal.waiting = true;
			return retVal;
		}
		
		public static FilterOptions parseOption(String str) throws DDRInteractiveCommandException
		{
			FilterOptions retVal = new FilterOptions();
			
			if(str.equalsIgnoreCase("all")) {
				retVal.all = true;
			} else if(str.equalsIgnoreCase("active")) {
				retVal.owner = true;
				retVal.blocked = true;
				retVal.waiting = true;
			} else if(str.equalsIgnoreCase("blocked")) {
				retVal.blocked = true;
			} else if(str.equalsIgnoreCase("waiting")) {
				retVal.waiting = true;
			} else if(str.equalsIgnoreCase("owned")) {
				retVal.owner = true;
			} else {
				String msg = "Unsupported subcommand \"" + str + "\", see \"!monitors help\" for usage.";
				throw new DDRInteractiveCommandException(msg);
			}
			
			return retVal;
		}
		
		public boolean shouldPrint(ObjectMonitor monitor) throws CorruptDataException 
		{
			if (all) {
				return true;
			}
			
			if(monitor.getOwner().notNull() && owner) {
				return true;
			}
			
			if(!monitor.getBlockedThreads().isEmpty() && blocked) {
				return true;
			}
			
			if(!monitor.getWaitingThreads().isEmpty() && waiting) {
				return true;
			}
			
			return false;
		}
		
		public boolean shouldPrint(SystemMonitor monitor) throws CorruptDataException
		{
			if (all) {
				return true;
			}

			if(monitor.getOwner().notNull() && owner) {
				return true;
			}
			
			if(!monitor.getBlockedThreads().isEmpty() && blocked) {
				return true;
			}
			
			if(!monitor.getWaitingThreads().isEmpty() && waiting) {
				return true;
			}
			
			return false;			
			
		}

	}

	/**
	 * See {@link MonitorsCommand#helpCommand(String[], PrintStream)} for
	 * function documentation
	 * 
	 * @param args
	 *            command args
	 * @param out
	 *            the output stream
	 * @throws DDRInteractiveCommandException
	 */
	private void deadlockCommand(String[] args, PrintStream out) throws DDRInteractiveCommandException
	{
		DeadlockDetector.findDeadlocks(out);
	}
	
	/**
	 * Dumps all monitors that are active for the specified J9Thread/J9VMThread/java.lang.Thread
	 * @param args
	 * @param out
	 * @throws DDRInteractiveCommandException
	 */
	private void threadCommand(String[] args, PrintStream out) throws DDRInteractiveCommandException
	{
		if (args.length < 2) {
			out.println("This command takes one address argument: \"!monitors thread <address>\"");
			return;
		}
		
		try {
		
			long address = CommandUtils.parsePointer(args[1], J9BuildFlags.env_data64);
			VoidPointer ptr = VoidPointer.cast(address);
			
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			J9VMThreadPointer mainThread = vm.mainThread();
			J9ThreadLibraryPointer lib = mainThread.osThread().library();
			J9PoolPointer pool = lib.thread_pool();
			Pool<J9ThreadPointer> threadPool = Pool.fromJ9Pool(pool, J9ThreadPointer.class);
			SlotIterator<J9ThreadPointer> poolIterator = threadPool.iterator();
			J9ThreadPointer osThreadPtr = null;

			while (poolIterator.hasNext()) {
				osThreadPtr = poolIterator.next();
				// Is there an associated J9VMThread?
				J9VMThreadPointer vmThread = J9ThreadHelper.getVMThread(osThreadPtr);
				J9ObjectPointer jlThread = null;

				if (vmThread.notNull()) {
					
					jlThread = vmThread.threadObject();
					
					if ((null != jlThread) && jlThread.equals(ptr)) {
						out.println("Found java/lang/Thread @ " + ptr.getHexAddress());
						printMonitorsForJ9VMThread(out, vm, vmThread);
						return;
						
					} else if (vmThread.equals(ptr)){
						out.println("Found j9vmthread @ " + ptr.getHexAddress());
						printMonitorsForJ9VMThread(out, vm, vmThread);
						return;
					}
					
				}
				
				if (osThreadPtr.equals(ptr)) {
					out.println("Found j9thread @ " + ptr.getHexAddress());
					
					if (vmThread.notNull()) {
						printMonitorsForJ9VMThread(out, vm, vmThread);
					} 
					printMonitorsForJ9Thread(out, vm, osThreadPtr);
					return;
				}
				
			}

		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
			
	}
	
	/**
	 * See {@link MonitorsCommand#helpCommand(String[], PrintStream)} for
	 * function documentation
	 * 
	 * @param args
	 *            command args
	 * @param out
	 *            the output stream
	 * @throws DDRInteractiveCommandException
	 */
	private void j9vmthreadCommand(String[] args, PrintStream out) throws DDRInteractiveCommandException
	{
		if (args.length < 2) {
			out.println("This command takes one address argument: \"!monitors j9vmthread <address>\"");
			return;
		}
		
		try {

			long address = CommandUtils.parsePointer(args[1], J9BuildFlags.env_data64);
			VoidPointer ptr = VoidPointer.cast(address);

			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			J9VMThreadPointer thread = null;
			
			GCVMThreadListIterator threadIterator = GCVMThreadListIterator.from();
			while(threadIterator.hasNext()) {
				if (ptr.equals(threadIterator.next())) {
					thread = J9VMThreadPointer.cast(ptr);
				}
			}
			
			if (null == thread) {
				throw new DDRInteractiveCommandException(String.format("Could not find any j9vmthread at address %s\n",  ptr.getHexAddress()));
			}

			// Step 1: Print the general info for the VM and native threads:
			out.println(String.format(
					"!j9vmthread 0x%08x\t!j9thread 0x%08x\t// %s",
					thread.getAddress(),
					thread.osThread().getAddress(),
					J9VMThreadHelper.getName(thread)));
			
			printMonitorsForJ9VMThread(out, vm, thread);
			
			printMonitorsForJ9Thread(out, vm, thread.osThread());

		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}

	private void printMonitorsForJ9VMThread(PrintStream out, J9JavaVMPointer vm, J9VMThreadPointer thread) throws CorruptDataException
	{
		FilterOptions defaultFilter = FilterOptions.defaultOption();
		MonitorIterator iterator = new MonitorIterator(vm);
		while (iterator.hasNext()) {
			Object current = iterator.next();
			if (current instanceof ObjectMonitor) {
				ObjectMonitor objMon = (ObjectMonitor) current;
				// Step 2: Print any monitors that the thread is the owner of:
				if (thread.equals(objMon.getOwner())) {
					out.print("Owns Object Monitor:\n\t");
					writeObjectMonitor(defaultFilter, objMon, out);
				}
				// Step 3: Print any monitors that the thread is blocking on::
				for (J9VMThreadPointer threadPtr : objMon.getBlockedThreads()) {
					if (threadPtr.equals(thread)) {
						out.print(String.format("Blocking on (Enter Waiter):\n\t"));
						writeObjectMonitor(defaultFilter, objMon, out);
					}
				}
				// Step 4: Print any monitors that the thread is waiting on:
				for (J9VMThreadPointer threadPtr : objMon.getWaitingThreads()) {
					if (threadPtr.equals(thread)) {
						out.print(String.format("Waiting on (Notify Waiter):\n\t"));
						writeObjectMonitor(defaultFilter, objMon, out);
					}
				}
			}
		}
	}

	/**
	 * See {@link MonitorsCommand#helpCommand(String[], PrintStream)} for
	 * function documentation
	 * 
	 * @param args
	 *            command args
	 * @param out
	 *            the output stream
	 * @throws DDRInteractiveCommandException
	 */
	private void j9threadCommand(String[] args, PrintStream out) throws DDRInteractiveCommandException
	{
		if (args.length < 2) {
			out.println("This command takes one address argument: \"!monitors j9thread <address>\"");
			return;
		}
		
		try {

			long address = CommandUtils.parsePointer(args[1], J9BuildFlags.env_data64);
			VoidPointer ptr = VoidPointer.cast(address);
			
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			J9VMThreadPointer mainThread = vm.mainThread();
			
			if (mainThread.isNull() || mainThread.osThread().isNull() || mainThread.osThread().library().isNull()) {
				throw new CorruptDataException("Cannot locate thread library");
			}
			
			J9ThreadLibraryPointer lib = mainThread.osThread().library();
			J9PoolPointer pool = lib.thread_pool();
			Pool<J9ThreadPointer> threadPool = Pool.fromJ9Pool(pool, J9ThreadPointer.class);
			SlotIterator<J9ThreadPointer> poolIterator = threadPool.iterator();
			J9ThreadPointer osThreadPtr = null;
			
			while (poolIterator.hasNext()) {
				if (ptr.equals(poolIterator.next())) {
					osThreadPtr = J9ThreadPointer.cast(ptr);
				}
			}
			
			if (null == osThreadPtr) {
				throw new DDRInteractiveCommandException(String.format("Could not find any j9thread at address %s\n", ptr.getHexAddress()));
			}
			
			// Is there an associated J9VMThread?
			J9VMThreadPointer vmThread = J9ThreadHelper.getVMThread(osThreadPtr);
			
			// Step 1: Print the general info for the VM and native threads:
			out.println(String.format(
					"%s\t%s\t// %s",
					osThreadPtr.formatShortInteractive(),
					vmThread.notNull() ? vmThread.formatShortInteractive() : "<none>",
					vmThread.notNull() ? J9VMThreadHelper.getName(vmThread) : "[osthread]")
			);
			
			printMonitorsForJ9Thread(out, vm, osThreadPtr);
		
		}  catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}

	private void printMonitorsForJ9Thread(PrintStream out, J9JavaVMPointer vm, J9ThreadPointer osThreadPtr) throws CorruptDataException
	{
		FilterOptions defaultFilter = FilterOptions.defaultOption();
		MonitorIterator iterator = new MonitorIterator(vm);
		while (iterator.hasNext()) {
			Object current = iterator.next();
			if (current instanceof J9ThreadMonitorPointer) { // System Monitor
				SystemMonitor monitor = SystemMonitor.fromJ9ThreadMonitor((J9ThreadMonitorPointer) current);
				List<J9ThreadPointer> threadList = null;
				
				// Step 2: Print any monitors that the thread is the owner of:
				J9ThreadPointer ownerPtr = monitor.getOwner();
				if (ownerPtr.notNull() && ownerPtr.equals(osThreadPtr)) {
					out.print("Owns System Monitor:\n\t");
					writeSystemMonitor(defaultFilter, monitor, out);
				}
				// Step 3: Print any monitors that the thread is blocking on:
				threadList = monitor.getBlockedThreads();
				for (J9ThreadPointer thread : threadList) {
					if (osThreadPtr.equals(thread)) {
						out.print("Blocking on System Monitor (Enter Waiter):\n\t");
						writeSystemMonitor(defaultFilter, monitor, out);
					}
				}
				// Step 4: Print any monitors that the thread is waiting on:
				threadList = monitor.getWaitingThreads();
				for (J9ThreadPointer thread : threadList) {
					if (osThreadPtr.equals(thread)) {
						out.print("Waiting on System Monitor (Notify Waiter):\n\t");
						writeSystemMonitor(defaultFilter, monitor, out);
					}
				}
			}
		}
	}
	
	
	/**
	 * See {@link MonitorsCommand#helpCommand(String[], PrintStream)} for
	 * function documentation
	 * 
	 * @param args
	 *            command args
	 * @param out
	 *            the output stream
	 * @throws DDRInteractiveCommandException
	 */
	private void objectsCommand(String[] args, PrintStream out) throws DDRInteractiveCommandException
	{	
		FilterOptions filter = FilterOptions.defaultOption();
		if (args.length > 1) {
			filter = FilterOptions.parseOption(args[1]);
		}
		else if (args.length == 1) {
			out.println("No filter specified, defaulting to 'active' monitors.");
		}
		
		objectsCommand(filter, out);
	}
	
	private void objectsCommand(FilterOptions filter, PrintStream out) throws DDRInteractiveCommandException
	{
		try {
			HashSet<ObjectMonitor> monitors = new HashSet<ObjectMonitor>();
			MonitorTableListIterator iterator = new MonitorTableListIterator();
			MonitorTable previousMonitorTable = null;

			StringBuilder builder = new StringBuilder();

			while (iterator.hasNext()) {
				J9ObjectMonitorPointer objectMonitorPointer = iterator.next();
				MonitorTable currentMonitorTable = iterator.currentMonitorTable();
				
				ObjectMonitor found = tablePrintHelper(filter, builder, objectMonitorPointer);
				if (null != found) {
					monitors.add(found);
				}
			
				if (!currentMonitorTable.equals(previousMonitorTable) && (builder.length() > 0)) {
					/* Print header for new monitor table */
					if (null != previousMonitorTable) {
						out.println();
					}
					out.println(iterator.currentMonitorTable().extraInfo());
					previousMonitorTable = currentMonitorTable;
				}
				
				out.append(builder);
				builder.setLength(0);		
			}
			
			// Don't forget leftover flatlocked monitors on heap, these have no associated table:		
			Iterator<ObjectMonitor> flatMonitorIterator = HeapWalker.getFlatLockedMonitors().iterator();
			while (flatMonitorIterator.hasNext()) {
				ObjectMonitor objMon = flatMonitorIterator.next();
				if (!monitors.contains(objMon)) {
					writeObjectMonitorToBuffer(filter, objMon, builder);
				}
			}
			
			if (builder.length() > 0) {
				out.append("<Flatlocked Monitors on Heap>\n");
				out.append(builder);
			}
			
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}

	
	/**
	 * See {@link MonitorsCommand#helpCommand(String[], PrintStream)} for
	 * function documentation
	 * 
	 * @param args
	 *            command args
	 * @param out
	 *            the output stream
	 * @throws DDRInteractiveCommandException
	 */
	private void systemCommand(String[] args, PrintStream out) throws DDRInteractiveCommandException
	{
		FilterOptions filter = FilterOptions.defaultOption();
		if (args.length > 1) {
			filter = FilterOptions.parseOption(args[1]);
		}
		if (args.length == 1) {
			out.println("No filter specified, defaulting to 'active' monitors.");
		}
		
		systemCommand(filter, out);
	}

	private void systemCommand(FilterOptions filter, PrintStream out) throws DDRInteractiveCommandException {
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			MonitorIterator iterator = new MonitorIterator(vm);

			while (iterator.hasNext()) {
				Object current = iterator.next();	
				if (current instanceof J9ThreadMonitorPointer) { // System Monitor
					SystemMonitor monitor  = SystemMonitor.fromJ9ThreadMonitor((J9ThreadMonitorPointer) current);
					writeSystemMonitor(filter, monitor, out);
				} 
				
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}

	
	/**
	 * Helper to create lists of thread strings.
	 * 
	 * @param thread
	 * @return
	 * @throws CorruptDataException
	 */
	private List<String> threadsListHelper(List<J9ThreadPointer> list) throws CorruptDataException
	{
		LinkedList<String> threadList = new LinkedList<String>();
		for (J9ThreadPointer thread : list) {
			J9VMThreadPointer vmThread = J9ThreadHelper.getVMThread(thread);
			if (vmThread.notNull()) {
				threadList.add(String.format("%s\t%s", vmThread.formatShortInteractive(), J9VMThreadHelper.getName(vmThread)));
			} else {
				threadList.add(String.format("%s\t%s", thread.formatShortInteractive(), "[osthread]"));
			}
			thread = thread.next();
		}
		return threadList;
	}
	
	
	/**
	 * Helper to print monitor tables.
	 * 
	 * @param filter
	 * @param builder
	 * @param objectMonitorPointer
	 * @return
	 * @throws DDRInteractiveCommandException
	 */
	private ObjectMonitor tablePrintHelper(FilterOptions filter, StringBuilder builder, J9ObjectMonitorPointer objectMonitorPointer) throws DDRInteractiveCommandException
	{
		try {
			J9ThreadMonitorPointer threadMonitorPointer = objectMonitorPointer.monitor();
			J9ThreadAbstractMonitorPointer lock = J9ThreadAbstractMonitorPointer.cast(threadMonitorPointer);

			if (lock.flags().allBitsIn(J9ThreadAbstractMonitor.J9THREAD_MONITOR_OBJECT)) {
				if (!lock.userData().eq(0)) { // this is an object monitor in the system monitor table
					J9ObjectPointer obj = J9ObjectPointer.cast(lock.userData());
					ObjectMonitor objMon = ObjectAccessBarrier.getMonitor(obj);
					if (null != objMon) {
						writeObjectMonitorToBuffer(filter, objMon, builder);
						return objMon;
					}
				}
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
		return null;
	}
	
	
	/**
	 * See {@link MonitorsCommand#helpCommand(String[], PrintStream)} for
	 * function documentation
	 * 
	 * @param args
	 *            command args
	 * @param out
	 *            the output stream
	 * @throws DDRInteractiveCommandException
	 */
	private void tableCommand(String[] args, PrintStream out) throws DDRInteractiveCommandException
	{
		FilterOptions filter = FilterOptions.defaultOption();
		if (args.length > 2) {
			filter = FilterOptions.parseOption(args[2]);
		} else if (args.length == 2) {
			out.println("No filter specified, defaulting to 'active' monitors.");
		} else if (args.length < 2) {
			out.println("This command takes one address argument: \"!monitors table <address>\"");
			return;
		}
		
		try {
			StringBuilder builder = new StringBuilder();

			long address = CommandUtils.parsePointer(args[1], J9BuildFlags.env_data64);
			VoidPointer ptr = VoidPointer.cast(address);

			MonitorTableListIterator iterator = new MonitorTableListIterator();
			
			boolean foundTable = false;
			while (iterator.hasNext()) {
				J9ObjectMonitorPointer objectMonitorPointer = iterator.next();
				MonitorTable currentMonitorTable = iterator.currentMonitorTable();
				if (currentMonitorTable.getMonitorTableListEntryPointer().equals(ptr)) {
					tablePrintHelper(filter, builder, objectMonitorPointer);
					foundTable = true;
				}
			}
			
			out.append(builder);
			
			if (false == foundTable) {
				out.append(String.format("Could not find any J9MonitorTableListEntryPointer at address %s\n", ptr.getHexAddress()));
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
	
	
	/**
	 * See {@link MonitorsCommand#helpCommand(String[], PrintStream)} for
	 * function documentation
	 * 
	 * @param args
	 *            command args
	 * @param out
	 *            the output stream
	 */
	private void tablesCommand(String[] args, PrintStream out) throws DDRInteractiveCommandException
	{	
		try {
			
			MonitorTableListIterator iterator = new MonitorTableListIterator();
			MonitorTable previousMonitorTable = null;

			while (iterator.hasNext()) {
				MonitorTable currentMonitorTable = iterator.currentMonitorTable();

				if (!currentMonitorTable.equals(previousMonitorTable)) {
					/* Print header for new monitor table */
					out.println(iterator.currentMonitorTable().extraInfo());
				}

				previousMonitorTable = currentMonitorTable;
				iterator.next();
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
	
	
	/**
	 * See {@link MonitorsCommand#helpCommand(String[], PrintStream)} for
	 * function documentation
	 * 
	 * @param args
	 *            command args
	 * @param out
	 *            the output stream
	 */
	private void objectCommand(String[] args, PrintStream out) throws DDRInteractiveCommandException
	{
		FilterOptions filter = FilterOptions.defaultOption();

		try {
			J9ObjectPointer object = null;

			if (args.length < 2) {
				out.println("This command takes one address \"!monitors object <J9Object address>\"");
				return;
			}

			long objectAddr = Long.decode(args[1]);
			object = J9ObjectPointer.cast(objectAddr);

			ObjectMonitor objMonitor = ObjectMonitor.fromJ9Object(object);

			if (null == objMonitor) {
				out.printf("No corresponding monitor was found for %s\n", object.getHexAddress());
			} else {
				writeObjectMonitor(filter, objMonitor, out);
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}

	}
	
	
	/**
	 * Helper to print out details for an object monitor.
	 * 
	 * @param objectMonitor
	 *            the DDR monitor we are interested in
	 * @param out
	 *            the PrintStream we write to
	 * @throws CorruptDataException
	 */
	private void writeObjectMonitor(FilterOptions filter, ObjectMonitor objectMonitor, PrintStream out) throws CorruptDataException
	{
		StringBuilder buf = new StringBuilder();
		writeObjectMonitorToBuffer(filter, objectMonitor, buf);
		out.print(buf.toString());
	}
	
	
	/**
	 * Helper to write details for an object monitor to a buffer.
	 * 
	 * @param objectMonitor
	 * @param out
	 * @throws CorruptDataException
	 */
	private void writeObjectMonitorToBuffer(FilterOptions filter, ObjectMonitor objMon, StringBuilder out) throws CorruptDataException
	{	
		
		if (filter.shouldPrint(objMon)) {
		
			out.append(String.format("Object monitor for %s\t\n", objMon.getObject().formatShortInteractive()));
		
			if (objMon.isInflated()) {
				out.append("\tInflated\n");
			} else {
				if (objMon.isContended()) {
					out.append("\tContended Flatlocked\n");
				} else if (objMon.getLockword().isNull()) {
					out.append("\tDeflated\n");
				} else {
					out.append("\tFlatlocked\n");
				}
			}
			
			J9ObjectMonitorPointer j9ObjMonPtr = objMon.getJ9ObjectMonitorPointer();
			
			if (j9ObjMonPtr.notNull()) {
				out.append(String.format("\t%s   %s\n",
						j9ObjMonPtr.formatShortInteractive(),
						j9ObjMonPtr.monitor().formatShortInteractive()));
			}
				
			J9VMThreadPointer ownerThreadPointer = objMon.getOwner();
			
			if (ownerThreadPointer.notNull()) {
				out.append(String.format("\tOwner:\t%s\t%s\n", 
						ownerThreadPointer.formatShortInteractive(),
						J9VMThreadHelper.getName(ownerThreadPointer)));
			}
			
			if (!objMon.getBlockedThreads().isEmpty()) {
				out.append(String.format("\t%s\t\n","Blocking (Enter Waiter):"));
				for (J9VMThreadPointer threadPtr : objMon.getBlockedThreads()) {
					out.append(String.format("\t\t%s\t%s\n", 
							threadPtr.formatShortInteractive(),
							J9VMThreadHelper.getName(threadPtr)));
				}
			}
			if (!objMon.getWaitingThreads().isEmpty()) {
				out.append(String.format("\t%s\t\n","Waiting (Notify Waiter):"));
				for (J9VMThreadPointer threadPtr : objMon.getWaitingThreads()) {
					out.append(String.format("\t\t%s\t%s\n", 
							threadPtr.formatShortInteractive(),
							J9VMThreadHelper.getName(threadPtr)));
				}
			}
			
			out.append(nl);
		}		
	}
	
	
	/**
	 * Helper to print out details for a system monitor.
	 * 
	 * @param filter
	 * @param monitor
	 * @param out
	 * @throws CorruptDataException
	 */
	private void writeSystemMonitor(FilterOptions filter, SystemMonitor monitor, PrintStream out) throws CorruptDataException
	{
		StringBuilder buf = new StringBuilder();
		writeSystemMonitorToBuffer(filter, monitor, buf);
		out.print(buf.toString());
	}
	
	
	/**
	 * Helper to write details for a system monitor to a buffer.
	 * 
	 * @param filter
	 * @param monitor
	 * @param out
	 * @throws CorruptDataException
	 */
	private void writeSystemMonitorToBuffer(FilterOptions filter, SystemMonitor monitor, StringBuilder out) throws CorruptDataException
	{
		if (filter.shouldPrint(monitor)) {
		
			out.append(String.format("%s\t%s\n", monitor.getRawMonitor().formatShortInteractive(), monitor.getName()));
			
			// Step 1: Check for owner:
			J9ThreadPointer ownerPtr = monitor.getOwner();
			
			if (!ownerPtr.isNull()) {
				out.append(String.format("\tOwner:\t%s\n", ownerPtr.formatShortInteractive()));
			}
			
			// Step 2: Check if there are blocked (enter waiters) threads:
			List<String> blockedThreads = threadsListHelper(monitor.getBlockedThreads());
			
			if (!blockedThreads.isEmpty()) {
				out.append("\tBlocking (Enter Waiter):\n");
				for (String aThread : blockedThreads) {
					out.append(String.format("\t\t%s\n", aThread));
				}
			}
			
			// Step 3: Check if there are waiting (notify waiters) threads:
			List<String> notifyThreads = threadsListHelper(monitor.getWaitingThreads());
			
			if (!notifyThreads.isEmpty()) {
				out.append("\tWaiting (Notify Waiter):\n");
				for (String aThread : notifyThreads) {
					out.append(String.format("\t\t%s\n", aThread));
				}
			}
		}
	}
	
	
	/**
	 * See {@link MonitorsCommand#helpCommand(String[], PrintStream)} for
	 * function documentation
	 * 
	 * @param args
	 *            command args
	 * @param out
	 *            the output stream
	 */
	private void allCommand(String[] args, PrintStream out) throws DDRInteractiveCommandException
	{
		
		FilterOptions filter = FilterOptions.defaultOption();
		if (args.length > 0) {
			// For !monitors [ owned | waiting | blocked | active | all ]
			filter = FilterOptions.parseOption(args[0]);
		}
		
		out.println("----------------");
		out.println("Object Monitors:");
		out.println("----------------");
		objectsCommand(filter, out);
		out.println("----------------");
		out.println("System Monitors:");
		out.println("----------------");
		systemCommand(filter, out);
	}

	/**
	 * Function documentation
	 * 
	 * @param args
	 *            command args
	 * @param out
	 *            the output stream
	 */
	private void helpCommand(String[] args, PrintStream out)
	{
		String blob = 
				"!monitors table <address> [ owned | waiting | blocked | active | all ]\n"
				+ "	- dump object monitor tables individually\n"
				+ "	- see \"!monitors system\" for owned/waiting/blocked/active/all description\n"
				+ "\n"
				+ "!monitors tables\n"
				+ "	- dump the object monitor table list\n"
				+ "\n"
				+ "!monitors system [ owned | waiting | blocked | active | all ]\n"
				+ "	- dump system monitors, where:\n"
				+ "		owned\n"
				+ "			- owned by a thread\n"
				+ "		waiting\n"
				+ "			- a thread is waiting on it\n"
				+ "		blocked\n"
				+ "			- a thread is blocked on it\n"
				+ "		active\n"
				+ "			- any of owned, waiting, blocked\n"
				+ "		all\n"
				+ "			- any monitor\n"
				+ "\n"
				+ "!monitors object <address>\n"
				+ "	- dump object monitors corresponding to the specified object (either flat locked or inflated)\n"
				+ "\n"
				+ "!monitors objects [ owned | waiting | blocked | active | all ]\n"
				+ "	- dump all object monitors\n"
				+ "	- see \"!monitors system\" for owned/waiting/blocked/active/all description\n"
				+ "\n"
				+ "!monitors j9thread <address>\n"
				+ "	- dump all monitors that are active for a J9Thread\n"
				+ "\n"
				+ "!monitors j9vmthread <address>\n"
				+ "	- dump all monitors that are active for a J9VMThread\n"
				+ "\n"
				+ "!monitors thread <address>\n"
				+ "	- dump all monitors that are active for the specified J9Thread/J9VMThread/java.lang.Thread\n"
				+ "\n"
				+ "!monitors deadlock\n"
				+ "	- search for deadlock conditions (supports J9Threads and J9VMThreads)\n"
				+ "\n"
				+ "!monitors [ owned | waiting | blocked | active | all ]\n"
				+ "	- dump the system and flat locked or inflated object monitors\n"
				+ "	- see \"!monitors system\" for owned/waiting/blocked/active/all description\n";

		out.print(blob);
	}
}
