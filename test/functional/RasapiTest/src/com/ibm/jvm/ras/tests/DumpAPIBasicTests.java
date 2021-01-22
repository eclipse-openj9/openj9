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
import static com.ibm.jvm.ras.tests.DumpAPISuite.getContentType;
import static com.ibm.jvm.ras.tests.DumpAPISuite.getFilesByPattern;
import static com.ibm.jvm.ras.tests.DumpAPISuite.isZOS;

import java.io.File;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

import junit.framework.TestCase;

import com.ibm.jvm.InvalidDumpOptionException;
import com.ibm.jvm.ras.tests.DumpAPISuite.DumpType;

public class DumpAPIBasicTests extends TestCase {

	private long uid = System.currentTimeMillis();
	
	// Record the files created by each testcase here so we can delete them
	// later. Otherwise we will fill the disk up pretty quickly. While we check
	// that the right kind of dump was created for each call we do any further
	// validity checking on the dumps themselves. This test is just checking the
	// API does what it should not that the dumps themselves are correct.
	private Set<String> fileNames;
	
	@Override
	protected void setUp() throws Exception {
		super.setUp();
		fileNames = new HashSet<String>();
		// This isn't to free space, it's to trigger GC tracepoints so we don't get empty snap dumps!
		System.gc();
	}

	@Override
	protected void tearDown() throws Exception {
		super.tearDown();
		for( String fileName: fileNames) { 
			deleteFile(fileName, this.getName());
		}
	}
	
	/**
	 * Test a java dump is generated from the standard com.ibm.jvm.Dump.JavaDump()
	 * call.
	 */
	public void testJavaDumpNoArgs() {
		String userDir = System.getProperty("user.dir");
		String javaCorePattern = "javacore\\..*\\.txt";
		
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, javaCorePattern);
		int beforeCount = beforeFileNames.length;
		
		com.ibm.jvm.Dump.JavaDump();
		
		String[] afterFileNames = getFilesByPattern(userDir, javaCorePattern);
		
		addNewFilesToDelete(beforeFileNames, afterFileNames);
		
