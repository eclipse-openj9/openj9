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
 * @brief  This file contains implementations of the Sun VM interface (JVM_ functions).
 * 
 * In a VM-Proxy environment all functions in this file are expected to run VM-side,
 * and are compiled into the offload services via vpath.  
 * 
 * On the launcher-side we compile in forwarders for JVM_ by reusing code declared
 * in the redirector.
 */
#include <stdlib.h>
#include "j9cfg.h"

#include "j9.h"
#include "omrgcconsts.h"
#define _UTE_STATIC_
#include "ut_sunvmi.h"

#include "sunvmi_api.h"
#include "util_api.h"		/* for packageName() helpers */
#include "jvminit.h"
#include "mmhook.h"
#include "mmomrhook.h"
#include "j2sever.h"
#include "classloadersearch.h"
#include "rommeth.h"
#include "j9modifiers_api.h"
#include "j9vmconstantpool.h"
#include "j9vmnls.h"
#include "jcl_internal.h"

extern JNIEXPORT jobject JNICALL
JVM_FindClassFromClassLoader(JNIEnv* env, char* className, jboolean init, jobject classLoader, jboolean throwError);

/**
 * Global pointer to JavaVM structure for functions that lack either
 * JavaVM or JNIEnv context.
 */

typedef struct SunVMGlobals {
	J9JavaVM *javaVM;

	/* JNI references cached for efficiency */
	jclass jlClass;
	jclass jlThread;
	jmethodID jliMethodHandles_Lookup_checkSecurity;

	/* Thread library functions looked up dynamically */
	UDATA threadLibrary;
	IDATA (*monitorEnter)(omrthread_monitor_t monitor);
	IDATA (*monitorExit)(omrthread_monitor_t monitor);
	
	/* stores the current_time_millis at the last GLOBAL_GC_END event */
	I_64 lastGCTime;

} SunVMGlobals;

static SunVMGlobals VM;

static void initializeReflectionGlobalsHook(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData);
static jint initializeReflectionGlobals(JNIEnv * env, BOOLEAN includeAccessors);
static UDATA latestUserDefinedLoaderIterator(J9VMThread * currentThread, J9StackWalkState * walkState);
static UDATA getClassContextIterator(J9VMThread * currentThread, J9StackWalkState * walkState);

static UDATA
latestUserDefinedLoaderIterator(J9VMThread * currentThread, J9StackWalkState * walkState)
{
	J9JavaVM * vm = currentThread->javaVM;
	J9Class * currentClass = J9_CLASS_FROM_CP(walkState->constantPool);
	J9ClassLoader * classLoader = currentClass->classLoader;

	/* Ignore jdk/internal/loader/ClassLoaders$PlatformClassLoader (Java 9+).
	 * In Java 9+, vm->extensionClassLoader is the PlatformClassLoader.
	 */
	BOOLEAN isPlatformClassLoader = FALSE;
	if ((J2SE_VERSION(vm) >= J2SE_V11) && (classLoader == vm->extensionClassLoader)) {
		isPlatformClassLoader = TRUE;
	}

	if ((classLoader != vm->systemClassLoader) && !isPlatformClassLoader) {
		J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

		Assert_SunVMI_mustHaveVMAccess(currentThread);
		if ( (vm->srMethodAccessor && vmFuncs->instanceOfOrCheckCast(currentClass, J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, *((j9object_t*) vm->srMethodAccessor)))) ||
			 (vm->srConstructorAccessor && vmFuncs->instanceOfOrCheckCast(currentClass, J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, *((j9object_t*) vm->srConstructorAccessor)))) ||
			 (vm->jliArgumentHelper && vmFuncs->instanceOfOrCheckCast(currentClass, J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, *((j9object_t*) vm->jliArgumentHelper))))
		) {
			/* skip reflection classes */
		} else {
			walkState->userData1 = J9CLASSLOADER_CLASSLOADEROBJECT(currentThread, classLoader);
			return J9_STACKWALK_STOP_ITERATING;
		}
	}
	return J9_STACKWALK_KEEP_ITERATING;
}


/**
 * JVM_LatestUserDefinedLoader
 */
JNIEXPORT jobject JNICALL
JVM_LatestUserDefinedLoader_Impl(JNIEnv *env)
{
	J9VMThread * vmThread = (J9VMThread *) env;
	J9JavaVM * vm = vmThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	J9StackWalkState walkState;
	jobject result;

	Trc_SunVMI_LatestUserDefinedLoader_Entry(env);

	walkState.walkThread = vmThread;
	walkState.skipCount = 0;
	walkState.flags = J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_INCLUDE_NATIVES;
	walkState.userData1 = NULL;
	walkState.frameWalkFunction = latestUserDefinedLoaderIterator;
	vmFuncs->internalEnterVMFromJNI(vmThread);
	vm->walkStackFrames(vmThread, &walkState);

	result = vmFuncs->j9jni_createLocalRef(env, walkState.userData1);
	vmFuncs->internalExitVMToJNI(vmThread);

	Trc_SunVMI_LatestUserDefinedLoader_Exit(env, result);

	return result;
}

JNIEXPORT jbyteArray JNICALL
JVM_GetClassTypeAnnotations_Impl(JNIEnv *env, jclass jlClass) {
	return getClassTypeAnnotationsAsByteArray(env, jlClass);
}

JNIEXPORT jbyteArray JNICALL
JVM_GetFieldTypeAnnotations_Impl(JNIEnv *env, jobject jlrField) {
	return getFieldTypeAnnotationsAsByteArray(env, jlrField);
}

JNIEXPORT jbyteArray JNICALL
JVM_GetMethodTypeAnnotations_Impl(JNIEnv *env, jobject jlrMethod) {
    return getMethodTypeAnnotationsAsByteArray(env, jlrMethod);
}

JNIEXPORT jobjectArray JNICALL
JVM_GetMethodParameters_Impl(JNIEnv *env, jobject jlrExecutable) {
	return (jobjectArray)getMethodParametersAsArray(env, jlrExecutable);
}

