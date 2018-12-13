/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#ifndef TR_AOTCLASSINFO_INCL
#define TR_AOTCLASSINFO_INCL

#include "codegen/FrontEnd.hpp"
#include "runtime/J9Runtime.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"


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
         void *classChain,
         TR_OpaqueMethodBlock *method,
         uint32_t cpIndex,
         TR_ExternalRelocationTargetKind
         reloKind) :
      _clazz(clazz),
      _classChain(classChain),
      _method(method),
      _constantPool((void *) ((TR_J9VMBase *)fe)->getConstantPoolFromMethod(method)),
      _cpIndex(cpIndex),
      _reloKind(reloKind)
      {}

   TR_ExternalRelocationTargetKind _reloKind;   // identifies validation needed (instance field, static field, class, arbitrary class)

   TR_OpaqueMethodBlock *_method;               // inlined method owning the cp entry or to which assumption is attached
                                                // _method must be compiled method or some method in the inlined site table
   void *_constantPool;                         // constant pool owning the cp entry, initialized based on _method

   uint32_t _cpIndex;                           // cpindex identifying the cp entry if known otherwise -1
   TR_OpaqueClassBlock *_clazz;                 // class on which assumption is formed
   void *_classChain;                           // class chain for clazz: captures the assumption
                                                // == NULL for TR_ValidateStaticField validations
   };

}
#endif
