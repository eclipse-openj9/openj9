/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#ifndef J9_H
#define J9_H

/* @ddr_namespace: default */
#include <string.h>

#include "j9cfg.h"
#include "j9comp.h"
#if defined(J9ZOS390)
/* This is the contents of the old zvarmaps.h - delete this once the builder symbols are gone */
#pragma variable( native2JITExitBCTable, NORENT )
#pragma variable( returnFromJ2IBytecodes, NORENT )
#endif /* J9ZOS390 */

#include "j9port.h"
#include "j9argscan.h"
#include "omrthread.h"
#include "j9class.h"
#include "j9pool.h"
#include "j9simplepool.h"
#include "omrhashtable.h"
#include "j9lib.h"
#include "jni.h"
#include "j9trace.h"
#include "omravl.h"
#include "vmhook_internal.h" /* for definition of struct J9JavaVMHookInterface */
#include "jlm.h" /* for definition of struct J9VMJlmDump */
#include "j9cp.h"
#include "j9accessbarrier.h"
#include "j9vmconstantpool.h"
#include "shcflags.h"
#include "j9modifiers_api.h"
#include "j9memcategories.h"
#include "j9segments.h"
#include "bcutil.h"
#include "ute_module.h"
#include "j9consts.h"
#include "omr.h"
#include "temp.h"
#include "j9thread.h"

typedef struct J9JNIRedirectionBlock {
	struct J9JNIRedirectionBlock* next;
	struct J9PortVmemIdentifier vmemID;
	U_8* alloc;
	U_8* end;
} J9JNIRedirectionBlock;

#define J9JNIREDIRECT_BLOCK_SIZE  0x1000

typedef struct J9ClassLoaderWalkState {
   struct J9JavaVM* vm;
   UDATA flags;
   struct J9PoolState classLoaderBlocksWalkState;
} J9ClassLoaderWalkState;

/* include ENVY-generated part of j9.h (which formerly was j9.h) */
#include "j9generated.h"
#include "j9cfg_builder.h"

#if !defined(J9VM_OUT_OF_PROCESS)
#include "j9accessbarrierhelpers.h"
#endif

/*------------------------------------------------------------------
 * AOT version defines
 *------------------------------------------------------------------*/
#define AOT_MAJOR_VERSION 1
#define AOT_MINOR_VERSION 0


/* temporary hack to ensure that we get the right verison of translate MethodHandle */
#define TRANSLATE_METHODHANDLE_TAKES_FLAGS

/* temporary define to allow JIT work to promote and be enabled at the same time as the vm side */
#define NEW_FANIN_INFRA

/* -------------- Add C-level global definitions below this point ------------------ */ 

/*
 * Simultaneously check if the flags field has the sign bit set and the valueOffset is non-zero.
 *
 * In a resolved field, flags will have the J9FieldFlagResolved bit set, thus having a higher
 * value than any valid valueOffset.
 *
 * This avoids the need for a barrier, as this check only succeeds if flags and valueOffset have
 * both been updated. It is crucial that we do not treat a field ref as resolved if only one of
 * the two values has been set (by another thread that is in the middle of a resolve).
 */

#define JNIENV_FROM_J9VMTHREAD(vmThread) ((JNIEnv*)(vmThread))
#define J9VMTHREAD_FROM_JNIENV(env) ((J9VMThread*)(env))

#define J9RAMFIELDREF_IS_RESOLVED(base) ((base)->flags > (base)->valueOffset)

#define J9DDR_DAT_FILE "j9ddr.dat"

/* RAM class shape flags - valid to use these when the underlying ROM class has been unloaded */

#define J9CLASS_SHAPE(ramClass) ((J9CLASS_FLAGS(ramClass) >> J9_JAVA_CLASS_RAM_SHAPE_SHIFT) & OBJECT_HEADER_SHAPE_MASK)
#define J9CLASS_IS_ARRAY(ramClass) ((J9CLASS_FLAGS(ramClass) & J9_JAVA_CLASS_RAM_ARRAY) != 0)
#define J9CLASS_IS_MIXED(ramClass) (((J9CLASS_FLAGS(ramClass) >> J9_JAVA_CLASS_RAM_SHAPE_SHIFT) & OBJECT_HEADER_SHAPE_MASK) == OBJECT_HEADER_SHAPE_MIXED)

