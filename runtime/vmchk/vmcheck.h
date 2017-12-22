/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#ifndef vmcheck_h
#define vmcheck_h

#include "vmchkdbg.h"

#define VMCHECK_PREFIX "<vm check:"
#define VMCHECK_FAILED "    "VMCHECK_PREFIX" FAILED"

/*
 * Note: All functions in this module that have parameters that are pointers
 *       to VM structures expect "remote" pointers when out of process.
 */

void vmchkPrintf(J9JavaVM *vm, const char *format, ...);
IDATA J9VMDllMain(J9JavaVM *vm, IDATA stage, void *reserved);

/* checkclasses.c */
void checkJ9ClassSanity(J9JavaVM *vm);
J9MemorySegment * findSegmentInClassLoaderForAddress(J9ClassLoader *classLoader, U_8 *address);

/* checkmethods.c */
void checkJ9MethodSanity(J9JavaVM *vm);

/* checkromclasses.c */
void checkJ9ROMClassSanity(J9JavaVM *vm);

/* checkthreads.c */
void checkJ9VMThreadSanity(J9JavaVM *vm);

/* checkinterntable.c */
void checkLocalInternTableSanity(J9JavaVM *vm);
BOOLEAN verifyUTF8(J9UTF8 *utf8);

/* checkclconstraints.c */
void checkClassLoadingConstraints(J9JavaVM *vm);

#endif     /* vmcheck_h */

