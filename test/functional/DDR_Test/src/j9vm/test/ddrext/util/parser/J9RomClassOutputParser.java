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
 * This class is used to extract info from !j9romclass <address> DDR extension output
 * @author ashutosh
 *
 */
public class J9RomClassOutputParser {
	private static Logger log = Logger.getLogger(J9RomClassOutputParser.class);
	
	/**
	 * This method finds the address of J9UTF8 representing the className from !j9romclass output.
	 * 
	 * Sample output:
	 * 	0x8: J9SRP(struct J9UTF8) className = !j9utf8 0x00007FECADCD107A
	 * 
	 * @param j9RomClassOutput
	 * @return
	 */
	public static String extractClassNameAddr(String j9RomClassOutput) {
		if (null == j9RomClassOutput) {
			log.error("!j9romclass output is null");
			return null;
		}
		
		String[] outputLines = j9RomClassOutput.split(Constants.NL);
		for (String aLine : outputLines) {
			if (aLine.indexOf("className =") != -1) {
				/* split around whitespace and use the last token to get class name address */
				String tokens[] = aLine.split("\\s");
				String addr = tokens[tokens.length-1];
				return addr;
			}
		}
		return null;
	}
}
