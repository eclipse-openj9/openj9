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
package com.ibm.dtfj.java.j9;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Properties;
import java.util.Set;
import java.util.Vector;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageThread;
import com.ibm.dtfj.image.j9.CorruptData;
import com.ibm.dtfj.image.j9.ImageProcess;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaObject;

/**
 * @author jmdisher
 *
 */
public class JavaRuntime implements com.ibm.dtfj.java.JavaRuntime
{
	private ImageProcess _containingProc;
	private ImagePointer  _address;
	private LinkedHashMap _classLoaders = new LinkedHashMap(); // use linked hashmap for predictable ordering
	private Vector _vmThreads = new Vector();
	private LinkedHashMap _classes = new LinkedHashMap(); // use linked hashmap for predictable ordering
	private Set _arrayClasses = new HashSet();
	private Map _arrayClassesMap = new HashMap();
	private Vector _monitors = new Vector();
	private Vector _heaps = new Vector();
	private Vector _heapRoots = new Vector();
	private HashMap _traceBuffers = new HashMap();
	private Properties _systemProperties = new Properties();
	private JavaVMInitArgs _javaVMInitArgs;
	private boolean _objectsShouldInferHash = false;	//used as a work-around for our hash problem.  Set to true if this is a VM version which uses our 15-bits shifted hash algorithm
	
	//these are caches provided to help optimize or clean-up other DTFJ routines
	private HashMap _methodsByID = new HashMap();
	private Vector deferMonitors = new Vector();
	
	//to contain objects that represent classes, threads, monitors or classloaders
	private HashMap _specialObjects = new HashMap(); 
	
	com.ibm.dtfj.java.j9.JavaClass _weakReferenceClass = null;
	com.ibm.dtfj.java.j9.JavaClass _softReferenceClass = null;
	com.ibm.dtfj.java.j9.JavaClass _phantomReferenceClass = null;
	
	com.ibm.dtfj.java.j9.JavaClass _javaLangObjectClass = null;

	private final static String WEAKREF_CLASS_NAME = "java/lang/ref/WeakReference";
	private final static String SOFTREF_CLASS_NAME = "java/lang/ref/SoftReference";
	private final static String PHANTOMREF_CLASS_NAME = "java/lang/ref/PhantomReference";
	private final static String OBJECT_CLASS_NAME = "java/lang/Object";
	
