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

import tests.sharedclasses.TestUtils;

/*
 * Check that when cacheDir is used, we never get a javasharedresources sub folder
 */
public class TestCacheDir05 extends TestUtils {
  public static void main(String[] args) {
	  runDestroyAllCaches();
	  String dir = createTemporaryDirectory("TestCacheDir05");
	  String currentCacheDir = getCacheDir();
	  File javaSharedResourcesFolder = new File(dir+File.separator+"javasharedresources");
	  setCacheDir(dir);
	  try {
		  createPersistentCache("Foo");	  
		  if (javaSharedResourcesFolder.exists()) 
			  fail("Did not expect a javasharedresources folder to be created after creating a persistent cache when cacheDir is used");
		  createNonPersistentCache("Bar");	  
		  if (javaSharedResourcesFolder.exists()) 
			  fail("Did not expect a javasharedresources folder to be created after creating a non-persistent cache when cacheDir is used");
		  runDestroyAllCaches();
	  } finally {
		  runDestroyAllCaches();
		  setCacheDir(currentCacheDir);
		  deleteTemporaryDirectory(dir);
	  }
  }
}
