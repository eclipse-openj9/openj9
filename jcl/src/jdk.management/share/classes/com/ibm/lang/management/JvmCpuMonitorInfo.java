/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2014, 2019 IBM Corp. and others
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
import javax.management.openmbean.InvalidKeyException;

import com.ibm.lang.management.internal.JvmCpuMonitorInfoUtil;

/**
 * <code>JvmCpuMonitorInfo</code> provides a snapshot of JVM CPU usage information
 * that is distributed across thread categories. A time stamp for the snapshot is
 * also provided.
 *
 * @see JvmCpuMonitorMXBean For more information on thread categories.
 */
public final class JvmCpuMonitorInfo {

	private static final int HASHMASK = 0x0FFFFFFF;
	private static final int NUM_USER_DEFINED_CATEGORY = 5;

	private long timestamp;
	private long applicationCpuTime;
	private long resourceMonitorCpuTime;
	private long systemJvmCpuTime;
	private long gcCpuTime;
	private long jitCpuTime;
	private final long applicationUserCpuTime[] = new long[NUM_USER_DEFINED_CATEGORY];

	/**
	 * Creates a new <code>JvmCpuMonitorInfo</code> instance.
	 */
	public JvmCpuMonitorInfo() {
		super();
	}

	/**
	 * Create a new <code>JvmCpuMonitorInfo</code> instance with the given info.
	 * 
	 * @param timestamp
	 * @param applicationCpuTime
	 * @param resourceMonitorCpuTime
	 * @param systemJvmCpuTime
	 * @param gcCpuTime
	 * @param jitCpuTime
	 * @param applicationUserCpuTime
	 * @throws IllegalArgumentException
	 */
	private JvmCpuMonitorInfo(long timestamp, long applicationCpuTime, long resourceMonitorCpuTime,
							  long systemJvmCpuTime, long gcCpuTime, long jitCpuTime, long applicationUserCpuTime[]) throws IllegalArgumentException {
		super();
		if (timestamp < 0 || applicationCpuTime < 0 || resourceMonitorCpuTime < 0
				|| systemJvmCpuTime < 0 || gcCpuTime < 0 || jitCpuTime < 0) {
			throw new IllegalArgumentException();
		}
		this.timestamp = timestamp;
		this.applicationCpuTime = applicationCpuTime;
		this.resourceMonitorCpuTime = resourceMonitorCpuTime;
		this.systemJvmCpuTime = systemJvmCpuTime;
		this.gcCpuTime = gcCpuTime;
		this.jitCpuTime = jitCpuTime;
		System.arraycopy(applicationUserCpuTime, 0, this.applicationUserCpuTime, 0, applicationUserCpuTime.length);
	}

	/**
	 * This method returns the last sampling time stamp.
	 *
	 * @return 	The last sampling time stamp in microseconds.
	 */
	public long getTimestamp() {
		return this.timestamp;
	}

	/**
	 * This method returns the total CPU usage for all application threads.
	 * It includes the threads that are in the "Application" category as well as those in the user defined categories (Application-UserX).  
	 * This does not include information about "Resource-monitor" threads.
	 * 
	 * @return	"Application" category CPU usage time in microseconds.
	 */
	public long getApplicationCpuTime() {
		return this.applicationCpuTime;
	}

	/**
	 * This method returns the total CPU usage for all threads of the "Resource-Monitor" category.
	 * "Resource-Monitor" thread is any thread, that the application has designated as a "Resource-Monitor". 
	 *
	 * @return	"Resource-Monitor" category CPU usage time in microseconds.
	 */
	public long getResourceMonitorCpuTime() {
		return this.resourceMonitorCpuTime;
	}

	/**
	 * This method returns the total CPU usage of the "System-JVM" category, which
	 * includes GC, JIT and other JVM daemon threads. The data also includes usage
	 * of any thread that has participated in "GC" activity for the duration spent
	 * in that activity if the -XX:-ReduceCPUMonitorOverhead option is specified.
	 * See the user guide for more information on this option.
	 *
	 * @return	"System-JVM" category CPU usage time in microseconds.
	 */
	public long getSystemJvmCpuTime() {
		return this.systemJvmCpuTime;
	}

	/**
	 * This method returns the total CPU usage of all GC threads.
	 * It also includes CPU usage by non GC threads that participate in GC activity for the period
	 * that they do GC work if the -XX:-ReduceCPUMonitorOverhead option is specified.
	 * See the user guide for more information on this option.
	 * -XX:-ReduceCPUMonitorOverhead is not supported on z/OS.
	 *
	 * @return	"GC" category CPU usage time in microseconds.
	 */
	public long getGcCpuTime() {
		return this.gcCpuTime;
	}

