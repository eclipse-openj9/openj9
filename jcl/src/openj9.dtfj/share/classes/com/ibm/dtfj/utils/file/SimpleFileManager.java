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
import java.util.ArrayList;
import java.util.List;

import javax.imageio.stream.FileImageInputStream;
import javax.imageio.stream.ImageInputStream;

/**
 * Simple file manager for dealing with files that are intended to be read directly.
 * 
 * @author adam
 *
 */
public class SimpleFileManager extends FileManager {
	protected final File managedFile;			//the file to manage
	private long filesize = Long.MAX_VALUE;		//file size for the file, default will pass any test and is set by getStream()
	
	public SimpleFileManager(File file) {
		managedFile = file;
	}

	public List<ManagedImageSource> getImageSources() throws IOException {
		ArrayList<ManagedImageSource> candidates = new ArrayList<ManagedImageSource>();
		ImageInputStream iis = getStream();
		if(identifiedCoreFile(candidates, iis)) {
			iis.close();
			return candidates;
		}
		if(identifiedJavaCore(candidates, iis)) {
			iis.close();
			return candidates;
		}
		if(identifiedPHD(candidates, iis)) {
			iis.close();
			return candidates;
		}
		iis.close();		//file contents not recognised so close stream and exit
		throw new IOException("No Image sources were found for " + managedFile.getAbsolutePath());
	}
	
	private boolean identifiedCoreFile(ArrayList<ManagedImageSource> candidates, ImageInputStream iis) throws IOException {
		if(FileSniffer.isCoreFile(iis, filesize)) {
			ManagedImageSource candidate = new ManagedImageSource(managedFile.getName(), ImageSourceType.CORE);
			candidate.setPath(managedFile.getPath());
			File meta = new File(managedFile.getParent(), managedFile.getName() + ".xml");
			logger.finer("Identified core file : " + candidate.toURI());
			if(fileExists(meta)) {
				ManagedImageSource metadata = new ManagedImageSource(meta.getName(), ImageSourceType.META);
				metadata.setPath(meta.getPath());
				candidate.setMetadata(metadata);
				logger.finer("Identified core file metadata : " + metadata.toURI());
			}
			candidates.add(candidate);
			return true;
		}
		return false;
	}

	private boolean identifiedJavaCore(ArrayList<ManagedImageSource> candidates, ImageInputStream iis) throws IOException {
		if(FileSniffer.isJavaCoreFile(iis, filesize)) {
			ManagedImageSource candidate = new ManagedImageSource(managedFile.getName(), ImageSourceType.JAVACORE);
			candidate.setPath(managedFile.getPath());
			candidates.add(candidate);
			logger.finer("Identified javacore file : " + candidate.toURI());
			return true;
		}
		return false;
	}
	
	private boolean identifiedPHD(ArrayList<ManagedImageSource> candidates, ImageInputStream iis) throws IOException {
		if(FileSniffer.isPHDFile(iis, filesize)) {
			ManagedImageSource candidate = new ManagedImageSource(managedFile.getName(), ImageSourceType.PHD);
			candidate.setPath(managedFile.getPath());
			//now see if there is an associated meta file - this will be a corresponding javacore
			String[] jcnames = getJavaCoreNameFromPHD(managedFile.getName());
			logger.finer("Identified PHD file : " + candidate.toURI());
			for(String jcname : jcnames) {
				File jc = new File(managedFile.getParent(), jcname);
				if(fileExists(jc)) {
					ManagedImageSource meta = new ManagedImageSource(jcname, ImageSourceType.META);
					meta.setPath(jc.getPath());
					candidate.setMetadata(meta);
					logger.finer("Identified associated javacore file : " + meta.toURI());
					break;		//found it so stop searching
				}
			}
			candidates.add(candidate);
			return true;
		}
		return false;
	}
	
	public ImageInputStream getStream() throws IOException {
		if (managedFile.exists()) {
			// found it in HFS on z/OS or normally on other platforms
			filesize = managedFile.length();
			return new FileImageInputStream(managedFile);
		}
		/*[IF PLATFORM-mz31 | PLATFORM-mz64 | ! ( Sidecar18-SE-OpenJ9 | Sidecar19-SE )]*/
		String os = System.getProperty("os.name"); //$NON-NLS-1$
		if (os == null) {
			throw new IOException("Cannot determine the system type from os.name"); //$NON-NLS-1$
		}
		if (os.toLowerCase().contains("z/os")) { //$NON-NLS-1$
			// z/OS check
			// Look for it as an MVS data set. The MVSImageInputStream constructor throws FNFE if the file does not exist
			try {
				MVSImageInputStream mvsStream = new MVSImageInputStream(managedFile.getName());
				filesize = mvsStream.size();
				return mvsStream;
			} catch (FileNotFoundException e) {
				// drop through to throw FileNotFoundException below (with absolute path)
			}
		}
		/*[ENDIF] PLATFORM-mz31 | PLATFORM-mz64 | ! ( Sidecar18-SE-OpenJ9 | Sidecar19-SE )*/
		throw new FileNotFoundException("The specified file " + managedFile.getAbsolutePath() + " was not found");
	}
}
