/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2019 IBM Corp. and others
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
package com.ibm.dtfj.image.j9;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FilenameFilter;
import java.io.IOException;
import java.lang.reflect.Method;
import java.net.MalformedURLException;
import java.net.URI;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Objects;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.imageio.stream.ImageInputStream;

import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.utils.ManagedImage;
import com.ibm.dtfj.utils.file.CompressedFileManager;
import com.ibm.dtfj.utils.file.FileManager;
import com.ibm.dtfj.utils.file.ImageSourceType;
import com.ibm.dtfj.utils.file.J9FileImageInputStream;
import com.ibm.dtfj.utils.file.ManagedImageSource;
import com.ibm.dtfj.utils.file.MultipleCandidateException;

/*[IF Sidecar19-SE]*/
import jdk.internal.module.Modules;
/*[ENDIF] Sidecar19-SE*/

public class ImageFactory implements com.ibm.dtfj.image.ImageFactory {

	static final class ImageReference {
		Image image;
	}

	private static final String FACTORY_DTFJ = "com.ibm.dtfj.image.j9.DTFJImageFactory"; //$NON-NLS-1$
	private static final String FACTORY_DDR = "com.ibm.j9ddr.view.dtfj.image.J9DDRImageFactory"; //$NON-NLS-1$
	private final ArrayList<Exception> exceptions = new ArrayList<>();
	// log exceptions that occur when trying to find the correct image factory
	private final Logger log = Logger.getLogger(DTFJ_LOGGER_NAME);
	private ClassLoader imageFactoryClassLoader;
	private File tmpdir = null; // the directory which holds any extracted files

	/*[IF Sidecar19-SE]*/
	static {
		/* 
		 * Even though j9ddr.jar is a resource within the module openj9.dtfj,
		 * the classes contained within appear at runtime to be in the un-named module.
		 * The following programmatically exports jdk.internal.org.objectweb.asm
		 * from java.base and certain packages from openj9.dtfj to the un-named
		 * module via reflection APIs for use by j9ddr.jar.
		 */
		try {
			Module baseModule = String.class.getModule();

			Modules.addExportsToAllUnnamed(baseModule, "jdk.internal.org.objectweb.asm"); //$NON-NLS-1$

			Module thisModule = ImageFactory.class.getModule();

			Modules.addExportsToAllUnnamed(thisModule, "com.ibm.dtfj.image.j9"); //$NON-NLS-1$
			Modules.addExportsToAllUnnamed(thisModule, "com.ibm.dtfj.utils.file"); //$NON-NLS-1$
			Modules.addExportsToAllUnnamed(thisModule, "com.ibm.java.diagnostics.utils"); //$NON-NLS-1$
			Modules.addExportsToAllUnnamed(thisModule, "com.ibm.java.diagnostics.utils.commands"); //$NON-NLS-1$
		} catch (Exception e) {
			throw new InternalError("Failed to adjust module exports", e); //$NON-NLS-1$
		}
	}
	/*[ENDIF] Sidecar19-SE*/
	
	/**
	 * This public constructor is intended for use with Class.newInstance().
	 * This class will generally be referred to by name (e.g. using Class.forName()).
	 * 
	 * @see com.ibm.dtfj.image.ImageFactory
	 */
	public ImageFactory() {
		// when the vm shuts down we need to make sure that all our extracted files are deleted
	}

