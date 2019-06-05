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
package com.ibm.dtfj.image;

import java.io.File;
import java.io.IOException;
import java.net.URI;

import javax.imageio.stream.ImageInputStream;

/**
 * <p>This interface is used for classes which can produce instances of Image
 * implementers.</p>
 *
 * <p>Classes that implement this interface should provide a zero argument, public
 * constructor so that they can be created using {@code Class.newInstance()}.</p>
 *
 * e.g.
 *
 * <pre>
 * Image image;
 * try {
 *   Class<?> factoryClass = Class.forName(&quot;com.ibm.dtfj.image.j9.ImageFactory&quot;);
 *   ImageFactory factory = (ImageFactory) factoryClass.newInstance();
 *   image = factory.getImage(new File(coreFileName), new File(xmlFileName));
 * } catch (ClassNotFoundException e) {
 *   System.err.println(&quot;Could not find DTFJ factory class:&quot;);
 *   e.printStackTrace(System.err);
 * } catch (IllegalAccessException e) {
 *   System.err.println(&quot;Could not instantiate DTFJ factory class:&quot;);
 *   e.printStackTrace(System.err);
 * } catch (InstantiationException e) {
 *   System.err.println(&quot;Could not instantiate DTFJ factory class:&quot;);
 *   e.printStackTrace(System.err);
 * } catch (IOException e) {
 *   System.err.println(&quot;Could not find file required for bootstrapping:&quot;);
 *   e.printStackTrace(System.err);
 * }
 * </pre>
 */
public interface ImageFactory {

	/**
	 * The major version number of the current API.
	 */
	public static final int DTFJ_MAJOR_VERSION = 1;

	/**
	 * The minor version number of the current API.
	 */
	public static final int DTFJ_MINOR_VERSION = 12;

	/**
	 * This is the name of the {@code java.util.logging.Logger} subsystem
	 * to which DTFJ passes verbose messages.
	 */
	public static final String DTFJ_LOGGER_NAME = "com.ibm.dtfj.log"; //$NON-NLS-1$

	/**
	 * Creates a new {@code Image} object based on the contents of {@code imageFile}.
	 *
	 * @param imageFile a file with Image information, typically a core file
	 * @return an instance of Image
	 * @throws IOException if there's a problem reading the specified file
	 * @throws NullPointerException if {@code imageFile} is null
	 */
	public Image getImage(File imageFile) throws IOException;

	/**
	 * Creates a new {@code Image} object based on the contents of the given input stream.
	 *
	 * @param in a stream with image information, typically a core file
	 * @param sourceID {@code URI} identifying the source of the image stream
	 * @return an instance of {@code Image}
	 * @throws IOException if there's a problem reading the given stream
	 * @throws UnsupportedOperationException if the factory does not support this method
	 * @throws NullPointerException if either of {@code in} or {@code sourceID} is null
	 * @since 1.10
	 */
	public Image getImage(ImageInputStream in, URI sourceID) throws IOException;

	/**
	 * Creates a new {@code Image} object based on the contents of the given input stream.
	 *
	 * @param in a stream with image information, typically a core file
	 * @param metadata a stream with metadata associated with the image stream
	 * @param sourceID {@code URI} identifying the source of the image stream
	 * @return an instance of {@code Image}
	 * @throws IOException if there's a problem reading the given streams
	 * @throws UnsupportedOperationException if the factory does not support this method
	 * @throws NullPointerException if any of {@code in}, {@code metadata} or {@code sourceID} is null
	 * @since 1.10
	 */
	public Image getImage(ImageInputStream in, ImageInputStream metadata, URI sourceID) throws IOException;

	/**
	 * Creates an array of {@code Image} objects from an archive such as a zip or jar file.
	 *
	 * @param archive which typically contains one or more diagnostic artifacts
	 * @param extract true if the files in the archive should be extracted to a temporary directory
	 * @return an array of {@code Image}s
	 * @throws IOException if there's a problem reading the specified archive
	 * @throws UnsupportedOperationException if the factory does not support this method
	 * @since 1.10
	 */
	public Image[] getImagesFromArchive(File archive, boolean extract) throws IOException;

	/**
	 * Creates a new {@code Image} object based on the contents of {@code imageFile} and {@code metadata}.
	 *
	 * @param imageFile a file with image information, typically a core file
	 * @param metadata an (implementation-defined) file with additional image information (or null if not required)
	 * @return an instance of {@code Image}
	 * @throws IOException if there's a problem reading either of the specified files
	 * @throws NullPointerException if {@code imageFile} is null
	 */
	public Image getImage(File imageFile, File metadata) throws IOException;

	/**
	 * Fetch the DTFJ major version number.
	 *
	 * @return an integer corresponding to the DTFJ API major version number
	 */
	public int getDTFJMajorVersion();

	/**
	 * Fetch the DTFJ minor version number.
	 *
	 * @return an integer corresponding to the DTFJ API minor version number
	 */
	public int getDTFJMinorVersion();

	/**
	 * Fetch the DTFJ modification level.
	 *
	 * @return an integer corresponding to the DTFJ API modification level
	 * @since SDK 6.0 SR1 (DTFJ version 1.2)
	 */
	public int getDTFJModificationLevel();

	/**
	 * If the image is to be created from a core file inside an archive, this property
	 * controls where the file will extracted. If this is not specified then the value of
	 * java.io.tmpdir will be used.
	 * @since 1.10
	 */
	public static final String SYSTEM_PROPERTY_TMPDIR = "com.ibm.java.diagnostic.tmpdir"; //$NON-NLS-1$

}
