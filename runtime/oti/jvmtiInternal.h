/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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

#ifndef jvmtiInternal_h
#define jvmtiInternal_h

#include <string.h>
#include "jvmti.h"
#include "ibmjvmti.h"
#include "j9.h"

#ifndef J9VM_SIZE_SMALL_CODE
#define J9_JVMTI_PERFORM_CHECKS
#endif

typedef enum { 
	J9JVMTI_BEFORE_FIRST_EXTENSION_EVENT = JVMTI_MAX_EVENT_TYPE_VAL,
	J9JVMTI_EVENT_COM_IBM_COMPILING_START,
	J9JVMTI_EVENT_COM_IBM_COMPILING_END,
	J9JVMTI_EVENT_COM_IBM_INSTRUMENTABLE_OBJECT_ALLOC,
	J9JVMTI_EVENT_COM_IBM_VM_DUMP_START,
	J9JVMTI_EVENT_COM_IBM_VM_DUMP_END,
	J9JVMTI_EVENT_COM_IBM_GARBAGE_COLLECTION_CYCLE_START,
	J9JVMTI_EVENT_COM_IBM_GARBAGE_COLLECTION_CYCLE_FINISH,
	J9JVMTI_EVENT_COM_SUN_HOTSPOT_EVENTS_VIRTUAL_THREAD_MOUNT,
	J9JVMTI_EVENT_COM_SUN_HOTSPOT_EVENTS_VIRTUAL_THREAD_UNMOUNT,
	J9JVMTI_AFTER_LAST_EXTENSION_EVENT
} J9JVMTIExtensionEvent;

typedef enum {
	J9JVMTI_STACK_TRACE_PRUNE_UNREPORTED_METHODS	= COM_IBM_GET_STACK_TRACE_PRUNE_UNREPORTED_METHODS,
	J9JVMTI_STACK_TRACE_ENTRY_LOCAL_STORAGE			= COM_IBM_GET_STACK_TRACE_ENTRY_LOCAL_STORAGE,
	J9JVMTI_STACK_TRACE_EXTRA_FRAME_INFO			= COM_IBM_GET_STACK_TRACE_EXTRA_FRAME_INFO,
	J9JVMTI_STACK_TRACE_MARK_INLINED_FRAMES			= COM_IBM_GET_STACK_TRACE_MARK_INLINED_FRAMES
} J9JVMTIStackTraceType;

#define J9JVMTI_LOWEST_EXTENSION_EVENT (J9JVMTI_BEFORE_FIRST_EXTENSION_EVENT + 1)
#define J9JVMTI_HIGHEST_EXTENSION_EVENT (J9JVMTI_AFTER_LAST_EXTENSION_EVENT - 1)

#define J9JVMTI_LOWEST_EVENT JVMTI_MIN_EVENT_TYPE_VAL
#define J9JVMTI_HIGHEST_EVENT J9JVMTI_HIGHEST_EXTENSION_EVENT

#define J9JVMTI_EVENT_COUNT (J9JVMTI_HIGHEST_EXTENSION_EVENT - J9JVMTI_LOWEST_EVENT + 1)

#define J9JVMTI_UDATA_BITS (sizeof(UDATA) * 8)

#define J9JVMTI_GETVMTHREAD_ERROR_ON_NULL_JTHREAD 0x1
#define J9JVMTI_GETVMTHREAD_ERROR_ON_DEAD_THREAD 0x2
#define J9JVMTI_GETVMTHREAD_ERROR_ON_VIRTUALTHREAD 0x4

typedef struct {
	J9NativeLibrary nativeLib;
	char * options;
	UDATA decorate;
	J9VMDllLoadInfo * xRunLibrary;
	UDATA loadCount;
	J9InvocationJavaVM * invocationJavaVM;
} J9JVMTIAgentLibrary;

typedef struct {
	UDATA eventEnable[(J9JVMTI_EVENT_COUNT + J9JVMTI_UDATA_BITS - 1) / J9JVMTI_UDATA_BITS];
} J9JVMTIEventEnableMap;

typedef struct {
	jvmtiExtensionEvent callbacks[J9JVMTI_HIGHEST_EXTENSION_EVENT - J9JVMTI_LOWEST_EXTENSION_EVENT + 1];
} J9JVMTIExtensionCallbacks;

#define J9JVMTI_EXTENSION_CALLBACK(env, index) ((env)->extensionCallbacks.callbacks[ (index) - J9JVMTI_LOWEST_EXTENSION_EVENT])

