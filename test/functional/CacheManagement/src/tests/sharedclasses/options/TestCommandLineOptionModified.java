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

import tests.sharedclasses.TestUtils;

/*
 * Use the modification context command line option and check some entries in the cache are
 * stored against it.
 */
public class TestCommandLineOptionModified extends TestUtils {
  public static void main(String[] args) {
	
	runDestroyAllCaches();
	runSimpleJavaProgramWithNonPersistentCache("testCache");
	runPrintAllStats("testCache",false);
	checkOutputDoesNotContain("ModContext", "Did not expect to find modContext mentioned in the stats output");

	runSimpleJavaProgramWithNonPersistentCache("testCache,modified=mod1");
	runPrintAllStats("testCache",false);
	// example output:
    //3: 0x4343DC10 ROMCLASS: java/net/URI at 0x42589CC8.
    //	Index 1 in classpath 0x4349F6FC
    //	ModContext mod1
	checkOutputContains("ModContext.*mod1", "Expected the ModContext to be mentioned for some entries");
	runDestroyAllCaches();
  }
}
