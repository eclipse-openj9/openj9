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

#include "jvmtiHelpers.h"
#include "bcnames.h"
#include "pcstack.h"
#include "cfr.h"
#include "jvmti_internal.h"

static void deallocateVariableTable (jvmtiEnv * env, jint size, jvmtiLocalVariableEntry * jvmtiVariableTable);
static jvmtiError setNativeMethodPrefixes(J9JVMTIEnv * env, jint prefix_count, char ** prefixes);


jvmtiError JNICALL
jvmtiGetMethodName(jvmtiEnv* env,
	jmethodID method,
	char** name_ptr,
	char** signature_ptr,
	char** generic_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9ROMMethod * romMethod;
	char * name = NULL;
	char * signature = NULL;
	char * generic = NULL;
	PORT_ACCESS_FROM_JAVAVM(vm);
	char *rv_name = NULL;
	char *rv_signature = NULL;
	char *rv_generic = NULL;

	Trc_JVMTI_jvmtiGetMethodName_Entry(env);

	ENSURE_PHASE_START_OR_LIVE(env);

	ENSURE_JMETHODID_NON_NULL(method);

	romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(((J9JNIMethodID *) method)->method);

	if (name_ptr != NULL) {
		J9UTF8 * utf = J9ROMMETHOD_GET_NAME(UNTAGGED_METHOD_CP(((J9JNIMethodID *) method)->method)->ramClass->romClass, romMethod);
		UDATA length = J9UTF8_LENGTH(utf);

		name = j9mem_allocate_memory(length + 1, J9MEM_CATEGORY_JVMTI_ALLOCATE);
		if (name == NULL) {
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
			goto done;
		}
		memcpy(name, J9UTF8_DATA(utf), length);
		name[length] = '\0';
		rv_name = name; 
	}

	if (signature_ptr != NULL) {
		J9UTF8 * utf = J9ROMMETHOD_GET_SIGNATURE(UNTAGGED_METHOD_CP(((J9JNIMethodID *) method)->method)->ramClass->romClass, romMethod);
		UDATA length = J9UTF8_LENGTH(utf);

		signature = j9mem_allocate_memory(length + 1, J9MEM_CATEGORY_JVMTI_ALLOCATE);
		if (signature == NULL) {
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
			goto done;
		}
		memcpy(signature, J9UTF8_DATA(utf), length);
		signature[length] = '\0';
		rv_signature = signature; 
	}

	if (generic_ptr != NULL) {
		J9UTF8 * utf = J9_GENERIC_SIGNATURE_FROM_ROM_METHOD(romMethod);

		if (utf != NULL) {
			UDATA length = J9UTF8_LENGTH(utf);

			generic = j9mem_allocate_memory(length + 1, J9MEM_CATEGORY_JVMTI_ALLOCATE);
			if (generic == NULL) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
				goto done;
			}
			memcpy(generic, J9UTF8_DATA(utf), length);
			generic[length] = '\0';
		}
	
		rv_generic = generic;
	}	

done:
	if (rc != JVMTI_ERROR_NONE) {
		j9mem_free_memory(name);
		j9mem_free_memory(signature);
		j9mem_free_memory(generic);
	}
	if (name_ptr != NULL) {
		*name_ptr = rv_name;
	}
	if (signature_ptr != NULL) {
		*signature_ptr = rv_signature;
	}
	if (generic_ptr != NULL) {
		*generic_ptr = rv_generic;
	}
	TRACE_JVMTI_RETURN(jvmtiGetMethodName);
}


jvmtiError JNICALL
jvmtiGetMethodDeclaringClass(jvmtiEnv* env,
	jmethodID method,
	jclass* declaring_class_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;
	jclass rv_declaring_class = NULL;

	Trc_JVMTI_jvmtiGetMethodDeclaringClass_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9Class * methodClass;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);

		ENSURE_JMETHODID_NON_NULL(method);
		ENSURE_NON_NULL(declaring_class_ptr);

		methodClass = getCurrentClass(J9_CLASS_FROM_METHOD(((J9JNIMethodID *) method)->method));
		rv_declaring_class = (jclass) vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *) currentThread, J9VM_J9CLASS_TO_HEAPCLASS(methodClass));

done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != declaring_class_ptr) {		
		*declaring_class_ptr = rv_declaring_class;
	}
	TRACE_JVMTI_RETURN(jvmtiGetMethodDeclaringClass);
}


