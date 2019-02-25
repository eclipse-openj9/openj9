
/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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
#include "jcl.h"
#include "jclglob.h"
#include "jclprots.h"
#include "jcl_internal.h"
#include "objhelp.h"
#include "rommeth.h"
#include "vmaccess.h"
#include "VMHelpers.hpp"

extern "C" {
#define RETAIN_CLASS_REFERENCE 1
#define SHOW_REFLECT_FRAMES 2
#define SHOW_HIDDEN_FRAMES 4
#define FRAME_VALID 8
#define FRAME_FILTER_MASK (RETAIN_CLASS_REFERENCE | SHOW_REFLECT_FRAMES | SHOW_HIDDEN_FRAMES)

static UDATA stackFrameFilter(J9VMThread * currentThread, J9StackWalkState * walkState);

static UDATA
stackFrameFilter(J9VMThread * currentThread, J9StackWalkState * walkState)
{
	/*
	 * userData1 contains filtering flags, i.e. show hidden, show reflect.
	 * userData2 contains stackWalkerMethod, the method name to search for
	 */
	UDATA result = J9_STACKWALK_STOP_ITERATING;

	if (NULL != walkState->userData2) { /* look for stackWalkerMethod */
		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(walkState->method);
		J9ROMClass *romClass = J9_CLASS_FROM_METHOD(walkState->method)->romClass;

		J9UTF8 *utf = J9ROMMETHOD_GET_NAME(romClass, romMethod);
		const char  *stackWalkerMethod = (const char  *) walkState->userData2;
		result = J9_STACKWALK_KEEP_ITERATING;

		if (compareUTF8Length(J9UTF8_DATA(utf), J9UTF8_LENGTH(utf), (void *) stackWalkerMethod, strlen(stackWalkerMethod)) == 0) {
			utf = J9ROMCLASS_CLASSNAME(romClass);
			if (J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(utf), J9UTF8_LENGTH(utf), "java/lang/StackWalker")) {
				walkState->userData2 = NULL; /* Iteration will skip hidden frames and stop at the true caller of stackWalkerMethod. */
			}
		}
	} else if (J9_ARE_NO_BITS_SET((UDATA) (walkState->userData1), SHOW_REFLECT_FRAMES | SHOW_HIDDEN_FRAMES)
			&&	VM_VMHelpers::isReflectionMethod(currentThread, walkState->method)
	) {
		/* skip reflection/MethodHandleInvoke frames */
		result = J9_STACKWALK_KEEP_ITERATING;
	} else if (J9_ARE_NO_BITS_SET((UDATA) (walkState->userData1), SHOW_HIDDEN_FRAMES) && VM_VMHelpers::isHiddenMethod(walkState->method)
	) {
			result = J9_STACKWALK_KEEP_ITERATING;
	} else {
		result = J9_STACKWALK_STOP_ITERATING;
	}

	return result;
}

