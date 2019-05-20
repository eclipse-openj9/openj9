/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.libraries;

import static java.util.logging.Level.FINE;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.ObjectOutputStream;
import java.io.RandomAccessFile;
import java.util.ArrayList;
import java.util.logging.ConsoleHandler;
import java.util.logging.Formatter;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.logging.SimpleFormatter;

//used by DC to collect the native libraries for a core file on linux and AIX

public class LibraryCollector {
	/**
	 * Used to indicated the overall outcome when invoking the library collection process
	 * @author Adam Pilkington
	 *
	 */
	public static enum CollectorResult {
		COMPLETE_NOT_REQUIRED,
		COMPLETE_ALREADY_PRESENT,
		COMPLETE_ERRORS,
		COMPLETE
	}
	
	/**
	 * Specifies the type of collection to use. DTFJ is specified for those builds which do not include the ddr.jar file.
	 * @author Adam Pilkington
	 *
	 */
	public static enum CollectionType {
		DTFJ,
		DDR
	}
	
	static final String LOGGER_NAME = "com.ibm.j9ddr.libraries";
	public static final String PUBLIC_LOGGER_NAME = "com.ibm.j9ddr.libraries.public";
	private static final Logger logger = Logger.getLogger(LOGGER_NAME);
	private static final Logger publicLogger = Logger.getLogger(PUBLIC_LOGGER_NAME);
	private static final byte[] FOOTER_MAGIC_BYTES = new byte[] { 0x1E, (byte) 0xAF, 0x10, (byte) 0xAD };
	private Footer footer = null;
	private static final int BUFFER_SIZE = 4096;
	private final byte[] buffer = new byte[BUFFER_SIZE];
	private long start = 0;
	private final ArrayList<File> searchPath = new ArrayList<File>();
	
	//used for testing purposes
	public static void main(String[] args) {
		LibraryCollector collector = new LibraryCollector();
		ConsoleHandler handler = new ConsoleHandler();
		handler.setLevel(Level.FINEST);
		Formatter formatter = new SimpleFormatter();
		handler.setFormatter(formatter);
		Logger log = Logger.getLogger(PUBLIC_LOGGER_NAME);
		log.addHandler(handler);
		log.setLevel(Level.FINEST);

		//assumes args[0] is the path to the core file, args[1] is the type of collection
		CollectorResult result = collector.collectLibrariesFor(args[0], args[1]);
		System.out.println("Collection completed with result : " + result.name());
	}
	
	/**
	 * Start the library collection process for a given core file
	 * @param coreFilePath path to the core file
	 * @param type the type of library collection to perform
	 * @return the result of the collection
	 */
	public CollectorResult collectLibrariesFor(final String coreFilePath, String type) {
		return collectLibrariesFor(coreFilePath, CollectionType.valueOf(type));
	}

	/**
	 * Start the library collection process for a given core file
	 * @param coreFilePath path to the core file
	 * @param type the type of library collection to perform
	 * @return the result of the collection
	 */
	public CollectorResult collectLibrariesFor(final String coreFilePath, CollectionType type) {
		final File coreFile = new File(coreFilePath);
		if (coreFile.exists()) {
			if (areLibrariesPresent(coreFile)) { //the libraries have already been collected
				return CollectorResult.COMPLETE_ALREADY_PRESENT;
			} else {
				return collectLibrariesFor(coreFile, type);
			}
		} else {
			throw new IllegalArgumentException("The core file specified at " + coreFilePath + " does not exist");
		}
	}
	
	/**
	 * Check to see if the libraries have already been collected for the specified core file.
	 * @param coreFile core file to check
	 * @return true if the libraries are already present, false if not and they need to be collected
	 */
	private static boolean areLibrariesPresent(final File coreFile) {
		logger.fine("Checking to see if the libraries have already been collected");
		RandomAccessFile stream = null;
		try {
			stream = new RandomAccessFile(coreFile, "r");
			stream.seek(stream.length() - FOOTER_MAGIC_BYTES.length);
			byte[] data = new byte[FOOTER_MAGIC_BYTES.length];
			stream.read(data);
			for (int i = 0; i < data.length; i++) {
				if (data[i] != FOOTER_MAGIC_BYTES[i]) {
					return false;
				}
			}
		} catch (Exception e) {
			publicLogger.log(FINE, "Could not check core file library collection status", e);
			return false;
		} finally {
			if (stream != null) {
				try {
					stream.close();
				} catch (Exception e) {
					publicLogger.log(FINE, "Could not close core file", e);
				}
			}
		}
		return true;
	}
	
