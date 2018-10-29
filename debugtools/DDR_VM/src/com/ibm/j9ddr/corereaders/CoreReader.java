/*******************************************************************************
 * Copyright (c) 2009, 2019 IBM Corp. and others
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

import static java.util.logging.Level.WARNING;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.logging.Logger;

import javax.imageio.stream.FileImageInputStream;
import javax.imageio.stream.ImageInputStream;

import com.ibm.j9ddr.corereaders.ICoreFileReader.DumpTestResult;
import com.ibm.j9ddr.corereaders.aix.AIXDumpReaderFactory;
import com.ibm.j9ddr.corereaders.debugger.JniReader;
import com.ibm.j9ddr.corereaders.elf.ELFDumpReaderFactory;
import com.ibm.j9ddr.corereaders.macho.MachoDumpReaderFactory;
import com.ibm.j9ddr.corereaders.minidump.MiniDumpReader;

/**
 * Factory for ICoreReader implementations.
 * 
 * @author andhall
 * 
 */
public class CoreReader
{

	private static final Logger logger = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);

	private static final List<Class<? extends ICoreFileReader>> coreReaders;

	static {
		List<Class<? extends ICoreFileReader>> localReaders = new ArrayList<Class<? extends ICoreFileReader>>();

		// AIX must be the last one, since its validation condition is very
		// weak.
		localReaders.add(JniReader.class);
		localReaders.add(MiniDumpReader.class);
		localReaders.add(ELFDumpReaderFactory.class);
		localReaders.add(MachoDumpReaderFactory.class);

		// Use reflection to find TDumpReader: it is not available on all platforms.
		try {
			Class<?> tdumpReaderClass = Class.forName("com.ibm.j9ddr.corereaders.tdump.TDumpReader");

			if (ICoreFileReader.class.isAssignableFrom(tdumpReaderClass)) {
				// the compiler doesn't recognize the significance of the test above
				@SuppressWarnings("unchecked")
				Class<? extends ICoreFileReader> cast = (Class<? extends ICoreFileReader>) tdumpReaderClass;

				localReaders.add(cast);
			}
		} catch (ClassNotFoundException e) {
			// proceed without TDumpReader
		}

		localReaders.add(AIXDumpReaderFactory.class);

		coreReaders = Collections.unmodifiableList(localReaders);
	};

	/**
	 * Create a ICore object for a core file.
	 * 
	 * @param file
	 *            File object referencing core file.
	 * @return ICoreReader object or NULL if no core reader will accept the
	 *         supplied file
	 * @throws IOException
	 *             If there's an I/O problem reading the core file
	 */
	public static ICore readCoreFile(String path)
			throws IOException
	{
		DumpTestResult accruedResult = null;
		IOException thrown = null;

		for (Class<? extends ICoreFileReader> clazz : coreReaders) {
			try {
				ICoreFileReader reader = clazz.newInstance();

				DumpTestResult result = reader.testDump(path);

				if (result == DumpTestResult.RECOGNISED_FORMAT) {
					return reader.processDump(path);
				} else {
					accruedResult = result.accrue(accruedResult);
				}
			} catch (UnsatisfiedLinkError e) {
				//ignore
			} catch (IllegalAccessException e) {
				logger.log(WARNING, "IllegalAccessException thrown creating " + clazz.getName(), e);
			} catch (InstantiationException e) {
				logger.log(WARNING, "Exception thrown creating " + clazz.getName(), e.getCause());
			} catch (InvalidDumpFormatException e) {
				logger.log(WARNING, "InvalidDumpFormatException thrown creating " + clazz.getName(), e);
			} catch (IOException e) {
				if (thrown == null) {
					thrown = e;
				}
			}
		}

		if (accruedResult == null) {
			if (thrown != null) {
				throw new IOException("I/O problems reading core file: " + thrown.getMessage());
			} else {
				throw new Error("No core file readers found");
			}
		}

		switch (accruedResult) {
		case FILE_NOT_FOUND:
			throw new FileNotFoundException("Could not find: " + new File(path).getAbsolutePath());
		case UNRECOGNISED_FORMAT:
			throw new IOException("Dump: " + path + " not recognised by any core reader");
		default:
			throw new IllegalStateException("Unexpected state: " + accruedResult);
		}
	}

	public static ICore readCoreFile(ImageInputStream in) throws IOException
	{
		DumpTestResult accruedResult = null;
		IOException thrown = null;
		
		for (Class<? extends ICoreFileReader> clazz : coreReaders) {
			try {
				ICoreFileReader reader = clazz.newInstance();
		
				DumpTestResult result = reader.testDump(in);
		
				if (result == DumpTestResult.RECOGNISED_FORMAT) {
					return reader.processDump(in);
				} else {
					accruedResult = result.accrue(accruedResult);
				}
			} catch (UnsatisfiedLinkError e) {
				//ignore
			} catch (IllegalAccessException e) {
				logger.log(WARNING, "IllegalAccessException thrown creating " + clazz.getName(), e);
			} catch (InstantiationException e) {
				logger.log(WARNING, "Exception thrown creating " + clazz.getName(), e.getCause());
			} catch (InvalidDumpFormatException e) {
				logger.log(WARNING, "InvalidDumpFormatException thrown creating " + clazz.getName(), e);
			} catch (IOException e) {
				if (thrown == null) {
					thrown = e;
				}
			}
		}
		
		if (accruedResult == null) {
			if (thrown != null) {
				throw new IOException("I/O problems reading core file: " + thrown.getMessage());
			} else {
				throw new Error("No core file readers found");
			}
		}
		
		switch (accruedResult) {
			case UNRECOGNISED_FORMAT:
				throw new IOException("Dump input stream : not recognised by any core reader");
			default:
				throw new IllegalStateException("Unexpected state: " + accruedResult);
		}
	}
	
	public static byte[] getFileHeader(String path) throws IOException {
		ImageInputStream iis = new FileImageInputStream(new File(path));
		return getFileHeader(iis);
	}
	
	public static byte[] getFileHeader(ImageInputStream iis) throws IOException {
		byte[] data = new byte[2048];

		try {
			iis.seek(0);		//position at start of the stream
			iis.readFully(data);
		} catch (IOException ex) {
			throw new IOException(ex.getMessage());
		} finally {
			iis.seek(0);		//reset the input stream ready for the next reader
		}
		return data;
	}
}
