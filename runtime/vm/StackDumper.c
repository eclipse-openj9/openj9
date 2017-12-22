/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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

#include "j9.h"
#include "j9protos.h"
#include "j9consts.h"
#include "vm_internal.h"
#include "rommeth.h"

typedef void (*printFunction_t)(void *cookie, const char *format, ...);

J9_DECLARE_CONSTANT_UTF8(unknownClassNameUTF, "(unknown class)");

/*
 * Helper function, called for each stack frame.  Assumptions:
 *	1) current thread and walk state are passed as parameters.
 *	2) walkState->userData1 is a flags field that controls the walk
 *	3) walkState->userData2 is a pointer to the printFunction(cookie,format,...)
 *	4) walkState->userData3 is a cookie passed to the printFunction
 *	5) walkState->userData4 is the end-of-line string
 */
UDATA
genericStackDumpIterator(J9VMThread *currentThread, J9StackWalkState *walkState)
{
	printFunction_t printFunction = (printFunction_t)walkState->userData2;
	void *cookie = walkState->userData3;
	const char *endOfLine = (const char *)walkState->userData4;
	J9UTF8 *className = (J9UTF8*)&unknownClassNameUTF;
	J9ConstantPool *constantPool = walkState->constantPool;
	J9Method *method = walkState->method;
	U_8 *pc = walkState->pc;
	if (NULL != constantPool) {
		className = J9ROMCLASS_CLASSNAME(constantPool->ramClass->romClass);
	}
	if (NULL == method) {
		printFunction(cookie, "0x%p %.*s (unknown method)%s", pc, J9UTF8_LENGTH(className), J9UTF8_DATA(className), endOfLine);
	} else {
		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
		J9UTF8 *name = J9ROMMETHOD_NAME(romMethod);
		J9UTF8 *sig = J9ROMMETHOD_SIGNATURE(romMethod);
		if (NULL == walkState->jitInfo) {
			if (J9_ARE_ANY_BITS_SET(romMethod->modifiers, J9AccNative)) {
				printFunction(cookie, " NATIVE   %.*s.%.*s%.*s%s",
						J9UTF8_LENGTH(className), J9UTF8_DATA(className),
						J9UTF8_LENGTH(name), J9UTF8_DATA(name),
						J9UTF8_LENGTH(sig), J9UTF8_DATA(sig),
						endOfLine);
			} else {
				printFunction(cookie, " %08x %.*s.%.*s%.*s%s",
						pc - method->bytecodes,
						J9UTF8_LENGTH(className), J9UTF8_DATA(className),
						J9UTF8_LENGTH(name), J9UTF8_DATA(name),
						J9UTF8_LENGTH(sig), J9UTF8_DATA(sig),
						endOfLine);
			}
		} else {
			if (0 == walkState->inlineDepth) {
				printFunction(cookie, " %08x %.*s.%.*s%.*s  (@%p)%s",
						pc - (U_8*)method->extra,
						J9UTF8_LENGTH(className), J9UTF8_DATA(className),
						J9UTF8_LENGTH(name), J9UTF8_DATA(name),
						J9UTF8_LENGTH(sig), J9UTF8_DATA(sig),
						pc,
						endOfLine);
			} else {
				printFunction(cookie, " INLINED  %.*s.%.*s%.*s  (@%p)%s",
						J9UTF8_LENGTH(className), J9UTF8_DATA(className),
						J9UTF8_LENGTH(name), J9UTF8_DATA(name),
						J9UTF8_LENGTH(sig), J9UTF8_DATA(sig),
						pc,
						endOfLine);
			}
		}
	}
	return J9_STACKWALK_KEEP_ITERATING;
}

void
dumpStackTrace(J9VMThread *currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9PortLibrary *portLibrary = vm->portLibrary;
	J9StackWalkState walkState;
	walkState.walkThread = currentThread;
	walkState.frameWalkFunction = genericStackDumpIterator;
	walkState.userData1 = (void*)0;
	walkState.userData2 = (void*)OMRPORT_FROM_J9PORT(portLibrary)->tty_printf;
	walkState.userData3 = (void*)portLibrary;
	walkState.userData4 = (void*)"\n";
	walkState.skipCount = 0;
	walkState.flags = J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_ITERATE_FRAMES;
	vm->walkStackFrames(currentThread, &walkState);
}
