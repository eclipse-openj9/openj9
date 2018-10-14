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

import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.List;
import java.util.Map;

/**
 * The management and monitoring interface for the runtime system of the virtual
 * machine.
 * <p>
 * Precisely one instance of this interface will be made available to management
 * clients.
 * </p>
 * <p>
 * Accessing this <code>MXBean</code> can be done in one of three ways.
 * <ol>
 * <li>Invoking the static ManagementFactory.getRuntimeMXBean() method.</li>
 * <li>Using a javax.management.MBeanServerConnection.</li>
 * <li>Obtaining a proxy MXBean from the static
 * {@link ManagementFactory#newPlatformMXBeanProxy} method, passing in
 * &quot;java.lang:type=Runtime&quot; for the value of the second parameter.
 * </li>
 * </ol>
 *  
 * @since 1.5
 */
public interface RuntimeMXBean extends PlatformManagedObject {

	/**
	 * If bootstrap class loading is supported by the virtual machine, returns a
	 * string containing the full bootstrap class path used by the boot class
	 * loader to locate and load class files.
	 * <p>
	 * An indication of whether or not the virtual machine supports a boot class
	 * loader mechanism can be found from invoking the
	 * {@link #isBootClassPathSupported()} method.
	 * </p>
	 * 
	 * @return the bootstrap classpath with each entry separated by the path
	 *         separator character corresponding to the underlying operating
	 *         system.
	 * @throws UnsupportedOperationException
	 *             if the virtual machine does not support boot class loading.
	 * @throws SecurityException
	 *             if there is a security manager in effect and the caller does
	 *             not have {@link ManagementPermission} of &quot;monitor&quot;.
	 */
	public String getBootClassPath();

	/**
	 * Returns the class path string used by the system class loader to locate
	 * and load class files. The value is identical to that which would be
	 * obtained from a call to {@link System#getProperty(java.lang.String)}
	 * supplying the value &quot;java.class.path&quot; for the key.
	 * 
	 * @return the system classpath with each entry separated by the path
	 *         separator character corresponding to the underlying operating
	 *         system.
	 * @throws SecurityException
	 *             if there is a security manager in operation and the caller
	 *             does not have permission to check system properties.
	 * @see System#getProperty(java.lang.String)
	 */
	public String getClassPath();

	/**
	 * Returns a list of all of the input arguments passed to the virtual
	 * machine on start-up. This will <i>not </i> include any input arguments
	 * that are passed into the application's <code>main(String[] args)</code>
	 * method.
	 * 
	 * @return a list of strings, each one containing an argument to the virtual
	 *         machine. If no virtual machine arguments were passed in at
	 *         start-up time then this will be an empty list.
	 */
	public List<String> getInputArguments();

	/**
	 * Returns the Java library path that will be used by the virtual machine to
	 * locate and load libraries. The value is identical to that which would be
	 * obtained from a call to {@link System#getProperty(java.lang.String)}
	 * supplying the value &quot;java.library.path&quot; for the key.
	 * 
	 * @return the Java library path with each entry separated by the path
	 *         separator character corresponding to the underlying operating
	 *         system.
	 * @throws SecurityException
	 *             if there is a security manager in operation and the caller
	 *             does not have permission to check system properties.
	 * @see System#getProperty(java.lang.String)
	 */
	public String getLibraryPath();

	/**
	 * Returns a string containing the management interface specification
	 * version that the virtual machine meets.
	 * 
	 * @return the version of the management interface specification adhered to
	 *         by the virtual machine.
	 */
	public String getManagementSpecVersion();

	/**
	 * Returns the string name of this virtual machine. This value may be
	 * different for each particular running virtual machine.
	 * 
	 * @return the name of this running virtual machine.
	 */
	public String getName();

	/*[IF Java10]*/
	/**
	 * Returns the process ID (PID) of the current running Java virtual machine.
	 * 
	 * @return the process ID of the current running JVM
	 * 
	 * @since 10
	 */
	@SuppressWarnings("boxing")
	default long getPid() {
		return AccessController.doPrivileged(new PrivilegedAction<Long>() {
			public Long run() {
				return ProcessHandle.current().pid();
			}
		});
	}
	/*[ENDIF]*/

