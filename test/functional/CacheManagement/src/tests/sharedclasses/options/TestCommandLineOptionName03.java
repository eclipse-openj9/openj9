/*******************************************************************************
 * Copyright (c) 2010, 2019 IBM Corp. and others
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

package tests.sharedclasses.options;

import java.util.Iterator;
import java.util.List;

import tests.sharedclasses.TestUtils;

/*
 * Check the 'name=' option 
 * - the value '%u' should be the userid
 * - the value '%g' should be the group id
 */
public class TestCommandLineOptionName03 extends TestUtils {
  public static void main(String[] args) {	
	runDestroyAllCaches();

	// %u - should be fine
	runSimpleJavaProgramWithNonPersistentCache("%u");

	if (isWindows()) {
		// %g - not supported
		runSimpleJavaProgramWithNonPersistentCache("%g",false);
		checkOutputContains("JVMSHRC154E.*character g", "Did not get expected message about escape character G not being supported on windows");		
	} else {
		// %g - should be fine
		runSimpleJavaProgramWithNonPersistentCache("%g");
	}
	listAllCaches();

	List l = processOutputForCompatibleCacheList();
	for (Iterator iterator = l.iterator(); iterator.hasNext();) {
		String name = (String) iterator.next();
		if (name.equals("")) {
			listAllCaches();
			fail("Cache found with a blank name!!!!");
		}
	}

	runDestroyAllCaches();
  }
}
