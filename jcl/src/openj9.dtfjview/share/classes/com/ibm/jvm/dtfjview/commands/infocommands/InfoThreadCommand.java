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
import java.lang.reflect.Modifier;
import java.math.BigInteger;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Properties;
import java.util.logging.Level;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DTFJException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.image.ImageRegister;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.image.ImageStackFrame;
import com.ibm.dtfj.image.ImageThread;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaField;
import com.ibm.dtfj.java.JavaLocation;
import com.ibm.dtfj.java.JavaMonitor;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaReference;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.java.JavaStackFrame;
import com.ibm.dtfj.java.JavaThread;
import com.ibm.dtfj.runtime.ManagedRuntime;
import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;
import com.ibm.jvm.dtfjview.commands.BaseJdmpviewCommand;
import com.ibm.jvm.dtfjview.commands.helpers.Exceptions;
import com.ibm.jvm.dtfjview.commands.helpers.MonitorState;
import com.ibm.jvm.dtfjview.commands.helpers.StateToString;
import com.ibm.jvm.dtfjview.commands.helpers.ThreadData;
import com.ibm.jvm.dtfjview.commands.helpers.Utils;

@DTFJPlugin(version="1.*", runtime=false)
public class InfoThreadCommand extends BaseJdmpviewCommand {
	
	private int _pointerSize;
	private boolean _is_zOS = false;
	private Map<JavaThread, MonitorState> monitors = new HashMap<JavaThread, MonitorState>();
	private long lastRTAddress = 0;
	private final static String JAVA_LANG_THREAD_CLASS = "java/lang/Thread";
	
