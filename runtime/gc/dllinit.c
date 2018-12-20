
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

/**
 * @file
 * @ingroup GC
 */

#include "j9.h"
#include "j9cfg.h"
#include "jni.h"
#include "jvminit.h"
#include "mminit.h"
#include "ModronAssertions.h"

jint JNICALL 
JVM_OnLoad( JavaVM *jvm, char* commandLineOptions, void *reserved ) 
{
	return JNI_OK;
}

/**
 * Main entrypoint for GC DLL
 * 
 * This function is called by the VM multiple times during startup and shutdown -
 * once for each stage. The implementation traps stages GC is interested in and
 * starts up/shuts down the memory manager as appropriate.
 * 
 * @param stage Stage the VM has reached
 * @return Success/failure
 */
IDATA 
J9VMDllMain(J9JavaVM* vm, IDATA stage, void* reserved) 
{
	J9VMDllLoadInfo* loadInfo = FIND_DLL_TABLE_ENTRY( THIS_DLL_NAME );
	IDATA rc = J9VMDLLMAIN_OK;
	
	switch (stage) {
		case PORT_LIBRARY_GUARANTEED:
		case ALL_DEFAULT_LIBRARIES_LOADED:
			break;

		case ALL_LIBRARIES_LOADED:
			/* Note: this must happen now, otherwise -verbose:sizes will not work, as verbose 
			 * support is initialized during this stage.
			 */
			rc = gcInitializeDefaults(vm);
			break;

		case DLL_LOAD_TABLE_FINALIZED:
		case VM_THREADING_INITIALIZED:
			break;

		case HEAP_STRUCTURES_INITIALIZED:
			rc = gcInitializeHeapStructures(vm);
			break;

		case ALL_VM_ARGS_CONSUMED :
		case BYTECODE_TABLE_SET :
		case SYSTEM_CLASSLOADER_SET :
		case DEBUG_SERVER_INITIALIZED :
			break;

		case JIT_INITIALIZED :
			/* Register this module with trace */
			UT_J9MM_MODULE_LOADED(J9_UTINTERFACE_FROM_VM(vm));
			Trc_MM_VMInitStages_Event1(NULL);

			rc = triggerGCInitialized(vm->mainThread);

			break;
		
		case ABOUT_TO_BOOTSTRAP :
			/* Expand heap based on hints stored by previous runs into Shared Cache */
			gcExpandHeapOnStartup(vm);
		case JCL_INITIALIZED :
		case LIBRARIES_ONUNLOAD :
			break;

		case HEAP_STRUCTURES_FREED :
			if ( IS_STAGE_COMPLETED( loadInfo->completedBits, HEAP_STRUCTURES_INITIALIZED ) ) {
				gcCleanupHeapStructures(vm);
			}
			break;

		case GC_SHUTDOWN_COMPLETE :
			if ( IS_STAGE_COMPLETED( loadInfo->completedBits, ALL_LIBRARIES_LOADED ) ) {
				gcCleanupInitializeDefaults(vm->omrVM);
			}
			break;
	}
	return rc;
}

