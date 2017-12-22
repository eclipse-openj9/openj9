/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2012, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.tools.utils;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.UnsupportedEncodingException;

public class FileUtils {

	/**
	 * To read a file.
	 * <p>
	 * @param cmdFile	The command file.
	 * @param charset	If not null, the charset is used to decode the strings in the file.
	 * <p>
	 * @return	content of the file.
	 * <p>
	 * @throws UnsupportedEncodingException		
	 * @throws IOException
	 */
	public static String [] read(File file) throws IOException, UnsupportedEncodingException {
		return read(file, null);
	}
	
	/**
	 * To read a file.
	 * <p>
	 * @param cmdFile	The command file.
	 * @param charset	If not null, the charset is used to decode the strings in the file.
	 * <p>
	 * @return	content of the file.
	 * <p>
	 * @throws UnsupportedEncodingException		
	 * @throws IOException
	 */
	public static String [] read(File file, String charset) throws IOException, UnsupportedEncodingException {
		if(!file.exists() || !file.isFile()) {
			return null;
		}
		if(file.length() > Integer.MAX_VALUE) {
			return null;
		}
		byte[] buffer = new byte[(int)file.length()];
		FileInputStream in = null;
		try {
			in = new FileInputStream(file);
			in.read(buffer);
		} finally {
			if(in != null) {
				in.close();
			}
		}
		return (charset == null ? new String(buffer) : new String(buffer, charset)).split("\n"); 
		// TODO needs to test "\n" if this works on zOS.
	}
}
