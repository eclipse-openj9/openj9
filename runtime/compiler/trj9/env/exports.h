/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef EXPORTS_H
#define EXPORTS_H

#include "j9.h"

#ifdef __cplusplus
extern "C" {
#endif

void JNICALL dbgjit_TrInitialize(J9JavaVM *localVM,
                                 J9PortLibrary *dbgPortLib,
				 void (*dbgPrint)(const char *s, ...),
				 void (*dbgReadMemory)(UDATA remoteAddr, void *localPtr, UDATA size, UDATA *bytesRead),
				 UDATA (*dbgGetExpression)(const char* args),
				 void* (*dbgMalloc)(UDATA size, void *originalAddress),
				 void* (*dbgMallocAndRead)(UDATA size, void *remoteAddress),
				 void (*dbgFree)(void * addr),
				 void* (*dbgFindPatternInRange)(U_8* pattern, UDATA patternLength, UDATA patternAlignment,
				 				U_8* startSearchFrom, UDATA bytesToSearch, UDATA* bytesSearched));
void JNICALL dbgjit_TrPrint(const char *name, void *addr, UDATA argCount, const char* args);

JNIEXPORT jint JNICALL Java_java_lang_invoke_InterfaceHandle_convertITableIndexToVTableIndex
  (JNIEnv *, jclass, jlong, jint, jlong);

JNIEXPORT jlong JNICALL Java_java_lang_invoke_ThunkTuple_initialInvokeExactThunk
  (JNIEnv *, jclass);

void * JNICALL createDebugExtObject(void * comp, void * jitFunctions, void * dxMalloc);

#ifdef __cplusplus
}
#endif

#endif /* EXPORTS_H */
