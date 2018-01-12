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
import java.io.IOException;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.imageio.stream.ImageInputStream;

import com.ibm.dtfj.image.ImageFactory;

/**
 * Class which manages the files on the local system.
 * 
 * @author adam
 *
 */
public abstract class FileManager {
	public final static long MIN_CORE_SIZE = 20971520;		//20MB
	protected final static Logger logger = Logger.getLogger(ImageFactory.DTFJ_LOGGER_NAME);
	
	public abstract List<ManagedImageSource> getImageSources() throws IOException;
	
	/**
	 * Gets a stream for the file which is being managed, this could be an archive file or a normal file.
	 * If this is a compressed file then the stream returned will be a 'raw' stream onto file itself.
	 * @return
	 * @throws IOException
	 */
	public abstract ImageInputStream getStream() throws IOException;
	
	/**
	 * Creates a unique temporary directory under the specified parent
	 * @param parent
	 * @return
	 * @throws IOException
	 */
	public static File createTempDir(File parent) throws IOException {
		if(!parent.exists() || !parent.isDirectory()) {
			throw new IOException("The specified parent temporary directory does not exist or is not a directory : " + parent.getAbsolutePath());
		}
		File tmpdir = File.createTempFile("dtfj", "cprss", parent);			//this creates the file
		tmpdir.delete();													//so delete it
		tmpdir.mkdirs();													//and then use the name for the directory
		tmpdir.deleteOnExit();
		logger.fine("Created temporary directory for extracted files : " + tmpdir.getAbsolutePath());
		return tmpdir;
	}
	

	/**
	 * A platform aware file existence checker that allows for MVS datasets on z/OS
	 * @param file the file to check
	 * @return true if the file exists, or the check cannot be performed in which case the default response of true is returned
	 */
	/*
	 * This function is required because although the DDR based core file readers can do a very lightweight check on
	 * file existence the legacy core readers attempt to parse the dump in order to see if they recognise a file.
	 */
	public static boolean fileExists(File file) {
		if (file.exists()) {
			// found it in HFS on z/OS or normally on other platforms so return true
			return true;
		}
		/*[IF PLATFORM-mz31 | PLATFORM-mz64 | ! ( Sidecar18-SE-OpenJ9 | Sidecar19-SE )]*/
		String os = System.getProperty("os.name"); //$NON-NLS-1$
		if (os == null) {
			// cannot perform the check so default to true to allow processing to continue blind
			return true;
		}
		if (os.toLowerCase().contains("z/os")) { //$NON-NLS-1$
			// z/OS check
			// Look for it as an MVS data set. The MVSImageInputStream constructor throws FNFE if the file does not exist
			try {
				@SuppressWarnings("resource")
				MVSImageInputStream mvsStream = new MVSImageInputStream(file.getName());
				try {
					mvsStream.close();
				} catch (IOException e) {
					// ignore
				}
				return true;
			} catch (FileNotFoundException e) {
				// ignore
			}
		}
		/*[ENDIF] PLATFORM-mz31 | PLATFORM-mz64 | ! ( Sidecar18-SE-OpenJ9 | Sidecar19-SE )*/
		return false;
	}
	
	/**
	 * Factory method for getting the correct manager to deal with the supplied file
	 * 
	 * @param file
	 * @return
	 */
	public static FileManager getManager(File file) {
		String name = file.getName().toLowerCase();
		try {
			if(FileSniffer.isZipFile(file)) {
				return new ZipFileManager(file);
			}
		} catch (Exception e) {
			//just log the error and carry on
			logger.log(Level.FINEST, "Error encountered sampling potential zip file", e);
		}
		if(name.endsWith(".gz")) {
			return new GZipFileManager(file);
		}
		return new SimpleFileManager(file);
	}
	
	public static boolean isArchive(File file) {
		String name = file.getName().toLowerCase();
		try {
			if(FileSniffer.isZipFile(file)) {
				return true;
			}
		} catch (Exception e) {
			//just log the error and carry on
			logger.log(Level.FINEST, "Error encountered sampling potential zip file", e);
		}
		if(name.endsWith(".gz")) {
			return true;
		}
		return false;
	}
	
	/**
	 * Generate a list of possible javacore names from a given phd name.
	 * @param name the heap dump
	 * @return candidate javacore names
	 */
	protected String[] getJavaCoreNameFromPHD(String name) {
		String prefix = "heapdump";
		// Allow extra stuff in front of "heapdump"
		int p = Math.max(0, name.indexOf(prefix));
		String extraPrefix = name.substring(0, p);
		name = name.substring(p);
		if (name.startsWith(prefix)) {
			// Name may be of form heapdump.20081016.125646.7376.0001.phd
			// with metafile javacore.20081016.125646.7376.0002.txt
			// or heapdump.20081016.125646.7376.phd
			// with metafile javacore.20081016.125646.7376.txt  
			// or heapdump.20081106.111729.675904.phd 
			// with metafile javacore.20081106.111729.675904.txt
			// or heapdump.20081016.125646.7376.0001.phd.gz
			// with metafile javacore.20081016.125646.7376.0002.txt
			// i.e. filename.date.time.pid.seq.ext
			// Also allow anything before the heapdump, e.g.
			// myFailingTest.123.heapdump.20081016.125646.7376.phd
			String s[] = name.split("\\.");
			s[0] = extraPrefix + "javacore" + s[0].substring(prefix.length());
			int sequence[];
			if (s.length >= 6) {
				sequence = new int[]{0,1,-1};
			} else {
				sequence = new int[]{0};
			}
			String[] javaCoreNames = new String[sequence.length];
			for(int index = 0; index < sequence.length; index++) {
				javaCoreNames[index] = genJavacoreName(s, sequence[index], 4);
			}
			return javaCoreNames;
		}
		//could not determine any possible matches so return an empty array
		return new String[]{};
	}
	
	protected String genJavacoreName(String[] s, int inc, int componentToInc) {
		StringBuffer sb = new StringBuffer(s[0]);
		for (int i = 1; i < s.length - 1; ++i) {
			String ss = s[i];
			// Increment the dump id if required
			if (i == componentToInc) {
				try {
					int iv = Integer.parseInt(ss);
					String si = Integer.toString(iv+inc);
					ss = ss.substring(0, Math.max(0, ss.length() - si.length())) + si;
				} catch (NumberFormatException e) {
				}
			} else if (i == s.length - 2 && s[i + 1].equals("gz")) {
				// Skip adding .phd for file ending .phd.gz
				break;
			}
			sb.append('.');
			sb.append(ss);
		}
		sb.append(".txt");
		return sb.toString();
	}
}

