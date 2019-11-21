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
import java.io.*;

/*
 * Create a persistent cache, move the cache to another location, use cachedir to pick it up from the other 
 * location and check no new classes are loaded.
 */
public class TestPersistentCacheMoving01 extends TestUtils {
  public static void main(String[] args) {
	  runDestroyAllCaches();

	  if (isMVS())
	  {
		  // no support on ZOS for persistent caches ...
		  return;
	  }
	  String currentCacheDir = getCacheDir();

	  if ( null == currentCacheDir && isOpenJ9() ) {
		  // Persistent cachefile without groupaccess are generated under HOME directory
		  // Current directory is under /tmp. Move a file from HOME directory to /tmp may not succeed on some platforms. 
		return;
	  }
	  
	  runSimpleJavaProgramWithPersistentCache("Foo,verboseIO");
	  checkOutputContains("Stored class SimpleApp2", "Did not find expected message about the test class being stored in the cache");

	  String tmpdir = createTemporaryDirectory("TestPersistentCacheMoving01");
	  try {
		  
		  // move the cache file to the temp directory
		  String loc = getCacheFileLocationForPersistentCache("Foo");
		  File f = new File(loc);
		  File fHidden = new File(tmpdir+File.separator+getCacheFileName("Foo", true));

		  if (!f.renameTo(fHidden)) fail("Could not rename the control file to be hidden");
		  setCacheDir(tmpdir);
		  listAllCaches();
		  checkOutputForCompatibleCache("Foo",true,true);
		  
		  runSimpleJavaProgramWithPersistentCache("Foo,verboseIO");
		  checkOutputDoesNotContain("Stored class SimpleApp2", "Unexpectedly found message about storing the SimpleApp2 class in the cache, it should already be there!");
		  checkOutputContains("Found class SimpleApp2", "Did not get expected message about looking for SimpleApp2 in the cache");

		  runDestroyAllCaches();
	  } finally {
		  deleteTemporaryDirectory(tmpdir);
		  setCacheDir(currentCacheDir);
	  }
  }
}
