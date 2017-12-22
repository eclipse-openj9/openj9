/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2011, 2017 IBM Corp. and others
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
package com.ibm.dtfj.utils.file;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.List;

import javax.imageio.stream.ImageInputStream;

/**
 * Abstract class for handling compressed files
 * 
 * @author adam
 *
 */
public abstract class CompressedFileManager extends SimpleFileManager {
	protected final byte[] buffer = new byte[4096];
	
	public CompressedFileManager(File file) {
		super(file);
	}

	public abstract void extract(File todir) throws IOException;
	
	public abstract void extract(ManagedImageSource file, File todir) throws IOException;

	public abstract ImageInputStream getStream(ManagedImageSource source) throws IOException;

	/**
	 * Will check that the directory to extract to is a directory if it exists or create it otherwise.
	 * @param todir directory to extract to
	 * @exception IllegalArgumentException if the specified File is not a directory
	 */
	protected void checkDirectoryToExtractTo(File todir) {
		if(todir.exists()) {
			//check that if the extraction directory exists it is indeed a directory and not a file
			if(!todir.isDirectory()) {
				throw new IllegalArgumentException("The specified directory " + todir.getAbsolutePath() + " is not a directory ");
			}
		} else {
			//extraction directory does not exist so create it
			todir.mkdirs();
		}
	}
	
	/**
	 * Returns a specified image source specified by the path in the zip file
	 * @param path path within the zip
	 * @return the created image
	 * @throws IOException for problems creating the image or FileNotFoundException if the file specified by the path could not be found
	 */
	public ManagedImageSource getImageSource(String path) throws IOException {
		List<ManagedImageSource> candidates = getImageSources();
		//a specific source was asked for inside of the archive
		if(candidates.contains(path)) {
			ManagedImageSource candidate = candidates.get(candidates.indexOf(path));
			return candidate;
		} else {
			throw new FileNotFoundException("The entry " + path + " was not found in " + managedFile.getAbsolutePath());
		}
	}
	
	protected void extractEntry(InputStream in, File path) throws IOException {
		FileOutputStream out = null;
		long total = 0;
		try {
			logger.fine("Extracting " + path.getName() + " to " + path.getAbsolutePath());
			out = new FileOutputStream(path);
			int bytesread = -1;
			while((bytesread = in.read(buffer)) != -1) {
				total += bytesread;
				out.write(buffer, 0, bytesread);
			}
		} catch (IOException e) {
			// if the extract fails, delete the partially extracted file CMVC 188711
			out.close();
			path.delete();
			throw new UnzipFailedException ("Error occured when extracting " + path.getName() + " from archive to " + path.getAbsolutePath() + " : " + e.getMessage());
		} finally {
			logger.fine("Extracted " + total + " bytes");
			if(out != null) {
				out.close();
			}
			if(in != null) {
				in.close();
			}
		}
	}
}
