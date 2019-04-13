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

import com.ibm.virtualization.management.internal.GuestOSProcessorUsageUtil;

/**
 * This provides a snapshot of the Guest (Virtual Machine(VM)/Logical Partition(LPAR))
 * Processor usage statistics as seen by the Hypervisor Host. The statistics are an aggregate across
 * all physical CPUs assigned to the Guest by the Hypervisor Host.
 * The supported Operating System and Hypervisor combinations are:
 * <ol>
 *     <li>Linux and Windows on VMWare.
 *     <li>AIX and Linux on PowerVM.
 *     <li>Linux and z/OS on z/VM.
 *     <li>Linux on PowerKVM.
 * </ol>
 *
 * @since 1.7.1
 */
public final class GuestOSProcessorUsage {

	private static final int HASHMASK = 0x0FFFFFFF;

	private long cpuTime;
	private long timestamp;
	private float cpuEntitlement;
	private long hostCpuClockSpeed;

	/**
	 * Creates a new {@link GuestOSProcessorUsage} instance.
	 */
	public GuestOSProcessorUsage() {
		super();
	}

	/**
	 * Creates a new {@link GuestOSProcessorUsage} instance.
	 *
	 * @param cpuTime		The total used time for this Guest in microseconds or -1 if not available.
	 * @param timestamp		The timestamp when the snapshot was taken in microseconds.
	 * @param cpuEntitlement	The total Processor entitlement for this Guest or -1 if not available.
	 * @param hostCpuClockSpeed	The clock speed of the Host Processor; -1 if undefined or in platform-dependent units:<br>
	 * <ul>
	 * <li>z/OS: Millions of Service Units (MSUs).
	 * <li>Linux, AIX, and Windows: MegaHertz (MHz).
	 * </ul>
	 * @throws IllegalArgumentException if
	 * <ul>
	 * <li>The values of cpuTime or cpuEntitlement or hostCpuClockSpeed are negative but not -1; or
	 * <li>The value of timestamp is negative. 
	 * </ul>
	 */
	private GuestOSProcessorUsage(long cpuTime, long timestamp,
				      float cpuEntitlement, long hostCpuClockSpeed) throws IllegalArgumentException {
		super();
		if ((cpuTime < -1) || (cpuEntitlement < -1) || (hostCpuClockSpeed < -1) || (timestamp < 0)) {
			throw new IllegalArgumentException();
		}
		this.cpuTime = cpuTime;
		this.timestamp = timestamp;
		this.cpuEntitlement = cpuEntitlement;
		this.hostCpuClockSpeed = hostCpuClockSpeed;
	}

	/**
	 * The total used time of the Guest as reported by the Hypervisor in microseconds.
	 * <ul> 
	 *     <li>z/OS maintains CPU usage history only for the last 4 hours. The value might
	 *         not be monotonically increasing.
	 * </ul>
	 *
	 * @return	used time in microseconds or -1 if info not available.
	 */
	public long getCpuTime() {
		return this.cpuTime;
	}

