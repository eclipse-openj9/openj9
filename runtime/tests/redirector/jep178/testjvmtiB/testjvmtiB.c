/*******************************************************************************
 * Copyright (c) 2014, 2014 IBM Corp. and others
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

/**
 * @file testjvmtiB.c
 * @brief File is to be compiled into a shared library (testjvmtiB).  All the symbols should
 * be exported and visible for external invocations.
 */

#include <stdio.h>
#include "jni.h"

/**
 * @brief Function indicates to the runtime that the library testjvmtiB has been
 * linked into the executable.
 */
jint JNICALL Agent_OnLoad(JavaVM *vm, char *options, void *reserved)
{
	fprintf(stdout, "[MSG] Reached OnLoad (testjvmtiB): Agent_OnLoad [dynamically]\n");
	fflush(stdout);
	/* Return success. */
	return 0;
}

jint JNICALL Agent_OnAttach(JavaVM *vm, char *options, void *reserved)
{
	fprintf(stdout, "[MSG] Reached OnLoad (testjvmtiB): Agent_OnAttach [dynamically]\n");
	fflush(stdout);
	/* Return success. */
	return 0;
}

/**
 * @brief Function indicates an alternative to the traditional unload routine to
 * the runtime specifically targeting the library testjvmtiB.
 */
void JNICALL Agent_OnUnload(JavaVM *vm)
{
	fprintf(stdout, "[MSG] Reached OnUnload (testjvmtiB): Agent_OnUnload [dynamically]\n");
	fflush(stdout);
	/* Nothing much to cleanup here. */
	return;
}
