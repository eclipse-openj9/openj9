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

/**
 * @file
 * @ingroup VMChk
 */

#include "j9.h"
#include "j9port.h"
#include "jvminit.h"
#include "mmhook.h"
#include "mmomrhook.h"
#include "vmcheck.h"
#include "ut_j9vmchk.h"
#include "vendor_version.h"

#include <stdarg.h>

static IDATA OnLoad(J9JavaVM *javaVM, const char *options);
static IDATA OnUnload(J9JavaVM *javaVM);
static void printVMCheckHelp(J9JavaVM *javaVM);
static void runAllVMChecks(J9VMThread *currentThread, const char *trigger);
void hookGlobalGcCycleStart(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);
void hookGlobalGcCycleEnd(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);
static void hookVmShutdown(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);

IDATA
J9VMDllMain(J9JavaVM *vm, IDATA stage, void *reserved)
{
	if (stage == ALL_VM_ARGS_CONSUMED) {
		const char *options = "";
		IDATA xcheckVMIndex = FIND_AND_CONSUME_ARG(OPTIONAL_LIST_MATCH, "-Xcheck:vm", NULL);

		if (xcheckVMIndex >= 0) {
			GET_OPTION_VALUE(xcheckVMIndex, ':', &options);
			options = strchr(options, ':');
			if (options == NULL) {
				options = "";
			} else {
				options++;
			}
		}

		return OnLoad(vm, options);

	} else if (stage == JIT_INITIALIZED) {
		/* Register this module with trace */
		UT_MODULE_LOADED(J9_UTINTERFACE_FROM_VM(vm));
		Trc_VMCHK_VMInitStages_Event1(NULL);
		return J9VMDLLMAIN_OK;

	} else if (stage == LIBRARIES_ONUNLOAD) {
		return OnUnload(vm);
	} else {
		return J9VMDLLMAIN_OK;
	}
}

/**
 * Perform the actions required by VMCheck on JVM load.
 */
static IDATA
OnLoad(J9JavaVM *javaVM, const char *options)
{
	J9HookInterface **vmHooks;
	J9HookInterface **gcOmrHooks;
	BOOLEAN allCheck = FALSE;
	BOOLEAN debugDataCheck = FALSE;

	/* catch just outputting the help text */
	if (!strcmp(options, "help")) {
		printVMCheckHelp(javaVM);
		return J9VMDLLMAIN_SILENT_EXIT_VM;
	}

	/* catch when running all checks */
	if (!strcmp(options, "all") || !strcmp(options, "")) {
		allCheck = TRUE;
	}

	/* catch -Xcheck:vm:debuginfo */
	if (!strcmp(options, "debuginfo")) {
		debugDataCheck = TRUE;
	}

	if (allCheck || debugDataCheck) {
		vmchkPrintf(javaVM, "-Xcheck:vm:debuginfo enabled \n");
		javaVM->extendedRuntimeFlags |= J9_EXTENDED_RUNTIME_CHECK_DEBUG_INFO_COMPRESSION;
	}

	gcOmrHooks = javaVM->memoryManagerFunctions->j9gc_get_omr_hook_interface(javaVM->omrVM);
	vmHooks = javaVM->internalVMFunctions->getVMHookInterface(javaVM);

	if (allCheck) {
		if ((*gcOmrHooks)->J9HookRegisterWithCallSite(gcOmrHooks, J9HOOK_MM_OMR_GC_CYCLE_START, hookGlobalGcCycleStart, OMR_GET_CALLSITE(), NULL)) {
			vmchkPrintf(javaVM, "<vm check: unable to hook J9HOOK_MM_GC_CYCLE_START event>\n");
			return J9VMDLLMAIN_FAILED;
		}
	}

	if (allCheck) {
		if ((*gcOmrHooks)->J9HookRegisterWithCallSite(gcOmrHooks, J9HOOK_MM_OMR_GC_CYCLE_END, hookGlobalGcCycleEnd, OMR_GET_CALLSITE(), NULL)) {
			vmchkPrintf(javaVM, "<vm check: unable to hook J9HOOK_MM_GC_CYCLE_END event>\n");
			return J9VMDLLMAIN_FAILED;
		}
	}

	if (allCheck) {
		if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_SHUTTING_DOWN, hookVmShutdown, OMR_GET_CALLSITE(), NULL)) {
			vmchkPrintf(javaVM, "<vm check: unable to hook J9HOOK_VM_SHUTTING_DOWN event>\n");
			return J9VMDLLMAIN_FAILED;
		}
	}
	return J9VMDLLMAIN_OK;
}

