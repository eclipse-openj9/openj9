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

/**
 * IBM specific platform management interface for the Operating System on which the Java Virtual Machine is running. 
 * <br>
 * <table border="1">
 * <caption><b>Usage example for the {@link com.ibm.lang.management.OperatingSystemMXBean}</b></caption>
 * <tr> <td> <pre>
 * {@code
 *  ...
 *     com.ibm.lang.management.OperatingSystemMXBean osmxbean = null;
 *    osmxbean = (com.ibm.lang.management.OperatingSystemMXBean) ManagementFactory.getOperatingSystemMXBean();
 *   ...
 * }
 * </pre></td></tr></table>
 *
 * @since 1.5
 */
public interface OperatingSystemMXBean extends com.sun.management.OperatingSystemMXBean {

	/**
	 * Returns the total available physical memory on the system in bytes.
	 * 
	 * @since 1.8
	 * @return the total available physical memory on the system in bytes.
	 */
	public long getTotalPhysicalMemorySize();

	/**
	 * Deprecated: use getTotalPhysicalMemorySize()
	 * 
	 * @return the total available physical memory on the system in bytes.
	 */
	/*[IF Sidecar19-SE]
	@Deprecated(forRemoval = true, since = "1.8")
	/*[ELSE]*/
	@Deprecated
	/*[ENDIF]*/
	public long getTotalPhysicalMemory();

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
	 * @return process cpu time.
	 */
	public long getProcessCpuTime();

	/**
	 * Deprecated. Use getProcessCpuTime()
	 */
	/*[IF Sidecar19-SE]
	@Deprecated(forRemoval = true, since = "1.8")
	/*[ELSE]*/
	@Deprecated
	/*[ENDIF]*/
	public long getProcessCpuTimeByNS();

	/**
	 * Returns the "recent cpu usage" for the whole system. This value is a double in
	 * the [0.0,1.0] interval. A value of 0.0 means all CPUs were idle in the recent
	 * period of time observed, while a value of 1.0 means that all CPUs were actively
	 * running 100% of the time during the recent period of time observed. All values
	 * between 0.0 and 1.0 are possible. The first call to the method always 
	 * returns {@link com.ibm.lang.management.CpuLoadCalculationConstants}.ERROR_VALUE
	 * (-1.0), which marks the starting point. If the Java Virtual Machine's recent CPU
	 * usage is not available, the method returns a negative error code from
	 * {@link com.ibm.lang.management.CpuLoadCalculationConstants}.
	 * <p>getSystemCpuLoad might not return the same value that is reported by operating system
	 * utilities such as Unix "top" or Windows task manager.</p>
	 *  
	 * @return A value between 0 and 1.0, or a negative error code from
	 *         {@link com.ibm.lang.management.CpuLoadCalculationConstants} in case
	 *         of an error. On the first call to the API,
	 *         {@link com.ibm.lang.management.CpuLoadCalculationConstants}.ERROR_VALUE
	 *         (-1.0) shall be returned.
	 * <ul>
	 * <li>Because this information is not available on z/OS, the call returns
	 * {@link com.ibm.lang.management.CpuLoadCalculationConstants}.UNSUPPORTED_VALUE
	 * (-3.0).
	 * </ul>
	 * @see CpuLoadCalculationConstants
	 */
	public double getSystemCpuLoad();

	/**
	 * Returns the amount of physical memory that is available on the system in bytes. 
	 * Returns -1 if the value is unavailable on this platform or in the case of an error.
	 * <ul>
	 * <li>This information is not available on the z/OS platform.
	 * </ul>
	 *
	 * @return amount of physical memory available in bytes.
	 */
	public long getFreePhysicalMemorySize();

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
	public long getCommittedVirtualMemorySize();

	/**
	 * Deprecated.  Use getCommittedVirtualMemorySize()
	 */
	/*[IF Sidecar19-SE]
	@Deprecated(forRemoval = true, since = "1.8")
	/*[ELSE]*/
	@Deprecated
	/*[ENDIF]*/
	public long getProcessVirtualMemorySize();

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
	public long getTotalSwapSpaceSize();

	/**
	 * Returns the amount of free swap space in bytes.
	 *
	 * @return the amount of free swap space in bytes.
	 * @since   1.7
	 */
	public long getFreeSwapSpaceSize();

	/**
	 * Returns the "recent cpu usage" for the Java Virtual Machine process.
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