	/**
	 * This method returns the total CPU usage of all JIT Threads.
	 *
	 * @return	"JIT" category CPU usage time in microseconds.
	 */
	public long getJitCpuTime() {
		return this.jitCpuTime;
	}

	/**
	 * This method returns an array of CPU usage for all user defined thread categories.
	 *
	 * @return Array of user defined thread categories' CPU usage time in microseconds.
	 */
	public long[] getApplicationUserCpuTime() {
		long[] applicationUserCpuTimeData = this.applicationUserCpuTime.clone();
		return applicationUserCpuTimeData;
	}

	/* (non-Javadoc)
	 * Setter method for updating JVM CPU usage parameters for this instance.
	 * 
	 * @param tstamp The time stamp when the snapshot was taken in microseconds.
	 * @param appTime Total CPU usage of Application Threads.
	 * @param resourceMonitorTime Total CPU usage of resource monitor Thread(s).
	 * @param sysJvmTime Total CPU usage of System-JVM Threads.
	 * @param gcTime Total CPU usage for GC activity.
	 * @param jitTime Total CPU usage of JIT Threads.
	 * @param appUserTime[] Array of CPU usage of user defined Application Threads. 
	 */
	void updateValues(long tstamp, long appTime, long rmonTime,
					  long sysTime, long gcTime, long jitTime, long appUserTime[]) {
		this.timestamp = tstamp;
		this.applicationCpuTime = appTime;
		this.resourceMonitorCpuTime = rmonTime;
		this.systemJvmCpuTime = sysTime;
		this.gcCpuTime = gcTime;
		this.jitCpuTime = jitTime;
		System.arraycopy(appUserTime, 0, this.applicationUserCpuTime, 0, appUserTime.length);
	}

