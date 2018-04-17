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
 * The interface for managing and monitoring the virtual machine's garbage
 * collection functionality.
 * <p>
 * Multiple instances of this interface are available to clients. Each may be
 * distinguished by their separate <code>ObjectName</code> value.
 * </p>
 * <p>
 * Accessing this kind of <code>MXBean</code> can be done in one of three
 * ways.
 * <ol>
 * <li>Invoking the static
 * {@link ManagementFactory#getGarbageCollectorMXBeans()} method which returns a
 * {@link java.util.List} of all currently instantiated GarbageCollectorMXBeans.
 * </li>
 * <li>Using a {@link javax.management.MBeanServerConnection}.</li>
 * <li>Obtaining a proxy MXBean from the static
 * {@link ManagementFactory#newPlatformMXBeanProxy} method, passing in the
 * string &quot;java.lang:type=GarbageCollector,name= <i>unique collector's name
 * </i>&quot; for the value of the second parameter.</li>
 * </ol>
 *  
 * @since 1.5
 */
public interface GarbageCollectorMXBean extends MemoryManagerMXBean {

	/**
	 * Returns in a long the number of garbage collections carried out by the
	 * associated collector.
	 * 
	 * @return the total number of garbage collections that have been carried
	 *         out by the associated garbage collector.
	 */
	public long getCollectionCount();

	/**
	 * For the associated garbage collector, returns the total amount of time in
	 * milliseconds that it has spent carrying out garbage collection.
	 * 
	 * @return the number of milliseconds that have been spent in performing
	 *         garbage collection. This is a cumulative figure.
	 */
	public long getCollectionTime();

}
