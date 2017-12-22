/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2012, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.tools.utils;

import java.util.ArrayList;
import java.util.List;
import java.util.StringTokenizer;

/**
 * This class checks if a line contains some strings.
 * <p>
 * @author Manqing Li, IBM.
 *
 */
public class MatchHandle implements IMatchHandle {

	public MatchHandle(String [] matchStringList, boolean ignoreCase) {
		this(matchStringList, ignoreCase, false, false);
	}
	
	public MatchHandle(String [] matchStringList, boolean ignoreCase, boolean negated, boolean isFixedString) {
		this.matchStringList = matchStringList;
		this.ignoreCase = ignoreCase;
		this.negated = negated;
		this.isFixedString = isFixedString;
	}

	/**
	 * To check if the string is matched.
	 * <p>
	 * @param s		The string to be checked.
	 * <p>
	 * @return		<code>true</code> if the string is matched;
	 * 				<code>false</code> otherwise.
	 */
	public boolean matches(String s) {
		for (String match : matchStringList) {
			if (negated != match(s, match)) {
				return true;
			}
		}
		return false;
	}
	
	/**
	 * To process the string.
	 * <p>
	 * @param s		The string to be processed.
	 * <p>
	 * @return		The processed string.
	 */
	public String process(String s) {
		return s;
	}

	/**
	 * To check if the string is matched.
	 * Note, this method does not consider if the match is negated.
	 */
	private boolean match(String s, String match) {
		if (isFixedString) {
			return matchFixedStrings(s, new String[] {match});
		}
		
		StringTokenizer st = new StringTokenizer(match, "*");
		List<String> ls = new ArrayList<String>();
		while (st.hasMoreTokens()) {
			ls.add(st.nextToken());
		}
		return matchFixedStrings(s, (String [])ls.toArray(new String [ls.size()]));
	}

	/**
	 * To check if a fixed string is matched.
	 * Note, this method does not consider if the match is negated.
	 */
	private boolean matchFixedStrings(String s, String [] matches) {
		if (ignoreCase) {
			s = s.toLowerCase();
		}
		for (String match : matches) {
			if (ignoreCase) {
				match = match.toLowerCase();
			}
			int index = s.indexOf(match);
			if (index < 0) {
				return false;
			}
			s = s.substring(index + match.length());
		}
		return true;
	}
	
	private final String [] matchStringList;
	private final boolean ignoreCase;
	private final boolean negated;
	private final boolean isFixedString;
}
