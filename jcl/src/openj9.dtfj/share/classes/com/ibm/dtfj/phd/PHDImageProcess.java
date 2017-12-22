/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2008, 2017 IBM Corp. and others
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

package com.ibm.dtfj.phd;

import java.io.File;
import java.io.IOException;
import java.text.ParsePosition;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Properties;

import javax.imageio.stream.ImageInputStream;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImageModule;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.image.ImageThread;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.phd.parser.HeapdumpReader;

/** 
 * @author ajohnson
 */
class PHDImageProcess implements ImageProcess {

	private final boolean is64bit;
	private final List<JavaRuntime> runtimes;
	private final String pid;
	private final ImageProcess metaImageProcess;
	private final ArrayList<ImageModule>modules = new ArrayList<ImageModule>();
	private final LinkedHashMap<Object, ImageThread>threads = new LinkedHashMap<Object, ImageThread>();
	private CorruptData modules_cd;
	private ImageThread currentThread;
	
	PHDImageProcess(ImageInputStream stream, PHDImage parentImage, ImageAddressSpace space, ImageProcess metaImageProcess) throws IOException {
		this.metaImageProcess = metaImageProcess;
		JavaRuntime metaRuntime = getJavaRuntime(metaImageProcess);
		HeapdumpReader reader = new HeapdumpReader(stream, parentImage);
		this.is64bit = reader.is64Bit();
		processData(space);
		//can't get the PID as there is no file name
		pid = "<unknown pid>";
		runtimes = new ArrayList<JavaRuntime>();
		runtimes.add(new PHDJavaRuntime(stream, parentImage, space,this,metaRuntime));
	}
	
	PHDImageProcess(File file, PHDImage parentImage, ImageAddressSpace space, ImageProcess metaImageProcess) throws IOException {
		this.metaImageProcess = metaImageProcess;
		JavaRuntime metaRuntime = getJavaRuntime(metaImageProcess);
		HeapdumpReader reader = new HeapdumpReader(file, parentImage);
		this.is64bit = reader.is64Bit();
		processData(space);
		pid = getPID(file);
		runtimes = new ArrayList<JavaRuntime>();
		runtimes.add(new PHDJavaRuntime(file, parentImage, space,this,metaRuntime));
	
	}
	
	private JavaRuntime getJavaRuntime(ImageProcess process) {
		JavaRuntime metaRuntime = null;
		if (process != null) {
			Iterator i2 = process.getRuntimes();
			if (i2.hasNext()) {
				Object o2 = i2.next();
				if (!(o2 instanceof CorruptData) && o2 instanceof JavaRuntime) {
					metaRuntime = (JavaRuntime)o2;
				}
			}
		}
		return metaRuntime;
	}
	
	private void processData(ImageAddressSpace space) {
//		JavaRuntime metaRuntime = null;
//		if (metaImageProcess != null) {
//			Iterator i2 = metaImageProcess.getRuntimes();
//			if (i2.hasNext()) {
//				Object o2 = i2.next();
//				if (!(o2 instanceof CorruptData) && o2 instanceof JavaRuntime) {
//					metaRuntime = (JavaRuntime)o2;
//				}
//			}
//		}
		if (metaImageProcess != null) {
			try {
				for (Iterator it = metaImageProcess.getLibraries(); it.hasNext(); ) {
					Object next = it.next();
					if (next instanceof CorruptData) {
						modules.add(new PHDCorruptImageModule(space, (CorruptData)next));
					} else {
						try {
							ImageModule mod = (ImageModule)next;
							modules.add(new PHDImageModule(mod.getName()));
						} catch (CorruptDataException e) {
							modules.add(new PHDCorruptImageModule(space, e));
						}
					}
				}
			} catch (CorruptDataException e) {
				modules_cd = new PHDCorruptData(space, e);
			} catch (DataUnavailable e) {
				// Ignore
			}
			/*
			 * Set up the image threads
			 */
			ImageThread current;
			try {
				current = metaImageProcess.getCurrentThread();
			} catch (CorruptDataException e) {
				currentThread = new PHDCorruptImageThread(space, e.getCorruptData());
				current = null;
			}
			for (Iterator it = metaImageProcess.getThreads(); it.hasNext(); ) {
				Object next = it.next();
				if (next instanceof CorruptData) {
					threads.put(next, new PHDCorruptImageThread(space, (CorruptData)next));
				} else {
					ImageThread thrd = (ImageThread)next;
					ImageThread imageThread = getThread(space, thrd);
					if (thrd.equals(current)) {
						currentThread = imageThread;
					}
				}
				
			}
		}
//		pid = getPID(file);
//		runtimes = new ArrayList<JavaRuntime>();
//		runtimes.add(new PHDJavaRuntime(file, parentImage, space,this,metaRuntime));
	}

