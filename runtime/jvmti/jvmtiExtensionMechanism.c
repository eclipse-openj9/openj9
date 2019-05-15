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

#include <string.h>
#include <stdarg.h>

#include "omrcfg.h"
#include "jvmtiHelpers.h"
#include "jvmtinls.h"
#include "jvmti_internal.h"
#include "monhelp.h"
#include "VerboseGCInterface.h"
#include "omr.h"

#include <limits.h>
#include "ute.h"

typedef struct J9JVMTIExtensionFunctionInfo {
	jvmtiExtensionFunction func;
	const char* id;
	U_32 descriptionModule;
	U_32 descriptionNum;
	jint param_count;
	const jvmtiParamInfo * params;
	jint error_count;
	const jvmtiError * errors;
} J9JVMTIExtensionFunctionInfo;

typedef struct J9JVMTIExtensionEventInfo {
	jint extension_event_index;
	const char* id;
	U_32 descriptionModule;
	U_32 descriptionNum;
	jint param_count;
	const jvmtiParamInfo * params;
} J9JVMTIExtensionEventInfo;

static jvmtiError copyErrors (jvmtiEnv* env, jvmtiError** dest, const jvmtiError* source, jint count);
static jvmtiError copyString (jvmtiEnv* env, char** dest, const char* source);
static void freeExtensionFunctionInfo (jvmtiEnv* env, jvmtiExtensionFunctionInfo* info);
static jvmtiError copyParam (jvmtiEnv* env, jvmtiParamInfo* dest, const jvmtiParamInfo* source);
static jvmtiError copyParams (jvmtiEnv* env, jvmtiParamInfo** dest, const jvmtiParamInfo* source, jint count);
static jvmtiError JNICALL jvmtiJlmDump (jvmtiEnv* env, void ** dump_info,...);
static jvmtiError JNICALL jvmtiDumpSet (jvmtiEnv* jvmti_env, ...);
static jvmtiError JNICALL jvmtiTraceSet (jvmtiEnv* jvmti_env, ...);
static jvmtiError JNICALL jvmtiResetVmDump (jvmtiEnv* jvmti_env, ...);
static jvmtiError JNICALL jvmtiQueryVmDump(jvmtiEnv* jvmti_env, jint buffer_size, void* options_buffer, jint* data_size_ptr, ...);
static jvmtiError copyExtensionFunctionInfo (jvmtiEnv* env, jvmtiExtensionFunctionInfo* dest, const J9JVMTIExtensionFunctionInfo* source);
static jvmtiError JNICALL jvmtiJlmSet (jvmtiEnv* env, jint option, ...);
static jvmtiError copyExtensionEventInfo (jvmtiEnv* env, jvmtiExtensionEventInfo* dest, const J9JVMTIExtensionEventInfo* source);
static void freeExtensionEventInfo (jvmtiEnv* env, jvmtiExtensionEventInfo* info);
static jvmtiError JNICALL jvmtiTriggerVmDump (jvmtiEnv* jvmti_env, ...);
static jvmtiError JNICALL jvmtiGetOSThreadID(jvmtiEnv* jvmti_env, jthread thread, jlong * threadid_ptr, ...);

static jvmtiError JNICALL jvmtiGetStackTraceExtended(jvmtiEnv* env, jint type, jthread thread, jint start_depth, jint max_frame_count, void* frame_buffer, jint* count_ptr, ...);
static jvmtiError JNICALL jvmtiGetAllStackTracesExtended(jvmtiEnv* env, jint type, jint max_frame_count, void** stack_info_ptr, jint* thread_count_ptr, ...);
static jvmtiError JNICALL jvmtiGetThreadListStackTracesExtended(jvmtiEnv* env, jint type, jint thread_count, const jthread* thread_list, jint max_frame_count, void** stack_info_ptr, ...);
static jvmtiError jvmtiInternalGetStackTraceExtended(jvmtiEnv* env, J9JVMTIStackTraceType type, J9VMThread * currentThread, J9VMThread * targetThread, jint start_depth, UDATA max_frame_count, jvmtiFrameInfo* frame_buffer, jint* count_ptr);
static UDATA jvmtiInternalGetStackTraceIteratorExtended(J9VMThread * currentThread, J9StackWalkState * walkState);

static jvmtiError JNICALL jvmtiGetHeapFreeMemory(jvmtiEnv* jvmti_env, jlong* heapFree_ptr, ...);
static jvmtiError JNICALL jvmtiGetHeapTotalMemory(jvmtiEnv* jvmti_env, jlong* heapTotal_ptr, ...);

static jvmtiError JNICALL jvmtiIterateSharedCaches(jvmtiEnv* env, jint version, const char *cacheDir, jint flags, jboolean useCommandLineValues, jvmtiIterateSharedCachesCallback callback, void *user_data, ...);
static jvmtiError JNICALL jvmtiDestroySharedCache(jvmtiEnv *env, const char *cacheDir, const char *name, jint cacheType, jboolean useCommandLineValues, jint *internalErrorCode, ...);

static jvmtiError JNICALL jvmtiRemoveAllTags(jvmtiEnv* env, ...);

static jvmtiError JNICALL jvmtiRegisterTraceSubscriber(jvmtiEnv *env, char *description, jvmtiTraceSubscriber subscriber, jvmtiTraceAlarm alarm, void *userData, void **subscriptionID, ...);
static jvmtiError JNICALL jvmtiDeregisterTraceSubscriber(jvmtiEnv *env, void *subscriberID, ...);
static jvmtiError JNICALL jvmtiFlushTraceData(jvmtiEnv *env, ...);
static jvmtiError JNICALL jvmtiGetTraceMetadata(jvmtiEnv *env, void **data, jint *length, ...);
static jvmtiError JNICALL jvmtiGetMethodAndClassNames(jvmtiEnv *env, void * ramMethods, jint ramMethodCount, jvmtiExtensionRamMethodData * ramMethodDataDescriptors, jchar * ramMethodStrings, jint * ramMethodStringsSize, ...);

static jvmtiError JNICALL jvmtiQueryVmLogOptions(jvmtiEnv* jvmti_env, jint buffer_size, void* options_buffer, jint* data_size, ...);
static jvmtiError JNICALL jvmtiSetVmLogOptions(jvmtiEnv* jvmti_env, char* options_buffer, ...);

static jvmtiError JNICALL jvmtiJlmDumpStats (jvmtiEnv* env, void ** dump_info, jint dump_format, ...);
static jvmtiError jvmtiJlmDumpHelper(jvmtiEnv* env, void ** dump_info, jint dump_format);

static U_32 indexFromCategoryCode( UDATA categories_mapping_size, U_32 cc );
static UDATA jvmtiGetMemoryCategoriesCallback (U_32 categoryCode, const char * categoryName, UDATA liveBytes, UDATA liveAllocations, BOOLEAN isRoot, U_32 parentCategoryCode, OMRMemCategoryWalkState * state);
static UDATA jvmtiCountMemoryCategoriesCallback (U_32 categoryCode, const char * categoryName, UDATA liveBytes, UDATA liveAllocations, BOOLEAN isRoot, U_32 parentCategoryCode, OMRMemCategoryWalkState * state);
static UDATA jvmtiCalculateSlotsForCategoriesMappingCallback(U_32 categoryCode, const char *categoryName, UDATA liveBytes, UDATA liveAllocations, BOOLEAN isRoot, U_32 parentCategoryCode, OMRMemCategoryWalkState *state);
static void fillInChildAndSiblingCategories(jvmtiMemoryCategory * categories_buffer, jint written_count);
static void fillInCategoryDeepCounters(jvmtiMemoryCategory * category);
static jvmtiError JNICALL jvmtiGetMemoryCategories(jvmtiEnv* env, jint version, jint max_categories, jvmtiMemoryCategory * categories_buffer, jint * written_count_ptr, jint * total_categories_ptr, ...);

static jvmtiError JNICALL jvmtiRegisterVerboseGCSubscriber(jvmtiEnv *env, char *description, jvmtiVerboseGCSubscriber subscriber, jvmtiVerboseGCAlarm alarm, void *userData, void **subscriptionID, ...);
static jvmtiError JNICALL jvmtiDeregisterVerboseGCSubscriber(jvmtiEnv *env, void *subscriberID, ...);

static jvmtiError JNICALL jvmtiGetJ9vmThread(jvmtiEnv *env, jthread thread, void **vmThreadPtr, ...);
static jvmtiError JNICALL jvmtiGetJ9method(jvmtiEnv *env, jmethodID mid, void **j9MethodPtr, ...);

static jvmtiError JNICALL jvmtiRegisterTracePointSubscriber(jvmtiEnv *env, char *description, jvmtiTraceSubscriber subscriber, jvmtiTraceAlarm alarm, void *userData, void **subscriptionID, ...);
static jvmtiError JNICALL jvmtiDeregisterTracePointSubscriber(jvmtiEnv *env, void *subscriptionID, ...);

/*
 * Struct to encapsulate the details of a verbose GC subscriber
 */
typedef struct VerboseGCSubscriberData {	 
	 jvmtiVerboseGCSubscriber subscriber;
	 jvmtiVerboseGCAlarm alarm;
	 jvmtiEnv* env;
	 void* userData;
} VerboseGCSubscriberData;

static void unhookVerboseGCOutput(J9JavaVM* vm, VerboseGCSubscriberData* subscriberData);
static void hookVerboseGCOutput(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);

/*
 * Parameter lists for extended functions and events
 */

/* (jvmtiEnv *jvmti_env, jmethodID method) */
static const jvmtiParamInfo jvmtiCompilingStart_params[] = { 
	{ "method", JVMTI_KIND_IN, JVMTI_TYPE_JMETHODID, JNI_FALSE } 
};

/* (jvmtiEnv *jvmti_env, jmethodID method) */
static const jvmtiParamInfo jvmtiCompilingEnd_params[] = { 
	{ "method", JVMTI_KIND_IN, JVMTI_TYPE_JMETHODID, JNI_FALSE } 
};

/* (jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jobject object, jclass object_klass, jlong size) */
static const jvmtiParamInfo jvmtiInstrumentableObjectAlloc_params[] = { 
	{ "jni_env", JVMTI_KIND_IN_PTR, JVMTI_TYPE_JNIENV, JNI_FALSE },
	{ "thread", JVMTI_KIND_IN, JVMTI_TYPE_JTHREAD, JNI_FALSE },
	{ "object", JVMTI_KIND_IN, JVMTI_TYPE_JOBJECT, JNI_FALSE },
	{ "object_klass", JVMTI_KIND_IN, JVMTI_TYPE_JCLASS, JNI_FALSE },
	{ "size", JVMTI_KIND_IN, JVMTI_TYPE_JLONG, JNI_FALSE }
};

static const jvmtiParamInfo jvmtiGetOSThreadID_params[] = { 
	{ "thread", JVMTI_KIND_IN, JVMTI_TYPE_JTHREAD, JNI_TRUE },
	{ "threadid_ptr", JVMTI_KIND_OUT, JVMTI_TYPE_JLONG, JNI_FALSE },
};

/* (jvmtiEnv *jvmti_env, jint option) */
static const jvmtiParamInfo jvmtiTraceSet_params[] = { 
	{ "option", JVMTI_KIND_IN, JVMTI_TYPE_JINT, JNI_FALSE } 
};

/* (jvmtiEnv *jvmti_env, const char* option) */
static const jvmtiParamInfo jvmtiDumpSet_params[] = { 
	{ "option", JVMTI_KIND_IN_BUF, JVMTI_TYPE_CCHAR, JNI_FALSE } 
};

/* (jvmtiEnv *jvmti_env,  jint option) */
static const jvmtiParamInfo jvmtiJlmSet_params[] = { 
	{ "option", JVMTI_KIND_IN_PTR, JVMTI_TYPE_JINT, JNI_FALSE } 
};

/* (jvmtiEnv *jvmti_env, ** JlmDump) */
static const jvmtiParamInfo jvmtiJlmDump_params[] = { 
	{ "jlm_dump_ptr", JVMTI_KIND_ALLOC_BUF, JVMTI_TYPE_CVOID, JNI_FALSE } 
};

/* (jvmtiEnv *jvmti_env, const char* option) */
static const jvmtiParamInfo jvmtiTriggerVmDump_params[] = { 
	{ "option", JVMTI_KIND_IN_BUF, JVMTI_TYPE_CCHAR, JNI_FALSE } 
};

/* (jvmtiEnv *jvmti_env, jint option) */
static const jvmtiParamInfo jvmtiVmDumpStart_params[] = {
	{ "label", JVMTI_KIND_IN_BUF, JVMTI_TYPE_CCHAR, JNI_FALSE },
	{ "event", JVMTI_KIND_IN_BUF, JVMTI_TYPE_CCHAR, JNI_FALSE },
	{ "detail", JVMTI_KIND_IN_BUF, JVMTI_TYPE_CCHAR, JNI_TRUE }
};

/* (jvmtiEnv *jvmti_env, jint option) */
static const jvmtiParamInfo jvmtiVmDumpEnd_params[] = {
	{ "label", JVMTI_KIND_IN_BUF, JVMTI_TYPE_CCHAR, JNI_FALSE },
	{ "event", JVMTI_KIND_IN_BUF, JVMTI_TYPE_CCHAR, JNI_FALSE },
	{ "detail", JVMTI_KIND_IN_BUF, JVMTI_TYPE_CCHAR, JNI_TRUE }
};

/* (jvmtiEnv *jvmti_env, jthread thread, jint start_depth, jint max_frame_count, void* frame_buffer, jint* count_ptr ) */
static const jvmtiParamInfo jvmtiGetStackTraceExtended_params[] = {
	{ "type", JVMTI_KIND_IN, JVMTI_TYPE_JINT, JNI_FALSE },	 
	{ "thread", JVMTI_KIND_IN, JVMTI_TYPE_JTHREAD, JNI_FALSE },
	{ "start_depth", JVMTI_KIND_IN, JVMTI_TYPE_JINT, JNI_FALSE },
	{ "max_frame_count", JVMTI_KIND_IN, JVMTI_TYPE_JINT, JNI_FALSE },
	{ "frame_buffer", JVMTI_KIND_OUT_BUF, JVMTI_TYPE_CVOID, JNI_FALSE },
	{ "count_ptr", JVMTI_KIND_OUT, JVMTI_TYPE_JINT, JNI_FALSE },
};

/* (jvmtiEnv *jvmti_env, jint max_frame_count, void** stack_info_ptr, jint* thread_count_ptr ) */
static const jvmtiParamInfo jvmtiGetAllStackTracesExtended_params[] = {
	{ "type", JVMTI_KIND_IN, JVMTI_TYPE_JINT, JNI_FALSE },
	{ "max_frame_count", JVMTI_KIND_IN, JVMTI_TYPE_JINT, JNI_FALSE },
	{ "stack_info_ptr", JVMTI_KIND_ALLOC_BUF, JVMTI_TYPE_CVOID, JNI_FALSE },
	{ "thread_count_ptr", JVMTI_KIND_OUT, JVMTI_TYPE_JINT, JNI_FALSE }
};

/* (jvmtiEnv *jvmti_env, jint thread_count, jthread* thread_list, jint max_frame_count, void** stack_info_ptr) */
static const jvmtiParamInfo jvmtiGetThreadListStackTracesExtended_params[] = {
	{ "type", JVMTI_KIND_IN, JVMTI_TYPE_JINT, JNI_FALSE },	 
	{ "thread_count", JVMTI_KIND_IN, JVMTI_TYPE_JINT, JNI_FALSE },
	{ "thread_list", JVMTI_KIND_IN, JVMTI_TYPE_JTHREAD, JNI_FALSE },
	{ "max_frame_count", JVMTI_KIND_IN, JVMTI_TYPE_JINT, JNI_FALSE },
	{ "stack_info_ptr", JVMTI_KIND_ALLOC_BUF, JVMTI_TYPE_CVOID, JNI_FALSE }
};

/* (jvmtiEnv *jvmti_env, jlong *heapFree_ptr) */
static const jvmtiParamInfo jvmtiGetHeapFreeMemory_params[] = {
	{ "heapFree_ptr", JVMTI_KIND_OUT, JVMTI_TYPE_JLONG, JNI_FALSE }
};

/* (jvmtiEnv *jvmti_env, jlong *heapTotal_ptr) */
static const jvmtiParamInfo jvmtiGetHeapTotalMemory_params[] = {
	{ "heapTotal_ptr", JVMTI_KIND_OUT, JVMTI_TYPE_JLONG, JNI_FALSE }
};

/* (jvmtiEnv *jvmti_env, jint version, const char *cacheDir, jboolean useCommandLineValues, jvmtiIterateSharedCachesCallback *callback, void *user_data) */
static const jvmtiParamInfo jvmtiIterateSharedCaches_params[] = {
	{ "version", JVMTI_KIND_IN, JVMTI_TYPE_JINT, JNI_FALSE },
	{ "cacheDir", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CCHAR, JNI_TRUE },
	{ "flags", JVMTI_KIND_IN, JVMTI_TYPE_JINT, JNI_FALSE },
	{ "useCommandLineValues", JVMTI_KIND_IN, JVMTI_TYPE_JBOOLEAN, JNI_FALSE },
	{ "callback", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CVOID, JNI_FALSE },
	{ "user_data", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CVOID, JNI_TRUE }
};

/* (jvmtiEnv *jvmti_env, const char *cacheDir, char *name, jint persistence, jboolean useCommandLineValues)  */
static const jvmtiParamInfo jvmtiDestroySharedCache_params[] = {
	{ "cacheDir", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CCHAR, JNI_TRUE },
	{ "name", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CCHAR, JNI_TRUE },
	{ "persistence", JVMTI_KIND_IN, JVMTI_TYPE_JINT, JNI_FALSE },
	{ "useCommandLineValues", JVMTI_KIND_IN, JVMTI_TYPE_JBOOLEAN, JNI_FALSE },
	{ "internalErrorCode", JVMTI_KIND_OUT, JVMTI_TYPE_JINT, JNI_TRUE }
};

/* (jvmtiEnv *jvmti_env, char *description, jvmtiTraceSubscriber *subscriber, jvmtiTraceAlarm *alarm, void *userData, void **subscriptionID) */
static const jvmtiParamInfo jvmtiRegisterTraceSubscriber_params[] = {
	{ "description", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CCHAR, JNI_FALSE },
	{ "subscriber", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CVOID, JNI_FALSE },
	{ "alarm", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CVOID, JNI_TRUE },
	{ "user_data", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CVOID, JNI_TRUE },
	{ "subscription_id", JVMTI_KIND_OUT, JVMTI_TYPE_CVOID, JNI_FALSE }
};

