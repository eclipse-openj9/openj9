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
package com.ibm.lang.management;

import javax.management.openmbean.CompositeData;
import javax.management.openmbean.InvalidKeyException;

import com.ibm.lang.management.internal.ProcessorUsageUtil;

/**
 * This represents a snapshot of the Operating System level Processor usage statistics.
 * The statistics could be at an individual Processor level or an aggregate across all
 * Processors. The statistics are available on AIX, Linux and Windows.
 * <ul>
 *     <li>This information is not available on z/OS.
 * </ul>
 * @author sonchakr, dgunigun
 * @since 1.7.1
 */
public class ProcessorUsage {

	private static final int HASHMASK = 0x0FFFFFFF;

	private long user;
	private long system;
	private long idle;
	private long wait;
	private long busy;
	private int id;
	private int online;
	private long timestamp;

	/**
	 * Creates a new {@link ProcessorUsage} instance.
	 */
	public ProcessorUsage() {
		super();
	}

	/**
	 * Creates a new {@link ProcessorUsage} instance.
	 *
	 * @param user		The time spent in user mode in microseconds or -1 if not available.
	 * @param system	The time spent in system mode in microseconds or -1 if not available.
	 * @param idle		The time spent by the Processor idling in microseconds or -1 if not available.
	 * @param wait		The time spent in Input/Output (IO) wait in microseconds or -1 if not available.
	 * @param busy		The time spent by the Processor executing a non-idle thread in microseconds or -1 if not available.
	 * @param id		An identifier for this Processor or -1 for the global record or if not available.
	 * @param online	This Processor's online status (0 = offline, 1 = online)
	 *         			or -1 for the global record or if not available.
	 * @param timestamp The timestamp when usage statistics were last sampled in microseconds.
	 * 
	 * @throws IllegalArgumentException if
	 * <ul><li>The values of user or system or idle or wait or busy or id or online are negative but not -1; or
	 * <li>The value of timestamp is negative; or
	 * <li>The value of online is anything other than 0 or 1 or -1; or
	 * <li>The sum of values of user, system and wait are greater than busy if defined.  
	 * </ul>
	 */
	private ProcessorUsage(long user, long system, long idle, long wait,
			       long busy, int id, int online, long timestamp) throws IllegalArgumentException {
		super();
		if ((user < -1) ||
		   (system < -1) ||
		   (idle < -1) ||
		   (wait < -1) ||
		   (busy < -1) ||
		   (id < -1) ||
		   (online < 0) || (online > 1) ||
		   (timestamp < 0) ||
		   ((user >= 0) && (system >= 0) && (wait >= 0) && (busy >= 0) && (busy < (user + system + wait))))
		{
			throw new IllegalArgumentException();
		}
		this.user = user;
		this.system = system;
		this.idle = idle;
		this.wait = wait;
		this.busy = busy;
		this.id = id;
		this.online = online;
		this.timestamp = timestamp;
	}

	/**
	 * The time spent in user mode in microseconds.
	 * 
	 * @return The user times in microseconds or -1 if not available.
	 */
	public long getUser() {
		return this.user;
	}

	/**
	 * The time spent in system mode in microseconds.
	 * 
	 * @return The system times in microseconds or -1 if not available.
	 */
	public long getSystem() {
		return this.system;
	}

	/**
	 * The time spent by the Processor sitting idle in microseconds.
	 * 
	 * @return The idle times in microseconds or -1 if not available.
	 */
	public long getIdle() {
		return this.idle;
	}

	/**
	 * The time spent by the Processor in Input/Output (IO) wait in microseconds.
	 * 
	 * @return The wait times in microseconds or -1 if not available.
	 */
	public long getWait() {
		return this.wait;
	}

	/**
	 * The time spent by the Processor executing a non-idle thread in microseconds.
	 * 
	 * @return The busy times in microseconds or -1 if not available.
	 */
	public long getBusy() {
		return this.busy;
	}

	/**
	 * A unique identifier assigned to the this Processor. This is -1 if method
	 * invoked on the global Processor record.
	 * 
	 * @return An identifier for this Processor or -1 for the global record or if not available.
	 */
	public int getId() {
		return this.id;
	}

	/**
	 * The online/offline status of this Processor. This is -1 if method
	 * invoked on the global Processor record or if this info is unavailable.
	 * <ul>
	 * <li>On AIX the online value is not reliable. Once a Processor becomes online,
	 * the status of the Processor will always be shown as online from that point onwards,
	 * even if it is currently offline. This is a limitation of the OS. Similarly offline
	 * on AIX means that the Processor was never online.
	 * </ul>
	 * 
	 * @return This Processor's online status (0 = offline, 1 = online)
	 *         or -1 for the global record or if not available.
	 */
	public int getOnline() {
		return this.online;
	}

	/**
	 * The timestamp when usage statistics were last sampled in microseconds.
	 * 
	 * @return Timestamp in microseconds.
	 */
	public long getTimestamp() {
		return this.timestamp;
	}

	/**
	 * Setter method for updating the Processor usage parameters into fields 
	 * of a {@link ProcessorUsage} instance.
	 * 
	 * @param user Time spent in User mode in microseconds.
	 * @param system Time spent in System (or kernel or privileged) mode in microseconds.
	 * @param idle Time spent in Idle mode in microseconds.
	 * @param wait Time spent doing IO Wait in microseconds.
	 * @param busy Time spent in Busy mode in microseconds.
	 * @param id This Processor's ID.
	 * @param online This Processor's online status.
	 * @param timestamp The timestamp when the snapshot was taken in microseconds.
	 */
	void updateValues(long user,
						long system,
						long idle,
						long wait,
						long busy,
						int id,
						int online,
						long timestamp)
	{
		this.user = user;
		this.system = system;
		this.idle = idle;
		this.wait = wait;
		this.busy = busy;
		this.id = id;
		this.online = online;
		this.timestamp = timestamp;
	}

