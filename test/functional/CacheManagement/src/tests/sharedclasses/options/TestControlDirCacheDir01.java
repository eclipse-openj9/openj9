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
 * Check that controlDir and cacheDir work the same.  Set the cacheDir, create a persistent cache and use it
 * then reset cacheDir and point controlDir at the same place and check we can see the same cache.
 */
public class TestControlDirCacheDir01 extends TestUtils {
  public static void main(String[] args) {
	  runDestroyAllCaches();
	  if (isMVS())
	  {
		  //this test checks persistent and non persistent cahces ... zos only has nonpersistent ... so we assume other tests cover this
		  return;
	  }
	  String dir = createTemporaryDirectory("TestControlDirCacheDir01");
	  String currentCacheDir = getCacheDir();
	  String currentControlDir = getControlDir();
	  try {
		  //tidy up default area
		  setCacheDir(null);setControlDir(null);
		  runDestroyAllCaches();
		  setCacheDir(currentCacheDir);setControlDir(currentControlDir);

		  setCacheDir(dir);

		  runDestroyAllCaches();
		  createPersistentCache("Foo");
		  runSimpleJavaProgramWithPersistentCache("Foo");
		  checkForSuccessfulPersistentCacheOpenMessage("Foo");
		  checkFileExistsForPersistentCache("Foo");
		  
		  setCacheDir(null);
		  checkFileDoesNotExistForPersistentCache("Foo");
		  
		  setControlDir(dir);
		  checkFileExistsForPersistentCache("Foo");
		  
		  runSimpleJavaProgramWithPersistentCache("Foo");
		  checkForSuccessfulPersistentCacheOpenMessage("Foo");
		  
		  // Delete the cache and check it has vanished when viewed via cacheDir
		  destroyPersistentCache("Foo");
		  setControlDir(null);
		  setCacheDir(dir);
		  checkFileDoesNotExistForPersistentCache("Foo");		  
	  } finally {
		  runDestroyAllCaches();
		  setCacheDir(currentCacheDir);
		  setControlDir(currentControlDir);
		  deleteTemporaryDirectory(dir);
	  }
  }
//  
//  public static String createTemporaryDirectory() {
//	try {
//	  File f = File.createTempFile("testSC","dir");
//	  if (!f.delete()) fail("Couldn't create the temp dir");
//	  if (!f.mkdir()) fail("Couldnt create the temp dir");
//	  return f.getAbsolutePath();  
//	} catch (IOException e) {
//		fail("Couldnt create temp dir: "+e);
//		return null;
//	}
//  }
}
