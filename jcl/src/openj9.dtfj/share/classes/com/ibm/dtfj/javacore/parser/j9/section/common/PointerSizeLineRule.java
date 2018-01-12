/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2017 IBM Corp. and others
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
package com.ibm.dtfj.javacore.parser.j9.section.common;

import java.util.regex.Matcher;

import com.ibm.dtfj.javacore.parser.framework.tag.LineRule;

/**
 * Retrieves the pointer size from a line containing OS and JVM information.
 *
 */
public class PointerSizeLineRule extends LineRule {

	/**
	 * 
	 */
	public void processLine(String source, int startingOffset) {
		consumeUntilFirstMatch(CommonPatternMatchers.open_paren);
		String pointerSize = "32";
		int lastIndex = indexOfLast(CommonPatternMatchers.build_string);
		if (lastIndex >= 0) {
			String pattern = consumeCharacters(0, lastIndex);
			Matcher matcher = CommonPatternMatchers.bits64;
			matcher.reset(pattern);
			if (matcher.find()) {
				pointerSize = "64";
			} else {
				matcher = CommonPatternMatchers.s390;
				matcher.reset(pattern);
				if (matcher.find()) {
					pointerSize = "31";
				}
			}
		}
		addToken(ICommonTypes.POINTER_SIZE, pointerSize);
	}
}
