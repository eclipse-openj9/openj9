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

import static com.ibm.jvm.ras.tests.DumpAPISuite.DUMP_COULD_NOT_CREATE_FILE;
import static com.ibm.jvm.ras.tests.DumpAPISuite.DUMP_ERROR_PARSING;
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

public class DumpAPITriggerTests extends TestCase {
	
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
		// TODO Auto-generated method stub
		super.tearDown();
		for( String fileName: fileNames) { 
			deleteFile(fileName, this.getName());
		}
	}
	
	public void testTriggerDumpNullOpts() {
		try {
			doTestTriggerBadDumpX(null);
			fail("Expected NPE to be thrown.");
		} catch( NullPointerException npe ) {
			// pass
		}
	}
	
	public void testTriggerDumpEmptyOpts() {
		doTestTriggerBadDumpX("");
	}
	
	public void testTriggerDumpJavaNoOpts() {
		doTestTriggerDumpNoOpts("java", DumpType.JAVA_TYPE);
	}
	
	public void testTriggerDumpSystemNoOpts() {
		if( isZOS() ) {
			System.err.printf("Skipping %s, z/OS system dumps currently inaccessable in Java 8, see CMVC 193090\n", this.getName());
			return;
		}
		doTestTriggerDumpNoOpts("system", DumpType.SYSTEM_TYPE);
	}
	
	public void testTriggerDumpHeapNoOpts() {
		doTestTriggerDumpNoOpts("heap", DumpType.PHD_HEAP_TYPE);
	}
	
	public void testTriggerDumpSnapNoOpts() {
		doTestTriggerDumpNoOpts("snap", DumpType.SNAP_TYPE);
	}
	
