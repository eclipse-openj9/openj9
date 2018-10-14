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

#ifndef X86_HELPERLINKAGE_INCL
#define X86_HELPERLINKAGE_INCL

#include "codegen/Linkage.hpp"
#include "env/jittypes.h"

namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }
namespace TR { class RegisterDependencyConditions; }

#define IMCOMPLETELINKAGE  "This class is only used to generate call-out sequence but no call-in sequence, so it is not used as a complete linkage."

// This class implements following calling conventions on different platforms:
//     X86-32 Windows and Linux: fastcall
//     X86-64 Windows          : Microsoft x64 calling convention, an extension of fastcall
//     X86-64 Linux            : System V AMD64 ABI
// This initial version has following limitations:
//     1. Floating point parameters are not currently in-support.
//     2. Return value can only be at most pointer size, i.e. DWORD on X86-32 and QWORD on X86-64.
namespace TR {
class X86HelperCallSite
   {
   public:
   X86HelperCallSite(TR::Node* callNode, TR::CodeGenerator* cg) :
      _cg(cg),
      _Node(callNode),
      _ReturnType(callNode->getDataType()),
      _SymRef(callNode->getSymbolReference()),
      _Params(cg->trMemory())
      {}
   X86HelperCallSite(TR::Node* callNode, TR::DataType callReturnType, TR::SymbolReference* callSymRef, TR::CodeGenerator* cg) :
      _cg(cg),
      _Node(callNode),
      _ReturnType(callReturnType),
      _SymRef(callSymRef),
      _Params(cg->trMemory())
      {}
   void AddParam(TR::Register* param)
      {
      TR_ASSERT(param->getKind() == TR_GPR, "HelperCallSite now only support GPR");
      _Params.add(param);
      }
   TR::Register* BuildCall();
   TR::CodeGenerator* cg() const
      {
      return _cg;
      }
   static const uint32_t                 PreservedRegisterMapForGC;
   static const bool                     CalleeCleanup;
   static const bool                     RegisterParameterShadowOnStack;
   static const size_t                   StackSlotSize;
   static const size_t                   NumberOfIntParamRegisters;;
   static const size_t                   StackIndexAdjustment;
   static const TR::RealRegister::RegNum IntParamRegisters[];
   static const TR::RealRegister::RegNum CallerSavedRegisters[];
   static const TR::RealRegister::RegNum CalleeSavedRegisters[];

   private:
   TR::CodeGenerator*      _cg;
   TR::Node*               _Node;
   TR::SymbolReference*    _SymRef;
   TR::DataType            _ReturnType;
   TR_Array<TR::Register*> _Params;
   };

class X86HelperLinkage : public TR::Linkage
   {
   public:
   X86HelperLinkage(TR::CodeGenerator *cg) : TR::Linkage(cg) {}
   const X86LinkageProperties& getProperties() { return _Properties; }
   virtual void createPrologue(TR::Instruction *cursor) { TR_ASSERT(false, IMCOMPLETELINKAGE); }
   virtual void createEpilogue(TR::Instruction *cursor) { TR_ASSERT(false, IMCOMPLETELINKAGE); }
   virtual TR::Register* buildIndirectDispatch(TR::Node *callNode) { TR_ASSERT(false, "Indirect dispatch is not currently supported"); return NULL; }
   virtual TR::Register* buildDirectDispatch(TR::Node* callNode, bool spillFPRegs)
      {
      X86HelperCallSite CallSite(callNode, cg());
      // Evaluate children
      for (int i = 0; i < callNode->getNumChildren(); i++)
         {
         cg()->evaluate(callNode->getChild(i));
         }
      // Setup parameters
      for (int i = callNode->getNumChildren() - 1; i >= 0; i--)
         {
         CallSite.AddParam(callNode->getChild(i)->getRegister());
         }
      // Supply VMThread as the first parameter if necessary
      if (!callNode->getSymbol()->getMethodSymbol()->isSystemLinkageDispatch())
         {
         CallSite.AddParam(cg()->getVMThreadRegister());
         }
      TR::Register* ret = CallSite.BuildCall();
      // Release children
      for (int i = 0; i < callNode->getNumChildren(); i++)
         {
         cg()->decReferenceCount(callNode->getChild(i));
         }
      return ret;
      }

   private:
   X86LinkageProperties _Properties;
};

}
#endif
