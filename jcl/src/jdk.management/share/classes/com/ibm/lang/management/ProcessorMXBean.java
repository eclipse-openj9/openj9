/*[INCLUDE-IF Sidecar17 & !Sidecar19-SE]*/
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

import java.lang.management.PlatformManagedObject;

/**
 * Interface for managing Processor resources.
 * 
 * <br>
 * <table border="1">
 * <caption><b>Usage example for the {@link ProcessorMXBean}</b></caption>
 * <tr> <td>
 * <pre>
 * <code>
 *   ...
 *   try {
 *	mxbeanName = new ObjectName("com.ibm.lang.management:type=Processor");
 *   } catch (MalformedObjectNameException e) {
 *	// Exception Handling
 *   }
 *   try {
 *	MBeanServer mbeanServer = ManagementFactory.getPlatformMBeanServer();
 *	if (true != mbeanServer.isRegistered(mxbeanName)) {
 *	   // ProcessorMXBean not registered
 *	}
 *	ProcessorMXBean procBean = JMX.newMXBeanProxy(mbeanServer, mxbeanName, ProcessorMXBean.class); 
 *   } catch (Exception e) {
 *	// Exception Handling
 *   }
 * </code>
 * </pre></td></tr></table>
 * @author Bjorn Vardal
 * @since 1.7.1
 */
public interface ProcessorMXBean extends PlatformManagedObject {
	
	/**
	 * Returns the number of physical CPUs as seen by the operating system
	 * where the Java Virtual Machine is running.
	 * 
	 * @return the number of physical CPUs.
	 */
	public int getNumberPhysicalCPUs();

	/**
	 * Returns the number of online CPUs as seen be the operating system
	 * where the JVM is running.
	 * 
	 * @return the number of online CPUs.
	 */
	public int getNumberOnlineCPUs();

	/**
	 * Returns the number of CPUs the process is bound to on the operating
	 * system level. 
	 * 
	 * @return the number of CPUs the process is bound to.
	 */
	public int getNumberBoundCPUs();

	/**
	 * Returns the target number of CPUs for the process. This is normally  
	 * equal to {@link #getNumberBoundCPUs()}, but is overridden by active
	 * CPUs when it is set, e.g. using {@link #setNumberActiveCPUs(int)}.
	 * 
	 * @return	the number of CPUs the process is entitled to.
	 */
	public int getNumberTargetCPUs();

	/**
	 * Sets the number of CPUs that the process is specified to use.
	 * {@link #getNumberTargetCPUs()}.
	 * 
	 * @param number
	 * 			The number of CPUs to specify the process to use. The process
	 * 			will behave as if <code>number</code> CPUs are available. If
	 *			this is set to 0, it will reset the number of CPUs specified
	 *			and the JVM will use the the number of CPUs detected on the system.
	 */
	public void setNumberActiveCPUs(int number);
}