jobject JNICALL
Java_java_lang_StackWalker_walkWrapperImpl(JNIEnv *env, jclass clazz, jint flags, jstring stackWalkerMethod, jobject function)
{
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM *vm = vmThread->javaVM;

	J9StackWalkState newWalkState;
	J9StackWalkState *walkState = vmThread->stackWalkState;

	Assert_JCL_notNull (stackWalkerMethod);
	memset(&newWalkState, 0, sizeof(J9StackWalkState));
	newWalkState.previous = walkState;
	vmThread->stackWalkState = &newWalkState;
	walkState->walkThread = vmThread;
	walkState->flags = J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_WALK_TRANSLATE_PC
			|  J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_VISIBLE_ONLY;
	walkState->frameWalkFunction = stackFrameFilter;
	const char * walkerMethodChars = env->GetStringUTFChars(stackWalkerMethod, NULL);
	if (NULL == walkerMethodChars) { /* native out of memory exception pending */
		return NULL;
	}
	walkState->userData2 = (void *) walkerMethodChars;
	UDATA walkStateResult = vm->walkStackFrames(vmThread, walkState);
	Assert_JCL_true(walkStateResult == J9_STACKWALK_RC_NONE);
	walkState->flags |= J9_STACKWALK_RESUME;
	walkState->userData1 = (void *)(UDATA)flags;
	if (J9SF_FRAME_TYPE_END_OF_STACK != walkState->pc) {
		/* indicate the we have the topmost client method's frame */
		walkState->userData1 = (void *)((UDATA) walkState->userData1 | FRAME_VALID);
	}

	jmethodID walkImplMID = JCL_CACHE_GET(env, MID_java_lang_StackWalker_walkWrapperImpl);
	if (NULL == walkImplMID) {
		walkImplMID = env->GetStaticMethodID( clazz, "walkImpl", "(Ljava/util/function/Function;J)Ljava/lang/Object;");
		Assert_JCL_notNull (walkImplMID);
		JCL_CACHE_SET(env, MID_java_lang_StackWalker_walkWrapperImpl, walkImplMID);
	}
	jobject result = env->CallStaticObjectMethod(clazz, walkImplMID, function, (jlong)(UDATA)walkState);

	if (NULL != walkerMethodChars) {
		env->ReleaseStringUTFChars(stackWalkerMethod, walkerMethodChars);
	}
	vmThread->stackWalkState = newWalkState.previous;

	return result;
}

static j9object_t 
utfToStringObject(JNIEnv *env, J9UTF8 *utf, UDATA flags) {
	j9object_t result = NULL;
	if (NULL != utf) {
		J9VMThread *vmThread = (J9VMThread *) env;
		J9JavaVM *vm = vmThread->javaVM;
		result = vm->memoryManagerFunctions->j9gc_createJavaLangString(vmThread, J9UTF8_DATA(utf), (U_32) J9UTF8_LENGTH(utf), flags);
	}
	return result;
}

