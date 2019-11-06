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


import java.io.File;
import java.io.IOException;


import tests.sharedclasses.TestUtils;

/*
 * Check that controlDir and cacheDir work the same.  Set the cacheDir, create a non-persistent cache and use it
 * then reset cacheDir and point controlDir at the same place and check we can see the same cache.
 */
public class TestControlDirCacheDir02 extends TestUtils {
  public static void main(String[] args) {
	  runDestroyAllCaches();
	  
	  String dir = createTemporaryDirectory("TestControlDirCacheDir02");
	  String currentCacheDir = getCacheDir();
	  String currentControlDir = getControlDir();
	  try {
		  // tidyup before we go...
		  setCacheDir(null);setControlDir(null);
		  runDestroyAllCaches();
		  setCacheDir(currentCacheDir);setControlDir(currentControlDir);

		  setCacheDir(dir);

		  runDestroyAllCaches();
		  createNonPersistentCache("Foo");
		  runSimpleJavaProgramWithNonPersistentCache("Foo");
		  checkForSuccessfulNonPersistentCacheOpenMessage("Foo");
		  checkFileExistsForNonPersistentCache("Foo");
		  
		  setCacheDir(null);
		  checkFileDoesNotExistForNonPersistentCache("Foo");
		  
		  setControlDir(dir);
		  checkFileExistsForNonPersistentCache("Foo");
		  
		  runSimpleJavaProgramWithNonPersistentCache("Foo");
		  checkForSuccessfulNonPersistentCacheOpenMessage("Foo");
		  
		  // Delete the cache and check it has vanished when viewed via cacheDir
		  destroyNonPersistentCache("Foo");
		  setControlDir(null);
		  setCacheDir(dir);
		  checkFileDoesNotExistForNonPersistentCache("Foo");		  
	  } finally {
		  setCacheDir(currentCacheDir);
		  setControlDir(currentControlDir);
		  deleteTemporaryDirectory(dir);
	  }
  }  
}
