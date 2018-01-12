/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2009, 2017 IBM Corp. and others
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
package com.ibm.dtfj.javacore.parser.j9.section.thread;

import java.util.regex.Matcher;

import com.ibm.dtfj.javacore.parser.framework.scanner.IParserToken;
import com.ibm.dtfj.javacore.parser.framework.tag.LineRule;
import com.ibm.dtfj.javacore.parser.j9.section.common.CommonPatternMatchers;

public class NativeStackTraceLineRule extends LineRule implements IThreadTypes {

	// Matchers are not thread safe so cannot be static
	private final Matcher mb = CommonPatternMatchers.generateMatcher("\\[");
	private final Matcher sign = CommonPatternMatchers.generateMatcher("[-+]");
	private final Matcher spaceparen = CommonPatternMatchers.generateMatcher("\\s*\\(");
	
	protected void processLine(String source, int startingOffset) {
		//4XENATIVESTACK               _threadstart  (thread.c:196, 0x7C34940F [MSVCR71+0x6c])
		//4XENATIVESTACK               GetModuleHandleA  (0x77E6482F [kernel32+0xdf])
		//4XENATIVESTACK               monitor_enter_three_tier  (j9thread.c, 0x7FFA284B [J9THR26+0xbb])
		//4XENATIVESTACK               (0x0000002A9632E2B9 [libj9prt26.so+0x112b9])
		//4XENATIVESTACK               0x0000002A9632E2B9
		//new format with routine  plus offset 
		//4XENATIVESTACK               j9sig_protect+0x41 (j9signal.c:144, 0x7FECBE31 [J9PRT24+0xbe31])
		//Java 7.0
		//4XENATIVESTACK               getFinalPath+0xaa (winntfilesystem_md.c:126, 0x00408C4F [java+0x8c4f])
		//4XENATIVESTACK               GetModuleFileNameA+0x1b4 (0x7C80B683 [kernel32+0xb683])
		consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
		// The routine name is ended by at least one space and an open parenthesis
		IParserToken routine = addAllCharactersAsTokenAndConsumeFirstMatch(STACK_ROUTINE, spaceparen);
		if (routine != null) {
			String val = routine.getValue();
			sign.reset(val);
			if (sign.find()) {
				addToken(STACK_ROUTINE, val.substring(0, sign.start()));
				CommonPatternMatchers.signed_hex_0x.reset(val);
				if (CommonPatternMatchers.signed_hex_0x.find()) {
					addToken(STACK_ROUTINE_OFFSET, CommonPatternMatchers.signed_hex_0x.group());
				}
			}
		}
		// The file name is ended by a comma
		int sourceIndex = indexOfLast(CommonPatternMatchers.comma);
		if (sourceIndex >= 0) {
			// The line number is indicated by a colon before the comma
			int sourceIndex2 = indexOfLast(CommonPatternMatchers.colon);
			if (sourceIndex2 < sourceIndex) {
				addAllCharactersAsTokenAndConsumeFirstMatch(STACK_FILE, CommonPatternMatchers.colon);
				addToken(STACK_LINE, CommonPatternMatchers.dec);
			} else {
				addAllCharactersAsTokenAndConsumeFirstMatch(STACK_FILE, CommonPatternMatchers.comma);				
			}
		}
		addPrefixedHexToken(STACK_PROC_ADDRESS);
		consumeUntilFirstMatch(mb);
		addAllCharactersAsTokenUntilFirstMatch(STACK_MODULE, sign);
		addToken(STACK_MODULE_OFFSET, CommonPatternMatchers.signed_hex_0x);
	}

}