jvmtiError JNICALL
jvmtiGetMethodModifiers(jvmtiEnv* env,
	jmethodID method,
	jint* modifiers_ptr)
{
	jvmtiError rc;
	J9ROMMethod * romMethod;
	jint rv_modifiers = 0;

	Trc_JVMTI_jvmtiGetMethodModifiers_Entry(env);

	ENSURE_PHASE_START_OR_LIVE(env);

	ENSURE_JMETHODID_NON_NULL(method);
	ENSURE_NON_NULL(modifiers_ptr);

	romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(((J9JNIMethodID *) method)->method);
	rv_modifiers = (jint) (romMethod->modifiers &
			(J9AccPublic | J9AccPrivate | J9AccProtected | J9AccStatic | J9AccFinal | J9AccSynchronized | J9AccNative | J9AccAbstract | J9AccStrict | J9AccVarArgs | J9AccBridge));
	rc = JVMTI_ERROR_NONE;

done:
	if (NULL != modifiers_ptr) {
		*modifiers_ptr = rv_modifiers;
	}
	TRACE_JVMTI_RETURN(jvmtiGetMethodModifiers);
}


jvmtiError JNICALL
jvmtiGetMaxLocals(jvmtiEnv* env,
	jmethodID method,
	jint* max_ptr)
{
	jvmtiError rc;
	J9ROMMethod * romMethod;
	jint rv_max = 0;

	Trc_JVMTI_jvmtiGetMaxLocals_Entry(env);

	ENSURE_PHASE_START_OR_LIVE(env);

	ENSURE_JMETHODID_NON_NULL(method);
	ENSURE_NON_NULL(max_ptr);

	romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(((J9JNIMethodID *) method)->method);
	if (romMethod->modifiers & J9AccNative) {
		JVMTI_ERROR(JVMTI_ERROR_NATIVE_METHOD);
	}

	/* Abstract methods have no code attribute in the .class file, so the max locals is 0 */

	if (romMethod->modifiers & J9AccAbstract) {
		rv_max = 0;
	} else {
		rv_max = romMethod->argCount + romMethod->tempCount;
	}
	rc = JVMTI_ERROR_NONE;

done:
	if (NULL != max_ptr) {
		*max_ptr = rv_max;
	}
	TRACE_JVMTI_RETURN(jvmtiGetMaxLocals);
}


jvmtiError JNICALL
jvmtiGetArgumentsSize(jvmtiEnv* env,
	jmethodID method,
	jint* size_ptr)
{
	jvmtiError rc;
	J9ROMMethod * romMethod;
	jint rv_size = 0;

	Trc_JVMTI_jvmtiGetArgumentsSize_Entry(env);

	ENSURE_PHASE_START_OR_LIVE(env);

	ENSURE_JMETHODID_NON_NULL(method);
	ENSURE_NON_NULL(size_ptr);

	romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(((J9JNIMethodID *) method)->method);
	if (romMethod->modifiers & J9AccNative) {
		JVMTI_ERROR(JVMTI_ERROR_NATIVE_METHOD);
	}
	rv_size = romMethod->argCount;
	rc = JVMTI_ERROR_NONE;

done:
	if (NULL != size_ptr) {
		*size_ptr = rv_size;
	}
	TRACE_JVMTI_RETURN(jvmtiGetArgumentsSize);
}