//	Don't test this, the default dump agent runs the debugger and connects back to this process.
//  It's a great way to hang the test cases.
//	public void testTriggerDumpToolNoOpts() {
//		doTestTriggerDumpNoOpts("snap", DumpType.TOOL_TYPE);
//	}
	
	public void testTriggerDumpConsoleNoOpts() {
		// Default console dump goes to stderr therefore no file will be found.
		doTestTriggerDumpNoOpts("console", DumpType.FILE_NOT_FOUND);
	}

	public void testTriggerDumpStackNoOpts() {
		// Default stack dump goes to stderr therefore no file will be found.
		doTestTriggerDumpNoOpts("stack", DumpType.FILE_NOT_FOUND);
	}
	
	public void testTriggerDumpSilentNoOpts() {
		doTestTriggerDumpNoOpts("silent", DumpType.FILE_NOT_FOUND);
	}
	
	public void testTriggerDumpBadAgentNoOpts() {
		// The chances of us adding a new dump type called walrus in the future are small.
		InvalidDumpOptionException e = doTestTriggerBadDumpX("walrus");
		assertEquals("Incorrect message triggering tool dump.", DUMP_ERROR_PARSING, e.getMessage());
	}
	
	public void testTriggerDumpJavaToFile() {
		doTestTriggerDumpWithFile("java", "javacore." + getName() + "." + uid + ".txt", DumpType.JAVA_TYPE);
	}
	
	public void testTriggerDumpSystemToFile() {
		if( isZOS() ) {
			System.err.printf("Skipping %s, z/OS system dumps currently inaccessable in Java 8, see CMVC 193090\n", this.getName());
			return;
		}
		doTestTriggerDumpWithFile("system", "system." + getName() + "." + uid + ".dmp", DumpType.SYSTEM_TYPE);
	}
	
	public void testTriggerDumpHeapToFile() {
		doTestTriggerDumpWithFile("heap", "heap." + getName() + "." + uid + ".phd", DumpType.PHD_HEAP_TYPE);
	}
	
	public void testTriggerDumpClassicHeapToFile() {
		String fileName = "heap." + getName() + "." + uid + ".txt";
		String type = "heap";
		File dumpFile = new File(fileName);
		assertFalse("Found file " + fileName + " before requesting " + fileName, dumpFile.exists());
		doTestTriggerDumpX(type +":file=" + fileName + ",opts=CLASSIC", DumpType.CLASSIC_HEAP_TYPE);
		assertTrue("Failed to find file " + fileName + " after requesting " + fileName, dumpFile.exists());
	}
	
	
	public void testTriggerDumpSnapToFile() {
		doTestTriggerDumpWithFile("snap", "snap." + getName() + "." + uid + ".trc", DumpType.SNAP_TYPE);
	}
	
	public void testTriggerDumpTool() {
		// This will be slightly broken as no file will be created but as we don't get the return code
		// from the command we just want to check the string is accepted and we attempt to run something.
		doTestTriggerDumpX("tool:exec=tool." + getName() + "." + uid + ".txt", DumpType.FILE_NOT_FOUND);
	}
	
	public void testTriggerDumpConsoleToFile() {
		doTestTriggerDumpWithFile("console", "console." + getName() + "." + uid + ".txt", DumpType.CONSOLE_TYPE);
	}

	public void testTriggerDumpStackToFile() {
		doTestTriggerDumpWithFile("stack", "stack." + getName() + "." + uid + ".txt", DumpType.STACK_TYPE);
	}
	
	public void testTriggerDumpSilentToFile() {
		doTestTriggerDumpWithFile("silent", "silent." + getName() + "." + uid + ".txt", DumpType.FILE_NOT_FOUND);
	}
	
	public void testTriggerDumpBadAgentToFile() {
		InvalidDumpOptionException e = doTestTriggerBadDumpX("walrus:label=walrus." + getName() + "." + uid + ".txt");
		assertEquals("Incorrect message triggering tool dump.", DUMP_ERROR_PARSING, e.getMessage());
	}
	
	public void testTriggerDumpJavaNoFile() {
		InvalidDumpOptionException e = doTestTriggerBadDumpX("java:file=");
		assertEquals("Incorrect message triggering java dump with missing file.", DUMP_COULD_NOT_CREATE_FILE, e.getMessage());
	}

	public void testTriggerDumpSystemNoFile() {
		if( isZOS() ) {
			System.err.printf("Skipping %s, z/OS system dumps currently inaccessable in Java 8, see CMVC 193090\n", this.getName());
			return;
		}
		InvalidDumpOptionException e = doTestTriggerBadDumpX("system:label=");
		assertEquals("Incorrect message triggering system dump with missing file.", DUMP_COULD_NOT_CREATE_FILE, e.getMessage());
	}

	public void testTriggerDumpHeapNoFile() {
		InvalidDumpOptionException e = doTestTriggerBadDumpX("heap:file=");
		assertEquals("Incorrect message triggering heap dump with missing file.", DUMP_COULD_NOT_CREATE_FILE, e.getMessage());
	}

	public void testTriggerDumpSnapNoFile() {
		InvalidDumpOptionException e = doTestTriggerBadDumpX("snap:file=");
		assertEquals("Incorrect message triggering snap dump with missing file.", DUMP_COULD_NOT_CREATE_FILE, e.getMessage());
	}

	public void testTriggerDumpConsoleNoFile() {
		InvalidDumpOptionException e = doTestTriggerBadDumpX("console:file=");
		assertEquals("Incorrect message triggering console dump with missing file.", DUMP_COULD_NOT_CREATE_FILE, e.getMessage());
	}

	public void testTriggerDumpStackNoFile() {
		InvalidDumpOptionException e = doTestTriggerBadDumpX("stack:file=");
		assertEquals("Incorrect message triggering java dump with missing file.", DUMP_COULD_NOT_CREATE_FILE, e.getMessage());
	}

	public void testTriggerDumpJavaBadFile() {
		InvalidDumpOptionException e = doTestTriggerBadDumpX("java:file=..");
		assertEquals("Incorrect message triggering java dump with missing file.", DUMP_COULD_NOT_CREATE_FILE, e.getMessage());
	}

	public void testTriggerDumpJavaToDirectory() {
		InvalidDumpOptionException e = doTestTriggerBadDumpX("java:file=javaDump" + File.separator);
		assertEquals("Incorrect message triggering java dump with missing file.", DUMP_COULD_NOT_CREATE_FILE, e.getMessage());
	}
	
	public void testTriggerDumpConsoleDashFile() {
		doTestTriggerDumpWithFile("console", "-", DumpType.FILE_NOT_FOUND);
	}
	
	public void testTriggerDumpStackDashFile() {
		doTestTriggerDumpWithFile("stack", "-", DumpType.FILE_NOT_FOUND);
	}
	
	public void testTriggerDumpJavaDashFile() {
		doSTDOUTTestTriggerDumpX("java:file=-");
	}
	
	public void testTriggerMultipleDumpsNoOpts() {
		InvalidDumpOptionException e = doTestTriggerBadDumpX("java+heap");
		assertEquals("Incorrect message triggering java dump with missing file.", DUMP_ERROR_PARSING, e.getMessage());
	}

	/* JIT dumps currently do nothing. */
	/*public void testTriggerDumpJit() {
		doTestTriggerDump("jit");
	}*/
	
	public void doTestTriggerDumpNoOpts(String type, DumpType expectedType) {
		doTestTriggerDumpX(type, expectedType);
	}
	

	/**
	 * Triggers a dump to a specific file name and checks it exists.
	 * Assumes that the filename does not contain wild cards.
	 * @param type
	 * @param fileName
	 */
	public void doTestTriggerDumpWithFile(String type, String fileName, DumpType expectedType) {
		File dumpFile = new File(fileName);
		assertFalse("Found file " + fileName + " before requesting " + fileName, dumpFile.exists());
		doTestTriggerDumpX(type +":label=" + fileName, expectedType);
	}
		
	/**
	 * Triggers a dump with the given options and does basic checking
	 * for files created and created with the right name.
	 * @param options
	 */
	public void doTestTriggerDumpX(String options, DumpType expectedType) {
		String userDir = System.getProperty("user.dir");
		String dumpFilePattern = ".*\\.*";
		
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, dumpFilePattern);
		int beforeCount = beforeFileNames.length;
		
		String fileName = null;
		try {
			fileName = com.ibm.jvm.Dump.triggerDump(options);
		} catch (SecurityException e) {
			e.printStackTrace();
			fail("Exception thrown by triggerDump(" + options +")");
		} catch (InvalidDumpOptionException e) {
			e.printStackTrace();
			fail("Exception thrown by triggerDump(" + options +")");
		}
		//System.err.println("Dump file name is: " + fileName);
		assertNotNull("Expected triggerDump to return file name, not null", fileName);
		
		// Check the generated file really exists, unless we wrote to stderr or don't expect to find a file!
		File dumpFile = new File(fileName);
		if( !"-".equals(fileName) && expectedType != DumpType.FILE_NOT_FOUND ) {
			assertTrue("Failed to find files " + fileName + " after requesting " + fileName, dumpFile.exists());
			fileNames.add(fileName);
		}
		// Check content
		DumpType type = getContentType(dumpFile);
		assertEquals("Expected file " + dumpFile + " to contain a dump of type " + expectedType.name + " but content type was: " + type.name, expectedType, type);
		
		// Check the number of files has increased. (We weren't returned a filename that already existed.)
		String[] afterFileNames = getFilesByPattern(userDir, dumpFilePattern);
		int afterCount = afterFileNames.length;
		if( "-".equals(fileName) || expectedType == DumpType.FILE_NOT_FOUND ) {
			// Wrote to stderr, or no file expected so no more files.
			assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount, afterCount);
		} else {
			// Wrote to filename, one more file.
			assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount + 1, afterCount);
		}
		
	}

	public void doSTDOUTTestTriggerDumpX(String options) {
		String userDir = System.getProperty("user.dir");
		String dumpFilePattern = ".*\\.*";
		
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, dumpFilePattern);
		int beforeCount = beforeFileNames.length;
		
		String fileName = null;
		
		try {
			fileName = com.ibm.jvm.Dump.triggerDump(options);
			assertEquals("Expected fileName /STDOUT/, but fileName was " + fileName, "/STDOUT/", fileName);
		} catch (InvalidDumpOptionException e) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}
		
		// Check the number of files has not increased. (We weren't returned a filename that already existed.)
		String[] afterFileNames = getFilesByPattern(userDir, dumpFilePattern);
		int afterCount = afterFileNames.length;
		// Should have failed to create a dump, so no more files should be created.
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount, afterCount);
	}
	
	/**
	 * Triggers a dump with the given options, which are incorrect,
	 * and checks no files are created.
	 * @param options
	 * @return the exception that was thrown so the caller can verify it was the correct one.
	 */
	public InvalidDumpOptionException doTestTriggerBadDumpX(String options) {
		String userDir = System.getProperty("user.dir");
		String dumpFilePattern = ".*\\.*";
		
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, dumpFilePattern);
		int beforeCount = beforeFileNames.length;
		
		String fileName = null;
		InvalidDumpOptionException exception = null;
		
		/* We are checking these options cause an error, so the InvalidDumpOptionException path is the good case. */
		try {
			fileName = com.ibm.jvm.Dump.triggerDump(options);
			fail("Exception not thrown by triggerDump(" + options +"), fileName returned was: " + fileName);
		} catch (SecurityException e) {
			fail("Security exception thrown by triggerDump(" + options +")");
		} catch (InvalidDumpOptionException e) {
			exception = e;
		}
		assertNull("Expected triggerDump not to return null file name", fileName);
		
		// Check the number of files has increased. (We weren't returned a filename that already existed.)
		String[] afterFileNames = getFilesByPattern(userDir, dumpFilePattern);
		int afterCount = afterFileNames.length;
		// Should have failed to create a dump, so no more files should be created.
		assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount, afterCount);
		return exception;
	}
	
}
