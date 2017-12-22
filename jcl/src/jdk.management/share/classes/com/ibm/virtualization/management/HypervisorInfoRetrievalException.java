/*[INCLUDE-IF Sidecar17]*/

package com.ibm.virtualization.management;

/*******************************************************************************
 * Copyright (c) 2012, 2013 IBM Corp. and others
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

/**
 * This exception class is thrown when retrieving Hypervisor information fails.
 * For exact cause one needs to inspect the exception string message capturing
 * the details by calling the toString() method.
 *
 * @author Dinakar Guniguntala
 * @since 1.7.1
 */
public class HypervisorInfoRetrievalException extends Exception {
	static final long serialVersionUID = -5646830947235093908L;

	/**
	 * Constructor for the exception class that takes a message string
	 * describing the exact nature of the exception.
	 *
	 * @param errMsg The string that describes the exact nature of the exception
	 * that occurred while retrieving Hypervisor info.
	 */
	public HypervisorInfoRetrievalException(String errMsg)
	{
		super(errMsg);
	}
}