static UDATA
getCallerClassIterator(J9VMThread * currentThread, J9StackWalkState * walkState)
{
	J9JavaVM * vm = currentThread->javaVM;
	

	if ((J9_ROM_METHOD_FROM_RAM_METHOD(walkState->method)->modifiers & J9AccMethodFrameIteratorSkip) == J9AccMethodFrameIteratorSkip) {
		/* Skip methods with java.lang.invoke.FrameIteratorSkip annotation */
		return J9_STACKWALK_KEEP_ITERATING;
	}

	if ((walkState->method != vm->jlrMethodInvoke) && (walkState->method != vm->jliMethodHandleInvokeWithArgs) && (walkState->method != vm->jliMethodHandleInvokeWithArgsList)) {
		J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
		J9Class * currentClass = J9_CLASS_FROM_CP(walkState->constantPool);

		Assert_SunVMI_mustHaveVMAccess(currentThread);

		if ( (vm->srMethodAccessor && vmFuncs->instanceOfOrCheckCast(currentClass, J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, *((j9object_t*) vm->srMethodAccessor)))) ||
		     (vm->srConstructorAccessor && vmFuncs->instanceOfOrCheckCast(currentClass, J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, *((j9object_t*) vm->srConstructorAccessor)))) ||
		     (vm->jliArgumentHelper && vmFuncs->instanceOfOrCheckCast(currentClass, J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, *((j9object_t*) vm->jliArgumentHelper))))
		) {
			/* skip reflection classes */
		} else {
			switch((UDATA)walkState->userData1) {
			case 1:
				break;
			case 0:
				walkState->userData2 = J9VM_J9CLASS_TO_HEAPCLASS(currentClass);
				return J9_STACKWALK_STOP_ITERATING;
			}
			walkState->userData1 = (void *) (((UDATA) walkState->userData1) - 1);
		}
	}
	return J9_STACKWALK_KEEP_ITERATING;
}


static UDATA
getCallerClassJEP176Iterator(J9VMThread * currentThread, J9StackWalkState * walkState)
{
	J9JavaVM * vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	J9Class * currentClass = J9_CLASS_FROM_CP(walkState->constantPool);
	
	Assert_SunVMI_mustHaveVMAccess(currentThread);

	if ((J9_ROM_METHOD_FROM_RAM_METHOD(walkState->method)->modifiers & J9AccMethodFrameIteratorSkip) == J9AccMethodFrameIteratorSkip) {
		/* Skip methods with java.lang.invoke.FrameIteratorSkip annotation */
		return J9_STACKWALK_KEEP_ITERATING;
	}

	switch((UDATA)walkState->userData1) {
	case 1:
		/* Caller must be annotated with @sun.reflect.CallerSensitive annotation */
		if (((vm->systemClassLoader != currentClass->classLoader) && (vm->extensionClassLoader != currentClass->classLoader))
				|| J9_ARE_NO_BITS_SET(J9_ROM_METHOD_FROM_RAM_METHOD(walkState->method)->modifiers, J9AccMethodCallerSensitive)
		) {
			walkState->userData3 = (void *) TRUE;
			return J9_STACKWALK_STOP_ITERATING;
		}
		break;
	case 0:
		if ((walkState->method == vm->jliMethodHandleInvokeWithArgs)
				|| (walkState->method == vm->jliMethodHandleInvokeWithArgsList)
				|| (walkState->method == vm->jlrMethodInvoke)
				|| (vm->srMethodAccessor && vmFuncs->instanceOfOrCheckCast(currentClass, J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, *((j9object_t*) vm->srMethodAccessor))))
				|| (vm->srConstructorAccessor && vmFuncs->instanceOfOrCheckCast(currentClass, J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, *((j9object_t*) vm->srConstructorAccessor))))
		) {
			/* skip reflection classes and MethodHandle.invokeWithArguments() when reaching depth 0 */
			return J9_STACKWALK_KEEP_ITERATING;
		}

		walkState->userData2 = J9VM_J9CLASS_TO_HEAPCLASS(currentClass);
		return J9_STACKWALK_STOP_ITERATING;
	}

	walkState->userData1 = (void *) (((UDATA) walkState->userData1) - 1);
	return J9_STACKWALK_KEEP_ITERATING;
}


/**
 * JVM_GetCallerClass
 */
JNIEXPORT jobject JNICALL
#if JAVA_SPEC_VERSION >= 11
JVM_GetCallerClass_Impl(JNIEnv *env)
#else /* JAVA_SPEC_VERSION >= 11 */
JVM_GetCallerClass_Impl(JNIEnv *env, jint depth)
#endif /* JAVA_SPEC_VERSION >= 11 */
{
	J9VMThread * vmThread = (J9VMThread *) env;
	J9JavaVM * vm = vmThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	J9StackWalkState walkState = {0};
	jobject result = NULL;
#if JAVA_SPEC_VERSION >= 11
	/* Java 11 removed getCallerClass(depth), and getCallerClass() is equivalent to getCallerClass(-1) */
	jint depth = -1;
#endif /* JAVA_SPEC_VERSION >= 11 */
	
	Trc_SunVMI_GetCallerClass_Entry(env, depth);
	
	walkState.frameWalkFunction = getCallerClassIterator;

	if (-1 == depth) {
		/* Assumes the stack looks like:
		 * [2] [jdk.internal|sun].reflect.Reflection.getCallerClass()
		 * [1] Method requesting caller
		 * [0] actual caller
		 */
		depth = 2;
		walkState.frameWalkFunction = getCallerClassJEP176Iterator;
	}

	walkState.walkThread = vmThread;
	walkState.skipCount = 0;
	walkState.flags = J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_ITERATE_FRAMES;
	walkState.userData1 = (void *) (IDATA) depth;
	walkState.userData2 = NULL;
	walkState.userData3 = (void *) FALSE;
	vmFuncs->internalEnterVMFromJNI(vmThread);
	vm->walkStackFrames(vmThread, &walkState);

	if (TRUE == (UDATA)walkState.userData3) {
		vmFuncs->setCurrentExceptionNLS(vmThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, J9NLS_VM_CALLER_NOT_ANNOTATED_AS_CALLERSENSITIVE);
	} else {
		result = vmFuncs->j9jni_createLocalRef(env, walkState.userData2);
	}

	vmFuncs->internalExitVMToJNI(vmThread);

	Trc_SunVMI_GetCallerClass_Exit(env, result);

	return result;
}


/**
 * JVM_NewInstanceFromConstructor
 */
