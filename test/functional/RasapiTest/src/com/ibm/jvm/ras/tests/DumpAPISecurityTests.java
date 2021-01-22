/*******************************************************************************
 * Copyright (c) 2016, 2021 IBM Corp. and others
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
package com.ibm.jvm.ras.tests;

import static com.ibm.jvm.ras.tests.DumpAPISuite.deleteFile;
import static com.ibm.jvm.ras.tests.DumpAPISuite.getFilesByPattern;
import static com.ibm.jvm.ras.tests.DumpAPISuite.isZOS;

import static java.lang.Boolean.TRUE;
import static java.lang.Boolean.FALSE;

import java.io.File;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

import junit.framework.TestCase;

import com.ibm.jvm.InvalidDumpOptionException;


/**
 * Tests of the dump API performed with a security manager set
 * to block creation of files.
 * 
 * No security manager is the default mode and covered by
 * DumpAPIBasicTests 
 * 
 * *** Security is enabled by the ant script that normally runs these tests ***
 * 
 * @author hhellyer
 *
 */
public class DumpAPISecurityTests extends TestCase {

	private long uid = System.currentTimeMillis();

	// Record the files created by each testcase here so we can delete them
	// later. Otherwise we will fill the disk up pretty quickly. While we check
	// that the right kind of dump was created for each call we do any further
	// validity checking on the dumps themselves. This test is just checking the
	// API does what it should not that the dumps themselves are correct.
	private Set<String> fileNames;

	@Override
	protected void setUp() throws Exception {
    	
    	uid = System.currentTimeMillis();
    	
		super.setUp();
		
		fileNames = new HashSet<String>();
		
		// This isn't to free space, it's to trigger GC tracepoints so we don't get empty snap dumps!
		System.gc();
		
		// Make sure this is off for the start of every test.
		System.clearProperty("com.ibm.jvm.enableLegacyDumpSecurity");
		// And for the Trace.snap() tests.
		System.clearProperty("com.ibm.jvm.enableLegacyTraceSecurity");
	}

	@Override
	protected void tearDown() throws Exception {
		super.tearDown();
		for( String fileName: fileNames) { 
			deleteFile(fileName, this.getName());
		}
	}
	
	private void addNewFilesToDelete(String[] beforeFileNames, String[] afterFileNames) {
		for( String after : afterFileNames ) {
			boolean newFile = true;
			for( String before : beforeFileNames ) {
				if( before.equals(after) ) {
					newFile = false;
					break;
				}
			}
			if( newFile ) {
				fileNames.add(after);
			}
		}
	}
	
	/**
	 * Test a java dump is generated from the standard com.ibm.jvm.Dump.JavaDump()
	 * call.
	 */
	public void testJavaDumpNoArgs() {
		String userDir = System.getProperty("user.dir");
		String javaCorePattern = "javacore\\..*\\.txt";
		
		System.setProperty("com.ibm.jvm.enableLegacyDumpSecurity", FALSE.toString());
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, javaCorePattern);
		int beforeCount = beforeFileNames.length;
		
		com.ibm.jvm.Dump.JavaDump();
		
		String[] afterFileNames = getFilesByPattern(userDir, javaCorePattern);
		
		addNewFilesToDelete(beforeFileNames, afterFileNames);
		
