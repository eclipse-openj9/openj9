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
 * {@code CudaJitTarget} identifies the target compute device class
 * for linking and loading modules.
 */
public enum CudaJitTarget {

	/** Compute device class 1.0. */
	Compute10(10),

	/** Compute device class 1.1. */
	Compute11(11),

	/** Compute device class 1.2. */
	Compute12(12),

	/** Compute device class 1.3. */
	Compute13(13),

	/** Compute device class 2.0. */
	Compute20(20),

	/** Compute device class 2.1. */
	Compute21(21),

	/** Compute device class 3.0. */
	Compute30(30),

	/** Compute device class 3.5. */
	Compute35(35);

	/* package */final int nativeValue;

	private CudaJitTarget(int value) {
		this.nativeValue = value;
	}
}
