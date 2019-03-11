/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef HOOK_HELPERS_H
#define HOOK_HELPERS_H

#include <stddef.h>

class TR_FrontEnd;
namespace OMR { struct CodeCacheMethodHeader; }
namespace OMR { struct FaintCacheBlock; }
struct J9ClassLoader;
struct J9JITConfig;
struct J9JITExceptionTable;
struct J9VMThread;

void jitReleaseCodeCollectMetaData(J9JITConfig *jitConfig, J9VMThread *vmThread, J9JITExceptionTable *metaData, OMR::FaintCacheBlock* = 0);
void jitRemoveAllMetaDataForClassLoader(J9VMThread * vmThread, J9ClassLoader * classLoader);
void jitReclaimMarkedAssumptions(bool isEager);
void vlogReclamation(const char *prefix, J9JITExceptionTable *metaData, size_t bytesToSaveAtStart);
void freeFastWalkCache(J9VMThread *vmThread, J9JITExceptionTable *metaData);

// Defined in Runtime.cpp
OMR::CodeCacheMethodHeader *getCodeCacheMethodHeader(char *p, int searchLimit, J9JITExceptionTable* metaData = NULL);

#endif