/* (jvmtiEnv *jvmti_env, jvmtiTraceSubscriber *subscriber, void (*alarm)(void)) */
static const jvmtiParamInfo jvmtiDeregisterTraceSubscriber_params[] = {
	{ "subscription_id", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CVOID, JNI_FALSE }
};

/* (jvmtiEnv *jvmti_env, jvmtiTraceSubscriber *subscriber, void (*alarm)(void)) */
static const jvmtiParamInfo jvmtiGetTraceMetadata_params[] = {
	{ "data", JVMTI_KIND_OUT, JVMTI_TYPE_CVOID, JNI_FALSE },
	{ "length", JVMTI_KIND_OUT, JVMTI_TYPE_JINT, JNI_FALSE }
};

/* (jvmtiEnv *jvmti_env,  void * ramMethods, jint ramMethodCount, jvmtiExtensionRamMethodData * ramMethodDataDescriptors, 
    jchar * ramMethodStrings, jint * ramMethodDataDescriptorsCount) */
static const jvmtiParamInfo jvmtiGetMethodAndClassNames_params[] = {
	{ "ramMethods", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CVOID, JNI_FALSE },
	{ "ramMethodCount", JVMTI_KIND_IN, JVMTI_TYPE_JINT, JNI_FALSE },
	{ "ramMethodDataDescriptors", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CCHAR, JNI_FALSE },
	{ "ramMethodStrings", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CCHAR, JNI_FALSE },
	{ "ramMethodStringsSize", JVMTI_KIND_IN, JVMTI_TYPE_JINT, JNI_FALSE },
	{ "ramMethodDataDescriptorsCount", JVMTI_KIND_IN_PTR, JVMTI_TYPE_JINT, JNI_FALSE }
};
 
/* (jvmtiEnv *jvmti_env, jobject object, jint buffer_size, void* options_buffer, jint* data_size_ptr) */
static const jvmtiParamInfo jvmtiQueryVmLogOptions_params[] = {
	{ "buffer_size", JVMTI_KIND_IN, JVMTI_TYPE_JINT, JNI_FALSE },
	{ "options_buffer",  JVMTI_KIND_OUT_BUF, JVMTI_TYPE_CVOID, JNI_FALSE },
	{ "data_size_ptr", JVMTI_KIND_OUT, JVMTI_TYPE_JINT, JNI_FALSE }
};

/* (jvmtiEnv *jvmti_env, void *options_buffer) */
static const jvmtiParamInfo jvmtiSetVmLogOptions_params[] = {
	{ "options_buffer",  JVMTI_KIND_IN_BUF, JVMTI_TYPE_CCHAR, JNI_FALSE }
};

/* (jvmtiEnv * jvmti_env, ** dump_info, jint dump_format) */
static const jvmtiParamInfo jvmtiJlmDumpStats_params[] = { 
	{ "dump_info", JVMTI_KIND_ALLOC_BUF, JVMTI_TYPE_CVOID, JNI_FALSE },
    { "dump_format", JVMTI_KIND_IN_BUF, JVMTI_TYPE_JINT, JNI_FALSE }
};

/* (jvmtiEnv* env, jint version, jint max_categories, jvmtiMemoryCategory * categories_buffer, jint * written_count_ptr, jint * total_categories_ptr) */
static const jvmtiParamInfo jvmtiGetMemoryCategories_params[] = {
	{ "version", JVMTI_KIND_IN, JVMTI_TYPE_JINT, JNI_FALSE },
	{ "max_categories", JVMTI_KIND_IN, JVMTI_TYPE_JINT, JNI_FALSE },
	{ "categories_buffer",  JVMTI_KIND_OUT_BUF, JVMTI_TYPE_CCHAR, JNI_TRUE },
	{ "written_count_ptr", JVMTI_KIND_OUT, JVMTI_TYPE_JINT, JNI_TRUE },
	{ "total_categories_ptr", JVMTI_KIND_OUT, JVMTI_TYPE_JINT, JNI_TRUE }
};

/* (jvmtiEnv *env, char *description, jvmtiVerboseGCSubscriber subscriber, jvmtiVerboseGCAlarm alarm, void *userData, void **subscriptionID) */
static const jvmtiParamInfo jvmtiRegisterVerboseGCSubscriber_params[] = {
	{ "description", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CCHAR, JNI_FALSE },
	{ "subscriber", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CVOID, JNI_FALSE },
	{ "alarm", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CVOID, JNI_TRUE },
	{ "user_data", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CVOID, JNI_TRUE },
	{ "subscription_id", JVMTI_KIND_OUT, JVMTI_TYPE_CVOID, JNI_FALSE }
};

/* (jvmtiEnv *env, void *subscriptionID) */
static const jvmtiParamInfo jvmtiDeregisterVerboseGCSubscriber_params[] = {
	{ "subscription_id", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CVOID, JNI_FALSE }
};

static const jvmtiParamInfo jvmtiGetJ9vmThread_params[] = {
	{ "thread", JVMTI_KIND_IN, JVMTI_TYPE_JTHREAD, JNI_TRUE },
	{ "vmThreadPtr", JVMTI_KIND_OUT, JVMTI_TYPE_CVOID, JNI_TRUE }
};

static const jvmtiParamInfo jvmtiGetJ9method_params[] = {
	{ "mid", JVMTI_KIND_IN, JVMTI_TYPE_JMETHODID, JNI_TRUE },
	{ "j9MethodPtr", JVMTI_KIND_OUT, JVMTI_TYPE_CVOID, JNI_TRUE }
};

/* (jvmtiEnv *env, char *description, jvmtiTraceSubscriber subscriber, jvmtiTraceAlarm alarm, void *userData, void **subscriptionID) */
static const jvmtiParamInfo jvmtiRegisterTracePointSubscriber_params[] = {
	{ "description", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CCHAR, JNI_FALSE },
	{ "subscriber", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CVOID, JNI_FALSE },
	{ "alarm", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CVOID, JNI_FALSE },
	{ "userData", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CVOID, JNI_TRUE },
	{ "subscriptionID", JVMTI_KIND_OUT, JVMTI_TYPE_CVOID, JNI_FALSE }
};

/* (jvmtiEnv *env, void *subscriptionID) */
static const jvmtiParamInfo jvmtiDeregisterTracepointSubscriber_params[] = {
	{ "subscriptionID", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CVOID, JNI_FALSE }
};

/*
 * Error lists for extended functions
 */

static const jvmtiError nullPointer_errors[] = {
	JVMTI_ERROR_NULL_POINTER
};

static const jvmtiError notAvailable_errors[] = {
	JVMTI_ERROR_NOT_AVAILABLE
};

static const jvmtiError ras_errors[] = {
	JVMTI_ERROR_NULL_POINTER,
	JVMTI_ERROR_OUT_OF_MEMORY,
	JVMTI_ERROR_INVALID_ENVIRONMENT,
	JVMTI_ERROR_WRONG_PHASE,
	JVMTI_ERROR_INTERNAL,
	JVMTI_ERROR_NOT_AVAILABLE,
	JVMTI_ERROR_ILLEGAL_ARGUMENT
};
static const jvmtiError jlm_set_errors[] = {
	JVMTI_ERROR_NULL_POINTER,
	JVMTI_ERROR_WRONG_PHASE,
	JVMTI_ERROR_INTERNAL,
	JVMTI_ERROR_NOT_AVAILABLE
};

static const jvmtiError jlm_dump_errors[] = {
	JVMTI_ERROR_NULL_POINTER,
	JVMTI_ERROR_OUT_OF_MEMORY,
	JVMTI_ERROR_INVALID_ENVIRONMENT,
	JVMTI_ERROR_WRONG_PHASE,
	JVMTI_ERROR_INTERNAL,
	JVMTI_ERROR_NOT_AVAILABLE,
	JVMTI_ERROR_ILLEGAL_ARGUMENT
};

static const jvmtiError get_os_thread_id_errors[] = {
	JVMTI_ERROR_WRONG_PHASE,
	JVMTI_ERROR_INVALID_THREAD,
	JVMTI_ERROR_THREAD_NOT_ALIVE,
	JVMTI_ERROR_NULL_POINTER
};

static const jvmtiError jvmtiGetStack_errors[] = {
	JVMTI_ERROR_WRONG_PHASE,
	JVMTI_ERROR_INVALID_THREAD,
	JVMTI_ERROR_THREAD_NOT_ALIVE,
	JVMTI_ERROR_NULL_POINTER,
	JVMTI_ERROR_ILLEGAL_ARGUMENT,
	JVMTI_ERROR_OUT_OF_MEMORY	
};

static const jvmtiError jvmtiIterateSharedCaches_errors[] = {
	JVMTI_ERROR_NULL_POINTER,
	JVMTI_ERROR_OUT_OF_MEMORY,
	JVMTI_ERROR_WRONG_PHASE,
	JVMTI_ERROR_INTERNAL,
	JVMTI_ERROR_NOT_AVAILABLE,
	JVMTI_ERROR_UNSUPPORTED_VERSION,
	JVMTI_ERROR_ILLEGAL_ARGUMENT,
	JVMTI_ERROR_INVALID_ENVIRONMENT
};

static const jvmtiError jvmtiDestroySharedCache_errors[] = {
	JVMTI_ERROR_OUT_OF_MEMORY,
	JVMTI_ERROR_WRONG_PHASE,
	JVMTI_ERROR_INTERNAL,
	JVMTI_ERROR_NOT_AVAILABLE,
	JVMTI_ERROR_ILLEGAL_ARGUMENT,
	JVMTI_ERROR_INVALID_ENVIRONMENT
};

static const jvmtiError jvmtiRegisterTraceSubscriber_errors[] = {
	JVMTI_ERROR_NULL_POINTER,
	JVMTI_ERROR_OUT_OF_MEMORY,
	JVMTI_ERROR_INVALID_ENVIRONMENT,
	JVMTI_ERROR_WRONG_PHASE
};

static const jvmtiError jvmtiDeregisterTraceSubscriber_errors[] = {
	JVMTI_ERROR_NULL_POINTER,
	JVMTI_ERROR_OUT_OF_MEMORY,
	JVMTI_ERROR_INVALID_ENVIRONMENT,
	JVMTI_ERROR_WRONG_PHASE,
	JVMTI_ERROR_ILLEGAL_ARGUMENT,
	JVMTI_ERROR_NOT_AVAILABLE
};

static const jvmtiError jvmtiFlushTraceData_errors[] = {
	JVMTI_ERROR_OUT_OF_MEMORY,
	JVMTI_ERROR_WRONG_PHASE,
	JVMTI_ERROR_INVALID_ENVIRONMENT
};

static const jvmtiError jvmtiGetTraceMetadata_errors[] = {
	JVMTI_ERROR_NULL_POINTER,		
	JVMTI_ERROR_WRONG_PHASE,
	JVMTI_ERROR_INVALID_ENVIRONMENT
};

static const jvmtiError jvmtiGetMemoryCategories_errors[] = {
	JVMTI_ERROR_UNSUPPORTED_VERSION,
	JVMTI_ERROR_ILLEGAL_ARGUMENT,
	JVMTI_ERROR_INVALID_ENVIRONMENT,
	JVMTI_ERROR_OUT_OF_MEMORY
};
 
static const jvmtiError jvmtiRegisterVerboseGCSubscriber_errors[] = {
	JVMTI_ERROR_NULL_POINTER,
	JVMTI_ERROR_OUT_OF_MEMORY,
	JVMTI_ERROR_INVALID_ENVIRONMENT,
	JVMTI_ERROR_WRONG_PHASE,
	JVMTI_ERROR_INTERNAL
};

static const jvmtiError jvmtiDeregisterVerboseGCSubscriber_errors[] = {
	JVMTI_ERROR_NULL_POINTER,
	JVMTI_ERROR_OUT_OF_MEMORY,
	JVMTI_ERROR_INVALID_ENVIRONMENT,
	JVMTI_ERROR_WRONG_PHASE,
	JVMTI_ERROR_NOT_AVAILABLE
};

static const jvmtiError jvmtiGet_j9vmthread_errors[] = {
	JVMTI_ERROR_WRONG_PHASE,
	JVMTI_ERROR_INVALID_THREAD,
	JVMTI_ERROR_THREAD_NOT_ALIVE,
	JVMTI_ERROR_NULL_POINTER
};

static const jvmtiError jvmtiGet_j9method_errors[] = {
	JVMTI_ERROR_WRONG_PHASE,
	JVMTI_ERROR_NOT_AVAILABLE,
	JVMTI_ERROR_NULL_POINTER
};

static const jvmtiError jvmtiRegisterTracePointSubscriber_errors[] = {
	JVMTI_ERROR_NULL_POINTER,
	JVMTI_ERROR_OUT_OF_MEMORY,
	JVMTI_ERROR_INVALID_ENVIRONMENT,
	JVMTI_ERROR_WRONG_PHASE,
	JVMTI_ERROR_INTERNAL
};

static const jvmtiError jvmtiDeregisterTracePointSubscriber_errors[] = {
	JVMTI_ERROR_NULL_POINTER,
	JVMTI_ERROR_OUT_OF_MEMORY,
	JVMTI_ERROR_INVALID_ENVIRONMENT,
	JVMTI_ERROR_WRONG_PHASE,
	JVMTI_ERROR_NOT_AVAILABLE,
	JVMTI_ERROR_INTERNAL
};	

#define SIZE_AND_TABLE(table) (sizeof(table) / sizeof(table[0])) , (table)
#define EMPTY_SIZE_AND_TABLE 0, NULL

/*
 * The extension function table
 */

