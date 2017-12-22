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
 * This exception is thrown when GPU operations fail due to configuration
 * or environment issues - for example, an invalid device has been specified
 * or we are running on an unsupported platform.
 */
public final class GPUConfigurationException extends Exception {

	private static final long serialVersionUID = 6716501495652292726L;

	/**
	 * Construct a new GPUConfigurationException with the provided message.
	 *
	 * @param message  The message to be provided.
	 */
	public GPUConfigurationException(String message) {
		super(message);
	}

	/**
	 * Construct a new GPUConfigurationException with the provided message
	 * and cause.
	 *
	 * @param message  The message to be provided.
	 * @param cause  The cause of this exception.
	 */
	public GPUConfigurationException(String message, Throwable cause) {
		super(message, cause);
	}

}
