/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#ifndef jvmti_internal_h
#define jvmti_internal_h

/* @ddr_namespace: default */
/**
* @file jvmti_internal.h
* @brief Internal prototypes used within the JVMTI module.
*
* This file contains implementation-private function prototypes and
* type definitions for the JVMTI module.
*
*/

#include "j9.h"
#include "j9comp.h"
#include "jni.h"
#include "jvmti.h"
#include "jvmtiInternal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- jvmtiBreakpoint.c ---------------- */

/**
* @brief
* @param env
* @param method
* @param location
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiClearBreakpoint(jvmtiEnv* env,
	jmethodID method,
	jlocation location);


/**
* @brief
* @param env
* @param method
* @param location
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiSetBreakpoint(jvmtiEnv* env,
	jmethodID method,
	jlocation location);


/* ---------------- jvmtiCapability.c ---------------- */

/**
* @brief
* @param env
* @param capabilities_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiAddCapabilities(jvmtiEnv* env,
	const jvmtiCapabilities* capabilities_ptr);


/**
* @brief
* @param env
* @param capabilities_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetCapabilities(jvmtiEnv* env,
	jvmtiCapabilities* capabilities_ptr);


/**
* @brief
* @param env
* @param capabilities_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetPotentialCapabilities(jvmtiEnv* env,
	jvmtiCapabilities* capabilities_ptr);


/**
* @brief
* @param env
* @param capabilities_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiRelinquishCapabilities(jvmtiEnv* env,
	const jvmtiCapabilities* capabilities_ptr);


/* ---------------- jvmtiClass.c ---------------- */

/**
* @brief
* @param env
* @param klass
* @param field_count_ptr
* @param fields_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetClassFields(jvmtiEnv* env,
	jclass klass,
	jint* field_count_ptr,
	jfieldID** fields_ptr);


/**
* @brief
* @param env
* @param klass
* @param classloader_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetClassLoader(jvmtiEnv* env,
	jclass klass,
	jobject* classloader_ptr);


/**
* @brief
* @param env
* @param initiating_loader
* @param class_count_ptr
* @param classes_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetClassLoaderClasses(jvmtiEnv* env,
	jobject initiating_loader,
	jint* class_count_ptr,
	jclass** classes_ptr);


/**
* @brief
* @param env
* @param klass
* @param method_count_ptr
* @param methods_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetClassMethods(jvmtiEnv* env,
	jclass klass,
	jint* method_count_ptr,
	jmethodID** methods_ptr);


/**
* @brief
* @param env
* @param klass
* @param modifiers_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetClassModifiers(jvmtiEnv* env,
	jclass klass,
	jint* modifiers_ptr);


/**
* @brief
* @param env
* @param klass
* @param signature_ptr
* @param generic_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetClassSignature(jvmtiEnv* env,
	jclass klass,
	char** signature_ptr,
	char** generic_ptr);


/**
* @brief
* @param env
* @param klass
* @param status_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetClassStatus(jvmtiEnv* env,
	jclass klass,
	jint* status_ptr);


/**
* @brief
* @param env
* @param klass
* @param minor_version_ptr
* @param major_version_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetClassVersionNumbers(jvmtiEnv* env,
	jclass klass,
	jint* minor_version_ptr,
	jint* major_version_ptr);


/**
* @brief
* @param env
* @param klass
* @param constant_pool_count_ptr
* @param constant_pool_byte_count_ptr
* @param constant_pool_bytes_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetConstantPool(jvmtiEnv* env,
	jclass klass,
	jint* constant_pool_count_ptr,
	jint* constant_pool_byte_count_ptr,
	unsigned char** constant_pool_bytes_ptr);

/** 
 * \brief 
 * \ingroup 
 * 
 * @param privatePortLibrary 
 * @param translation 
 * @param class 
 * @return jvmtiError
 * 
 */
jvmtiError 
jvmtiGetConstantPool_translateCP(J9PortLibrary *privatePortLibrary, 
				 jvmtiGcp_translation *translation,
				 J9Class *clazz,
				 jboolean translateUTF8andNAS);

/** 
 * \brief 
 * \ingroup 
 * 
 * @param privatePortLibrary 
 * @param translation 
 * 
 */
void
jvmtiGetConstantPool_free(J9PortLibrary *privatePortLibrary, 
			  jvmtiGcp_translation *translation);

/**
* @brief
* @param env
* @param klass
* @param interface_count_ptr
* @param interfaces_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetImplementedInterfaces(jvmtiEnv* env,
	jclass klass,
	jint* interface_count_ptr,
	jclass** interfaces_ptr);


/**
* @brief
* @param env
* @param class_count_ptr
* @param classes_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetLoadedClasses(jvmtiEnv* env,
	jint* class_count_ptr,
	jclass** classes_ptr);


/**
* @brief
* @param env
* @param klass
* @param source_debug_extension_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetSourceDebugExtension(jvmtiEnv* env,
	jclass klass,
	char** source_debug_extension_ptr);


/**
* @brief
* @param env
* @param klass
* @param source_name_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetSourceFileName(jvmtiEnv* env,
	jclass klass,
	char** source_name_ptr);


/**
* @brief
* @param env
* @param klass
* @param is_array_class_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiIsArrayClass(jvmtiEnv* env,
	jclass klass,
	jboolean* is_array_class_ptr);


/**
* @brief
* @param env
* @param klass
* @param is_interface_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiIsInterface(jvmtiEnv* env,
	jclass klass,
	jboolean* is_interface_ptr);


/**
* @brief
* @param env
* @param klass
* @param is_modifiable_class_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiIsModifiableClass(jvmtiEnv* env,
	jclass klass,
	jboolean* is_modifiable_class_ptr);


/**
* @brief
* @param env
* @param class_count
* @param class_definitions
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiRedefineClasses(jvmtiEnv* env,
	jint class_count,
	const jvmtiClassDefinition* class_definitions);


/**
* @brief
* @param env
* @param class_count
* @param classes
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiRetransformClasses(jvmtiEnv* env,
	jint class_count,
	const jclass* classes);


/* ---------------- jvmtiClassLoaderSearch.c ---------------- */

