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
package jit.test.jitt.casting;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class InstOf001 extends jit.test.jitt.Test {

	private static Logger logger = Logger.getLogger(InstOf001.class);
	static final int TEST_NOTHING = 0;
	static final int TEST_OBJECT_OBJECT_PASS = 1;
	static final int TEST_OBJECT_OBJECT_FAIL = 2;
	static final int TEST_OBJECT_INTERFACE_PASS = 3;
	static final int TEST_OBJECT_INTERFACE_FAIL = 4;
	static final int TEST_ARRAY_OBJECT_PASS = 5;
	static final int TEST_ARRAY_OBJECT_FAIL = 6;
	static final int TEST_ARRAY_ARRAY_PASS = 7;
	static final int TEST_ARRAY_ARRAY_FAIL = 8;
	static final int TEST_ARRAY_INTERACE_PASS = 9;
	static final int TEST_ARRAY_INTERFACE_FAIL = 10;

	static class Z extends X {
	}

	static class X implements java.lang.Cloneable {

		public void tst_jit(Object arg, int test) {
			boolean passed = false;
			boolean debug = false;

			switch (test) {

				case TEST_NOTHING :
					passed = true;
					break;

				case TEST_OBJECT_OBJECT_PASS :
					logger.debug("pos: obj --instanceOf--> obj, passed=");
					passed = this instanceof Z;
					break;

				case TEST_OBJECT_OBJECT_FAIL :
					logger.debug("neg: obj --!instanceOf--> obj, passed=");
					passed = !(arg instanceof String);
					break;

				case TEST_OBJECT_INTERFACE_PASS :
					logger.debug("pos: obj --instanceOf--> interface, passed=");
					passed = arg instanceof Cloneable;
					break;

				case TEST_OBJECT_INTERFACE_FAIL :
					logger.debug("neg: obj --!instanceOf--> interface, passed=");
					passed = !(arg instanceof java.io.Serializable);
					break;

				case TEST_ARRAY_OBJECT_PASS :
					logger.debug("pos: array --!instanceOf--> obj, passed=");
					passed = (new Object[] {arg}
					instanceof Object);
					break;

				case TEST_ARRAY_OBJECT_FAIL :
					logger.debug("neg: array --!instanceOf--> obj, passed=");
					passed = !(new Object[] {arg}
					instanceof String[]);
					break;

				case TEST_ARRAY_ARRAY_PASS :
					logger.debug("pos: array --!instanceOf--> array, passed=");
					passed = (new Object[] {this}
					instanceof Object[]);
					break;

				case TEST_ARRAY_ARRAY_FAIL :
					logger.debug("neg: array --!instanceOf--> array, passed=");
					passed = !(new Object[] {arg}
					instanceof String[]);
					break;

				case TEST_ARRAY_INTERACE_PASS :
					logger.debug("pos: array --!instanceOf--> interface, passed=");
					passed = (new Object[] {arg}
					instanceof Cloneable);
					break;

				case TEST_ARRAY_INTERFACE_FAIL :
					logger.debug("neg: array --!instanceOf--> interface, passed=");
					passed = !(new X[] {(X) arg}
					instanceof java.io.Serializable[]);
					break;
			}

			if (!passed) {
				logger.error("Bad test case" + test);
				Assert.fail("unexpected casting failure");
			} else {
				if (test != TEST_NOTHING && debug) {
					logger.debug(passed);
				}
			}
			return;
		}
	}

	@Test
	public void testInstOf001() {
		X x = new Z();

		x.tst_jit(x, TEST_OBJECT_OBJECT_PASS);
		x.tst_jit(x, TEST_OBJECT_OBJECT_FAIL);
		x.tst_jit(x, TEST_OBJECT_INTERFACE_PASS);
		x.tst_jit(x, TEST_OBJECT_INTERFACE_FAIL);
		x.tst_jit(x, TEST_ARRAY_OBJECT_PASS);
		x.tst_jit(x, TEST_ARRAY_OBJECT_FAIL);
		x.tst_jit(x, TEST_ARRAY_ARRAY_PASS);
		x.tst_jit(x, TEST_ARRAY_ARRAY_FAIL);
		x.tst_jit(x, TEST_ARRAY_INTERACE_PASS);
		x.tst_jit(x, TEST_ARRAY_INTERFACE_FAIL);

		for (int j = 0; j < sJitThreshold; j++) {
			x.tst_jit(x, TEST_NOTHING);
		}

		x.tst_jit(x, TEST_OBJECT_OBJECT_PASS);
		x.tst_jit(x, TEST_OBJECT_OBJECT_FAIL);
		x.tst_jit(x, TEST_OBJECT_INTERFACE_PASS);
		x.tst_jit(x, TEST_OBJECT_INTERFACE_FAIL);
		x.tst_jit(x, TEST_ARRAY_OBJECT_PASS);
		x.tst_jit(x, TEST_ARRAY_OBJECT_FAIL);
		x.tst_jit(x, TEST_ARRAY_ARRAY_PASS);
		x.tst_jit(x, TEST_ARRAY_ARRAY_FAIL);
		x.tst_jit(x, TEST_ARRAY_INTERACE_PASS);
		x.tst_jit(x, TEST_ARRAY_INTERFACE_FAIL);
	}

}
