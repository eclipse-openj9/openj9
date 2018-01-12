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
package com.ibm.jvm.dtfjview.commands.helpers;

import com.ibm.dtfj.java.JavaThread;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImageThread;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.MemoryAccessException;


public class NodeList {

	private MonitorNode head;
	private MonitorNode tail;
	private int id;
	
	public NodeList(MonitorNode node, int _id)
	{
		head = node;
		tail = node;
		node.inList = this;
		id = _id;
	}
	
	public int getID()
	{
		return id;
	}
	
	public boolean equals(Object other)
	{
		return ((NodeList)other).getID() == id;
	}

	// returns null if attached; a NodeList representing the part split off from
	//  this NodeList if a split was required
	public NodeList attachOrSplit(NodeList other, int nodeListNum)
	{
		MonitorNode headToAttach = other.getHead();

		if (tail == headToAttach)
		{
			tail = other.getTail();
			MonitorNode currNode = tail;
			currNode.inList = this;
			if (currNode != headToAttach) {
				do {
					currNode = currNode.waitingOn;
					currNode.inList = this;
				} while (currNode != headToAttach);
			}
			return null;
		}
		else
		{
			MonitorNode currNode = tail;
			NodeList split = new NodeList(tail, nodeListNum);
			do
			{
				if (currNode == headToAttach)
				{
					tail = currNode;
					return split;
				}
				currNode = currNode.waitingOn;
				split.add(currNode);
				if (currNode != head)
					currNode.inList = split;
			} while (currNode != head);
			return null;
		}
	}
	
	public void add(MonitorNode node)
	{
		head = node;
	}
	
	public MonitorNode getHead()
	{
		return head;
	}
	
	public MonitorNode getTail()
	{
		return tail;
	}
	
	public boolean isLoop()
	{
		return tail == head;
	}
	
	// Print this list of monitor nodes (ie the owning threads and there object addresses)
	public String toString()
	{
		String retval = "";
		MonitorNode currNode = tail;
		MonitorNode lastNode = null;
		boolean firstTime = true;
		boolean done = false;
		
		do {
			String name = "";
			String objAddr = "";
			JavaObject object = null;
			JavaThread owner = null;
			
			try {
				owner = currNode.getOwner();
				name = owner.getName();
				ImageThread nativeThread = owner.getImageThread();
				if (nativeThread != null) {
					name = name + " id: " + nativeThread.getID();
				}
			} catch (CorruptDataException e) {
				name = Exceptions.getCorruptDataExceptionString();
			} catch (DataUnavailable e) {
				name = Exceptions.getDataUnavailableString();
			} catch (MemoryAccessException e) {
				name = Exceptions.getMemoryAccessExceptionString();
			}
			
			object = currNode.getObject();
			if (null == object) {
				objAddr = " at : " + Utils.toHex(currNode.getMonitorAddress());
			} else {
				objAddr = " object : " + Utils.toHex(object.getID().getAddress());
			}
			
			String lockName = currNode.getType();
						
			retval += "thread: " + name + " (owns " + lockName + objAddr + ") waiting for =>\n\t  ";
			
			lastNode = currNode;
			currNode = currNode.waitingOn;

			if (head == lastNode)
			{
				if (firstTime && head == tail)
					done = false;
				else
					done = true;
			}
			firstTime = false;
			
		} while (!done);
		
		retval = retval.substring(0, retval.length() - 18); // removes the tail of the last entry
		return retval;
	}
}
