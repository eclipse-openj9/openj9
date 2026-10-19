/*[INCLUDE-IF JAVA_SPEC_VERSION >= 8]*/
/*
 * Copyright IBM Corp. and others 2004
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package com.ibm.dtfj.image.j9;

import java.io.File;
import java.io.IOException;
import java.net.URI;

import javax.imageio.stream.ImageInputStream;

import com.ibm.dtfj.image.Image;

public class DTFJImageFactory implements com.ibm.dtfj.image.ImageFactory {

	private static IOException notSupported() throws IOException {
		throw new IOException("Legacy DTFJ with XML metadata not supported.");
	}

	/**
	 * This public constructor is intended for use with Class.newInstance().
	 * This class will generally be referred to by name (e.g. using Class.forName()).
	 *
	 * @see com.ibm.dtfj.image.ImageFactory
	 */
	public DTFJImageFactory() {
	}

	@Override
	public Image[] getImagesFromArchive(File archive, boolean extract) throws IOException {
		throw notSupported();
	}

	/**
	 * Creates a new Image object based on the contents of imageFile
	 *
	 * @param imageFile a file with Image information, typically a core file but may also be a container such as a zip
	 * @return an instance of Image (null if no image can be constructed from the given file)
	 * @throws IOException
	 */
	@Override
	public Image getImage(File imageFile) throws IOException
	{
		throw notSupported();
	}

	@Override
	public Image getImage(ImageInputStream in, URI sourceID) throws IOException {
		throw notSupported();
	}

	@Override
	public Image getImage(final ImageInputStream in, final ImageInputStream meta, URI sourceID) throws IOException {
		throw notSupported();
	}

	/**
	 * Creates a new Image object based on the contents of imageFile and metadata
	 *
	 * @param imageFile a file with Image information, typically a core file
	 * @param metadata a file with additional Image information. This is an implementation defined file
	 * @return an instance of Image
	 * @throws IOException
	 */
	@Override
	public Image getImage(File imageFile, File metadata) throws IOException {
		throw notSupported();
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
		int buildNumber;
		try {
			buildNumber = Integer.parseInt(ImageFactory.class.getPackage().getImplementationVersion());
		} catch (NumberFormatException nfe) {
			buildNumber = -1;
		}
		return buildNumber;
	}

}
