/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "jvmtiHelpers.h"
#include "jvmti_internal.h"

typedef struct J9JVMTIClassStats {
	J9JavaVM * vm;
	J9VMThread * currentThread;
	jint classCount;
	jclass * classRefs;
} J9JVMTIClassStats;

#define ENSURE_CLASS_PRIMITIVE_ARRAY_OR_PREPARED(clazz) \
	do { \
		if (J9ROMCLASS_IS_PRIMITIVE_OR_ARRAY((clazz)->romClass) == 0) { \
			if ((getClassStatus(clazz) & JVMTI_CLASS_STATUS_PREPARED) == 0) { \
				JVMTI_ERROR(JVMTI_ERROR_CLASS_NOT_PREPARED); \
			} \
		} \
	} while(0)

/* Static J9UTF8's used to for MethodHandle.invokeExact/invoke methods */
#define DECLARE_CLASSNAME_LIST_DATA(instanceName, name) \
static const struct { \
	U_16 length; \
	U_8 data[sizeof(name) - 1]; \
} instanceName = { sizeof(name) - 1, name }

DECLARE_CLASSNAME_LIST_DATA(MH_invokeExact_utf, "invokeExact");
DECLARE_CLASSNAME_LIST_DATA(MH_invoke_utf, "invoke");
DECLARE_CLASSNAME_LIST_DATA(MH_methodHandle_utf, "java.lang.invoke.MethodHandle");

static jvmtiError redefineClassesCommon(jvmtiEnv* env, jint class_count, const jvmtiClassDefinition* class_definitions, J9VMThread * currentThread, UDATA options);
static void restoreBreakpointsInClasses (J9VMThread * currentThread, J9HashTable* classHashTable);
static void clearBreakpointsInClasses (J9VMThread * currentThread, J9HashTable* classHashTable);
static jint getClassStatus (J9Class * clazz);
static UDATA countInitiatedClass (J9Class * clazz, J9JVMTIClassStats * results);
static UDATA copyInitiatedClass (J9Class * clazz, J9JVMTIClassStats * results);
static UDATA jvmtiGetConstantPool_HashFn(void *entry, void *userData);
static UDATA jvmtiGetConstantPool_HashEqualFn(void *lhsEntry, void *rhsEntry, void *userData);
static jvmtiError jvmtiGetConstantPool_writeConstants(jvmtiGcp_translation *translation, unsigned char *constantPoolBuf);
static jvmtiError jvmtiGetConstantPool_addNAS(jvmtiGcp_translation *translation, J9ROMNameAndSignature *nas, U_32 *sunCpIndex, U_32 *refIndex);
static jvmtiError jvmtiGetConstantPool_addReference(jvmtiGcp_translation *translation, UDATA cpIndex, U_8 cpType, J9ROMFieldRef *ref, U_32 *sunCpIndex);
static jvmtiError jvmtiGetConstantPool_addUTF8(jvmtiGcp_translation *translation, J9UTF8 *utf8, U_32 *sunCpIndex, U_32 *refIndex);
static jvmtiError jvmtiGetConstantPool_addClassOrString(jvmtiGcp_translation *translation, UDATA cpIndex, U_8 cpType, J9UTF8 *utf8, U_32 *sunCpIndex, U_32 *refIndex);
static jvmtiError jvmtiGetConstantPool_addIntFloat(jvmtiGcp_translation *translation, UDATA cpIndex, U_8 cpType, U_32 data, U_32 *sunCpIndex);
static jvmtiError jvmtiGetConstantPool_addLongDouble(jvmtiGcp_translation *translation, UDATA cpIndex, U_8 cpType, U_32 slot1, U_32 slot2, U_32 *sunCpIndex);
static jvmtiError jvmtiGetConstantPool_addMethodHandle(jvmtiGcp_translation *translation, UDATA cpIndex, U_8 cpType, J9ROMMethodHandleRef *ref, U_32 *sunCpIndex);
static jvmtiError jvmtiGetConstantPool_addMethodType(jvmtiGcp_translation *translation, UDATA cpIndex, U_8 cpType, J9ROMMethodTypeRef *ref, U_32 *sunCpIndex);
static jvmtiError jvmtiGetConstantPool_addNAS_name_sig(jvmtiGcp_translation *translation, void * key, J9UTF8 *name, J9UTF8* sig, U_32 *sunCpIndex, U_32 *refIndex);
static jvmtiError jvmtiGetConstantPool_addInvokeDynamic(jvmtiGcp_translation *translation, UDATA key, J9ROMNameAndSignature *nameAndSig, U_32 bsmIndex, U_32 *sunCpIndex);


/** 
 * \brief	Get the count of loaded classes  
 * 
 * @param[in] vm 
 * @return  Count of loaded classes
 * 
 *	Assumes caller acquired classTableMutex 
 */
static jint
jvmtiGetLoadedClassesCount(J9JavaVM * vm)
{
	J9ClassWalkState classWalkState;
	jint classCount = 0;
	J9Class *clazz;

	/* Count classes (ignore primitive types and old versions of redefined classes) */
	clazz = vm->internalVMFunctions->allLiveClassesStartDo(&classWalkState, vm, NULL);
	while (clazz) {
		if (J9ROMCLASS_IS_PRIMITIVE_TYPE(clazz->romClass) == 0) {
			if ((J9CLASS_FLAGS(clazz) & J9AccClassHotSwappedOut) == 0) {
				classCount++;
			}
		}
		clazz = vm->internalVMFunctions->allLiveClassesNextDo(&classWalkState);
	}
	vm->internalVMFunctions->allLiveClassesEndDo(&classWalkState);

	return classCount;
}

jvmtiError JNICALL
jvmtiGetLoadedClasses(jvmtiEnv* env,
	jint* class_count_ptr,
	jclass** classes_ptr)
{
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_ENV(env);
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	jclass * classRefs = NULL;
	jvmtiError rc;
	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_JVMTI_jvmtiGetLoadedClasses_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9ClassWalkState classWalkState;
		jint lastClassCount;
		J9Class *clazz;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);

		ENSURE_NON_NULL(class_count_ptr);
		ENSURE_NON_NULL(classes_ptr);

		omrthread_monitor_enter(vm->classTableMutex);

		lastClassCount = (jint) jvmtiData->lastClassCount;
		if (lastClassCount == 0) {
			lastClassCount = jvmtiGetLoadedClassesCount(vm);
		}

		classRefs = j9mem_allocate_memory(lastClassCount * sizeof(jclass), J9MEM_CATEGORY_JVMTI_ALLOCATE);
		if (classRefs == NULL) {
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
		} else {
			jint i = 0;

			/* Copy class references (ignore primitive types and old versions of redefined classes) */
			clazz = vm->internalVMFunctions->allLiveClassesStartDo(&classWalkState, vm, NULL);
			while (clazz) {
				/* Check if we need more memory */
				if (i == lastClassCount) {
					jclass * tempClassRefs;

					lastClassCount = lastClassCount + 128; 
					tempClassRefs = j9mem_reallocate_memory(classRefs, lastClassCount * sizeof(jclass), J9MEM_CATEGORY_JVMTI);
					if (NULL == tempClassRefs) {
						/* realloc failed - need to free the original allocation */
						j9mem_free_memory(classRefs);
						classRefs = NULL;
						vm->internalVMFunctions->allLiveClassesEndDo(&classWalkState);
						omrthread_monitor_exit(vm->classTableMutex);
						rc = JVMTI_ERROR_OUT_OF_MEMORY;
						goto done;
					}
					classRefs = tempClassRefs;
				}

				if (J9ROMCLASS_IS_PRIMITIVE_TYPE(clazz->romClass) == 0) {
					if ((J9CLASS_FLAGS(clazz) & J9AccClassHotSwappedOut) == 0) {
						classRefs[i++] = (jclass)vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *) currentThread, J9VM_J9CLASS_TO_HEAPCLASS(clazz));
					}
				}

				clazz = vm->internalVMFunctions->allLiveClassesNextDo(&classWalkState);
			}
			vm->internalVMFunctions->allLiveClassesEndDo(&classWalkState);

			jvmtiData->lastClassCount = (UDATA) i;
			*class_count_ptr = i;
			*classes_ptr = classRefs;
		}

		omrthread_monitor_exit(vm->classTableMutex);

done:
		vm->internalVMFunctions->internalReleaseVMAccess(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiGetLoadedClasses);
}


jvmtiError JNICALL
jvmtiGetClassLoaderClasses(jvmtiEnv* env,
	jobject initiating_loader,
	jint* class_count_ptr,
	jclass** classes_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	J9ClassLoader * loader;
	jvmtiError rc;
	J9HashTableState walkState;
	J9Class* clazz;
	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_JVMTI_jvmtiGetClassLoaderClasses_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9JVMTIClassStats stats;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);

		ENSURE_NON_NULL(class_count_ptr);
		ENSURE_NON_NULL(classes_ptr);

		/* NULL means to use the boot loader */

		if (initiating_loader == NULL) {
			loader = vm->systemClassLoader;
		} else {
			loader = J9VMJAVALANGCLASSLOADER_VMREF(currentThread, J9_JNI_UNWRAP_REFERENCE(initiating_loader));
			if (NULL == loader) {
				*class_count_ptr = 0;
				*classes_ptr = j9mem_allocate_memory(0, J9MEM_CATEGORY_JVMTI_ALLOCATE);
				if (NULL == *classes_ptr) {
					rc = JVMTI_ERROR_OUT_OF_MEMORY;
				}
				goto done;
			}
		}

		omrthread_monitor_enter(vm->classTableMutex);

		memset(&stats, 0, sizeof(J9JVMTIClassStats));

		stats.vm = vm;
		stats.currentThread = currentThread;

		/* Search for classes who have this class loader as the initiating loader (wind up count) */
		clazz = vm->internalVMFunctions->hashClassTableStartDo(loader, &walkState);
		while (clazz != NULL) {
			countInitiatedClass(clazz, &stats);
			clazz = vm->internalVMFunctions->hashClassTableNextDo(&walkState);
		}

		stats.classRefs = j9mem_allocate_memory(stats.classCount * sizeof(jclass), J9MEM_CATEGORY_JVMTI_ALLOCATE);
		if (stats.classRefs == NULL) {
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
		} else {

			/* Save count before it gets modified below... */
			*class_count_ptr = (jint) stats.classCount;
			*classes_ptr = stats.classRefs;

			/* Record classes who have this class loader as the initiating loader (wind down count) */
			clazz = vm->internalVMFunctions->hashClassTableStartDo(loader, &walkState);
			while (clazz != NULL) {
				copyInitiatedClass(clazz, &stats);
				clazz = vm->internalVMFunctions->hashClassTableNextDo(&walkState);
			}
		}

		omrthread_monitor_exit(vm->classTableMutex);

done:
		vm->internalVMFunctions->internalReleaseVMAccess(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiGetClassLoaderClasses);
}


