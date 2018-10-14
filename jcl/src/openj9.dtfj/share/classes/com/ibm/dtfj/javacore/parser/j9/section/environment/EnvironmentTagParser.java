/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2018 IBM Corp. and others
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
package com.ibm.dtfj.javacore.parser.j9.section.environment;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.ibm.dtfj.javacore.parser.framework.tag.ILineRule;
import com.ibm.dtfj.javacore.parser.framework.tag.LineRule;
import com.ibm.dtfj.javacore.parser.framework.tag.TagParser;
import com.ibm.dtfj.javacore.parser.j9.section.common.CommonPatternMatchers;
import com.ibm.dtfj.javacore.parser.j9.section.common.ICommonTypes;

/**
 * Parser rules for lines in the environment (CI) section in the javacore
 */
public class EnvironmentTagParser extends TagParser implements
		IEnvironmentTypes {

	public EnvironmentTagParser() {
		super(ENVIRONMENT_SECTION);
	}
	
	/**
	 * Initialize parser with rules for lines in the environment (CI) section in 
	 * the javacore
	 */
	protected void initTagAttributeRules() {
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				// version is on a single line, can include whitespace, separators etc
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addToken(ARG_STRING, CommonPatternMatchers.allButControlChars);
			}
		};
		ILineRule lineRule2 = new LineRule() {
			public void processLine(String source, int startingOffset) {
				// 1CIJAVAHOMEDIR Java Home Dir:   /home/rtjaxxon/sdk/ibm-wrt-i386-60/jre
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				consumeUntilFirstMatch(CommonPatternMatchers.colon);
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addToken(ARG_STRING, CommonPatternMatchers.allButControlChars);
			}
		};
		ILineRule lineRule3 = new LineRule() {
			public void processLine(String source, int startingOffset) {
				String bits = "32";
				CommonPatternMatchers.build_string.reset(source);
				if (CommonPatternMatchers.build_string.find()) {
					int build = CommonPatternMatchers.build_string.start();
					String version = source.substring(0, build);
					CommonPatternMatchers.bits64.reset(version);
					if (CommonPatternMatchers.bits64.find()) {
						bits = "64";
					} else {
						CommonPatternMatchers.s390.reset(version);
						if (CommonPatternMatchers.s390.matches()) {
							bits = "31";
						}
					}
				}
				addToken(ICommonTypes.POINTER_SIZE, bits);
				// version is on a single line, can include whitespace, separators etc
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addToken(ARG_STRING, CommonPatternMatchers.allButLineFeed);
			}
		};
		
		addTag(T_1CIJAVAVERSION, lineRule3);
		addTag(T_1CIVMVERSION, lineRule);
		addTag(T_1CIJITVERSION, lineRule);
		addTag(T_1CIGCVERSION, lineRule);
		addJITModesRule();
		addTag(T_1CIRUNNINGAS, null);
		addProcessIDRule(); 
		addCmdLineRule(); 
		addTag(T_1CIJAVAHOMEDIR, lineRule2);
		addTag(T_1CIJAVADLLDIR, lineRule2);
		addTag(T_1CISYSCP, lineRule2);
		
		addTag(T_1CIUSERARGS, lineRule);
		addUserArgs();

		addTag(T_1CIJVMMI, null);
		addTag(T_2CIJVMMIOFF, null);
		
		addTag(T_1CIENVVARS, lineRule);
		addEnvironmentVars();
		
		addStartTimeRule();
		addStartTimeNanoRule();
	}
	
	/**
	 * Add rule for the command line (2CICMDLINE tag)
	 */
	private void addCmdLineRule() {
		
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				// command line is on a single line, can include whitespace, separators etc
				// Java 7.0 - can get [not available]
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addToken(CMD_LINE, CommonPatternMatchers.allButLineFeed);
			}
		};
		addTag(T_1CICMDLINE, lineRule);		
	}
	
	/**
	 * Add rule for the process id (2CIPROCESSID tag)
	 */
	private void addProcessIDRule() {
		
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				// pid is in decimal then hex, we only need the first one.
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addToken(PID_STRING, CommonPatternMatchers.dec);
			}
		};
		addTag(T_1CIPROCESSID, lineRule);		
	}
		
	/**
	 * Add rule for the JIT modes (2CICMDLINE tag)
	 */
	private void addJITModesRule() {
		
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				// command line is on a single line, can include whitespace, separators etc
				// Java 7.0 - can get [not available]
				addToken(JIT_MODE, CommonPatternMatchers.allButLineFeed);
			}
		};
		addTag(T_1CIJITMODES, lineRule);		
	}
	
	/**
	 * Add rule for an individual user args line (2CIUSERARG tag)
	 */
	private void addUserArgs() {
		final Matcher hexEnd = CommonPatternMatchers.generateMatcher(" 0x\\p{XDigit}+[\\r\\n]*$");
		
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				// each user arg setting is on a separate line, comprising a string after 
				// some whitespace, with optional extra information (hex address) following
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				if (addAllCharactersAsTokenUntilFirstMatch(ARG_STRING, hexEnd) != null) {
					addPrefixedHexToken(ARG_EXTRA);
				} else {
					addToken(ARG_STRING, CommonPatternMatchers.allButLineFeed);
				}
			}
		};
		addTag(T_2CIUSERARG, lineRule);		
	}
	
	/**
	 * Add rule for an individual environment variable tags
	 */
	private static final Matcher NOT_EQUALS = CommonPatternMatchers.generateMatcher("[^\\n\\r][^=\\n\\r]*", Pattern.CASE_INSENSITIVE);

	private void addEnvironmentVars() {
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addToken(ENV_NAME, NOT_EQUALS);
				if (consumeUntilFirstMatch(CommonPatternMatchers.equals)) {
					addToken(ENV_VALUE, CommonPatternMatchers.allButLineFeed);
				}
			}
		};
		addTag(T_2CIENVVAR, lineRule);
	}
	
	/**
	 * Add rule for the JVM start time line
	 * 1CISTARTTIME   JVM start time: 2015/07/17 at 13:31:04:547
	 */
	private void addStartTimeRule() {
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				consumeUntilFirstMatch(CommonPatternMatchers.colon);
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addToken(START_TIME, CommonPatternMatchers.allButLineFeed);
			}
		};
		addTag(T_1CISTARTTIME, lineRule);
	}
	
	/**
	 * Add rule for the JVM start nanotime line
	 * 1CISTARTNANO   JVM start nanotime: 3534023113503
	 */
	private void addStartTimeNanoRule() {
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				// 1TINANOTIME    System nanotime: 3534320355422
				consumeUntilFirstMatch(CommonPatternMatchers.colon);
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addToken(START_NANO, CommonPatternMatchers.allButLineFeed);
			}
		};
		addTag(T_1CISTARTNANO, lineRule);
	}
}
