/*******************************************************************************
 * Copyright (c) 2010, 2019 IBM Corp. and others
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

package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;
import java.io.File;

/**
 * This test checks for the content of the zipcache in the shared cache.
 * It creates a persistent shared cache and runs
 * java -Xshareclasses:printstats=zipcache
 * on the shared cache.
 * The list of boot strap jar is got from the system property sun.boot.class.path
 * and we check that every file in the sun.boot.class.path is found in the
 * shared cache
 * 
 * @author jeanpb
 */
public class TestZipCacheStoresAllBootstrapJar extends TestUtils {
	public static void main(String[] args) {
		runSimpleJavaProgramWithPersistentCache("TestZipCacheStoresAllBootstrapJar");
		runPrintStats("TestZipCacheStoresAllBootstrapJar",true,"zipcache");

		String bootClassPath = System.getProperty("sun.boot.class.path");
		
		// Assumimg that the sun.boot.class.path is of the following format:
		// full_path_to_file1.jar;full_path_to_file2.jar
		String[] bootClassPathSplit = bootClassPath.split(";");
		
		for (String bootClassPathEntry : bootClassPathSplit) {
			// Not all files in the "sun.boot.class.path" have to exist, check if they do
			if ((new File(bootClassPathEntry)).exists()) {
				// get the file name, split for \ or /.
				String[] bootClassPathEntryParts = bootClassPathEntry.split("[\\\\/]");
	
				String filename = bootClassPathEntryParts[bootClassPathEntryParts.length- 1];
				checkOutputContains(filename, "The shared cache printstats output must contain the ZIPCACHE data for the file "+filename+".");
			}
		}
		runDestroyAllCaches();
	}
}
