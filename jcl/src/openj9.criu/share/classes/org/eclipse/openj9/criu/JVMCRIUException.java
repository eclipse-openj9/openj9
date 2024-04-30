/*[INCLUDE-IF CRIU_SUPPORT]*/
/*
 * Copyright IBM Corp. and others 2021
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package org.eclipse.openj9.criu;

/**
 * Abstract CRIU exception superclass. Contains an error code returned from a
 * failed operation.
 */
public abstract class JVMCRIUException extends RuntimeException {
	private static final long serialVersionUID = 4486137934620495516L;
	protected int errorCode;

	protected JVMCRIUException(String message, int errorCode) {
		super(message);
		this.errorCode = errorCode;
	}

	protected JVMCRIUException(String message, int errorCode, Throwable causedBy) {
		super(message, causedBy);
		this.errorCode = errorCode;
	}

	/**
	 * Returns the error code.
	 *
	 * @return errorCode
	 */
	public int getErrorCode() {
		return errorCode;
	}

	/**
	 * Sets the error code.
	 *
	 * @param errorCode the value to set to
	 */
	public void setErrorCode(int errorCode) {
		this.errorCode = errorCode;
	}
}
