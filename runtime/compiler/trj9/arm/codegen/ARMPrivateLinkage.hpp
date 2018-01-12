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

#ifndef ARM_PRIVATELINKAGE_INCL
#define ARM_PRIVATELINKAGE_INCL

#include "codegen/Linkage.hpp"

#include "infra/Assert.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class Register; }

namespace TR {

class ARMPrivateLinkage : public TR::Linkage
   {
   static TR::ARMLinkageProperties properties;

   public:

   ARMPrivateLinkage(TR::CodeGenerator *codeGen) : TR::Linkage(codeGen) {}

   virtual uint32_t getRightToLeft();
   virtual void mapStack(TR::ResolvedMethodSymbol *method);
   virtual void mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t &stackIndex);
   virtual void initARMRealRegisterLinkage();
   virtual void setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method);

   virtual TR::MemoryReference *getOutgoingArgumentMemRef(int32_t               totalParmAreaSize,
                                                            int32_t               argOffset,
                                                            TR::Register          *argReg,
                                                            TR_ARMOpCodes         opCode,
                                                            TR::ARMMemoryArgument &memArg);

   virtual TR::ARMLinkageProperties& getProperties();

   virtual void createPrologue(TR::Instruction *cursor);
   virtual void createEpilogue(TR::Instruction *cursor);

   virtual int32_t buildArgs(TR::Node                            *callNode,
                             TR::RegisterDependencyConditions *dependencies,
                             TR::Register* &vftReg,
                             bool                                isVirtual);

   virtual void buildVirtualDispatch(TR::Node *callNode,
                        TR::RegisterDependencyConditions *dependencies,
                        TR::RegisterDependencyConditions *postDeps,
                        TR::Register                     *vftReg,
                        uint32_t                         sizeOfArguments);

   virtual TR::Register *buildDirectDispatch(TR::Node *callNode);
   virtual TR::Register *buildIndirectDispatch(TR::Node *callNode);
   };

class ARMHelperLinkage : public TR::ARMPrivateLinkage
   {
   public:

   ARMHelperLinkage(TR::CodeGenerator *codeGen) : TR::ARMPrivateLinkage(codeGen) {}

   virtual int32_t buildArgs(TR::Node                            *callNode,
                             TR::RegisterDependencyConditions *dependencies,
                             TR::Register* &vftReg,
                             bool                                isVirtual);
   };

}

#endif
