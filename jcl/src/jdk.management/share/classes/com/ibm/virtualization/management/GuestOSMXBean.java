/*[INCLUDE-IF Sidecar18-SE]*/

package com.ibm.virtualization.management;

/*******************************************************************************
 * Copyright (c) 2013, 2013 IBM Corp. and others
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

import java.lang.management.*;

/**
 * This interface provides Processor and Memory usage statistics of the
 * Guest (Virtual Machine(VM)/Logical Partition(LPAR)) as seen by the Hypervisor Host.
 * <b>Where there are multiple levels of Hypervisor, only the top level Hypervisor information is returned.</b>
 * These are the supported Hypervisor and Guest Operating System combinations:
 * <ol>
 *      <li>Windows and Linux on VMWare ESXi.
 *      <ul>
 *          <li><a href="http://www.vmware.com/support/developer/guest-sdk" target="_blank">VMware GuestSDK</a>
 *              (Generally packaged with VMWare tools) must be installed in the Guest Operating System.
 *      </ul>
 *      <li>AIX and Linux on PowerVM.
 *      <li>Linux on PowerKVM.
 *      <ul>
 *          <li>Guest Operating System memory usage statistics are not available on Linux for PowerKVM.
 *      </ul>
 *      <li>z/OS and Linux on z/VM.
 *      <ul>
 *          <li>hypfs filesystem (s390_hypfs) must be mounted on Linux on z/VM. The userid that runs the Java
 *              process must have read and write access.
 *      </ul>
 * </ol>
 * <br>
 * <table border="1">
 * <caption><b>Usage example for the {@link GuestOSMXBean}</b></caption>
 * <tr> <td>
 * <pre>
 * {@code
 *   ...
 *   try {
 *	mxbeanName = new ObjectName("com.ibm.virtualization.management:type=GuestOS");
 *   } catch (MalformedObjectNameException e) {
 *	// Exception Handling
 *   }
 *   try {
 *	MBeanServer mbeanServer = ManagementFactory.getPlatformMBeanServer();
 *	if (true != mbeanServer.isRegistered(mxbeanName)) {
 *	   // GuestOSMXBean not registered
 *	}
 *	GuestOSMXBean guestBean = JMX.newMXBeanProxy(mbeanServer, mxbeanName, GuestOSMXBean.class); 
 *   } catch (Exception e) {
 *	// Exception Handling
 *   }
 * }
 * </pre></td></tr></table>
 *
 * @since 1.7.1
 */
public interface GuestOSMXBean extends PlatformManagedObject {

	/**
	 * Snapshot of the Guest Processor usage statistics as seen by the Hypervisor, returned as a
	 * {@link GuestOSProcessorUsage} object. The statistics are an
	 * aggregate across all physical CPUs assigned to the Guest by the Hypervisor.
	 *
	 * @param gpUsage User provided {@link GuestOSProcessorUsage} object.
	 *
	 * @return The updated {@link GuestOSProcessorUsage} object.
	 *
	 * @throws NullPointerException if a null reference is passed.
	 * @throws GuestOSInfoRetrievalException if it failed to obtain usage statistics.
	 *
	 * <p>In case of an exception, the handler code can use toString() on the exception code
	 * to obtain a description of the exception.
	 */
	public GuestOSProcessorUsage retrieveProcessorUsage(GuestOSProcessorUsage gpUsage)
		throws NullPointerException, GuestOSInfoRetrievalException;

	/**
	 * Function instantiates a {@link GuestOSProcessorUsage} object and populates it with the
	 * current snapshot of Processor Usage statistics of the Guest as seen by the Hypervisor. 
	 * The statistics are an aggregate across all physical CPUs assigned to the Guest by the Hypervisor.
	 *
	 * @return The new {@link GuestOSProcessorUsage} object.
	 *
	 * @throws GuestOSInfoRetrievalException if it failed to obtain usage statistics.
	 *
	 * <p>In case of an exception, the handler code can use toString() on the exception code
	 * to obtain a description of the exception.
	 */
	public GuestOSProcessorUsage retrieveProcessorUsage()
		throws GuestOSInfoRetrievalException;

	/**
	 * Snapshot of the Guest Memory usage statistics as seen by the Hypervisor, returned as
	 * a {@link GuestOSMemoryUsage} object.
	 *
	 * @param gmUsage User provided {@link GuestOSMemoryUsage} object.
	 *
	 * @return The updated {@link GuestOSMemoryUsage} object.
	 *
	 * @throws NullPointerException if a null reference is passed.
	 * @throws GuestOSInfoRetrievalException if it failed to obtain usage statistics.
	 *
	 * <p>In case of an exception, the handler code can use toString() on the exception code
	 * to obtain a description of the exception.
	 */
	public GuestOSMemoryUsage retrieveMemoryUsage(GuestOSMemoryUsage gmUsage)
		throws NullPointerException, GuestOSInfoRetrievalException;

	/**
	 * Function instantiates a {@link GuestOSMemoryUsage} object and populates it with the
	 * current snapshot of Memory Usage statistics of the Guest as seen by the Hypervisor.
	 *
	 * @return The new {@link GuestOSMemoryUsage} object.
	 * 
	 * @throws GuestOSInfoRetrievalException if it failed to obtain usage statistics.
	 * 
	 * <p>In case of an exception, the handler code can use toString() on the exception code
	 * to obtain a description of the exception.
	 */
	public GuestOSMemoryUsage retrieveMemoryUsage()
		throws GuestOSInfoRetrievalException;
}
