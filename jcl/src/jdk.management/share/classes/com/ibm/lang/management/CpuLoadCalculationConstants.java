/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2001, 2016 IBM Corp. and others
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
 * Constants used by getProcessCpuLoad() and getSystemCpuLoad() methods.
 * 
 * @author Sridevi
 *
 * @since 1.7.1
 */
public interface CpuLoadCalculationConstants {

	/**
	 * Indicates transient error in the port library call.
	 */
	int ERROR_VALUE = -1;

	/**
	 * Indicates this function not supported due to insufficient user privilege.
	 */
	int INSUFFICIENT_PRIVILEGE = -2;

	/**
	 * Indicates this function is not supported at all on this platform.
	 */
	int UNSUPPORTED_VALUE = -3;

	/**
	 * Indicates unexpected internal error.
	 */
	int INTERNAL_ERROR = -4;

	/**
	 *  The minimum time between successive calls required
	 *  to obtain a valid CPU load measurement.
	 *  
	 *  10 ms in nanoseconds.
	 */
	long MINIMUM_INTERVAL = 10000000;

}
