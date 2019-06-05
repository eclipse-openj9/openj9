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

/**
 * @file
 * @ingroup GC_Base
 */

#if !defined(MODRON_H_)
#define MODRON_H_

/* @ddr_namespace: default */
#include "j9.h"
#include "j9cfg.h"

#include "modronbase.h"
#include "gc_internal.h"

/* required for the J9VMTHREAD_JAVAVM and J9VMJAVALANGCLASS_VMREF macros */
#include "j9cp.h"
 
#define PORT_ACCESS_FROM_ENVIRONMENT(env) PORT_ACCESS_FROM_JAVAVM((J9JavaVM *)env->getLanguageVM())

/**
 * @ingroup GC_Base
 * @name Bit flags expanding the heap
 * @{
 */
#define J9GC_HEAP_EXPAND_FLAGS_MERGEABLE 1
#define J9GC_HEAP_EXPAND_FLAGS_FREE_LIST 2
#define J9GC_HEAP_EXPAND_FLAGS_NO_MINIMUM 4
/** @} */
 
/**
 * @ingroup GC_Base
 * @name Return codes for heap expansion 
 * @{
 */
#define J9GC_HEAP_EXPAND_RESULT_OK 0
#define J9GC_HEAP_EXPAND_RESULT_OUT_OF_MEMORY 1
#define J9GC_HEAP_EXPAND_RESULT_INSUFFICIENT_VMEM 2
/** @} */

/**
 * @ingroup GC_Base
 * @name Field access macros
 * These macros can be used as both L-values and R-values.
 * @{
 */
/* We may eventually want to have separate getter and setter macros for
 * each field, like the regular VM field access macros. The single-macro
 * per field approach was chosen for simplicity.
 * 
 * When using these macros as an R-value, you may need to cast the result,
 * e.g. J9Class *clazz = (J9Class *)J9GC_OBJECT_CLAZZ(object);
 */

#define	J9GC_J9OBJECT_CLAZZ_FLAGS_MASK	((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))
#define	J9GC_J9OBJECT_CLAZZ_ADDRESS_MASK (~((UDATA)J9GC_J9OBJECT_CLAZZ_FLAGS_MASK))

#define J9GC_J9OBJECT_CLAZZ(object) ((J9Class*)((UDATA)((object)->clazz) & J9GC_J9OBJECT_CLAZZ_ADDRESS_MASK))
#define J9GC_J9OBJECT_CLAZZ_WITH_FLAGS(object) ((J9Class*)(UDATA)(object)->clazz)
#define J9GC_J9OBJECT_CLAZZ_WITH_FLAGS_EA(object) (&((object)->clazz))

#define J9GC_J9OBJECT_FLAGS_FROM_CLAZZ(object) ((U_32)((UDATA)((object)->clazz) & J9GC_J9OBJECT_CLAZZ_FLAGS_MASK))

#define J9GC_J9OBJECT_FIRST_HEADER_SLOT(object) (*((j9objectclass_t *)(object)))

/* 
 * NOTE: since these macros use getOmrVM() they only work in-process (not out-of-process).
 * They may not be used in gccheck.
 */
#define J9GC_J9VMJAVALANGREFERENCE_REFERENT(env, object) (*(fj9object_t*)((U_8*)(object) + J9VMJAVALANGREFREFERENCE_REFERENT_OFFSET((J9VMThread*)(env)->getLanguageVMThread())))
#define J9GC_J9VMJAVALANGREFERENCE_QUEUE(env, object) (*(fj9object_t*)((U_8*)(object) + J9VMJAVALANGREFREFERENCE_QUEUE_OFFSET((J9VMThread*)(env)->getLanguageVMThread())))
#define J9GC_J9VMJAVALANGREFERENCE_STATE(env, object) (*(I_32*)((U_8*)(object) + J9VMJAVALANGREFREFERENCE_STATE_OFFSET((J9VMThread*)(env)->getLanguageVMThread())))
#define J9GC_J9VMJAVALANGSOFTREFERENCE_AGE(env, object) (*(I_32*)((U_8*)(object) + J9VMJAVALANGREFSOFTREFERENCE_AGE_OFFSET((J9VMThread*)(env)->getLanguageVMThread())))

