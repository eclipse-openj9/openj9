/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Vector;
import java.util.logging.Level;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageThread;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaField;
import com.ibm.dtfj.java.JavaMonitor;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.java.JavaThread;
import com.ibm.dtfj.runtime.ManagedRuntime;
import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;
import com.ibm.jvm.dtfjview.commands.BaseJdmpviewCommand;
import com.ibm.jvm.dtfjview.commands.helpers.Exceptions;
import com.ibm.jvm.dtfjview.commands.helpers.Utils;

@DTFJPlugin(version="1.*",runtime=false)
public class InfoLockCommand extends BaseJdmpviewCommand{
	{
		addCommand("info lock", "", "outputs a list of system monitors and locked objects");	
	}
	
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		if(args.length != 0) {
			out.println("\"info lock\" command does not take any parameters");
			return;
		}
		showSystemLocks();
		showJavaUtilConcurrentLocks();
	}
	
	private void showSystemLocks() {
		Vector vMonitorsWithLockedObjects = new Vector();
		JavaRuntime jRuntime = ctx.getRuntime();
		Iterator monitors = jRuntime.getMonitors();
		
		out.println("\nSystem locks...");
		while (monitors.hasNext()){
			JavaMonitor jMonitor = (JavaMonitor)monitors.next();
			JavaObject jObject = jMonitor.getObject();
			try {
				String monitorName = jMonitor.getName().trim();
				if (monitorName.equalsIgnoreCase("")) {
					monitorName = "<un-named monitor>";
				}
				out.println("id: 0x" + jMonitor.getID() + " name: " + jMonitor.getName());

				JavaThread owner = jMonitor.getOwner();
				if (null != owner) {
					try {
						out.println("\towner thread id: " + owner.getImageThread().getID()
								+ " name: " + owner.getName());
					} catch (DataUnavailable e) {
						out.println("\towner thread id: " + Exceptions.getDataUnavailableString());
						logger.log(Level.FINE, Exceptions.getDataUnavailableString(), e);
					}
				}
				
				showWaiters(jMonitor);
				
			} catch(CorruptDataException cde) {
				out.println("\nwarning, corrupt data encountered during scan for system locks...");
			}
			if (null != jObject) {
				// Remember object monitors (flat or inflated) for later
				vMonitorsWithLockedObjects.add(jMonitor);
			}
		}
		showLockedObjects(vMonitorsWithLockedObjects);
	}

	private void showWaiters(JavaMonitor jMonitor)
			throws CorruptDataException {
		// List any threads waiting on enter or notify for this monitor
		Iterator itEnterWaiter = jMonitor.getEnterWaiters();
		while (itEnterWaiter.hasNext()) {
			Object t = itEnterWaiter.next();
			if( !(t instanceof JavaThread) ) {
				continue;
			}
			JavaThread jThread = (JavaThread)t;
			try {
				out.println("\twaiting thread id: " + jThread.getImageThread().getID() 
						+ " name: " + jThread.getName());
			} catch (DataUnavailable dae) {
				out.println("\twaiting thread id: <unknown>");
				logger.log(Level.FINE, Exceptions.getDataUnavailableString(), dae);
			}
		}
		Iterator itNotifyWaiter = jMonitor.getNotifyWaiters();
		while (itNotifyWaiter.hasNext()) {
			Object t = itNotifyWaiter.next();
			if( !(t instanceof JavaThread) ) {
				continue;
			}
			JavaThread jThread = (JavaThread)t;
			try {
				out.println("\twaiting thread id: " + jThread.getImageThread().getID() 
						+ " name: " + jThread.getName());
			} catch (DataUnavailable dae) {
				out.println("\twaiting thread id: <unknown>");
				logger.log(Level.FINE, Exceptions.getDataUnavailableString(), dae);
			}
		}
	}
	
	private void showLockedObjects(Vector vMonitorsWithLockedObjects) {
		out.println("\nObject Locks in use...");
		if (0 == vMonitorsWithLockedObjects.size()){
			out.println("\t...None.");
			return;
		}
		Iterator lockedObjects = vMonitorsWithLockedObjects.iterator();
		while(lockedObjects.hasNext()){
			JavaMonitor jMonitor = (JavaMonitor)lockedObjects.next();
			JavaObject jObject = jMonitor.getObject();
			try{
				JavaThread owner = jMonitor.getOwner();
				String className = "<unknown class>";
				JavaClass jClass = jObject.getJavaClass();	
				if (null != jClass) {
				    className = jClass.getName();
				}
				String jObjectID = Long.toHexString(jObject.getID().getAddress());
				if (null == owner) {
					out.println(className + "@0x" + jObjectID );
				} else {
					String owningThreadID = null;
					try {
						owningThreadID = owner.getImageThread().getID();
					} catch (DataUnavailable e) {
						owningThreadID = Exceptions.getDataUnavailableString();
					}
					out.println(className + "@0x" + jObjectID );
					out.println("\towner thread id: " + owningThreadID
							+ " name: " + owner.getName());
				}
				showWaiters(jMonitor);
			}catch(CorruptDataException cde){
				logger.log(Level.FINE, Exceptions.getCorruptDataExceptionString(), cde);
			}
		}
	}
	
	private void showJavaUtilConcurrentLocks() {
		// A map of lock objects and their waiting threads.
		Map locksToThreads = new HashMap();
		JavaRuntime jr = ctx.getRuntime();
		Iterator itThread = jr.getThreads();
		while (itThread.hasNext()) {
			try {
				Object o = itThread.next();
				if( !(o instanceof JavaThread) ) {
					continue;
				}
				JavaThread jt = (JavaThread)o;
				if( (jt.getState() & JavaThread.STATE_PARKED) != 0 ) {
					JavaObject lock = jt.getBlockingObject();
					if( lock != null ) {
						List parkedList = (List)locksToThreads.get(lock);
						if( parkedList == null ) {
							parkedList = new LinkedList();
							locksToThreads.put(lock, parkedList);
						}
						parkedList.add(jt);
					}
				}
			} catch(CorruptDataException cde) {
				out.println("\nwarning, corrupt data encountered during scan for java.util.concurrent locks...");
				logger.log(Level.FINE, Exceptions.getCorruptDataExceptionString(), cde);
			} catch(DataUnavailable du) {
				out.println("\nwarning, data unavailable encountered during scan for java.util.concurrent locks...");
				logger.log(Level.FINE, Exceptions.getDataUnavailableString(), du);
			}
		}
		out.println("\njava.util.concurrent locks in use...");
		if( locksToThreads.size() == 0 ) {
			out.println("\t...None.");
			out.println();
		}
		for( Object e: locksToThreads.entrySet() ) {
			try {
				Map.Entry entry = (Map.Entry)e;
				JavaObject lock = (JavaObject)entry.getKey();
				List threads = (List)entry.getValue();
				String threadName = "<unowned>";
				JavaThread lockOwnerThread = Utils.getParkBlockerOwner(lock, ctx.getRuntime());
				if( lockOwnerThread != null ) {
					threadName = lockOwnerThread.getName();
				} else {
					// If the owning thread has ended we won't find the JavaThread
					// We can still get the owning thread name from the java.lang.Thread object itself.
					JavaObject lockOwnerObj = Utils.getParkBlockerOwnerObject(lock, ctx.getRuntime());
					if( lockOwnerObj != null ) {
						threadName = Utils.getThreadNameFromObject(lockOwnerObj, ctx.getRuntime(), out);
					} else {
						threadName = "<unknown>";
					}
				}
				if( threads != null && threads.size() > 0) {
					String lockID = Long.toHexString(lock.getID().getAddress());
					
					ImageThread imageThread = (lockOwnerThread!=null?lockOwnerThread.getImageThread():null);
					
					out.println(lock.getJavaClass().getName() + "@0x" + lockID + "\n\tlocked by java thread id: "
							+ ((imageThread!=null)?imageThread.getID():"<null>") + " name: " + (threadName));
					
					for( Object t: threads) {
						JavaThread waiter = (JavaThread) t;
						out.println("\twaiting thread id: " + waiter.getImageThread().getID() 
								+ " name: " + waiter.getName());
					}
				}
			} catch(CorruptDataException cde) {
				out.println("\nwarning, corrupt data encountered during scan for java.util.concurrent locks...");
				logger.log(Level.FINE, Exceptions.getCorruptDataExceptionString(), cde);
			} catch(MemoryAccessException ma) {
				out.println("\nwarning, memory access error encountered during scan for java.util.concurrent locks...");
				logger.log(Level.FINE, Exceptions.getMemoryAccessExceptionString(), ma);
			} catch(DataUnavailable du) {
				out.println("\nwarning, data unavailable encountered during scan for java.util.concurrent locks...");
				logger.log(Level.FINE, Exceptions.getDataUnavailableString(), du);
			}
		}
	}
	
	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println("outputs a list of system monitors and locked objects\n\n" +
				"parameters: none\n\n" +
				"The info lock command outputs a list of system monitors and locked objects.");
		
	}
}
