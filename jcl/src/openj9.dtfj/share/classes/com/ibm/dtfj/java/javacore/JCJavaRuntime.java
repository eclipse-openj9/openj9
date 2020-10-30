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
package com.ibm.dtfj.java.javacore;

import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Properties;
import java.util.Vector;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.image.javacore.JCCorruptData;
import com.ibm.dtfj.image.javacore.JCImageAddressSpace;
import com.ibm.dtfj.image.javacore.JCImageProcess;
import com.ibm.dtfj.image.javacore.JCImageThread;
import com.ibm.dtfj.image.javacore.LookupKey;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.java.JavaVMInitArgs;
import com.ibm.dtfj.javacore.builder.IBuilderData;

/**
 * A javacore-based implementation of 
 * JavaRuntime. This supports partial object creation, meaning
 * that a javaclass, javathread, etc.. can be partially constructed and
 * stored, and at a later time during the javacore parsing, when
 * more information is available, existing javaclasses, javathreads,
 *  and so on can be looked up and data added.
 * <br><br>
 * However, all partial objects must contain at least one immutable field.
 * See the implementation of each runtime component for more information.
 * <br><br>
 * Rudimentary multiplatform support is also added in the form of unique
 * runtime ids that must be passed during construction.
 * 
 * @see com.ibm.dtfj.java.javacore.JCJavaClass
 * @see com.ibm.dtfj.java.javacore.JCJavaClassLoader
 * @see com.ibm.dtfj.java.javacore.JCJavaThread
 * @see com.ibm.dtfj.java.javacore.JCJavaMonitor
 * 
 * @see com.ibm.dtfj.java.JavaRuntime
 */
public class JCJavaRuntime implements JavaRuntime {
	
	private Vector fHeaps;
	private Vector fCompiledMethods;
	private HashMap fJavaClassLoaders;
	private HashMap fJavaClasses;
	private HashMap fMonitors;
	private HashMap fJavaThreads;
	private HashMap fJavaClassIDs;
	private JCJavaVMInitArgs fJavaVMInitArgs;
	private List fMemoryCategories;
	
	private String fFullVersion;
	private String fVersion;
	private String fID;
	
	private final LookupKey fIDLookupKey;
	private final JCImageProcess fImageProcess;
	private final JCImageAddressSpace fImageAddressSpace;
	
	private boolean isJITEnabled = false;
	private Properties jitOptions = new Properties();
	
	private long fStartTime;
	private boolean fStartTimeSet = false;
	private long fStartTimeNanos;
	private boolean fStartTimeNanosSet = false;
	

	public JCJavaRuntime(JCImageProcess imageProcess, String id) throws JCInvalidArgumentsException {
		if (imageProcess == null) {
			throw new JCInvalidArgumentsException("A runtime must be associated with a valid image process");
		}
		if (id == null) {
			throw new JCInvalidArgumentsException("A runtime must be associated with a valid id. This is necessary when parsing multiple runtimes");
		}
		fID = id;
		fImageProcess = imageProcess;
		fImageAddressSpace = imageProcess.getImageAddressSpace();
		
		fHeaps = new Vector();
		fCompiledMethods = new Vector();
		fJavaClassLoaders = new LinkedHashMap();
		fMonitors = new LinkedHashMap();
		fJavaThreads = new LinkedHashMap();
		fJavaClasses = new HashMap();
		fJavaClassIDs = new HashMap();
		fMemoryCategories = new LinkedList();
		
		fFullVersion = null;
		fVersion = null;
		fIDLookupKey = new LookupKey(IBuilderData.NOT_AVAILABLE);
		imageProcess.addRuntime(this);
	}
	
	/**
	 * @see com.ibm.dtfj.java.JavaRuntime#getCompiledMethods()
	 */
	public Iterator getCompiledMethods() {
		return fCompiledMethods.iterator();
	}

	/**
	 * @see com.ibm.dtfj.java.JavaRuntime#getHeaps()
	 */
	public Iterator getHeaps() {
		return fHeaps.iterator();
	}

