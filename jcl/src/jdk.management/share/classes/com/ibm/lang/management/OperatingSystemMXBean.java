/*[INCLUDE-IF Sidecar17]*/
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
package com.ibm.lang.management;

/**
 * <p>OpenJ9 platform management extension interface for the Operating System on which the Java Virtual Machine is running.</p>
 * <br>
 * <b>Usage example for the {@link com.ibm.lang.management.OperatingSystemMXBean}</b>
 * <pre>
 * {@code
 * ...
 * com.ibm.lang.management.OperatingSystemMXBean osmxbean = null;
 * osmxbean = (com.ibm.lang.management.OperatingSystemMXBean) ManagementFactory.getOperatingSystemMXBean();
 * ...
 * }
 * </pre>
 * <br>
 *
 * The following methods depend on certain information that is not available on z/OS;
 * their return values are listed here:
 * <table border="1">
 * <caption><b>Methods not fully supported on z/OS</b></caption>
 * <tr>
 * <th style="text-align:left">Method</th>
 * <th style="text-align:left">Return Value</th>
 * </tr>
 * <tr>
 * <td>{@link #getCommittedVirtualMemorySize()}</td>
 * <td>{@code -1}</td>
 * </tr>
/*[IF JAVA_SPEC_VERSION >= 14]
 * <tr>
 * <td>{@link #getCpuLoad()}</td>
 * <td>{@code -3.0} ({@link com.ibm.lang.management.CpuLoadCalculationConstants#UNSUPPORTED_VALUE CpuLoadCalculationConstants.UNSUPPORTED_VALUE})</td>
 * </tr>
 * <tr>
 * <td>{@link #getFreeMemorySize()}</td>
 * <td>{@code -1}</td>
 * </tr>
/*[ENDIF] JAVA_SPEC_VERSION >= 14
 * <tr>
 * <td>{@link #getFreePhysicalMemorySize()}</td>
 * <td>{@code -1}</td>
 * </tr>
 * <tr>
 * <td>{@link #getProcessPhysicalMemorySize()}</td>
 * <td>{@code -1}</td>
 * </tr>
 * <tr>
 * <td>{@link #getProcessPrivateMemorySize()}</td>
 * <td>{@code -1}</td>
 * </tr>
 * <tr>
 * <td>{@link #getSystemCpuLoad()}</td>
 * <td>{@code -3.0} ({@link com.ibm.lang.management.CpuLoadCalculationConstants#UNSUPPORTED_VALUE CpuLoadCalculationConstants.UNSUPPORTED_VALUE})</td>
 * </tr>
 * </table>
 *
 * @see CpuLoadCalculationConstants
 * @since 1.5
 */
public interface OperatingSystemMXBean extends com.sun.management.OperatingSystemMXBean {

/*[IF JAVA_SPEC_VERSION < 19]*/
	/**
/*[IF JAVA_SPEC_VERSION >= 14]
	 * Deprecated: use getTotalMemorySize()
/*[ELSE] IF JAVA_SPEC_VERSION >= 14
	 * Deprecated: use getTotalPhysicalMemorySize()
/*[ENDIF] IF JAVA_SPEC_VERSION >= 14
	 *
	 * @return the total available physical memory on the system in bytes.
	 */
	/*[IF JAVA_SPEC_VERSION > 8]
	@Deprecated(forRemoval = true, since = "1.8")
	/*[ELSE] JAVA_SPEC_VERSION > 8 */
	@Deprecated
	/*[ENDIF] JAVA_SPEC_VERSION > 8 */
	public long getTotalPhysicalMemory();
/*[ENDIF] JAVA_SPEC_VERSION < 19 */

	/**
	 * Returns the collective capacity of the virtual processors in
	 * the partition the VM is running in. The value returned is in
	 * units of 1% of a physical processor's capacity, so a value of
	 * 100 is equal to 1 physical processor. In environments without
	 * such partitioning support, this call will return
	 * getAvailableProcessors() * 100.
	 *
	 * @return the collective capacity of the virtual processors available
	 * to the VM.
	 */
	public int getProcessingCapacity();

	/**
	 * Returns total amount of time the process has been scheduled or
	 * executed so far in both kernel and user modes. Returns -1 if the
	 * value is unavailable on this platform or in the case of an error.
	 *
	 * Note that as of Java 8 SR5 the returned value is in 1 ns units, unless the system property
	 * com.ibm.lang.management.OperatingSystemMXBean.isCpuTime100ns is set to "true".
	 *
	 * @return process CPU time.
	 */
	public long getProcessCpuTime();

/*[IF JAVA_SPEC_VERSION < 19]*/
	/**
	 * Deprecated. Use getProcessCpuTime()
	 */
	/*[IF JAVA_SPEC_VERSION >= 9]
	@Deprecated(forRemoval = true, since = "1.8")
	/*[ELSE] JAVA_SPEC_VERSION >= 9 */
	@Deprecated
	/*[ENDIF] JAVA_SPEC_VERSION >= 9 */
	public long getProcessCpuTimeByNS();
/*[ENDIF] JAVA_SPEC_VERSION < 19 */

