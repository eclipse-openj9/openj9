/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
#include "j9gs.h"
#include "j9port_generated.h"
#include "j9nonbuilder.h"
#include "vm_internal.h"

#if defined(OMR_GC_CONCURRENT_SCAVENGER) && defined(J9VM_ARCH_S390)

J9_EXTERN_BUILDER_SYMBOL(handleHardwareReadBarrier);

void
invokeJ9ReadBarrier(J9VMThread *currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9MemoryManagerFunctions *mmFuncs = vm->memoryManagerFunctions;
	mmFuncs->J9ReadBarrier(currentThread, (fj9object_t *) currentThread->gsParameters.operandAddr);
}

int32_t
j9gs_initializeThread(J9VMThread *vmThread)
{
	PORT_ACCESS_FROM_VMC(vmThread);
	int32_t success = 0;

	/* Check if thread is already initialized */
	if (IS_THREAD_GS_INITIALIZED(vmThread)) {
		success = 1;
	} else {
		BOOLEAN supportsGuardedStorageFacility = FALSE;
		J9JavaVM *vm = vmThread->javaVM;
		J9MemoryManagerFunctions *mmFuncs = vm->memoryManagerFunctions;
		J9ProcessorDesc  processorDesc;
		j9sysinfo_get_processor_description(&processorDesc);
		if (j9sysinfo_processor_has_feature(&processorDesc, J9PORT_S390_FEATURE_GUARDED_STORAGE) &&
				j9sysinfo_processor_has_feature(&processorDesc, J9PORT_S390_FEATURE_SIDE_EFFECT_ACCESS) &&
				!mmFuncs->j9gc_software_read_barrier_enabled(vm)) {
			supportsGuardedStorageFacility = TRUE;
		}

		/* Allocate the control block */
		J9GSControlBlock *gsControlBlock = (J9GSControlBlock *)j9mem_allocate_memory(sizeof(J9GSControlBlock), J9MEM_CATEGORY_VM);
		if (NULL != gsControlBlock) {
			/* Determine the shift value */
			J9JavaVM * javaVM = vmThread->javaVM;
			int32_t compressedRefShift = javaVM->memoryManagerFunctions->j9gc_objaccess_compressedPointersShift(vmThread);
			
			/* Default parameters */
			gsControlBlock->reserved = 0;
			gsControlBlock->designationRegister = 0;
			gsControlBlock->sectionMask = 0;
			gsControlBlock->paramListAddr = (uint64_t) &vmThread->gsParameters;
			vmThread->gsParameters.handlerAddr = (uint64_t)(uintptr_t)J9_BUILDER_SYMBOL(handleHardwareReadBarrier);

			/* Initialize the parameters */
			j9gs_params_init(&vmThread->gsParameters, gsControlBlock);
		
			/* Initialize the current thread */

			if (TRUE == supportsGuardedStorageFacility) {
				success = j9gs_initialize(&vmThread->gsParameters, compressedRefShift);
			} else {
				success = 1;
			}
		
			/* Check to see if initialization was a success */
			if (0 == success) {
				j9mem_free_memory(gsControlBlock);
				(vmThread->gsParameters).controlBlock = NULL;
			}
		}
	}
	
	return success;
}

int32_t
j9gs_deinitializeThread(J9VMThread *vmThread)
{
	PORT_ACCESS_FROM_VMC(vmThread);
	int32_t success = 0;
	BOOLEAN supportsGuardedStorageFacility = FALSE;
	J9JavaVM *vm = vmThread->javaVM;
	J9MemoryManagerFunctions *mmFuncs = vm->memoryManagerFunctions;
	J9ProcessorDesc  processorDesc;
	j9sysinfo_get_processor_description(&processorDesc);
	if (j9sysinfo_processor_has_feature(&processorDesc, J9PORT_S390_FEATURE_GUARDED_STORAGE) &&
			j9sysinfo_processor_has_feature(&processorDesc, J9PORT_S390_FEATURE_SIDE_EFFECT_ACCESS) &&
			!mmFuncs->j9gc_software_read_barrier_enabled(vm)) {
		supportsGuardedStorageFacility = TRUE;
	}

	/* Check if thread is already deinitialized */
	if (!IS_THREAD_GS_INITIALIZED(vmThread)) {
		success = 1;
	} else {
		J9GSControlBlock *gsControlBlock = (J9GSControlBlock*)((vmThread->gsParameters).controlBlock);
		if (NULL != gsControlBlock) {
			/* Deinitialize the current thread */
			if (TRUE == supportsGuardedStorageFacility) {
				success = j9gs_deinitialize(&vmThread->gsParameters);
			} else {
				success = 1;
			}
			/* Free the Control Block */
			j9mem_free_memory(gsControlBlock);
			(vmThread->gsParameters).controlBlock = NULL;
		}
	}
	
	return success;
}

#endif /* OMR_GC_CONCURRENT_SCAVENGER */