jvmtiError JNICALL
jvmtiGetClassSignature(jvmtiEnv* env,
	jclass klass,
	char** signature_ptr,
	char** generic_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;
	char * signature = NULL;
	char * generic = NULL;
	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_JVMTI_jvmtiGetClassSignature_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9Class * clazz;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);

		ENSURE_JCLASS_NON_NULL(klass);

		clazz = J9VM_J9CLASS_FROM_JCLASS(currentThread, klass);

		/* Get signature if requested */

		if (signature_ptr != NULL) {
			if (J9ROMCLASS_IS_PRIMITIVE_TYPE(clazz->romClass)) {
				signature = j9mem_allocate_memory(2, J9MEM_CATEGORY_JVMTI_ALLOCATE);
				if (signature == NULL) {
					rc = JVMTI_ERROR_OUT_OF_MEMORY;
					goto done;
				}
				signature[0] = (clazz->arrayClass == NULL) ? 'V' : J9UTF8_DATA(J9ROMCLASS_CLASSNAME(clazz->arrayClass->romClass))[1];
				signature[1] = '\0';
			} else if (J9ROMCLASS_IS_ARRAY(clazz->romClass)) {
				J9ArrayClass * arrayClass = (J9ArrayClass *) clazz;
				J9Class * leafType = arrayClass->leafComponentType;
				UDATA arity = arrayClass->arity;
				UDATA allocSize = arity;
				J9UTF8 * utf;
				UDATA utfLength;

				if (J9ROMCLASS_IS_PRIMITIVE_TYPE(leafType->romClass)) {
					allocSize += 1;	/* +1 or primitive sig char */
				} else {
					utf = J9ROMCLASS_CLASSNAME(leafType->romClass);
					utfLength = J9UTF8_LENGTH(utf);
					allocSize += (utfLength + 2);	/* +2 for L and ; */
				}
				signature = j9mem_allocate_memory(allocSize + 1, J9MEM_CATEGORY_JVMTI_ALLOCATE);
				if (signature == NULL) {
					rc = JVMTI_ERROR_OUT_OF_MEMORY;
					goto done;
				}
				memset(signature, '[', arity);
				if (J9ROMCLASS_IS_PRIMITIVE_TYPE(leafType->romClass)) {
					signature[arrayClass->arity] = J9UTF8_DATA(J9ROMCLASS_CLASSNAME(leafType->arrayClass->romClass))[1];
				} else {
					J9UTF8 * utf = J9ROMCLASS_CLASSNAME(leafType->romClass);
					UDATA utfLength = J9UTF8_LENGTH(utf);

					signature[arity] = 'L';
					memcpy(signature + arity + 1, J9UTF8_DATA(utf), utfLength);
					signature[allocSize - 1] = ';';
				}
				signature[allocSize] = '\0';
			} else {
				J9UTF8 * utf = J9ROMCLASS_CLASSNAME(clazz->romClass);
				UDATA utfLength = J9UTF8_LENGTH(utf);

				signature = j9mem_allocate_memory(utfLength + 3, J9MEM_CATEGORY_JVMTI_ALLOCATE);
				if (signature == NULL) {
					rc = JVMTI_ERROR_OUT_OF_MEMORY;
					goto done;
				}
				signature[0] = 'L';
				memcpy(signature + 1, J9UTF8_DATA(utf), utfLength);
				signature[utfLength + 1] = ';';
				signature[utfLength + 2] = '\0';
			}
		}

		/* Get generic signature if requested */

		if (generic_ptr != NULL) {
			J9UTF8 * genericSignature = getGenericSignatureForROMClass(vm, clazz->classLoader, clazz->romClass);

			if (genericSignature != NULL) {
				rc = cStringFromUTF(env, genericSignature, &generic);
				releaseOptInfoBuffer(vm, clazz->romClass);
				if (rc != JVMTI_ERROR_NONE) {
					goto done;
				}
			}
		}

		if (signature_ptr != NULL) {
			*signature_ptr = signature; 
		}
		if (generic_ptr != NULL) {
			*generic_ptr = generic;
		}

done:
		vm->internalVMFunctions->internalReleaseVMAccess(currentThread);
	}

	if (rc != JVMTI_ERROR_NONE) {
		j9mem_free_memory(signature);
		j9mem_free_memory(generic);
	}
	TRACE_JVMTI_RETURN(jvmtiGetClassSignature);
}


jvmtiError JNICALL
jvmtiGetClassStatus(jvmtiEnv* env,
	jclass klass,
	jint* status_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;

	Trc_JVMTI_jvmtiGetClassStatus_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9Class* clazz;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);

		ENSURE_JCLASS_NON_NULL(klass);
		ENSURE_NON_NULL(status_ptr);

		clazz = J9VM_J9CLASS_FROM_JCLASS(currentThread, klass);
		*status_ptr = getClassStatus(clazz);

done:
		vm->internalVMFunctions->internalReleaseVMAccess(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiGetClassStatus);
}


jvmtiError JNICALL
jvmtiGetSourceFileName(jvmtiEnv* env,
	jclass klass,
	char** source_name_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;

	Trc_JVMTI_jvmtiGetSourceFileName_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9UTF8 *sourceFileName;
		J9Class* clazz;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);
		ENSURE_CAPABILITY(env, can_get_source_file_name);

		ENSURE_JCLASS_NON_NULL(klass);
		ENSURE_NON_NULL(source_name_ptr);

		/* Assume that having the capability means that the debug info server is active */

		clazz = J9VM_J9CLASS_FROM_JCLASS(currentThread, klass);
		rc = JVMTI_ERROR_ABSENT_INFORMATION;
		sourceFileName = getSourceFileNameForROMClass(vm, clazz->classLoader, clazz->romClass);
		if (sourceFileName != NULL) {
			rc = cStringFromUTF(env, sourceFileName, source_name_ptr);
			releaseOptInfoBuffer(vm, clazz->romClass);
		}
done:
		vm->internalVMFunctions->internalReleaseVMAccess(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiGetSourceFileName);
}


jvmtiError JNICALL
jvmtiGetClassModifiers(jvmtiEnv* env,
	jclass klass,
	jint* modifiers_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;

	Trc_JVMTI_jvmtiGetClassModifiers_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9Class * clazz;
		J9ROMClass * romClass;
		U_32 modifiers;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);

		ENSURE_JCLASS_NON_NULL(klass);
		ENSURE_NON_NULL(modifiers_ptr);

		/* Primitive classes have the correct modifiers, so no special processing needs to be done */

		clazz = J9VM_J9CLASS_FROM_JCLASS(currentThread, klass);
		romClass = clazz->romClass;

		/* Array classes have the modifiers of their leaf component type */

		if (J9ROMCLASS_IS_ARRAY(romClass)) {
			clazz = ((J9ArrayClass *) clazz)->leafComponentType;
		}

		/* Inner classes use the memberAccessFlags instead of the modifiers */

		modifiers = J9_ARE_NO_BITS_SET(clazz->romClass->extraModifiers, J9AccClassInnerClass)
		 			? clazz->romClass->modifiers
		 			: clazz->romClass->memberAccessFlags;

		/* Array classes must always be final and not interface */

		if (J9ROMCLASS_IS_ARRAY(romClass)) {
			modifiers = (modifiers | J9AccFinal) & ~J9AccInterface;
		}

		/* Only the low 16 bit of the modifiers are specified - the rest are J9 internal */

		*modifiers_ptr = (jint) (modifiers & 0xFFFF);

done:
		vm->internalVMFunctions->internalReleaseVMAccess(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiGetClassModifiers);
}


jvmtiError JNICALL
jvmtiGetClassMethods(jvmtiEnv* env,
	jclass klass,
	jint* method_count_ptr,
	jmethodID** methods_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;
	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_JVMTI_jvmtiGetClassMethods_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9Class * clazz;
		UDATA methodCount;
		jmethodID * methodIDs;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);

		ENSURE_JCLASS_NON_NULL(klass);
		ENSURE_NON_NULL(method_count_ptr);
		ENSURE_NON_NULL(methods_ptr);

		clazz = J9VM_J9CLASS_FROM_JCLASS(currentThread, klass);
		ENSURE_CLASS_PRIMITIVE_ARRAY_OR_PREPARED(clazz);

		methodCount = clazz->romClass->romMethodCount;
		methodIDs = j9mem_allocate_memory(methodCount * sizeof(jmethodID), J9MEM_CATEGORY_JVMTI_ALLOCATE);
		if (methodIDs == NULL) {
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
		} else {
			J9Method * methods = clazz->ramMethods;
			UDATA i;

			for (i = 0; i < methodCount; ++i) {
				J9Method * method;
				J9JNIMethodID * methodID;

				method = &(methods[i]);
				methodID = vm->internalVMFunctions->getJNIMethodID(currentThread, method);
				if (methodID == NULL) {
					rc = JVMTI_ERROR_OUT_OF_MEMORY;
					j9mem_free_memory(methodIDs);
					goto done;
				}
				methodIDs[i] = (jmethodID) methodID;
			}
			*method_count_ptr = (jint) methodCount;
			*methods_ptr = methodIDs;
		}
done:
		vm->internalVMFunctions->internalReleaseVMAccess(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiGetClassMethods);
}


jvmtiError JNICALL
jvmtiGetClassFields(jvmtiEnv* env,
	jclass klass,
	jint* field_count_ptr,
	jfieldID** fields_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;
	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_JVMTI_jvmtiGetClassFields_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		const J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
		J9Class * clazz;
		J9ROMClass * romClass;
		UDATA fieldCount;
		jfieldID * fieldIDs;

		vmFuncs->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);

		ENSURE_JCLASS_NON_NULL(klass);
		ENSURE_NON_NULL(field_count_ptr);
		ENSURE_NON_NULL(fields_ptr);

		clazz = J9VM_J9CLASS_FROM_JCLASS(currentThread, klass);
		ENSURE_CLASS_PRIMITIVE_ARRAY_OR_PREPARED(clazz);

		romClass = clazz->romClass;
		fieldCount = romClass->romFieldCount;
		fieldIDs = j9mem_allocate_memory(fieldCount * sizeof(jfieldID), J9MEM_CATEGORY_JVMTI_ALLOCATE);
		if (fieldIDs == NULL) {
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
		} else {
			J9ROMFieldOffsetWalkState state;
			J9ROMFieldOffsetWalkResult * result;
			UDATA i;

			i = 0;

			result = vmFuncs->fieldOffsetsStartDo(vm, romClass, GET_SUPERCLASS(clazz), &state,
					J9VM_FIELD_OFFSET_WALK_INCLUDE_STATIC | J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE );
			while (NULL != result->field) {
				UDATA inconsistentData = 0;
				J9JNIFieldID * fieldID = vmFuncs->getJNIFieldID(currentThread, clazz, result->field, result->offset, &inconsistentData);
				/* vmaccess isn't given up so the J9ROMFieldShape and clazz->romClass must be consistent here */
				Assert_JVMTI_true(0 == inconsistentData);
				if (fieldID == NULL) {
					rc = JVMTI_ERROR_OUT_OF_MEMORY;
					j9mem_free_memory(fieldIDs);
					goto done;
				}
				fieldIDs[i++] = (jfieldID) fieldID;
				result = vmFuncs->fieldOffsetsNextDo(&state);
			}

			*field_count_ptr = (jint) fieldCount;
			*fields_ptr = fieldIDs;
		}
done:
		vmFuncs->internalReleaseVMAccess(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiGetClassFields);
}


jvmtiError JNICALL
jvmtiGetImplementedInterfaces(jvmtiEnv* env,
	jclass klass,
	jint* interface_count_ptr,
	jclass** interfaces_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;
	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_JVMTI_jvmtiGetImplementedInterfaces_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9Class * clazz;
		J9ROMClass * romClass;
		jint interfaceCount;
		jclass * interfaces;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);

		ENSURE_JCLASS_NON_NULL(klass);
		ENSURE_NON_NULL(interface_count_ptr);
		ENSURE_NON_NULL(interfaces_ptr);

		clazz = J9VM_J9CLASS_FROM_JCLASS(currentThread, klass);
		ENSURE_CLASS_PRIMITIVE_ARRAY_OR_PREPARED(clazz);

		romClass = clazz->romClass;

		/* Array and primitive classes must return an empty list */

		if (J9ROMCLASS_IS_PRIMITIVE_OR_ARRAY(romClass)) {
			interfaceCount = 0;
			interfaces = NULL;
		} else {

			/* Must walk the interface names in the ROM class to be sure to get the whole list */
 
			interfaceCount = (jint) romClass->interfaceCount;
			interfaces = j9mem_allocate_memory(interfaceCount * sizeof(jclass), J9MEM_CATEGORY_JVMTI_ALLOCATE);
			if (interfaces == NULL) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
			} else {
				J9SRP * interfaceNames = J9ROMCLASS_INTERFACES(romClass);
				jint i;

				for (i = 0; i < interfaceCount; i++) {
					J9Class * interfaceClass;
					J9UTF8 * interfaceName = SRP_PTR_GET(&interfaceNames[i], J9UTF8 *);

					/* The interfaces for this class are guaranteed to already be loaded */

					interfaceClass = vm->internalVMFunctions->internalFindClassUTF8(currentThread, J9UTF8_DATA(interfaceName), J9UTF8_LENGTH(interfaceName),
						clazz->classLoader, J9_FINDCLASS_FLAG_EXISTING_ONLY);
					interfaces[i] = (jclass) vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *) currentThread, J9VM_J9CLASS_TO_HEAPCLASS(interfaceClass));
				}
			}
		}

		*interface_count_ptr = interfaceCount;
		*interfaces_ptr = interfaces;
done:
		vm->internalVMFunctions->internalReleaseVMAccess(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiGetImplementedInterfaces);
}