#define IS_STRING_COMPRESSION_ENABLED(vmThread) (FALSE != ((vmThread)->javaVM)->strCompEnabled)

#define IS_STRING_COMPRESSION_ENABLED_VM(javaVM) (FALSE != (javaVM)->strCompEnabled)

#define IS_STRING_COMPRESSED(vmThread, object) \
	(IS_STRING_COMPRESSION_ENABLED(vmThread) ? \
		(((I_32) J9VMJAVALANGSTRING_COUNT(vmThread, object)) >= 0) : FALSE)

#define IS_STRING_COMPRESSED_VM(javaVM, object) \
	(IS_STRING_COMPRESSION_ENABLED_VM(javaVM) ? \
		(((I_32) J9VMJAVALANGSTRING_COUNT_VM(javaVM, object)) >= 0) : FALSE)

#define J9VMJAVALANGSTRING_LENGTH(vmThread, object) \
	(IS_STRING_COMPRESSION_ENABLED(vmThread) ? \
		(J9VMJAVALANGSTRING_COUNT(vmThread, object) & 0x7FFFFFFF) : \
		(J9VMJAVALANGSTRING_COUNT(vmThread, object)))

#define J9VMJAVALANGSTRING_LENGTH_VM(javaVM, object) \
	(IS_STRING_COMPRESSION_ENABLED_VM(javaVM) ? \
		(J9VMJAVALANGSTRING_COUNT_VM(javaVM, object) & 0x7FFFFFFF) : \
		(J9VMJAVALANGSTRING_COUNT_VM(javaVM, object)))

/* UTF8 access macros - all access to J9UTF8 fields should be done through these macros */

#define J9UTF8_LENGTH(j9UTF8Address) (((struct J9UTF8 *)(j9UTF8Address))->length)
#define J9UTF8_SET_LENGTH(j9UTF8Address, len) (((struct J9UTF8 *)(j9UTF8Address))->length = (len))
#define J9UTF8_DATA(j9UTF8Address) (((struct J9UTF8 *)(j9UTF8Address))->data)

#define J9UTF8_DATA_EQUALS(data1, length1, data2, length2) ((((length1) == (length2)) && (memcmp((data1), (data2), (length1)) == 0)))
#define J9UTF8_EQUALS(utf1, utf2) (((utf1) == (utf2)) || (J9UTF8_DATA_EQUALS(J9UTF8_DATA(utf1), J9UTF8_LENGTH(utf1), J9UTF8_DATA(utf2), J9UTF8_LENGTH(utf2))))
#define J9UTF8_LITERAL_EQUALS(data1, length1, cString) (J9UTF8_DATA_EQUALS((data1), (length1), (cString), sizeof(cString) - 1))

/*
 * Equivalent to ((int)strlen(string_literal)) when given a literal string.
 * E.g.: LITERAL_STRLEN("lib") == 3
 */
#define LITERAL_STRLEN(string_literal) ((IDATA)(sizeof(string_literal) - 1))

#define ROUND_UP_TO_POWEROF2(value, powerof2) (((value) + ((powerof2) - 1)) & (UDATA)~((powerof2) - 1))
#undef ROUND_DOWN_TO_POWEROF2
#define ROUND_DOWN_TO_POWEROF2(value, powerof2) ((value) & (UDATA)~((powerof2) - 1))

#define J9_REDIRECTED_REFERENCE 1
#define J9_JNI_UNWRAP_REDIRECTED_REFERENCE(ref)  ((*(UDATA*)(ref)) & J9_REDIRECTED_REFERENCE ? **(j9object_t**)(((UDATA)(ref)) - J9_REDIRECTED_REFERENCE) : *(j9object_t*)(ref))

#define VERBOSE_CLASS 1
#define VERBOSE_GC 2
#define VERBOSE_RESERVED 4
#define VERBOSE_DYNLOAD 8
#define VERBOSE_STACK 16
#define VERBOSE_DEBUG 32
#define VERBOSE_INIT 64
#define VERBOSE_RELOCATIONS 128
#define VERBOSE_ROMCLASS 256
#define VERBOSE_STACKTRACE 512
#define VERBOSE_SHUTDOWN 1024
#define VERBOSE_DUMPSIZES	2048