		int afterCount = afterFileNames.length;
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount + 1, afterCount);
	}

	private void addNewFilesToDelete(String[] beforeFileNames,
			String[] afterFileNames) {
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

	public void testJavaDumpWithFile() {
		String userDir = System.getProperty("user.dir");
		
		String expectedName = userDir + File.separator + "javacore."+ getName() + "." + uid + ".txt";
		
		// Check the file doesn't exist, otherwise the dump code moves it.
		File expectedFile = new File(expectedName);
		assertFalse("The javacore file " + expectedName + " already exists", expectedFile.exists());
		
		// Check we got the correct name.
		String javacoreName = null;
		try {
			javacoreName = com.ibm.jvm.Dump.javaDumpToFile(expectedName);
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}
		
		assertNotNull("Expected javacore filename to be returned, not null", javacoreName);
		fileNames.add(javacoreName);
		assertEquals("Expected javacore to be written to file: " + expectedName + " but was written to " + javacoreName, expectedName, javacoreName);
		
		// Check it really exists.
		File javacoreFile = new File(javacoreName);
		assertTrue("Failed to find files " + javacoreName + " after requesting " + javacoreName, javacoreFile.exists());
		DumpType type = getContentType(javacoreFile);
		assertEquals("Expected file " + javacoreName + " to contain a java core but content type was: " + type, DumpType.JAVA_TYPE, type);
	}
	
	/** Test what happens when a filename includes options e.g "heap.phd,opts=CLASSIC"
	 */
	public void testJavaDumpWithOptions() {
		String[] options = {
		    	",exec=gdb",
		    	",file=foo.dmp",
		    	",filter=java/lang/OutOfMemoryError",
		    	",opts=CLASSIC",
		    	",priority=500",
		    	",range=1..6",
		    	",request=exclusive",
		    	",suspendwith=2",
		    };
		
		String userDir = System.getProperty("user.dir");
		
		String expectedName = userDir + File.separator + "javacore."+ getName() + "." + uid + ".txt";
		
		// Check the file doesn't exist, otherwise the dump code moves it.
		File expectedFile = new File(expectedName);
		assertFalse("The java core file " + expectedName + " already exists", expectedFile.exists());
		
		// Check none of these options works in a file name.
		for( String option : options ) {
			String javaDumpName = null;
			try {
				javaDumpName = com.ibm.jvm.Dump.javaDumpToFile(expectedName + option);
				fail("Expected InvalidDumpOption exception to be thrown instead " + javaDumpName + " was written when specifying: " + option);
			} catch (InvalidDumpOptionException e ) {
			}
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
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}

		assertNotNull("Expected javacore filename to be returned, not null", javacoreName);
		fileNames.add(javacoreName);
		
		// Check the generated file really exists.
		File javacoreFile = new File(javacoreName);
		assertTrue("Failed to find files " + javacoreName + " after requesting " + javacoreName, javacoreFile.exists());
		
		// Check the number of files has increased. (We weren't returned a filename that already existed.)
		String[] afterFileNames = getFilesByPattern(userDir, javaCorePattern);
		int afterCount = afterFileNames.length;
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount + 1, afterCount);

		// Check content
		DumpType type = getContentType(javacoreFile);
		assertEquals("Expected file " + javacoreName + " to contain a java core but content type was: " + type, DumpType.JAVA_TYPE, type);
	}
	
	/**
	 * Test a java dump is generated from the standard com.ibm.jvm.Dump.JavaDump()
	 * call.
	 */
	public void testJavaDumpEmptyFile() {
		String userDir = System.getProperty("user.dir");
		String javaCorePattern = "javacore\\..*\\.txt";
		
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, javaCorePattern);
		int beforeCount = beforeFileNames.length;
	
		String javacoreName = null;
		try {
			javacoreName = com.ibm.jvm.Dump.javaDumpToFile("");
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}

		assertNotNull("Expected javacore filename to be returned, not null", javacoreName);
		fileNames.add(javacoreName);
		
		// Check the generated file really exists.
		File javacoreFile = new File(javacoreName);
		assertTrue("Failed to find files " + javacoreName + " after requesting " + javacoreName, javacoreFile.exists());
		
		// Check the number of files has increased. (We weren't returned a filename that already existed.)
		String[] afterFileNames = getFilesByPattern(userDir, javaCorePattern);
		int afterCount = afterFileNames.length;
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount + 1, afterCount);
		
		// Check content
		DumpType type = getContentType(javacoreFile);
		assertEquals("Expected file " + javacoreName + " to contain a java core but content type was: " + type, DumpType.JAVA_TYPE, type);

	}
	
	/**
	 * Test a java dump isn't generated when the path is "-"
	 */
	public void testJavaDumpToDashFile() {
		String userDir = System.getProperty("user.dir");
		String javaCorePattern = "javacore\\..*\\.txt";
		
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, javaCorePattern);
		int beforeCount = beforeFileNames.length;
	
		String javacoreName = null;
		try {
			javacoreName = com.ibm.jvm.Dump.javaDumpToFile("-");
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}

		// Check the number of files has stayed the same
		String[] afterFileNames = getFilesByPattern(userDir, javaCorePattern);
		int afterCount = afterFileNames.length;
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount, afterCount);
		
	}
	
	/* Test we relocate files if they clash with an existing file, but only once! */
	public void testJavaDumpWithSameFile() {
		String userDir = System.getProperty("user.dir");
		
		String expectedName = userDir + File.separator + "javacore."+ getName() + "." + uid + ".txt";
		
		// Check the file doesn't exist, otherwise the dump code moves it.
		File expectedFile = new File(expectedName);
		assertFalse("The javacore file " + expectedName + " already exists", expectedFile.exists());
		
		// Check we got the correct name.
		String javacoreName = null;
		try {
			javacoreName = com.ibm.jvm.Dump.javaDumpToFile(expectedName);
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}
		
		assertNotNull("Expected javacore filename to be returned, not null", javacoreName);
		fileNames.add(javacoreName);
		assertEquals("Expected javacore to be written to file: " + expectedName + " but was written to " + javacoreName, expectedName, javacoreName);
		
		// Check it really exists.
		File javacoreFile = new File(javacoreName);
		assertTrue("Failed to find files " + javacoreName + " after requesting " + javacoreName, javacoreFile.exists());
		
		// Check we get a different name the second time.
		
		String javacoreName2 = null;
		try {
			javacoreName2 = com.ibm.jvm.Dump.javaDumpToFile(expectedName);
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}
		
		assertNotNull("Expected javacore filename to be returned, not null", javacoreName2);
		fileNames.add(javacoreName2);
		assertFalse(
				"Expected second dump file to be written to a different location. Original dump: "
				+ javacoreName + " second dump: " + javacoreName2, javacoreName.equals(javacoreName2));
		// Check it really exists.
		File javacoreFile2 = new File(javacoreName2);
		assertTrue("Failed to find files " + javacoreName2
				+ " after requesting " + javacoreName + " a second time.",
				javacoreFile2.exists());

		// Now check this file is replaced if we end up writing to it again
		// the same way.
		long timestamp2 = javacoreFile2.lastModified();

		// Some timestamps are only down to the second on some platforms.
		// Dumps can happen quicker than that.
		try {
			Thread.sleep(1000);
		} catch (InterruptedException e){
			// Ignore.
		}
		
		// Check we get a different name the second time.
		String javacoreName3 = null;
		try {
			javacoreName3 = com.ibm.jvm.Dump.javaDumpToFile(expectedName);
			fail("Expected to fail when we tried overwriting the file we failed over to.");
		} catch (InvalidDumpOptionException e ) {
			System.out.println(e);
			System.out.println("reach here");
		}

	}
	
	/*
	 * Test a heap dump is generated from the standard com.ibm.jvm.Dump.HeapDump()
	 * call.
	 */
	public void testHeapDumpNoArgs() {
		String userDir = System.getProperty("user.dir");
		String javaCorePattern = "heapdump\\..*\\.phd";
		
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, javaCorePattern);
		int beforeCount = beforeFileNames.length;
		
		com.ibm.jvm.Dump.HeapDump();
		
		String[] afterFileNames = getFilesByPattern(userDir, javaCorePattern);
		
		addNewFilesToDelete(beforeFileNames, afterFileNames);
		
		int afterCount = afterFileNames.length;
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount + 1, afterCount);
	}

	public void testHeapDumpWithFile() {
		String userDir = System.getProperty("user.dir");
		
		String expectedName = userDir + File.separator + "heapdump."+ getName() + "." + uid + ".phd";
		
		// Check the file doesn't exist, otherwise the dump code moves it.
		File expectedFile = new File(expectedName);
		assertFalse("The javacore file " + expectedName + " already exists", expectedFile.exists());
		
		// Check we got the correct name.
		String heapDumpName = null;
		try {
			heapDumpName = com.ibm.jvm.Dump.heapDumpToFile(expectedName);
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}
		
		assertNotNull("Expected javacore filename to be returned, not null", heapDumpName);
		fileNames.add(heapDumpName);
		assertEquals("Expected javacore to be written to file: " + expectedName + " but was written to " + heapDumpName, expectedName, heapDumpName);
		
		// Check it really exists.
		File heapDumpFile = new File(heapDumpName);
		assertTrue("Failed to find files " + heapDumpName + " after requesting " + heapDumpName, heapDumpFile.exists());
		
		// Check content
		DumpType type = getContentType(heapDumpFile);
		assertEquals("Expected file " + heapDumpName + " to contain a heap dump but content type was: " + type, DumpType.PHD_HEAP_TYPE, type);

	}
	
	/**
	 * Test what happens when a filename includes options e.g "heap.phd,opts=CLASSIC"
	 */
	public void testHeapDumpWithOptions() {
		String[] options = {
		    	",exec=gdb",
		    	",file=foo.dmp",
		    	",filter=java/lang/OutOfMemoryError",
		    	",opts=CLASSIC",
		    	",priority=500",
		    	",range=1..6",
		    	",request=exclusive",
		    	",suspendwith=2",
		    };
		
		String userDir = System.getProperty("user.dir");
		
		String expectedName = userDir + File.separator + "heapdump."+ getName() + "." + uid + ".phd";
		
		// Check the file doesn't exist, otherwise the dump code moves it.
		File expectedFile = new File(expectedName);
		assertFalse("The heap dump file " + expectedName + " already exists", expectedFile.exists());
		
		// Check none of these options works in a file name.
		for( String option : options ) {
			String heapDumpName = null;
			try {
				heapDumpName = com.ibm.jvm.Dump.heapDumpToFile(expectedName + option);
				fail("Expected InvalidDumpOption exception to be thrown instead " + heapDumpName + " was written when specifying: " + option);
			} catch (InvalidDumpOptionException e ) {
			}
		}
	}
	
	/**
	 * Test a java dump is generated from the standard com.ibm.jvm.Dump.JavaDump()
	 * call.
	 */
	public void testHeapDumpNullFile() {
		String userDir = System.getProperty("user.dir");
		String heapDumpPattern = "heapdump\\..*\\.phd";
		
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, heapDumpPattern);
		int beforeCount = beforeFileNames.length;
		
		String heapDumpName = null;
		try {
			heapDumpName = com.ibm.jvm.Dump.heapDumpToFile(null);
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}
		
		assertNotNull("Expected heapdump filename to be returned, not null", heapDumpName);
		fileNames.add(heapDumpName);
		
		// Check the generated file really exists.
		File heapDumpFile = new File(heapDumpName);
		assertTrue("Failed to find files " + heapDumpName + " after requesting " + heapDumpName, heapDumpFile.exists());
		
		// Check the number of files has increased. (We weren't returned a filename that already existed.)
		String[] afterFileNames = getFilesByPattern(userDir, heapDumpPattern);
		int afterCount = afterFileNames.length;
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount + 1, afterCount);
		
		// Check content
		DumpType type = getContentType(heapDumpFile);
		assertEquals("Expected file " + heapDumpName + " to contain a heap dump but content type was: " + type, DumpType.PHD_HEAP_TYPE, type);
	}
	
	/**
	 * Test a heap dump isn't generated when the path is "-"
	 */
	public void testHeapDumpToDashFile() {
		String userDir = System.getProperty("user.dir");
		String javaCorePattern = "heap\\..*\\.phd";
		
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, javaCorePattern);
		int beforeCount = beforeFileNames.length;
		
		String heapName = null;
		try {
			heapName = com.ibm.jvm.Dump.heapDumpToFile("-");
			fail("Expected InvalidDumpOption exception to be thrown filename " + heapName + " was returned");
		} catch (InvalidDumpOptionException e ) {
			// Pass
		}
	}
	
	/* Test we relocate files if they clash with an existing file, but only once! */
	public void testHeapDumpWithSameFile() {
		String userDir = System.getProperty("user.dir");
		
		String expectedName = userDir + File.separator + "heapdump."+ getName() + "." + uid + ".phd";
		
		// Check the file doesn't exist, otherwise the dump code moves it.
		File expectedFile = new File(expectedName);
		assertFalse("The heapdump file " + expectedName + " already exists", expectedFile.exists());
		
		// Check we got the correct name.
		String heapDumpName = null;
		try {
			heapDumpName = com.ibm.jvm.Dump.heapDumpToFile(expectedName);
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}
		
		assertNotNull("Expected heapdump filename to be returned, not null", heapDumpName);
		fileNames.add(heapDumpName);
		assertEquals("Expected heapdump to be written to file: " + expectedName + " but was written to " + heapDumpName, expectedName, heapDumpName);
		
		// Check it really exists.
		File heapdumpFile = new File(heapDumpName);
		assertTrue("Failed to find files " + heapDumpName + " after requesting " + heapDumpName, heapdumpFile.exists());
		
		// Check we get a different name the second time.
		String heapdumpName2 = null;
		try {
			heapdumpName2 = com.ibm.jvm.Dump.heapDumpToFile(expectedName);
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}
		
		assertNotNull("Expected heapdump filename to be returned, not null", heapdumpName2);
		fileNames.add(heapdumpName2);
		assertFalse(
				"Expected second dump file to be written to a different location. Original dump: "
				+ heapDumpName + " second dump: " + heapdumpName2, heapDumpName.equals(heapdumpName2));
		// Check it really exists.
		File heapdumpFile2 = new File(heapdumpName2);
		assertTrue("Failed to find files " + heapdumpName2
				+ " after requesting " + heapDumpName + " a second time.",
				heapdumpFile2.exists());

		// Now check this file is replaced if we end up writing to it again
		// the same way.
		long timestamp2 = heapdumpFile2.lastModified();

		// Some timestamps are only down to the second on some platforms.
		// Dumps can happen quicker than that.
		try {
			Thread.sleep(1000);
		} catch (InterruptedException e){
			// Ignore.
		}
		
		// Check we get a different name the second time.
		String heapdumpName3 = null;
		try {
			heapdumpName3 = com.ibm.jvm.Dump.heapDumpToFile(expectedName);
			fail("Expected to fail when we tried overwriting the file we failed over to.");
		} catch (InvalidDumpOptionException e ) {
		}
		
	}

	
	/**
	 * Test a snap dump is generated from the standard com.ibm.jvm.Dump.SnapDump()
	 * call.
	 */
	public void testSnapDumpNoArgs() {
		String userDir = System.getProperty("user.dir");
		String javaCorePattern = "Snap\\..*\\.trc";
		
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, javaCorePattern);
		int beforeCount = beforeFileNames.length;
		
		com.ibm.jvm.Dump.SnapDump();
		
		String[] afterFileNames = getFilesByPattern(userDir, javaCorePattern);
		
		addNewFilesToDelete(beforeFileNames, afterFileNames);
		
		int afterCount = afterFileNames.length;
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount + 1, afterCount);
	}
	
	public void testSnapDumpWithFile() {
		String userDir = System.getProperty("user.dir");
		
		String expectedName = userDir + File.separator + "snap."+ getName() + "." + uid + ".trc";
		
		// Check the file doesn't exist, otherwise the dump code moves it.
		File expectedFile = new File(expectedName);
		assertFalse("The snap file " + expectedName + " already exists", expectedFile.exists());
		
		// Check we got the correct name.
		String snapName = null;
		try {
			snapName = com.ibm.jvm.Dump.snapDumpToFile(expectedName);
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}
		
		assertNotNull("Expected snap filename to be returned, not null", snapName);
		fileNames.add(snapName);
		assertEquals("Expected snap to be written to file: " + expectedName + " but was written to " + snapName, expectedName, snapName);
		
		// Check it really exists.
		File snapFile = new File(snapName);
		assertTrue("Failed to find files " + snapName + " after requesting " + snapName, snapFile.exists());
		
		// Check content
		DumpType type = getContentType(snapFile);
		assertEquals("Expected file " + snapName + " to contain a snap trace but content type was: " + type, DumpType.SNAP_TYPE, type);

	}
	
	/**
	 * Test what happens when a filename includes options e.g "heap.phd,opts=CLASSIC"
	 */
	public void testSnapDumpWithOptions() {
		String[] options = {
		    	",exec=gdb",
		    	",file=foo.dmp",
		    	",filter=java/lang/OutOfMemoryError",
		    	",opts=CLASSIC",
		    	",priority=500",
		    	",range=1..6",
		    	",request=exclusive",
		    	",suspendwith=2",
		    };
		
		String userDir = System.getProperty("user.dir");
		
		String expectedName = userDir + File.separator + "Snap."+ getName() + "." + uid + ".trc";
		
		// Check the file doesn't exist, otherwise the dump code moves it.
		File expectedFile = new File(expectedName);
		assertFalse("The snap dump file " + expectedName + " already exists", expectedFile.exists());
		
		// Check none of these options works in a file name.
		for( String option : options ) {
			String snapDumpName = null;
			try {
				snapDumpName = com.ibm.jvm.Dump.snapDumpToFile(expectedName + option);
				fail("Expected InvalidDumpOption exception to be thrown instead " + snapDumpName + " was written when specifying: " + option);
			} catch (InvalidDumpOptionException e ) {
			}
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
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}
		
		assertNotNull("Expected snap filename to be returned, not null", snapName);
		fileNames.add(snapName);
		
		// Check the generated file really exists.
		File snapFile = new File(snapName);
		assertTrue("Failed to find files " + snapName + " after requesting " + snapName, snapFile.exists());
		
		// Check the number of files has increased. (We weren't returned a filename that already existed.)
		String[] afterFileNames = getFilesByPattern(userDir, snapPattern);
		int afterCount = afterFileNames.length;
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount + 1, afterCount);
		
		// Check content
		DumpType type = getContentType(snapFile);
		assertEquals("Expected file " + snapName + " to contain a snap trace but content type was: " + type, DumpType.SNAP_TYPE, type);

	}
	
	/**
	 * Test a snap dump isn't generated when the path is "-"
	 */
	public void testSnapDumpToDashFile() {
		String userDir = System.getProperty("user.dir");
		String javaCorePattern = "snap\\..*\\.trc";
		
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, javaCorePattern);
		int beforeCount = beforeFileNames.length;
		
		String snapName = null;
		try {
			snapName = com.ibm.jvm.Dump.snapDumpToFile("-");
			fail("Expected InvalidDumpOption exception to be thrown filename " + snapName + " was returned");
		} catch (InvalidDumpOptionException e ) {
			// Pass
		}
	}
	
	/* Test we relocate files if they clash with an existing file, but only once! */
	public void testSnapDumpWithSameFile() {
		String userDir = System.getProperty("user.dir");
		
		String expectedName = userDir + File.separator + "Snap."+ getName() + "." + uid + ".trc";
		
		// Check the file doesn't exist, otherwise the dump code moves it.
		File expectedFile = new File(expectedName);
		assertFalse("The Snap file " + expectedName + " already exists", expectedFile.exists());
		
		// Check we got the correct name.
		String snapName = null;
		try {
			snapName = com.ibm.jvm.Dump.snapDumpToFile(expectedName);
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}
		assertNotNull("Expected Snap filename to be returned, not null", snapName);
		fileNames.add(snapName);
		assertEquals("Expected Snap to be written to file: " + expectedName + " but was written to " + snapName, expectedName, snapName);
		
		// Check it really exists.
		File snapFile = new File(snapName);
		assertTrue("Failed to find files " + snapName + " after requesting " + snapName, snapFile.exists());
		
		// Check we get a different name the second time.
		String snapName2 = null;
		try {
			snapName2 = com.ibm.jvm.Dump.snapDumpToFile(expectedName);
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}
		
		assertNotNull("Expected Snap filename to be returned, not null", snapName2);
		fileNames.add(snapName2);
		assertFalse(
				"Expected second dump file to be written to a different location. Original dump: "
				+ snapName + " second dump: " + snapName2, snapName.equals(snapName2));
		// Check it really exists.
		File snapFile2 = new File(snapName2);
		assertTrue("Failed to find files " + snapName2
				+ " after requesting " + snapName + " a second time.",
				snapFile2.exists());

		// Now check this file is replaced if we end up writing to it again
		// the same way.
		long timestamp2 = snapFile2.lastModified();

		// Some timestamps are only down to the second on some platforms.
		// Dumps can happen quicker than that.
		try {
			Thread.sleep(1000);
		} catch (InterruptedException e){
			// Ignore.
		}
		
		// Check we get a different name the second time.
		String snapName3 = null;
		try {
			snapName3 = com.ibm.jvm.Dump.snapDumpToFile(expectedName);
			fail("Expected to fail when we tried overwriting the file we failed over to.");
		} catch (InvalidDumpOptionException e ) {
		}

//		assertNotNull("Expected Snap filename to be returned, not null", snapName);
//		assertFalse(
//				"Expected third dump file to be written to a different location. Original dump: "
//				+ snapName + " second dump: " + snapName3, snapName.equals(snapName3));
//		assertEquals(
//				"Expected third dump file to be written to the same location as the second location. Original dump: "
//				+ snapName + " second dump: " + snapName2 + " third dump " + snapName3, snapName2, snapName3);
//		// Check it really exists.
//		File snapFile3 = new File(snapName3);
//		assertTrue("Failed to find files " + snapName3
//				+ " after requesting " + snapName + " a second time.",
//				snapFile3.exists());
//		
//		// Now check this file is replaced if we end up writing to it again
//		// the same way.
//		long timestamp3 = snapFile3.lastModified();
//		System.err.println("Timestamp 3: " + timestamp3 + " > " + timestamp2);
//		assertTrue("Expected " + snapName3 + " to be updated by second dump to " + expectedName, timestamp3 > timestamp2);

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
		
		String userDir = System.getProperty("user.dir");
		String javaCorePattern = "core\\..*\\.dmp";
		
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, javaCorePattern);
		int beforeCount = beforeFileNames.length;
		
		com.ibm.jvm.Dump.SystemDump();
		
		String[] afterFileNames = getFilesByPattern(userDir, javaCorePattern);
		
		addNewFilesToDelete(beforeFileNames, afterFileNames);
		
		int afterCount = afterFileNames.length;
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
		String coreName = null;
		try {
			coreName = com.ibm.jvm.Dump.systemDumpToFile(expectedName);
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}
		assertNotNull("Expected core filename to be returned, not null", coreName);
		fileNames.add(coreName);
		assertEquals("Expected core to be written to file: " + expectedName + " but was written to " + coreName, expectedName, coreName);
		
		// Check it really exists.
		File coreFile = new File(coreName);
		assertTrue("Failed to find files " + coreName + " after requesting " + coreName, coreFile.exists());
		
		// Check content
		DumpType type = getContentType(coreFile);
		assertEquals("Expected file " + coreName + " to contain a system core but content type was: " + type, DumpType.SYSTEM_TYPE, type);

	}
	
	/**
	 * Test what happens when a filename includes options e.g "heap.phd,opts=CLASSIC"
	 */
	public void testSystemDumpWithOptions() {
		if( isZOS() ) {
			System.err.printf("Skipping %s, z/OS system dumps currently inaccessable in Java 8, see CMVC 193090\n", this.getName());
			return;
		}
		
		String[] options = {
		    	",exec=gdb",
		    	",file=foo.dmp",
		    	",filter=java/lang/OutOfMemoryError",
		    	",opts=CLASSIC",
		    	",priority=500",
		    	",range=1..6",
		    	",request=exclusive",
		    	",suspendwith=2",
		    };
		
		String userDir = System.getProperty("user.dir");
		
		String expectedName = userDir + File.separator + "core."+ getName() + "." + uid + ".dmp";
		
		// Check the file doesn't exist, otherwise the dump code moves it.
		File expectedFile = new File(expectedName);
		assertFalse("The core dump file " + expectedName + " already exists", expectedFile.exists());
		
		// Check none of these options works in a file name.
		for( String option : options ) {
			String coreDumpName = null;
			try {
				coreDumpName = com.ibm.jvm.Dump.systemDumpToFile(expectedName + option);
				fail("Expected InvalidDumpOption exception to be thrown instead " + coreDumpName + " was written when specifying: " + option);
			} catch (InvalidDumpOptionException e ) {
			}
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
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}
		
		assertNotNull("Expected core filename to be returned, not null", coreName);
		fileNames.add(coreName);
		
		// Check the generated file really exists.
		File coreFile = new File(coreName);
		assertTrue("Failed to find files " + coreName + " after requesting " + coreName, coreFile.exists());
		
		// Check the number of files has increased. (We weren't returned a filename that already existed.)
		String[] afterFileNames = getFilesByPattern(userDir, corePattern);
		int afterCount = afterFileNames.length;
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount + 1, afterCount);
		
		// Check content
		DumpType type = getContentType(coreFile);
		assertEquals("Expected file " + coreName + " to contain a system core but content type was: " + type, DumpType.SYSTEM_TYPE, type);

	}
	
	/**
	 * Test a core dump isn't generated when the path is "-"
	 */
	public void testSystemDumpToDashFile() {
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
			coreName = com.ibm.jvm.Dump.systemDumpToFile("-");
			fail("Expected InvalidDumpOption exception to be thrown filename " + coreName + " was returned");
		} catch (InvalidDumpOptionException e ) {
			// Pass
		}
	}
	
	/* Test we relocate files if they clash with an existing file, but only once! */
	public void testSystemDumpWithSameFile() {
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
		String coreName = null;
		try {
			coreName = com.ibm.jvm.Dump.systemDumpToFile(expectedName);
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}
		assertNotNull("Expected core filename to be returned, not null", coreName);
		fileNames.add(coreName);
		assertEquals("Expected core to be written to file: " + expectedName + " but was written to " + coreName, expectedName, coreName);
		
		// Check it really exists.
		File coreFile = new File(coreName);
		assertTrue("Failed to find files " + coreName + " after requesting " + coreName, coreFile.exists());
		
		// Check we get a different name the second time.
		String coreName2 = null;
		try {
			coreName2 = com.ibm.jvm.Dump.systemDumpToFile(expectedName);
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}
		
		assertNotNull("Expected core filename to be returned, not null", coreName2);
		fileNames.add(coreName2);
		assertFalse(
				"Expected second dump file to be written to a different location. Original dump: "
				+ coreName + " second dump: " + coreName2, coreName.equals(coreName2));
		// Check it really exists.
		File coreFile2 = new File(coreName2);
		assertTrue("Failed to find files " + coreName2
				+ " after requesting " + coreName + " a second time.",
				coreFile2.exists());

		// Now check this file is replaced if we end up writing to it again
		// the same way.
		long timestamp2 = coreFile2.lastModified();

		// Some timestamps are only down to the second on some platforms.
		// Dumps can happen quicker than that.
		try {
			Thread.sleep(1000);
		} catch (InterruptedException e){
			// Ignore.
		}
	
		// Check we get a different name the second time.
		String coreName3 = null;
		try {
			coreName3 = com.ibm.jvm.Dump.systemDumpToFile(expectedName);
			fail("Expected to fail when we tried overwriting the file we failed over to.");
		} catch (InvalidDumpOptionException e ) {
		}
		
//		assertNotNull("Expected core filename to be returned, not null", coreName);
//		assertFalse(
//				"Expected third dump file to be written to a different location. Original dump: "
//				+ coreName + " second dump: " + coreName3, coreName.equals(coreName3));
//		assertEquals(
//				"Expected third dump file to be written to the same location as the second location. Original dump: "
//				+ coreName + " second dump: " + coreName2 + " third dump " + coreName3, coreName2, coreName3);
//		// Check it really exists.
//		File coreFile3 = new File(coreName3);
//		assertTrue("Failed to find files " + coreName3
//				+ " after requesting " + coreName + " a second time.",
//				coreFile3.exists());
//		
//		// Now check this file is replaced if we end up writing to it again
//		// the same way.
//		long timestamp3 = coreFile3.lastModified();
//		System.err.println("Timestamp 3: " + timestamp3 + " > " + timestamp2);
//		assertTrue("Expected " + coreName3 + " to be updated by second dump to " + expectedName, timestamp3 > timestamp2);

	}

}
