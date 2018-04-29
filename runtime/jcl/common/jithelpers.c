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
#include "j9modifiers_api.h"

/*
 * The following natives are called by the JITHelpers static initializer. They do not require special treatment by the JIT.
 */

jint JNICALL
Java_com_ibm_jit_JITHelpers_javaLangClassJ9ClassOffset(JNIEnv *env, jclass ignored)
{
	J9VMThread * vmThread = (J9VMThread *)env;
	jint offset;

	vmThread->javaVM->internalVMFunctions->internalEnterVMFromJNI(vmThread);
	offset = (jint) J9VMJAVALANGCLASS_VMREF_OFFSET(vmThread);
	vmThread->javaVM->internalVMFunctions->internalExitVMToJNI(vmThread);

	return offset;
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_j9ObjectJ9ClassOffset(JNIEnv *env, jclass ignored)
{
	return (jint) offsetof(J9Object, clazz);
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_objectHeaderHasBeenMovedInClass(JNIEnv *env, jclass ignored)
{
	return (jint) OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS;
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_objectHeaderHasBeenHashedInClass(JNIEnv *env, jclass ignored)
{
	return (jint) OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS;
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_j9ObjectFlagsMask32(JNIEnv *env, jclass ignored)
{
	return (jint) (J9_REQUIRED_CLASS_ALIGNMENT - 1);
}

jlong JNICALL
Java_com_ibm_jit_JITHelpers_j9ObjectFlagsMask64(JNIEnv *env, jclass ignored)
{
	return (jlong)(J9_REQUIRED_CLASS_ALIGNMENT - 1);
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_javaLangThreadJ9ThreadOffset(JNIEnv *env, jclass ignored)
{
	J9VMThread * vmThread = (J9VMThread *)env;
	return (jint) J9VMJAVALANGTHREAD_THREADREF_OFFSET(vmThread);
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_j9ThreadJ9JavaVMOffset(JNIEnv *env, jclass ignored)
{
	return (jint) offsetof(J9VMThread, javaVM);
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_j9ROMArrayClassArrayShapeOffset(JNIEnv *env, jclass ignored)
{
	return (jint) offsetof(J9ROMArrayClass, arrayShape);
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_j9ClassBackfillOffsetOffset(JNIEnv *env, jclass ignored)
{
	return (jint) offsetof(J9Class, backfillOffset);
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_arrayShapeElementCountMask(JNIEnv *env, jclass ignored)
{
	return (jint) 0x0000FFFF;
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_j9JavaVMIdentityHashDataOffset(JNIEnv *env, jclass ignored)
{
	return (jint) offsetof(J9JavaVM, identityHashData);
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_j9IdentityHashDataHashData1Offset(JNIEnv *env, jclass ignored)
{
	return (jint) offsetof(J9IdentityHashData, hashData1);
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_j9IdentityHashDataHashData2Offset(JNIEnv *env, jclass ignored)
{
	return (jint) offsetof(J9IdentityHashData, hashData2);
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_j9IdentityHashDataHashData3Offset(JNIEnv *env, jclass ignored)
{
	return (jint) offsetof(J9IdentityHashData, hashData3);
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_j9IdentityHashDataHashSaltTableOffset(JNIEnv *env, jclass ignored)
{
	return (jint) offsetof(J9IdentityHashData, hashSaltTable);
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_j9IdentityHashSaltPolicyStandard(JNIEnv *env, jclass ignored)
{
	return (jint) J9_IDENTITY_HASH_SALT_POLICY_STANDARD;
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_j9IdentityHashSaltPolicyRegion(JNIEnv *env, jclass ignored)
{
	return (jint) J9_IDENTITY_HASH_SALT_POLICY_REGION;
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_j9IdentityHashSaltPolicyNone(JNIEnv *env, jclass ignored)
{
	return (jint) J9_IDENTITY_HASH_SALT_POLICY_NONE;
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_identityHashSaltPolicy(JNIEnv *env, jclass ignored)
{
	J9VMThread * vmThread = (J9VMThread *)env;
	J9IdentityHashData *hashData = vmThread->javaVM->identityHashData;
	UDATA saltPolicy = hashData->hashSaltPolicy;
	return (jint) saltPolicy;
}


jint JNICALL
Java_com_ibm_jit_JITHelpers_j9ContiguousArrayHeaderSize(JNIEnv *env, jclass ignored)
{
	return (jint) sizeof(J9IndexableObjectContiguous);
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_j9DiscontiguousArrayHeaderSize(JNIEnv *env, jclass ignored)
{
	return (jint) sizeof(J9IndexableObjectDiscontiguous);
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_j9ObjectContiguousLengthOffset(JNIEnv *env, jclass ignored)
{
	return (jint) offsetof(J9IndexableObjectContiguous, size);
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_j9ObjectDiscontiguousLengthOffset(JNIEnv *env, jclass ignored)
{
	return (jint) offsetof(J9IndexableObjectDiscontiguous, size);
}

jboolean JNICALL
Java_com_ibm_jit_JITHelpers_isPlatformLittleEndian(JNIEnv *env, jclass ignored)
{
	unsigned int temp = 1;

	if (*((char*)&temp)) {
		return JNI_TRUE;
	} else {
		return JNI_FALSE;
	}
}

/*
 * The following natives are recognized and handled specially by the JIT.
 */

jboolean JNICALL
Java_com_ibm_jit_JITHelpers_is32Bit(JNIEnv *env, jobject rcv)
{
#if defined(J9VM_ENV_DATA64)
	return JNI_FALSE;
#else
	return JNI_TRUE;
#endif
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_getNumBitsInReferenceField(JNIEnv *env, jobject rcv)
{
	return (jint) (sizeof(fj9object_t) * 8);
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_getNumBytesInReferenceField(JNIEnv *env, jobject rcv)
{
	return (jint) sizeof(fj9object_t);
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_getNumBitsInDescriptionWord(JNIEnv *env, jobject rcv)
{
	return (jint) (sizeof(UDATA) * 8);
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_getNumBytesInDescriptionWord(JNIEnv *env, jobject rcv)
{
	return (jint) sizeof(UDATA);
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_getNumBytesInJ9ObjectHeader(JNIEnv *env, jobject rcv)
{
	return (jint) sizeof(J9Object);
}

#if defined(J9VM_ENV_DATA64)

jlong JNICALL
Java_com_ibm_jit_JITHelpers_getJ9ClassFromClass64(JNIEnv *env, jobject rcv, jclass c)
{
	J9VMThread * vmThread = (J9VMThread *)env;
	J9Class * clazz;

	vmThread->javaVM->internalVMFunctions->internalEnterVMFromJNI(vmThread);
	clazz = J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, J9_JNI_UNWRAP_REFERENCE(c));
	vmThread->javaVM->internalVMFunctions->internalExitVMToJNI(vmThread);
	return (jlong)(UDATA)clazz;
}

jobject JNICALL
Java_com_ibm_jit_JITHelpers_getClassFromJ9Class64(JNIEnv *env, jobject rcv, jlong j9clazz)
{
	J9VMThread * vmThread = (J9VMThread *)env;
	J9InternalVMFunctions * vmfns = vmThread->javaVM->internalVMFunctions;
	jobject classRef;

	vmfns->internalEnterVMFromJNI(vmThread);
	classRef = vmfns->j9jni_createLocalRef(env, J9VM_J9CLASS_TO_HEAPCLASS((J9Class*)(UDATA)j9clazz));
	if (NULL == classRef) {
		vmfns->setNativeOutOfMemoryError(vmThread, 0, 0);
	}
	vmfns->internalExitVMToJNI(vmThread);
	return classRef;
}

jlong JNICALL
Java_com_ibm_jit_JITHelpers_getTotalInstanceSizeFromJ9Class64(JNIEnv *env, jobject rcv, jlong j9clazz)
{
	J9Class * clazz = (J9Class *)(UDATA)j9clazz;

	return (jlong)clazz->totalInstanceSize;
}

jlong JNICALL
Java_com_ibm_jit_JITHelpers_getInstanceDescriptionFromJ9Class64(JNIEnv *env, jobject rcv, jlong j9clazz)
{
	J9Class * clazz = (J9Class *)(UDATA)j9clazz;

	return (jlong)(UDATA)clazz->instanceDescription;
}

jlong JNICALL
Java_com_ibm_jit_JITHelpers_getDescriptionWordFromPtr64(JNIEnv *env, jobject rcv, jlong descriptorPtr)
{
	return *(jlong*)(UDATA)descriptorPtr;
}

jlong JNICALL
Java_com_ibm_jit_JITHelpers_getRomClassFromJ9Class64(JNIEnv *env, jobject rcv, jlong j9clazz)
{
	J9Class * clazz = (J9Class *)(UDATA)j9clazz;

	return (jlong)(UDATA)clazz->romClass;
}

jlong JNICALL
Java_com_ibm_jit_JITHelpers_getSuperClassesFromJ9Class64(JNIEnv *env, jobject rcv, jlong j9clazz)
{
	J9Class * clazz = (J9Class *)(UDATA)j9clazz;

	return (jlong)(UDATA)clazz->superclasses;
}

jlong JNICALL
Java_com_ibm_jit_JITHelpers_getClassDepthAndFlagsFromJ9Class64(JNIEnv *env, jobject rcv, jlong j9clazz)
{
	J9Class * clazz = (J9Class *)(UDATA)j9clazz;

	return (jlong)clazz->classDepthAndFlags;
}

jlong JNICALL
Java_com_ibm_jit_JITHelpers_getBackfillOffsetFromJ9Class64(JNIEnv *env, jobject rcv, jlong j9clazz)
{
	J9Class * clazz = (J9Class *)(UDATA)j9clazz;

	return (jlong)clazz->backfillOffset;
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_getArrayShapeFromRomClass64(JNIEnv *env, jobject rcv, jlong j9romclazz)
{
	J9ROMArrayClass * romArrayClass = (J9ROMArrayClass *)(UDATA)j9romclazz;

	return (jint)romArrayClass->arrayShape;
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_getModifiersFromRomClass64(JNIEnv *env, jobject rcv, jlong j9romclazz)
{
	J9ROMClass * romClass = (J9ROMClass *)(UDATA)j9romclazz;

	return (jint)romClass->modifiers;
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_getClassFlagsFromJ9Class64(JNIEnv *env, jobject rcv, jlong j9clazz)
{
	J9Class *clazz = (J9Class *)(UDATA)j9clazz;
	return clazz->classFlags;
}
#else

jint JNICALL
Java_com_ibm_jit_JITHelpers_getJ9ClassFromClass32(JNIEnv *env, jobject rcv, jclass c)
{
	J9VMThread * vmThread = (J9VMThread *)env;
	J9Class * clazz;

	vmThread->javaVM->internalVMFunctions->internalEnterVMFromJNI(vmThread);
	clazz = J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, J9_JNI_UNWRAP_REFERENCE(c));
	vmThread->javaVM->internalVMFunctions->internalExitVMToJNI(vmThread);
	return (jint)(UDATA)clazz;
}

jobject JNICALL
Java_com_ibm_jit_JITHelpers_getClassFromJ9Class32(JNIEnv *env, jobject rcv, jint j9clazz)
{
	J9VMThread * vmThread = (J9VMThread *)env;
	J9InternalVMFunctions * vmfns = vmThread->javaVM->internalVMFunctions;
	jobject classRef;

	vmfns->internalEnterVMFromJNI(vmThread);
	classRef = vmfns->j9jni_createLocalRef(env, J9VM_J9CLASS_TO_HEAPCLASS((J9Class*)(UDATA)j9clazz));
	if (NULL == classRef) {
		vmfns->setNativeOutOfMemoryError(vmThread, 0, 0);
	}
	vmfns->internalExitVMToJNI(vmThread);
	return classRef;
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_getTotalInstanceSizeFromJ9Class32(JNIEnv *env, jobject rcv, jint j9clazz)
{
	J9Class * clazz = (J9Class *)(UDATA)j9clazz;

	return (jint)clazz->totalInstanceSize;
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_getInstanceDescriptionFromJ9Class32(JNIEnv *env, jobject rcv, jint j9clazz)
{
	J9Class * clazz = (J9Class *)(UDATA)j9clazz;

	return (jint)(UDATA)clazz->instanceDescription;
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_getDescriptionWordFromPtr32(JNIEnv *env, jobject rcv, jint descriptorPtr)
{
	return *(jint*)(UDATA)descriptorPtr;
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_getRomClassFromJ9Class32(JNIEnv *env, jobject rcv, jint j9clazz)
{
	J9Class * clazz = (J9Class *)(UDATA)j9clazz;

	return (jint)(UDATA)clazz->romClass;
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_getSuperClassesFromJ9Class32(JNIEnv *env, jobject rcv, jint j9clazz)
{
	J9Class * clazz = (J9Class *)(UDATA)j9clazz;

	return (jint)(UDATA)clazz->superclasses;
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_getClassDepthAndFlagsFromJ9Class32(JNIEnv *env, jobject rcv, jint j9clazz)
{
	J9Class * clazz = (J9Class *)(UDATA)j9clazz;

	return (jint)clazz->classDepthAndFlags;
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_getBackfillOffsetFromJ9Class32(JNIEnv *env, jobject rcv, jint j9clazz)
{
	J9Class * clazz = (J9Class *)(UDATA)j9clazz;

	return (jint)clazz->backfillOffset;
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_getArrayShapeFromRomClass32(JNIEnv *env, jobject rcv, jint j9romclazz)
{
	J9ROMArrayClass * romArrayClass = (J9ROMArrayClass *)(UDATA)j9romclazz;

	return (jint)romArrayClass->arrayShape;
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_getModifiersFromRomClass32(JNIEnv *env, jobject rcv, jint j9romclazz)
{
	J9ROMClass * romClass = (J9ROMClass *)(UDATA)j9romclazz;

	return (jint)romClass->modifiers;
}

jint JNICALL
Java_com_ibm_jit_JITHelpers_getClassFlagsFromJ9Class32(JNIEnv *env, jobject rcv, jint j9clazz)
{
	J9Class *clazz = (J9Class *)(UDATA)j9clazz;
	return clazz->classFlags;
}

#endif
