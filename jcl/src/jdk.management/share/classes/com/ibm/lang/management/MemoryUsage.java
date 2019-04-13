/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2012, 2019 IBM Corp. and others
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

import com.ibm.lang.management.internal.MemoryUsageUtil;

/**
 * This represents a snapshot of the Operating System level Memory usage statistics.
 * This is available on AIX, Linux, Windows and z/OS. 
 * 
 * @author sonchakr, dgunigun
 * @since 1.7.1
 */
public class MemoryUsage {

	private static final int HASHMASK = 0x0FFFFFFF;

	private long total;
	private long free;
	private long swapTotal;
	private long swapFree;
	private long cached;
	private long buffered;
	private long timestamp;

	/**
	 * Creates a new {@link com.ibm.lang.management.MemoryUsage} instance.
	 */
	public MemoryUsage() {
		super();
	}

	/**
	 * Creates a new {@link com.ibm.lang.management.MemoryUsage} instance.
	 *
	 * @param total The total amount of usable physical memory in bytes.
	 * @param free The amount of free memory in bytes.
	 * @param swapTotal The configured swap memory size in bytes; or -1 if undefined.
	 * @param swapFree The amount of swap memory that is free in bytes; or -1 if undefined.
	 * @param cached The size of cache area in bytes; or -1 if undefined.
	 * @param buffered The size of file buffer area in bytes; or -1 if undefined.
	 * @param timestamp The timestamp when the snapshot was taken in microseconds.
	 * 
	 * @throws IllegalArgumentException if
	 * <ul><li>The values of total or free or timestamp are negative; or
	 * <li>The values of swapTotal or swapFree or cached or buffered are negative but not -1; or
	 * <li>The value of free is greater than total if defined; or
	 * <li>The value of swapFree is greater than swapTotal if defined; or
	 * <li>The sum of values of cached, buffered and free are greater than total if defined.  
	 * </ul>
	 */
	private MemoryUsage(long total, long free, long swapTotal, long swapFree,
			    long cached, long buffered, long timestamp) throws IllegalArgumentException {
		super();
		if ((total < 0) || (free < 0) || (timestamp < 0) ||
			((swapTotal < 0) && (swapTotal != -1)) ||
			((swapFree < 0) && (swapFree != -1)) ||
			((cached < 0) && (cached != -1)) ||
			((buffered < 0) && (buffered != -1)) ||
			((free >= 0) && (total >= 0) && (free > total)) ||
			((swapFree >= 0) && (swapTotal >= 0) && (swapFree > swapTotal)) ||
			((total >= 0) && (cached >= 0) && (buffered >= 0) && (free >= 0) &&
					(total < (cached + buffered + free)))) {
			throw new IllegalArgumentException();
		}
		this.total = total;
		this.free = free;
		this.swapTotal = swapTotal;
		this.swapFree = swapFree;
		this.cached = cached;
		this.buffered = buffered;
		this.timestamp = timestamp;
	}

	/**
	 * The total amount of usable physical memory in bytes.
	 * 
	 * @return Total physical memory in bytes or -1 if info not available.
	 */
	public long getTotal() {
		return this.total;
	}

	/**
	 * The total amount of free physical memory in bytes.
	 * 
	 * @return Free physical memory in bytes or -1 if info not available.
	 */
	public long getFree() {
		return this.free;
	}

	/**
	 * The amount of total swap space in bytes.
	 * 
	 * @return Total swap space in bytes or -1 if info not available.
	 */
	public long getSwapTotal() {
		return this.swapTotal;
	}

	/**
	 * The amount of free swap space in bytes.
	 * 
	 * @return Free swap space in bytes or -1 if info not available.
	 */
	public long getSwapFree() {
		return this.swapFree;
	}

	/**
	 * The amount of cached memory in bytes. Cached memory usually consists of
	 * contents of files and block devices.
	 *
	 * @return Cached memory in bytes or -1 if info not available.
	 */
	public long getCached() {
		return this.cached;
	}

	/**
	 * The amount of buffered memory in bytes. Buffered memory usually consists of
	 * file metadata.
	 * <ul>
	 * <li>Buffered memory is not available on AIX and Windows.
	 * </ul>
	 * 
	 * @return Buffered memory in bytes or -1 if info not available.
	 */
	public long getBuffered() {
		return this.buffered;
	}

	/**
	 * The timestamp when the usage statistics were last sampled in microseconds.
	 * 
	 * @return Timestamp in microseconds.
	 */
	public long getTimestamp() {
		return this.timestamp;
	}

	/* (non-Javadoc)
	 * Setter method for updating the memory usage parameters into fields of a 
	 * {@link MemoryUsage} instance.
	 * 
	 * @param total The total Memory installed on the system in bytes.
	 * @param free The amount of free memory in bytes.
	 * @param swapTotal The configured swap memory size in bytes.
	 * @param swapFree The amount of swap memory that is free in bytes.
	 * @param cached The size of cache area in bytes.
	 * @param buffered The size of file buffer area in bytes.
	 * @param timestamp The timestamp when the snapshot was taken in microseconds.
	 */
	void updateValues(long total,
			long free,
			long swapTotal,
			long swapFree,
			long cached,
			long buffered,
			long timestamp)
	{
		this.total = total;
		this.free = free;
		this.swapTotal = swapTotal;
		this.swapFree = swapFree;
		this.cached = cached;
		this.buffered = buffered;
		this.timestamp = timestamp;
	}

