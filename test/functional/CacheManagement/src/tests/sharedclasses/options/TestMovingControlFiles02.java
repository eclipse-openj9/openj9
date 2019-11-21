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
 * Create non-persistent cache, rename the control file in the javasharedresources folder, try and delete it.
 */
public class TestMovingControlFiles02 extends TestUtils {
  public static void main(String[] args) {
	  runDestroyAllCaches();
	  createNonPersistentCache("Foo");	  
	  runSimpleJavaProgramWithNonPersistentCache("Foo");
	  
	  String tmpdir = createTemporaryDirectory("TestMovingControlFiles02");
	  
	  File fHidden = null;
	  try {
		  // rename the control file, add '.hidden' suffix
		  
		  if (isWindows()) {
			  String loc = getCacheFileLocationForNonPersistentCache("Foo");
			  File f = new File(loc);
			  fHidden = new File(loc+".hidden");
			  fHidden.delete();
			  if (!f.renameTo(fHidden)) fail("Could not rename the control file to be hidden");
		  } else {
			  // linux, two files to rename
			  String loc = getCacheFileLocationForNonPersistentCache("Foo");
			  File f= new File(loc);
			  fHidden = new File(loc+".hidden");
			  fHidden.delete();
			  if (!f.renameTo(fHidden)) fail("Could not rename the 'memory' control file from "+f.getAbsolutePath()+" to "+fHidden.getAbsolutePath());
			  loc = getCacheFileLocationForNonPersistentCache("Foo").replace("memory","semaphore");
			  f= new File(loc);
			  fHidden = new File(loc+".hidden");
			  fHidden.delete();
			  if (!f.renameTo(fHidden)) fail("Could not rename the 'semaphore' control file from "+f.getAbsolutePath()+" to "+fHidden.getAbsolutePath());
			  
		  }
		  // shouldnt be able to find it now...
		  listAllCaches();
		  checkOutputForCompatibleCache("Foo",false,false);

		  if (isWindows()) {
			  runSimpleJavaProgramWithNonPersistentCache("Foo");
			  checkForSuccessfulNonPersistentCacheCreationMessage("Foo");
		  } else {
			  // this may fail if our new file reuses inodes ...
			  runSimpleJavaProgramWithNonPersistentCacheMayFail("Foo");
		  }
		  

	  } finally {
		  runDestroyAllCaches();
		  // tidy up!
		  if (isWindows()) {
			  String loc = getCacheFileLocationForNonPersistentCache("Foo");
			  File f = new File(loc);
			  fHidden = new File(loc+".hidden");
			  f.delete();
			  if (!fHidden.renameTo(f)) fail("Could not rename the control file to its original name");
		  } else {
			  // linux, two files to rename
			  String loc = getCacheFileLocationForNonPersistentCache("Foo");
			  File f= new File(loc);
			  fHidden = new File(loc+".hidden");
			  f.delete();
			  if (!fHidden.renameTo(f)) fail("Could not rename the 'memory' control file to its original name");
			  loc = getCacheFileLocationForNonPersistentCache("Foo").replace("memory","semaphore");;
			  f= new File(loc);
			  fHidden = new File(loc+".hidden");
			  f.delete();
			  if (!fHidden.renameTo(f)) fail("Could not rename the 'semaphore' control file to its original name");
		  }
		  runDestroyAllCaches();
		  deleteTemporaryDirectory(tmpdir);
	  }
  }
}
