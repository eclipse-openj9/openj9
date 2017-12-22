/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
#if !defined(BYTECODEACTION_HPP_)
#define BYTECODEACTION_HPP_

/*
 * Important: To work around an XLC compiler bug, the cases in PERFORM_ACTION in
 * BytecodeInterpreter.hpp must be in the identical order to their declaration here.
 *
 * TODO: re-test with current compilers.
 */
typedef enum {
	EXECUTE_BYTECODE,
	GOTO_RUN_METHOD,
	GOTO_NOUPDATE,
	GOTO_DONE,
	GOTO_BRANCH_WITH_ASYNC_CHECK,
	GOTO_THROW_CURRENT_EXCEPTION,
	GOTO_ASYNC_CHECK,
	GOTO_JAVA_STACK_OVERFLOW,
	GOTO_RUN_METHODHANDLE,
	GOTO_RUN_METHODHANDLE_COMPILED,
	GOTO_RUN_METHOD_FROM_METHOD_HANDLE,
	THROW_MONITOR_ALLOC_FAIL,
	THROW_HEAP_OOM,
	THROW_NEGATIVE_ARRAY_SIZE,
	THROW_NPE,
	THROW_AIOB,
	THROW_ARRAY_STORE,
	THROW_DIVIDE_BY_ZERO,
	THROW_ILLEGAL_MONITOR_STATE,
	THROW_INCOMPATIBLE_CLASS_CHANGE,
	THROW_WRONG_METHOD_TYPE,
	PERFORM_DLT,
	RUN_METHOD_INTERPRETED,
	RUN_JNI_NATIVE,
	RUN_METHOD_COMPILED,
	/* All values after this line are for the debug interpreter only - add general values above this line */
	GOTO_EXECUTE_BREAKPOINTED_BYTECODE,
	HANDLE_POP_FRAMES,
	REPORT_METHOD_ENTER,
	FALL_THROUGH,
} VM_BytecodeAction;

#endif /* BYTECODEACTION_HPP_ */
