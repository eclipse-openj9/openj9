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
package com.ibm.dtfj.javacore.parser.j9.section.platform;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.ibm.dtfj.javacore.parser.framework.tag.ILineRule;
import com.ibm.dtfj.javacore.parser.framework.tag.LineRule;
import com.ibm.dtfj.javacore.parser.framework.tag.TagParser;
import com.ibm.dtfj.javacore.parser.j9.section.common.CommonPatternMatchers;

public class PlatformTagParser extends TagParser implements IPlatformTypes {
	
	public PlatformTagParser() {
		super(PLATFORM_SECTION);
	}

	/**
	 * Initialize parser with rules for lines in the host platform (XH) section in 
	 * the javacore
	 */
	protected void initTagAttributeRules() {

		addHostName();
		addOSLevel();
		addTag(T_2XHCPUS, null);

		addCPUArch();
		addCPUCount();
		addExceptionCodeRule();
		addTag(T_1XHERROR2, null);
		addExceptionModuleRule();
		addRegisterRule();
		addEnvironmentVars();
	}

	/**
	 * Add rules for the host name
	 * 2XHHOSTNAME    Host             : myhost:9.20.2.67
	 */
	private void addHostName() {
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				consumeUntilFirstMatch(CommonPatternMatchers.colon);
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addAllCharactersAsTokenAndConsumeFirstMatch(PL_HOST_NAME, CommonPatternMatchers.colon);
				addToken(PL_HOST_ADDR, CommonPatternMatchers.allButLineFeed);
			}
		};
		addTag(T_2XHHOSTNAME, lineRule);
	}
	
	/**
	 * Add rules for the OS level information (2XHOSLEVEL line)
	 */
	private void addOSLevel() {
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				// here we get the OS name, which in javacore is concatenated with 
				// the OS level, so we have to pull out the specific OS names
				consumeUntilFirstMatch(CommonPatternMatchers.colon);
				if (findFirst(PlatformPatternMatchers.Windows_XP)) {
					addToken(PL_OS_NAME, PlatformPatternMatchers.Windows_XP);
				} else if (findFirst(PlatformPatternMatchers.Windows_Server_2003)) {
					addToken(PL_OS_NAME, PlatformPatternMatchers.Windows_Server_2003);
				} else if (findFirst(PlatformPatternMatchers.Windows_Server_2008)) {
					addToken(PL_OS_NAME, PlatformPatternMatchers.Windows_Server_2008);
				} else if (findFirst(PlatformPatternMatchers.Windows_2000)) {
					addToken(PL_OS_NAME, PlatformPatternMatchers.Windows_2000);
				} else if (findFirst(PlatformPatternMatchers.Windows_Vista)) {
					addToken(PL_OS_NAME, PlatformPatternMatchers.Windows_Vista);
				} else if (findFirst(PlatformPatternMatchers.Windows_7)) {
					addToken(PL_OS_NAME, PlatformPatternMatchers.Windows_7);
				} else if (findFirst(PlatformPatternMatchers.Windows_8)) {
					addToken(PL_OS_NAME, PlatformPatternMatchers.Windows_8);
				} else if (findFirst(PlatformPatternMatchers.Windows_Server_8)) {
					addToken(PL_OS_NAME, PlatformPatternMatchers.Windows_Server_8);
				} else if (findFirst(PlatformPatternMatchers.Windows_Server_2012)) {
					addToken(PL_OS_NAME, PlatformPatternMatchers.Windows_Server_2012);
				} else if (findFirst(PlatformPatternMatchers.Windows_Generic)) {
					addToken(PL_OS_NAME, PlatformPatternMatchers.Windows_Generic);
				} else if (findFirst(PlatformPatternMatchers.Linux)) {
					addToken(PL_OS_NAME, PlatformPatternMatchers.Linux);
				} else if (findFirst(PlatformPatternMatchers.AIX)) {
					addToken(PL_OS_NAME, PlatformPatternMatchers.AIX);
				} else if (findFirst(PlatformPatternMatchers.z_OS)) {
					addToken(PL_OS_NAME, PlatformPatternMatchers.z_OS);
				}
				// if we did not find a recognised OS name the token is not
				// set and will show DataUnavailable through the DTFJ API
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				// grab the remainder of the line as the OS version
				addToken(PL_OS_VERSION, CommonPatternMatchers.allButLineFeed);
			}
		};
		addTag(T_2XHOSLEVEL, lineRule);
	}

	/**
	 * Add rule for the CPU architecture information (3XHCPUARCH line)
	 */
	private void addCPUArch() {
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				// the CPU architecture is just a string following a colon
				consumeUntilFirstMatch(CommonPatternMatchers.colon);
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addToken(PL_CPU_ARCH, CommonPatternMatchers.allButLineFeed);
			}
		};
		addTag(T_3XHCPUARCH, lineRule);
	}

	/**
	 * Add rule for the number of CPUs information (3XHNUMCPUS line)
	 */
	private void addCPUCount() {
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				// the CPU count is a decimal number following a colon
				consumeUntilFirstMatch(CommonPatternMatchers.colon);
				addToken(PL_CPU_COUNT, CommonPatternMatchers.dec);
			}
		};
		addTag(T_3XHNUMCPUS, lineRule);
		addTag(T_3XHNUMASUP, null);
	}
	
	/**
	 * Add rule for J9Generic_Signal/J9Generic_Signal_Number (1XHEXCPCODE line)
	 */
	private void addExceptionCodeRule() {
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				// the signal number is a hex value following "J9Generic_Signal_Number: " or
				// "J9Generic_Signal: "
				if (findFirst(PlatformPatternMatchers.J9Signal_1)) {
					consumeUntilFirstMatch(PlatformPatternMatchers.J9Signal_1);
					addNonPrefixedHexToken(PL_SIGNAL);
				} else if (findFirst(PlatformPatternMatchers.J9Signal_2)) {
					consumeUntilFirstMatch(PlatformPatternMatchers.J9Signal_2);
					addNonPrefixedHexToken(PL_SIGNAL);
				}
			}
		};
		addTag(T_1XHEXCPCODE, lineRule);
	}
	
	/**
	 * Add rule for Windows exception module
	 */
	private void addExceptionModuleRule() {
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				// 1XHEXCPMODULE  Module: C:\Program Files\IBM\java60sr5\jre\bin\jdwp.dll
				// 1XHEXCPMODULE  Module_base_address: 7F3F0000
				// 1XHEXCPMODULE  Offset_in_DLL: 00044340
				if (findFirst(PlatformPatternMatchers.Module)) {
					consumeUntilFirstMatch(PlatformPatternMatchers.Module);
					consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
					addToken(PL_MODULE_NAME, CommonPatternMatchers.allButLineFeed);
				} else if (findFirst(PlatformPatternMatchers.Module_base)) {
					consumeUntilFirstMatch(PlatformPatternMatchers.Module_base);
					addNonPrefixedHexToken(PL_MODULE_BASE);
				} else if (findFirst(PlatformPatternMatchers.Module_offset)) {
					consumeUntilFirstMatch(PlatformPatternMatchers.Module_offset);
					addNonPrefixedHexToken(PL_MODULE_OFFSET);					
				}
			}
		};
		addTag(T_1XHEXCPMODULE, lineRule);
	}
	
	private void addRegisterRule() {
		// 1XHREGISTERS   Registers:
		addTag(T_1XHREGISTERS, new LineRule() {
			public void processLine(String source, int startingOffset) {
			}
		});
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				// 2XHREGISTER      EDI: 41460098
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addAllCharactersAsTokenAndConsumeFirstMatch(PL_REGISTER_NAME, CommonPatternMatchers.colon);
				addNonPrefixedHexToken(PL_REGISTER_VALUE);
			}
		};
		addTag(T_2XHREGISTER, lineRule);		
	}
	
	/**
	 * Add rule for an individual environment variable tags
	 */
	private static final Matcher NOT_EQUALS = CommonPatternMatchers.generateMatcher("[^\\n\\r][^=\\n\\r]*", Pattern.CASE_INSENSITIVE);	
	
	private void addEnvironmentVars() {
		
		addTag(T_1XHENVVARS, new LineRule() {
			public void processLine(String source, int startingOffset) {
			}
		});
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addToken(ENV_NAME, NOT_EQUALS);
				consumeUntilFirstMatch(CommonPatternMatchers.equals);
				addToken(ENV_VALUE, CommonPatternMatchers.allButLineFeed);
			}
		};
		addTag(T_2XHENVVAR, lineRule);
	}
}