jvmtiError JNICALL
jvmtiIsInterface(jvmtiEnv* env,
	jclass klass,
	jboolean* is_interface_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;

	Trc_JVMTI_jvmtiIsInterface_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9Class * clazz;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);

		ENSURE_JCLASS_NON_NULL(klass);
		ENSURE_NON_NULL(is_interface_ptr);

		clazz = J9VM_J9CLASS_FROM_JCLASS(currentThread, klass);
		*is_interface_ptr = (clazz->romClass->modifiers & J9AccInterface) ? JNI_TRUE : JNI_FALSE;

done:
		vm->internalVMFunctions->internalReleaseVMAccess(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiIsInterface);
}


jvmtiError JNICALL
jvmtiIsArrayClass(jvmtiEnv* env,
	jclass klass,
	jboolean* is_array_class_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;

	Trc_JVMTI_jvmtiIsArrayClass_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9Class * clazz;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);

		ENSURE_JCLASS_NON_NULL(klass);
		ENSURE_NON_NULL(is_array_class_ptr);

		clazz = J9VM_J9CLASS_FROM_JCLASS(currentThread, klass);
		*is_array_class_ptr = (J9ROMCLASS_IS_ARRAY(clazz->romClass)) ? JNI_TRUE : JNI_FALSE;

done:
		vm->internalVMFunctions->internalReleaseVMAccess(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiIsArrayClass);
}


jvmtiError JNICALL
jvmtiGetClassLoader(jvmtiEnv* env,
	jclass klass,
	jobject* classloader_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;

	Trc_JVMTI_jvmtiGetClassLoader_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9Class * clazz;
		J9ClassLoader * classLoader;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);

		ENSURE_JCLASS_NON_NULL(klass);
		ENSURE_NON_NULL(classloader_ptr);

		clazz = J9VM_J9CLASS_FROM_JCLASS(currentThread, klass);
		classLoader = clazz->classLoader;
		if (classLoader == vm->systemClassLoader) {
			*classloader_ptr = NULL;
		} else {
			*classloader_ptr = vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *)currentThread, J9CLASSLOADER_CLASSLOADEROBJECT(currentThread, classLoader));
		}
done:
		vm->internalVMFunctions->internalReleaseVMAccess(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiGetClassLoader);
}


jvmtiError JNICALL
jvmtiGetSourceDebugExtension(jvmtiEnv* env,
	jclass klass,
	char** source_debug_extension_ptr)
{
	jvmtiError rc;

#if defined(J9VM_OPT_DEBUG_JSR45_SUPPORT)
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;

	Trc_JVMTI_jvmtiGetSourceDebugExtension_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9SourceDebugExtension * sourceDebugExtension;
		J9Class* clazz;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);
		ENSURE_CAPABILITY(env, can_get_source_debug_extension);

		ENSURE_JCLASS_NON_NULL(klass);
		ENSURE_NON_NULL(source_debug_extension_ptr);

		/* Assume that having the capability means that the debug info server is active */

		rc = JVMTI_ERROR_ABSENT_INFORMATION;
		clazz = J9VM_J9CLASS_FROM_JCLASS(currentThread, klass);
		sourceDebugExtension = getSourceDebugExtensionForROMClass(vm, clazz->classLoader, clazz->romClass);
		if (sourceDebugExtension != NULL) {
			U_32 length = sourceDebugExtension->size;

			if (length != 0) {
				rc = cStringFromUTFChars(env, (U_8 *) (sourceDebugExtension + 1), length, source_debug_extension_ptr);
			}
			releaseOptInfoBuffer(vm, clazz->romClass);
		}
done:
		vm->internalVMFunctions->internalReleaseVMAccess(currentThread);
	}
#else
	Trc_JVMTI_jvmtiGetSourceDebugExtension_Entry(env);
	rc = JVMTI_ERROR_MUST_POSSESS_CAPABILITY;
#endif

	TRACE_JVMTI_RETURN(jvmtiGetSourceDebugExtension);
}


jvmtiError JNICALL
jvmtiRedefineClasses(jvmtiEnv* env,
	jint class_count,
	const jvmtiClassDefinition* class_definitions)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(vm);

	Trc_JVMTI_jvmtiRedefineClasses_Entry(env);

	omrthread_monitor_enter(jvmtiData->redefineMutex);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_redefine_classes);

		ENSURE_NON_NEGATIVE(class_count);
		ENSURE_NON_NULL(class_definitions);

		rc = redefineClassesCommon(env, class_count, class_definitions, currentThread, J9_FINDCLASS_FLAG_REDEFINING);
done:
		vm->internalVMFunctions->internalReleaseVMAccess(currentThread);
	}

	omrthread_monitor_exit(jvmtiData->redefineMutex);

	TRACE_JVMTI_RETURN(jvmtiRedefineClasses);
}


static jvmtiError
redefineClassesCommon(jvmtiEnv* env,
					  jint class_count,
					  const jvmtiClassDefinition* class_definitions,
					  J9VMThread * currentThread,
					  UDATA options)
{
	jvmtiError rc;
	J9JavaVM * vm = currentThread->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);
	UDATA extensionsEnabled = 0;
	UDATA extensionsUsed = 0;    /** Set to true when redefine extensions have been used */
	J9JVMTIClassPair * specifiedClasses = NULL;
	J9HashTable * methodPairs = NULL;
	J9HashTable * classPairs = NULL;
	J9HashTable * methodEquivalences = NULL;
#ifdef J9VM_INTERP_NATIVE_SUPPORT
	J9JVMTIHCRJitEventData jitEventData;
#endif
	J9JVMTIHCRJitEventData *jitEventDataPtr = NULL;
	UDATA safePoint = J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_OSR_SAFE_POINT);

#ifdef J9VM_INTERP_NATIVE_SUPPORT	
	/* Ensure that jitEventData is initialized in case we hit failure handling before
	 * it is fully initialized */
	memset(&jitEventData, 0, sizeof(J9JVMTIHCRJitEventData));
#endif

	/* Check whether we should permit j9 specific class redefine extensions */

	extensionsEnabled = areExtensionsEnabled(vm);
	
	/* Verify that all of the classes are allowed to be replaced */

	rc = verifyClassesCanBeReplaced(currentThread, class_count, class_definitions);
	if (rc != JVMTI_ERROR_NONE) {
		return rc;
	}

	/* Allocate a buffer to hold the new versions of the classes */

	specifiedClasses = j9mem_allocate_memory(class_count * sizeof(J9JVMTIClassPair), J9MEM_CATEGORY_JVMTI);
	if (specifiedClasses == NULL) {
		return JVMTI_ERROR_OUT_OF_MEMORY;
	}
	memset(specifiedClasses, 0, class_count * sizeof(J9JVMTIClassPair));
	
	/* Create ROM classes for each of the replaced classes */

	rc = reloadROMClasses(currentThread, class_count, class_definitions, specifiedClasses, options);
	if (rc != JVMTI_ERROR_NONE) {
		goto failed;
	}

	/* Make sure the replaced classes are compatible with the old versions */

	rc = verifyClassesAreCompatible(currentThread, class_count, specifiedClasses, extensionsEnabled, &extensionsUsed);
	if (rc != JVMTI_ERROR_NONE) {
		goto failed;
	}

	if (NULL != vm->bytecodeVerificationData) {
		rc = verifyNewClasses(currentThread, class_count, specifiedClasses);
	}

	if (rc != JVMTI_ERROR_NONE) {
		goto failed;
	}

#ifdef J9VM_INTERP_NATIVE_SUPPORT
	if (!extensionsUsed && vm->jitConfig) {
		jitEventDataPtr = &jitEventData;
	}
#endif

	if (safePoint) {
		vm->internalVMFunctions->acquireSafePointVMAccess(currentThread);	
	} else {
		vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);
	}

	/* Determine all ROM classes which need a new RAM class, and pair them with their current RAM class */

	rc = determineClassesToRecreate(currentThread, class_count, specifiedClasses, &classPairs,
			&methodPairs, jitEventDataPtr, !extensionsEnabled);
	if (rc == JVMTI_ERROR_NONE) {
		/* Recreate the RAM classes for all classes */

		rc = recreateRAMClasses(currentThread, classPairs, methodPairs, extensionsUsed, !extensionsEnabled);
		if (rc == JVMTI_ERROR_NONE) {

			if (!extensionsEnabled) {
				/* Fast HCR path - where the J9Class is redefined in place. */

				/* Add method equivalences for the methods that were re-defined (reverse of before!). */
				rc = fixMethodEquivalences(currentThread, classPairs, jitEventDataPtr, TRUE, &methodEquivalences, extensionsUsed);
				if (rc != JVMTI_ERROR_NONE) {
					goto failed;
				}
				/* Fix the vTables of all subclasses */
				fixVTables_forNormalRedefine(currentThread, classPairs, methodPairs, TRUE, &methodEquivalences);

				/* Update method references in DirectHandles */
				fixDirectHandles(currentThread, classPairs, methodPairs);

				/* Fix JNI */
				fixJNIRefs(currentThread, classPairs, TRUE, extensionsUsed);

				/* Update the iTables of any classes which implement a replaced interface */
				fixITablesForFastHCR(currentThread, classPairs);

				/* Fix resolved constant pool references to point to new methods. */
				fixConstantPoolsForFastHCR(currentThread, classPairs, methodPairs);

#ifdef J9VM_OPT_SIDECAR
				/* Fix return bytecodes in unsafe classes */
				fixReturnsInUnsafeMethods(currentThread, classPairs);
#endif

				/* Flush the reflect method cache */
				flushClassLoaderReflectCache(currentThread, classPairs);

				/* Indicate that a redefine has occurred */
				vm->hotSwapCount += 1;

				/* Notify the JIT about redefined classes */
				jitClassRedefineEvent(currentThread, &jitEventData, FALSE);

			} else {

				/* Clear/suspend all breakpoints in the classes being replaced */
				clearBreakpointsInClasses(currentThread, classPairs);

				/* Fix static refs */
				fixStaticRefs(currentThread, classPairs, extensionsUsed);
 
				/* Copy preserved values */
				copyPreservedValues(currentThread, classPairs, extensionsUsed);

				/* Update heap references */
				fixHeapRefs(vm, classPairs);

				/* Update method references in DirectHandles */
				fixDirectHandles(currentThread, classPairs, methodPairs);
 
				/* Update the componentType and leafComponentType fields of array classes */
				fixArrayClasses(currentThread, classPairs);

				/* Fix JNI */
				fixJNIRefs(currentThread, classPairs, FALSE, extensionsUsed);

				/* Update the iTables of any classes which implement a replaced interface */
				fixITables(currentThread, classPairs);

				/* Fix subclass hierarchy */
				fixSubclassHierarchy(currentThread, classPairs);
 
				/* Unresolve all classes */
				unresolveAllClasses(currentThread, classPairs, methodPairs, extensionsUsed);
 
				/* Update method equivalences */
				rc = fixMethodEquivalences(currentThread, classPairs, jitEventDataPtr, FALSE, &methodEquivalences, extensionsUsed);
				if (rc != JVMTI_ERROR_NONE) {
					goto failed;
				}
				/* Fix the vTables of all subclasses */
				if (!extensionsUsed) {
					fixVTables_forNormalRedefine(currentThread, classPairs, methodPairs, FALSE, &methodEquivalences);
				}

#ifdef J9VM_OPT_SIDECAR
				/* Fix return bytecodes in unsafe classes */
				fixReturnsInUnsafeMethods(currentThread, classPairs);
#endif

				/* Restore breakpoints in the implicitly replaced classes */
				restoreBreakpointsInClasses(currentThread, classPairs);

				/* Flush the reflect method cache */
				flushClassLoaderReflectCache(currentThread, classPairs);

				/* Indicate that a redefine has occurred */
				vm->hotSwapCount += 1;

#ifdef J9VM_INTERP_NATIVE_SUPPORT
				/* Notify the JIT about redefined classes */
				jitClassRedefineEvent(currentThread, &jitEventData, extensionsEnabled);
#endif
			}
		}
		notifyGCOfClassReplacement(currentThread, classPairs, !extensionsEnabled);
		hashTableFree(classPairs);
	}

	J9JVMTI_DATA_FROM_VM(vm)->flags = J9JVMTI_DATA_FROM_VM(vm)->flags & ~J9JVMTI_FLAG_REDEFINE_CLASS_EXTENSIONS_USED;

	if (rc == JVMTI_ERROR_NONE) {
		TRIGGER_J9HOOK_VM_CLASSES_REDEFINED(vm->hookInterface, currentThread);
	}

	if (safePoint) {
		vm->internalVMFunctions->releaseSafePointVMAccess(currentThread);
	} else {
		vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);
	}