		int afterCount = afterFileNames.length;
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount + 1, afterCount);
	}

	/**
	 * Test a java dump is generated from the standard com.ibm.jvm.Dump.JavaDump()
	 * but not if the security permission should be listened to.
	 */
	public void testJavaDumpNoArgsBlocked() {
		String userDir = System.getProperty("user.dir");
		String javaCorePattern = "javacore\\..*\\.txt";
		
		System.clearProperty("com.ibm.jvm.enableLegacyDumpSecurity");
		
		
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, javaCorePattern);
		int beforeCount = beforeFileNames.length;
		
		try {
			com.ibm.jvm.Dump.JavaDump();
			fail("Expected security exception to be thrown.");
		} catch (SecurityException e) {
			//Pass
		}
		
		String[] afterFileNames = getFilesByPattern(userDir, javaCorePattern);
		
		addNewFilesToDelete(beforeFileNames, afterFileNames);
		
		int afterCount = afterFileNames.length;
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount, afterCount);

		System.setProperty("com.ibm.jvm.enableLegacyDumpSecurity", FALSE.toString());
		
		com.ibm.jvm.Dump.JavaDump();
	
		afterFileNames = getFilesByPattern(userDir, javaCorePattern);
		
		addNewFilesToDelete(beforeFileNames, afterFileNames);
		
		afterCount = afterFileNames.length;
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount + 1, afterCount);
		
	}
	
	public void testJavaDumpWithFile() {
		String userDir = System.getProperty("user.dir");
		
		String expectedName = userDir + File.separator + "javacore."+ getName() + "." + uid + ".txt";
		
		// Check the file doesn't exist, otherwise the dump code moves it.
		File expectedFile = new File(expectedName);
		assertFalse("The javacore file " + expectedName + " already exists", expectedFile.exists());
		
		// Check we got the correct name.
		try {
			String javacoreName = com.ibm.jvm.Dump.javaDumpToFile(expectedName);
			fail("Expected security exception to be thrown.");
		} catch (SecurityException e ) {
			// This is the success path!
		} catch (InvalidDumpOptionException e) {
			fail("Expected security exception to be thrown not InvalidDumpOptionException.");
		} 
	}
	
	/**
	 * Test a java dump is generated from the standard com.ibm.jvm.Dump.JavaDump()
	 * call.
	 */
	public void testJavaDumpNullFile() {
		String userDir = System.getProperty("user.dir");
		String javaCorePattern = "javacore\\..*\\.txt";
		
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, javaCorePattern);
		int beforeCount = beforeFileNames.length;
		
		String javacoreName = null;
		try {
			javacoreName = com.ibm.jvm.Dump.javaDumpToFile(null);
			fail("Expected security exception to be thrown.");
		} catch (SecurityException e ) {
			// This is the success path!
		} catch (InvalidDumpOptionException e) {
			fail("Expected security exception to be thrown not InvalidDumpOptionException.");
		}
	}
	
	/**
	 * Test a heap dump is generated from the standard com.ibm.jvm.Dump.HeapDump()
	 * call.
	 */
	public void testHeapDumpNoArgs() {
		String userDir = System.getProperty("user.dir");
		String heapdumpPattern = "heapdump\\..*\\.phd";
		
		System.setProperty("com.ibm.jvm.enableLegacyDumpSecurity", FALSE.toString());
		
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, heapdumpPattern);
		int beforeCount = beforeFileNames.length;
		
		com.ibm.jvm.Dump.HeapDump();
		
		String[] afterFileNames = getFilesByPattern(userDir, heapdumpPattern);
		
		addNewFilesToDelete(beforeFileNames, afterFileNames);
		
		int afterCount = afterFileNames.length;
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount + 1, afterCount);
	}

	public void testHeapDumpNoArgsBlocked() {
		String userDir = System.getProperty("user.dir");
		String heapdumpPattern = "heapdump\\..*\\.phd";
		
		System.clearProperty("com.ibm.jvm.enableLegacyDumpSecurity");
		
		
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, heapdumpPattern);
		int beforeCount = beforeFileNames.length;
		
		try {
			com.ibm.jvm.Dump.HeapDump();
			fail("Expected security exception to be thrown.");
		} catch (SecurityException e) {
			//Pass
		}
		
		String[] afterFileNames = getFilesByPattern(userDir, heapdumpPattern);
		
		addNewFilesToDelete(beforeFileNames, afterFileNames);
		
		int afterCount = afterFileNames.length;
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount, afterCount);

		System.setProperty("com.ibm.jvm.enableLegacyDumpSecurity", FALSE.toString());
		
		com.ibm.jvm.Dump.HeapDump();
	
		afterFileNames = getFilesByPattern(userDir, heapdumpPattern);
		
		addNewFilesToDelete(beforeFileNames, afterFileNames);
		
		afterCount = afterFileNames.length;
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount + 1, afterCount);
		
	}
	
	public void testHeapDumpWithFile() {
		String userDir = System.getProperty("user.dir");
		
		String expectedName = userDir + File.separator + "heapdump."+ getName() + "." + uid + ".txt";
		
		// Check the file doesn't exist, otherwise the dump code moves it.
		File expectedFile = new File(expectedName);
		assertFalse("The heapdump file " + expectedName + " already exists", expectedFile.exists());
		
		// Check we got the correct name.
		try {
			String heapdumpName = com.ibm.jvm.Dump.heapDumpToFile(expectedName);
			fail("Expected security exception to be thrown.");
		} catch (SecurityException e ) {
			// This is the success path!
		} catch (InvalidDumpOptionException e) {
			fail("Expected security exception to be thrown not InvalidDumpOptionException.");
		}
	}
	
	/**
	 * Test a java dump is generated from the standard com.ibm.jvm.Dump.JavaDump()
	 * call.
	 */
	public void testHeapDumpNullFile() {
		String userDir = System.getProperty("user.dir");
		String heapdumpPattern = "heapdump\\..*\\.phd";
		
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, heapdumpPattern);
		int beforeCount = beforeFileNames.length;
		
		String heapdumpName = null;
		try {
			heapdumpName = com.ibm.jvm.Dump.heapDumpToFile(null);
			fail("Expected security exception to be thrown.");
		} catch (SecurityException e ) {
			// This is the success path!
		} catch (InvalidDumpOptionException e) {
			fail("Expected security exception to be thrown not InvalidDumpOptionException.");
		}
	}
	
	/**
	 * Test a heap dump is generated from the standard com.ibm.jvm.Dump.HeapDump()
	 * call.
	 */
	public void testSnapDumpNoArgs() {
		String userDir = System.getProperty("user.dir");
		String snapPattern = "Snap\\..*\\.trc";
		
		System.setProperty("com.ibm.jvm.enableLegacyDumpSecurity", FALSE.toString());
		
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, snapPattern);
		int beforeCount = beforeFileNames.length;
		
		com.ibm.jvm.Dump.SnapDump();
		
		String[] afterFileNames = getFilesByPattern(userDir, snapPattern);
		
		addNewFilesToDelete(beforeFileNames, afterFileNames);
		
		int afterCount = afterFileNames.length;
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount + 1, afterCount);
	}
	
	public void testSnapDumpNoArgsBlocked() {
		String userDir = System.getProperty("user.dir");
		String snapPattern = "Snap\\..*\\.trc";
		
		System.clearProperty("com.ibm.jvm.enableLegacyDumpSecurity");
		
		
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, snapPattern);
		int beforeCount = beforeFileNames.length;
		
		try {
			com.ibm.jvm.Dump.SnapDump();
			fail("Expected security exception to be thrown.");
		} catch (SecurityException e) {
			//Pass
		}
		
		String[] afterFileNames = getFilesByPattern(userDir, snapPattern);
		
		addNewFilesToDelete(beforeFileNames, afterFileNames);
		
		int afterCount = afterFileNames.length;
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount, afterCount);

		System.setProperty("com.ibm.jvm.enableLegacyDumpSecurity", FALSE.toString());
		
		com.ibm.jvm.Dump.SnapDump();
	
		afterFileNames = getFilesByPattern(userDir, snapPattern);
		
		addNewFilesToDelete(beforeFileNames, afterFileNames);
		
		afterCount = afterFileNames.length;
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount + 1, afterCount);
		
	}
	
	/**
	 * Test a snap dump is generated from the standard com.ibm.jvm.Trace.snap()
	 * call.
	 * Done here rather than in the trace tests as this code knows how to delete
	 * a snap dump.
	 */
	public void testTraceAPISnapDumpNoArgs() {
		String userDir = System.getProperty("user.dir");
		String snapPattern = "Snap\\..*\\.trc";
		
		System.setProperty("com.ibm.jvm.enableLegacyTraceSecurity", FALSE.toString());
		
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, snapPattern);
		int beforeCount = beforeFileNames.length;
		
		com.ibm.jvm.Trace.snap();
		
		String[] afterFileNames = getFilesByPattern(userDir, snapPattern);
		
		addNewFilesToDelete(beforeFileNames, afterFileNames);
		
		int afterCount = afterFileNames.length;
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount + 1, afterCount);
	}
	
	/**
	 * Test a snap dump is blocked from the standard com.ibm.jvm.Trace.snap()
	 * call.
	 * Done here rather than in the trace tests as this code knows how to delete
	 * a snap dump.
	 */
	public void testTraceAPISnapDumpNoArgsBlocked() {
		String userDir = System.getProperty("user.dir");
		String snapPattern = "Snap\\..*\\.trc";
		
		System.clearProperty("com.ibm.jvm.enableLegacyTraceSecurity");
		
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, snapPattern);
		int beforeCount = beforeFileNames.length;
		
		try {
			com.ibm.jvm.Trace.snap();
			fail("Expected security exception to be thrown.");
		} catch (SecurityException e) {
			//Pass
		}
		
		String[] afterFileNames = getFilesByPattern(userDir, snapPattern);
		
		addNewFilesToDelete(beforeFileNames, afterFileNames);
		
		int afterCount = afterFileNames.length;
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount, afterCount);

		System.setProperty("com.ibm.jvm.enableLegacyDumpSecurity", FALSE.toString());
		
		com.ibm.jvm.Dump.SnapDump();
	
		afterFileNames = getFilesByPattern(userDir, snapPattern);
		
		addNewFilesToDelete(beforeFileNames, afterFileNames);
		
		afterCount = afterFileNames.length;
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount + 1, afterCount);
		
	}
	
	public void testSnapDumpWithFile() {
		String userDir = System.getProperty("user.dir");
		
		String expectedName = userDir + File.separator + "snap."+ getName() + "." + uid + ".trc";
		
		// Check the file doesn't exist, otherwise the dump code moves it.
		File expectedFile = new File(expectedName);
		assertFalse("The snap file " + expectedName + " already exists", expectedFile.exists());
		
		// Check we got the correct name.
		try {
			String snapName = com.ibm.jvm.Dump.snapDumpToFile(expectedName);
			fail("Expected security exception to be thrown.");
		} catch (SecurityException e ) {
			// This is the success path!
		} catch (InvalidDumpOptionException e) {
			fail("Expected security exception to be thrown not InvalidDumpOptionException.");
		}
	}
	
	/**
	 * Test a java dump is generated from the standard com.ibm.jvm.Dump.JavaDump()
	 * call.
	 */
	public void testSnapDumpNullFile() {
		String userDir = System.getProperty("user.dir");
		String snapPattern = "Snap\\..*\\.trc";
		
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, snapPattern);
		int beforeCount = beforeFileNames.length;
		
		String snapName = null;
		try {
			snapName = com.ibm.jvm.Dump.snapDumpToFile(null);
			fail("Expected security exception to be thrown.");
		} catch (SecurityException e ) {
			// This is the success path!
		} catch (InvalidDumpOptionException e) {
			fail("Expected security exception to be thrown not InvalidDumpOptionException.");
		}
	}
	
	/**
	 * Test a system dump is generated from the standard com.ibm.jvm.Dump.SystemDump()
	 * call.
	 */
	public void testSystemDumpNoArgs() {
		if( isZOS() ) {
			System.err.printf("Skipping %s, z/OS system dumps currently inaccessable in Java 8, see CMVC 193090\n", this.getName());
			return;
		}
		System.setProperty("com.ibm.jvm.enableLegacyDumpSecurity", FALSE.toString());
		
		String userDir = System.getProperty("user.dir");
		String corePattern = "core\\..*\\.dmp";
		
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, corePattern);
		int beforeCount = beforeFileNames.length;
		
		com.ibm.jvm.Dump.SystemDump();
		
		String[] afterFileNames = getFilesByPattern(userDir, corePattern);
		
		addNewFilesToDelete(beforeFileNames, afterFileNames);
		
		int afterCount = afterFileNames.length;
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount + 1, afterCount);
	}
	
	public void testSystemDumpNoArgsBlocked() {
		if( isZOS() ) {
			System.err.printf("Skipping %s, z/OS system dumps currently inaccessable in Java 8, see CMVC 193090\n", this.getName());
			return;
		}
		
		String userDir = System.getProperty("user.dir");
		String corePattern = "core\\..*\\.dmp";
		
		System.clearProperty("com.ibm.jvm.enableLegacyDumpSecurity");
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, corePattern);
		int beforeCount = beforeFileNames.length;
		
		try {
			com.ibm.jvm.Dump.SystemDump();
			fail("Expected security exception to be thrown.");
		} catch (SecurityException e) {
			//Pass
		}
		
		String[] afterFileNames = getFilesByPattern(userDir, corePattern);
		
		addNewFilesToDelete(beforeFileNames, afterFileNames);
		
		int afterCount = afterFileNames.length;
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount, afterCount);

		System.setProperty("com.ibm.jvm.enableLegacyDumpSecurity", FALSE.toString());
		
		com.ibm.jvm.Dump.SystemDump();
	
		afterFileNames = getFilesByPattern(userDir, corePattern);
		
		addNewFilesToDelete(beforeFileNames, afterFileNames);
		
		afterCount = afterFileNames.length;
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount + 1, afterCount);
		
	}
	
	public void testSystemDumpWithFile() {
		if( isZOS() ) {
			System.err.printf("Skipping %s, z/OS system dumps currently inaccessable in Java 8, see CMVC 193090\n", this.getName());
			return;
		}
		
		String userDir = System.getProperty("user.dir");
		
		String expectedName = userDir + File.separator + "core."+ getName() + "." + uid + ".dmp";
		
		// Check the file doesn't exist, otherwise the dump code moves it.
		File expectedFile = new File(expectedName);
		assertFalse("The core file " + expectedName + " already exists", expectedFile.exists());
		
		// Check we got the correct name.
		try {
			String coreName = com.ibm.jvm.Dump.systemDumpToFile(expectedName);
			fail("Expected security exception to be thrown.");
		} catch (SecurityException e ) {
			// This is the success path!
		} catch (InvalidDumpOptionException e) {
			fail("Expected security exception to be thrown not InvalidDumpOptionException.");
		}
	}
	
	/**
	 * Test a java dump is generated from the standard com.ibm.jvm.Dump.JavaDump()
	 * call.
	 */
	public void testSystemDumpNullFile() {
		if( isZOS() ) {
			System.err.printf("Skipping %s, z/OS system dumps currently inaccessable in Java 8, see CMVC 193090\n", this.getName());
			return;
		}
		
		String userDir = System.getProperty("user.dir");
		String corePattern = "core\\..*\\.dmp";
		
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, corePattern);
		int beforeCount = beforeFileNames.length;
		
		String coreName = null;
		try {
			coreName = com.ibm.jvm.Dump.systemDumpToFile(null);
			fail("Expected security exception to be thrown.");
		} catch (SecurityException e ) {
			// This is the success path!
		} catch (InvalidDumpOptionException e) {
			fail("Expected security exception to be thrown not InvalidDumpOptionException.");
		}
	}

	public void testTriggerToolDump() {
		try {
			// The default options for tool dump may be invalid but we don't expect the dump
			// to actually run.
			com.ibm.jvm.Dump.triggerDump("tool");
			fail("Expected security exception to be thrown.");
		} catch (SecurityException e ) {
			// This is the success path!
		} catch (InvalidDumpOptionException e) {
			fail("Expected security exception to be thrown not InvalidDumpOptionException.");
		} 
	}
	
}
