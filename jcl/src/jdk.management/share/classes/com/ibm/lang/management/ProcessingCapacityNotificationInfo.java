/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2005, 2019 IBM Corp. and others
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
 * {@link com.ibm.lang.management.OperatingSystemMXBean} when the available
 * processing capacity changes.
 * Specifically, this notification indicates that the value returned by
 * {@link com.ibm.lang.management.OperatingSystemMXBean#getProcessingCapacity()}
 * has changed.
 *
 * @since 1.5
 */
public class ProcessingCapacityNotificationInfo {

	public static final String PROCESSING_CAPACITY_CHANGE = "com.ibm.management.processing.capacity.change"; //$NON-NLS-1$

	private final int newProcessingCapacity;

    /**
     * Constructs a new instance of this object.
     * 
     * @param newProcessingCapacity
     *            the new processing capacity in units of 1% of a physical
     *            processor's capacity
     */
	public ProcessingCapacityNotificationInfo(int newProcessingCapacity) {
		super();
		this.newProcessingCapacity = newProcessingCapacity;
    }

    /**
     * Returns the new processing capacity after the change that this
     * notification corresponds to.
     * 
     * @return the new processing capacity in units of 1% of a physical
     *         processor's capacity.
     */
    public int getNewProcessingCapacity() {
        return this.newProcessingCapacity;
    }

    /**
     * Receives a {@link CompositeData} representing a
     * <code>ProcessingCapacityNotificationInfo</code> object and attempts to
     * return the root <code>ProcessingCapacityNotificationInfo</code>
     * instance.
     * 
     * @param cd
     *            a <code>CompositeDate</code> that represents a
     *            <code>ProcessingCapacityNotificationInfo</code>.
     * @return if <code>cd</code> is non- <code>null</code>, returns a new
     *         instance of <code>ProcessingCapacityNotificationInfo</code>.
     *         If <code>cd</code> is <code>null</code>, returns
     *         <code>null</code>.
     * @throws IllegalArgumentException
     *             if argument <code>cd</code> does not correspond to a
     *             <code>ProcessingCapacityNotificationInfo</code> with the
     *             following attribute:
     *             <ul>
     *             <li><code>newProcessingCapacity</code>(
     *             <code>java.lang.Integer</code>)
     *             </ul>
     */
	public static ProcessingCapacityNotificationInfo from(CompositeData cd) {
		ProcessingCapacityNotificationInfo result = null;

		if (cd != null) {
			// Does cd meet the necessary criteria to create a new
			// ProcessingCapacityNotificationInfo ? If not then one of the
			// following method invocations will exit on an
			// IllegalArgumentException...
			ManagementUtils.verifyFieldNumber(cd, 1);
			String[] attributeNames = { "newProcessingCapacity" }; //$NON-NLS-1$ 
			ManagementUtils.verifyFieldNames(cd, attributeNames);
			String[] attributeTypes = { "java.lang.Integer" }; //$NON-NLS-1$
			ManagementUtils.verifyFieldTypes(cd, attributeNames, attributeTypes);

			// Extract the values of the attributes and use them to construct
			// a new ProcessingCapacityNotificationInfo.
			Object[] attributeVals = cd.getAll(attributeNames);
			int capacityVal = ((Integer) attributeVals[0]).intValue();
			result = new ProcessingCapacityNotificationInfo(capacityVal);
		}

		return result;
	}

}
