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
package com.ibm.dtfj.javacore.parser.j9.section.thread;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.ibm.dtfj.javacore.parser.j9.section.common.CommonPatternMatchers;

public abstract class ThreadPatternMatchers {
	public static final Matcher native_method_string = CommonPatternMatchers.generateMatcher("Native Method", Pattern.CASE_INSENSITIVE);
	public static final Matcher compiled_code = CommonPatternMatchers.generateMatcher("Compiled Code", Pattern.CASE_INSENSITIVE);
	public static final Matcher scope = CommonPatternMatchers.generateMatcher("scope", Pattern.CASE_INSENSITIVE);
	public static final Matcher bytecode_pc = CommonPatternMatchers.generateMatcher("Bytecode PC", Pattern.CASE_INSENSITIVE);
	public static final Matcher cpu_time = CommonPatternMatchers.generateMatcher("\\d+\\.\\d+");
	public static final Matcher cpu_time_total = CommonPatternMatchers.generateMatcher("total:\\s+");
	public static final Matcher cpu_time_user = CommonPatternMatchers.generateMatcher("user:\\s+");
	public static final Matcher cpu_time_system = CommonPatternMatchers.generateMatcher("system:\\s+");
	public static final Matcher vmstate = CommonPatternMatchers.generateMatcher("vmstate", Pattern.CASE_INSENSITIVE);
	public static final Matcher vmflags = CommonPatternMatchers.generateMatcher("vm thread flags", Pattern.CASE_INSENSITIVE);
}
