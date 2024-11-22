/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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

#ifndef TR_AOTCLASSINFO_INCL
#define TR_AOTCLASSINFO_INCL

#include "env/FrontEnd.hpp"
#include "runtime/J9Runtime.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"

class AOTCacheClassChainRecord;


// AOTClassInfo is a structure used to record assumptions on classes made by the
// current compilation.  There are two types of validations: ones based on a
// constant pool entry, and ones that are not (could be recognized method where
// we don't have the cp info or could be based on profile data)
//
// TR_ValidateClass, TR_ValidateStaticField, TR_ValidateInstanceField are all
// based on a constant pool entry
//
// TR_ValidateArbitraryClass is not based on a constant pool entry
//
// First class of validation has _cpIndex >= 0, second class has _cpIndex == -1
//

namespace TR
{

class AOTClassInfo
   {
public:

   TR_ALLOC(TR_Memory::AOTClassInfo)

   AOTClassInfo(
         TR_FrontEnd *fe,
         TR_OpaqueClassBlock *clazz,
         uintptr_t classChainOffset,
         TR_OpaqueMethodBlock *method,
         uint32_t cpIndex,
         TR_ExternalRelocationTargetKind reloKind,
         const AOTCacheClassChainRecord *aotCacheClassChainRecord = NULL
   ) :
      _clazz(clazz),
      _classChainOffset(classChainOffset),
      _method(method),
      _constantPool((void *) ((TR_J9VMBase *)fe)->getConstantPoolFromMethod(method)),
      _cpIndex(cpIndex),
      _reloKind(reloKind)
      {
#if defined(J9VM_OPT_JITSERVER)
      TR_ASSERT(!aotCacheClassChainRecord || (fe->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER),
                "Must always be NULL at JIT client");
      _aotCacheClassChainRecord = aotCacheClassChainRecord;
#endif /* defined(J9VM_OPT_JITSERVER) */
      }

#if defined(J9VM_OPT_JITSERVER)
   const AOTCacheClassChainRecord *getAOTCacheClassChainRecord() const { return _aotCacheClassChainRecord; }
#else /* defined(J9VM_OPT_JITSERVER) */
   const AOTCacheClassChainRecord *getAOTCacheClassChainRecord() const { return NULL; }
#endif /* defined(J9VM_OPT_JITSERVER) */

   TR_ExternalRelocationTargetKind _reloKind;   // identifies validation needed (instance field, static field, class, arbitrary class)
   uint32_t _cpIndex;                           // cpindex identifying the cp entry if known otherwise -1
   TR_OpaqueMethodBlock *_method;               // inlined method owning the cp entry or to which assumption is attached
                                                // _method must be compiled method or some method in the inlined site table
   void *_constantPool;                         // constant pool owning the cp entry, initialized based on _method
   TR_OpaqueClassBlock *_clazz;                 // class on which assumption is formed
   uintptr_t _classChainOffset;                 // class chain offset for clazz: captures the assumption
                                                // == TR_SharedCache::INVALID_CLASS_CHAIN_OFFSET for TR_ValidateStaticField validations
#if defined(J9VM_OPT_JITSERVER)
   const AOTCacheClassChainRecord *_aotCacheClassChainRecord; // NULL at JITServer if compiled method won't be cached
                                                              // Always NULL at JIT client
#endif /* defined(J9VM_OPT_JITSERVER) */
   };

}

#endif
