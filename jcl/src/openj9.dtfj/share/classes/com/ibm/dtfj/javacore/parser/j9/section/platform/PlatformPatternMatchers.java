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
import com.ibm.dtfj.javacore.parser.j9.section.common.CommonPatternMatchers;

public class PlatformPatternMatchers {

	public static final Matcher Windows_XP = CommonPatternMatchers.generateMatcher("Windows XP", Pattern.CASE_INSENSITIVE);
	public static final Matcher Windows_Server_2003 = CommonPatternMatchers.generateMatcher("Windows Server 2003", Pattern.CASE_INSENSITIVE);
	public static final Matcher Windows_Server_2008 = CommonPatternMatchers.generateMatcher("Windows Server 2008", Pattern.CASE_INSENSITIVE);
	public static final Matcher Windows_2000 = CommonPatternMatchers.generateMatcher("Windows 2000", Pattern.CASE_INSENSITIVE);
	public static final Matcher Windows_Vista = CommonPatternMatchers.generateMatcher("Windows Vista", Pattern.CASE_INSENSITIVE);
	public static final Matcher Windows_7 = CommonPatternMatchers.generateMatcher("Windows 7", Pattern.CASE_INSENSITIVE);
	public static final Matcher Windows_8 = CommonPatternMatchers.generateMatcher("Windows 8", Pattern.CASE_INSENSITIVE);
	public static final Matcher Windows_Server_8 = CommonPatternMatchers.generateMatcher("Windows Server 8", Pattern.CASE_INSENSITIVE);
	public static final Matcher Windows_Server_2012 = CommonPatternMatchers.generateMatcher("Windows Server 2012", Pattern.CASE_INSENSITIVE);
	/* Windows_Generic is a catch all for versions of Windows we that haven't yet been released.
	 * New versions should be added above as soon as we find the strings that identify them. */
	public static final Matcher Windows_Generic = CommonPatternMatchers.generateMatcher("Windows", Pattern.CASE_INSENSITIVE);
	public static final Matcher Linux = CommonPatternMatchers.generateMatcher("Linux", Pattern.CASE_INSENSITIVE);
	public static final Matcher AIX = CommonPatternMatchers.generateMatcher("AIX", Pattern.CASE_INSENSITIVE);
	public static final Matcher z_OS = CommonPatternMatchers.generateMatcher("z/OS", Pattern.CASE_INSENSITIVE);
	public static final Matcher J9Signal_1 = CommonPatternMatchers.generateMatcher("J9Generic_Signal:", Pattern.CASE_INSENSITIVE);
	public static final Matcher J9Signal_2 = CommonPatternMatchers.generateMatcher("J9Generic_Signal_Number:", Pattern.CASE_INSENSITIVE);
	public static final Matcher Module = CommonPatternMatchers.generateMatcher("Module:", Pattern.CASE_INSENSITIVE);
	public static final Matcher Module_base = CommonPatternMatchers.generateMatcher("Module_base_address:", Pattern.CASE_INSENSITIVE);
	public static final Matcher Module_offset = CommonPatternMatchers.generateMatcher("Module_offset:", Pattern.CASE_INSENSITIVE);


}