/**
* @brief
* @param env
* @param segment
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiAddToBootstrapClassLoaderSearch(jvmtiEnv* env,
	const char* segment);


/**
* @brief
* @param env
* @param segment
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiAddToSystemClassLoaderSearch(jvmtiEnv* env,
	const char* segment);


/* ---------------- jvmtiEventManagement.c ---------------- */

/**
* @brief
* @param env
* @param event_type
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGenerateEvents(jvmtiEnv* env,
	jvmtiEvent event_type);


/**
* @brief
* @param env
* @param callbacks
* @param size_of_callbacks
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiSetEventCallbacks(jvmtiEnv* env,
	const jvmtiEventCallbacks* callbacks,
	jint size_of_callbacks);


/**
* @brief
* @param env
* @param mode
* @param event_type
* @param event_thread
* @param ...
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiSetEventNotificationMode(jvmtiEnv* env,
	jvmtiEventMode mode,
	jvmtiEvent event_type,
	jthread event_thread,
	...);


/* ---------------- jvmtiExtensionMechanism.c ---------------- */

/**
* @brief
* @param env
* @param extension_count_ptr
* @param extensions
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetExtensionEvents(jvmtiEnv* env,
	jint* extension_count_ptr,
	jvmtiExtensionEventInfo** extensions);


/**
* @brief
* @param env
* @param extension_count_ptr
* @param extensions
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetExtensionFunctions(jvmtiEnv* env,
	jint* extension_count_ptr,
	jvmtiExtensionFunctionInfo** extensions);


/**
* @brief
* @param env
* @param extension_event_index
* @param callback
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiSetExtensionEventCallback(jvmtiEnv* env,
	jint extension_event_index,
	jvmtiExtensionEvent callback);


/* ---------------- jvmtiField.c ---------------- */

/**
* @brief
* @param env
* @param klass
* @param field
* @param declaring_class_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetFieldDeclaringClass(jvmtiEnv* env,
	jclass klass,
	jfieldID field,
	jclass* declaring_class_ptr);


/**
* @brief
* @param env
* @param klass
* @param field
* @param modifiers_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetFieldModifiers(jvmtiEnv* env,
	jclass klass,
	jfieldID field,
	jint* modifiers_ptr);


/**
* @brief
* @param env
* @param klass
* @param field
* @param name_ptr
* @param signature_ptr
* @param generic_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetFieldName(jvmtiEnv* env,
	jclass klass,
	jfieldID field,
	char** name_ptr,
	char** signature_ptr,
	char** generic_ptr);


/**
* @brief
* @param env
* @param klass
* @param field
* @param is_synthetic_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiIsFieldSynthetic(jvmtiEnv* env,
	jclass klass,
	jfieldID field,
	jboolean* is_synthetic_ptr);


/* ---------------- jvmtiForceEarlyReturn.c ---------------- */

/**
* @brief
* @param env
* @param thread
* @param value
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiForceEarlyReturnDouble(jvmtiEnv* env,
	jthread thread,
	jdouble value);


/**
* @brief
* @param env
* @param thread
* @param value
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiForceEarlyReturnFloat(jvmtiEnv* env,
	jthread thread,
	jfloat value);


/**
* @brief
* @param env
* @param thread
* @param value
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiForceEarlyReturnInt(jvmtiEnv* env,
	jthread thread,
	jint value);


/**
* @brief
* @param env
* @param thread
* @param value
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiForceEarlyReturnLong(jvmtiEnv* env,
	jthread thread,
	jlong value);


/**
* @brief
* @param env
* @param thread
* @param value
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiForceEarlyReturnObject(jvmtiEnv* env,
	jthread thread,
	jobject value);


/**
* @brief
* @param env
* @param thread
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiForceEarlyReturnVoid(jvmtiEnv* env,
	jthread thread);


/* ---------------- jvmtiGeneral.c ---------------- */

/**
* @brief
* @param env
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiDisposeEnvironment(jvmtiEnv* env);


/**
* @brief
* @param env
* @param data_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetEnvironmentLocalStorage(jvmtiEnv* env,
	void** data_ptr);


/**
* @brief
* @param env
* @param error
* @param name_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetErrorName(jvmtiEnv* env,
	jvmtiError error,
	char** name_ptr);


/**
* @brief
* @param env
* @param format_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetJLocationFormat(jvmtiEnv* env,
	jvmtiJlocationFormat* format_ptr);


/**
* @brief
* @param env
* @param phase_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetPhase(jvmtiEnv* env,
	jvmtiPhase* phase_ptr);


/**
* @brief
* @param env
* @param version_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetVersionNumber(jvmtiEnv* env,
	jint* version_ptr);


/**
* @brief
* @param env
* @param data
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiSetEnvironmentLocalStorage(jvmtiEnv* env,
	const void* data);


/**
* @brief
* @param env
* @param flag
* @param value
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiSetVerboseFlag(jvmtiEnv* env,
	jvmtiVerboseFlag flag,
	jboolean value);


/* ---------------- jvmtiHeap.c ---------------- */