JNIEXPORT jobject JNICALL
JVM_NewInstanceFromConstructor_Impl(JNIEnv * env, jobject c, jobjectArray args)
{
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM * vm = vmThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jobject obj;
	j9object_t classObj;
	jclass	classRef;
	J9JNIMethodID *methodID;

	Trc_SunVMI_NewInstanceFromConstructor_Entry(env, c, args);

	vmFuncs->internalEnterVMFromJNI(vmThread);
	methodID = vm->reflectFunctions.idFromConstructorObject(vmThread, J9_JNI_UNWRAP_REFERENCE(c));
	classObj = J9VM_J9CLASS_TO_HEAPCLASS(J9_CURRENT_CLASS(J9_CLASS_FROM_METHOD(methodID->method)));
	classRef = vmFuncs->j9jni_createLocalRef(env, classObj);
	vmFuncs->internalExitVMToJNI(vmThread);

	obj = (*env)->AllocObject(env, classRef);
	if (obj) {
		vmFuncs->sidecarInvokeReflectConstructor(vmThread, c, obj, args);
		if ((*env)->ExceptionCheck(env)) {
			(*env)->DeleteLocalRef(env, obj);
			obj = (jobject) NULL;
		}
	}
	(*env)->DeleteLocalRef(env, classRef);
	Trc_SunVMI_NewInstanceFromConstructor_Exit(env, obj);

	return obj;
}


/**
 * JVM_InvokeMethod
 */
JNIEXPORT jobject JNICALL 
JVM_InvokeMethod_Impl(JNIEnv * env, jobject method, jobject obj, jobjectArray args)
{
	J9VMThread *vmThread = (J9VMThread *) env;

	Trc_SunVMI_InvokeMethod_Entry(vmThread, method, obj, args);

	vmThread->javaVM->internalVMFunctions->sidecarInvokeReflectMethod(vmThread, method, obj, args);

	Trc_SunVMI_InvokeMethod_Exit(vmThread, vmThread->returnValue);

	return (jobject) vmThread->returnValue;
}


/**
 * JVM_GetClassAccessFlags
 */
JNIEXPORT jint JNICALL
JVM_GetClassAccessFlags_Impl(JNIEnv * env, jclass clazzRef)
{
	J9ROMClass *romClass;
	jint modifiers = 0;
	J9VMThread *vmThread = (J9VMThread *)env;
	J9JavaVM * vm = vmThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

	Trc_SunVMI_GetClassAccessFlags_Entry(env, clazzRef);

	vmFuncs->internalEnterVMFromJNI(vmThread);
	
	if (clazzRef == NULL) {
		Trc_SunVMI_GetClassAccessFlags_NullClassRef(env);
		vmFuncs->setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		Assert_SunVMI_true(J9VM_IS_INITIALIZED_HEAPCLASS(vmThread, *(j9object_t*)clazzRef));
		romClass = J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, *(j9object_t*)clazzRef)->romClass;
		if (J9ROMCLASS_IS_PRIMITIVE_TYPE(romClass)) {
			modifiers = J9AccAbstract | J9AccFinal | J9AccPublic;
		} else {
			modifiers = romClass->modifiers & 0xFFFF;
		}
	}	
	vmFuncs->internalExitVMToJNI(vmThread);

	Trc_SunVMI_GetClassAccessFlags_Exit(env, modifiers);

	return modifiers;
}


static jint 
initializeReflectionGlobals(JNIEnv * env,BOOLEAN includeAccessors) {
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM * vm = vmThread->javaVM;
	jclass clazz, clazzConstructorAccessorImpl, clazzMethodAccessorImpl;

	clazz = (*env)->FindClass(env, "java/lang/Class");
	if (NULL == clazz) {
		return JNI_ERR;
	}

	VM.jlClass = (*env)->NewGlobalRef(env, clazz);
	if (NULL == VM.jlClass) {
		return JNI_ERR;
	}

#if defined(J9VM_OPT_METHOD_HANDLE) && !defined(J9VM_IVE_RAW_BUILD) /* J9VM_IVE_RAW_BUILD is not enabled by default */
	clazz = (*env)->FindClass(env, "java/lang/invoke/MethodHandles$Lookup");
	if (NULL == clazz) {
		return JNI_ERR;
	}
	VM.jliMethodHandles_Lookup_checkSecurity = (*env)->GetMethodID(env, clazz, "checkSecurity", "(Ljava/lang/Class;Ljava/lang/Class;I)V");
	if (NULL == VM.jliMethodHandles_Lookup_checkSecurity) {
		return JNI_ERR;
	}
#endif /* defined(J9VM_OPT_METHOD_HANDLE) && !defined(J9VM_IVE_RAW_BUILD) */
	
	if (J2SE_VERSION(vm) >= J2SE_V11) {
		clazzConstructorAccessorImpl = (*env)->FindClass(env, "jdk/internal/reflect/ConstructorAccessorImpl");
		clazzMethodAccessorImpl = (*env)->FindClass(env, "jdk/internal/reflect/MethodAccessorImpl");
	} else {
		clazzConstructorAccessorImpl = (*env)->FindClass(env, "sun/reflect/ConstructorAccessorImpl");
		clazzMethodAccessorImpl = (*env)->FindClass(env, "sun/reflect/MethodAccessorImpl");
	}
	if ((NULL == clazzConstructorAccessorImpl) || (NULL == clazzMethodAccessorImpl)) {
		return JNI_ERR;
	}

	vm->srConstructorAccessor = (*env)->NewGlobalRef(env, clazzConstructorAccessorImpl);
	if (NULL == vm->srConstructorAccessor) {
		return JNI_ERR;
	}

	vm->srMethodAccessor = (*env)->NewGlobalRef(env, clazzMethodAccessorImpl);
	if (NULL == vm->srMethodAccessor) {
		return JNI_ERR;
	}
	
	return JNI_OK;
}


static void
initializeReflectionGlobalsHook(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData)
{
	J9VMThreadCreatedEvent *data = eventData;
	J9VMThread *vmThread = data->vmThread;
	J9JavaVM * vm = vmThread->javaVM;
	JNIEnv *env = (JNIEnv*)(vmThread);

	/* We only need to initialize handles to reflect for Sun libraries */
	if (JNI_OK != initializeReflectionGlobals(env, TRUE)) {
		data->continueInitialization = FALSE;
	}

	(*hookInterface)->J9HookUnregister(hookInterface, J9HOOK_VM_INITIALIZE_REQUIRED_CLASSES_DONE, initializeReflectionGlobalsHook, NULL);
}


/**
 * JVM_GetClassContext
 */