jvmtiError JNICALL
jvmtiGetLineNumberTable(jvmtiEnv* env,
	jmethodID method,
	jint* entry_count_ptr,
	jvmtiLineNumberEntry** table_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9MethodDebugInfo *methodDebugInfo;
	jvmtiError rc = JVMTI_ERROR_ABSENT_INFORMATION;
	J9Method * ramMethod;
	J9ROMMethod * romMethod;
	J9LineNumber lineNumber;
	PORT_ACCESS_FROM_JAVAVM(vm);
	jint rv_entry_count = 0;
	jvmtiLineNumberEntry *rv_table = NULL;

	Trc_JVMTI_jvmtiGetLineNumberTable_Entry(env);

	ENSURE_PHASE_START_OR_LIVE(env);
	ENSURE_CAPABILITY(env, can_get_line_numbers);

	ENSURE_JMETHODID_NON_NULL(method);
	ENSURE_NON_NULL(entry_count_ptr);
	ENSURE_NON_NULL(table_ptr);

	lineNumber.lineNumber = 0;
	lineNumber.location = 0;

	/* Native and abstract methods don't have variable tables */

	ramMethod = ((J9JNIMethodID *) method)->method;
	romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);
	if (romMethod->modifiers & J9AccNative) {
		JVMTI_ERROR(JVMTI_ERROR_NATIVE_METHOD);
	}
	if (romMethod->modifiers & J9AccAbstract) {
		JVMTI_ERROR(JVMTI_ERROR_ABSENT_INFORMATION);
	}

	/* Assume that having the capability means that the debug info server is active */

	methodDebugInfo = getMethodDebugInfoForROMClass(vm, ramMethod);
	if (methodDebugInfo != NULL) {
		jint lineTableSize = (jint) getLineNumberCount(methodDebugInfo);
		U_8 * lineTable = getLineNumberTable(methodDebugInfo);

		if (lineTable != NULL) {
			jvmtiLineNumberEntry * jvmtiLineTable;

			jvmtiLineTable = j9mem_allocate_memory(lineTableSize * sizeof(jvmtiLineNumberEntry), J9MEM_CATEGORY_JVMTI_ALLOCATE);
			if (jvmtiLineTable == NULL) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
			} else {
				jint i;

				rc = JVMTI_ERROR_NONE;
				for (i = 0; i < lineTableSize; ++i) {
					if (!getNextLineNumberFromTable(&lineTable, &lineNumber)) {
						rc = JVMTI_ERROR_INTERNAL;
						j9mem_free_memory(jvmtiLineTable);
						goto release;
					}
					jvmtiLineTable[i].start_location = (jlocation)lineNumber.location;
					jvmtiLineTable[i].line_number = (jint) lineNumber.lineNumber;

				}
				rv_entry_count = lineTableSize;
				rv_table = jvmtiLineTable;
			}
		}
release:
		releaseOptInfoBuffer(vm, J9_CLASS_FROM_METHOD(ramMethod)->romClass);
	}

done:
	if (NULL != entry_count_ptr) {
		*entry_count_ptr = rv_entry_count;
	}
	if (NULL != table_ptr) {
		*table_ptr = rv_table;
	}
	TRACE_JVMTI_RETURN(jvmtiGetLineNumberTable);
}


jvmtiError JNICALL
jvmtiGetMethodLocation(jvmtiEnv* env,
	jmethodID method,
	jlocation* start_location_ptr,
	jlocation* end_location_ptr)
{
	jvmtiError rc;
	J9ROMMethod * romMethod;
	jlocation rv_start_location = 0;
	jlocation rv_end_location = 0;

	Trc_JVMTI_jvmtiGetMethodLocation_Entry(env);

	ENSURE_PHASE_START_OR_LIVE(env);

	ENSURE_JMETHODID_NON_NULL(method);
	ENSURE_NON_NULL(start_location_ptr);
	ENSURE_NON_NULL(end_location_ptr);

	romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(((J9JNIMethodID *) method)->method);
	if (romMethod->modifiers & J9AccNative) {
		JVMTI_ERROR(JVMTI_ERROR_NATIVE_METHOD);
	}
	if (romMethod->modifiers & J9AccAbstract) {
		rv_start_location = -1;
		rv_end_location = -1;
	} else {
		rv_start_location = 0;
		rv_end_location = J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod) - 1;
	}
	rc = JVMTI_ERROR_NONE;

done:
	if (NULL != start_location_ptr) {
		*start_location_ptr = rv_start_location;
	}
	if (NULL != end_location_ptr) {
		*end_location_ptr = rv_end_location;
	}
	TRACE_JVMTI_RETURN(jvmtiGetMethodLocation);
}