failed:

	if (specifiedClasses) {
		int i;
		J9JVMTIClassPair * classPair = specifiedClasses;
		for (i = 0; i < class_count; i++) {
			if (classPair->methodRemap) {
				j9mem_free_memory(classPair->methodRemap);
			}
 			if (classPair->methodRemapIndices) {
				j9mem_free_memory(classPair->methodRemapIndices);
			} 
			classPair++;
		}
		j9mem_free_memory(specifiedClasses);
	}
	
	hashTableFree(methodPairs);

	hashTableFree(methodEquivalences);

#ifdef J9VM_INTERP_NATIVE_SUPPORT
	if (jitEventData.initialized) {
		jitEventFree(vm, &jitEventData);
	}
#endif

	return rc;
}



static void
clearBreakpointsInClasses(J9VMThread * currentThread, J9HashTable* classHashTable)
{
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(currentThread->javaVM);
	J9HashTableState hashTableState;
	J9JVMTIClassPair * classPair;

	classPair = hashTableStartDo(classHashTable, &hashTableState);

	while (classPair != NULL) {
		J9Class * originalRAMClass = classPair->originalRAMClass;
		J9Class * replacementRAMClass = classPair->replacementClass.ramClass;
		J9JVMTIAgentBreakpointDoState state;
		J9JVMTIAgentBreakpoint * agentBreakpoint;
		UDATA explicitlyRecreated;

		if (replacementRAMClass == NULL) {
			classPair = hashTableNextDo(&hashTableState);
			continue;
		}
		
		explicitlyRecreated = (originalRAMClass->romClass != replacementRAMClass->romClass);
		
		agentBreakpoint = allAgentBreakpointsStartDo(jvmtiData, &state);
		while (agentBreakpoint != NULL) {
			if (J9_CLASS_FROM_METHOD(((J9JNIMethodID *) agentBreakpoint->method)->method) == originalRAMClass) {
				if (explicitlyRecreated) {
					deleteAgentBreakpoint(currentThread, state.j9env, agentBreakpoint);
				} else {
					suspendAgentBreakpoint(currentThread, agentBreakpoint);
				}
			}
			agentBreakpoint = allAgentBreakpointsNextDo(&state);
		}
		classPair = hashTableNextDo(&hashTableState);
	}
}

static UDATA
countInitiatedClass(J9Class * clazz, J9JVMTIClassStats * results)
{
	/* Count classes (ignore primitive types and old versions of redefined classes) */
	if (((J9CLASS_FLAGS(clazz) & J9AccClassHotSwappedOut) == 0) &&
		((J9ROMCLASS_IS_PRIMITIVE_TYPE(clazz->romClass)) == 0)) {

		results->classCount++;
	}

	return 0;
}


static UDATA
copyInitiatedClass(J9Class * clazz, J9JVMTIClassStats * results)
{
	/* Copy classes (ignore primitive types and old versions of redefined classes) */
	if (((J9CLASS_FLAGS(clazz) & J9AccClassHotSwappedOut) == 0) &&
		(J9ROMCLASS_IS_PRIMITIVE_TYPE(clazz->romClass) == 0)) {

		J9JavaVM * vm = results->vm;
		JNIEnv * jniEnv = (JNIEnv *)results->currentThread;
		jint slot = (jint)results->classCount - 1;

		/* Reverse fill */
		if (slot >= 0) {
			j9object_t classObject = J9VM_J9CLASS_TO_HEAPCLASS(clazz);
			
			results->classRefs[slot] = (jclass)vm->internalVMFunctions->j9jni_createLocalRef(jniEnv, classObject);
			results->classCount = slot;
		}
	}

	return 0;
}


static void
restoreBreakpointsInClasses(J9VMThread * currentThread, J9HashTable * classPairs)
{
	J9HashTableState hashTableState;
	J9JVMTIClassPair * classPair;
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(currentThread->javaVM);
	
	classPair = hashTableStartDo(classPairs, &hashTableState);
	while (classPair != NULL) {
		J9Class * originalRAMClass = classPair->originalRAMClass;
		J9Class * replacementRAMClass = classPair->replacementClass.ramClass;

		/* Breakpoints in the explicitly recreated classes are gone - only restore those in implicitly recreated classes */
		if (replacementRAMClass && (originalRAMClass->romClass == replacementRAMClass->romClass)) {
			J9JVMTIAgentBreakpointDoState state;
			J9JVMTIAgentBreakpoint * agentBreakpoint;

			agentBreakpoint = allAgentBreakpointsStartDo(jvmtiData, &state);
			while (agentBreakpoint != NULL) {
				/* JNI refs have been updated to point to the replacement classes by now */

				if (J9_CLASS_FROM_METHOD(((J9JNIMethodID *) agentBreakpoint->method)->method) == replacementRAMClass) {
					installAgentBreakpoint(currentThread, agentBreakpoint);
				}
				agentBreakpoint = allAgentBreakpointsNextDo(&state);
			}
		}
		classPair = hashTableNextDo(&hashTableState);
	}
}


static jint
getClassStatus(J9Class * clazz)
{
	jint status = JVMTI_CLASS_STATUS_ERROR;

	if (J9ROMCLASS_IS_PRIMITIVE_TYPE(clazz->romClass)) {
		status = JVMTI_CLASS_STATUS_PRIMITIVE;
	} else if (J9ROMCLASS_IS_ARRAY(clazz->romClass)) {
		status = JVMTI_CLASS_STATUS_ARRAY;
	} else {
		switch (clazz->initializeStatus & J9ClassInitStatusMask) {
			case J9ClassInitUnverified:
				status = 0;
				break;
			case J9ClassInitUnprepared:
				status = JVMTI_CLASS_STATUS_VERIFIED;
				break;
			case J9ClassInitNotInitialized:
				status = JVMTI_CLASS_STATUS_VERIFIED | JVMTI_CLASS_STATUS_PREPARED;
				break;
			case J9ClassInitSucceeded:
				status = JVMTI_CLASS_STATUS_INITIALIZED | JVMTI_CLASS_STATUS_VERIFIED | JVMTI_CLASS_STATUS_PREPARED;
				break;
			case J9ClassInitFailed:
				status = JVMTI_CLASS_STATUS_ERROR | JVMTI_CLASS_STATUS_VERIFIED | JVMTI_CLASS_STATUS_PREPARED;
				break;
		}
	}

	return status;
}


jvmtiError JNICALL
jvmtiRetransformClasses(jvmtiEnv* env,
	jint class_count,
	const jclass* classes)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(vm);

	Trc_JVMTI_jvmtiRetransformClasses_Entry(env);

	omrthread_monitor_enter(jvmtiData->redefineMutex);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		jvmtiClassDefinition * class_definitions;
		PORT_ACCESS_FROM_JAVAVM(vm);

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_retransform_classes);

		ENSURE_NON_NEGATIVE(class_count);
		ENSURE_NON_NULL(classes);

		/* Create a jvmtiClassDefinition for each retransformed class from the stored bytes and call the common redefine helper */

		class_definitions = j9mem_allocate_memory(class_count * sizeof(jvmtiClassDefinition), J9MEM_CATEGORY_JVMTI);
		if (class_definitions == NULL) {
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
		} else {
			J9MemorySegmentList * classSegments = vm->classMemorySegments;
			jint i;

			memset(class_definitions, 0, class_count * sizeof(jvmtiClassDefinition));

			omrthread_monitor_enter(classSegments->segmentMutex);
			for (i = 0; i < class_count; ++i) {
				jclass klass;
				J9Class * clazz;
				U_8 * classFileBytes = NULL;
				U_32 classFileBytesCount = 0;

				klass = classes[i];
				if (klass == NULL) {
					rc = JVMTI_ERROR_INVALID_CLASS;
					break;
				}

				clazz = J9VM_J9CLASS_FROM_JCLASS(currentThread, klass);
				if (!classIsModifiable(vm, clazz)) {
					rc = JVMTI_ERROR_UNMODIFIABLE_CLASS;
					break;
				}
				if (WSRP_GET(clazz->romClass->intermediateClassData, U_8*) == NULL) {
					rc = JVMTI_ERROR_UNMODIFIABLE_CLASS;
					break;
				}

				if (!J9ROMCLASS_IS_INTERMEDIATE_DATA_A_CLASSFILE(clazz->romClass)) {
					J9ROMClass * intermediateROMClass = (J9ROMClass *) J9ROMCLASS_INTERMEDIATECLASSDATA(clazz->romClass);
					IDATA result = 0;

					/* -XX:-StoreIntermediateClassfile (enabled by default) : recreate class file bytes from intermediateROMClass pointed by current ROMClass */
					result = vm->dynamicLoadBuffers->transformROMClassFunction(vm, PORTLIB, intermediateROMClass, &classFileBytes, &classFileBytesCount);
					if (BCT_ERR_NO_ERROR != result) {
						Trc_JVMTI_jvmtiRetransformClasses_ErrorInRecreatingClassfile(env, intermediateROMClass, result);
						rc = JVMTI_ERROR_INTERNAL;
						if (BCT_ERR_OUT_OF_MEMORY == result) {
							rc = JVMTI_ERROR_OUT_OF_MEMORY;
						}
						break;
					}
				} else {
					classFileBytesCount = (jint) clazz->romClass->intermediateClassDataLength;
					classFileBytes = WSRP_GET(clazz->romClass->intermediateClassData, U_8*);
				}
				class_definitions[i].klass = klass;
				class_definitions[i].class_byte_count = (jint) classFileBytesCount;
				class_definitions[i].class_bytes = classFileBytes;
			}
			omrthread_monitor_exit(classSegments->segmentMutex);

			if (rc == JVMTI_ERROR_NONE) {
				rc = redefineClassesCommon(env, class_count, class_definitions, currentThread, J9_FINDCLASS_FLAG_RETRANSFORMING);
			}

			/* free classFileBytes returned by j9bcutil_transformROMClass */
			for (i = 0; i < class_count; ++i) {
				if (NULL != class_definitions[i].class_bytes) {
					J9Class * clazz = J9VM_J9CLASS_FROM_JCLASS(currentThread, classes[i]);

					if (!J9ROMCLASS_IS_INTERMEDIATE_DATA_A_CLASSFILE(clazz->romClass)) {
						j9mem_free_memory((void *)class_definitions[i].class_bytes);
					}
				}
			}
			j9mem_free_memory(class_definitions);
		}
done:
		vm->internalVMFunctions->internalReleaseVMAccess(currentThread);
	}

	omrthread_monitor_exit(jvmtiData->redefineMutex);

	TRACE_JVMTI_RETURN(jvmtiRetransformClasses);
}


jvmtiError JNICALL
jvmtiIsModifiableClass(jvmtiEnv* env,
	jclass klass,
	jboolean* is_modifiable_class_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;

	Trc_JVMTI_jvmtiIsModifiableClass_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9Class * clazz;
		jboolean modifiable = JNI_FALSE;
		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);

		ENSURE_JCLASS_NON_NULL(klass);
		ENSURE_JCLASS(currentThread, klass);
		ENSURE_NON_NULL(is_modifiable_class_ptr);

		clazz = J9VM_J9CLASS_FROM_JCLASS(currentThread, klass);
		if (NULL == clazz) {
			JVMTI_ERROR(JVMTI_ERROR_INVALID_CLASS);
		}

		modifiable = classIsModifiable(vm, clazz);
		if (modifiable) {
			if (((J9JVMTIEnv *) env)->capabilities.can_retransform_classes) {
				J9MemorySegmentList * classSegments = vm->classMemorySegments;

				omrthread_monitor_enter(classSegments->segmentMutex);
				modifiable = (WSRP_GET(clazz->romClass->intermediateClassData, U_8*) != NULL);
				omrthread_monitor_exit(classSegments->segmentMutex);
			}
		}
		*is_modifiable_class_ptr = modifiable;
done:
		vm->internalVMFunctions->internalReleaseVMAccess(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiIsModifiableClass);
}