	@Override
	public Image[] getImagesFromArchive(File archive, boolean extract) throws IOException {
		Objects.requireNonNull(archive);
		if (!FileManager.fileExists(archive)) {
			throw new FileNotFoundException("Archive '" + archive.getAbsolutePath() + "' not found."); //$NON-NLS-1$ //$NON-NLS-2$
		}
		if (!FileManager.isArchive(archive)) {
			throw new IOException("The specified archive " + archive.getAbsolutePath() + " was not recognised"); //$NON-NLS-1$ //$NON-NLS-2$
		}
		ArrayList<Image> images = new ArrayList<>(); // images created from this zip file
		exceptions.clear();
		//the cast to an ArchiveFileManager is safe as we've already checked earlier with FileManager.isArchive
		CompressedFileManager manager = (CompressedFileManager) FileManager.getManager(archive);
		List<ManagedImageSource> sources = manager.getImageSources();
		if (extract) { //create the temporary directory in preparation for the extraction
			File parent = getTempDirParent();
			tmpdir = FileManager.createTempDir(parent);
		}
		for (ManagedImageSource source : sources) {
			ImageInputStream corestream = null;
			ImageInputStream metastream = null;
			if (extract) { //extract the files to disk
				manager.extract(source, tmpdir);
				File coreFile = new File(source.getPathToExtractedFile());
				corestream = new J9FileImageInputStream(coreFile, source);
				if (source.hasMetaData()) {
					manager.extract(source.getMetadata(), tmpdir);
					File metaFile = new File(source.getMetadata().getPathToExtractedFile());
					metastream = new J9FileImageInputStream(metaFile, source.getMetadata());
				}
			} else { //run from in memory
				corestream = manager.getStream(source);
				if (source.hasMetaData()) {
					metastream = manager.getStream(source.getMetadata());
				}
			}
			try {
				// Try each factory listed for this source file type, checking to see if we find a JRE
				ImageReference imageReference = new ImageReference();
				for (String factory : source.getType().getFactoryNames()) {
					if (foundRuntimeInImage(factory, imageReference, source.toURI(), corestream, metastream)) {
						if (imageReference.image instanceof ManagedImage) {
							((ManagedImage) imageReference.image).setImageSource(source);
						}
						images.add(imageReference.image);
						break; //quit looking for an image
					}
					imageReference.image = null;
				}
				// Final fallback is to get a native-only DDR Image (no JRE), if the source is a core dump 
				if (imageReference.image == null && source.getType().equals(ImageSourceType.CORE)) {
					com.ibm.dtfj.image.ImageFactory factory = createImageFactory(FACTORY_DDR);
					if (factory != null) {
						imageReference.image = factory.getImage(corestream, source.toURI());
						if (imageReference.image != null) {
							images.add(imageReference.image);
						}
					}
				}
			} catch (Exception e) {
				//log exception and attempt to carry on creating images
				exceptions.add(e);
			}
		}
		printExceptions();
		return images.toArray(new Image[images.size()]);
	}
	
	/**
	 * Creates a new Image object based on the contents of imageFile
	 * 
	 * @param imageFile a file with Image information, typically a core file but may also be a container such as a zip
	 * @return an instance of Image (null if no image can be constructed from the given file)
	 * @throws IOException
	 */
	@Override
	public Image getImage(File imageFile) throws IOException {
		Objects.requireNonNull(imageFile);
		ImageReference imageReference = new ImageReference();
		if (!FileManager.fileExists(imageFile)) {
			throw new FileNotFoundException("Image file '" + imageFile.getAbsolutePath() + "' not found."); //$NON-NLS-1$ //$NON-NLS-2$
		}
		exceptions.clear();
		FileManager manager = FileManager.getManager(imageFile);
		List<ManagedImageSource> candidates = manager.getImageSources();
		ManagedImageSource source = null;
		File core = null;
		File meta = null;
		if (FileManager.isArchive(imageFile)) {
			boolean foundCoreFile = false; //flag to indicate that a core file has been found
			CompressedFileManager archiveManager = (CompressedFileManager) manager;
			for (ManagedImageSource candidate : candidates) {
				if (candidate.getType().equals(ImageSourceType.CORE)) {
					if (foundCoreFile) {
						//for legacy behaviour compatibility this can only return 1 core when invoked this way
						throw new MultipleCandidateException(candidates, imageFile);
					}
					source = candidate; //note the core file
					foundCoreFile = true;
				}
			}
			if (foundCoreFile) {
				//if a single core file has been located then extract it and any associated meta data into a temp directory
				File parent = getTempDirParent();
				tmpdir = FileManager.createTempDir(parent);
				archiveManager.extract(source, tmpdir);
				core = source.getExtractedTo();
				if (source.hasMetaData()) {
					archiveManager.extract(source.getMetadata(), tmpdir);
					if (source.getType().equals(ImageSourceType.CORE)) {
						meta = imageFile; //when extracting from a zip the archive itself is now the metadata file
					} else {
						meta = source.getMetadata().getExtractedTo();
					}
				}
			} else {
				throw new IOException("No core files were detected in " + imageFile.getAbsolutePath()); //$NON-NLS-1$
			}
		} else {
			if (candidates.size() > 1) {
				//for backwards behavioural compatibility only one image is allowed to be returned from the supplied file
				throw new MultipleCandidateException(candidates, imageFile);
			}
			source = candidates.get(0);
			core = new File(source.getPath());
			if (source.hasMetaData()) {
				//the presence of a metadata file means that this is not a z/OS dataset and so a FileImageInputStream is safe
				meta = new File(source.getMetadata().getPath());
			}
		}
		for (String factory : source.getType().getFactoryNames()) {
			if (foundImage(factory, imageReference, core, meta)) {
				if (imageReference.image instanceof ManagedImage) {
					((ManagedImage) imageReference.image).setImageSource(source);
				}
				return imageReference.image;
			}
		}

		printExceptions();
		//the final fallback is to try and get just the native aspect if the file is a core file
		if (source.getType().equals(ImageSourceType.CORE)) {
			com.ibm.dtfj.image.ImageFactory f = createImageFactory(FACTORY_DDR);
			//no valid runtime so return DDR factory for use with Image API
			if (null == f) {
				throw propagateIOException("Could not create a valid ImageFactory"); //$NON-NLS-1$
			}
			return f.getImage(imageFile);
		} else {
			throw new IOException("The file " + imageFile.getAbsolutePath() + " was not recognised by any reader"); //$NON-NLS-1$ //$NON-NLS-2$
		}
	}