jvmtiError JNICALL
jvmtiGetLocalVariableTable(jvmtiEnv* env,
	jmethodID method,
	jint* entry_count_ptr,
	jvmtiLocalVariableEntry** table_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9MethodDebugInfo *methodDebugInfo;
	jvmtiError rc = JVMTI_ERROR_ABSENT_INFORMATION;
	J9Method * ramMethod;
	J9ROMMethod * romMethod;
	PORT_ACCESS_FROM_JAVAVM(vm);
	jint rv_entry_count = 0;
	jvmtiLocalVariableEntry *rv_table = NULL;

	Trc_JVMTI_jvmtiGetLocalVariableTable_Entry(env);

	ENSURE_PHASE_LIVE(env);
	ENSURE_CAPABILITY(env, can_access_local_variables);

	ENSURE_JMETHODID_NON_NULL(method);
	ENSURE_NON_NULL(entry_count_ptr);
	ENSURE_NON_NULL(table_ptr);

	/* Native and abstract methods don't have variable tables */

	ramMethod = ((J9JNIMethodID *) method)->method;
	romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);
	if (romMethod->modifiers & J9AccNative) {
		JVMTI_ERROR(JVMTI_ERROR_NATIVE_METHOD);
	}
	if (romMethod->modifiers & J9AccAbstract) {
		JVMTI_ERROR(JVMTI_ERROR_ABSENT_INFORMATION);
	}

	/* Assume that having the capability means that the debug info server is active */

	methodDebugInfo = getMethodDebugInfoForROMClass(vm, ramMethod);
	if (methodDebugInfo != NULL) {
		jint variableCount = (jint) methodDebugInfo->varInfoCount;
		J9VariableInfoWalkState state;
		J9VariableInfoValues *values = NULL;

		values = variableInfoStartDo(methodDebugInfo, &state);
		if (values != NULL) {
			jvmtiLocalVariableEntry * jvmtiVariableTable;

			jvmtiVariableTable = j9mem_allocate_memory(variableCount * sizeof(jvmtiLocalVariableEntry), J9MEM_CATEGORY_JVMTI_ALLOCATE);
			if (jvmtiVariableTable == NULL) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
			} else {
				jint i = 0;

				rc = JVMTI_ERROR_NONE;
				while(values != NULL) {
					jvmtiVariableTable[i].start_location = (jlocation) values->startVisibility;
					jvmtiVariableTable[i].length = (jint) values->visibilityLength;
					jvmtiVariableTable[i].slot = (jint) values->slotNumber;
					jvmtiVariableTable[i].generic_signature = NULL;	/* in case the name alloc fails */
					jvmtiVariableTable[i].name = NULL; 						/* in case the name alloc fails */
					jvmtiVariableTable[i].signature = NULL;					/* in case the name alloc fails */
					if (((rc = cStringFromUTF(env, values->name, &(jvmtiVariableTable[i].name))) != JVMTI_ERROR_NONE) ||
						  ((rc = cStringFromUTF(env, values->signature, &(jvmtiVariableTable[i].signature))) != JVMTI_ERROR_NONE)){
						deallocateVariableTable(env, i + 1, jvmtiVariableTable);
						goto release;
					}
					if (values->genericSignature != NULL) {
						if ((rc = cStringFromUTF(env, values->genericSignature, &(jvmtiVariableTable[i].generic_signature))) != JVMTI_ERROR_NONE) {
							deallocateVariableTable(env, i + 1, jvmtiVariableTable);
							goto release;
						}
					}
					values = variableInfoNextDo(&state);
					i++;
				}
				rv_entry_count = variableCount;
				rv_table = jvmtiVariableTable;
			}
		}
release:
		releaseOptInfoBuffer(vm, J9_CLASS_FROM_METHOD(ramMethod)->romClass);
	}

done:
	if (NULL != entry_count_ptr) {
		*entry_count_ptr = rv_entry_count;
	}
	if (NULL != table_ptr) {
		*table_ptr = rv_table;
	}
	TRACE_JVMTI_RETURN(jvmtiGetLocalVariableTable);
}



/** 
 * \brief	Get method byte codes 
 * \ingroup	jvmtiClass
 * 
 * @param env 
 * @param method	[IN] Method to get bytecodes for 
 * @param bytecode_count_ptr 	[OUT] Number of bytecode bytes returned by this call
 * @param bytecodes_ptr 	[OUT] Bytecodes for the specified method. User is responsible for freeing the memory with Deallocate
 * @return a jvmtiError code
 * 
 * @todo	This call might/should be optimized by caching a small number of constant pool
 * 		translation structures. It is conceivable that we are called sequentially to query
 * 		byte codes for N methods of the same class. This incurs the penalty of having to 
 * 		create the translation structure on each call. The cache should be flushed on
 * 		each class retransform or redefine. 
 *		
 */
