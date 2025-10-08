package j9vm.test.threads.regression;

/*
 * Copyright IBM Corp. and others 2025
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */

import junit.framework.Test;
import junit.framework.TestSuite;
import junit.framework.TestCase;

public class RegressionTests extends TestCase {

	/**
	 * method that can be used to run the test by itself
	 *
	 * @param args args[0] must be the path to where VM_Test.jar is available
	 */
	public static void main (String[] args) {
		TestUtils.args = args;
		junit.textui.TestRunner.run(suite());
	}

	public static Test suite(){
		TestSuite suite = new TestSuite();
		suite.addTestSuite(ProcessWaitFor.class);
		return suite;
	}
}
