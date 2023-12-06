/*******************************************************************************
 * Copyright IBM Corp. and others 2023
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

/**
 * @file compiler.cpp
 * @brief File is compiled into shared library (access)
 * in support of org.openj9.test.util.CompilerAccess.
 */

#include "j9.h"
#include "j9lib.h"
#include "jni.h"

struct NativeMapping
{
	const char *methodName;
	const char *methodSignature;
	const char *implementationName;
	const char *implementationSignature;
};

static const NativeMapping mappings[] =
{
	{
		"commandImpl",
		"(Ljava/lang/Object;)Ljava/lang/Object;",
		"Java_java_lang_Compiler_commandImpl",
		"LLLL"
	},
	{
		"compileClassImpl",
		"(Ljava/lang/Class;)Z",
		"Java_java_lang_Compiler_compileClassImpl",
		"ZLLL"
	},
	{
		"compileClassesImpl",
		"(Ljava/lang/String;)Z",
		"Java_java_lang_Compiler_compileClassesImpl",
		"ZLLL"
	},
	{
		"disable",
		"()V",
		"Java_java_lang_Compiler_disable",
		"VLL"
	},
	{
		"enable",
		"()V",
		"Java_java_lang_Compiler_enable",
		"VLL"
	},
};

#define METHOD_COUNT (sizeof(mappings) / sizeof(mappings[0]))

extern "C" jboolean
Java_org_openj9_test_util_CompilerAccess_registerNatives(JNIEnv *env, jclass clazz)
{
	jboolean result = JNI_FALSE;
	PORT_ACCESS_FROM_ENV(env);
	uintptr_t j9jcl = 0;
	JNINativeMethod methods[METHOD_COUNT];

	if ((0 != j9sl_open_shared_library((char *)J9_JAVA_SE_DLL_NAME, &j9jcl, OMRPORT_SLOPEN_DECORATE))
		|| (0 == j9jcl)
	) {
		j9tty_printf(PORTLIB,"CompilerAccess: could not open %s.\n", J9_JAVA_SE_DLL_NAME);
		goto done;
	}

	for (uintptr_t i = 0; i < METHOD_COUNT; ++i) {
		const NativeMapping *mapping = &mappings[i];
		JNINativeMethod *method = &methods[i];
		uintptr_t function = 0;

		if ((0 != j9sl_lookup_name(
				j9jcl,
				(char *)mapping->implementationName,
				&function,
				(char *)mapping->implementationSignature))
			|| (0 == function)
		) {
			j9tty_printf(PORTLIB,"CompilerAccess: '%s' not found.\n", mapping->implementationName);
			goto done;
		}

		method->name      = (char *)mapping->methodName;
		method->signature = (char *)mapping->methodSignature;
		method->fnPtr     = (void *)function;
	}

	if (JNI_OK != env->RegisterNatives(clazz, methods, METHOD_COUNT)) {
		j9tty_printf(PORTLIB,"CompilerAccess: RegisterNatives failed.\n");
		goto done;
	}

	result = JNI_TRUE;

done:
	if (0 != j9jcl) {
		j9sl_close_shared_library(j9jcl);
	}

	return result;
}
