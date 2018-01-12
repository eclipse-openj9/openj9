/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.dtfj.image.j9;

import java.io.File;
import java.io.FileNotFoundException;

/**
 * @author jmdisher
 *
 */
public class DefaultFileLocationResolver implements IFileLocationResolver
{
	private static char[] PATH_DELIMS = new char[]{'/', '\\', '.'};
	private File _supportFileDir;
	
	public DefaultFileLocationResolver(File parentDirectory)
	{
		_supportFileDir = parentDirectory;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.j9.IFileLocationResolver#findFileWithFullPath(java.lang.String)
	 */
	public File findFileWithFullPath(String fullPath)
			throws FileNotFoundException
	{
		/* given that we may be looking for a file which came from another system, look it up in our 
		 * support file dir first (we need that to over-ride the absolute path since it may be looking for 
		 * a file in the same location as one on the local machine - this happens often if moving a core 
		 * file from one Unix system to another).
		 * 
		 * resolver hierarchy = next to, relative to , absolute
		 * 
		 * next to = the library is in the same directory as the core file
		 * relative to = the absolute path of the library in the core is translated to be relative to the current core location
		 * absolute = use the system library
		 * 
		 */
		File absolute = new File(fullPath);
		String fileName = absolute.getName();
		String relativePath = getRelativePath(fullPath);
		File[] resolvers = new File[]{new File(_supportFileDir, fileName), new File(_supportFileDir, relativePath), absolute};
		for(File file : resolvers) {
			if(file.exists()) {
				return file;
			}
		}
		//didn't find this anywhere
		throw new FileNotFoundException(fullPath);
	}

	/*
	 * Differing implementations of java.io.File will potentially behave differently when the 
	 * java.io.File(File, String) constructor is used to create a file based on the parent. If the
	 * String is an absolute path then on Windows it will be treated as relative, but on linux will result
	 * in the absolute path ignoring the parent File parameter
	 */
	private String getRelativePath(String fullPath) {
		if(fullPath.length() == 0) {
			return fullPath;		//empty string so just return it
		}
		if((fullPath.length() > 3) && (fullPath.charAt(1) == ':')) {
			//absolute windows path
			return fullPath.substring(3);
		}
		for(char delim : PATH_DELIMS) {
			if(fullPath.charAt(0) == delim) {
				return fullPath.substring(1);
			}
		}
		return fullPath;
	}
	
}
