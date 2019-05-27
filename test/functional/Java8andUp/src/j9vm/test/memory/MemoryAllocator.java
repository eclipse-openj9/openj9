/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
package j9vm.test.memory;

import org.testng.annotations.AfterMethod;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeMethod;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

//Test annotation need to be on class level for this test.
//Test package need to be modified with native code: j9vm_test_memory_MemoryAllocator_allocateMemory in jintest.c

@Test(groups = { "level.extended" })
public class MemoryAllocator {

	private static final Logger logger = Logger.getLogger(MemoryAllocator.class);

	/*
	 * any string, followed by "+--", followed by a the category name terminated by a colon,
	 * followed by the value
	 */
	private static final String CATEGORY_REGEX = "^.*\\+--?([^:]+): (.*)";

	/* library of JNI natives for testing */
	String TEST_LIBRARY = "j9ben";
	boolean libraryLoaded = false;
	private File workingDirectory = new File(System.getProperty("user.dir"));
	private HashMap<String, String> oldCategoryValues; /* used to compare old vs new values */

	@BeforeMethod
	protected void setUp() {
		if (!libraryLoaded) {
			try {
				System.loadLibrary(TEST_LIBRARY);
			} catch (UnsatisfiedLinkError e) {
				Assert.fail(e.getMessage() + "\nlibrary path = " + System.getProperty("java.library.path"));
			}
			logger.debug(TEST_LIBRARY + " loaded successfully");
			libraryLoaded = true;
		}
	}

	@AfterMethod
	protected void tearDown(){
		deleteLocalJavacores();
	}

	/**
	 * Sanity test for allocation.
	 */

	public void testAllocateMemory() {
		for ( Long code: categoryCodesToNames.keySet() ) {
			String categoryName = categoryCodesToNames.get(code);
			logger.debug("Allocate memory in category 0x"+Long.toHexString(code)+": "+categoryName);
			boolean result = allocateMemory(1000, code.intValue());
			AssertJUnit.assertTrue("allocation failed", result);
		}
		com.ibm.jvm.Dump.JavaDump();
		BufferedReader javacore = locateAndOpenJavacoreFile(workingDirectory);
		dumpJavacoreMemory(javacore);
		try {
			javacore.close();
		} catch (IOException e) {
			Assert.fail("exception closing javacore:"+e);
		}
	}

	protected void dumpJavacoreMemory(BufferedReader javacore) {
		String l;
		/* dump out the memory section for human inspection */
		try {
			while ((null != (l = javacore.readLine())) && !l.startsWith("0MEMUSER"));
			while (!l.startsWith("0SECTION")) {
				logger.debug(l);
				l = javacore.readLine();
			}
		} catch (IOException e) {
			e.printStackTrace();
			Assert.fail("Error reading javacore");
		}
	}

	/**
	 * Allocate memory in each category and verify that the category changed in the javacore.
	 * Note that this isn't perfect: the javacore category values may change as a side effect of other operations.
	 */
	public void testAllocateMemoryInCategory() {
		com.ibm.jvm.Dump.JavaDump(); /* grab a reference version */
		oldCategoryValues = parseJavaCore(workingDirectory);
		for ( Long code: categoryCodesToNames.keySet() ) {
			String categoryName = categoryCodesToNames.get(code);
			deleteLocalJavacores(); /* ensure there are no javacores to confuse us */
			logger.debug("Allocate memory in category 0x" + Long.toHexString(code) + ": " + categoryName);
			boolean result = allocateMemory(1000, code.intValue());
			AssertJUnit.assertTrue("allocation failed", result);
			com.ibm.jvm.Dump.JavaDump();
			HashMap<String, String> newCategoryValues = parseJavaCore(workingDirectory);

			verifyCategoryValueChanged(newCategoryValues, code);
			oldCategoryValues = newCategoryValues; /* make the new javacore the reference edition */
		}
	}

