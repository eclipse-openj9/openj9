/*******************************************************************************
 * Copyright (c) 2013, 2019 IBM Corp. and others
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
#include "vm_api.h"
#include "ute.h"
#include "ut_j9vm.h"

void jvmPhaseChange(J9JavaVM* vm, UDATA phase) {
	J9VMThread *currentThread = currentVMThread(vm);
	vm->phase = phase;
	Trc_VM_VMPhases_JVMPhaseChange(phase);
	
	if( phase == J9VM_PHASE_NOT_STARTUP ) {
		RasGlobalStorage *tempRasGbl;

		if (J9_ARE_NO_BITS_SET(vm->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_DISABLE_FAST_CLASS_HASH_TABLE)) {
			if (NULL != vm->classLoaderBlocks) {
				pool_state clState;
				J9ClassLoader *loader;

				omrthread_monitor_enter(vm->classTableMutex);
				omrthread_monitor_enter(vm->classLoaderBlocksMutex);
				loader = pool_startDo(vm->classLoaderBlocks, &clState);
				while (loader != NULL) {
					J9HashTable *table = loader->classHashTable;
					if (NULL != table) {
						table->flags |= J9HASH_TABLE_DO_NOT_GROW;
					}
					loader = pool_nextDo(&clState);
				}
				omrthread_monitor_enter(vm->runtimeFlagsMutex);
				vm->extendedRuntimeFlags |= J9_EXTENDED_RUNTIME_FAST_CLASS_HASH_TABLE;
				omrthread_monitor_exit(vm->runtimeFlagsMutex);

				omrthread_monitor_exit(vm->classLoaderBlocksMutex);
				omrthread_monitor_exit(vm->classTableMutex);
				Trc_VM_VMPhases_FastClassHashTable_Enabled();
			}
		}

		tempRasGbl = (RasGlobalStorage *)vm->j9rasGlobalStorage;
		if (tempRasGbl != NULL && tempRasGbl->utIntf != NULL) {
			((J9UtServerInterface *)((UtInterface *)tempRasGbl->utIntf)->server)->StartupComplete(currentThread);
		}
	}
	if (NULL != vm->memoryManagerFunctions) {
		vm->memoryManagerFunctions->jvmPhaseChange(currentThread, phase);
	}
	if (NULL != vm->sharedClassConfig) {
		vm->sharedClassConfig->jvmPhaseChange(currentThread, phase);
	}
}
