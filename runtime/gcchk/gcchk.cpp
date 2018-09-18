
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
 * @ingroup GCChk
 */

#include "j9.h"
#include "jni.h"
#include "j9port.h"
#include "jvminit.h"
#include "modronopt.h"
#include "gcchk.h"

#include <string.h>

#include "CheckBase.hpp"
#include "CheckEngine.hpp"
#include "CheckError.hpp"
#include "CheckReporterTTY.hpp"
#include "GCExtensions.hpp"
#include "ModronTypes.hpp"
#include "ObjectModel.hpp"
#include "CycleState.hpp"

extern "C" {

void hookGcCycleStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
void hookGcCycleEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);

#if (defined(J9VM_GC_MODRON_SCAVENGER)) /* priv. proto (autogen) */
void hookScavengerBackOut(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
#endif /* J9VM_GC_MODRON_SCAVENGER (autogen) */

#if defined(J9VM_GC_GENERATIONAL)
void hookRememberedSetOverflow(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
#endif /* J9VM_GC_GENERATIONAL */

void hookInvokeGCCheck(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);


static IDATA OnLoad(J9JavaVM * javaVM, const char *options);
static IDATA OnUnload(J9JavaVM * javaVM);

IDATA 
J9VMDllMain(J9JavaVM* vm, IDATA stage, void* reserved) 
{
	if (stage == ALL_VM_ARGS_CONSUMED) {
		const char* options = "";
		IDATA xcheckGCIndex = FIND_AND_CONSUME_ARG( OPTIONAL_LIST_MATCH, "-Xcheck:gc", NULL );

		if (xcheckGCIndex >= 0) {
			GET_OPTION_VALUE(xcheckGCIndex, ':', &options);
			options = strchr(options, ':');
			if (options == NULL) {
				options = "";
			} else {
				options++;
			}
		}
		
		return OnLoad(vm, options);	
	} else if (stage == LIBRARIES_ONUNLOAD) {
		return OnUnload(vm);
	} else {
		return J9VMDLLMAIN_OK;
	}
}

/**
 * Perform the actions required by GCCheck on JVM load.
 * 
 * Initialize a GC_CheckEngine object, parse the J9 command line for the prefered
 * options and install the appropriate hooks.
 */
static IDATA
OnLoad(J9JavaVM *javaVM, const char *options)
{
	GCCHK_Extensions *extensions;
	GC_CheckReporter *reporter;
	J9HookInterface** mmPrivateHooks = J9_HOOK_INTERFACE((MM_GCExtensions::getExtensions(javaVM))->privateHookInterface);
	J9HookInterface** mmOmrHooks = J9_HOOK_INTERFACE((MM_GCExtensions::getExtensions(javaVM))->omrHookInterface);
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	MM_Forge *forge = MM_GCExtensions::getExtensions(javaVM)->getForge();
	
	/* catch just outputting the help text */
	if(!strcmp(options, "help")) {
		GC_CheckCycle::printHelp(PORTLIB);
		return J9VMDLLMAIN_SILENT_EXIT_VM;
	}

	extensions = (GCCHK_Extensions *)forge->allocate(sizeof(GCCHK_Extensions), MM_AllocationCategory::DIAGNOSTIC, J9_GET_CALLSITE());
	if (!extensions) {
		goto error_no_memory;
	}
	memset(extensions, 0, sizeof(GCCHK_Extensions));
	(MM_GCExtensions::getExtensions(javaVM))->gcchkExtensions = extensions;

	/* Instantiate the reporter, check, and command-line cycle objects */
	reporter = GC_CheckReporterTTY::newInstance(javaVM);
	if (!reporter) {
		goto error_no_memory;	
	}
	extensions->checkEngine = (void *)GC_CheckEngine::newInstance(javaVM, reporter);
	if (!extensions->checkEngine) {
		goto error_no_memory;	
	}
	
	extensions->checkCycle = (void *)GC_CheckCycle::newInstance(javaVM, (GC_CheckEngine *)extensions->checkEngine, options);
	if(!extensions->checkCycle) {
		goto error_no_memory;
	}

	if(!(((GC_CheckCycle *)extensions->checkCycle)->getMiscFlags() & J9MODRON_GCCHK_MANUAL)) {
		/* Install the hooks */
		(*mmOmrHooks)->J9HookRegisterWithCallSite(mmOmrHooks, J9HOOK_MM_OMR_GC_CYCLE_START, hookGcCycleStart, OMR_GET_CALLSITE(), NULL);
		(*mmOmrHooks)->J9HookRegisterWithCallSite(mmOmrHooks, J9HOOK_MM_OMR_GC_CYCLE_END, hookGcCycleEnd, OMR_GET_CALLSITE(), NULL);

#if defined(J9VM_GC_MODRON_SCAVENGER)
		/* Install scavengerBackOut hooks */
		(*mmPrivateHooks)->J9HookRegisterWithCallSite(mmPrivateHooks, J9HOOK_MM_PRIVATE_SCAVENGER_BACK_OUT, hookScavengerBackOut, OMR_GET_CALLSITE(), NULL);
#endif /* J9VM_GC_MODRON_SCAVENGER */
	
#if defined(J9VM_GC_GENERATIONAL)
		(*mmPrivateHooks)->J9HookRegisterWithCallSite(mmPrivateHooks, J9HOOK_MM_PRIVATE_REMEMBEREDSET_OVERFLOW, hookRememberedSetOverflow, OMR_GET_CALLSITE(), NULL);
#endif /* J9VM_GC_GENERATIONAL */
	}

	/* Install handler to run GCCheck from anywhere in GC code. */ 
	(*mmPrivateHooks)->J9HookRegisterWithCallSite(mmPrivateHooks, J9HOOK_MM_PRIVATE_INVOKE_GC_CHECK, hookInvokeGCCheck, OMR_GET_CALLSITE(), NULL);
	
	/* Set the ALLOW_USER_HEAP_WALK bit in the requiredDebugAttributes flag so GC Check can walk the heap */
	javaVM->requiredDebugAttributes |= J9VM_DEBUG_ATTRIBUTE_ALLOW_USER_HEAP_WALK;

	if (((GC_CheckCycle *)extensions->checkCycle)->getMiscFlags() & J9MODRON_GCCHK_VERBOSE) {
		j9tty_printf(PORTLIB, "<gc check installed>\n");
	}

	return J9VMDLLMAIN_OK;
	
error_no_memory:
	if (extensions) {
		if (extensions->checkEngine) {
			((GC_CheckEngine *)extensions->checkEngine)->kill();
		} else if (reporter) {
			/* CheckEngine will kill the reporter if the CheckEngine was created */
			reporter->kill();	
		}

		if(extensions->checkCycle) {
			((GC_CheckCycle *)extensions->checkCycle)->kill();
		}

		forge->free(extensions);
		((MM_GCExtensions *)javaVM->gcExtensions)->gcchkExtensions = NULL;
	}
	return J9VMDLLMAIN_FAILED;
}

/**
 * Perform the actions required by GCCheck on JVM Unload.
 * Free any OS resources created by OnLoad.
 */
static IDATA
OnUnload(J9JavaVM *javaVM ) 
{
	GCCHK_Extensions *extensions = (GCCHK_Extensions *)(MM_GCExtensions::getExtensions(javaVM))->gcchkExtensions;
	MM_Forge *forge = MM_GCExtensions::getExtensions(javaVM)->getForge();

	if(extensions) {
		((GC_CheckEngine *)extensions->checkEngine)->kill();
		((GC_CheckCycle *)extensions->checkCycle)->kill();
		forge->free(extensions);
		((MM_GCExtensions *)javaVM->gcExtensions)->gcchkExtensions = NULL;
	}

	return J9VMDLLMAIN_OK;
}

#if (defined(J9VM_GC_MODRON_SCAVENGER)) /* priv. proto (autogen) */
/**
 * Determine whether the next local GC should be excluded from checking.
 * 
 * @return false if we SHOULD perform a check before the next local 
 * GC, i.e. the next local GC should NOT be excluded
 * @return true if we should NOT perform a check
 */
bool
excludeLocalGc(J9JavaVM *javaVM) 
{
	MM_GCExtensions *gcExtensions = MM_GCExtensions::getExtensions(javaVM);
	GCCHK_Extensions *extensions = (GCCHK_Extensions *)gcExtensions->gcchkExtensions;
	GC_CheckEngine *gcCheck = (GC_CheckEngine *)extensions->checkEngine;
	GC_CheckCycle *cycle = (GC_CheckCycle *)extensions->checkCycle;
	bool ret = false;

	/* User either requested that localGC be suppressed, or inform them only on remembered set overflow.  
	 * Either way, local information is not provided.
	 */
	if ((cycle->getMiscFlags() & J9MODRON_GCCHK_SUPPRESS_LOCAL) ||
		((cycle->getMiscFlags() & J9MODRON_GCCHK_REMEMBEREDSET_OVERFLOW) && (!gcCheck->_rsOverflowState))) {
		return true;
	}

	/* Don't check after Concurrent Scavenge aborted (even if J9MODRON_GCCHK_SCAVENGER_BACKOUT requested). Nursery is not walkable
	 * between aborted CS and following percolated global GC. The check will be done after percolate GC is complete (which will do most of heap fixup).
	 * Since gcCheck->_scavengerBackout is only set if J9MODRON_GCCHK_SCAVENGER_BACKOUT is requested, relying on gcExtensions->isScavengerBackOutFlagRaised() instead.
	 */
	if (gcExtensions->isConcurrentScavengerEnabled() && gcExtensions->isScavengerBackOutFlagRaised()) {
		return true;
	}

	/* If the request was for scavenger backout and there wasn't one, no check */
	if ((cycle->getMiscFlags() & J9MODRON_GCCHK_SCAVENGER_BACKOUT) && (!gcCheck->_scavengerBackout)) {
		return true;
	}

	if (cycle->getMiscFlags() & J9MODRON_GCCHK_LOCAL_INTERVAL) {
		if ((extensions->localGcCount % extensions->localGcInterval) == 0) {
			return false;
		} else {
			ret = true;
		}
	}

	if (cycle->getMiscFlags() & J9MODRON_GCCHK_INTERVAL) {
		return  ( ((extensions->globalGcCount + extensions->localGcCount) % extensions->gcInterval) != 0 );
	}

	if (cycle->getMiscFlags() & J9MODRON_GCCHK_START_INDEX) {
		return ((extensions->globalGcCount + extensions->localGcCount) < extensions->gcStartIndex);
	}

	return ret;
}

/**
 * GCCheck function for hookScavengerBackOut.
 * The scavenger is backing out.  Set our flag so we know to report the localEnd hook information.
 */
void
hookScavengerBackOut(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_ScavengerBackOutEvent* event = (MM_ScavengerBackOutEvent*)eventData;
	bool value = (event->value == TRUE);
	GCCHK_Extensions *extensions = (GCCHK_Extensions *)(MM_GCExtensions::getExtensions(event->omrVM))->gcchkExtensions;
	GC_CheckEngine *checkEngine = (GC_CheckEngine *)extensions->checkEngine;
	GC_CheckCycle *cycle = (GC_CheckCycle *)extensions->checkCycle;

	if (cycle->getMiscFlags() & J9MODRON_GCCHK_SCAVENGER_BACKOUT) {
		checkEngine->_scavengerBackout = value;
	}
}
#endif /* J9VM_GC_MODRON_SCAVENGER (autogen) */

/**
 * Determine whether the next global GC should be excluded from checking.
 * 
 * @return false if we SHOULD perform a check before the next global 
 * GC, i.e. the next global GC should NOT be excluded
 * @return true if we should NOT perform a check
 */
bool
excludeGlobalGc(J9VMThread* vmThread)
{
	J9JavaVM *javaVM = vmThread->javaVM;
	MM_GCExtensions *gcExtensions = MM_GCExtensions::getExtensions(javaVM);
	GCCHK_Extensions *extensions = (GCCHK_Extensions *)gcExtensions->gcchkExtensions;
	GC_CheckCycle *cycle = (GC_CheckCycle *)extensions->checkCycle;
	GC_CheckEngine *gcCheck = (GC_CheckEngine *)extensions->checkEngine;
	bool ret = false;
	UDATA gcCount;

	/* If Concurrent Scavenger aborted, do not check before global GC (regardless if J9MODRON_GCCHK_SCAVENGER_BACKOUT is requested or not).
	 * Since checkEngine->_scavengerBackout is set only if J9MODRON_GCCHK_SCAVENGER_BACKOUT requested, gcExtensions->isScavengerBackOutFlagRaised() is checked instead.  */
	if (gcExtensions->isConcurrentScavengerEnabled() && gcExtensions->isScavengerBackOutFlagRaised() && (OMRVMSTATE_GC_CHECK_BEFORE_GC == vmThread->omrVMThread->vmState)) {
		return true;
	}

	/* User either requested that globalGC be suppressed, or inform them only on 
	 * scavenger backout or remembered set overflow.  
	 * Either way, global information is not provided.
	 */
	if ((cycle->getMiscFlags() & J9MODRON_GCCHK_SUPPRESS_GLOBAL) ||
		(cycle->getMiscFlags() & J9MODRON_GCCHK_REMEMBEREDSET_OVERFLOW)) {
		return true;
	}

	if (cycle->getMiscFlags() & J9MODRON_GCCHK_SCAVENGER_BACKOUT) {
		/* if J9MODRON_GCCHK_SCAVENGER_BACKOUT is requested, the check will be done at the end of percolate global GC (not at the end of the aborted scavenge).
		 * Recovery from the aborted CS is mostly done in the global GC and Nursery is not walkable between aborted CS and percolate global.
		 * Since GC check is done in global GC cycle end hook, gcExtensions->isScavengerBackOutFlagRaised just has been reset, so we have to rely
		 * on gcCheck->_scavengerBackout, which is yet to be cleared in the initialization of this check cycle  */
		if (!(gcExtensions->isConcurrentScavengerEnabled() && gcCheck->_scavengerBackout)) {
			return true;
		}
	}

	if (cycle->getMiscFlags() & J9MODRON_GCCHK_GLOBAL_INTERVAL) {
		if ( (extensions->globalGcCount % extensions->globalGcInterval) == 0 ) {
			return false;
		} else {
			ret = true;
		}
	}

	gcCount = extensions->globalGcCount;
#if defined(J9VM_GC_MODRON_SCAVENGER)
	gcCount += extensions->localGcCount;
#endif /* J9VM_GC_MODRON_SCAVENGER */
	if (cycle->getMiscFlags() & J9MODRON_GCCHK_INTERVAL) {
		return ((gcCount % extensions->gcInterval) != 0 );
	}

	if (cycle->getMiscFlags() & J9MODRON_GCCHK_START_INDEX) {
		return (gcCount < extensions->gcStartIndex);
	}

	return ret;
}

/**
 * GCCheck function for hookGcCycleStart.
 * Check memory management structures prior to garbage collection.
 */
void
hookGcCycleStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_GCCycleStartEvent* event = (MM_GCCycleStartEvent*)eventData;
	J9VMThread* vmThread = (J9VMThread*)MM_EnvironmentBase::getEnvironment(event->omrVMThread)->getLanguageVMThread();
	J9JavaVM *javaVM = vmThread->javaVM;
	GCCHK_Extensions *extensions = (GCCHK_Extensions *)(MM_GCExtensions::getExtensions(javaVM))->gcchkExtensions;
	GC_CheckCycle *cycle = (GC_CheckCycle *)extensions->checkCycle;
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	UDATA oldVMState = vmThread->omrVMThread->vmState;
	vmThread->omrVMThread->vmState = OMRVMSTATE_GC_CHECK_BEFORE_GC;

	if (OMR_GC_CYCLE_TYPE_GLOBAL == event->cycleType) {
		++extensions->globalGcCount;
		if (!excludeGlobalGc(vmThread)) {
			if (cycle->getMiscFlags() & J9MODRON_GCCHK_VERBOSE) {
				j9tty_printf(PORTLIB, "<gc check: start verifying slots before global gc (%zu)>\n", extensions->globalGcCount);
			}

			cycle->run(invocation_global_start);

			if (cycle->getMiscFlags() & J9MODRON_GCCHK_VERBOSE) {
				j9tty_printf(PORTLIB, "<gc check: finished verifying slots before global gc (%zu)>\n", extensions->globalGcCount);
			}
		}
	}
#if defined(J9VM_GC_MODRON_SCAVENGER)
	else if (OMR_GC_CYCLE_TYPE_SCAVENGE == event->cycleType) {
		++extensions->localGcCount;
		if (!excludeLocalGc(javaVM)) {
			if (cycle->getMiscFlags() & J9MODRON_GCCHK_VERBOSE) {
				j9tty_printf(PORTLIB, "<gc check: start verifying slots before local gc (%zu)>\n", extensions->localGcCount);
			}

			cycle->run(invocation_local_start);

			if (cycle->getMiscFlags() & J9MODRON_GCCHK_VERBOSE) {
				j9tty_printf(PORTLIB, "<gc check: finished verifying slots before local gc (%zu)>\n", extensions->localGcCount);
			}
		}
	}
#endif /* defined(J9VM_GC_MODRON_SCAVENGER) */
	else {
		++extensions->globalGcCount;
		if (!excludeGlobalGc(vmThread)) {
			if (cycle->getMiscFlags() & J9MODRON_GCCHK_VERBOSE) {
				j9tty_printf(PORTLIB, "<gc check: start verifying slots before default gc (%zu)>\n", extensions->globalGcCount);
			}

			cycle->run(invocation_global_start);

			if (cycle->getMiscFlags() & J9MODRON_GCCHK_VERBOSE) {
				j9tty_printf(PORTLIB, "<gc check: finished verifying slots before default gc (%zu)>\n", extensions->globalGcCount);
			}
		}
	}
	vmThread->omrVMThread->vmState = oldVMState;
}

/**
 * GCCheck function for hookGcCycleEnd.
 * Check memory management structures after garbage collection.
 */
void
hookGcCycleEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_GCCycleEndEvent* event = (MM_GCCycleEndEvent*)eventData;
	J9VMThread* vmThread = (J9VMThread*)MM_EnvironmentBase::getEnvironment(event->omrVMThread)->getLanguageVMThread();
	J9JavaVM *javaVM = vmThread->javaVM;
	GCCHK_Extensions *extensions = (GCCHK_Extensions *)(MM_GCExtensions::getExtensions(javaVM))->gcchkExtensions;
	GC_CheckCycle *cycle = (GC_CheckCycle *)extensions->checkCycle;
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	UDATA oldVMState = vmThread->omrVMThread->vmState;
	vmThread->omrVMThread->vmState = OMRVMSTATE_GC_CHECK_AFTER_GC;

	if (OMR_GC_CYCLE_TYPE_GLOBAL == event->cycleType) {
		if (!excludeGlobalGc(vmThread)) {
			if (cycle->getMiscFlags() & J9MODRON_GCCHK_VERBOSE) {
				j9tty_printf(PORTLIB, "<gc check: start verifying slots after global gc (%zu)>\n", extensions->globalGcCount);
			}

			cycle->run(invocation_global_end);

			if (cycle->getMiscFlags() & J9MODRON_GCCHK_VERBOSE) {
				j9tty_printf(PORTLIB, "<gc check: finished verifying slots after global gc (%zu)>\n", extensions->globalGcCount);
			}
		}
	}
#if defined(J9VM_GC_MODRON_SCAVENGER)
	else if (OMR_GC_CYCLE_TYPE_SCAVENGE == event->cycleType) {
		if (!excludeLocalGc(javaVM)) {
			if (cycle->getMiscFlags() & J9MODRON_GCCHK_VERBOSE) {
				j9tty_printf(PORTLIB, "<gc check: start verifying slots after local gc (%zu)>\n", extensions->localGcCount);
			}
	
			cycle->run(invocation_local_end);

			if (cycle->getMiscFlags() & J9MODRON_GCCHK_VERBOSE) {
				j9tty_printf(PORTLIB, "<gc check: finished verifying slots after local gc (%zu)>\n", extensions->localGcCount);
			}
		}
	}
#endif /* defined(J9VM_GC_MODRON_SCAVENGER) */
	else {
		if (!excludeGlobalGc(vmThread)) {
			if (cycle->getMiscFlags() & J9MODRON_GCCHK_VERBOSE) {
				j9tty_printf(PORTLIB, "<gc check: start verifying slots after default gc (%zu)>\n", extensions->globalGcCount);
			}

			cycle->run(invocation_global_end);

			if (cycle->getMiscFlags() & J9MODRON_GCCHK_VERBOSE) {
				j9tty_printf(PORTLIB, "<gc check: finished verifying slots after default gc (%zu)>\n", extensions->globalGcCount);
			}
		}
	}
	vmThread->omrVMThread->vmState = oldVMState;
}

