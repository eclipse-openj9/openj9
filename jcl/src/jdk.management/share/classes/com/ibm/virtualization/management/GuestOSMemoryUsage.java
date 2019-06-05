/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2013, 2019 IBM Corp. and others
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
package com.ibm.virtualization.management;

import javax.management.openmbean.CompositeData;
import javax.management.openmbean.InvalidKeyException;

import com.ibm.virtualization.management.internal.GuestOSMemoryUsageUtil;

/**
 * This provides a snapshot of the Guest (Virtual Machine(VM)/Logical Partition(LPAR))
 * Memory usage statistics as seen by the Hypervisor Host.
 * The supported Operating System and Hypervisor combinations are:
 * <ol>
 *     <li>Linux and Windows on VMWare.
 *     <li>AIX and Linux on PowerVM.
 *     <li>Linux and z/OS on z/VM.
 *     <li>Guest Operating System memory usage statistics are not available on Linux for PowerKVM.
 * </ol>
 * @since 1.7.1
 */
public final class GuestOSMemoryUsage {

	private static final int HASHMASK = 0x0FFFFFFF;

	private long memUsed;
	private long timestamp;
	private long maxMemLimit;

	/**
	 * Creates a new {@link GuestOSMemoryUsage} instance.
	 */
	public GuestOSMemoryUsage() {
		super();
	}

	/**
	 * Creates a new {@link GuestOSMemoryUsage} instance.
	 *
	 * @param memUsed		The current used snapshot of Memory in MB or -1 if undefined.
	 * @param timestamp		The timestamp when the snapshot was taken in microseconds.
	 * @param maxMemLimit	The Max Memory limit if any set for this Guest or -1 if undefined.
	 * 
	 * @throws IllegalArgumentException if
	 * <ul>
	 *     <li>The values of cpuTime or cpuEntitlement or hostCpuClockSpeed are negative but not -1; or
	 *     <li>memUsed is greater than maxMemLimit if defined.
	 *     <li>The value of timestamp is negative. 
	 * </ul>
	 */
	private GuestOSMemoryUsage(long memUsed, long timestamp, long maxMemLimit) throws IllegalArgumentException {
		if ((memUsed < -1) || (maxMemLimit < -1) || (timestamp < 0)
				|| ((memUsed > maxMemLimit) && (maxMemLimit >= 0))) {
			throw new IllegalArgumentException();
		}
		this.memUsed = memUsed;
		this.timestamp = timestamp;
		this.maxMemLimit = maxMemLimit;
	}

	/**
	 * The total Memory used by the Guest as reported by the Hypervisor in MB.
	 *
	 * @return	Current used Memory by the Guest in MB or -1 if info not available.
	 */
	public long getMemUsed() {
		return this.memUsed;
	}

	/**
	 * The timestamp when the usage statistics were last sampled in microseconds.
	 *
	 * @return 	The last sampling timestamp in microseconds.
	 */
	public long getTimestamp() {
		return this.timestamp;
	}

	/**
	 * The amount of real Memory allowed to be used by the Guest as reported by the Hypervisor in MB.
	 *
	 * @return 	Max Memory that can be used by the Guest in MB or -1 if info not available
	 * 		or if no limit has been set.
	 */
	public long getMaxMemLimit() {
		return this.maxMemLimit;
	}

	/* (non-Javadoc)
	 * Setter method for updating Guest Memory usage parameters for this instance.
	 * 
	 * @param used	The current used snapshot of Memory in MB.
	 * @param timestamp The timestamp when the snapshot was taken in microseconds.
	 * @param limit The Max Memory limit if any set for this Guest.
	 */
	void updateValues(long used, long timestamp, long limit) {
		this.memUsed = used;
		this.timestamp = timestamp;
		this.maxMemLimit = limit;
	}