	/**
	 * Returns the amount of virtual memory used by the process in bytes.
	 * Returns -1 if the value is unavailable on this platform or in the
	 * case of an error.
	 * <ul>
	 * <li>This information is not available on the z/OS platform.
	 * </ul>
	 *
	 * @since   1.8
	 * @return amount of virtual memory used by the process in bytes.
	 */
	@Override
	public long getCommittedVirtualMemorySize();

/*[IF JAVA_SPEC_VERSION < 19]*/
	/**
	 * Deprecated.  Use getCommittedVirtualMemorySize()
	 */
	/*[IF JAVA_SPEC_VERSION >= 9]
	@Deprecated(forRemoval = true, since = "1.8")
	/*[ELSE] JAVA_SPEC_VERSION >= 9 */
	@Deprecated
	/*[ENDIF] JAVA_SPEC_VERSION >= 9 */
	public long getProcessVirtualMemorySize();
/*[ENDIF] JAVA_SPEC_VERSION < 19 */

	/**
	 * Returns the amount of private memory used by the process in bytes.
	 * Returns -1 if the value is unavailable on this platform or in the
	 * case of an error.
	 * <ul>
	 * <li>This information is not available on the z/OS platform.
	 * </ul>
	 *
	 * @return amount of private memory used by the process in bytes.
	 */
	public long getProcessPrivateMemorySize();

	/**
	 * Returns the amount of physical memory being used by the process
	 * in bytes. Returns -1 if the value is unavailable on this platform
	 * or in the case of an error.
	 * <ul>
	 * <li>This information is not available on the AIX and z/OS platforms.
	 * </ul>
	 *
	 * @return amount of physical memory being used by the process in bytes.
	 */
	public long getProcessPhysicalMemorySize();

	/**
	 * Returns the total amount of swap space in bytes.
	 *
	 * @return the total amount of swap space in bytes.
	 * @since   1.7
	 */
	@Override
	public long getTotalSwapSpaceSize();

	/**
	 * Returns the amount of free swap space in bytes.
	 *
	 * @return the amount of free swap space in bytes.
	 * @since   1.7
	 */
	@Override
	public long getFreeSwapSpaceSize();

	/**
	 * Returns the recent CPU usage for the Java Virtual Machine process.
	 * This value is a double in the [0.0,1.0] interval. A value of 0.0 means
	 * that none of the CPUs were running threads from the JVM process during
	 * the recent period of time observed, while a value of 1.0 means that all
	 * CPUs were actively running threads from the JVM 100% of the time
	 * during the recent period of time observed. Threads from the JVM include
	 * application threads as well as JVM internal threads. All values
	 * between 0.0 and 1.0 are possible. The first call to the method always
	 * returns {@link com.ibm.lang.management.CpuLoadCalculationConstants}.ERROR_VALUE
	 * (-1.0), which marks the starting point. If the Java Virtual Machine's recent CPU
	 * usage is not available, the method returns a negative error code from
	 * {@link com.ibm.lang.management.CpuLoadCalculationConstants}.
	 *
	 * @return A value between 0 and 1.0, or a negative error code from
	 *         {@link com.ibm.lang.management.CpuLoadCalculationConstants} in case
	 *         of an error. On the first call to the API,
	 *         {@link com.ibm.lang.management.CpuLoadCalculationConstants}.ERROR_VALUE
	 *         (-1.0) shall be returned.
	 * @see CpuLoadCalculationConstants
	 * @since   1.7
	 */
	@Override
	public double getProcessCpuLoad();

	/**
	 * Returns an updated {@link com.ibm.lang.management.ProcessorUsage} object that represents the
	 * current snapshot of Processor usage statistics. The snapshot is the aggregate of all
	 * Processors that are online at the time of sampling.
	 *
	 * @param procObj User provided {@link com.ibm.lang.management.ProcessorUsage} object.
	 *
	 * @return The updated {@link com.ibm.lang.management.ProcessorUsage} object.
	 *
	 * @throws NullPointerException if a null reference is passed as parameter.
	 * @throws ProcessorUsageRetrievalException if it failed obtaining Processor usage statistics.
	 *
	 * <p>In case of an exception, the handler code might use toString() on the exception code
	 * to obtain a description of the exception.</p>
	 * @since   1.7.1
	 */
	public ProcessorUsage retrieveTotalProcessorUsage(ProcessorUsage procObj)
			throws NullPointerException, ProcessorUsageRetrievalException;

