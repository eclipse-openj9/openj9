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

public interface IPlatformTypes {

	public static final String PLATFORM_SECTION = "GPINFO";
	public static final String T_2XHHOSTNAME = "2XHHOSTNAME";
	public static final String T_2XHOSLEVEL = "2XHOSLEVEL";
	public static final String T_2XHCPUS = "2XHCPUS";
	public static final String T_3XHCPUARCH = "3XHCPUARCH";
	public static final String T_3XHNUMCPUS = "3XHNUMCPUS";
	public static final String T_3XHNUMASUP = "3XHNUMASUP";
	public static final String T_1XHEXCPCODE = "1XHEXCPCODE";
	public static final String T_1XHERROR2 = "1XHERROR2";
	
	public static final String T_1XHEXCPMODULE = "1XHEXCPMODULE";

	public static final String T_1XHREGISTERS = "1XHREGISTERS";
	public static final String T_2XHREGISTER = "2XHREGISTER";

	public static final String T_1XHENVVARS = "1XHENVVARS";
	public static final String T_2XHENVVAR = "1XHENVVAR";

	/*
	 * Attributes
	 */
	public static final String PL_HOST_NAME = "platform_host_name";
	public static final String PL_HOST_ADDR = "platform_host_addr";
	public static final String PL_OS_NAME = "platform_os_name";
	public static final String PL_OS_VERSION = "platform_os_version";
	public static final String PL_CPU_ARCH = "platform_cpu_arch";
	public static final String PL_CPU_COUNT = "platform_cpu_count";
	public static final String PL_SIGNAL = "platform_signal";
	
	public static final String PL_MODULE_NAME = "platform_module_name";
	public static final String PL_MODULE_BASE = "platform_module_base";
	public static final String PL_MODULE_OFFSET = "platform_module_offset";

	public static final String PL_REGISTER_NAME = "platform_register_name";
	public static final String PL_REGISTER_VALUE = "platform_register_value";

	public static final String ENV_NAME = "environment_variable_name";
	public static final String ENV_VALUE = "environment_variable_value";
}