	/**
	 * Write the footer for the native libraries onto the end of the core file
	 * @param coreFile core file to append to
	 */
	private void writeFooter(final File coreFile) {
		final ObjectOutputStream stream;
		try {
			MonitoredFileOutputStream out = new MonitoredFileOutputStream(coreFile, true);		//set to output at end of file
			stream = new ObjectOutputStream(out);		//Serialise the footer to disk
			stream.writeObject(footer);
			stream.flush();
			logger.fine("Footer size : " + out.getBytesWritten());
			out.write(out.getBytesWrittenAsArray());
			out.write(FOOTER_MAGIC_BYTES);
			out.flush();
			out.close();
			if(logger.isLoggable(Level.FINEST)) {
				//log the serialised footer separately
				FileOutputStream fos = new FileOutputStream("footer.ser");
				ObjectOutputStream oos = new ObjectOutputStream(fos);
				oos.writeObject(footer);
				oos.close();
			}
		} catch (IOException e) {
			publicLogger.log(FINE, "Could not write out collection footer", e);
		}
		logger.finer(footer.toString());
	}
	
	/**
	 * Controller for the collection process.
	 * @param coreFile core file to process
	 * @param type collection type
	 * @return collection result
	 */
	public CollectorResult collectLibrariesFor(final File coreFile, CollectionType type) {
		LibraryAdapter adapter = getAdapterForCollectionType(type);
		if (adapter.isLibraryCollectionRequired(coreFile)) {
			ArrayList<String> modules = adapter.getLibraryList(coreFile);
			createSearchPathForLibraries(modules);
			footer = new Footer(modules.size());
			for (String msg : adapter.getErrorMessages()) {
				//add any error messages from the adapter
				footer.addErrorMessage(msg);
			}
			RandomAccessFile out = null;
			try {
				out = new RandomAccessFile(coreFile, "rw");
				start = out.length();					//move to the end of the file
				out.seek(start);						//to begin appending libraries
				for(String module : modules) { 
					int bytesAppended = appendLibraryToCoreFile(module, out);
					if(bytesAppended != 0) {			//only create an entry if some bytes have been written
						footer.addEntry(module, bytesAppended, start);
					}
					start += bytesAppended;
				}
				out.close();
				writeFooter(coreFile);
			} catch (FileNotFoundException e) {
				publicLogger.log(FINE, "Could not find core file", e);
			} catch (IOException e) {
				publicLogger.log(FINE, "Error writing libraries", e);
			} finally {
				if (out != null) {
					try {
						out.close();
					} catch (IOException e) {
						logger.log(FINE, "Could not close core file output stream", e);
					}
				}
			}
			if (footer.getErrorMessages().length == 0) {
				return CollectorResult.COMPLETE;
			} else {
				return CollectorResult.COMPLETE_ERRORS;
			}
		} else {
			publicLogger.fine("This core file is not identified as requiring library collection");
			return CollectorResult.COMPLETE_NOT_REQUIRED;			//this core file format does not have missing libraries
		}
	}
	
	/**
	 * Gets the appropriate adapter for providing a list of libraries to collect.
	 * @param type the collection type being used
	 * @return the adapter to use
	 */
	private static LibraryAdapter getAdapterForCollectionType(CollectionType type) {
		switch (type) {
		case DTFJ:
			logger.fine("Using DTFJ library adaptor");
			return new DTFJLibraryAdapter();
		case DDR:
			logger.fine("Using DDR library adaptor");
			return new DDRLibraryAdapter();
		default:
			logger.fine("Using DDR library adaptor (default)");
			return new DDRLibraryAdapter();
		}
	}

