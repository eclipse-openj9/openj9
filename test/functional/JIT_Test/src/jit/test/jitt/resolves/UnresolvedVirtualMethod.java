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
package jit.test.jitt.resolves;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class UnresolvedVirtualMethod extends jit.test.jitt.Test {

	static class Y {
		private boolean test = false;
		boolean state(Y ignored1, int ignored2) {
			return test;
		};
		void setTrue() {
			test = true;
		}
		void setFalse() {
			test = false;
		}
	}

	static class X {
		public void tstFoo(Y y, int i) {
			if (i == 1) {
				if (y.state(y, 0xBADF00D1) != false)
					Assert.fail("bad state #1");
				if (i == 1) {
					y.setTrue();
					if (y.state(y, 0xBADF00D2) != true)
						Assert.fail("bad state #2");
					y.setFalse();
					if (y.state(y, 0xBADF00D3) != false)
						Assert.fail("bad state #3");
				}
			}
		}
	}

	@Test
	public void testUnresolvedVirtualMethod() {
		X x = new X();
		for (int j = 0; j < sJitThreshold; j++) {
			x.tstFoo(null, 0);
		}
		x.tstFoo(new Y(), 1);
	}
}