JNIEXPORT jobject JNICALL
JVM_GetClassContext_Impl(JNIEnv *env)
{
	J9VMThread * vmThread = (J9VMThread *) env;
	J9JavaVM *vm = vmThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	J9StackWalkState walkState;
	jobject classCtx;

	Trc_SunVMI_GetClassContext_Entry(env);

	walkState.walkThread = vmThread;
	walkState.flags = J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_ITERATE_FRAMES;

	walkState.frameWalkFunction = getClassContextIterator;

	/* calculate length of class context */
	walkState.skipCount = 1;
	walkState.userData1 = (void *) 0;
	walkState.userData2 = (void *) NULL;

	vmFuncs->internalEnterVMFromJNI(vmThread);
	vm->walkStackFrames(vmThread, &walkState);
	vmFuncs->internalExitVMToJNI(vmThread);

	classCtx = (*env)->NewObjectArray(env, (jsize)(UDATA) walkState.userData1, VM.jlClass, 0);
	if (classCtx) {
		/* fill in class context elements */
		walkState.skipCount = 1;
		walkState.userData1 = (void *) 0;
		vmFuncs->internalEnterVMFromJNI(vmThread);
		walkState.userData2 = *((j9object_t*) classCtx);
		vm->walkStackFrames(vmThread, &walkState);
		vmFuncs->internalExitVMToJNI(vmThread);
	}

	Trc_SunVMI_GetClassContext_Exit(env, classCtx);

	return classCtx;
}


JNIEXPORT void JNICALL
JVM_Halt_Impl(jint exitCode)
{
	J9VMThread* vmThread = VM.javaVM->internalVMFunctions->currentVMThread(VM.javaVM);

	Trc_SunVMI_Halt_Entry(vmThread, exitCode);

	VM.javaVM->internalVMFunctions->exitJavaVM(vmThread, exitCode);

	/* should not get here */

	Trc_SunVMI_Halt_Exit(vmThread);

	exit(exitCode);
}


JNIEXPORT void JNICALL
JVM_GCNoCompact_Impl(void)
{
	J9VMThread *currentThread = NULL;

	Trc_SunVMI_GCNoCompact_Entry(currentThread);
	currentThread = VM.javaVM->internalVMFunctions->currentVMThread(VM.javaVM);

	VM.javaVM->internalVMFunctions->internalEnterVMFromJNI(currentThread);
	VM.javaVM->memoryManagerFunctions->j9gc_modron_global_collect_with_overrides(currentThread, J9MMCONSTANT_EXPLICIT_GC_NOT_AGGRESSIVE);
	VM.javaVM->internalVMFunctions->internalExitVMToJNI(currentThread);

	Trc_SunVMI_GCNoCompact_Exit(currentThread);
}


/* Walk the stack, and for frames that should be visited:
 * 	1) update the frame count in userData1.
 * 	2) if userData2 isn't null, record the j.l.Class for the frame in the Class[]
 *
 * Special cases:
 * --------------
 * 	For JSR292, MethodHandles.lookup().find* methods need to store the Lookup's accessClass into the Class[] rather than MethodHandles.Lookup.class
 *
 * WalkState userdata values:
 * --------------------------
 * walkState->userData1 (UDATA) is the index of the current frame.  Its incremented for each frame visited.
 * walkState->userData2 (Object) is null or a Class[] holding the j.l.Class for each visited frame.
 */
static UDATA
getClassContextIterator(J9VMThread * currentThread, J9StackWalkState * walkState)
{
	J9JavaVM * vm = currentThread->javaVM;

	if ((J9_ROM_METHOD_FROM_RAM_METHOD(walkState->method)->modifiers & J9AccMethodFrameIteratorSkip) == J9AccMethodFrameIteratorSkip) {
		/* Skip methods with java.lang.invoke.FrameIteratorSkip annotation */
		return J9_STACKWALK_KEEP_ITERATING;
	}

	if ((walkState->method != vm->jlrMethodInvoke) && (walkState->method != vm->jliMethodHandleInvokeWithArgs) && (walkState->method != vm->jliMethodHandleInvokeWithArgsList)) {
		J9Class * currentClass = J9_CLASS_FROM_CP(walkState->constantPool);
		J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

		Assert_SunVMI_mustHaveVMAccess(currentThread);

		if ( (vm->srMethodAccessor && vmFuncs->instanceOfOrCheckCast(currentClass, J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, *((j9object_t*) vm->srMethodAccessor)))) ||
			 (vm->srConstructorAccessor && vmFuncs->instanceOfOrCheckCast(currentClass, J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, *((j9object_t*) vm->srConstructorAccessor)))) ||
			 (vm->jliArgumentHelper && vmFuncs->instanceOfOrCheckCast(currentClass, J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, *((j9object_t*) vm->jliArgumentHelper))))
		) {
			/* skip reflection classes */
		} else {
			IDATA n = (IDATA) walkState->userData1;

			/* first pass gets us the size of the array, second pass fills in the elements */
			if (walkState->userData2 != NULL) {
				j9object_t classObject = J9VM_J9CLASS_TO_HEAPCLASS(currentClass);
				J9JAVAARRAYOFOBJECT_STORE(currentThread, walkState->userData2, (I_32)n, classObject);
			}

			walkState->userData1 = (void *) (n + 1);
		}
	}
	return J9_STACKWALK_KEEP_ITERATING;
}


/**
 * JVM_GC
 */
JNIEXPORT void JNICALL
JVM_GC_Impl(void)
{
	J9VMThread *currentThread = VM.javaVM->internalVMFunctions->currentVMThread(VM.javaVM);

	Trc_SunVMI_GC_Entry(currentThread);

	VM.javaVM->internalVMFunctions->internalEnterVMFromJNI(currentThread);
	VM.javaVM->memoryManagerFunctions->j9gc_modron_global_collect(currentThread);
	VM.javaVM->internalVMFunctions->internalExitVMToJNI(currentThread);

	Trc_SunVMI_GC_Exit(currentThread);
}


/**
 * JVM_TotalMemory
 */
JNIEXPORT jlong JNICALL
JVM_TotalMemory_Impl(void)
{
	jlong result;

	Trc_SunVMI_TotalMemory_Entry();

	result = VM.javaVM->memoryManagerFunctions->j9gc_heap_total_memory(VM.javaVM);

	Trc_SunVMI_TotalMemory_Exit(result);

	return result;
}


JNIEXPORT jlong JNICALL
JVM_FreeMemory_Impl(void)
{
	jlong result;

	Trc_SunVMI_FreeMemory_Entry();
	result = VM.javaVM->memoryManagerFunctions->j9gc_heap_free_memory(VM.javaVM);
	Trc_SunVMI_FreeMemory_Exit(result);

	return result;
}


