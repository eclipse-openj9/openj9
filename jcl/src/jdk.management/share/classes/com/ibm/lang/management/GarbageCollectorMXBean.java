/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2005, 2017 IBM Corp. and others
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
 * The IBM-specific interface for managing and monitoring the virtual machine's
 * garbage collection functionality.
 *
 * @since 1.5
 */
public interface GarbageCollectorMXBean extends
			com.sun.management.GarbageCollectorMXBean {

    /**
     * Returns the start time <em>in milliseconds</em> of the last garbage
     * collection that was carried out by this collector.
     * 
     * @return the start time of the most recent collection
     */
    public long getLastCollectionStartTime();

    /**
     * Returns the end time <em>in milliseconds</em> of the last garbage
     * collection that was carried out by this collector.
     * 
     * @return the end time of the most recent collection
     */
    public long getLastCollectionEndTime();

    /**
     * Returns the amount of heap memory used by objects that are managed
     * by the collector corresponding to this bean object.
     * 
     * @return memory used in bytes
     */
    public long getMemoryUsed();

    /**
     * Returns the cumulative total amount of memory freed, in bytes, by the
     * garbage collector corresponding to this bean object.
     * 
     * @return memory freed in bytes
     */
    public long getTotalMemoryFreed();

    /**
     * Returns the cumulative total number of compacts that was performed by
     * garbage collector corresponding to this bean object.
     * 
     * @return number of compacts performed
     */
    public long getTotalCompacts();
}
