/*******************************************************************************
 * Copyright (c) 2005, 2018 IBM Corp. and others
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
package Utilities;

import java.io.File;
import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;

/**
 * @author Matthew Kilner
 */
public class URLClassPathCreator {

	String classpathString;
	
	/**
	 * 
	 */
	public URLClassPathCreator(String cp) {
		super();
		
		classpathString  = prepareString(cp);		
	}
	
	private String prepareString(String string){
		String newString = null;
		newString = string.replace(';',File.pathSeparatorChar);
		return newString;
	}
	
	public URL[] createURLClassPath(){
		int index = 0, count = 0, end = classpathString.length();
		while (index < end) {
			int next = classpathString.indexOf(File.pathSeparatorChar, index);
			if (next == -1) next = end;
			if (next - index > 0) count++;
			index = next + 1;
		}
		URL[] urlPath = new URL[count];
		index = count = 0;
		while (index < end) {
			int next = classpathString.indexOf(File.pathSeparatorChar, index);
			if (next == -1) next = end;
			if (next - index > 0) {
				String path = classpathString.substring(index, next);
				try {
					File f = new File(path);
					path = f.getCanonicalPath();
					if (File.separatorChar != '/')
						path = path.replace(File.separatorChar, '/');
					if (f.isDirectory()) {
						if (!path.endsWith("/"))
							path = new StringBuffer(path.length() + 1).
								append(path).append('/').toString();
					}
					if (!path.startsWith("/"))
						path = new StringBuffer(path.length() + 1).
							append('/').append(path).toString();
					if (!path.startsWith("//")) {
						try {
							urlPath[count++] = new URL("file", null, -1, path, null);
						} catch (MalformedURLException e) {}
						index = next + 1;
						continue;
					}
					path = new StringBuffer(path.length() + 5).
						append("file:").append(path).toString();

					urlPath[count] = new URL(path);
					count++;
				} catch (IOException e) {}
			}
			index = next + 1;
		}
		if (count < urlPath.length) {
			URL[] paths = new URL[count];
			System.arraycopy(urlPath, 0, paths, 0, count);
			urlPath = paths;
		}
		return urlPath;
	}
}
