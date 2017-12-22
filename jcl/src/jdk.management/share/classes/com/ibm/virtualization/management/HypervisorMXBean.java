/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2012, 2014 IBM Corp. and others
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

import java.lang.management.PlatformManagedObject;

/**
 * This interface provides information on whether the current Operating System is running directly on physical hardware
 * or running as a Guest (Virtual Machine (VM)/Logical Partition (LPAR)) on top of a Hypervisor.
 * If the Operating System is running as a Guest, the interface provides information about the Hypervisor.
 * <b>Where there are multiple levels of Hypervisor, only the top level Hypervisor information is returned.</b>
 * The supported Hypervisors and the platforms on which they are supported:
 * <ol>
 *     <li>Linux and Windows on VMWare.
 *     <li>Linux and Windows on KVM.
 *     <li>Windows on Hyper-V.
 *     <li>AIX and Linux on PowerVM.
 *     <li>Linux and z/OS on z/VM.
 *     <li>Linux and z/OS on PR/SM.
 * </ol>
 * <p>On some hardware, Hypervisor detection might fail, even if the Hypervisor and Operating System combination
 * is supported. In this case you can set an Environment variable - IBM_JAVA_HYPERVISOR_SETTINGS that forces the
 * JVM to recognize the Hypervisor.
 * <ul>
 *      <li>For example: On Windows to set the Hypervisor as VMWare,<br>
 *          set IBM_JAVA_HYPERVISOR_SETTINGS="DefaultName=VMWare"
 *      <li>Other valid Hypervisor name strings (case insensitive):
 *      <ul>
 *          <li>VMWare, Hyper-V, KVM, PowerVM, PR/SM, z/VM.
 *      </ul>
 *      <li>If the actual Hypervisor and the one provided through the Environment variable differ, the results are
 *          indeterminate.
 * </ul>
 * <br>
 * <table border="1">
 * <caption><b>Usage example for the {@link HypervisorMXBean}</b></caption>
 * <tr> <td> <pre>
 * {@code
 *   ...
 *   try {
 *	mxbeanName = new ObjectName("com.ibm.virtualization.management:type=Hypervisor");
 *   } catch (MalformedObjectNameException e) {
 *	// Exception Handling
 *   }
 *   try {
 *	MBeanServer mbeanServer = ManagementFactory.getPlatformMBeanServer();
 *	if (true != mbeanServer.isRegistered(mxbeanName)) {
 *	   // HypervisorMXBean not registered
 *	}
 *	HypervisorMXBean hypBean = JMX.newMXBeanProxy(mbeanServer, mxbeanName, HypervisorMXBean.class); 
 *   } catch (Exception e) {
 *	// Exception Handling
 *   }
 * }
 * </pre></td></tr></table>
 *
 * @since 1.7.1
 */
public interface HypervisorMXBean extends PlatformManagedObject {

	/**
	 * Indicates if the Operating System is running on a Hypervisor or not.
	 * 
	 * @return true if running on a Hypervisor, false otherwise.

	 * @throws UnsupportedOperationException if the underlying Hypervisor is unsupported.
	 * @throws HypervisorInfoRetrievalException if there is an error during Hypervisor detection.
	 */
	public boolean isEnvironmentVirtual() throws UnsupportedOperationException, HypervisorInfoRetrievalException;

	/**
	 * Returns the vendor of the Hypervisor if running in a virtualized environment.
	 * 
	 * @return string identifying the vendor of the Hypervisor if running under
	 *         Hypervisor, null otherwise.
	 */
	public String getVendor();
}
