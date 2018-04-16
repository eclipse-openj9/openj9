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

import java.io.InputStream;

import CustomClassloaders.DataCachingClassLoader;

/**
 * Load a pair of resources, from different jars on the classpath.
 */
public class DataCachingTest03 extends DataCachingTestbase {

	public static void main(String[] args) {
		new DataCachingTest03().run();
	}

	public void run() {
		log("simple data caching test");
		DataCachingClassLoader classLoader = getDataCachingLoader(new String[]{CLASSPATH_JARONE,CLASSPATH_JARTWO});
		InputStream dataStream = null;
		
		log("make sure the resources we are testing is not in the cache already");
		dataStream = classLoader.findInCache(FILEONE);
		if (dataStream!=null) fail("'"+FILEONE+"' should not already be in the cache");
		dataStream = classLoader.findInCache(FILETHREE);
		if (dataStream!=null) fail("'"+FILETHREE+"' should not already be in the cache");
		
		log("load the resources, they should get cached");
		classLoader.storeInCache = true;
		dataStream = classLoader.getResourceAsStream(FILEONE);
		readAndCheck(dataStream,CONTENTS_JARONE_FILEONE);
		dataStream = classLoader.getResourceAsStream(FILETHREE);
		readAndCheck(dataStream,CONTENTS_JARTWO_FILETHREE);
		
		log("look in the cache for them");
		dataStream = classLoader.findInCache(FILEONE);
		readAndCheck(dataStream,CONTENTS_JARONE_FILEONE);	
		dataStream = classLoader.findInCache(FILETHREE);
		readAndCheck(dataStream,CONTENTS_JARTWO_FILETHREE);	
		log("test successful");
	}
}