	{
		addCommand("info thread", "[<native thread ID>|<zOS TCB>|all|*]", "Displays information about Java and native threads");	
	}
	
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		try {
			long currentRTAddress = ctx.getRuntime().getJavaVM().getAddress();
			if(currentRTAddress != lastRTAddress) {
				lastRTAddress = currentRTAddress;
				monitors = new HashMap<JavaThread, MonitorState>();		//reset the monitors
			}
		} catch (Exception e) {
			logger.fine("Error getting address of the JVM, cannot use cached monitor values. No JVM in process?");
			logger.log(Level.FINEST, "Error getting address of the JVM", e);
			monitors = new HashMap<JavaThread, MonitorState>();		//reset the monitors
		}
		if (monitors.isEmpty() && ctx.getRuntime() != null) {
			getMonitors(ctx.getRuntime());
		}
		doCommand(args);
	}
	
	public void doCommand(String[] args) {
		
		try {
			_is_zOS = ctx.getImage().getSystemType().toLowerCase().indexOf("z/os") >= 0;
		} catch (DataUnavailable e) {
			out.print(Exceptions.getDataUnavailableString());
		} catch (CorruptDataException e) {
			out.print(Exceptions.getCorruptDataExceptionString());
		}
		
		String param = null;
		
		switch(args.length) {
			case 0 :
				try {
					ImageThread it = ctx.getProcess().getCurrentThread();
					if (null != it) {
						param = it.getID();
					} else {
						out.print("\nNo current (failing) thread, try specifying a native thread ID, \"all\" or \"*\"\n");
						
						ImageProcess ip = ctx.getProcess();
						if (ip!=null){
							printThreadSummary(ip);
						}
						return;
					}
				} catch (CorruptDataException e) {
					out.println("exception encountered while getting information about current thread");
					return;
				}
				break;
			case 1 :
				if(args[0].equalsIgnoreCase("ALL")) {
					param = "*";
				} else {
					param = args[0];
				}
				break;
			default :
				out.println("\"info thread\" takes at most one parameter, which, if specified, must be a native thread ID or \"all\" or \"*\"");
				return;
		}
		
		if (param.equals("*")) {
			printAddressSpaceInfo(null, getJavaThreads(null));
		} else {
			printAddressSpaceInfo(param, getJavaThreads(param));
		}
	}
	
	private void printAddressSpaceInfo(String id, Map threads)
	{
		Iterator itAddressSpace;
		ImageAddressSpace ias;
		int asnum = 0;
		
		if (id == null) { // omit header line if we are only printing one thread
			out.print("native threads for address space\n");
		}
		printProcessInfo(id, threads);

		
		if (!threads.isEmpty()) {
			out.print("\nJava threads not associated with known native threads:\n\n");
			
			// retrieve any orphaned Java threads from the hashmap
			ArrayList ta = (ArrayList)threads.remove(null);
			if ( ta != null ) {
				for ( int i=0; i<ta.size(); i++) {
					ThreadData td = (ThreadData)ta.get(i);
					printJavaThreadInfo(td.getThread(), false);
				}
			}
			// If that still hasn't emptied the list we've managed to find id's for threads
			if( !threads.isEmpty() ) {
				Iterator remainingThreads = threads.values().iterator();
				while( remainingThreads.hasNext() ) {
					ThreadData td = (ThreadData)remainingThreads.next();
					if( td == null ) {
						continue;
					}
					printJavaThreadInfo(td.getThread(), false);
				}
			}
		}
	}
	
	private void printProcessInfo(String id, Map threads)
	{
		Iterator itProcess;
		ImageProcess ip;
		boolean found = false;
		
		ip = ctx.getProcess();
		_pointerSize = ip.getPointerSize();
		
		out.print(" process id: ");
		try {
			out.print(ip.getID());
		} catch (DataUnavailable d) {
			out.print(Exceptions.getDataUnavailableString());
		} catch (CorruptDataException e) {
			out.print(Exceptions.getCorruptDataExceptionString());
		}
		out.print("\n\n");
		found = printThreadInfo(id, threads);
		
		if (!found) {
			out.print(" no native threads found with specified id\n");
		}
	}
	
	private boolean printThreadInfo(String id, Map threads)
	{
		Iterator itThread;
		ImageThread it;
		boolean found = false;
		
		itThread = ctx.getProcess().getThreads();
		while (itThread.hasNext())
		{
			Object next = itThread.next();
			if (next instanceof CorruptData) {
				out.print("\n  <corrupt data>");
				continue;
			}
			it = (ImageThread)next;
			
			// Obtain the native thread ID for this thread, and for zOS also obtain the TCB
			String currentTID = null;
			String currentTCB = null;
			try {
				currentTID = it.getID();
				if (_is_zOS) {
					currentTCB = it.getProperties().getProperty("TCB");
				}
			} catch (CorruptDataException e) {
				// Continue with what we have obtained so far
			}
			
			// If the user request was for all threads (id parameter is null) or if we have found a match for
			// the requested native thread ID or zOS TCB, then go ahead and print out this thread.  
			if (null == id || id.equalsIgnoreCase(currentTID) || id.equalsIgnoreCase(currentTCB)) {
				out.print("  thread id: ");
				out.print(currentTID);
				out.print("\n");
				printRegisters(it);
				
				out.print("   native stack sections:");
				out.print("\n");
				
				Iterator itStackSection = it.getStackSections();
				while (itStackSection.hasNext()) {
					Object nextStackSection = itStackSection.next();
					if (nextStackSection instanceof CorruptData) {
						out.print("    " + Exceptions.getCorruptDataExceptionString() + "\n");
						continue;
					}
					ImageSection is = (ImageSection)nextStackSection;
					printStackSection(is);
				}
				printStackFrameInfo(it);
				
				out.print("   properties:");
				out.print("\n");
				printProperties(it);
				
				out.print("   associated Java thread: ");
				ThreadData td = (ThreadData)threads.remove(currentTID);
				if (null != td) {
					out.print("\n");
					printJavaThreadInfo(td.getThread(), true);
				} else {
					out.print("<no associated Java thread>\n");
				}
				out.print("\n");
				found = true;
			}
		}
		
		return found;
	}
	
	public void printRegisters(ImageThread it)
	{
		out.print("   registers:");
		int count = 0;
		Iterator itImageRegister = it.getRegisters();
		while (itImageRegister.hasNext()) {
			if (count % 4 == 0) {
				out.print("\n ");
			}
			out.print("   ");
			ImageRegister ir = (ImageRegister)itImageRegister.next();
			printRegisterInfo(ir);
			count++;
		}
		out.print("\n");
	}
	
	public void printRegisterInfo(ImageRegister ir)
	{
		out.print(Utils.padWithSpaces(ir.getName(), 6) + " = ");
		try {
			Number value = ir.getValue();
			if (value instanceof BigInteger) {
				BigInteger bigValue = (BigInteger) value;
				int width = bigValue.bitLength();
				if (width > 64) {
					// round up to a multiple of 64 bits
					int paddedBits = ((width - 1) | 63) + 1;
					out.print("0x" + Utils.padWithZeroes(bigValue.toString(16), paddedBits / 4));
					return;
				}
			}
			long registerValue = value.longValue();
			if (_pointerSize > 32) {
				out.print("0x" + Utils.toFixedWidthHex(registerValue));
			} else {
				if (_is_zOS) { // Show high and low word separated by '_' as in IPCS etc
					out.print("0x" + Utils.toFixedWidthHex((int)(registerValue >> 32))
							 + "_" + Utils.toFixedWidthHex((int)registerValue));
				} else {
					out.print("0x" + Utils.toFixedWidthHex((int)registerValue));
				}
			}
		} catch (CorruptDataException e) {
			out.print(Exceptions.getCorruptDataExceptionString());
		}
	}
	
	public void printStackSection(ImageSection is)
	{
		long startAddr = is.getBaseAddress().getAddress();
		long size = is.getSize();
		long endAddr = startAddr + size;

		out.print("    ");
		out.print(Utils.toHex(startAddr));
		out.print(" to ");
		out.print(Utils.toHex(endAddr));
		out.print(" (length ");
		out.print(Utils.toHex(size));
		out.print(")\n");
	}
	
	private void printStackFrameInfo(ImageThread it)
	{
		Iterator itStackFrame;
		ImageStackFrame isf;
		
		try {
			itStackFrame = it.getStackFrames();
		} catch (DataUnavailable d) {
			out.print("   native stack frames: " + Exceptions.getDataUnavailableString() + "\n");
			return;
		}
		
		out.print("   native stack frames:");
		out.print("\n");

		while (itStackFrame.hasNext()) {
			Object o = itStackFrame.next();
			if (o instanceof CorruptData) {
				out.print("    <corrupt stack frame: "+((CorruptData)o).toString()+">\n");
				continue;
			}
			isf = (ImageStackFrame)o;
			out.print("    bp: ");
			try {
				out.print(toAdjustedHex(isf.getBasePointer().getAddress()));
			} catch (CorruptDataException e) {
				if (getArtifactType() == ArtifactType.javacore) {
					// javacore does not provide native stack base pointers, show as unavailable, not corrupt
					out.print(Exceptions.getDataUnavailableString());
				} else {
					out.print(Exceptions.getCorruptDataExceptionString());
				}
			}
			out.print(" pc: ");
			try {
				out.print(toAdjustedHex(isf.getProcedureAddress().getAddress()));
			} catch (CorruptDataException e) {
				out.print(Exceptions.getCorruptDataExceptionString());
			}
			out.print(" ");
			try {
				out.print(isf.getProcedureName());
			} catch (CorruptDataException e) {
				out.print(Exceptions.getCorruptDataExceptionString());
			}
			out.print("\n");
		}
	}
	
	private Map getJavaThreads(String id)
	{
		Map threads = new HashMap();
		ManagedRuntime mr = ctx.getRuntime();
		if (mr instanceof JavaRuntime) {
			JavaRuntime jr = (JavaRuntime)mr;
			Iterator itThread = jr.getThreads();
			
			while (itThread.hasNext()) {
				Object next = itThread.next();
				if (next instanceof CorruptData) continue; // skip any corrupt threads
				
				JavaThread jt = (JavaThread)next;
				
				// Obtain the native thread ID for this thread, and for zOS also obtain the TCB
				String currentTID = null;
				String currentTCB = null;
				try {
					ImageThread it = jt.getImageThread();  
					currentTID = it.getID();
					if (_is_zOS) {
						currentTCB = it.getProperties().getProperty("TCB");
					}
				} catch (DTFJException e) {
					// Continue with what we have obtained so far
				}
				
				if (null == id) {
					// thread id set to null means we want all the threads, and we
					// save all orphaned java threads in a list within the hashmap
					if ( null == currentTID) {
						if (threads.containsKey(null)) {
							ArrayList ta = (ArrayList)threads.get(null);
							ta.add(new ThreadData(jt, jr));
						} else {
							ArrayList ta = new ArrayList(1);
							ta.add(new ThreadData(jt, jr));
							threads.put(null, ta);
						}
					} else {
						threads.put(currentTID, new ThreadData(jt, jr));
					}
				} else if (id.equalsIgnoreCase(currentTID) || id.equalsIgnoreCase(currentTCB)) {
					// We just want the specific Java thread that matches the native thread ID or zOS TCB
					threads.put(currentTID, new ThreadData(jt, jr));
				} 
			}
		}
		
		return threads;
	}
	
	@SuppressWarnings("rawtypes")
	private void getMonitors(JavaRuntime jr)
	{
		int corruptThreadCount = 0;
		int corruptMonitorCount = 0;
		Iterator itMonitor = jr.getMonitors();
		while (itMonitor.hasNext()) {
			Object obj = itMonitor.next();
			if(obj instanceof CorruptData) {
				corruptMonitorCount++;
				continue;
			}
			JavaMonitor jm = (JavaMonitor)obj;
			Iterator itEnterWaiter = jm.getEnterWaiters();
			while (itEnterWaiter.hasNext()) {
				obj = itEnterWaiter.next();
				if(obj instanceof CorruptData) {
					corruptThreadCount++;
					continue;
				}
				JavaThread jt = (JavaThread)obj;
				monitors.put(jt, new MonitorState(jm, MonitorState.WAITING_TO_ENTER));
			}
			if(corruptThreadCount > 0) {
				String msg = String.format("Warning : %d corrupt thread(s) were found waiting to enter monitor %s", corruptThreadCount, getMonitorName(jm));
				logger.fine(msg);
			}
			corruptThreadCount = 0;
			Iterator itNotifyWaiter = jm.getNotifyWaiters();
			while (itNotifyWaiter.hasNext()) {
				obj = itNotifyWaiter.next();
				if(obj instanceof CorruptData) {
					corruptThreadCount++;
					continue;
				}
				JavaThread jt = (JavaThread) obj;
				monitors.put(jt, new MonitorState(jm, MonitorState.WAITING_TO_BE_NOTIFIED_ON));
			}
			if(corruptThreadCount > 0) {
				String msg = String.format("Warning : %d corrupt thread(s) were found waiting to be notified on monitor %s", corruptThreadCount, getMonitorName(jm));
				logger.fine(msg);
			}
		}
		if(corruptMonitorCount > 0) {
			String msg = String.format("Warning : %d corrupt monitor(s) were found", corruptMonitorCount);
			logger.fine(msg);
		}
	}
	
	private String getMonitorName(JavaMonitor monitor) {
		try {
			return monitor.getName();
		} catch (CorruptDataException e) {
			return "<corrupt monitor name>";
		}
	}
	
	private void printJavaThreadInfo(JavaThread jt, boolean idPrinted)
	{
		out.print("    name:          ");
		try {
			out.print(jt.getName());
		} catch (CorruptDataException e) {
			out.print(Exceptions.getCorruptDataExceptionString());
			logger.log(Level.FINEST, Exceptions.getCorruptDataExceptionString(), e);
		}
		out.print("\n");
		
		if( !idPrinted ) {
			try {
				if( jt.getImageThread() != null ) {
					out.print("    id:            ");
					out.print(jt.getImageThread().getID());
				}
			} catch (CorruptDataException e) {
				out.print(Exceptions.getCorruptDataExceptionString());
				logger.log(Level.FINEST, Exceptions.getCorruptDataExceptionString(), e);
			} catch (DataUnavailable e) {
				out.print(Exceptions.getDataUnavailableString());
				logger.log(Level.FINEST, Exceptions.getDataUnavailableString(), e);
			} finally {
				out.print("\n");
			}
		}
		
		out.print("    Thread object: ");
		try {
			JavaObject threadObj = jt.getObject();
			if (null == threadObj) {
				out.print("<no associated Thread object>");
			} else {
				String threadClassName = null;
				try {
					JavaClass threadClass = threadObj.getJavaClass();
					threadClassName = threadClass.getName();
					if( threadClassName != null ) {
						out.print(threadClassName + " @ " );
					}
					out.print(Utils.toHex(threadObj.getID().getAddress()));
					// Navigate to the parent java/lang.Thread class to get the 'tid' and 'isDaemon' fields
					while (!JAVA_LANG_THREAD_CLASS.equals(threadClass.getName()) && threadClass != null) {
						threadClass = threadClass.getSuperclass();
					}
					if (threadClass != null) {
						Iterator itField = threadClass.getDeclaredFields();
						boolean foundThreadId = false;
						while (itField.hasNext()) {
							JavaField jf = (JavaField)itField.next();
							/* "uniqueId" field in java.lang.Thread is renamed to "tid". The old field name is
							 * checked to preserve functionality of DDR command with old core files that reference
							 * the "uniqueId" field.
							 */
							if (!foundThreadId && (jf.getName().equals("uniqueId") || jf.getName().equals("tid"))) {
								foundThreadId = true;
								out.print("\n    ID:            " + Utils.getVal(threadObj, jf));
							} else if (jf.getName().equals("isDaemon")) {
								out.print("\n    Daemon:        " + Utils.getVal(threadObj, jf));
							}
						}
					}
				} catch (CorruptDataException cde ) {
					out.print(" <in-flight or corrupt data encountered>");
					logger.log(Level.FINEST, Exceptions.getCorruptDataExceptionString(), cde);
				}
			}
		} catch (CorruptDataException cde) {
			out.print(Exceptions.getCorruptDataExceptionString());
			logger.log(Level.FINEST, Exceptions.getCorruptDataExceptionString(), cde);
		}
		out.print("\n");

		out.print("    Priority:      ");
		try {
			Integer pri = Integer.valueOf(jt.getPriority());
			out.print(pri.toString());
		} catch (CorruptDataException e) {
			out.print(Exceptions.getCorruptDataExceptionString());
			logger.log(Level.FINEST, Exceptions.getCorruptDataExceptionString(), e);
		}
		out.print("\n");
		
		out.print("    Thread.State:  ");
		try {
			out.print(StateToString.getThreadStateString(jt.getState()));
		} catch (CorruptDataException cde) {
			out.print(Exceptions.getCorruptDataExceptionString());
			logger.log(Level.FINEST, Exceptions.getCorruptDataExceptionString(), cde);
		}
		out.print("\n");
		
		out.print("    JVMTI state:   ");
		try {
			out.print(StateToString.getJVMTIStateString(jt.getState()));
		} catch (CorruptDataException cde) {
			out.print(Exceptions.getCorruptDataExceptionString());
			logger.log(Level.FINEST, Exceptions.getCorruptDataExceptionString(), cde);
		}
		out.print("\n");
				
		printThreadBlocker(jt);
		
		out.print("    Java stack frames: ");
		printJavaStackFrameInfo(jt);
		out.print("\n");
	}

	private void printThreadBlocker(JavaThread jt) {
		try {
			if( (jt.getState() & JavaThread.STATE_PARKED) != 0 ) {
				out.print("      parked on: ");			
				// java.util.concurrent locks
				if( jt.getBlockingObject() == null ) {
					out.print("<unknown>");
				} else {
					JavaObject jo = jt.getBlockingObject();
					String lockID = Long.toHexString(jo.getID().getAddress());
					out.print(jo.getJavaClass().getName() + "@0x" + lockID);
					String ownerThreadName = "<unknown>";
					String ownerThreadID = "<null>";
					out.print(" owner name: ");
					JavaThread lockOwnerThread = Utils.getParkBlockerOwner(jo, ctx.getRuntime());
					if( lockOwnerThread != null ) {
						ownerThreadName = lockOwnerThread.getName();
						if( lockOwnerThread.getImageThread() != null ) {
							ownerThreadID = lockOwnerThread.getImageThread().getID();
						}
					} else {
						// If the owning thread has ended we won't find the JavaThread
						// We can still get the owning thread name from the java.lang.Thread object itself.
						// We won't get a thread id.
						JavaObject lockOwnerObj = Utils.getParkBlockerOwnerObject(jo, ctx.getRuntime());
						if( lockOwnerObj != null ) {
							ownerThreadName = Utils.getThreadNameFromObject(lockOwnerObj, ctx.getRuntime(), out);
						}
					}
					out.print("\"" + ownerThreadName + "\"");
					out.print(" owner id: " + ownerThreadID);
				}
				out.print("\n");
			} else if( (jt.getState() & JavaThread.STATE_IN_OBJECT_WAIT) != 0 ) {
				out.print("      waiting to be notified on: ");
			} else if( (jt.getState() & JavaThread.STATE_BLOCKED_ON_MONITOR_ENTER) != 0 )  {
				out.print("      waiting to enter: ");
			}
			if( (jt.getState() & JavaThread.STATE_IN_OBJECT_WAIT) != 0 ||
				(jt.getState() & JavaThread.STATE_BLOCKED_ON_MONITOR_ENTER) != 0  ) {
				// java monitors
				MonitorState ms = (MonitorState)monitors.get(jt);
				if(ms == null) {
					out.println("<monitor information not available>");
				} else {
					JavaObject jo = ms.getMonitor().getObject();
					if (null == jo) {
						// working with a raw monitor
						String name = ms.getMonitor().getName();
						if (name.equals("")) {
							name = "<unnamed>";
						}
						out.print("\"" + name + "\"");
						out.print(" with ID ");
						out.print(Utils.toHex(ms.getMonitor().getID().getAddress()));
					} else {
						// working with a Java monitor
						String lockID = Long.toHexString(jo.getID().getAddress());
						out.print(jo.getJavaClass().getName() + "@0x" + lockID);
					}
					out.print(" owner name: ");
					if( ms.getMonitor().getOwner() != null ) {
						out.print("\"" + ms.getMonitor().getOwner().getName() + "\"");
						if( ms.getMonitor().getOwner().getImageThread() != null ) {
							out.print(" owner id: " + ms.getMonitor().getOwner().getImageThread().getID());
						}
					} else {
						out.print("<unowned>");
					}
					out.print("\n");
				}
			}
		} catch (CorruptDataException cde) {
			out.print(Exceptions.getCorruptDataExceptionString());
			logger.log(Level.FINEST, Exceptions.getCorruptDataExceptionString(), cde);
		} catch (DataUnavailable e) {
			out.print(Exceptions.getDataUnavailableString());
			logger.log(Level.FINEST, Exceptions.getDataUnavailableString(), e);
		} catch (MemoryAccessException e) {
			out.print(Exceptions.getMemoryAccessExceptionString());
			logger.log(Level.FINEST, Exceptions.getMemoryAccessExceptionString(), e);
		}
	}
	
	private void printJavaStackFrameInfo(JavaThread jt)
	{
		Iterator itStackFrame;
		JavaStackFrame jsf;
		JavaLocation jl;

		itStackFrame = jt.getStackFrames();
		if (!itStackFrame.hasNext()) {
			out.print("<no frames to print>\n");
			return;
		} else {
			out.print("\n");
		}
		while (itStackFrame.hasNext()) {
			// this iterator can contain JavaStackFrame or CorruptData objects
			Object next = itStackFrame.next();
			if (next instanceof CorruptData) {
				out.print("     " + Exceptions.getCorruptDataExceptionString() + "\n");
				return;
			} else {
				jsf = (JavaStackFrame)next;
			}
			try {
				jl = jsf.getLocation();
			} catch (CorruptDataException e) {
				out.print("     " + Exceptions.getCorruptDataExceptionString()+ "\n");
				logger.log(Level.FINEST, Exceptions.getCorruptDataExceptionString(), e);
				return;
			}
			
			out.print("     bp: ");
			try {
				out.print(toAdjustedHex(jsf.getBasePointer().getAddress()));
			} catch (CorruptDataException e) {
				// jsf.getBasePointer() can't throw DataUnavailable, so we don't know if this is really
				// a corruption. Log the exception but revert to issuing a DataUnavailable message. 
				out.print(Exceptions.getDataUnavailableString());
				logger.log(Level.FINEST, Exceptions.getCorruptDataExceptionString(), e);
			}
			
			out.print("  method: ");
			try {
				String signature = null;
				try {
					signature = jl.getMethod().getSignature();
				} catch (CorruptDataException e) {
					// jl.getMethod() can't throw DataUnavailable, so we don't know if this is really a
					// corruption.  I don't think we need to be pedantic and insert 'not available' where
					// the return type and the parameter types would be. Just print class name and method.
					logger.log(Level.FINEST, Exceptions.getCorruptDataExceptionString(), e);
				}
				if (signature == null) {
					out.print(jl.getMethod().getDeclaringClass().getName() + "." +
							jl.getMethod().getName());
				} else {
					out.print(Utils.getReturnValueName(signature) + " " + 
							jl.getMethod().getDeclaringClass().getName() + "." +
							jl.getMethod().getName() +
							Utils.getMethodSignatureName(signature));
				}
			} catch (CorruptDataException e) {
				out.print(Exceptions.getCorruptDataExceptionString());
				logger.log(Level.FINEST, Exceptions.getCorruptDataExceptionString(), e);
			} catch (DataUnavailable e) {
				out.print(Exceptions.getDataUnavailableString());
				logger.log(Level.FINEST, Exceptions.getDataUnavailableString(), e);
			}

			// Assume the method is a java method in case of corrupt data.
			boolean isNative = false;
			try {
				isNative = Modifier.isNative(jl.getMethod().getModifiers());
			} catch (CorruptDataException e) {
				logger.log(Level.FINEST, Exceptions.getCorruptDataExceptionString(), e);
			}
			if( !isNative) {
				out.print("  source: ");
				try {
					out.print(jl.getFilename());
				} catch (DataUnavailable d) {
					out.print(Exceptions.getDataUnavailableString());
					logger.log(Level.FINEST, Exceptions.getDataUnavailableString(), d);
				} catch (CorruptDataException e) {
					out.print(Exceptions.getCorruptDataExceptionString());
					logger.log(Level.FINEST, Exceptions.getCorruptDataExceptionString(), e);
				}
				out.print(":");
				try {
					out.print(Integer.toString(jl.getLineNumber()));
				} catch (DataUnavailable d) {
					out.print(Exceptions.getDataUnavailableString());
					logger.log(Level.FINE, Exceptions.getDataUnavailableString(), d);
				} catch (CorruptDataException e) {
					out.print(Exceptions.getCorruptDataExceptionString());
					logger.log(Level.FINEST, Exceptions.getCorruptDataExceptionString(), e);
				}
			} else {
				out.print("  (Native Method)");
			}
			
			out.print("\n      objects:");
			Iterator itObjectRefs = jsf.getHeapRoots();
			if (!itObjectRefs.hasNext()) {
				out.print(" <no objects in this frame>");
			}
			while (itObjectRefs.hasNext()) {
				Object nextRef = itObjectRefs.next();
				if (nextRef instanceof CorruptData) {
					out.print(Exceptions.getCorruptDataExceptionString() + "\n");
					break; // give up on this frame
				} else {
					JavaReference jr = (JavaReference)nextRef;
					try {
						if (jr.isObjectReference()) {
							JavaObject target = (JavaObject)(jr.getTarget());
							out.print(" " + Utils.toHex(target.getID().getAddress()));
						}
					} catch (DataUnavailable d) {
						out.print(Exceptions.getDataUnavailableString());
						logger.log(Level.FINEST, Exceptions.getDataUnavailableString(), d);
					} catch (CorruptDataException e) {
						out.print(Exceptions.getCorruptDataExceptionString());
						logger.log(Level.FINEST, Exceptions.getCorruptDataExceptionString(), e);
					} catch (NullPointerException n) {
						out.print(Exceptions.getDataUnavailableString());
						logger.log(Level.FINEST, Exceptions.getDataUnavailableString(), n);
					}
				}
			}
			out.print("\n");
		}
	}

	private String toAdjustedHex(long l)
	{
		if (_pointerSize > 32) {
			return "0x" + Utils.toFixedWidthHex(l);
		} else if (31 == _pointerSize) {
			return "0x" + Utils.toFixedWidthHex((int)(l & (((long)1) << _pointerSize) - 1));
		} else {
			return "0x" + Utils.toFixedWidthHex((int)l);
		}
	}
	
	private void printThreadSummary(ImageProcess ip)
	{
		// Prints a summary list of native thread IDs
		int count = 0;
		Iterator itThread = ip.getThreads();
			
		while (itThread.hasNext()) {
			Object next = itThread.next();
			if (next instanceof CorruptData) continue;
			ImageThread it = (ImageThread)next;
			
			if (count % 8 == 0) {
				if (0 == count) out.print("\n\nNative thread IDs for current process:");
				out.print("\n ");
			}
			
			try {
				out.print(Utils.padWithSpaces(it.getID(), 8));
			} catch (CorruptDataException e) {
				out.print(Exceptions.getCorruptDataExceptionString());
			}
			count++;
		}
		out.print("\n");
	}
	private void printProperties(ImageThread it)
	{
		Properties jtp = it.getProperties();
		String[] keys = jtp.keySet().toArray(new String[0]);
		ArrayList<String> table = new ArrayList<String>(keys.length);
		int maxLen = 0;
		Arrays.sort(keys);
		// We may have a lot of properties so print them out in two columns.
		for( String key: keys ) {
			String formatted = String.format("%s=%s", key, jtp.get(key));
			table.add(formatted);
			maxLen = Math.max(maxLen, formatted.length());
		}
		Iterator<String> tableIterator = table.iterator();
		String tableFormatString = "\t%-"+maxLen+"s\t%-"+maxLen+"s\n";
		while( tableIterator.hasNext() ) {
			out.printf(tableFormatString, tableIterator.next(), tableIterator.hasNext()?tableIterator.next():"");
			
		}
	}

	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println("Displays information about Java and native threads\n\n" +
				"Parameters: none, native thread ID, zOS TCB address, \"all\", or \"*\"\n\n" +
				"If no parameter is supplied, information is printed for the current or failing thread, if any\n" +
				"If a native thread ID or zOS TCB address is supplied, information is printed for that specific thread\n" +
				"If \"all\", or \"*\" is specified, information is printed for all threads\n\n" +
				"The following information is printed for each thread:\n" +
				" - native thread ID\n" +
				" - registers\n" +
				" - native stack sections\n" +
				" - native stack frames: procedure name and base pointer\n" +
				" - thread properties\n" +
				" - associated Java thread (if applicable):\n" +
				"  - name of Java thread\n" +
				"  - address of associated java.lang.Thread object\n" +
				"  - current state according to JVMTI specification\n" +
				"  - current state relative to java.lang.Thread.State\n" +
				"  - the Java thread priority\n" +
				"  - the monitor the thread is waiting to enter or waiting on notify\n" +
				"  - Java stack frames: base pointer, method, source filename and objects on frame\n\n" +
				"Note: the \"info proc\" command provides a summary list of native thread IDs\n"
				);
		
	}
}