	/**
	 * Creates a new Image object based on the contents of input stream
	 * 
	 * @param imageFile a file with Image information, typically a core file but may also be a container such as a zip
	 * @return an instance of Image (null if no image can be constructed from the given file)
	 * @throws IOException
	 */
	@Override
	public Image getImage(ImageInputStream in, URI sourceID) throws IOException {
		return getImage(in , null, sourceID);
	}
	
	/**
	 * Creates a new Image object based on the contents of input stream
	 * 
	 * @param imageFile a file with Image information, typically a core file but may also be a container such as a zip
	 * @return an instance of Image (null if no image can be constructed from the given file)
	 * @throws IOException
	 */
	@Override
	public Image getImage(ImageInputStream in, ImageInputStream meta, URI sourceID) throws IOException {
		ImageReference imageReference = new ImageReference();
		com.ibm.dtfj.image.ImageFactory factory = null;
		exceptions.clear();
		//when being passed a stream directly, need to go through all possible image factories looking for
		//TODO - need to go through all available image factories rather than try and decompose the URI
		if (foundRuntimeInImage(FACTORY_DDR, imageReference, sourceID, in, meta)) { //try DDR
			return imageReference.image;
		}
		if (foundRuntimeInImage(FACTORY_DTFJ, imageReference, sourceID, in, meta)) { //try legacy DTFJ
			return imageReference.image;
		}
		factory = createImageFactory(FACTORY_DDR);
		//no valid runtime so return DDR factory for use with Image API
		printExceptions();
		if (null == factory) {
			throw propagateIOException("Could not create a valid ImageFactory"); //$NON-NLS-1$
		}
		return factory.getImage(in, meta, sourceID);
	}

	private static File getTempDirParent() {
		String tmpdir = System.getProperty(SYSTEM_PROPERTY_TMPDIR);
		if (tmpdir == null) {
			//hasn't been overridden so return value for java.io.tmpdir
			return new File(System.getProperty("java.io.tmpdir")); //$NON-NLS-1$
		} else {
			return new File(tmpdir);
		}
	}
	