	public JavaRuntime(ImageProcess containingProc, ImagePointer baseAddress, String runtimeVersion)
	{
		if (null == baseAddress) {
			throw new IllegalArgumentException("A Java Runtime cannot have a null base address");
		}
		_containingProc = containingProc;
		_address = baseAddress;
		_objectsShouldInferHash = ("2.2".equals(runtimeVersion)) || ("2.3".equals(runtimeVersion)) || ("2.4".equals(runtimeVersion));
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaRuntime#getJavaVM()
	 */
	public ImagePointer getJavaVM() throws CorruptDataException
	{
		return _address;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaRuntime#getJavaClassLoaders()
	 */
	public Iterator getJavaClassLoaders()
	{
		return _classLoaders.values().iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaRuntime#getThreads()
	 */
	public Iterator getThreads()
	{
		return _vmThreads.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaRuntime#getCompiledMethods()
	 */
	public Iterator getCompiledMethods()
	{
		Iterator classes = _classes.values().iterator();
		Vector compiledMethods = new Vector();
		
		while (classes.hasNext()) {
			JavaAbstractClass oneClass = (JavaAbstractClass) classes.next();
			Iterator methods = oneClass.getDeclaredMethods();
			
			while (methods.hasNext()) {
				JavaMethod method = (JavaMethod)methods.next();
				
				if (method.getCompiledSections().hasNext()) {
					//this is jitted at least once
					compiledMethods.add(method);
				}
			}
		}
		return compiledMethods.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaRuntime#getMonitors()
	 */
	public Iterator getMonitors()
	{
		// we need to check that all the deferred items have been processed by now
		checkDeferredMonitors();
		return _monitors.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaRuntime#getHeaps()
	 */
	public Iterator getHeaps()
	{
		return _heaps.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaRuntime#getTraceBuffer(java.lang.String, boolean)
	 */
	public Object getTraceBuffer(String bufferName, boolean formatted)
			throws CorruptDataException
	{
		return _traceBuffers.get(bufferName);
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.runtime.ManagedRuntime#getFullVersion()
	 */
	public String getFullVersion() throws CorruptDataException
	{
		//TODO: remove after this code has passed into testing
		return getVersion();
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.runtime.ManagedRuntime#getVersion()
	 */
	public String getVersion() throws CorruptDataException
	{
		String javaFullVersion = getRequiredSystemProperty("java.fullversion");
		String javaRuntimeVersion = getRequiredSystemProperty("java.runtime.version");
		String javaRuntimeName = getRequiredSystemProperty("java.runtime.name");
		String javaVMName = getRequiredSystemProperty("java.vm.name");
		String version;
		
		version = javaRuntimeName + "(build " + javaRuntimeVersion + ")\n";
		version += javaVMName + "(" + javaFullVersion + ")";
		
		return version;
	}
	
	public void addClass(JavaAbstractClass theClass)
	{
		if (null == theClass) {
			return;
		}
		long id = theClass.getID().getAddress();
		_classes.put(Long.valueOf(id), theClass);
		try {
			if (theClass.isArray()) {
				// Useful for component type lookups
				// Only array classes added, since only arrays with dimension greater than 1 require lookup
				// (For 1-dimensional arrays, the component type is just their leaf class, which doesn't require lookup)
				
				// The name of an array class is built starting from its leaf class. When this method is called 
				// (=during the XML parsing phase), the leaf class may not have been parsed yet, so getName() would 
				// fail with a NPE (see implementation). Therefore, we cannot build the map now, we will have to do it lazily
				// at the first invocation of getComponentTypeForClass(). For now, we'll just record the array classes somewhere.
				_arrayClasses.add(theClass);
			}
		} catch (CorruptDataException e1) {
			// Nothing to do. Just swallow it...
		}
		if (theClass instanceof com.ibm.dtfj.java.j9.JavaClass) {
			try {
				if (WEAKREF_CLASS_NAME.equals(theClass.getName())) {
					_weakReferenceClass = (com.ibm.dtfj.java.j9.JavaClass)theClass;
				} else if (SOFTREF_CLASS_NAME.equals(theClass.getName())) {
					_softReferenceClass = (com.ibm.dtfj.java.j9.JavaClass)theClass;
				} else if (PHANTOMREF_CLASS_NAME.equals(theClass.getName())) {
					_phantomReferenceClass = (com.ibm.dtfj.java.j9.JavaClass)theClass;
				} else if (OBJECT_CLASS_NAME.equals(theClass.getName())) {
					_javaLangObjectClass = (com.ibm.dtfj.java.j9.JavaClass)theClass;
				}
			} catch (CorruptDataException e) {
				// just swallow it...
			}
		}		
	}
	
	public void addClassLoader(com.ibm.dtfj.java.j9.JavaClassLoader loader)
	{
		long id = loader.getID();
		_classLoaders.put(Long.valueOf(id), loader);
	}

	public com.ibm.dtfj.java.JavaClass getClassForID(long classID)
	{
		return (com.ibm.dtfj.java.JavaClass)_classes.get(Long.valueOf(classID));
	}

	JavaClass getComponentTypeForClass(JavaClass theClass) throws CorruptDataException
	{
		//CMVC 161798 add check to see if array class set is valid before populating the map
		if ((_arrayClasses != null) && _arrayClassesMap.isEmpty()) {
			//1st invocation. Build the map
			Iterator i = _arrayClasses.iterator();
			while (i.hasNext()) {
				// CMVC 165884 add check for null classloader
				JavaArrayClass currentClass = (JavaArrayClass)i.next();
				String className = currentClass.getName();
				String classLoaderID = "";
				JavaClassLoader currentClassLoader = currentClass.getClassLoader();
				if( currentClassLoader != null ) {
					classLoaderID = currentClassLoader.toString();
				}
				_arrayClassesMap.put(className+classLoaderID, currentClass);
			}
			// don't need _arrayClasses anymore
			_arrayClasses.clear();
			_arrayClasses = null;
		}
		
		
		JavaClass componentType = null;
		String componentClassName = theClass.getName().substring(1);
		// CMVC 165884 add check for null classloader
		String classLoaderID = "";
		JavaClassLoader theClassLoader = theClass.getClassLoader();
		if( theClassLoader != null ) {
			classLoaderID = theClassLoader.toString();
		}
		Object obj = _arrayClassesMap.get(componentClassName+classLoaderID);
		if (obj instanceof JavaClass) {
			componentType = (JavaClass)obj;
		}
		
		return componentType;
	}

	public JavaClassLoader getClassLoaderForID(long loaderID)
	{
		return (JavaClassLoader) _classLoaders.get(Long.valueOf(loaderID));
	}

	public void addMonitor(JavaMonitor monitor)
	{
		_monitors.add(monitor);
		// and look at the deferred items to see if any match
		for (int i=0;i<deferMonitors.size();i++) {
			DeferMonitor mon = (DeferMonitor) deferMonitors.get(i);
			if (monitor.getID().getAddress() == mon.id) {
				if (mon.blocked) monitor.addBlockedThread(mon.thread);
				else monitor.addWaitingThread(mon.thread);
				mon.id = 0;
			}
		}
	}

	public void addHeap(JavaHeap heap)
	{
		_heaps.add(heap);
	}

	public void addHeapRoot(JavaReference heapRoot)
	{
		_heapRoots.add(heapRoot);
	}

	/** 
	 * It turns out that there's an assumption in the parser that's not valid.  Threads can be mentioned in the
	 * xml before the monitors that they may be waiting for or blocked on.  Hence the code in addThread would
	 * fail to find a matching monitor and just ignored the issue.  As a simple and local fix we build a small
	 * object to represent the forward referenced monitor together with the thread that is either blocked or 
	 * waiting on it.  When the monitor eventually turns up in addMonitor we can process the deferred item and
	 * add the thread to the waiting or blocked lists.  When the parsing is complete we expect all the deferred
	 * to have been consumed.  In fact we make the check if anyone asks for the set of monitors since that must be
	 * after the parsing is complete and is before anyone can make any use of monitors. 
	 */
	static private class DeferMonitor {
		long 		id;  		// monitor ident
		boolean 	blocked;	// thread is blocked or waiting if false 
		JavaThread 	thread;		// the waiting or blocked thread
		DeferMonitor(long id, boolean blocked, JavaThread thread) {
			this.id = id;
			this.blocked = blocked;
			this.thread = thread;			
		}
		public String toString() {
			return "Defer monitor " + id;
		}
	}
	private void checkDeferredMonitors() {
		int errs = 0;
		for (int i=0;i<deferMonitors.size();i++) {
			DeferMonitor mon = (DeferMonitor) deferMonitors.get(i);
			if (mon.id != 0) {
				// At this point we attempt a cover-up operation by building the monitor
				ImagePointer ptr = pointerInAddressSpace(mon.id);
				JavaMonitor autoMon = new JavaMonitor(this,ptr,"Auto-generated monitor #"+mon.id,null,0);
				// adding this to the runtime automatically resolves any other referencing deferred monitors
				addMonitor(autoMon);
				errs++;
			}
		}
		deferMonitors.clear();
		//if (errs > 0) {
		//	System.out.println("Some deferred monitors not processed " + errs);
		//}
	}
	/**
	 * Adds a JavaThread to the runtime along with optional IDs of monitors that it is blocked on or waiting on
	 * 
	 * @param thread The JavaThread to add to the runtime
	 * @param blockedOnMonitor The ID of the monitor that this thread is blocked on (0 if it is not blocked)
	 * @param waitingOnMonitor The ID of the monitor that this thread is waiting on (0 if it is not waiting)
	 */
	public void addThread(JavaThread thread, long blockedOnMonitor, long waitingOnMonitor)
	{
		_vmThreads.add(thread);
		if (0 != blockedOnMonitor) {
			assert (0 == waitingOnMonitor) : "Thread cannot be blocked and waiting at the same time";
			Iterator iter = _monitors.iterator();

			boolean found = false;
			while (iter.hasNext()) {
				JavaMonitor monitor = (JavaMonitor)iter.next();
				if(monitor.getID().getAddress() == blockedOnMonitor) {
					monitor.addBlockedThread(thread);
					found = true;
					break;
				}
			}
			if (! found) {
				deferMonitors.add(new DeferMonitor(blockedOnMonitor,true,thread));
			}
		}

		if (0 != waitingOnMonitor) {
			assert (0 == blockedOnMonitor) : "Thread cannot be blocked and waiting at the same time";
			Iterator iter = _monitors.iterator();

			boolean found = false;
			while (iter.hasNext()) {
				JavaMonitor monitor = (JavaMonitor)iter.next();
				if(monitor.getID().getAddress() == waitingOnMonitor) {
					monitor.addWaitingThread(thread);
					found = true;
					break;
				}
			}
			if (! found) {
				deferMonitors.add(new DeferMonitor(waitingOnMonitor,false,thread));
			}
		}
	}

	public void setTraceBufferForName(TraceBuffer buffer, String key)
	{
		_traceBuffers.put(key, buffer);
	}

	public String getSystemProperty(String key)
	{
		return _systemProperties.getProperty(key);
	}

	public String getSystemProperty(String key, String defaultValue)
	{
		return _systemProperties.getProperty(key, defaultValue);
	}
	
	String getRequiredSystemProperty(String key) throws CorruptDataException
	{
		String value = getSystemProperty(key);
		
		if (value == null) {
			throw new CorruptDataException(new CorruptData("Required system property " + key + " not found in JExtract output", null));
		}
		
		return value;
	}
	
	public void setSystemProperty(String key, String value)
	{
		_systemProperties.setProperty(key, value);
	}

	public boolean equals(Object obj)
	{
		boolean isEqual = false;
		
		if (obj instanceof JavaRuntime) {
			isEqual = _address.equals(((JavaRuntime)obj)._address);
		}
		return isEqual;
	}

	public int hashCode()
	{
		//the version is mutable so might not have been set
		return _address.hashCode();
	}
	
	public com.ibm.dtfj.java.JavaVMInitArgs getJavaVMInitArgs() throws DataUnavailable, CorruptDataException {
		if (_javaVMInitArgs == null) {
			throw new DataUnavailable("JavaVMInitArgs");
		} else {
			return _javaVMInitArgs;
		}
	}
	
	/**
	 * Not to ever be put into the public interface.  This is used by JavaObjects to see if they can try to guess their hash by looking at their flags
	 * @return true if objects have their hashed stored in the known low 15 bits of the hash
	 */
	boolean objectShouldInferHash()
	{
		return _objectsShouldInferHash;
	}

	/**
	 * Since a JavaVM cannot span address spaces, use the VM's container address space to build this pointer, given the raw ID
	 *  
	 * @param id
	 * @return
	 */
	public ImagePointer pointerInAddressSpace(long id)
	{
		return _address.getAddressSpace().getPointer(id);
	}

	public ImageThread nativeThreadForID(long nativeID)
	{
		ImageThread thread = null;
		try {
			if (null != _containingProc) {
				for (Iterator iter = _containingProc.getThreads(); iter.hasNext();) {
					ImageThread t = (ImageThread) iter.next();
					if (Long.decode(t.getID()).longValue() == nativeID)
						thread = t;
				}
			}
		} catch (Exception e) {
			// Ignore
		}
		return thread;
	}

	public JavaMethod methodForID(long method)
	{
		return (JavaMethod) _methodsByID.get(Long.valueOf(method));
	}
	
	public void addMethodForID(JavaMethod method, long id)
	{
		_methodsByID.put(Long.valueOf(id), method);
	}

	public JavaVMInitArgs createJavaVMInitArgs(int version, boolean ignoreUnrecognized)
	{
		if (null != _javaVMInitArgs) {
			//this means that we tried to create the init args array twice which doesn't make sense
			throw new IllegalStateException("JavaVMInitArgs being created more than once");
		} else {
			_javaVMInitArgs = new JavaVMInitArgs(_address.getAddressSpace(), version, ignoreUnrecognized);
			return _javaVMInitArgs;
		}
	}
	
	/**
	 * A helper method required by some of the structures hanging off of the VM.  Returns the number of bytes required to express a native
	 * pointer on the VM target platform
	 * @return 4 or 8, depending on the pointer size of the target platform
	 */
	public int bytesPerPointer()
	{
		// CMVC 156226 - DTFJ exception: XML and core file pointer sizes differ (zOS)
		int s = _containingProc.getPointerSize();
	    int s2 = ((com.ibm.dtfj.image.j9.ImageAddressSpace)(_address.getAddressSpace())).bytesPerPointer();
		if (s == 64 || s == 32 || s == 31) {
			return (s + 7) / 8;
		}
		return s2;
	}
	
	protected Iterator getClasses() {
		return _classes.values().iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaRuntime#getHeapRoots()
	 */
	public Iterator getHeapRoots()
	{
		return _heapRoots.iterator();
	}

	private static final int DEFAULT_OBJECT_ALIGNMENT = 8;		//default object alignment if it cannot be found in the XML
	
	//CMVC 173262 - improve validation - throw NPE if address is null, throw IAE if address = 0, throw IAE if address in not correctly aligned
	public JavaObject getObjectAtAddress(ImagePointer address) throws CorruptDataException, IllegalArgumentException
	{
		if(null == address) {
			throw new NullPointerException("The ImagePointer was null");
		}
		long ptr = address.getAddress();
		if(0 == ptr) {
			throw new IllegalArgumentException("The object address " + ptr + " is not in any heap");
		}
		Iterator heaps = getHeaps();
		JavaHeapRegion region = null;
		JavaHeap heap = null;
		
		while ((null == region) && (heaps.hasNext())) {
			heap = (JavaHeap)(heaps.next());
			region = heap.regionForPointer(address);
		}
		if (null == region) {
			// We permit off-heap objects to be created, eg for class objects prior to J9 2.4 (default to 8-byte alignment - this won't actually be used since it is only needed for walking arraylet sections)
			if((address.getAddress() & DEFAULT_OBJECT_ALIGNMENT - 1) != 0) {
				throw new IllegalArgumentException("Invalid alignment for JavaObject. Address = " + address.toString());
			}
			return new com.ibm.dtfj.java.j9.JavaObject(this, address, heap, 0, 0, false, DEFAULT_OBJECT_ALIGNMENT);
		} else {
			return region.getObjectAtAddress(address);
		}
	}

	public void addSpecialObject(JavaObject obj) {
		_specialObjects.put(obj.getID(), obj);
	}

	public JavaObject getSpecialObject(ImagePointer address) {
		return (JavaObject)_specialObjects.get(address);
	}

	public Iterator getMemoryCategories() throws DataUnavailable
	{
		throw new DataUnavailable("This implementation of DTFJ does not support getMemoryCategories");
	}

	public Iterator getMemorySections(boolean includeFreed)
			throws DataUnavailable
	{
		throw new DataUnavailable("This implementation of DTFJ does not support getMemorySections");
	}

	public boolean isJITEnabled() throws DataUnavailable, CorruptDataException {
		throw new DataUnavailable("This implementation of DTFJ does not support isJITEnabled");
	}

	public Properties getJITProperties() throws DataUnavailable,	CorruptDataException {
		throw new DataUnavailable("This implementation of DTFJ does not support getJITProperies");
	}

	public long getStartTime() throws DataUnavailable, CorruptDataException {
		// Not supported in legacy DTFJ (pre-DDR)
		throw new DataUnavailable("Dump start time is not available");
	}

	public long getStartTimeNanos() throws DataUnavailable, CorruptDataException {
		// Not supported in legacy DTFJ (pre-DDR)
		throw new DataUnavailable("Dump start time is not available");
	}

}