jobject JNICALL
Java_java_lang_StackWalker_getImpl(JNIEnv *env, jobject clazz, jlong walkStateP)
{
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM *vm = vmThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	J9StackWalkState *walkState = (J9StackWalkState *) ((UDATA) walkStateP);
	jobject result = NULL;

	enterVMFromJNI(vmThread);

	if (J9_ARE_NO_BITS_SET((UDATA) (walkState->userData1), FRAME_VALID)) {
		/* skip over the current frame */
		walkState->userData1 = (void *) ((UDATA)walkState->userData1 & FRAME_FILTER_MASK);
		if (J9_STACKWALK_RC_NONE != vm->walkStackFrames(vmThread, walkState)) {
			vmFuncs->setNativeOutOfMemoryError(vmThread, 0, 0);
			goto _done;
		}
	}
	/* clear the valid bit */
	walkState->userData1 = (void *) (((UDATA) walkState->userData1 & FRAME_FILTER_MASK));

	if (J9SF_FRAME_TYPE_END_OF_STACK != walkState->pc) {
		J9Class * frameClass = J9VMJAVALANGSTACKWALKERSTACKFRAMEIMPL_OR_NULL(vm);
		j9object_t frame = vm->memoryManagerFunctions->J9AllocateObject(vmThread, frameClass, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
		if (NULL == frame) {
			vmFuncs->setHeapOutOfMemoryError(vmThread);
		} else {
			J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(walkState->method);
			J9Class *ramClass = J9_CLASS_FROM_METHOD(walkState->method);
			J9ROMClass *romClass = ramClass->romClass;
			J9ClassLoader* classLoader = ramClass->classLoader;

			result = vmFuncs->j9jni_createLocalRef(env, frame);
			UDATA bytecodeOffset = walkState->bytecodePCOffset;  /* need this for StackFrame */
			UDATA lineNumber = getLineNumberForROMClassFromROMMethod(vm, romMethod, romClass, classLoader, bytecodeOffset);
			PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, frame);

			/* set the class object if requested */
			if (J9_ARE_ANY_BITS_SET((UDATA) walkState->userData1, RETAIN_CLASS_REFERENCE)) {
				j9object_t classObject =J9VM_J9CLASS_TO_HEAPCLASS(ramClass);
				J9VMJAVALANGSTACKWALKERSTACKFRAMEIMPL_SET_DECLARINGCLASS(vmThread, frame, classObject);
			}

			/* set bytecode index */
			J9VMJAVALANGSTACKWALKERSTACKFRAMEIMPL_SET_BYTECODEINDEX(vmThread, frame, (U_32) bytecodeOffset);

			/* Fill in line number - Java wants -2 for natives, -1 for no line number (which will be 0 coming in from the iterator) */

			if (J9_ARE_ANY_BITS_SET(romMethod->modifiers, J9AccNative)) {
				lineNumber = -2;
			} else if (lineNumber == 0) {
				lineNumber = -1;
			}
			J9VMJAVALANGSTACKWALKERSTACKFRAMEIMPL_SET_LINENUMBER(vmThread, frame, (I_32) lineNumber);

			UDATA flags = J9_STR_XLAT;
			if (J9_ARE_ALL_BITS_SET(romClass->extraModifiers, J9AccClassAnonClass)) {
				flags |= J9_STR_ANON_CLASS_NAME;
			}

			j9object_t stringObject = J9VMJAVALANGCLASSLOADER_CLASSLOADERNAME(vmThread, classLoader->classLoaderObject);
			J9VMJAVALANGSTACKWALKERSTACKFRAMEIMPL_SET_CLASSLOADERNAME(vmThread, frame, stringObject);

			J9UTF8 *nameUTF =  J9ROMCLASS_CLASSNAME(romClass);
			J9Module *module = ramClass->module;
			if (NULL != module) {
				J9VMJAVALANGSTACKWALKERSTACKFRAMEIMPL_SET_FRAMEMODULE(vmThread, frame, module->moduleObject);
			}

			stringObject = utfToStringObject(env, nameUTF, flags);
			if (VM_VMHelpers::exceptionPending(vmThread)) {
				goto _pop_frame;
			}
			J9VMJAVALANGSTACKWALKERSTACKFRAMEIMPL_SET_CLASSNAME(vmThread, PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0), stringObject);

			stringObject = utfToStringObject(env, J9ROMMETHOD_GET_NAME(romClass, romMethod), J9_STR_INTERN);
			if (VM_VMHelpers::exceptionPending(vmThread)) {
				goto _pop_frame;
			}
			J9VMJAVALANGSTACKWALKERSTACKFRAMEIMPL_SET_METHODNAME(vmThread, PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0), stringObject);

			stringObject = utfToStringObject(env, J9ROMMETHOD_GET_SIGNATURE(romClass, romMethod), J9_STR_INTERN);
			if (VM_VMHelpers::exceptionPending(vmThread)) {
				goto _pop_frame;
			}
			J9VMJAVALANGSTACKWALKERSTACKFRAMEIMPL_SET_METHODSIGNATURE(vmThread, PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0), stringObject);

			stringObject = utfToStringObject(env, getSourceFileNameForROMClass(vm, classLoader, romClass), J9_STR_INTERN);
			if (VM_VMHelpers::exceptionPending(vmThread)) {
				goto _pop_frame;
			}
			J9VMJAVALANGSTACKWALKERSTACKFRAMEIMPL_SET_FILENAME(vmThread, PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0), stringObject);

			if (J9ROMMETHOD_IS_CALLER_SENSITIVE(romMethod)) {
				J9VMJAVALANGSTACKWALKERSTACKFRAMEIMPL_SET_CALLERSENSITIVE(vmThread, PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0), TRUE);
			}

_pop_frame:
			DROP_OBJECT_IN_SPECIAL_FRAME(vmThread);
		}
	}
_done:
	exitVMToJNI(vmThread);

	return result;
}
}
