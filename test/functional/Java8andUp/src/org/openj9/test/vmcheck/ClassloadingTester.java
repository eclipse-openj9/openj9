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
package org.openj9.test.vmcheck;

import org.testng.Assert;
import org.testng.AssertJUnit;
import org.testng.log4testng.Logger;


public class ClassloadingTester {

	/**
	 * loads the requested classes.  If they cannot be loaded, print "TEST_STATUS=TEST_FAILED"
	 * @param args list of classes to load
	 */
	public static void main(String args[]) {
		for (String testClassName: args) {
			printStatus(TestLoadingClassesFromJarfile.TEST_INFO +" loading " + testClassName);
			try {
				AssertJUnit.assertNotNull("could not load "+testClassName, Class.forName(testClassName));
			} catch (LinkageError e) {
				System.err.println("Permissible LinkageError");
				continue; /* permissible exception */
			} catch (SecurityException e) {
				System.err.println("Permissible SecurityException");
				continue; /* permissible exception */
			} catch (ClassNotFoundException e) {
				System.err.println("Permissible ClassNotFoundException");
				continue; /* permissible exception */
			} catch (Throwable e) {
				System.err.println("Unexpected exception");
				printStatus(TestLoadingClassesFromJarfile.TEST_FAILED + " : " + testClassName);
				e.printStackTrace();
				Assert.fail("Unexpected exception" + e);
			}
		}
		printStatus(TestLoadingClassesFromJarfile.TEST_PASSED);
		System.exit(0);
	}
	private static void printStatus(String statusMessage) {
		System.out.println(TestLoadingClassesFromJarfile.TEST_STATUS + statusMessage);
	}
}
