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

package com.ibm.j9ddr.libraries;

import java.io.Serializable;

//entry in the footer which corresponds to a library

public class FooterLibraryEntry implements Serializable {
	private static final long serialVersionUID = -7954132728424146386L;
	private final String path;
	private final String name;
	private final int size;
	private final long start;
	
	public FooterLibraryEntry(String path, String name, int size, long start) {
		this.path = new String(path);
		this.name = new String(name);
		this.size = size;
		this.start = start;
	}
	
	public String getPath() {
		return new String(path);
	}
		
	public int getSize() {
		return size;
	}
	
	public long getStart() {
		return start;
	}
	
	public String getName() {
		return name;
	}

	@Override
	public String toString() {
		StringBuilder data = new StringBuilder();
		data.append("\tPath : ");
		data.append(path);
		data.append("\n\t\tName : ");
		data.append(name);
		data.append("\n\t\tSize : ");
		data.append(size);
		data.append("\n\t\tStart : ");
		data.append(start);
		data.append('\n');
		return data.toString();
	}
	
	
}
