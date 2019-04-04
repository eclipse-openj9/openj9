/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2019 IBM Corp. and others
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
import java.util.Comparator;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.logging.Level;
import java.util.stream.Collectors;
import java.util.stream.IntStream;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DTFJException;
import com.ibm.dtfj.image.DataUnavailable;
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

@DTFJPlugin(version = "1.*", runtime = false)
@SuppressWarnings({ "boxing", "javadoc", "nls" })
public class InfoThreadCommand extends BaseJdmpviewCommand {

	private int _pointerSize;
	private boolean _is_zOS = false;
	private Map<JavaThread, MonitorState> monitors = new HashMap<>();
	private long lastRTAddress = 0;
	private final static String JAVA_LANG_THREAD_CLASS = "java/lang/Thread";

	{
		addCommand("info thread", "[<native thread ID>|<zOS TCB>|all|*]", "Displays information about Java and native threads");
	}

	@Override
	public void run(String command, String[] args, IContext context, PrintStream outStream) throws CommandException {
		if (initCommand(command, args, context, outStream)) {
			return; // processing already handled by superclass
		}
		try {
			long currentRTAddress = ctx.getRuntime().getJavaVM().getAddress();
			if (currentRTAddress != lastRTAddress) {
				lastRTAddress = currentRTAddress;
				monitors = new HashMap<>(); // reset the monitors
			}
		} catch (Exception e) {
			logger.fine("Error getting address of the JVM, cannot use cached monitor values. No JVM in process?");
			logger.log(Level.FINEST, "Error getting address of the JVM", e);
			monitors = new HashMap<>(); // reset the monitors
		}
		if (monitors.isEmpty() && ctx.getRuntime() != null) {
			getMonitors(ctx.getRuntime());
		}
		doCommand(args);
	}

	public void doCommand(String[] args) {
		try {
			_is_zOS = ctx.getImage().getSystemType().toLowerCase().contains("z/os");
		} catch (DataUnavailable e) {
			out.print(Exceptions.getDataUnavailableString());
		} catch (CorruptDataException e) {
			out.print(Exceptions.getCorruptDataExceptionString());
		}

		String param = null;

		switch (args.length) {
		case 0:
			try {
				ImageThread it = ctx.getProcess().getCurrentThread();
				if (null != it) {
					param = it.getID();
				} else {
					out.println();
					out.println("No current (failing) thread, try specifying a native thread ID, \"all\" or \"*\"");

					ImageProcess ip = ctx.getProcess();
					if (ip != null) {
						printThreadSummary(ip);
					}
					return;
				}
			} catch (CorruptDataException e) {
				out.println("exception encountered while getting information about current thread");
				return;
			}
			break;
		case 1:
			param = args[0];
			if ("ALL".equalsIgnoreCase(param) || "*".equals(param)) {
				param = null;
			}
			break;
		default:
			out.println("\"info thread\" takes at most one parameter, which, if specified, must be a native thread ID or \"all\" or \"*\"");
			return;
		}

		printAddressSpaceInfo(param, getJavaThreads(param));
	}

	private void printAddressSpaceInfo(String id, Map<String, List<ThreadData>> threads) {
		if (id == null) { // omit header line if we are only printing one thread
			out.println("native threads for address space");
		}

		printProcessInfo(id, threads);

		if (!threads.isEmpty()) {
			out.println("Java threads not associated with known native threads:");
			out.println();

			// retrieve any orphaned Java threads from the hashmap
			List<ThreadData> orphans = threads.remove(null);

			if (orphans != null) {
				for (ThreadData orphan : orphans) {
					printJavaThreadInfo(orphan.getThread(), false);
				}
			}

			// If that hasn't emptied the list we've managed to find ids for threads.
			for (List<ThreadData> list : threads.values()) {
				for (ThreadData td : list) {
					printJavaThreadInfo(td.getThread(), false);
				}
			}
		}
	}

	private void printProcessInfo(String id, Map<String, List<ThreadData>> threads) {
		ImageProcess ip = ctx.getProcess();
		_pointerSize = ip.getPointerSize();

		out.print(" process id: ");
		try {
			out.print(ip.getID());
		} catch (DataUnavailable d) {
			out.print(Exceptions.getDataUnavailableString());
		} catch (CorruptDataException e) {
			out.print(Exceptions.getCorruptDataExceptionString());
		}
		out.println();
		out.println();

		boolean found = printThreadInfo(id, threads);

		if (!found) {
			out.println(" no native threads found with specified id");
			out.println();
		}
	}