	/**
	 * Receives a {@link javax.management.openmbean.CompositeData} representing a {@link ProcessorUsage}
	 * object and attempts to return the root {@link ProcessorUsage}
	 * instance.
	 *
	 * @param cd	A {@link javax.management.openmbean.CompositeData} that represents a {@link ProcessorUsage}
	 * 
	 * @return	if <code>cd</code> is non- <code>null</code>, returns a new instance of
	 * 		{@link ProcessorUsage}, If <code>cd</code>
	 * 		is <code>null</code>, returns <code>null</code>.
	 *
	 * @throws IllegalArgumentException	if argument <code>cd</code> does not correspond to a
	 * 		{@link ProcessorUsage} with the following attributes:
	 * 		<ul>
	 * 		<li><code>user</code>(<code>java.lang.Long</code>)</li>
	 * 		<li><code>system</code>(<code>java.lang.Long</code>)</li>
	 * 		<li><code>idle</code>(<code>java.lang.Long</code>)</li>
	 * 		<li><code>wait</code>(<code>java.lang.Long</code>)</li>
	 * 		<li><code>busy</code>(<code>java.lang.Long</code>)</li>
	 * 		<li><code>id</code>(<code>java.lang.Integer</code>)</li>
	 * 		<li><code>online</code>(<code>java.lang.Integer</code>)</li>
	 * 		<li><code>timestamp</code>(<code>java.lang.Long</code>)</li>
	 * 		</ul>
	 */
	public static ProcessorUsage from(CompositeData cd) {
		ProcessorUsage result = null;

		if (null != cd) {
			// Is the new received CompositeData of the required type to create
			// a new ProcessorUsage ?
			if (!ProcessorUsageUtil.getCompositeType().isValue(cd)) {
				/*[MSG "K05E5", "CompositeData is not of the expected type."]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05E5")); //$NON-NLS-1$
			}

			long user;
			long system;
			long idle;
			long wait;
			long busy;
			int id;
			int online;
			long timestamp;

			try {
				user = ((Long) cd.get("user")).longValue(); //$NON-NLS-1$
				system = ((Long) cd.get("system")).longValue(); //$NON-NLS-1$
				idle = ((Long) cd.get("idle")).longValue(); //$NON-NLS-1$
				wait = ((Long) cd.get("wait")).longValue(); //$NON-NLS-1$
				busy = ((Long) cd.get("busy")).longValue(); //$NON-NLS-1$
				id = ((Integer) cd.get("id")).intValue(); //$NON-NLS-1$
				online = ((Integer) cd.get("online")).intValue(); //$NON-NLS-1$
				timestamp = ((Long) cd.get("timestamp")).longValue(); //$NON-NLS-1$
			} catch (InvalidKeyException e) {
				/*[MSG "K05E6", "CompositeData object does not contain expected key."]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05E6")); //$NON-NLS-1$
			}

			result = new ProcessorUsage(user, system, idle, wait, busy, id, online, timestamp);
		}

		return result;
	}

	/**
	 * Text description of this {@link ProcessorUsage} object.
	 *
	 * @return Text description of this {@link ProcessorUsage} object.
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
		sb.append("id = "); //$NON-NLS-1$
		sb.append(this.id);
		sb.append("\n"); //$NON-NLS-1$
		sb.append("online = "); //$NON-NLS-1$
		sb.append(this.online);
		sb.append("\n"); //$NON-NLS-1$
		sb.append("user = "); //$NON-NLS-1$
		sb.append(this.user);
		sb.append("\n"); //$NON-NLS-1$
		sb.append("system = "); //$NON-NLS-1$
		sb.append(this.system);
		sb.append("\n"); //$NON-NLS-1$
		sb.append("wait = "); //$NON-NLS-1$
		sb.append(this.wait);
		sb.append("\n"); //$NON-NLS-1$
		sb.append("busy = "); //$NON-NLS-1$
		sb.append(this.busy);
		sb.append("\n"); //$NON-NLS-1$
		sb.append("idle = "); //$NON-NLS-1$
		sb.append(this.idle);
		sb.append("\n"); //$NON-NLS-1$

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

		if (!(obj instanceof ProcessorUsage)) {
			return false;
		}

		ProcessorUsage pu = (ProcessorUsage) obj;

		if (!(pu.getUser() == this.getUser())) {
			return false;
		}

		if (!(pu.getSystem() == this.getSystem())) {
			return false;
		}

		if (!(pu.getIdle() == this.getIdle())) {
			return false;
		}

		if (!(pu.getWait() == this.getWait())) {
			return false;
		}

		if (!(pu.getBusy() == this.getBusy())) {
			return false;
		}

		if (!(pu.getId() == this.getId())) {
			return false;
		}

		if (!(pu.getOnline() == this.getOnline())) {
			return false;
		}

		if (!(pu.getTimestamp() == this.getTimestamp())) {
			return false;
		}

		return true;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public int hashCode() {
		long pHash = this.getUser()
				+ this.getSystem()
				+ this.getIdle()
				+ this.getWait()
				+ this.getBusy()
				+ this.getId()
				+ this.getOnline()
				+ this.getTimestamp();
		return (int) ((((pHash >> 32) + pHash) & HASHMASK) * 23);
	}

}
