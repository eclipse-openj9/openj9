/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.exceptions;

import java.io.IOException;

import com.ibm.j9ddr.corereaders.memory.IProcess;

/**
 * When trying to automatically determine support for in service JVMs the architecture needs to be determined from the 
 * core file reader. This is a string which can change and this exception indicates that the string returned by the
 * reader was not recognised. 
 * 
 * @author adam
 *
 */
public class UnknownArchitectureException extends IOException {
	private static final long serialVersionUID = 1419668845640283554L;
	private final IProcess process;

	public UnknownArchitectureException() {
		super();
		process = null;
	}
	
	public UnknownArchitectureException(IProcess process) {
		super();
		this.process = process;
	}

	public UnknownArchitectureException(IProcess process, String message) {
		super(message);
		this.process = process;
	}

	public UnknownArchitectureException(IProcess process, Throwable cause) {
		initCause(cause);
		this.process = process;
	}

	public UnknownArchitectureException(IProcess process, String message, Throwable cause) {
		super(message);
		initCause(cause);
		this.process = process;
	}

	public IProcess getProcess() {
		return process;
	}
	
}