jvmtiError JNICALL
jvmtiGetBytecodes(jvmtiEnv* env,
	jmethodID method,
	jint* bytecode_count_ptr,
	unsigned char** bytecodes_ptr)
{
	jint size;
	unsigned char * bytecodes = NULL;
	J9ROMMethod * romMethod;
	J9Class * class;
	jvmtiError rc;
	jvmtiGcp_translation translation;
	PORT_ACCESS_FROM_JVMTI(env);
	jint rv_bytecode_count = 0;
	unsigned char *rv_bytecodes = NULL;
	
	Trc_JVMTI_jvmtiGetBytecodes_Entry(env);

	translation.ht = NULL;
	translation.cp = NULL;

	ENSURE_PHASE_START_OR_LIVE(env);
	ENSURE_CAPABILITY(env, can_get_bytecodes);

	ENSURE_JMETHODID_NON_NULL(method);
	ENSURE_NON_NULL(bytecode_count_ptr);
	ENSURE_NON_NULL(bytecodes_ptr);

	romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(((J9JNIMethodID *) method)->method);
	if (romMethod->modifiers & J9AccNative) {
		JVMTI_ERROR(JVMTI_ERROR_NATIVE_METHOD);
	}

	/* Create a translation hashtable that will assist us reindexing constant pool
	 * indices used by the returned bytecodes. We must ensure that GetBytecodes 
	 * returns indices consistent with the once returned by GetConstantPool */
	
	class = J9_CLASS_FROM_METHOD(((J9JNIMethodID *) method)->method);
	rc = jvmtiGetConstantPool_translateCP(PORTLIB, &translation, class, FALSE);
	if (rc != JVMTI_ERROR_NONE) {
		JVMTI_ERROR(rc);
	}
			 
	size = (jint)J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);
	bytecodes = j9mem_allocate_memory(size, J9MEM_CATEGORY_JVMTI_ALLOCATE);
	if (bytecodes == NULL) {
		rc = JVMTI_ERROR_OUT_OF_MEMORY;
	} else {
		U_8 * j9Bytecodes = J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);
		jint index;

		/* Copy the bytecodes then walk them to fix them back into (more or less) Sun format */

		memcpy(bytecodes, j9Bytecodes, size);
		index = 0;
		while (index < size) {
			jvmtiGcp_translationEntry entry, *htEntry;
			U_8 bc = j9Bytecodes[index];
			jint bytecodeSize = (jint)(J9JavaInstructionSizeAndBranchActionTable[bc] & 7);

			if (bytecodeSize == 0) {
				JVMTI_ERROR(JVMTI_ERROR_INTERNAL);
			}

			switch (bc) {
				case JBldc:
					/* Rewrite the CP index in order to expose an identical CP shape as
					 * that returned by the GetConstantPool jvmti call */
					
					entry.key = (void *) ((UDATA) bytecodes[index + 1]); 
					if (NULL == (htEntry = hashTableFind(translation.ht, &entry))) { 
						JVMTI_ERROR(JVMTI_ERROR_INTERNAL); 
					} 
					bytecodes[index + 1] = (U_8) htEntry->sunCpIndex;

					break;
 
				case JBldc2dw:
				case JBldc2lw:
					
					bytecodes[index + 0] = CFR_BC_ldc2_w;

					/* Fall-through */
					
				case JBgetstatic:
				case JBputstatic:
				case JBgetfield:
				case JBputfield:
				case JBinstanceof:
				case JBinvokeinterface:
				case JBinvokespecial:
				case JBinvokestatic:
				case JBinvokevirtual:
				case JBmultianewarray:
				case JBanewarray:
				case JBcheckcast:
				case JBldcw:
				case JBnew:
				case JBinvokehandle:
				case JBinvokehandlegeneric:
				case JBinvokestaticsplit:
				case JBinvokespecialsplit:
				case JBinvokedynamic:

					/* Rewrite the CP index in order to expose an identical CP shape as
					 * that returned by the GetConstantPool jvmti call */
					if (bc != JBinvokedynamic) {
						U_16 cpIndex = *(U_16 *) &bytecodes[index + 1];

						switch (bc) {
						case JBinvokestaticsplit:
							/* treat cpIndex as index into static split table */
							cpIndex = *(U_16 *)(J9ROMCLASS_STATICSPLITMETHODREFINDEXES(class->romClass) + cpIndex);
							/* reset bytecode to non-split version */
							bytecodes[index] = CFR_BC_invokestatic;
							break;
						case JBinvokespecialsplit:
							/* treat cpIndex as index into special split table */
							cpIndex = *(U_16 *)(J9ROMCLASS_SPECIALSPLITMETHODREFINDEXES(class->romClass) + cpIndex);
							/* reset bytecode to non-split version */
							bytecodes[index] = CFR_BC_invokespecial;
							break;
						case JBinvokehandle:
						case JBinvokehandlegeneric:
							bytecodes[index] = CFR_BC_invokevirtual;
							/* intentional fall-through */
						default:
							cpIndex = *(U_16 *) &bytecodes[index + 1];
						}
						entry.key = (void *) (UDATA) cpIndex;
					} else {
						/* bc == JBinvokedynamic */
						/* TODO: fix this when we allow the full 3 byte index */
						entry.key = (void *) (((UDATA) *((U_16 *) &bytecodes[index + 1])) + ((UDATA) J9ROMCLASS_CALLSITEDATA(class->romClass)));
					}
					if (NULL == (htEntry = hashTableFind(translation.ht, &entry))) { 
						JVMTI_ERROR(JVMTI_ERROR_INTERNAL); 
					} 
#ifdef J9VM_ENV_LITTLE_ENDIAN 
					bytecodes[index + 1] = ((U_8 *) (&htEntry->sunCpIndex))[1];
					bytecodes[index + 2] = ((U_8 *) (&htEntry->sunCpIndex))[0];
#else
					bytecodes[index + 1] = ((U_8 *) (&htEntry->sunCpIndex))[0];
					bytecodes[index + 2] = ((U_8 *) (&htEntry->sunCpIndex))[1];
#endif
					break;

				case JBiloadw:
				case JBlloadw:
				case JBfloadw:
				case JBdloadw:
				case JBaloadw:
					bc = bc - JBiloadw + JBiload;
readdWide:
					bytecodes[index + 0] = CFR_BC_wide;
					bytecodes[index + 1] = bc;
#ifdef J9VM_ENV_LITTLE_ENDIAN
/*					bytecodes[index + 2] = j9Bytecodes[index + 2]; */
					bytecodes[index + 3] = j9Bytecodes[index + 1];
#else
					bytecodes[index + 2] = j9Bytecodes[index + 1];
					bytecodes[index + 3] = j9Bytecodes[index + 2];
#endif
					break;

				case JBistorew:
				case JBlstorew:
				case JBfstorew:
				case JBdstorew:
				case JBastorew:
					bc = bc - JBistorew + JBistore;
					goto readdWide;

				case JBiincw:
#ifdef J9VM_ENV_LITTLE_ENDIAN
/*					bytecodes[index + 4] = j9Bytecodes[index + 4]; */
					bytecodes[index + 5] = j9Bytecodes[index + 3];
#else
					bytecodes[index + 4] = j9Bytecodes[index + 3];
					bytecodes[index + 5] = j9Bytecodes[index + 4];
#endif
					bc = JBiinc;
					goto readdWide;

				case JBgenericReturn:
				case JBreturnFromConstructor:
				case JBreturn0:
				case JBreturn1:
				case JBreturn2:
				case JBreturnB:
				case JBreturnC:
				case JBreturnS:
				case JBreturnZ:
				case JBsyncReturn0:
				case JBsyncReturn1:
				case JBsyncReturn2: {
					J9UTF8 * signature = J9ROMMETHOD_GET_SIGNATURE(UNTAGGED_METHOD_CP(((J9JNIMethodID *) method)->method)->ramClass->romClass, romMethod);
					U_8 * sigData = J9UTF8_DATA(signature);
					U_16 sigLength = J9UTF8_LENGTH(signature);

					switch (sigData[sigLength - 2] == '[' ? ';' : sigData[sigLength - 1]) {
						case 'V':
							bc = CFR_BC_return;
							break;
						case 'J':
							bc = CFR_BC_lreturn;
							break;
						case 'D':
							bc = CFR_BC_dreturn;
							break;
						case 'F':
							bc = CFR_BC_freturn;
							break;
						case ';':
							bc = CFR_BC_areturn;
							break;
						default:
							bc = CFR_BC_ireturn;
							break;
					}
					bytecodes[index] = bc;
					break;
				}

				case JBaload0getfield:
					bytecodes[index] = JBaload0;
					break;

				case JBnewdup:
					bytecodes[index] = JBnew;
					break;

				case JBinvokeinterface2:
					bytecodes[index + 0] = CFR_BC_invokeinterface;
					
					entry.key = (void *) ((UDATA) *((U_16 *) &bytecodes[index + 3]));
					if (NULL == (htEntry = hashTableFind(translation.ht, &entry))) { 
						JVMTI_ERROR(JVMTI_ERROR_INTERNAL); 
					}  
#ifdef J9VM_ENV_LITTLE_ENDIAN
					bytecodes[index + 1] = ((U_8 *) (&htEntry->sunCpIndex))[1];
					bytecodes[index + 2] = ((U_8 *) (&htEntry->sunCpIndex))[0];
#else
					bytecodes[index + 1] = ((U_8 *) (&htEntry->sunCpIndex))[0];
					bytecodes[index + 2] = ((U_8 *) (&htEntry->sunCpIndex))[1];
#endif
					bytecodes[index + 3] = 0;
					bytecodes[index + 4] = 0;
					break;

#ifdef J9VM_ENV_LITTLE_ENDIAN
#define FLIP_32BIT(index) \
	do { \
		bytecodes[(index) + 0] = j9Bytecodes[(index) + 3]; \
		bytecodes[(index) + 1] = j9Bytecodes[(index) + 2]; \
		bytecodes[(index) + 2] = j9Bytecodes[(index) + 1]; \
		bytecodes[(index) + 3] = j9Bytecodes[(index) + 0]; \
	} while(0)
#else
#define FLIP_32BIT(index)
#endif

				case JBlookupswitch:
				case JBtableswitch: {
					jint tempIndex = index + (4 - (index & 3));
					UDATA numEntries;
					I_32 low;

					FLIP_32BIT(tempIndex);
					tempIndex += 4;
					low = *((I_32 *) (bytecodes + tempIndex));
					FLIP_32BIT(tempIndex);
					tempIndex += 4;

					if (bc == JBtableswitch) {
						I_32 high = *((I_32 *) (bytecodes + tempIndex));

						FLIP_32BIT(tempIndex);
						tempIndex += 4;
						numEntries = (UDATA) (high - low + 1);
					} else {
						numEntries = ((UDATA) low) * 2;
					}

					while (numEntries-- != 0) {
						FLIP_32BIT(tempIndex);
						tempIndex += 4;
					}

					bytecodeSize = tempIndex - index;
					break;
				}

#ifdef J9VM_ENV_LITTLE_ENDIAN
				case JBgotow:
					FLIP_32BIT(index + 1);
					break;

				case JBiinc:
					/* Size is 3 - avoid the default endian flip case */
					break;

				default:
					/* Any bytecode of size 3 or more which does not have a single word parm first must be added to the switch */
					if (bytecodeSize >= 3) {
						bytecodes[index + 1] = j9Bytecodes[index + 2];
						bytecodes[index + 2] = j9Bytecodes[index + 1];
					}
#endif
#undef FLIP_32BIT
			}

			index += bytecodeSize;
		}

		rv_bytecodes = bytecodes;
		rv_bytecode_count = size;
	}

