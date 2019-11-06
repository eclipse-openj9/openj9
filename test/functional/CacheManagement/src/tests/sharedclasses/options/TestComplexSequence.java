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
 * Create persistent cache Foo - check file is in right place
 * Create non-persistent cache Bar - check cache control file is in right place
 * Try to attach to persistent cache Foo - check attach to existing
 * Try to attach to non-persistent cache Bar - check attach to existing
 * Create non-persistent cache Foo - check new cache is created (and we dont connect to existing)
 * Create persistent cache Bar - check new cache is created (and we dont connect to existing)
 * Try to attach to non-persistent cache Foo - check attach to correct cache
 * Try to attach to persistent cache Bar - check attach to correct cache
 */
public class TestComplexSequence extends TestUtils {
  public static void main(String[] args) {
	  runDestroyAllCaches();
	  
	  if (isMVS())
	  {
		  //this test checks persistent and non persistent cahces ... zos only has nonpersistent ... so we assume other tests cover this
		  return;
	  }
	  
	  createPersistentCache("Foo");
	  checkFileExistsForPersistentCache("Foo");
	  checkNoFileForNonPersistentCache("Foo");
	  
	  createNonPersistentCache("Bar");
	  checkFileExistsForNonPersistentCache("Bar");
	  checkNoFileForPersistentCache("Bar");
	  
	  runSimpleJavaProgramWithPersistentCache("Foo");
	  checkOutputContains("JVMSHRC237I.*Foo", "Did not find expected message 'JVMSHRC237I Opened shared classes persistent cache Foo'");
	  runSimpleJavaProgramWithNonPersistentCache("Bar");
	  checkOutputContains("JVMSHRC159I.*Bar", "Did not find expected message 'JVMSHRC159I Successfully opened shared class cache \"Bar\"'");
	  
	  createNonPersistentCache("Foo");
	  checkOutputContains("JVMSHRC158I.*Foo", "Did not find expected message 'JVMSHRC158I Successfully created shared class cache \"Foo\"'");
	  checkFileExistsForNonPersistentCache("Foo");

	  createPersistentCache("Bar");
	  checkForSuccessfulPersistentCacheCreationMessage("Bar");
	  checkFileExistsForPersistentCache("Bar");
	  
	  runSimpleJavaProgramWithNonPersistentCache("Foo");
	  checkForSuccessfulNonPersistentCacheOpenMessage("Foo");
	  
	  runSimpleJavaProgramWithPersistentCache("Bar");
	  checkForSuccessfulPersistentCacheOpenMessage("Bar");
	  
	  runDestroyAllCaches();
  }
}
