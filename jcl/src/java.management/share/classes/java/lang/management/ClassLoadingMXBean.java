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
 * The management and monitoring interface for the virtual machine's class
 * loading functionality.
 * <p>
 * Precisely one instance of this interface will be made
 * available to management clients.
 * </p>
 * <p>
 * Accessing this <code>MXBean</code> can be done in one of three ways.
 * <ol>
 * <li>Invoking the static ManagementFactory.getClassLoadingMXBean() method.</li>
 * <li>Using a javax.management.MBeanServerConnection.</li>
 * <li>Obtaining a proxy MXBean from the static
 * {@code ManagementFactory.newPlatformMXBeanProxy(MBeanServerConnection connection,
 * String mxbeanName, Class<T> mxbeanInterface())} method, passing in
 * &quot;java.lang:type=ClassLoading&quot; for the value of the mxbeanName
 * parameter.</li>
 * </ol>
 * 
 */
public interface ClassLoadingMXBean extends PlatformManagedObject {

	/**
	 * Returns the number of classes currently loaded by the virtual machine.
	 * @return the number of loaded classes
	 */
	public int getLoadedClassCount();

	/**
	 * Returns a figure for the total number of classes that have been
	 * loaded by the virtual machine during its lifetime.  
	 * @return the total number of classes that have been loaded
	 */
	public long getTotalLoadedClassCount();

	/**
	 * Returns a figure for the total number of classes that have 
	 * been unloaded by the virtual machine over its lifetime.
	 * @return the total number of unloaded classes
	 */
	public long getUnloadedClassCount();

	/**
	 * Returns a boolean indication of whether the virtual 
	 * machine's class loading system is producing verbose output. 
	 * @return true if running in verbose mode
	 */
	public boolean isVerbose();

	/**
	 * Updates the virtual machine's class loading system to 
	 * operate in verbose or non-verbose mode. 
	 * @param value true to put the class loading system into verbose
	 * mode, false to take the class loading system out of verbose mode.
	 */
	public void setVerbose(boolean value);

}
