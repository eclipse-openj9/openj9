/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders;

import java.io.IOException;

import javax.imageio.stream.ImageInputStream;

/**
 * Interface for entities that can parse core dumps.
 * @author andhall
 *
 */
public interface ICoreFileReader
{
	/**
	 * This is the name of the java.util.logging.Logger subsystem to which the core readers pass verbose messages. 
	 */
	public static final String J9DDR_CORE_READERS_LOGGER_NAME = "j9ddr.core_readers";
	
	public static enum DumpTestResult { 
		
		FILE_NOT_FOUND(0), 
		
		UNRECOGNISED_FORMAT(1), 
		
		RECOGNISED_FORMAT(2);
		
		private final int level;
		
		DumpTestResult(int level)
		{
			this.level = level;
		}
		
		public final DumpTestResult accrue(DumpTestResult oldResult)
		{
			if (oldResult == null) {
				return this;
			}
			
			if (oldResult.level > this.level) {
				return oldResult;
			} else {
				return this;
			}
		}
	}
	
	/**
	 * Tests the dump described by the path for compatibility with this CoreFileReader.
	 * 
	 * @param path Path to dump file.
	 * @throws IOException If there was an IO problem reading the file (disk full etc.). Note that FileNotFound should be handled with DumpTestResult.
	 */
	public DumpTestResult testDump(String path) throws IOException;

	/**
	 * Tests the dump represented by the input stream for compatibility with this CoreFileReader.
	 * 
	 * @param path Path to dump file.
	 * @throws IOException If there was an IO problem reading the file (disk full etc.). Note that FileNotFound should be handled with DumpTestResult.
	 */
	public DumpTestResult testDump(ImageInputStream in) throws IOException;
	
	/**
	 * Called when a core reader should process the dump. This allows lazy
	 * initialisation of the readers especially when they are not subsequently
	 * used.
	 */
	public ICore processDump(String path) throws InvalidDumpFormatException, IOException;
	
	/**
	 * Called when a core reader should process the dump. This allows lazy
	 * initialisation of the readers especially when they are not subsequently
	 * used.
	 */
	public ICore processDump(ImageInputStream in) throws InvalidDumpFormatException, IOException;
}
