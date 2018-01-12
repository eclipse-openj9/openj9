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
package java.lang.management;

import javax.management.openmbean.CompositeData;

import com.ibm.java.lang.management.internal.ManagementUtils;

/**
 * Represents the memory usage of either a memory pool or the heap and non-heap
 * memory areas of the virtual machine.
 *
 * @since 1.5
 */
public class MemoryUsage {

	/**
	 * The initial amount of memory requested from the underlying operating
	 * system when the virtual machine started up. The value is in bytes.
	 */
	private long init;

	/**
	 * The amount of memory currently in use. The value is in bytes.
	 */
	private long used;

	/**
	 * The amount of memory that the virtual machine can currently assume is
	 * available for it to use. It is possible that this value may change over
	 * the lifetime of the virtual machine. The value is in bytes.
	 */
	private long committed;

	/**
	 * The maximum amount of memory that is available to the virtual machine.
	 * May change over the lifecycle of the virtual machine. The value is in
	 * bytes.
	 */
	private long max;

	private String TOSTRING_VALUE;

	/**
	 * Constructs a new <code>MemoryUsage</code> instance.
	 * 
	 * @param init
	 *            if defined, the initial amount of memory that can be allocated
	 *            by the virtual machine in bytes. If not defined, then
	 *            <code>-1</code>.
	 * @param used
	 *            the number of bytes currently used for memory.
	 * @param committed
	 *            the number of bytes of committed memory.
	 * @param max
	 *            if defined, the maximum number of bytes that can be used for
	 *            memory management purposes. If not defined, <code>-1</code>.
	 * @throws IllegalArgumentException
	 *             if any of the following conditions applies:
	 *             <ul>
	 *             <li><code>init</code> &lt; <code>-1</code>
	 *             <li><code>max</code> &lt; <code>-1</code>
	 *             <li><code>used</code> &lt; <code>0</code>
	 *             <li><code>committed</code> &lt; <code>0</code>
	 *             <li><code>used</code> &gt; <code>committed</code>
	 *             <li><code>committed</code> &gt; <code>max</code> if
	 *             <code>max</code> is not <code>-1</code>.
	 *             </ul>
	 */
	public MemoryUsage(long init, long used, long committed, long max) {
		// Validate inputs
		if (init < -1) {
			/*[MSG "K0608", "init argument cannot be less than -1"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0608")); //$NON-NLS-1$
		}

		if (max < -1) {
			/*[MSG "K0609", "max argument cannot be less than -1"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0609")); //$NON-NLS-1$
		}

		if (used < 0) {
			/*[MSG "K060A", "used argument cannot be less than 0"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K060A")); //$NON-NLS-1$
		}

		if (committed < 0) {
			/*[MSG "K060B", "committed argument cannot be less than 0"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K060B")); //$NON-NLS-1$
		}

		if (used > committed) {
			/*[MSG "K060C", "used value cannot be larger than the committed value"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K060C")); //$NON-NLS-1$
		}

		if ((max != -1) && (committed > max)) {
			/*[MSG "K060D", "committed value cannot be larger than the max value"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K060D")); //$NON-NLS-1$
		}

		this.init = init;
		this.used = used;
		this.committed = committed;
		this.max = max;
	}

	/**
	 * Returns the amount of memory that has been pledged by the operating
	 * system for the virtual machine to use. This value is in bytes.
	 * 
	 * @return the number of bytes committed to memory.
	 */
	public long getCommitted() {
		return this.committed;
	}

	/**
	 * Returns the initial amount of memory requested from the underlying
	 * operating system when the virtual machine started up. The value is given
	 * in bytes.
	 * <p>
	 * if the initial memory size was not defined, this method will return a
	 * value of <code>-1</code>.
	 * </p>
	 * 
	 * @return the initial amount of memory requested at virtual machine start
	 *         up. <code>-1</code> if not defined.
	 */
	public long getInit() {
		return this.init;
	}

	/**
	 * Returns the maximum amount of memory that is available to the virtual
	 * machine which may change over the lifecycle of the virtual machine. The
	 * value is in bytes.
	 * 
	 * @return if defined, the maximum memory size in bytes. If not defined,
	 *         <code>-1</code>.
	 */
	public long getMax() {
		return this.max;
	}

	/**
	 * Returns the number of bytes currently used for memory management
	 * purposes.
	 * 
	 * @return the current number of bytes used for memory.
	 */
	public long getUsed() {
		return this.used;
	}

	/**
	 * Returns a text description of this memory usage.
	 * 
	 * @return a text description of this memory usage.
	 */
	@Override
	public String toString() {
		// Since MemoryUsages are immutable the string value need only be
		// calculated the one time
		if (TOSTRING_VALUE == null) {
			StringBuilder buff = new StringBuilder();
			buff.append("init = "); //$NON-NLS-1$
			buff.append(this.init);
			appendSizeInKBytes(buff, this.init);

			buff.append("used = "); //$NON-NLS-1$
			buff.append(this.used);
			appendSizeInKBytes(buff, this.used);

			buff.append("committed = "); //$NON-NLS-1$
			buff.append(this.committed);
			appendSizeInKBytes(buff, this.committed);

			buff.append("max = "); //$NON-NLS-1$
			buff.append(this.max);
			appendSizeInKBytes(buff, this.max);
			TOSTRING_VALUE = buff.toString().trim();
		}
		return TOSTRING_VALUE;
	}

	/**
	 * Convenience method which takes a {@link StringBuilder} and appends to it
	 * the <code>long</code> value expressed as an integral number of
	 * kilobytes.
	 * <p>
	 * If <code>value</code> is <code>-1</code> then the appended text will
	 * comprise of the word &quot;undefined&quot;.
	 * </p>
	 * 
	 * @param buff
	 *            an existing <code>StringBuilder</code> that will be updated
	 *            with <code>value</code> expressed in kilobytes.
	 * @param value
	 *            a <code>long</code> value.
	 */
	private static void appendSizeInKBytes(StringBuilder buff, long value) {
		// it would be better to write "undefined" for values of -1 but in 
		// order to do as the RI does...
		if (value == -1) {
			buff.append("(-1K) "); //$NON-NLS-1$
		} else {
			buff.append("(").append(value / 1024).append("K) "); //$NON-NLS-1$ //$NON-NLS-2$
		}
	}

	/**
	 * Receives a {@link CompositeData} representing a <code>MemoryUsage</code>
	 * object and attempts to return the root <code>MemoryUsage</code>
	 * instance.
	 * 
	 * @param cd
	 *            a <code>CompositeDate</code> that represents a
	 *            <code>MemoryUsage</code>.
	 * @return if <code>cd</code> is non- <code>null</code>, returns a new
	 *         instance of <code>MemoryUsage</code>. If <code>cd</code> is
	 *         <code>null</code>, returns <code>null</code>.
	 * @throws IllegalArgumentException
	 *             if argument <code>cd</code> does not correspond to a
	 *             <code>MemoryUsage</code> with the following attributes all
	 *             of type <code>java.long.Long</code>:
	 *             <ul>
	 *             <li><code>committed</code>
	 *             <li><code>init</code>
	 *             <li><code>max</code>
	 *             <li><code>used</code>
	 *             </ul>
	 */
	public static MemoryUsage from(CompositeData cd) {
		MemoryUsage result = null;

		if (cd != null) {
			// Does cd meet the necessary criteria to create a new
			// MemoryUsage? If not then exit on an IllegalArgumentException.
			// TODO : Use this method? verifyType(cd);

			ManagementUtils.verifyFieldNumber(cd, 4);
			String[] attributeNames = { "init", "used", //$NON-NLS-1$ //$NON-NLS-2$
					"committed", "max" }; //$NON-NLS-1$ //$NON-NLS-2$
			ManagementUtils.verifyFieldNames(cd, attributeNames);
			String[] attributeTypes = { "java.lang.Long", "java.lang.Long", //$NON-NLS-1$ //$NON-NLS-2$
					"java.lang.Long", "java.lang.Long" }; //$NON-NLS-1$ //$NON-NLS-2$
			ManagementUtils.verifyFieldTypes(cd, attributeNames, attributeTypes);

			// Extract the values of the attributes and use them to construct
			// a new MemoryUsage.
			Object[] attributeVals = cd.getAll(attributeNames);
			long initVal = ((Long) attributeVals[0]).longValue();
			long usedVal = ((Long) attributeVals[1]).longValue();
			long committedVal = ((Long) attributeVals[2]).longValue();
			long maxVal = ((Long) attributeVals[3]).longValue();
			result = new MemoryUsage(initVal, usedVal, committedVal, maxVal);
		}

		return result;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean equals(Object obj) {
		if (this == obj) {
			return true;
		}

		if (obj == null) {
			return false;
		}

		if (!(obj instanceof MemoryUsage)) {
			return false;
		}

		// Safe to cast if the instanceof test above was passed.
		MemoryUsage mu = (MemoryUsage) obj;
		if (!(mu.getInit() == this.getInit())) {
			return false;
		}
		if (!(mu.getCommitted() == this.getCommitted())) {
			return false;
		}
		if (!(mu.getUsed() == this.getUsed())) {
			return false;
		}
		if (!(mu.getMax() == this.getMax())) {
			return false;
		}
		return true;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public int hashCode() {
		return (Long.toString(this.getCommitted())
				+ this.getInit()
				+ this.getMax()
				+ this.getUsed()).hashCode();
	}

}