static const J9JVMTIExtensionFunctionInfo J9JVMTIExtensionFunctionInfoTable[] = {
	{
		(jvmtiExtensionFunction) jvmtiTraceSet,
		COM_IBM_SET_VM_TRACE,
		J9NLS_JVMTI_COM_IBM_JVM_TRACE_SET_DESCRIPTION,
		SIZE_AND_TABLE(jvmtiTraceSet_params),
		SIZE_AND_TABLE(ras_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiDumpSet,
		COM_IBM_SET_VM_DUMP,
		J9NLS_JVMTI_COM_IBM_JVM_DUMP_SET_DESCRIPTION,
		SIZE_AND_TABLE(jvmtiDumpSet_params),
		SIZE_AND_TABLE(ras_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiJlmSet,
		COM_IBM_SET_VM_JLM,
		J9NLS_JVMTI_COM_IBM_JVM_JLM_SET_DESCRIPTION,
		SIZE_AND_TABLE(jvmtiJlmSet_params),
		SIZE_AND_TABLE(jlm_set_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiJlmDump,
		COM_IBM_SET_VM_JLM_DUMP,
		J9NLS_JVMTI_COM_IBM_JVM_JLM_DUMP_DESCRIPTION,
		SIZE_AND_TABLE(jvmtiJlmDump_params),
		SIZE_AND_TABLE(jlm_dump_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiTriggerVmDump,
		COM_IBM_TRIGGER_VM_DUMP,
		J9NLS_JVMTI_COM_IBM_JVM_TRIGGER_VM_DUMP_DESCRIPTION,
		SIZE_AND_TABLE(jvmtiTriggerVmDump_params),
		SIZE_AND_TABLE(ras_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiGetOSThreadID,
		COM_IBM_GET_OS_THREAD_ID,
		J9NLS_JVMTI_COM_IBM_GET_OS_THREAD_ID,
		SIZE_AND_TABLE(jvmtiGetOSThreadID_params),
		SIZE_AND_TABLE(get_os_thread_id_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiGetStackTraceExtended,
		COM_IBM_GET_STACK_TRACE_EXTENDED,
		J9NLS_JVMTI_COM_IBM_GET_STACK_TRACE_EXTENDED_DESCRIPTION,
		SIZE_AND_TABLE(jvmtiGetStackTraceExtended_params),
		SIZE_AND_TABLE(jvmtiGetStack_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiGetAllStackTracesExtended,
		COM_IBM_GET_ALL_STACK_TRACES_EXTENDED,
		J9NLS_JVMTI_COM_IBM_GET_ALL_STACK_TRACES_EXTENDED_DESCRIPTION,
		SIZE_AND_TABLE(jvmtiGetAllStackTracesExtended_params),
		SIZE_AND_TABLE(jvmtiGetStack_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiGetThreadListStackTracesExtended,
		COM_IBM_GET_THREAD_LIST_STACK_TRACES_EXTENDED,
		J9NLS_JVMTI_COM_IBM_GET_THREAD_LIST_STACK_TRACES_EXTENDED_DESCRIPTION,
		SIZE_AND_TABLE(jvmtiGetThreadListStackTracesExtended_params),
		SIZE_AND_TABLE(jvmtiGetStack_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiResetVmDump,
		COM_IBM_RESET_VM_DUMP,
		J9NLS_JVMTI_COM_IBM_JVM_RESET_VM_DUMP_DESCRIPTION,
		0, NULL,
		SIZE_AND_TABLE(ras_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiGetHeapFreeMemory,
		COM_IBM_GET_HEAP_FREE_MEMORY,
		J9NLS_JVMTI_COM_IBM_GET_HEAP_FREE_MEMORY_DESCRIPTION,
		SIZE_AND_TABLE(jvmtiGetHeapFreeMemory_params),
		SIZE_AND_TABLE(nullPointer_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiGetHeapTotalMemory,
		COM_IBM_GET_HEAP_TOTAL_MEMORY,
		J9NLS_JVMTI_COM_IBM_GET_HEAP_TOTAL_MEMORY_DESCRIPTION,
		SIZE_AND_TABLE(jvmtiGetHeapTotalMemory_params),
		SIZE_AND_TABLE(nullPointer_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiIterateSharedCaches,
		COM_IBM_ITERATE_SHARED_CACHES,
		J9NLS_JVMTI_COM_IBM_JVM_ITERATE_SHARED_CACHES_DESCRIPTION,
		SIZE_AND_TABLE(jvmtiIterateSharedCaches_params),
		SIZE_AND_TABLE(jvmtiIterateSharedCaches_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiDestroySharedCache,
		COM_IBM_DESTROY_SHARED_CACHE,
		J9NLS_JVMTI_COM_IBM_JVM_DESTROY_SHARED_CACHE_DESCRIPTION,
		SIZE_AND_TABLE(jvmtiDestroySharedCache_params),
		SIZE_AND_TABLE(jvmtiDestroySharedCache_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiRemoveAllTags,
		COM_IBM_REMOVE_ALL_TAGS,
		J9NLS_JVMTI_COM_IBM_REMOVE_ALL_TAGS,
		0, NULL,
		SIZE_AND_TABLE(notAvailable_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiRegisterTraceSubscriber,
		COM_IBM_REGISTER_TRACE_SUBSCRIBER,
		J9NLS_JVMTI_COM_IBM_JVM_REGISTER_TRACE_SUBSCRIBER_DESCRIPTION,
		SIZE_AND_TABLE(jvmtiRegisterTraceSubscriber_params),
		SIZE_AND_TABLE(jvmtiRegisterTraceSubscriber_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiDeregisterTraceSubscriber,
		COM_IBM_DEREGISTER_TRACE_SUBSCRIBER,
		J9NLS_JVMTI_COM_IBM_JVM_DEREGISTER_TRACE_SUBSCRIBER_DESCRIPTION,
		SIZE_AND_TABLE(jvmtiDeregisterTraceSubscriber_params),
		SIZE_AND_TABLE(jvmtiDeregisterTraceSubscriber_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiFlushTraceData,
		COM_IBM_FLUSH_TRACE_DATA,
		J9NLS_JVMTI_COM_IBM_JVM_FLUSH_TRACE_DATA_DESCRIPTION,
		0, NULL,
		SIZE_AND_TABLE(jvmtiFlushTraceData_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiGetTraceMetadata,
		COM_IBM_GET_TRACE_METADATA,
		J9NLS_JVMTI_COM_IBM_JVM_GET_TRACE_METADATA_DESCRIPTION,
		SIZE_AND_TABLE(jvmtiGetTraceMetadata_params),
		SIZE_AND_TABLE(jvmtiGetTraceMetadata_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiGetMethodAndClassNames,
		COM_IBM_GET_METHOD_AND_CLASS_NAMES,
		J9NLS_JVMTI_COM_IBM_GET_METHOD_AND_CLASS_NAMES,
		SIZE_AND_TABLE(jvmtiGetMethodAndClassNames_params),
		SIZE_AND_TABLE(jvmtiFlushTraceData_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiQueryVmDump,
		COM_IBM_QUERY_VM_DUMP,
		J9NLS_JVMTI_COM_IBM_QUERY_VM_DUMP,
		0, NULL,
		SIZE_AND_TABLE(ras_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiQueryVmLogOptions,
		COM_IBM_QUERY_VM_LOG_OPTIONS,
		J9NLS_JVMTI_COM_IBM_QUERY_VM_LOG_OPTIONS,
		SIZE_AND_TABLE(jvmtiQueryVmLogOptions_params),
		SIZE_AND_TABLE(ras_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiSetVmLogOptions,
		COM_IBM_SET_VM_LOG_OPTIONS,
		J9NLS_JVMTI_COM_IBM_SET_VM_LOG_OPTIONS,
		SIZE_AND_TABLE(jvmtiSetVmLogOptions_params),
		SIZE_AND_TABLE(ras_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiJlmDumpStats,
		COM_IBM_JLM_DUMP_STATS,
		J9NLS_JVMTI_COM_IBM_JLM_STATS_DUMP_DESCRIPTION,
		SIZE_AND_TABLE(jvmtiJlmDumpStats_params),
		SIZE_AND_TABLE(jlm_dump_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiGetMemoryCategories,
		COM_IBM_GET_MEMORY_CATEGORIES,
		J9NLS_JVMTI_COM_IBM_GET_MEMORY_CATEGORIES_DESCRIPTION,
		SIZE_AND_TABLE(jvmtiGetMemoryCategories_params),
		SIZE_AND_TABLE(jvmtiGetMemoryCategories_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiRegisterVerboseGCSubscriber,
		COM_IBM_REGISTER_VERBOSEGC_SUBSCRIBER,
		J9NLS_JVMTI_COM_IBM_JVM_REGISTER_VERBOSEGC_SUBSCRIBER_DESCRIPTION,
		SIZE_AND_TABLE(jvmtiRegisterVerboseGCSubscriber_params),
		SIZE_AND_TABLE(jvmtiRegisterVerboseGCSubscriber_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiDeregisterVerboseGCSubscriber,
		COM_IBM_DEREGISTER_VERBOSEGC_SUBSCRIBER,
		J9NLS_JVMTI_COM_IBM_JVM_DEREGISTER_VERBOSEGC_SUBSCRIBER_DESCRIPTION,
		SIZE_AND_TABLE(jvmtiDeregisterVerboseGCSubscriber_params),
		SIZE_AND_TABLE(jvmtiDeregisterVerboseGCSubscriber_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiGetJ9vmThread,
		COM_IBM_GET_J9VMTHREAD,
		J9NLS_JVMTI_COM_IBM_GET_J9VMTHREAD,
		SIZE_AND_TABLE(jvmtiGetJ9vmThread_params),
		SIZE_AND_TABLE(jvmtiGet_j9vmthread_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiGetJ9method,
		COM_IBM_GET_J9METHOD,
		J9NLS_JVMTI_COM_IBM_GET_J9METHOD,
		SIZE_AND_TABLE(jvmtiGetJ9method_params),
		SIZE_AND_TABLE(jvmtiGet_j9method_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiRegisterTracePointSubscriber,
		COM_IBM_REGISTER_TRACEPOINT_SUBSCRIBER,
		J9NLS_JVMTI_COM_IBM_JVM_REGISTER_TRACEPOINT_SUBSCRIBER_DESCRIPTION,
		SIZE_AND_TABLE(jvmtiRegisterTracePointSubscriber_params),
		SIZE_AND_TABLE(jvmtiRegisterTracePointSubscriber_errors)
	},
	{
		(jvmtiExtensionFunction) jvmtiDeregisterTracePointSubscriber,
		COM_IBM_DEREGISTER_TRACEPOINT_SUBSCRIBER,
		J9NLS_JVMTI_COM_IBM_JVM_DEREGISTER_TRACEPOINT_SUBSCRIBER_DESCRIPTION,
		SIZE_AND_TABLE(jvmtiDeregisterTracepointSubscriber_params),
		SIZE_AND_TABLE(jvmtiDeregisterTracePointSubscriber_errors)
	},
};

#define NUM_EXTENSION_FUNCTIONS (sizeof(J9JVMTIExtensionFunctionInfoTable) / sizeof(J9JVMTIExtensionFunctionInfoTable[0]))

/*
 * The extension event table
 */

static const J9JVMTIExtensionEventInfo J9JVMTIExtensionEventInfoTable[] = {
	{
		J9JVMTI_EVENT_COM_IBM_COMPILING_START,
		COM_IBM_COMPILING_START,
		J9NLS_JVMTI_COM_IBM_COMPILING_START_DESCRIPTION,
		SIZE_AND_TABLE(jvmtiCompilingStart_params),
	}, 
	{
		J9JVMTI_EVENT_COM_IBM_COMPILING_END,
		COM_IBM_COMPILING_END,
		J9NLS_JVMTI_COM_IBM_COMPILING_END_DESCRIPTION,
		SIZE_AND_TABLE(jvmtiCompilingEnd_params),
	}, 
	{
		J9JVMTI_EVENT_COM_IBM_INSTRUMENTABLE_OBJECT_ALLOC,
		COM_IBM_INSTRUMENTABLE_OBJECT_ALLOC,
		J9NLS_JVMTI_COM_IBM_INSTRUMENTABLE_OBJET_ALLOC,
		SIZE_AND_TABLE(jvmtiInstrumentableObjectAlloc_params),
	}, 
	{
		J9JVMTI_EVENT_COM_IBM_VM_DUMP_START,
		COM_IBM_VM_DUMP_START,
		J9NLS_JVMTI_COM_IBM_VM_DUMP_START,
		SIZE_AND_TABLE(jvmtiVmDumpStart_params),
	}, 
	{
		J9JVMTI_EVENT_COM_IBM_VM_DUMP_END,
		COM_IBM_VM_DUMP_END,
		J9NLS_JVMTI_COM_IBM_VM_DUMP_END,
		SIZE_AND_TABLE(jvmtiVmDumpEnd_params),
	}, 
	{
		J9JVMTI_EVENT_COM_IBM_GARBAGE_COLLECTION_CYCLE_START,
		COM_IBM_GARBAGE_COLLECTION_CYCLE_START,
		J9NLS_JVMTI_COM_IBM_GARBAGE_COLLECTION_CYCLE_START_DESCRIPTION,
		EMPTY_SIZE_AND_TABLE,
	},
	{
		J9JVMTI_EVENT_COM_IBM_GARBAGE_COLLECTION_CYCLE_FINISH,
		COM_IBM_GARBAGE_COLLECTION_CYCLE_FINISH,
		J9NLS_JVMTI_COM_IBM_GARBAGE_COLLECTION_CYCLE_FINISH_DESCRIPTION,
		EMPTY_SIZE_AND_TABLE,
	},
};

#define NUM_EXTENSION_EVENTS (sizeof(J9JVMTIExtensionEventInfoTable) / sizeof(J9JVMTIExtensionEventInfoTable[0]))


jvmtiError JNICALL
jvmtiGetExtensionFunctions(jvmtiEnv* env,
	jint* extension_count_ptr,
	jvmtiExtensionFunctionInfo** extensions)
{
	unsigned char* mem;
	jvmtiError rc = JVMTI_ERROR_NONE;
	PORT_ACCESS_FROM_JVMTI(env);
	jint rv_extension_count = 0;
	jvmtiExtensionFunctionInfo *rv_extensions = NULL;

	Trc_JVMTI_jvmtiGetExtensionFunctions_Entry(env, extension_count_ptr, extensions);

	ENSURE_PHASE_ONLOAD_OR_LIVE(env);

	ENSURE_NON_NULL(extension_count_ptr);
	ENSURE_NON_NULL(extensions);

	mem = j9mem_allocate_memory(sizeof(**extensions) * NUM_EXTENSION_FUNCTIONS, J9MEM_CATEGORY_JVMTI_ALLOCATE);
	if (mem == NULL) {
		rc = JVMTI_ERROR_OUT_OF_MEMORY;
	} else {
		J9JVMTIExtensionFunctionInfo* source = GLOBAL_TABLE(J9JVMTIExtensionFunctionInfoTable);
		jvmtiExtensionFunctionInfo* dest = (jvmtiExtensionFunctionInfo*)mem;
		jint i;

		memset(dest, 0, sizeof(**extensions) * NUM_EXTENSION_FUNCTIONS);

		for (i = 0; i < NUM_EXTENSION_FUNCTIONS; i++, dest++, source++) {
			rc = copyExtensionFunctionInfo(env, dest, source);
			if (rc != JVMTI_ERROR_NONE) {
				/* error -- clean up the memory we've already allocated */
				for (; i >= 0; i--, dest--) {
					freeExtensionFunctionInfo(env, dest);
				}
				j9mem_free_memory(mem);
				goto done;
			}
		}

		rv_extension_count = NUM_EXTENSION_FUNCTIONS;
		rv_extensions = (jvmtiExtensionFunctionInfo*)mem;
	}

done:
	if (NULL != extension_count_ptr) {
		*extension_count_ptr = rv_extension_count;
	}
	if (NULL != extensions) {
		*extensions = rv_extensions;
	}
	TRACE_JVMTI_RETURN(jvmtiGetExtensionFunctions);
}


jvmtiError JNICALL
jvmtiGetExtensionEvents(jvmtiEnv* env,
	jint* extension_count_ptr,
	jvmtiExtensionEventInfo** extensions)
{
	jvmtiError rc = JVMTI_ERROR_NONE;
	unsigned char* mem;
	PORT_ACCESS_FROM_JVMTI(env);
	jint rv_extension_count = 0;
	jvmtiExtensionEventInfo *rv_extensions = NULL;

	Trc_JVMTI_jvmtiGetExtensionEvents_Entry(env, extension_count_ptr, extensions);

	ENSURE_PHASE_ONLOAD_OR_LIVE(env);

	ENSURE_NON_NULL(extension_count_ptr);
	ENSURE_NON_NULL(extensions);

	mem = j9mem_allocate_memory(sizeof(**extensions) * NUM_EXTENSION_EVENTS, J9MEM_CATEGORY_JVMTI_ALLOCATE);
	if (mem == NULL) {
		rc = JVMTI_ERROR_OUT_OF_MEMORY;
	} else {
		J9JVMTIExtensionEventInfo* source = GLOBAL_TABLE(J9JVMTIExtensionEventInfoTable);
		jvmtiExtensionEventInfo* dest = (jvmtiExtensionEventInfo*)mem;
		jint i;

		memset(dest, 0, sizeof(**extensions) * NUM_EXTENSION_EVENTS);

		for (i = 0; i < NUM_EXTENSION_EVENTS; i++, dest++, source++) {
			rc = copyExtensionEventInfo(env, dest, source);
			if (rc != JVMTI_ERROR_NONE) {
				/* error -- clean up the memory we've already allocated */
				for (; i >= 0; i--, dest--) {
					freeExtensionEventInfo(env, dest);
				}
				j9mem_free_memory(mem);
				goto done;
			}
		}

		rv_extension_count = NUM_EXTENSION_EVENTS;
		rv_extensions = (jvmtiExtensionEventInfo*)mem;
	}

done:
	if (NULL != extension_count_ptr) {
		*extension_count_ptr = rv_extension_count;
	}
	if (NULL != extensions) {
		*extensions = rv_extensions;
	}
	TRACE_JVMTI_RETURN(jvmtiGetExtensionEvents);
}


jvmtiError JNICALL
jvmtiSetExtensionEventCallback(jvmtiEnv* env,
	jint extension_event_index,
	jvmtiExtensionEvent callback)
{
	J9JVMTIEnv * j9env = (J9JVMTIEnv *) env;
	J9JavaVM * vm = j9env->vm;
	J9VMThread * currentThread;
	jvmtiError rc;

	Trc_JVMTI_jvmtiSetExtensionEventCallback_Entry(env, extension_event_index, callback);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);				
		rc = setEventNotificationMode(j9env, currentThread, (callback == NULL) ? JVMTI_DISABLE : JVMTI_ENABLE, extension_event_index, NULL,
			J9JVMTI_LOWEST_EXTENSION_EVENT, J9JVMTI_HIGHEST_EXTENSION_EVENT);
		if (rc == JVMTI_ERROR_NONE) {
			J9JVMTI_EXTENSION_CALLBACK(j9env, extension_event_index) = (jvmtiExtensionEvent) J9_COMPATIBLE_FUNCTION_POINTER( callback );
		}
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiSetExtensionEventCallback);
}



static jvmtiError
copyString(jvmtiEnv* env, char** dest, const char* source)
{
	unsigned char* mem;
	jvmtiError rc = JVMTI_ERROR_NONE;
	PORT_ACCESS_FROM_JVMTI(env);

	mem = j9mem_allocate_memory(strlen(source) + 1, J9MEM_CATEGORY_JVMTI_ALLOCATE);
	if (mem == NULL) {
		rc = JVMTI_ERROR_OUT_OF_MEMORY;
	} else {
		*dest = (char*)mem;
		strcpy(*dest, source);
	}

	return rc;
}

static jvmtiError 
copyExtensionFunctionInfo(jvmtiEnv* env, jvmtiExtensionFunctionInfo* dest, const J9JVMTIExtensionFunctionInfo* source)
{
	jvmtiError rc;
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	PORT_ACCESS_FROM_JAVAVM(vm);

	dest->func = source->func;

	rc = copyString(env, &dest->id, source->id);
	if (rc != JVMTI_ERROR_NONE) {
		return rc;
	}

	rc = copyString(env, &dest->short_description,
		OMRPORT_FROM_J9PORT(PORTLIB)->nls_lookup_message(
			OMRPORT_FROM_J9PORT(PORTLIB),
			J9NLS_INFO | J9NLS_DO_NOT_APPEND_NEWLINE, 
			source->descriptionModule, 
			source->descriptionNum, 
			NULL));
	if (rc != JVMTI_ERROR_NONE) {
		return rc;
	}

	dest->param_count = source->param_count;
	rc = copyParams(env, &dest->params, source->params, source->param_count);
	if (rc != JVMTI_ERROR_NONE) {
		return rc;
	}

	dest->error_count = source->error_count;
	rc = copyErrors(env, &dest->errors, source->errors, source->error_count);
	if (rc != JVMTI_ERROR_NONE) {
		return rc;
	}

	return rc;
}


static jvmtiError
copyParams(jvmtiEnv* env, jvmtiParamInfo** dest, const jvmtiParamInfo* source, jint count)
{
	unsigned char* mem;
	jvmtiError rc = JVMTI_ERROR_NONE;
	PORT_ACCESS_FROM_JVMTI(env);

	mem = j9mem_allocate_memory(sizeof(**dest) * count, J9MEM_CATEGORY_JVMTI_ALLOCATE);
	if (mem == NULL) {
		rc = JVMTI_ERROR_OUT_OF_MEMORY;
	} else {
		jint i;

		memset(mem, 0, sizeof(**dest) * count);
		*dest = (jvmtiParamInfo*)mem;

		for (i = 0; i < count; i++) {
			rc = copyParam(env, *dest + i, source + i);
			if (rc != JVMTI_ERROR_NONE) {
				/* TODO: free mem */
				break;
			}
		}
	}

	return rc;
}

static jvmtiError
copyParam(jvmtiEnv* env, jvmtiParamInfo* dest, const jvmtiParamInfo* source)
{
	dest->kind = source->kind;
	dest->base_type = source->base_type;
	dest->null_ok = source->null_ok;

	return copyString(env, &dest->name, source->name);
}

static jvmtiError
copyErrors(jvmtiEnv* env, jvmtiError** dest, const jvmtiError* source, jint count)
{
	unsigned char* mem;
	jvmtiError rc = JVMTI_ERROR_NONE;
	PORT_ACCESS_FROM_JVMTI(env);

	mem = j9mem_allocate_memory(sizeof(**dest) * count, J9MEM_CATEGORY_JVMTI_ALLOCATE);
	if (mem == NULL) {
		rc = JVMTI_ERROR_OUT_OF_MEMORY;
	} else {
		memcpy(mem, source, sizeof(**dest) * count);
		*dest = (jvmtiError*)mem;
	}

	return rc;
}

static void 
freeExtensionFunctionInfo(jvmtiEnv* env, jvmtiExtensionFunctionInfo* info)
{
	PORT_ACCESS_FROM_JVMTI(env);
	jint i;

	for (i = 0; i < info->param_count; i++) {
		j9mem_free_memory(info->params[i].name);
	}

	j9mem_free_memory(info->id);
	j9mem_free_memory(info->short_description);
	j9mem_free_memory(info->params);
	j9mem_free_memory(info->errors);
}

static jvmtiError 
copyExtensionEventInfo(jvmtiEnv* env, jvmtiExtensionEventInfo* dest, const J9JVMTIExtensionEventInfo* source)
{
	jvmtiError rc;
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	PORT_ACCESS_FROM_JAVAVM(vm);

	dest->extension_event_index = source->extension_event_index;

	rc = copyString(env, &dest->id, source->id);
	if (rc != JVMTI_ERROR_NONE) {
		return rc;
	}

	rc = copyString(env, &dest->short_description,
		OMRPORT_FROM_J9PORT(PORTLIB)->nls_lookup_message(
			OMRPORT_FROM_J9PORT(PORTLIB),
			J9NLS_INFO | J9NLS_DO_NOT_APPEND_NEWLINE, 
			source->descriptionModule, 
			source->descriptionNum, 
			NULL));
	if (rc != JVMTI_ERROR_NONE) {
		return rc;
	}

	dest->param_count = source->param_count;
	rc = copyParams(env, &dest->params, source->params, source->param_count);
	if (rc != JVMTI_ERROR_NONE) {
		return rc;
	}

	return rc;
}


static void 
freeExtensionEventInfo(jvmtiEnv* env, jvmtiExtensionEventInfo* info)
{
	PORT_ACCESS_FROM_JVMTI(env);
	jint i;

	for (i = 0; i < info->param_count; i++) {
		j9mem_free_memory(info->params[i].name);
	}

	j9mem_free_memory(info->id);
	j9mem_free_memory(info->short_description);
	j9mem_free_memory(info->params);
}



static jvmtiError JNICALL
jvmtiTraceSet(jvmtiEnv* jvmti_env, ...)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(jvmti_env);
	jvmtiError rc = JVMTI_ERROR_NOT_AVAILABLE;
	RasGlobalStorage * j9ras = (RasGlobalStorage *)vm->j9rasGlobalStorage;
	J9VMThread * currentThread;
	
	const char* option_ptr;
	va_list args;

	va_start(args, jvmti_env);
	option_ptr = va_arg(args, const char*);
	va_end(args);

	Trc_JVMTI_jvmtiTraceSet_Entry(jvmti_env, option_ptr);

	ENSURE_PHASE_NOT_DEAD(jvmti_env);
	ENSURE_NON_NULL(option_ptr);

	rc = getCurrentVMThread(vm, &currentThread);
	
	if (JVMTI_ERROR_NONE != rc) {
		goto done;
	}
	
	/* Is trace running? */
	if ( j9ras && j9ras->configureTraceEngine) {
		/* Pass option to the trace engine */
		omr_error_t result = j9ras->configureTraceEngine(currentThread, option_ptr);
		/* Map back to JVMTI error code */
		switch (result) {
			case OMR_ERROR_NONE:
				rc = JVMTI_ERROR_NONE;
				break;
			case OMR_ERROR_OUT_OF_NATIVE_MEMORY:
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
				break;
			case OMR_ERROR_ILLEGAL_ARGUMENT:
				rc = JVMTI_ERROR_ILLEGAL_ARGUMENT;
				break;
			case OMR_ERROR_INTERNAL:
			default:
				rc = JVMTI_ERROR_INTERNAL;
				break;
		}
	}

done:
	TRACE_JVMTI_RETURN(jvmtiTraceSet);
}

static jvmtiError JNICALL
jvmtiDumpSet(jvmtiEnv* jvmti_env, ...)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(jvmti_env);
	jvmtiError rc = JVMTI_ERROR_NOT_AVAILABLE;
	omr_error_t result = OMR_ERROR_NONE;

	char* option_ptr;
	va_list args;

	va_start(args, jvmti_env);
	option_ptr = va_arg(args, char*);
	va_end(args);

	Trc_JVMTI_jvmtiDumpSet_Entry(jvmti_env, option_ptr);

	ENSURE_PHASE_NOT_DEAD(jvmti_env);
	ENSURE_NON_NULL(option_ptr);

#ifdef J9VM_RAS_DUMP_AGENTS

	/* Pass option to the dump facade */
	result = vm->j9rasDumpFunctions->setDumpOption(vm, option_ptr);

	/* Map back to JVMTI error code */
	switch (result) {
	case OMR_ERROR_NONE:
		rc = JVMTI_ERROR_NONE;
		break;
	case OMR_ERROR_INTERNAL:
		rc = JVMTI_ERROR_ILLEGAL_ARGUMENT;
		break;
	case OMR_ERROR_NOT_AVAILABLE:
		rc = JVMTI_ERROR_NOT_AVAILABLE;
		break;
	default:
		rc = JVMTI_ERROR_INTERNAL;
		break;
	}

#endif /* J9VM_RAS_DUMP_AGENTS */

done:
	TRACE_JVMTI_RETURN(jvmtiDumpSet);
}

static jvmtiError JNICALL
jvmtiResetVmDump(jvmtiEnv* jvmti_env, ...)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(jvmti_env);
	jvmtiError rc = JVMTI_ERROR_NOT_AVAILABLE;
	omr_error_t result = OMR_ERROR_NONE;

	Trc_JVMTI_jvmtiResetVmDump_Entry(jvmti_env);

	ENSURE_PHASE_NOT_DEAD(jvmti_env);

#ifdef J9VM_RAS_DUMP_AGENTS
	/* request dump reset from dump module */
	result = vm->j9rasDumpFunctions->resetDumpOptions(vm);

	/* Map back to JVMTI error code */
	switch (result) {
	case OMR_ERROR_NONE:
		rc = JVMTI_ERROR_NONE;
		break;
	case OMR_ERROR_INTERNAL:
		rc = JVMTI_ERROR_ILLEGAL_ARGUMENT;
		break;
	case OMR_ERROR_NOT_AVAILABLE:
		rc = JVMTI_ERROR_NOT_AVAILABLE;
		break;
	default:
		rc = JVMTI_ERROR_INTERNAL;
		break;
	}

#endif /* J9VM_RAS_DUMP_AGENTS */	

done:
	TRACE_JVMTI_RETURN(jvmtiResetVmDump);
}

/*
 * Writes a string of newline separated dump agent specifications into the supplied memory.
 * The length of the string (including null terminator) is written into the data_size.
 * In the event of too little memory supplied to receive the specification string, the data
 * is not written, however the size of the require memory is.
 * 
 *   buffer_size - the size of the supplied memory
 *   options_buffer - pointer to the memory to receive the specification string.
 *   data_size - pointer to an integer to receive the length of the specification string.
 *    
 * Return values with meaning specific to this function:
 *	JVMTI_ERROR_NONE - success
 *	JVMTI_ERROR_NULL_POINTER - a null pointer has been passed as a parameter.
 *	JVMTI_ERROR_WRONG_PHASE - function cannot be run in JVMTI_DEAD_PHASE
 *	JVMTI_ERROR_ILLEGAL_ARGUMENT - an invalid parameter has been passed
 *	JVMTI_ERROR_OUT_OF_MEMORY - system could not allocate enough memory
 *	JVMTI_ERROR_INTERNAL
 */
static jvmtiError JNICALL
jvmtiQueryVmDump(jvmtiEnv* jvmti_env, jint buffer_size, void* options_buffer, jint* data_size_ptr, ...)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(jvmti_env);
	jvmtiError rc = JVMTI_ERROR_NOT_AVAILABLE;
	omr_error_t result = OMR_ERROR_NONE;

	Trc_JVMTI_jvmtiQueryVmDump_Entry(jvmti_env);

	ENSURE_PHASE_NOT_DEAD(jvmti_env);

#ifdef J9VM_RAS_DUMP_AGENTS
	/* request dump reset from dump module */
	
	result = vm->j9rasDumpFunctions->queryVmDump(vm, buffer_size, options_buffer, data_size_ptr);

	/* Map back to JVMTI error code */
	switch (result) {
	case OMR_ERROR_NONE:
		rc = JVMTI_ERROR_NONE;
		break;
	case OMR_ERROR_INTERNAL:
		rc = JVMTI_ERROR_ILLEGAL_ARGUMENT;
		break;
	case OMR_ERROR_OUT_OF_NATIVE_MEMORY:
		rc = JVMTI_ERROR_OUT_OF_MEMORY;
		break;
	case OMR_ERROR_ILLEGAL_ARGUMENT:
		rc = JVMTI_ERROR_NULL_POINTER;
		break;
	default:
		rc = JVMTI_ERROR_INTERNAL;
		break;
	}

#endif /* J9VM_RAS_DUMP_AGENTS */	

done:
	TRACE_JVMTI_RETURN(jvmtiQueryVmDump);
}

static jvmtiError JNICALL
jvmtiJlmSet(jvmtiEnv* env, jint option, ...)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_NOT_AVAILABLE;
#if defined(OMR_THR_JLM)	
	J9VMThread * currentThread;
#endif	

	Trc_JVMTI_jvmtiJlmSet_Entry(env, option);
	ENSURE_PHASE_ONLOAD_OR_LIVE(env);

#if defined(OMR_THR_JLM)
	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		jint jlmRC = 0;
		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);
		vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);
		switch (option) {
				case COM_IBM_JLM_START:
					/* profiler command jlmstartlite */
					jlmRC = JlmStart(currentThread);
					if (jlmRC != JLM_SUCCESS) {
						rc = JVMTI_ERROR_NOT_AVAILABLE;
					}
					break;

				case COM_IBM_JLM_START_TIME_STAMP:
				/*
				 * profiler command 'jlmstart'
				 *
				 * If hold time supported, use it, otherwise default to 
				 * lesser functionality
				 */
#if defined(OMR_THR_JLM_HOLD_TIMES)
					jlmRC = JlmStartTimeStamps();
#else
					jlmRC = JlmStart(currentThread);
#endif
					if (jlmRC) {
						rc = JVMTI_ERROR_NOT_AVAILABLE;
					}
					break;
				case COM_IBM_JLM_STOP:
					jlmRC = JlmStop();
					if (jlmRC != JLM_SUCCESS) {
						rc = JVMTI_ERROR_NOT_AVAILABLE;
					}
					break;
				case COM_IBM_JLM_STOP_TIME_STAMP:
#if defined(OMR_THR_JLM_HOLD_TIMES)
					jlmRC = JlmStopTimeStamps();
#else
					jlmRC = JlmStop();
#endif
					if (jlmRC != JLM_SUCCESS) {
						rc = JVMTI_ERROR_NOT_AVAILABLE;
					}
					break;
				default:
					rc = JVMTI_ERROR_INTERNAL;
					break;
		}
	
		vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

#endif /* OMR_THR_JLM */

done:
	TRACE_JVMTI_RETURN(jvmtiJlmSet);
}


static jvmtiError JNICALL
jvmtiJlmDump(jvmtiEnv* env, void ** dump_info,...)
{
	jvmtiError rc = JVMTI_ERROR_NOT_AVAILABLE;

	Trc_JVMTI_jvmtiJlmDump_Entry(env, dump_info);
	ENSURE_PHASE_LIVE(env);
	ENSURE_NON_NULL(dump_info);

#if defined(OMR_THR_JLM)	
	rc = jvmtiJlmDumpHelper(env, dump_info, COM_IBM_JLM_DUMP_FORMAT_OBJECT_ID);		
#endif /* OMR_THR_JLM */

done:
	TRACE_JVMTI_RETURN(jvmtiJlmDump);
}

static jvmtiError JNICALL
jvmtiTriggerVmDump(jvmtiEnv* jvmti_env, ...)
{
#ifdef J9VM_RAS_DUMP_AGENTS
	omr_error_t result = OMR_ERROR_NONE;
	jvmtiError rc;
	J9JVMTIEnv * j9env = (J9JVMTIEnv *) jvmti_env;
	J9JavaVM * vm = j9env->vm;
	char* option_ptr;
	va_list args;

	va_start(args, jvmti_env);
	option_ptr = va_arg(args, char*);
	va_end(args);

	Trc_JVMTI_jvmtiTriggerVmDump_Entry(jvmti_env, option_ptr);

	ENSURE_PHASE_LIVE(jvmti_env);
	ENSURE_NON_NULL(option_ptr);

	result = vm->j9rasDumpFunctions->triggerOneOffDump(vm, option_ptr, "JVMTI", NULL, 0);

	/* Map back to JVMTI error code */
	switch (result) {
	case OMR_ERROR_NONE:
		rc = JVMTI_ERROR_NONE;
		break;
	case OMR_ERROR_INTERNAL:
		rc = JVMTI_ERROR_ILLEGAL_ARGUMENT;
		break;
	default:
		rc = JVMTI_ERROR_INTERNAL;
		break;
	}
done:
	TRACE_JVMTI_RETURN(jvmtiTriggerVmDump);
#else
	return JNI_ERR;
#endif /* J9VM_RAS_DUMP_AGENTS */
}

static jvmtiError JNICALL
jvmtiGetOSThreadID(jvmtiEnv* jvmti_env, jthread thread, jlong * threadid_ptr, ...)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(jvmti_env);
	jvmtiError rc;
	J9VMThread * currentThread;
	jlong rv_threadid = 0;

	Trc_JVMTI_jvmtiGetOSThreadID_Entry(jvmti_env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9VMThread * targetThread;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(jvmti_env);
		ENSURE_NON_NULL(threadid_ptr);

		rc = getVMThread(currentThread, thread, &targetThread, TRUE, TRUE);
		if (rc == JVMTI_ERROR_NONE) {
			rv_threadid = (jlong) omrthread_get_osId(targetThread->osThread);
			releaseVMThread(currentThread, targetThread);
		}
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != threadid_ptr) {
		*threadid_ptr = rv_threadid;
	}
	TRACE_JVMTI_RETURN(jvmtiGetOSThreadID);
}

static jvmtiError JNICALL
jvmtiGetStackTraceExtended(jvmtiEnv* env,
	jint type,
	jthread thread,
	jint start_depth,
	jint max_frame_count,
	void* frame_buffer,
	jint* count_ptr, ...)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;
	jint rv_count = 0;

	Trc_JVMTI_jvmtiGetStackTraceExtended_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9VMThread * targetThread;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);

		ENSURE_NON_NEGATIVE(max_frame_count);
		ENSURE_NON_NULL(frame_buffer);
		ENSURE_NON_NULL(count_ptr);

		rc = getVMThread(currentThread, thread, &targetThread, TRUE, TRUE);
		if (rc == JVMTI_ERROR_NONE) {
			vm->internalVMFunctions->haltThreadForInspection(currentThread, targetThread);

			rc = jvmtiInternalGetStackTraceExtended(env, type, currentThread, targetThread, start_depth, (UDATA) max_frame_count, frame_buffer, &rv_count);

			vm->internalVMFunctions->resumeThreadForInspection(currentThread, targetThread);
			releaseVMThread(currentThread, targetThread);
		}
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != count_ptr) {
		*count_ptr = rv_count;
	}
	TRACE_JVMTI_RETURN(jvmtiGetStackTraceExtended);
}


static jvmtiError JNICALL
jvmtiGetAllStackTracesExtended(jvmtiEnv* env,
	jint type,
	jint max_frame_count,
	void** stack_info_ptr,
	jint* thread_count_ptr, ...)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;
	PORT_ACCESS_FROM_JAVAVM(vm);
	void *rv_stack_info = NULL;
	jint rv_thread_count = 0;

	Trc_JVMTI_jvmtiGetAllStackTracesExtended_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		UDATA threadCount;
		jvmtiStackInfoExtended * stackInfo;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);

		ENSURE_NON_NEGATIVE(max_frame_count);
		ENSURE_NON_NULL(stack_info_ptr);
		ENSURE_NON_NULL(thread_count_ptr);

		vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);

		threadCount = vm->totalThreadCount;
		stackInfo = j9mem_allocate_memory(((sizeof(jvmtiStackInfoExtended) + (max_frame_count * sizeof(jvmtiFrameInfoExtended))) * threadCount) + sizeof(jlocation), J9MEM_CATEGORY_JVMTI_ALLOCATE);
		if (stackInfo == NULL) {
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
		} else {
			jvmtiFrameInfoExtended * currentFrameInfo = (jvmtiFrameInfoExtended *) ((((UDATA) (stackInfo + threadCount)) + sizeof(jlocation)) & ~sizeof(jlocation));
			jvmtiStackInfoExtended * currentStackInfo = stackInfo;
			J9VMThread * targetThread;

			targetThread = vm->mainThread;
			do {
				/* If threadObject is NULL, ignore this thread */

				if (targetThread->threadObject == NULL) {
					--threadCount;
				} else {
					rc = jvmtiInternalGetStackTraceExtended(env, type, currentThread, targetThread, 0, (UDATA) max_frame_count, (void *) currentFrameInfo, &(currentStackInfo->frame_count));
					if (rc != JVMTI_ERROR_NONE) {
						j9mem_free_memory(stackInfo);
						goto fail;
					}

					currentStackInfo->thread = (jthread) vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *) currentThread, (j9object_t) targetThread->threadObject);
					currentStackInfo->state = getThreadState(currentThread, targetThread->threadObject);
					currentStackInfo->frame_buffer = currentFrameInfo;

					++currentStackInfo;
					currentFrameInfo += max_frame_count;
				}
			} while ((targetThread = targetThread->linkNext) != vm->mainThread);

			rv_stack_info = stackInfo;
			rv_thread_count = (jint) threadCount;
		}
fail:
		vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);

done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != stack_info_ptr) {
		*stack_info_ptr = rv_stack_info;
	}
	if (NULL != thread_count_ptr) {
		*thread_count_ptr = rv_thread_count;
	}
	TRACE_JVMTI_RETURN(jvmtiGetAllStackTracesExtended);
}


static jvmtiError JNICALL
jvmtiGetThreadListStackTracesExtended(jvmtiEnv* env,
	jint type,
	jint thread_count,
	const jthread* thread_list,
	jint max_frame_count,
	void** stack_info_ptr, ...)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;
	PORT_ACCESS_FROM_JAVAVM(vm);
	void *rv_stack_info = NULL;

	Trc_JVMTI_jvmtiGetThreadListStackTracesExtended_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		jvmtiStackInfoExtended * stackInfo;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);

		ENSURE_NON_NEGATIVE(thread_count);
		ENSURE_NON_NULL(thread_list);
		ENSURE_NON_NEGATIVE(max_frame_count);
		ENSURE_NON_NULL(stack_info_ptr);

		vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);

		stackInfo = j9mem_allocate_memory(((sizeof(jvmtiStackInfoExtended) + (max_frame_count * sizeof(jvmtiFrameInfoExtended))) * thread_count) + sizeof(jlocation), J9MEM_CATEGORY_JVMTI_ALLOCATE);
		if (stackInfo == NULL) {
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
		} else {
			jvmtiFrameInfoExtended * currentFrameInfo = (jvmtiFrameInfoExtended *) ((((UDATA) (stackInfo + thread_count)) + sizeof(jlocation)) & ~sizeof(jlocation));
			jvmtiStackInfoExtended * currentStackInfo = stackInfo;
			
			while (thread_count != 0) {
				jthread thread = *thread_list;
				J9VMThread * targetThread;
				j9object_t threadObject;

				if (thread == NULL) {
					rc = JVMTI_ERROR_NULL_POINTER;
					goto deallocate;
				}
				threadObject = *((j9object_t*) thread);
				targetThread = J9VMJAVALANGTHREAD_THREADREF(currentThread, threadObject);
				if (targetThread == NULL) {
					currentStackInfo->frame_count = 0;
				} else {
					rc = jvmtiInternalGetStackTraceExtended(env, type, currentThread, targetThread, 0, (UDATA) max_frame_count, (void *) currentFrameInfo, &(currentStackInfo->frame_count));
					if (rc != JVMTI_ERROR_NONE) {
deallocate:
						j9mem_free_memory(stackInfo);
						goto fail;
					}
				}
				currentStackInfo->thread = thread;
				currentStackInfo->state = getThreadState(currentThread, threadObject);
				currentStackInfo->frame_buffer = currentFrameInfo;

				++thread_list;
				--thread_count;
				++currentStackInfo;
				currentFrameInfo += max_frame_count;
			}

			rv_stack_info = stackInfo;
		}
fail:
		vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);

done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != stack_info_ptr) {
		*stack_info_ptr = rv_stack_info;
	}
	TRACE_JVMTI_RETURN(jvmtiGetThreadListStackTracesExtended);
}


static jvmtiError
jvmtiInternalGetStackTraceExtended(jvmtiEnv* env,
	J9JVMTIStackTraceType type,
	J9VMThread * currentThread,
	J9VMThread * targetThread,
	jint start_depth,
	UDATA max_frame_count,
	jvmtiFrameInfo* frame_buffer,
	jint* count_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9StackWalkState walkState;
	UDATA framesWalked;
	UDATA basicFlags;

	walkState.walkThread = targetThread;
	basicFlags = J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_ITERATE_FRAMES;
	if (type & J9JVMTI_STACK_TRACE_PRUNE_UNREPORTED_METHODS) {
		basicFlags |= J9_STACKWALK_SKIP_INLINES;
	}
	walkState.flags = basicFlags;
	walkState.frameWalkFunction = jvmtiInternalGetStackTraceIteratorExtended;
	walkState.skipCount = 0;
	walkState.userData1 = NULL;
	walkState.userData2 = (void *) type;
	walkState.userData3 = (void *) 0;
	walkState.userData4 = (void *) 0;
	vm->walkStackFrames(currentThread, &walkState);
	framesWalked = (UDATA) walkState.userData3;
	if (start_depth == 0) {
		/* This violates the spec, but matches JDK behaviour - allows querying an empty stack with start_depth == 0 */
		walkState.skipCount = 0;
	} else if (start_depth > 0) {
		if (((UDATA) start_depth) >= framesWalked) {
			return JVMTI_ERROR_ILLEGAL_ARGUMENT;
		}
		walkState.skipCount = (UDATA) start_depth;
	} else {
		if (((UDATA) -start_depth) > framesWalked) {
			return JVMTI_ERROR_ILLEGAL_ARGUMENT;
		}
		walkState.skipCount = framesWalked + start_depth;
	}

	walkState.flags = basicFlags | J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET;
	walkState.userData1 = frame_buffer;
	walkState.userData2 = (void *) type;
	walkState.userData3 = (void *) 0;
	walkState.userData4 = (void *) max_frame_count;
	vm->walkStackFrames(currentThread, &walkState);
	if (walkState.userData1 == NULL) {
		return JVMTI_ERROR_OUT_OF_MEMORY;
	}
	*count_ptr = (jint)(UDATA) walkState.userData3;

	return JVMTI_ERROR_NONE;
}


static UDATA
jvmtiInternalGetStackTraceIteratorExtended(J9VMThread * currentThread, J9StackWalkState * walkState)
{
	J9JVMTIStackTraceType type;
	jmethodID methodID;
	jvmtiFrameInfoExtended * frame_buffer;
	UDATA frameCount;

	/* In extra info mode when method enter is enabled, exclude natives which have not had method enter reported for them */

	type = (J9JVMTIStackTraceType) walkState->userData2;
	if (type & J9JVMTI_STACK_TRACE_PRUNE_UNREPORTED_METHODS) {
		if ((UDATA)walkState->pc == J9SF_FRAME_TYPE_NATIVE_METHOD) {
			/* INL natives never have enter/exit reported */
			return J9_STACKWALK_KEEP_ITERATING;
		}
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
		if ((UDATA)walkState->pc == J9SF_FRAME_TYPE_JNI_NATIVE_METHOD) {
			if (walkState->frameFlags & J9_STACK_FLAGS_JIT_JNI_CALL_OUT_FRAME) {
				/* Direct JNI inlined into JIT method */
				return J9_STACKWALK_KEEP_ITERATING;
			}
		}
		/* Direct JNI thunks (method is native, jitInfo != NULL) do have enter/exit reported */
#endif
	}

	frame_buffer = walkState->userData1;
	if (frame_buffer != NULL) {
		methodID = getCurrentMethodID(currentThread, walkState->method);
		if (methodID == NULL) {
			walkState->userData1 = NULL;
			return J9_STACKWALK_STOP_ITERATING;
		} 

		frame_buffer->method = methodID;
		
		if (type & J9JVMTI_STACK_TRACE_EXTRA_FRAME_INFO) {
			/* Fill in the extended data  */
#ifdef J9VM_INTERP_NATIVE_SUPPORT 
			if (walkState->jitInfo == NULL) {
				frame_buffer->type = COM_IBM_STACK_FRAME_EXTENDED_NOT_JITTED;
			} else {
				frame_buffer->type = COM_IBM_STACK_FRAME_EXTENDED_JITTED;
			}	
#else
			frame_buffer->type = COM_IBM_STACK_FRAME_EXTENDED_NOT_JITTED;			
#endif
			frame_buffer->machinepc = -1; /* not supported yet */
	}			

		if (type & J9JVMTI_STACK_TRACE_ENTRY_LOCAL_STORAGE) {
#ifdef J9VM_INTERP_NATIVE_SUPPORT 
			if ((jlocation) walkState->bytecodePCOffset == -1) {
				frame_buffer->nativeFrameAddress = (void *) walkState->walkedEntryLocalStorage;
			} else {
				frame_buffer->nativeFrameAddress = NULL;	
			}
#else
			frame_buffer->nativeFrameAddress = NULL;	
#endif
		}	

		/* The location = -1 for native method case is handled in the stack walker */		
		frame_buffer->location = (jlocation) walkState->bytecodePCOffset;
	
		/* If the location specifies a JBinvokeinterface, back it up to the JBinvokeinterface2 */
		if (!IS_SPECIAL_FRAME_PC(walkState->pc)) {
			if (*(walkState->pc) == JBinvokeinterface) {
				frame_buffer->location -= 2;
			}
		}

		walkState->userData1 = frame_buffer + 1;
	}

	frameCount = (UDATA) walkState->userData3;
	++frameCount;
	walkState->userData3 = (void *) frameCount;
	if (frameCount == (UDATA) walkState->userData4) {
		return J9_STACKWALK_STOP_ITERATING;
	}
	return J9_STACKWALK_KEEP_ITERATING;
}

static jvmtiError JNICALL
jvmtiGetHeapFreeMemory(jvmtiEnv* jvmti_env, jlong* heapFree_ptr, ...)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(jvmti_env);
	jvmtiError rc;
	jlong rv_heapFree = 0;

	Trc_JVMTI_jvmtiGetHeapFreeMemory_Entry(jvmti_env);

	ENSURE_PHASE_START_OR_LIVE(jvmti_env);
	ENSURE_NON_NULL(heapFree_ptr);
	rv_heapFree = vm->memoryManagerFunctions->j9gc_heap_free_memory(vm);
	rc = JVMTI_ERROR_NONE;
	
done:
	if (NULL != heapFree_ptr) {
		*heapFree_ptr = rv_heapFree;
	}
	TRACE_JVMTI_RETURN(jvmtiGetHeapFreeMemory);
}

static jvmtiError JNICALL
jvmtiGetHeapTotalMemory(jvmtiEnv* jvmti_env, jlong* heapTotal_ptr, ...)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(jvmti_env);
	jvmtiError rc;
	jlong rv_heapTotal = 0;

	Trc_JVMTI_jvmtiGetHeapTotalMemory_Entry(jvmti_env);

	ENSURE_PHASE_START_OR_LIVE(jvmti_env);
	ENSURE_NON_NULL(heapTotal_ptr);
	rv_heapTotal = vm->memoryManagerFunctions->j9gc_heap_total_memory(vm);
	rc = JVMTI_ERROR_NONE;
	
done:
	if (NULL != heapTotal_ptr) {
		*heapTotal_ptr = rv_heapTotal;
	}
	TRACE_JVMTI_RETURN(jvmtiGetHeapTotalMemory);
}

#if defined(J9VM_OPT_SHARED_CLASSES)
struct IterateSharedCacheUserdata {
	jvmtiEnv *env;
	jvmtiIterateSharedCachesCallback callback;
	void *userdata;
	I_32 version;
};

/**
 * Callback called for each cache on the system by the shared classes code. We then populate 
 * a jvmtiSharedCacheInfo structure and call the user's callback.
 */
IDATA
iterateSharedCachesCallback(J9JavaVM *vm, J9SharedCacheInfo *event_data, void *user_data)
{
	struct IterateSharedCacheUserdata *userdata = (struct IterateSharedCacheUserdata *)user_data;
	jvmtiSharedCacheInfo cacheInfo;
	
	memset(&cacheInfo, 0, sizeof(jvmtiSharedCacheInfo));
	cacheInfo.name = event_data->name;
	cacheInfo.isCompatible = (jboolean) event_data->isCompatible;
	cacheInfo.isPersistent = (jboolean) (J9PORT_SHR_CACHE_TYPE_PERSISTENT == event_data->cacheType);
	cacheInfo.modLevel = (jint) event_data->modLevel;
	cacheInfo.isCorrupt = (jboolean)(0 < (IDATA)event_data->isCorrupt);
	cacheInfo.os_shmid = (jint) event_data->os_shmid;
	cacheInfo.os_semid = (jint) event_data->os_semid;
	cacheInfo.lastDetach = (jlong) event_data->lastDetach;
	cacheInfo.cacheSize = (-1 == event_data->cacheSize) ? (jlong) -1 : (jlong) event_data->cacheSize;
	cacheInfo.freeBytes = (-1 == event_data->freeBytes) ? (jlong) -1 : (jlong) event_data->freeBytes;
	if (COM_IBM_ITERATE_SHARED_CACHES_VERSION_2 <= userdata->version) {
		cacheInfo.cacheType = (jint) event_data->cacheType;
	}
	if (userdata->version < COM_IBM_ITERATE_SHARED_CACHES_VERSION_3) {
		cacheInfo.addrMode = (jint) COM_IBM_ITERATE_SHARED_CACHES_GET_ADDR_MODE(event_data->addrMode);
	} else {
		cacheInfo.addrMode = (jint) event_data->addrMode;
	}
	
	if (COM_IBM_ITERATE_SHARED_CACHES_VERSION_4 <= userdata->version) {
		cacheInfo.softMaxBytes = ((UDATA)-1 == event_data->softMaxBytes) ? (jlong) -1 : (jlong) event_data->softMaxBytes;
	}
	if (JNI_OK != userdata->callback(userdata->env, &cacheInfo, userdata->userdata)) {
		return -1;
	}
	
	return 0;
}
#endif /* J9VM_OPT_SHARED_CLASSES */

/**
 * Extension function called by users to iterate over shared caches.
 * callback - a user-supplied callback function which we will call for each cache.
 * user_data - opaque user-supplied pointer which will be passed to their callback.
 */ 
static jvmtiError JNICALL
jvmtiIterateSharedCaches(jvmtiEnv *env,
	jint version,
	const char *cacheDir,
	jint flags,
	jboolean useCommandLineValues,
	jvmtiIterateSharedCachesCallback callback,
	void *user_data, ...)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_NOT_AVAILABLE;
#if defined(J9VM_OPT_SHARED_CLASSES)
	struct IterateSharedCacheUserdata userdata;
#endif
	
	Trc_JVMTI_jvmtiIterateSharedCaches_Entry(env, callback, user_data);
	
	if ((COM_IBM_ITERATE_SHARED_CACHES_VERSION_1 != version)
		&& (COM_IBM_ITERATE_SHARED_CACHES_VERSION_2 != version)
		&& (COM_IBM_ITERATE_SHARED_CACHES_VERSION_3 != version)
		&& (COM_IBM_ITERATE_SHARED_CACHES_VERSION_4 != version)
	) {
		rc = JVMTI_ERROR_UNSUPPORTED_VERSION;
		goto done;
	}

	if (COM_IBM_ITERATE_SHARED_CACHES_NO_FLAGS != flags) {
		rc = JVMTI_ERROR_ILLEGAL_ARGUMENT;
		goto done;
	}

#if defined(J9VM_OPT_SHARED_CLASSES)
	ENSURE_PHASE_LIVE(env);
	ENSURE_NON_NULL(callback);
	
	if (NULL == vm->sharedCacheAPI) {
		rc = JVMTI_ERROR_NOT_AVAILABLE;
		goto done;
	}
	
	userdata.env = env;
	userdata.callback = callback;
	userdata.userdata = user_data;
	userdata.version = (I_32)version;
	
	if (-1 == vm->sharedCacheAPI->iterateSharedCaches(vm, cacheDir, (UDATA)flags, useCommandLineValues, &iterateSharedCachesCallback, &userdata)) {
		rc = JVMTI_ERROR_INTERNAL;
		goto done;
	}
	
	rc = JVMTI_ERROR_NONE;
#endif /* J9VM_OPT_SHARED_CLASSES */

done:
	TRACE_JVMTI_RETURN(jvmtiIterateSharedCaches);
}

/**
 * Extension function called by users to delete a shared cache.
 * name - the name of the cache to delete
 */ 
static jvmtiError JNICALL
jvmtiDestroySharedCache(jvmtiEnv *env,
	const char *cacheDir,
	const char *name,
	jint cacheType,
	jboolean useCommandLineValues,
	jint *internalErrorCode, ...)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_NOT_AVAILABLE;
	IDATA sharedCacheRC = 0;
	jint rv_internalErrorCode = 0;
	
	Trc_JVMTI_jvmtiDestroySharedCache_Entry(env, name);

#if defined(J9VM_OPT_SHARED_CLASSES)
	ENSURE_PHASE_LIVE(env);

	if (NULL == vm->sharedCacheAPI) {
		rc = JVMTI_ERROR_NOT_AVAILABLE;
		goto done;
	}
	
	if ((COM_IBM_SHARED_CACHE_PERSISTENCE_DEFAULT != cacheType) &&
		(COM_IBM_SHARED_CACHE_PERSISTENT != cacheType) &&
		(COM_IBM_SHARED_CACHE_NONPERSISTENT != cacheType) &&
		(COM_IBM_SHARED_CACHE_SNAPSHOT != cacheType)
	) {
		rc = JVMTI_ERROR_ILLEGAL_ARGUMENT;
		goto done;
	}

	sharedCacheRC = vm->sharedCacheAPI->destroySharedCache(vm, cacheDir, name, cacheType, useCommandLineValues);
	rv_internalErrorCode = (jint)sharedCacheRC;
	if (COM_IBM_DESTROYED_ALL_CACHE != sharedCacheRC) {
		rc = JVMTI_ERROR_INTERNAL;
		goto done;
	}
	
	rc = JVMTI_ERROR_NONE;
done:
#endif /* J9VM_OPT_SHARED_CLASSES */

	if (NULL != internalErrorCode) {
		*internalErrorCode = rv_internalErrorCode;
	}
	TRACE_JVMTI_RETURN(jvmtiDestroySharedCache);
}