	/**
	 * Instantiate and return a new {@link com.ibm.lang.management.ProcessorUsage} object that
	 * represents the current snapshot of Processor usage statistics. The snapshot is the
	 * aggregate of all Processors that are online at the time of sampling.
	 *
	 * @return The new {@link com.ibm.lang.management.ProcessorUsage} object.
	 *
	 * @throws ProcessorUsageRetrievalException if it failed obtaining Processor usage statistics.
	 *
	 * <p>In case of an exception, the handler code might use toString() on the exception code
	 * to obtain a description of the exception.</p>
	 * @since   1.7.1
	 */
	public ProcessorUsage retrieveTotalProcessorUsage() throws ProcessorUsageRetrievalException;

	/**
	 * Returns an updated array of {@link com.ibm.lang.management.ProcessorUsage} objects
	 * that represent the current snapshot of individual Processor usage times.
	 *
	 * @param procArray User provided array of {@link com.ibm.lang.management.ProcessorUsage} objects.
	 *
	 * @return The updated array of {@link com.ibm.lang.management.ProcessorUsage} objects.
	 *
	 * @throws NullPointerException if a null reference is passed as parameter.
	 * @throws ProcessorUsageRetrievalException if it failed obtaining Processor usage statistics.
	 * @throws IllegalArgumentException if array provided has insufficient entries and there are more
	 *                   Processors to report on.
	 *
	 * <p>In case of an exception, the handler code might use toString() on the exception code
	 * to obtain a description of the exception.</p>
	 * @since   1.7.1
	 */
	public ProcessorUsage[] retrieveProcessorUsage(ProcessorUsage[] procArray)
			throws NullPointerException, ProcessorUsageRetrievalException, IllegalArgumentException;

	/**
	 * Instantiates and returns an array of {@link com.ibm.lang.management.ProcessorUsage} objects
	 * that represent the current snapshot of individual Processor usage times.
	 *
	 * @return The new array of {@link com.ibm.lang.management.ProcessorUsage} objects.
	 *
	 * @throws ProcessorUsageRetrievalException if it failed obtaining Processor usage statistics.
	 *
	 * <p>In case of an exception, the handler code might use toString() on the exception code
	 * to obtain a description of the exception.</p>
	 * @since   1.7.1
	 */
	public ProcessorUsage[] retrieveProcessorUsage() throws ProcessorUsageRetrievalException;

	/**
	 * Returns an updated {@link com.ibm.lang.management.MemoryUsage} object that represents the
	 * current snapshot of Memory usage statistics.
	 *
	 * @param memObj User provided {@link com.ibm.lang.management.MemoryUsage} object.
	 *
	 * @return The updated {@link com.ibm.lang.management.MemoryUsage} object.
	 *
	 * @throws NullPointerException if a null reference is passed as parameter.
	 * @throws MemoryUsageRetrievalException if it failed obtaining Memory usage statistics.
	 *
	 * <p>In case of an exception, the handler code might use toString() on the exception code
	 * to obtain a description of the exception.</p>
	 * @since   1.7.1
	 */
	public MemoryUsage retrieveMemoryUsage(MemoryUsage memObj)
			throws NullPointerException, MemoryUsageRetrievalException;

	/**
	 * Instantiates and returns an instance of {@link com.ibm.lang.management.MemoryUsage} object
	 * that represents the current snapshot of Memory usage statistics.
	 *
	 * @return The new {@link com.ibm.lang.management.MemoryUsage} object.
	 *
	 * @throws MemoryUsageRetrievalException if it failed obtaining Memory usage statistics.
	 *
	 * <p>In case of an exception, the handler code might use toString() on the exception code
	 * to obtain a description of the exception.</p>
	 * @since   1.7.1
	 */
	public MemoryUsage retrieveMemoryUsage() throws MemoryUsageRetrievalException;

	/**
	 * Instantiates and returns an instance of a {@link java.lang.String} object containing
	 * hardware model information
	 *
	 * @return The new {@link java.lang.String} object or NULL in case of an error.
	 * @throws UnsupportedOperationException if the operation is not implemented on this platform.
	 * UnsupportedOperationException will also be thrown if the operation is implemented but it
	 * cannot be performed because the system does not satisfy all the requirements, for example,
	 * an OS service is not installed.
	 * @since   1.7
	 */
	public String getHardwareModel() throws UnsupportedOperationException;

	/**
	 * Identify whether the underlying hardware is being emulated
	 *
	 * @return True if it is possible to identify that the hardware is being emulated. False otherwise.
	 * @throws UnsupportedOperationException if the emulated status cannot be determined
	 * @since   1.7
	 */
	public boolean isHardwareEmulated() throws UnsupportedOperationException;

	/**
	 * Indicates if the specified process is running
	 * @param pid Operating system process ID
	 * @return True if the specified process exists
	 * @since   1.8
	 */
	public boolean isProcessRunning(long pid);

}
