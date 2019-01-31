/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
package jit.test.ra;

import java.math.BigInteger;
import java.util.Arrays;
import org.testng.annotations.Test;

@Test(groups = { "level.sanity", "component.jit" })
public class TestOOLSpill31Bit {

	// public volatile prevents JIT optimizer from eliminating stores
	public static volatile BigInteger bi;

	public static final BigInteger powerOfTenBI(long i) {
		char[] ten = new char[(int)i + 1];
		Arrays.fill(ten,'0');
		ten[0] = '1';
		return new BigInteger(new String(ten));
	}

	/**
	 * The following test is articulated such that the {@code powerOfTenBI} method gets deterministically JIT compiled and
	 * eventually has to call the VM JIT helper to allocate memory as we run out of TLH space.
	 *
	 * On 31-bit platforms when using 64-bit registers we can encounter problems in this path if the register allocator
	 * fails to properly spill 64-bit values across an OOL code section.
	 *
	 * @see eclipse/omr#3337
	 */
	@Test
	public static void testOOLSpill31Bit() {
		for (int i = 0; i < 100000; ++i) {
			bi = powerOfTenBI(0x15);
		}
	}
}
