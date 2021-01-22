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

import java.io.File;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.HashSet;
import java.util.Set;

import junit.framework.TestCase;

import com.ibm.jvm.InvalidDumpOptionException;

/**
 * This testcase checks that the dump tokens are correctly formatted into filenames.
 * It only creates javacores rather than testing this for every dump type.
 * 
 * The tokens are:
 *   %Y     year    1900..????
 *   %y     century   00..99
 *   %m     month 	01..12
 *   %d     day   	01..31
 *   %H     hour  	00..23
 *   %M     minute	00..59
 *   %S     second	00..59
 *
 *   %pid   process id
 *   %uid   user name
 *   %seq   dump counter
 *   %tick  msec counter
 *   %home  java home
 *   %last  last dump
 *   %event dump event
 * 
 * The date and time tokens are tested together.
 * %home and %last are not tested as they contain paths
 * and are only really intended for use with tool dumps.
 * 
 * @author hhellyer
 *
 */
public class DumpAPITokensTests extends TestCase {
	
//	private static String originalUserDir = System.getProperty("user.dir"); 
	
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
	}

	@Override
	protected void tearDown() throws Exception {
		// TODO Auto-generated method stub
		super.tearDown();
		for( String fileName: fileNames) { 
			deleteFile(fileName, this.getName());
		}
	}
	

	public void testJavaDumpWithDateAndTimeTokens() {
		String userDir = System.getProperty("user.dir");	
		
		// %Y     year    1900..????
		// %y     century   00..99
		// %m     month 	01..12
		// %d     day   	01..31
		// %H     hour  	00..23
		// %M     minute	00..59
		// %S     second	00..59
		// These two patterns should correspond.
		String dateFormatDump =  "%Y%m%d%H%M%S";
		
		String dateFormatJava = "yyyyMMddHHmmss";
		SimpleDateFormat dateFormat = new SimpleDateFormat(dateFormatJava);

		
		String dumpName = userDir + File.separator + "javacore."+ getName() + "." + dateFormatDump + "." + uid + ".txt";
		String dumpFilePattern = ".*javacore\\."+ getName() + "\\.[0-9]+\\." + uid + "\\.txt";
		
		// Check the file doesn't exist, otherwise the dump code moves it.
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, dumpFilePattern);
		assertTrue("Found a file matching our output pattern before taking the dump.", beforeFileNames.length == 0);
		
		// Check we got the correct name.
		long beforeTime = System.currentTimeMillis();
		// The dump timestamp will round down to the nearest millsecond once
		// parsed. Dumping may take less than a second so we need to make sure
		// the timestamp will advance.
		// (We aren't worried about millisecond precision in file timestamps just
		// that it's actually filled out correctly.!)
		try {
			Thread.sleep(1000);
		} catch (InterruptedException e) {
			// Do nothing.
		}
		String javacoreName = null;
		try {
			javacoreName = com.ibm.jvm.Dump.javaDumpToFile(dumpName);
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}
		long afterTime = System.currentTimeMillis();
		assertNotNull("Expected javacore filename to be returned, not null", javacoreName);
		fileNames.add(javacoreName);
		
		/* Count the number of files before and after. Should increase by one. */
		String[] afterFileNames = getFilesByPattern(userDir, dumpFilePattern);
		assertTrue("Found a file matching our output pattern before taking the dump.", afterFileNames.length == 1);
		
		// Check it really exists.
		File javacoreFile = new File(javacoreName);
		assertTrue("Failed to find files " + javacoreName + " after requesting " + javacoreName, javacoreFile.exists());

		
		// Check the filename returned matches the file we found searching the directory.
		assertEquals("Expected created file to match returned filename", javacoreName,  userDir + File.separator + afterFileNames[0]);
		
		// Check the date formatted correctly and had the correct timestamp.
		String javacoreDate = javacoreFile.getName().split("\\.")[2];
		long javacoreTime = 0;
		try {
			// Parse it in the UTC timezone.
			//System.err.println("Javacore date: " + javacoreDate);
			javacoreTime = dateFormat.parse(javacoreDate).getTime();
		} catch (ParseException e) {
			fail("The timestamp in the filename was not in the expected format, timestamp was: " + javacoreDate + " format was: " + dateFormatJava);
		}
		//System.err.println("beforeTime = " + (new Date(beforeTime)) + " javacoreTime = " + (new Date(javacoreTime)) + " afterTime = " + (new Date(afterTime)));
		//System.err.println("beforeTime = " + beforeTime + " javacoreTime = " + javacoreTime + " afterTime = " + afterTime);
		assertTrue("The timestamp from the javacore was not between the timestamps taken before and after we took the dump.", javacoreTime >= beforeTime && afterTime >= javacoreTime );
		
	}

	public void testJavaDumpWithTickToken() {
		String userDir = System.getProperty("user.dir");	
		
		// This will use the j9time_hires_clock() value.
		// We can't assume it is in a particular unit or that it will be the same unit on different platforms.
		String timeFormatDump =  "%tick";
	
		String dumpName = userDir + File.separator + "javacore."+ getName() + "." + timeFormatDump + "." + uid + ".txt";
		String dumpFilePattern = ".*javacore\\."+ getName() + "\\.[0-9]+\\." + uid + "\\.txt";
		
		// Check the file doesn't exist, otherwise the dump code moves it.
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, dumpFilePattern);
		assertTrue("Found a file matching our output pattern before taking the dump.", beforeFileNames.length == 0);
		
		String javacoreName = null;
		try {
			javacoreName = com.ibm.jvm.Dump.javaDumpToFile(dumpName);
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}
		

		assertNotNull("Expected javacore filename to be returned, not null", javacoreName);
		fileNames.add(javacoreName);
		
		/* Count the number of files before and after. Should increase by one. */
		String[] afterFileNames = getFilesByPattern(userDir, dumpFilePattern);
		assertTrue("Found a file matching our output pattern before taking the dump.", afterFileNames.length == 1);
		
		// Check it really exists.
		File javacoreFile = new File(javacoreName);
		assertTrue("Failed to find files " + javacoreName + " after requesting " + javacoreName, javacoreFile.exists());

		
		// Check the filename returned matches the file we found searching the directory.
		assertEquals("Expected created file to match returned filename", javacoreName,  userDir + File.separator + afterFileNames[0]);
		
		// We've checked we got a file named correctly, and that the file really was created.
		// Cannot check the absolute value of the time stamp as its just the hires time counter.
	}
	
	public void testJavaDumpWithPidToken() {
		String userDir = System.getProperty("user.dir");	
		
		String dumpToken =  "%pid";
	
		String dumpName = userDir + File.separator + "javacore."+ getName() + "." + dumpToken + "." + uid + ".txt";
		String dumpFilePattern = ".*javacore\\."+ getName() + "\\.[0-9]+\\." + uid + "\\.txt";
		
		// Check the file doesn't exist, otherwise the dump code moves it.
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, dumpFilePattern);
		assertTrue("Found a file matching our output pattern before taking the dump.", beforeFileNames.length == 0);
		
		String javacoreName = null;
		try {
			javacoreName = com.ibm.jvm.Dump.javaDumpToFile(dumpName);
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}

		assertNotNull("Expected javacore filename to be returned, not null", javacoreName);
		fileNames.add(javacoreName);
		
		/* Count the number of files before and after. Should increase by one. */
		String[] afterFileNames = getFilesByPattern(userDir, dumpFilePattern);
		assertTrue("Found a file matching our output pattern before taking the dump.", afterFileNames.length == 1);
		
		// Check it really exists.
		File javacoreFile = new File(javacoreName);
		assertTrue("Failed to find files " + javacoreName + " after requesting " + javacoreName, javacoreFile.exists());

		
		// Check the filename returned matches the file we found searching the directory.
		assertEquals("Expected created file to match returned filename", javacoreName,  userDir + File.separator + afterFileNames[0]);
		
	}
	
	/* This test fails as there isn't a mapping to a friendly name for
	 * J9RAS_DUMP_ON_USER_REQUEST. We could fix that, until then this
	 * test will fail.
	 */
	public void testJavaDumpWithEventToken() {
		String userDir = System.getProperty("user.dir");	
		
		String dumpToken =  "%event";
	
		String dumpName = userDir + File.separator + "javacore."+ getName() + "." + dumpToken + "." + uid + ".txt";
		String dumpFilePattern = ".*javacore\\."+ getName() + "\\.api." + uid + "\\.txt";
		
		// Check the file doesn't exist, otherwise the dump code moves it.
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, dumpFilePattern);
		assertTrue("Found a file matching our output pattern before taking the dump.", beforeFileNames.length == 0);
		
		String javacoreName = null;
		try {
			javacoreName = com.ibm.jvm.Dump.javaDumpToFile(dumpName);
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}

		assertNotNull("Expected javacore filename to be returned, not null", javacoreName);
		fileNames.add(javacoreName);
		
		// Check it really exists.
		File javacoreFile = new File(javacoreName);
		assertTrue("Failed to find files " + javacoreName + " after requesting " + javacoreName, javacoreFile.exists());
		
		/* Count the number of files before and after. Should increase by one. */
		String[] afterFileNames = getFilesByPattern(userDir, dumpFilePattern);
		assertEquals("Found a file matching our output pattern before taking the dump.", 1, afterFileNames.length);
		

		
		// Check the filename returned matches the file we found searching the directory.
		assertEquals("Expected created file to match returned filename", javacoreName,  userDir + File.separator + afterFileNames[0]);
		
	}
	
	public void testJavaDumpWithUidToken() {
		String userDir = System.getProperty("user.dir");	
		
		// This will use the j9time_hires_clock() value.
		// We can't assume it is in a particular unit or that it will be the same unit on different platforms.
		String dumpToken =  "%uid";
		String javaUserid = System.getProperty("user.name");
	
		String dumpName = userDir + File.separator + "javacore."+ getName() + "." + dumpToken + "." + uid + ".txt";
		String dumpFilePattern = ".*javacore\\."+ getName() + "\\." + javaUserid + "\\." + uid + "\\.txt";
		
		// Check the file doesn't exist, otherwise the dump code moves it.
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, dumpFilePattern);
		assertTrue("Found a file matching our output pattern before taking the dump.", beforeFileNames.length == 0);
		
		String javacoreName = null;
		try {
			javacoreName = com.ibm.jvm.Dump.javaDumpToFile(dumpName);
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}

		assertNotNull("Expected javacore filename to be returned, not null", javacoreName);
		fileNames.add(javacoreName);
		
		/* Count the number of files before and after. Should increase by one. */
		String[] afterFileNames = getFilesByPattern(userDir, dumpFilePattern);
		assertTrue("Found a file matching our output pattern before taking the dump.", afterFileNames.length == 1);
		
		// Check it really exists.
		File javacoreFile = new File(javacoreName);
		assertTrue("Failed to find files " + javacoreName + " after requesting " + javacoreName, javacoreFile.exists());

		
		// Check the filename returned matches the file we found searching the directory.
		assertEquals("Expected created file to match returned filename", javacoreName,  userDir + File.separator + afterFileNames[0]);
		
	}
	
	public void testJavaDumpWithSequenceToken() {
		String userDir = System.getProperty("user.dir");	
		
		String dumpToken =  "%seq";
			
		String dumpName = userDir + File.separator + "javacore."+ getName() + "." + dumpToken + "." + uid + ".txt";
		String dumpFilePattern = ".*javacore\\."+ getName() + "\\.[0-9]+\\." + uid + "\\.txt";
		
		// Check the file doesn't exist, otherwise the dump code moves it.
		/* Count the number of files before and after. Should increase by one. */
		String[] beforeFileNames = getFilesByPattern(userDir, dumpFilePattern);
		assertTrue("Found a file matching our output pattern before taking the dump.", beforeFileNames.length == 0);
		
		String javacoreName1 = null;
		try {
			javacoreName1 = com.ibm.jvm.Dump.javaDumpToFile(dumpName);
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}
		assertNotNull("Expected javacore filename to be returned, not null", javacoreName1);
		fileNames.add(javacoreName1);
		
		/* Count the number of files before and after. Should increase by one. */
		String[] afterFileNames = getFilesByPattern(userDir, dumpFilePattern);
		assertTrue("Found a file matching our output pattern before taking the dump.", afterFileNames.length == 1);
		
		// Check it really exists.
		File javacoreFile1 = new File(javacoreName1);
		assertTrue("Failed to find files " + javacoreName1 + " after requesting " + javacoreName1, javacoreFile1.exists());

		
		// Check the filename returned matches the file we found searching the directory.
		assertEquals("Expected created file to match returned filename", javacoreName1,  userDir + File.separator + afterFileNames[0]);
		
		// Check the date formatted correctly and had the correct timestamp.
		String seq1Str = javacoreFile1.getName().split("\\.")[2];

		// Create a second javacore, to see if %seq has incremented.
		String javacoreName2 = null;
		try {
			javacoreName2 = com.ibm.jvm.Dump.javaDumpToFile(dumpName);
		} catch (InvalidDumpOptionException e ) {
			e.printStackTrace();
			fail("Unexpected InvalidDumpOption exception thrown.");
		}
		assertNotNull("Expected javacore filename to be returned, not null", javacoreName2);
		fileNames.add(javacoreName2);

		// Check it really exists.
		File javacoreFile2 = new File(javacoreName2);
		assertTrue("Failed to find files " + javacoreName2 + " after requesting " + javacoreName1, javacoreFile2.exists());

		String seq2Str = javacoreFile2.getName().split("\\.")[2];
		
		int seq1 = Integer.valueOf(seq1Str);
		int seq2 = Integer.valueOf(seq2Str);
		
		assertTrue("Expected sequence number to increment by one, but seq1 = " + seq1 + " seq2 = " + seq2, seq1 + 1 == seq2);
		
	}
}
