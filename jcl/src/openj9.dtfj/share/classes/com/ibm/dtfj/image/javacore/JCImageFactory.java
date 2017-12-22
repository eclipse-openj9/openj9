/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2017 IBM Corp. and others
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
package com.ibm.dtfj.image.javacore;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URI;

import javax.imageio.stream.ImageInputStream;

import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageFactory;
import com.ibm.dtfj.javacore.builder.javacore.ImageBuilderFactory;
import com.ibm.dtfj.javacore.parser.j9.JavaCoreReader;

public class JCImageFactory implements ImageFactory {

	public int getDTFJMajorVersion() {
		return DTFJ_MAJOR_VERSION;
	}

	public int getDTFJMinorVersion() {
		return DTFJ_MINOR_VERSION;
	}

	public Image getImage(final ImageInputStream in, URI sourceID) throws IOException {
		InputStream stream = new InputStream() {

			public int read() throws IOException {
				return in.read();
			}
			
		};
		try {
			Image image = new JavaCoreReader(new ImageBuilderFactory()).generateImage(stream);
			((JCImage) image).setSource(sourceID);
			return image;
		} finally {
			in.close();
		}
	}

	public Image[] getImagesFromArchive(File archive, boolean extract) throws IOException {
		throw new IOException("Not supported for JavaCore files");
	}
	
	public Image getImage(ImageInputStream in, ImageInputStream meta, URI sourceID) throws IOException {
		throw new IOException("Not supported for JavaCore files");
	}

	/**
	 * 
	 */
	public Image getImage(File arg0) throws IOException {
		InputStream stream = new FileInputStream(arg0);
		try {
			Image image = new JavaCoreReader(new ImageBuilderFactory()).generateImage(stream);
			// following cast is safe since examining the code path from generateImage
			// shows that what comes back can only be the JCImage from
			// com.ibm.dtfj.javacore.builder.javacore.ImageBuilder, instance variable _fImage
			((JCImage) image).setSource(arg0.toURI());
			return image;
		} finally {
			stream.close();
		}
	}

	/**
	 * 
	 */
	public Image getImage(File arg0, File arg1) throws IOException {
		throw new IOException("JavaCore does not use metadata files (yet)");
	}

	public int getDTFJModificationLevel() {
		// modification level is VM stream plus DTFJ Java Core tag from RAS_Auto-Build/projects.psf
		return 28001;
	}

}
