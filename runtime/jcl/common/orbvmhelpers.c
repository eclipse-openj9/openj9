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

#include "jni.h"
#include "j9.h"
#include "j9cp.h"
#include "jclprots.h"

jboolean JNICALL
Java_com_ibm_oti_vm_ORBVMHelpers_is32Bit(JNIEnv *env, jclass rcv)
{
#if defined(J9VM_ENV_DATA64)
	return JNI_FALSE;
#else
	return JNI_TRUE;
#endif
}

jint JNICALL
Java_com_ibm_oti_vm_ORBVMHelpers_getNumBitsInReferenceField(JNIEnv *env, jclass rcv)
{
	return (jint) (sizeof(fj9object_t) * 8);
}

jint JNICALL
Java_com_ibm_oti_vm_ORBVMHelpers_getNumBytesInReferenceField(JNIEnv *env, jclass rcv)
{
	return (jint) sizeof(fj9object_t);
}

jint JNICALL
Java_com_ibm_oti_vm_ORBVMHelpers_getNumBitsInDescriptionWord(JNIEnv *env, jclass rcv)
{
	return (jint) (sizeof(UDATA) * 8);
}

jint JNICALL
Java_com_ibm_oti_vm_ORBVMHelpers_getNumBytesInDescriptionWord(JNIEnv *env, jclass rcv)
{
	return (jint) sizeof(UDATA);
}

jint JNICALL
Java_com_ibm_oti_vm_ORBVMHelpers_getNumBytesInJ9ObjectHeader(JNIEnv *env, jclass rcv)
{
	return (jint) sizeof(J9Object);
}

#if defined(J9VM_ENV_DATA64)

jlong JNICALL
Java_com_ibm_oti_vm_ORBVMHelpers_getJ9ClassFromClass64(JNIEnv *env, jclass rcv, jclass c)
{
	J9VMThread * vmThread = (J9VMThread *)env;
	J9InternalVMFunctions *vmFuncs = vmThread->javaVM->internalVMFunctions;
	J9Class * clazz = NULL;

	vmFuncs->internalEnterVMFromJNI(vmThread);

	if (NULL == c) {
		vmFuncs->setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		clazz = J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, *(j9object_t*)c);
	}

	vmFuncs->internalExitVMToJNI(vmThread);

	return (jlong)(UDATA)clazz;
}

jlong JNICALL
Java_com_ibm_oti_vm_ORBVMHelpers_getTotalInstanceSizeFromJ9Class64(JNIEnv *env, jclass rcv, jlong j9clazz)
{
	J9Class * clazz = (J9Class *)(UDATA)j9clazz;

	return (jlong)clazz->totalInstanceSize;
}

jlong JNICALL
Java_com_ibm_oti_vm_ORBVMHelpers_getInstanceDescriptionFromJ9Class64(JNIEnv *env, jclass rcv, jlong j9clazz)
{
	J9Class * clazz = (J9Class *)(UDATA)j9clazz;

	return (jlong)(UDATA)clazz->instanceDescription;
}

jlong JNICALL
Java_com_ibm_oti_vm_ORBVMHelpers_getDescriptionWordFromPtr64(JNIEnv *env, jclass rcv, jlong descriptorPtr)
{
	return *(jlong*)(UDATA)descriptorPtr;
}

#else

jint JNICALL
Java_com_ibm_oti_vm_ORBVMHelpers_getJ9ClassFromClass32(JNIEnv *env, jclass rcv, jclass c)
{
	J9VMThread * vmThread = (J9VMThread *)env;
	J9Class * clazz;

	vmThread->javaVM->internalVMFunctions->internalEnterVMFromJNI(vmThread);
	clazz = J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, *(j9object_t*)c);
	vmThread->javaVM->internalVMFunctions->internalExitVMToJNI(vmThread);
	return (jint)(UDATA)clazz;
}

jint JNICALL
Java_com_ibm_oti_vm_ORBVMHelpers_getTotalInstanceSizeFromJ9Class32(JNIEnv *env, jclass rcv, jint j9clazz)
{
	J9Class * clazz = (J9Class *)(UDATA)j9clazz;

	return (jint)clazz->totalInstanceSize;
}

jint JNICALL
Java_com_ibm_oti_vm_ORBVMHelpers_getInstanceDescriptionFromJ9Class32(JNIEnv *env, jclass rcv, jint j9clazz)
{
	J9Class * clazz = (J9Class *)(UDATA)j9clazz;

	return (jint)(UDATA)clazz->instanceDescription;
}

jint JNICALL
Java_com_ibm_oti_vm_ORBVMHelpers_getDescriptionWordFromPtr32(JNIEnv *env, jclass rcv, jint descriptorPtr)
{
	return *(jint*)(UDATA)descriptorPtr;
}

#endif

/**
 * Native method to mark the current frame as the point at which to terminate
 * the Last User-Defined ClassLoader (LUDCL) search and always return null.
 *
 * The method marks the previous frame and returns the values from the VMThread
 * with the stackOffset encoded in the high 32 bits and the inlined depth
 * encoded in the low 32 bits.
 *
 * It is the caller's responsibility to correctly "stack" these values.  The
 * VMThread will only ever track the "current" values.
 *
 * Marks the calling frame:
 * 		ORBVMHelpers.LUDCLMarkFrame()
 * 		Caller			<-- Marks this frame
 *
 */
