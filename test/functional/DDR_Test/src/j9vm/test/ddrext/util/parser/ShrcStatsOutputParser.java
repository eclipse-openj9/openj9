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

import java.util.ArrayList;

import org.testng.log4testng.Logger;

public class ShrcStatsOutputParser {
	private static Logger log = Logger.getLogger(ShrcStatsOutputParser.class);
	
	/*
	 * Sample output:
	 * 	segment area     : 0x00007FADA701C000 - 0x00007FADA717CE28 size 1445416
	 */
	/**
	 * This method is used to extract start address of ROMClass segment area in shared cache.
	 * 
	 * @param output
	 * @return
	 */
	public static String extractROMClassSegmentStartAddr(String output) {
		if (null == output) {
			log.error("!shrc stats output is null");
			return null;
		}
		
		String[] outputLines = output.split("\n");
		for (String aLine : outputLines) {
			if (aLine.startsWith("segment area")) {
				int indexOfColon = aLine.indexOf(':');
				String startAddr = aLine.substring(indexOfColon+1, aLine.indexOf("-", indexOfColon)).trim();
				return startAddr;
			}
		}
		return null;
	}
	
	/*
	 * Sample output:
	 * 	segment area     : 0x00007FADA701C000 - 0x00007FADA717CE28 size 1445416
	 */
	/**
	 * This method is used to extract end address of ROMClass segment area in shared cache.
	 * 
	 * @param output
	 * @return
	 */
	public static String extractROMClassSegmentEndAddr(String output) {
		if (null == output) {
			log.error("!shrc stats output is null");
			return null;
		}
		
		String[] outputLines = output.split("\n");
		for (String aLine : outputLines) {
			if (aLine.startsWith("segment area")) {
				int indexOfHyphen = aLine.indexOf('-');
				String endAddr = aLine.substring(indexOfHyphen+1, aLine.indexOf("size", indexOfHyphen)).trim();
				return endAddr;
			}
		}
		return null;
	}
	
	public static String extractRawClassDataAreaStartAddr(String output) {
		if (null == output) {
			log.error("!shrc stats output is null");
			return null;
		}
		
		String[] outputLines = output.split("\n");
		for (String aLine : outputLines) {
			if (aLine.startsWith("raw class area")) {
				int indexOfColon = aLine.indexOf(':');
				String startAddr = aLine.substring(indexOfColon+1, aLine.indexOf("-", indexOfColon)).trim();
				return startAddr;
			}
		}
		return null;
	}
	
	public static String extractRawClassDataAreaEndAddr(String output) {
		if (null == output) {
			log.error("!shrc stats output is null");
			return null;
		}
		
		String[] outputLines = output.split("\n");
		for (String aLine : outputLines) {
			if (aLine.startsWith("raw class area")) {
				int indexOfHyphen = aLine.indexOf('-');
				String endAddr = aLine.substring(indexOfHyphen+1, aLine.indexOf("size", indexOfHyphen)).trim();
				return endAddr;
			}
		}
		return null;
	}
}
