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

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;

import tests.sharedclasses.RunCommand;
import tests.sharedclasses.TestUtils;

/*
 * Create a persistent cache by running an app and then move the app. Check that 
 * classes are re-cached successfully.
 */
public class TestPersistentCacheMoving02 extends TestUtils {
  public static void main(String[] args) {
	  String mytestjar = "MySimpleApp.jar";
	  runDestroyAllCaches();
	  
	  if (isMVS()) {
		  // no support on ZOS for persistent caches ...
		  return;
	  }
	  
	  String tmpdir1 = createTemporaryDirectory("TestPersistentCacheMoving02Dir1");
	  String tmpdir2 = createTemporaryDirectory("TestPersistentCacheMoving02Dir2");
	  
	  checkFileExists("SimpleApp.java");
	  
	  /*1.) Copy two files to tmp dir*/
	  File javafile = new File("SimpleApp.java"); 
	  checkFileExists(javafile.toString());
	  File tmpfile1_dir1 = new File(tmpdir1+File.separator+"SimpleApp.java");
	  File tmpfile1_dir2 = new File(tmpdir2+File.separator+"SimpleApp.java");
	  
	  if (!copyFile(javafile,tmpfile1_dir1)) {
		  fail("Could not copy file '"+javafile+"' to '"+tmpfile1_dir1+"'");
	  }

	  File javafile2 = new File("SimpleApp2.java"); 
	  checkFileExists(javafile2.toString());
	  File tmpfile2_dir1 = new File(tmpdir1+File.separator+"SimpleApp2.java");
	  File tmpfile2_dir2 = new File(tmpdir2+File.separator+"SimpleApp2.java");
	  
	  if (!copyFile(javafile2,tmpfile2_dir1)) {
		  fail("Could not copy file '"+javafile+"' to '"+tmpfile2_dir1+"'");
	  }
	  
	  /*2.) Compile two files*/
	  checkFileExists(tmpfile1_dir1.toString());
	  checkFileExists(tmpfile2_dir1.toString());
	  
	  String compileline = getCommand("runCompileSimpleApp",tmpdir1);
	  //System.out.println(compileline);
	  RunCommand.execute(compileline);
	  
	  compileline = getCommand("runCompileSimpleApp2",tmpdir1);
	  //System.out.println(compileline);
	  RunCommand.execute(compileline);
	  
	  /*3.) Jar the two class files*/
	  checkFileExists(tmpdir1);
	  String jarline = getCommand("runJarClassesInDir",tmpdir1);
	  //System.out.println(jarline);
	  RunCommand.execute(jarline);
	  checkFileExists(mytestjar);
	  
	  /*4.) Copy new jar file into tmp directories*/
	  File f = new File(mytestjar);
	  File fNew = null;
	  
	  fNew = new File(tmpdir1+File.separator+mytestjar);
	  if (!copyFile(f,fNew)) {
		  fail("Could not copy file '"+f+"' to '"+fNew+"'");
	  }
	  checkFileExists(tmpdir1+File.separator+mytestjar);
	  
	  fNew = new File(tmpdir2+File.separator+mytestjar);
	  if (!copyFile(f,fNew)) {
		  fail("Could not copy file '"+f+"' to '"+fNew+"'");
	  }
	  checkFileExists(tmpdir2+File.separator+mytestjar);
	  
	  /*5.) Run sumple app*/
	  String runProg1 = getCommand("runProgramSimpleApp","Foo,verboseIO",tmpdir1+File.separator+mytestjar);
	  //System.out.println(runProg1);
	  RunCommand.execute(runProg1);
	  checkOutputContains("Failed to find class SimpleApp2 in shared cache for class-loader id 2", "Command 1a: Did not get expected message about looking for SimpleApp2 in the cache");
	  checkOutputContains("Stored class SimpleApp2 in shared cache for class-loader id 2","Command 1b: Did not find expected message about the test class being stored in the cache : "+runProg1);
	  
	  /*6.) Re-run sumple app*/
	  String runProg2 = getCommand("runProgramSimpleApp","Foo,verboseIO",tmpdir2+File.separator+mytestjar);
	  //System.out.println(runProg2);
	  RunCommand.execute(runProg2);
	  checkOutputContains("Failed to find class SimpleApp2 in shared cache for class-loader id 2", "Command 2a: Did not get expected message about looking for SimpleApp2 in the cache");
	  checkOutputContains("Stored class SimpleApp2 in shared cache for class-loader id 2","Command 2b: Did not find expected message about the test class being stored in the cache : " +runProg2);
	  	  
	  deleteTemporaryDirectory(tmpdir1);
	  deleteTemporaryDirectory(tmpdir2);	  
	  f.delete();
  }
}
