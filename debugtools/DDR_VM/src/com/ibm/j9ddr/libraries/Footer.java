/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

import java.io.File;
import java.io.Serializable;
import java.util.ArrayList;

//enclosing class for the footer

public class Footer implements Serializable {
	private static final long serialVersionUID = -8257413568391677575L;
	private final int version = 1;
	private final FooterLibraryEntry[] entries;
	private int index = 0;
	private ArrayList<String> errorMessages = new ArrayList<String>();
	
	public Footer(int size) {
		entries = new FooterLibraryEntry[size];
	}
	
	public int getVersion() {
		return version;
	}
	
	public void addErrorMessage(String message) {
		errorMessages.add(message);
	}
	
	public String[] getErrorMessages() {
		String[] messages = new String[errorMessages.size()];
		return errorMessages.toArray(messages);
	}
	
	public void addEntry(String path, int size, long start) {
		int pos = path.lastIndexOf(File.separatorChar);
		String name = (pos == -1) ? path : path.substring(pos + 1);
		FooterLibraryEntry entry = new FooterLibraryEntry(path, name, size, start);
		entries[index++] = entry;
	}

	@Override
	public String toString() {
		StringBuilder data = new StringBuilder();
		data.append("Footer : version ");
		data.append(version);
		data.append("\n");
		for(int i = 0; i < entries.length; i++) {
			if(entries[i] != null) {		//skip over entries that have not been initialized
				data.append(entries[i].toString());
			}
		}
		return data.toString();
	}
	
	public FooterLibraryEntry findEntry(String path) {
		FooterLibraryEntry namematch = null;			//match against the name  
		for(int i = 0; i < entries.length; i++) {
			if(entries[i] != null) {		//skip over entries that have not been initialized
				if(entries[i].getPath().equals(path)) {
					return entries[i];
				}
				if(entries[i].getName().equals(path)) {
					namematch = entries[i];
				}
			}
		}
		return namematch;					//return null or a name match if one was found
	}
	
	public FooterLibraryEntry[] getEntries() {
		return entries;
	}
	
}
