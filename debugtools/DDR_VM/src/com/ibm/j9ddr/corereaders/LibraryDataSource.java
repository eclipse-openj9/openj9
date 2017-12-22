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

package com.ibm.j9ddr.corereaders;

import java.io.File;

import javax.imageio.stream.ImageInputStream;

//data source for a library

public class LibraryDataSource {
	public static enum Source {
		NOT_FOUND,
		FILE,
		STREAM
	}
	
	private final Source type;
	private final File file;
	private final ImageInputStream stream;
	private final String name;
	
	public LibraryDataSource(String name) {
		type = Source.NOT_FOUND;
		stream = null;
		file = null;
		this.name = name;
	}
	
	public LibraryDataSource(File file) {
		type = Source.FILE;
		this.file = file;
		stream = null;
		this.name = file.getName();
	}
	
	public LibraryDataSource(String name, ImageInputStream in) {
		type = Source.STREAM;
		this.stream = in;
		file = null;
		this.name = name;
	}

	public Source getType() {
		return type;
	}

	public String getName() {
		return name;
	}
	
	public File getFile() {
		if(type == Source.FILE) {
			return file;
		}
		throw new UnsupportedOperationException("This operation is not supported for data sources of type " + type.name());
	}

	public ImageInputStream getStream() {
		if(type == Source.STREAM) {
			return stream;
		}
		throw new UnsupportedOperationException("This operation is not supported for data sources of type " + type.name());
	}

	@Override
	public String toString() {
		StringBuilder data = new StringBuilder();
		data.append("Library Data Source : ");
		data.append(type.name());
		data.append("\n");
		switch(type) {
			case FILE :
				data.append(file.getAbsolutePath());
				break;
			case STREAM:
				data.append(stream.getClass().getName());
				break;
		}
		return data.toString();
	}
	
	
}
