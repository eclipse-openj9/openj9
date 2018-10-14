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
 * {@code CudaJitInputType} identifies the type of input being provided to
 * {@code CudaLinker#add(...)}.
 */
public enum CudaJitInputType {

	/**
	 * Compiled device-class-specific device code.
	 * <p>
	 * Applicable options: none
	 */
	CUBIN(0),

	/**
	 * Bundle of multiple cubins and/or PTX of some device code.
	 * <p>
	 * Applicable options: PTX compiler options, CudaJitOption.FALLBACK_STRATEGY
	 */
	FATBINARY(1),

	/**
	 * Archive of host objects with embedded device code.
	 * <p>
	 * Applicable options: PTX compiler options, CudaJitOption.FALLBACK_STRATEGY
	 */
	LIBRARY(2),

	/**
	 * Host object with embedded device code.
	 * <p>
	 * Applicable options: PTX compiler options, CudaJitOption.FALLBACK_STRATEGY
	 */
	OBJECT(3),

	/**
	 * PTX source code.
	 * <p>
	 * Applicable options: PTX compiler options
	 */
	PTX(4);

	final int nativeValue;

	private CudaJitInputType(int value) {
		this.nativeValue = value;
	}
}
