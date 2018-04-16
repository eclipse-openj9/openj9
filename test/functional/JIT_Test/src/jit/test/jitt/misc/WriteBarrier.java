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
package jit.test.jitt.misc;

import org.testng.annotations.Test;

@Test(groups = { "level.sanity","component.jit" })
public class WriteBarrier extends jit.test.jitt.Test {

	String stringInstVar;

	// write barriers are produced when storing into all types of object.
	// We use writing to a field to test write barriers
	private void tstFoo(int i, String z) {
		if (i == 1) {
			this.stringInstVar = z;
		}
	}

	@Test
	public void testWriteBarrier() {
		WriteBarrier x = new WriteBarrier();
		for (int j = 0; j < sJitThreshold; j++) {
			x.tstFoo(0, "a string");
		}
		x.tstFoo(1, "another string");
	}

}