/**
 * Perform the actions required by VMCheck on JVM Unload.
 */
static IDATA
OnUnload(J9JavaVM *javaVM)
{
	J9HookInterface **gcOmrHooks;
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	gcOmrHooks = javaVM->memoryManagerFunctions->j9gc_get_omr_hook_interface(javaVM->omrVM);
	(*gcOmrHooks)->J9HookUnregister(gcOmrHooks, J9HOOK_MM_OMR_GC_CYCLE_START, hookGlobalGcCycleStart, javaVM);
	(*gcOmrHooks)->J9HookUnregister(gcOmrHooks, J9HOOK_MM_OMR_GC_CYCLE_END, hookGlobalGcCycleEnd, javaVM);

	return J9VMDLLMAIN_OK;
}

void
vmchkPrintf(J9JavaVM *javaVM, const char *format, ...)
{
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	va_list args;

	/* If tracepoint enabled, mirror stdout line to tracepoint j9vmchk.1, tracepoint group j9vmchk{checkvm} */
	if (TrcEnabled_Trc_VMCHK_vmchkPrintf) {
		char buffer[1024];
	
		/* Format the message text for the tracepoint insert and issue tracepoint. */
		va_start(args, format);
		j9str_vprintf(buffer, sizeof(buffer), format, args);
		va_end(args);
		Trc_VMCHK_vmchkPrintf(buffer);
	}
	
	va_start(args, format);
	j9tty_vprintf(format, args);
	va_end(args);
}

static void
printVMCheckHelp(J9JavaVM *vm)
{
	vmchkPrintf(vm, "vmchk VM Check utility for J9, Version " J9JVM_VERSION_STRING "\n");
	vmchkPrintf(vm, J9_COPYRIGHT_STRING "\n\n");
	vmchkPrintf(vm, "  help              print this screen\n");
	vmchkPrintf(vm, "  all               all checks\n");
	vmchkPrintf(vm, "  debuginfo         verify the internal format of class debug attributes\n");
	vmchkPrintf(vm, "  none              no checks\n");
	vmchkPrintf(vm, "\n");
}

/**
 * Run all VM checks against the local VM.
 *
 * Note: Caller must either have exclusive VM access or must
 *       be executing on behalf of a thread that does (which
 *       is the case for GC cycle events under the balanced
 *       GC policy).
 *
 * @param[in] currentThread The current VM thread.
 * @param[in] trigger       String describing the trigger event.
 *
 */
static void
runAllVMChecks(J9VMThread *currentThread, const char *trigger)
{
	J9JavaVM *javaVM = currentThread->javaVM;

	vmchkPrintf(javaVM, "%s started (%s)>\n", VMCHECK_PREFIX, trigger);

	checkJ9VMThreadSanity(javaVM);
	checkJ9ClassSanity(javaVM);
	checkJ9ROMClassSanity(javaVM);
	checkJ9MethodSanity(javaVM);
	checkLocalInternTableSanity(javaVM);
	checkClassLoadingConstraints(javaVM);

	vmchkPrintf(javaVM, "%s done>\n", VMCHECK_PREFIX);
}

void
hookGlobalGcCycleStart(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	MM_GCCycleStartEvent *event = eventData;

	runAllVMChecks((J9VMThread *)event->omrVMThread->_language_vmthread, "pre-GC");
}

void
hookGlobalGcCycleEnd(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	MM_GCCycleEndEvent *event = eventData;

	runAllVMChecks((J9VMThread *)event->omrVMThread->_language_vmthread, "post-GC");
}

static void
hookVmShutdown(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	J9VMShutdownEvent *event = eventData;

	runAllVMChecks(event->vmThread, "VM shutdown");
}
