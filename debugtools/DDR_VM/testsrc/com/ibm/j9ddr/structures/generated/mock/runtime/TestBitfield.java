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

package com.ibm.j9ddr.structures.generated.mock.runtime;

/**
 * Structure: TestBitfield
 *
 * Hacked to allow unit test of bitfield algorithm
 */

public final class TestBitfield {

	// VM Constants

	public static final boolean RUNTIME = true;

	public static final long SIZEOF;

	// Offsets

	public static final int _x1_s_;
	public static final int _x1_b_;
	public static final int _x2_s_;
	public static final int _x2_b_;
	public static final int _x3_s_;
	public static final int _x3_b_;
	public static final int _x4_s_;
	public static final int _x4_b_;
	public static final int _x5_s_;
	public static final int _x5_b_;
	public static final int _x6_s_;
	public static final int _x6_b_;
	public static final int _x7_s_;
	public static final int _x7_b_;
	public static final int _x8_s_;
	public static final int _x8_b_;

	 // Static Initializer
	static {
		SIZEOF = 0x0L;
		_x1_s_ = 0;
		_x1_b_ = 1;
		_x2_s_ = 1;
		_x2_b_ = 15;
		_x3_s_ = 16;
		_x3_b_ = 6;
		_x4_s_ = 22;
		_x4_b_ = 10;
		_x5_s_ = 32;
		_x5_b_ = 7;
		_x6_s_ = 39;
		_x6_b_ = 9;
		_x7_s_ = 48;
		_x7_b_ = 4;
		_x8_s_ = 52;
		_x8_b_ = 12;

		if (!RUNTIME) {
			throw new IllegalArgumentException("This stub class should not be on your classpath");
		}
	}

}
