/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2016, 2017 IBM Corp. and others
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

/**
 * Interface provides platform-specific management utilities on Unix and Unix-like
 * operating systems.
 * 
 * @since 1.8
 */
public interface UnixOperatingSystemMXBean extends com.sun.management.UnixOperatingSystemMXBean, OperatingSystemMXBean {

	/**
	 * Returns the maximum number of file descriptors that can be opened in a process.
	 * 
	 * @return The maximum number of file descriptors that can be opened in a process or
	 * -1, if an error occurred while obtaining this. If the operating system doesn't have 
	 * any limits configured, Long.MAX_VALUE is returned.
	 * 
	 * @since   1.8
	 */
	public long getMaxFileDescriptorCount();

	/**
	 * Returns the current number of file descriptors that are in opened state.
	 * 
	 * @return The current number of file descriptors that are in opened state or
	 * -1, if an error occurred while obtaining this.
	 * 
	 * @since   1.8
	 */
	public long getOpenFileDescriptorCount();

}
