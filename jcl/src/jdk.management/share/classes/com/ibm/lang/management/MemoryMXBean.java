/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2005, 2016 IBM Corp. and others
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

package com.ibm.lang.management;

/**
 * The IBM-specific interface for monitoring the virtual machine's memory
 * management system.
 * 
 * @since 1.5
 */
public interface MemoryMXBean extends java.lang.management.MemoryMXBean {

	/**
	 * Get the maximum size in bytes to which the max heap size could be
	 * increased in the currently running VM. This may be larger than the
	 * current max heap size.
	 * 
	 * @return value of -Xmx in bytes
	 */
	public long getMaxHeapSizeLimit();
	
	/**
	 * Get the current maximum heap size in bytes.
	 * 
	 * @return current value of -Xsoftmx in bytes
	 */
	public long getMaxHeapSize();
	
	/**
	 * Get the minimum heap size in bytes.
	 * 
	 * @return value of -Xms in bytes
	 */
	public long getMinHeapSize();
	
	/**
	 * Set the current maximum heap size to <code>size</code>.
	 * The parameter specifies the max heap size in bytes and must be
	 * between getMinHeapSize() and getMaxHeapSizeLimit().
	 * See -Xsoftmx in the command line reference for additional
	 * details on the effect of setting softmx. 
	 * 
	 * @param size new -Xsoftmx value in bytes
     * @throws UnsupportedOperationException
     *             if this operation is not supported.
     * @throws IllegalArgumentException
     *             if input value <code>size</code> is either less than
     *             getMinHeapSize() or greater than getMaxHeapSizeLimit().
     * @throws SecurityException
     *             if a {@link SecurityManager} is being used and the caller
     *             does not have the <code>ManagementPermission</code> value
     *             of "control".
	 */
	public void setMaxHeapSize( long size );
	
	/**
	 * Query whether the VM supports runtime reconfiguration of the
	 * maximum heap size through the setMaxHeapSize() call.
	 * 
	 * @return true if setMaxHeapSize is supported, false otherwise
	 */
	public boolean isSetMaxHeapSizeSupported();
    
    /**
     * Returns the total size in bytes of the cache that the JVM is currently
     * connected to.
     * 
     * @return the number of bytes in the shared class cache.
     */
    public long getSharedClassCacheSize();
    
    /**
     * Returns the softmx size in bytes of the cache that the JVM is currently
     * connected to.
     * 
     * @return the softmx bytes in the shared class cache or cache size if it is not set.
     */
    public long getSharedClassCacheSoftmxBytes();
    
    /**
     * Returns the minimum space reserved for AOT data of the cache that the JVM is currently
     * connected to.
     * 
     * @return the minimum shared classes cache space reserved for AOT data in bytes or -1 if it is not set.
     */
    public long getSharedClassCacheMinAotBytes();
    
    /**
     * Returns the maximum space allowed for AOT data of the cache that the JVM is currently
     * connected to.
     * 
     * @return the maximum shared classes cache space allowed for AOT data or -1 if it is not set.
     */
    public long getSharedClassCacheMaxAotBytes();
    
    /**
     * Returns the minimum space reserved for JIT data of the cache that the JVM is currently
     * connected to.
     * 
     * @return the minimum shared classes cache space reserved for JIT data or -1 if it is not set.
     */
    public long getSharedClassCacheMinJitDataBytes();
    
    /**
     * Returns the maximum space allowed for JIT data of the cache that the JVM is currently
     * connected to.
     * 
     * @return the maximum shared classes cache space allowed for JIT data or -1 if it is not set.
     */
    public long getSharedClassCacheMaxJitDataBytes();
    
    /**
	 * Set the shared class softmx size to <code>value</code>.
	 * The parameter specifies the softmx in bytes.
	 * See -Xscmx in the command line reference for additional
	 * details on the effect of setting shared class softmx. 
	 * 
	 * @param value new shared cache soft max value in bytes
	 * @return whether the requested operation has been completed.
     * @throws IllegalArgumentException
     *             if input value <code>value</code> is less than 0.
     * @throws SecurityException
     *             if a {@link SecurityManager} is being used and the caller
     *             does not have the <code>ManagementPermission</code> value
     *             of "control".
	 */
    public boolean setSharedClassCacheSoftmxBytes(long value);
    