	/**
	 * @see com.ibm.dtfj.java.JavaRuntime#getJavaClassLoaders()
	 */
	public Iterator getJavaClassLoaders() {
		return fJavaClassLoaders.values().iterator();
	}
	
	/**
	 * @see com.ibm.dtfj.java.JavaRuntime#getMonitors()
	 */
	public Iterator getMonitors() {
		return fMonitors.values().iterator();
	}
	
	/**
	 * @see com.ibm.dtfj.java.JavaRuntime#getThreads()
	 */
	public Iterator getThreads() {
		return fJavaThreads.values().iterator();
	}

	/**
	 *  @see com.ibm.dtfj.java.JavaRuntime#getJavaVMInitArgs()
	 */
	public JavaVMInitArgs getJavaVMInitArgs() throws DataUnavailable, CorruptDataException {
		if (fJavaVMInitArgs == null) {
			throw new DataUnavailable("JavaVMInitArgs not available");
		} else {
			return fJavaVMInitArgs;
		}
	}
	
	/**
	 *  @see com.ibm.dtfj.java.JavaRuntime#getJavaVM()
	 */
	public ImagePointer getJavaVM() throws CorruptDataException {
		throw new CorruptDataException(new JCCorruptData(null));
	}

	/**
	 *  @see com.ibm.dtfj.java.JavaRuntime#getTraceBuffer()
	 */
	public Object getTraceBuffer(String arg0, boolean arg1) throws CorruptDataException {
		throw new CorruptDataException(new JCCorruptData(null));
	}

	/**
	 * @see com.ibm.dtfj.runtime.ManagedRuntime#getFullVersion()
	 */
	public String getFullVersion() throws CorruptDataException {
		if (fFullVersion == null) {
			throw new CorruptDataException(new JCCorruptData(null));
		}
		return fFullVersion;
	}

	/**
	 * @see com.ibm.dtfj.runtime.ManagedRuntime#getVersion()
	 */
	public String getVersion() throws CorruptDataException {
		if (fVersion == null) {
			throw new CorruptDataException(new JCCorruptData(null));
		}
		return fVersion;
	}
	
	/*
	 * ****************
	 * NON-DTFJ methods
	 * ****************
	 */
	
	/**
	 * NON-DTFJ
	 * <br>
	 * <b>For internal building purposes only</b>. Do not call outside the DTFJ implementation.
	 * <br><br>
	 * May be used in multiple runtime environments, where each runtime in a javacore
	 * is identified uniquely. This method is generally just used during the building process only.
	 * 
	 */
	public String getInternalID() {
		return fID;
	}
	
	/**
	 * NON-DTFJ
	 * <br>
	 * <b>For internal building purposes only</b>. Do not call outside the DTFJ implementation.
	 * <br><br>
	 * @param javaClassLoader
	 * @throws JCRegistrationFailureException if invalid class loader passed.
	 */
	public void addJavaClassLoader(JCJavaClassLoader javaClassLoader) throws JCInvalidArgumentsException  {
		if (javaClassLoader == null) {
			throw new JCInvalidArgumentsException("Not a valid java class loader.");
		}
		fJavaClassLoaders.put(new LookupKey(((JCJavaClassLoader)javaClassLoader).getPointerID().getAddress()), javaClassLoader);
	}
	
	/**
	 * NON-DTFJ
	 * <br>
	 * <b>For internal building purposes only</b>. Do not call outside the DTFJ implementation.
	 * <br><br>
	 * @param javaClassLoaderName
	 * @return javaclassloader if found or null
	 */
	public JCJavaClassLoader findJavaClassLoader(long clLoaderID) {
		fIDLookupKey.setKey(clLoaderID);
		return (JCJavaClassLoader) fJavaClassLoaders.get(fIDLookupKey);
	}
	
	/**
	 * NON-DTFJ
	 * <br>
	 * <b>For internal building purposes only</b>. Do not call outside the DTFJ implementation.
	 * <br><br>
	 * @param monitor must not be null
	 * @throws JCRegistrationFailureException if invalid monitor passed.
	 */
	public void addMonitor(JCJavaMonitor monitor) throws JCInvalidArgumentsException {
		if (monitor == null) {
			throw new JCInvalidArgumentsException("Must pass a valid java monitor");
		}
		fMonitors.put(new LookupKey(monitor.getID().getAddress()), monitor);
	}
	
