/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2014, 2016 IBM Corp. and others
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
package com.ibm.gpu;

/**
 * This exception is thrown when there is an attempt to perform a sort on GPU and the sort operation fails.
 * Specific information is provided in the exception message.
 */
public final class GPUSortException extends Exception {

	private static final long serialVersionUID = -7798837158762951342L;

	/**
	 * Creates a new GPUSortException with a provided message.
	 * 
	 * @param message The message to be provided.
	 */
	public GPUSortException(String message) {
		super(message);
	}

	/**
	 * Creates a new GPUSortException with a provided message and cause.
	 * 
	 * @param message The message to be provided.
	 * @param cause The cause of this exception.
	 */
	public GPUSortException(String message, Throwable cause) {
		super(message, cause);
	}

}
