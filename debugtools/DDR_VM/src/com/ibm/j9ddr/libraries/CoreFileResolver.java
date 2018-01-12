/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

//resolves libraries from a core file

import static java.util.logging.Level.WARNING;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.nio.ByteOrder;
import java.util.logging.Logger;

import javax.imageio.stream.FileImageInputStream;
import javax.imageio.stream.ImageInputStream;

import com.ibm.j9ddr.corereaders.ILibraryResolver;
import com.ibm.j9ddr.corereaders.LibraryDataSource;

public class CoreFileResolver implements ILibraryResolver {
	public static final int FOOTER_MAGIC = 0x1EAF10AD;
	public static final byte[] FOOTER_MAGIC_BYTES = new byte[] {0x1E, (byte)0xAF, 0x10, (byte)0xAD};
	private static final Logger logger = Logger.getLogger("com.ibm.j9ddr.libraries");
	protected final File coreFile;
	protected ImageInputStream stream;
	protected boolean validFile = false;
	protected Footer footer = null;
	
	public static void main(String[] args) {
		@SuppressWarnings("unused")
		CoreFileResolver resolver = new CoreFileResolver(args[0]);
	}
	
	public CoreFileResolver(String coreFilePath) {
		coreFile = new File(coreFilePath);
		validateCoreFile();
	}
	
	public CoreFileResolver(File file) {
		coreFile = file;
		validateCoreFile();
	}

	public CoreFileResolver(ImageInputStream stream) {
		coreFile = null;
		this.stream = stream;
		validateContents();
	}
	
	//validate that the supplied core file exists 
	private void validateCoreFile() {
		if(coreFile.exists()) {
			try {
				if(stream == null) {
					stream = new FileImageInputStream(coreFile); 
				}
				validateContents();
			} catch (FileNotFoundException e) {
				throw new IllegalArgumentException("The core file specified at " + coreFile.getAbsolutePath() + " does not exist");
			} catch (IOException e) {
				logger.log(WARNING, "Could not validate core file " + coreFile.getAbsolutePath(), e);
				return;
			}
			
		} else {
			throw new IllegalArgumentException("The core file specified at " + coreFile.getAbsolutePath() + " does not exist");
		}
	}
	
	//validate that the contents of the file by reading in the footer
	private void validateContents() {
		validFile = isValidFile();
		if(!validFile) {
			try {
				if(coreFile != null) {
					//only close the stream if we created it on the core file, otherwise leave it open
					stream.close();
				}
			} catch (IOException e) {
				logger.log(WARNING, "Could not close core file", e);
				return;
			}
		}
	}

	public void extractLibrariesToDir(String path) throws IOException {
		File dir = new File(path);
		if(!dir.exists()) {
			throw new IllegalArgumentException("");
		}
		for(FooterLibraryEntry entry : footer.getEntries()) {
			File library = new File(dir, entry.getName());
			extractLibrary(entry.getPath(), library);
		}
	}
	
	public void extractLibrary(String libraryName, File path) throws IOException {
		FooterLibraryEntry entry = footer.findEntry(libraryName);
		FileOutputStream out = null;
		SlidingFileInputStream in = null;
		try {
			out = new FileOutputStream(path);
			byte[] buffer = new byte[4096];
			in = new SlidingFileInputStream(coreFile, entry.getStart(), entry.getSize());
			int bytesRead = in.read(buffer);
			while(bytesRead != -1) {
				out.write(buffer, 0, bytesRead);
				bytesRead = in.read(buffer);
			}
		} finally {
			in.close();
			out.close();
		}
	}
	
	public boolean hasLibraries() {
		return validFile;
	}
	
	private void readFooter() throws IOException {
		ByteOrder bo = stream.getByteOrder();		//the footer is a serialised java object and so always BIG_ENDIAN
		try {
			stream.setByteOrder(ByteOrder.BIG_ENDIAN);
			long streamLength = stream.length();
			stream.seek(streamLength - 8);
			int length = stream.readInt();
			SlidingFileInputStream in = new SlidingFileInputStream(stream, streamLength - length - 8, length);
			ObjectInputStream objin = new ObjectInputStream(in);
			try {
				footer = (Footer) objin.readObject();
				logger.fine(footer.toString());
			} catch (ClassNotFoundException e) {
				logger.log(WARNING, "Could find footer class", e);
			}
			objin.close();
		} finally {
			//restore the stream to the original byte order
			stream.setByteOrder(bo);
		}
	}
	
	private boolean isValidFile() {
		try {
			byte[] check = new byte[FOOTER_MAGIC_BYTES.length];
			stream.seek(stream.length() - check.length);
			stream.readFully(check);
			for(int i = 0; i < check.length; i++) {
				if(check[i] != FOOTER_MAGIC_BYTES[i]) {
					return false;
				}
			}
			readFooter();
			return true;
		} catch (IOException e) {
			logger.log(WARNING, "Could not check core file library validity", e);
			return false;
		}
	}
	
	public LibraryDataSource getLibrary(String fileName, boolean silent)	throws FileNotFoundException {
		if(!validFile) {
			throw new FileNotFoundException("Cannot find " + fileName + " as the required libraries are missing from the core file");
		}
		FooterLibraryEntry entry = footer.findEntry(fileName);
		if(entry == null) {
			if(!silent) {
				logger.fine(String.format("Cannot find %s", fileName));
			}
			throw new FileNotFoundException("Cannot find " + fileName);
		}
		LibraryDataSource source;
		try {
			SlidingImageInputStream in = new SlidingImageInputStream(stream, entry.getStart(), entry.getSize());
			source = new LibraryDataSource(fileName, in);
		} catch (IOException e) {
			logger.log(WARNING, "Error setting sliding input window", e);
			source = new LibraryDataSource(fileName);
		}
		return source;
	}

	public LibraryDataSource getLibrary(String fileName) throws FileNotFoundException {
		if(!validFile) {
			throw new FileNotFoundException("Cannot find " + fileName + " as the required libraries are missing from the core file");
		}
		return getLibrary(fileName, true);
	}

	/**
	 * Give the resolver a chance to clean up any used resources
	 */
	public void dispose() {
		try {
			if(validFile) {
				stream.close();
			}
		} catch (IOException e) {
			logger.log(WARNING, "Failed to dispose of underlying file stream", e);
		}
	}
	
}
