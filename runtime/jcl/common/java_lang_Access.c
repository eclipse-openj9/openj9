/*******************************************************************************
 * Copyright IBM Corp. and others 1998
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

#include "j9cp.h"
#include "j9vmconstantpool.h"
#include "jni.h"
#include "jcl.h"
#include "jclprots.h"
#include "jclglob.h"
#include "jniidcacheinit.h"

/**
 * Return the constant pool for a class.
 * Note: This method is also called from annparser.c:Java_com_ibm_oti_reflect_AnnotationParser_getConstantPool.
 *
 * @param env				The current thread.
 * @param unusedClass		See note above - this parameter should not be used.
 * @param classToIntrospect	The Class or InternalConstantPool instance the ConstantPool should represent.
 *
 * @return a ConstantPool object reference or NULL on error.
 */
jobject JNICALL
Java_java_lang_Access_getConstantPool(JNIEnv *env, jclass unusedClass, jobject classToIntrospect)
{
	jclass sun_reflect_ConstantPool = JCL_CACHE_GET(env,CLS_sun_reflect_ConstantPool);
	jobject constantPool;
	J9VMThread *vmThread = (J9VMThread *)env;
	J9InternalVMFunctions *vmFunctions = vmThread->javaVM->internalVMFunctions;
	J9MemoryManagerFunctions *gcFunctions = vmThread->javaVM->memoryManagerFunctions;
	j9object_t classObject = NULL;
	J9Class *clazz = NULL;

	/* lazy-initialize the cached field IDs */
	if (NULL == sun_reflect_ConstantPool) {
		if (!initializeSunReflectConstantPoolIDCache(env)) {
			return NULL;
		}
		sun_reflect_ConstantPool = JCL_CACHE_GET(env, CLS_sun_reflect_ConstantPool);
	}

	/* allocate the new ConstantPool object */
	constantPool = (*env)->AllocObject(env, sun_reflect_ConstantPool);
	if (NULL == constantPool) {
		return NULL;
	}

	/* if this method is called with java/lang/Class, allocate an InternalConstantPool and store the J9ConstantPool */
	vmFunctions->internalEnterVMFromJNI(vmThread);
	classObject = J9_JNI_UNWRAP_REFERENCE(classToIntrospect);
	if (J9VMJAVALANGCLASS_OR_NULL(vmThread->javaVM) == J9OBJECT_CLAZZ(vmThread, classObject)) {
		clazz = J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, classObject);
		J9ConstantPool *j9CP = (J9ConstantPool *)clazz->ramConstantPool;
		J9Class *internalConstantPool = J9VMJAVALANGINTERNALCONSTANTPOOL_OR_NULL(vmThread->javaVM);
		Assert_JCL_notNull(internalConstantPool);
		j9object_t internalConstantPoolObject = gcFunctions->J9AllocateObject(vmThread, internalConstantPool, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
		if (NULL == internalConstantPoolObject) {
			vmFunctions->setHeapOutOfMemoryError(vmThread);
			vmFunctions->internalExitVMToJNI(vmThread);
			return NULL;
		}
		J9VMJAVALANGINTERNALCONSTANTPOOL_SET_VMREF(vmThread, internalConstantPoolObject, j9CP);
		J9VMJAVALANGINTERNALCONSTANTPOOL_SET_CLAZZ(vmThread, internalConstantPoolObject, classObject);
		classToIntrospect = vmFunctions->j9jni_createLocalRef(env, internalConstantPoolObject);
	}

#if JAVA_SPEC_VERSION == 8
	J9VMSUNREFLECTCONSTANTPOOL_SET_CONSTANTPOOLOOP(vmThread, J9_JNI_UNWRAP_REFERENCE(constantPool), J9_JNI_UNWRAP_REFERENCE(classToIntrospect));
#else /* JAVA_SPEC_VERSION == 8 */
	J9VMJDKINTERNALREFLECTCONSTANTPOOL_SET_CONSTANTPOOLOOP(vmThread, J9_JNI_UNWRAP_REFERENCE(constantPool), J9_JNI_UNWRAP_REFERENCE(classToIntrospect));
#endif /* JAVA_SPEC_VERSION == 8 */

	if (NULL == clazz) {
		/* If we are here then classToIntrospect is an InternalConstantPool object, find the class object. */
		classObject = J9VMJAVALANGINTERNALCONSTANTPOOL_CLAZZ(vmThread, J9_JNI_UNWRAP_REFERENCE(classToIntrospect));
		clazz = J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, classObject);
	}
	if (NULL == clazz->replacedClass) {
		J9VMJAVALANGCLASS_SET_CONSTANTPOOLOBJECT(vmThread, classObject, J9_JNI_UNWRAP_REFERENCE(constantPool));
	}
	vmFunctions->internalExitVMToJNI(vmThread);

	return constantPool;
}