/**
* @brief
* @param env
* @param heap_filter
* @param klass
* @param initial_object
* @param callbacks
* @param user_data
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiFollowReferences(jvmtiEnv* env,
	jint heap_filter,
	jclass klass,
	jobject initial_object,
	const jvmtiHeapCallbacks* callbacks,
	const void* user_data);


/**
* @brief
* @param env
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiForceGarbageCollection(jvmtiEnv* env);


/**
* @brief
* @param env
* @param tag_count
* @param tags
* @param count_ptr
* @param object_result_ptr
* @param tag_result_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetObjectsWithTags(jvmtiEnv* env,
	jint tag_count,
	const jlong* tags,
	jint* count_ptr,
	jobject** object_result_ptr,
	jlong** tag_result_ptr);


/**
* @brief
* @param env
* @param object
* @param tag_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetTag(jvmtiEnv* env,
	jobject object,
	jlong* tag_ptr);


/**
* @brief
* @param env
* @param heap_filter
* @param klass
* @param callbacks
* @param user_data
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiIterateThroughHeap(jvmtiEnv* env,
	jint heap_filter,
	jclass klass,
	const jvmtiHeapCallbacks* callbacks,
	const void* user_data);


/**
* @brief
* @param env
* @param object
* @param tag
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiSetTag(jvmtiEnv* env,
	jobject object,
	jlong tag);


/* ---------------- jvmtiHeap10.c ---------------- */

/**
* @brief
* @param env
* @param object_filter
* @param heap_object_callback
* @param user_data
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiIterateOverHeap(jvmtiEnv* env,
	jvmtiHeapObjectFilter object_filter,
	jvmtiHeapObjectCallback heap_object_callback,
	const void * user_data);


/**
* @brief
* @param env
* @param klass
* @param object_filter
* @param heap_object_callback
* @param user_data
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiIterateOverInstancesOfClass(jvmtiEnv* env,
	jclass klass,
	jvmtiHeapObjectFilter object_filter,
	jvmtiHeapObjectCallback heap_object_callback,
	const void * user_data);


/**
* @brief
* @param env
* @param object
* @param object_reference_callback
* @param user_data
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiIterateOverObjectsReachableFromObject(jvmtiEnv* env,
	jobject object,
	jvmtiObjectReferenceCallback object_reference_callback,
	const void * user_data);


/**
* @brief
* @param env
* @param heap_root_callback
* @param stack_ref_callback
* @param object_ref_callback
* @param user_data
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiIterateOverReachableObjects(jvmtiEnv* env,
	jvmtiHeapRootCallback heap_root_callback,
	jvmtiStackReferenceCallback stack_ref_callback,
	jvmtiObjectReferenceCallback object_ref_callback,
	const void * user_data);


/* ---------------- jvmtiHelpers.c ---------------- */

/**
* @brief Make the heap walkable, assume exclusive VM access is held
* @param currentThread The current J9VMThread
* @return void
*/
void
ensureHeapWalkable(J9VMThread *currentThread);


/**
* @brief
* @param state
* @return J9JVMTIAgentBreakpoint *
*/
J9JVMTIAgentBreakpoint *
allAgentBreakpointsNextDo(J9JVMTIAgentBreakpointDoState * state);


/**
* @brief
* @param jvmtiData
* @param state
* @return J9JVMTIAgentBreakpoint *
*/
J9JVMTIAgentBreakpoint *
allAgentBreakpointsStartDo(J9JVMTIData * jvmtiData, J9JVMTIAgentBreakpointDoState * state);


/**
* @brief
* @param invocationJavaVM
* @param version
* @param penv
* @return jint
*/
jint
allocateEnvironment(J9InvocationJavaVM * invocationJavaVM, jint version, void ** penv);


/**
* @brief
* @param j9env
* @param vmThread
* @return jvmtiError
*/
jvmtiError
createThreadData(J9JVMTIEnv * j9env, J9VMThread * vmThread);


/**
* @brief
* @param env
* @param utf
* @param cString
* @return jvmtiError
*/
jvmtiError
cStringFromUTF(jvmtiEnv * env, J9UTF8 * utf, char ** cString);


/**
* @brief
* @param env
* @param data
* @param length
* @param cString
* @return jvmtiError
*/
jvmtiError
cStringFromUTFChars(jvmtiEnv * env, U_8 * data, UDATA length, char ** cString);


/**
* @brief
* @param currentThread
* @param j9env
* @param agentBreakpoint
* @return void
*/
void
deleteAgentBreakpoint(J9VMThread * currentThread, J9JVMTIEnv * j9env, J9JVMTIAgentBreakpoint * agentBreakpoint);


/**
* @brief
* @param j9env
* @param freeData
* @return void
*/
void
disposeEnvironment(J9JVMTIEnv * j9env, UDATA freeData);


/**
* @brief
* @param signatureType
* @param jvaluePtr
* @param valueAddress
* @param objectStorage
* @return void
*/
void
fillInJValue(char signatureType, jvalue * jvaluePtr, void * valueAddress, j9object_t * objectStorage);


