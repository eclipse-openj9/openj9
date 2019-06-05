/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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


import org.testng.log4testng.Logger;
import j9vm.test.ddrext.Constants;

public class MethodForNameOutputParser {
	
	private static Logger log = Logger.getLogger(MethodForNameOutputParser.class);

	/*
	 * Sample !methodforname output :
	 * 
	 * > !methodforname java/lang/System.arraycopy 
	 * Searching for methods named 'java/lang/System.arraycopy' in VM=0x000BAEE8... 
	 * !j9method 0x41FCDB04 -->
	 * java/lang/System.arraycopy(Ljava/lang/Object;ILjava/lang/Object;II)V
	 * !j9method 0x41FCDB14 -->
	 * java/lang/System.arraycopy([Ljava/lang/Object;I[Ljava/lang/Object;II)V
	 * !j9method 0x41FCDB04 -->
	 * java/lang/System.arraycopy(Ljava/lang/Object;ILjava/lang/Object;II)V
	 * !j9method 0x41FCDB14 -->
	 * java/lang/System.arraycopy([Ljava/lang/Object;I[Ljava/lang/Object;II)V
	 * Found 4 method(s) named java/lang/System.arraycopy
	 * 
	 * /** This method is used to extract the j9method address from
	 * !methodforname output.
	 * 
	 * @param fineMethodForNameOutput !methodforname output
	 * @param methodArguments JVM signature of the arguments to the method, e.g. "JI". 
	 * 			This must be the full set of arguments or "null" to take the first matching method.
	 * 
	 * @return String representation of extracted J9Method address from
	 * !methodforname extension. return null, if any error occurs or address
	 * can not be found in given !methodforname output.
	 */
	public static String extractMethodAddress(String methodForNameOutput, String methodArguments) {
		if (null == methodForNameOutput) {
			log.error("!methodforname output is null");
			return null;
		}
		
		String[] outputLines = methodForNameOutput.split("!j9method");
		for (String aLine : outputLines) {
			if (aLine.contains("-->")) {
				String[] tokens = aLine.split("-->");
				String method = tokens[1].trim();
				if (null == methodArguments 
				|| method.substring(method.indexOf('(') + 1, method.indexOf(')')).equals(methodArguments)
				) {
					String address = tokens[0].trim();
					return address;
				}
			}
		}
		return null;
	}

}
