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
package com.ibm.dtfj.javacore.parser.j9.section.stack;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.ibm.dtfj.javacore.parser.framework.scanner.IParserToken;
import com.ibm.dtfj.javacore.parser.framework.tag.ILineRule;
import com.ibm.dtfj.javacore.parser.framework.tag.LineRule;
import com.ibm.dtfj.javacore.parser.framework.tag.TagParser;
import com.ibm.dtfj.javacore.parser.j9.section.common.CommonPatternMatchers;

/**
 * Parser rules for lines in the (native) stack section in the javacore
 */
public class StackTagParser extends TagParser implements
		IStackTypes {

	public StackTagParser() {
		super(STACK_SECTION);
	}
	
	// E.g. Linux [0x123456]
	private static final Matcher LINUXADDRESS = CommonPatternMatchers.generateMatcher("\\[0[xX]\\p{XDigit}+\\]");
	/**
	 * Initialize parser with rules for lines in the environment (CI) section in 
	 * the javacore
	 */
	protected void initTagAttributeRules() {
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				// version is on a single line, can include whitespace, separators etc
				consumeUntilFirstMatch(CommonPatternMatchers.colon);
				addPrefixedHexToken(STACK_THREAD);
			}
		};
		
		addTag(T_BTTHREADID, lineRule);
		
		lineRule = new LineRule() {
			// Consider all the possibilities
			// Linux
			//1BTSTACKENT          /home/vmfarm/pxa6460sr6/ibm-java-x86_64-60/jre/lib/amd64/default/libj9dyn24.so(j9bcutil_createRomClass+0x40) [0x2a99e1be00]
			//1BTSTACKENT          /home/vmfarm/pxa6460sr6/ibm-java-x86_64-60/jre/lib/amd64/default/libj9dyn24.so [0x2a99e1be00]
			//1BTSTACKENT          [0x2a99e1be00]
			// AIX
			//1BTSTACKENT          0x3076AB84
			//1BTSTACKENT          /data/AIX/testjava/java6-32/sdk/jre/lib/ppc/libj9vm24.so:0xD538DDB4 [0xD5363000 +0x0002ADB4]
			// z/OS
			//1BTSTACKENT          MM_ConcurrentSweepScheme::workThreadFindMinimumSizeFreeEntry(MM_EnvironmentModron*,MM_Memor...:0x2C280D0C [0x2C280840 +0x000004CC]
			//1BTSTACKENT          :0x2C284192 [0x2C283FF0 +0x000001A2]
			//1BTSTACKENT          thread_wrapper:0x2B697FBA [0x2B697B48 +0x00000472]
			//1BTSTACKENT          CEEVROND:0x0F7E1AE8 [0x0F7E1198 +0x00000950]			

			public void processLine(String source, int startingOffset) {
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				
				//this.consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				//this.matchAndConsumeValue(CommonPatternMatchers.whitespace);
				//this.indexOfLast(CommonPatternMatchers.whitespace);
				//this.addAllCharactersAsTokenAndConsumeFirstMatch("ab", CommonPatternMatchers.whitespace);
				//this.addAllCharactersAsTokenUntilFirstMatch("abc", CommonPatternMatchers.whitespace);
				//this.addAllCharactersAsTokenUntilIndex("abc", 1, true);
				//this.consumeCharacters(1, 10);
				
				if (indexOfLast(LINUXADDRESS) >= 0) {
					IParserToken token = addAllCharactersAsTokenAndConsumeFirstMatch(STACK_MODULE, CommonPatternMatchers.open_paren);
					if (token != null) {
						addAllCharactersAsTokenAndConsumeFirstMatch(STACK_ROUTINE, Pattern.compile("[+-]").matcher(""));
						addPrefixedHexToken(STACK_OFFSET);
					} else {
						addAllCharactersAsTokenAndConsumeFirstMatch(STACK_MODULE, CommonPatternMatchers.whitespace);
					}
					addPrefixedHexToken(STACK_PROC_ADDRESS);
				} else {
					int lastColon = indexOfLast(CommonPatternMatchers.colon);
					if (lastColon >= 0) {
						String name = consumeCharacters(0, lastColon);
						if (name.indexOf("/") != -1) {
							addToken(STACK_MODULE, name);
						} else {
							addToken(STACK_ROUTINE, name);
						}
					}
					addPrefixedHexToken(STACK_PROC_ADDRESS);
					addPrefixedHexToken(STACK_ROUTINE_ADDRESS);
					addPrefixedHexToken(STACK_OFFSET);
				}
			}
		};
		
		addTag(T_1BTSTACKENT, lineRule);
	}
	
}