	/**
	 * Receives a {@link javax.management.openmbean.CompositeData} representing a {@link GuestOSMemoryUsage}
	 * object and attempts to return the root {@link GuestOSMemoryUsage} instance.
	 *
	 * @param cd	A {@link javax.management.openmbean.CompositeData} that represents a 
	 *                {@link GuestOSMemoryUsage}.
	 * 
	 * @return	if <code>cd</code> is non- <code>null</code>, returns a new instance of
	 * 		{@link GuestOSMemoryUsage}, If <code>cd</code>
	 * 		is <code>null</code>, returns <code>null</code>.
	 *
	 * @throws IllegalArgumentException	if argument <code>cd</code> does not correspond to a
	 * 		{@link GuestOSMemoryUsage} with the following attributes:
	 * 		<ul>
	 * 		<li><code>memUsed</code>(<code>java.lang.Long</code>)</li>
	 * 		<li><code>timestamp</code>(<code>java.lang.Long</code>)</li>
	 * 		<li><code>maxMemLimit</code>(<code>java.lang.Long</code>)</li>
	 * 		</ul>
	 */
	public static GuestOSMemoryUsage from(CompositeData cd) {
		GuestOSMemoryUsage result = null;
		if (null != cd) {
			// Is the new received CompositeData of the required type to create
			// a new ProcessorUsage?
			if (!GuestOSMemoryUsageUtil.getCompositeType().isValue(cd)) {
				/*[MSG "K05E5", "CompositeData is not of the expected type."]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05E5")); //$NON-NLS-1$
			}

			long memUsed;
			long timestamp;
			long maxMemLimit;

			try {
				memUsed = ((Long) cd.get("memUsed")).longValue(); //$NON-NLS-1$
				timestamp = ((Long) cd.get("timestamp")).longValue(); //$NON-NLS-1$
				maxMemLimit = ((Long) cd.get("maxMemLimit")).longValue(); //$NON-NLS-1$
			} catch (InvalidKeyException e) {
				/*[MSG "K05E6", "CompositeData object does not contain expected key."]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05E6")); //$NON-NLS-1$
			}

			result = new GuestOSMemoryUsage(memUsed, timestamp, maxMemLimit);
		}
		return result;
	}

	/**
	 * Text description of this {@link GuestOSMemoryUsage} object.
	 *
	 * @return text description of this {@link GuestOSMemoryUsage} object.
	 */
	@Override
	public String toString() {
		StringBuilder sb = new StringBuilder();
		sb.append("\n========== "); //$NON-NLS-1$
		sb.append(this.getClass().getSimpleName());
		sb.append(" ==========\n\n"); //$NON-NLS-1$
		sb.append("timestamp = "); //$NON-NLS-1$
		sb.append(this.timestamp);
		sb.append("\n"); //$NON-NLS-1$
		sb.append("memUsed = "); //$NON-NLS-1$
		sb.append(this.memUsed);
		sb.append("\n"); //$NON-NLS-1$
		sb.append("maxMemLimit = "); //$NON-NLS-1$
		sb.append(this.maxMemLimit);
		sb.append("\n"); //$NON-NLS-1$

		return sb.toString();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean equals(Object obj) {
		if (null == obj) {
			return false;
		}

		if (!(obj instanceof GuestOSMemoryUsage)) {
			return false;
		}

		if (this == obj) {
			return true;
		}

		GuestOSMemoryUsage gmu = (GuestOSMemoryUsage) obj;
		if (!(gmu.getMemUsed() == this.getMemUsed())) {
			return false;
		}

		if (!(gmu.getTimestamp() == this.getTimestamp())) {
			return false;
		}

		if (!(gmu.getMaxMemLimit() == this.getMaxMemLimit())) {
			return false;
		}

		return true;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public int hashCode() {
		long gmHash = this.getMemUsed()
					+ this.getTimestamp()
					+ this.getMaxMemLimit();
		return (int) ((((gmHash >> 32) + gmHash) & HASHMASK) * 23);
	}

}
