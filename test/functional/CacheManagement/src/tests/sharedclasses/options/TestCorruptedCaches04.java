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

import tests.sharedclasses.RunCommand;
import tests.sharedclasses.TestUtils;

/*
 * Complex corruption - CMVC defect 123221
 * 1. start a VM using a cache
 * 2. start/stop a second VM using the same cache
 * 3. ask the first VM to change the cache (load something new)
 * 4. start/stop a third VM using the same cache (will crash!)
 * 
 * To achieve this, this test program launches the VM described in step 1.  See the type
 * TestCorruptedCaches04Helper to see the other steps.
 */
public class TestCorruptedCaches04 extends TestUtils {
	public static void main(String[] args) {
		runDestroyAllCaches();
		if (isMVS()) {
			// disabling for ZOS ... it doesn't make sense to corrupt the file
			// (it only contains os id for shared mem chunk) ...
			return;
		}
		createPersistentCache("Foo");

		String cmd = getCommand(RunComplexJavaProgramWithPersistentCache,
				"Foo", System.getProperty("config.properties",
						"config.properties"));
		RunCommand.execute(cmd);
		showOutput();
	}
}
