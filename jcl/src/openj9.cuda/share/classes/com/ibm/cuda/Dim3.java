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
 * The {@code Dim3} class represents the dimensions of a cube.
 *
 * @see CudaGrid
 */
public final class Dim3 {

	/**
	 * The size of this cube in the x dimension.
	 */
	public int x;

	/**
	 * The size of this cube in the y dimension.
	 */
	public int y;

	/**
	 * The size of this cube in the z dimension.
	 */
	public int z;

	/**
	 * Creates a cube of dimension (x,1,1).
	 *
	 * @param x
	 *          the x dimension
	 */
	public Dim3(int x) {
		this(x, 1, 1);
	}

	/**
	 * Creates a cube of dimension (x,y,1).
	 *
	 * @param x
	 *          the x dimension
	 * @param y
	 *          the y dimension
	 */
	public Dim3(int x, int y) {
		this(x, y, 1);
	}

	/**
	 * Creates a cube of dimension (x,y,z).
	 *
	 * @param x
	 *          the x dimension
	 * @param y
	 *          the y dimension
	 * @param z
	 *          the z dimension
	 */
	public Dim3(int x, int y, int z) {
		super();
		this.x = x;
		this.y = y;
		this.z = z;
	}
}
