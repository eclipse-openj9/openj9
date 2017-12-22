/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
#ifndef classpathcache_h
#define classpathcache_h

#include "j9.h"

#define ID_NOT_FOUND 0x20000
#define FAILED_MATCH_NOTSET 0xFF
#define CPC_ALIGN 4
#define CPC_PAD(n) (((n) % CPC_ALIGN == 0) ? (n) : ((n) + CPC_ALIGN - ((n) % CPC_ALIGN)))
#define MAX_CACHED_CLASSPATHS 300

#ifdef __cplusplus
extern "C" {
#endif

void clearIdentifiedClasspath(J9PortLibrary* portlib, struct J9ClasspathByIDArray* theArray, void* cp);
void registerFailedMatch(J9VMThread* currentThread, struct J9ClasspathByIDArray* theArray, IDATA callerHelperID, IDATA arrayIndex, UDATA indexInCacheHelper, const char* partition, UDATA partitionLen);
UDATA setIdentifiedClasspath(J9VMThread* currentThread, struct J9ClasspathByIDArray** theArrayPtr, IDATA helperID, UDATA itemsAdded, const char* partition, UDATA partitionLen, void* cp);
IDATA getIDForIdentified(J9PortLibrary* portlib, struct J9ClasspathByIDArray* theArray, void* cp, IDATA walkFrom);
UDATA hasMatchFailedBefore(J9VMThread* currentThread, struct J9ClasspathByIDArray* theArray, IDATA callerHelperID, IDATA arrayIndex, UDATA indexInCacheHelper, const char* partition, UDATA partitionLen);
void resetIdentifiedClasspath(struct J9ClasspathByID* toReset, UDATA arrayLength);
void freeIdentifiedClasspathArray(J9PortLibrary* portlib, struct J9ClasspathByIDArray* toFree);
void* getIdentifiedClasspath(J9VMThread* currentThread, struct J9ClasspathByIDArray* theArray, IDATA helperID, UDATA itemsAdded, const char* partition, UDATA partitionLen, void** cpToFree);
struct J9ClasspathByIDArray* initializeIdentifiedClasspathArray(J9PortLibrary* portlib, UDATA elements, const char* partition, UDATA partitionLen, IDATA partitionHash);


#ifdef __cplusplus
}
#endif

#endif     /* classpathcache_h */

