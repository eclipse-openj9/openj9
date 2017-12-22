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
import java.net.URI;
import java.util.UUID;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.dtfj.image.ImageFactory;

/**
 * Represents a managed core file. The core can reside in 
 * a compressed archive or be extracted to the local file system.
 * 
 * @author adam
 *
 */
public class ManagedImageSource {
	private final static Logger logger = Logger.getLogger(ImageFactory.DTFJ_LOGGER_NAME);
	private final String name;			//the name of the image source - the pathname within the archive
	private String path;				//the path to the archive e.g. zip file
	private int refcount = 0;			//reference count for this file
	private File archive = null;		//compressed archive from which this image may be derived
	private File extractedTo = null;	//where the source has been extracted to
	private ManagedImageSource metadata = null;
	private final ImageSourceType type;
	private final UUID uuid = UUID.randomUUID();	//used as a backup unique identifier
	
	public ManagedImageSource(String name, ImageSourceType type) {
		this.name = name;
		this.type = type;
	}
	
	public ImageSourceType getType() {
		return type;
	}

	public boolean hasMetaData() {
		return metadata != null;
	}

	public ManagedImageSource getMetadata() {
		return metadata;
	}

	public void setMetadata(ManagedImageSource metadata) {
		this.metadata = metadata;
	}

	public String getPath() {
		return path;
	}

	public void setPath(String path) {
		this.path = path;
	}

	public boolean isCompressed() {
		return (extractedTo == null);
	}

	public String getName() {
		return name;
	}

	public File getArchive() {
		return archive;
	}

	public void setArchive(File archive) {
		this.archive = archive;
	}
	
	public File getExtractedTo() {
		return extractedTo;
	}

	public void setExtractedTo(File extractedTo) {
		this.extractedTo = extractedTo;
	}

	public int getRefCount() {
		return refcount;
	}
	
	public void incRefCount() {
		refcount++;
	}
	
	public void decRefCount() {
		if(refcount > 0) {
			refcount--;
		}
	}

	@Override
	public boolean equals(Object obj) {
		if((obj == null) || !(obj instanceof ManagedImageSource)) {
			return false;
		}
		ManagedImageSource compareTo = (ManagedImageSource) obj;
		return toURI().equals(compareTo.toURI());
	}

	@Override
	public int hashCode() {
		return name.hashCode();
	}
	
	public String getPathToExtractedFile() {
		return extractedTo.getAbsolutePath();
	}

	/**
	 * Provides a URI for the location of the file after it has been extracted i.e. in a temporary directory
	 * @return
	 */
	public URI getURIOfExtractedFile() {
		if (extractedTo != null) {
			return extractedTo.toURI();
		} else {
			return null;
		}
	}
	
	/**
	 * Provides a URI for the location of the file before it has been extracted i.e. within the archive
	 * @return
	 */
	public URI toURI() {
		try {
			StringBuilder uri = new StringBuilder();
			if(archive != null) {
				uri.append(archive.toURI().toString());
				uri.append("#");
				if(path != null) {
					uri.append(path);
					uri.append("/");
				}
				uri.append(name);	
				String unsafeURI = uri.toString();	// the string builder contains a potentially invalid URI so extract it to a String
				// replace \ with / and spaces are encoded to %20 (this is common for Windows paths)
				String safeURI = unsafeURI.replace('\\', '/').replace(" ", "%20");
				return new URI(safeURI);			//build a new URI from the processed String
			} else {
				if(path != null) {
					File loc = null;
					if(path.toLowerCase().endsWith(name.toLowerCase())) {
						loc = new File(path);
					} else {
						loc = new File(path, name);
					}
					return loc.toURI();
				}
			}
		} catch (Exception e) {
			String msg = "Error creating URI for managed image source : " + name;
			logger.fine(msg);
			logger.log(Level.FINEST, msg, e);
		}
		try {
			//catch all if can't work out a suitable URI
			return new URI("file://" + uuid);	
		} catch (Exception e) {
			return null;
		}
	}

	@Override
	public String toString() {
		StringBuilder data = new StringBuilder(toURI().toString());
		data.append(" (");
		data.append(type.toString());
		data.append(")");
		return data.toString();
	}
	
	
	
}