/**
 * Returns an array of strings for the currently loaded system packages. The name returned for each package is to be '/' separated and end with a '/' character.
 *
 * @arg[in] env          - JNI environment.
 *
 * @return a jarray of jstrings
 *
 */
JNIEXPORT jobject JNICALL
JVM_GetSystemPackages_Impl(JNIEnv* env)
{
	J9VMThread* vmThread = (J9VMThread*)env;
	J9JavaVM* vm = vmThread->javaVM;
	J9InternalVMFunctions* funcs = vm->internalVMFunctions;
	J9HashTableState walkState;
	J9PackageIDTableEntry* packageID;
	J9PackageIDTableEntry** packageIDList;
	UDATA packageCount, i;
	jarray result = NULL;
	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_SunVMI_GetSystemPackages_Entry(env);

	/* it might not be strictly necessary to acquire VM access here, but be safe */
	funcs->internalEnterVMFromJNI(vmThread);
	VM.monitorEnter(vm->classTableMutex);

	/* first count the packages */
	packageCount = 0;
	packageID = funcs->hashPkgTableStartDo(vm->systemClassLoader, &walkState);
	while (packageID) {
		packageCount += 1;
		packageID = funcs->hashPkgTableNextDo(&walkState);
	}

	/* attempt to allocate a temporary array to hold them all */
	packageIDList = j9mem_allocate_memory(packageCount * sizeof(J9PackageIDTableEntry*), OMRMEM_CATEGORY_VM);

	/* fill in the list */
	if (NULL == packageIDList) {
		VM.monitorExit(vm->classTableMutex);
		funcs->setNativeOutOfMemoryError(vmThread, 0, 0);
	} else {
		packageCount = 0;
		packageID = funcs->hashPkgTableStartDo(vm->systemClassLoader, &walkState);
		while (packageID) {
			packageIDList[packageCount++] = packageID;
			packageID = funcs->hashPkgTableNextDo(&walkState);
		}
		VM.monitorExit(vm->classTableMutex);
	}

	funcs->internalExitVMToJNI(vmThread);

	/* push local frame with room for the 3 elements: jlsClass, result, packageStringRef */
	if (NULL != packageIDList) {
		if ( (*env)->PushLocalFrame(env, 3) == 0 ) {
			jclass jlsClass = (*env)->FindClass(env, "java/lang/String");
			if (jlsClass) {
				result = (*env)->NewObjectArray(env, (jsize) packageCount, jlsClass, NULL);
				if (result) {
					for (i = 0; i < packageCount; i++) {
						j9object_t packageString;
						jobject packageStringRef = NULL;
						const U_8* packageName = NULL;
						UDATA packageNameLength;

						funcs->internalEnterVMFromJNI(vmThread);
						packageName = getPackageName(packageIDList[i], &packageNameLength);
						/*
						 * java.lang.Package.getSystemPackages() expects a trailing slash in Java 8
						 * but no trailing slash in Java 9.
						 */
						if (J2SE_VERSION(vm) >= J2SE_V11) {
							packageString = vm->memoryManagerFunctions->j9gc_createJavaLangString(vmThread,
									(U_8*) packageName, packageNameLength, 0);
						} else {
							packageString = funcs->catUtfToString4(vmThread, packageName, packageNameLength, (U_8*)"/", 1, NULL, 0, NULL, 0);
						}
						if (packageString) {
							packageStringRef = funcs->j9jni_createLocalRef(env, packageString);
						}
						funcs->internalExitVMToJNI(vmThread);

						if (packageStringRef) {
							(*env)->SetObjectArrayElement(env, result, (jsize) i, packageStringRef);
							(*env)->DeleteLocalRef(env, packageStringRef);
						}

						if ( (*env)->ExceptionCheck(env) ) {
							result = NULL;
							break;
						}
					}
				}
			}
			result = (*env)->PopLocalFrame(env, result);
		}
		j9mem_free_memory(packageIDList);
	}

	Trc_SunVMI_GetSystemPackages_Exit(env, result);

	return result;
}


/**
 * Returns the package information for the specified package name.  Package information is the directory or
 * jar file name from where the package was loaded (separator is to be '/' and for a directory the return string is
 * to end with a '/' character). If the package is not loaded then null is to be returned.
 *
 * @arg[in] env          - JNI environment.
 * @arg[in] pkgName -  A Java string for the name of a package. The package must be separated with '/' and optionally end with a '/' character.
 *
 * @return Package information as a string.
 *
 * @note In the current implementation, the separator is not guaranteed to be '/', not is a directory guaranteed to be
 * terminated with a slash. It is also unclear what the expected implementation is for UNC paths.
 *
 * @note see CMVC defects 81175 and 92979
 */