static jvmtiError JNICALL
jvmtiRemoveAllTags(jvmtiEnv* jvmti_env, ...)
{
	J9JVMTIEnv * j9env = (J9JVMTIEnv *) jvmti_env;
	jvmtiError rc = JVMTI_ERROR_NOT_AVAILABLE;

	J9HashTableHashFn hashFn = j9env->objectTagTable->hashFn;
	J9HashTableEqualFn hashEqualFn = j9env->objectTagTable->hashEqualFn;


	Trc_JVMTI_jvmtiRemoveAllTags_Entry(jvmti_env);

	/* Ensure exclusive access to tag table */
	omrthread_monitor_enter(j9env->mutex);

	if (j9env->objectTagTable != NULL) {
		hashTableFree(j9env->objectTagTable);
		j9env->objectTagTable = hashTableNew(OMRPORT_FROM_J9PORT(j9env->vm->portLibrary), J9_GET_CALLSITE(), 0, sizeof(J9JVMTIObjectTag), sizeof(jlong), 0,  J9MEM_CATEGORY_JVMTI, hashFn, hashEqualFn, NULL, NULL);
		rc = JVMTI_ERROR_NONE;
	}

	omrthread_monitor_exit(j9env->mutex);

	TRACE_JVMTI_RETURN(jvmtiRemoveAllTags);
}



