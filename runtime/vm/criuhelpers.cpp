/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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
#include "ut_j9vm.h"
#include "vm_api.h"
#include "j9comp.h"

#include "VMHelpers.hpp"

extern "C" {

J9_DECLARE_CONSTANT_UTF8(runPostRestoreHooks_sig, "()V");
J9_DECLARE_CONSTANT_UTF8(runPostRestoreHooks_name, "runPostRestoreHooks");
J9_DECLARE_CONSTANT_UTF8(runPreCheckpointHooks_sig, "()V");
J9_DECLARE_CONSTANT_UTF8(runPreCheckpointHooks_name, "runPreCheckpointHooks");
J9_DECLARE_CONSTANT_UTF8(j9InternalCheckpointHookAPI_name, "org/eclipse/openj9/criu/J9InternalCheckpointHookAPI");

BOOLEAN
jvmCheckpointHooks(J9VMThread *currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;
	BOOLEAN result = TRUE;
	J9NameAndSignature nas = {0};
	nas.name = (J9UTF8 *)&runPreCheckpointHooks_name;
	nas.signature = (J9UTF8 *)&runPreCheckpointHooks_sig;

	/* make sure Java hooks are the first thing run when initiating checkpoint */
	runStaticMethod(currentThread, J9UTF8_DATA(&j9InternalCheckpointHookAPI_name), &nas, 0, NULL);

	if (VM_VMHelpers::exceptionPending(currentThread)) {
		result = FALSE;
		goto done;
	}

	TRIGGER_J9HOOK_VM_PREPARING_FOR_CHECKPOINT(vm->hookInterface, currentThread);

done:
	return result;
}

BOOLEAN
jvmRestoreHooks(J9VMThread *currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;
	BOOLEAN result = TRUE;
	J9NameAndSignature nas = {0};
	nas.name = (J9UTF8 *)&runPostRestoreHooks_name;
	nas.signature = (J9UTF8 *)&runPostRestoreHooks_sig;

	Assert_VM_true(vm->checkpointState.isCheckPointEnabled);

	if (vm->checkpointState.isNonPortableRestoreMode) {
		vm->checkpointState.isCheckPointAllowed = FALSE;
	}

	TRIGGER_J9HOOK_VM_PREPARING_FOR_RESTORE(vm->hookInterface, currentThread);

	/* make sure Java hooks are the last thing run before restore */
	runStaticMethod(currentThread, J9UTF8_DATA(&j9InternalCheckpointHookAPI_name), &nas, 0, NULL);

	if (VM_VMHelpers::exceptionPending(currentThread)) {
		result = FALSE;
	}

	return result;
}

BOOLEAN
isCRIUSupportEnabled(J9VMThread *currentThread)
{
	return currentThread->javaVM->checkpointState.isCheckPointEnabled;
}

BOOLEAN
isCheckpointAllowed(J9VMThread *currentThread)
{
	BOOLEAN result = FALSE;

	if (isCRIUSupportEnabled(currentThread)) {
		result = currentThread->javaVM->checkpointState.isCheckPointAllowed;
	}

	return result;
}

} /* extern "C" */
