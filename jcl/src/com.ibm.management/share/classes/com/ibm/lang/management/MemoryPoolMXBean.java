/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2005, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package com.ibm.lang.management;

import java.lang.management.MemoryUsage;

/**
 * The IBM-specific interface for managing and monitoring the virtual machine's
 * memory pools.
 *
 * @since 1.5
 */
public interface MemoryPoolMXBean extends java.lang.management.MemoryPoolMXBean {

	/**
	 * If supported by the virtual machine, returns a {@link java.lang.management.MemoryUsage} which
	 * encapsulates this memory pool's memory usage <em>before</em> the most
	 * recent run of the garbage collector. No garbage collection will be
	 * actually occur as a result of this method getting called.
	 * <p>
	 * The method will return a <code>null</code> if the virtual machine does
	 * not support this type of functionality.
	 * </p>
	 * <h2>MBeanServer access:</h2>
	 * The return value will be mapped to a
	 * {@link javax.management.openmbean.CompositeData} with attributes as
	 * specified in {@link java.lang.management.MemoryUsage}.
	 * 
	 * @return a {@link java.lang.management.MemoryUsage} containing the usage details for the memory
	 *         pool just before the most recent collection occurred.
	 */
	public MemoryUsage getPreCollectionUsage();

}
