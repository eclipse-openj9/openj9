/*******************************************************************************
 * Copyright (c) 2011, 2014 IBM Corp. and others
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

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import javax.imageio.stream.ImageInputStream;
import javax.imageio.stream.MemoryCacheImageInputStream;

/**
 * Resolves entries in a zip file without extracting the file
 * 
 * @author adam
 *
 */
public class ZipFileResolver implements ILibraryResolver {
	private static Logger log = Logger.getLogger(LibraryResolverFactory.LIBRARY_PATH_SYSTEM_PROPERTY);
	
	private final File zipfile;
	private ArrayList<String> entryNames = null;
	private ZipFile zip = null;
	
	public ZipFileResolver(ImageInputStream stream) {
		URI uri = null;
		try {
			uri = new URI(stream.toString());
			if((uri.getFragment() != null) && (uri.getFragment().length() != 0)) {
				//need to strip any fragments from the URI which point to the core in the zip file
				uri = new URI(uri.getScheme() + "://" + uri.getPath());
			}
		} catch (URISyntaxException e) {
			log.log(Level.FINE, "Skipping zip resolver as the supplied stream was not a ZipMemoryCacheImageInputStream");
			zipfile = null;
			return;
		}
		if(uri.getScheme().equals("file")) {
			zipfile = new File(uri);
		} else {
			zipfile = null;
		}
	}
	
	public LibraryDataSource getLibrary(String fileName, boolean silent) throws FileNotFoundException {
		if(zipfile == null) {
			throw new FileNotFoundException(fileName);
		}
		ZipEntry entry = null;
		try {
			zip = new ZipFile(zipfile);
			if(entryNames == null) {
				entryNames = new ArrayList<String>();
				Enumeration<? extends ZipEntry> entries = zip.entries();
				while(entries.hasMoreElements()) {
					ZipEntry nextEntry = entries.nextElement();
					entryNames.add(nextEntry.getName());
					if(nextEntry.getName().equals(fileName)) {
						entry = nextEntry;		//we found the name we were asked to resolve
					}
				}
				if(entry == null) {
					//didn't find the named entry whilst building the cache
					throw new FileNotFoundException(fileName);
				}
			} else {
				if(entryNames.contains(fileName)) {
					entry = zip.getEntry(fileName);
				} else {
					throw new FileNotFoundException(fileName);
				}
			}
			MemoryCacheImageInputStream stream = new MemoryCacheImageInputStream(zip.getInputStream(entry));
			log.fine("Resolved library " + fileName + " in zip file " + zipfile.getAbsolutePath());
			return new LibraryDataSource(fileName, stream);
		} catch (IOException e) {
			log.log(Level.FINE, "Error resolving library in zip file " + zipfile.getAbsolutePath(), e);
			throw new FileNotFoundException(fileName + " (due to underlying IOException)");
		}
	}

	public LibraryDataSource getLibrary(String fileName) throws FileNotFoundException {
		return getLibrary(fileName, true);
	}

	public void dispose() {
			if(zip != null) {
				try {
					zip.close();
				} catch (IOException e) {
					log.log(Level.FINE, "Error closing zip file", e);
				}
			}
	}
	
}