	/**
	 * Creates a new Image object based on the contents of imageFile and metadata
	 * 
	 * @param imageFile a file with Image information, typically a core file
	 * @param metadata a file with additional Image information. This is an implementation defined file
	 * @return an instance of Image
	 * @throws IOException
	 */
	/*
	 * This is left as a separate implementation to provide a route by which legacy behaviour
	 * can be invoked first as all other Image creation mechanisms will try DDR first.
	 */
	@Override
	public Image getImage(File imageFile, File metadata) throws IOException {
		Objects.requireNonNull(imageFile);
		if (metadata != null && !metadata.exists()) {
			//xml file is not supported as a dataset, so the File.exists() check is fine
			throw new FileNotFoundException("Metadata file '" + metadata.getAbsolutePath() + "' not found."); //$NON-NLS-1$ //$NON-NLS-2$
		}
		if (!FileManager.fileExists(imageFile)) {
			throw new FileNotFoundException("Image file '" + imageFile.getAbsolutePath() + "' not found."); //$NON-NLS-1$ //$NON-NLS-2$
		}
		ImageReference imageReference = new ImageReference();
		com.ibm.dtfj.image.ImageFactory factory = null;
		exceptions.clear();
		if (null != metadata) { //metadata XML file specified
			if (foundImage(FACTORY_DTFJ, imageReference, imageFile, metadata)) { //try legacy DTFJ
				return imageReference.image;
			}
			if (foundImage(FACTORY_DDR, imageReference, imageFile, metadata)) { //try DDR
				return imageReference.image;
			}
		} else {
			if (foundImage(FACTORY_DDR, imageReference, imageFile, null)) { //try DDR
				return imageReference.image;
			}
			if (foundImage(FACTORY_DTFJ, imageReference, imageFile, null)) { //try legacy DTFJ
				return imageReference.image;
			}
		}
		//no valid runtime so return DDR factory for use with Image API
		factory = createImageFactory(FACTORY_DDR);
		printExceptions();
		if (null == factory) {
			throw propagateIOException("Could not create a valid ImageFactory"); //$NON-NLS-1$
		}
		return factory.getImage(imageFile);
	}

	/**
	 * Output any exception messages encountered during processing and the full exception to the
	 * logger
	 */
	private void printExceptions() {
		if (0 == exceptions.size()) {
			return; //nothing went wrong
		}
		log.fine("Warning : errors encountered whilst creating ImageFactory"); //$NON-NLS-1$
		for (int i = 0; i < exceptions.size(); i++) {
			Object obj = exceptions.get(i);
			if (obj instanceof Exception) {
				Exception e = (Exception) obj;
				log.log(Level.FINE, e.getMessage(), e); //write to log if turned on
			}
		}
	}

	/**
	 * Throw an IOException unless we have already hit one and only one while
	 * trying to open a dump in which case throw that to give the caller a
	 * bit more information.
	 * 
	 * This method will always throw an IOException and never returns normally.
	 * 
	 * @param newExceptionMessage
	 * @throws IOException
	 */
	private IOException propagateIOException(String newExceptionMessage) throws IOException {
		if (exceptions.size() == 1 && exceptions.get(0) instanceof IOException) {
			throw (IOException) exceptions.get(0);
		} else {
			throw new IOException(newExceptionMessage);
		}
	}

	/**
	 * Attempt to create an image and check for the presence of a Java runtime
	 * @param imageFile
	 * @param metadata
	 * @return true if this image should be used as a return value
	 */
	private boolean foundImage(String className, ImageReference imageReference, File imageFile, File metadata) {
		try {
			com.ibm.dtfj.image.ImageFactory _factory = createImageFactory(className);
			if (null == _factory) {
				return false;
			}
			imageReference.image = (metadata == null) ? _factory.getImage(imageFile) : _factory.getImage(imageFile,
					metadata);
			boolean foundRuntime = hasJavaRuntime(imageReference);
			if (!foundRuntime && (imageReference.image != null)) {
				//close and release resources used whilst detecting the image
				imageReference.image.close();
			}
			return foundRuntime;
		} catch (Exception e) {
			exceptions.add(e); //log the exception
		}
		return false; //return unable to create image
	}
	
	/**
	 * Attempt to create an image and check for the presence of a Java runtime
	 * @param imageFile
	 * @param metadata
	 * @return true if this image should be used as a return value
	 */
	private boolean foundRuntimeInImage(String className, ImageReference imageReference, URI source, ImageInputStream in, ImageInputStream metadata) {
		try {
			com.ibm.dtfj.image.ImageFactory _factory = createImageFactory(className);
			if (null == _factory) {
				return false;
			}
			imageReference.image = (metadata == null) ? _factory.getImage(in, source) : _factory.getImage(in, metadata, source);
			return hasJavaRuntime(imageReference);
		} catch (Exception e) {
			exceptions.add(e); //log the exception
		}
		return false; //return unable to create image
	}

