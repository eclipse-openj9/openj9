/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2017 IBM Corp. and others
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
package com.ibm.dtfj.image.javacore;

import java.util.Iterator;
import java.util.Properties;
import java.util.Vector;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageModule;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.image.ImageThread;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.java.javacore.JCInvalidArgumentsException;
import com.ibm.dtfj.javacore.builder.IBuilderData;

public class JCImageProcess implements ImageProcess {
	
	private Vector fRuntimes;
	private Vector fImageThreads;
	private Vector fLibraries;
	private Properties fProperties;
	
	private String fCommandLine;
	private String fID;
	private int fPointerSize;
	private ImageModule fExecutable;
	private int fSignal;
	private long fCurrentThreadID = IBuilderData.NOT_AVAILABLE;

	private final JCImageAddressSpace fImageAddressSpace;
	
	public JCImageProcess(JCImageAddressSpace imageAddressSpace) throws JCInvalidArgumentsException {
		if (imageAddressSpace == null) {
			throw new JCInvalidArgumentsException("An image process must pertain to an image address space");
		}
		fImageAddressSpace = imageAddressSpace;
		fRuntimes = new Vector();
		fImageThreads = new Vector();
		fLibraries = new Vector();
		fProperties = new Properties();

		fCommandLine = null;
		fID = null;
		fPointerSize = IBuilderData.NOT_AVAILABLE;
		fExecutable = null;
		fSignal = IBuilderData.NOT_AVAILABLE;
		imageAddressSpace.addImageProcess(this);
	}

	
	
	/**
	 * 
	 */
	public String getCommandLine() throws DataUnavailable, CorruptDataException {
		if (fCommandLine == null) {
			throw new DataUnavailable("No command line available");
		}
		return fCommandLine;
	}

	/**
	 * 
	 */
	public ImageThread getCurrentThread() throws CorruptDataException {
		ImageThread currentThread = null;
		if (fCurrentThreadID != IBuilderData.NOT_AVAILABLE) {
			// Return the indicated thread
			ImagePointer ip = fImageAddressSpace.getPointer(fCurrentThreadID);
			currentThread = getImageThread(ip);
			if (currentThread == null) {
				throw new CorruptDataException(new JCCorruptData("Bad native thread ID", ip));
			}
		}
		return currentThread;
	}
	
	/**
	 * 
	 * NON-DTFJ
	 * Overwrites whatever was there if a key already exists.
	 * @param key
	 * @param value
	 */
	public void addEnvironment(Object key, Object value) {
		fProperties.put(key, value);
	}

	
	/**
	 * 
	 */
	public Properties getEnvironment() throws DataUnavailable, CorruptDataException {
		if (fProperties == null || fProperties.size() == 0) {
			throw new DataUnavailable("Environment data not available");
		}
		return fProperties;
	}

	
	/**
	 * 
	 */
	public ImageModule getExecutable() throws DataUnavailable, CorruptDataException {
		if (fExecutable == null) {
			throw new DataUnavailable("No executable data");
		}
		return fExecutable;
	}

	
	/**
	 * 
	 */
	public String getID() throws DataUnavailable, CorruptDataException {
		if (fID == null) {
			throw new DataUnavailable("No process ID available");
		}
		return fID;
	}
	
	
	/**
	 * 
	 */
	public Iterator getLibraries() throws DataUnavailable, CorruptDataException {
		if (fLibraries.size() == 0) {
			throw new DataUnavailable("No library information available");
		}
		return fLibraries.iterator();
	}

	/**
	 * NON-DTFJ
	 * @param module
	 */
	public void addLibrary(JCImageModule module)  {
		if (module != null) {
			fLibraries.add(module);
		}
	}
	
	/**
	 * NON-DTFJ
	 * @param name
	 * 
	 */
	public ImageModule getLibrary(String name) {
		JCImageModule module = null;
		if (name != null) {
			Iterator it = fLibraries.iterator();
			while (module == null && it.hasNext()) {
				JCImageModule foundModule = (JCImageModule) it.next();
				if (foundModule != null) {
					String foundID = foundModule.getInternalName();
					if (foundID != null && foundID.equals(name)) {
						module = foundModule;
					}
				}
			}
		}
		return module;
	}
	
	
	
	/**
	 */
	public int getPointerSize() {
		return fPointerSize;
	}
	
	/**
	 * 
	 * @param pointerSize
	 */
	public void setPointerSize(int pointerSize) {
		fPointerSize = pointerSize;
	}

	/**
	 * Non-DTFJ. Sets signal number found in javacore
	 * @param signal
	 */
	public void setSignal(int signal) {
		fSignal = signal;
	}
	
	/** 
	 * Non-DTFJ. Sets command line found in javacore
	 */
	public void setCommandLine(String commandLine) {
		fCommandLine = commandLine;
	}
	