/* Maximum array dimensions, according to the spec for the array bytecodes, is 255 */
#define J9_ARRAY_DIMENSION_LIMIT 255

#define J9FLAGSANDCLASS_FROM_CLASSANDFLAGS(classAndFlags) (((classAndFlags) >> J9_REQUIRED_CLASS_SHIFT) | ((classAndFlags) << (sizeof(UDATA) * 8 - J9_REQUIRED_CLASS_SHIFT)))
#define J9CLASSANDFLAGS_FROM_FLAGSANDCLASS(flagsAndClass) (((flagsAndClass) >> (8 * sizeof(UDATA) - J9_REQUIRED_CLASS_SHIFT)) | ((flagsAndClass) << J9_REQUIRED_CLASS_SHIFT))
#define J9RAMSTATICFIELDREF_CLASS(base) ((J9Class *) ((base)->flagsAndClass << J9_REQUIRED_CLASS_SHIFT))
/* In a resolved J9RAMStaticFieldRef, the high bit of valueOffset is set, so it must be masked when adding valueOffset to the ramStatics address.
 * Note that ~IDATA_MIN has all but the high bit set. */
#define J9STATICADDRESS(flagsAndClass, valueOffset) ((void *) (((UDATA) ((J9Class *) ((flagsAndClass) << J9_REQUIRED_CLASS_SHIFT))->ramStatics) + ((valueOffset) & ~IDATA_MIN)))
#define J9RAMSTATICFIELDREF_VALUEADDRESS(base) ((void *) (((UDATA) J9RAMSTATICFIELDREF_CLASS(base)->ramStatics) + ((base)->valueOffset & ~IDATA_MIN)))
/* Important: We check both valueOffset and flagsAndClass because we do not set them atomically on resolve. */
#define J9RAMSTATICFIELDREF_IS_RESOLVED(base) ((-1 != (base)->valueOffset) && ((base)->flagsAndClass) > 0)

#define J9ROMMETHOD_GET_NAME(romClass, rommethod) J9ROMMETHOD_NAME(rommethod)
#define J9ROMMETHOD_GET_SIGNATURE(romClass, rommethod) J9ROMMETHOD_SIGNATURE(rommethod)

/* From tracehelp.c - prototyped here because the macro is here */

/**
 * Get the trace interface from the VM using GetEnv.
 *
 * @param j9vm the J9JavaVM
 *
 * @return the trace interface, or NULL if GetEnv fails
 */
extern J9_CFUNC UtInterface * getTraceInterfaceFromVM(J9JavaVM *j9vm);

#define J9_UTINTERFACE_FROM_VM(vm) getTraceInterfaceFromVM(vm)

/**
 * @name Port library access
 * @anchor PortAccess
 * Macros for accessing port library.
 * @{
 */
#ifdef USING_VMI
#define PORT_ACCESS_FROM_ENV(jniEnv) \
VMInterface *portPrivateVMI = VMI_GetVMIFromJNIEnv(jniEnv); \
J9PortLibrary *privatePortLibrary = (*portPrivateVMI)->GetPortLibrary(portPrivateVMI)
#define PORT_ACCESS_FROM_JAVAVM(javaVM) \
VMInterface *portPrivateVMI = VMI_GetVMIFromJavaVM(javaVM); \
J9PortLibrary *privatePortLibrary = (*portPrivateVMI)->GetPortLibrary(portPrivateVMI)
#else
#define PORT_ACCESS_FROM_ENV(jniEnv) J9PortLibrary *privatePortLibrary = (J9VMTHREAD_FROM_JNIENV(jniEnv))->javaVM->portLibrary
#define PORT_ACCESS_FROM_JAVAVM(javaVM) J9PortLibrary *privatePortLibrary = (javaVM)->portLibrary
#define PORT_ACCESS_FROM_VMC(vmContext) J9PortLibrary *privatePortLibrary = (vmContext)->javaVM->portLibrary
#define PORT_ACCESS_FROM_GINFO(javaVM) J9PortLibrary *privatePortLibrary = (javaVM)->portLibrary
#define PORT_ACCESS_FROM_JITCONFIG(jitConfig) J9PortLibrary *privatePortLibrary = (jitConfig)->javaVM->portLibrary
#define PORT_ACCESS_FROM_WALKSTATE(walkState) J9PortLibrary *privatePortLibrary = (walkState)->walkThread->javaVM->portLibrary
#define OMRPORT_ACCESS_FROM_J9VMTHREAD(vmContext) OMRPORT_ACCESS_FROM_J9PORT((vmContext)->javaVM->portLibrary)
#endif
#define PORT_ACCESS_FROM_VMI(vmi) J9PortLibrary *privatePortLibrary = (*vmi)->GetPortLibrary(vmi)
/** @} */

