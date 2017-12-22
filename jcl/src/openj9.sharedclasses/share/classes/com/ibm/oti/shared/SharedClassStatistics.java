/*[INCLUDE-IF SharedClasses]*/
package com.ibm.oti.shared;

/*******************************************************************************
 * Copyright (c) 1998, 2017 IBM Corp. and others
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

/**
 * SharedClassStatistics provides static functions that report basic cache statistics.
 * <p>
 */
public class SharedClassStatistics {

	/**
	 * Returns the size of the shared cache that the JVM is currently connected to
	 * <p>
	 * @return The total size in bytes
	 */
	public static long maxSizeBytes() {
		return maxSizeBytesImpl();
	}
	
	/**
	 * Returns the free space of the shared cache that the JVM is currently connected to
	 * <p>
	 * @return The free space in bytes
	 */
	public static long freeSpaceBytes() {
		return freeSpaceBytesImpl();
	}
	
	/**
	 * Returns the soft limit in bytes for the available space of the cache that the JVM is currently connected to
	 * <p>
	 * 
	 * @return the soft max size or cache size in bytes if it is not set.
	 */
	public static long softmxBytes() {
		return softmxBytesImpl();
	}
	
	/**
     * Returns the minimum space reserved for AOT data of the cache that the JVM is currently
     * connected to.
     * 
     * @return the minimum shared classes cache space reserved for AOT data in bytes or -1 if it is not set.
     */
    public static long minAotBytes() {
    	return minAotBytesImpl();
    }
    
    /**
     * Returns the maximum space allowed for AOT data of the cache that the JVM is currently
     * connected to.
     * 
     * @return the maximum shared classes cache space allowed for AOT data in bytes or -1 if it is not set.
     */
    public static long maxAotBytes() {
    	return maxAotBytesImpl();
    }
    
    /**
     * Returns the minimum space reserved for JIT data of the cache that the JVM is currently
     * connected to.
     * 
     * @return the minimum shared classes cache space reserved for JIT data in bytes or -1 if it is not set.
     */
    public static long minJitDataBytes() {
    	return minJitDataBytesImpl();
    }
    
    /**
     * Returns the maximum space allowed for JIT data of the cache that the JVM is currently
     * connected to.
     * 
     * @return the maximum shared classes cache space allowed for JIT data in bytes  or -1 if it is not set.
     */
    public static long maxJitDataBytes() {
    	return maxJitDataBytesImpl();
    }

	
	private static native long maxSizeBytesImpl();

	private static native long freeSpaceBytesImpl();
	
	private static native long softmxBytesImpl();
	
	private static native long minAotBytesImpl();
	
	private static native long maxAotBytesImpl();
	
	private static native long minJitDataBytesImpl();
	
	private static native long maxJitDataBytesImpl();
}