/*
 * Struct to insulate trace from jvmti types
 */
typedef struct UserDataWrapper {
	J9PortLibrary			*portlib;
	jvmtiTraceSubscriber	 subscriber;
	jvmtiTraceAlarm			 alarm;
	jvmtiEnv				*env;
	void					*userData;
} UserDataWrapper;

/*
 * Wrapper to insulate trace from jvmti types.
 */
omr_error_t 
subscriberWrapper(UtSubscription *sub) 
{
	jvmtiError rc = JVMTI_ERROR_NONE;
	omr_error_t result = OMR_ERROR_NONE;
	UserDataWrapper *wrapper = (UserDataWrapper*)sub->userData;
	
	rc = wrapper->subscriber(wrapper->env, sub->data, sub->dataLength, wrapper->userData);
	
	switch (rc) {
	case JVMTI_ERROR_NONE:
		result = OMR_ERROR_NONE;
		break;
	case JVMTI_ERROR_OUT_OF_MEMORY:
		result = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		break;
	case JVMTI_ERROR_ILLEGAL_ARGUMENT:
		result = OMR_ERROR_ILLEGAL_ARGUMENT;
		break;
	case JVMTI_ERROR_NOT_AVAILABLE:
		result = OMR_ERROR_NOT_AVAILABLE;
		break;
	case JVMTI_ERROR_INTERNAL: /* FALLTHRU */
	default:
		result = OMR_ERROR_INTERNAL;
		break;
	}
	
	return result;
}

