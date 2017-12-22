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
package com.ibm.dtfj.corereaders;

import java.util.Iterator;
import java.util.Vector;

/**
 * This class represents a single "generic" thread within the dump and is basically a 
 * data holding class (together with a toString() that allows a nice view of the 
 * class ..... 
 *  - what attributes does a "generic thread" have as against extenders representing
 * a specific type of thread (viz a J9 thread)
 *  J9Thread extends this Generic thread. System threads are generic threads and 
 *  do not necessarily have same detail as java threads 
 */ 
public class GenericThread {
	protected String threadId;
	protected String javaLangThreadObjectAddress;
	 
	protected String threadDetails;
	protected String threadName;
	protected String state;
	protected String monitorId;
	boolean isJavaThread;
	long stackstart = 0; // used by system threads
	int stacksize = 0;	// used by system threads
	int rva = 0;		// used by system threads
	Vector registers = new Vector();
	Vector nativeFrames = new Vector();
	protected NativeThreadContext context = null;
	
	
	public GenericThread(GenericThread thread) {
		threadId = thread.getThreadId();
		javaLangThreadObjectAddress = thread.javaLangThreadObjectAddress();
		state = thread.getState();
		monitorId = thread.getMonitorId();
		isJavaThread = true; // only java threads use this constructor
		 
	}
	
	public GenericThread(String threadId, long stackstart, int stacksize,int rva) {
		this.threadId = threadId; 
		this.stackstart = stackstart;
		this.stacksize = stacksize;
		this.rva = rva;
		this.state = "Unknown";
		isJavaThread = false; // Only system threads use this constructor 
	}	
	
	public GenericThread(String id, String obj,String state,String monitor) {
		threadId = id;
		javaLangThreadObjectAddress = obj;
		this.state = state;
		monitorId = monitor;
		isJavaThread = true; // only java threads use this constructor
	}

	/**
	 * @return
	 */
	public   String javaLangThreadObjectAddress() {
		// we assume they want details - so will fill in the details at this point!
		 
		return javaLangThreadObjectAddress;
	}

	/**
	 * @return
	 */
	public   String getThreadId() {
		return threadId;
	}
	
	

	/**
	 * @return
	 */
	public String getMonitorId() {
		return monitorId;
	}

	/**
	 * @return
	 */
	public String getState() {
		return state;
	}

	/**
	 * @return
	 */
	public String getThreadName() {
		if (null == threadName) return " Un-established";
		return threadName;
	}

	/**
	 * @return
	 */	
	public boolean isJavaThread() {
		return false; 
		
	}
	
	/**
	 * @return
	 */	
	public void addRegister(Register r) {
		 registers.add(r); 
		
	}
	
	public void addNativeFrame(StackFrame f) {
		nativeFrames.add(f);
	}
	
	public Register getNamedRegister(String name) {
		// allow uppercase or lower case in the name
		 
		
		String name1 = name.toUpperCase();
		Iterator it = getRegisters();
		while (it.hasNext()) {
			 Register r = (Register)it.next();
			 if (r.name.toUpperCase().equals(name1)) {
			 	return r;
			 }
	
		}

		
		return null;
		
	}
	
	public Iterator getRegisters() {
		return registers.iterator();
	}
	
	public Iterator getNativeFrames() {
		return nativeFrames.iterator();
	}

	/**
	 * @return Returns the stacksize.
	 */
	public int getStacksize() {
		return stacksize;
	}
	
	/**
	 * @return Returns the stackstart.
	 */
	public long getStackstart() {
		return stackstart;
	}
	
	public String toString() {
		
		StringBuffer  sb = new StringBuffer();
		
		printHeader(sb);
 
		 
		sb.append(" System thread\n");
		sb.append("  Id      : "+threadId + "\n");
 		
		sb.append("  State   : "+ this.state);
		
		printNativeFrames(sb);
		
		sb.append("\n");
		return sb.toString();
		
	}

	public void printHeader(StringBuffer sb) {
		sb.append("Info for system thread - "+ threadId + "\n====================================\n");
	}

	public void printNativeFrames(StringBuffer sb) {
		if (0 != nativeFrames.size()) {
			sb.append("\n\tStack:\n");
			for (int i=0; i<nativeFrames.size();i++ ) {
				StackFrame frameInfo = (StackFrame) nativeFrames.get(i);
				if (null != frameInfo) {
					sb.append("\t\t" + frameInfo.toString() + "\n");
				}
			}
		} else sb.append("\n\t No Stack available");
	}
	
	/**
	 * @return Returns the context.
	 */
	public NativeThreadContext getContext() {
		return context;
	}
	/**
	 * @param context The context to set.
	 */
	public void setContext(NativeThreadContext context) {
		this.context = context;
	}
	/**
	 * @return Returns the javaLangThreadObjectAddress.
	 */
	public String getJavaLangThreadObjectAddress() {
		return javaLangThreadObjectAddress;
	}
	/**
	 * @param stacksize The stacksize to set.
	 */
	public void setStacksize(int stacksize) {
		this.stacksize = stacksize;
	}
	/**
	 * @param stackstart The stackstart to set.
	 */
	public void setStackstart(long stackstart) {
		this.stackstart = stackstart;
	}

	public boolean matchIdOrName(String key) {
		if ("*".equals(key)) {
			return true;
		} else {
			String tid = getThreadId();
			String name = getThreadName();
			
			if (tid.equals(key) ||
				tid.toUpperCase().equals(key.toUpperCase()) ||
				tid.equals("0x"+key) ||
				tid.toUpperCase().equals(("0x"+key).toUpperCase())) {
				
				return true;
			}
			
			if (null != name) {
				if (name.equals(key) || 
				name.toUpperCase().equals(key.toUpperCase()) ||
				name.startsWith(key) ||
				name.toUpperCase().startsWith(key.toUpperCase())) {
					return true;
				}
			}
			
			String deriveThread = new String(key);
			
			if (deriveThread.toUpperCase().startsWith("0x")) {
				deriveThread = deriveThread.substring(2);
			}
			while (deriveThread.startsWith("0")) {
				deriveThread = deriveThread.substring(1);
			}
			deriveThread = "0x" + deriveThread;
			deriveThread = deriveThread.toUpperCase();
			if (tid.toUpperCase().equals(deriveThread)) {
				return true;
			}
			
		}
		return false;
	}
}