	/**
	 * Allocate memory in each category and verify that the category changed in the javacore.
	 * Note that this isn't perfect: the javacore category values may change as a side effect of other operations.
	 */
	public void testAllocateMemoryInAnUnknownCategory() {
		com.ibm.jvm.Dump.JavaDump(); /* grab a reference version */
		oldCategoryValues = parseJavaCore(workingDirectory);
		long maxCode = 0;
		for ( Long code: categoryCodesToNames.keySet() ) {
			maxCode = Math.max(maxCode, code);
		}

		// Increment maxcode, should be an unknown value.
		Long code = maxCode+1;
		deleteLocalJavacores(); /* ensure there are no javacores to confuse us */
		logger.debug("Allocate memory in unknown category " + Long.toHexString(code) );
		boolean result = allocateMemory(1000, code.intValue());
		AssertJUnit.assertTrue("allocation failed", result);
		com.ibm.jvm.Dump.JavaDump();
		HashMap<String, String> newCategoryValues = parseJavaCore(workingDirectory);

		String testCategoryName = "Unknown";

		// Verify the value changed.
		String oldValue = oldCategoryValues.get(testCategoryName);
		String newValue = newCategoryValues.get(testCategoryName);

		logger.debug("Testing category 0x" + Long.toHexString(code) + ", " + testCategoryName + " old value='" + oldValue + "' new value='" + newValue + "'");
		AssertJUnit.assertFalse("value for " + testCategoryName + " did not change from " + oldValue, newValue.equals(oldValue));
	}

	private void verifyCategoryValueChanged(
			HashMap<String, String> newCategoryValues, Long categoryNumber) {
		String testCategoryName = categoryCodesToNames.get(categoryNumber);
		String oldValue = oldCategoryValues.get(testCategoryName);
		String newValue = newCategoryValues.get(testCategoryName);
		logger.debug("Testing category 0x" + Long.toHexString(categoryNumber) + ", "
				+ testCategoryName + " old value='" + oldValue
				+ "' new value='" + newValue + "'");
		AssertJUnit.assertFalse("value for " + testCategoryName + " did not change from "
				+ oldValue, newValue.equals(oldValue));
	}

	protected void deleteLocalJavacores() {
		File[] localFiles = workingDirectory.listFiles();
		logger.debug("deleting javacores in "+workingDirectory.getAbsolutePath());
		for (File f: localFiles) {
			if (isJavacore(f)) {
				if (!f.delete()) {
					Assert.fail("could not delete "+f.getAbsolutePath());
				}
			}
		}
	}

	/**
	 * Parse the memory allocation results in a javacore.
	 * @param dir directory in which to search for the javacore
	 * @return hash of category names and the corresponding values.
	 */
	private HashMap<String, String> parseJavaCore(File dir) {
		BufferedReader javacore = locateAndOpenJavacoreFile(dir);

		String l;
		HashMap<String, String> categoryValues = new HashMap<String, String>();
		try {
			while ((null != (l = javacore.readLine())) && !l.startsWith("0MEMUSER")); /* skip to the memory section */
			Pattern categoryMatcher = Pattern.compile(CATEGORY_REGEX);
			while (!l.startsWith("0SECTION")) { /* stop at the start of the next section */
				l = javacore.readLine();
				if (!l.contains("+--")) {
					continue; /* empty line */
				}
				if (l.contains("+--Other")) {
					continue; /* this change reflected in the parent category */
				}
				Matcher m = categoryMatcher.matcher(l);
				AssertJUnit.assertTrue(("\""+l+"\""+" + did not match regular expression "+CATEGORY_REGEX), m.matches()) ;
				String categoryName = m.group(1);
				AssertJUnit.assertNotNull("Could not find category name in "+"\""+l+"\"", categoryName);
				String categoryValue = m.group(2);
				AssertJUnit.assertNotNull("Could not find category value in "+"\""+l+"\"", categoryValue);
//				logger.debug("Storing: %s with value %s\n",categoryName, categoryValue);
				categoryValues.put(categoryName, categoryValue);
			}
			javacore.close();
		} catch (IOException e) {
			Assert.fail("Error "+e+" reading javacore");
		}
		return categoryValues;
	}

	protected BufferedReader locateAndOpenJavacoreFile(File dir) {
		File[] localFiles = dir.listFiles();
		BufferedReader javacore = null;
		for (File f: localFiles) {
			if (isJavacore(f)) {
				logger.debug("Opening "+f.getPath());
				try {
					javacore = new BufferedReader(new FileReader(f));
					break;
				} catch (FileNotFoundException e) {
					Assert.fail("file not found: "+f.getAbsolutePath());
				}
			}
		}
		if (null == javacore) {
			Assert.fail("No javacore generated");
		}
		return javacore;
	}

	protected boolean isJavacore(File f) {
		return f.getName().startsWith("javacore");
	}