	private boolean printThreadInfo(String id, Map<String, List<ThreadData>> threads) {
		boolean found = false;
		Iterator<?> itThread = ctx.getProcess().getThreads();

		while (itThread.hasNext()) {
			Object next = itThread.next();
			if (next instanceof CorruptData) {
				out.println();
				out.print("  <corrupt data>");
				continue;
			}
			ImageThread it = (ImageThread) next;

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
				out.println(currentTID);
				printRegisters(it);

				out.println("   native stack sections:");

				Iterator<?> itStackSection = it.getStackSections();
				while (itStackSection.hasNext()) {
					Object nextStackSection = itStackSection.next();
					if (nextStackSection instanceof CorruptData) {
						out.println("    " + Exceptions.getCorruptDataExceptionString());
						continue;
					}
					ImageSection is = (ImageSection) nextStackSection;
					printStackSection(is);
				}
				printStackFrameInfo(it);

				out.println("   properties:");
				printProperties(it);

				out.print("   associated Java thread: ");
				List<ThreadData> list = threads.remove(currentTID);
				if ((null == list) || list.isEmpty()) {
					out.println("<no associated Java thread>");
				} else {
					out.println();
					printJavaThreadInfo(list.get(0).getThread(), true);
				}
				out.println();
				found = true;
			}
		}

