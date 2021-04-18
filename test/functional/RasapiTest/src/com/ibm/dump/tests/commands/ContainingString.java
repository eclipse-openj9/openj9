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
/**
 * This class is to check if the given string is contained in any of the output lines.
 * Usage:
 * 		com.ibm.dump.tests.commands.ContainingString [N] [-i] [-negate] <StringToSearch>
 * 		Use N to check the Nth line only (N starts from 0!).
 * 			Note, Use -1 to check the last line, and -N to check the Nth line from the last line.
 * 		Use -i to ignore case.
 * 		Use -negate to fail the test when the given string is contained in any of the output lines.
 * <p>
 * @author Manqing Li
 *
 */
public class ContainingString implements ICommandOutputChecker {

	public boolean check(String command, String args, List<String> outputLines) {
		boolean ignoreCase = false;
		boolean negate = false;
		int n = Integer.MAX_VALUE;
		
		args = args.trim();
		int tokenEndIndex = StringUtils.getFirstTokenEndIndex(args);

		while((tokenEndIndex = StringUtils.getFirstTokenEndIndex(args)) >= 0) {
			String token = args.substring(0, tokenEndIndex + 1).trim();
			if(token.equals(ARG_IGNORE_CASE)) {
				ignoreCase = true;
				args = args.substring(tokenEndIndex + 1).trim();
			} else if (token.equals(ARG_NEGATE)) {
				negate = true;
				args = args.substring(tokenEndIndex + 1).trim();
			} else if(n == Integer.MAX_VALUE) {
				try {
					n = Integer.parseInt(token);
					args = args.substring(tokenEndIndex + 1).trim();
				} catch (NumberFormatException e) {
					break;
				}
			} else {
				break;
			}
		}
		
		if(n != Integer.MAX_VALUE) {
			if(n < 0) {
				n = outputLines.size() + n;
			}
			if(n >= 0 && n < outputLines.size()) {
				return StringUtils.contain(outputLines.get(n), args, ignoreCase) != negate;
			}
			return negate == false;
		}
		
		if(StringUtils.containInAnyLines(outputLines, args, ignoreCase)) {
			return negate == false;
		} else {
			return negate == true;
		}
	}
	
	private static final String ARG_IGNORE_CASE = "-i";
	private static final String ARG_NEGATE = "-negate";
}