jvmtiError JNICALL
jvmtiGetClassVersionNumbers(jvmtiEnv* env,
	jclass klass,
	jint* minor_version_ptr,
	jint* major_version_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;

	Trc_JVMTI_jvmtiGetClassVersionNumbers_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9Class* clazz;
		J9ROMClass * romClass;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);

		ENSURE_JCLASS_NON_NULL(klass);
		ENSURE_NON_NULL(minor_version_ptr);
		ENSURE_NON_NULL(major_version_ptr);
		ENSURE_JCLASS(currentThread, klass);

		clazz = J9VM_J9CLASS_FROM_JCLASS(currentThread, klass);
		romClass = clazz->romClass;
		if (J9ROMCLASS_IS_PRIMITIVE_OR_ARRAY(romClass)) {
			rc = JVMTI_ERROR_ABSENT_INFORMATION;
		} else {
			*minor_version_ptr = (jint) romClass->minorVersion;
			*major_version_ptr = (jint) romClass->majorVersion;
		}

done:
		vm->internalVMFunctions->internalReleaseVMAccess(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiGetClassVersionNumbers);
}


#define GCP_WRITE_U8(dst, src) \
		do { \
			*(dst) = src; \
			dst++; \
		} while (0)

#ifdef J9VM_ENV_LITTLE_ENDIAN
#define GCP_WRITE_U16(dst, src) \
		do { \
			dst[0] = ((U_8 *) &src)[1]; \
			dst[1] = ((U_8 *) &src)[0]; \
			dst += 2; \
		} while (0)
#else
#define GCP_WRITE_U16(dst, src) \
		do { \
			*((U_16 *) dst) = (src); \
			dst += 2; \
		} while (0)
#endif
		

#ifdef J9VM_ENV_LITTLE_ENDIAN
#define GCP_WRITE_U32(dst, src) \
		do { \
			dst[0] = ((U_8 *) &src)[3]; \
			dst[1] = ((U_8 *) &src)[2]; \
			dst[2] = ((U_8 *) &src)[1]; \
			dst[3] = ((U_8 *) &src)[0]; \
			dst += 4; \
		} while (0)
#else
#define GCP_WRITE_U32(dst, src) \
	do { \
		*((U_32 *) dst) = (src); \
		dst += 4; \
	} while (0)
#endif
 

#ifdef JVMTI_GCP_DEBUG
#define jvmtiGetConstantPoolTranslate_printf(args) printf args
#define jvmtiGetConstantPoolWrite_printf(args) printf args
#else
#define jvmtiGetConstantPoolTranslate_printf(args)
#define jvmtiGetConstantPoolWrite_printf(args)
#endif


/** 
 * \brief	Returnt the raw Constant Pool bytes for the specified class
 * \ingroup	jvmtiClass 
 * 
 * 
 * @param env  				JVMTI environment 
 * @param klass 			The class to query
 * @param constant_pool_count_ptr 	On return, points to the number of entries in the constant pool table plus one.
 * @param constant_pool_byte_count_ptr	On return, points to the number of bytes in the returned raw constant pool.
 * @param constant_pool_bytes_ptr 	On return, points to the raw constant pool, that is the bytes defined by the constant_pool 
 * 					item of the Class File Format. Agent passes a pointer to a unsigned char*. On return, the 
 * 					unsigned char* points to a newly allocated array of size *constant_pool_byte_count_ptr. 
 * 					The array should be freed with Deallocate.
 * @return 				a jvmtiError code
 *      
 *      The call GetConstantPool call is described in detail by the JVMTI1.1 spec: 
 *      http://www.icogitate.com/~jsr163/eg/jvmti.html#GetConstantPool
 * 
 */
jvmtiError JNICALL
jvmtiGetConstantPool(jvmtiEnv* env,
		     jclass klass,
		     jint* constant_pool_count_ptr,
		     jint* constant_pool_byte_count_ptr,
		     unsigned char** constant_pool_bytes_ptr)
{
	jvmtiError rc;
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	jvmtiGcp_translation translation;
	unsigned char *constantPoolBuf;
	
	Trc_JVMTI_jvmtiGetConstantPool_Entry(env);

    memset(&translation, 0x00, sizeof(jvmtiGcp_translation));
	
	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9Class* clazz;
		PORT_ACCESS_FROM_JAVAVM(vm);
		
		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);
		ENSURE_CAPABILITY(env, can_get_constant_pool);

		ENSURE_JCLASS_NON_NULL(klass);
		ENSURE_JCLASS(currentThread, klass);
		ENSURE_NON_NULL(constant_pool_count_ptr);
		ENSURE_NON_NULL(constant_pool_byte_count_ptr);
		ENSURE_NON_NULL(constant_pool_bytes_ptr);

		clazz = J9VM_J9CLASS_FROM_JCLASS(currentThread, klass);
		
		/* Return an error for Array or Primitive classes */
		if (J9ROMCLASS_IS_PRIMITIVE_OR_ARRAY(clazz->romClass)) {
			rc = JVMTI_ERROR_ABSENT_INFORMATION;
			JVMTI_ERROR(rc);
		}

		/* Figure out how much space we'll need on the return buffer and store translation 
		 * entries for all CP types. The translation is performed to make sure the CP indices 
		 * match those used by the bytecodes returned by GetBytecodes jvmti call. 
		 */
		rc = jvmtiGetConstantPool_translateCP(PORTLIB, &translation, clazz, TRUE);
		if (rc != JVMTI_ERROR_NONE) {    
			JVMTI_ERROR(rc);
		}

		/* Allocate the return buffer */
		constantPoolBuf = j9mem_allocate_memory(translation.totalSize, J9MEM_CATEGORY_JVMTI_ALLOCATE);
		if (constantPoolBuf == NULL) {
			JVMTI_ERROR(JVMTI_ERROR_OUT_OF_MEMORY);
		}
		
		/* Write out all constants */
		rc = jvmtiGetConstantPool_writeConstants(&translation, constantPoolBuf);
		if (rc != JVMTI_ERROR_NONE) {
			j9mem_free_memory(constantPoolBuf);
			JVMTI_ERROR(rc);
		}

		*constant_pool_count_ptr = translation.cpSize;
		*constant_pool_byte_count_ptr = translation.totalSize;
		*constant_pool_bytes_ptr = constantPoolBuf;
		
done:
		vm->internalVMFunctions->internalReleaseVMAccess(currentThread);

		jvmtiGetConstantPool_free(PORTLIB, &translation);
	}

	TRACE_JVMTI_RETURN(jvmtiGetConstantPool);
}




/** 
 * \brief	Translate constant pool indices 
 * \ingroup 	jvmtiClass
 *
 * @param privatePortLibrary	port library  
 * @param translation		a data structure holding the translation HashTable and Array. 
 * 				This call will allocate a HashTable and an array that must be
 * 				freed via the jvmtiGetConstantPool_free() call once done. 
 * @param class			class to be used as the constant pool source
 * @param translateUTF8andNAS	the translated constant pool should include UTF8 and Name and Type 
 * 				constants. A distinction is made since when used by GetBytecodes
 * 				we do not care about those two constant types. 
 * @return 			a jvmtiError code
 * 
 *
 *	This call is used by the GetConstantPool and GetBytecodes jvmti API calls. It serves as
 *	means of obtaining identical constant pool indices for the said two calls. 
 *
 *	To achieve this it provides a HashTable of unique constant pool entries. The HashTable is 
 *	keyed by the j9vm constant pool index for all items except UTF8 and NameAndSignature. The two 
 *	prior items are not directly present on J9ROMClass CP and are instead keyed by their SRPs.
 *	The HashTable is used by the GetBytecodes call. It is also utilized while reindexing the 
 *	constant pool.
 *	
 *	The translation structure also provides an array of HashTable nodes. The array is indexed
 *	by the 'reindexed' constant pool indices. It provides efficient means of writing out the
 *	constant pool by the jvmtiGetConstantPool_writeConstants function. 
 * 
 *	ISSUES:
 *		The UTF8 and NameAndSignature constants are not stored on the constant pool and
 *		therefore do not have an "index" but rather use SRP references. This call will 
 *		create CP entries and update refering CP items accordingly
 *
 *		The Long and Double type is defined by the spec to take _TWO_ constant pool entries
 *		instead of one. This creates a problem since our bytecode's cp indices have been
 *		rewritten to get rid of the extra CP index. The returned constant pool is written
 *		out containing that extra index. This in turn forced us to change the GetBytecodes
 *		jvmti call to reindex all bytecodes using the CP to account for that such that the
 *		bytecodes returned by it match the constant pool returned here.
 *
 *		The first 255 CP items must not be reindexed to an index greater then 255. Breaking
 *		that rule will cause LDC bytecode breakage as it uses a 1 byte constant pool index
 *		that cannot be fixed if the referenced CP item is out of range. To address this issue
 *		we first write out the J9RomClass constant pool and then append UTF8 and NameAndType 
 *		items. This ensures that whatever items were indexed below 255 still remain so. 
 * 
 *	NOTE: 
 *		Caller is responsible for freeing the hashtable and cp array tracked by the translation 
 *		structure by calling the jvmtiGetConstantPool_free API call. 
 *
 */
