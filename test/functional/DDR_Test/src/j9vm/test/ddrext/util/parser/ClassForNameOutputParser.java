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


package j9vm.test.ddrext.util.parser;


import java.util.ArrayList;

import org.testng.log4testng.Logger;
import j9vm.test.ddrext.Constants;

public class ClassForNameOutputParser {
	
	private static Logger log = Logger.getLogger(ClassForNameOutputParser.class);

	/*
	 * Sample !classforname output :
	 * 
	 * >!classforname java/lang/Object 
	 * Searching for classes named 'java/lang/Object' in VM=1006d2f8 
	 * !j9class 0x101B8300 named java/lang/Object 
	 * Found 1 class(es) named java/lang/Object
	 */
	/**
	 * This method is used to extract the j9class address from !classforname output. 
	 * 
	 * @param fineClassForNameOutput !classforname output  
	 * @return String representation of extracted j9class address from !classforname extension. 
	 *         return null, if any error occurs or address can not be found in given !findVM output. 	
	 */
	public static String extractClassAddress(String classForNameOutput) {
		if (null == classForNameOutput) {
			log.error("!classforname output is null");
			return null;
		}
		
		String[] outputLines = classForNameOutput.split(Constants.NL);
		for (String aLine : outputLines) {
			if (aLine.startsWith("!j9class")) {
				String[] tokens = aLine.split(" ");
				return tokens[1].trim();
			}
		}
		return null;
	}
	
	/*
	 * Sample !classforname output :
	 * 
	 * >!classforname java/lang/Object 
	 * Searching for classes named 'java/lang/Object' in VM=1006d2f8 
	 * !j9class 0x101B8300 named java/lang/Object
	 * !j9class 0x102A9400 names java/lang/Object
	 * Found 2 class(es) named java/lang/Object
	 */
	/**
	 * This method is used to extract list of all j9class addresses from !classforname output. 
	 * 
	 * @param fineClassForNameOutput !classforname output  
	 * @return ArrayList containing all extracted j9class addresses from !classforname extension. 
	 *         return null, if any error occurs or address can not be found in given !findVM output. 	
	 */
	public static ArrayList<String> extractClassAddressList(String classForNameOutput) {
		if (null == classForNameOutput) {
			log.error("!classforname output is null");
			return null;
		}
		ArrayList<String> addrList = new ArrayList<String>();
		String[] outputLines = classForNameOutput.split(Constants.NL);
		for (String aLine : outputLines) {
			if (aLine.startsWith("!j9class")) {
				String[] tokens = aLine.split("\\s");
				addrList.add(tokens[1].trim());
			}
		}
		if (addrList.size() != 0) {
			return addrList;
		}
		return null;
	}

}
