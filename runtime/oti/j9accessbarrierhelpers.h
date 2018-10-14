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

#ifndef J9ACCESSBARRIERHELPERS_H
#define J9ACCESSBARRIERHELPERS_H

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
	fj9object_t *loadAddress = J9JAVAARRAY_EA(vmThread, array, index, fj9object_t);
	J9OBJECT__PRE_OBJECT_LOAD_ADDRESS(vmThread, array, loadAddress);
	return (j9object_t)J9_CONVERT_POINTER_FROM_TOKEN__(vmThread, *loadAddress);
}

VMINLINE static j9object_t
j9javaArrayOfObject_load_VM(J9JavaVM *vm, J9IndexableObject *array, I_32 index)
{
	fj9object_t *loadAddress = J9JAVAARRAY_EA_VM(vm, array, index, fj9object_t);
	J9OBJECT__PRE_OBJECT_LOAD_ADDRESS_VM(vm, array, loadAddress);
	return (j9object_t)J9_CONVERT_POINTER_FROM_TOKEN_VM__(vm, *loadAddress);
}

#endif /* J9ACCESSBARRIERHELPERS_H */