	/**
	 * Function to append a library to the end of a core file.
	 * @param name path to the library to collect
	 * @param out the already open core file to append to
	 * @return number of bytes appended to the core file
	 */
	private int appendLibraryToCoreFile(final String name, final RandomAccessFile out) {
		int bytesAppended = 0;
		final File file = locateLibrary(name);
		FileInputStream in = null;
		if (file.exists()) {
			publicLogger.fine("Collecting library: " + name);
			try {
				in = new FileInputStream(file);
				int bytesRead = in.read(buffer);
				while(bytesRead != -1) {
					bytesAppended += bytesRead;
					out.write(buffer, 0, bytesRead);
					bytesRead = in.read(buffer);
				}
			} catch (FileNotFoundException e) {
				if (!isVirtualLibrary(name)) {
					String msg = "Could not add library [" + name + "] as the file does not exist";
					publicLogger.fine(msg);
					footer.addErrorMessage(msg);
				}
			} catch (IOException e) {
				footer.addErrorMessage("Could not append library : " + e.getMessage());
				publicLogger.log(FINE, "Could not collect library", e);
			} finally {
				if(in != null) {
					try {
						in.close();
					} catch (IOException e) {
						logger.log(FINE, "Could not close library input stream", e);
					}
				}
			}
		} else {
			if (!isVirtualLibrary(name)) {
				String msg = "Could not add library [" + name + "] as the file does not exist";
				publicLogger.fine(msg);
				footer.addErrorMessage(msg);
			}
		}
		return bytesAppended;
	}

	/**
	 * Is the given libraryName one of several known not to exist in the file system?
	 *
	 * See http://man7.org/linux/man-pages/man7/vdso.7.html.
	 */
	private static boolean isVirtualLibrary(String libraryName) {
		return libraryName.equals("lib.so") // the SOname that appears in executables
				// ABI: i386, ia64, sh
				|| libraryName.startsWith("linux-gate")
				// ABI: aarch64, arm, mips, x86_64, x86/32
				|| libraryName.startsWith("linux-vdso.so.1")
				// ABI: ppc/32, s390
				|| libraryName.startsWith("linux-vdso32.so.1")
				// ABI: ppc/64, s390x
				|| libraryName.startsWith("linux-vdso64.so.1");
	}

	/**
	 * Locates the named library. Will attempt to resolve relative locations.
	 * Note AIX only supplies java as the name of the main library and so this
	 * needs to be located via a search.
	 * @param name
	 * @return
	 */
	private File locateLibrary(final String name) {
		File module = new File(name);
		if (module.exists()) {
			return module; //file exists
		}
		for (File dir : searchPath) { //look through the search path for the file
			module = new File(dir, name);
			logger.finer("Searching " + module.getAbsolutePath());
			if (module.exists()) {
				return module;
			}
		}
		return new File(name); //unable to resolve so just return as a file
	}
	
	/**
	 * Create a search path with which to resolve relative module paths
	 * @param modules
	 */
	private void createSearchPathForLibraries(ArrayList<String> modules) {
		//add the jre bin directory to the search path
		String jre_bin = System.getProperty("java.home") + File.separator + "bin";
		File javaPath = new File(jre_bin);
		logger.fine("Adding search path " + javaPath.getAbsolutePath());
		searchPath.add(javaPath);

		// Strictly, there is an involved search strategy for how libraries are found on UNIX that involves 
		// examining the LD_LIBRARY_PATH (Linux) or LIBPATH (AIX) environment variable or examining the dynamic table of each library 
		// for DT_NEEDED and DT_RPATH entries and the system default path. 
		// See Chap 5. of System V Application Binary Interface document, section "Shared Object Dependencies".
		// or the following link (if it still exists) http://www.eyrie.org/~eagle/notes/rpath.html
		// Most of the JRE libraries are found via the full path names in the debug data in the executable so it
		// is often just the system libraries e.g. libnuma.so which are on the system default path that are harder to find.  
		// We cannot easily know the full default search path on Linux as it is set from the contents of 
		// /etc/ld.so.conf which can contain quite a number of directories and recurse to other conf files.
		// For example it may well contain various X11, gnome and kde directories.
		// In practice though we reliably gather most of the system libraries by adding just the following 
		// libraries to the search path ... note we add all the possible library locations for all the
		// likely operating systems and it is fine - it does not disturb the collection to have paths that do 
		// not exist.
		// for various flavours of 32/64 bit Linux, 32/64 bit AIX, and UNIX System Services on Z 
		searchPath.add(new File("/lib"));
		searchPath.add(new File("/lib64"));
		searchPath.add(new File("/usr/lib"));
		searchPath.add(new File("/usr/lib64"));
		searchPath.add(new File("/usr/local/lib"));
		searchPath.add(new File("/usr/local/lib64"));

		File file = null;
		for (String module : modules) {
			file = new File(module);
			File parent = file.getParentFile();
			if ((parent != null) && parent.isDirectory()) {
				if (!searchPath.contains(parent)) {
					logger.fine("Adding search path " + parent.getAbsolutePath());
					searchPath.add(parent);
				}
			}
		}
	}

}
