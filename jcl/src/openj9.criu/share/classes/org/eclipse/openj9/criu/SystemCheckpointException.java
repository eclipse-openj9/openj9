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
 * An exception representing a failed system operation before checkpoint.
 */
public final class SystemCheckpointException extends JVMCRIUException {

	private static final long serialVersionUID = 5269852717246242036L;

	/**
	 * Creates a SystemCheckpointException with the specified message and a default error code.
	 *
	 * @param message   the message
	 */
	public SystemCheckpointException(String message) {
		super(message, 0);
	}

	/**
	 * Creates a SystemCheckpointException with the specified message and error code.
	 *
	 * @param message   the message
	 * @param errorCode the error code
	 */
	public SystemCheckpointException(String message, int errorCode) {
		super(message, errorCode);
	}

	/**
	 * Creates a SystemCheckpointException with the specified message, error code and throwable that caused the exception.
	 *
	 * @param message   the message
	 * @param errorCode the error code
	 * @param causedBy  throwable that caused the exception
	 */
	public SystemCheckpointException(String message, int errorCode, Throwable causedBy) {
		super(message, errorCode, causedBy);
	}
}
