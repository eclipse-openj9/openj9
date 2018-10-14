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

#ifndef PPC_PRIVATELINKAGE_INCL
#define PPC_PRIVATELINKAGE_INCL

#include "codegen/Linkage.hpp"

#include "infra/Assert.hpp"

class TR_BitVector;
class TR_ResolvedMethod;
namespace TR { class AutomaticSymbol; }
namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class MemoryReference; }
namespace TR { class Node; }
namespace TR { class ParameterSymbol; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class ResolvedMethodSymbol; }

namespace TR {

struct PPCPICItem
   {
   TR_ALLOC(TR_Memory::Linkage);

   PPCPICItem(TR_OpaqueClassBlock *clazz, TR_ResolvedMethod *method, float freq) :
      _clazz(clazz), _method(method), _frequency(freq) {}

   TR_OpaqueClassBlock *_clazz;
   TR_ResolvedMethod *_method;
   float _frequency;
   };


class PPCPrivateLinkage : public TR::Linkage
   {
   public:

   PPCPrivateLinkage(TR::CodeGenerator *cg);

   virtual const TR::PPCLinkageProperties& getProperties();
   virtual uint32_t getRightToLeft();
   virtual bool hasToBeOnStack(TR::ParameterSymbol *parm);
   virtual void mapStack(TR::ResolvedMethodSymbol *method);
   virtual void mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t &stackIndex);
   virtual void initPPCRealRegisterLinkage();
   virtual TR::MemoryReference *getOutgoingArgumentMemRef(int32_t argSize, TR::Register *argReg, TR::InstOpCode::Mnemonic opCode, TR::PPCMemoryArgument &memArg, uint32_t len);
   virtual TR::MemoryReference *getOutgoingArgumentMemRef(int32_t argSize, TR::Register *argReg, TR::InstOpCode::Mnemonic opCode, TR::PPCMemoryArgument &memArg, uint32_t len, const TR::PPCLinkageProperties& properties);
   virtual void setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method);

   virtual void createPrologue(TR::Instruction *cursor);

   virtual void createEpilogue(TR::Instruction *cursor);

   virtual int32_t buildArgs(
         TR::Node *callNode,
         TR::RegisterDependencyConditions *dependencies);

   virtual void buildVirtualDispatch(
         TR::Node *callNode,
         TR::RegisterDependencyConditions *dependencies,
         uint32_t sizeOfArguments);

   protected:

   TR::PPCLinkageProperties _properties;

   int32_t buildPrivateLinkageArgs(
         TR::Node *callNode,
         TR::RegisterDependencyConditions *dependencies,
         TR_LinkageConventions linkage);

   void buildDirectCall(
         TR::Node *callNode,
         TR::SymbolReference *callSymRef,
         TR::RegisterDependencyConditions *dependencies,
         const TR::PPCLinkageProperties &pp,
         int32_t argSize);

   virtual TR::Register *buildDirectDispatch(TR::Node *callNode);

   virtual TR::Register *buildIndirectDispatch(TR::Node *callNode);

   virtual TR::Register *buildalloca(TR::Node *BIFCallNode);
   };


class PPCHelperLinkage : public TR::PPCPrivateLinkage
   {
   public:

   PPCHelperLinkage(TR::CodeGenerator *cg, TR_LinkageConventions helperLinkage) : _helperLinkage(helperLinkage), TR::PPCPrivateLinkage(cg)
      {
      TR_ASSERT(helperLinkage == TR_Helper || helperLinkage == TR_CHelper, "Unexpected helper linkage convention");
      }

   virtual int32_t buildArgs(
         TR::Node *callNode,
         TR::RegisterDependencyConditions *dependencies);
   protected:

   TR_LinkageConventions _helperLinkage;
   };

}

#endif
