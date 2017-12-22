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
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.RandomAccessFile;

import javax.imageio.stream.FileImageInputStream;

public class J9FileImageInputStream extends FileImageInputStream {
	private final ManagedImageSource source;
	private final File file;
	private final RandomAccessFile raf;
	
	public J9FileImageInputStream(File f, ManagedImageSource source) throws FileNotFoundException,	IOException {
		super(f);
		this.file = f;
		raf = null;
		this.source = source;
	}

	public J9FileImageInputStream(RandomAccessFile raf, ManagedImageSource source) {
		super(raf);
		this.raf = raf;
		file = null;
		this.source = source;
	}

	public ManagedImageSource getSource() {
		return source;
	}

	public File getFile() {
		return file;
	}

	public RandomAccessFile getRaf() {
		return raf;
	}

	/*
	 * WARNING : if you change the way that this string is constructed then you may also
	 * break the ZipFileResolver class in DDR_Core_Readers which is using the output
	 * from this method to determine the zip file without having to create a binding
	 * from DDR to DTFJ.
	 */
	@Override
	public String toString() {
		if(source.getArchive() != null) {
			return source.getArchive().toURI().toString();
		} else {
			return "file:unknown";
		}
	}
	
}
