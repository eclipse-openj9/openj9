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

#include "iohelp.h"
#include "j9.h"
#include "omrgcconsts.h"
#include "j9jclnls.h"
#include "j9port.h"
#include "j9protos.h"
#include "jclglob.h"
#include "jclprots.h"
#include "jni.h"
#include "ut_j9jcl.h"

/* require for new string merging primitives */
#include "stackwalk.h"
#include "omrlinkedlist.h"
#include "j9cp.h"
#include "rommeth.h"
#include "vmhook.h"
#include "HeapIteratorAPI.h"


typedef struct AllInstancesData {
	J9Class *clazz;             /* class being looked for */
	J9VMThread *vmThread;       
	J9IndexableObject *target;  /* the array */
	UDATA size;                 /* size of the array */
	UDATA storeIndex;           /* current index being stored */
	UDATA instanceCount;        /* count of instances found regardless if they are being collected in the array or not */
} AllInstancesData;

static UDATA hasConstructor(J9VMThread *vmThread, J9StackWalkState *state);
static jvmtiIterationControl collectInstances(J9JavaVM *vm, J9MM_IterateObjectDescriptor *objDesc, void *state);
static int hasActiveConstructor(J9VMThread *vmThread, J9Class *clazz);
static UDATA allInstances (JNIEnv * env, jclass clazz, jobjectArray target);


void JNICALL
Java_com_ibm_oti_vm_VM_localGC(JNIEnv *env, jclass clazz)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	vm->memoryManagerFunctions->j9gc_modron_local_collect(currentThread);
	vmFuncs->internalExitVMToJNI(currentThread);
}


void JNICALL
Java_com_ibm_oti_vm_VM_globalGC(JNIEnv *env, jclass clazz)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	vm->memoryManagerFunctions->j9gc_modron_global_collect(currentThread);
	vmFuncs->internalExitVMToJNI(currentThread);
}


jint JNICALL Java_com_ibm_oti_vm_VM_getClassPathCount(JNIEnv * env, jclass clazz)
{
#if defined(J9VM_OPT_DYNAMIC_LOAD_SUPPORT)
	J9JavaVM *javaVM;
	J9ClassLoader *classLoader;

	javaVM = ((J9VMThread *) env)->javaVM;
	classLoader = (J9ClassLoader*)javaVM->systemClassLoader;
	return (jint)classLoader->classPathEntryCount;
#else
	/* No dynamic loader. */
	return 0;
#endif
}





/* Java_test_StringCompress_allInstances
 * Fill in all the instances of clazz into target.
 * return the count of the instances in the system
 * if the count is > than the array size, there are more strings than fit.
 * if count is <= the array size, the array may have null references. 
 * static void allInstances (Class, Object[])
 */
jint JNICALL 
Java_com_ibm_oti_vm_VM_allInstances(JNIEnv * env, jclass unused, jclass clazz, jobjectArray target ) 
{
	jint count = 0;
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM *javaVM = vmThread->javaVM;
	
	if (OMR_GC_ALLOCATION_TYPE_SEGREGATED != javaVM->gcAllocationType) {
		UDATA savedGCFlags = 0;

		/* enterVMFromJNI(vmThread); */
		javaVM->internalVMFunctions->internalEnterVMFromJNI(vmThread);
		javaVM->internalVMFunctions->acquireExclusiveVMAccess(vmThread);

		savedGCFlags = vmThread->javaVM->requiredDebugAttributes & J9VM_DEBUG_ATTRIBUTE_ALLOW_USER_HEAP_WALK;
		if (savedGCFlags == 0) { /* if the flags was not set, you set it */
				vmThread->javaVM->requiredDebugAttributes |= J9VM_DEBUG_ATTRIBUTE_ALLOW_USER_HEAP_WALK;
		}
		javaVM->memoryManagerFunctions->j9gc_modron_global_collect(vmThread);
		if (savedGCFlags == 0) { /* if you set it, you have to unset it */
			vmThread->javaVM->requiredDebugAttributes &= ~J9VM_DEBUG_ATTRIBUTE_ALLOW_USER_HEAP_WALK;
		}
	 	count = (jint)allInstances(env, clazz, target);

		javaVM->internalVMFunctions->releaseExclusiveVMAccess(vmThread);
		javaVM->internalVMFunctions->internalExitVMToJNI(vmThread);
	}

	return count;
}


/* The string that keeps its original bytes is string1.
 * String2 has its bytes set to be string1-> bytes if the offsets already match and the bytes are not already set to the same value
 * The bytes being set already could happen frequently as this primitive will be used repeatedly to remerge strings in the runtime
*/
jint JNICALL 
Java_com_ibm_oti_vm_VM_setCommonData(JNIEnv * env, jclass unused, jobject string1, jobject string2 ) 
{
	UDATA allowMerge = 0;
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM *javaVM = vmThread->javaVM; 

	if (OMR_GC_ALLOCATION_TYPE_SEGREGATED != javaVM->gcAllocationType) {
		if (string1 == NULL || string2 == NULL) {
			allowMerge = 0;
		} else {
			j9object_t unwrappedString1 = NULL;
			j9object_t unwrappedString2 = NULL;
			j9object_t stringBytes1 = NULL;
			j9object_t stringBytes2 = NULL;

			allowMerge = 1;
			javaVM->internalVMFunctions->internalEnterVMFromJNI(vmThread);

			unwrappedString1 = J9_JNI_UNWRAP_REFERENCE(string1);
			unwrappedString2 = J9_JNI_UNWRAP_REFERENCE(string2);

			stringBytes1 = J9VMJAVALANGSTRING_VALUE(vmThread, unwrappedString1);
			stringBytes2 = J9VMJAVALANGSTRING_VALUE(vmThread, unwrappedString2);

			 /* skip merging of bytes if they are already the same. As this primitive is called repeatedly, then its likely the character data will be the same
			 * on repeat calls. 
			 */

			if (stringBytes1 == stringBytes2) {
				allowMerge = 0;
			}

			if (allowMerge) {
				/* set the duplicate string (string2) bytes to be (original) string1's bytes */
				J9VMJAVALANGSTRING_SET_VALUE(vmThread, unwrappedString2, stringBytes1);
			}

			javaVM->internalVMFunctions->internalExitVMToJNI(vmThread);
		}
	}

	
	return (jint)allowMerge;
}