/*
 * Wrapper to insulate trace from jvmti types.
 */
void alarmWrapper(UtSubscription *sub) {
	UserDataWrapper *wrapper = (UserDataWrapper*)sub->userData;
	
	PORT_ACCESS_FROM_PORT(wrapper->portlib);
	
	if (wrapper->alarm != NULL) {
		wrapper->alarm(wrapper->env, sub, wrapper->userData);
	}
	
	j9mem_free_memory(sub->userData);
}

/*
 * Registers a callback function for processing trace buffers with the UTE.
 * Inputs:
 *   description - a textual name for the agent that will appear in javacores
 *   subscriber - the callback function. Prototype and semantics are defined below.
 *   alarm - a callback that's executed if the subscriber returns an error or misbehaves, maybe be NULL.
 *   userData - data to pass to subscriber and the alarm function, may be NULL.
 * Outputs:
 *   subscriptionID - the ID of the subscription, needed to unsubscribe.
 *    
 * Return values with meaning specific to this function:
 *	JVMTI_ERROR_NONE - subscriber registered
 *	JVMTI_ERROR_INVALID_ENVIRONMENT - trace is not present or enabled
 *
 * J9JVMTIExtensionFunctionInfo.id = "com.ibm.RegisterTraceSubscriber"
 */
static jvmtiError JNICALL
jvmtiRegisterTraceSubscriber(jvmtiEnv* env, char *description, jvmtiTraceSubscriber subscriber, jvmtiTraceAlarm alarm, void *userData, void **subscriptionID, ...)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_INVALID_ENVIRONMENT;
	J9VMThread * currentThread;
	void *rv_subscriptionID = NULL;

	RasGlobalStorage *j9ras;
	UtInterface *uteInterface;

	Trc_JVMTI_jvmtiRegisterTraceSubscriber_Entry(env, description, subscriber, alarm, userData, subscriptionID);

	ENSURE_PHASE_START_OR_LIVE(env);
	ENSURE_NON_NULL(subscriber);
	ENSURE_NON_NULL(subscriptionID);
	ENSURE_NON_NULL(description);
	
	rc = getCurrentVMThread(vm, &currentThread);
	if (rc != JVMTI_ERROR_NONE) {
		/* Cannot see any trace from this env */
		rc = JVMTI_ERROR_INVALID_ENVIRONMENT;
		goto done;
	}


	j9ras = (RasGlobalStorage *)vm->j9rasGlobalStorage;
	uteInterface = (UtInterface *)(j9ras ? j9ras->utIntf : NULL);

	/* Is trace running? */
	rc = JVMTI_ERROR_INVALID_ENVIRONMENT;		
	if ( uteInterface && uteInterface->server ) {
		omr_error_t result = OMR_ERROR_NONE;
		
		PORT_ACCESS_FROM_JAVAVM(vm);
		
		UserDataWrapper *wrapper = j9mem_allocate_memory(sizeof(UserDataWrapper), J9MEM_CATEGORY_JVMTI);
		if (wrapper == NULL) {
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
			goto done;
		}
		
		wrapper->subscriber = subscriber;
		wrapper->alarm = alarm;
		wrapper->userData = userData;
		wrapper->portlib = PORTLIB;
		wrapper->env = env;

		/* Pass option to the trace engine */
		result = uteInterface->server->RegisterRecordSubscriber( UT_THREAD_FROM_VM_THREAD(currentThread), description, subscriberWrapper, alarmWrapper, wrapper, (struct UtTraceBuffer *)-1, NULL, (UtSubscription**)&rv_subscriptionID, TRUE);

		/* Map back to JVMTI error code */
		switch (result) {
			case OMR_ERROR_NONE:
				rc = JVMTI_ERROR_NONE;
				break;
			case OMR_ERROR_OUT_OF_NATIVE_MEMORY:
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
				break;
			case OMR_ERROR_ILLEGAL_ARGUMENT:
				rc = JVMTI_ERROR_ILLEGAL_ARGUMENT;
				break;
			case OMR_ERROR_INTERNAL:
			default:
				rc = JVMTI_ERROR_INTERNAL;
				break;
		}
	}

done:
	if (NULL != subscriptionID) {
		*subscriptionID = rv_subscriptionID;
	}
	TRACE_JVMTI_RETURN(jvmtiRegisterTraceSubscriber);
}


/*
 * Asynchronous
 * Removes the specified subscriber callback, preventing it from being passed any more data.
 * subscriberID - the subscription to deregister
 * alarm - a callback that's executed when the deregistration is complete, may be NULL.
 * Return values with meaning specific to this function:
 *	JVMTI_ERROR_NONE - deregistration complete
 *	JVMTI_ERROR_NOT_AVAILABLE - callback in use, will be deregistered on return and alarm will be called if not NULL.
 *	JVMTI_ERROR_ILLEGAL_ARGUMENT - unknown subscriber
 *	JVMTI_ERROR_INVALID_ENVIRONMENT - trace is not present or enabled
 *
 * J9JVMTIExtensionFunctionInfo.id = "com.ibm.DeregisterTraceSubscriber"
 */
static jvmtiError JNICALL
jvmtiDeregisterTraceSubscriber(jvmtiEnv* env, void *subscriberID, ...)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_INVALID_ENVIRONMENT;
	J9VMThread * currentThread;

	RasGlobalStorage *j9ras;
	UtInterface *uteInterface;

	Trc_JVMTI_jvmtiDeregisterTraceSubscriber_Entry(env, subscriberID);
	
	ENSURE_NON_NULL(subscriberID);
	
	rc = getCurrentVMThread(vm, &currentThread);
	if (rc != JVMTI_ERROR_NONE) {
		/* Cannot see any trace from this env */
		rc = JVMTI_ERROR_INVALID_ENVIRONMENT;
		goto done;
	}
	
	j9ras = (RasGlobalStorage *)vm->j9rasGlobalStorage;
	uteInterface = (UtInterface *)(j9ras ? j9ras->utIntf : NULL);

	/* Is trace running? */
	rc = JVMTI_ERROR_INVALID_ENVIRONMENT;
	if ( uteInterface && uteInterface->server ) {

		/* Pass option to the trace engine */
		omr_error_t result = uteInterface->server->DeregisterRecordSubscriber( UT_THREAD_FROM_VM_THREAD(currentThread), subscriberID );

		/* Map back to JVMTI error code */
		switch (result) {
			case OMR_ERROR_NONE:
				rc = JVMTI_ERROR_NONE;
				break;
			case OMR_ERROR_OUT_OF_NATIVE_MEMORY:
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
				break;
			case OMR_ERROR_ILLEGAL_ARGUMENT:
				rc = JVMTI_ERROR_ILLEGAL_ARGUMENT;
				break;
			case OMR_ERROR_NOT_AVAILABLE:
				rc = JVMTI_ERROR_NOT_AVAILABLE;
				break;
			case OMR_ERROR_INTERNAL: /* FALLTHRU */
			default:
				rc = JVMTI_ERROR_INTERNAL;
				break;
		}
	}

	done:
	TRACE_JVMTI_RETURN(jvmtiDeregisterTraceSubscriber);
}

/*
 * Adds all trace buffers containing data to the write queue, then prompts processing of the write
 * queue via the standard mechanism.
 * 
 * Return values with meaning specific to this function:
 *	JVMTI_ERROR_NONE - success
 *	JVMTI_ERROR_INVALID_ENVIRONMENT - trace is not present,enabled or no consumers are registered
 *
 * J9JVMTIExtensionFunctionInfo.id = "com.ibm.FlushTraceData"
 */
static jvmtiError JNICALL
jvmtiFlushTraceData(jvmtiEnv* env, ...)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_INVALID_ENVIRONMENT;
	J9VMThread * currentThread;

	RasGlobalStorage *j9ras;
	UtInterface *uteInterface;

	Trc_JVMTI_jvmtiFlushTraceData_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc != JVMTI_ERROR_NONE) {
		/* Cannot see any trace from this env */
		rc = JVMTI_ERROR_INVALID_ENVIRONMENT;
		goto done;
	}

	j9ras = (RasGlobalStorage *)vm->j9rasGlobalStorage;
	uteInterface = (UtInterface *)(j9ras ? j9ras->utIntf : NULL);

	/* Is trace running? */
	if ( uteInterface && uteInterface->server ) {

		/* Pass option to the trace engine */
		omr_error_t result = uteInterface->server->FlushTraceData( UT_THREAD_FROM_VM_THREAD(currentThread), NULL, NULL, FALSE );

		/* Map back to JVMTI error code */
		switch (result) {
			case OMR_ERROR_NONE:
				rc = JVMTI_ERROR_NONE;
				break;
			case OMR_ERROR_OUT_OF_NATIVE_MEMORY:
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
				break;
			case OMR_ERROR_ILLEGAL_ARGUMENT:
				rc = JVMTI_ERROR_INVALID_ENVIRONMENT;
				break;
			case OMR_ERROR_INTERNAL: /* FALLTHRU */
			default:
				rc = JVMTI_ERROR_INTERNAL;
				break;
		}
	}

	done:
	TRACE_JVMTI_RETURN(jvmtiFlushTraceData);
}

/*
 * This function supplies the trace metadata for use with the trace formatter.
 * data - metadata in a form usable by the trace formatter
 * length - the length of the metadata returned
 * 
 * Return values with meaning specific to this function:
 *	JVMTI_ERROR_NONE - success
 *	JVMTI_ERROR_INVALID_ENVIRONMENT - trace is not present,enabled or no consumers are registered
 *
 * J9JVMTIExtensionFunctionInfo.id = "com.ibm.FlushTraceData"
 */
static jvmtiError JNICALL
jvmtiGetTraceMetadata(jvmtiEnv *env, void **data, jint *length, ...)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_INVALID_ENVIRONMENT;
	J9VMThread * currentThread;
	void *rv_data = NULL;
	jint rv_length = 0;
	
	RasGlobalStorage *j9ras;
	UtInterface *uteInterface;
	
	Trc_JVMTI_jvmtiGetTraceMetadata_Entry(env, data, length);
	
	ENSURE_NON_NULL(data);	
	ENSURE_NON_NULL(length);	
	
	rc = getCurrentVMThread(vm, &currentThread);
	if (rc != JVMTI_ERROR_NONE) {
		/* Cannot see any trace from this env */
		rc = JVMTI_ERROR_INVALID_ENVIRONMENT;
		goto done;
	}
	
	j9ras = (RasGlobalStorage *)vm->j9rasGlobalStorage;
	uteInterface = (UtInterface *)(j9ras ? j9ras->utIntf : NULL);
	
	/* Is trace running? */
	if ( uteInterface && uteInterface->server ) {
	
		/* Pass option to the trace engine */
		omr_error_t result = uteInterface->server->GetTraceMetadata(&rv_data, &rv_length);
	
		/* Map back to JVMTI error code */
		switch (result) {
			case OMR_ERROR_NONE:
				rc = JVMTI_ERROR_NONE;
				break;
			case OMR_ERROR_ILLEGAL_ARGUMENT:
				rc = JVMTI_ERROR_INVALID_ENVIRONMENT;
				break;
			case OMR_ERROR_INTERNAL: /* FALLTHRU */
			default:
				rc = JVMTI_ERROR_INTERNAL;
				break;
		}
	}
	
done:
	if (NULL != data) {
		*data = rv_data;
	}
	if (NULL != length) {
		*length = rv_length;
	}
	TRACE_JVMTI_RETURN(jvmtiGetTraceMetadata);
}


/** 
 * \brief	Verify that a given ram method pointer is valid 
 * \ingroup jvmtiExtensions
 * 
 * @param[in] vm		Java VM structure 
 * @param[in] ramMethod RAM Method pointer to verify
 * @return JVMTI_ERROR_NOT_FOUND if the method pointer is not valid, JVMTI_ERROR_NONE if it is valid.
 * 
 * Verify that the passed in ram method is actually valid by first finding the correct class segment
 * followed by walking its classes in search of one that contains it in its ramMethods array or else
 * by walking its classloader's classes in search of one that contains it in its ramMethods array.
 */
static jvmtiError 
jvmtiGetMethodAndClassNames_verifyRamMethod(J9JavaVM * vm, J9Method * ramMethod)
{
	J9Class * clazz;
	J9MemorySegment *segment;
	J9ClassWalkState walkState;


	omrthread_monitor_enter(vm->classTableMutex);

	segment = (J9MemorySegment *) avl_search(&vm->classMemorySegments->avlTreeData, (UDATA) ramMethod);
	if ((segment == NULL) || (segment->type == MEMORY_TYPE_UNDEAD_CLASS) || (segment->classLoader == NULL)) {
		/* Bail out if we happen to hit a segment with undead classes, those segments do not have a valid classloader
		 * and would contain invalid methods in the first place. */
		omrthread_monitor_exit(vm->classTableMutex);
		return JVMTI_ERROR_NOT_FOUND;
	}

	/* NOTE: don't call allClassesStartDo/allClassesEndDo because we already own the classTableMutex */

	/* Setup a class walk starting with the class segment we've found this ramMethod in */

	walkState.vm = vm;
	walkState.nextSegment = segment;
	walkState.heapPtr = NULL;
	walkState.classLoader = NULL;

	while (((clazz = vm->internalVMFunctions->allClassesNextDo(&walkState)) != NULL) && (walkState.nextSegment == segment)) {
		UDATA methodCount = clazz->romClass->romMethodCount;

		/* Check if the ramMethod falls within the ramMethods array for clazz */

		if ((clazz->ramMethods <= ramMethod) && (ramMethod < (clazz->ramMethods + methodCount)) &&
			(((UDATA)ramMethod - (UDATA)clazz->ramMethods) % sizeof(J9Method) == 0)) {
			omrthread_monitor_exit(vm->classTableMutex);
			return JVMTI_ERROR_NONE;
		}
	}

	/* Walk all classes for the classloader owning the segment */

	walkState.nextSegment = segment->classLoader->classSegments;
	walkState.heapPtr = NULL;
	walkState.classLoader = segment->classLoader;

	while ((clazz = vm->internalVMFunctions->allClassesNextDo(&walkState)) != NULL) {
		UDATA methodCount = clazz->romClass->romMethodCount;

		/* Check if the ramMethod falls within the ramMethods array for clazz */

		if ((clazz->ramMethods <= ramMethod) && (ramMethod < (clazz->ramMethods + methodCount)) &&
			(((UDATA)ramMethod - (UDATA)clazz->ramMethods) % sizeof(J9Method) == 0)) {
			omrthread_monitor_exit(vm->classTableMutex);
			return JVMTI_ERROR_NONE;
		}
	}

	omrthread_monitor_exit(vm->classTableMutex);

	return JVMTI_ERROR_NOT_FOUND;
}



/** 
 * \brief 
 * \ingroup 
 * 
 * @param[in] jvmti_env 
 * @param[in] ramMethods                    an array of ram methods to be retrieved names for
 * @param[in] ramMethodCount                number of ram method pointers in ramMethods
 * @param[in] ramMethodDataDescriptors      return array of class and method names
 * @param[in] ramMethodStrings              buffer of class and method name strings
 * @param[in/out] ramMethodStringsSize      size of ramMethodStrings in bytes, if not enough we'll change it to indicate the correct size
 * @return jvmtiError code
 *
 *
 * 	The user is responsible for passing in a ramMethods array containing ram method pointers they wish to query. 
 *	The ramMethodCount argument contains the count of passed in ram method pointers.
 *
 *	The ramMethodDataDescriptors is a buffer large enough to accommodate ramMethodCount number of jvmtiExtensionRamMethodData
 *	structures. It is written to by this extension as ram method pointers are processed. The first jvmtiExtensionRamMethodData structure
 *	written to the ramMethodDataDescriptors corresponds to the first ram method pointer in the ramMethods array and so on. Ram method
 *	pointers that are detected to be no longer valid (e.g. class has been unloaded) will result in className and methodName pointers being 
 *	set to NULL in the corresponding jvmtiExtensionRamMethodData structure.
 *	
 *
 *	The ramMethodStrings serves as a buffer to copy class, method and package name strings for valid ram method pointers. The 
 *	jvmtiExtensionRamMethodData structures are initialized with pointers into this array. Each string is null terminated. Care should be
 *	taken to pass in a large enough buffer to accommodate strings for all requested ram methods. In case the buffer is not sufficiently 
 *	large, the extension will stop processing any further ram methods pointers and return whatever has been processed up to that point.
 *
 *  ramMethodStringsSize is used by the user to specify the size of ramMethodStrings. If not enough space was provided, this 
 *  API call will change the value to the required amount of space. 
 *
 *	Note that ramMethodDataDescriptors and ramMethodStrings are decoupled into two separate buffers as the user might wish to 
 *	keep the strings as is to prevent a copy and discard the ramMethodDataDescriptors once the pointers have been reassigned to
 *	some other user defined mechanism. 
 *
 * */