#define NAME_BUFFER_SIZE 256
JNIEXPORT jstring JNICALL
JVM_GetSystemPackage_Impl(JNIEnv* env, jstring pkgName)
{
	J9VMThread* vmThread = (J9VMThread*)env;
	J9JavaVM* vm = vmThread->javaVM;
	J9InternalVMFunctions* funcs = vm->internalVMFunctions;
	const char* utfPkgName;
	size_t utfPkgNameLen;
	J9Class *clazz = NULL;
	jstring result = NULL;
	J9ROMClass tempClass;
	J9ROMClass *queryROMClass = &tempClass;
	U_8 nameBuffer[NAME_BUFFER_SIZE];
	J9UTF8 *pkgNameUTF8 = (J9UTF8 *) nameBuffer;
	J9PackageIDTableEntry *idEntry;
	j9object_t jlstringObject = NULL;
	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_SunVMI_GetSystemPackage_Entry(env, pkgName);

	if (NULL == pkgName) {
		return NULL;
	}

	utfPkgName = (const char *) (*env)->GetStringUTFChars(env, pkgName, NULL);
	if (NULL == utfPkgName) {
		return NULL;
	}

	utfPkgNameLen = strlen(utfPkgName);

	if (0 == utfPkgNameLen) {
		return NULL;
	}

	funcs->internalEnterVMFromJNI(vmThread);

	if (utfPkgNameLen > (NAME_BUFFER_SIZE - 3)) { /* subtract 2 bytes for length and one byte for terminating '/' */
		/*
		 * Allocate a buffer for the temporary ROM class and the package name plus space for trailing '/' if needed.
		 * This allocates a few extra bytes but will be need for only a short time.
		 */
		UDATA allocationSize = sizeof(J9ROMClass) + sizeof(J9UTF8) + utfPkgNameLen + 1;
		Trc_SunVMI_AllocateRomClass(vmThread, allocationSize);
		queryROMClass = j9mem_allocate_memory(allocationSize, J9MEM_CATEGORY_VM);
		if (NULL == queryROMClass) {
			Trc_SunVMI_AllocateRomClassFailed(vmThread);
			goto done;
		} else {
			/* the end of a J9ROMClass is guaranteed to be aligned */
			pkgNameUTF8 =  (J9UTF8 *)(queryROMClass + 1);
		}
	}
	memset(queryROMClass, 0, sizeof(J9ROMClass));
	memcpy(J9UTF8_DATA(pkgNameUTF8), utfPkgName, utfPkgNameLen);
	if ('/' != J9UTF8_DATA(pkgNameUTF8)[utfPkgNameLen - 1]) {
		J9UTF8_DATA(pkgNameUTF8)[utfPkgNameLen] = '/';
		utfPkgNameLen += 1;
	}
	J9UTF8_SET_LENGTH(pkgNameUTF8, (U_16)utfPkgNameLen);
	NNSRP_SET(queryROMClass->className, pkgNameUTF8);

	Trc_SunVMI_GetSystemPackage_SearchingForPackage(vmThread, utfPkgName, utfPkgNameLen);

	VM.monitorEnter(vm->classTableMutex);

	/* find a representative class from the package in the bootstrap loader */
	idEntry =  funcs->hashPkgTableAt(vm->systemClassLoader, queryROMClass);
	if (NULL != idEntry) {
		J9ROMClass *resultROMClass = (J9ROMClass *)(idEntry->taggedROMClass & ~(UDATA)(J9PACKAGE_ID_TAG | J9PACKAGE_ID_GENERATED));
		J9UTF8* className = J9ROMCLASS_CLASSNAME(resultROMClass);
		clazz = funcs->hashClassTableAt(vm->systemClassLoader, J9UTF8_DATA(className), J9UTF8_LENGTH(className));
	}
	VM.monitorExit(vm->classTableMutex);

	if (queryROMClass != &tempClass) {
		j9mem_free_memory(queryROMClass);
	}
	if (NULL != clazz) {
		UDATA pathLength = 0;
		U_8 *path = getClassLocation(vmThread, clazz, &pathLength);

		if (NULL != path) {
			jlstringObject = vm->memoryManagerFunctions->j9gc_createJavaLangString(vmThread, path, pathLength, 0);
		}
	}
	if (NULL != jlstringObject) {
		result = funcs->j9jni_createLocalRef(env, jlstringObject);
	}

done:
	funcs->internalExitVMToJNI(vmThread);

	(*env)->ReleaseStringUTFChars(env, pkgName, utfPkgName);

	Trc_SunVMI_GetSystemPackage_Exit(env, result);

	return result;
}
#undef NAME_BUFFER_SIZE



/**
 * NOTE THIS IS NOT REQUIRED FOR 1.4
 */
JNIEXPORT jobject JNICALL
JVM_AllocateNewObject_Impl(JNIEnv *env, jclass caller, jclass current, jclass init) {
	jobject result = NULL;
	jmethodID mID;

	Trc_SunVMI_AllocateNewObject_Entry(env, caller, current, init);

	mID = (*env)->GetMethodID(env, init, "<init>", "()V");

	if (mID) {
		result = (*env)->NewObject(env, current, mID);
	}

	Trc_SunVMI_AllocateNewObject_Exit(env, result);

	return result;
}


/**
 * NOTE THIS IS NOT REQUIRED FOR 1.4
 */
JNIEXPORT jobject JNICALL
JVM_AllocateNewArray_Impl(JNIEnv *env, jclass caller, jclass current, jint length)
{
	jobject result;

	Trc_SunVMI_AllocateNewArray_Entry(env, caller, current, length);

	if ((*env)->IsSameObject(env, (*env)->FindClass(env, "[Z"), current)) {
		result = (*env)->NewBooleanArray(env, length);
	} else if ((*env)->IsSameObject(env, (*env)->FindClass(env, "[B"), current)) {
		result = (*env)->NewByteArray(env, length);
	} else if ((*env)->IsSameObject(env, (*env)->FindClass(env, "[C"), current)) {
		result = (*env)->NewCharArray(env, length);
	} else if ((*env)->IsSameObject(env, (*env)->FindClass(env, "[S"), current)) {
		result = (*env)->NewShortArray(env, length);
	} else if ((*env)->IsSameObject(env, (*env)->FindClass(env, "[I"), current)) {
		result = (*env)->NewIntArray(env, length);
	} else if ((*env)->IsSameObject(env, (*env)->FindClass(env, "[J"), current)) {
		result = (*env)->NewLongArray(env, length);
	} else if ((*env)->IsSameObject(env, (*env)->FindClass(env, "[F"), current)) {
		result = (*env)->NewFloatArray(env, length);
	} else if ((*env)->IsSameObject(env, (*env)->FindClass(env, "[D"), current)) {
		result = (*env)->NewDoubleArray(env, length);
	} else {
		J9VMThread *currentThread = (J9VMThread*)env;
		J9ArrayClass *currentClass;
		jclass elementType;

		currentThread->javaVM->internalVMFunctions->internalEnterVMFromJNI(currentThread);
		currentClass = (J9ArrayClass *)J9VM_J9CLASS_FROM_JCLASS(currentThread, current);
		elementType = currentThread->javaVM->internalVMFunctions->j9jni_createLocalRef(
			env, J9VM_J9CLASS_TO_HEAPCLASS(currentClass->componentType));
		currentThread->javaVM->internalVMFunctions->internalExitVMToJNI(currentThread);

		result = (*env)->NewObjectArray(env, length, elementType, 0);
		(*env)->DeleteLocalRef(env, elementType);
	}

	Trc_SunVMI_AllocateNewArray_Exit(env, result);

	return result;
}






/**
 * JVM_GetClassLoader
 */
