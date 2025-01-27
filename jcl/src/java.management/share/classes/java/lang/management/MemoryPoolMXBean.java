/*[INCLUDE-IF JAVA_SPEC_VERSION >= 8]*/
/*
 * Copyright IBM Corp. and others 2005
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package java.lang.management;

/**
 * The management and monitoring interface for a virtual machine memory pool. A
 * memory pool is managed by one or more instances of
 * {@link MemoryManagerMXBean}.
 * <P>
 * In any given Java virtual machine there can be at one or more instances of
 * this interface. Each may be distinguished by their separate
 * <code>ObjectName</code> value.
 * </P>
 * <p>
 * Accessing this kind of <code>MXBean</code> can be done in one of three
 * ways.
 * <ol>
 * <li>Invoking the static {@link ManagementFactory#getMemoryPoolMXBeans()}
 * method which returns a {@link java.util.List} of all currently instantiated
 * MemoryPoolMXBeans.</li>
 * <li>Using a {@link javax.management.MBeanServerConnection}.</li>
 * <li>Obtaining a proxy MXBean from the static
 * {@link ManagementFactory#newPlatformMXBeanProxy} method, passing in the
 * string &quot;java.lang:type=MemoryPool,name= <i>unique memory pool name
 * </i>&quot; for the value of the second parameter.</li>
 * </ol>
 *
 * @since 1.5
 */
public interface MemoryPoolMXBean extends PlatformManagedObject {

	/**
	 * If supported by the virtual machine, returns a {@link MemoryUsage} which
	 * encapsulates this memory pool's memory usage after the most recent run of
	 * the garbage collector. No garbage collection will be actually occur as a
	 * result of this method getting called.
	 *
	 * @return a {@link MemoryUsage} object that may be interrogated by the
	 *         caller to determine the details of the memory usage. Returns
	 *         <code>null</code> if the virtual machine does not support this
	 *         method.
	 *
	 */
	public MemoryUsage getCollectionUsage();

	/**
	 * Returns this memory pool's collection usage threshold.
	 *
	 * @return the collection usage threshold in bytes. The default value as set
	 *         by the virtual machine will be zero.
	 * @throws UnsupportedOperationException
	 *             if the memory pool does not support a collection usage
	 *             threshold.
	 * @see #isCollectionUsageThresholdSupported()
	 */
	public long getCollectionUsageThreshold();

	/**
	 * Returns the number of times that the memory usage for this memory pool
	 * has grown to exceed the collection usage threshold.
	 *
	 * @return a count of the number of times that the collection usage
	 *         threshold has been surpassed.
	 * @throws UnsupportedOperationException
	 *             if the memory pool does not support a collection usage
	 *             threshold.
	 * @see #isCollectionUsageThresholdSupported()
	 */
	public long getCollectionUsageThresholdCount();

	/**
	 * Returns a string array containing the unique names of each memory manager
	 * that manages this memory pool. A memory pool will always have at least
	 * one memory manager associated with it.
	 *
	 * @return the names of all the memory managers for this memory pool.
	 */
	public String[] getMemoryManagerNames();

	/**
	 * Returns the name of this memory pool.
	 *
	 * @return the name of this memory pool.
	 */
	public String getName();

	/**
	 * Returns information on the peak usage of the memory pool. The scope of
	 * this covers all elapsed time since either the start of the virtual
	 * machine or the peak usage was reset.
	 *
	 * @return a {@link MemoryUsage} which can be interrogated by the caller to
	 *         determine details of the peak memory usage. A <code>null</code>
	 *         value will be returned if the memory pool no longer exists (and
	 *         the pool is therefore considered to be invalid).
	 * @see #resetPeakUsage()
	 * @see #isValid()
	 */
	MemoryUsage getPeakUsage();

	/**
	 * Returns the memory pool's type.
	 *
	 * @return a {@link MemoryType} value indicating the type of the memory pool
	 *         (heap or non-heap).
	 */
	public MemoryType getType();

	/**
	 * Returns the current memory usage of this memory pool as estimated by the
	 * virtual machine.
	 *
	 * @return an instance of {@link MemoryUsage} that can be interrogated by
	 *         the caller to determine details on the pool's current memory
	 *         usage. A <code>null</code> value will be returned if the memory
	 *         pool no longer exists (in which case it is considered to be
	 *         invalid).
	 * @see #isValid()
	 */
	public MemoryUsage getUsage();

	/**
	 * Returns this memory pool's usage threshold.
	 *
	 * @return the usage threshold in bytes. The default value as set by the
	 *         virtual machine depends on the platform the virtual machine is
	 *         running on. will be zero.
	 * @throws UnsupportedOperationException
	 *             if the memory pool does not support a usage threshold.
	 * @see #isUsageThresholdSupported()
	 * @see #setUsageThreshold(long)
	 */
	public long getUsageThreshold();

