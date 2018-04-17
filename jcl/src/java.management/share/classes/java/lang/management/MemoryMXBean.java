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
 * The management and monitoring interface for a virtual machine's memory
 * system.
 * <p>
 * Precisely one instance of this interface will be made available to management
 * clients.
 * </p>
 * <p>
 * Accessing this <code>MXBean</code> can be done in one of three ways.
 * <ol>
 * <li>Invoking the static {@link ManagementFactory#getMemoryMXBean} method.
 * </li>
 * <li>Using a {@link javax.management.MBeanServerConnection}.</li>
 * <li>Obtaining a proxy MXBean from the static
 * {@link ManagementFactory#newPlatformMXBeanProxy} method, passing in the
 * string &quot;java.lang:type=ClassLoading&quot; for the value of the second
 * parameter.</li>
 * </ol>
 * 
 */
public interface MemoryMXBean extends PlatformManagedObject {

	/**
	 * Requests the virtual machine to run the system garbage collector.
	 */
	public void gc();

	/**
	 * Returns the current memory usage of the heap for both live objects and
	 * for objects no longer in use which are awaiting garbage collection.
	 * 
	 * @return an instance of {@link MemoryUsage} which can be interrogated by
	 *         the caller.
	 */
	public MemoryUsage getHeapMemoryUsage();

	/**
	 * Returns the current non-heap memory usage for the virtual machine.
	 * 
	 * @return an instance of {@link MemoryUsage} which can be interrogated by
	 *         the caller.
	 */
	public MemoryUsage getNonHeapMemoryUsage();

	/**
	 * Returns the number of objects in the virtual machine that are awaiting
	 * finalization. The returned value should only be used as an approximate
	 * guide.
	 * 
	 * @return the number of objects awaiting finalization.
	 */
	public int getObjectPendingFinalizationCount();

	/**
	 * Returns a boolean indication of whether or not the memory system is
	 * producing verbose output.
	 * 
	 * @return <code>true</code> if verbose output is being produced ;
	 *         <code>false</code> otherwise.
	 */
	public boolean isVerbose();

	/**
	 * Updates the verbose output setting of the memory system.
	 * 
	 * @param value
	 *            <code>true</code> enables verbose output ;
	 *            <code>false</code> disables verbose output.
	 * @throws SecurityException
	 *             if a {@link SecurityManager} is being used and the caller
	 *             does not have the <code>ManagementPermission</code> value
	 *             of &quot;control&quot;.
	 * @see ManagementPermission
	 */
	public void setVerbose(boolean value);

}
