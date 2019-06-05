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

#include "codegen/Linkage_inlines.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/CodeGenerator.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "codegen/X86Instruction.hpp"
#include "x/codegen/J9LinkageUtils.hpp"
#include "env/VMJ9.h"
#include "codegen/Machine.hpp"
#include "x/codegen/IA32LinkageUtils.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/CompilerEnv.hpp"
#include "env/VMJ9.h"

namespace TR
{

// can be commoned
void J9LinkageUtils::cleanupReturnValue(
      TR::Node *callNode,
      TR::Register *linkageReturnReg,
      TR::Register *targetReg,
      TR::CodeGenerator *cg)
   {
   if (!callNode->getOpCode().isFloatingPoint())
      {
      // Native and JNI methods may not return a full register in some cases so we need to get the declared
      // type so that we sign and zero extend the narrower integer return types properly.
      //
      TR_X86OpCodes op;
      TR::ResolvedMethodSymbol *callSymbol = callNode->getSymbol()->castToResolvedMethodSymbol();
      TR_ResolvedMethod *resolvedMethod = callSymbol->getResolvedMethod();

      bool isUnsigned = resolvedMethod->returnTypeIsUnsigned();
      switch (resolvedMethod->returnType())
         {
         case TR::Int8:
            if (isUnsigned)
               {
               op = TR::Compiler->target.is64Bit() ? MOVZXReg8Reg1 : MOVZXReg4Reg1;
               }
            else
               {
               op = TR::Compiler->target.is64Bit() ? MOVSXReg8Reg1 : MOVSXReg4Reg1;
               }
            break;
         case TR::Int16:
            if (isUnsigned)
               {
               op = TR::Compiler->target.is64Bit() ? MOVZXReg8Reg2 : MOVZXReg4Reg2;
               }
            else
               {
               op = TR::Compiler->target.is64Bit() ? MOVSXReg8Reg2 : MOVSXReg4Reg2;
               }
            break;
         default:
            // TR::Address, TR_[US]Int64, TR_[US]Int32
            //
            op = (linkageReturnReg != targetReg) ? MOVRegReg() : BADIA32Op;
            break;
         }

      if (op != BADIA32Op)
         generateRegRegInstruction(op, callNode, targetReg, linkageReturnReg, cg);
      }
   }

void J9LinkageUtils::switchToMachineCStack(TR::Node *callNode, TR::CodeGenerator *cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR::RealRegister *espReal = cg->machine()->getRealRegister(TR::RealRegister::esp);
   TR::Register *vmThreadReg = cg->getMethodMetaDataRegister();

   // Squirrel Java SP away into VM thread.
   //
   generateMemRegInstruction(SMemReg(),
                             callNode,
                             generateX86MemoryReference(vmThreadReg, fej9->thisThreadGetJavaSPOffset(), cg),
                             espReal,
                             cg);

   // Load machine SP from VM thread.
   //
   generateRegMemInstruction(LRegMem(),
                             callNode,
                             espReal,
                             generateX86MemoryReference(vmThreadReg, fej9->thisThreadGetMachineSPOffset(), cg),
                             cg);
   }

void J9LinkageUtils::switchToJavaStack(TR::Node *callNode, TR::CodeGenerator *cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR::RealRegister *espReal = cg->machine()->getRealRegister(TR::RealRegister::esp);
   TR::Register *vmThreadReg = cg->getMethodMetaDataRegister();

   //  Load up the java sp so we have the callout frame on top of the java stack.
   //
   generateRegMemInstruction(
      LRegMem(),
      callNode,
      espReal,
      generateX86MemoryReference(vmThreadReg, cg->fej9()->thisThreadGetJavaSPOffset(), cg),
      cg);
   }

}
