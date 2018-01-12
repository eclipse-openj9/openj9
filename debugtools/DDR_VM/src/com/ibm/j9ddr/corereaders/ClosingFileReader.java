/*******************************************************************************
 * Copyright (c) 2004, 2014 IBM Corp. and others
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
import java.nio.ByteOrder;

import javax.imageio.stream.FileImageInputStream;


/**
 * A wrapper around the functionality that we require from RandomAccessFiles but
 * with the added auto-closing functionality that we require. Since it is not
 * always easy to determine who "owns" a file and we do not want to introduce
 * the concept of state, this class will manage all such file ownership.
 * 
 * We now also extend javax.imageio.stream.ImageInputStreamImpl as a convenient
 * common interface that we can share with the zebedee corefile reader
 * 
 * @author jmdisher
 */
public class ClosingFileReader extends FileImageInputStream
{
	


	public ClosingFileReader(File file) throws IOException, FileNotFoundException
	{
		super(file);
	}

	/**
	 * Create a closing file reader which can close/delete the backing file on
	 * shutdown
	 * 
	 * @param file
	 * @param deleteOnCloseOrExit
	 *            Whether to delete the file when the file is closed or at
	 *            shutdown
	 * @throws FileNotFoundException
	 */
	public ClosingFileReader(final File file, boolean deleteOnCloseOrExit)
			throws IOException, FileNotFoundException
	{
		this(file);
		if (deleteOnCloseOrExit) {
			/*
			 * Ensure this file is closed on shutdown so that it can be deleted
			 * if required.
			 */
			Runtime.getRuntime().addShutdownHook(new Thread() {
				public void run()
				{
					try {
						close();
					} catch (IOException e) {
						// Ignore errors e.g. if file is already closed
					}
				}
			});
		}
	}

	/**
	 * Create a closing file reader with the given endianness which can
	 * close/delete the backing file on shutdown
	 * 
	 * @param file
	 * @param deleteOnCloseOrExit
	 *            Whether to delete the file when the file is closed or at
	 *            shutdown
	 * @param endian
	 *            ther
	 * @throws FileNotFoundException
	 */
	public ClosingFileReader(final File file, ByteOrder endian)
			throws IOException, FileNotFoundException
	{
		this(file, false);
		setByteOrder(endian);
	}
}