	/**
	 * Returns the number of times that the memory usage for this memory pool
	 * has grown to exceed the current usage threshold.
	 *
	 * @return a count of the number of times that the usage threshold has been
	 *         surpassed.
	 * @throws UnsupportedOperationException
	 *             if the memory pool does not support a usage threshold.
	 * @see #isUsageThresholdSupported()
	 */
	public long getUsageThresholdCount();

	/**
	 * Returns a boolean indication of whether or not this memory pool hit or
	 * exceeded the current value of the collection usage threshold after the
	 * latest garbage collection run.
	 *
	 * @return <code>true</code> if the collection usage threshold was
	 *         surpassed after the latest garbage collection run, otherwise
	 *         <code>false</code>.
	 * @throws UnsupportedOperationException
	 *             if the memory pool does not support a collection usage
	 *             threshold.
	 * @see #isCollectionUsageThresholdSupported()
	 */
	public boolean isCollectionUsageThresholdExceeded();

	/**
	 * Returns a boolean indication of whether or not this memory pool supports
	 * a collection usage threshold.
	 *
	 * @return <code>true</code> if supported, <code>false</code> otherwise.
	 */
	public boolean isCollectionUsageThresholdSupported();

	/**
	 * Returns a boolean indication of whether or not this memory pool has hit
	 * or has exceeded the current value of the usage threshold.
	 *
	 * @return <code>true</code> if the usage threshold has been surpassed,
	 *         otherwise <code>false</code>.
	 * @throws UnsupportedOperationException
	 *             if the memory pool does not support a usage threshold.
	 * @see #isUsageThresholdSupported()
	 */
	public boolean isUsageThresholdExceeded();

	/**
	 * Returns a boolean indication of whether or not this memory pool supports
	 * a usage threshold.
	 *
	 * @return <code>true</code> if supported, <code>false</code> otherwise.
	 */
	public boolean isUsageThresholdSupported();

	/**
	 * Returns a boolean indication of whether or not this memory pool may still
	 * be considered valid. A memory pool becomes invalid once it has been
	 * removed by the virtual machine.
	 *
	 * @return <code>true</code> if the memory pool has not been removed by
	 *         the virtual machine, <code>false</code> otherwise.
	 */
	public boolean isValid();

	/**
	 * Updates this memory pool's memory usage peak value to be whatever the
	 * value of the current memory usage is.
	 *
	/*[IF JAVA_SPEC_VERSION < 24]
	 * @throws SecurityException
	 *             if there is a security manager active and the method caller
	 *             does not have ManagementPermission "control".
	 * @see ManagementPermission
	/*[ENDIF] JAVA_SPEC_VERSION < 24
	 */
	public void resetPeakUsage();

	/**
	 * Updates this memory pool to have a new value for its collection usage
	 * threshold. Only values of zero or greater should be supplied. A zero
	 * value effectively turns off any further checking of collection memory
	 * usage by the virtual machine. A value greater than zero establishes the
	 * new threshold which the virtual machine will check against after each run
	 * of the garbage collector in the memory pool.
	 *
	 * @param threshold
	 *            the size of the new collection usage threshold expressed in
	 *            bytes.
	 * @throws UnsupportedOperationException
	 *             if the memory pool does not support a collection usage
	 *             threshold.
	 * @throws IllegalArgumentException
	 *             if input value <code>threshold</code> is either negative or
	 *             else is in excess of any maximum memory size that may have
	 *             been defined for this memory pool.
	/*[IF JAVA_SPEC_VERSION < 24]
	 * @throws SecurityException
	 *             if there is a security manager active and the method caller
	 *             does not have {@link ManagementPermission}"control".
	/*[ENDIF] JAVA_SPEC_VERSION < 24
	 */
	public void setCollectionUsageThreshold(long threshold);

	/**
	 * Updates this memory pool to have a new value for its usage threshold.
	 * Only values of zero or greater should be supplied. A zero value
	 * effectively turns off any further checking of memory usage by the virtual
	 * machine. A value greater than zero establishes the new threshold which
	 * the virtual machine will check against.
	 *
	 * @param threshold
	 *            the size of the new usage threshold expressed in bytes.
	 * @throws UnsupportedOperationException
	 *             if the memory pool does not support a usage threshold.
	 * @throws IllegalArgumentException
	 *             if input value <code>threshold</code> is either negative or
	 *             else is in excess of any maximum memory size that may have
	 *             been defined for this memory pool.
	/*[IF JAVA_SPEC_VERSION < 24]
	 * @throws SecurityException
	 *             if there is a security manager active and the method caller
	 *             does not have {@link ManagementPermission}"control".
	/*[ENDIF] JAVA_SPEC_VERSION < 24
	 */
	public void setUsageThreshold(long threshold);

}