	/**
	 * 
	 */
	public Iterator getRuntimes() {
		return fRuntimes.iterator();
	}
	
	
	
	/**
	 * NON-DTFJ
	 * @param javaRuntime
	 */
	public void addRuntime(JavaRuntime javaRuntime) {
		if (javaRuntime != null) {
			fRuntimes.add(javaRuntime);
		}
	}

	// Mapping from signal number to signal name. The same mapping is also used in class 
	// com.ibm.dtfj.image.j9.ImageProcess in the DTFJ J9 project. This is a candidate
	// for moving to a DTFJ 'Common' project at some point.
	private static String[] names = {
		"ZERO",
		"SIGHUP",		// 1
		"SIGINT",		// 2 + win
		"SIGQUIT",		// 3
		"SIGILL",		// 4 + win
		"SIGTRAP",		// 5
		"SIGABRT",		// 6
		"SIGEMT",		// 7
		"SIGFPE",		// 8 + win
		"SIGKILL",		// 9
		"SIGBUS",		// 10
		"SIGSEGV",		// 11 + win
		"SIGSYS",		// 12
		"SIGPIPE",		// 13
		"SIGALRM",		// 14
		"SIGTERM",		// 15 + win
		"SIGUSR1",		// 16
		"SIGUSR2",		// 17
		"SIGCHLD",		// 18
		"SIGPWR",		// 19
		"SIGWINCH",		// 20
		"SIGURG/BREAK",	// 21 / win
		"SIGPOLL/ABRT",	// 22 / win
		"SIGSTOP",		// 23
		"SIGTSTP",		// 24
		"SIGCONT",		// 25
		"SIGTTIN",		// 26
		"SIGTTOU",		// 27
		"SIGVTALRM",	// 28
		"SIGPROF",		// 29
		"SIGXCPU",		// 30
		"SIGXFSZ",		// 31
		"SIGWAITING",	// 32
		"SIGLWP",		// 33
		"SIGAIO",		// 34
		"SIGFPE_DIV_BY_ZERO",		// 35 - synthetic from generic signal
		"SIGFPE_INT_DIV_BY_ZERO",	// 36 - synthetic from generic signal
		"SIGFPE_INT_OVERFLOW"		// 37 - synthetic from generic signal
	};
	private String resolvePlatformName(int num) {
		if (num >= 0 && num < names.length) return names[num];
		else return "Signal."+Integer.toString(num);
	}
	/**
	 * Get signal name (if signal was available in javacore)
	 */
	public String getSignalName() throws DataUnavailable, CorruptDataException {
		if (fSignal == IBuilderData.NOT_AVAILABLE ) {
			throw new DataUnavailable("No signal name available");
		}
		if (fSignal >= 0 && fSignal < names.length) {
			return names[fSignal];
		} else {
			// unknown signal, use same naming convention as core-file DTFJ
			return "Signal."+Integer.toString(fSignal);
		}
	}

	
	/**
	 * Get signal number (if signal was available in javacore)
	 */
	public int getSignalNumber() throws DataUnavailable, CorruptDataException {
		if (fSignal == IBuilderData.NOT_AVAILABLE ) {
			throw new DataUnavailable("No signal number available");
		} else {
			return fSignal;
		}
	}

	
	/**
	 * 
	 */
	public Iterator getThreads() {
		return fImageThreads.iterator();
	}
	
	
	/**
	 * NOT in DTFJ
	 * @param thread
	 */
	public void addImageThread(JCImageThread thread) {
		if (thread != null) {
			fImageThreads.add(thread);
		}
	}
	
	
	
	/**
	 * NON-DTFJ
	 * @param id
	 * 
	 */
	public JCImageThread getImageThread(ImagePointer id) {
		JCImageThread thread = null;
		if (id != null) {
			Iterator it = getThreads();
			while (thread == null && it.hasNext()) {
				JCImageThread foundThread = (JCImageThread) it.next();
				if (foundThread != null) {
					ImagePointer foundID = foundThread.getInternalID();
					if (foundID != null && foundID.equals(id)) {
						thread = foundThread;
					}
				}
			}
		}
		return thread;
	}
	
	
	/**
	 * NON-DTFJ
	 */
	public JCImageAddressSpace getImageAddressSpace() {
		return fImageAddressSpace;
	}

	/**
	 * NON-DTFJ
	 */
	public void setExecutable(ImageModule execMod) {
		fExecutable = execMod;
	}


	/**
	 * NON-DTFJ
	 */
	public void setID(String pid) {
		fID = pid;
	}

	/**
	 * NON-DTFJ
	 */
	public void setCurrentThreadID(long imageThreadID) {
		if (fCurrentThreadID == IBuilderData.NOT_AVAILABLE) {
			fCurrentThreadID = imageThreadID;
		}
	}

	public Properties getProperties() {
		return new Properties();		//not supported for this reader
	}
}