		return found;
	}

	public void printRegisters(ImageThread it) {
		out.print("   registers:");
		int count = 0;
		Iterator<?> itImageRegister = it.getRegisters();
		while (itImageRegister.hasNext()) {
			if (count % 4 == 0) {
				out.println();
				out.print(" ");
			}
			out.print("   ");
			ImageRegister ir = (ImageRegister) itImageRegister.next();
			printRegisterInfo(ir);
			count += 1;
		}
		out.println();
	}

	public void printRegisterInfo(ImageRegister ir) {
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

	public void printStackSection(ImageSection is) {
		long startAddr = is.getBaseAddress().getAddress();
		long size = is.getSize();
		long endAddr = startAddr + size;

		out.print("    ");
		out.print(Utils.toHex(startAddr));
		out.print(" to ");
		out.print(Utils.toHex(endAddr));
		out.print(" (length ");
		out.print(Utils.toHex(size));
		out.println(")");
	}

	private void printStackFrameInfo(ImageThread it) {
		Iterator<?> itStackFrame;

		try {
			itStackFrame = it.getStackFrames();
		} catch (DataUnavailable d) {
			out.println("   native stack frames: " + Exceptions.getDataUnavailableString());
			return;
		}

		out.println("   native stack frames:");

		while (itStackFrame.hasNext()) {
			Object o = itStackFrame.next();
			if (o instanceof CorruptData) {
				out.println("    <corrupt stack frame: " + ((CorruptData) o).toString() + ">");
				continue;
			}
			ImageStackFrame isf = (ImageStackFrame) o;
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
			out.println();
		}
	}

	private Map<String, List<ThreadData>> getJavaThreads(String id) {
		Map<String, List<ThreadData>> threads = new HashMap<>();
		ManagedRuntime mr = ctx.getRuntime();

		if (mr instanceof JavaRuntime) {
			JavaRuntime jr = (JavaRuntime) mr;
			Iterator<?> itThread = jr.getThreads();

			while (itThread.hasNext()) {
				Object next = itThread.next();

				if (next instanceof CorruptData) {
					continue; // skip any corrupt threads
				}

				JavaThread jt = (JavaThread) next;

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

				// thread id set to null means we want all the threads, and we
				// save all orphaned java threads in a list within the hashmap
				// We just want the specific Java thread that matches the native thread ID or zOS TCB
				if (null == id || id.equalsIgnoreCase(currentTID) || id.equalsIgnoreCase(currentTCB)) {
					List<ThreadData> ta = threads.get(currentTID);
					if (ta == null) {
						ta = new ArrayList<>(1);
						threads.put(currentTID, ta);
					}
					ta.add(new ThreadData(jt, jr));
				}
			}
		}

		return threads;
	}

	private void getMonitors(JavaRuntime jr) {
		int corruptThreadCount = 0;
		int corruptMonitorCount = 0;
		Iterator<?> itMonitor = jr.getMonitors();
		while (itMonitor.hasNext()) {
			Object obj = itMonitor.next();
			if (obj instanceof CorruptData) {
				corruptMonitorCount += 1;
				continue;
			}
			JavaMonitor jm = (JavaMonitor) obj;
			Iterator<?> itEnterWaiter = jm.getEnterWaiters();
			while (itEnterWaiter.hasNext()) {
				obj = itEnterWaiter.next();
				if (obj instanceof CorruptData) {
					corruptThreadCount += 1;
					continue;
				}
				JavaThread jt = (JavaThread) obj;
				monitors.put(jt, new MonitorState(jm, MonitorState.WAITING_TO_ENTER));
			}
			if (corruptThreadCount > 0) {
				String msg = String.format("Warning : %d corrupt thread(s) were found waiting to enter monitor %s",
						corruptThreadCount, getMonitorName(jm));
				logger.fine(msg);
			}
			corruptThreadCount = 0;
			Iterator<?> itNotifyWaiter = jm.getNotifyWaiters();
			while (itNotifyWaiter.hasNext()) {
				obj = itNotifyWaiter.next();
				if (obj instanceof CorruptData) {
					corruptThreadCount += 1;
					continue;
				}
				JavaThread jt = (JavaThread) obj;
				monitors.put(jt, new MonitorState(jm, MonitorState.WAITING_TO_BE_NOTIFIED_ON));
			}
			if (corruptThreadCount > 0) {
				String msg = String.format(
						"Warning : %d corrupt thread(s) were found waiting to be notified on monitor %s",
						corruptThreadCount, getMonitorName(jm));
				logger.fine(msg);
			}
		}
		if (corruptMonitorCount > 0) {
			String msg = String.format("Warning : %d corrupt monitor(s) were found", corruptMonitorCount);
			logger.fine(msg);
		}
	}

	private static String getMonitorName(JavaMonitor monitor) {
		try {
			return monitor.getName();
		} catch (CorruptDataException e) {
			return "<corrupt monitor name>";
		}
	}

	private void printJavaThreadInfo(JavaThread jt, boolean idPrinted) {
		out.print("    name:          ");
		try {
			out.print(jt.getName());
		} catch (CorruptDataException e) {
			out.print(Exceptions.getCorruptDataExceptionString());
			logger.log(Level.FINEST, Exceptions.getCorruptDataExceptionString(), e);
		}
		out.println();

		if (!idPrinted) {
			try {
				if (jt.getImageThread() != null) {
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
				out.println();
			}
		}

		out.print("    Thread object: ");
		try {
			JavaObject threadObj = jt.getObject();
			if (null == threadObj) {
				out.print("<no associated Thread object>");
			} else {
				try {
					JavaClass threadClass = threadObj.getJavaClass();
					String threadClassName = threadClass.getName();
					if (threadClassName != null) {
						out.print(threadClassName + " @ ");
					}
					out.print(Utils.toHex(threadObj.getID().getAddress()));
					// navigate to the parent java/lang/Thread class to get the 'tid' and 'isDaemon' fields
					while (threadClass != null && !JAVA_LANG_THREAD_CLASS.equals(threadClass.getName())) {
						threadClass = threadClass.getSuperclass();
					}
					if (threadClass != null) {
						Iterator<?> itField = threadClass.getDeclaredFields();
						boolean foundThreadId = false;
						while (itField.hasNext()) {
							JavaField jf = (JavaField) itField.next();
							switch (jf.getName()) {
							case "uniqueId":
								/* The field "uniqueId" in java.lang.Thread was renamed "tid".
								 * We still recognize the old name to preserve functionality
								 * for old core files using that name.
								 */
								// FALL THROUGH
							case "tid":
								if (!foundThreadId) {
									out.println();
									out.print("    ID:            " + Utils.getVal(threadObj, jf));
									foundThreadId = true;
								}
								break;
							case "isDaemon":
								out.println();
								out.print("    Daemon:        " + Utils.getVal(threadObj, jf));
								break;
							case "threadRef":
								try {
									String hex = Utils.toHex(jf.getLong(threadObj));

									out.println();
									out.print("    Native info:   !j9vmthread " + hex + "  !stack " + hex);
								} catch (MemoryAccessException e) {
									// omit unavailable information
								}
								break;
							default:
								break;
							}
						}
					}
				} catch (CorruptDataException cde) {
					out.print(" <in-flight or corrupt data encountered>");
					logger.log(Level.FINEST, Exceptions.getCorruptDataExceptionString(), cde);
				}
			}
		} catch (CorruptDataException cde) {
			out.print(Exceptions.getCorruptDataExceptionString());
			logger.log(Level.FINEST, Exceptions.getCorruptDataExceptionString(), cde);
		}
		out.println();

		out.print("    Priority:      ");
		try {
			Integer pri = Integer.valueOf(jt.getPriority());
			out.print(pri.toString());
		} catch (CorruptDataException e) {
			out.print(Exceptions.getCorruptDataExceptionString());
			logger.log(Level.FINEST, Exceptions.getCorruptDataExceptionString(), e);
		}
		out.println();

		out.print("    Thread.State:  ");
		try {
			out.print(StateToString.getThreadStateString(jt.getState()));
		} catch (CorruptDataException cde) {
			out.print(Exceptions.getCorruptDataExceptionString());
			logger.log(Level.FINEST, Exceptions.getCorruptDataExceptionString(), cde);
		}
		out.println();

		out.print("    JVMTI state:   ");
		try {
			out.print(StateToString.getJVMTIStateString(jt.getState()));
		} catch (CorruptDataException cde) {
			out.print(Exceptions.getCorruptDataExceptionString());
			logger.log(Level.FINEST, Exceptions.getCorruptDataExceptionString(), cde);
		}
		out.println();

		printThreadBlocker(jt);

		out.print("    Java stack frames: ");
		printJavaStackFrameInfo(jt);
		out.println();
	}

	private void printThreadBlocker(JavaThread jt) {
		try {
			if ((jt.getState() & JavaThread.STATE_PARKED) != 0) {
				out.print("      parked on: ");
				// java.util.concurrent locks
				if (jt.getBlockingObject() == null) {
					out.print("<unknown>");
				} else {
					JavaObject jo = jt.getBlockingObject();
					String lockID = Long.toHexString(jo.getID().getAddress());
					out.print(jo.getJavaClass().getName() + "@0x" + lockID);
					String ownerThreadName = "<unknown>";
					String ownerThreadID = "<null>";
					out.print(" owner name: ");
					JavaThread lockOwnerThread = Utils.getParkBlockerOwner(jo, ctx.getRuntime());
					if (lockOwnerThread != null) {
						ownerThreadName = lockOwnerThread.getName();
						if (lockOwnerThread.getImageThread() != null) {
							ownerThreadID = lockOwnerThread.getImageThread().getID();
						}
					} else {
						// If the owning thread has ended we won't find the JavaThread
						// We can still get the owning thread name from the java.lang.Thread object itself.
						// We won't get a thread id.
						JavaObject lockOwnerObj = Utils.getParkBlockerOwnerObject(jo, ctx.getRuntime());
						if (lockOwnerObj != null) {
							ownerThreadName = Utils.getThreadNameFromObject(lockOwnerObj, ctx.getRuntime(), out);
						}
					}
					out.print("\"" + ownerThreadName + "\"");
					out.print(" owner id: " + ownerThreadID);
				}
				out.println();
			} else if ((jt.getState() & JavaThread.STATE_IN_OBJECT_WAIT) != 0) {
				out.print("      waiting to be notified on: ");
			} else if ((jt.getState() & JavaThread.STATE_BLOCKED_ON_MONITOR_ENTER) != 0) {
				out.print("      waiting to enter: ");
			}
			if ((jt.getState() & (JavaThread.STATE_IN_OBJECT_WAIT | JavaThread.STATE_BLOCKED_ON_MONITOR_ENTER)) != 0) {
				// java monitors
				MonitorState ms = monitors.get(jt);
				if (ms == null) {
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
					if (ms.getMonitor().getOwner() != null) {
						out.print("\"" + ms.getMonitor().getOwner().getName() + "\"");
						if (ms.getMonitor().getOwner().getImageThread() != null) {
							out.print(" owner id: " + ms.getMonitor().getOwner().getImageThread().getID());
						}
					} else {
						out.print("<unowned>");
					}
					out.println();
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

	private void printJavaStackFrameInfo(JavaThread jt) {
		Iterator<?> itStackFrame = jt.getStackFrames();
		if (!itStackFrame.hasNext()) {
			out.println("<no frames to print>");
			return;
		} else {
			out.println();
		}
		while (itStackFrame.hasNext()) {
			// this iterator can contain JavaStackFrame or CorruptData objects
			Object next = itStackFrame.next();
			JavaStackFrame jsf;
			if (next instanceof CorruptData) {
				out.println("     " + Exceptions.getCorruptDataExceptionString());
				return;
			} else {
				jsf = (JavaStackFrame) next;
			}
			JavaLocation jl;
			try {
				jl = jsf.getLocation();
			} catch (CorruptDataException e) {
				out.println("     " + Exceptions.getCorruptDataExceptionString());
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
			if (!isNative) {
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

			out.println();
			out.print("      objects:");
			Iterator<?> itObjectRefs = jsf.getHeapRoots();
			if (!itObjectRefs.hasNext()) {
				out.print(" <no objects in this frame>");
			}
			while (itObjectRefs.hasNext()) {
				Object nextRef = itObjectRefs.next();
				if (nextRef instanceof CorruptData) {
					out.println(Exceptions.getCorruptDataExceptionString());
					break; // give up on this frame
				} else {
					JavaReference jr = (JavaReference) nextRef;
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
			out.println();
		}
	}

	private String toAdjustedHex(long l) {
		if (_pointerSize > 32) {
			return "0x" + Utils.toFixedWidthHex(l);
		} else if (31 == _pointerSize) {
			return "0x" + Utils.toFixedWidthHex((int) (l & ((1L << 31) - 1)));
		} else {
			return "0x" + Utils.toFixedWidthHex((int) l);
		}
	}

	private void printThreadSummary(ImageProcess ip) {
		// Prints a summary list of native thread IDs
		int count = 0;
		Iterator<?> itThread = ip.getThreads();

		while (itThread.hasNext()) {
			Object next = itThread.next();
			if (next instanceof CorruptData) {
				continue;
			}
			ImageThread it = (ImageThread) next;

			if (count % 8 == 0) {
				out.println();
				if (0 == count) {
					out.println();
					out.println("Native thread IDs for current process:");
				}
				out.print(" ");
			}

			try {
				out.print(Utils.padWithSpaces(it.getID(), 8));
			} catch (CorruptDataException e) {
				out.print(Exceptions.getCorruptDataExceptionString());
			}
			count += 1;
		}
		out.println();
	}

	private void printProperties(ImageThread it) {
		// We may have a lot of properties so print them out in two columns.
		List<String> table = it.getProperties().entrySet().stream()
				.sorted(Comparator.comparing(e -> String.valueOf(e.getKey())))
				.map(e -> String.format("%s=%s", e.getKey(), e.getValue()))
				.collect(Collectors.toCollection(ArrayList::new));

		int count = table.size();

		// Ensure the table has an even number of entries.
		if ((count % 2) != 0) {
			table.add("");
			count += 1;
		}

		// Compute the maximum width of entries in the first column.
		int maxLen = 0;

		for (int i = 0; i < count; i += 2) {
			maxLen = Math.max(table.get(i).length(), maxLen);
		}

		String tableFormatString = "    %-" + maxLen + "s   %s%n";

		for (int i = 0; i < count; i += 2) {
			out.printf(tableFormatString, table.get(i), table.get(i + 1));
		}
	}

	@Override
	public void printDetailedHelp(PrintStream outStream) {
		outStream.printf(""
				+ "Displays information about Java and native threads%n"
				+ "%n"
				+ "Parameters: none, native thread ID, zOS TCB address, \"all\", or \"*\"%n"
				+ "%n"
				+ "If no parameter is supplied, information is printed for the current or failing thread, if any%n"
				+ "If a native thread ID or zOS TCB address is supplied, information is printed for that specific thread%n"
				+ "If \"all\", or \"*\" is specified, information is printed for all threads%n"
				+ "%n"
				+ "The following information is printed for each thread:%n"
				+ " - native thread ID%n"
				+ " - registers%n"
				+ " - native stack sections%n"
				+ " - native stack frames: procedure name and base pointer%n"
				+ " - thread properties%n"
				+ " - associated Java thread (if applicable):%n"
				+ "  - name of Java thread%n"
				+ "  - address of associated java.lang.Thread object%n"
				+ "  - current state according to JVMTI specification%n"
				+ "  - current state relative to java.lang.Thread.State%n"
				+ "  - the Java thread priority%n"
				+ "  - the monitor the thread is waiting to enter or waiting on notify%n"
				+ "  - Java stack frames: base pointer, method, source filename and objects on frame%n"
				+ "%n"
				+ "Note: the \"info proc\" command provides a summary list of native thread IDs%n"
				);
	}

}
