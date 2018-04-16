/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package apitesting.datahelper;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.net.MalformedURLException;
import java.net.URL;

import apitesting.Testbase;

import CustomClassloaders.DataCachingClassLoader;

/**
 * Structure of the test data available to these test programs.  Where the same file is mentioned as available from
 * two places, the contents of the file indicate where it came from (so, below, contents of fileone.txt indicate
 * whether it came from dataone or datatwo)
 * 
 * datacaching/dataone.jar
 * 	- contains two text files (fileone.txt, filetwo.txt)
 * 
 * datacaching/datatwo.jar
 *  - contains two text files (fileone.txt, filethree.txt)
 * 
 * Constants are defined in this base class for paths to these files and the expected contents
 */
public class DataCachingTestbase extends Testbase {

	static final String FILEONE = "fileone.txt";
	static final String FILETWO = "filetwo.txt";
	static final String FILETHREE = "filethree.txt";

	static final String CLASSPATH_JARONE = "datacaching/dataone.jar";
	protected static final String CONTENTS_JARONE_FILEONE = "jar one,  file one";
	protected static final String CONTENTS_JARONE_FILETWO = "jar one,  file two";
 
	static final String CLASSPATH_JARTWO = "datacaching/datatwo.jar";
	protected static final String CONTENTS_JARTWO_FILEONE = "jar two, file one";
	protected static final String CONTENTS_JARTWO_FILETHREE = "jar two, file three";

	/**
	 * Retrieves a DataCachingClassLoader instance - this is a classloader that can be 
	 * configured to utilise the sharedcache for any data that it loads.
	 */
	protected DataCachingClassLoader getDataCachingLoader(String[] cp) {
		try {
			URL[] urlcp = new URL[cp.length];
			for (int i = 0; i < cp.length; i++) {
				urlcp[i] = new File(cp[i]).toURL();
			}
			return new DataCachingClassLoader(urlcp,this.getClass().getClassLoader());
		} catch (MalformedURLException e) {
			e.printStackTrace();
			fail("unexpected exception "+e.toString());
		}
		return null;
	}
	
	protected DataCachingClassLoader getDataCachingLoader(String cp) { return getDataCachingLoader(new String[]{cp});}

	/**
	 * Read a string from the supplied data stream and compare it with the expected text, produce 
	 * an error if they don't match.
	 */
	protected void readAndCheck(InputStream dataStream, String expectedContainedText) {
		try {
			if (dataStream==null) fail("should have found the resource but no data retrieved...");
			BufferedInputStream bis = new BufferedInputStream(dataStream);
			byte[] data = new byte[256];
			StringBuffer theData = new StringBuffer();
			int readBytes = -1;
			while ( (readBytes=bis.read(data))!=-1) {
				theData.append(new String(data,0,readBytes));
			}
			if (theData.toString().indexOf(expectedContainedText)==-1) {
				fail("Data from stream '"+theData+"' does not match contain expected data '"+expectedContainedText+"'");
			}
		} catch (IOException e) {
			e.printStackTrace();
			fail("problem reading data stream: "+e.toString());
		}
		
	}

}