typedef struct J9JVMTIThreadData {
	J9VMThread * vmThread;
	void * tls;
	J9JVMTIEventEnableMap threadEventEnable;
} J9JVMTIThreadData;

typedef struct J9JVMTIObjectTag {
	j9object_t ref;
	jlong tag;
} J9JVMTIObjectTag;

#define J9JVMTI_WATCHED_FIELD_BITS_PER_FIELD 2
#define J9JVMTI_WATCHED_FIELDS_PER_UDATA \
	((sizeof(UDATA) * 8) / J9JVMTI_WATCHED_FIELD_BITS_PER_FIELD)
#define J9JVMTI_CLASS_REQUIRES_ALLOCATED_J9JVMTI_WATCHED_FIELD_ACCESS_BITS(clazz) \
	((clazz)->romClass->romFieldCount > J9JVMTI_WATCHED_FIELDS_PER_UDATA)
#define J9JVMTI_WATCHED_FIELD_ARRAY_INDEX(index) \
	((index) / J9JVMTI_WATCHED_FIELDS_PER_UDATA)
#define J9JVMTI_WATCHED_FIELD_ACCESS_BIT(index) \
	((UDATA)1 << (((index) % J9JVMTI_WATCHED_FIELDS_PER_UDATA) * J9JVMTI_WATCHED_FIELD_BITS_PER_FIELD))
#define J9JVMTI_WATCHED_FIELD_MODIFICATION_BIT(index) \
	(J9JVMTI_WATCHED_FIELD_ACCESS_BIT(index) << 1)

typedef struct J9JVMTIWatchedClass {
	J9Class *clazz;
	UDATA *watchBits;
} J9JVMTIWatchedClass;

typedef struct J9JVMTIBreakpointedMethod {
	UDATA referenceCount;
	J9Method * method;
	J9ROMMethod * originalROMMethod;
	J9ROMMethod * copiedROMMethod;
} J9JVMTIBreakpointedMethod;

typedef struct J9JVMTIGlobalBreakpoint {
	UDATA referenceCount;
	UDATA flags;
	J9JVMTIBreakpointedMethod * breakpointedMethod;
	IDATA location;
	struct J9JVMTIGlobalBreakpoint * equivalentBreakpoint;
} J9JVMTIGlobalBreakpoint;

typedef struct J9JVMTIAgentBreakpoint {
	jmethodID method;
	IDATA location;
	J9JVMTIGlobalBreakpoint * globalBreakpoint;
} J9JVMTIAgentBreakpoint;

typedef struct J9JVMTICompileEvent {
	struct J9JVMTICompileEvent * linkPrevious;
	struct J9JVMTICompileEvent * linkNext;
	jmethodID methodID;
	const void* code_addr;
	const void* compile_info;
	UDATA code_size;
	UDATA isLoad;
} J9JVMTICompileEvent;

typedef struct J9JVMTIHookInterfaceWithID {
	J9HookInterface ** hookInterface;
	UDATA agentID;
} J9JVMTIHookInterfaceWithID;

typedef struct J9JVMTIEnv {
	jvmtiNativeInterface * functions;
	J9JavaVM* vm;
	J9NativeLibrary * library;
	UDATA flags;
	struct J9JVMTIEnv * linkPrevious;
	struct J9JVMTIEnv * linkNext;
	omrthread_monitor_t mutex;
	void* environmentLocalStorage;
	jvmtiCapabilities capabilities;
	jvmtiEventCallbacks callbacks;
	J9JVMTIExtensionCallbacks extensionCallbacks;
	omrthread_monitor_t threadDataPoolMutex;
	J9Pool* threadDataPool;
	J9HashTable* objectTagTable;
	J9JVMTIEventEnableMap globalEventEnable;
	J9HashTable *watchedClasses;
	J9Pool* breakpoints;
#if JAVA_SPEC_VERSION >= 19
	UDATA tlsKey;
#else /* JAVA_SPEC_VERSION >= 19 */
	omrthread_tls_key_t tlsKey;
#endif /* JAVA_SPEC_VERSION >= 19 */
	J9JVMTIHookInterfaceWithID vmHook;
	J9JVMTIHookInterfaceWithID gcHook;
	J9JVMTIHookInterfaceWithID gcOmrHook;
	jint prefixCount;
	char * prefixes;
#if defined (J9VM_INTERP_NATIVE_SUPPORT)
	J9JVMTIHookInterfaceWithID jitHook;
#endif 
} J9JVMTIEnv;