jvmtiError
jvmtiGetConstantPool_translateCP(J9PortLibrary *privatePortLibrary, jvmtiGcp_translation *translation, 
				 J9Class *class, jboolean translateUTF8andNAS)
{
	U_32 sunCpIndex = 1;
	UDATA cpIndex, i;
	U_8  cpItemType;
	J9ROMConstantPoolItem *cpItem;
	jvmtiError rc = JVMTI_ERROR_NONE;
	jvmtiGcp_translationEntry *htEntry;

	J9ROMClass *romClass;
	J9ROMConstantPoolItem *constantPool;
	U_32 constantPoolCount;
	U_32 * cpShapeDescription;


	romClass = class->romClass;
	constantPool = (J9ROMConstantPoolItem*) ((U_8*) romClass + sizeof(J9ROMClass));
	constantPoolCount = romClass->romConstantPoolCount;
	cpShapeDescription = J9ROMCLASS_CPSHAPEDESCRIPTION(romClass);

	/* Clear and initialize the translation struct */
	memset(translation, 0x00, sizeof(struct jvmtiGcp_translation));
	translation->romConstantPool = constantPool;

	/* Allocate the Hash Table used to store CP entries. */
	translation->ht = hashTableNew(OMRPORT_FROM_J9PORT(privatePortLibrary), J9_GET_CALLSITE(), 256, sizeof(jvmtiGcp_translationEntry), 0, 0, J9MEM_CATEGORY_JVMTI,
				       jvmtiGetConstantPool_HashFn, jvmtiGetConstantPool_HashEqualFn, NULL, NULL);
	if (NULL == translation->ht) {
		JVMTI_ERROR(JVMTI_ERROR_OUT_OF_MEMORY);
	}

	/* Allocate an array of hash node ptrs. The size is an upper bound on the number of CP items given the
	 * constantPoolCount. ie the max number of items we can have is if they were all Reference types. 
	 * We allocate more in order to prevent a second pass after the real total number of CP items is calculated and
	 * the HashTable populated. Instead we initialize the lookup array at the same time as creating the HT. */
	/* The "6" is calculated by taking a worst case of lets say a FieldRef and counting the max number of 
	 * unique items used to completely describe it */
	translation->cp = j9mem_allocate_memory((constantPoolCount * 6) * sizeof(jvmtiGcp_translationEntry *), J9MEM_CATEGORY_JVMTI);
	if (NULL == translation->cp) {
		JVMTI_ERROR(JVMTI_ERROR_OUT_OF_MEMORY);
	}

		
	jvmtiGetConstantPoolTranslate_printf(("JVMTI GetConstantPool: Creating Translation Table\n"));

	for (cpIndex = 1; cpIndex < constantPoolCount; cpIndex++) {
		cpItem = &constantPool[cpIndex];

		cpItemType = J9_CP_TYPE(cpShapeDescription, cpIndex);

		jvmtiGetConstantPoolTranslate_printf(("S %2d/%d vmCPType: %d\n", cpIndex, constantPoolCount, cpItemType));

		switch(cpItemType) {
			case J9CPTYPE_CLASS:
			case J9CPTYPE_STRING:
			case J9CPTYPE_ANNOTATION_UTF8:

				rc = jvmtiGetConstantPool_addClassOrString(translation, cpIndex,
									   (U_8) ((cpItemType == J9CPTYPE_CLASS) ? CFR_CONSTANT_Class : CFR_CONSTANT_String),
									   J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) cpItem),
									   &sunCpIndex, NULL);
				if (rc != JVMTI_ERROR_NONE) {
					JVMTI_ERROR(rc);
				}
				break;

			case J9CPTYPE_FIELD:

				rc = jvmtiGetConstantPool_addReference(translation, cpIndex, (U_8)CFR_CONSTANT_Fieldref,
								       (J9ROMFieldRef *) cpItem,
								       &sunCpIndex);
				if (rc != JVMTI_ERROR_NONE) {
					JVMTI_ERROR(rc);
				}
				break;

			case J9CPTYPE_HANDLE_METHOD:
			case J9CPTYPE_INSTANCE_METHOD:
			case J9CPTYPE_STATIC_METHOD:
			case J9CPTYPE_INTERFACE_METHOD:

				rc = jvmtiGetConstantPool_addReference(translation, cpIndex,
								       (U_8) ((cpItemType == J9CPTYPE_INTERFACE_METHOD) ? CFR_CONSTANT_InterfaceMethodref : CFR_CONSTANT_Methodref),
								       (J9ROMFieldRef *) cpItem,
								       &sunCpIndex);
				if (rc != JVMTI_ERROR_NONE) {
					JVMTI_ERROR(rc);
				}
				break;

			case J9CPTYPE_METHOD_TYPE:
				rc = jvmtiGetConstantPool_addMethodType(translation, cpIndex,
									   (U_8) CFR_CONSTANT_MethodType,
									   (J9ROMMethodTypeRef *) cpItem,
									   &sunCpIndex);

				if (rc != JVMTI_ERROR_NONE) {
					JVMTI_ERROR(rc);
				}
				break;


			case J9CPTYPE_METHODHANDLE:

				rc = jvmtiGetConstantPool_addMethodHandle(translation, cpIndex,
								       (U_8) CFR_CONSTANT_MethodHandle,
								       (J9ROMMethodHandleRef *) cpItem,
								       &sunCpIndex);
				if (rc != JVMTI_ERROR_NONE) {
					JVMTI_ERROR(rc);
				}

				break;

			case J9CPTYPE_INT:
			case J9CPTYPE_FLOAT:

				rc = jvmtiGetConstantPool_addIntFloat(translation, cpIndex,
								      (U_8) ((cpItemType == J9CPTYPE_INT) ? CFR_CONSTANT_Integer : CFR_CONSTANT_Float),
								      ((J9ROMSingleSlotConstantRef *) cpItem)->data,
								      &sunCpIndex);
				if (rc != JVMTI_ERROR_NONE) {
					JVMTI_ERROR(rc);
				}
				break;

			case J9CPTYPE_LONG:
			case J9CPTYPE_DOUBLE:
				rc = jvmtiGetConstantPool_addLongDouble(translation, cpIndex,
									(U_8) ((cpItemType == J9CPTYPE_DOUBLE) ? CFR_CONSTANT_Double : CFR_CONSTANT_Long),
									((J9ROMConstantRef *) cpItem)->slot1,
									((J9ROMConstantRef *) cpItem)->slot2,
									&sunCpIndex);
				if (rc != JVMTI_ERROR_NONE) {
					JVMTI_ERROR(rc);
				}
				break;

			default:        
				jvmtiGetConstantPoolTranslate_printf(("	Unknown cpType %2d\n", cpItemType));
				JVMTI_ERROR(JVMTI_ERROR_INTERNAL);
		}
	}

	/* Check if we have a CallSiteTable and map the values back to CONSTANT_InvokeDynamics */
	if (romClass->callSiteCount > 0) {
		J9SRP *callSiteData = (J9SRP *) J9ROMCLASS_CALLSITEDATA(romClass);
		U_16 *bsmIndices = (U_16 *) (callSiteData + romClass->callSiteCount);
		UDATA callSiteIndex;

		for (callSiteIndex = 0; callSiteIndex < romClass->callSiteCount; callSiteIndex++) {
			J9ROMNameAndSignature* nameAndSig = SRP_PTR_GET(callSiteData + callSiteIndex, J9ROMNameAndSignature*);
			U_16 bsmIndex = bsmIndices[callSiteIndex];

			rc = jvmtiGetConstantPool_addInvokeDynamic(translation,
									(UDATA) callSiteData + callSiteIndex, /* Use the SRP as the hashtable key */
									nameAndSig,
									bsmIndex,
									&sunCpIndex);
			if (rc != JVMTI_ERROR_NONE) {
				JVMTI_ERROR(rc);
			}
		}
	}

	/* Add UTF8 or NameAndType items at the end and fix late bindings */

	if (translateUTF8andNAS) {

		translation->cpSize = sunCpIndex;

		for (i = 1; i < translation->cpSize; i++) {
			J9UTF8 *utf8;
			J9ROMNameAndSignature *nas;
			J9ROMFieldRef *ref;

			htEntry = translation->cp[i];

			/* Skip the NULL pads that follow Double/Long entries */
			if (NULL == htEntry)
				continue;

			cpItemType = htEntry->cpType;

			switch (htEntry->cpType) {
				case CFR_CONSTANT_Class:
				case CFR_CONSTANT_String:

					/* Add and Bind the UTF8 used by this Class or String item */
					rc = jvmtiGetConstantPool_addUTF8(translation, htEntry->type.clazz.utf8, &sunCpIndex, &htEntry->type.clazz.nameIndex);
					if (rc != JVMTI_ERROR_NONE) {
						JVMTI_ERROR(rc);
					}

					break;

				case CFR_CONSTANT_Fieldref:
				case CFR_CONSTANT_Methodref:
				case CFR_CONSTANT_InterfaceMethodref: 

					/* Fix the Reference by adding the UTF8 and NameAndSignature items to the hashtable and binding
					 * the indices to this ref */

					ref = htEntry->type.ref.ref;
					nas = J9ROMFIELDREF_NAMEANDSIGNATURE(ref);
					utf8 = J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) &translation->romConstantPool[ref->classRefCPIndex]);
					jvmtiGetConstantPoolTranslate_printf(("	Ref : Class [%*s] NAS [%*s][%*s]\n", J9UTF8_LENGTH(utf8), J9UTF8_DATA(utf8),
									     J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_NAME(nas)),
									     J9UTF8_DATA(J9ROMNAMEANDSIGNATURE_NAME(nas)),
									     J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_SIGNATURE(nas)),
									     J9UTF8_DATA(J9ROMNAMEANDSIGNATURE_SIGNATURE(nas))));

					/* Add the referenced Class item to the HT, we explicitly do it here in case the refered class
					 * has not yet been added. Defering it to be done via the CFR_CONSTANT_Class case would 
					 * prevent us from being able to save the index in htEntry->type.ref.classIndex (ie another
					 * pass would be needed once the CFR_CONSTANT_Class case adds it) */
					rc = jvmtiGetConstantPool_addClassOrString(translation, ref->classRefCPIndex, (U_8)CFR_CONSTANT_Class,
										   utf8, &sunCpIndex, &(htEntry->type.ref.classIndex));
					if (rc != JVMTI_ERROR_NONE) {
						return rc;
					}

					/* Add the referenced NameAndSignature item to the HT */
					rc = jvmtiGetConstantPool_addNAS(translation, nas,
									 &sunCpIndex, &htEntry->type.ref.nameAndTypeIndex);
					if (rc != JVMTI_ERROR_NONE) {
						return rc;
					}

					break;

				case CFR_CONSTANT_InvokeDynamic:
					/* Fix the invokedynamic by adding the NameAndSignature to the hashtable and binding in the index */
					nas = htEntry->type.invokedynamic.nas;

					jvmtiGetConstantPoolTranslate_printf(("	invokedynamic : bsmIndex [%i] NAS [%*s][%*s]\n",
										htEntry->type.invokedynamic.bsmIndex,
									    J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_NAME(nas)),
									    J9UTF8_DATA(J9ROMNAMEANDSIGNATURE_NAME(nas)),
									    J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_SIGNATURE(nas)),
									    J9UTF8_DATA(J9ROMNAMEANDSIGNATURE_SIGNATURE(nas))));

					/* Add the referenced NameAndSignature item to the HT */
					rc = jvmtiGetConstantPool_addNAS(translation, nas,
									 &sunCpIndex, &htEntry->type.invokedynamic.nameAndTypeIndex);
					if (rc != JVMTI_ERROR_NONE) {
						return rc;
					}

					break;

				case CFR_CONSTANT_MethodType:
					{
						U_16 origin = htEntry->type.methodType.methodType->cpType >> J9DescriptionCpTypeShift;
						switch(origin) {
						case J9_METHOD_TYPE_ORIGIN_LDC:
							/* Add and Bind the UTF8 used by this MethodType item */
							rc = jvmtiGetConstantPool_addUTF8(translation, htEntry->type.methodType.utf8, &sunCpIndex, &htEntry->type.methodType.methodTypeIndex);
							if (rc != JVMTI_ERROR_NONE) {
								JVMTI_ERROR(rc);
							}
							break;

						case J9_METHOD_TYPE_ORIGIN_INVOKE:
						case J9_METHOD_TYPE_ORIGIN_INVOKE_EXACT:
							/* Add the referenced Class item to the HT, we explicitly do it here in case the referred class
							 * has not yet been added. Deferring it to be done via the CFR_CONSTANT_Class case would
							 * prevent us from being able to save the index in htEntry->type.methodType.classIndex (ie another
							 * pass would be needed once the CFR_CONSTANT_Class case adds it) */
							rc = jvmtiGetConstantPool_addClassOrString(translation, (UDATA)&MH_methodHandle_utf, (U_8)CFR_CONSTANT_Class,
									(J9UTF8 *)&MH_methodHandle_utf, &sunCpIndex, &(htEntry->type.methodType.classIndex));
							if (rc != JVMTI_ERROR_NONE) {
								return rc;
							}

							/* Add the referenced NameAndSignature item to the HT */
							rc = jvmtiGetConstantPool_addNAS_name_sig(translation,
									(void *)(((U_8 *)htEntry->type.methodType.utf8) + 1),  /* Key for this is 1 off the sig utf8 */
									(origin == J9_METHOD_TYPE_ORIGIN_INVOKE_EXACT ? (J9UTF8 *)&MH_invokeExact_utf : (J9UTF8 *)&MH_invoke_utf),
									htEntry->type.methodType.utf8,
									&sunCpIndex,
									&htEntry->type.ref.nameAndTypeIndex);
							if (rc != JVMTI_ERROR_NONE) {
								return rc;
							}
							break;
						}
					}
					break;

				case CFR_CONSTANT_MethodHandle:
					/* Nothing to do: no UTF8s to bind */
					break;

				case CFR_CONSTANT_Integer:
				case CFR_CONSTANT_Float:
				case CFR_CONSTANT_Double:
				case CFR_CONSTANT_Long:

					break;

				default:
					jvmtiGetConstantPoolTranslate_printf(("   Unknown cpType %2d\n", cpItemType));
					JVMTI_ERROR(JVMTI_ERROR_INTERNAL);
			}
		}
	}

	translation->cpSize = sunCpIndex;

	return rc;

done:
	/* Error handling path */

	jvmtiGetConstantPool_free(privatePortLibrary, translation);

	return rc;
}



/** 
 * \brief	Free the translation HashTable and Array 
 * \ingroup 	jvmtiClass
 * 
 * 
 * @param privatePortLibrary 	port library
 * @param translation 		constant pool translation data
 * 
 */
void
jvmtiGetConstantPool_free(J9PortLibrary *privatePortLibrary, jvmtiGcp_translation *translation)
{
	if (translation->ht) {
		hashTableFree(translation->ht);
		translation->ht = NULL;
	}

	if (translation->cp) {
		j9mem_free_memory(translation->cp);
		translation->cp = NULL;
	}
}



/** 
 * \brief 	Write the translated constant pool in SUN byte packed format
 * \ingroup	jvmtiClass 
 * 
 * 
 * @param translation 		an initialized translation structure
 * @param constantPoolBuf 	the return buffer to contain the written constant pool bytes
 * @return 			a jvmtiError code
 * 
 * 	Use the translation data gathered by jvmtiGetConstantPool_translateCP to create a byte packed
 * 	constant pool stream. The constant pool conforms to the SUN classfile specification and must
 * 	match with the indices used by the bytecodes returned by the GetBytecodes jvmti API call. 
 *	
 */