static UDATA 
allInstances (JNIEnv * env, jclass clazz, jobjectArray target) 
{
	J9VMThread *vmThread = (J9VMThread *) env;
	AllInstancesData data;

	J9Class *unwrappedObj = clazz ? J9VM_J9CLASS_FROM_JCLASS(vmThread, clazz) : NULL;
	J9IndexableObject *unwrappedArray = target ? *(J9IndexableObject**)target : NULL;

	if (unwrappedObj == NULL) {
		return 0;
	}

	if (hasActiveConstructor (vmThread, unwrappedObj)) {
			/* printf ("\nFOUND A CONSTRUCTOR for the class, allInstances aborted"); */
		return 0;
	}

	data.clazz = unwrappedObj;
	data.target = unwrappedArray;
	data.vmThread = vmThread;
	if (unwrappedArray != NULL) {
		data.size =  J9INDEXABLEOBJECT_SIZE (vmThread, unwrappedArray);
	} else {
		data.size =  0;
	}
	data.storeIndex = 0;
	data.instanceCount = 0;

	vmThread->javaVM->memoryManagerFunctions->j9mm_iterate_all_objects(vmThread->javaVM, vmThread->javaVM->portLibrary, 0, collectInstances, &data);

	return data.instanceCount;
}


static jvmtiIterationControl
collectInstances (J9JavaVM *vm, J9MM_IterateObjectDescriptor *objDesc, void *state)
{
	AllInstancesData *data = (AllInstancesData *) state;
	j9object_t obj = objDesc->object;

	if (J9OBJECT_CLAZZ_VM(vm, obj) == data->clazz) {
		data->instanceCount++;
		 /* fill in the array only if one was passed in */
		if (data->target != NULL) { 
			 /* if no room, you can ignore the object and not store it */
			if (data->storeIndex < data->size) { 
 				J9JAVAARRAYOFOBJECT_STORE_VM(vm, data->target, (I_32)data->storeIndex, obj);
				data->storeIndex++;
			}
		}
	}
	return JVMTI_ITERATION_CONTINUE;
}


static int  
hasActiveConstructor(J9VMThread *vmThread, J9Class *clazz) 
{
	J9JavaVM *vm = vmThread->javaVM;
	J9VMThread * walkThread;

	walkThread = J9_LINKED_LIST_START_DO(vm->mainThread);
	while (walkThread != NULL) {
		J9StackWalkState walkState;
		walkState.walkThread = walkThread;
		walkState.flags = J9_STACKWALK_ITERATE_FRAMES;
		walkState.skipCount = 0;
		walkState.userData1 = (void*) clazz;
		walkState.userData2 = (void*) 0;
		walkState.frameWalkFunction = hasConstructor;

		vm->walkStackFrames(vmThread, &walkState);
		if (walkState.userData2 == (void*)1) {
			return 1;
		}
		walkThread = J9_LINKED_LIST_NEXT_DO(vm->mainThread, walkThread);
	}
	return 0;
}


static UDATA 
hasConstructor(J9VMThread *vmThread, J9StackWalkState *state)
{
	J9Method *method = state->method;
	J9Class *classToFind = (J9Class*) state->userData1;

	if (method == NULL) {
		return J9_STACKWALK_KEEP_ITERATING;
	} else {
		J9Class *methodClass = J9_CLASS_FROM_METHOD(method);
		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
		J9UTF8 *methodName = J9ROMMETHOD_GET_NAME(methodClass->romClass, romMethod);
	
		/* if the method is a constructor in the class we're looking for then quit looking for more
		 * a constructor is a non-static (to filter clinit) method which starts with '<'. 
		 */
		if (methodClass == classToFind) {
			if (!(romMethod->modifiers & J9AccStatic)) {
				if (J9UTF8_DATA(methodName)[0] == '<') {
					state->userData2 = (void*)1;
					return J9_STACKWALK_STOP_ITERATING;
				}
			}
		}
		return J9_STACKWALK_KEEP_ITERATING;
	}
}



/*
 * Dump a String to stderr using the port library.
 *
 * @param[in] str - a java.lang.String object
 */
void JNICALL
Java_com_ibm_oti_vm_VM_dumpString(JNIEnv * env, jclass clazz, jstring str)
{
	PORT_ACCESS_FROM_ENV(env);

	if (str == NULL) {
		j9tty_printf(PORTLIB, "null");
	} else {
		/* Note: GetStringUTFChars nul-terminates the resulting chars, even though the spec does not say so */
		const char * utfChars = (const char *) (*env)->GetStringUTFChars(env, str, NULL);

		if (utfChars != NULL) {
			Trc_JCL_com_ibm_oti_vm_VM_dumpString(env, utfChars);
			j9tty_printf(PORTLIB, "%s", utfChars);
			(*env)->ReleaseStringUTFChars(env, str, utfChars);
		}
	}
}
