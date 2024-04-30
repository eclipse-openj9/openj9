/*
 * Copyright IBM Corp. and others 2010
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */

package tests.sharedclasses.options;

import java.io.File;

import tests.sharedclasses.TestUtils;

/*
 * Check that when cacheDir is used and a persistent cache created, we create the directory if it does not exist
 */
public class TestCacheDir06 extends TestUtils {
  public static void main(String[] args) {
	  runDestroyAllCaches();
	  String dir = createTemporaryDirectory("TestCacheDir06");
	  String currentCacheDir = getCacheDir();
	  String subdir = dir+File.separator+"subdir";
	  setCacheDir(subdir);
	  try {
		  runSimpleJavaProgramWithPersistentCache("Foo",false);
		  checkOutputDoesNotContain("JVMSHRC215E", 
				  "Did not expect message about the path not being found, it should have been constructed: "+
				  subdir);		  
		  runDestroyAllCaches();
	  } finally {
		  runDestroyAllCaches();
		  setCacheDir(currentCacheDir);
		  deleteTemporaryDirectory(dir);
	  }
  }
}
