/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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

#include "j9protos.h"
#include "jcl.h"
#include "jclglob.h"
#include "jclprots.h"
#include "jcl_internal.h"
#include "jni.h"

extern "C" {

#if JAVA_SPEC_VERSION >= 16

/**
 * A memory segment is associated with a native scope/session owned by a thread, in which case
 * the memory segment belonging to the scope/session will be automatically released in OpenJDK
 * by invoking this function when the native scope/session is terminated.
 * See src/jdk.incubator.foreign/share/classes/jdk/internal/foreign/abi/UpcallStubs.java
 * in OpenJDK for details.
 *
 * To adapt to the calling mechanism in our implementation, a hashtable is created to stored
 * the metadata for each generated thunk, which means the allocated memories associated with
 * the metadata and the thunk are released in this way to ensure we have sufficient memory
 * to be allocated on the heap for thunk (especially on AIX only with 4KB virtual memory).
 */
jboolean JNICALL
Java_jdk_internal_foreign_abi_UpcallStubs_freeUpcallStub0(JNIEnv *env, jclass clazz, jlong address)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	const J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
#if defined(AIXPPC) || defined(J9ZOS390)
	/* The first element of the function pointer holds the thunk memory address,
	 * as specified in vm/ap64/UpcallThunkGen.cpp and vm/mz64/UpcallThunkGen.cpp.
	 */
	void **functionPtr = (void **)(intptr_t)address;
	void *thunkAddr = *functionPtr;
#else /* defined(AIXPPC) || defined(J9ZOS390) */
	void *thunkAddr = (void *)(intptr_t)address;
#endif /* defined(AIXPPC) || defined(J9ZOS390) */

	PORT_ACCESS_FROM_JAVAVM(vm);

	omrthread_monitor_enter(vm->thunkHeapListMutex);
	if (NULL != thunkAddr) {
		J9HashTable *metaDataHashTable = vm->thunkHeapHead->metaDataHashTable;

		if (NULL != metaDataHashTable) {
			J9UpcallMetaDataEntry metaDataEntry = {0};
			metaDataEntry.thunkAddrValue = (UDATA)thunkAddr;
			metaDataEntry.upcallMetaData = NULL;
			J9UpcallMetaDataEntry * result = (J9UpcallMetaDataEntry *)hashTableFind(metaDataHashTable, &metaDataEntry);
			if (NULL != result) {
				J9UpcallMetaData *metaData = result->upcallMetaData;
				J9Heap *thunkHeap = metaData->thunkHeapWrapper->heap;
				J9UpcallNativeSignature *nativeFuncSig = metaData->nativeFuncSignature;
				if (NULL != nativeFuncSig) {
					j9mem_free_memory(nativeFuncSig->sigArray);
					j9mem_free_memory(nativeFuncSig);
					nativeFuncSig = NULL;
				}
				vmFuncs->internalEnterVMFromJNI(currentThread);
				vmFuncs->j9jni_deleteGlobalRef(env, metaData->mhMetaData, JNI_FALSE);
				vmFuncs->internalExitVMToJNI(currentThread);
				j9mem_free_memory(metaData);
				hashTableRemove(metaDataHashTable, result);

				if (NULL != thunkHeap) {
					omrthread_jit_write_protect_disable();
					j9heap_free(thunkHeap, thunkAddr);
					omrthread_jit_write_protect_enable();
				}
			}
		}
	}
	omrthread_monitor_exit(vm->thunkHeapListMutex);

	return JNI_TRUE;
}

/**
 * The function is unused in OpenJ9 which is only invoked in OpenJDK
 * so as to avoid modifying the existing code in OpenJDK.
 */
void JNICALL
Java_jdk_internal_foreign_abi_UpcallStubs_registerNatives(JNIEnv *env, jclass clazz)
{
}

#endif /* JAVA_SPEC_VERSION >= 16 */

} /* extern "C" */