#define J9JVMTIENV_FLAG_DISPOSED 1
#define J9JVMTIENV_FLAG_UNUSED_2 2
#define J9JVMTIENV_FLAG_CLASS_LOAD_HOOK_EVER_ENABLED 4
#define J9JVMTIENV_FLAG_RETRANSFORM_CAPABLE 8


typedef struct J9JVMTIData {
	J9JavaVM* vm;
	UDATA flags;
	struct J9JVMTIEnv * environmentsHead;
	struct J9JVMTIEnv * environmentsTail;
	J9Pool* agentLibraries;
	J9NativeLibrary * agentLibrariesHead;
	J9NativeLibrary * agentLibrariesTail;
	J9Pool* environments;
	omrthread_monitor_t mutex;
	UDATA phase;
	UDATA requiredDebugAttributes;
	J9Pool* breakpoints;
	J9Pool* breakpointedMethods;
	jniNativeInterface * copiedJNITable;
	omrthread_monitor_t redefineMutex;
	UDATA lastClassCount;
#if defined (J9VM_INTERP_NATIVE_SUPPORT)
	UDATA compileEventThreadState;
	omrthread_t compileEventThread;
	J9VMThread * compileEventVMThread;
	J9Pool* compileEvents;
	J9JVMTICompileEvent* compileEventQueueHead;
	omrthread_monitor_t compileEventMutex;
#endif
} J9JVMTIData;

/** 
 *  Global JVMTI flags (bits)
 */ 
#define J9JVMTI_FLAG_REDEFINE_CLASS_EXTENSIONS_ENABLED 1   /** Class Redefinition Extensions are enabled */
#define J9JVMTI_FLAG_REDEFINE_CLASS_EXTENSIONS_USED    2   /** Class Redefinition Extensions have been actually used. Set (and remains set) the first time a redefined class with extensions has been used */
#define J9JVMTI_FLAG_SAMPLED_OBJECT_ALLOC_ENABLED      4   /** Sampling Object Allocation is enabled */

#define J9JVMTI_COMPILE_EVENT_THREAD_STATE_NEW 0
#define J9JVMTI_COMPILE_EVENT_THREAD_STATE_ALIVE 1
#define J9JVMTI_COMPILE_EVENT_THREAD_STATE_DYING 2
#define J9JVMTI_COMPILE_EVENT_THREAD_STATE_DEAD 3

typedef struct J9JVMTIMethodEquivalence {
	J9Method* oldMethod;
	J9Method* currentMethod;
} J9JVMTIMethodEquivalence;

typedef struct J9JVMTIAgentBreakpointDoState {
	J9JVMTIEnv * j9env;
	pool_state environmentState;
	pool_state breakpointState;
} J9JVMTIAgentBreakpointDoState;

/**
 *  Used by the JVMTI GetConstantPool and GetBytecodes API calls to construct a HashTable
 *  of constant pool translation entries.
 *  
 *  A HashTable entry used to describe one constant pool item. The entries are created by 
 *  first scanning a J9ROMClass and reindexing it on the fly to account for the issues with
 *  our internal representation of UTF8, NameAndSignature and Long/Double types 
 */