#define JNI_NATIVE_CALLED_FAST(env) (J9_ARE_ANY_BITS_SET(J9VMTHREAD_FROM_JNIENV(env)->publicFlags, J9_PUBLIC_FLAGS_VM_ACCESS))

#define J9_DECLARE_CONSTANT_UTF8(instanceName, name) \
static const struct { \
	U_16 length; \
	U_8 data[sizeof(name)]; \
} instanceName = { sizeof(name) - 1, name }

#define J9_DECLARE_CONSTANT_NAS(instanceName, name, sig) const J9NameAndSignature instanceName = { (J9UTF8*)&name, (J9UTF8*)&sig }

#if defined(J9ZOS390) || (defined(LINUXPPC64) && defined(J9VM_ENV_LITTLE_ENDIAN))
#define J9_EXTERN_BUILDER_SYMBOL(name) extern void name()
#else
#define J9_EXTERN_BUILDER_SYMBOL(name) extern void* name
#endif
#define J9_BUILDER_SYMBOL(name) TOC_UNWRAP_ADDRESS(&name)

#define J9JAVASTACK_STACKOVERFLOWMARK(stack) ((UDATA*)(((UDATA)(stack)->end) - (stack)->size))

#define J9VM_PACKAGE_NAME_BUFFER_LENGTH 128

#if defined(J9VM_ARCH_X86)
/* x86 - one slot on the end of J9VMThread used to save the machineSP */
#define J9_VMTHREAD_SEGREGATED_ALLOCATION_CACHE_OFFSET (sizeof(J9VMThread) + sizeof(UDATA))
#else /* J9VM_ARCH_X86 */
/* Not x86 - no extra space required */
#define J9_VMTHREAD_SEGREGATED_ALLOCATION_CACHE_OFFSET sizeof(J9VMThread)
#endif /* J9VM_ARCH_X86 */

#if !defined(J9VM_INTERP_ATOMIC_FREE_JNI)
#define internalEnterVMFromJNI internalAcquireVMAccess
#define internalExitVMToJNI internalReleaseVMAccess
#endif /* !J9VM_INTERP_ATOMIC_FREE_JNI */

/* Move marcro MAXIMUM_HEAP_SIZE_RECOMMENDED_FOR_xxx from redirector.c in order to be referenced from IBM i series */
#define MAXIMUM_HEAP_SIZE_RECOMMENDED_FOR_COMPRESSEDREFS	((U_64)57 * 1024 * 1024 * 1024)
#define MAXIMUM_HEAP_SIZE_RECOMMENDED_FOR_3BIT_SHIFT_COMPRESSEDREFS	((U_64)25 * 1024 * 1024 * 1024)

#define J9_IS_J9MODULE_UNNAMED(vm, module) ((NULL == module) || (module == vm->unamedModuleForSystemLoader))

#define J9_IS_J9MODULE_OPEN(module) (TRUE == module->isOpen)

#define J9_ARE_MODULES_ENABLED(vm) (J2SE_VERSION(vm) >= J2SE_19)

/* Macrco for VM internalVMFunctions */
#if defined(J9_INTERNAL_TO_VM)
#define J9_VM_FUNCTION(currentThread, function) function
#define J9_VM_FUNCTION_VIA_JAVAVM(javaVM, function) function
#else /* J9_INTERNAL_TO_VM */
#define J9_VM_FUNCTION(currentThread, function) ((currentThread)->javaVM->internalVMFunctions->function)
#define J9_VM_FUNCTION_VIA_JAVAVM(javaVM, function) ((javaVM)->internalVMFunctions->function)
#endif /* J9_INTERNAL_TO_VM */

#endif /* J9_H */
