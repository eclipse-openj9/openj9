/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2011, 2018 IBM Corp. and others
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
package com.ibm.jvm.dtfjview;

/**
 * Indicates that jdmpview failed to initialize
 * 
 * @author adam
 *
 */
public class JdmpviewInitException extends RuntimeException {
	private static final long serialVersionUID = -5317431018607771062L;
	private final int exitCode;
	
	public JdmpviewInitException(int exitCode) {
		super();
		this.exitCode = exitCode;
	}

	public JdmpviewInitException(int exitCode, String msg) {
		super(msg);
		this.exitCode = exitCode;
	}

	public JdmpviewInitException(int exitCode, Throwable e) {
		super(e);
		this.exitCode = exitCode;
	}

	public JdmpviewInitException(int exitCode, String msg, Throwable e) {
		super(msg, e);
		this.exitCode = exitCode;
	}

	public int getExitCode() {
		return exitCode;
	}

}
