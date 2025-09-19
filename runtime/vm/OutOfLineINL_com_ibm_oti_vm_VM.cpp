/*******************************************************************************
 * Copyright IBM Corp. and others 2025
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/
#include "OutOfLineINL.hpp"
#include "BytecodeAction.hpp"
#include "VMHelpers.hpp"
#include "ut_j9vm.h"

extern "C" {

#if defined(J9VM_OPT_SNAPSHOTS)
/* com.ibm.oti.vm.VM: public final static native void rcpAssignClassLoader(ClassLoader loader, int id) */
VM_BytecodeAction
OutOfLineINL_com_ibm_oti_vm_VM_rcpAssignClassLoader(J9VMThread *currentThread, J9Method *method)
{
	I_32 loaderType = *(I_32 *)currentThread->sp;
	j9object_t classLoaderObject = *(j9object_t *)(currentThread->sp + 1);
	VM_BytecodeAction rc = EXECUTE_BYTECODE;
	J9JavaVM *vm = currentThread->javaVM;

	VM_OutOfLineINL_Helpers::buildInternalNativeStackFrame(currentThread, method);
	if (NULL != J9VMJAVALANGCLASSLOADER_VMREF(currentThread, classLoaderObject)) {
internalError:
		setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
		rc = GOTO_THROW_CURRENT_EXCEPTION;
		goto done;
	}
	if (J9_CLASSLOADER_TYPE_BOOT == loaderType) {
		/* Initialize boot class loader. */
		if (!VM_VMHelpers::initializeBootClassLoader(currentThread, classLoaderObject)) {
			goto internalError;
		}
	} else if (J9_CLASSLOADER_TYPE_PLATFORM == loaderType) {
		/* Initialize platform class loader. */
		vm->internalVMFunctions->initializeSnapshotClassLoaderObject(vm, vm->extensionClassLoader, classLoaderObject);
	} else if (J9_CLASSLOADER_TYPE_APP == loaderType) {
		/* Initialize application class loader. */
		vm->internalVMFunctions->initializeSnapshotClassLoaderObject(vm, vm->applicationClassLoader, classLoaderObject);
	} else {
		/* Currently only the immortal ClassLoaders are supported. */
		Assert_VM_unreachable();
	}

	VM_OutOfLineINL_Helpers::restoreInternalNativeStackFrame(currentThread);
done:
	VM_OutOfLineINL_Helpers::returnVoid(currentThread, 2);
	return rc;
}
#endif /* defined(J9VM_OPT_SNAPSHOTS) */

} /* extern "C" */
