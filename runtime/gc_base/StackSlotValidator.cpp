
/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#include "j9cfg.h"
#include "j9.h"
#include "modron.h"
#include "rommeth.h"
#include "ModronAssertions.h"

#include "StackSlotValidator.hpp"

#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"

void 
MM_StackSlotValidator::threadCrash(MM_EnvironmentBase* env)
{
	reportStackSlot(env, "Unhandled exception while validating object in stack frame");
}

void
MM_StackSlotValidator::reportStackSlot(MM_EnvironmentBase* env, const char* message)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	J9VMThread* walkThread = _walkState->walkThread;
	Trc_MM_StackSlotValidator_reportStackSlot_Entry(env->getLanguageVMThread(), walkThread);

	char* threadName = getOMRVMThreadName(walkThread->omrVMThread);
	j9tty_printf(PORTLIB, "%p: %s in thread %s\n", walkThread, message, NULL == threadName ? "NULL" : threadName);
	Trc_MM_StackSlotValidator_thread(env->getLanguageVMThread(), message, NULL == threadName ? "NULL" : threadName);
	
	j9tty_printf(PORTLIB, "%p:\tO-Slot=%p\n", walkThread, _stackLocation);
	Trc_MM_StackSlotValidator_OSlot(env->getLanguageVMThread(), _stackLocation);
	
	j9tty_printf(PORTLIB, "%p:\tO-Slot value=%p\n", walkThread, _slotValue);
	Trc_MM_StackSlotValidator_OSlotValue(env->getLanguageVMThread(), _slotValue);
	
	j9tty_printf(PORTLIB, "%p:\tPC=%p\n", walkThread, _walkState->pc); 
	Trc_MM_StackSlotValidator_PC(env->getLanguageVMThread(), _walkState->pc);
	
	j9tty_printf(PORTLIB, "%p:\tframesWalked=%zu\n", walkThread, _walkState->framesWalked);
	Trc_MM_StackSlotValidator_framesWalked(env->getLanguageVMThread(), _walkState->framesWalked);
	
	j9tty_printf(PORTLIB, "%p:\targ0EA=%p\n", walkThread, _walkState->arg0EA);
	Trc_MM_StackSlotValidator_arg0EA(env->getLanguageVMThread(), _walkState->arg0EA);
	
	j9tty_printf(PORTLIB, "%p:\twalkSP=%p\n", walkThread, _walkState->walkSP);
	Trc_MM_StackSlotValidator_walkSP(env->getLanguageVMThread(), _walkState->walkSP);
	
	j9tty_printf(PORTLIB, "%p:\tliterals=%p\n", walkThread, _walkState->literals);
	Trc_MM_StackSlotValidator_literals(env->getLanguageVMThread(), _walkState->literals);

#ifdef J9VM_INTERP_NATIVE_SUPPORT
	j9tty_printf(PORTLIB, "%p:\tjitInfo=%p\n", walkThread, _walkState->jitInfo);
	Trc_MM_StackSlotValidator_jitInfo(env->getLanguageVMThread(), _walkState->jitInfo);
#endif /* J9VM_INTERP_NATIVE_SUPPORT */

	J9Method * method = _walkState->method;
	if (NULL != method) {
		J9UTF8 * className = J9ROMCLASS_CLASSNAME(J9_CP_FROM_METHOD(method)->ramClass->romClass);
		J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
		J9UTF8 * name = J9ROMMETHOD_NAME(romMethod);
		J9UTF8 * sig = J9ROMMETHOD_SIGNATURE(romMethod);
		const char* type = "Interpreted";
#ifdef J9VM_INTERP_NATIVE_SUPPORT
		if (NULL != _walkState->jitInfo) {
			type = "JIT";
		}
#endif /* J9VM_INTERP_NATIVE_SUPPORT */
		j9tty_printf(PORTLIB, "%p:\tmethod=%p (%.*s.%.*s%.*s) (%s)\n", 
				walkThread, _walkState->method, 
				(U_32) J9UTF8_LENGTH(className), J9UTF8_DATA(className), (U_32) J9UTF8_LENGTH(name), J9UTF8_DATA(name), (U_32) J9UTF8_LENGTH(sig), J9UTF8_DATA(sig),
				type);
		Trc_MM_StackSlotValidator_method(
				env->getLanguageVMThread(), _walkState->method,
				(U_32) J9UTF8_LENGTH(className), J9UTF8_DATA(className), (U_32) J9UTF8_LENGTH(name), J9UTF8_DATA(name), (U_32) J9UTF8_LENGTH(sig), J9UTF8_DATA(sig),
				type);
	}
	
	j9tty_printf(PORTLIB, "%p:\tstack=%p-%p\n", walkThread, walkThread->stackObject + 1, walkThread->stackObject->end);
	Trc_MM_StackSlotValidator_stack(env->getLanguageVMThread(), walkThread->stackObject + 1, walkThread->stackObject->end);

	releaseOMRVMThreadName(walkThread->omrVMThread);
	
	Trc_MM_StackSlotValidator_reportStackSlot_Exit(env->getLanguageVMThread());
}

