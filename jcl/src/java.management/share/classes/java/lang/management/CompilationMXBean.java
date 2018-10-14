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
 * The management and monitoring interface for the virtual machine's compilation
 * functionality.
 * <p>
 * If the virtual machine has a compilation system enabled, precisely one
 * instance of this interface will be made available to management clients.
 * Otherwise, there will be no instances of this <code>MXBean</code> available.
 * </p>
 * <p>
 * Accessing this <code>MXBean</code> can be done in one of three ways.
 * <ol>
 * <li>Invoking the static ManagementFactory.getCompilationMXBean() method.
 * </li>
 * <li>Using a javax.management.MBeanServerConnection.</li>
 * <li>Obtaining a proxy MXBean from the static
 * {@link ManagementFactory#newPlatformMXBeanProxy}
 * method, passing in the string &quot;java.lang:type=Compilation&quot; for
 * the value of the second parameter.
 * </li>
 * </ol>
 */
public interface CompilationMXBean extends PlatformManagedObject {

	/**
	 * Returns the name of the virtual machine's Just In Time (JIT) compiler.
	 * 
	 * @return the name of the JIT compiler
	 */
	public String getName();

	/**
	 * If supported (see {@link #isCompilationTimeMonitoringSupported()}),
	 * returns the total number of <b>milliseconds </b> spent by the virtual
	 * machine performing compilations. The figure is taken over the lifetime of
	 * the virtual machine.
	 * 
	 * @return the compilation time in milliseconds
	 * @throws java.lang.UnsupportedOperationException
	 *             if the virtual machine does not support compilation
	 *             monitoring. This can be tested by calling the
	 *             {@link #isCompilationTimeMonitoringSupported()} method.
	 */
	public long getTotalCompilationTime();

	/**
	 * A boolean indication of whether or not the virtual machine supports the
	 * timing of its compilation facilities.
	 * 
	 * @return <code>true</code> if compilation timing is supported, otherwise
	 *         <code>false</code>.
	 */
	public boolean isCompilationTimeMonitoringSupported();

}