	private com.ibm.dtfj.image.ImageFactory createImageFactory(String className) {
		if (imageFactoryClassLoader == null) {
			initClassLoader();
		}

		try {
			Class<?> clazz = Class.forName(className, true, imageFactoryClassLoader);
			Object obj = clazz.newInstance();
			if (obj instanceof com.ibm.dtfj.image.ImageFactory) {
				return (com.ibm.dtfj.image.ImageFactory) obj;
			}
		} catch (Exception e) {
			exceptions.add(e);
		}
		return null;
	}

	/**
	 * Checks to see if the supplied image has an available DTFJ Java Runtime. This is to force the parsing of the jextract XML
	 * for legacy DTFJ and blob parsing for DDR. It does not make any guarantees about the quality and availability of the 
	 * run time data.
	 * @return true if a non-corrupt runtime is returned
	 */
	private static boolean hasJavaRuntime(ImageReference imageReference) {
		if (null == imageReference || null == imageReference.image) {
			return false;
		}
		Iterator<?> spaces = imageReference.image.getAddressSpaces();
		while ((null != spaces) && spaces.hasNext()) { //search address spaces
			Object obj = spaces.next();
			if ((null != obj) && (obj instanceof ImageAddressSpace)) {
				ImageAddressSpace space = (ImageAddressSpace) obj;
				Iterator<?> procs = space.getProcesses();
				while ((null != procs) && procs.hasNext()) { //search processes
					Object procobj = procs.next();
					if ((null != procobj) && (procobj instanceof ImageProcess)) {
						ImageProcess proc = (ImageProcess) procobj;
						Iterator<?> runtimes = proc.getRuntimes();
						while ((null != runtimes) && runtimes.hasNext()) {
							Object rtobj = runtimes.next();
							if ((null != rtobj) && (rtobj instanceof JavaRuntime)) {
								return true; //found a non-corrupt java runtime
							}
						}
					}
				}
			}
		}
		return false;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageFactory#getDTFJMajorVersion()
	 */
	@Override
	public int getDTFJMajorVersion() {
		return DTFJ_MAJOR_VERSION;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageFactory#getDTFJMinorVersion()
	 */
	@Override
	public int getDTFJMinorVersion() {
		return DTFJ_MINOR_VERSION;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageFactory#getDTFJModificationLevel()
	 */
	@Override
	public int getDTFJModificationLevel() {
		// modification level is VM stream plus historically the tag from RAS_Auto-Build/projects.psf - now just a number
		return 29003;
	}

	private void initClassLoader() {
		// add all JAR files in the DDR directory to class path
		// use property com.ibm.java.diagnostics.ddr.home if set, otherwise {java.home}/lib/ddr
		File ddrDir = null;
		String ddrHome = System.getProperty("com.ibm.java.diagnostics.ddr.home"); //$NON-NLS-1$
		if (ddrHome != null) {
			ddrDir = new File(ddrHome);
		} else {
			ddrDir = new File(System.getProperty("java.home", ""), "lib/ddr"); //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
		}

		if (ddrDir.isDirectory()) {
			File[] jarFiles = null;
			FilenameFilter jarFilter = new JarFilter();
			jarFiles = ddrDir.listFiles(jarFilter);
			URL[] urls = new URL[jarFiles.length];
			for (int i = 0; i < jarFiles.length; i++) {
				try {
					urls[i] = jarFiles[i].toURI().toURL();
				} catch (MalformedURLException e) {
					exceptions.add(e);
				}
			}
			imageFactoryClassLoader = new URLClassLoader(urls, getClass().getClassLoader());
		} else {
			// Don't leave this as null as null is the bootstrap class loader.
			imageFactoryClassLoader = getClass().getClassLoader();
		}
	}

	private static final class JarFilter implements FilenameFilter {

		JarFilter() {
			super();
		}

		@Override
		public boolean accept(File dir, String name) {
			// accept JAR files *.jar only
			return name.regionMatches(true, name.length() - 4, ".jar", 0, 4); //$NON-NLS-1$
		}

	}

}
