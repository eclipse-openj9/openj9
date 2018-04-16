/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package jit.test.jitt.codecache;

public class Constants {

	public static final String PREFIX_JITTED = "+" ;

	public static final String AOT_LOAD = "+ (AOT load)";

	public static final String CC_ALLOCATED = "# CodeCache allocated";

	public static final String CC_MAX_ALLOCATED = "# CodeCache maximum allocated";

	public static final String METHOD_RECLAIMED = "# Reclaimed";

	public static final String METHOD_RECOMPILED = "# Recompile";

	public static final String METHOD_UNLOADED = "# Unloaded";

	public static final int BASIC_TEST = 1;

	public static final int BOUNDARY_TEST = 2;

	public static final int AOT_TEST = 3;

	public static final String ADVANCED_TRAMPOLINE_TEST_METHOD_SIG_CALLEE = "jit/test/codecache/TargetClass_AT.callee(I)I";

	public static final String ADVANCED_TRAMPOLINE_TEST_METHOD_SIG_CALLER = "jit/test/codecache/Caller1_AT.caller()I";

	public static final String BASIC_TRAMPOLINE_TEST_METHOD_SIG_CALLEE = "jit/test/codecache/TargetClass_BT.callee(I)I";

	public static final String BASIC_TRAMPOLINE_TEST_METHOD_SIG_CALLER2 = "jit/test/codecache/Caller2_BT.caller()I";

	public static final String IPIC_TRAMPOLINE_TEST_IADDERIMPL_STR = "jit/test/codecache/IAdderImpl_String.add(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;";

	public static final String IPIC_TRAMPOLINE_TEST_IADDERIMPL_INT = "jit/test/codecache/IAdderImpl_Int.add(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;";

	public static final String AOT_TEST_TARGET_METHOD_SIG = "jit/test/codecache/TargetClass_AotTest.callee(I)I";

	public static final String JAR_DIR =  "jardir" ;

	public static final String CCTEST_BASIC = "basic";

	public static final String CCTEST_BOUNDARY = "boundary";

	public static final String CCTEST_TRAMPOLINE_ADVANCED = "trampoline_advanced";

	public static final String CCTEST_TRAMPOLINE_BASIC = "trampoline_basic";

	public static final String CCTEST_AOT_PHASE1 = "aotload";

	public static final String CCTEST_AOT_PHASE2 = "aotcc";

	public static final String CCTEST_JITHELPER = "jithelper";

	public static final String CCTEST_SPACE_REUSE = "spacereuse";

	public static final String CCTEST_TRAMPOLINE_IPIC = "trampoline_ipic";

}
