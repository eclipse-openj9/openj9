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


import org.testng.log4testng.Logger;
import j9vm.test.ddrext.Constants;

public class J9MethodOutputParser {
	
	private static Logger log = Logger.getLogger(J9MethodOutputParser.class);

	/*
	 * Sample !j9method output :
	 * 
	 *  > !j9method 0x41FCDB04
	 *  J9Method at 0x41fcdb04 {
     *  0x0: U8 * bytecodes = !j9x 0x42D0DA60
     *  0x4: struct J9ConstantPool * constantPool = !j9constantpool 0x41FCD4A0
     *  0x8: void * methodRunAddress = !j9x 0x7F9ACA10
     *  0xc: void * extra = !j9x 0x7F9ACA11
	 */
	 /** This method is used to extract the methodRunAddress from !j9method output.
	  * 
	 * @param j9methodOutput !j9method output
	 * 
	 * @return String representation of extracted methodRunAddress from
	 * !j9method extension. return null, if any error occurs or address
	 * can not be found in given !j9method output.
	 */
	public static String extractMethodAddress(String j9methodOutput) {
		if (null == j9methodOutput) {
			log.error("!j9method output is null");
			return null;
		}

		String[] outputLines = j9methodOutput.split(Constants.NL);
		for (String aLine : outputLines) {
			if (aLine.contains("methodRunAddress")) {
				String[] tokens = aLine.split("methodRunAddress");
				System.out.println(tokens[1].trim().split(" ")[2].trim());
				return tokens[1].trim().split(" ")[2].trim();
			}
		}
		return null;
	}

	/** This method is used to extract the bytecode from !j9method output.
	  * 
	 * @param j9methodOutput !j9method output
	 * 
	 * @return String representation of extracted bytecode from
	 * !j9method extension. return null, if any error occurs or address
	 * can not be found in given !j9method output.
	 */
	public static String extractByteCodesAddress(String j9methodOutput) {
		if (null == j9methodOutput) {
			log.error("!j9method output is null");
			return null;
		}
		
		String[] outputLines = j9methodOutput.split(Constants.NL);
		for (String aLine : outputLines) {
			if (aLine.contains("!bytecodes")) {
				return aLine.split("bytecodes")[1].trim();
			}
		}
		return null;
	}

}
