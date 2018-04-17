/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2005, 2018 IBM Corp. and others
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

/**
 * The management and monitoring interface for the operating system where the
 * virtual machine is running.
 * <p>
 * Precisely one instance of this interface will be made available to management
 * clients.
 * </p>
 * <p>
 * Accessing this <code>MXBean</code> can be done in one of three ways.
 * <ol>
 * <li>Invoking the static ManagementFactory.getOperatingSystemMXBean() method.
 * </li>
 * <li>Using a javax.management.MBeanServerConnection.</li>
 * <li>Obtaining a proxy MXBean from the static
 * {@link ManagementFactory#newPlatformMXBeanProxy} method, passing in
 * &quot;java.lang:type=OperatingSystem&quot; for the value of the second
 * parameter.</li>
 * </ol>
 *  
 * @since 1.5
 */
public interface OperatingSystemMXBean extends PlatformManagedObject {

	/**
	 * Returns a unique string identifier for the architecture of the underlying
	 * operating system. The identifier value is identical to that which would
	 * be obtained from a call to {@link System#getProperty(java.lang.String)}
	 * supplying the value &quot;os.arch&quot; for the key.
	 * 
	 * @return the identifier for the operating system architecture.
	 * @throws SecurityException
	 *             if there is a security manager in operation and the caller
	 *             does not have permission to check system properties.
	 * @see System#getProperty(java.lang.String)
	 */
	public String getArch();

	/**
	 * Returns the number of processors that are available for the virtual
	 * machine to run on. The information returned from this method is identical
	 * to that which would be received from a call to
	 * {@link Runtime#availableProcessors()}.
	 * 
	 * @return the number of available processors.
	 */
	public int getAvailableProcessors();

	/**
	 * Returns the name of the underlying operating system. The value is
	 * identical to that which would be obtained from a call to
	 * {@link System#getProperty(java.lang.String)} supplying the value
	 * &quot;os.name&quot; for the key.
	 * 
	 * @return the name of the operating system.
	 * @throws SecurityException
	 *             if there is a security manager in operation and the caller
	 *             does not have permission to check system properties.
	 * @see System#getProperty(java.lang.String)
	 */
	public String getName();

	/**
	 * Returns the version string for the underlying operating system. The value
	 * is identical to that which would be obtained from a call to
	 * {@link System#getProperty(java.lang.String)} supplying the value
	 * &quot;os.version&quot; for the key.
	 * 
	 * @return the version of the operating system.
	 * @throws SecurityException
	 *             if there is a security manager in operation and the caller
	 *             does not have permission to check system properties.
	 * @see System#getProperty(java.lang.String)
	 */
	public String getVersion();

	/**
	 * Returns a double value which holds the system load average calculated for
	 * the minute preceding the call, where <i>system load average</i> is taken
	 * to mean the following:
	 * <p>
	 * the time-averaged value of the sum of the number of runnable entities
	 * running on the available processors and the number of runnable entities
	 * ready and queued to run on the available processors. The averaging
	 * technique adopted can vary depending on the underlying operating system.
	 * 
	 * @return normally, the system load average as a double. If the system load
	 *         average is not obtainable (e.g. because the calculation may
	 *         involve an unacceptable performance impact) then a negative value
	 *         is returned.
	 * @since 1.6        
	 */
	public double getSystemLoadAverage();

}
