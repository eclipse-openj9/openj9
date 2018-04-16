/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package jit.test.jitt.sieve;

import org.testng.annotations.Test;

@Test(groups = { "level.sanity","component.jit" })
public class Sieve03 extends jit.test.jitt.Test {

	int primes[];
	int max;

	public Sieve03() {
		primes = new int[512];
		max = 512;
	}

	private int tstSieve() {
		int a7 = 1;
		int b8 = 0;
		int c5;
		int d6;

		this.primes[0] = 1;
		this.primes[1] = 2;
		a7 = 2;
		c5 = 3;
		while (c5 < this.max) {
			b8 = 1;
			if (b8 >= a7) {
				if (this.primes[b8] <= c5) {
					b8++;
				} else {
					a7++;
				}
			}
			c5++;
		}
		return a7;
	}

	@Test
	public void testSieve03() {
		for (int j = 0; j < sJitThreshold; j++) {
			tstSieve();
		}
	}

}