	/**
	 * The the clock speed of the Host Processor in platform-dependent units.
	 * <ul>
	 *     <li>z/OS: Millions of Service Units (MSUs).
	 *     <li>Linux, AIX, and Windows: MegaHertz (MHz).
	 * </ul>
	 *
	 * @return	Host CPU clock speed or -1 if not available.
	 */
	public long getHostCpuClockSpeed() {
		return this.hostCpuClockSpeed;
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
	 * The total CPU Entitlement assigned to this Guest by the Hypervisor.
	 * The value is in Processor units and could be a fraction.
	 * <ul>
	 *     <li>VMWare: <code>CpuLimitMHz</code>, <code>CpuReservationMHz</code>, <code>CpuShares</code> and <code>HostProcessorSpeed</code>
	 * are used to arrive at the Entitlement.
	 *     <ul>
	 *         <li>See the <a href="http://www.vmware.com/support/developer/guest-sdk" target="_blank">VMware GuestSDK</a> for more on what these mean.
	 *     </ul>
	 *     <li>PowerVM: <code>entitled_proc_capacity/100</code> on AIX and <code>partition_entitled_capacity/100</code> on Linux represents the Entitlement.
	 *     <ul>
	 *         <li>See <a href="http://pic.dhe.ibm.com/infocenter/aix/v7r1/index.jsp?topic=%2Fcom.ibm.aix.prftools%2Fdoc%2Fprftools%2Fidprftools_perfstat_glob_partition.htm" target="_blank">perfstat_partition_total</a> interface on AIX.
	 *         <li>See <a href="https://www.ibm.com/developerworks/community/wikis/home?lang=en#!/wiki/W51a7ffcf4dfd_4b40_9d82_446ebc23c550/page/PowerLinux%20LPAR%20configuration%20data%20%28lparcfg%29" target="_blank">lparcfg</a> documentation on Linux.
	 *     </ul>
	 *     <li>z/VM: CPU Entitlement is unavailable.
	 * </ul>
	 *
	 * @return	CPU Entitlement assigned for this Guest or -1 if not available.
	 */
	public float getCpuEntitlement() {
		return this.cpuEntitlement;
	}

	/* (non-Javadoc)
	 * Setter method for updating Guest Processor usage parameters for this instance
	 * 
	 * @param time The total used time for this Guest in microseconds.
	 * @param timestamp The timestamp when the snapshot was taken in microseconds.
	 * @param entitlement The total Processor entitlement for this Guest.
	 * @param cpuSpeed The clock speed of the Host Processor in platform-dependent units:<br>
	 * <ul>
	 * <li>z/OS: Millions of Service Units (MSUs).
	 * <li>Linux, AIX, and Windows: MegaHertz (MHz).
	 * <ul>
	 */
	void updateValues(long time, long timestamp, float entitlement, long cpuSpeed) {
		this.cpuTime = time;
		this.timestamp = timestamp;
		this.cpuEntitlement = entitlement;
		this.hostCpuClockSpeed = cpuSpeed;
	}

	/**
	 * Receives a {@link javax.management.openmbean.CompositeData} representing a 
	 * {@link GuestOSProcessorUsage} object and attempts to return the root
	 * {@link GuestOSProcessorUsage} instance.
	 *
	 * @param cd	A {@link javax.management.openmbean.CompositeData} that represents a 
	 * 		{@link GuestOSProcessorUsage}.
	 * 
	 * @return	if <code>cd</code> is non- <code>null</code>, returns a new instance of
	 * 		{@link GuestOSProcessorUsage},
	 * 		 If <code>cd</code> is <code>null</code>, returns <code>null</code>.
	 *
	 * @throws IllegalArgumentException	if argument <code>cd</code> does not correspond to a
	 * 		{@link GuestOSProcessorUsage} with the following attributes:
	 * 		<ul>
	 * 		<li><code>cpuTime</code>(<code>java.lang.Long</code>)</li>
	 * 		<li><code>timestamp</code>(<code>java.lang.Long</code>)</li>
	 * 		<li><code>cpuEntitlement</code>(<code>java.lang.Float</code>)</li>
	 * 		<li><code>hostCpuClockSpeed</code>(<code>java.lang.Long</code>)</li>
	 * 		</ul>
	 */
	public static GuestOSProcessorUsage from(CompositeData cd) {
		GuestOSProcessorUsage result = null;

		if (null != cd) {
			// Is the new received CompositeData of the required type to create
			// a new GuestOSProcessorUsage ?
			if (!GuestOSProcessorUsageUtil.getCompositeType().isValue(cd)) {
				/*[MSG "K05E5", "CompositeData is not of the expected type."]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05E5")); //$NON-NLS-1$
			}

			long cpuTime;
			long timestamp;
			float cpuEntitlement;
			long hostCpuClockSpeed;

			try {
				cpuTime = ((Long) cd.get("cpuTime")).longValue(); //$NON-NLS-1$
				timestamp = ((Long) cd.get("timestamp")).longValue(); //$NON-NLS-1$
				cpuEntitlement = ((Float) cd.get("cpuEntitlement")).floatValue(); //$NON-NLS-1$
				hostCpuClockSpeed = ((Long) cd.get("hostCpuClockSpeed")).longValue(); //$NON-NLS-1$
			} catch (InvalidKeyException e) {
				/*[MSG "K05E6", "CompositeData object does not contain expected key."]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05E6")); //$NON-NLS-1$
			}

			result = new GuestOSProcessorUsage(cpuTime, timestamp, cpuEntitlement, hostCpuClockSpeed);
		}

		return result;
	}

	/**
	 * Text description of this {@link GuestOSProcessorUsage} object.
	 *
	 * @return Text description of this {@link GuestOSProcessorUsage} object.
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
		sb.append("cpuTime = "); //$NON-NLS-1$
		sb.append(this.cpuTime);
		sb.append("\n"); //$NON-NLS-1$
		sb.append("cpuEntitlement = "); //$NON-NLS-1$
		sb.append(this.cpuEntitlement);
		sb.append("\n"); //$NON-NLS-1$
		sb.append("hostCpuClockSpeed = "); //$NON-NLS-1$
		sb.append(this.hostCpuClockSpeed);
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

		if (!(obj instanceof GuestOSProcessorUsage)) {
			return false;
		}

		if (this == obj) {
			return true;
		}

		GuestOSProcessorUsage gpu = (GuestOSProcessorUsage) obj;
		if (!(gpu.getCpuTime() == this.getCpuTime())) {
			return false;
		}

		if (!(gpu.getTimestamp() == this.getTimestamp())) {
			return false;
		}

		if (!(gpu.getCpuEntitlement() == this.getCpuEntitlement())) {
			return false;
		}

		if (!(gpu.getHostCpuClockSpeed() == this.getHostCpuClockSpeed())) {
			return false;
		}

		return true;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public int hashCode() {
		long gpHash = this.getCpuTime() 
					+ this.getTimestamp()
					+ this.getHostCpuClockSpeed()
					+ (long) this.getCpuEntitlement();
		
		return (int) ((((gpHash >> 32) + gpHash) & HASHMASK) * 23);
	}

}