typedef struct jvmtiGcp_translationEntry {
	void	*key;		/*!<	HashTable key for this entry */
	U_8	cpType;		/*!< 	Constant Pool item type */
	U_16	sunCpIndex;	/*!<	translated SUN constant pool index for this entry */

	union type {
		struct {
			J9UTF8  *data;		/*!< UTF8 data for CONSTANT_Utf8 item */
		} utf8;
		struct {
			J9UTF8 *utf8;		/*!< UTF8 data describing the class name of the CONSTANT_Class item */
			U_32 nameIndex;		/*!< The translated index of the class name item on the SUN constant pool */
		} clazz;
		struct {
			J9UTF8 *utf8;		/*!< UTF8 data describing the string name of the CONSTANT_String item */
			U_32 stringIndex;	/*!< The translated index of the string name item on the SUN constant pool */
		} string;
		struct {
			J9ROMMethodTypeRef *methodType;  /*!< MethodType itself */
			J9UTF8 *utf8;			/*!< UTF8 data describing the methodtype descriptor of the CONSTANT_MethodType item */
			U_32 methodTypeIndex;	/*!< The translated index of the string name item on the SUN constant pool */
			U_32 classIndex;		/*!< The translated index of the class referenced by this item if its from an invokevirtual */
			U_32 nameAndTypeIndex;	/*!< The translated index of the NameAndType/Signature referenced by this item if its from an invokevirtual */
		} methodType;
		struct {
			U_32 methodOrFieldRefIndex;	/*!< The translated index of the field/method referenced by this item */
			U_32 handleType;			/*!< The handleKind for this item */
		} methodHandle;
		struct {
			J9ROMNameAndSignature *nas;	/*!< NaS referred to by this constant_invokedynamic */
			U_32 bsmIndex;				/*!< The index into the BootstrapMethods attribute */
			U_32 nameAndTypeIndex;		/*!< The translated index of the NameAndType/Signature referenced by this item */
		} invokedynamic;
		struct { 
			J9ROMFieldRef *ref;	/*!< VM Field/Method reference */
			U_32 classIndex;	/*!< The translated index of the class referenced by this item */
			U_32 nameAndTypeIndex;	/*!< The translated index of the NameAndType/Signature referenced by this item */
		} ref;
		struct {
			J9UTF8 *name;		/*!< UTF8 data describing the name of the referenced item */
			J9UTF8 *signature;	/*!< UTF8 data describing the signature of the referenced item */
			U_32 nameIndex;		/*!< The index of the UTF8 for the name field of CONSTANT_NameAndSignature */
			U_32 signatureIndex;	/*!< The index of the UTF8 for the signature/type field of CONSTANT_NameAndSignature */
		} nas;
		struct {
			U_32 data;		/*!< Integer constant data */
		} intFloat;
		struct {
			U_32 slot1;		/*!< high order 32bits of the Long/Double constant */
			U_32 slot2;		/*!< low order  32bits of the Long/Double constant */
		} longDouble;
	} type;
	
} jvmtiGcp_translationEntry; 

 
typedef struct jvmtiGcp_translation {
	J9HashTable			*ht;	/*!< A HashTable of reindexed constant pool items contained in jvmtiGcp_translationEntry entries */
	jvmtiGcp_translationEntry	**cp;	/*!< An array of J9HashTable nodes indexed by the translated SUN constant pool indices */
	J9ROMConstantPoolItem 		*romConstantPool;	/*!< J9RomClass constant pool for the class under translation */
	U_32				cpSize;		/*!< Number of translated constant pool items */
	U_32				totalSize;	/*!< Total number of bytes required for writing the translated constant pool */
} jvmtiGcp_translation;


#define JAVAVM_FROM_ENV(env) (((J9JVMTIEnv *) (env))->vm)
#define J9JVMTI_DATA_FROM_VM(vm) ((J9JVMTIData *) ((vm)->jvmtiData))
#define J9JVMTI_DATA_FROM_ENV(env) J9JVMTI_DATA_FROM_VM(JAVAVM_FROM_ENV(env))
#define J9JVMTI_PHASE(env) (J9JVMTI_DATA_FROM_ENV(env)->phase)

#define TRACE_JVMTI_RETURN(func) \
	do { \
		Trc_JVMTI_##func##_Exit(rc); \
		return rc; \
	} while(0)

#define TRACE_ONE_JVMTI_RETURN(func, param) \
	do { \
		Trc_JVMTI_##func##_Exit(rc, param); \
		return rc; \
	} while(0)

#define TRACE_TWO_JVMTI_RETURN(func, param, param2) \
	do { \
		Trc_JVMTI_##func##_Exit(rc, param, param2); \
		return rc; \
	} while(0)

#define TRACE_THREE_JVMTI_RETURN(func, param, param2, param3) \
	do { \
		Trc_JVMTI_##func##_Exit(rc, param, param2, param3); \
		return rc; \
	} while(0)

#define TRACE_FOUR_JVMTI_RETURN(func, param, param2, param3, param4) \
	do { \
		Trc_JVMTI_##func##_Exit(rc, param, param2, param3, param4); \
		return rc; \
	} while(0)

/*
 * True if the given JLClass instance is a JLClass instance but is not fully initialized.
 */
