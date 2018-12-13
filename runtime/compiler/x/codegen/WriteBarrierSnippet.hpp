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

#ifndef X86WRITEBARRIERSNIPPET_INCL
#define X86WRITEBARRIERSNIPPET_INCL

#include "x/codegen/HelperCallSnippet.hpp"
#include "codegen/X86Instruction.hpp"

namespace TR {

class X86WriteBarrierSnippet : public TR::X86HelperCallSnippet
   {
   public:

   X86WriteBarrierSnippet(
      TR::CodeGenerator   *cg,
      TR::Node            *node,
      TR::LabelSymbol      *restartlab,
      TR::LabelSymbol      *snippetlab,
      TR::SymbolReference *helper,
      int32_t             helperArgCount,
      TR::RegisterDependencyConditions *deps)
      : TR::X86HelperCallSnippet(cg, node, restartlab, snippetlab, helper),
            _deps(deps), _helperArgCount(helperArgCount) {}

   TR::RealRegister *getDestOwningObjectRegister()
      {
      return cg()->machine()->getRealRegister(_deps->getPostConditions()->getRegisterDependency(0)->getRealRegister());
      }

   TR::RealRegister *getSourceRegister()
      {
      return cg()->machine()->getRealRegister(_deps->getPostConditions()->getRegisterDependency(1)->getRealRegister());
      }

   TR::RealRegister *getDestAddressRegister()
      {
      return cg()->machine()->getRealRegister(_deps->getPostConditions()->getRegisterDependency(2)->getRealRegister());
      }

   int32_t getHelperArgCount() {return _helperArgCount;}

   TR::RegisterDependencyConditions *getDependencies() {return _deps;}

   uint8_t *emitSnippetBody();
   virtual uint8_t *buildArgs(uint8_t *buffer, bool restoreRegs) = 0;
   virtual uint32_t getLength(int32_t estimatedSnippetStart) = 0;

   TR::RegisterDependencyConditions  *_deps;
   int32_t                              _helperArgCount;

   };

class IA32WriteBarrierSnippet : public TR::X86WriteBarrierSnippet
   {
   TR_WriteBarrierKind _wrtBarrierKind;
   public:

   IA32WriteBarrierSnippet(TR::CodeGenerator    *cg,
                           TR::Node             *node,
                           TR::LabelSymbol       *restartlab,
                           TR::LabelSymbol       *snippetlab,
                           TR::SymbolReference  *helper,
                           int32_t              helperArgCount,
                           TR_WriteBarrierKind  wrtBarrierKind,
                           TR::RegisterDependencyConditions  *deps)
      : TR::X86WriteBarrierSnippet(cg, node, restartlab, snippetlab, helper, helperArgCount, deps), _wrtBarrierKind(wrtBarrierKind) {}

   virtual Kind getKind() { return IsWriteBarrier; }
   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   virtual uint8_t *buildArgs(uint8_t *buffer, bool restoreRegs);

   TR_WriteBarrierKind getWriteBarrierKind() { return _wrtBarrierKind; }
   };


class AMD64WriteBarrierSnippet : public TR::X86WriteBarrierSnippet
   {
   public:

   AMD64WriteBarrierSnippet(TR::CodeGenerator   *cg,
                            TR::Node            *node,
                            TR::LabelSymbol      *restartlab,
                            TR::LabelSymbol      *snippetlab,
                            TR::SymbolReference *helper,
                            int32_t             helperArgCount,
                            TR::RegisterDependencyConditions  *deps)
      : TR::X86WriteBarrierSnippet(cg, node, restartlab, snippetlab, helper, helperArgCount, deps){}

   virtual Kind getKind() { return IsWriteBarrierAMD64; }
   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   virtual uint8_t *buildArgs(uint8_t *buffer, bool restoreRegs);

   protected:
   friend class TR_Debug;

   };

#define AMD64_MAX_XCHG_BUFFER_SIZE (12) // = 4 * (Rex, opCode, ModRM)


TR::X86WriteBarrierSnippet *
generateX86WriteBarrierSnippet(
   TR::CodeGenerator   *cg,
   TR::Node            *node,
   TR::LabelSymbol      *restartLabel,
   TR::LabelSymbol      *snippetLabel,
   TR::SymbolReference *helperSymRef,
   int32_t             helperArgCount,
   TR_WriteBarrierKind gcMode,
   TR::RegisterDependencyConditions *conditions);

}

#endif
