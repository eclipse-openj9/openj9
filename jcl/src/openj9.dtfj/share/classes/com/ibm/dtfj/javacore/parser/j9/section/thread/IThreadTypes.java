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

public interface IThreadTypes {
	
	
	public static final String THREAD_SECTION = "THREADS";
	/*
	 * Thread section tags
	 */
	public static final String T_1XMCURTHDINFO = "1XMCURTHDINFO";
	public static final String T_1XMTHDINFO = "1XMTHDINFO";
	public static final String T_2XMFULLTHDDUMP = "2XMFULLTHDDUMP";
	public static final String T_3XMTHREADINFO = "3XMTHREADINFO";
	public static final String T_3XMTHREADINFO1 = "3XMTHREADINFO1";
	public static final String T_3XMTHREADINFO2 = "3XMTHREADINFO2";
	public static final String T_3XMTHREADINFO3 = "3XMTHREADINFO3";
	public static final String T_4XESTACKTRACE = "4XESTACKTRACE";
	public static final String T_4XENATIVESTACK = "4XENATIVESTACK";
	public static final String T_3XMTHREADBLOCK = "3XMTHREADBLOCK";
	public static final String T_3XMCPUTIME = "3XMCPUTIME";

	
	/*
	 * Attributes
	 */
	
	/*
	 * VM thread id struct has a pointer to a corresponding abstract thread id
	 * that represents an OS thread (native thread). The abstract thread id
	 * then has a pointer to an actual native thread. 
	 * 
	 * A VM thread may or may not be associated with a Java thread object (gc VM threads
	 * have no associated Java thread object).
	 * 
	 * Hierarchy:
	 * Java Thread Object (not found in javacore files)
	 * VM Thread (typically an internal data structure like jniENV or J9JVMThread
	 * Abstract VM Thread
	 * Native (OS) Thread
	 */
	
	public static final String VM_THREAD_ID = "vm_thread_id";
	public static final String ABSTRACT_THREAD_ID = "abstract_thread_id";
	public static final String JAVA_THREAD_OBJ = "java_thread_obj";
	public static final String NATIVE_THREAD_ID = "native_thread_id";
	
	/*
	 * More attributes
	 */
	public static final String UNPARSED_ENV_INFORMATION = "unparsed_env";
	public static final String JAVA_THREAD_NAME = "java_thread_name";
	
	public static final String JAVA_STATE = "java_thread_state";
	public static final String VM_THREAD_PRIORITY = "vm_thread_priority";
	
	public static final String NATIVE_THREAD_PRIORITY = "native_thread_priority";
	public static final String NATIVE_THREAD_POLICY = "native_thread_policy";
	public static final String SCOPE = "scope";
	public static final String VM_STATE = "vm_thread_state";
	public static final String VM_FLAGS = "vm_thread_flags";

	public static final String NATIVE_STACK_FROM = "native_stack_from";
	public static final String NATIVE_STACK_TO = "native_stack_to";
	public static final String NATIVE_STACK_SIZE = "native_stack_size";
	
	public static final String FULL_NAME = "full_location_name";
	public static final String METHOD_NAME = "method_name";
	public static final String CLASS_FILE_NAME = "class_file_name";
	public static final String STACKTRACE_LINE_NUMBER = "stacktrace_line_number";
	public static final String STACKTRACE_METHOD_TYPE = "stacktrace_method_type";
	public static final String STACKTRACE_NATIVE_METHOD = "stacktrace_native_method";
	public static final String STACKTRACE_JAVA_METHOD = "stacktrace_java_method";
	public static final String COMPILATION_LEVEL = "comp_level";
	
	public static final String COMPILED = "compiled";
	public static final String NON_COMPILED = "non_compiled";
	public static final String UNKNOWN_COMPILATION_LEVEL = "unknown_comp_lvl";
	
	public static final String STACK_THREAD = "stack_thread";
	public static final String STACK_MODULE = "stack_module";
	public static final String STACK_ROUTINE = "stack_routine";
	public static final String STACK_MODULE_OFFSET = "stack_module_offset";
	public static final String STACK_ROUTINE_OFFSET = "stack_routine_offset";
	public static final String STACK_PROC_ADDRESS = "stack_proc_address";
	public static final String STACK_ROUTINE_ADDRESS = "stack_routine_address";
	public static final String STACK_FILE = "stack_file";
	public static final String STACK_LINE = "stack_line";
	
	public static final String BLOCKER_OBJECT_FULL_JAVA_NAME = "blocker_object_full_java_name";
	public static final String BLOCKER_OBJECT_ADDRESS = "blocker_object_address";
	
	public static final String CPU_TIME_TOTAL = "cpu_time_total";
	public static final String CPU_TIME_USER = "cpu_time_user";
	public static final String CPU_TIME_SYSTEM = "cpu_time_system";
}