static jvmtiError JNICALL
jvmtiGetMethodAndClassNames(jvmtiEnv *jvmti_env,  void * ramMethods, jint ramMethodCount, 
					  		jvmtiExtensionRamMethodData * ramMethodDataDescriptors, 
					   		jchar * ramMethodStrings, jint * ramMethodStringsSize, ...)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(jvmti_env);
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9VMThread * currentThread;
	J9Method ** ramMethodsArray;
	UDATA stringsSize;
	char * stringsPtr;
	UDATA index;

	Trc_JVMTI_jvmtiGetMethodAndClassNames_Entry(jvmti_env, ramMethods, ramMethodCount);

	ENSURE_POSITIVE(ramMethodCount);
	ENSURE_NON_NULL(ramMethods);
	ENSURE_NON_NULL(ramMethodDataDescriptors);
	ENSURE_NON_NULL(ramMethodStrings);

	stringsSize = (UDATA) *ramMethodStringsSize;
	stringsPtr  = (char *) ramMethodStrings;
	ramMethodsArray = (J9Method **) ramMethods;

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
        UDATA totalStringsLength = 0;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		for (index = 0; index < (UDATA)ramMethodCount; index++) {
			J9Method * ramMethod;

			ramMethod = ramMethodsArray[index];

			/* check if the ramMethod pointer is valid without dereferencing it */ 

			if (jvmtiGetMethodAndClassNames_verifyRamMethod(vm, ramMethod) == JVMTI_ERROR_NONE) {
				UDATA length;
				J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);
				J9UTF8 * methodName = J9ROMMETHOD_NAME(romMethod);
				J9UTF8 * methodSig  = J9ROMMETHOD_SIGNATURE(romMethod);
				J9Class* clazz = J9_CLASS_FROM_METHOD(ramMethod);
				J9UTF8 * className = J9ROMCLASS_CLASSNAME(clazz->romClass);
                                                                                     
				/* Amount of space we need to copy the null terminated class name string and
				   null terminated methodName/Signature string */

				length = J9UTF8_LENGTH(className) + 1 + J9UTF8_LENGTH(methodName) + J9UTF8_LENGTH(methodSig) + 1;
                
                /* Keep a running total of required string buffer space */
				   
				totalStringsLength += length;

				/* got enough space to accommodate this ramMethod's strings? */

				if (stringsSize >= length) {

					/* copy the class name (package name is implied) */

					memcpy(stringsPtr, J9UTF8_DATA(className), J9UTF8_LENGTH(className));
					ramMethodDataDescriptors[index].className = (jchar *) stringsPtr;
					stringsPtr[J9UTF8_LENGTH(className)] = 0;
					stringsPtr += J9UTF8_LENGTH(className) + 1;

					/* copy the method name and signature */ 

					memcpy(stringsPtr, J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodName));
					memcpy(stringsPtr + J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodSig), J9UTF8_LENGTH(methodSig));
					ramMethodDataDescriptors[index].methodName = (jchar *) stringsPtr;
					stringsPtr[J9UTF8_LENGTH(methodName) + J9UTF8_LENGTH(methodSig)] = 0;
					stringsPtr += J9UTF8_LENGTH(methodName) + J9UTF8_LENGTH(methodSig) + 1;

					ramMethodDataDescriptors[index].reasonCode = JVMTI_ERROR_NONE;
					
					stringsSize -= length;

					Trc_JVMTI_jvmtiGetMethodAndClassNames_methodFound(ramMethodDataDescriptors[index].className, 
																	  ramMethodDataDescriptors[index].methodName, ramMethod);

				} else {

					/* Not enough string buffer space, we'll keep trying to add more methods tho
					 * perhaps some of them will have shorter class/method names and might fit */

					ramMethodDataDescriptors[index].className = NULL;
					ramMethodDataDescriptors[index].methodName = NULL;
					ramMethodDataDescriptors[index].reasonCode = JVMTI_ERROR_OUT_OF_MEMORY;

					Trc_JVMTI_jvmtiGetMethodAndClassNames_outOfStringMemory(ramMethod, length, stringsSize);
				}

			} else {
				ramMethodDataDescriptors[index].className = NULL;
				ramMethodDataDescriptors[index].methodName = NULL;
				ramMethodDataDescriptors[index].reasonCode = JVMTI_ERROR_INVALID_METHODID;

				Trc_JVMTI_jvmtiGetMethodAndClassNames_methodNotFound(ramMethod);
			}
		}

        /* Save the total number of bytes required for ramMethodStrings to pass back all strings */

		*ramMethodStringsSize = (jint)totalStringsLength;

		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

done:

	TRACE_JVMTI_RETURN(jvmtiGetMethodAndClassNames);
}


/*
 * Writes a string containing the value of the internal log options expressed as text.
 * For example, if the error and warning bits of the log options were set, then a string
 * containing "error+warn" would be returned.
 * The length of the string (including null terminator) is written into the data_size.
 * If there would be a buffer overflow, the string is not written, however the 
 * size of the required memory is still set.
 *
 *   buffer_size - the size of the supplied memory
 *   options - pointer to the memory to receive the specification string.
 *   data_size - pointer to an integer to receive the length of the specification string.
 *
 * Return values with meaning specific to this function:
 *	JVMTI_ERROR_NONE - success
 *	JVMTI_ERROR_NULL_POINTER - a null pointer has been passed as a parameter.
 *	JVMTI_ERROR_WRONG_PHASE - function cannot be run in JVMTI_DEAD_PHASE
 *	JVMTI_ERROR_ILLEGAL_ARGUMENT - an invalid parameter has been passed
 *	JVMTI_ERROR_INTERNAL
 */
static jvmtiError JNICALL
jvmtiQueryVmLogOptions(jvmtiEnv *jvmti_env, jint buffer_size, void *options, jint *data_size_ptr, ...) {
	J9JavaVM * vm = JAVAVM_FROM_ENV(jvmti_env);
	jvmtiError rc = JVMTI_ERROR_NOT_AVAILABLE;
	jint rv_data_size = 0;

	Trc_JVMTI_jvmtiQueryVmLogOptions_Entry(jvmti_env);

	ENSURE_PHASE_NOT_DEAD(jvmti_env);
	ENSURE_NON_NULL(data_size_ptr);
	
	/* request log options */
	rc = vm->internalVMFunctions->queryLogOptions(vm, buffer_size, options, (I_32 *)&rv_data_size);

done:
	if (NULL != data_size_ptr) {
		*data_size_ptr = rv_data_size;
	}
	TRACE_JVMTI_RETURN(jvmtiQueryVmLogOptions);
}

/*
 * Sets the internal log options. A null terminated string containing the requested log options
 * can be passed to the function. The string will be parsed and the current log options set
 * accordingly.
 *
 *   options_buffer - pointer to the memory that contains the logging options string.
 *
 * Return values with meaning specific to this function:
 *	JVMTI_ERROR_NONE - success
 *	JVMTI_ERROR_NULL_POINTER - a null pointer has been passed as a parameter.
 *	JVMTI_ERROR_WRONG_PHASE - function cannot be run in JVMTI_DEAD_PHASE
 *	JVMTI_ERROR_ILLEGAL_ARGUMENT - an invalid parameter has been passed
 *	JVMTI_ERROR_OUT_OF_MEMORY - system could not allocate enough memory
 *	JVMTI_ERROR_INTERNAL
 */
static jvmtiError
JNICALL jvmtiSetVmLogOptions(jvmtiEnv *jvmti_env, char *options_buffer, ...) {
	J9JavaVM * vm = JAVAVM_FROM_ENV(jvmti_env);
	jvmtiError rc = JVMTI_ERROR_NOT_AVAILABLE;

	Trc_JVMTI_jvmtiSetVmLogOptions_Entry(jvmti_env);

	ENSURE_PHASE_NOT_DEAD(jvmti_env);

	rc = vm->internalVMFunctions->setLogOptions(vm, options_buffer);

done:
	TRACE_JVMTI_RETURN(jvmtiSetVmLogOptions);
}


/*
 * New JLM Dump interface, with the dump_format option
 */
static jvmtiError JNICALL
jvmtiJlmDumpStats(jvmtiEnv* env, void ** dump_info, jint dump_format, ...)
{
	jvmtiError rc = JVMTI_ERROR_NOT_AVAILABLE;
	void *rv_dump_info = NULL;

	Trc_JVMTI_jvmtiJlmDumpStats_Entry(env);
	ENSURE_PHASE_LIVE(env);
	ENSURE_NON_NULL(dump_info);

    if ( (dump_format < COM_IBM_JLM_DUMP_FORMAT_OBJECT_ID) || (dump_format > COM_IBM_JLM_DUMP_FORMAT_TAGS)) {
        rc = JVMTI_ERROR_ILLEGAL_ARGUMENT;
        goto done;
    }

#if defined(OMR_THR_JLM)
	rc = jvmtiJlmDumpHelper(env, &rv_dump_info, dump_format);		
#endif /* OMR_THR_JLM */

done:
	if (NULL != dump_info) {
		*dump_info = rv_dump_info;
	}
	TRACE_JVMTI_RETURN(jvmtiJlmDumpStats);
}


/*
 * A helper function used for both jvmtiJlmDump() and jvmtiJlmDumpStats()
 */
static jvmtiError 
jvmtiJlmDumpHelper(jvmtiEnv* env, void ** dump_info, jint dump_format)
{
	jvmtiError rc = JVMTI_ERROR_NOT_AVAILABLE;

#if defined(OMR_THR_JLM)
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	PORT_ACCESS_FROM_JAVAVM(vm);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9VMJlmDump * dump;
		UDATA dump_size;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);
		vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);

		dump = j9mem_allocate_memory(sizeof(jlm_dump), J9MEM_CATEGORY_JVMTI_ALLOCATE);
		if (dump == NULL) {
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
		} else {
			omrthread_t self = omrthread_self();
			omrthread_lib_lock(self);
			rc = request_MonitorJlmDumpSize(vm, &dump_size, dump_format);
			if (rc == JLM_SUCCESS) {
				dump->begin = j9mem_allocate_memory(dump_size, J9MEM_CATEGORY_JVMTI_ALLOCATE);
				if (dump->begin == NULL) {
					rc = JVMTI_ERROR_OUT_OF_MEMORY;
				} else {
					rc = request_MonitorJlmDump(env, dump, dump_format);
					if (rc == JLM_SUCCESS) {
						dump->end = dump->begin + dump_size;
						*dump_info = dump;
					}
				}
			} else {
				rc = JVMTI_ERROR_NOT_AVAILABLE;
				j9mem_free_memory(dump);
			}
			omrthread_lib_unlock(self);
		}
		vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}
#endif
	return rc;
}

/* Categories defined in OMR have category codes from 0x80000000 up */
static U_32 indexFromCategoryCode( UDATA categories_mapping_size, U_32 cc ) {
	/* Start the port library codes from the last element (total_categories - 1) */
	return ((cc) > OMRMEM_LANGUAGE_CATEGORY_LIMIT) ? ((((U_32)categories_mapping_size) - 1) - (OMRMEM_OMR_CATEGORY_INDEX_FROM_CODE(cc))): (cc);
}

struct jvmtiGetMemoryCategoriesState
{
	jvmtiMemoryCategory * categories_buffer;
	jint max_categories;
	jint written_count;
	/* Records mapping of category indexes to slots in categories_buffer */
	jvmtiMemoryCategory ** categories_mapping;
	BOOLEAN buffer_overflow;
	UDATA total_categories;
	UDATA categories_mapping_size;
};

/**
 * Callback used by jvmtiGetMemoryCategories with j9mem_walk_categories
 */
static UDATA jvmtiGetMemoryCategoriesCallback (U_32 categoryCode, const char * categoryName, UDATA liveBytes, UDATA liveAllocations, BOOLEAN isRoot, U_32 parentCategoryCode, OMRMemCategoryWalkState * state)
{
	struct jvmtiGetMemoryCategoriesState * userData = (struct jvmtiGetMemoryCategoriesState *) state->userData1;
	U_32 index = indexFromCategoryCode(userData->categories_mapping_size, categoryCode);

	if (userData->written_count < userData->max_categories) {
		jvmtiMemoryCategory * jvmtiCategory = userData->categories_buffer + userData->written_count;

		/* Record the mapping of index to category */
		userData->categories_mapping[index] = jvmtiCategory;

		jvmtiCategory->name = categoryName;
		jvmtiCategory->liveBytesShallow = liveBytes;
		jvmtiCategory->liveAllocationsShallow = liveAllocations;

		if (isRoot) {
			jvmtiCategory->parent = NULL;
		} else {
			U_32 parentIndex = indexFromCategoryCode(userData->categories_mapping_size, parentCategoryCode);

			/* Memory category walk is depth first - we will definitely have walked through and recorded our parent */
			jvmtiCategory->parent = userData->categories_mapping[parentIndex];
		}

		userData->written_count++;

		return J9MEM_CATEGORIES_KEEP_ITERATING;
	} else {
		/* We've filled up the user's buffer. Stop iterating */
		userData->buffer_overflow = TRUE;
		return J9MEM_CATEGORIES_STOP_ITERATING;
	}
}

/**
 * Callback used by jvmtiGetMemoryCategories with j9mem_walk_categories
 */
static UDATA jvmtiCountMemoryCategoriesCallback (U_32 categoryCode, const char * categoryName, UDATA liveBytes, UDATA liveAllocations, BOOLEAN isRoot, U_32 parentCategoryCode, OMRMemCategoryWalkState * state)
{
	struct jvmtiGetMemoryCategoriesState * userData = (struct jvmtiGetMemoryCategoriesState *) state->userData1;
	userData->total_categories++;
	return J9MEM_CATEGORIES_KEEP_ITERATING;
}

/**
 * Callback used by jvmtiGetMemoryCategories with j9mem_walk_categories
 */
static UDATA
jvmtiCalculateSlotsForCategoriesMappingCallback(U_32 categoryCode, const char *categoryName, UDATA liveBytes,
		UDATA liveAllocations, BOOLEAN isRoot, U_32 parentCategoryCode, OMRMemCategoryWalkState *state)
{

	if (categoryCode < OMRMEM_LANGUAGE_CATEGORY_LIMIT) {
		UDATA maxLanguageCategoryIndex = (UDATA)state->userData1;
		UDATA categoryIndex = (UDATA)categoryCode;

		state->userData1 = (void *)((maxLanguageCategoryIndex > categoryIndex) ? maxLanguageCategoryIndex : categoryIndex);

	} else if (categoryCode > OMRMEM_LANGUAGE_CATEGORY_LIMIT) {
		UDATA maxOMRCategoryIndex = (UDATA)state->userData2;
		UDATA categoryIndex = (UDATA)OMRMEM_OMR_CATEGORY_INDEX_FROM_CODE(categoryCode);

		state->userData2 = (void *)((maxOMRCategoryIndex > categoryIndex) ? maxOMRCategoryIndex : categoryIndex);
	}
	return J9MEM_CATEGORIES_KEEP_ITERATING;
}


static void fillInChildAndSiblingCategories(jvmtiMemoryCategory * categories_buffer, jint written_count)
{
	jint i;

	/* Note: for efficiency, we will temporarily retask the liveBytesDeep field as the tail of the child list */
	for (i = 0; i < written_count; i++) {
		jvmtiMemoryCategory * thisCategory = categories_buffer + i;
		jvmtiMemoryCategory * parent = thisCategory->parent;
		if (parent) {
			jvmtiMemoryCategory * lastChild = (jvmtiMemoryCategory *)(UDATA) parent->liveBytesDeep;

			if (lastChild) {
				lastChild->nextSibling = thisCategory;
			} else {
				/* We are adding the first child */
				parent->firstChild = thisCategory;
			}
			parent->liveBytesDeep = (jlong)(UDATA) thisCategory;
		}
	}
}

static void fillInCategoryDeepCounters(jvmtiMemoryCategory * category)
{
	jvmtiMemoryCategory * childCursor;

	category->liveBytesDeep = category->liveBytesShallow;
	category->liveAllocationsDeep = category->liveAllocationsShallow;

	childCursor = category->firstChild;

	while (childCursor) {
		/* Children will add to our deep counters */
		fillInCategoryDeepCounters(childCursor);

		childCursor = childCursor->nextSibling;
	}

	/* Pass back to our parent's deep counters */
	if (category->parent) {
		category->parent->liveBytesDeep += category->liveBytesDeep;
		category->parent->liveAllocationsDeep += category->liveAllocationsDeep;
	}
}

/**
  * Samples the values of the JVM memory categories and writes them into a buffer allocated by the user.
  *
  * @param[in] env JVMTI environment
  * @param[in] version API version number
  * @param[in] max_categories Maximum number of categories to read into category_buffer
  * @param[out] category_buffer Block of memory to write result into. 0th entry is the first root category. All other nodes can be walked from the root.
  * @param[out] written_count_ptr If not NULL, the number of categories written to the buffer is written to this address
  * @param[out] total_categories_ptr If not NULL, jthe total number of categories available is written to this address
  *
  * @return JVMTI error code:
  * JVMTI_ERROR_NONE - success
  * JVMTI_ERROR_UNSUPPORTED_VERSION - Unsupported version of jvmtiMemoryCategory
  * JVMTI_ERROR_ILLEGAL_ARGUMENT - Illegal argument (categories_buffer, count_ptr & total_categories_ptr are all NULL)
  * JVMTI_ERROR_INVALID_ENVIRONMENT - Invalid JVMTI environment
  * JVMTI_ERROR_OUT_OF_MEMORY - Memory category data was truncated because category_buffer/max_categories was not large enough
  *
  */
