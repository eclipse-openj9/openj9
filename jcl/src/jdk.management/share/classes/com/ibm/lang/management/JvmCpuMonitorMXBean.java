/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2014, 2016 IBM Corp. and others
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

import java.lang.management.PlatformManagedObject;

/**
 * <p>
 * This interface provides APIs to obtain JVM CPU usage information in terms of thread categories. APIs are also available to
 * get and set the thread category.
 * <ol>
 *     <li>The top level thread categories are "System-JVM", "Application", and "Resource-Monitor".
 *     <li>The "System-JVM" category is further divided into "GC", "JIT", and "Other" and is an aggregate of the three categories.
 *     <li>The "Application" category can be further divided, if required, in to a maximum of five user defined categories.
 *         These are called "Application-User1" through to "Application-User5".
 *     <li>The application can designate any of its threads to be part of any user defined category.
 *     <li>The application can also designate any of its threads as a "Resource-Monitor".
 *         "Resource-Monitor" threads are special because they do not participate in idle accounting.
 *         This means that any CPU usage of these threads does not count towards determining the state of the application.
 *     <li>A thread can be part of only one category at any given time but can change categories any number of times during its timeline.
 *     <li>The usage information is in microseconds and increases monotonically. 
 *     <li>The CPU usage information consists of the following data:
 *     <ol>
 *         <li>Attached threads that are live.
 *         <li>Attached threads that have previously exited.
 *         <li>Threads attached to the JVM for the period that they were attached.
 *     </ol>
 *     <li>CPU usage information notes:
 *       <ol>
 *         <li>Unattached native threads are not reflected.
 *         <li>Finalizer slave threads are considered as "Application" threads.
 *         <li>Threads of any category that participate in GC activity are accounted for in the "GC" category for the duration of the
 *             activity if the -XX:-ReduceCPUMonitorOverhead is specified. See the user guide for more information on this option.
 *         <li>If the application thread switches categories, the CPU usage is measured against each category for the duration of
 *             time that the thread spends in that category.
 *       </ol>
 *     </li>
 * </ol>
 * This information is based on repeatedly checking the CPU usage in the following use case scenarios: 
 * <ol>
 *     <li>Monitoring application idle and active behavior.
 *     <li>Calculating the JVM Overhead over a specific interval.
 *     <li>Collecting transaction metrics for a specific set of application threads over a specific duration.
 * </ol>
 * <br>
 * <table border="1">
 * <caption><b>Usage example for the {@link JvmCpuMonitorMXBean}</b></caption>
 * <tr> <td>
 * <pre>
 * {@code
 *   ...
 *   try {
 *      mxbeanName = new ObjectName("com.ibm.lang.management:type=JvmCpuMonitor");
 *   } catch (MalformedObjectNameException e) {
 *      // Exception Handling
 *   }
 *   try {
 *      MBeanServer mbeanServer = ManagementFactory.getPlatformMBeanServer();
 *      if (true != mbeanServer.isRegistered(mxbeanName)) {
 *         // JvmCpuMonitorMXBean not registered
 *      }
 *      JvmCpuMonitorMXBean jcmBean = JMX.newMXBeanProxy(mbeanServer, mxbeanName, JvmCpuMonitorMXBean.class); 
 *   } catch (Exception e) {
 *      // Exception Handling
 *   }
 * }
 * </pre></td></tr>
 * </table>
 */
public interface JvmCpuMonitorMXBean extends PlatformManagedObject {
	
	/**
	 * This function updates the user provided <code>JvmCpuMonitorInfo</code> object
	 * with CPU usage statistics of the various thread categories.
	 * The statistics are an aggregate across all CPUs of the operating system.
	 *
	 * @param jcmInfo	User provided JvmCpuMonitorInfo object.
	 * 
	 * @return the updated JvmCpuMonitorInfo instance.
	 *
	 * @throws NullPointerException if a null reference is passed.
	 * @throws UnsupportedOperationException if CPU monitoring is disabled.
	 */
	public JvmCpuMonitorInfo getThreadsCpuUsage(JvmCpuMonitorInfo jcmInfo)
		throws NullPointerException, UnsupportedOperationException;

	/**
	 * This function creates a new {@link JvmCpuMonitorInfo} object and populates it
	 * with CPU usage statistics of the various thread categories.
	 * The statistics are an aggregate across all CPUs of the operating system.
	 * 
	 * @return the new <code>JvmCpuMonitorInfo</code> instance.
	 *
	 * @throws UnsupportedOperationException if CPU monitoring is disabled.
	 */
	public JvmCpuMonitorInfo getThreadsCpuUsage()
		throws UnsupportedOperationException;

	/**
	 * This function sets the thread category of the target thread.
	 * Valid categories are
	 * <ol><li>"Resource-Monitor"
	 * <li>"Application"
	 * <li>"Application-User1" through to "Application-User5"
	 * </ol>
	 * Some notes on the setting the thread categories
	 * <ol><li>"Application" threads cannot be changed to any "System-JVM" category.
	 * <li>Threads in the "System-JVM" category cannot be modified.
	 * <li>Once a thread is designated as "Resource-Monitor", it cannot be changed. 
	 * </ol>
	 * @param id The target thread id for which the type needs to be set.
	 * @param category The category of the target thread.
	 *
	 * @return -1 to indicate failure or 0 on success.
	 *
	 * @throws IllegalArgumentException if a value is passed for the thread category or thread id that is not valid.
	 */
	public int setThreadCategory(long id, String category)
		throws IllegalArgumentException;

	/**
	 * This function gets the current value of the thread category for the target thread.
	 *
	 * @param id The target thread id for which we need the thread category.
	 *
	 * @return NULL to indicate failure, else the category string of the target thread.
	 *
	 * @throws IllegalArgumentException if the thread id is not valid.
	 */
	public String getThreadCategory(long id)
		throws IllegalArgumentException;
}