/** 
 * \brief Get a primitive type for the supplied signature. 
 * \ingroup jvmti.helpers
 * 
 * @param[in]	signature 	signature of the data
 * @param[out]	primitiveType	primitive type 
 * @return JVMTI_ERROR_NONE on success or JVMTI_ERROR_ILLEGAL_ARGUMENT if signature does not describe a primitive type
 *	
 */
jvmtiError 
getPrimitiveType(J9UTF8 *signature, jvmtiPrimitiveType *primitiveType);


/**
* @brief
* @param currentThread
* @param j9env
* @param ramMethod
* @param location
* @return J9JVMTIAgentBreakpoint *
*/
J9JVMTIAgentBreakpoint *
findAgentBreakpoint(J9VMThread * currentThread, J9JVMTIEnv * j9env, J9Method * ramMethod, IDATA location);


/**
* @brief
* @param jvmtiData
* @param ramMethod
* @return J9JVMTIBreakpointedMethod *
*/
J9JVMTIBreakpointedMethod *
findBreakpointedMethod(J9JVMTIData * jvmtiData, J9Method * ramMethod);


/**
* @brief
* @param currentThread
* @param eventNumber
* @param hadVMAccess
* @param javaOffloadOldState
* @return void
*/
void
finishedEvent(J9VMThread * currentThread, UDATA eventNumber, UDATA hadVMAccess, UDATA javaOffloadOldState);


#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
/**
 * Switch onto the zAAP processor if not running there after native finishes running on GP.
 *
 * @param[in] *currentThread - current thread
 * @param[in] eventNumber - the event ID
 * @param[in] javaOffloadOldState - the old offload state of current thread
 */
void
javaOffloadSwitchOn(J9VMThread * currentThread, UDATA eventNumber, UDATA javaOffloadOldState);
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */


/**
* @brief
* @param clazz
* @return J9Class *
*/
J9Class *
getCurrentClass(J9Class * clazz);


/**
* @brief
* @param currentThread
* @param method
* @return jmethodID
*/
jmethodID
getCurrentMethodID(J9VMThread * currentThread, J9Method * method);


/**
* @brief
* @param vm
* @param currentThread
* @return jvmtiError
*/
jvmtiError
getCurrentVMThread(J9JavaVM * vm, J9VMThread ** currentThread);


/**
* @brief
* @param ref
* @return j9object_t 
*/
j9object_t
getObjectFromRef(j9object_t * ref);


/**
* @brief
* @param *vm
* @param obj
* @return jlong
*/
jlong
getObjectSize(J9JavaVM *vm, j9object_t obj);


/**
* @brief
* @param *currentThread
* @param threadObject
* @return jint
*/
jint
getThreadState(J9VMThread *currentThread, j9object_t threadObject);


/**
* @brief
* @param currentThread
* @param thread
* @param vmThreadPtr
* @param allowNull
* @param mustBeAlive
* @return jvmtiError
*/
jvmtiError
getVMThread(J9VMThread * currentThread, jthread thread, J9VMThread ** vmThreadPtr, UDATA allowNull, UDATA mustBeAlive);


/**
* @brief
* @param currentThread
* @param agentBreakpoint
* @return jvmtiError
*/
jvmtiError
installAgentBreakpoint(J9VMThread * currentThread, J9JVMTIAgentBreakpoint * agentBreakpoint);


/**
* @brief
* @param pUtfData
* @return U_16
*/
U_16
nextUTFChar(U_8 ** pUtfData);


/**
* @brief
* @param j9env
* @param currentThread
* @param eventThread
* @param eventNumber
* @param threadRefPtr
* @param hadVMAccessPtr
* @param wantVMAccess
* @param jniRefSlots
* @param javaOffloadOldState
* @return UDATA
*/
UDATA
prepareForEvent(J9JVMTIEnv * j9env, J9VMThread * currentThread, J9VMThread * eventThread, UDATA eventNumber, jthread * threadRefPtr, UDATA * hadVMAccessPtr, UDATA wantVMAccess, UDATA jniRefSlots, UDATA * javaOffloadOldState);


#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
/**
 * Switch away from the zAAP processor if running there.
 *
 * @param[in] *j9env - pointer the current JVMTI environment
 * @param[in] *currentThread - current thread
 * @param[in] eventNumber - the event ID
 * @param[in] *javaOffloadOldState - the old offload state of current thread
 */
void
javaOffloadSwitchOff(J9JVMTIEnv * j9env, J9VMThread * currentThread, UDATA eventNumber, UDATA * javaOffloadOldState);
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */


#if defined(J9VM_INTERP_NATIVE_SUPPORT) 
/**
* @brief
* @param jvmtiData
* @param methodID
* @param startPC
* @param length
* @param metaData
* @param isLoad
* @return J9JVMTICompileEvent *
*/
J9JVMTICompileEvent *
queueCompileEvent(J9JVMTIData * jvmtiData, jmethodID methodID, void * startPC, UDATA length, void * metaData, UDATA isLoad);
#endif /* INTERP_NATIVE_SUPPORT */


/**
* @brief
* @param currentThread
* @param targetThread
* @return void
*/
void
releaseVMThread(J9VMThread * currentThread, J9VMThread * targetThread);


/**
* @brief
* @param j9env
* @param currentThread
* @param mode
* @param event_type
* @param event_thread
* @param low
* @param high
* @return jvmtiError
*/
jvmtiError
setEventNotificationMode(J9JVMTIEnv * j9env, J9VMThread * currentThread, jint mode, jint event_type, jthread event_thread, jint low, jint high);


/**
* @brief
* @param pUtfData
* @return void
*/
void
skipSignature(U_8 ** pUtfData);


