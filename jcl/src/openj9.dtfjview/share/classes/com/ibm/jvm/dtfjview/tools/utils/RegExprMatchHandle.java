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

import java.util.regex.Pattern;

/**
 * This class handles the matching tasks when regular expressions are used.
 * Note, the regular expression is defined by Javadoc of class java.util.regex.Pattern.
 * Also, it always enables the dotall mode (java.util.regex.Pattern.DOTALL).
 * <p>
 * @author Manqing Li, IBM.
 */
public class RegExprMatchHandle implements IMatchHandle {

	public RegExprMatchHandle(String [] regExList, boolean ignoreCase) {
		this(regExList, ignoreCase, false);
	}
	public RegExprMatchHandle(String [] regExList) {
		this(regExList, false, false);
	}
	public RegExprMatchHandle(String [] regExList, boolean ignoreCase, boolean negated) {
		if (ignoreCase) {
			this.patternList = new Pattern[regExList.length];
			for (int i = 0; i < regExList.length; i++) {
				this.patternList [i] = Pattern.compile(regExList[i], Pattern.DOTALL + Pattern.CASE_INSENSITIVE);
			}
		} else {
			this.patternList = new Pattern[regExList.length];
			for (int i = 0; i < regExList.length; i++) {
				this.patternList [i] = Pattern.compile(regExList[i], Pattern.DOTALL);
			}
		}
		this.negated = negated;
	}

	public boolean matches(String s) {
		for (Pattern p : patternList) {
			if (negated != p.matcher(s).find()) {
				return true;
			}
		}
		return false;
	}

	public String process(String s) {
		return s;
	}
	
	private final Pattern [] patternList;
	private final boolean negated;
}
