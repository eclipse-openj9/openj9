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

#ifndef J9ACCESSBARRIERHELPERS_H
#define J9ACCESSBARRIERHELPERS_H

#if defined (J9VM_ENV_DATA64)
#if (defined(__GNUC__) && (defined(J9HAMMER) || defined(S390)))
/* Forcing non-inlining on GNU for X and Z, where inlining seems to create much register or code cache pressure within Bytecode Interpreter */
__attribute__ ((noinline))
#else /* (defined(__GNUC__) && (defined(J9HAMMER) || defined(S390))) */
VMINLINE
#endif /* (defined(__GNUC__) && (defined(J9HAMMER) || defined(S390))) */
static UDATA j9javaArray_BA(J9VMThread *vmThread, J9IndexableObject *array, UDATA *index, U_8 elementSize)
{
	UDATA baseAddress = (UDATA)array;

	if (J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread)) {
		baseAddress += sizeof(J9IndexableObjectContiguousCompressed);
	} else {
		baseAddress += sizeof(J9IndexableObjectContiguousFull);
	}

	if (J9IndexableObjectLayout_NoDataAddr_NoArraylet == vmThread->indexableObjectLayout) {
		/* Standard GCs: nothing extra to do - just explicitly listed for clarity */
	} else if (J9IndexableObjectLayout_DataAddr_NoArraylet == vmThread->indexableObjectLayout) {
		/* Balanced Offheap; dereference dataAddr that is just after the (base) header */
		baseAddress = *(UDATA *)baseAddress;
	} else {
		/* GCs that may have arraylet (Balanced arraylet or Metronome) - will recalculate baseAddress from scratch */
		if (J9ISCONTIGUOUSARRAY(vmThread, array)) {
			baseAddress = (UDATA)array + vmThread->contiguousIndexableHeaderSize;
		} else {
			fj9object_t *arrayoid = (fj9object_t *)((UDATA)array + vmThread->discontiguousIndexableHeaderSize);
			/* While arrayletLeafSize is UDATA, the result of this division will fit into U_32 (simply because Java can't have more array elements) */
			U_32 elementsPerLeaf = (U_32)(J9VMTHREAD_JAVAVM(vmThread)->arrayletLeafSize / elementSize);
			U_32 leafIndex = ((U_32)*index) / elementsPerLeaf;
			*index = ((U_32)*index) % elementsPerLeaf;

			if (J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread)) {
				U_32 leafToken = *((U_32 *)arrayoid + leafIndex);
				baseAddress = (UDATA)J9_CONVERT_POINTER_FROM_TOKEN__(vmThread, leafToken);
			} else {
				UDATA leafToken = *((UDATA *)arrayoid + leafIndex);
				baseAddress = leafToken;
			}
		}
	}


	return baseAddress;
}

#define J9JAVAARRAY_C_EA(elemType)																					\
VMINLINE static elemType *j9javaArray_##elemType##_EA(J9VMThread *vmThread, J9IndexableObject *array, UDATA index)	\
{																													\
	UDATA baseAddress = j9javaArray_BA(vmThread, array, &index, (U_8)sizeof(elemType));								\
	/* Intentionally inlining this to treat sizeof value as an immediate value */									\
	return (elemType *)(baseAddress + index * sizeof(elemType));													\
} 																													\

/* generate C bodies */

J9JAVAARRAY_C_EA(I_8)
J9JAVAARRAY_C_EA(U_8)
J9JAVAARRAY_C_EA(I_16)
J9JAVAARRAY_C_EA(U_16)
J9JAVAARRAY_C_EA(I_32)
J9JAVAARRAY_C_EA(U_32)
J9JAVAARRAY_C_EA(I_64)
J9JAVAARRAY_C_EA(U_64)
J9JAVAARRAY_C_EA(IDATA)
J9JAVAARRAY_C_EA(UDATA)
#endif /* defined (J9VM_ENV_DATA64) */

/**
 * These helpers could be written as macros (where the body of methods would be wrapped around oval parenthesis, which would mean that the last expression in the block
 * is return value of the block). However, it is not fully supported by ANSI, but only select C compilers, like GNU C: https://gcc.gnu.org/onlinedocs/gcc/Statement-Exprs.html).
 * Therefore, we are using inline methods to implement the macros (in j9accessbarrier.h). The helpers are in separate file to resolve inter-file dependency (for example J9JavaVM
 * is not known type in j9accessbarrier.h).
 */

#if defined(__GNUC__)
/*
 * Source files which include (directly or indirectly) this file
 * but do not use the functions defined below may fail to compile unless the "unused" attribute
 * is applied.
 * Note that Microsoft compilers do not allow this attribute.
 */
VMINLINE static j9object_t
j9javaArrayOfObject_load(J9VMThread *vmThread, J9IndexableObject *array, I_32 index)   __attribute__ ((__unused__));

VMINLINE static j9object_t
j9javaArrayOfObject_load_VM(J9JavaVM *vm, J9IndexableObject *array, I_32 index)  __attribute__ ((__unused__));
#endif /* __GNUC__ */

VMINLINE static j9object_t
j9javaArrayOfObject_load(J9VMThread *vmThread, J9IndexableObject *array, I_32 index)
{
	if (J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread)) {
		U_32 *loadAddress = J9JAVAARRAY_EA(vmThread, array, index, U_32);
		J9OBJECT__PRE_OBJECT_LOAD_ADDRESS(vmThread, array, loadAddress);
		return (j9object_t)J9_CONVERT_POINTER_FROM_TOKEN__(vmThread, *loadAddress);
	} else {
		UDATA *loadAddress = J9JAVAARRAY_EA(vmThread, array, index, UDATA);
		J9OBJECT__PRE_OBJECT_LOAD_ADDRESS(vmThread, array, loadAddress);
		return (j9object_t)*loadAddress;
	}
}

VMINLINE static j9object_t
j9javaArrayOfObject_load_VM(J9JavaVM *vm, J9IndexableObject *array, I_32 index)
{
	if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm)) {
		U_32 *loadAddress = J9JAVAARRAY_EA_VM(vm, array, index, U_32);
		J9OBJECT__PRE_OBJECT_LOAD_ADDRESS_VM(vm, array, loadAddress);
		return (j9object_t)J9_CONVERT_POINTER_FROM_TOKEN_VM__(vm, *loadAddress);
	} else {
		UDATA *loadAddress = J9JAVAARRAY_EA_VM(vm, array, index, UDATA);
		J9OBJECT__PRE_OBJECT_LOAD_ADDRESS_VM(vm, array, loadAddress);
		return (j9object_t)*loadAddress;	
	}
}

#endif /* J9ACCESSBARRIERHELPERS_H */
