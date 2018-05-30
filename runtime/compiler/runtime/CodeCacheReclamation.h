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

#ifndef __CODECACHERECLAMATION_H
#define __CODECACHERECLAMATION_H

#ifdef __cplusplus
extern "C" {
#endif
   typedef void JIT_GC_END_HOOK(J9VMThread *vmContext);
   U_8* jitAllocateCodeMemory(J9JITConfig* jitConfig, uint32_t numBytes, uint32_t headRoom);
   UDATA releaseCodeFunc(J9VMThread * currentThread, J9StackWalkState * walkState);
   void jitReleaseCodeMemory(TR_FrontEnd * javaVM,void *startPC, int32_t startOffset, int32_t endOffset);
   void jitCodeHook(J9VMThread *vmContext);
   void jitReleaseCodeHookGlobalGCEnd(J9VMThread *vmContext);
   void jitReleaseCodeHookLocalGCEnd(J9VMThread *vmContext);
#ifdef __cplusplus
}
#endif
#endif
