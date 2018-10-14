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
package com.ibm.jvm.dtfjview.commands;

import java.io.PrintStream;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.SortedMap;
import java.util.TreeMap;
import java.util.Vector;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.java.JavaMonitor;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.java.JavaThread;
import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;
import com.ibm.jvm.dtfjview.commands.helpers.Exceptions;
import com.ibm.jvm.dtfjview.commands.helpers.JUCMonitorNode;
import com.ibm.jvm.dtfjview.commands.helpers.MonitorNode;
import com.ibm.jvm.dtfjview.commands.helpers.NodeList;
import com.ibm.jvm.dtfjview.commands.helpers.Utils;

@DTFJPlugin(version="1.*",runtime=false)
public class DeadlockCommand extends BaseJdmpviewCommand {
	{
		addCommand("deadlock", "", "displays information about deadlocks if there are any");	
	}
	
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		if(args.length != 0) {
			out.println("The deadlock command does not take any parameters");
			return;
		}
		doCommand();
	}
	
	public void doCommand()
	{
		SortedMap monitorNodes = new TreeMap();
		JavaRuntime jr = ctx.getRuntime();
		Iterator itMonitor = jr.getMonitors();
		int nodeListNum = 0;
		
		out.print("\n  deadlocks for runtime \n");
		
		// Step 1. iterate over all monitors, creating a MonitorNode for each monitor that 
		// contains the monitor (JavaMonitor) and some parameters and adding that MonitorNode 
		// to a Hashtable, indexed by owner (JavaThread object address)
		// Note: defect 133638, this code used to use the ImageThread address as index, but
		// JVM dumps on Linux don't have all the image threads, so we now use JavaThreads
		while (itMonitor.hasNext()) {
			JavaMonitor monitor = (JavaMonitor)itMonitor.next();
			MonitorNode node = new MonitorNode(monitor);
			JavaThread owner = null;
			Long id = null;
			
			try {
				owner = monitor.getOwner();
			} catch (CorruptDataException e) {
				out.println("exception encountered while getting monitor owner: " +
						Exceptions.getCorruptDataExceptionString());
				return;
			}

			if (null == owner) {
				// A monitor with no owner cannot be involved in a deadlock, according to the 
				// algorithm used here anyway, because in order for a monitor to be in a deadlock,
				// its owner must be in a deadlock or an owner somewhere down the chain of 
				// ownership must own the given monitor. Since there is no owner, we can't get 
				// the monitor's owner or the next monitor in a potential deadlock chain.
				continue;
			} else {
				JavaObject threadObject;
				try {
					threadObject = owner.getObject();
				} catch (CorruptDataException e) {
					out.println("exception encountered while getting owner's JavaObject: " +
							Exceptions.getCorruptDataExceptionString());
					return;
				}
				id = Long.valueOf(threadObject.getID().getAddress());
			}
			
			// Note: defect 133638, we used to give up here with an error if there was already
			// a monitor node in the table with the same key (thread). This is very common (a
			// thread owning multiple monitors). Longer term the intention is to replace this
			// algorithm with the one used in javacore, but for now we carry on to see if we can
			// still find a deadlock, with some node(s) discarded.
			monitorNodes.put(id, node);
		}
		
		// Step 1.b
		// Add the JUC locks, technically to find all of them you need to walk the whole
		// heap. But the active ones can be found by walking the thread list and looking
		// at the blocking objects. (Any others aren't blocking any threads anyway so aren't
		// interesting.
		Iterator itThread = jr.getThreads();
		while (itThread.hasNext()) {
			try {
				Object o = itThread.next();
				if( !(o instanceof JavaThread) ) {
					continue;
				}
				JavaThread jt = (JavaThread)o;
				JavaThread owner = null;
				Long id = null;
				if( (jt.getState() & JavaThread.STATE_PARKED) != 0 ) {
					JavaObject lock = jt.getBlockingObject();
					MonitorNode node = new JUCMonitorNode(lock, jr);
					try {
						owner = Utils.getParkBlockerOwner(lock, jr);
						if( owner == null ) {
							continue;
						}
					} catch (MemoryAccessException e) {
						out.println("exception encountered while getting monitor owner: " +
								Exceptions.getCorruptDataExceptionString());
						return;
					}
					JavaObject threadObject;
					try {
						threadObject = owner.getObject();
					} catch (CorruptDataException e) {
						out.println("exception encountered while getting owner's JavaObject: " +
								Exceptions.getCorruptDataExceptionString());
						return;
					}
					id = Long.valueOf(threadObject.getID().getAddress());
					monitorNodes.put(id, node);
				}
			} catch(CorruptDataException cde) {
				out.println("\nwarning, corrupt data encountered during scan for java.util.concurrent locks...");
			} catch(DataUnavailable du) {
				out.println("\nwarning, data unavailable encountered during scan for java.util.concurrent locks...");
			}
		}

		Iterator values = monitorNodes.values().iterator();
		
		// Step 2. iterate over Hashtable and for every MonitorNode, iterate over monitor m1's
		// enter waiters (JavaMonitor.getEnterWaiters()), which are JavaThreads, and for each 
		// enter waiter, set that waiter's MonitorNode's waitingOn to m1.
		while (values.hasNext()) {
			MonitorNode currNode = (MonitorNode)values.next();
			
			Iterator itWaiters = currNode.getEnterWaiters();
			while (itWaiters.hasNext()) {
				Object o = itWaiters.next();
				if( !(o instanceof JavaThread) ) {
					continue;
				}
				JavaThread waiter = (JavaThread)o;
				JavaObject threadObject;
				Long id = null;
				
				try {
					threadObject = waiter.getObject();
				} catch (CorruptDataException e) {
					out.println("exception encountered while getting waiter's ImageThread: " +
							Exceptions.getCorruptDataExceptionString());
					return;
				} 
				
				id = Long.valueOf(threadObject.getID().getAddress());					
				
				MonitorNode waiterNode = (MonitorNode)monitorNodes.get(id);
				if (null != waiterNode) {
					waiterNode.waitingOn = currNode;
				}
			}
		}

		values = monitorNodes.values().iterator();
		int visit = 1;
		Vector lists = new Vector();
		
		// Step 3. iterate over Hashtable and for every MonitorNode m1:
		// Step 3a. set a unique visit number, visit > 0 (visit++ would work)
		// Step 3b. iterate over waitingOns, setting visit number, until a null
		//  waitingOn is reached (no deadlock), a deadlocked node is reached
		//  (m1 is part of a branch that joins a deadlock loop), or a node with
		//  the same visit is reached (m1 is part of a deadlock loop)
		// Step 3c. if deadlock found, start at m1 and iterate over all
		//  waitingOns until a deadlocked node is found, setting deadlock
		//  (branch or loop, depending on result in 3b) along the way

		// note: Step 4* are not laid out precisely as specified; the instructions
		//  listed for Step 4* are integrated into Step 3
		
		// Step 4. for each MonitorNode m1 where inList == false and m1 is part
		//  of a deadlock loop *, create a new list and push it on the list
		//  stack right away:
		// Step 4a. iterate over all enter waiters, setting inList to true at
		//  every MonitorNode
		// Step 4b. if there are no enter waiters, terminate the current list
		// Step 4c. if there is one enter waiter: if the enter waiter is the
		//  current list, stop; else continue creating the current list,
		//  iterating to that thread's MonitorNode
		// Step 4d. if there is more than one enter waiter, continue creating
		//  current list and start a new list, pushing it on the list stack
		//  right away
		
		while (values.hasNext())
		{
			MonitorNode startNode = (MonitorNode)values.next();
			MonitorNode currNode = startNode;
			MonitorNode endNode;
			
			if (0 != startNode.visit)
			{
				continue;
			}
			
			while (true)
			{
				currNode.visit = visit;
				
				if (null == currNode.waitingOn)
				{
					currNode.deadlock = MonitorNode.NO_DEADLOCK;
					break;
				}
				
				if (isDeadlocked(currNode.waitingOn))
				{
					// we've encountered a deadlocked node in the chain;
					//  set branch deadlock for all nodes between startNode
					//  and currNode
					
					endNode = currNode.waitingOn;
					currNode = startNode;
					NodeList branchList = null;
					while (currNode != endNode)
					{
						if (null == branchList)
						{
							branchList = new NodeList(currNode, nodeListNum++);
						}
						currNode.deadlock = MonitorNode.BRANCH_DEADLOCK;
						currNode = currNode.waitingOn;
						branchList.add(currNode);
						if (currNode != endNode)
							currNode.inList = branchList;
					}
					
					if (endNode.inList.isLoop())
					{
						lists.insertElementAt(branchList, lists.indexOf(endNode.inList));
					}
					else
					{
						NodeList oldList = endNode.inList;
						
						// FIXME: the below line will cause problems with at least
						//  one case that was not considered when attachOrSplit was
						//  coded: if a NodeList n1 has already been split and another
						//  NodeList n2 tries to attach to the end of n1, then n1 will
						//  allow n2 to attach to n1, while what n1 should really do is
						//  just return n2 and not allow n2 to attach to itself
						NodeList split = endNode.inList.attachOrSplit(branchList, nodeListNum++);
						if (null != split)
						{
							lists.insertElementAt(split, lists.indexOf(oldList));
							lists.insertElementAt(branchList, lists.indexOf(oldList));
						}
					}
					break;
				}
				
				if (currNode.waitingOn.visit == visit)
				{
					// we've encountered a node in the same visit as the current
					//  visit, ie. we've found a loop; first flag the whole loop
					//  with a loop deadlock flag, then flag the rest of the nodes
					//  in the chain with a branch deadlock
					
					endNode = currNode.waitingOn;
					currNode = endNode;
					NodeList loopList = new NodeList(currNode, nodeListNum++);
					lists.insertElementAt(loopList, 0);
					
					do
					{
						currNode.deadlock = MonitorNode.LOOP_DEADLOCK;
						currNode = currNode.waitingOn;
						loopList.add(currNode);
						currNode.inList = loopList;
					} while (currNode != endNode);
					
					currNode = startNode;
					NodeList branchList = null;
					while (currNode != endNode)
					{
						if (null == branchList)
						{
							branchList = new NodeList(currNode, nodeListNum++);
							lists.insertElementAt(branchList, 0);
						}
						currNode.deadlock = MonitorNode.BRANCH_DEADLOCK;
						currNode = currNode.waitingOn;
						branchList.add(currNode);
						if (currNode != endNode)
							currNode.inList = branchList;
					}
					break;
				}
				
				currNode = currNode.waitingOn;
			}
			
			visit++;
		}

		if (lists.isEmpty())
		{
			out.print("\n");
			out.print("\t no deadlocks detected");
			out.print("\n");
			return;
		}
		
		boolean lastListWasLoop = true;
		Iterator itList = lists.iterator();

		// Step 5. print the lists
		while (itList.hasNext())
		{
			NodeList list = (NodeList)itList.next();
			
			if (list.isLoop()) {
				out.print("\n    deadlock loop:\n");
				lastListWasLoop = true;
			} else if (lastListWasLoop)	{ // && !list.isLoop()
				out.print("\n\n    deadlock branch(es):\n");
				lastListWasLoop = false;
			}
			
			out.print("\t  " + list.toString());
			out.print("\n");
		}
		out.print("\n");

	}
	
	private boolean isDeadlocked(MonitorNode node)
	{
		return MonitorNode.LOOP_DEADLOCK == node.deadlock ||
			MonitorNode.BRANCH_DEADLOCK == node.deadlock;
	}

	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println("displays information about deadlocks if there are any\n\n" +
				"parameters: none\n\n" +
				"The \"deadlock\" command shows detailed information about deadlocks " +
				"or \"no deadlocks detected\" if there are no deadlocks.  A deadlock " +
				"situation consists of one or more deadlock loops and zero or more " +
				"branches attached to those loops.  This command prints out each " +
				"branch attached to a loop and then the loop itself.  If there is a " +
				"split in a deadlock branch, separate branches will be created for " +
				"each side of the split in the branch.  Deadlock branches start with " +
				"a monitor that has no threads waiting on it and the continues until " +
				"it reaches a monitor that exists in another deadlock branch or loop.  " +
				"Deadlock loops start and end with the same monitor.\n\n" +
				"Monitors are represented by their owner and the object associated with " +
				"the given monitor.  For example, \"3435 (0x45ae67)\" represents the " +
				"monitor that is owned by the thread with id 3435 and is associated " +
				"the object at address 0x45ae67.  Objects can be viewed by using a " +
				"command like \"x/j 0x45ae67\" and threads can be viewed using a " +
				"command like \"info thread 3435\".\n"
		);
	}
}
