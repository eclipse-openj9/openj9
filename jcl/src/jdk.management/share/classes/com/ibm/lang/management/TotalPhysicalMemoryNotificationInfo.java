/*[INCLUDE-IF Sidecar17]*/
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

import javax.management.openmbean.CompositeData;

import com.ibm.java.lang.management.internal.ManagementUtils;

/**
 * Encapsulates the details of a DLPAR notification emitted by a
 * {@link com.ibm.lang.management.OperatingSystemMXBean} when the total
 * physical memory changes.
 * Specifically, this notification indicates that the value returned by
 * {@link com.ibm.lang.management.OperatingSystemMXBean#getTotalPhysicalMemorySize()}
 * has changed.
 */
public class TotalPhysicalMemoryNotificationInfo {

	public static final String TOTAL_PHYSICAL_MEMORY_CHANGE = "com.ibm.management.total.physical.memory.change"; //$NON-NLS-1$

	private long newTotalPhysicalMemory;

	/**
	 * Constructs a new instance of this object.
	 * 
	 * @param newTotalPhysicalMemory
	 *            the new total bytes of physical memory
	 */
	public TotalPhysicalMemoryNotificationInfo(long newTotalPhysicalMemory) {
		super();
		this.newTotalPhysicalMemory = newTotalPhysicalMemory;
	}

	/**
	 * Returns the new value of bytes for the total physical memory after
	 * the change that this notification corresponds to.
	 * @return the new physical memory total in bytes
	 */
	public long getNewTotalPhysicalMemory() {
		return this.newTotalPhysicalMemory;
    }

    /**
     * Receives a {@link CompositeData} representing a
     * <code>TotalPhysicalMemoryNotificationInfo</code> object and attempts to
     * return the root <code>TotalPhysicalMemoryNotificationInfo</code>
     * instance.
     * 
     * @param cd
     *            a <code>CompositeDate</code> that represents a
     *            <code>TotalPhysicalMemoryNotificationInfo</code>.
     * @return if <code>cd</code> is non- <code>null</code>, returns a new
     *         instance of <code>TotalPhysicalMemoryNotificationInfo</code>.
     *         If <code>cd</code> is <code>null</code>, returns
     *         <code>null</code>.
     * @throws IllegalArgumentException
     *             if argument <code>cd</code> does not correspond to a
     *             <code>TotalPhysicalMemoryNotificationInfo</code> with the
     *             following attribute:
     *             <ul>
     *             <li><code>newTotalPhysicalMemory</code>( <code>java.lang.Long</code>)
     *             </ul>
     */
	public static TotalPhysicalMemoryNotificationInfo from(CompositeData cd) {
		TotalPhysicalMemoryNotificationInfo result = null;

		if (cd != null) {
			// Does cd meet the necessary criteria to create a new
			// TotalPhysicalMemoryNotificationInfo ? If not then one of the
			// following method invocations will exit on an
			// IllegalArgumentException...
			ManagementUtils.verifyFieldNumber(cd, 1);
			String[] attributeNames = { "newTotalPhysicalMemory" }; //$NON-NLS-1$ 
			ManagementUtils.verifyFieldNames(cd, attributeNames);
			String[] attributeTypes = { "java.lang.Long" }; //$NON-NLS-1$
			ManagementUtils.verifyFieldTypes(cd, attributeNames, attributeTypes);

			// Extract the values of the attributes and use them to construct
			// a new TotalPhysicalMemoryNotificationInfo.
			Object[] attributeVals = cd.getAll(attributeNames);
			long memoryVal = ((Long) attributeVals[0]).longValue();

			result = new TotalPhysicalMemoryNotificationInfo(memoryVal);
		}

		return result;
	}
}
