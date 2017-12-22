/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
package com.ibm.uma.util;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;

import com.ibm.uma.UMAException;

public class FileAssistant {
	String filename;
	StringBuffer buffer;
	
	public FileAssistant(String filename, StringBuffer buffer) {
		this.filename = filename;
		this.buffer = buffer;
	}
	
	public FileAssistant(String filename) {
		this.filename = filename;
		this.buffer = new StringBuffer();
	}
	
	public StringBuffer getStringBuffer() {
		return buffer;
	}
	
	private boolean differentFromCopyOnDisk() throws UMAException {
		File file = new File(filename);
		if (!file.exists()) return true;
		StringBuffer fileBuffer;
		try {
			FileReader fr = new FileReader(file);
			char []charArray = new char[1024];
			fileBuffer = new StringBuffer();
			int numRead;
			while ( (numRead = fr.read(charArray)) != -1 ) {
				fileBuffer.append(charArray, 0, numRead);
			}
			if ( buffer.toString().equals(fileBuffer.toString()) ) {
				fr.close();
				return false;
			}
			fr.close();
		} catch (FileNotFoundException e) {
			throw new UMAException(e);
		} catch (IOException e) {
			throw new UMAException(e);
		}
		return true;
	}
	
	public void writeToDisk() throws UMAException {
		if ( !differentFromCopyOnDisk() ) {
			Logger.getLogger().println(Logger.InformationL2Log, "\tskipped writing [same as on file system]: " + filename);
			return;
		}
		try {
			File file = new File(filename);
			file.delete();
			FileWriter fw = new FileWriter(filename);
			fw.write(buffer.toString());
			fw.close();
		} catch (IOException e) {
			throw new UMAException(e);
		}
		Logger.getLogger().println(Logger.InformationL1Log, "\twrote: " + filename);
	}
}