    /**
	 * Set the minimum shared classes cache space reserved for AOT data to <code>value</code> bytes.
	 * See -Xscminaot in the command line reference for additional
	 * details on the effect of setting shared class -Xscminaot. 
	 * 
	 * @param value new -Xscminaot value in bytes
	 * @return whether the requested operation has been completed.
     * @throws IllegalArgumentException
     *             if input value <code>value</code> is less than 0.
     * @throws SecurityException
     *             if a {@link SecurityManager} is being used and the caller
     *             does not have the <code>ManagementPermission</code> value
     *             of "control".
	 */
    public boolean setSharedClassCacheMinAotBytes(long value);
    
    /**
	 * Set the maximum shared classes cache space allowed for AOT data to <code>value</code> bytes.
	 * See -Xscmaxaot in the command line reference for additional
	 * details on the effect of setting shared class -Xscmaxaot. 
	 * 
	 * @param value new -Xscmaxaot value in bytes
	 * @return whether the requested operation has been completed.
     * @throws IllegalArgumentException
     *             if input value <code>value</code> is less than 0.
     * @throws SecurityException
     *             if a {@link SecurityManager} is being used and the caller
     *             does not have the <code>ManagementPermission</code> value
     *             of "control".
	 */
    public boolean setSharedClassCacheMaxAotBytes(long value);
    
    /**
	 * Set the minimum shared classes cache space reserved for JIT data to <code>value</code> bytes.
	 * See -Xscminjitdata in the command line reference for additional
	 * details on the effect of setting shared class -Xscminjitdata. 
	 * 
	 * @param value new -Xscminjitdata value in bytes
	 * @return whether the requested operation has been completed.
     * @throws IllegalArgumentException
     *             if input value <code>value</code> is less than 0.
     * @throws SecurityException
     *             if a {@link SecurityManager} is being used and the caller
     *             does not have the <code>ManagementPermission</code> value
     *             of "control".
	 */
    public boolean setSharedClassCacheMinJitDataBytes(long value);
    
    /**
	 * Set the maximum shared classes cache space allowed for JIT data to <code>value</code> bytes.
	 * See -Xscmaxjitdata in the command line reference for additional
	 * details on the effect of setting shared class -Xscmaxjitdata. 
	 * 
	 * @param value new -Xscmaxjitdata value in bytes
	 * @return whether the requested operation has been completed.
     * @throws IllegalArgumentException
     *             if input value <code>value</code> is less than 0.
     * @throws SecurityException
     *             if a {@link SecurityManager} is being used and the caller
     *             does not have the <code>ManagementPermission</code> value
     *             of "control".
	 */
    public boolean setSharedClassCacheMaxJitDataBytes(long value);
    
    /**
     * Returns the bytes which are not stored into the shared classes cache due to the current setting of softmx in shared classes.
     * 
     * @return the unstored bytes.
     */
    public long getSharedClassCacheSoftmxUnstoredBytes();
    
    /**
     * Returns the bytes which are not stored into the shared classes cache due to the current setting of maximum space allowed for AOT data.
     * 
     * @return the unstored bytes.
     */
    public long getSharedClassCacheMaxAotUnstoredBytes();
    
    /**
     * Returns the bytes which are not stored into the shared classes cache due to the current setting of maximum space allowed for JIT data.
     * 
     * @return the unstored bytes.
     */
    public long getSharedClassCacheMaxJitDataUnstoredBytes();
    
    /**
     * Returns the <b>free space</b> in bytes of the cache that the JVM is
     * currently connected to.
     * 
     * @return the number of bytes free in the shared class cache.
     */
    public long getSharedClassCacheFreeSpace();
	
	/**
	 * Returns the current GC mode as a human-readable string.  
	 * 
	 * @return a String describing the mode the GC is currently operating in
	 */
	public String getGCMode();

	/**
     * Returns the amount of CPU time spent in the GC by the master thread, in milliseconds.
     * 
     * @return CPU time used in milliseconds
     */
	public long getGCMasterThreadCpuUsed();

	/**
     * Returns the total amount of CPU time spent in the GC by all slave threads, in milliseconds.
     * 
     * @return CPU time used in milliseconds
     */
	public long getGCSlaveThreadsCpuUsed();

	/**
     * Returns the maximum number of GC worker threads.
     * 
     * @return maximum number of GC worker threads
     */
	public int getMaximumGCThreads();

	/**
     * Returns the number of GC worker threads that participated in the most recent collection.
     * 
     * @return number of active GC worker threads
     */
	public int getCurrentGCThreads();
}
