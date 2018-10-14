/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2013, 2018 IBM Corp. and others
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
package com.ibm.cuda;

/**
 * The {@code CudaException} class represents an unusual or unexpected response
 * from a CUDA-capable device.
 */
public final class CudaException extends Exception {

	private static final long serialVersionUID = 6037047583818305980L;

	public final int code;

	/**
	 * Creates a new exception object.
	 *
	 * @param code
	 *          the error code
	 *
	 * @see CudaError
	 */
	public CudaException(int code) {
		super();
		this.code = code;
	}

	/**
	 * Returns a human-readable string describing the problem represented
	 * by this exception.
	 */
	@Override
	public String getMessage() {
		String message = Cuda.getErrorMessage(code);

		if (message == null) {
			message = "CUDA runtime error " + code; //$NON-NLS-1$
		}

		return message;
	}
}