#define J9VM_IS_UNINITIALIZED_HEAPCLASS(vmThread, _clazzObject) \
		(\
				(J9OBJECT_CLAZZ((vmThread), (_clazzObject)) == J9VMJAVALANGCLASS_OR_NULL((J9VMTHREAD_JAVAVM(vmThread)))) && \
				(0 == ((J9Class *)J9VMJAVALANGCLASS_VMREF((vmThread), (j9object_t)(_clazzObject)))) \
		)

#define J9VM_IS_UNINITIALIZED_HEAPCLASS_VM(vm, _clazzObject) \
		(\
				(J9OBJECT_CLAZZ_VM((vm), (_clazzObject)) == J9VMJAVALANGCLASS_OR_NULL((vm))) && \
				(0 == ((J9Class *)J9VMJAVALANGCLASS_VMREF_VM((vm), (j9object_t *)(_clazzObject)))) \
		)


#ifdef J9_JVMTI_PERFORM_CHECKS

#define JVMTI_ERROR(err) \
	do { \
		rc = (err); \
		goto done; \
	} while(0)

#define ENSURE_PHASE_START_OR_LIVE(env) \
	do { \
		UDATA phase = J9JVMTI_PHASE(env); \
		if ((phase != JVMTI_PHASE_START) && (phase != JVMTI_PHASE_LIVE)) { \
			JVMTI_ERROR(JVMTI_ERROR_WRONG_PHASE); \
		} \
	} while(0)

#define ENSURE_PHASE_ONLOAD_OR_LIVE(env) \
	do { \
		UDATA phase = J9JVMTI_PHASE(env); \
		if ((phase != JVMTI_PHASE_LIVE) && (phase != JVMTI_PHASE_ONLOAD)) { \
			JVMTI_ERROR(JVMTI_ERROR_WRONG_PHASE); \
		} \
	} while(0)

#define ENSURE_PHASE_ONLOAD(env) \
	do { \
		if (J9JVMTI_PHASE(env) != JVMTI_PHASE_ONLOAD) { \
			JVMTI_ERROR(JVMTI_ERROR_WRONG_PHASE); \
		} \
	} while(0)

#define ENSURE_PHASE_LIVE(env) \
	do { \
		if (J9JVMTI_PHASE(env) != JVMTI_PHASE_LIVE) { \
			JVMTI_ERROR(JVMTI_ERROR_WRONG_PHASE); \
		} \
	} while(0)

#define ENSURE_PHASE_NOT_DEAD(env) \
	do { \
		if (J9JVMTI_PHASE(env) == JVMTI_PHASE_DEAD) { \
			JVMTI_ERROR(JVMTI_ERROR_WRONG_PHASE); \
		} \
	} while(0)

#define ENSURE_CAPABILITY(env, capability) \
	do { \
		if (!(((J9JVMTIEnv *) (env))->capabilities.capability)) { \
			JVMTI_ERROR(JVMTI_ERROR_MUST_POSSESS_CAPABILITY); \
		} \
	} while(0)

#define ENSURE_NON_NULL(var) \
	do { \
		if ((var) == NULL) { \
			JVMTI_ERROR(JVMTI_ERROR_NULL_POINTER); \
		} \
	} while(0)

#define ENSURE_NON_NEGATIVE(var) \
	do { \
		if ((var) < 0) { \
			JVMTI_ERROR(JVMTI_ERROR_ILLEGAL_ARGUMENT); \
		} \
	} while(0)
 
#define ENSURE_POSITIVE(var) \
	do { \
		if ((var) <= 0) { \
			JVMTI_ERROR(JVMTI_ERROR_ILLEGAL_ARGUMENT); \
		} \
	} while(0) 

#define ENSURE_JMETHODID_NON_NULL(var) \
	do { \
		if ((var) == NULL) { \
			JVMTI_ERROR(JVMTI_ERROR_INVALID_METHODID); \
		} \
	} while(0)

#define ENSURE_JFIELDID_NON_NULL(var) \
	do { \
		if ((var) == NULL) { \
			JVMTI_ERROR(JVMTI_ERROR_INVALID_FIELDID); \
		} \
	} while(0)

#define ENSURE_JNI_OBJECT_NON_NULL(var, err) \
	do { \
		if (((var) == NULL) || (*((j9object_t*) (var)) == NULL)) { \
			JVMTI_ERROR(err); \
		} \
	} while(0)

