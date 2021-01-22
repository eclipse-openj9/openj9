/*******************************************************************************
 * Copyright (c) 2016, 2021 IBM Corp. and others
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
package com.ibm.dump.tests.commands;

import java.util.List;
import java.util.StringTokenizer;
/**
 * This class is to count how many characters in the output lines.
 * Note, the leading spaces and tailing spaces will be trimmed for all lines
 * before it starts counting.
 * Usage:
 * 			com.ibm.dump.tests.commands.CountingChars [<Num>] [max=<Num>] [min=<Num>]
 * <p>
 * @author Manqing Li
 *
 */
public class CountingChars implements ICommandOutputChecker {

	public boolean check(String command, String args, List<String> output) {
		int counter = count(output); 
		StringTokenizer st = new StringTokenizer(args);
		while(st.hasMoreTokens()) {
			String t = st.nextToken();
			int index = t.indexOf("=");
			if(index > 0) {
				String key = t.substring(0, index);
				String value = t.substring(index + 1);
				
				if(MAX.equals(key)) {
					if(counter > Integer.parseInt(value)) {
						return false;
					}
				} else if(MIN.equals(key)) {
					if(counter < Integer.parseInt(value)) {
						return false;
					}
				}
			} else if( counter != Integer.parseInt(t)) {
				return false;
			}
		}
		return true;
	}

	private int count (List<String> output) {
		int counter = 0;
		for(String line : output) {
			counter += line.trim().length();
		}
		return counter;
	}
	
	private static final String MAX = "max";
	private static final String MIN = "min";
}