	/**
	 * Returns the name of the Java virtual machine specification followed by
	 * this virtual machine. The value is identical to that which would be
	 * obtained from a call to {@link System#getProperty(java.lang.String)}
	 * supplying the value &quot;java.vm.specification.name&quot; for the key.
	 * 
	 * @return the name of the Java virtual machine specification.
	 * @throws SecurityException
	 *             if there is a security manager in operation and the caller
	 *             does not have permission to check system properties.
	 * @see System#getProperty(java.lang.String)
	 */
	public String getSpecName();

	/**
	 * Returns the name of the Java virtual machine specification vendor. The
	 * value is identical to that which would be obtained from a call to
	 * {@link System#getProperty(java.lang.String)} supplying the value
	 * &quot;java.vm.specification.vendor&quot; for the key.
	 * 
	 * @return the name of the Java virtual machine specification vendor.
	 * @throws SecurityException
	 *             if there is a security manager in operation and the caller
	 *             does not have permission to check system properties.
	 * @see System#getProperty(java.lang.String)
	 */
	public String getSpecVendor();

	/**
	 * Returns the name of the Java virtual machine specification version. The
	 * value is identical to that which would be obtained from a call to
	 * {@link System#getProperty(java.lang.String)} supplying the value
	 * &quot;java.vm.specification.version&quot; for the key.
	 * 
	 * @return the Java virtual machine specification version.
	 * @throws SecurityException
	 *             if there is a security manager in operation and the caller
	 *             does not have permission to check system properties.
	 * @see System#getProperty(java.lang.String)
	 */
	public String getSpecVersion();

	/**
	 * Returns the time, in milliseconds, when the virtual machine was started.
	 * 
	 * @return the virtual machine start time in milliseconds.
	 */
	public long getStartTime();

	/**
	 * Returns a map of the names and values of every system property known to
	 * the virtual machine.
	 * 
	 * @return a map containing the names and values of every system property.
	 * @throws SecurityException
	 *             if there is a security manager in operation and the caller
	 *             does not have permission to check system properties.
	 */
	public Map<String, String> getSystemProperties();

	/**
	 * Returns the lifetime of the virtual machine in milliseconds.
	 * 
	 * @return the number of milliseconds the virtual machine has been running.
	 */
	public long getUptime();

	/**
	 * Returns the name of the Java virtual machine implementation. The value is
	 * identical to that which would be obtained from a call to
	 * {@link System#getProperty(java.lang.String)} supplying the value
	 * &quot;java.vm.name&quot; for the key.
	 * 
	 * @return the name of the Java virtual machine implementation.
	 * @throws SecurityException
	 *             if there is a security manager in operation and the caller
	 *             does not have permission to check system properties.
	 * @see System#getProperty(java.lang.String)
	 */
	public String getVmName();

	/**
	 * Returns the name of the Java virtual machine implementation vendor. The
	 * value is identical to that which would be obtained from a call to
	 * {@link System#getProperty(java.lang.String)} supplying the value
	 * &quot;java.vm.vendor&quot; for the key.
	 * 
	 * @return the name of the Java virtual machine implementation vendor.
	 * @throws SecurityException
	 *             if there is a security manager in operation and the caller
	 *             does not have permission to check system properties.
	 * @see System#getProperty(java.lang.String)
	 */
	public String getVmVendor();

	/**
	 * Returns the version of the Java virtual machine implementation. The value
	 * is identical to that which would be obtained from a call to
	 * {@link System#getProperty(java.lang.String)} supplying the value
	 * &quot;java.vm.version&quot; for the key.
	 * 
	 * @return the version of the Java virtual machine implementation.
	 * @throws SecurityException
	 *             if there is a security manager in operation and the caller
	 *             does not have permission to check system properties.
	 * @see System#getProperty(java.lang.String)
	 */
	public String getVmVersion();

	/**
	 * Returns a boolean indication of whether or not the virtual machine
	 * supports a bootstrap class loading mechanism.
	 * 
	 * @return <code>true</code> if supported, <code>false</code> otherwise.
	 */
	public boolean isBootClassPathSupported();

}