JNIEXPORT jobject JNICALL
JVM_GetClassLoader_Impl(JNIEnv *env, jobject obj)
{
	j9object_t j9ClassLoader;
	jobject cloader = NULL;
	J9JavaVM* vm = ((J9VMThread*)env)->javaVM;
	J9VMThread *vmThread = (J9VMThread *)env;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

	Trc_SunVMI_GetClassLoader_Entry(env, obj);

	vmFuncs->internalEnterVMFromJNI(vmThread);

	if(obj) {
		J9Class *clazz = NULL;
		j9object_t localObject = *(j9object_t*)obj;
		Assert_SunVMI_true(J9VM_IS_INITIALIZED_HEAPCLASS((J9VMThread *)env, localObject));
		clazz = J9VM_J9CLASS_FROM_HEAPCLASS((J9VMThread *)env, localObject);
		j9ClassLoader = J9CLASSLOADER_CLASSLOADEROBJECT(vmThread, clazz->classLoader);
	} else {
		j9ClassLoader = J9CLASSLOADER_CLASSLOADEROBJECT(vmThread, (J9ClassLoader *)vm->systemClassLoader);
	}

	cloader = vmFuncs->j9jni_createLocalRef(env, j9ClassLoader);

	vmFuncs->internalExitVMToJNI(vmThread);

	Trc_SunVMI_GetClassLoader_Exit(env, cloader);

	return cloader;
}


/**
 * JVM_GetThreadInterruptEvent
 */
JNIEXPORT void* JNICALL
JVM_GetThreadInterruptEvent_Impl(void)
{
	J9VMThread* vmThread = VM.javaVM->internalVMFunctions->currentVMThread(VM.javaVM);
	void* result;

	Trc_SunVMI_GetThreadInterruptEvent_Entry(vmThread);

	result = vmThread->sidecarEvent;

	Trc_SunVMI_GetThreadInterruptEvent_Exit(vmThread, result);

	return result;
}

/**
 * JNIEXPORT jlong JNICALL JVM_MaxObjectInspectionAge()
 *  This function is used by a native in java.dll
 *
 *  @returns a jlong representing the max object inspection age
 *
 *	DLL: jvm
 */
