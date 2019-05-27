/*******************************************************************************
 * Copyright (c) 2010, 2019 IBM Corp. and others
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

#ifndef J9MEMCATEGORIES_H
#define J9MEMCATEGORIES_H

/*
 * This include file includes all the categories from omrmemcategories.h.
 * j9vm/j9memcategories.c organises the whole set into the tree for the JVM.
 */
#include "omrmemcategories.h"

/*
 * To add a new category:
 * - Add a new #define to the end of this list
 * - Add the corresponding entries in VM_Sidecar/j9vm/j9memcategories.c
 *
 * Don't delete old categories or move existing ones.
 * They may be in use by the class library native code.
 * Only add new categories at the end of the range.
 * (The unused categories have moved to OMR and can be re-used.)
 */
#define J9MEM_CATEGORY_JRE 0
#define J9MEM_CATEGORY_UNUSED1 1 /* #define J9MEM_CATEGORY_VM 1 */
#define J9MEM_CATEGORY_CLASSES 2
#define J9MEM_CATEGORY_CLASSES_SHC_CACHE 3
#define J9MEM_CATEGORY_CUDA4J 4
#define J9MEM_CATEGORY_UNUSED5 5 /* #define J9MEM_CATEGORY_MM_RUNTIME_HEAP 7 */
#define J9MEM_CATEGORY_UNUSED6 6 /* #define J9MEM_CATEGORY_THREADS 6 */
#define J9MEM_CATEGORY_UNUSED7 7 /* #define J9MEM_CATEGORY_THREADS_RUNTIME_STACK 7 */
#define J9MEM_CATEGORY_UNUSED8 8 /* #define J9MEM_CATEGORY_THREADS_NATIVE_STACK 8 */
#define J9MEM_CATEGORY_UNUSED9 9 /* #define J9MEM_CATEGORY_TRACE 9 */
#define J9MEM_CATEGORY_UNUSED10 10 /* #define J9MEM_CATEGORY_JIT 10 */
#define J9MEM_CATEGORY_UNUSED11 11 /* #define J9MEM_CATEGORY_JIT_CODE_CACHE 11 */
#define J9MEM_CATEGORY_UNUSED12 12 /* #define J9MEM_CATEGORY_JIT_DATA_CACHE 12 */
#define J9MEM_CATEGORY_HARMONY 13
#define J9MEM_CATEGORY_SUN_MISC_UNSAFE_ALLOCATE 14
#define J9MEM_CATEGORY_VM_JCL 15
#define J9MEM_CATEGORY_CLASS_LIBRARIES 16
#define J9MEM_CATEGORY_JVMTI 17
#define J9MEM_CATEGORY_JVMTI_ALLOCATE 18
#define J9MEM_CATEGORY_JNI 19
#define J9MEM_CATEGORY_SUN_JCL 20
#define J9MEM_CATEGORY_CLASSLIB_IO_MATH_LANG 21
#define J9MEM_CATEGORY_CLASSLIB_ZIP 22
#define J9MEM_CATEGORY_CLASSLIB_WRAPPERS 23
#define J9MEM_CATEGORY_CLASSLIB_WRAPPERS_MALLOC 24
#define J9MEM_CATEGORY_CLASSLIB_WRAPPERS_EBCDIC 25
#define J9MEM_CATEGORY_CLASSLIB_NETWORKING 26
#define J9MEM_CATEGORY_CLASSLIB_NETWORKING_NET 27
#define J9MEM_CATEGORY_CLASSLIB_NETWORKING_NIO 28
#define J9MEM_CATEGORY_CLASSLIB_NETWORKING_RMI 29
#define J9MEM_CATEGORY_CLASSLIB_GUI 30
#define J9MEM_CATEGORY_CLASSLIB_GUI_AWT 31
#define J9MEM_CATEGORY_CLASSLIB_GUI_MAWT 32
#define J9MEM_CATEGORY_CLASSLIB_GUI_JAWT 33
#define J9MEM_CATEGORY_CLASSLIB_GUI_MEDIALIB 34
#define J9MEM_CATEGORY_CLASSLIB_FONT 35
#define J9MEM_CATEGORY_CLASSLIB_SOUND 36
#define J9MEM_CATEGORY_UNUSED37 37
#define J9MEM_CATEGORY_SUN_MISC_UNSAFE_ALLOCATEDBB 38
#define J9MEM_CATEGORY_MODULES 39

#endif /* J9MEMCATEGORIES_H */