	/**
	 * NON-DTFJ
	 * <br>
	 * <b>For internal building purposes only</b>. Do not call outside the DTFJ implementation.
	 * <br><br>
	 * @param id
	 * @return found java monitor or null
	 */
	public JCJavaMonitor findMonitor(long id) {
		fIDLookupKey.setKey(id);
		return (JCJavaMonitor) fMonitors.get(fIDLookupKey);
	}
	/**
	 * NON-DTFJ
	 * <br>
	 * <b>For internal building purposes only</b>. Do not call outside the DTFJ implementation.
	 * <br><br>
	 * @param javaThread must not be null.
	 * @throws JCRegistrationFailureException if java thread is null
	 */
	public void addJavaThread(JCJavaThread javaThread) throws JCInvalidArgumentsException{
		if (javaThread == null) {
			throw new JCInvalidArgumentsException("Must pass a valid java thread");
		}
		fJavaThreads.put(new LookupKey(javaThread.getThreadID().getAddress()), javaThread);
	}
	
	/**
	 * NON-DTFJ
	 * <br>
	 * <b>For internal building purposes only</b>. Do not call outside the DTFJ implementation.
	 * <br><br>
	 * In most cases, the threadID is the tid in a javacore, but in some other occasions, the id
	 * passed could be the system_thread_id or even the native thread. If a java thread is not
	 * found via tid, see if it can be found indirectly by an image thread that may be associated
	 * with the java thread (the image thread has a system thread ID as well as a native thread ID).
	 * @param threadID usually, tid in a javacore, may be id of some other internal vm datastructure.
	 * @return found java thread or null.
	 */
	public JCJavaThread findJavaThread(long threadID) {
		Object javaThread = null;
		fIDLookupKey.setKey(threadID);
		if ((javaThread = fJavaThreads.get(fIDLookupKey)) == null) {
			Iterator it = fJavaThreads.values().iterator();
			while (javaThread == null && it.hasNext()) {
				JCJavaThread jThread = (JCJavaThread) it.next();
				JCImageThread imageThread = jThread.internalGetImageThread();
				if (imageThread != null) {
					// lookup via systemthreadID
					ImagePointer pointer = imageThread.getSystemThreadID();
					long address = (pointer != null) ? pointer.getAddress() : IBuilderData.NOT_AVAILABLE;
					if (address != IBuilderData.NOT_AVAILABLE && address == threadID) {
						javaThread = jThread;
					}
					// lookup via nativethreadID. This is always set for any valid image thread.
					else {
						pointer = imageThread.getInternalID();
						if (pointer.getAddress() == threadID) {
							javaThread = jThread;
						}
					}
				}
			}
		}
		return (JCJavaThread) javaThread;
	}
	
	/**
	 * NON-DTFJ
	 * <br>
	 * <b>For internal building purposes only</b>. Do not call outside the DTFJ implementation.
	 * <br><br>
	 * @return image process containing this java runtime. This should never be null.
	 */
	public JCImageProcess getImageProcess() {
		return fImageProcess;
	}
	
	/**
	 * NON-DTFJ
	 * <br>
	 * <b>For internal building purposes only</b>. Do not call outside the DTFJ implementation.
	 * <br><br>
	 * Two separate maps are kept for classes. One based on class ID, the other based on class name. Both
	 * are used for lookups while building the runtime object. When a valid javaclass is added, it gets
	 * added at least the name-based map, and may be added to the ID-based map if the ID is available. The
	 * class name of the java class is a requirement, so this field must be set in the java class being passed.
	 * 
	 * @param javaClass must not be null or exception thrown
	 * @throws JCRegistrationFailureException if java class is null
	 */
	public void addJavaClass(JCJavaClass javaClass) throws JCInvalidArgumentsException {
		if (javaClass == null) {
			throw new JCInvalidArgumentsException("Must pass a valid javaClass");
		}
		String javaClassName = javaClass.internalGetName();
		ImagePointer pointer = javaClass.getID();
		if (pointer != null) {
			long id = pointer.getAddress();
			fJavaClassIDs.put(new LookupKey(id), javaClass);
		}
		fJavaClasses.put(javaClassName, javaClass);
	}

