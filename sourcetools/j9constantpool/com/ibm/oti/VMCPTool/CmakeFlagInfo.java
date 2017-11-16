/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package com.ibm.oti.VMCPTool;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.HashSet;
import java.util.Set;
import java.util.regex.Pattern;
import java.util.regex.Matcher;


class CmakeFlagInfo implements IFlagInfo {
	private HashSet<String> seenFlags = new HashSet<String>();
	private HashSet<String> setFlags = new HashSet<String>();

	// Convert string  to  bool unsing same rules as cmake
	private static boolean strToBool(String str) {
		str = str.trim().toUpperCase();
		if (str.isEmpty() || str.equals("NO") || str.equals("FALSE") || str.equals("OFF") || str.endsWith("-NOTFOUND")) {
			return false;
		}
		return true;
	}

	public CmakeFlagInfo(String cacheInfo) throws Throwable {
		// CMake cache file lines look like this:
		// <VariableName>:<Type>=<Value>

		// Pick out any lines Where <Type> == BOOL
		Pattern cacheVarPattern = Pattern.compile("([a-zA-Z0-9_]+):BOOL=(.*)$");
		BufferedReader reader = new BufferedReader(new FileReader(cacheInfo));
		String line;

		while (null != (line = reader.readLine())) {
			Matcher matcher = cacheVarPattern.matcher(line);
			if (matcher.matches()) {
				
				String flagName = matcher.group(1);
				boolean flagValue = strToBool(matcher.group(2));

				if (flagName.startsWith("J9VM_")) {
					flagName = Util.transformFlag(flagName);
					if (flagValue) {
						setFlags.add(flagName);
					} 
					seenFlags.add(flagName);

				}
			}
		}
	}

	public Set<String> getAllSetFlags() {
		return setFlags;
	}


	public boolean isFlagValid(String flagName) {
		// TODO We need to properly define all the flags from cmake before we turn this on
		// return seenFlags.contains(Util.transformFlag(flagName));
		return true;
	}
	
}