#define J9GC_J9OBJECT_FIELD_EA(object, byteOffset) ((fj9object_t*)((U_8 *)(object) + (byteOffset) + sizeof(J9Object)))

#define J9GC_J9CLASSLOADER_CLASSLOADEROBJECT(classLoader) ((j9object_t)(classLoader)->classLoaderObject)
#define J9GC_J9CLASSLOADER_CLASSLOADEROBJECT_EA(classLoader) (&(classLoader)->classLoaderObject)

#define J9GC_CLASS_SHAPE(ramClass)		(J9CLASS_SHAPE(ramClass))
#define J9GC_CLASS_IS_ARRAY(ramClass)	(J9CLASS_IS_ARRAY(ramClass))

#if defined (OMR_GC_COMPRESSED_POINTERS)

extern "C" mm_j9object_t j9gc_objaccess_pointerFromToken(J9VMThread *vmThread, fj9object_t token);
extern "C" fj9object_t j9gc_objaccess_tokenFromPointer(J9VMThread *vmThread, mm_j9object_t object);

#if defined (OMR_GC_FULL_POINTERS)
#define mmPointerFromToken(vmThread, token) (J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread) ? j9gc_objaccess_pointerFromToken((vmThread), (token)) : ((mm_j9object_t)(UDATA)(token)))
#define mmTokenFromPointer(vmThread, token) (J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread) ? j9gc_objaccess_tokenFromPointer((vmThread), (token)) : ((fj9object_t)(object)))
#else /* defined (OMR_GC_FULL_POINTERS) */
#define mmPointerFromToken(vmThread, token) (j9gc_objaccess_pointerFromToken((vmThread), (token) ))
#define mmTokenFromPointer(vmThread, object) (j9gc_objaccess_tokenFromPointer((vmThread), (object) ))
#endif /* defined (OMR_GC_FULL_POINTERS) */

/* The size of the reserved area at the beginning of the compressed pointer heap */
#define J9GC_COMPRESSED_POINTER_NULL_REGION_SIZE 4096

#else /* OMR_GC_COMPRESSED_POINTERS */

#define mmPointerFromToken(vmThread, token) ((mm_j9object_t)(token))
#define mmTokenFromPointer(vmThread, object) ((fj9object_t)(object))

#endif /* OMR_GC_COMPRESSED_POINTERS */

#if defined(J9VM_GC_REALTIME)
/* Note that the "reserved" index is used for 2 different purposes with the
 * sATBBarrierRememberedSet:
 * 1) As a per-thread flag indicating the double barrier is on.
 * 2) As a global flag indicating the barrier is disabled.
 * 
 * If the global or local fragment indexes must be preserved for any other
 * reason and the JIT relies on the (localFragmentIndex == globalFragmentIndex)
 * check, we must ensure that both indexes aren't preserved at the same time.
 */
#define J9GC_REMEMBERED_SET_RESERVED_INDEX 0
#endif /* J9VM_GC_REALTIME */

/*
 * True if the given JLClass instance is fully initialized and known to be a JLClass instance.  This must be called
 * in any code path where heap class might only be allocated but not yet initialized.
 * This differs from J9VM_IS_INITIALIZED_HEAPCLASS in that it goes through the back door to avoid access barriers.
 * It is only to be used from inside the GC.
 */
#define J9GC_IS_INITIALIZED_HEAPCLASS(vmThread, _clazzObject) \
		(\
				(_clazzObject) && \
				(J9GC_J9OBJECT_CLAZZ(_clazzObject) == J9VMJAVALANGCLASS_OR_NULL(J9VMTHREAD_JAVAVM(vmThread))) && \
				(0 != ((J9Class *)J9VMJAVALANGCLASS_VMREF((vmThread), (j9object_t)(_clazzObject)))) \
		)

/** @} */

#endif /* MODRON_H_ */