#if defined(J9VM_GC_GENERATIONAL)
/**
 * GCCheck function for hookRememberedSetOverflow.
 */
void
hookRememberedSetOverflow(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_RememberedSetOverflowEvent* event = (MM_RememberedSetOverflowEvent*)eventData;
	GCCHK_Extensions *extensions = (GCCHK_Extensions *)(MM_GCExtensions::getExtensions(event->currentThread))->gcchkExtensions;
	GC_CheckEngine *checkEngine = (GC_CheckEngine *)extensions->checkEngine;
	GC_CheckCycle *cycle = (GC_CheckCycle *)extensions->checkCycle;

	if (cycle->getMiscFlags() & J9MODRON_GCCHK_REMEMBEREDSET_OVERFLOW) {
		checkEngine->_rsOverflowState = MM_GCExtensions::getExtensions(event->currentThread)->isRememberedSetInOverflowState();
	}
}
#endif /* J9VM_GC_GENERATIONAL */

/**
 * Manually initiate a GCCheck run (as opposed to having one triggered by a GC).
 * Users wishing to manually start GCCheck should add code like the following 
 * <pre>
#include "mmhook_internal.h"

	PORT_ACCESS_FROM_JAVAVM(javaVM);
	
	TRIGGER_J9HOOK_MM_PRIVATE_INVOKE_GC_CHECK(_extensions->hookInterface, _javaVM, PORTLIB, "all:all:verbose", 0);
}
 * </pre>
 * GCCheck must be installed for via the command line.  Specifying -Xrunj9gcchk23:none:none:quiet,nocheck will ensure
 * GCCheck does not run for any GC event.  Thus all output is controlled by the user placed invocations.  Users
 * can specify other options on the command line if they wish.  These options are not passed to user invoked
 * invocations.
 * 
 * @param javaVM the javaVM
 * @param portLibrary where to output results
 * @param options what events to invoke GCCheck on
 * @param invocationNumber uniquely identifies which manual invocation has triggered the GCCheck
 * 
 * @return void
 */
void
hookInvokeGCCheck(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_InvokeGCCheckEvent* event = (MM_InvokeGCCheckEvent*)eventData;
	J9JavaVM *javaVM = static_cast<J9JavaVM*>(event->omrVM->_language_vm);
	GCCHK_Extensions *extensions = (GCCHK_Extensions *)((MM_GCExtensions *)javaVM->gcExtensions)->gcchkExtensions;
	GC_CheckEngine *checkEngine = (GC_CheckEngine *)extensions->checkEngine;
	GC_CheckCycle *cycle;

	if (checkEngine) {
		cycle = GC_CheckCycle::newInstance(javaVM, checkEngine, event->options, event->invocationNumber);
		if(cycle) {
			cycle->run(invocation_manual);
			cycle->kill();
		}
	}
}

} /* extern "C" */