done:
	jvmtiGetConstantPool_free(PORTLIB, &translation);
	if (rc != JVMTI_ERROR_NONE) {
		j9mem_free_memory(bytecodes);
	}
	if (NULL != bytecode_count_ptr) {
		*bytecode_count_ptr = rv_bytecode_count;
	}
	if (NULL != bytecodes_ptr) {
		*bytecodes_ptr = rv_bytecodes;
	}
	TRACE_JVMTI_RETURN(jvmtiGetBytecodes);
}


jvmtiError JNICALL
jvmtiIsMethodNative(jvmtiEnv* env,
	jmethodID method,
	jboolean* is_native_ptr)
{
	jvmtiError rc;
	J9ROMMethod * romMethod;
	jboolean rv_is_native = JNI_FALSE;

	Trc_JVMTI_jvmtiIsMethodNative_Entry(env);

	ENSURE_PHASE_START_OR_LIVE(env);

	ENSURE_JMETHODID_NON_NULL(method);
	ENSURE_NON_NULL(is_native_ptr);

	romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(((J9JNIMethodID *) method)->method);
	rv_is_native = (romMethod->modifiers & J9AccNative) ? JNI_TRUE : JNI_FALSE;
	rc = JVMTI_ERROR_NONE;

done:
	if (NULL != is_native_ptr) {
		*is_native_ptr = rv_is_native;
	}
	TRACE_JVMTI_RETURN(jvmtiIsMethodNative);
}