	/**
	 * Receives a {@link javax.management.openmbean.CompositeData} representing a {@link com.ibm.lang.management.MemoryUsage}
	 *
	 * @param cd A {@link javax.management.openmbean.CompositeData} that represents a {@link com.ibm.lang.management.MemoryUsage}
	 *
	 * @return If <code>cd</code> is non- <code>null</code>, returns a new instance of
	 * 	   {@link com.ibm.lang.management.MemoryUsage}, If <code>cd</code> is <code>null</code>,
	 * 	   returns <code>null</code>.
	 *
	 * @throws IllegalArgumentException if argument <code>cd</code> does not correspond
	 * 	   to a {@link com.ibm.lang.management.MemoryUsage} with the following attributes:
	 * 	   <ul>
	 *	   <li><code>total</code>(<code>java.lang.Long</code>)</li>
	 *	   <li><code>free</code>(<code>java.lang.Long</code>)</li>
	 * 	   <li><code>swapTotal</code>(<code>java.lang.Long</code>)</li>
	 * 	   <li><code>swapFree</code>(<code>java.lang.Long</code>)</li>
	 * 	   <li><code>cached</code>(<code>java.lang.Long</code>)</li>
	 * 	   <li><code>buffered</code>(<code>java.lang.Long</code>)</li>
	 * 	   <li><code>timestamp</code>(<code>java.lang.Long</code>)</li>
	 * 	   </ul>
	 */
	public static MemoryUsage from(CompositeData cd) {
		MemoryUsage result = null;
		if (null != cd) {
			// Is the new received CompositeData of the required type to create
			// a new MemoryUsage ?
			if (!MemoryUsageUtil.getCompositeType().isValue(cd)) {
				/*[MSG "K05E5", "CompositeData is not of the expected type."]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05E5")); //$NON-NLS-1$
			}

			long total;
			long free;
			long swapTotal;
			long swapFree;
			long cached;
			long buffered;
			long timestamp;

			try {
				total = ((Long) cd.get("total")).longValue(); //$NON-NLS-1$
				free = ((Long) cd.get("free")).longValue(); //$NON-NLS-1$
				swapTotal = ((Long) cd.get("swapTotal")).longValue(); //$NON-NLS-1$
				swapFree = ((Long) cd.get("swapFree")).longValue(); //$NON-NLS-1$
				cached = ((Long) cd.get("cached")).longValue(); //$NON-NLS-1$
				buffered = ((Long) cd.get("buffered")).longValue(); //$NON-NLS-1$
				timestamp = ((Long) cd.get("timestamp")).longValue(); //$NON-NLS-1$
			} catch (InvalidKeyException e) {
	        	/*[MSG "K05E6", "CompositeData object does not contain expected key."]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05E6")); //$NON-NLS-1$
			}

			result = new MemoryUsage(total, free, swapTotal, swapFree, cached, buffered, timestamp);
		}
		return result;
	}

	/**
	 * Text description of this {@link com.ibm.lang.management.MemoryUsage} object.
	 *
	 * @return a text description of this {@link com.ibm.lang.management.MemoryUsage} object.
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
		sb.append("total = "); //$NON-NLS-1$
		sb.append(this.total);
		sb.append("\n"); //$NON-NLS-1$
		sb.append("free = "); //$NON-NLS-1$
		sb.append(this.free);
		sb.append("\n"); //$NON-NLS-1$
		sb.append("swapTotal = "); //$NON-NLS-1$
		sb.append(this.swapTotal);
		sb.append("\n"); //$NON-NLS-1$
		sb.append("swapFree = "); //$NON-NLS-1$
		sb.append(this.swapFree);
		sb.append("\n"); //$NON-NLS-1$
		sb.append("cached = "); //$NON-NLS-1$
		sb.append(this.cached);
		sb.append("\n"); //$NON-NLS-1$
		sb.append("buffered = "); //$NON-NLS-1$
		sb.append(this.buffered);
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

		if (!(obj instanceof MemoryUsage)) {
			return false;
		}

		MemoryUsage mu = (MemoryUsage) obj;

		if (!(mu.getTotal() == this.getTotal())) {
			return false;
		}

		if (!(mu.getFree() == this.getFree())) {
			return false;
		}

		if (!(mu.getSwapTotal() == this.getSwapTotal())) {
			return false;
		}

		if (!(mu.getSwapFree() == this.getSwapFree())) {
			return false;
		}

		if (!(mu.getCached() == this.getCached())) {
			return false;
		}

		if (!(mu.getBuffered() == this.getBuffered())) {
			return false;
		}

		if (!(mu.getTimestamp() == this.getTimestamp())) {
			return false;
		}

		return true;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public int hashCode() {
		long mHash = this.getTotal()
				+ this.getFree()
				+ this.getSwapTotal()
				+ this.getSwapFree()
				+ this.getCached()
				+ this.getBuffered()
				+ this.getTimestamp();
		return (int) ((((mHash >> 32) + mHash) & HASHMASK) * 23);
	}

}