jlong JNICALL
Java_com_ibm_rmi_io_IIOPInputStream_00024LUDCLStackWalkOptimizer_LUDCLMarkFrame(JNIEnv *env, jclass rcv)
{
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM *vm = vmThread->javaVM;
	J9InternalVMFunctions *vmfuncs = vm->internalVMFunctions;
	J9StackWalkState walkState;
	jlong previousValue = (((jlong)vmThread->ludclBPOffset) << 32) | vmThread->ludclInlineDepth;

	walkState.walkThread = vmThread;
	walkState.skipCount = 1;
	walkState.maxFrames = 1;
	walkState.flags = J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_COUNT_SPECIFIED | J9_STACKWALK_INCLUDE_NATIVES;
	walkState.userData1 = NULL;

	vmfuncs->internalEnterVMFromJNI(vmThread);
	vm->walkStackFrames(vmThread, &walkState);
	vmfuncs->internalExitVMToJNI(vmThread);

	vmThread->ludclBPOffset = (U_32) (vmThread->stackObject->end - walkState.bp);
	vmThread->ludclInlineDepth = (U_32) walkState.inlineDepth;

	return previousValue;
}

/**
 * Native method to restore the previous marked frame state to the VMThread for
 * the Last User-Defined ClassLoader (LUDCL) search optimization.
 *
 * This method uses the returned value from the markFrame() native to restore
 * the previous state of the mark values.
 *
 * Returns true if unmarking a frame that has been marked.  False otherwise.
 *
 */
jboolean JNICALL
Java_com_ibm_rmi_io_IIOPInputStream_00024LUDCLStackWalkOptimizer_LUDCLUnmarkFrameImpl(JNIEnv *env, jclass rcv, jlong previousValue)
{
	J9VMThread *thread = (J9VMThread *) env;
	U_32 bpOffset = (U_32) (previousValue >> 32);
	U_32 inlineDepth = (U_32)previousValue;

	if ((thread->ludclBPOffset == 0) && (thread->ludclInlineDepth == 0)) {
		return JNI_FALSE;
	}
	thread->ludclInlineDepth = inlineDepth;
	thread->ludclBPOffset = bpOffset;
	return JNI_TRUE;
}

/**
 * Caller must hold VMACCESS.
 *
 * Stack Frame iterator function that searches for the latest User-Defined ClassLoader with early
 * walk termination based if it finds a frame that has been "marked".
 *
 * See sunvmi.c for a similar iterator without the early stop capability.
 */
static UDATA latestUserDefinedLoaderIterator(J9VMThread * currentThread, J9StackWalkState * walkState)
{
	J9JavaVM * vm = currentThread->javaVM;
	J9Class * currentClass = J9_CLASS_FROM_CP(walkState->constantPool);
	J9ClassLoader * classLoader = currentClass->classLoader;
	U_32 bpOffset = (U_32) (currentThread->stackObject->end - walkState->bp);

	if (classLoader != vm->systemClassLoader) {
		if ( (vm->jliArgumentHelper && vm->internalVMFunctions->instanceOfOrCheckCast(currentClass, J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, *((j9object_t*) vm->jliArgumentHelper)))) ||
		     (vm->srMethodAccessor && vm->internalVMFunctions->instanceOfOrCheckCast(currentClass, J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, *((j9object_t*) vm->srMethodAccessor)))) ||
		     (vm->srConstructorAccessor && vm->internalVMFunctions->instanceOfOrCheckCast(currentClass, J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, *((j9object_t*) vm->srConstructorAccessor)))) ) {
			/* skip reflection classes */
		} else {
			walkState->userData1 = J9CLASSLOADER_CLASSLOADEROBJECT(currentThread, classLoader);
			return J9_STACKWALK_STOP_ITERATING;
		}
	}

	if ((currentThread->ludclBPOffset == bpOffset) && (currentThread->ludclInlineDepth == walkState->inlineDepth)) {
		return J9_STACKWALK_STOP_ITERATING;
	}

	return J9_STACKWALK_KEEP_ITERATING;
}

/**
 * Walk the stackframes from the TOS and find the latest user-defined class loader.
 * This will terminate the walk early if it detects a "marked" frame.
 * (See Java_com_ibm_oti_vm_ORBVMHelpers_LUDCLMarkFrame for details).
 */
jobject JNICALL
Java_com_ibm_oti_vm_ORBVMHelpers_LatestUserDefinedLoader(JNIEnv *env, jclass rcv)
{
	J9VMThread * vmThread = (J9VMThread *) env;
	J9JavaVM * vm = vmThread->javaVM;
	J9StackWalkState walkState;
	jobject result;

	walkState.walkThread = vmThread;
	walkState.skipCount = 0;
	walkState.flags = J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_INCLUDE_NATIVES;
	walkState.userData1 = NULL;
	walkState.frameWalkFunction = latestUserDefinedLoaderIterator;
	vm->internalVMFunctions->internalEnterVMFromJNI(vmThread);
	vm->walkStackFrames(vmThread, &walkState);

	result = vm->internalVMFunctions->j9jni_createLocalRef(env, walkState.userData1);
	vm->internalVMFunctions->internalExitVMToJNI(vmThread);

	return result;
}

