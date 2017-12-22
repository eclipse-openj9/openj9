/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#include <string.h>
#include "j9port.h"
#include "j9.h"
#include "j9protos.h"
#include "j9comp.h"
#include "j9consts.h"
#include "bcnames.h"
#include "bcproftest_internal.h"

#if (defined(J9VM_INTERP_PROFILING_BYTECODES)) 
static void profilingListener (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
#endif /* J9VM_INTERP_PROFILING_BYTECODES */
#if (defined(J9VM_INTERP_PROFILING_BYTECODES)) 
static UDATA parseBuffer (J9VMThread* vmThread, const U_8* dataStart, UDATA size);
#endif /* J9VM_INTERP_PROFILING_BYTECODES */
#if (defined(J9VM_INTERP_PROFILING_BYTECODES)) 
static void shutdownListener (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
#endif /* J9VM_INTERP_PROFILING_BYTECODES */


static UDATA TEST_verbose;
static UDATA TEST_errors;
static UDATA TEST_events;
static UDATA TEST_records;


jint JNICALL 
JVM_OnLoad( JavaVM *jvm, char* options, void *reserved ) 
{
	J9JavaVM* javaVM = (J9JavaVM*)jvm;
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	J9HookInterface** hooks = javaVM->internalVMFunctions->getVMHookInterface(javaVM);

	if (strcmp(options, "verbose") == 0) {
		TEST_verbose = 1;
		j9tty_printf(PORTLIB, "Verbose mode enabled.\n");
	} else if (strcmp(options, "") != 0) {
		j9tty_printf(PORTLIB, "This shared library tests the profiling bytecode capabilities of the J9 VM\n");
		j9tty_printf(PORTLIB, "Valid options are:\n");
		j9tty_printf(PORTLIB, "\tverbose - enable verbose output\n");
		return JNI_ERR;
	}

#if defined (J9VM_INTERP_PROFILING_BYTECODES)

	j9tty_printf(PORTLIB, "Installing profiling bytecode hooks...\n");

	if ((*hooks)->J9HookRegisterWithCallSite(hooks, J9HOOK_VM_PROFILING_BYTECODE_BUFFER_FULL, profilingListener, OMR_GET_CALLSITE(), NULL)) {
		j9tty_printf(PORTLIB, "Error: Unable to install J9HOOK_VM_PROFILING_BYTECODE_BUFFER_FULL listener\n");
		TEST_errors += 1;
		return JNI_ERR;
	} else {
		j9tty_printf(PORTLIB, "Succesfully installed J9HOOK_VM_PROFILING_BYTECODE_BUFFER_FULL listener\n");
	}

	if ((*hooks)->J9HookRegisterWithCallSite(hooks, J9HOOK_VM_SHUTTING_DOWN, shutdownListener, OMR_GET_CALLSITE(), NULL)) {
		j9tty_printf(PORTLIB, "Error: Unable to install J9HOOK_VM_SHUTTING_DOWN listener\n");
		TEST_errors += 1;
		return JNI_ERR;
	} else {
		j9tty_printf(PORTLIB, "Succesfully installed J9HOOK_VM_SHUTTING_DOWN listener\n");
	}

#else

	j9tty_printf(PORTLIB, "Profiling bytecodes are not supported on this VM.\n(J9VM_INTERP_PROFILING_BYTECODES is not defined).\n");
	j9tty_printf(PORTLIB, "Total errors: 0\n");

#endif

	return JNI_OK;
}




#if (defined(J9VM_INTERP_PROFILING_BYTECODES)) 
static void
profilingListener(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMProfilingBytecodeBufferFullEvent* event = eventData;
	J9VMThread* vmThread = event->currentThread;
	const U_8* cursor = event->bufferStart;
	UDATA size = event->bufferSize;
	UDATA records;

	PORT_ACCESS_FROM_VMC(vmThread);

	if (TEST_verbose) {
		j9tty_printf(PORTLIB, "%p - Buffer full: %zu bytes at %p\n", vmThread, size, cursor);
	}
	TEST_events += 1;

	records = parseBuffer(vmThread, cursor, size);
	if (records > 0) {
		TEST_records += records;
		if (TEST_verbose) {
			j9tty_printf(PORTLIB, "Found %d records\n", records);
		}
	} else {
		j9tty_printf(PORTLIB, "An error occurred while parsing the buffer\n");
	}
}

#endif /* J9VM_INTERP_PROFILING_BYTECODES */


#if (defined(J9VM_INTERP_PROFILING_BYTECODES)) 
static UDATA
parseBuffer(J9VMThread* vmThread, const U_8* dataStart, UDATA size)
{
	PORT_ACCESS_FROM_VMC(vmThread);
	const U_8* cursor = dataStart;
	U_8 * pc;
	U_32 switchOperand;
	UDATA records = 0;
	U_8 branchTaken;
	J9Class* receiverClass;

	while (cursor < dataStart + size) {
		records += 1;

		memcpy(&pc, cursor, sizeof(pc));
		cursor += sizeof(pc);

		switch (*pc) {
			case JBcheckcast:
			case JBinstanceof:
				memcpy(&receiverClass, cursor, sizeof(receiverClass));
				cursor += sizeof(receiverClass);
				if (TEST_verbose) {
					J9ROMClass* romClass = receiverClass->romClass;
					J9UTF8* name = J9ROMCLASS_CLASSNAME(romClass);
					j9tty_printf(PORTLIB, "pc=%p (cast bc=%d) operand=%.*s(%p)\n", pc, *pc, (UDATA)name->length, name->data, receiverClass);
				}
				break;
			case JBinvokevirtual:
			case JBinvokeinterface:
			case JBinvokeinterface2:
				memcpy(&receiverClass, cursor, sizeof(receiverClass));
				cursor += sizeof(receiverClass);
				if (TEST_verbose) {
					J9ROMClass* romClass = receiverClass->romClass;
					J9UTF8* name = J9ROMCLASS_CLASSNAME(romClass);
					j9tty_printf(PORTLIB, "pc=%p (invoke bc=%d) operand=%.*s(%p)\n", pc, *pc, (UDATA)name->length, name->data, receiverClass);
				}
				break;
			case JBlookupswitch:
			case JBtableswitch:
				memcpy(&switchOperand, cursor, sizeof(switchOperand));
				cursor += sizeof(switchOperand);
				if (TEST_verbose) {
					j9tty_printf(PORTLIB, "pc=%p (switch bc=%d) operand=%d\n", pc, *pc, switchOperand);
				}
				break;
			case JBifacmpeq:
			case JBifacmpne:
			case JBifeq:
			case JBifge:
			case JBifgt:
			case JBifle:
			case JBiflt:
			case JBifne:
			case JBificmpeq:
			case JBificmpne:
			case JBificmpge:
			case JBificmpgt:
			case JBificmple:
			case JBificmplt:
			case JBifnull:
			case JBifnonnull:
				branchTaken = *cursor++;
				if (TEST_verbose) {
					j9tty_printf(PORTLIB, "pc=%p (branch bc=%d) taken=%d\n", pc, *pc, branchTaken);
				}
				break;
			default:
				TEST_errors += 1;
				j9tty_printf(PORTLIB, "Error! Unrecognized bytecode (pc=%p, bc=%d) in record %d.\n", pc, *pc, records);
				return 0;
		}
	}
	
	if (cursor != dataStart + size) {
		TEST_errors += 1;
		j9tty_printf(PORTLIB, "Error! Parser overran buffer.\n");
		return 0;
	}

	return records;

}
#endif /* J9VM_INTERP_PROFILING_BYTECODES */


#if (defined(J9VM_INTERP_PROFILING_BYTECODES)) 
static void
shutdownListener(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMShutdownEvent* event = eventData;
	J9VMThread* vmThread = event->vmThread;
	J9HookInterface** hooks = vmThread->javaVM->internalVMFunctions->getVMHookInterface(vmThread->javaVM);

	PORT_ACCESS_FROM_VMC(vmThread);

	(*hooks)->J9HookUnregister(hooks, J9HOOK_VM_PROFILING_BYTECODE_BUFFER_FULL, profilingListener, NULL);

	j9tty_printf(PORTLIB, "VM shutdown event received.\n");
	j9tty_printf(PORTLIB, "Total events: %d\n", TEST_events);
	j9tty_printf(PORTLIB, "Total records: %d\n", TEST_records);
	j9tty_printf(PORTLIB, "Total errors: %d\n", TEST_errors);
}
#endif /* J9VM_INTERP_PROFILING_BYTECODES */
