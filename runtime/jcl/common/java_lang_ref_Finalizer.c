/*******************************************************************************
 * Copyright (c) 1998, 2018 IBM Corp. and others
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
#include "jni.h"
#include "jcl.h"
#include "jclglob.h"
#include "jclprots.h"
#include "jcl_internal.h"

/* java.lang.ref.Finalizer: private native static void runAllFinalizersImpl(); */
void JNICALL
Java_java_lang_ref_Finalizer_runAllFinalizersImpl(JNIEnv *env, jclass recv)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9MemoryManagerFunctions *mmFuncs = vm->memoryManagerFunctions;
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	vmFuncs->internalReleaseVMAccess(currentThread);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
	mmFuncs->j9gc_runFinalizersOnExit(currentThread, TRUE);
	mmFuncs->j9gc_finalizer_completeFinalizersOnExit(currentThread);
}

/* java.lang.ref.Finalizer: private native static void runFinalizationImpl(); */
void JNICALL
Java_java_lang_ref_Finalizer_runFinalizationImpl(JNIEnv *env, jclass recv)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9MemoryManagerFunctions *mmFuncs = vm->memoryManagerFunctions;
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	vmFuncs->internalReleaseVMAccess(currentThread);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
	mmFuncs->runFinalization(currentThread);
}