/**
* @brief
* @param currentThread
* @param agentBreakpoint
* @return void
*/
void
suspendAgentBreakpoint(J9VMThread * currentThread, J9JVMTIAgentBreakpoint * agentBreakpoint);

/**
 * Locate information for decompilation.
 *
 * After the walk is complete, walkState->userData1 contains the JVMTI error code.  If it's
 * JVMTI_ERROR_NONE, the frame was located and the following information from the located frame
 * is filled in:
 *
 *	walkState->userData2 is the inline depth
 *	walkState->userData3 is the method
 *	walkState->userData4 is the bytecodePCOffset
 *
 * On success, the walkState is at the outer frame for the decompilation.
 *
 * @param[in] *currentThread current thread
 * @param[in] *targetThread the thread to walk
 * @param[in] depth the frame depth
 * @param[in] *walkState a stack walk state
 *
 * @return a JVMTI error code
 */

UDATA
findDecompileInfo(J9VMThread *currentThread, J9VMThread *targetThread, UDATA depth, J9StackWalkState *walkState);

/* ---------------- jvmtiHook.c ---------------- */

/**
* @brief
* @param j9env
* @param attribute
* @return IDATA
*/
IDATA
enableDebugAttribute(J9JVMTIEnv * j9env, UDATA attribute);


/**
* @brief
* @param j9env
* @param event
* @return IDATA
*/
IDATA
hookEvent(J9JVMTIEnv * j9env, jint event);


/**
* @brief
* @param jvmtiData
* @return IDATA
*/
IDATA
hookGlobalEvents(J9JVMTIData * jvmtiData);


/**
* @brief
* @param j9env
* @param capabilities
* @return IDATA
*/
IDATA
hookNonEventCapabilities(J9JVMTIEnv * j9env, jvmtiCapabilities * capabilities);


/**
* @brief
* @param j9env
* @return IDATA
*/
IDATA
hookRequiredEvents(J9JVMTIEnv * j9env);


/**
* @brief
* @param j9env
* @param event
* @return UDATA
*/
UDATA
isEventHookable(J9JVMTIEnv * j9env, jvmtiEvent event);


/**
* @brief
* @param j9env
* @param event
* @return IDATA
*/
IDATA
reserveEvent(J9JVMTIEnv * j9env, jint event);


#if defined(J9VM_INTERP_NATIVE_SUPPORT)
/**
* @brief
* @param jvmtiData
* @return jvmtiError
*/
jvmtiError
startCompileEventThread(J9JVMTIData * jvmtiData);
#endif /* INTERP_NATIVE_SUPPORT */


/**
* @brief
* @param j9env
* @return void
*/
void
unhookAllEvents(J9JVMTIEnv * j9env);


/**
* @brief
* @param j9env
* @param event
* @return IDATA
*/
IDATA
unhookEvent(J9JVMTIEnv * j9env, jint event);


/**
* @brief
* @param jvmtiData
* @return void
*/
void
unhookGlobalEvents(J9JVMTIData * jvmtiData);


/* ---------------- jvmtiJNIFunctionInterception.c ---------------- */

/**
* @brief
* @param env
* @param function_table
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetJNIFunctionTable(jvmtiEnv* env,
	jniNativeInterface** function_table);


/**
* @brief
* @param env
* @param function_table
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiSetJNIFunctionTable(jvmtiEnv* env,
	const jniNativeInterface* function_table);


/* ---------------- jvmtiLocalVariable.c ---------------- */

/**
* @brief
* @param env
* @param thread
* @param depth
* @param slot
* @param value_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetLocalDouble(jvmtiEnv* env,
	jthread thread,
	jint depth,
	jint slot,
	jdouble* value_ptr);


/**
* @brief
* @param env
* @param thread
* @param depth
* @param slot
* @param value_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetLocalFloat(jvmtiEnv* env,
	jthread thread,
	jint depth,
	jint slot,
	jfloat* value_ptr);


/**
* @brief
* @param env
* @param thread
* @param depth
* @param slot
* @param value_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetLocalInt(jvmtiEnv* env,
	jthread thread,
	jint depth,
	jint slot,
	jint* value_ptr);


/**
* @brief
* @param env
* @param thread
* @param depth
* @param slot
* @param value_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetLocalLong(jvmtiEnv* env,
	jthread thread,
	jint depth,
	jint slot,
	jlong* value_ptr);


/**
* @brief
* @param env
* @param thread
* @param depth
* @param slot
* @param value_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetLocalObject(jvmtiEnv* env,
	jthread thread,
	jint depth,
	jint slot,
	jobject* value_ptr);


/**
* @brief
* @param env
* @param thread
* @param depth
* @param slot
* @param value_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetLocalInstance(jvmtiEnv* env,
	jthread thread,
	jint depth,
	jobject* value_ptr);

/**
* @brief Set the allocation sampling interval
* @param env The JVMTI environment pointer.
* @param samplingInterval The sampling interval in bytes.
* @return jvmtiError Error code returned by JVMTI function
*/
#if JAVA_SPEC_VERSION >= 11
jvmtiError JNICALL 
jvmtiSetHeapSamplingInterval(jvmtiEnv *env,
	jint samplingInterval);
#endif /* JAVA_SPEC_VERSION >= 11 */

