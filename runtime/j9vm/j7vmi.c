/*******************************************************************************
 * Copyright (c) 2002, 2019 IBM Corp. and others
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
 * @brief  This file contains implementations of the public JVM interface (JVM_ functions)
 * which simply forward to a concrete implementation located either in the JCL library
 * or proxy forwarder.
 */

#include <stdlib.h>
#include <assert.h>


#include "j9consts.h"
#include "jclprots.h"
#include "j9protos.h"
#include "j9.h"
#include "omrgcconsts.h"
#include "j9modifiers_api.h"
#include "j9vm_internal.h"
#include "j9vmconstantpool.h"
#include "rommeth.h"
#include "sunvmi_api.h"
#include "util_api.h"
/* TODO need to determine why j9vmnls.h is not sufficient */
#include "j9jclnls.h"
#include "ut_j9scar.h"
#include "j9vmnls.h"

extern void initializeVMI(void);

#define POK_BOOLEAN 4
#define POK_CHAR 5
#define POK_FLOAT 6
#define POK_DOUBLE 7
#define POK_BYTE 8
#define POK_SHORT 9
#define POK_INT 10
#define POK_LONG 11

static J9ThreadEnv*
getJ9ThreadEnv(JNIEnv* env)
{
	JavaVM* jniVM;
	static J9ThreadEnv* threadEnv;

	if (NULL != threadEnv) {
		return threadEnv;
	}

	/* Get the thread functions */
	(*env)->GetJavaVM(env, &jniVM);
	(*jniVM)->GetEnv(jniVM, (void**)&threadEnv, J9THREAD_VERSION_1_1);
	return threadEnv;
}


/**
 * Copies the contents of <code>array1</code> starting at offset <code>start1</code>
 * into <code>array2</code> starting at offset <code>start2</code> for
 * <code>length</code> elements.
 *
 * @param		array1 		the array to copy out of
 * @param		start1 		the starting index in array1
 * @param		array2 		the array to copy into
 * @param		start2 		the starting index in array2
 * @param		length 		the number of elements in the array to copy
 */
void JNICALL
JVM_ArrayCopy(JNIEnv *env, jclass ignored, jobject src, jint src_pos, jobject dst, jint dst_pos, jint length)
{
	J9VMThread *currentThread;
	J9JavaVM *vm;
	J9InternalVMFunctions *vmFuncs;

	Assert_SC_notNull(env);

	currentThread = (J9VMThread *) env;
	vm = currentThread->javaVM;
	vmFuncs = vm->internalVMFunctions;

	vmFuncs->internalEnterVMFromJNI(currentThread);

	if ((NULL == src) || (NULL == dst)) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		j9object_t srcArray = J9_JNI_UNWRAP_REFERENCE(src);
		j9object_t dstArray = J9_JNI_UNWRAP_REFERENCE(dst);

		J9ArrayClass *srcArrayClass = (J9ArrayClass *) J9OBJECT_CLAZZ(currentThread, srcArray);
		J9ArrayClass *dstArrayClass = (J9ArrayClass *) J9OBJECT_CLAZZ(currentThread, dstArray);

		if (J9CLASS_IS_ARRAY(srcArrayClass) && J9CLASS_IS_ARRAY(dstArrayClass)) {
			if ((src_pos < 0) || (dst_pos < 0) || (length < 0)
			|| (((jint) J9INDEXABLEOBJECT_SIZE(currentThread, srcArray)) < (src_pos + length))
			|| (((jint) J9INDEXABLEOBJECT_SIZE(currentThread, dstArray)) < (dst_pos + length))) {
				vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGARRAYINDEXOUTOFBOUNDSEXCEPTION, NULL);
			} else {
				J9Class *srcTypeOfArray = srcArrayClass->componentType;
				J9Class *dstTypeOfArray = dstArrayClass->componentType;

				int i = 0;
				if (J9ROMCLASS_IS_PRIMITIVE_TYPE(srcTypeOfArray->romClass) && J9ROMCLASS_IS_PRIMITIVE_TYPE(dstTypeOfArray->romClass)) {
					if (srcTypeOfArray == dstTypeOfArray) {
						if (vm->longReflectClass == srcTypeOfArray) {
							if ((srcArray == dstArray) && (((src_pos < dst_pos) && (src_pos + length > dst_pos)))) {
								for (i = length - 1; i >= 0; i--) {
									J9JAVAARRAYOFLONG_STORE(currentThread, dstArray, i + dst_pos, J9JAVAARRAYOFLONG_LOAD(currentThread, srcArray, i + src_pos));
								}
							} else {
								for (i = 0; i < length; i++) {
									J9JAVAARRAYOFLONG_STORE(currentThread, dstArray, i + dst_pos, J9JAVAARRAYOFLONG_LOAD(currentThread, srcArray, i + src_pos));
								}
							}
						} else if (vm->booleanReflectClass == srcTypeOfArray) {
							if ((srcArray == dstArray) && (((src_pos < dst_pos) && (src_pos + length > dst_pos)))) {
								for (i = length - 1; i >= 0; i--) {
									J9JAVAARRAYOFBOOLEAN_STORE(currentThread, dstArray, i + dst_pos, J9JAVAARRAYOFBOOLEAN_LOAD(currentThread, srcArray, i + src_pos));
								}
							} else {
								for (i = 0; i < length; i++) {
									J9JAVAARRAYOFBOOLEAN_STORE(currentThread, dstArray, i + dst_pos, J9JAVAARRAYOFBOOLEAN_LOAD(currentThread, srcArray, i + src_pos));
								}
							}
						} else if (vm->byteReflectClass == srcTypeOfArray) {
							if ((srcArray == dstArray) && (((src_pos < dst_pos) && (src_pos + length > dst_pos)))) {
								for (i = length - 1; i >= 0; i--) {
									J9JAVAARRAYOFBYTE_STORE(currentThread, dstArray, i + dst_pos, J9JAVAARRAYOFBYTE_LOAD(currentThread, srcArray, i + src_pos));
								}
							} else {
								for (i = 0; i < length; i++) {
									J9JAVAARRAYOFBYTE_STORE(currentThread, dstArray, i + dst_pos, J9JAVAARRAYOFBYTE_LOAD(currentThread, srcArray, i + src_pos));
								}
							}
						} else if (vm->charReflectClass == srcTypeOfArray) {
							if ((srcArray == dstArray) && (((src_pos < dst_pos) && (src_pos + length > dst_pos)))) {
								for (i = length - 1; i >= 0; i--) {
									J9JAVAARRAYOFCHAR_STORE(currentThread, dstArray, i + dst_pos, J9JAVAARRAYOFCHAR_LOAD(currentThread, srcArray, i + src_pos));
								}
							} else {
								for (i = 0; i < length; i++) {
									J9JAVAARRAYOFCHAR_STORE(currentThread, dstArray, i + dst_pos, J9JAVAARRAYOFCHAR_LOAD(currentThread, srcArray, i + src_pos));
								}
							}
						} else if (vm->shortReflectClass == srcTypeOfArray) {
							if ((srcArray == dstArray) && (((src_pos < dst_pos) && (src_pos + length > dst_pos)))) {
								for (i = length - 1; i >= 0; i--) {
									J9JAVAARRAYOFSHORT_STORE(currentThread, dstArray, i + dst_pos, J9JAVAARRAYOFSHORT_LOAD(currentThread, srcArray, i + src_pos));
								}
							} else {
								for (i = 0; i < length; i++) {
									J9JAVAARRAYOFSHORT_STORE(currentThread, dstArray, i + dst_pos, J9JAVAARRAYOFSHORT_LOAD(currentThread, srcArray, i + src_pos));
								}
							}
						} else if (vm->intReflectClass == srcTypeOfArray) {
							if ((srcArray == dstArray) && (((src_pos < dst_pos) && (src_pos + length > dst_pos)))) {
								for (i = length - 1; i >= 0; i--) {
									J9JAVAARRAYOFINT_STORE(currentThread, dstArray, i + dst_pos, J9JAVAARRAYOFINT_LOAD(currentThread, srcArray, i + src_pos));
								}
							} else {
								for (i = 0; i < length; i++) {
									J9JAVAARRAYOFINT_STORE(currentThread, dstArray, i + dst_pos, J9JAVAARRAYOFINT_LOAD(currentThread, srcArray, i + src_pos));
								}
							}
						} else if (vm->floatReflectClass == srcTypeOfArray) {
							if ((srcArray == dstArray) && (((src_pos < dst_pos) && (src_pos + length > dst_pos)))) {
								for (i = length - 1; i >= 0; i--) {
									J9JAVAARRAYOFFLOAT_STORE(currentThread, dstArray, i + dst_pos, J9JAVAARRAYOFFLOAT_LOAD(currentThread, srcArray, i + src_pos));
								}
							} else {
								for (i = 0; i < length; i++) {
									J9JAVAARRAYOFFLOAT_STORE(currentThread, dstArray, i + dst_pos, J9JAVAARRAYOFFLOAT_LOAD(currentThread, srcArray, i + src_pos));
								}
							}
						} else if (vm->doubleReflectClass == srcTypeOfArray) {
							if ((srcArray == dstArray) && (((src_pos < dst_pos) && (src_pos + length > dst_pos)))) {
								for (i = length - 1; i >= 0; i--) {
									J9JAVAARRAYOFDOUBLE_STORE(currentThread, dstArray, i + dst_pos, J9JAVAARRAYOFDOUBLE_LOAD(currentThread, srcArray, i + src_pos));
								}
							} else {
								for (i = 0; i < length; i++) {
									J9JAVAARRAYOFDOUBLE_STORE(currentThread, dstArray, i + dst_pos, J9JAVAARRAYOFDOUBLE_LOAD(currentThread, srcArray, i + src_pos));
								}
							}
						} else {
							vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
						}
					} else {
						vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGARRAYSTOREEXCEPTION, NULL);
					}
				} else if (!J9ROMCLASS_IS_PRIMITIVE_TYPE(srcTypeOfArray->romClass) && !J9ROMCLASS_IS_PRIMITIVE_TYPE(dstTypeOfArray->romClass)) {
					if (srcArray == dstArray) {
						if ((((src_pos < dst_pos) && (src_pos + length > dst_pos)))) {
							for (i = length - 1; i >= 0; i--) {
								J9JAVAARRAYOFOBJECT_STORE(currentThread, dstArray, i + dst_pos, J9JAVAARRAYOFOBJECT_LOAD(currentThread, srcArray, i + src_pos));
							}
						} else {
							for (i = 0; i < length; i++) {
								J9JAVAARRAYOFOBJECT_STORE(currentThread, dstArray, i + dst_pos, J9JAVAARRAYOFOBJECT_LOAD(currentThread, srcArray, i + src_pos));
							}
						}
					} else {
						j9object_t srcObject = NULL;
						J9Class *srcObjectClass = NULL;
						for (i = 0; i < length; i++) {
							srcObject = J9JAVAARRAYOFOBJECT_LOAD(currentThread, srcArray, i + src_pos);
							if (NULL == srcObject) {
								J9JAVAARRAYOFOBJECT_STORE(currentThread, dstArray, i + dst_pos, srcObject);
							} else {
								srcObjectClass = J9OBJECT_CLAZZ(currentThread, srcObject);

								if (isSameOrSuperClassOf (dstTypeOfArray, srcObjectClass)) {
									J9JAVAARRAYOFOBJECT_STORE(currentThread, dstArray, i + dst_pos, srcObject);
								} else {
									vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGARRAYSTOREEXCEPTION, NULL);
									break;
								}
							}
						}
					}
				} else {
					vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGARRAYSTOREEXCEPTION, NULL);
				}
			}
		} else {
			vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGARRAYSTOREEXCEPTION, NULL);
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);
}