static jvmtiError
jvmtiGetConstantPool_writeConstants(jvmtiGcp_translation *translation, unsigned char *constantPoolBuf)

{
	jvmtiError rc = JVMTI_ERROR_NONE;
	U_16 sunCpIndex;
	U_8  cpItemType;	 
	jvmtiGcp_translationEntry *htEntry;
	unsigned char * constantPoolBufIndex;
	
		
	jvmtiGetConstantPoolWrite_printf(("============ JVMTI GetConstantPool: Writing Constant Pool ===============\n"));
	
	constantPoolBufIndex = constantPoolBuf;
	
	
	for (sunCpIndex = 1; sunCpIndex < translation->cpSize; sunCpIndex++) {
		
		htEntry = translation->cp[sunCpIndex];
		cpItemType = htEntry->cpType;

		jvmtiGetConstantPoolWrite_printf(("W %2d/%d type: %2d    [%3d/%3d]     ---  ", 
						  sunCpIndex, translation->cpSize, cpItemType, 
						  constantPoolBufIndex - constantPoolBuf, translation->totalSize));



		switch(cpItemType) {
			case CFR_CONSTANT_Class:
				jvmtiGetConstantPoolWrite_printf(("        HT CPT %2d <Class> UTF8 %d->[%s]\n", 
								  htEntry->cpType, htEntry->type.clazz.nameIndex, ""));
				GCP_WRITE_U8 (constantPoolBufIndex, CFR_CONSTANT_Class);
				GCP_WRITE_U16(constantPoolBufIndex, htEntry->type.clazz.nameIndex);

				break;

			case CFR_CONSTANT_Fieldref:
			case CFR_CONSTANT_Methodref:
			case CFR_CONSTANT_InterfaceMethodref:
				jvmtiGetConstantPoolWrite_printf(("        HT CPT %2d <Ref> Class %d  NAS %d\n", 
								  htEntry->cpType, htEntry->type.ref.classIndex, htEntry->type.ref.nameAndTypeIndex));
				GCP_WRITE_U8 (constantPoolBufIndex, cpItemType);
				GCP_WRITE_U16(constantPoolBufIndex, htEntry->type.ref.classIndex);
				GCP_WRITE_U16(constantPoolBufIndex, htEntry->type.ref.nameAndTypeIndex);

				break;

			case CFR_CONSTANT_MethodHandle:
				jvmtiGetConstantPoolWrite_printf(("        HT CPT %2d <MethodHandle> method/fieldIndex %d  refType %d\n",
								  htEntry->cpType, htEntry->type.methodHandle.methodOrFieldRefIndex, htEntry->type.methodHandle.handleType));
				GCP_WRITE_U8 (constantPoolBufIndex, cpItemType);
				GCP_WRITE_U16(constantPoolBufIndex, htEntry->type.methodHandle.handleType);
				GCP_WRITE_U16(constantPoolBufIndex, htEntry->type.methodHandle.methodOrFieldRefIndex);

				break;

			case CFR_CONSTANT_InvokeDynamic:
				jvmtiGetConstantPoolWrite_printf(("        HT CPT %2d <InvokeDynamic> bsmIndex %d  NAS %d\n",
								  htEntry->cpType, htEntry->type.invokedynamic.bsmIndex, htEntry->type.invokedynamic.nameAndTypeIndex));
				GCP_WRITE_U8 (constantPoolBufIndex, cpItemType);
				GCP_WRITE_U16(constantPoolBufIndex, htEntry->type.invokedynamic.bsmIndex);
				GCP_WRITE_U16(constantPoolBufIndex, htEntry->type.invokedynamic.nameAndTypeIndex);

				break;

			case CFR_CONSTANT_String:
				jvmtiGetConstantPoolWrite_printf(("        HT CPT %2d <String> UTF8 %d->[%s]\n", 
								  htEntry->cpType, htEntry->type.string.stringIndex, ""));
				GCP_WRITE_U8 (constantPoolBufIndex, CFR_CONSTANT_String);
				GCP_WRITE_U16(constantPoolBufIndex, htEntry->type.string.stringIndex);
				
				break;
				
			case CFR_CONSTANT_MethodType:
				if (J9_METHOD_TYPE_ORIGIN_LDC == (htEntry->type.methodType.methodType->cpType >> J9DescriptionCpTypeShift)) {
					jvmtiGetConstantPoolWrite_printf(("        HT CPT %2d <MethodType> UTF8 %d->[%s]\n",
									  htEntry->cpType, htEntry->type.methodType.methodTypeIndex, ""));
					GCP_WRITE_U8 (constantPoolBufIndex, CFR_CONSTANT_MethodType);
					GCP_WRITE_U16(constantPoolBufIndex, htEntry->type.methodType.methodTypeIndex);
				} else {
					/* invokehandle or invokehandlegeneric: MethodType mapped to MethodRef */
					jvmtiGetConstantPoolWrite_printf(("        HT CPT %2d <MethodRef from MethodType> UTF8 %d->[%s]\n",
														  htEntry->cpType, htEntry->type.methodType.methodTypeIndex, ""));
					GCP_WRITE_U8 (constantPoolBufIndex, CFR_CONSTANT_Methodref);
					GCP_WRITE_U16(constantPoolBufIndex, htEntry->type.methodType.classIndex);
					GCP_WRITE_U16(constantPoolBufIndex, htEntry->type.methodType.nameAndTypeIndex);
				}
				break;

			case CFR_CONSTANT_Integer:
				jvmtiGetConstantPoolWrite_printf(("        HT CPT %2d <Int> [0x%04x]\n", 
								  htEntry->cpType, htEntry->type.intFloat.data));
				GCP_WRITE_U8 (constantPoolBufIndex, CFR_CONSTANT_Integer);
				GCP_WRITE_U32(constantPoolBufIndex, htEntry->type.intFloat.data);
				break;

			case CFR_CONSTANT_Float:
				jvmtiGetConstantPoolWrite_printf(("        HT CPT %2d <Float> [0x%04x]\n", 
								  htEntry->cpType, htEntry->type.intFloat.data));
				GCP_WRITE_U8 (constantPoolBufIndex, CFR_CONSTANT_Float);
				GCP_WRITE_U32(constantPoolBufIndex, htEntry->type.intFloat.data);
				break;

			case CFR_CONSTANT_Double:
			case CFR_CONSTANT_Long:
				jvmtiGetConstantPoolWrite_printf(("        HT CPT %2d <Double/Long> [0x%04x%04x]\n", 
								  htEntry->cpType, htEntry->type.longDouble.slot1,  
								  htEntry->type.longDouble.slot2));
				if (cpItemType == CFR_CONSTANT_Double) {
					GCP_WRITE_U8 (constantPoolBufIndex, CFR_CONSTANT_Double);
				} else {
					GCP_WRITE_U8 (constantPoolBufIndex, CFR_CONSTANT_Long);
				}
#ifdef J9VM_ENV_LITTLE_ENDIAN
				GCP_WRITE_U32(constantPoolBufIndex, htEntry->type.longDouble.slot2);
				GCP_WRITE_U32(constantPoolBufIndex, htEntry->type.longDouble.slot1);
#else					
				GCP_WRITE_U32(constantPoolBufIndex, htEntry->type.longDouble.slot1);
				GCP_WRITE_U32(constantPoolBufIndex, htEntry->type.longDouble.slot2);
#endif				
				/* Skip additional CP entry. See JVM spec v2 section 4.4.5 for the briliant rationale */
				sunCpIndex++;

				break;

			case CFR_CONSTANT_Utf8:
				jvmtiGetConstantPoolWrite_printf(("        HT CPT %2d <UTF8> [%*s]\n", 
								 htEntry->cpType,  J9UTF8_LENGTH(htEntry->type.utf8.data), J9UTF8_DATA(htEntry->type.utf8.data)));
				GCP_WRITE_U8 (constantPoolBufIndex, CFR_CONSTANT_Utf8);
				GCP_WRITE_U16(constantPoolBufIndex, J9UTF8_LENGTH(htEntry->type.utf8.data));
				memcpy(constantPoolBufIndex,  J9UTF8_DATA(htEntry->type.utf8.data),  J9UTF8_LENGTH(htEntry->type.utf8.data));
				constantPoolBufIndex +=  J9UTF8_LENGTH(htEntry->type.utf8.data);

				break;

			case CFR_CONSTANT_NameAndType:

				jvmtiGetConstantPoolWrite_printf(("        HT CPT %2d <NAS> Name %d->[%s] Sig %d->[%s]\n", htEntry->cpType, 
								 htEntry->type.nas.nameIndex, "",
								 htEntry->type.nas.signatureIndex, ""));

				GCP_WRITE_U8 (constantPoolBufIndex, CFR_CONSTANT_NameAndType);
				GCP_WRITE_U16(constantPoolBufIndex, htEntry->type.nas.nameIndex);
				GCP_WRITE_U16(constantPoolBufIndex, htEntry->type.nas.signatureIndex);

				break;


			default:
				JVMTI_ERROR(JVMTI_ERROR_INTERNAL);
				break;
		}

	}

done:

	/* Belt and suspenders, check if we have not overflown the return buffer */
	if ((U_32) (constantPoolBufIndex - constantPoolBuf) > translation->totalSize) {
		jvmtiGetConstantPoolWrite_printf(("ERROR: Constant Pool buffer Overflow   %d > %d\n", 
						 constantPoolBufIndex - constantPoolBuf, translation->totalSize));
		rc = JVMTI_ERROR_INTERNAL;
	}

	return rc;
}



static jvmtiError
jvmtiGetConstantPool_addClassOrString(jvmtiGcp_translation *translation, UDATA cpIndex, U_8 cpType, J9UTF8 *utf8, U_32 *sunCpIndex, U_32 *refIndex)
{
	jvmtiGcp_translationEntry entry;
	jvmtiGcp_translationEntry *htEntry;

	/* Add the Class item to the hashtable, use our CP index as key */
	entry.key = (void *) cpIndex;

	if (NULL != (htEntry = hashTableFind(translation->ht, &entry))) {
		/* Found a match, we've already added it before hence return to prevent
		 * unnecessary duplication */
		if (refIndex) 
			*refIndex = htEntry->sunCpIndex;
		return JVMTI_ERROR_NONE;
	}


	entry.cpType = cpType;
	entry.sunCpIndex = *sunCpIndex;
	if (cpType == CFR_CONSTANT_Class) {
		entry.type.clazz.nameIndex = 0;
		entry.type.clazz.utf8 = utf8;
	} else {
		entry.type.string.stringIndex = 0;
		entry.type.string.utf8 = utf8;
	}

	if (NULL == (htEntry = hashTableAdd(translation->ht, &entry))) {
		return JVMTI_ERROR_OUT_OF_MEMORY;
	}
	if (refIndex) {
		*refIndex = *sunCpIndex;
	}

	translation->cp[*sunCpIndex] = htEntry;
	translation->totalSize += 3;
	(*sunCpIndex)++;

	jvmtiGetConstantPoolTranslate_printf(("	Class|String : UTF8 [%*s]\n", J9UTF8_LENGTH(utf8), J9UTF8_DATA(utf8)));
	
	return JVMTI_ERROR_NONE;
}