static jvmtiError JNICALL
jvmtiGetMemoryCategories(jvmtiEnv* env, jint version, jint max_categories, jvmtiMemoryCategory * categories_buffer, jint * written_count_ptr, jint * total_categories_ptr, ...)
{
	jvmtiError rc = JVMTI_ERROR_NOT_AVAILABLE;
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	PORT_ACCESS_FROM_JAVAVM(vm);
	OMRMemCategoryWalkState walkState;
	struct jvmtiGetMemoryCategoriesState userData;
	jint rv_written_count = 0;
	jint rv_total_categories = 0;

	Trc_JVMTI_jvmtiGetMemoryCategories_Entry(env, version, max_categories, categories_buffer, written_count_ptr, total_categories_ptr);

	memset(&userData, 0, sizeof(struct jvmtiGetMemoryCategoriesState));

	userData.categories_buffer = categories_buffer;
	userData.max_categories = max_categories;
	userData.total_categories = 0;

	if (max_categories < 0) {
		max_categories = 0;
	}

	/* If we're asked to write to categories_buffer (max_categories > 0), make sure categories_buffer isn't NULL */
	if (max_categories > 0 && NULL == categories_buffer) {
		Trc_JVMTI_jvmtiGetMemoryCategories_NullOutput_Exit(max_categories);
		return JVMTI_ERROR_ILLEGAL_ARGUMENT;
	}

	/* If the user has set max_categories and categories_buffer, but hasn't set written_count_ptr, fail. */
	if (max_categories > 0 && categories_buffer && NULL == written_count_ptr) {
		Trc_JVMTI_jvmtiGetMemoryCategories_NullWrittenPtr_Exit(max_categories, categories_buffer);
		return JVMTI_ERROR_ILLEGAL_ARGUMENT;
	}

	/* If all the out parameters are NULL, we can't do any work. */
	if (NULL == categories_buffer && NULL == written_count_ptr && NULL == total_categories_ptr) {
		Trc_JVMTI_jvmtiGetMemoryCategories_AllOutputsNull_Exit();
		return JVMTI_ERROR_ILLEGAL_ARGUMENT;
	}

	/* Check the user has the same structure shape as we do */
	if (COM_IBM_GET_MEMORY_CATEGORIES_VERSION_1 != version) {
		Trc_JVMTI_jvmtiGetMemoryCategories_WrongVersion_Exit(version);
		return JVMTI_ERROR_UNSUPPORTED_VERSION;
	}

	/* We need to allocate space to hold the categories data.
	 * Do a walk to count the categories and populate userData.total_categories.
	 */
	walkState.walkFunction = jvmtiCountMemoryCategoriesCallback;
	walkState.userData1 = &userData;
	j9mem_walk_categories(&walkState);

	if (categories_buffer) {
		jint i;

		/* Calculate the storage we need for the mapping array.
		 * It needs to handle the largest possible indexes for OMR or language categories,
		 * not the total number of categories, which may be smaller.
		 */
		walkState.walkFunction = jvmtiCalculateSlotsForCategoriesMappingCallback;
		walkState.userData1 = 0;
		walkState.userData2 = 0;
		j9mem_walk_categories(&walkState);
		/* Both + 1 because we have the max indexes which start from 0 */
		userData.categories_mapping_size = ((UDATA)walkState.userData1) + 1 + ((UDATA)walkState.userData2) + 1;

		walkState.walkFunction = jvmtiGetMemoryCategoriesCallback;
		walkState.userData1 = &userData;
		walkState.userData2 = 0;

		userData.categories_mapping = (jvmtiMemoryCategory**)j9mem_allocate_memory(userData.categories_mapping_size * sizeof(jvmtiMemoryCategory*), J9MEM_CATEGORY_JVMTI);
		if (NULL == userData.categories_mapping) {
			/* JVMTI_ERROR_OUT_OF_MEMORY is returned if the buffer the user gave
			 * use wasn't big enough. That's not fatal. This is.
			 */
			Trc_JVMTI_jvmtiGetMemoryCategories_GetMemoryCategories_J9MemAllocFail_Exit(userData.categories_mapping_size * sizeof(jvmtiMemoryCategory*));
			return JVMTI_ERROR_INTERNAL;
		}

		memset(userData.categories_mapping, 0, userData.categories_mapping_size * sizeof(jvmtiMemoryCategory*));
		memset(categories_buffer, 0, max_categories * sizeof(jvmtiMemoryCategory));

		/* Walk categories to get shallow values */
		j9mem_walk_categories(&walkState);

		/* Fix-up child and sibling references */
		fillInChildAndSiblingCategories(categories_buffer, userData.written_count);

		/* Fill-in deep counters */
		for (i = 0; i < userData.written_count; i++) {
			jvmtiMemoryCategory * category = categories_buffer + i;

			if (NULL == category->parent) {
				fillInCategoryDeepCounters(category);
			}
		}

		if (userData.buffer_overflow) {
			Trc_JVMTI_jvmtiGetMemoryCategories_BufferOverflow();
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
		} else {
			rc = JVMTI_ERROR_NONE;
		}

		j9mem_free_memory(userData.categories_mapping);
	}

	if (written_count_ptr) {
		rv_written_count = userData.written_count;
		if (JVMTI_ERROR_NOT_AVAILABLE == rc) {
			rc = JVMTI_ERROR_NONE;
		}
	}

	if (total_categories_ptr) {
		rv_total_categories = (jint)userData.total_categories;
		if (JVMTI_ERROR_NOT_AVAILABLE == rc) {
			rc = JVMTI_ERROR_NONE;
		}
	}

	Trc_JVMTI_jvmtiGetMemoryCategories_Exit(rc);

	if (NULL != written_count_ptr) {
		*written_count_ptr = rv_written_count;
	}
	if (NULL != total_categories_ptr) {
		*total_categories_ptr = rv_total_categories;
	}
	return rc;
}

/*
 * Helper function to unhook a subscriber.
 * Unfortunately J9HookUnregister doesn't return a value, so we can't 
 * tell when this fails (e.g. unregistering a non-registered hook).
 */
static void
unhookVerboseGCOutput(J9JavaVM* vm, VerboseGCSubscriberData* subscriberData)
{
	J9HookInterface **gcHooks = vm->memoryManagerFunctions->j9gc_get_omr_hook_interface(vm->omrVM);
	(*gcHooks)->J9HookUnregister(gcHooks, J9HOOK_MM_OMR_VERBOSE_GC_OUTPUT, hookVerboseGCOutput, subscriberData);
}

/*
 * Hook wrapper function used to call the user's callback and handle errors.
 */
static void
hookVerboseGCOutput(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	MM_VerboseGCOutputEvent* event = (MM_VerboseGCOutputEvent*)eventData;
	VerboseGCSubscriberData* subscriberData = (VerboseGCSubscriberData*)userData;
	J9JavaVM *javaVM = NULL;
	
	if(NULL != subscriberData) {
		jvmtiError rc = subscriberData->subscriber(
				subscriberData->env, 
				event->string, 
				strlen(event->string), 
				subscriberData->userData);
		
		if(JVMTI_ERROR_NONE != rc) {
			/* The subscriber returned an error. Call the alarm function. */
			subscriberData->alarm(
					subscriberData->env, 
					(void*)subscriberData, 
					subscriberData->userData);
			
			/* And now deregister the subscriber */
			javaVM = ((J9VMThread*)event->currentThread->_language_vmthread)->javaVM;
			unhookVerboseGCOutput(javaVM, subscriberData);
		}
	}
}

/**
 * Register a new verbose GC subscriber.
 *
 * As verbose GC data is produced, the subscriber function is called along with the userData and the ASCII-encoded verbose GC XML data.
 * The subscriber must process the data during the function call. Once the subscriber returns the data passed in may no longer be valid.
 * Note that the subscriber is not guaranteed to receive a complete XML file; i.e. the stream may not begin with the <?xml> and <verbosegc>
 * tags, nor end with </verbosegc>.
 *
 * J9JVMTIExtensionFunctionInfo.id = "com.ibm.RegisterVerboseGCSubscriber"
 *
 * @param env[in] the jvmti env
 * @param description[in] a string describing the subscriber
 * @param subscriber[in] a function to be called when verbose GC data is available
 * @param alarm[in] a function which is called if the subscriber returns an error or misbehaves
 * @param userData[in] a pointer which is passes to the subscriber or alarm functions
 * @param subscriptionID[out] an opaque pointer describing the subscriber. Use to deregister.
 * @return JVMTI_ERROR_NONE on success, or a JVMTI error on failure
 */
jvmtiError JNICALL 
jvmtiRegisterVerboseGCSubscriber(jvmtiEnv *env, char *description, jvmtiVerboseGCSubscriber subscriber, jvmtiVerboseGCAlarm alarm, void *userData, void **subscriptionID, ...)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9JVMTIEnv * j9env = (J9JVMTIEnv *) env;
	jvmtiError rc = JVMTI_ERROR_INVALID_ENVIRONMENT;
	J9VMThread * currentThread;
	J9HookInterface **gcHooks;
	J9MemoryManagerVerboseInterface *verboseFunctions;
	VerboseGCSubscriberData* subscriberData;
	IDATA hookRC;
	void *rv_subscriptionID = NULL;

	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_JVMTI_jvmtiRegisterVerboseGCSubscriber_Entry(env, description, subscriber, alarm, userData, subscriptionID);

	ENSURE_PHASE_START_OR_LIVE(env);
	ENSURE_NON_NULL(subscriber);
	ENSURE_NON_NULL(subscriptionID);
	ENSURE_NON_NULL(description);
	
	rc = getCurrentVMThread(vm, &currentThread);
	if (rc != JVMTI_ERROR_NONE) {
		/* Not a valid env */
		rc = JVMTI_ERROR_INVALID_ENVIRONMENT;
		goto done;
	}
	
	/* (Re)initialize the verbose GC component, enabling the hook writer. */ 
	verboseFunctions = (J9MemoryManagerVerboseInterface *)vm->memoryManagerFunctions->getVerboseGCFunctionTable(vm);
	if(0 == verboseFunctions->configureVerbosegc(vm, 1, "hook", 0, 0)) {
		rc = JVMTI_ERROR_NOT_AVAILABLE;
		goto done;
	}
	
	subscriberData = j9mem_allocate_memory(sizeof(VerboseGCSubscriberData), J9MEM_CATEGORY_JVMTI);
	if(NULL == subscriberData) {
		rc = JVMTI_ERROR_OUT_OF_MEMORY;
		goto done;
	}
	subscriberData->subscriber = subscriber;
	subscriberData->alarm = alarm;
	subscriberData->env = env;
	subscriberData->userData = userData;
	
	/* Hook the event. Pass in the userData struct */	
	gcHooks = vm->memoryManagerFunctions->j9gc_get_omr_hook_interface(vm->omrVM);
	hookRC = (*gcHooks)->J9HookRegisterWithCallSite(gcHooks, J9HOOK_TAG_AGENT_ID | J9HOOK_MM_OMR_VERBOSE_GC_OUTPUT, hookVerboseGCOutput, OMR_GET_CALLSITE(), subscriberData, j9env->gcHook.agentID);
	if(0 != hookRC) {
		/* Failed to hook the event. Try to return an appropriate error. */
		switch(hookRC) {
		case J9HOOK_ERR_DISABLED:
			rc = JVMTI_ERROR_NOT_AVAILABLE;
			break;
			
		case J9HOOK_ERR_NOMEM:
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
			break;
			
		default:
			rc = JVMTI_ERROR_INTERNAL;
			break;
		}
		goto done;		
	}
	rc = JVMTI_ERROR_NONE;

	rv_subscriptionID = subscriberData;
	
done:
	if (NULL != subscriptionID) {
		*subscriptionID = rv_subscriptionID;
	}
	TRACE_JVMTI_RETURN(jvmtiRegisterVerboseGCSubscriber);
}

/**
 * Deregister an existing verbose GC subscriber.
 *
 * J9JVMTIExtensionFunctionInfo.id = "com.ibm.RegisterVerboseGCSubscriber"
 *
 * @param env[in] the jvmti env
 * @param subscriptionID[in] an opaque pointer describing the subscriber
 * @return JVMTI_ERROR_NONE on success, or a JVMTI error on failure
 */
jvmtiError JNICALL 
jvmtiDeregisterVerboseGCSubscriber(jvmtiEnv *env, void *subscriberID, ...)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_INVALID_ENVIRONMENT;
	J9VMThread * currentThread;
	VerboseGCSubscriberData* subscriberData;

	PORT_ACCESS_FROM_JAVAVM(vm);
	
	Trc_JVMTI_jvmtiDeregisterVerboseGCSubscriber_Entry(env, subscriberID);
	
	ENSURE_NON_NULL(subscriberID);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc != JVMTI_ERROR_NONE) {
		rc = JVMTI_ERROR_INVALID_ENVIRONMENT;
		goto done;
	}
	
	subscriberData = (VerboseGCSubscriberData*)subscriberID; 
	unhookVerboseGCOutput(vm, subscriberData);
	j9mem_free_memory(subscriberData);
	rc = JVMTI_ERROR_NONE;
done:
	TRACE_JVMTI_RETURN(jvmtiDeregisterVerboseGCSubscriber);
}

static jvmtiError JNICALL
jvmtiGetJ9vmThread(jvmtiEnv *env, jthread thread, void **vmThreadPtr, ...)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread *currentThread;
	void *rv_vmThread = NULL;

	Trc_JVMTI_jvmtiGetJ9vmThread_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9VMThread * targetThread;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);
		ENSURE_JTHREAD_NON_NULL(thread);
		ENSURE_NON_NULL(vmThreadPtr);

		rc = getVMThread(currentThread, thread, &targetThread, TRUE, TRUE);
		if (rc == JVMTI_ERROR_NONE) {
			rv_vmThread = targetThread;
			releaseVMThread(currentThread, targetThread);
		}
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != vmThreadPtr) {
		*vmThreadPtr = rv_vmThread;
	}
	TRACE_JVMTI_RETURN(jvmtiGetJ9vmThread);
}

static jvmtiError JNICALL
jvmtiGetJ9method(jvmtiEnv *env, jmethodID mid, void **j9MethodPtr, ...)
{
	jvmtiError rc = JVMTI_ERROR_NONE;
	void *rv_j9Method = NULL;

	Trc_JVMTI_jvmtiGetJ9method_Entry(env);

	ENSURE_PHASE_START_OR_LIVE(env);

	ENSURE_JMETHODID_NON_NULL(mid);
	ENSURE_NON_NULL(j9MethodPtr);

	rv_j9Method = ((J9JNIMethodID *) mid)->method;

done:
	if (NULL != j9MethodPtr) {
		*j9MethodPtr = rv_j9Method;
	}
	TRACE_JVMTI_RETURN(jvmtiGetJ9method);
}

/*
 * Registers a tracepoint subscriber. This is the documented JVMTI API for listening to individual formatted tracepoints. This
 * API is distinct from jvmtiRegisterTraceSubscriber(), which is the undocumented JVMTI API for listening to binary trace buffers
 * used by the Health Centre. It replaces the old JVMRI trace listener.
 * 
 * Parameters:
 *	description - a textual name for the subscriber
 *	subscriber - the subscriber callback function
 *	alarm - alarm callback function, called if the subscriber callback returns an error
 *	userData - user data to pass to the subscriber callback function and the alarm function, may be NULL.
 *	subscriptionID - opaque pointer (internally points to a UtSubscription) identifying the subscriber, needed to unsubscribe.
 *
 * Return values with meaning specific to this function:
 *	JVMTI_ERROR_NONE - subscriber successfully registered
 *	JVMTI_ERROR_INVALID_ENVIRONMENT - trace is not present or enabled
 *
 * J9JVMTIExtensionFunctionInfo.id = "com.ibm.RegisterTracePointSubscriber"
 */
static jvmtiError JNICALL
jvmtiRegisterTracePointSubscriber(jvmtiEnv* env, char *description, jvmtiTraceSubscriber subscriber, jvmtiTraceAlarm alarm, void *userData, void **subscriptionID, ...)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_INVALID_ENVIRONMENT;
	J9VMThread * currentThread = NULL;
	RasGlobalStorage *rasGlobal = NULL;
	UtInterface *traceInterface = NULL;

	Trc_JVMTI_jvmtiRegisterTracePointSubscriber_Entry(env, description, subscriber, alarm, userData, subscriptionID);

	ENSURE_PHASE_START_OR_LIVE(env);
	ENSURE_NON_NULL(subscriber);
	ENSURE_NON_NULL(subscriptionID);
	ENSURE_NON_NULL(description);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc != JVMTI_ERROR_NONE) {
		rc = JVMTI_ERROR_INVALID_ENVIRONMENT;
		goto done;
	}

	rc = JVMTI_ERROR_INVALID_ENVIRONMENT;
	/* Check that the trace engine interface is available */
	rasGlobal = (RasGlobalStorage *)vm->j9rasGlobalStorage;
	if (rasGlobal != NULL ) {
		traceInterface = (UtInterface *)(rasGlobal->utIntf);
	}
	if ((traceInterface != NULL) && (traceInterface->server != NULL)) {
		omr_error_t result = OMR_ERROR_NONE;

		PORT_ACCESS_FROM_JAVAVM(vm);

		/* Create and initialize wrapper structure. This is used in the function wrappers for the user callbacks */
		UserDataWrapper *wrapper = j9mem_allocate_memory(sizeof(UserDataWrapper), J9MEM_CATEGORY_JVMTI);
		if (wrapper == NULL) {
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
			goto done;
		}

		wrapper->subscriber = subscriber;
		wrapper->alarm = alarm;
		wrapper->userData = userData;
		wrapper->portlib = PORTLIB;
		wrapper->env = env;

		/* Call into the trace engine to register the subscriber. Note the use of function wrappers for the user callbacks */
		result = traceInterface->server->RegisterTracePointSubscriber(UT_THREAD_FROM_VM_THREAD(currentThread), description, subscriberWrapper, alarmWrapper, wrapper, (UtSubscription**)subscriptionID);

		/* Map result back to public JVMTI return code */
		switch (result) {
			case OMR_ERROR_NONE:
				rc = JVMTI_ERROR_NONE;
				break;
			case OMR_ERROR_OUT_OF_NATIVE_MEMORY:
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
				break;
			case OMR_ERROR_ILLEGAL_ARGUMENT:
				rc = JVMTI_ERROR_ILLEGAL_ARGUMENT;
				break;
			case OMR_ERROR_INTERNAL: /* fall through */
			default:
				rc = JVMTI_ERROR_INTERNAL;
				break;
		}
	}

	done:
	TRACE_JVMTI_RETURN(jvmtiRegisterTracePointSubscriber);
}

/*
 * De-registers a tracepoint subscriber, previously registered via jvmtiRegisterTracePointSubscriber(). The subscriber
 * to be de-registered is identified by the subscriberID (opaque pointer to a UtSubscription data structure).
 *  
 * Parameters:
 *	subscriberID - pointer to the subscription to deregister
 * Return values with meaning specific to this function:
 *	JVMTI_ERROR_NONE - deregistration complete
 *	JVMTI_ERROR_ILLEGAL_ARGUMENT - unknown subscriber
 *	JVMTI_ERROR_INVALID_ENVIRONMENT - trace is not present or enabled
 *
 * J9JVMTIExtensionFunctionInfo.id = "com.ibm.DeregisterTracePointSubscriber"
 */
static jvmtiError JNICALL
jvmtiDeregisterTracePointSubscriber(jvmtiEnv* env, void *subscriberID, ...)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_INVALID_ENVIRONMENT;
	J9VMThread * currentThread = NULL;
	RasGlobalStorage *rasGlobal = NULL;
	UtInterface *traceInterface = NULL;

	Trc_JVMTI_jvmtiDeregisterTracePointSubscriber_Entry(env, subscriberID);

	ENSURE_NON_NULL(subscriberID);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc != JVMTI_ERROR_NONE) {
		rc = JVMTI_ERROR_INVALID_ENVIRONMENT;
		goto done;
	}

	/* Check that trace engine interface is available */
	rc = JVMTI_ERROR_INVALID_ENVIRONMENT;
	rasGlobal = (RasGlobalStorage *)vm->j9rasGlobalStorage;
	if (rasGlobal != NULL ) {
		traceInterface = (UtInterface *)(rasGlobal->utIntf);
	}
	if ((traceInterface != NULL) && (traceInterface->server != NULL)) {

		/* Call into the trace engine to deregister the subscriber */
		omr_error_t result = traceInterface->server->DeregisterTracePointSubscriber(UT_THREAD_FROM_VM_THREAD(currentThread), subscriberID );

		/* Map result back to JVMTI return code */
		switch (result) {
			case OMR_ERROR_NONE:
				rc = JVMTI_ERROR_NONE;
				break;
			case OMR_ERROR_OUT_OF_NATIVE_MEMORY:
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
				break;
			case OMR_ERROR_ILLEGAL_ARGUMENT:
				rc = JVMTI_ERROR_ILLEGAL_ARGUMENT;
				break;
			case OMR_ERROR_NOT_AVAILABLE:
				rc = JVMTI_ERROR_NOT_AVAILABLE;
				break;
			case OMR_ERROR_INTERNAL: /* fall through */
			default:
				rc = JVMTI_ERROR_INTERNAL;
				break;
		}
	}

	done:
	TRACE_JVMTI_RETURN(jvmtiDeregisterTracePointSubscriber);
}
