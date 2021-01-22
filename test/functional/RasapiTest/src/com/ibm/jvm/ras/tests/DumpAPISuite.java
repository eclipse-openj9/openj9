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

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.UnsupportedEncodingException;

import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageFactory;

import junit.framework.Test;
import junit.framework.TestSuite;

public class DumpAPISuite {

	public static enum DumpType {
		SNAP_TYPE("snap"),
		CONSOLE_TYPE("console"),
		JAVA_TYPE("java"),
		STACK_TYPE("stack"),
		PHD_HEAP_TYPE("phd"),
		CLASSIC_HEAP_TYPE("classic"),
		SYSTEM_TYPE("system"),
		FILE_NOT_FOUND("<file not found>"),
		READ_ERROR("<read error>"),
		UNKNOWN("<unknown>");
		
		public final String name;
		
		private DumpType(String name) {
			this.name = name;
		}
		
	} 
	
	/* Common constants */
	public static final String DUMP_TOOL_TYPE_ERR = "Tool dumps cannot be triggered via com.ibm.jvm.Dump API";
	public static final String DUMP_ERROR_PARSING = "Error in dump options.";
	public static final String DUMP_UNKNOWN_TYPE = "Error parsing dump options. Unknown dump type specified.";
	public static final String DUMP_COULD_NOT_CREATE_FILE = DUMP_ERROR_PARSING;
	public static final String DUMP_INVALID_OPTION = "Invalid dump option specified.";
	
	/**
	 * This runs all the tests as a single suite.
	 * @return
	 */
	public static Test suite() {
		TestSuite suite = new TestSuite(DumpAPISuite.class.getName());
		//$JUnit-BEGIN$
		suite.addTestSuite(DumpAPIBasicTests.class);
		suite.addTestSuite(DumpAPITriggerTests.class);
		suite.addTestSuite(DumpAPIQuerySetReset.class);
		suite.addTestSuite(DumpAPITokensTests.class);
		suite.addTestSuite(DumpAPISetTestXdumpdynamic.class);
		//$JUnit-END$
		return suite;
	}

	// Deletes the named file, or prints an error message if it
	// can't find it. On z/OS will check that a file doesn't exist
	// in a dataset as well as on HFS. (Will check both for all files
	// in case it has been copied onto HFS and exists in both locations.)
	public static void deleteFile(String fileName, String testName) {
		File toDelete = new File(fileName);
		if( toDelete.exists() ) {
			if(!toDelete.delete()) {
				System.err.printf("Unable to delete file %s for test case %s will try to delete on exit.\n", fileName, testName);
				toDelete.deleteOnExit();
			} else {
				//System.err.printf("Deleted file %s for test case %s\n", fileName, this.getName());
			}
			
		} else {
			System.err.printf("Unable to find file %s to delete for test case %s\n", fileName, testName);
		}
	}
	
	/* Some common functions for the whole suite. */
	public static String[] getFilesByPattern(String dirName, final String pattern) {
		File dir = new File(dirName);
		
		FilenameFilter filter = new FilenameFilter() {
			public boolean accept(File dir, String name) {
				if( name.matches(pattern) ) {
					return true;
				}
				return false;
			}
		};
		String[] fileNames = dir.list(filter);
		return fileNames;
	}
	
	public static boolean isZOS() {
		return "z/OS".equals(System.getProperty("os.name") );
	}
	
	/**
	 * Return the dump file type for the given file parameter.
	 * @param dumpFile
	 * @return a constant string representing the dump type.
	 */
	public static DumpType getContentType(File dumpFile) {
		// Hopefully we can usually do this by reading the first
		// few bytes.
		FileInputStream fileIn = null;
		final byte[] portable_heap_dump;
		try {
			portable_heap_dump = "portable heap dump".getBytes("US-ASCII");
		} catch (UnsupportedEncodingException e1) {
			// US-ASCII really should be supported.
			return DumpType.UNKNOWN;
		}

		try {
			// Grab the first few bytes.
			fileIn = new FileInputStream(dumpFile);
			byte firstBytes[] = new byte[64];
			int length = fileIn.read(firstBytes);

			// Try various things we know will identify the different file types.
			String initialString = new String(firstBytes); 
			if( (new String(firstBytes, "US-ASCII")).startsWith("UTTH") ) {
				// Trace file header, trace file is binary UTTH will be in ASCII.
				return DumpType.SNAP_TYPE;
			} else if( initialString.startsWith("Thread=") ) {
				// This is a console or stack dump, it depends how many threads!
				StringBuffer contents = new StringBuffer((int)dumpFile.length());
				contents.append(initialString);
				byte[] buffer = new byte[1024];
				int read = fileIn.read(buffer);
				while( read > -1 ) {
					contents.append(new String(buffer,0, read));
					read = fileIn.read(buffer);
				}
				String[] threads = contents.toString().split("Thread=");

				// We get an initial emptry array so we will have two
				// elements for STACK and more for CONSOLE.
				// (It is safe to assume the VM is running more than one thread.)
				if( threads.length == 2) {
					return DumpType.STACK_TYPE;
				} else if (threads.length > 2) {
					return DumpType.CONSOLE_TYPE;	
				}
			} else if( initialString.startsWith("0SECTION       TITLE")) {
				return DumpType.JAVA_TYPE;
			} else if( initialString.startsWith("// Version: ")) {
				return DumpType.CLASSIC_HEAP_TYPE;
			}
			// Do the phd check by reading the first few bytes.
			boolean phd = false;
			for( int i = 2; i < length && i-2 < portable_heap_dump.length; i++ ) {
				if( firstBytes[i] != portable_heap_dump[i-2] ) {
					phd = false;
					break;
				} else {
					phd = true;
				}
			}
			if( phd ) {
				return DumpType.PHD_HEAP_TYPE;
			}

			// Check for core dump. This needs to be last as the core readers can also
			// open java dumps and heap dumps.
			Image image = null;
			try {
				Class factoryClass = Class.forName("com.ibm.dtfj.image.j9.ImageFactory");
				ImageFactory factory = (ImageFactory) factoryClass.newInstance();
				image = factory.getImage(dumpFile);
				if( image != null ) {
					image.close();
					return DumpType.SYSTEM_TYPE;
				}
			} catch (ClassNotFoundException e) {
			} catch (IllegalAccessException e) {
			} catch (InstantiationException e) {
			} finally {
				if( image != null ) {
					image.close();
				}
			}


		} catch (FileNotFoundException e) {
			// FILE_NOT_FOUND is expected for some tests.
			return DumpType.FILE_NOT_FOUND;
		} catch (IOException e) {
			return DumpType.READ_ERROR;
		} finally {
			if( fileIn != null ) {
				try {
					fileIn.close();
				} catch (IOException e) {
					// Not able to fix this!
				}
			}
		}

		return DumpType.UNKNOWN;
	}
	
}