	/**
	 * NON-DTFJ
	 * <br>
	 * <b>For internal building purposes only</b>. Do not call outside the DTFJ implementation.
	 * @param javaClassName
	 * @return found class or null if not found
	 */
	public JCJavaClass findJavaClass(String javaClassName) {
		return (JCJavaClass) fJavaClasses.get(javaClassName);
	}
	
	/**
	 * NON-DTFJ
	 * <br>
	 * <b>For internal building purposes only</b>. Do not call outside the DTFJ implementation.
	 * <br><br>
	 * In some cases, all that is available is a class ID, so it should be possible to retrieve
	 * a class based on just an ID.
	 * 
	 * @param id class address
	 * @return found class or null if not found
	 */
	public JCJavaClass findJavaClass(long id) {
		JCJavaClass javaClass = null;
		if (fImageAddressSpace.isValidAddressID(id)) {
			fIDLookupKey.setKey(id);
			javaClass = (JCJavaClass) fJavaClassIDs.get(fIDLookupKey);
		}
		return javaClass;
	}
	
	/**
	 * NON-DTFJ
	 * <br>
	 * <b>For internal building purposes only</b>. Do not call outside the DTFJ implementation.
	 * <br><br>
	 * @param args, must not be null
	 * @throws JCRegistrationFailureException if invalid args passed.
	 */
	public void addJavaVMInitArgs(JCJavaVMInitArgs args) throws JCInvalidArgumentsException {
		if (args == null) {
			throw new JCInvalidArgumentsException("Must pass a valid JavaVMInitArgs object");
		}
		fJavaVMInitArgs = args;
	}

	public Iterator getHeapRoots() {
		return Collections.EMPTY_LIST.iterator();
	}

	public JavaObject getObjectAtAddress(ImagePointer address)	throws DataUnavailable {
		throw new DataUnavailable("Object information not available.");
	}
	
	/**
	 * NON-DTFJ
	 * <br>
	 * <b>For internal building purposes only</b>. Do not call outside the DTFJ implementation.
	 * <br><br>
	 * @param args, the version string
	 */
	public void setVersion(String version) {
		fVersion = version;
	}

	public Iterator getMemoryCategories() throws DataUnavailable
	{
		return Collections.unmodifiableList(fMemoryCategories).iterator();
	}

	public Iterator getMemorySections(boolean includeFreed)
			throws DataUnavailable
	{
		throw new DataUnavailable("Memory section data not available in JavaCore");
	}

	public void addTopLevelMemoryCategory(JCJavaRuntimeMemoryCategory category)
	{
		fMemoryCategories.add(category);
	}

	public boolean isJITEnabled() throws DataUnavailable, CorruptDataException {
		return isJITEnabled;
	}

	public void setJITEnabled(boolean enabled) {
		isJITEnabled = enabled;
	}
	
	public void addJITProperty(String name, String value) {
		jitOptions.put(name, value);
	}
	
	public Properties getJITProperties() throws DataUnavailable,	CorruptDataException {
		if(isJITEnabled) {
			return jitOptions;
		} else {
			throw new DataUnavailable("The JIT was not enabled for this runtime");
		}
	}

	public long getStartTime() throws DataUnavailable, CorruptDataException {
		if (fStartTimeSet) {
			return fStartTime;
		} else {
			throw new DataUnavailable("JVM start time not available");
		}
	}

	public long getStartTimeNanos() throws DataUnavailable, CorruptDataException {
		if (fStartTimeNanosSet) {
			return fStartTimeNanos;
		} else {
			throw new DataUnavailable("JVM start nanotime not available");
		}
	}
	
	public void setStartTime(long startTime) {
		fStartTime = startTime;
		fStartTimeSet = true;
	}
	
	public void setStartTimeNanos(long nanoTime) {
		fStartTimeNanos = nanoTime;
		fStartTimeNanosSet = true;
	}
	
}
