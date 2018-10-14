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

import j9vm.test.ddrext.Constants;

import org.testng.log4testng.Logger;

/**
 * This class is used to extract info from !j9class <address> DDR extension output
 * @author ashutosh
 *
 */
public class J9ClassOutputParser {
	private static Logger log = Logger.getLogger(J9ClassOutputParser.class);

	/**
	 * This method finds the address of J9ROMClass from !j9class output.
	 * 
	 * Sample output:
	 * 	0x8: struct J9ROMClass * romClass = !j9romclass 0x00007FECADCF23A0
	 * 
	 * @param j9ClassOutput
	 * @return
	 */
	public static String extractRomClassAddr(String j9ClassOutput) {
		if (null == j9ClassOutput) {
			log.error("!j9class output is null");
			return null;
		}
		
		String[] outputLines = j9ClassOutput.split(Constants.NL);
		for (String aLine : outputLines) {
			if (aLine.indexOf("struct J9ROMClass * romClass") != -1) {
				/* split around whitespace and use the last token to get romClass address */
				String tokens[] = aLine.split("\\s");
				String addr = tokens[tokens.length-1];
				/* remove leading zeroes if any */
				return ParserUtil.removeExtraZero(addr);
			}
		}
		return null;
	}
	
	/**
	 * This method finds the address of arrayClass from !j9class output.
	 * 
	 * Sample output:
	 * 	0x50: struct J9Class * arrayClass = !j9class 0x0000000001DE8500 // [Ljava/lang/Object;
	 * 
	 * @param j9ClassOutput
	 * @return
	 */
	public static String extractArrayClassAddr(String j9ClassOutput) {
		if (null == j9ClassOutput) {
			log.error("!j9class output is null");
			return null;
		}
		
		String[] outputLines = j9ClassOutput.split(Constants.NL);
		for (String aLine : outputLines) {
			if (aLine.indexOf("struct J9Class * arrayClass") != -1) {
				/* split around whitespace and use the token next to "!j9class" to get arrayClass address */
				String tokens[] = aLine.split("\\s");
				int i = 0;
				while (!tokens[i].equals("!j9class")) {
					i++;
				}
				/* remove leading zeroes if any */
				return ParserUtil.removeExtraZero(tokens[i+1]);
			}
		}
		return null;
	}
	
	/**
	 * This method finds the address of replacedClass from !j9class output.
	 * 
	 * Sample output:
	 * 	0x50: struct J9Class * replacedClass = !j9class 0x0000000001DE8500 // [Ljava/lang/Object;
	 * 
	 * @param j9ClassOutput
	 * @return
	 */
	public static String extractReplacedClassAddr(String j9ClassOutput) {
		if (null == j9ClassOutput) {
			log.error("!j9class output is null");
			return null;
		}
		
		String[] outputLines = j9ClassOutput.split(Constants.NL);
		for (String aLine : outputLines) {
			if (aLine.indexOf("struct J9Class * replacedClass") != -1) {
				/* split around whitespace and use the token next to "!j9class" to get replacedClass address */
				String tokens[] = aLine.split("\\s");
				int i = 0;
				while (!tokens[i].equals("!j9class")) {
					i++;
				}
				/* remove leading zeroes if any */
				return ParserUtil.removeExtraZero(tokens[i+1]);
			}
		}
		return null;
	}
}