	private String getPID(File file) {
		String pid = null;
		String fn = file.getName();
		
		pid = getPID(fn, PHDImageFactory.earliestDump, PHDImageFactory.latestDump);
		
		if (pid == null) {
			pid = getPIDAIX(fn, PHDImageFactory.earliestDump, PHDImageFactory.latestDump);
		}

		return pid;
	}
	
	private String getPID(String fn, Date d1, Date d2) {
		SimpleDateFormat sdf = new SimpleDateFormat("yyyyMMdd.HHmmss");
		ParsePosition pp = new ParsePosition(0);
		for (int i = 0; i < fn.length(); ++i) {
			pp.setIndex(i);
			Date dF = sdf.parse(fn, pp);
			if (dF != null && !d1.before(dF) && d2.after(dF)) {
				// Match
				String rest[] = fn.substring(pp.getIndex()).split("\\.");
				for (int j = 0; j < rest.length; ++j) {
					if (rest[j].equals("")) continue;
					try {
						Integer.parseInt(rest[j]);
						return rest[j];
					} catch (NumberFormatException e) {
					}
					break;
				}
			}
		}
		return null;
	}

	private String getPIDAIX(String name, Date d1, Date d2) {
		// or for AIX 1.4.2 heapdumpPID.EPOCHTIME.phd
		// heapdump454808.1244656860.phd
		String prefix = "heapdump";
		// Allow extra stuff in front of "heapdump"
		int p = Math.max(0, name.indexOf(prefix));
		name = name.substring(p);
		String s[] = name.split("\\.");
		if ((s.length == 3  || s.length == 4 && s[3].equals("gz")) && s[0].startsWith(prefix)) {
			try {
				// Check the first part is also a number (PID)
				int i1 = Integer.parseInt(s[0].substring(prefix.length()));
				// Check the second part is also a number
				long l2 = Long.parseLong(s[1]);
				// The second number is the number of seconds since the epoch
				// Simple validation - since circa 2000 ?
				Date dF = new Date(l2 * 1000L);
				if (!dF.before(d1) && dF.before(d2)) {
					return Integer.toString(i1);
				}
			} catch (NumberFormatException e) {
			}
		}	
		return null;
	}

	public String getCommandLine() throws DataUnavailable, CorruptDataException {
		if (metaImageProcess != null) return metaImageProcess.getCommandLine();
		throw new DataUnavailable();
	}

	public ImageThread getCurrentThread() throws CorruptDataException {
		// Javacore doesn't have an accurate version of this
		return currentThread;
	}

	public Properties getEnvironment() throws DataUnavailable,
			CorruptDataException {
		if (metaImageProcess != null) return metaImageProcess.getEnvironment();
		throw new DataUnavailable();
	}

	public ImageModule getExecutable() throws DataUnavailable,
			CorruptDataException {
		throw new DataUnavailable();
	}

	public String getID() throws DataUnavailable, CorruptDataException {
		if (pid != null) {
			return pid;
		} else {
			throw new DataUnavailable();
		}
	}

	public Iterator<ImageModule> getLibraries() throws DataUnavailable, CorruptDataException {
		if (modules_cd != null) throw new CorruptDataException(modules_cd);
		if (modules.size() == 0) throw new DataUnavailable();
		return modules.iterator();
	}

	public int getPointerSize() {
		return is64bit ? 64 : 32;
	}

	public Iterator<JavaRuntime> getRuntimes() {
		return runtimes.iterator();
	}

	public String getSignalName() throws DataUnavailable, CorruptDataException {
		if (metaImageProcess != null) return metaImageProcess.getSignalName();
		throw new DataUnavailable();
	}

	public int getSignalNumber() throws DataUnavailable, CorruptDataException {
		if (metaImageProcess != null) return metaImageProcess.getSignalNumber();
		throw new DataUnavailable();
	}

	public Iterator<ImageThread> getThreads() {
		return threads.values().iterator();
	}
	
	/**
	 * Get a ImageThread corresponding to the metafile ImageThread.
	 * If it doesn't exist, then create one.
	 * @param metaThread
	 * @return
	 */
	ImageThread getThread(ImageAddressSpace space, ImageThread metaThread) {
		if (!threads.containsKey(metaThread)) {
			final PHDImageThread imageThread = new PHDImageThread(space, metaThread);
			threads.put(metaThread, imageThread);
		}
		return threads.get(metaThread);
	}

	public Properties getProperties() {
		return new Properties();		//not supported for this reader
	}

}
