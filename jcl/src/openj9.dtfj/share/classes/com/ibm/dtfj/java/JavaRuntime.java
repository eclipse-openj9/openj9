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
package com.ibm.dtfj.java;

import java.util.Iterator;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.runtime.ManagedRuntime;
import java.util.Properties;

/**
 * Represents a Java runtime.
 */
public interface JavaRuntime extends ManagedRuntime {

    /**
     * Get the object that represents the virtual machine
     * @return the address of the JavaVM structure which represents this JVM instance in JNI
     * @throws CorruptDataException 
     */
    public ImagePointer getJavaVM() throws CorruptDataException;
    
    /**
     * Fetch the JavaVMInitArgs which were used to create this VM.
     * See JNI_CreateJavaVM in the JNI Specification for more details.
     * 
     * @return the JavaVMInitArgs which were used to create this VM.
     * @throws DataUnavailable if the arguments are not available 
     * @throws CorruptDataException
     */
    public JavaVMInitArgs getJavaVMInitArgs() throws DataUnavailable, CorruptDataException;
    
    /**
     * Get the set of class loaders active in this VM
     * @return an iterator of all of the class loaders within this JavaVM
     * 
     * @see JavaClassLoader
     * @see com.ibm.dtfj.image.CorruptData
     */
    public Iterator getJavaClassLoaders();
    
    /**
     * Get the set of Java threads known by the VM 
     * @return an iterator of the JavaThreads in the runtime
     * 
     * @see JavaThread
     * @see com.ibm.dtfj.image.CorruptData
     */
    public Iterator getThreads();
    
    /**
     * This is short cut method. The same result can be found by iterating over all
     * methods in all class loaders in all classes.
     * 
     * @return an iterator over all of the JavaMethods in the JavaRuntime which
     * have been compiled
     * 
     * @see JavaMethod
     * @see com.ibm.dtfj.image.CorruptData
     */
    public Iterator getCompiledMethods();
    
    /**
     * Provides access to the collection of monitors used in the JavaVM. This 
     * collection includes both monitors associated with managed objects (e.g. object
     * monitors) and monitors associated with internal control structures (e.g.
     * raw monitors)
     * 
     * @return an iterator over the collection of monitors
     * 
     * @see JavaMonitor
     * @see com.ibm.dtfj.image.CorruptData
     */
    public Iterator getMonitors();
    
    /**
     * Get the set of heaps known by the VM
     * @return an iterator for all of the Java heaps within this runtime. Heaps
     * may be specific to this JavaVM instance, or may be shared between multiple
     * JavaVM instances
     * 
     * @see JavaHeap
     * @see com.ibm.dtfj.image.CorruptData
     */
    public Iterator getHeaps();
    
    /**
     * Get the set of object and class roots known to the VM.
     * Stack frame roots are not included in the set, they can be retrieved using JavaStackFrame.getHeapRoots().
	 *
     * @return an iterator over the collection of JavaReferences representing the known global heap roots within this runtime. 
     *      
     * @see JavaReference
     * @see JavaStackFrame
     * @see com.ibm.dtfj.image.CorruptData
     * 
     */
    public Iterator getHeapRoots();
    
    /**
     * Fetches implementation specific trace buffers, like the verbose GC buffer
     * or the Universal Trace Engine (UTE) buffer
     * 
     * @param bufferName a String naming the buffer to be fetched
     * @param formatted true if formatting should be performed on the buffer, or 
     * false if the raw buffer contents should be returned
     * @return an implementation specific result, depending on the parameters 
     * @throws CorruptDataException 
     */
    public Object getTraceBuffer(String bufferName, boolean formatted) throws CorruptDataException;
    
    /**
     * Gets the object located at address <code>address</code> in the heap.
     * @param	address the <code>ImagePointer</code> instance representing the start address of object in the heap;
     * @return	the <code>JavaObject</code> instance representing the located object.
     * @throws 	IllegalArgumentException	if  <code>address</code> is outside the heap's boundaries, or if it doesn't point to the start location of an object;
     * @throws 	MemoryAccessException 		if  <code>address</code> is is in the heap but it's not accessible from the dump;
     * @throws 	CorruptDataException 		if any data needed to build the returned instance of <code>JavaObject</code> is corrupt.
     * @throws 	DataUnavailable	 			if any data needed to build the returned instance of <code>JavaObject</code> is not available.
     * @see 	com.ibm.dtfj.java.JavaObject
     */
     public JavaObject getObjectAtAddress(ImagePointer address) throws CorruptDataException, IllegalArgumentException, MemoryAccessException, DataUnavailable;
     
 	
     /**
      * Returns iterator of the top-level memory categories used by this
      * Java runtime.
      * 
      * @return Iterator of memory categories
      * @see JavaRuntimeMemoryCategory CorruptData
      * @since 1.5
      */
     public Iterator getMemoryCategories() throws DataUnavailable;

     /**
      * Returns an iterator of JavaRuntimeMemorySection objects corresponding to the blocks of memory allocated by the JavaRuntime.
      * 
      * @param includeFreed If true, iterator will iterate over blocks of memory that have been freed, but haven't been re-used yet.
      * @return Iterator of memory sections.
      * @see JavaRuntimeMemorySection CorruptData
      * @since 1.5
      */
     public Iterator getMemorySections(boolean includeFreed) throws DataUnavailable;

    
	/**
	 * @param obj
	 * @return True if the given object refers to the same Java Runtime in the image
	 */
	public boolean equals(Object obj);
	public int hashCode();
	
	
	/**
	 * Determine if the JIT was enabled for this Java runtime.
	 * 
	 * @return true if the JIT was enabled, false if not
	 * @throws DataUnavailable if it is not possible to determine the JIT status
	 * @throws CorruptDataException
	 * @since 1.8
	 */
	public boolean isJITEnabled() throws DataUnavailable, CorruptDataException;
	
	/**
	 * Get any vendor specific properties in use by the JIT for this Java runtime
	 * @return the set of properties, which may be empty if none were set.
	 * @throws DataUnavailable if the JIT was not enabled for this runtime
	 * @throws CorruptDataException
	 * @since 1.8
	 */
	public Properties getJITProperties() throws DataUnavailable, CorruptDataException;

	/**
	 * Get the time when the JVM was started.
	 * 
	 * @return the time the JVM was started, in milliseconds since 1970
	 * 
	 * @throws DataUnavailable if the JVM start time is not available
	 * @throws CorruptDataException if the JVM start time is corrupted
	 * 
	 * @since 1.12
	 */
	public long getStartTime() throws DataUnavailable, CorruptDataException;
	
	/**
	 * Get the value of the JVM's high-resolution timer when the JVM was started.
	 * 
	 * @return the value of the high-resolution timer when the JVM was started, in nanoseconds
	 * 
	 * @throws DataUnavailable if the JVM start time is not available
	 * @throws CorruptDataException if the JVM start time is corrupted
	 * 
	 * @since 1.12
	 */
	public long getStartTimeNanos() throws DataUnavailable, CorruptDataException;
}