/**
* @brief
* @param env
* @param thread
* @param depth
* @param slot
* @param value
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiSetLocalDouble(jvmtiEnv* env,
	jthread thread,
	jint depth,
	jint slot,
	jdouble value);


/**
* @brief
* @param env
* @param thread
* @param depth
* @param slot
* @param value
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiSetLocalFloat(jvmtiEnv* env,
	jthread thread,
	jint depth,
	jint slot,
	jfloat value);


/**
* @brief
* @param env
* @param thread
* @param depth
* @param slot
* @param value
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiSetLocalInt(jvmtiEnv* env,
	jthread thread,
	jint depth,
	jint slot,
	jint value);


/**
* @brief
* @param env
* @param thread
* @param depth
* @param slot
* @param value
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiSetLocalLong(jvmtiEnv* env,
	jthread thread,
	jint depth,
	jint slot,
	jlong value);


/**
* @brief
* @param env
* @param thread
* @param depth
* @param slot
* @param value
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiSetLocalObject(jvmtiEnv* env,
	jthread thread,
	jint depth,
	jint slot,
	jobject value);


/* ---------------- jvmtiMemory.c ---------------- */

/**
* @brief
* @param env
* @param size
* @param mem_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiAllocate(jvmtiEnv* env,
	jlong size,
	unsigned char** mem_ptr);


/**
* @brief
* @param env
* @param mem
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiDeallocate(jvmtiEnv* env,
	unsigned char* mem);


/* ---------------- jvmtiMethod.c ---------------- */

/**
* @brief
* @param env
* @param method
* @param size_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetArgumentsSize(jvmtiEnv* env,
	jmethodID method,
	jint* size_ptr);


/**
* @brief
* @param env
* @param method
* @param bytecode_count_ptr
* @param bytecodes_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetBytecodes(jvmtiEnv* env,
	jmethodID method,
	jint* bytecode_count_ptr,
	unsigned char** bytecodes_ptr);


/**
* @brief
* @param env
* @param method
* @param entry_count_ptr
* @param table_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetLineNumberTable(jvmtiEnv* env,
	jmethodID method,
	jint* entry_count_ptr,
	jvmtiLineNumberEntry** table_ptr);


/**
* @brief
* @param env
* @param method
* @param entry_count_ptr
* @param table_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetLocalVariableTable(jvmtiEnv* env,
	jmethodID method,
	jint* entry_count_ptr,
	jvmtiLocalVariableEntry** table_ptr);


/**
* @brief
* @param env
* @param method
* @param max_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetMaxLocals(jvmtiEnv* env,
	jmethodID method,
	jint* max_ptr);


/**
* @brief
* @param env
* @param method
* @param declaring_class_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetMethodDeclaringClass(jvmtiEnv* env,
	jmethodID method,
	jclass* declaring_class_ptr);


/**
* @brief
* @param env
* @param method
* @param start_location_ptr
* @param end_location_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetMethodLocation(jvmtiEnv* env,
	jmethodID method,
	jlocation* start_location_ptr,
	jlocation* end_location_ptr);


/**
* @brief
* @param env
* @param method
* @param modifiers_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetMethodModifiers(jvmtiEnv* env,
	jmethodID method,
	jint* modifiers_ptr);


/**
* @brief
* @param env
* @param method
* @param name_ptr
* @param signature_ptr
* @param generic_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetMethodName(jvmtiEnv* env,
	jmethodID method,
	char** name_ptr,
	char** signature_ptr,
	char** generic_ptr);


/**
* @brief
* @param env
* @param method
* @param is_native_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiIsMethodNative(jvmtiEnv* env,
	jmethodID method,
	jboolean* is_native_ptr);


/**
* @brief
* @param env
* @param method
* @param is_obsolete_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiIsMethodObsolete(jvmtiEnv* env,
	jmethodID method,
	jboolean* is_obsolete_ptr);


/**
* @brief
* @param env
* @param method
* @param is_synthetic_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiIsMethodSynthetic(jvmtiEnv* env,
	jmethodID method,
	jboolean* is_synthetic_ptr);


/**
* @brief
* @param env
* @param prefix
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiSetNativeMethodPrefix(jvmtiEnv* env,
	const char* prefix);


/**
* @brief
* @param env
* @param prefix_count
* @param prefixes
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiSetNativeMethodPrefixes(jvmtiEnv* env,
	jint prefix_count,
	char** prefixes);


/* ---------------- jvmtiObject.c ---------------- */

/**
* @brief
* @param env
* @param object
* @param hash_code_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetObjectHashCode(jvmtiEnv* env,
	jobject object,
	jint* hash_code_ptr);


/**
* @brief
* @param env
* @param object
* @param info_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetObjectMonitorUsage(jvmtiEnv* env,
	jobject object,
	jvmtiMonitorUsage* info_ptr);


/**
* @brief
* @param env
* @param object
* @param size_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetObjectSize(jvmtiEnv* env,
	jobject object,
	jlong* size_ptr);


/* ---------------- jvmtiRawMonitor.c ---------------- */

/**
* @brief
* @param env
* @param name
* @param monitor_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiCreateRawMonitor(jvmtiEnv* env,
	const char* name,
	jrawMonitorID* monitor_ptr);


/**
* @brief
* @param env
* @param monitor
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiDestroyRawMonitor(jvmtiEnv* env,
	jrawMonitorID monitor);


/**
* @brief
* @param env
* @param monitor
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiRawMonitorEnter(jvmtiEnv* env,
	jrawMonitorID monitor);


/**
* @brief
* @param env
* @param monitor
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiRawMonitorExit(jvmtiEnv* env,
	jrawMonitorID monitor);


/**
* @brief
* @param env
* @param monitor
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiRawMonitorNotify(jvmtiEnv* env,
	jrawMonitorID monitor);


/**
* @brief
* @param env
* @param monitor
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiRawMonitorNotifyAll(jvmtiEnv* env,
	jrawMonitorID monitor);


/**
* @brief
* @param env
* @param monitor
* @param millis
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiRawMonitorWait(jvmtiEnv* env,
	jrawMonitorID monitor,
	jlong millis);


/* ---------------- jvmtiStackFrame.c ---------------- */

