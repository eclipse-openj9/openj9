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

import java.io.InputStream;
import java.util.zip.ZipEntry;

import javax.imageio.stream.MemoryCacheImageInputStream;

/**
 * Sub-class of an image input stream that comes from a zip file entry
 * which allows it to provide more information than the standard
 * MemoryCacheImageInputStream such as the length of the stream.
 * 
 * @author adam
 *
 */
public class ZipMemoryCacheImageInputStream extends MemoryCacheImageInputStream {
	private final long length;
	private final ManagedImageSource source;

	public ZipMemoryCacheImageInputStream(InputStream stream) {
		super(stream);
		source = null;
		length = -1;
	}

	public ZipMemoryCacheImageInputStream(ZipEntry entry, InputStream is, ManagedImageSource source) {
		super(is);
		length = entry.getSize();
		this.source = source;
	}

	@Override
	public long length() {
		return length;
	}
	
	public ManagedImageSource getSource() {
		return source;
	}

	/*
	 * WARNING : if you change the way that this string is constructed then you may also
	 * break the ZipFileResolver class in DDR_Core_Readers which is using the output
	 * from this method to determine the zip file without having to create a binding
	 * from DDR to DTFJ.
	 */
	@Override
	public String toString() {
		if(source == null) {
			return super.toString();
		}
		return source.toURI().toString();
	}
	
	
	
}
