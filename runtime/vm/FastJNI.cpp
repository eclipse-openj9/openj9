/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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

#include "j9.h"
#include "j9protos.h"
#include "j9consts.h"
#include "j9cp.h"
#include "rommeth.h"
#include "vm_api.h"
#include "fastJNI.h"
#include "ut_j9vm.h"
#include "jvmtiInternal.h"

#undef DEBUG

extern "C" {

/* Native tables */

J9_FAST_JNI_METHOD_TABLE_EXTERN(java_lang_J9VMInternals);
J9_FAST_JNI_METHOD_TABLE_EXTERN(java_lang_Class);
J9_FAST_JNI_METHOD_TABLE_EXTERN(java_lang_Object);
J9_FAST_JNI_METHOD_TABLE_EXTERN(java_lang_System);
J9_FAST_JNI_METHOD_TABLE_EXTERN(com_ibm_oti_vm_VM);
J9_FAST_JNI_METHOD_TABLE_EXTERN(sun_misc_Unsafe);
J9_FAST_JNI_METHOD_TABLE_EXTERN(java_lang_ClassLoader);
J9_FAST_JNI_METHOD_TABLE_EXTERN(java_lang_ref_Reference);
J9_FAST_JNI_METHOD_TABLE_EXTERN(java_lang_Thread);
J9_FAST_JNI_METHOD_TABLE_EXTERN(java_lang_String);
J9_FAST_JNI_METHOD_TABLE_EXTERN(java_lang_reflect_Array);
J9_FAST_JNI_METHOD_TABLE_EXTERN(java_lang_Throwable);
J9_FAST_JNI_METHOD_TABLE_EXTERN(java_lang_invoke_MethodHandle);

J9_FAST_JNI_CLASS_TABLE(fastJNINatives)
	J9_FAST_JNI_CLASS("java/lang/J9VMInternals", java_lang_J9VMInternals)
	J9_FAST_JNI_CLASS("java/lang/Class", java_lang_Class)
	J9_FAST_JNI_CLASS("java/lang/Object", java_lang_Object)
	J9_FAST_JNI_CLASS("java/lang/System", java_lang_System)
	J9_FAST_JNI_CLASS("com/ibm/oti/vm/VM", com_ibm_oti_vm_VM)
	J9_FAST_JNI_CLASS("sun/misc/Unsafe", sun_misc_Unsafe)
	J9_FAST_JNI_CLASS("java/lang/ClassLoader", java_lang_ClassLoader)
	J9_FAST_JNI_CLASS("java/lang/ref/Reference", java_lang_ref_Reference)
	J9_FAST_JNI_CLASS("java/lang/Thread", java_lang_Thread)
	J9_FAST_JNI_CLASS("java/lang/String", java_lang_String)
	J9_FAST_JNI_CLASS("java/lang/reflect/Array", java_lang_reflect_Array)
	J9_FAST_JNI_CLASS("java/lang/Throwable", java_lang_Throwable)
	J9_FAST_JNI_CLASS("java/lang/invoke/MethodHandle", java_lang_invoke_MethodHandle)
J9_FAST_JNI_CLASS_TABLE_END

static bool
removePrefixes(J9JVMTIData *jvmtiData, U_8 **methodNameData, UDATA *methodNameLength, const char *entryData, UDATA entryLength, UDATA retransformFlag)
{
	bool match = false;
	J9JVMTIEnv *j9env = NULL;
	U_8 *localData = *methodNameData;
	UDATA localLength = *methodNameLength;

	JVMTI_ENVIRONMENTS_REVERSE_DO(jvmtiData, j9env) {
		if ((j9env->flags & J9JVMTIENV_FLAG_RETRANSFORM_CAPABLE) == retransformFlag) {
			jint prefixCount = j9env->prefixCount;
			char *prefix = j9env->prefixes;
	
			/* Remove the prefixes */
			while (0 != prefixCount) {
				size_t prefixLength = strlen(prefix);
				if (prefixLength < localLength) {
					if (0 == memcmp(localData, prefix, prefixLength)) {
						/* Prefix found at the beginning of the name - remove it */
						localData += prefixLength;
						localLength -= prefixLength;
						if ((entryLength == localLength) && (0 == memcmp(localData, entryData, entryLength))) {
							match = true;
							goto done;
						}
					}
				}
				prefix += (prefixLength + 1);
				prefixCount -= 1;
			}
		}
		JVMTI_ENVIRONMENTS_REVERSE_NEXT_DO(jvmtiData, j9env);
	}
done:
	*methodNameData = localData;
	*methodNameLength = localLength;
	return match;
}

/* Note that INL natives which have a fast JNI equivalent are currently not submitted for individual compilation
 * (so-called JNI thunks) - they are only accessible via inlining of the JNI call into the caller.  The upshot of
 * this is that any such native which is called virtually (via the vTable) will not get the fast JNI optimization.
 * Before adding a native like this, consult your team leader.
 */

void*
jniNativeMethodProperties(J9VMThread *currentThread, J9Method *jniNativeMethod, UDATA *properties)
{
	UDATA flags = 0;
	void *address = NULL;
	J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(jniNativeMethod);
	/* Don't look up properties for non-native methods */
	if (romMethod->modifiers & J9AccNative) {
		J9FastJNINativeClassDescriptor *classDescriptor = fastJNINatives;
		J9UTF8 *className = J9ROMCLASS_CLASSNAME(J9_CLASS_FROM_METHOD(jniNativeMethod)->romClass);
		UDATA classNameLength = J9UTF8_LENGTH(className);
		U_8 *classNameData = J9UTF8_DATA(className);

		/* Search the table for the class */

		while (NULL != classDescriptor->className) {
			if (J9UTF8_DATA_EQUALS(classNameData, classNameLength, classDescriptor->className, classDescriptor->classNameLength)) {
				/* Class found, search the sub-table for the method */
				J9UTF8 *methodName = J9ROMMETHOD_NAME(romMethod);
				UDATA methodNameLength = J9UTF8_LENGTH(methodName);
				U_8 *methodNameData = J9UTF8_DATA(methodName);
				J9UTF8 *methodSignature = J9ROMMETHOD_SIGNATURE(romMethod);
				UDATA methodSignatureLength = J9UTF8_LENGTH(methodSignature);
				U_8 *methodSignatureData = J9UTF8_DATA(methodSignature);
				J9FastJNINativeMethodDescriptor *methodDescriptor = classDescriptor->natives;

				/* Search the class table for the method*/
				while (NULL != methodDescriptor->methodName) {
					if (J9UTF8_DATA_EQUALS(methodSignatureData, methodSignatureLength, methodDescriptor->methodSignature, methodDescriptor->methodSignatureLength)) {
						const char *entryData = methodDescriptor->methodName;
						UDATA entryLength = methodDescriptor->methodNameLength;
						if (J9UTF8_DATA_EQUALS(methodNameData, methodNameLength, entryData, entryLength)) {
found:
							address = methodDescriptor->function;
							flags = methodDescriptor->flags;
							Trc_VM_fastJNINativeFound(
									currentThread, jniNativeMethod,
									classNameLength, classNameData,
									methodNameLength, methodNameData,
									methodSignatureLength, methodSignatureData,
									flags, address);
#if defined(DEBUG)
							{
								PORT_ACCESS_FROM_VMC(currentThread);
								j9tty_printf(PORTLIB, "Fast JNI found: %.*s.%.*s%.*s flags %p function %p\n", classNameLength, classNameData, methodNameLength, methodNameData, methodSignatureLength, methodSignatureData, flags, address);
							}
#endif /* DEBUG */
							/* All fast JNI functions must use at least one optimization */
							Assert_VM_false(0 == flags);
							/* If the native is not a GC point, it must retain VM access */
							if (flags & J9_FAST_JNI_NOT_GC_POINT) {
								Assert_VM_true(flags & J9_FAST_JNI_RETAIN_VM_ACCESS);
							}
							/* If no native method frame is required, the native must not be a GC point */
							if (flags & J9_FAST_JNI_NO_NATIVE_METHOD_FRAME) {
								Assert_VM_true(flags & J9_FAST_JNI_NOT_GC_POINT);
							}
							/* If exceptions are thrown, this native must be a GC point and must do special teardown */
							if (0 == (flags & J9_FAST_JNI_NO_EXCEPTION_THROW)) {
								Assert_VM_true(0 == (flags & (J9_FAST_JNI_NOT_GC_POINT | J9_FAST_JNI_NO_SPECIAL_TEAR_DOWN)));
							}
							/* If special tear down is required, the frame build is required */
							if (0 == (flags & J9_FAST_JNI_NO_SPECIAL_TEAR_DOWN)) {
								Assert_VM_true(0 == (flags & J9_FAST_JNI_NO_NATIVE_METHOD_FRAME));
							}
							/* If JNI refs are used, the frame build is required */
							if (0 == (flags & J9_FAST_JNI_DO_NOT_WRAP_OBJECTS)) {
								Assert_VM_true(0 == (flags & J9_FAST_JNI_NO_NATIVE_METHOD_FRAME));
							}
							goto done;
						}
						/* Strip JVMTI native method prefixes and continue looking for the name */
						J9JVMTIData *jvmtiData = (J9JVMTIData*)currentThread->javaVM->jvmtiData;
						if (NULL != jvmtiData) {
							U_8 *tempData = methodNameData;
							UDATA tempLength = methodNameLength;
							/* Remove prefixes for retransform-capable agents */
							if (removePrefixes(jvmtiData, &tempData, &tempLength, entryData, entryLength, J9JVMTIENV_FLAG_RETRANSFORM_CAPABLE)) {
								goto found;
							}
							/* Remove prefixes for retransform-incapable agents */
							if (removePrefixes(jvmtiData, &tempData, &tempLength, entryData, entryLength, 0)) {
								goto found;
							}
						}
					}
					methodDescriptor += 1;
				}
			}
			classDescriptor	+= 1;
		}

		/* If the method has no fast JNI optimizations and is a bound JNI native, return the bound address */

		if ((0 == flags) && (0 != (((UDATA)jniNativeMethod->constantPool) & J9_STARTPC_JNI_NATIVE))) {
			address = jniNativeMethod->extra;
#if defined(DEBUG)
			{
				PORT_ACCESS_FROM_VMC(currentThread);
				J9UTF8 *methodName = J9ROMMETHOD_NAME(romMethod);
				UDATA methodNameLength = J9UTF8_LENGTH(methodName);
				U_8 *methodNameData = J9UTF8_DATA(methodName);
				J9UTF8 *methodSignature = J9ROMMETHOD_SIGNATURE(romMethod);
				UDATA methodSignatureLength = J9UTF8_LENGTH(methodSignature);
				U_8 *methodSignatureData = J9UTF8_DATA(methodSignature);
				j9tty_printf(PORTLIB, "Normal JNI found: %.*s.%.*s%.*s function %p\n", classNameLength, classNameData, methodNameLength, methodNameData, methodSignatureLength, methodSignatureData, address);
			}
#endif /* DEBUG */
		}
	}
done:
	*properties = flags;
	return address;
}

} /* extern "C" */