/**
* @brief
* @param env
* @param max_frame_count
* @param stack_info_ptr
* @param thread_count_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetAllStackTraces(jvmtiEnv* env,
	jint max_frame_count,
	jvmtiStackInfo** stack_info_ptr,
	jint* thread_count_ptr);


/**
* @brief
* @param env
* @param thread
* @param count_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetFrameCount(jvmtiEnv* env,
	jthread thread,
	jint* count_ptr);


/**
* @brief
* @param env
* @param thread
* @param depth
* @param method_ptr
* @param location_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetFrameLocation(jvmtiEnv* env,
	jthread thread,
	jint depth,
	jmethodID* method_ptr,
	jlocation* location_ptr);


/**
* @brief
* @param env
* @param thread
* @param start_depth
* @param max_frame_count
* @param frame_buffer
* @param count_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetStackTrace(jvmtiEnv* env,
	jthread thread,
	jint start_depth,
	jint max_frame_count,
	jvmtiFrameInfo* frame_buffer,
	jint* count_ptr);


/**
* @brief
* @param env
* @param thread_count
* @param thread_list
* @param max_frame_count
* @param stack_info_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetThreadListStackTraces(jvmtiEnv* env,
	jint thread_count,
	const jthread* thread_list,
	jint max_frame_count,
	jvmtiStackInfo** stack_info_ptr);


/**
* @brief
* @param env
* @param thread
* @param depth
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiNotifyFramePop(jvmtiEnv* env,
	jthread thread,
	jint depth);


/**
* @brief
* @param env
* @param thread
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiPopFrame(jvmtiEnv* env,
	jthread thread);


/* ---------------- jvmtiStartup.c ---------------- */

/**
* @brief
* @param vm
* @param stage
* @param reserved
* @return IDATA
*/
IDATA J9VMDllMain(J9JavaVM* vm, IDATA stage, void* reserved);


/**
* @brief
* @param *jvm
* @param options
* @param *reserved
* @return jint
*/
jint JNICALL JVM_OnLoad(JavaVM *jvm, char* options, void *reserved);


/* ---------------- jvmtiSystemProperties.c ---------------- */

/**
* @brief
* @param env
* @param count_ptr
* @param property_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetSystemProperties(jvmtiEnv* env,
	jint* count_ptr,
	char*** property_ptr);


/**
* @brief
* @param env
* @param property
* @param value_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetSystemProperty(jvmtiEnv* env,
	const char* property,
	char** value_ptr);


/**
* @brief
* @param env
* @param property
* @param value
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiSetSystemProperty(jvmtiEnv* env,
	const char* property,
	const char* value);


/* ---------------- jvmtiThread.c ---------------- */

/**
* @brief
* @param env
* @param threads_count_ptr
* @param threads_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetAllThreads(jvmtiEnv* env,
	jint* threads_count_ptr,
	jthread** threads_ptr);


/**
* @brief
* @param env
* @param thread
* @param monitor_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetCurrentContendedMonitor(jvmtiEnv* env,
	jthread thread,
	jobject* monitor_ptr);


/**
* @brief
* @param env
* @param thread_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetCurrentThread(jvmtiEnv* env,
	jthread* thread_ptr);


/**
* @brief
* @param env
* @param thread
* @param owned_monitor_count_ptr
* @param owned_monitors_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetOwnedMonitorInfo(jvmtiEnv* env,
	jthread thread,
	jint* owned_monitor_count_ptr,
	jobject** owned_monitors_ptr);


/**
* @brief
* @param env
* @param thread
* @param monitor_info_count_ptr
* @param monitor_info_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetOwnedMonitorStackDepthInfo(jvmtiEnv* env,
	jthread thread,
	jint* monitor_info_count_ptr,
	jvmtiMonitorStackDepthInfo** monitor_info_ptr);


/**
* @brief
* @param env
* @param thread
* @param info_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetThreadInfo(jvmtiEnv* env,
	jthread thread,
	jvmtiThreadInfo* info_ptr);


/**
* @brief
* @param env
* @param thread
* @param data_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetThreadLocalStorage(jvmtiEnv* env,
	jthread thread,
	void** data_ptr);


/**
* @brief
* @param env
* @param thread
* @param thread_state_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetThreadState(jvmtiEnv* env,
	jthread thread,
	jint* thread_state_ptr);


/**
* @brief
* @param env
* @param thread
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiInterruptThread(jvmtiEnv* env,
	jthread thread);


/**
* @brief
* @param env
* @param thread
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiResumeThread(jvmtiEnv* env,
	jthread thread);


/**
* @brief
* @param env
* @param request_count
* @param request_list
* @param results
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiResumeThreadList(jvmtiEnv* env,
	jint request_count,
	const jthread* request_list,
	jvmtiError* results);


/**
* @brief
* @param env
* @param thread
* @param proc
* @param arg
* @param priority
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiRunAgentThread(jvmtiEnv* env,
	jthread thread,
	jvmtiStartFunction proc,
	const void* arg,
	jint priority);


/**
* @brief
* @param env
* @param thread
* @param data
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiSetThreadLocalStorage(jvmtiEnv* env,
	jthread thread,
	const void* data);


/**
* @brief
* @param env
* @param thread
* @param exception
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiStopThread(jvmtiEnv* env,
	jthread thread,
	jobject exception);


/**
* @brief
* @param env
* @param thread
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiSuspendThread(jvmtiEnv* env,
	jthread thread);


/**
* @brief
* @param env
* @param request_count
* @param request_list
* @param results
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiSuspendThreadList(jvmtiEnv* env,
	jint request_count,
	const jthread* request_list,
	jvmtiError* results);


/* ---------------- jvmtiThreadGroup.c ---------------- */