jvmtiError JNICALL
jvmtiIsMethodSynthetic(jvmtiEnv* env,
	jmethodID method,
	jboolean* is_synthetic_ptr)
{
	jvmtiError rc;
	J9ROMMethod * romMethod;
	jboolean rv_is_synthetic = JNI_FALSE;

	Trc_JVMTI_jvmtiIsMethodSynthetic_Entry(env);

	ENSURE_PHASE_START_OR_LIVE(env);
	ENSURE_CAPABILITY(env, can_get_synthetic_attribute);

	ENSURE_JMETHODID_NON_NULL(method);
	ENSURE_NON_NULL(is_synthetic_ptr);

	romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(((J9JNIMethodID *) method)->method);
	rv_is_synthetic = (romMethod->modifiers & J9AccSynthetic) ? JNI_TRUE : JNI_FALSE;
	rc = JVMTI_ERROR_NONE;

done:
	if (NULL != is_synthetic_ptr) {
		*is_synthetic_ptr = rv_is_synthetic;
	}
	TRACE_JVMTI_RETURN(jvmtiIsMethodSynthetic);
}


jvmtiError JNICALL
jvmtiIsMethodObsolete(jvmtiEnv* env,
	jmethodID method,
	jboolean* is_obsolete_ptr)
{
	jvmtiError rc;
	jboolean rv_is_obsolete = JNI_FALSE;

	Trc_JVMTI_jvmtiIsMethodObsolete_Entry(env);

	ENSURE_PHASE_START_OR_LIVE(env);

	ENSURE_JMETHODID_NON_NULL(method);
	ENSURE_NON_NULL(is_obsolete_ptr);

	rv_is_obsolete = (J9CLASS_FLAGS(J9_CLASS_FROM_METHOD(((J9JNIMethodID *) method)->method)) & J9AccClassHotSwappedOut) ? JNI_TRUE : JNI_FALSE;
	rc = JVMTI_ERROR_NONE;

done:
	if (NULL != is_obsolete_ptr) {
		*is_obsolete_ptr = rv_is_obsolete;
	}
	TRACE_JVMTI_RETURN(jvmtiIsMethodObsolete);
}


