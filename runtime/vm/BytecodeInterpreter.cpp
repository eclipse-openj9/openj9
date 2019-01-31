/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#include "clang_comp.h"
#include "j9.h"

#if defined(_MSC_VER) && !defined(__clang__)
/* MSVC compiler hangs on this file at max opt (/Ox) */
#pragma optimize( "g", off )
#endif /* _MSC_VER */

#if defined(DEBUG_VERSION)
#define LOOP_NAME debugBytecodeLoop
#else
#define LOOP_NAME bytecodeLoop
#endif

/* USE_COMPUTED_GOTO on Windows has a performance improvement of 15%.
 * USE_COMPUTED_GOTO on Linux amd64 has a performance improvement of 3%.
 * USE_COMPUTED_GOTO is enabled on Linux s390 when compiling with gcc7+.
 * USE_COMPUTED_GOTO improves startup performance by ~10% on Linux s390.
 */
#if (defined(WIN32) && defined(__clang__))
#define USE_COMPUTED_GOTO
#elif (defined(LINUX) && defined(J9HAMMER))
#define USE_COMPUTED_GOTO
#elif (defined(LINUX) && defined(S390) && (__GNUC__ >= 7))
#define USE_COMPUTED_GOTO
#elif defined(OSX)
#define USE_COMPUTED_GOTO
#else
#undef USE_COMPUTED_GOTO
#endif

#undef TRACE_TRANSITIONS

#if defined(TRACE_TRANSITIONS)
#include "bcnames.h"
#include "rommeth.h"

static void
getMethodName(J9PortLibrary *PORTLIB, J9Method *method, U_8 *pc, char *buffer)
{
	char temp[1024];
	buffer[0] = '\0';
	if ((UDATA)pc <= J9SF_MAX_SPECIAL_FRAME_TYPE) {
		j9str_printf(PORTLIB, temp, sizeof(temp), "SPECIAL %d", pc);
		strcat(buffer, temp);
	} else if (((U_8*)-1 != pc) && (JBimpdep1 == *pc)) {
		strcat(buffer, "MHMAGIC");
	} else if (((U_8*)-1 != pc) && (JBimpdep2 == *pc)) {
		strcat(buffer, "CALLIN");
	} else if (((U_8*)-1 != pc) && (((*pc >= JBretFromNative0) && (*pc <= JBreturnFromJ2I)) || (JBreturnFromJ2I == *pc))) {
		strcat(buffer, "JITRETURN");
	} else if (NULL == method->bytecodes) {
		j9str_printf(PORTLIB, temp, sizeof(temp), "(no bytecodes) (%p)", method);
		strcat(buffer, temp);
	} else {
		J9UTF8 *className = J9ROMCLASS_CLASSNAME(J9_CLASS_FROM_METHOD(method)->romClass);
		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
		J9UTF8 *methodName = J9ROMMETHOD_NAME(romMethod);
		J9UTF8 *methodSig = J9ROMMETHOD_SIGNATURE(romMethod);
		if (romMethod->modifiers & J9AccNative) {
			if ((UDATA)method->constantPool & J9_STARTPC_JNI_NATIVE) {
				strcat(buffer, "JNI ");
			} else {
				strcat(buffer, "INL ");			
			}
		}
		if (0 == ((UDATA)method->extra & 1)) {
			strcat(buffer, "JITTED ");
		}
		j9str_printf(PORTLIB, temp, sizeof(temp), "%.*s.%.*s%.*s (%p)", className->length, className->data, methodName->length, methodName->data, methodSig->length, methodSig->data, method);
		strcat(buffer, temp);
		if ((U_8*)-1 != pc) {
			j9str_printf(PORTLIB, temp, sizeof(temp), " @ %p (offset %d)", pc, pc - method->bytecodes);
			strcat(buffer, temp);
		}
	}
}
#endif

#include "BytecodeInterpreter.hpp"

/* Entry point must be C, not C++ */
extern "C" UDATA
LOOP_NAME(J9VMThread *currentThread)
{
	INTERPRETER_CLASS interpreter(currentThread);
	return interpreter.run(currentThread);
}
