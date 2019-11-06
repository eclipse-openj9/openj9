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
 * Check -Xscmcx
 */
public class TestCommandLineOptionXscmx01 extends TestUtils {
  public static void main(String[] args) {	
	runDestroyAllCaches();
	
	/* Note: AOT is turned off for the below tests. In some cases the JIT has 
	 * enough time to store information in the already small cache. During this 
	 * test this may cause the alredy to small cache to be marked as full 
	 * (when this is not expected to occur).
	 */
	
	// set 4k cache, too small
	RunCommand.execute(getCommand("runSimpleJavaProgramWithPersistentCachePlusOptions","Foo","-Xscmx4k -Xnoaot"));
	checkOutputContains("JVMSHRC096I.*is full", "Did not see expected message about the cache being full");
	
	runDestroyAllCaches();
	
	// set 5000k cache, ok
	RunCommand.execute(getCommand("runSimpleJavaProgramWithPersistentCachePlusOptions","Foo","-Xscmx5000k -Xnoaot"));
	checkOutputDoesNotContain("JVMSHRC096I.*is full", "Did not expect message about the cache being full");

	runDestroyAllCaches();
	
	// set 8M cache, ok
	RunCommand.execute(getCommand("runSimpleJavaProgramWithPersistentCachePlusOptions","Foo","-Xscmx8M -Xnoaot"));
	checkOutputDoesNotContain("JVMSHRC096I.*is full", "Did not expect message about the cache being full");
	
	runDestroyAllCaches();
  }
}
