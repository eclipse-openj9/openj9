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
 * This class is used to extract info from !dumpromclass <address> DDR extension output
 * @author ashutosh
 *
 */
public class DumpRomClassOutputParser {
	private static Logger log = Logger.getLogger(DumpRomClassOutputParser.class);
	private String[] outputLines;
	
	public DumpRomClassOutputParser(String dumpRomClassOutput) {
		if (null == dumpRomClassOutput) {
			log.error("!dumpromclass output is null");
		}
		outputLines = dumpRomClassOutput.split(Constants.NL);
	}
	
	/**
	 * This method finds the address of intermediate class data from !dumpromclass output.
	 * 
	 * Sample output:
	 * 	Intermediate Class Data (50434 bytes): 7fef1ef6a0f8
	 * 
	 * @return
	 */
	public String extractIntermediateDataAddr() {
		for (String aLine : outputLines) {
			if (aLine.startsWith("Intermediate Class Data")) {
				String[] tokens = aLine.split("\\s");
				return "0x" + tokens[tokens.length-1];
			}
		}
		return null;
	}

	/**
	 * This method finds the size of intermediate class data from !dumpromclass output.
	 * 
	 * Sample output:
	 * 	Intermediate Class Data (50434 bytes): 7fef1ef6a0f8
	 * 
	 * @return
	 */
	public int extractIntermediateDataLength() {
		for (String aLine : outputLines) {
			if (aLine.startsWith("Intermediate Class Data")) {
				String size = aLine.substring(aLine.indexOf("(")+1, aLine.indexOf(" ", aLine.indexOf("(")));
				int intermediateDataLength = Integer.parseInt(size);
				return intermediateDataLength;
			}
		}
		return 0;
	}

	/**
	 * This method finds the size of J9ROMClass class data from !dumpromclass output.
	 * 
	 * Sample output:
	 * 	ROM Size: 0x138 (312)
	 * 
	 * @return
	 */
	public int extractROMClassSize() {
		for (String aLine : outputLines) {
			if (aLine.startsWith("ROM Size")) {
				String size = aLine.substring(aLine.indexOf("(")+1, aLine.indexOf(")"));
				int intermediateDataLength = Integer.parseInt(size);
				return intermediateDataLength;
			}
		}
		return 0;
	}
}
