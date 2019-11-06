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
 * Create non-persistent cache, use it then move the control file, use it again (new one created), delete than one
 * and return the original control file, use it then delete it.
 */
public class TestMovingControlFiles01 extends TestUtils {
  public static void main(String[] args) {
	  runDestroyAllCaches();
	  createNonPersistentCache("Foo");	  
	  runSimpleJavaProgramWithNonPersistentCache("Foo");

	  String fromdir = getCacheFileDirectory(false);
	  String todir = createTemporaryDirectory("TestListAllWhenNoCaches");
	  try {
		  
		  // move the control file to a temp directory
		  moveControlFilesForNonPersistentCache("Foo",fromdir,todir);
		  
		  // shouldnt be able to find it now...
		  listAllCaches();
		  checkOutputForCompatibleCache("Foo",false,false);
		  
		  // recreated by this run
		  runSimpleJavaProgramWithNonPersistentCache("Foo");
		  checkForSuccessfulNonPersistentCacheCreationMessage("Foo");
	
		  destroyNonPersistentCache("Foo");
		  
		  // Copy the old control file back

		  moveControlFilesForNonPersistentCache("Foo",todir,fromdir);
		  // use it
		  if (isWindows()) {
			  runSimpleJavaProgramWithNonPersistentCache("Foo");
			  checkForSuccessfulNonPersistentCacheOpenMessage("Foo");
		  } else {
			  // this may fail if our new file reuses inodes ...
			  runSimpleJavaProgramWithNonPersistentCacheMayFail("Foo");
		  }
		  
		  destroyNonPersistentCache("Foo");
		  
		  if (isWindows()) {
			  checkForSuccessfulNonPersistentCacheDestoryMessage("Foo");
		  }

	  } finally {
		  deleteTemporaryDirectory(todir);
	  }
  }
}