static jvmtiError
jvmtiGetConstantPool_addMethodType(jvmtiGcp_translation *translation, UDATA cpIndex, U_8 cpType, J9ROMMethodTypeRef *ref, U_32 *sunCpIndex)
{
	jvmtiGcp_translationEntry entry;
	jvmtiGcp_translationEntry *htEntry;
	J9UTF8 *utf8 = J9ROMMETHODTYPEREF_SIGNATURE(ref);
	jvmtiError rc;

	/* Add the MethodType item to the hashtable, use our CP index as key so that
	 * jvmtiMethod.c:jvmtiGetBytecodes can map back to the right cpItem
	 */
	entry.key = (void *) cpIndex;

	if (NULL != (hashTableFind(translation->ht, &entry))) {
		return JVMTI_ERROR_NONE;
	}

	entry.cpType = cpType;
	entry.sunCpIndex = *sunCpIndex;
	entry.type.methodType.methodTypeIndex = 0;
	entry.type.methodType.utf8 = utf8;
	entry.type.methodType.methodType = ref;

	if (NULL == (htEntry = hashTableAdd(translation->ht, &entry))) {
		return JVMTI_ERROR_OUT_OF_MEMORY;
	}

	translation->cp[*sunCpIndex] = htEntry;
	if ((ref->cpType >> J9DescriptionCpTypeShift) == J9_METHOD_TYPE_ORIGIN_LDC) {
		translation->totalSize += 3;
		rc = JVMTI_ERROR_NONE;
	} else {
		/* invoke or invokeExact: therefore a methodref and needs 5 */
		translation->totalSize += 5;

		/* also register an entry for the MethodHandle class:
		 * use address of the MH_methodHandle_utf as the cpIndex aka key to hashtable */
		rc = jvmtiGetConstantPool_addClassOrString(translation,
								(UDATA)&MH_methodHandle_utf,
								(U_8) CFR_CONSTANT_Class,
								(J9UTF8 *)&MH_methodHandle_utf,
								sunCpIndex, NULL);
	}

	(*sunCpIndex)++;

	jvmtiGetConstantPoolTranslate_printf(("	MethodType : UTF8 [%*s]\n", J9UTF8_LENGTH(utf8), J9UTF8_DATA(utf8)));

	return rc;
}

static jvmtiError
jvmtiGetConstantPool_addInvokeDynamic(jvmtiGcp_translation *translation, UDATA key, J9ROMNameAndSignature *nameAndSig, U_32 bsmIndex, U_32 *sunCpIndex)
{
	jvmtiGcp_translationEntry entry;
	jvmtiGcp_translationEntry *htEntry;

	entry.key = (void *) key;

	if (NULL != (hashTableFind(translation->ht, &entry))) {
		return JVMTI_ERROR_NONE;
	}

	entry.cpType = CFR_CONSTANT_InvokeDynamic;
	entry.sunCpIndex = *sunCpIndex;
	entry.type.invokedynamic.bsmIndex = bsmIndex;
	entry.type.invokedynamic.nameAndTypeIndex = 0;
	entry.type.invokedynamic.nas = nameAndSig;

	if (NULL == (htEntry = hashTableAdd(translation->ht, &entry))) {
		return JVMTI_ERROR_OUT_OF_MEMORY;
	}

	translation->cp[*sunCpIndex] = htEntry;
	translation->totalSize += 5;

	(*sunCpIndex)++;

	jvmtiGetConstantPoolTranslate_printf(("	InvokeDynamic : bsmIndex [%i] NaS [toBeTranslated]\n", bsmIndex));

	return JVMTI_ERROR_NONE;
}

static jvmtiError
jvmtiGetConstantPool_addUTF8(jvmtiGcp_translation *translation, J9UTF8 *utf8, U_32 *sunCpIndex, U_32 *refIndex)
{
	jvmtiGcp_translationEntry entry;
	jvmtiGcp_translationEntry *htEntry;

	entry.key = utf8;

	jvmtiGetConstantPoolTranslate_printf(("	UTF8 [%*s]\n", J9UTF8_LENGTH(utf8), J9UTF8_DATA(utf8)));

	if (NULL != (htEntry = hashTableFind(translation->ht, &entry))) {
		/* Found a match, we've already added it before hence return to prevent
		 * unnecessary duplication */
		*refIndex = htEntry->sunCpIndex;
		return JVMTI_ERROR_NONE;
	}

	/* Store the new/translated CP index */
	entry.sunCpIndex = *sunCpIndex;
	entry.cpType = CFR_CONSTANT_Utf8;
	entry.type.utf8.data = utf8;

	/* Add the UTF8 to the hashtable */
	if (NULL == (translation->cp[*sunCpIndex] = hashTableAdd(translation->ht, &entry))) {
		return JVMTI_ERROR_OUT_OF_MEMORY;
	}

	*refIndex = *sunCpIndex;
	(*sunCpIndex)++;

	translation->totalSize += 3 + J9UTF8_LENGTH(utf8);

	return JVMTI_ERROR_NONE;
}

	
static jvmtiError
jvmtiGetConstantPool_addReference(jvmtiGcp_translation *translation, UDATA cpIndex, U_8 cpType, J9ROMFieldRef *ref, U_32 *sunCpIndex)
{
	jvmtiGcp_translationEntry entry;
	jvmtiGcp_translationEntry *htEntry;
	
	/* Add the Reference item to the hashtable, use our CP index as key */
	entry.key = (void *) cpIndex;
	entry.cpType = cpType;
	entry.sunCpIndex = *sunCpIndex;
	entry.type.ref.ref = ref;
	entry.type.ref.classIndex = 0;
	entry.type.ref.nameAndTypeIndex = 0;
	if (NULL == (htEntry = hashTableAdd(translation->ht, &entry))) {
		return JVMTI_ERROR_OUT_OF_MEMORY;
	}
	
	translation->totalSize += 5;
	translation->cp[*sunCpIndex] = htEntry;
	(*sunCpIndex)++;
	
	return JVMTI_ERROR_NONE;
}

static jvmtiError
jvmtiGetConstantPool_addMethodHandle(jvmtiGcp_translation *translation, UDATA cpIndex, U_8 cpType, J9ROMMethodHandleRef *ref, U_32 *sunCpIndex)
{
	jvmtiGcp_translationEntry entry;
	jvmtiGcp_translationEntry *htEntry;
	U_8 fieldOrMethodCpType;

	/* Add the Reference item to the hashtable, use our CP index as key */
	entry.key = (void *) cpIndex;
	entry.cpType = cpType;
	entry.sunCpIndex = *sunCpIndex;
	entry.type.methodHandle.methodOrFieldRefIndex = 0;
	entry.type.methodHandle.handleType = ref->handleTypeAndCpType >> J9DescriptionCpTypeShift;
	if (NULL == (htEntry = hashTableAdd(translation->ht, &entry))) {
		return JVMTI_ERROR_OUT_OF_MEMORY;
	}

	translation->totalSize += 5;
	translation->cp[*sunCpIndex] = htEntry;
	(*sunCpIndex)++;

	switch(entry.type.methodHandle.handleType) {
	case MH_REF_GETFIELD:
	case MH_REF_PUTFIELD:
	case MH_REF_GETSTATIC:
	case MH_REF_PUTSTATIC:
		fieldOrMethodCpType = CFR_CONSTANT_Fieldref;
		break;

	case MH_REF_INVOKEVIRTUAL:
	case MH_REF_INVOKESTATIC:
	case MH_REF_INVOKESPECIAL:
	case MH_REF_NEWINVOKESPECIAL:
		fieldOrMethodCpType = CFR_CONSTANT_Methodref;
		break;

	case MH_REF_INVOKEINTERFACE:
		fieldOrMethodCpType = CFR_CONSTANT_InterfaceMethodref;
		break;
	default:
		Assert_JVMTI_true(0);
		return JVMTI_ERROR_INTERNAL;
	}

	/* Ensure the field/method ref is also added */
	return jvmtiGetConstantPool_addReference(translation, ref->methodOrFieldRefIndex,
						fieldOrMethodCpType,
						(J9ROMFieldRef *) &translation->romConstantPool[ref->methodOrFieldRefIndex],
						sunCpIndex);
}


static jvmtiError
jvmtiGetConstantPool_addNAS(jvmtiGcp_translation *translation, J9ROMNameAndSignature *nas, U_32 *sunCpIndex, U_32 *refIndex)
{
	return jvmtiGetConstantPool_addNAS_name_sig(translation, nas, J9ROMNAMEANDSIGNATURE_NAME(nas), J9ROMNAMEANDSIGNATURE_SIGNATURE(nas), sunCpIndex, refIndex);
}

/* 292 MTs that convert to MethodRefs don't have a ROM NAS - just a static one in this file.  This allows us to pass
 * all the right data in without having to determine if its a ROM NaS or static one.
 */
static jvmtiError
jvmtiGetConstantPool_addNAS_name_sig(jvmtiGcp_translation *translation, void *key, J9UTF8 *name, J9UTF8* sig, U_32 *sunCpIndex, U_32 *refIndex)
{
	jvmtiError rc;
	jvmtiGcp_translationEntry entry, *htEntry;
	
	entry.key = key;
	entry.sunCpIndex = *sunCpIndex;
	entry.cpType = CFR_CONSTANT_NameAndType;
	entry.type.nas.name = name;
	entry.type.nas.signature = sig;

	jvmtiGetConstantPoolTranslate_printf(("        NAS [%*s][%*s]\n", 
					     J9UTF8_LENGTH(entry.type.nas.name), J9UTF8_DATA(entry.type.nas.name),
					     J9UTF8_LENGTH(entry.type.nas.signature), J9UTF8_DATA(entry.type.nas.signature)));


	if (NULL != (htEntry = hashTableFind(translation->ht, &entry))) {
		/* Found a match, we've already added it before hence return to prevent
		 * unnecessary duplication */
		*refIndex = htEntry->sunCpIndex;
		return JVMTI_ERROR_NONE;
	}

	/* Add the NameAndSignature entry to the hashtable */
	if (NULL == (htEntry = hashTableAdd(translation->ht, &entry))) {
		return JVMTI_ERROR_OUT_OF_MEMORY;
	}

	translation->cp[*sunCpIndex] = htEntry;
	*refIndex = *sunCpIndex;
	(*sunCpIndex)++;

	/* Add an entry for the name_index part of the NAS */
	rc = jvmtiGetConstantPool_addUTF8(translation, name, sunCpIndex, &htEntry->type.nas.nameIndex);
	if (rc != JVMTI_ERROR_NONE) {
		return rc;
	}

	/* Add an entry for the signature_index part of the NAS entry */ 
	rc = jvmtiGetConstantPool_addUTF8(translation, sig, sunCpIndex, &htEntry->type.nas.signatureIndex);
	if (rc != JVMTI_ERROR_NONE) {
		return rc;
	}

	translation->totalSize += 5;

	return JVMTI_ERROR_NONE; 

}

static jvmtiError
jvmtiGetConstantPool_addIntFloat(jvmtiGcp_translation *translation, UDATA cpIndex, U_8 cpType, U_32 data, U_32 *sunCpIndex)
{
	jvmtiGcp_translationEntry entry;

	jvmtiGetConstantPoolTranslate_printf(("	Int/Float [0x%08x]\n", data));

	entry.key = (void *) cpIndex;
	entry.sunCpIndex = *sunCpIndex;
	entry.cpType = cpType;
	entry.type.intFloat.data = data;

	/* Add the entry to the hashtable */
	if (NULL == (translation->cp[*sunCpIndex] = hashTableAdd(translation->ht, &entry))) {
		return JVMTI_ERROR_OUT_OF_MEMORY;
	}

	(*sunCpIndex)++;

	translation->totalSize += 5;

	return JVMTI_ERROR_NONE;
}


static jvmtiError
jvmtiGetConstantPool_addLongDouble(jvmtiGcp_translation *translation, UDATA cpIndex, U_8 cpType, U_32 slot1, U_32 slot2, U_32 *sunCpIndex)
{
	jvmtiGcp_translationEntry entry;
	
	jvmtiGetConstantPoolTranslate_printf(("	Long/Double [0x%08x%08x]\n", slot1, slot2));
	
	entry.key = (void *) cpIndex;
	entry.sunCpIndex = *sunCpIndex;
	entry.cpType = cpType;
	entry.type.longDouble.slot1 = slot1;
	entry.type.longDouble.slot2 = slot2;
	
	/* Add the entry to the hashtable */
	if (NULL == (translation->cp[*sunCpIndex] = hashTableAdd(translation->ht, &entry))) {
		return JVMTI_ERROR_OUT_OF_MEMORY;
	}

	translation->cp[*sunCpIndex + 1] = NULL;

	(*sunCpIndex) += 2;

	translation->totalSize += 9;

	return JVMTI_ERROR_NONE;
}

static UDATA
jvmtiGetConstantPool_HashFn(void *entry, void *userData)
{
	jvmtiGcp_translationEntry *key = (jvmtiGcp_translationEntry *) entry;

	return (UDATA) key->key;
}


static UDATA
jvmtiGetConstantPool_HashEqualFn(void *lhsEntry, void *rhsEntry, void *userData)
{
	jvmtiGcp_translationEntry *e1 = (jvmtiGcp_translationEntry *) lhsEntry;
	jvmtiGcp_translationEntry *e2 = (jvmtiGcp_translationEntry *) rhsEntry;

	return (e1->key == e2->key);
}
