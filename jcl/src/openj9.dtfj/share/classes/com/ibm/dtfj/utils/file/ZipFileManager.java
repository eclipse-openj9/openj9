/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2011, 2018 IBM Corp. and others
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
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipException;
import java.util.zip.ZipFile;

import javax.imageio.stream.ImageInputStream;

public class ZipFileManager extends CompressedFileManager {
	
	public ZipFileManager(File file) {
		super(file);
	}

	/**
	 * Returns all image sources found in this zip file
	 * @return all the recognised images
	 * @throws IOException thrown for problems reading the zip, or by individual image factories
	 */
	public List<ManagedImageSource> getImageSources() throws IOException {
		ArrayList<ManagedImageSource> candidates = new ArrayList<ManagedImageSource>();
		ZipFile zip = new ZipFile(managedFile);
		Enumeration<? extends ZipEntry> entries = zip.entries();
		while(entries.hasMoreElements()) {
			ZipEntry entry = entries.nextElement();
			if(entry.isDirectory()) {
				continue;			//skip directories
			}
			if(identifiedCoreFile(zip, entry, candidates)) {
				continue;		//the entry was recognised and handled as a core file
			}
			if(identifiedJavaCore(zip, entry, candidates)) {
				continue;		//the entry was recognised and handled as a core file
			}
			if(identifiedPHD(zip, entry, candidates)) {
				continue;		//the entry was recognised and handled as a core file
			}
		}
		zip.close();		//file contents not recognised so close file and exit
		return candidates;
	}
	
	private boolean identifiedCoreFile(ZipFile zip, ZipEntry entry, ArrayList<ManagedImageSource> candidates) throws IOException {
		if(entry.getSize() < MIN_CORE_SIZE) {
			return false;		// entry is not big enough for a core file so skip
		}
		//now check to see if the file that has been located in the zip is a core file or not
		InputStream stream = zip.getInputStream(entry);
		if(FileSniffer.isCoreFile(stream, entry.getSize())) {
			ManagedImageSource candidate = new ManagedImageSource(entry.getName(), ImageSourceType.CORE);
			candidate.setArchive(managedFile);
			logger.finer("Identified core file : " + candidate.toURI());
			//check for XML in legacy extraction systems
			ZipEntry metadata = zip.getEntry(entry.getName() + ".xml");
			if(metadata != null) {
				ManagedImageSource meta = new ManagedImageSource(metadata.getName(), ImageSourceType.META);
				meta.setArchive(managedFile);
				candidate.setMetadata(meta);
				logger.finer("Identified core file metadata : " + meta.toURI());
			}
			candidates.add(candidate);
			return true;
		}
		return false;
	}
	
	private boolean identifiedJavaCore(ZipFile zip, ZipEntry entry, ArrayList<ManagedImageSource> candidates) throws IOException {
		//now check to see if the file that has been located in the zip is a core file or not
		InputStream stream = zip.getInputStream(entry);
		if(FileSniffer.isJavaCoreFile(stream, entry.getSize())) {
			ManagedImageSource candidate = new ManagedImageSource(entry.getName(), ImageSourceType.JAVACORE);
			candidate.setArchive(managedFile);
			candidates.add(candidate);
			logger.finer("Identified javacore file : " + candidate.toURI());
			return true;
		}
		return false;	
	}
	
	private boolean identifiedPHD(ZipFile zip, ZipEntry entry, ArrayList<ManagedImageSource> candidates) throws IOException {
		//now check to see if the file that has been located in the zip is a core file or not
		InputStream stream = zip.getInputStream(entry);
		if(FileSniffer.isPHDFile(stream, entry.getSize())) {
			ManagedImageSource candidate = new ManagedImageSource(entry.getName(), ImageSourceType.PHD);
			candidate.setArchive(managedFile);
			logger.finer("Identified PHD file : " + candidate.toURI());
			//now see if there is an associated meta file - this will be a corresponding javacore
			String[] jcnames = getJavaCoreNameFromPHD(entry.getName());
			for(String jcname : jcnames) {
				if(zip.getEntry(jcname) != null) {
					ManagedImageSource meta = new ManagedImageSource(jcname, ImageSourceType.META);
					meta.setArchive(managedFile);
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
	
	/**
	 * Extracts the specified core file to the specified directory
	 * @param file
	 * @throws IOException 
	 * @throws ZipException 
	 */
	public void extract(ManagedImageSource file, File todir) throws IOException {
		checkDirectoryToExtractTo(todir);
		//check if the name contains the directory hierarchy which needs to be replicated
		File extractTo = getExtractLocation(file.getName(), todir);
		//extract into the correct path
		
		ZipFile zip = new ZipFile(managedFile);
		ZipEntry entry = zip.getEntry(file.getName());
		extractEntry(zip, entry, extractTo);
		
		//update the managed file object
		file.setExtractedTo(extractTo);
		file.incRefCount();
	}
	
	//zip files are hierarchical so this method maps the hierarchy in the zip onto the extraction directory
	private File getExtractLocation(String path, File todir) throws IOException {
		String todirPath = todir.getCanonicalPath();
		//make a copy so as to not accidentally change the parameter
		File folder = new File(todirPath);
		String npath = path.replace('\\', '/');
		if((npath.length() > 2) && (npath.charAt(1) == ':')) {
			npath = npath.substring(2);
		}
		
		File tempFile = new File(todirPath, npath);
		String tempFileString = tempFile.getCanonicalPath();
		if (!tempFileString.startsWith(todirPath)) {
			throw new IOException(tempFileString + " should be subdirectory of " + todirPath);
		}
		
		String[] subdirs = npath.split("/");
		for(int i = 0; i < subdirs.length - 1; i++) {
			if(subdirs[i].length() > 0) {
				folder = new File(folder, subdirs[i]);
				folder.deleteOnExit();
				folder.mkdir();
			}
		}
		File extractTo = new File(folder, subdirs[subdirs.length - 1]);
		extractTo.deleteOnExit();
		return extractTo;
	}
	
	private void extractEntry(ZipFile zip, ZipEntry entry, File path) throws IOException {
		InputStream in = zip.getInputStream(entry);
		extractEntry(in, path);
	}
	
	public ImageInputStream getStream(ManagedImageSource source) throws IOException {
		ZipFile zip = new ZipFile(source.getArchive());
		ZipEntry entry = zip.getEntry(source.getName());
		InputStream is = zip.getInputStream(entry);
		ImageInputStream iis = new ZipMemoryCacheImageInputStream(entry, is, source);
		return iis;
	}
	
	//this will extract the entire contents of the zip file to the specified directory
	@Override
	public void extract(File todir) throws IOException {
		ZipFile zip = new ZipFile(managedFile);
		Enumeration<? extends ZipEntry> entries = zip.entries();
		while(entries.hasMoreElements()) {
			ZipEntry entry = entries.nextElement();
			File extractTo = getExtractLocation(entry.getName(), todir);
			extractEntry(zip, entry, extractTo);
		}
	}

}