	/**
	 * Receives a {@link javax.management.openmbean.CompositeData} representing a 
	 * {@link JvmCpuMonitorInfo} object and attempts to return the root
	 * {@link JvmCpuMonitorInfo} instance.
	 *
	 * @param cd	A {@link javax.management.openmbean.CompositeData} that represents a 
	 * 		{@link JvmCpuMonitorInfo}.
	 * 
	 * @return	if <code>cd</code> is non- <code>null</code>, returns a new instance of
	 * 		{@link JvmCpuMonitorInfo},
	 * 		 If <code>cd</code> is <code>null</code>, returns <code>null</code>.
	 *
	 * @throws IllegalArgumentException	if argument <code>cd</code> does not correspond to a
	 * 		{@link JvmCpuMonitorInfo} with the following attributes:
	 * 		<ul>
	 *		<li><code>timestamp</code>(<code>java.lang.Long</code>)</li>
	 *		<li><code>applicationCpuTime</code>(<code>java.lang.Long</code>)</li>
	 *		<li><code>resourceMonitorCpuTime</code>(<code>java.lang.Long</code>)</li>
	 *		<li><code>systemJvmCpuTime</code>(<code>java.lang.Long</code>)</li>
	 *		<li><code>gcCpuTime</code>(<code>java.lang.Long</code>)</li>
	 *		<li><code>jitCpuTime</code>(<code>java.lang.Long</code>)</li>
	 *		<li><code>applicationUserCpuTime</code></li>
	 * 		</ul>
	 */
	public static JvmCpuMonitorInfo from(CompositeData cd) {
		JvmCpuMonitorInfo result = null;

		if (null != cd) {
			// Is the new received CompositeData of the required type to create
			// a new JvmCpuMonitorInfo ?
			if (!JvmCpuMonitorInfoUtil.getCompositeType().isValue(cd)) {
				/*[MSG "K05E5", "CompositeData is not of the expected type."]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05E5")); //$NON-NLS-1$
			}

			long timestamp;
			long applicationCpuTime;
			long resourceMonitorCpuTime;
			long systemJvmCpuTime;
			long gcCpuTime;
			long jitCpuTime;
			long[] applicationUserCpuTime;

			try {
				timestamp = ((Long) cd.get("timestamp")).longValue(); //$NON-NLS-1$
				applicationCpuTime = ((Long) cd.get("applicationCpuTime")).longValue(); //$NON-NLS-1$
				resourceMonitorCpuTime = ((Long) cd.get("resourceMonitorCpuTime")).longValue(); //$NON-NLS-1$
				systemJvmCpuTime = ((Long) cd.get("systemJvmCpuTime")).longValue(); //$NON-NLS-1$
				gcCpuTime = ((Long) cd.get("gcCpuTime")).longValue(); //$NON-NLS-1$
				jitCpuTime = ((Long) cd.get("jitCpuTime")).longValue(); //$NON-NLS-1$
				applicationUserCpuTime = (long[]) cd.get("applicationUserCpuTime"); //$NON-NLS-1$
			} catch (InvalidKeyException e) {
				/*[MSG "K05E6", "CompositeData object does not contain expected key."]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05E6")); //$NON-NLS-1$
			}

			result = new JvmCpuMonitorInfo(timestamp, applicationCpuTime, resourceMonitorCpuTime,
										   systemJvmCpuTime, gcCpuTime, jitCpuTime, applicationUserCpuTime);
		}

		return result;
	}

	/**
	 * Text description of this {@link JvmCpuMonitorInfo} object.
	 *
	 * @return Text description of this {@link JvmCpuMonitorInfo} object.
	 */
	@Override
	public String toString() {
		StringBuilder sb = new StringBuilder();
		sb.append("\n========== "); //$NON-NLS-1$
		sb.append(this.getClass().getSimpleName());
		sb.append(" ==========\n\n"); //$NON-NLS-1$
		sb.append("             timestamp = "); //$NON-NLS-1$
		sb.append(this.timestamp);
		sb.append("\n"); //$NON-NLS-1$
		sb.append("      systemJvmCpuTime = "); //$NON-NLS-1$
		sb.append(this.systemJvmCpuTime);
		sb.append("\n"); //$NON-NLS-1$
		sb.append("             gcCpuTime = "); //$NON-NLS-1$
		sb.append(this.gcCpuTime);
		sb.append("\n"); //$NON-NLS-1$
		sb.append("            jitCpuTime = "); //$NON-NLS-1$
		sb.append(this.jitCpuTime);
		sb.append("\n"); //$NON-NLS-1$
		sb.append("resourceMonitorCpuTime = "); //$NON-NLS-1$
		sb.append(this.resourceMonitorCpuTime);
		sb.append("\n"); //$NON-NLS-1$
		sb.append("    applicationCpuTime = "); //$NON-NLS-1$
		sb.append(this.applicationCpuTime);
		sb.append("\n"); //$NON-NLS-1$
		for (int i = 0; i < this.applicationUserCpuTime.length; ++i) {
			sb.append("      applicationUser"); //$NON-NLS-1$
			sb.append((i + 1));
			sb.append(" = "); //$NON-NLS-1$
			sb.append(this.applicationUserCpuTime[i]);
			sb.append("\n"); //$NON-NLS-1$
		}
		return sb.toString();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean equals(Object obj) {
		if (this == obj) {
			return true;
		}

		if (null == obj) {
			return false;
		}

		if (!(obj instanceof JvmCpuMonitorInfo)) {
			return false;
		}

		JvmCpuMonitorInfo jcmInfo = (JvmCpuMonitorInfo) obj;

		if (jcmInfo.getTimestamp() != this.getTimestamp()) {
			return false;
		}

		if (jcmInfo.getApplicationCpuTime() != this.getApplicationCpuTime()) {
			return false;
		}

		if (jcmInfo.getResourceMonitorCpuTime() != this.getResourceMonitorCpuTime()) {
			return false;
		}

		if (jcmInfo.getSystemJvmCpuTime() != this.getSystemJvmCpuTime()) {
			return false;
		}

		if (jcmInfo.getGcCpuTime() != this.getGcCpuTime()) {
			return false;
		}

		if (jcmInfo.getJitCpuTime() != this.getJitCpuTime()) {
			return false;
		}

		long[] applicationUserCpuTimeData = jcmInfo.getApplicationUserCpuTime();

		for (int i = 0; i < applicationUserCpuTimeData.length; ++i) {
			if (applicationUserCpuTimeData[i] != this.applicationUserCpuTime[i]) {
				return false;
			}
		}

		return true;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public int hashCode() {
		long uHash = this.getTimestamp()
					+ this.getApplicationCpuTime()
					+ this.getResourceMonitorCpuTime()
					+ this.getSystemJvmCpuTime()
					+ this.getGcCpuTime()
					+ this.getJitCpuTime();

		for (int i = 0; i < this.applicationUserCpuTime.length; ++i) {
			uHash += this.applicationUserCpuTime[i];
		}

		return (int) ((((uHash >> 32) + uHash) & HASHMASK) * 23);
	}

}