	static native boolean allocateMemory(long size, int category);



// From j9memcategories.h:
//#define J9MEM_CATEGORY_JRE 0
//#define J9MEM_CATEGORY_UNUSED1 1 /* #define J9MEM_CATEGORY_VM 1 */
//#define J9MEM_CATEGORY_CLASSES 2
//#define J9MEM_CATEGORY_CLASSES_SHC_CACHE 3
//#define J9MEM_CATEGORY_UNUSED4 4 /* #define J9MEM_CATEGORY_MM 4 */
//#define J9MEM_CATEGORY_UNUSED5 5 /* #define J9MEM_CATEGORY_MM_RUNTIME_HEAP 7 */
//#define J9MEM_CATEGORY_UNUSED6 6 /* #define J9MEM_CATEGORY_THREADS 6 */
//#define J9MEM_CATEGORY_UNUSED7 7 /* #define J9MEM_CATEGORY_THREADS_RUNTIME_STACK 7 */
//#define J9MEM_CATEGORY_UNUSED8 8 /* #define J9MEM_CATEGORY_THREADS_NATIVE_STACK 8 */
//#define J9MEM_CATEGORY_UNUSED9 9 /* #define J9MEM_CATEGORY_TRACE 9 */
//#define J9MEM_CATEGORY_JIT 10
//#define J9MEM_CATEGORY_JIT_CODE_CACHE 11
//#define J9MEM_CATEGORY_JIT_DATA_CACHE 12
//#define J9MEM_CATEGORY_HARMONY 13
//#define J9MEM_CATEGORY_SUN_MISC_UNSAFE_ALLOCATE 14
//#define J9MEM_CATEGORY_VM_JCL 15
//#define J9MEM_CATEGORY_CLASS_LIBRARIES 16
//#define J9MEM_CATEGORY_JVMTI 17
//#define J9MEM_CATEGORY_JVMTI_ALLOCATE 18
//#define J9MEM_CATEGORY_JNI 19
//#define J9MEM_CATEGORY_SUN_JCL 20
//#define J9MEM_CATEGORY_CLASSLIB_IO_MATH_LANG 21
//#define J9MEM_CATEGORY_CLASSLIB_ZIP 22
//#define J9MEM_CATEGORY_CLASSLIB_WRAPPERS 23
//#define J9MEM_CATEGORY_CLASSLIB_WRAPPERS_MALLOC 24
//#define J9MEM_CATEGORY_CLASSLIB_WRAPPERS_EBCDIC 25
//#define J9MEM_CATEGORY_CLASSLIB_NETWORKING 26
//#define J9MEM_CATEGORY_CLASSLIB_NETWORKING_NET 27
//#define J9MEM_CATEGORY_CLASSLIB_NETWORKING_NIO 28
//#define J9MEM_CATEGORY_CLASSLIB_NETWORKING_RMI 29
//#define J9MEM_CATEGORY_CLASSLIB_GUI 30
//#define J9MEM_CATEGORY_CLASSLIB_GUI_AWT 31
//#define J9MEM_CATEGORY_CLASSLIB_GUI_MAWT 32
//#define J9MEM_CATEGORY_CLASSLIB_GUI_JAWT 33
//#define J9MEM_CATEGORY_CLASSLIB_GUI_MEDIALIB 34
//#define J9MEM_CATEGORY_CLASSLIB_FONT 35
//#define J9MEM_CATEGORY_CLASSLIB_SOUND 36
//#define J9MEM_CATEGORY_SUN_MISC_UNSAFE_ALLOCATEDBB 38


// From omrmemcategories.h:
/* Special memory category for memory allocated for unknown categories */
//	#define J9MEM_CATEGORY_UNKNOWN 0x80000000
//	/* Special memory category for memory allocated for the port library itself */
//	#define J9MEM_CATEGORY_PORT_LIBRARY 0x80000001
//	#define J9MEM_CATEGORY_VM 0x80000002
//	#define J9MEM_CATEGORY_MM 0x80000003
//	#define J9MEM_CATEGORY_THREADS 0x80000004
//	#define J9MEM_CATEGORY_THREADS_RUNTIME_STACK 0x80000005
//	#define J9MEM_CATEGORY_THREADS_NATIVE_STACK 0x80000006
//	#define J9MEM_CATEGORY_TRACE 0x80000007
//	#define J9MEM_CATEGORY_OMRTI 0x80000008
//
//	/* Special memory category for *unused* sections of regions allocated for <32bit allocations on 64 bit.
//	 * The used sections will be accounted for under the categories they are used by. */
//	#define J9MEM_CATEGORY_PORT_LIBRARY_UNUSED_ALLOCATE32_REGIONS 0x80000009
//	#define J9MEM_CATEGORY_MM_RUNTIME_HEAP 0x8000000A

