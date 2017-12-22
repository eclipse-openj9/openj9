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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef j9cp_h
#define j9cp_h

/* @ddr_namespace: default */
#include "j9.h"
#include "j9consts.h"

#define J9_AFTER_CLASS(clazz) ((UDATA *) (((J9Class *) (clazz)) + 1))

#define J9_CP_FROM_METHOD(method) ((J9ConstantPool *) ((UDATA)((method)->constantPool) & ~J9_STARTPC_STATUS))
#ifndef J9_CP_FROM_CLASS
#define J9_CP_FROM_CLASS(clazz) ((J9ConstantPool *) (clazz)->ramConstantPool)
#endif
#define J9_CLASS_FROM_CP(cp)	(((J9ConstantPool *) (cp))->ramClass)
#define J9_CLASS_FROM_METHOD(method) J9_CLASS_FROM_CP(J9_CP_FROM_METHOD(method))
#define J9_ROM_CP_FROM_CP(cp) (((J9ConstantPool *) (cp))->romConstantPool)
#define J9_ROM_CP_FROM_ROM_CLASS(romClass) ((J9ROMConstantPoolItem *) (romClass + 1))

#define J9_CP_TYPE(cpShapeDescription, index) \
	(((cpShapeDescription)[(index) / J9_CP_DESCRIPTIONS_PER_U32] >> \
		(((index) % J9_CP_DESCRIPTIONS_PER_U32) * J9_CP_BITS_PER_DESCRIPTION)) & J9_CP_DESCRIPTION_MASK)

#ifdef J9VM_INTERP_HOT_CODE_REPLACEMENT
#define J9_IS_CLASS_OBSOLETE(clazz) (J9CLASS_FLAGS(clazz) & J9AccClassHotSwappedOut)
#else
#define J9_IS_CLASS_OBSOLETE(clazz) (0)
#endif

#define J9_CURRENT_CLASS(clazz) (J9_IS_CLASS_OBSOLETE(clazz) ? (clazz)->arrayClass : (clazz))

#define J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, _clazzObject) ((_clazzObject)? ((J9Class *)J9VMJAVALANGCLASS_VMREF((vmThread), (j9object_t)(_clazzObject))): NULL)
#define J9VM_J9CLASS_FROM_HEAPCLASS_VM(javaVM, _clazzObject) ((_clazzObject)? ((J9Class *)J9VMJAVALANGCLASS_VMREF_VM((javaVM), (j9object_t *)(_clazzObject))): NULL)
#define J9VM_J9CLASS_TO_HEAPCLASS(_clazz) ((j9object_t)((_clazz) ? (_clazz)->classObject: NULL))

/*
 * Fetch the J9Class* from a jclass JNI reference.
 * The reference must not be NULL.
 * The caller must have VM access.
 */ 
#define J9VM_J9CLASS_FROM_JCLASS(vmThread, jclazz) J9VM_J9CLASS_FROM_HEAPCLASS((vmThread), *(j9object_t*)(jclazz))

/*
 * True if the given JLClass instance is fully initialized and known to be a JLClass instance.  This must be called
 * in any code path where heap class might only be allocated but not yet initialized.
 * This differs from J9GC_IS_INITIALIZED_HEAPCLASS in that it uses the correct access barriers which makes it suitable
 * for use from anywhere in the VM.
 */
#define J9VM_IS_INITIALIZED_HEAPCLASS(vmThread, _clazzObject) \
		(\
				(_clazzObject) && \
				(J9OBJECT_CLAZZ(((vmThread)), (_clazzObject)) == J9VMJAVALANGCLASS_OR_NULL((J9VMTHREAD_JAVAVM(vmThread)))) && \
				(0 != ((J9Class *)J9VMJAVALANGCLASS_VMREF((vmThread), (j9object_t)(_clazzObject)))) \
		)

#define J9VM_IS_INITIALIZED_HEAPCLASS_VM(javaVM, _clazzObject) \
		(\
				(_clazzObject) && \
				(J9OBJECT_CLAZZ_VM(((javaVM)), (_clazzObject)) == J9VMJAVALANGCLASS_OR_NULL(javaVM)) && \
				(0 != ((J9Class *)J9VMJAVALANGCLASS_VMREF_VM((javaVM), (j9object_t *)(_clazzObject)))) \
		)

#define J9_NATIVE_METHOD_IS_BOUND(nativeMethod) \
	((nativeMethod)->extra != (void *) J9_STARTPC_NOT_TRANSLATED)

#endif /* j9cp_h */
 