#define ENSURE_JOBJECT_NON_NULL(var) ENSURE_JNI_OBJECT_NON_NULL((var), JVMTI_ERROR_INVALID_OBJECT)
#define ENSURE_JCLASS_NON_NULL(var) ENSURE_JNI_OBJECT_NON_NULL((var), JVMTI_ERROR_INVALID_CLASS)
#define ENSURE_JTHREADGROUP_NON_NULL(var) ENSURE_JNI_OBJECT_NON_NULL((var), JVMTI_ERROR_INVALID_THREAD_GROUP)
#define ENSURE_JOBJECT_NON_NULL(var) ENSURE_JNI_OBJECT_NON_NULL((var), JVMTI_ERROR_INVALID_OBJECT)

#define ENSURE_JTHREAD(vmThread, jthrd) \
    do { \
        if (!IS_JAVA_LANG_THREAD((vmThread), J9_JNI_UNWRAP_REFERENCE(jthrd))) { \
            JVMTI_ERROR(JVMTI_ERROR_INVALID_THREAD); \
        } \
    } while(0)

#define ENSURE_JCLASS(vmThread, jcls) \
    do { \
        if (!isSameOrSuperClassOf(J9VMJAVALANGCLASS_OR_NULL((vmThread->javaVM)), J9OBJECT_CLAZZ((vmThread), *((j9object_t *) (jcls))))) { \
            JVMTI_ERROR(JVMTI_ERROR_INVALID_CLASS); \
        } \
    } while(0)

#define ENSURE_VALID_HEAP_OBJECT_FILTER(var) \
	do { \
		if (((var) != JVMTI_HEAP_OBJECT_TAGGED) && ((var) != JVMTI_HEAP_OBJECT_UNTAGGED) && ((var) != JVMTI_HEAP_OBJECT_EITHER)) { \
			JVMTI_ERROR(JVMTI_ERROR_ILLEGAL_ARGUMENT); \
		} \
	} while(0)

#define ENSURE_MONITOR_NON_NULL(var) \
	do { \
		if ((var) == NULL) { \
			JVMTI_ERROR(JVMTI_ERROR_INVALID_MONITOR); \
		} \
	} while(0)

#if JAVA_SPEC_VERSION >= 19
#define ENSURE_JTHREAD_NOT_VIRTUAL(vmThread, jthrd, error) \
	do { \
		if (IS_JAVA_LANG_VIRTUALTHREAD((vmThread), J9_JNI_UNWRAP_REFERENCE(jthrd))) { \
			JVMTI_ERROR(error); \
		} \
	} while(0)

#define ENSURE_JTHREADOBJECT_NOT_VIRTUAL(vmThread, jthrdObject, error) \
	do { \
		if (IS_JAVA_LANG_VIRTUALTHREAD((vmThread), (jthrdObject))) { \
			JVMTI_ERROR(error); \
		} \
	} while(0)
#endif /* JAVA_SPEC_VERSION >= 19 */

#else

#define ENSURE_PHASE_START_OR_LIVE(env) 
#define ENSURE_PHASE_ONLOAD_OR_LIVE(env)
#define ENSURE_PHASE_ONLOAD(env)
#define ENSURE_PHASE_LIVE(env)
#define ENSURE_CAPABILITY(env, capability)
#define ENSURE_NON_NULL(var)
#define ENSURE_NON_NEGATIVE(var)
#define ENSURE_JMETHODID_NON_NULL(var)
#define ENSURE_JFIELDID_NON_NULL(var)
#define ENSURE_JOBJECT_NON_NULL(var)
#define ENSURE_JCLASS_NON_NULL(var)
#define ENSURE_JTHREADGROUP_NON_NULL(var)
#define ENSURE_VALID_HEAP_OBJECT_FILTER(var)
#define ENSURE_MONITOR_NON_NULL(var)
#if JAVA_SPEC_VERSION >= 19
#define ENSURE_JTHREAD_NOT_VIRTUAL(vmThread, jthrd, error)
#define ENSURE_JTHREADOBJECT_NOT_VIRTUAL(vmThread, jthrdObject, error)
#endif /* JAVA_SPEC_VERSION >= 19 */

#endif

#define NORMALIZED_EVENT_NUMBER(eventNumber) \
	((eventNumber) - J9JVMTI_LOWEST_EVENT)

#define EVENT_WORD(eventNumber, eventMap) \
	((eventMap)->eventEnable[NORMALIZED_EVENT_NUMBER(eventNumber) / J9JVMTI_UDATA_BITS])