JNIEXPORT jlong JNICALL
JVM_MaxObjectInspectionAge_Impl(void)
{
	PORT_ACCESS_FROM_JAVAVM(VM.javaVM);
	jlong delta = (jlong)(j9time_current_time_millis() - VM.lastGCTime);

	Trc_SunVMI_MaxObjectInspectionAge(delta);

	return delta;
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
static jobject
internalFindClassFromClassLoader(JNIEnv* env, char* className, jboolean init, jobject classLoader, jboolean throwError)
{
	J9JavaVM* vm;
	J9InternalVMFunctions *vmFuncs;
	J9VMThread * currentThread;
	J9ClassLoader * loader;
	UDATA options = 0;
	BOOLEAN initializeSent = FALSE;
	J9Class *clazz;
	jobject classRef = NULL;

	currentThread = (J9VMThread*) env;
	vm = currentThread->javaVM;
	vmFuncs = vm->internalVMFunctions;

	vmFuncs->internalEnterVMFromJNI(currentThread);

	if (classLoader == NULL) {
		loader = vm->systemClassLoader;
	} else {
		loader = J9VMJAVALANGCLASSLOADER_VMREF(currentThread, J9_JNI_UNWRAP_REFERENCE(classLoader));
		if (NULL == loader) {
			loader = vmFuncs->internalAllocateClassLoader(vm, J9_JNI_UNWRAP_REFERENCE(classLoader));
			if (NULL == loader) {
				vmFuncs->internalExitVMToJNI((J9VMThread *)env);
				if (throwError == JNI_FALSE) {
					(*env)->ExceptionClear(env);
				}
				return NULL;
			}
		}
	}

	if (throwError == JNI_TRUE) {
		options |= J9_FINDCLASS_FLAG_THROW_ON_FAIL;
	}

	clazz = vmFuncs->internalFindClassUTF8(currentThread, (U_8*)className, strlen((const char*)className), loader, options);

	if (NULL != clazz) {
		if (init == JNI_TRUE) {
			UDATA initStatus = clazz->initializeStatus;
			if ((initStatus != J9ClassInitSucceeded) && (initStatus != (UDATA) currentThread)) {
				vmFuncs->initializeClass(currentThread, clazz);

				initializeSent = TRUE;

				/* Check for a pending exception directly, do not use ExceptionCheck as it will 
				   cause jnichk to dump an error since we are calling it with vm access */
				if (currentThread->currentException != NULL) {
					clazz = NULL;
				}
			}
		}

		/* Unsure whether this is supposed to be a local or global ref. JNI FindClass() returns a local ref so lets go
		   with that. */

		classRef = vmFuncs->j9jni_createLocalRef(env, J9VM_J9CLASS_TO_HEAPCLASS(clazz));
	}

	vmFuncs->internalExitVMToJNI(currentThread);

	if (initializeSent == TRUE) {

		/* initializeClass might have thrown and the caller requested not to throw, clear the exception */
		if (throwError == JNI_FALSE) {
			(*env)->ExceptionClear(env);
		}
	}

	return classRef;
}

typedef struct {
	JNIEnv* env;
	char* className;
	jboolean init;
	jobject classLoader;
	jboolean throwError;
} J9RedirectedFindClassFromClassLoaderArgs;

/**
 * This is an helper function to call internalFindClassFromClassLoader indirectly from gpProtectAndRun function.
 * 
 * @param entryArg	Argument structure (J9RedirectedFindClassFromClassLoaderArgs).
 */
static UDATA
gpProtectedInternalFindClassFromClassLoader(void * entryArg)
{
	J9RedirectedFindClassFromClassLoaderArgs * args = (J9RedirectedFindClassFromClassLoaderArgs *) entryArg;
	return (UDATA) internalFindClassFromClassLoader(args->env, args->className, args->init, args->classLoader, args->throwError);
}

/**
 * This function makes sure that call to "internalFindClassFromClassLoader" is gpProtected
 *
 * @param env
 * @param className    null-terminated class name string.
 * @param init         initialize the class when set
 * @param classLoader  classloader of the class
 * @param throwError   set to true in order to throw errors
 * @return Assumed to be a jclass.
 *
 * 
 */
JNIEXPORT jobject JNICALL
JVM_FindClassFromClassLoader_Impl(JNIEnv* env, char* className, jboolean init, jobject classLoader, jboolean throwError)
{
	if (NULL == env) {
		return NULL;
	} 

	if (((J9VMThread*) env)->gpProtected) {
		return internalFindClassFromClassLoader(env, className, init, classLoader, throwError);
	} else {
		J9RedirectedFindClassFromClassLoaderArgs args;
		args.env = env;
		args.className = className;
		args.init = init;
		args.classLoader = classLoader;
		args.throwError = throwError;
		return (jobject) gpProtectAndRun(gpProtectedInternalFindClassFromClassLoader, env, &args);
	}
}


static void
gcDidComplete(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	PORT_ACCESS_FROM_JAVAVM(VM.javaVM);
	VM.lastGCTime = j9time_current_time_millis();
}


static BOOLEAN
initializeThreadFunctions(J9PortLibrary* portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	
	if (0 != j9sl_open_shared_library(J9_THREAD_DLL_NAME, &VM.threadLibrary, J9PORT_SLOPEN_DECORATE)) {
		return FALSE;
	}
	if (0 != j9sl_lookup_name(VM.threadLibrary, "omrthread_monitor_enter", (UDATA*)&VM.monitorEnter, NULL)) {
		return FALSE;		
	}

	if (0 != j9sl_lookup_name(VM.threadLibrary, "omrthread_monitor_exit", (UDATA*)&VM.monitorExit, NULL)) {
		return FALSE;		
	}

	return TRUE;
}

JNIEXPORT jlong JNICALL JVM_MaxMemory_Impl(void) {
	jlong result;
	PORT_ACCESS_FROM_JAVAVM(VM.javaVM);

	Trc_SunVMI_MaxMemory_Entry();

	result = VM.javaVM->memoryManagerFunctions->j9gc_get_maximum_heap_size(VM.javaVM);

	Trc_SunVMI_MaxMemory_Exit(result);

	return result;
}


/**
 * Extend boot classpath
 *
 * @param env
 * @param pathSegment		path to add to the boot classpath
 * @return void
 *
 * Append specified path segment to the boot classpath
 */
JNIEXPORT void JNICALL
JVM_ExtendBootClassPath_Impl(JNIEnv* env, const char * pathSegment)
{
	J9JavaVM* vm = ((J9VMThread*) env)->javaVM;

	if (vm->systemClassLoader->classLoaderObject == NULL) {
		vm->internalVMFunctions->addToBootstrapClassLoaderSearch(vm, pathSegment, CLS_TYPE_ADD_TO_SYSTEM_PROPERTY, FALSE);
	} else {
		vm->internalVMFunctions->addToBootstrapClassLoaderSearch(vm, pathSegment, CLS_TYPE_ADD_TO_SYSTEM_CLASSLOADER, FALSE);
	}

	return;
}

static SunVMI VMIFunctionTable = {
		SUNVMI_VERSION_1_1,
		JVM_AllocateNewArray_Impl,
		JVM_AllocateNewObject_Impl,
		JVM_FreeMemory_Impl,
		JVM_GC_Impl,
		JVM_GCNoCompact_Impl,
		JVM_GetCallerClass_Impl,
		JVM_GetClassAccessFlags_Impl,
		JVM_GetClassContext_Impl,
		JVM_GetClassLoader_Impl,
		JVM_GetSystemPackage_Impl,
		JVM_GetSystemPackages_Impl,
		JVM_GetThreadInterruptEvent_Impl,
		JVM_Halt_Impl,
		JVM_InvokeMethod_Impl,
		JVM_LatestUserDefinedLoader_Impl,
		JVM_MaxObjectInspectionAge_Impl,
		JVM_NewInstanceFromConstructor_Impl,
		JVM_TotalMemory_Impl,
		JVM_MaxMemory_Impl,
		JVM_FindClassFromClassLoader_Impl,
		JVM_ExtendBootClassPath_Impl,
		JVM_GetClassTypeAnnotations_Impl,
		JVM_GetFieldTypeAnnotations_Impl,
		JVM_GetMethodTypeAnnotations_Impl,
		JVM_GetMethodParameters_Impl
		};


/**
 * Handle a GetEnv() request looking for the SUNVMI_VERSION_X_Y interface.
 */
static void
vmGetEnvHook(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData)
{
	J9VMGetEnvEvent *data = eventData;

	if (SUNVMI_VERSION_1_1 == data->version) {
		*(data->penv) = (void*)&VMIFunctionTable;
		data->rc = JNI_OK;
		return;
	}
}


/**
 * Called before class library initialization to allow the JVM interface layer to perform any
 * necessary initialization
 */
IDATA
SunVMI_LifecycleEvent(J9JavaVM* vm, IDATA stage, void* reserved)
{
	IDATA returnVal = J9VMDLLMAIN_OK;
	PORT_ACCESS_FROM_JAVAVM(vm);

	/* Now figure out why we are initializing */
	switch (stage) {

	case JCL_INITIALIZED:
	{
		J9HookInterface** vmHooks;
		IDATA anyErrors=-1;

		/* Register this module with trace */
		UT_MODULE_LOADED(J9_UTINTERFACE_FROM_VM(vm));

		/* Save the javaVM for future reference */
		VM.javaVM = vm;

		/* Look up thread functions */
		if (!initializeThreadFunctions(vm->portLibrary)) {
			return J9VMDLLMAIN_FAILED;
		}

		/* Register a hook to initialize reflection globals*/
		vmHooks = vm->internalVMFunctions->getVMHookInterface(vm);
		anyErrors = (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_INITIALIZE_REQUIRED_CLASSES_DONE, initializeReflectionGlobalsHook, OMR_GET_CALLSITE(), NULL);
		if (anyErrors) {
			return J9VMDLLMAIN_FAILED;
		}

		anyErrors = (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_GETENV, vmGetEnvHook, OMR_GET_CALLSITE(), NULL);
		if (anyErrors) {
			return J9VMDLLMAIN_FAILED;
		}
		break;
	}

	case VM_INITIALIZATION_COMPLETE:
	{
		/* the VM is now ready to go so setup the end GC hook we require for getting the time for the last time a GC ended */
		J9HookInterface** gcOmrHooks = vm->memoryManagerFunctions->j9gc_get_omr_hook_interface(vm->omrVM);
		if ((*gcOmrHooks)->J9HookRegisterWithCallSite(gcOmrHooks, J9HOOK_MM_OMR_GLOBAL_GC_END, gcDidComplete, OMR_GET_CALLSITE(), NULL)) {
			returnVal = J9VMDLLMAIN_FAILED;
		}
		break;
	}

	case INTERPRETER_SHUTDOWN:
	{
		j9sl_close_shared_library(VM.threadLibrary);
		VM.threadLibrary = 0;
		VM.monitorEnter = VM.monitorExit = NULL;
		break;
	}

	}
	return returnVal;
}