jobject JNICALL
JVM_AssertionStatusDirectives(jint arg0, jint arg1)
{
	assert(!"JVM_AssertionStatusDirectives() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_Clone(jint arg0, jint arg1)
{
	assert(!"JVM_Clone() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_CompileClass(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_CompileClass() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_CompileClasses(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_CompileClasses() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_CompilerCommand(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_CompilerCommand() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_CountStackFrames(jint arg0, jint arg1)
{
	assert(!"JVM_CountStackFrames() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_CurrentThread(JNIEnv* env, jclass java_lang_Thread)
{
	J9VMThread* vmThread = (J9VMThread*)env;
	if (NULL == vmThread->threadObject) {
		return NULL;
	} else {
		return (jobject)(&vmThread->threadObject);
	}
}



jboolean JNICALL
JVM_DesiredAssertionStatus(JNIEnv* env, jobject arg1, jobject arg2)
{
	return JNI_FALSE;
}



jobject JNICALL
JVM_DisableCompiler(jint arg0, jint arg1)
{
	assert(!"JVM_DisableCompiler() stubbed!");
	return NULL;
}

static jclass
java_lang_J9VMInternals(JNIEnv* env)
{
	static jclass cached = NULL;
	jclass localRef = NULL;

	if (NULL != cached) {
		return cached;
	}

	localRef = (*env)->FindClass(env, "java/lang/J9VMInternals");
	assert(localRef != NULL);

	cached = (*env)->NewGlobalRef(env, localRef);
	if (cached == NULL) {
		return NULL;
	}

	(*env)->DeleteLocalRef(env,localRef);
	assert(localRef != NULL);
	return cached;
}

static jmethodID
java_lang_J9VMInternals_doPrivileged(JNIEnv* env)
{
	static jmethodID cached = NULL;
	if (NULL != cached) {
		return cached;
	}

	cached = (*env)->GetStaticMethodID(env, java_lang_J9VMInternals(env), "doPrivileged", "(Ljava/security/PrivilegedAction;)Ljava/lang/Object;" );
	assert(cached != NULL);
	return cached;
}

static jmethodID
java_lang_J9VMInternals_doPrivilegedWithException(JNIEnv* env)
{
	static jmethodID cached = NULL;
	if (NULL != cached) {
		return cached;
	}

	cached = (*env)->GetStaticMethodID(env, java_lang_J9VMInternals(env), "doPrivileged", "(Ljava/security/PrivilegedExceptionAction;)Ljava/lang/Object;" );
	assert(cached != NULL);
	return cached;
}


jobject JNICALL
JVM_DoPrivileged(JNIEnv* env, jobject java_security_AccessController, jobject action, jboolean unknown, jboolean isExceptionAction)
{
	PORT_ACCESS_FROM_ENV(env);
#if 0
	j9tty_printf(
			PORTLIB,
			"JVM_DoPrivileged(env=0x%p, accessController=0x%p, action=0x%p, arg3=0x%p, arg4=0x%p\n",
			env, java_security_AccessController, action, arg3, arg4);
#endif

	jmethodID methodID =
			(JNI_TRUE == isExceptionAction)
				? java_lang_J9VMInternals_doPrivilegedWithException(env)
				: java_lang_J9VMInternals_doPrivileged(env);

	return (*env)->CallStaticObjectMethod(
			env,
			java_lang_J9VMInternals(env),
			methodID,
			action);
}



jobject JNICALL
JVM_EnableCompiler(jint arg0, jint arg1)
{
	assert(!"JVM_EnableCompiler() stubbed!");
	return NULL;
}



void JNICALL
JVM_FillInStackTrace(JNIEnv* env, jobject throwable)
{
	J9VMThread* currentThread = (J9VMThread*) env;
	J9JavaVM* javaVM = currentThread->javaVM;
	J9InternalVMFunctions* vmfns = javaVM->internalVMFunctions;
	j9object_t unwrappedThrowable;

	vmfns->internalEnterVMFromJNI(currentThread);
	unwrappedThrowable = J9_JNI_UNWRAP_REFERENCE(throwable);
	if ((0 == (javaVM->runtimeFlags & J9_RUNTIME_OMIT_STACK_TRACES)) &&
		/* If the enableWritableStackTrace field is resolved, check it. If it's false, do not create the stack trace. */
		/* TODO should be: (0 == J9VMCONSTANTPOOL_FIELDREF_AT(javaVM, J9VMCONSTANTPOOL_JAVALANGTHROWABLE_ENABLEWRITABLESTACKTRACE)->flags) */
		((J9_CP_TYPE(J9ROMCLASS_CPSHAPEDESCRIPTION(J9_CLASS_FROM_CP((javaVM)->jclConstantPool)->romClass), J9VMCONSTANTPOOL_JAVALANGTHROWABLE_ENABLEWRITABLESTACKTRACE) == J9CPTYPE_UNUSED) ||
			J9VMJAVALANGTHROWABLE_ENABLEWRITABLESTACKTRACE(currentThread, unwrappedThrowable)))
	{
		UDATA flags = J9_STACKWALK_CACHE_PCS | J9_STACKWALK_WALK_TRANSLATE_PC | J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_SKIP_INLINES;
		J9StackWalkState* walkState = currentThread->stackWalkState;
		j9object_t result = (j9object_t) J9VMJAVALANGTHROWABLE_WALKBACK(currentThread, unwrappedThrowable);
		UDATA rc;
		UDATA i;
		UDATA framesWalked;

		/* Do not hide exception frames if fillInStackTrace is called on an exception which already has a stack trace.  In the out of memory case,
		 * there is a bit indicating that we should explicitly override this behaviour, since we've precached the stack trace array. */
		if ((NULL == result) || (J9_PRIVATE_FLAGS_FILL_EXISTING_TRACE == (currentThread->privateFlags & J9_PRIVATE_FLAGS_FILL_EXISTING_TRACE))) {
			flags |= J9_STACKWALK_HIDE_EXCEPTION_FRAMES;
			walkState->restartException = unwrappedThrowable;
		}
		walkState->skipCount = 1; /* skip the INL frame -- TODO revisit this */
		walkState->walkThread = currentThread;
		walkState->flags = flags;

		rc = javaVM->walkStackFrames(currentThread, walkState);

		if (J9_STACKWALK_RC_NONE != rc) {
			/* Avoid infinite recursion if already throwing OOM. */
			if (J9_PRIVATE_FLAGS_OUT_OF_MEMORY == (currentThread->privateFlags & J9_PRIVATE_FLAGS_OUT_OF_MEMORY)) {
				goto setThrowableSlots;
			}
			vmfns->setNativeOutOfMemoryError(currentThread, J9NLS_JCL_FAILED_TO_CREATE_STACK_TRACE); /* TODO replace with local NLS message */
			goto done;
		}
		framesWalked = walkState->framesWalked;

		/* If there is no stack trace in the exception, or we are not in the out of memory case, allocate a new stack trace. */
		if ((NULL == result) || (0 == (currentThread->privateFlags & J9_PRIVATE_FLAGS_FILL_EXISTING_TRACE))) {
#ifdef J9VM_ENV_DATA64
			J9Class* arrayClass = javaVM->longArrayClass;
#else
			J9Class* arrayClass = javaVM->intArrayClass;
#endif
			result = javaVM->memoryManagerFunctions->J9AllocateIndexableObject(currentThread, arrayClass, (U_32)framesWalked, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
			if (NULL == result) {
				vmfns->setHeapOutOfMemoryError(currentThread);
				goto done;
			}
			/* Reload after allocation */
			unwrappedThrowable = J9_JNI_UNWRAP_REFERENCE(throwable);
		} else {
			UDATA maxSize = J9INDEXABLEOBJECT_SIZE(currentThread, result);
			if (framesWalked > maxSize) {
				framesWalked = maxSize;
			}
		}

		for (i = 0; i < framesWalked; ++i) {
			J9JAVAARRAYOFUDATA_STORE(currentThread, result, i, walkState->cache[i]);
		}

		vmfns->freeStackWalkCaches(currentThread, walkState);

setThrowableSlots:
		J9VMJAVALANGTHROWABLE_SET_WALKBACK(currentThread, unwrappedThrowable, result);
		J9VMJAVALANGTHROWABLE_SET_STACKTRACE(currentThread, unwrappedThrowable, NULL);
	}
done:
	vmfns->internalExitVMToJNI(currentThread);
}


/**
 * Find the specified class in given class loader 
 *
 * @param env
 * @param className    null-terminated class name string.
 * @param init         initialize the class when set
 * @param classLoader  classloader of the class 
 * @param throwError   set to true in order to throw errors
 * @return Assumed to be a jclass.
 *
 * Note: this call is implemented from info provided via CMVC 154874. 
 */
jobject JNICALL
JVM_FindClassFromClassLoader(JNIEnv* env, char* className, jboolean init, jobject classLoader, jboolean throwError)
{
	ENSURE_VMI();
	return g_VMI->JVM_FindClassFromClassLoader(env, className, init, classLoader, throwError);
}



/**
 * Find the specified class using boot class loader
 *
 * @param env
 * @param className    null-terminated class name string.
 * @return jclass or NULL on error.
 *
 * Note: this call is implemented from info provided via CMVC 156938.
 */
jobject JNICALL
JVM_FindClassFromBootLoader(JNIEnv* env, char* className)
{
	return JVM_FindClassFromClassLoader(env, className, JNI_TRUE, NULL, JNI_FALSE);
}


jobject JNICALL
JVM_FindLoadedClass(JNIEnv* env, jobject classLoader, jobject className)
{
	J9VMThread* currentThread = (J9VMThread*) env;
	J9JavaVM*  vm = currentThread->javaVM;
	J9ClassLoader* vmClassLoader;
	J9Class* loadedClass = NULL;

	vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

	if (NULL == className) {
		goto done;
	}

	vmClassLoader = J9VMJAVALANGCLASSLOADER_VMREF(currentThread, J9_JNI_UNWRAP_REFERENCE(classLoader));
	if (NULL == vmClassLoader) {
		goto done;
	}

	loadedClass = vm->internalVMFunctions->internalFindClassString(
			currentThread,
			NULL,
			J9_JNI_UNWRAP_REFERENCE(className),
			vmClassLoader,
			J9_FINDCLASS_FLAG_EXISTING_ONLY);
done:
	vm->internalVMFunctions->internalExitVMToJNI(currentThread);

	if (NULL == loadedClass) {
		return NULL;
	}

	return (jobject)(&loadedClass->classObject);
}



jobject JNICALL
JVM_FindPrimitiveClass(JNIEnv* env, char* name)
{
	J9JavaVM* vm = ((J9VMThread*)env)->javaVM;

	/* code inspired by reflecthelp.c */
	if (0 == strcmp(name,"int")) {
		return (jobject)(&vm->intReflectClass->classObject);
	}
	if (0 == strcmp(name,"boolean")) {
		return (jobject)(&vm->booleanReflectClass->classObject);
	}
	if (0 == strcmp(name,"long")) {
		return (jobject)(&vm->longReflectClass->classObject);
	}
	if (0 == strcmp(name,"double")) {
		return (jobject)(&vm->doubleReflectClass->classObject);
	}
	if (0 == strcmp(name,"float")) {
		return (jobject)(&vm->floatReflectClass->classObject);
	}
	if (0 == strcmp(name,"char")) {
		return (jobject)(&vm->charReflectClass->classObject);
	}
	if (0 == strcmp(name,"byte")) {
		return (jobject)(&vm->byteReflectClass->classObject);
	}
	if (0 == strcmp(name,"short")) {
		return (jobject)(&vm->shortReflectClass->classObject);
	}
	if (0 == strcmp(name,"void")) {
		return (jobject)(&vm->voidReflectClass->classObject);
	}

	assert(!"JVM_FindPrimitiveClass() stubbed!");
	return NULL;
}



/**
 * Get the Array element at the index
 * This function may lock, gc or throw exception.
 * @param array The array
 * @param index The index
 * @return the element at the index
 */
jobject JNICALL
JVM_GetArrayElement(JNIEnv *env, jobject array, jint index)
{
	J9VMThread *currentThread;
	J9JavaVM *vm;
	J9InternalVMFunctions *vmFuncs;
	jobject elementJNIRef = NULL;

	Assert_SC_notNull(env);

	currentThread = (J9VMThread *) env;
	vm = currentThread->javaVM;
	vmFuncs = vm->internalVMFunctions;

	vmFuncs->internalEnterVMFromJNI(currentThread);

	if (NULL == array) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		j9object_t j9array = J9_JNI_UNWRAP_REFERENCE(array);
		J9ArrayClass *arrayClass = (J9ArrayClass *) J9OBJECT_CLAZZ(currentThread, j9array);
		J9Class *typeOfArray = arrayClass->componentType;

		if (J9CLASS_IS_ARRAY(arrayClass)) {
			if ((index < 0) || (((jint) J9INDEXABLEOBJECT_SIZE(currentThread, j9array)) <= index)) {
				vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGARRAYINDEXOUTOFBOUNDSEXCEPTION, NULL);
			} else {
				if (J9ROMCLASS_IS_PRIMITIVE_TYPE(typeOfArray->romClass)) {
					j9object_t primitiveElement = NULL;
					J9MemoryManagerFunctions *memManagerFuncs = vm->memoryManagerFunctions;
					BOOLEAN illegalArgSeen = FALSE;

					if (vm->longReflectClass == typeOfArray) {
						primitiveElement = memManagerFuncs->J9AllocateObject(currentThread, J9VMJAVALANGLONG_OR_NULL(vm), J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
						if (NULL != primitiveElement) {
							jlong val = J9JAVAARRAYOFLONG_LOAD(currentThread, j9array, index);
							J9VMJAVALANGLONG_SET_VALUE(currentThread, primitiveElement, val);
							elementJNIRef = vmFuncs->j9jni_createLocalRef((JNIEnv *)currentThread, primitiveElement);
						}
					} else if (vm->booleanReflectClass == typeOfArray) {
						primitiveElement = memManagerFuncs->J9AllocateObject(currentThread, J9VMJAVALANGBOOLEAN_OR_NULL(vm), J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
						if (NULL != primitiveElement) {
							jboolean val = J9JAVAARRAYOFBOOLEAN_LOAD(currentThread, j9array, index);
							J9VMJAVALANGBOOLEAN_SET_VALUE(currentThread, primitiveElement, val);
							elementJNIRef = vmFuncs->j9jni_createLocalRef((JNIEnv *) currentThread, primitiveElement);
						}
					} else if (vm->byteReflectClass == typeOfArray) {
						primitiveElement = memManagerFuncs->J9AllocateObject(currentThread, J9VMJAVALANGBYTE_OR_NULL(vm), J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
						if (NULL != primitiveElement) {
							jbyte val = J9JAVAARRAYOFBYTE_LOAD(currentThread, j9array, index);
							J9VMJAVALANGBYTE_SET_VALUE(currentThread, primitiveElement, val);
							elementJNIRef = vmFuncs->j9jni_createLocalRef((JNIEnv *) currentThread, primitiveElement);
						}
					} else if (vm->charReflectClass == typeOfArray) {
						primitiveElement = memManagerFuncs->J9AllocateObject(currentThread, J9VMJAVALANGCHARACTER_OR_NULL(vm), J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
						if (NULL != primitiveElement) {
							jchar val = J9JAVAARRAYOFCHAR_LOAD(currentThread, j9array, index);
							J9VMJAVALANGCHARACTER_SET_VALUE(currentThread, primitiveElement, val);
							elementJNIRef = vmFuncs->j9jni_createLocalRef((JNIEnv *) currentThread, primitiveElement);
						}
					} else if (vm->shortReflectClass == typeOfArray) {
						primitiveElement = memManagerFuncs->J9AllocateObject(currentThread, J9VMJAVALANGSHORT_OR_NULL(vm), J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
						if (NULL != primitiveElement) {
							jshort val = J9JAVAARRAYOFSHORT_LOAD(currentThread, j9array, index);
							J9VMJAVALANGSHORT_SET_VALUE(currentThread, primitiveElement, val);
							elementJNIRef = vmFuncs->j9jni_createLocalRef((JNIEnv *) currentThread, primitiveElement);
						}
					} else if (vm->intReflectClass == typeOfArray) {
						primitiveElement = memManagerFuncs->J9AllocateObject(currentThread, J9VMJAVALANGINTEGER_OR_NULL(vm), J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
						if (NULL != primitiveElement) {
							jint val = J9JAVAARRAYOFINT_LOAD(currentThread, j9array, index);
							J9VMJAVALANGINTEGER_SET_VALUE(currentThread, primitiveElement, val);
							elementJNIRef = vmFuncs->j9jni_createLocalRef((JNIEnv *) currentThread, primitiveElement);
						}
					} else if (vm->floatReflectClass == typeOfArray) {
						primitiveElement = memManagerFuncs->J9AllocateObject(currentThread, J9VMJAVALANGFLOAT_OR_NULL(vm), J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
						if (NULL != primitiveElement) {
							U_32 val = J9JAVAARRAYOFFLOAT_LOAD(currentThread, j9array, index);
							J9VMJAVALANGFLOAT_SET_VALUE(currentThread, primitiveElement, val);
							elementJNIRef = vmFuncs->j9jni_createLocalRef((JNIEnv *) currentThread, primitiveElement);
						}
					} else if (vm->doubleReflectClass == typeOfArray) {
						primitiveElement = memManagerFuncs->J9AllocateObject(currentThread, J9VMJAVALANGDOUBLE_OR_NULL(vm), J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
						if (NULL != primitiveElement) {
							U_64 val = J9JAVAARRAYOFDOUBLE_LOAD(currentThread, j9array, index);
							J9VMJAVALANGDOUBLE_SET_VALUE(currentThread, primitiveElement, val);
							elementJNIRef = vmFuncs->j9jni_createLocalRef((JNIEnv *) currentThread, primitiveElement);
						}
					} else {
						vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
						illegalArgSeen = TRUE;
					}
					if (!illegalArgSeen) {
						if (NULL == primitiveElement) {
							vmFuncs->setHeapOutOfMemoryError(currentThread);
						} else if (NULL == elementJNIRef) {
							vmFuncs->setNativeOutOfMemoryError(currentThread, J9NLS_VM_NATIVE_OOM);
						}
					}
				} else {
					j9object_t j9arrayElement = J9JAVAARRAYOFOBJECT_LOAD(currentThread, j9array, index);
					elementJNIRef = vmFuncs->j9jni_createLocalRef((JNIEnv *)currentThread, j9arrayElement);

					if ((NULL == elementJNIRef) && (NULL != j9arrayElement)) {
						vmFuncs->setNativeOutOfMemoryError(currentThread, J9NLS_VM_NATIVE_OOM);
					}
				}
			}
		} else {
			vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);

	return elementJNIRef;
}


/**
 * Get the array length.
 * This function may lock, gc or throw exception.
 * @param array The array
 * @return the array length
 */
jint JNICALL
JVM_GetArrayLength(JNIEnv *env, jobject array)
{
	J9VMThread *currentThread;
	J9JavaVM *vm;
	J9InternalVMFunctions *vmFuncs;
	jsize arrayLength = 0;

	Assert_SC_notNull(env);

	currentThread = (J9VMThread*) env;
	vm = currentThread->javaVM;
	vmFuncs = vm->internalVMFunctions;

	vmFuncs->internalEnterVMFromJNI(currentThread);

	if (NULL == array) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		J9Class *ramClass;
		j9object_t j9array;

		j9array = J9_JNI_UNWRAP_REFERENCE(array);
		ramClass = J9OBJECT_CLAZZ(currentThread, j9array);

		if (J9CLASS_IS_ARRAY(ramClass)) {
			arrayLength = J9INDEXABLEOBJECT_SIZE(currentThread, j9array);
		} else {
			vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
		}
	}

	vmFuncs->internalExitVMToJNI(currentThread);

	return arrayLength;
}

J9Class*
java_lang_Class_vmRef(JNIEnv* env, jobject clazz)
{
	J9VMThread* currentThread = (J9VMThread*) env;
	J9JavaVM*  vm = currentThread->javaVM;
	J9Class* ramClass;

	vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);
	ramClass = J9VMJAVALANGCLASS_VMREF(currentThread, J9_JNI_UNWRAP_REFERENCE(clazz) );
	vm->internalVMFunctions->internalExitVMToJNI(currentThread);

	return ramClass;
}

/**
 * Helper function to convert a J9UTF8* to a null-terminated C string.
 */
static char*
utf8_to_cstring(JNIEnv* env, J9UTF8* utf)
{
	PORT_ACCESS_FROM_ENV(env);
	char* cstring = (char*)j9mem_allocate_memory(J9UTF8_LENGTH(utf) + 1, OMRMEM_CATEGORY_VM);
	if (NULL != cstring) {
		memcpy(cstring, J9UTF8_DATA(utf), J9UTF8_LENGTH(utf));
		cstring[J9UTF8_LENGTH(utf)] = 0;
	}
	return cstring;
}

/**
 * Helper function to convert a J9UTF8* to a java/lang/String.
 */
static jobject
utf8_to_java_lang_String(JNIEnv* env, J9UTF8* utf)
{
	PORT_ACCESS_FROM_ENV(env);
	char* cstring = utf8_to_cstring(env, utf);
	jobject jlString = (*env)->NewStringUTF(env, cstring);
	if (NULL != cstring) {
		j9mem_free_memory(cstring);
	}
	return jlString;
}


jobject JNICALL
JVM_GetClassDeclaredConstructors(JNIEnv* env, jclass clazz, jboolean unknown)
{
	J9Class* ramClass;
	J9ROMClass* romClass;
	jclass jlrConstructor;
	jobjectArray result;
	char* eyecatcher = "<init>";
	U_16 initLength = 6;
	jsize size = 0;
	PORT_ACCESS_FROM_ENV(env);

	ramClass = java_lang_Class_vmRef(env, clazz);
	romClass = ramClass->romClass;

	/* Primitives/Arrays don't have fields. */
	if (J9ROMCLASS_IS_PRIMITIVE_OR_ARRAY(romClass) || J9ROMCLASS_IS_INTERFACE(romClass)) {
		size = 0;
	} else {
		U_32 romMethodCount = romClass->romMethodCount;
		J9Method * method = ramClass->ramMethods;

		while (romMethodCount-- != 0) {
			J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method++);
			J9UTF8 * nameUTF = J9ROMMETHOD_GET_NAME(romClass, romMethod);

			if (J9UTF8_DATA_EQUALS(J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), eyecatcher, 6)) {
				size++;
			}
		}
	}

	/* Look up the field class */
	jlrConstructor  = (*env)->FindClass(env, "java/lang/reflect/Constructor");
	if (NULL == jlrConstructor) {
		return NULL;
	}

	/* Create the result array */
	result = (*env)->NewObjectArray(env, size, jlrConstructor, NULL);
	if (NULL == result) {
		return NULL;
	}

	/* Now walk and fill in the contents */
	if (size != 0) {
		U_32 romMethodCount = romClass->romMethodCount;
		J9Method* method = ramClass->ramMethods;
		jsize index = 0;

		while (romMethodCount-- != 0) {
			J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method++);
			J9UTF8 * nameUTF = J9ROMMETHOD_GET_NAME(romClass, romMethod);

			if (J9UTF8_DATA_EQUALS(J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), eyecatcher, 6)) {
				J9UTF8 * signatureUTF = J9ROMMETHOD_GET_SIGNATURE(romClass, romMethod);
				char* name = utf8_to_cstring(env, nameUTF);
				char* signature = utf8_to_cstring(env, signatureUTF);
				jmethodID methodID = (*env)->GetMethodID(env, clazz, name, signature);
				jobject reflectedMethod;

				assert(methodID != NULL);
				if (NULL != name) {
					j9mem_free_memory(name);
				}
				if (NULL != signature) {
					j9mem_free_memory(signature);
				}

				reflectedMethod = (*env)->ToReflectedMethod(env, clazz, methodID, JNI_FALSE);
				assert(reflectedMethod != NULL);
				(*env)->SetObjectArrayElement(env, result, index++, reflectedMethod);
			}
		}
	}

	return result;
}



jobject JNICALL
JVM_GetClassDeclaredFields(JNIEnv* env, jobject clazz, jint arg2)
{
	J9Class* ramClass;
	J9ROMClass* romClass;
	jclass jlrField;
	jsize size;
	jobjectArray result;
	J9ROMFieldShape *field;
	J9ROMFieldWalkState walkState;
	jsize index = 0;
	PORT_ACCESS_FROM_ENV(env);

	ramClass = java_lang_Class_vmRef(env, clazz);
	romClass = ramClass->romClass;

	/* Primitives/Arrays don't have fields. */
	if (J9ROMCLASS_IS_PRIMITIVE_OR_ARRAY(romClass)) {
		size = 0;
	} else {
		size = romClass->romFieldCount;
	}

	/* Look up the field class */
	jlrField  = (*env)->FindClass(env, "java/lang/reflect/Field");
	if (NULL == jlrField) {
		return NULL;
	}

	/* Create the result array */
	result = (*env)->NewObjectArray(env, size, jlrField, NULL);
	if (NULL == result) {
		return NULL;
	}

	/* Iterate through the fields */
	field = romFieldsStartDo( romClass, &walkState );
	while( field != NULL ) {
		U_32 modifiers = field->modifiers;
		jfieldID fieldID;
		jobject reflectedField;
		jboolean isStatic;
		J9UTF8* nameUTF =  J9ROMFIELDSHAPE_NAME(field);
		J9UTF8* signatureUTF = J9ROMFIELDSHAPE_SIGNATURE(field);
		char* name = utf8_to_cstring(env, nameUTF);
		char* signature = utf8_to_cstring(env, signatureUTF);

		if(modifiers & J9AccStatic) {
			fieldID  = (*env)->GetStaticFieldID(env, clazz, name, signature);
			isStatic = JNI_TRUE;
		} else {
			fieldID = (*env)->GetFieldID(env, clazz, name, signature);
			isStatic = JNI_FALSE;
		}

		if (NULL != name) {
			j9mem_free_memory(name);
		}
		if (NULL != signature) {
			j9mem_free_memory(signature);
		}

		assert(fieldID != NULL);
		reflectedField = (*env)->ToReflectedField(env, clazz, fieldID, isStatic);
		assert(reflectedField != NULL);
		(*env)->SetObjectArrayElement(env, result, index++, reflectedField);
		field = romFieldsNextDo( &walkState );
	}

	return result;
}



jobject JNICALL
JVM_GetClassDeclaredMethods(JNIEnv* env, jobject clazz, jboolean unknown)
{
	J9Class* ramClass;
	J9ROMClass* romClass;
	jclass jlrMethod;
	jobjectArray result;
	char* eyecatcher = "<init>";
	U_16 initLength = 6;
	jsize size = 0;
	PORT_ACCESS_FROM_ENV(env);

	ramClass = java_lang_Class_vmRef(env, clazz);
	romClass = ramClass->romClass;

	/* Primitives/Arrays don't have fields. */
	if (J9ROMCLASS_IS_PRIMITIVE_OR_ARRAY(romClass) || J9ROMCLASS_IS_INTERFACE(romClass)) {
		size = 0;
	} else {
		U_32 romMethodCount = romClass->romMethodCount;
		J9Method * method = ramClass->ramMethods;

		while (romMethodCount-- != 0) {
			J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method++);
			J9UTF8 * nameUTF = J9ROMMETHOD_GET_NAME(romClass, romMethod);

			if (!J9UTF8_DATA_EQUALS(J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), eyecatcher, 6)) {
				size++;
			}
		}
	}

	/* Look up the field class */
	jlrMethod  = (*env)->FindClass(env, "java/lang/reflect/Method");
	if (NULL == jlrMethod) {
		return NULL;
	}

	/* Create the result array */
	result = (*env)->NewObjectArray(env, size, jlrMethod, NULL);
	if (NULL == result) {
		return NULL;
	}

	/* Now walk and fill in the contents */
	if (size != 0) {
		U_32 romMethodCount = romClass->romMethodCount;
		J9Method* method = ramClass->ramMethods;
		jsize index = 0;

		while (romMethodCount-- != 0) {
			J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method++);
			J9UTF8 * nameUTF = J9ROMMETHOD_GET_NAME(romClass, romMethod);

			if (!J9UTF8_DATA_EQUALS(J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), eyecatcher, 6)) {
				J9UTF8 * signatureUTF = J9ROMMETHOD_GET_SIGNATURE(romClass, romMethod);
				char* name = utf8_to_cstring(env, nameUTF);
				char* signature = utf8_to_cstring(env, signatureUTF);
				U_32 modifiers = romMethod->modifiers;
				jmethodID methodID;
				jobject reflectedMethod;
				jboolean isStatic;

				if(modifiers & J9AccStatic) {
					methodID = (*env)->GetStaticMethodID(env, clazz, name, signature);
					isStatic = JNI_TRUE;
				} else {
					methodID = (*env)->GetMethodID(env, clazz, name, signature);
					isStatic = JNI_FALSE;
				}


				assert(methodID != NULL);
				if (NULL != name) {
					j9mem_free_memory(name);
				}
				if (NULL != signature) {
					j9mem_free_memory(signature);
				}

				reflectedMethod = (*env)->ToReflectedMethod(env, clazz, methodID, isStatic);
				assert(reflectedMethod != NULL);
				(*env)->SetObjectArrayElement(env, result, index++, reflectedMethod);
			}
		}
	}

	return result;
}



jobject JNICALL
JVM_GetClassInterfaces(jint arg0, jint arg1)
{
	assert(!"JVM_GetClassInterfaces() stubbed!");
	return NULL;
}



jint JNICALL
JVM_GetClassModifiers(JNIEnv* env, jclass clazz)
{
	J9Class* ramClass = java_lang_Class_vmRef(env, clazz);
	J9ROMClass* romClass = ramClass->romClass;

	if (J9ROMCLASS_IS_ARRAY(romClass)) {
		J9ArrayClass* arrayClass = (J9ArrayClass*)ramClass;
		jint result = 0;
		J9ROMClass *leafRomClass = arrayClass->leafComponentType->romClass;
		if (J9_ARE_ALL_BITS_SET(leafRomClass->extraModifiers, J9AccClassInnerClass)) {
			result = leafRomClass->memberAccessFlags;
		} else {
			result = leafRomClass->modifiers;
		}

		result |= (J9AccAbstract | J9AccFinal);
		return result;
	} else {
		if (J9_ARE_ALL_BITS_SET(romClass->extraModifiers, J9AccClassInnerClass)) {
			return romClass->memberAccessFlags;
		} else {
			return romClass->modifiers;
		}
	}
}



jobject JNICALL
JVM_GetClassSigners(jint arg0, jint arg1)
{
	assert(!"JVM_GetClassSigners() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetComponentType(JNIEnv* env, jclass clazz)
{
	J9Class* ramClass = java_lang_Class_vmRef(env, clazz);
	J9ROMClass* romClass = ramClass->romClass;

	if (J9ROMCLASS_IS_ARRAY(romClass)) {
		J9ArrayClass* arrayClass = (J9ArrayClass*)ramClass;
		return (jobject)&(arrayClass->leafComponentType->classObject);
	}
	return NULL;
}



jobject JNICALL
JVM_GetDeclaredClasses(jint arg0, jint arg1)
{
	assert(!"JVM_GetDeclaredClasses() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetDeclaringClass(jint arg0, jint arg1)
{
	assert(!"JVM_GetDeclaringClass() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetInheritedAccessControlContext(jint arg0, jint arg1)
{
	assert(!"JVM_GetInheritedAccessControlContext() stubbed!");
	return NULL;
}



/**
 * Get the primitive array element at index.
 * This function may lock, gc or throw exception.
 * @param array The array
 * @param index The index
 * @param wCode the native symbol code
 * @return a union of primitive value
 */
jvalue JNICALL
JVM_GetPrimitiveArrayElement(JNIEnv *env, jobject array, jint index, jint wCode)
{
	J9VMThread *currentThread;
	J9JavaVM *vm;
	J9InternalVMFunctions *vmFuncs;
	jvalue value;

	Assert_SC_notNull(env);

	currentThread = (J9VMThread *) env;
	vm = currentThread->javaVM;
	vmFuncs = vm->internalVMFunctions;
	value.j = 0;

	vmFuncs->internalEnterVMFromJNI(currentThread);

	if (NULL == array) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		j9object_t j9array = J9_JNI_UNWRAP_REFERENCE(array);
		J9ArrayClass *arrayClass = (J9ArrayClass *) J9OBJECT_CLAZZ(currentThread, j9array);
		J9Class *typeOfArray = (J9Class *) arrayClass->componentType;

		if (J9CLASS_IS_ARRAY(arrayClass) && J9ROMCLASS_IS_PRIMITIVE_TYPE(typeOfArray->romClass)) {
			if ((index < 0) || (((jint) J9INDEXABLEOBJECT_SIZE(currentThread, j9array)) <= index)) {
				vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGARRAYINDEXOUTOFBOUNDSEXCEPTION, NULL);
			} else {
				BOOLEAN invalidArgument = FALSE;

				if (vm->booleanReflectClass == typeOfArray) {
					if (POK_BOOLEAN == wCode) {
						value.z = J9JAVAARRAYOFBOOLEAN_LOAD(currentThread, j9array, index);
					} else {
						invalidArgument = TRUE;
					}
				} else if (vm->charReflectClass == typeOfArray) {
					switch (wCode) {
					case POK_CHAR:
						value.c = J9JAVAARRAYOFCHAR_LOAD(currentThread, j9array, index);
						break;
					case POK_FLOAT:
						value.f = (jfloat) J9JAVAARRAYOFCHAR_LOAD(currentThread, j9array, index);
						break;
					case POK_DOUBLE:
						value.d = (jdouble) J9JAVAARRAYOFCHAR_LOAD(currentThread, j9array, index);
						break;
					case POK_INT:
						value.i = J9JAVAARRAYOFCHAR_LOAD(currentThread, j9array, index);
						break;
					case POK_LONG:
						value.j = J9JAVAARRAYOFCHAR_LOAD(currentThread, j9array, index);
						break;
					default:
						invalidArgument = TRUE;
						break;
					}
				} else if (vm->floatReflectClass == typeOfArray) {
					jfloat val = 0;
					switch (wCode) {
					case POK_FLOAT:
						*(U_32 *) &value.f = J9JAVAARRAYOFFLOAT_LOAD(currentThread, j9array, index);
						break;
					case POK_DOUBLE:
						*(U_32 *) &val = J9JAVAARRAYOFFLOAT_LOAD(currentThread, j9array, index);
						value.d = (jdouble) val;
						break;
					default:
						invalidArgument = TRUE;
						break;
					}
				} else if (vm->doubleReflectClass == typeOfArray) {
					if (POK_DOUBLE == wCode) {
						*(U_64 *) &value.d = J9JAVAARRAYOFDOUBLE_LOAD(currentThread, j9array, index);
					} else {
						invalidArgument = TRUE;
					}
				} else if (vm->byteReflectClass == typeOfArray) {
					switch (wCode) {
					case POK_FLOAT:
						value.f = (jfloat) J9JAVAARRAYOFBYTE_LOAD(currentThread, j9array, index);
						break;
					case POK_DOUBLE:
						value.d = (jdouble) J9JAVAARRAYOFBYTE_LOAD(currentThread, j9array, index);
						break;
					case POK_BYTE:
						value.b = J9JAVAARRAYOFBYTE_LOAD(currentThread, j9array, index);
						break;
					case POK_SHORT:
						value.s = J9JAVAARRAYOFBYTE_LOAD(currentThread, j9array, index);
						break;
					case POK_INT:
						value.i = J9JAVAARRAYOFBYTE_LOAD(currentThread, j9array, index);
						break;
					case POK_LONG:
						value.j = J9JAVAARRAYOFBYTE_LOAD(currentThread, j9array, index);
						break;
					default:
						invalidArgument = TRUE;
						break;
					}
				} else if (vm->shortReflectClass == typeOfArray) {
					switch (wCode) {
					case POK_FLOAT:
						value.f = (jfloat) J9JAVAARRAYOFSHORT_LOAD(currentThread, j9array, index);
						break;
					case POK_DOUBLE:
						value.d = (jdouble) J9JAVAARRAYOFSHORT_LOAD(currentThread, j9array, index);
						break;
					case POK_SHORT:
						value.s = J9JAVAARRAYOFSHORT_LOAD(currentThread, j9array, index);
						break;
					case POK_INT:
						value.i = J9JAVAARRAYOFSHORT_LOAD(currentThread, j9array, index);
						break;
					case POK_LONG:
						value.j = J9JAVAARRAYOFSHORT_LOAD(currentThread, j9array, index);
						break;
					default:
						invalidArgument = TRUE;
						break;
					}
				} else if (vm->intReflectClass == typeOfArray) {
					switch (wCode) {
					case POK_FLOAT:
						value.f = (jfloat) J9JAVAARRAYOFINT_LOAD(currentThread, j9array, index);
						break;
					case POK_DOUBLE:
						value.d = (jdouble) J9JAVAARRAYOFINT_LOAD(currentThread, j9array, index);
						break;
					case POK_INT:
						value.i = J9JAVAARRAYOFINT_LOAD(currentThread, j9array, index);
						break;
					case POK_LONG:
						value.j = J9JAVAARRAYOFINT_LOAD(currentThread, j9array, index);
						break;
					default:
						invalidArgument = TRUE;
						break;
					}
				} else if (vm->longReflectClass == typeOfArray) {
					switch (wCode) {
					case POK_FLOAT:
						value.f = (jfloat) J9JAVAARRAYOFLONG_LOAD(currentThread, j9array, index);
						break;
					case POK_DOUBLE:
						value.d = (jdouble) J9JAVAARRAYOFLONG_LOAD(currentThread, j9array, index);
						break;
					case POK_LONG:
						value.j = J9JAVAARRAYOFLONG_LOAD(currentThread, j9array, index);
						break;
					default:
						invalidArgument = TRUE;
						break;
					}
				} else {
					invalidArgument = TRUE;
				}
				if (invalidArgument) {
					vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
				}
			}
		} else {
			vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);

	return value;
}



jobject JNICALL
JVM_GetProtectionDomain(jint arg0, jint arg1)
{
	assert(!"JVM_GetProtectionDomain() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_GetStackAccessControlContext(JNIEnv* env, jclass java_security_AccessController)
{
	return NULL;
}

jint JNICALL
JVM_GetStackTraceDepth(JNIEnv* env, jobject throwable)
{
	J9VMThread* currentThread = (J9VMThread*) env;
	J9JavaVM * vm = currentThread->javaVM;
	J9InternalVMFunctions * vmfns = vm->internalVMFunctions;
	jint numberOfFrames;
	UDATA pruneConstructors = 0;

	vmfns->internalEnterVMFromJNI(currentThread);
	numberOfFrames = (jint)vmfns->iterateStackTrace(currentThread, (j9object_t*)throwable, NULL, NULL, pruneConstructors);
	vmfns->internalExitVMToJNI(currentThread);

	return numberOfFrames;
}


static jclass
java_lang_StackTraceElement(JNIEnv* env)
{
	static jclass cached = NULL;
	jclass localRef = NULL;

	if (NULL != cached) {
		return cached;
	}

	localRef = (*env)->FindClass(env, "java/lang/StackTraceElement");
	assert(localRef != NULL);

	cached = (*env)->NewGlobalRef(env, localRef);
	if (cached == NULL) {
		return NULL;
	}

	(*env)->DeleteLocalRef(env,localRef);
	assert(localRef != NULL);
	return cached;
}

static jmethodID
java_lang_StackTraceElement_init(JNIEnv* env)
{
	static jmethodID cached = NULL;
	if (NULL != cached) {
		return cached;
	}

	cached = (*env)->GetMethodID(env, java_lang_StackTraceElement(env), "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I)V" );
	assert(cached != NULL);
	return cached;
}


typedef struct GetStackTraceElementUserData {
	J9ROMClass * romClass;
	J9ROMMethod * romMethod;
	J9UTF8 * fileName;
	UDATA lineNumber;
	J9ClassLoader* classLoader;
	UDATA seekFrameIndex;
	UDATA currentFrameIndex;
	BOOLEAN found;
} GetStackTraceElementUserData;

/* Return TRUE to keep iterating, FALSE to halt the walk. */
static UDATA
getStackTraceElementIterator(J9VMThread * vmThread, void * voidUserData, J9ROMClass * romClass, J9ROMMethod * romMethod, J9UTF8 * fileName, UDATA lineNumber, J9ClassLoader* classLoader)
{
	GetStackTraceElementUserData * userData = voidUserData;

	if (userData->seekFrameIndex == userData->currentFrameIndex) {
		/* We are done, remember the current state and return */
		userData->romClass = romClass;
		userData->romMethod = romMethod;
		userData->fileName = fileName;
		userData->lineNumber = lineNumber;
		userData->classLoader = classLoader;
		userData->found = TRUE;
		return FALSE;
	}

	userData->currentFrameIndex++;
	return TRUE;
}

jobject JNICALL
JVM_GetStackTraceElement(JNIEnv* env, jobject throwable, jint index)
{
	J9VMThread* currentThread = (J9VMThread*) env;
	J9JavaVM * vm = currentThread->javaVM;
	J9InternalVMFunctions * vmfns = vm->internalVMFunctions;
	jobject stackTraceElement;
	UDATA pruneConstructors = 0;
	GetStackTraceElementUserData userData;
	jobject declaringClass = NULL;
	jobject methodName = NULL;
	jobject fileName = NULL;
	jint lineNumber = -1;

	memset(&userData,0, sizeof(userData));
	userData.seekFrameIndex = index;

	vmfns->internalEnterVMFromJNI(currentThread);
	vmfns->iterateStackTrace(currentThread, (j9object_t*)throwable, getStackTraceElementIterator, &userData, pruneConstructors);
	vmfns->internalExitVMToJNI(currentThread);

	/* Bail if we couldn't find the frame */
	if (TRUE != userData.found) {
		return NULL;
	}

	/* Convert to Java format */
	declaringClass = utf8_to_java_lang_String(env, J9ROMCLASS_CLASSNAME(userData.romClass));
	methodName = utf8_to_java_lang_String(env, J9ROMMETHOD_GET_NAME(userData.romClass, userData.romMethod));
	fileName = utf8_to_java_lang_String(env, userData.fileName);
	lineNumber = (jint)userData.lineNumber;

	stackTraceElement = (*env)->NewObject(
			env,
			java_lang_StackTraceElement(env),
			java_lang_StackTraceElement_init(env),
			declaringClass,
			methodName,
			fileName,
			lineNumber);

	assert(NULL != stackTraceElement);
	return stackTraceElement;
}



jobject JNICALL
JVM_HoldsLock(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_HoldsLock() stubbed!");
	return NULL;
}



/**
 * Get hashCode of the object. This function may lock, gc or throw exception.
 * @return hashCode, if obj is not NULL.
 * @return 0, if obj is NULL.
 */
jint JNICALL
JVM_IHashCode(JNIEnv *env, jobject obj)
{
	jint result = 0;

	if (obj != NULL) {
		J9VMThread * currentThread = (J9VMThread *) env;
		J9JavaVM * vm = currentThread->javaVM;
		J9InternalVMFunctions * vmFuncs = vm->internalVMFunctions;

		vmFuncs->internalEnterVMFromJNI(currentThread);
		result = vm->memoryManagerFunctions->j9gc_objaccess_getObjectHashCode(vm, J9_JNI_UNWRAP_REFERENCE(obj));
		vmFuncs->internalExitVMToJNI(currentThread);
	}

	return result;
}



jobject JNICALL
JVM_InitProperties(JNIEnv* env, jobject properties)
{
	/* This JVM method is invoked by JCL native Java_java_lang_System_initProperties
	 * only for initialization of platform encoding.
	 * This is only required by Java 11 raw builds.
	 * This method is not invoked by other Java levels.
	 */
#if JAVA_SPEC_VERSION < 11
	assert(!"JVM_InitProperties should not be called!");
#endif /* JAVA_SPEC_VERSION < 11 */
	return properties;
}



/**
 * Returns a canonical representation for the string object. If the string is already in the pool, just return
 * the string. If not, add the string to the pool and return the string.
 * This function may lock, gc or throw exception.
 * @param str The string object.
 * @return NULL if str is NULL
 * @return The interned string
 */
jstring JNICALL
JVM_InternString(JNIEnv *env, jstring str)
{
	if (str != NULL) {
		J9VMThread* currentThread = (J9VMThread*) env;
		J9JavaVM* javaVM = currentThread->javaVM;
		J9InternalVMFunctions* vmfns = javaVM->internalVMFunctions;
		j9object_t stringObject;

		vmfns->internalEnterVMFromJNI(currentThread);
		stringObject = J9_JNI_UNWRAP_REFERENCE(str);
		stringObject = javaVM->memoryManagerFunctions->j9gc_internString(currentThread, stringObject);
		str = vmfns->j9jni_createLocalRef(env, stringObject);
		vmfns->internalExitVMToJNI(currentThread);
	}

	return str;
}



jobject JNICALL
JVM_Interrupt(jint arg0, jint arg1)
{
	assert(!"JVM_Interrupt() stubbed!");
	return NULL;
}



jboolean JNICALL
JVM_IsArrayClass(JNIEnv* env, jclass clazz)
{
	J9Class * ramClass = java_lang_Class_vmRef(env, clazz);
	if (J9ROMCLASS_IS_ARRAY(ramClass->romClass)) {
		return JNI_TRUE;
	} else {
		return JNI_FALSE;
	}
}



jboolean JNICALL
JVM_IsInterface(JNIEnv* env, jclass clazz)
{
	J9Class * ramClass = java_lang_Class_vmRef(env, clazz);
	if (J9ROMCLASS_IS_INTERFACE(ramClass->romClass)) {
		return JNI_TRUE;
	} else {
		return JNI_FALSE;
	}
}



jboolean JNICALL
JVM_IsInterrupted(JNIEnv* env, jobject thread, jboolean unknown)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9VMThread *targetThread;
	J9JavaVM* vm = currentThread->javaVM;
	J9ThreadEnv* threadEnv = getJ9ThreadEnv(env);
	UDATA rcClear;

	vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);
	targetThread = J9VMJAVALANGTHREAD_THREADREF(currentThread, J9_JNI_UNWRAP_REFERENCE(thread) );
	vm->internalVMFunctions->internalExitVMToJNI(currentThread);

	assert(targetThread == currentThread);

	if (NULL != vm->sidecarClearInterruptFunction) {
		vm->sidecarClearInterruptFunction(currentThread);
	}

	rcClear = threadEnv->clear_interrupted();
	if (0 != rcClear) {
		return JNI_TRUE;
	} else {
		return JNI_FALSE;
	}
}



jboolean JNICALL
JVM_IsPrimitiveClass(JNIEnv* env, jclass clazz)
{
	J9Class * ramClass = java_lang_Class_vmRef(env, clazz);
	if (J9ROMCLASS_IS_PRIMITIVE_TYPE(ramClass->romClass)) {
		return JNI_TRUE;
	} else {
		return JNI_FALSE;
	}
}


/**
 * Check whether the jni version is supported.
 * This function may not lock, gc or throw exception.
 * @param version
 * @return true if version is JNI_VERSION_1_1, JNI_VERSION_1_2, JNI_VERSION_1_4, JNI_VERSION_1_6, or
 * 		   JNI_VERSION_1_8, JNI_VERSION_9, JNI_VERSION_10; false if not.
 * @careful
 */
jboolean JNICALL
JVM_IsSupportedJNIVersion(jint version)
{
	switch(version) {
	case JNI_VERSION_1_1:
	case JNI_VERSION_1_2:
	case JNI_VERSION_1_4:
	case JNI_VERSION_1_6:
	case JNI_VERSION_1_8:
#if JAVA_SPEC_VERSION >= 9
	case JNI_VERSION_9:
#endif /* JAVA_SPEC_VERSION >= 9 */
#if JAVA_SPEC_VERSION >= 10
	case JNI_VERSION_10:
#endif /* JAVA_SPEC_VERSION >= 10 */
		return JNI_TRUE;

	default:
		return JNI_FALSE;
	}
}



jboolean JNICALL
JVM_IsThreadAlive(JNIEnv* jniEnv, jobject targetThread)
{
	J9VMThread* currentThread = (J9VMThread*) jniEnv;
	J9JavaVM*  vm = currentThread->javaVM;
	J9VMThread* vmThread;

	vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);
	vmThread = J9VMJAVALANGTHREAD_THREADREF(currentThread, J9_JNI_UNWRAP_REFERENCE(targetThread) );
	vm->internalVMFunctions->internalExitVMToJNI(currentThread);

	/* Assume that a non-null threadRef indicates the thread is alive */
	return (NULL == vmThread) ? JNI_FALSE : JNI_TRUE;
}



jobject JNICALL
JVM_NewArray(JNIEnv* jniEnv, jclass componentType, jint dimension)
{
	J9VMThread* currentThread = (J9VMThread*) jniEnv;
	J9JavaVM*  vm = currentThread->javaVM;
	J9MemoryManagerFunctions * mmfns = vm->memoryManagerFunctions;
	J9Class* ramClass = java_lang_Class_vmRef(jniEnv, componentType);
	J9ROMClass* romClass = ramClass->romClass;
	j9object_t newArray;
	jobject arrayRef;

	vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);
	if (ramClass->arrayClass == NULL) {
		vm->internalVMFunctions->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
		return NULL;
	}

	newArray = vm->memoryManagerFunctions->J9AllocateIndexableObject(
			currentThread, ramClass->arrayClass, dimension, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);

	if (NULL == newArray) {
		vm->internalVMFunctions->setHeapOutOfMemoryError(currentThread);
		return NULL;
	}

	arrayRef = vm->internalVMFunctions->j9jni_createLocalRef(jniEnv, newArray);
	vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	return arrayRef;
}


static J9Class *
fetchArrayClass(struct J9VMThread *vmThread, J9Class *elementTypeClass)
{
	/* Quick check before grabbing the mutex */
	J9Class *resultClass = elementTypeClass->arrayClass;
	if (NULL == resultClass) {
		/* Allocate an array class */
		J9ROMArrayClass* arrayOfObjectsROMClass = (J9ROMArrayClass*)J9ROMIMAGEHEADER_FIRSTCLASS(vmThread->javaVM->arrayROMClasses);

		resultClass = vmThread->javaVM->internalVMFunctions->internalCreateArrayClass(vmThread, arrayOfObjectsROMClass, elementTypeClass);
	}
	return resultClass;
}


/**
 * Allocate a multi-dimension array with class specified
 * This function may lock, gc or throw exception.
 * @param eltClass The class of the element
 * @param dim The dimension
 * @return The newly allocated array
 */
jobject JNICALL
JVM_NewMultiArray(JNIEnv *env, jclass eltClass, jintArray dim)
{
	/* Maximum array dimensions, according to the spec for the array bytecodes, is 255 */
#define MAX_DIMENSIONS 255
	J9VMThread *currentThread = (J9VMThread *) env;
	J9InternalVMFunctions * vmFuncs = currentThread->javaVM->internalVMFunctions;
	jobject result = NULL;

	vmFuncs->internalEnterVMFromJNI(currentThread);

	if (NULL == dim) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		j9object_t dimensionsArrayObject = J9_JNI_UNWRAP_REFERENCE(dim);
		UDATA dimensions = J9INDEXABLEOBJECT_SIZE(currentThread, dimensionsArrayObject);

		dimensionsArrayObject = NULL; /* must be refetched after GC points below */
		if (dimensions > MAX_DIMENSIONS) {
			/* the spec says to throw this exception if the number of dimensions in greater than the count we support (and a NULL message appears to be the behaviour of the reference implementation) */
			vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
		} else {
			j9object_t componentTypeClassObject = J9_JNI_UNWRAP_REFERENCE(eltClass);
	
			if (NULL != componentTypeClassObject) {
				J9Class *componentTypeClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, componentTypeClassObject);
				
				/* create an array class with one greater arity than desired */
				UDATA count = dimensions + 1;
				J9Class *componentArrayClass = componentTypeClass;
				BOOLEAN exceptionIsPending = FALSE;
	
				while ((count > 0) && (!exceptionIsPending)) {
					componentArrayClass = fetchArrayClass(currentThread, componentArrayClass);
					exceptionIsPending = (NULL != currentThread->currentException);
					count -= 1;
				}
				
				if (!exceptionIsPending) {
					/* make a copy of the dimensions array in non-object memory */
					I_32 onStackDimensions[MAX_DIMENSIONS];
					j9object_t directObject = NULL;
					UDATA i = 0;
					
					memset(onStackDimensions, 0, sizeof(onStackDimensions));
					dimensionsArrayObject = J9_JNI_UNWRAP_REFERENCE(dim);
					for (i = 0; i < dimensions; i++) {
						onStackDimensions[i] = J9JAVAARRAYOFINT_LOAD(currentThread, dimensionsArrayObject, i);
					}
	
					directObject = vmFuncs->helperMultiANewArray(currentThread, (J9ArrayClass *)componentArrayClass, (UDATA)dimensions, onStackDimensions, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
					if (NULL != directObject) {
						result = vmFuncs->j9jni_createLocalRef(env, directObject);
					}
				}
			}
		}
	}

	vmFuncs->internalExitVMToJNI(currentThread);
	return result;
}




jobject JNICALL
JVM_ResolveClass(jint arg0, jint arg1)
{
	assert(!"JVM_ResolveClass() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_ResumeThread(jint arg0, jint arg1)
{
	assert(!"JVM_ResumeThread() stubbed!");
	return NULL;
}



/**
 * Set the val to the array at the index.
 * This function may lock, gc or throw exception.
 * @param array The array
 * @param index The index
 * @param value The set value
 */
void JNICALL
JVM_SetArrayElement(JNIEnv *env, jobject array, jint index, jobject value)
{
	J9VMThread *currentThread;
	J9JavaVM *vm;
	J9InternalVMFunctions *vmFuncs;

	Assert_SC_notNull(env);

	currentThread = (J9VMThread *) env;
	vm = currentThread->javaVM;
	vmFuncs = vm->internalVMFunctions;

	vmFuncs->internalEnterVMFromJNI(currentThread);

	if (NULL == array) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		j9object_t j9array = J9_JNI_UNWRAP_REFERENCE(array);
		J9ArrayClass *arrayClass = (J9ArrayClass *) J9OBJECT_CLAZZ(currentThread, j9array);
		J9Class *typeOfArray = arrayClass->componentType;

		if (J9CLASS_IS_ARRAY(arrayClass)) {
			if ((index < 0) || (((jint) J9INDEXABLEOBJECT_SIZE(currentThread, j9array)) <= index)) {
				vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGARRAYINDEXOUTOFBOUNDSEXCEPTION, NULL);
			} else {
				if (J9ROMCLASS_IS_PRIMITIVE_TYPE(typeOfArray->romClass)) {
					if (NULL == value) {
						vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
					} else {
						J9Class *booleanWrapperClass = J9VMJAVALANGBOOLEAN_OR_NULL(vm);
						J9Class *byteWrapperClass = J9VMJAVALANGBYTE_OR_NULL(vm);
						J9Class *shortWrapperClass = J9VMJAVALANGSHORT_OR_NULL(vm);
						J9Class *charWrapperClass = J9VMJAVALANGCHARACTER_OR_NULL(vm);
						J9Class *intWrapperClass = J9VMJAVALANGINTEGER_OR_NULL(vm);
						J9Class *floatWrapperClass = J9VMJAVALANGFLOAT_OR_NULL(vm);
						J9Class *doubleWrapperClass = J9VMJAVALANGDOUBLE_OR_NULL(vm);
						J9Class *longWrapperClass = J9VMJAVALANGLONG_OR_NULL(vm);

						BOOLEAN invalidArgument = FALSE;
						j9object_t j9value = J9_JNI_UNWRAP_REFERENCE(value);
						J9Class *valueClass = J9OBJECT_CLAZZ(currentThread, j9value);

						if (vm->longReflectClass == typeOfArray) {
							jlong val = 0;
							if (longWrapperClass == valueClass) {
								val = J9VMJAVALANGLONG_VALUE(currentThread, j9value);
							} else if (intWrapperClass == valueClass) {
								val = (jlong) J9VMJAVALANGINTEGER_VALUE(currentThread, j9value);
							} else if (shortWrapperClass == valueClass) {
								val = (jlong) J9VMJAVALANGSHORT_VALUE(currentThread, j9value);
							} else if (charWrapperClass == valueClass) {
								val = (jlong) J9VMJAVALANGCHARACTER_VALUE(currentThread, j9value);
							} else if (byteWrapperClass == valueClass) {
								val = (jlong) J9VMJAVALANGBYTE_VALUE(currentThread, j9value);
							} else {
								invalidArgument = TRUE;
							}
							if (!invalidArgument) {
								J9JAVAARRAYOFLONG_STORE(currentThread, j9array, index, val);
							}
						} else if (vm->booleanReflectClass == typeOfArray) {
							if (booleanWrapperClass == valueClass) {
								J9JAVAARRAYOFBOOLEAN_STORE(currentThread, j9array, index, J9VMJAVALANGBOOLEAN_VALUE(currentThread, j9value));
							} else {
								invalidArgument = TRUE;
							}
						} else if (vm->byteReflectClass == typeOfArray) {
							if (byteWrapperClass == valueClass) {
								J9JAVAARRAYOFBYTE_STORE(currentThread, j9array, index, J9VMJAVALANGBYTE_VALUE(currentThread, j9value));
							} else {
								invalidArgument = TRUE;
							}
						} else if (vm->charReflectClass == typeOfArray) {
							if (charWrapperClass == valueClass) {
								J9JAVAARRAYOFCHAR_STORE(currentThread, j9array, index, J9VMJAVALANGCHARACTER_VALUE(currentThread, j9value));
							} else {
								invalidArgument = TRUE;
							}
						} else if (vm->shortReflectClass == typeOfArray) {
							jshort val = 0;
							if (shortWrapperClass == valueClass) {
								val = J9VMJAVALANGSHORT_VALUE(currentThread, j9value);
							} else if (byteWrapperClass == valueClass) {
								val = (jshort) J9VMJAVALANGBYTE_VALUE(currentThread, j9value);
							} else {
								invalidArgument = TRUE;
							}
							if (!invalidArgument) {
								J9JAVAARRAYOFSHORT_STORE(currentThread, j9array, index, val);
							}
						} else if (vm->intReflectClass == typeOfArray) {
							jint val = 0;
							if (intWrapperClass == valueClass) {
								val = J9VMJAVALANGINTEGER_VALUE(currentThread, j9value);
							} else if (shortWrapperClass == valueClass) {
								val = (jint) J9VMJAVALANGSHORT_VALUE(currentThread, j9value);
							} else if (charWrapperClass == valueClass) {
								val = (jint) J9VMJAVALANGCHARACTER_VALUE(currentThread, j9value);
							} else if (byteWrapperClass == valueClass) {
								val = (jint) J9VMJAVALANGBYTE_VALUE(currentThread, j9value);
							} else {
								 invalidArgument = TRUE;
							}
							if (!invalidArgument) {
								J9JAVAARRAYOFINT_STORE(currentThread, j9array, index, val);
							}
						} else if (vm->floatReflectClass == typeOfArray) {
							jfloat val = 0;
							if (floatWrapperClass == valueClass) {
								*(U_32 *) &val = J9VMJAVALANGFLOAT_VALUE(currentThread, j9value);
							} else if (longWrapperClass == valueClass) {
								val = (jfloat) J9VMJAVALANGLONG_VALUE(currentThread, j9value);
							} else if (intWrapperClass == valueClass) {
								val = (jfloat) ((I_32) J9VMJAVALANGINTEGER_VALUE(currentThread, j9value));
							} else if (shortWrapperClass == valueClass) {
								val = (jfloat) ((I_32) J9VMJAVALANGSHORT_VALUE(currentThread, j9value));
							} else if (charWrapperClass == valueClass) {
								val = (jfloat) J9VMJAVALANGCHARACTER_VALUE(currentThread, j9value);
							} else if (byteWrapperClass == valueClass) {
								val = (jfloat) ((I_32) J9VMJAVALANGBYTE_VALUE(currentThread, j9value));
							} else {
								invalidArgument = TRUE;
							}
							if (!invalidArgument) {
								J9JAVAARRAYOFFLOAT_STORE(currentThread, j9array, index, *(U_32 *) &val);
							}
						} else if (vm->doubleReflectClass == typeOfArray) {
							jdouble val = 0;
							if (doubleWrapperClass == valueClass){
								*(U_64 *) &val = J9VMJAVALANGDOUBLE_VALUE(currentThread, j9value);
							} else if (floatWrapperClass == valueClass) {
								jfloat floatNumber;
								*(U_32 *) &floatNumber = J9VMJAVALANGFLOAT_VALUE(currentThread, j9value);
								val = (jdouble) floatNumber;
							} else if (longWrapperClass == valueClass) {
								val = (jdouble) J9VMJAVALANGLONG_VALUE(currentThread, j9value);
							} else if (intWrapperClass == valueClass) {
								val = (jdouble) ((I_32) J9VMJAVALANGINTEGER_VALUE(currentThread, j9value));
							} else if (shortWrapperClass == valueClass) {
								val = (jdouble) ((I_32) J9VMJAVALANGSHORT_VALUE(currentThread, j9value));
							} else if (charWrapperClass == valueClass) {
								val = (jdouble) J9VMJAVALANGCHARACTER_VALUE(currentThread, j9value);
							} else if (byteWrapperClass == valueClass) {
								val = (jdouble) ((I_32) J9VMJAVALANGBYTE_VALUE(currentThread, j9value));
							} else {
								invalidArgument = TRUE;
							}
							if (!invalidArgument) {
								J9JAVAARRAYOFDOUBLE_STORE(currentThread, j9array, index, *(U_64 *) &val);
							}
						} else {
							invalidArgument = TRUE;
						}
						if (invalidArgument) {
							vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
						}
					}
				} else {
					if (NULL == value) {
						J9JAVAARRAYOFOBJECT_STORE(currentThread, j9array, index, value);
					} else {
						j9object_t j9value = J9_JNI_UNWRAP_REFERENCE(value);
						J9Class *valueClass = J9OBJECT_CLAZZ(currentThread, j9value);

						if (isSameOrSuperClassOf ((J9Class*) arrayClass->componentType, valueClass)) {
							J9JAVAARRAYOFOBJECT_STORE(currentThread, j9array, index, j9value);
						} else {
							vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
						}
					}
				}
			}
		} else {
			vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);

	return;
}



jobject JNICALL
JVM_SetClassSigners(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_SetClassSigners() stubbed!");
	return NULL;
}



/**
 * Set the value to the primitive array at the index.
 * This function may lock, gc or throw exception.
 * @param array The array
 * @param index The index
 * @param value The set value
 * @param vCode The primitive symbol type code
 */
void JNICALL
JVM_SetPrimitiveArrayElement(JNIEnv *env, jobject array, jint index, jvalue value, unsigned char vCode)
{
	J9VMThread *currentThread;
	J9JavaVM *vm;
	J9InternalVMFunctions *vmFuncs;

	Assert_SC_notNull(env);

	currentThread = (J9VMThread *) env;
	vm = currentThread->javaVM;
	vmFuncs = vm->internalVMFunctions;

	vmFuncs->internalEnterVMFromJNI(currentThread);

	if (NULL == array) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		j9object_t j9array = J9_JNI_UNWRAP_REFERENCE(array);
		J9ArrayClass *arrayClass = (J9ArrayClass *)J9OBJECT_CLAZZ(currentThread, j9array);
		J9Class *typeOfArray = arrayClass->componentType;

		if (J9CLASS_IS_ARRAY(arrayClass) && J9ROMCLASS_IS_PRIMITIVE_TYPE(typeOfArray->romClass)) {
			if ((index < 0) || (((jint) J9INDEXABLEOBJECT_SIZE(currentThread, j9array)) <= index)) {
				vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGARRAYINDEXOUTOFBOUNDSEXCEPTION, NULL);
			} else {
				BOOLEAN invalidArgument = FALSE;

				if (vm->intReflectClass == typeOfArray) {
					switch (vCode) {
					case POK_CHAR:
						J9JAVAARRAYOFINT_STORE(currentThread, j9array, index, value.c);
						break;
					case POK_BYTE:
						J9JAVAARRAYOFINT_STORE(currentThread, j9array, index, value.b);
						break;
					case POK_SHORT:
						J9JAVAARRAYOFINT_STORE(currentThread, j9array, index, value.s);
						break;
					case POK_INT:
						J9JAVAARRAYOFINT_STORE(currentThread, j9array, index, value.i);
						break;
					default:
						invalidArgument = TRUE;
						break;
					}
				} else if (vm->longReflectClass == typeOfArray) {
					switch (vCode) {
					case POK_CHAR:
						J9JAVAARRAYOFLONG_STORE(currentThread, j9array, index, value.c);
						break;
					case POK_BYTE:
						J9JAVAARRAYOFLONG_STORE(currentThread, j9array, index, value.b);
						break;
					case POK_SHORT:
						J9JAVAARRAYOFLONG_STORE(currentThread, j9array, index, value.s);
						break;
					case POK_INT:
						J9JAVAARRAYOFLONG_STORE(currentThread, j9array, index, value.i);
						break;
					case POK_LONG:
						J9JAVAARRAYOFLONG_STORE(currentThread, j9array, index, value.j);
						break;
					default:
						invalidArgument = TRUE;
						break;
					}
				} else if (vm->byteReflectClass == typeOfArray) {
					if (POK_BYTE == vCode) {
						J9JAVAARRAYOFBYTE_STORE(currentThread, j9array, index, value.b);
					} else {
						invalidArgument = TRUE;
					}
				} else if (vm->doubleReflectClass == typeOfArray) {
					jdouble val = 0;
					switch(vCode) {
					case POK_CHAR:
						val = (jdouble) value.c;
						break;
					case POK_FLOAT:
						val = (jdouble) value.f;
						break;
					case POK_DOUBLE:
						val = value.d;
						break;
					case POK_BYTE:
						val = (jdouble) value.b;
						break;
					case POK_SHORT:
						val = (jdouble) value.s;
						break;
					case POK_INT:
						val = (jdouble) value.i;
						break;
					case POK_LONG:
						val = (jdouble) value.j;
						break;
					default:
						invalidArgument = TRUE;
						break;
					}
					if (!invalidArgument) {
						J9JAVAARRAYOFDOUBLE_STORE(currentThread, j9array, index, *(U_64 *) &val);
					}
				} else if (vm->floatReflectClass == typeOfArray) {
					jfloat val = 0;
					switch (vCode) {
					case POK_CHAR:
						val = (jfloat) value.c;
						break;
					case POK_FLOAT:
						val = value.f;
						break;
					case POK_BYTE:
						val = (jfloat) value.b;
						break;
					case POK_SHORT:
						val = (jfloat) value.s;
						break;
					case POK_INT:
						val = (jfloat) value.i;
						break;
					case POK_LONG:
						val = (jfloat) value.j;
						break;
					default:
						invalidArgument = TRUE;
						break;
					}
					if (!invalidArgument) {
						J9JAVAARRAYOFFLOAT_STORE(currentThread, j9array, index, *(U_32 *) &val);
					}
				} else if (vm->shortReflectClass == typeOfArray) {
					switch (vCode) {
					case POK_BYTE:
						J9JAVAARRAYOFSHORT_STORE(currentThread, j9array, index, value.b);
						break;
					case POK_SHORT:
						J9JAVAARRAYOFSHORT_STORE(currentThread, j9array, index, value.s);
						break;
					default:
						invalidArgument = TRUE;
						break;
					}
				} else if (vm->charReflectClass == typeOfArray) {
					if (POK_CHAR == vCode) {
						J9JAVAARRAYOFCHAR_STORE(currentThread, j9array, index, value.c);
					} else {
						invalidArgument = TRUE;
					}
				} else if ((vm->booleanReflectClass == typeOfArray) && (4 == vCode)) {
					J9JAVAARRAYOFBOOLEAN_STORE(currentThread, j9array, index, value.z);
				} else {
					invalidArgument = TRUE;
				}
				if (invalidArgument) {
					vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
				}
			}
		} else {
			vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);

	return;
}



jobject JNICALL
JVM_SetProtectionDomain(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_SetProtectionDomain() stubbed!");
	return NULL;
}


void JNICALL
JVM_SetThreadPriority(JNIEnv* env, jobject thread, jint priority)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM* vm = currentThread->javaVM;
	const UDATA *prioMap = currentThread->javaVM->java2J9ThreadPriorityMap;
	J9VMThread *vmThread;

	if (currentThread->javaVM->runtimeFlags & J9_RUNTIME_NO_PRIORITIES) {
		return;
	}

	assert(prioMap != NULL);
	assert(priority >= 0);
	assert(priority <
		sizeof(currentThread->javaVM->java2J9ThreadPriorityMap)/sizeof(currentThread->javaVM->java2J9ThreadPriorityMap[0]));

	vm = currentThread->javaVM;
	vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);
	vmThread = J9VMJAVALANGTHREAD_THREADREF(currentThread, J9_JNI_UNWRAP_REFERENCE(thread) );
	vm->internalVMFunctions->internalExitVMToJNI(currentThread);

	if ( (NULL != vmThread) && (NULL != vmThread->osThread)) {
		J9ThreadEnv* threadEnv = getJ9ThreadEnv(env);
		threadEnv->set_priority(vmThread->osThread, prioMap[priority]);
	}
}



void JNICALL
JVM_StartThread(JNIEnv* jniEnv, jobject newThread)
{
	J9VMThread* currentThread = (J9VMThread*)jniEnv;
	J9JavaVM* javaVM = currentThread->javaVM;
	UDATA priority, isDaemon, privateFlags;
	j9object_t newThreadObject;
	UDATA result;

	javaVM->internalVMFunctions->internalEnterVMFromJNI(currentThread);

	newThreadObject = J9_JNI_UNWRAP_REFERENCE(newThread);

	if (0 != (javaVM->runtimeFlags & J9RuntimeFlagNoPriorities)) {
		priority = J9THREAD_PRIORITY_NORMAL;
	} else {
		priority = J9VMJAVALANGTHREAD_PRIORITY(currentThread, newThreadObject);
	}

	isDaemon = J9VMJAVALANGTHREAD_ISDAEMON(currentThread, newThreadObject);
	if (isDaemon) {
		privateFlags = J9_PRIVATE_FLAGS_DAEMON_THREAD;
	} else {
		privateFlags = 0;
	}

	result = javaVM->internalVMFunctions->startJavaThread(
			currentThread,
			newThreadObject,
			J9_PRIVATE_FLAGS_DAEMON_THREAD | J9_PRIVATE_FLAGS_NO_EXCEPTION_IN_START_JAVA_THREAD,
			javaVM->defaultOSStackSize,
			priority,
			(omrthread_entrypoint_t)javaVM->internalVMFunctions->javaThreadProc,
			javaVM,
			NULL);

	javaVM->internalVMFunctions->internalExitVMToJNI(currentThread);

	if (result != J9_THREAD_START_NO_ERROR) {
		assert(!"JVM_StartThread() failed!");
	}

	return;
}



jobject JNICALL
JVM_StopThread(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_StopThread() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_SuspendThread(jint arg0, jint arg1)
{
	assert(!"JVM_SuspendThread() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_UnloadLibrary(jint arg0)
{
	assert(!"JVM_UnloadLibrary() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_Yield(jint arg0, jint arg1)
{
	assert(!"JVM_Yield() stubbed!");
	return NULL;
}


/**
 * CMVC 150207 : Used by libnet.so on linux x86.
 */
jint JNICALL
JVM_SetSockOpt(jint fd, int level, int optname, const char *optval, int optlen)
{
	jint retVal;
	
#if defined(WIN32)
	retVal = setsockopt(fd, level, optname, optval, optlen);
#elif defined(J9ZTPF)
	retVal = setsockopt(fd, level, optname, (char *)optval, (socklen_t)optlen);
#else
	retVal = setsockopt(fd, level, optname, optval, (socklen_t)optlen);
#endif
	
	return retVal;
}


jint JNICALL
JVM_GetSockOpt(jint fd, int level, int optname, char *optval, int *optlen)
{
	jint retVal;

#if defined(WIN32)
	retVal = getsockopt(fd, level, optname, optval, optlen);
#elif defined(J9ZTPF)
	retVal = getsockopt(fd, level, optname, (char *)optval, (socklen_t *)optlen);
#else
	retVal = getsockopt(fd, level, optname, optval, (socklen_t *)optlen);
#endif
	
	return retVal;
}

/**
 * CMVC 150207 : Used by libnet.so on linux x86.
 */
jint JNICALL
JVM_SocketShutdown(jint fd, jint howto)
{
	jint retVal = JNI_FALSE;

#if defined(J9UNIX)
	retVal = shutdown(fd, howto);
#elif defined(WIN32) /* defined(J9UNIX) */
	retVal = closesocket(fd);
#else /* defined(J9UNIX) */
	assert(!"JVM_SocketShutdown() stubbed!");
#endif /* defined(J9UNIX) */
	
	return retVal;
}


/**
 * CMVC 150207 : Used by libnet.so on linux x86.
 */
jint JNICALL
JVM_GetSockName(jint fd, struct sockaddr *him, int *len)
{
	jint retVal;

#if defined(WIN32)
	retVal = getsockname(fd, him, len);
#else
	retVal = getsockname(fd, him, (socklen_t *)len);
#endif
	
	return retVal;
}

/**
 * CMVC 150207 : Used by libnet.so on linux x86.
 */
int JNICALL
JVM_GetHostName(char* name, int namelen)
{
	jint retVal;

	retVal = gethostname(name, namelen);

	return retVal;
}


/*
 * com.sun.tools.attach.VirtualMachine support
 *
 * Initialize the agent properties with the properties maintained in the VM.
 * The following properties are set by the reference implementation:
 * 	sun.java.command = name of the main class
 *  sun.jvm.flags = vm arguments passed to the launcher
 *  sun.jvm.args =
 */
/*
 * Notes: 
 * 	Redirector has an implementation of JVM_InitAgentProperties. 
 * 	This method is still kept within the actual jvm dll in case that a launcher uses this jvm dll directly without going through the redirector. 
 * 	If this method need to be modified, the changes have to be synchronized for both versions. 
 */ 
jobject JNICALL
JVM_InitAgentProperties(JNIEnv *env, jobject agent_props)
{
	/* CMVC 150259 : Assert in JDWP Agent
	 *   Simply returning the non-null properties instance is
	 *   sufficient to make the agent happy. */
	return agent_props;
}


/**
 * Extend boot classpath
 *
 * @param env
 * @param pathSegment		path to add to the bootclasspath
 * @return void
 *
 * Append specified path segment to the boot classpath
 */

void JNICALL
JVM_ExtendBootClassPath(JNIEnv* env, const char * pathSegment)
{
	ENSURE_VMI();

	g_VMI->JVM_ExtendBootClassPath(env, pathSegment);
}

/**
  * Throw java.lang.OutOfMemoryError
  */
void
throwNativeOOMError(JNIEnv *env, U_32 moduleName, U_32 messageNumber)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	vmFuncs->setNativeOutOfMemoryError(currentThread, moduleName, messageNumber);
	vmFuncs->internalExitVMToJNI(currentThread);
}


/**
  * Throw java.lang.NullPointerException with the message provided
  */
void
throwNewNullPointerException(JNIEnv *env, char *message)
{
	jclass exceptionClass = (*env)->FindClass(env, "java/lang/NullPointerException");
	if (exceptionClass == 0) {
		/* Just return if we can't load the exception class. */
		return;
	}
	(*env)->ThrowNew(env, exceptionClass, message);
}

/**
  * Throw java.lang.IndexOutOfBoundsException
  */
void
throwNewIndexOutOfBoundsException(JNIEnv *env, char *message)
{
	jclass exceptionClass = (*env)->FindClass(env, "java/lang/IndexOutOfBoundsException");
	if (exceptionClass == 0) {
		/* Just return if we can't load the exception class. */
		return;
	}
	(*env)->ThrowNew(env, exceptionClass, message);
}


/**
  * Throw java.lang.InternalError
  */
void
throwNewInternalError(JNIEnv *env, char *message)
{
	jclass exceptionClass = (*env)->FindClass(env, "java/lang/InternalError");
	if (exceptionClass == 0) {
		/* Just return if we can't load the exception class. */
		return;
	}
	(*env)->ThrowNew(env, exceptionClass, message);
}


/* Callers of this function must have already ensured that classLoaderObject has been initialized */

jclass
jvmDefineClassHelper(JNIEnv *env, jobject classLoaderObject,
	jstring className, jbyte * classBytes, jint offset, jint length, jobject protectionDomain, UDATA options)
{
/* Try a couple of GC passes (1 doesn't sem to be enough), but don't try forever */
#define MAX_RETRY_COUNT 2

	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	J9TranslationBufferSet *dynFuncs = NULL;
	J9ClassLoader *classLoader = NULL;
	UDATA retried = FALSE;
	UDATA utf8Length = 0;
	char utf8NameStackBuffer[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
	U_8 *utf8Name = NULL;
	J9Class *clazz = NULL;
	jclass result = NULL;
	J9ThreadEnv* threadEnv = getJ9ThreadEnv(env);
	J9ROMClass *loadedClass = NULL;
	U_8 *tempClassBytes = NULL;
	I_32 tempLength = 0;
	J9TranslationLocalBuffer localBuffer = {J9_CP_INDEX_NONE, LOAD_LOCATION_UNKNOWN, NULL};
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (vm->dynamicLoadBuffers == NULL) {
		throwNewInternalError(env, "Dynamic loader is unavailable");
		return NULL;
	}
	dynFuncs = vm->dynamicLoadBuffers;

	if (classBytes == NULL) {
		throwNewNullPointerException(env, NULL);
		return NULL;
	}

	vmFuncs->internalEnterVMFromJNI(currentThread);

	if (NULL != className) {
		utf8Name = (U_8*)vmFuncs->copyStringToUTF8WithMemAlloc(currentThread, J9_JNI_UNWRAP_REFERENCE(className), J9_STR_NULL_TERMINATE_RESULT | J9_STR_XLAT, "", 0, utf8NameStackBuffer, J9VM_PACKAGE_NAME_BUFFER_LENGTH, &utf8Length);
		if (NULL == utf8Name) {
			vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
			goto done;
		}
	}

	classLoader = J9VMJAVALANGCLASSLOADER_VMREF(currentThread, J9_JNI_UNWRAP_REFERENCE(classLoaderObject));

retry:

	threadEnv->monitor_enter(vm->classTableMutex);

	if (vmFuncs->hashClassTableAt(classLoader, utf8Name, utf8Length) != NULL) {
		/* Bad, we have already defined this class - fail */
		threadEnv->monitor_exit(vm->classTableMutex);
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGLINKAGEERROR, (UDATA *)*(j9object_t*)className);
		goto done;
	}

	tempClassBytes = (U_8 *) classBytes;
	tempLength = length;

	/* Check for romClass cookie, it indicates that we are  defining a class out of a JXE not from class bytes */

	loadedClass = vmFuncs->romClassLoadFromCookie(currentThread, (U_8*)utf8Name, utf8Length, (U_8*)classBytes, (UDATA) length);

	if (NULL != loadedClass) {
		/* An existing ROMClass is found in the shared class cache.
		 * If -Xshareclasses:enableBCI is present, need to give VM a chance to trigger ClassFileLoadHook event.
		 */
		if ((NULL == vm->sharedClassConfig) || (0 == vm->sharedClassConfig->isBCIEnabled(vm))) {
			clazz = vm->internalVMFunctions->internalCreateRAMClassFromROMClass(currentThread,
																			classLoader,
																			loadedClass,
																			0,
																			NULL,
																			protectionDomain ? *(j9object_t*)protectionDomain : NULL,
																			NULL,
																			J9_CP_INDEX_NONE,
																			LOAD_LOCATION_UNKNOWN,
																			NULL,
																			NULL);
			/* Done if a class was found or and exception is pending, otherwise try to define the bytes */
			if ((clazz != NULL) || (currentThread->currentException != NULL)) {
				goto done;
			} else {
				loadedClass = NULL;
			}
		} else {
			tempClassBytes = J9ROMCLASS_INTERMEDIATECLASSDATA(loadedClass);
			tempLength = loadedClass->intermediateClassDataLength;
			options |= J9_FINDCLASS_FLAG_SHRC_ROMCLASS_EXISTS;
		}
	}

	/* The defineClass helper requires you hold the class table mutex and releases it for you */

	clazz = dynFuncs->internalDefineClassFunction(currentThread,
												utf8Name, utf8Length,
												tempClassBytes, (UDATA) tempLength, NULL,
												classLoader,
												protectionDomain ? *(j9object_t*)protectionDomain : NULL,
												options | J9_FINDCLASS_FLAG_THROW_ON_FAIL | J9_FINDCLASS_FLAG_NO_CHECK_FOR_EXISTING_CLASS,
												loadedClass,
												NULL,
												&localBuffer);

	/* If OutOfMemory, try a GC to free up some memory */

	if (currentThread->privateFlags & J9_PRIVATE_FLAGS_CLOAD_NO_MEM) {
		if (!retried) {
			/*Trc_VM_internalFindClass_gcAndRetry(vmThread);*/
			currentThread->javaVM->memoryManagerFunctions->j9gc_modron_global_collect_with_overrides(currentThread, J9MMCONSTANT_EXPLICIT_GC_NATIVE_OUT_OF_MEMORY);
			retried = TRUE;
			goto retry;
		}
		vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
	}

done:
	if ((clazz == NULL) && (currentThread->currentException == NULL)) {
		/* should not get here -- throw the default exception just in case */
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGCLASSFORMATERROR, NULL);
	}

	result = vmFuncs->j9jni_createLocalRef(env, J9VM_J9CLASS_TO_HEAPCLASS(clazz));

	vmFuncs->internalExitVMToJNI(currentThread);

	if ((U_8*)utf8NameStackBuffer != utf8Name) {
		j9mem_free_memory(utf8Name);
	}

	return result;
}


jobject JNICALL
JVM_Bind(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_Bind() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_DTraceActivate(jint arg0, jint arg1, jint arg2, jint arg3, jint arg4)
{
	assert(!"JVM_DTraceActivate() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_DTraceDispose(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_DTraceDispose() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_DTraceGetVersion(jint arg0)
{
	assert(!"JVM_DTraceGetVersion() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_DTraceIsProbeEnabled(jint arg0, jint arg1)
{
	assert(!"JVM_DTraceIsProbeEnabled() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_DTraceIsSupported(jint arg0)
{
	assert(!"JVM_DTraceIsSupported() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_DefineClass(jint arg0, jint arg1, jint arg2, jint arg3, jint arg4, jint arg5)
{
	assert(!"JVM_DefineClass() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_DefineClassWithSourceCond(jint arg0, jint arg1, jint arg2, jint arg3, jint arg4, jint arg5, jint arg6, jint arg7)
{
	assert(!"JVM_DefineClassWithSourceCond() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_EnqueueOperation(jint arg0, jint arg1, jint arg2, jint arg3, jint arg4)
{
	assert(!"A HotSpot VM Attach API is attempting to connect to an OpenJ9 VM. This is not supported.");
	return NULL;
}

jobject JNICALL
JVM_Exit(jint arg0)
{
	assert(!"JVM_Exit() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_GetCPFieldNameUTF(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_GetCPFieldNameUTF() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_GetClassConstructor(jint arg0, jint arg1, jint arg2, jint arg3)
{
	assert(!"JVM_GetClassConstructor() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_GetClassConstructors(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_GetClassConstructors() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_GetClassField(jint arg0, jint arg1, jint arg2, jint arg3)
{
	assert(!"JVM_GetClassField() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_GetClassFields(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_GetClassFields() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_GetClassMethod(jint arg0, jint arg1, jint arg2, jint arg3, jint arg4)
{
	assert(!"JVM_GetClassMethod() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_GetClassMethods(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_GetClassMethods() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_GetField(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_GetField() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_GetFieldAnnotations(jint arg0, jint arg1)
{
	assert(!"JVM_GetFieldAnnotations() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_GetMethodAnnotations(jint arg0, jint arg1)
{
	assert(!"JVM_GetMethodAnnotations() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_GetMethodDefaultAnnotationValue(jint arg0, jint arg1)
{
	assert(!"JVM_GetMethodDefaultAnnotationValue() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_GetMethodParameterAnnotations(jint arg0, jint arg1)
{
	assert(!"JVM_GetMethodParameterAnnotations() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_GetPrimitiveField(jint arg0, jint arg1, jint arg2, jint arg3)
{
	assert(!"JVM_GetPrimitiveField() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_InitializeCompiler(jint arg0, jint arg1)
{
	assert(!"JVM_InitializeCompiler() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_IsSilentCompiler(jint arg0, jint arg1)
{
	assert(!"JVM_IsSilentCompiler() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_LoadClass0(jint arg0, jint arg1, jint arg2, jint arg3)
{
	assert(!"JVM_LoadClass0() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_NewInstance(jint arg0, jint arg1)
{
	assert(!"JVM_NewInstance() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_PrintStackTrace(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_PrintStackTrace() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_SetField(jint arg0, jint arg1, jint arg2, jint arg3)
{
	assert(!"JVM_SetField() stubbed!");
	return NULL;
}

jobject JNICALL
JVM_SetPrimitiveField(jint arg0, jint arg1, jint arg2, jint arg3, jint arg4, jint arg5)
{
	assert(!"JVM_SetPrimitiveField() stubbed!");
	return NULL;
}

void JNICALL
JVM_SetNativeThreadName(jint arg0, jobject arg1, jstring arg2)
{
	assert(!"JVM_SetNativeThreadName() stubbed!");
}
