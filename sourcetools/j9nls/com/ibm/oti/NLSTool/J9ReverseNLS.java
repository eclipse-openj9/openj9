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
package com.ibm.oti.NLSTool;

import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.StringReader;
import java.io.Writer;
import java.util.Hashtable;
import java.util.Iterator;
import java.util.Properties;
import java.util.Vector;

public class J9ReverseNLS implements NLSConstants {

	public static void main(String[] args) throws IOException {
		
		if (args.length != 2 || args[0].compareToIgnoreCase("-help") == 0) {
			printUsage();
			System.exit(0);
		}
	
		String propertiesFile = args[0];
		String locale = args[1];
		
		J9NLS nlsTool = new J9NLS();
		Hashtable nlsFiles = nlsTool.searchNLSFiles();
		
		J9NLS.dp("Reading " + propertiesFile + "...");
		Properties javaProperties = new Properties();
		javaProperties.load(new FileInputStream(propertiesFile));
		
		Vector baseNLSFiles = (Vector)nlsFiles.get("");
		for (Iterator iter = baseNLSFiles.iterator(); iter.hasNext();) {
			File nlsInputFile = (File) iter.next();
			Properties nlsProperties = new Properties();
			InputStream is = new FileInputStream(nlsInputFile);
			nlsProperties.load(is);
			is.close();
			
			File nlsOutputFile = localeSpecificNLSFile(nlsInputFile, locale);
			J9NLS.dp("Writing " + nlsOutputFile + "...");
			Writer out = new FileWriter(nlsOutputFile);
			out.write("# This file was automatically reverse engineered from " + nlsInputFile.getName() + " and " + propertiesFile + "\n");
			out.write(MODULE_KEY + "=" + nlsProperties.getProperty(MODULE_KEY) + "\n");
			out.write(HEADER_KEY + "=" + nlsProperties.getProperty(HEADER_KEY) + "\n");
			out.write("\n");
			
			Vector orderedKeys = nlsTool.getOrderedKeys(nlsProperties, nlsInputFile);
			for (int index = 0; index < orderedKeys.size(); index++) {
				String key = (String) orderedKeys.get(index);
				String smallKey =  nlsProperties.getProperty(MODULE_KEY) + nlsTool.formatMsgNumber(index);
				String value = javaProperties.getProperty(smallKey);
				
				if (value != null) {
					out.write(encode(key, value));
				} else {
					System.err.println("Warning: no translated value found for " + key);
					out.write("\n#translation required for the following key: \n");
					out.write("#" + encode(key, nlsProperties.getProperty(key)) + "\n");
				}
			}
			
			out.close();
		}
	}
	
	
	private static void printUsage() {
		System.out.println("Reverse engineers .NLS files from a .properties file.");
		System.out.println("Usage: J9ReverseNLS java<locale>.properties locale");
		System.out.println("e.g. J9ReverseNLS java_fr.properties _fr");
	}
	
	private static File localeSpecificNLSFile(File baseFile, String locale) {
		String baseName = baseFile.getName();
		String extension = baseName.substring(baseName.indexOf('.'), baseName.length());
		baseName = baseName.substring(0, baseName.indexOf('.'));
		return new File(baseFile.getParentFile(), baseName + locale + extension);
	}
	
	private static String encode(String key, String plainText) throws IOException {
		Properties props = new Properties();
		OutputStream os = new ByteArrayOutputStream();
		String result = "";
		
		props.setProperty(key, plainText);
		props.store(os, null);
		
		BufferedReader reader = new BufferedReader(new StringReader( os.toString() ) );
		String line = reader.readLine();
		/* skip leading comments */
		while (line != null && line.length() > 0 && line.charAt(0) == '#') {
			line = reader.readLine();
		}
		/* read the rest */
		while (line != null) {
			result += line;
			result += "\n";
			line = reader.readLine();
		}
		
		return result;
	}
}
