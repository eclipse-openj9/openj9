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
package jit.test.jitt.crashes;

import org.testng.annotations.Test;
import org.testng.Assert;

class B {}

class D extends B{}


@Test(groups = { "level.sanity","component.jit" })
public class InvalidArrayStoreNotCaught extends jit.test.jitt.Test {

	static boolean sResult = true;	// prevent optimizations
	
	void tstInvalidArrayStoreNotCaught(boolean triggerNow) {
		if (!triggerNow)
			return;
		
     B[] ob = new D[10];
     try{
       ob[1] = new B();    // Run-time error: Assignment not valid.
       sResult = false;
     }
     catch(ArrayStoreException a) { return;}
			
	Assert.fail("invalid array store not caught");
}

	@Test
	public void testInvalidArrayStoreNotCaught() {
		tstInvalidArrayStoreNotCaught(true);
		for (int i = 0; i < sJitThreshold; i++) {
			tstInvalidArrayStoreNotCaught(false);
		}
		tstInvalidArrayStoreNotCaught(true);
	}
}
