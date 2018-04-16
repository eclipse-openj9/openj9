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
package jit.test.jitt.gc;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class Scavenge extends jit.test.jitt.Test {

	private void interpGC() {
		System.gc();

		/* force enough allocations for new objects to be pushed into tenure space */
		for (int i = 0; i < 1000; i++) {
			int int_array[] = new int[1024];
			for (int j = 0; j < int_array.length; j++) {
				int_array[j] = j;
			}
		}

		System.gc();
	}

	private void tst_jit1(int recursionDepth, Object z, boolean triggerGC) {

		Integer i = new Integer(recursionDepth - 1); // make a temp

		if (recursionDepth == 0) {
			if (triggerGC)
				this.interpGC();
			String printed = i.toString();
			if (printed == null)
				Assert.fail(printed + "shouldn't happen!!");
		} else {
			tst_jit1(recursionDepth - 1, i, triggerGC);
			String printed = i.toString();
			if (printed == null)
				Assert.fail(printed + "shouldn't happen!!");
		}
	}

	@Test
	public void testScavenge() {
		Scavenge x = new Scavenge();
		for (int j = 0; j < 101; j++) {
			x.tst_jit1(10, new Integer(j), false);
		}
		x.tst_jit1(10, "trigger_gc_now", true);
	}

}
