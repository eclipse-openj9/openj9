/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URI;
import java.net.URISyntaxException;
import java.nio.ByteOrder;
import java.util.zip.ZipException;
import java.util.zip.ZipFile;

import javax.imageio.stream.ImageInputStream;

import com.ibm.dtfj.corereaders.ClosingFileReader;
import com.ibm.dtfj.corereaders.DumpFactory;
import com.ibm.dtfj.corereaders.ICoreFileReader;
import com.ibm.dtfj.corereaders.ResourceReleaser;
import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.utils.file.FileManager;
import com.ibm.jvm.j9.dump.indexsupport.XMLIndexReader;
import com.ibm.jvm.j9.dump.indexsupport.XMLInputStream;

public class DTFJImageFactory implements com.ibm.dtfj.image.ImageFactory
{
	/**
	 * This public constructor is intended for use with Class.newInstance().
	 * This class will generally be referred to by name (e.g. using Class.forName()).
	 * 
	 * @see com.ibm.dtfj.image.ImageFactory
	 *
	 */
	public DTFJImageFactory() {
	}
	
	public Image[] getImagesFromArchive(File archive, boolean extract) throws IOException {
		throw new IOException("Not supported for legacy DTFJ");
	}
	
	/**
	 * Creates a new Image object based on the contents of imageFile
	 * 
	 * @param imageFile a file with Image information, typically a core file but may also be a container such as a zip
	 * @return an instance of Image (null if no image can be constructed from the given file)
	 * @throws IOException
	 */
	public Image getImage(File imageFile) throws IOException
	{
		ReleasingImage image = null;
		//this may be a zip file
		try {
			ZipFile zip = new ZipFile(imageFile);
			ZipExtractionResolver resolver = new ZipExtractionResolver(zip);
			File coreFile = resolver.decompressCoreFile();
			InputStream metaStream = resolver.decompressMetaDataStream();
			image = getImage(coreFile, metaStream, resolver);
			image.addReleasable(resolver);
		} catch (ZipException z) {
			//this is not a zip file so assume that it is pointing at the core component of JExtract output
			return getImage(imageFile, new File(imageFile.toString() + ".xml"));
		}
		return image;
	}
	
	public Image getImage(ImageInputStream in, URI sourceID) throws IOException {
		throw new IOException("Legacy DTFJ requires an XML file to be supplied when creating an image from a stream.");
	}

	public Image getImage(final ImageInputStream in, final ImageInputStream meta, URI sourceID) throws IOException {
		/*
		 * Legacy core readers implement the ImageInputStream interface, but are backed by an implementation which
		 * is based around reading bytes directly from the stream without honouring the ByteOrder setting of the stream.
		 * This has the consequence that the readers expect all streams passes to them to be in BIG_ENDIAN order and
		 * then internally provide wrapper classes when LITTLE_ENDIAN is required. This is why the stream being
		 * passed has it's byte order fixed to BIG_ENDIAN.
		 */
		in.setByteOrder(ByteOrder.BIG_ENDIAN);
		ICoreFileReader core = DumpFactory.createDumpForCore(in);
		XMLIndexReader indexData = new XMLIndexReader();
		//downcast the image input stream to an input stream
		InputStream stream = new InputStream() {

			@Override
			public int read() throws IOException {
				return meta.read();
			}
			
		};
		//CMVC 154851 : pass the metadata stream through the new XML cleanup class
		XMLInputStream xmlstream = new XMLInputStream(stream);
		IFileLocationResolver resolver = null;
		File source = null;
		if((sourceID.getFragment() != null) && (sourceID.getFragment().length() != 0)) {
			//need to strip any fragments from the URI which point to the core in the zip file
			try {
				URI uri = new URI(sourceID.getScheme() + "://" + sourceID.getPath());
				source = new File(uri);
			} catch (URISyntaxException e) {
				//if the URL cannot be constructed then assume it does not point to a file
			}
		} else {
			source = new File(sourceID);
		}
		if(source.exists()) {
			if(FileManager.isArchive(source)) {
				ZipFile zip = new ZipFile(source);
				resolver = new ZipExtractionResolver(zip);
			} else {
				resolver = new DefaultFileLocationResolver(source.getParentFile());
			}
		}
		ReleasingImage image = indexData.parseIndexWithDump(xmlstream, core, in, resolver); 
		meta.close();				//close access to the XML file as we won't be needing it again
		image.addReleasable(core); //because the IIS is used to derive the core reader only the reader needs to be added as a ReleasingResource
		if(image instanceof com.ibm.dtfj.image.j9.Image) {
			((com.ibm.dtfj.image.j9.Image)image).SetSource(sourceID);
		}
		return image;
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
	 * Now that extracting from a zip is handled by the ImageFactory so that DDR and legacy based
	 * implementations can work the same way, this factory cannot tell what type of resolver
	 * to use. To solve this the metadata parameter can now be a zip file which expects the XML
	 * file to have already been extracted next to the core and can use the zip for library resolution.
	 */
	public Image getImage(File imageFile, File metadata) throws IOException {
		FileInputStream metastream = null;
		IFileLocationResolver resolver = null;
		if(FileManager.isArchive(metadata)) {
			//archive from within which libraries should be resolved
			File meta = new File(imageFile.getParentFile(), imageFile.getName() + ".xml");
			metastream = new FileInputStream(meta);
			ZipFile zip = new ZipFile(metadata);
			resolver = new ZipExtractionResolver(zip);
		} else {
			metastream = new FileInputStream(metadata);
			resolver = new DefaultFileLocationResolver(imageFile.getParentFile());
		}
		ReleasingImage image = getImage(imageFile, metastream, resolver);
		metastream.close();
		return image;
	}

	private ReleasingImage getImage(File imageFile, InputStream metadata, IFileLocationResolver resolver) throws IOException
	{
		ClosingFileReader reader = new ClosingFileReader(imageFile);
		ICoreFileReader core = DumpFactory.createDumpForCore(reader);
		XMLIndexReader indexData = new XMLIndexReader();
		//CMVC 154851 : pass the metadata stream through the new XML cleanup class
		XMLInputStream in = new XMLInputStream(metadata);
		ReleasingImage image = indexData.parseIndexWithDump(in, core, reader, resolver);
		image.addReleasable(in);
		image.addReleasable(reader);
		image.addReleasable(core);
		if(resolver instanceof ResourceReleaser) {
			image.addReleasable((ResourceReleaser) resolver);
		}
		return image;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageFactory#getDTFJMajorVersion()
	 */
	public int getDTFJMajorVersion() {
		return DTFJ_MAJOR_VERSION;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageFactory#getDTFJMinorVersion()
	 */
	public int getDTFJMinorVersion() {
		return DTFJ_MINOR_VERSION;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageFactory#getDTFJModificationLevel()
	 */
	public int getDTFJModificationLevel() {
		 
		int buildNumber= 0;
		try {
			buildNumber=Integer.parseInt(ImageFactory.class.getPackage().getImplementationVersion());
		} catch(NumberFormatException nfe) {
			buildNumber=-1;
		}
		return buildNumber;
	}

}
