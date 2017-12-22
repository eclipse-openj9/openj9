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
package com.ibm.j9ddr.view.dtfj.image;

import static com.ibm.j9ddr.logging.LoggerNames.LOGGER_VIEW_DTFJ;

import java.io.File;
import java.io.IOException;
import java.net.URI;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.imageio.stream.FileImageInputStream;
import javax.imageio.stream.ImageInputStream;

import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageFactory;
import com.ibm.dtfj.utils.file.FileManager;
import com.ibm.dtfj.utils.file.ImageSourceType;
import com.ibm.dtfj.utils.file.J9FileImageInputStream;
import com.ibm.dtfj.utils.file.ManagedImageSource;
import com.ibm.j9ddr.corereaders.CoreReader;
import com.ibm.j9ddr.corereaders.ICore;
import com.ibm.j9ddr.util.WeakValueMap;

public class J9DDRImageFactory implements ImageFactory {

	public static final int DTFJ_MAJOR_VERSION = 1;
	public static final int DTFJ_MINOR_VERSION = 11;
	public static final int DTFJ_MODIFICATION_LEVEL = 28001;
	
	public int getDTFJMajorVersion() {
		return DTFJ_MAJOR_VERSION;
	}

	public int getDTFJMinorVersion() {
		return DTFJ_MINOR_VERSION;
	}

	public int getDTFJModificationLevel() {
		return DTFJ_MODIFICATION_LEVEL;
	}

	public Image[] getImagesFromArchive(File archive, boolean extract) throws IOException {
		return new Image[]{};
	}

	public Image getImage(ImageInputStream in, ImageInputStream meta, URI sourceID) throws IOException {
		// Divert the core readers logging to the ddr logger.
		Logger coreLogger = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);
		Logger ddrLogger = Logger.getLogger(LOGGER_VIEW_DTFJ);
		coreLogger.setParent(ddrLogger);
		if (ddrLogger.getLevel() == null && coreLogger.getLevel() == null) {
			// This blocks warning or below messages from the core readers
			// unless logging has been manually configured via a properties
			// file.
			// To re-enable logging just from the core readers set:
			// j9ddr.core_readers.level = <level>
			// in a logging.properties file.
			coreLogger.setLevel(Level.SEVERE);
		}
		
		ICore reader = CoreReader.readCoreFile(in);
		
		if (reader == null) {
			return null;
		}
		
		
		return new J9DDRImage(sourceID, reader, meta);
	}

	/**
	 * Map of images already available in the image. Some of the core caching uses a lot of memory - so we don't want 
	 * to duplicate that data in memory if we can avoid it.
	 */
	private static WeakValueMap<File, J9DDRImage> imageMap = new WeakValueMap<File, J9DDRImage>();
	
	public Image getImage(File core) throws IOException {
		J9DDRImage cachedImage = imageMap.get(core.getAbsoluteFile());
		
		if (cachedImage != null) {
			if(!cachedImage.isClosed()) {
				/*
				 * Only return the cached Image if it has not been closed. If it has
				 * allow a new one to be created which will then be cached replacing this
				 * closed entry.
				 */
				return cachedImage;
			}
		}
		
		// Divert the core readers logging to the ddr logger.
		Logger coreLogger = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);
		Logger ddrLogger = Logger.getLogger(LOGGER_VIEW_DTFJ);
		coreLogger.setParent(ddrLogger);
		if (ddrLogger.getLevel() == null && coreLogger.getLevel() == null) {
			// This blocks warning or below messages from the core readers
			// unless logging has been manually configured via a properties
			// file.
			// To re-enable logging just from the core readers set:
			// j9ddr.core_readers.level = <level>
			// in a logging.properties file.
			coreLogger.setLevel(Level.SEVERE);
		}
		
		ICore reader = CoreReader.readCoreFile(core.getPath());
		
		if (reader == null) {
			return null;
		}
		
		cachedImage = new J9DDRImage(core.toURI(), reader, null);
		
		imageMap.put(core.getAbsoluteFile(), cachedImage);
		
		return cachedImage; 
	}
	
	public Image getImage(ImageInputStream in, URI sourceID) throws IOException {
		return getImage(in, null, sourceID);
		
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageFactory#getImage(java.io.File, java.io.File)
	 * J9DDR does not require the jextract xml metadata and so this is ignored
	 */
	public Image getImage(File core, File metadata) throws IOException {
		if(FileManager.isArchive(metadata)) {
			//if the metadata is a zip file, construct a managed source to allow libraries to be resolved from within the zip
			ManagedImageSource source = new ManagedImageSource(core.getName(), ImageSourceType.CORE);
			source.setArchive(metadata);
			source.setExtractedTo(core);
			source.setPath(core.getAbsolutePath());
			FileImageInputStream in = new J9FileImageInputStream(core, source);
			return getImage(in, null, source.getURIOfExtractedFile());
		} else {
			return getImage(core);
		}
	}

}
