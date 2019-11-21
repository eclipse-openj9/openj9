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

package tests.sharedclasses;

import java.io.FileWriter;

/**
 * Based on input parameters and environment variables, this will create a config.properties file suitable for 
 * running the tests.
 */
public class CreateConfig {

	public static void main(String[] args) throws Exception {

//		Properties p = System.getProperties();
//		p.list(System.out);
		
		// format is:
//		# Which java.exe to call
//		#java_exe=c:/andyc/j9vmwi3224/sdk/jre/bin/java.exe
//		#java_exe=c:/ipartrid/j9vmwi3224/jre/bin/java.exe
//		java_exe=c:/ben/j9vmwi3224/sdk/jre/bin/java.exe
//
//		# Default location for cache files and javasharedresources
//		defaultCacheLocation=C:/Documents and Settings/clemas/Local Settings/Application Data
//		# These are used if set, otherwise just the default is assumed
//		# cacheDir=
//		# controlDir=
//
//		# and a java for creating old incompatible cache files
//		java5_exe=c:/andyc/j9vmwi3223/sdk/jre/bin/java.exe
//
//		# If set, this will tell us what commands are executing
//		logCommands=true
		
		String javaForTesting = System.getProperty("testjava");
		String cacheDir = System.getProperty("cachedir");
		String java5 = System.getProperty("refjava");
		
		// make sure '/' the right way round!
		javaForTesting = javaForTesting.replace('\\','/');
		cacheDir = cacheDir.replace('\\','/');
		java5 = java5.replace('\\','/');
		
		FileWriter writer = new FileWriter("config.properties");
		writer.write("# Java to be tested\n");
		writer.write("java_exe="+javaForTesting+"\n\n");
		writer.write("# Cache directory\n");
		writer.write("cacheDir="+cacheDir+"\n\n");
		writer.write("# Old jdk for creating old caches\n");
		writer.write("java5_exe="+java5+"\n");
		writer.flush();
		writer.close();
	}
	
}