static void
deallocateVariableTable(jvmtiEnv * env, jint size, jvmtiLocalVariableEntry * jvmtiVariableTable)
{
	PORT_ACCESS_FROM_JVMTI(env);

	while (size-- != 0) {
		j9mem_free_memory(jvmtiVariableTable[size].name);
		j9mem_free_memory(jvmtiVariableTable[size].signature);
		j9mem_free_memory(jvmtiVariableTable[size].generic_signature);
	}
	j9mem_free_memory(jvmtiVariableTable);
}

jvmtiError JNICALL
jvmtiSetNativeMethodPrefix(jvmtiEnv* env,
	const char* prefix)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiSetNativeMethodPrefix_Entry(env);

	ENSURE_CAPABILITY(env, can_set_native_method_prefix);

	rc = setNativeMethodPrefixes((J9JVMTIEnv *) env, (prefix == NULL) ? 0 : 1, (char **) &prefix);

done:
	TRACE_JVMTI_RETURN(jvmtiSetNativeMethodPrefix);
}


jvmtiError JNICALL
jvmtiSetNativeMethodPrefixes(jvmtiEnv* env,
	jint prefix_count,
	char** prefixes)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiSetNativeMethodPrefix_Entry(env);

	ENSURE_CAPABILITY(env, can_set_native_method_prefix);

	ENSURE_NON_NEGATIVE(prefix_count);
	ENSURE_NON_NULL(prefixes);

	rc = setNativeMethodPrefixes((J9JVMTIEnv *) env, prefix_count, prefixes);

done:
	TRACE_JVMTI_RETURN(jvmtiSetNativeMethodPrefixes);
}


static jvmtiError
setNativeMethodPrefixes(J9JVMTIEnv * j9env, jint prefix_count, char ** prefixes)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(j9env);
	jint i;
	char * prefixData = NULL;
	PORT_ACCESS_FROM_JAVAVM(vm);

	/* Allocate space for the new prefixes */

	if (prefix_count != 0) {
		UDATA allocSize = 0;

		for (i = 0; i < prefix_count; ++i) {
			size_t prefixLength = strlen(prefixes[i]);

			if (prefixLength != 0) {
				allocSize += (prefixLength + 1);
			}
		}
		if (allocSize != 0) {
			prefixData = j9mem_allocate_memory(allocSize, J9MEM_CATEGORY_JVMTI);
			if (prefixData == NULL) {
				return JVMTI_ERROR_OUT_OF_MEMORY;
			}
		}
	}

	omrthread_monitor_enter(j9env->mutex);

	/* Discard the old prefixes */

	j9mem_free_memory(j9env->prefixes);

	/* Copy the new prefixes in reverse order */

	j9env->prefixes = prefixData;
	if (prefixData == NULL) {
		j9env->prefixCount = 0;
	} else {
		j9env->prefixCount = prefix_count;
		for (i = prefix_count - 1; i >= 0; --i) {
			char * prefix = prefixes[i];
			size_t prefixLength = strlen(prefix);

			if (prefixLength != 0) {
				++prefixLength;
				memcpy(prefixData, prefix, prefixLength);
				prefixData += prefixLength;
			}
		}
	}

	omrthread_monitor_exit(j9env->mutex);

	return JVMTI_ERROR_NONE;
}