#define EVENT_BIT(eventNumber) \
	(((UDATA)1) << (NORMALIZED_EVENT_NUMBER(eventNumber) % J9JVMTI_UDATA_BITS))

#define EVENT_IS_ENABLED(eventNumber, eventMap) \
	(EVENT_WORD(eventNumber, eventMap) & EVENT_BIT(eventNumber))

#define ENABLE_EVENT(eventNumber, eventMap) \
	EVENT_WORD(eventNumber, eventMap) |= EVENT_BIT(eventNumber)

#define DISABLE_EVENT(eventNumber, eventMap) \
	EVENT_WORD(eventNumber, eventMap) &= ~EVENT_BIT(eventNumber)

#ifdef DEBUG
#define TRACE_JVMTI_RETURN_NOT_IMPLEMENTED(func) \
	do { \
		J9PortLibrary * notImplementedPortlib = JAVAVM_FROM_ENV(env)->portLibrary; \
		OMRPORT_FROM_J9PORT(notImplementedPortlib)->tty_printf(OMRPORT_FROM_J9PORT(notImplementedPortlib), "JVMTI call not implemented: " J9_GET_CALLSITE() "\n"); \
		rc = JVMTI_ERROR_INTERNAL; \
		TRACE_JVMTI_RETURN(func); \
	} while (0)
#else
#define TRACE_JVMTI_RETURN_NOT_IMPLEMENTED(func) \
	do { \
		rc = JVMTI_ERROR_INTERNAL; \
		TRACE_JVMTI_RETURN(func); \
	} while(0)
#endif

#define GET_SUPERCLASS(clazz) \
	((J9CLASS_DEPTH(clazz) == 0) ? NULL : \
		(clazz)->superclasses[J9CLASS_DEPTH(clazz) - 1])

#define IBMJVMTI_EXTENDED_CALLSTACK     1
#define IBMJVMTI_UNEXTENDED_CALLSTACK   0

#if JAVA_SPEC_VERSION >= 19
/* These macros corresponds to the states in j.l.VirtualThread. */
#define JVMTI_VTHREAD_STATE_NEW 0
#define JVMTI_VTHREAD_STATE_STARTED 1
#define JVMTI_VTHREAD_STATE_RUNNABLE 2
#define JVMTI_VTHREAD_STATE_RUNNING 3
#define JVMTI_VTHREAD_STATE_PARKING 4
#define JVMTI_VTHREAD_STATE_PARKED 5
#define JVMTI_VTHREAD_STATE_PINNED 6
#define JVMTI_VTHREAD_STATE_TIMED_PARKING 7
#define JVMTI_VTHREAD_STATE_TIMED_PARKED 8
#define JVMTI_VTHREAD_STATE_TIMED_PINNED 9
#define JVMTI_VTHREAD_STATE_YIELDING 10
#define JVMTI_VTHREAD_STATE_TERMINATED 99
#define JVMTI_VTHREAD_STATE_SUSPENDED (1 << 8)
#endif /* JAVA_SPEC_VERSION >= 19 */

/* The brace mismatches in the macros below are due to the usage pattern:
 *
 * JVMTI_ENVIRONMENTS_DO(jvmtiData, j9env) {
 * 		...use j9env...
 * 		JVMTI_ENVIRONMENTS_NEXT_DO(jvmtiData, j9env);
 * }
 *
 */

#define JVMTI_ENVIRONMENTS_DO(jvmtiData, j9env) \
	(j9env) = (jvmtiData)->environmentsHead; \
	while ((j9env) != NULL) { \
		if (0 == ((j9env)->flags & J9JVMTIENV_FLAG_DISPOSED))

#define JVMTI_ENVIRONMENTS_NEXT_DO(jvmtiData, j9env) \
		} \
		(j9env) = (j9env)->linkNext
	
#define JVMTI_ENVIRONMENTS_REVERSE_DO(jvmtiData, j9env) \
	(j9env) = (jvmtiData)->environmentsTail; \
	while ((j9env) != NULL) { \
		if (0 == ((j9env)->flags & J9JVMTIENV_FLAG_DISPOSED))

#define JVMTI_ENVIRONMENTS_REVERSE_NEXT_DO(jvmtiData, j9env) \
		} \
		(j9env) = (j9env)->linkPrevious

#define PORT_ACCESS_FROM_JVMTI(env) PORT_ACCESS_FROM_JAVAVM(((J9JVMTIEnv *) (env))->vm)

#endif     /* jvmtiInternal_h */