	/**
	 * Category names and codes.
	 * These must be in the same spelling as in j9memcategories.c and have the same ids as in j9memcategories.h
	 * or omrmemcategories.h
	 */

	private static Map<Long, String> categoryCodesToNames = new HashMap<Long, String>();

	static {
		// Java Codes (CATEGORY_REGEX does not match the first line, JRE, successfully).
//		categoryCodesToNames.put(0l, "JRE"); // #define J9MEM_CATEGORY_JRE 0
//		categoryCodesToNames.put(1l, "VM"); //#define J9MEM_CATEGORY_UNUSED1 1 /* #define J9MEM_CATEGORY_VM 1 */
		categoryCodesToNames.put(2l, "Classes"); //#define J9MEM_CATEGORY_CLASSES 2
		categoryCodesToNames.put(3l, "Shared Class Cache"); //#define J9MEM_CATEGORY_CLASSES_SHC_CACHE 3
//		categoryCodesToNames.put(4l, "Memory Manager (GC)"); //#define J9MEM_CATEGORY_UNUSED4 4 /* #define J9MEM_CATEGORY_MM 4 */
//		categoryCodesToNames.put(5l, "Java Heap"); //#define J9MEM_CATEGORY_UNUSED5 5 /* #define J9MEM_CATEGORY_MM_RUNTIME_HEAP 7 */
//		categoryCodesToNames.put(6l, "Threads"); //#define J9MEM_CATEGORY_UNUSED6 6 /* #define J9MEM_CATEGORY_THREADS 6 */
//		categoryCodesToNames.put(7l, "Java Stack"); //#define J9MEM_CATEGORY_UNUSED7 7 /* #define J9MEM_CATEGORY_THREADS_RUNTIME_STACK 7 */
//		categoryCodesToNames.put(8l, "Native Stack"); //#define J9MEM_CATEGORY_UNUSED8 8 /* #define J9MEM_CATEGORY_THREADS_NATIVE_STACK 8 */
//		categoryCodesToNames.put(9l, "Trace"); //#define J9MEM_CATEGORY_UNUSED9 9 /* #define J9MEM_CATEGORY_TRACE 9 */
		// categoryCodesToNames.put(10l, "JIT"); //#define J9MEM_CATEGORY_JIT 10 - Moved to omr
		// categoryCodesToNames.put(11l, "JIT Code Cache"); //#define J9MEM_CATEGORY_JIT_CODE_CACHE 11 - Moved to omr
		// categoryCodesToNames.put(12l, "JIT Data Cache"); //#define J9MEM_CATEGORY_JIT_DATA_CACHE 12 - Moved to omr
		categoryCodesToNames.put(13l, "Harmony Class Libraries"); //#define J9MEM_CATEGORY_HARMONY 13
		categoryCodesToNames.put(14l, "sun.misc.Unsafe"); //#define J9MEM_CATEGORY_SUN_MISC_UNSAFE_ALLOCATE 14
		categoryCodesToNames.put(15l, "VM Class Libraries"); //#define J9MEM_CATEGORY_VM_JCL 15
		categoryCodesToNames.put(16l, "Class Libraries"); //#define J9MEM_CATEGORY_CLASS_LIBRARIES 16
		categoryCodesToNames.put(17l, "JVMTI"); //#define J9MEM_CATEGORY_JVMTI 17
		categoryCodesToNames.put(18l, "JVMTI Allocate()"); //#define J9MEM_CATEGORY_JVMTI_ALLOCATE 18
		categoryCodesToNames.put(19l, "JNI"); //#define J9MEM_CATEGORY_JNI 19
		categoryCodesToNames.put(20l, "Standard Class Libraries"); //#define J9MEM_CATEGORY_SUN_JCL 20
		categoryCodesToNames.put(21l, "IO/Math/Language"); //#define J9MEM_CATEGORY_CLASSLIB_IO_MATH_LANG 21
		categoryCodesToNames.put(22l, "Zip"); //#define J9MEM_CATEGORY_CLASSLIB_ZIP 22
		categoryCodesToNames.put(23l, "Wrappers"); //#define J9MEM_CATEGORY_CLASSLIB_WRAPPERS 23
		categoryCodesToNames.put(24l, "Malloc"); //#define J9MEM_CATEGORY_CLASSLIB_WRAPPERS_MALLOC 24
		categoryCodesToNames.put(25l, "z/OS EBCDIC Conversion"); //#define J9MEM_CATEGORY_CLASSLIB_WRAPPERS_EBCDIC 25
		categoryCodesToNames.put(26l, "Networking"); //#define J9MEM_CATEGORY_CLASSLIB_NETWORKING 26
		categoryCodesToNames.put(27l, "NET"); //#define J9MEM_CATEGORY_CLASSLIB_NETWORKING_NET 27
		categoryCodesToNames.put(28l, "NIO and NIO.2"); //#define J9MEM_CATEGORY_CLASSLIB_NETWORKING_NIO 28
		categoryCodesToNames.put(29l, "RMI"); //#define J9MEM_CATEGORY_CLASSLIB_NETWORKING_RMI 29
		categoryCodesToNames.put(30l, "GUI"); //#define J9MEM_CATEGORY_CLASSLIB_GUI 30
		categoryCodesToNames.put(31l, "AWT"); //#define J9MEM_CATEGORY_CLASSLIB_GUI_AWT 31
		categoryCodesToNames.put(32l, "MAWT"); //#define J9MEM_CATEGORY_CLASSLIB_GUI_MAWT 32
		categoryCodesToNames.put(33l, "JAWT"); //#define J9MEM_CATEGORY_CLASSLIB_GUI_JAWT 33
		categoryCodesToNames.put(34l, "Medialib Image"); //#define J9MEM_CATEGORY_CLASSLIB_GUI_MEDIALIB 34
		categoryCodesToNames.put(35l, "Font"); //#define J9MEM_CATEGORY_CLASSLIB_FONT 35
		categoryCodesToNames.put(36l, "Sound"); //#define J9MEM_CATEGORY_CLASSLIB_SOUND 36
		categoryCodesToNames.put(38l, "Direct Byte Buffers"); //#define J9MEM_CATEGORY_SUN_MISC_UNSAFE_ALLOCATEDBB 38

		// OMR Codes
		categoryCodesToNames.put(0x80000000l, "Unknown"); //	#define J9MEM_CATEGORY_UNKNOWN 0x80000000
		categoryCodesToNames.put(0x80000001l, "Port Library"); //	#define J9MEM_CATEGORY_PORT_LIBRARY 0x80000001
		categoryCodesToNames.put(0x80000002l, "VM"); //	#define J9MEM_CATEGORY_VM 0x80000002
		categoryCodesToNames.put(0x80000003l, "Memory Manager (GC)"); //	#define J9MEM_CATEGORY_MM 0x80000003
		categoryCodesToNames.put(0x80000004l, "Threads"); //	#define J9MEM_CATEGORY_THREADS 0x80000004
		categoryCodesToNames.put(0x80000005l, "Java Stack"); //	#define J9MEM_CATEGORY_THREADS_RUNTIME_STACK 0x80000005
		categoryCodesToNames.put(0x80000006l, "Native Stack"); //	#define J9MEM_CATEGORY_THREADS_NATIVE_STACK 0x80000006
		categoryCodesToNames.put(0x80000007l, "Trace"); //	#define J9MEM_CATEGORY_TRACE 0x80000007
		// categoryNamesToCodes.put(0x80000008l); //	#define J9MEM_CATEGORY_OMRTI 0x80000008 - Will not be defined inside Java!
		// categoryCodesToNames.put(0x80000009l, "Unused <32bit allocation regions"); //	#define J9MEM_CATEGORY_PORT_LIBRARY_UNUSED_ALLOCATE32_REGIONS 0x80000009 - Will not be defined for 32 bit JVMs
		categoryCodesToNames.put(0x8000000Al, "Java Heap" ); //	#define J9MEM_CATEGORY_MM_RUNTIME_HEAP 0x8000000A
		categoryCodesToNames.put(0x8000000Bl, "JIT"); //#define J9MEM_CATEGORY_JIT 0x8000000B
		categoryCodesToNames.put(0x8000000Cl, "JIT Code Cache"); //#define J9MEM_CATEGORY_JIT_CODE_CACHE 0x8000000C
		categoryCodesToNames.put(0x8000000Dl, "JIT Data Cache"); //#define J9MEM_CATEGORY_JIT_DATA_CACHE 0x8000000D
	}

}
