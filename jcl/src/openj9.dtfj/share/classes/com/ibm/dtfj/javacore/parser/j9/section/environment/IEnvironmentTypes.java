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
package com.ibm.dtfj.javacore.parser.j9.section.environment;

public interface IEnvironmentTypes {
	
	public static final String ENVIRONMENT_SECTION = "ENVINFO";
	
	public static final String T_1CIJAVAVERSION = "1CIJAVAVERSION";
	public static final String T_1CIVMVERSION = "1CIVMVERSION"; 
	public static final String T_1CIJITVERSION = "1CIJITVERSION";
	public static final String T_1CIGCVERSION = "1CIGCVERSION";	
	public static final String T_1CIJITMODES = "1CIJITMODES";
	public static final String T_1CIRUNNINGAS = "1CIRUNNINGAS";  
	public static final String T_1CIPROCESSID = "1CIPROCESSID";
	public static final String T_1CICMDLINE = "1CICMDLINE";   
	public static final String T_1CIJAVAHOMEDIR = "1CIJAVAHOMEDIR";
	public static final String T_1CIJAVADLLDIR = "1CIJAVADLLDIR";
	public static final String T_1CISYSCP = "1CISYSCP";      
	public static final String T_1CIUSERARGS = "1CIUSERARGS";  
	public static final String T_2CIUSERARG = "2CIUSERARG";    
	public static final String T_1CIJVMMI = "1CIJVMMI";  
	public static final String T_2CIJVMMIOFF = "2CIJVMMIOFF";
	public static final String T_1CIENVVARS = "1CIENVVARS";
	public static final String T_2CIENVVAR = "2CIENVVAR";
	public static final String T_1CISTARTTIME = "1CISTARTTIME";
	public static final String T_1CISTARTNANO = "1CISTARTNANO";
	
	/*
	 * Attributes
	 */
	public static final String CMD_LINE = "environment_cmd_line";
	public static final String PID_STRING = "environment_pid_string";
	public static final String ARG_STRING = "environment_arg_string";
	public static final String ARG_EXTRA = "environment_arg_extra";
	public static final String JIT_MODE = "environment_jit_mode";
	public static final String START_TIME = "environment_start_time";
	public static final String START_NANO = "environment_start_nano";
	
	public static final String ENV_NAME = "environment_variable_name";
	public static final String ENV_VALUE = "environment_variable_value";
}