/**
* @brief
* @param env
* @param group
* @param thread_count_ptr
* @param threads_ptr
* @param group_count_ptr
* @param groups_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetThreadGroupChildren(jvmtiEnv* env,
	jthreadGroup group,
	jint* thread_count_ptr,
	jthread** threads_ptr,
	jint* group_count_ptr,
	jthreadGroup** groups_ptr);


/**
* @brief
* @param env
* @param group
* @param info_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetThreadGroupInfo(jvmtiEnv* env,
	jthreadGroup group,
	jvmtiThreadGroupInfo* info_ptr);


/**
* @brief
* @param env
* @param group_count_ptr
* @param groups_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetTopThreadGroups(jvmtiEnv* env,
	jint* group_count_ptr,
	jthreadGroup** groups_ptr);


/* ---------------- jvmtiTimers.c ---------------- */

/**
* @brief
* @param env
* @param processor_count_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetAvailableProcessors(jvmtiEnv* env,
	jint* processor_count_ptr);


/**
* @brief
* @param env
* @param nanos_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetCurrentThreadCpuTime(jvmtiEnv* env,
	jlong* nanos_ptr);


/**
* @brief
* @param env
* @param info_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetCurrentThreadCpuTimerInfo(jvmtiEnv* env,
	jvmtiTimerInfo* info_ptr);


/**
* @brief
* @param env
* @param thread
* @param nanos_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetThreadCpuTime(jvmtiEnv* env,
	jthread thread,
	jlong* nanos_ptr);


/**
* @brief
* @param env
* @param info_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetThreadCpuTimerInfo(jvmtiEnv* env,
	jvmtiTimerInfo* info_ptr);


/**
* @brief
* @param env
* @param nanos_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetTime(jvmtiEnv* env,
	jlong* nanos_ptr);


/**
* @brief
* @param env
* @param info_ptr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetTimerInfo(jvmtiEnv* env,
	jvmtiTimerInfo* info_ptr);


/* ---------------- jvmtiWatchedField.c ---------------- */

/**
* @brief
* @param env
* @param klass
* @param field
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiClearFieldAccessWatch(jvmtiEnv* env,
	jclass klass,
	jfieldID field);


/**
* @brief
* @param env
* @param klass
* @param field
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiClearFieldModificationWatch(jvmtiEnv* env,
	jclass klass,
	jfieldID field);


/**
* @brief
* @param env
* @param klass
* @param field
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiSetFieldAccessWatch(jvmtiEnv* env,
	jclass klass,
	jfieldID field);


/**
* @brief
* @param env
* @param klass
* @param field
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiSetFieldModificationWatch(jvmtiEnv* env,
	jclass klass,
	jfieldID field);

#if JAVA_SPEC_VERSION >= 9
/* ---------------- jvmtiModules.c ---------------- */
/**
* @brief
* @param env
* @param moduleCountPtr
* @param modulesPtr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetAllModules(jvmtiEnv* env,
		jint* moduleCountPtr,
		jobject** modulesPtr);

/**
* @brief
* @param env
* @param class Loader
* @param pkgName
* @param modulePtr
* @return jvmtiError
*/
jvmtiError JNICALL
jvmtiGetNamedModule(jvmtiEnv* env,
		jobject classLoader,
		const char* packageName,
		jobject* modulePtr);

jvmtiError JNICALL
jvmtiAddModuleReads(jvmtiEnv* env,
		jobject module,
		jobject toModule);

jvmtiError JNICALL
jvmtiAddModuleExports(jvmtiEnv* env,
		jobject module,
		const char* pkgName,
		jobject toModule);

jvmtiError JNICALL
jvmtiAddModuleOpens(jvmtiEnv* env,
		jobject module,
		const char* pkgName,
		jobject toModule);

jvmtiError JNICALL
jvmtiAddModuleUses(jvmtiEnv* env,
		jobject module,
		jclass service);

jvmtiError JNICALL
jvmtiAddModuleProvides(jvmtiEnv* env,
		jobject module,
		jclass service,
		jclass impl_class);

jvmtiError JNICALL
jvmtiIsModifiableModule(jvmtiEnv* env,
		jobject module,
		jboolean* is_modifiable_module_ptr);

#endif /* JAVA_SPEC_VERSION >= 9 */
/* ---------------- suspendhelper.cpp ---------------- */
/**
* @brief
* @param currentThread
* @param thread
* @param allowNull
* @param currentThreadSuspended
* @return jvmtiError
*/
jvmtiError
suspendThread(J9VMThread *currentThread, jthread thread, UDATA allowNull, UDATA *currentThreadSuspended);

#ifdef __cplusplus
}
#endif

#endif /* jvmti_internal_h */

