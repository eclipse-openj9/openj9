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

#include "z/codegen/J9SystemLinkageLinux.hpp"

#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/GCStackMap.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "compile/Compilation.hpp"
#include "env/CHTable.hpp"
#include "env/CompilerEnv.hpp"
#include "env/VMJ9.h"
#include "env/jittypes.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "runtime/RuntimeAssumptions.hpp"
#include "z/codegen/J9S390PrivateLinkage.hpp"
#include "z/codegen/CallSnippet.hpp"
#include "z/codegen/OpMemToMem.hpp"
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390HelperCallSnippet.hpp"
#include "z/codegen/S390StackCheckFailureSnippet.hpp"

////////////////////////////////////////////////////////////////////////////////
//  TR::J9S390zLinuxSystemLinkage Implementations
////////////////////////////////////////////////////////////////////////////////
TR::J9S390zLinuxSystemLinkage::J9S390zLinuxSystemLinkage(TR::CodeGenerator * codeGen)
   : TR::S390zLinuxSystemLinkage(codeGen)
   {
   }

/////////////////////////////////////////////////////////////////////////////////
// TR::J9S390zLinuxSystemLinkage::generateInstructionsForCall - Front-end
//  customization of callNativeFunction
////////////////////////////////////////////////////////////////////////////////
void
TR::J9S390zLinuxSystemLinkage::generateInstructionsForCall(TR::Node * callNode,
	TR::RegisterDependencyConditions * deps, intptrj_t targetAddress,
		TR::Register * methodAddressReg, TR::Register * javaLitOffsetReg,
		TR::LabelSymbol * returnFromJNICallLabel,
		TR::S390JNICallDataSnippet * jniCallDataSnippet, bool isJNIGCPoint)
   {
   TR::CodeGenerator * codeGen = cg();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(codeGen->fe());
   TR::S390PrivateLinkage * privateLinkage = (TR::S390PrivateLinkage *) cg()->getLinkage(TR_Private);
   TR::Register * javaLitPoolRegister = privateLinkage->getLitPoolRealRegister();
   TR::Register * javaStackRegister = privateLinkage->getStackPointerRealRegister();
   TR::Register * parm3 = deps->searchPreConditionRegister(getIntegerArgumentRegister(2));
   TR::Register * parm4 = deps->searchPreConditionRegister(getIntegerArgumentRegister(3));
   TR::Register * parm5 = deps->searchPreConditionRegister(getIntegerArgumentRegister(4));

   TR::RegisterDependencyConditions * postDeps =
		   new (trHeapMemory()) TR::RegisterDependencyConditions(NULL,
				   deps->getPostConditions(), 0, deps->getAddCursorForPost(), cg());

   TR::Register * systemReturnAddressRegister =
		   deps->searchPostConditionRegister(getReturnAddressRegister());

   bool passLitPoolReg = false;
   if (codeGen->isLiteralPoolOnDemandOn())
      {
      passLitPoolReg = true;
      }

   if (((TR::RealRegister *) javaLitPoolRegister)->getState() != TR::RealRegister::Locked)
      {
      javaLitPoolRegister = deps->searchPostConditionRegister(privateLinkage->getLitPoolRegister());
      }

   // get the address of the function descriptor
   /*
    * for AOT we create a relocation where uint8_t  *_targetAddress is set to callNode->getSymbolReference(),
    * _targetAddress is used in different relocation types, where sometimes it's an actual address and sometimes (eg. TR_HelperAddress) it's a symref
    *  for cases when _targetAddress is actually a symref we cast it back to symref in TR::AheadOfTimeCompile::initializeAOTRelocationHeader
    *
    * In this case
    * generateRegLitRefInstruction creates a ConstantDataSnippet with reloType: TR_HelperAddress, which creates a 32Bit/64Bit ExternalRelocation
    * of type TR_AbsoluteHelperAddress with _targetAddress set to TR::SymbolReference from the call
    *
    */
   if (fej9->helpersNeedRelocation()
		   && !(callNode->getSymbol()->isResolvedMethod()
				   || jniCallDataSnippet))
      {
      generateRegLitRefInstruction(cg(), TR::InstOpCode::getLoadOpCode(), callNode, systemReturnAddressRegister,
    		  (uintptrj_t) callNode->getSymbolReference(), TR_HelperAddress, NULL, NULL, NULL);
	   }
   // get the address of the function descriptor
   else if (callNode->getSymbol()->isResolvedMethod() && jniCallDataSnippet) // unresolved means a helper being called using system linkage
      {
      generateRXInstruction(codeGen, TR::InstOpCode::getLoadOpCode(), callNode,
    		  systemReturnAddressRegister, generateS390MemoryReference(jniCallDataSnippet->getBaseRegister(),
    				  jniCallDataSnippet->getTargetAddressOffset(), codeGen));
      }
   else if (codeGen->needClassAndMethodPointerRelocations()
    		  && callNode->isPreparedForDirectJNI())
      {
      uint32_t reloType;
      if (callNode->getSymbol()->castToResolvedMethodSymbol()->isSpecial())
         reloType = TR_JNISpecialTargetAddress;
      else if (callNode->getSymbol()->castToResolvedMethodSymbol()->isStatic())
         reloType = TR_JNIStaticTargetAddress;
      else if (callNode->getSymbol()->castToResolvedMethodSymbol()->isVirtual())
         reloType = TR_JNIVirtualTargetAddress;
      else
         {
         reloType = TR_NoRelocation;
         TR_ASSERT(0,"JNI relocation not supported.");
         }
      generateRegLitRefInstruction(cg(), TR::InstOpCode::getLoadOpCode(), callNode,
				systemReturnAddressRegister, (uintptrj_t) targetAddress,
				reloType, NULL, NULL, NULL);
	   }
   else
      {
      genLoadAddressConstant(codeGen, callNode, targetAddress,
				systemReturnAddressRegister, NULL, NULL, javaLitPoolRegister);
      }

	//param 3, 4 and 5 are currently in gpr8, gpr9 and gpr10, move them in correct regs( gpr4, gpr5 and gpr6 )
   if (parm3)
      {
      TR::Register * gpr4Reg = deps->searchPostConditionRegister(TR::RealRegister::GPR4);
      generateRRInstruction(codeGen, TR::InstOpCode::getLoadRegOpCode(), callNode, gpr4Reg, parm3);
      }
   if (parm4)
      {
      generateRRInstruction(codeGen, TR::InstOpCode::getLoadRegOpCode(), callNode, javaStackRegister, parm4);
      }
   if (parm5)
      {
      //save litpool reg GPR6
      generateRRInstruction(codeGen, TR::InstOpCode::getLoadRegOpCode(), callNode,
    		  javaLitOffsetReg, javaLitPoolRegister);
      generateRRInstruction(codeGen, TR::InstOpCode::getLoadRegOpCode(), callNode,
    		  javaLitPoolRegister, parm5);
      }
   // call the JNI function
   TR::Instruction * gcPoint = generateRRInstruction(codeGen, TR::InstOpCode::BASR,
		   callNode, systemReturnAddressRegister,
		   systemReturnAddressRegister, deps);

   if (isJNIGCPoint)
      {
      gcPoint->setNeedsGCMap(0x00000000);
      }
   generateS390LabelInstruction(codeGen, TR::InstOpCode::LABEL, callNode, returnFromJNICallLabel);

   if (parm5)
      {
      //restore litpool reg GPR6
      generateRRInstruction(codeGen, TR::InstOpCode::getLoadRegOpCode(), callNode,
    		  javaLitPoolRegister, javaLitOffsetReg);
      }

}

void
TR::J9S390zLinuxSystemLinkage::setupRegisterDepForLinkage(TR::Node * callNode, TR_DispatchType dispatchType,
   TR::RegisterDependencyConditions * &deps, int64_t & killMask, TR::SystemLinkage * systemLinkage,
   TR::Node * &GlobalRegDeps, bool &hasGlRegDeps, TR::Register ** methodAddressReg, TR::Register * &javaLitOffsetReg)
   {
   // call j9 private linkage specialization
   TR::S390PrivateLinkage * privateLinkage = (TR::S390PrivateLinkage *) cg()->getLinkage(TR_Private);
   privateLinkage->setupRegisterDepForLinkage(callNode, dispatchType, deps, killMask, systemLinkage, GlobalRegDeps, hasGlRegDeps, methodAddressReg, javaLitOffsetReg);
   }

void
TR::J9S390zLinuxSystemLinkage::setupBuildArgForLinkage(TR::Node * callNode, TR_DispatchType dispatchType, TR::RegisterDependencyConditions * deps, bool isFastJNI,
      bool isPassReceiver, int64_t & killMask, TR::Node * GlobalRegDeps, bool hasGlRegDeps, TR::SystemLinkage * systemLinkage)
   {
   TR::S390PrivateLinkage * privateLinkage = (TR::S390PrivateLinkage *) cg()->getLinkage(TR_Private);
   privateLinkage->setupBuildArgForLinkage(callNode, dispatchType, deps, isFastJNI, isPassReceiver, killMask, GlobalRegDeps, hasGlRegDeps, systemLinkage);
   }

void
TR::J9S390zLinuxSystemLinkage::performCallNativeFunctionForLinkage(TR::Node * callNode, TR_DispatchType dispatchType, TR::Register * &javaReturnRegister, TR::SystemLinkage * systemLinkage,
      TR::RegisterDependencyConditions * &deps, TR::Register * javaLitOffsetReg, TR::Register * methodAddressReg, bool isJNIGCPoint)
   {
   // call base class implementation first
   OMR::Z::Linkage::performCallNativeFunctionForLinkage(callNode, dispatchType, javaReturnRegister, systemLinkage, deps, javaLitOffsetReg, methodAddressReg, isJNIGCPoint);

   // get javaStack Real Register
   TR::CodeGenerator * codeGen = cg();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(codeGen->fe());
   TR::S390PrivateLinkage * privateLinkage = (TR::S390PrivateLinkage *) cg()->getLinkage(TR_Private);
   TR::RealRegister * javaStackPointerRealRegister = privateLinkage->getStackPointerRealRegister();

   // get methodMetaDataVirtualRegister
   TR::Register * methodMetaDataVirtualRegister = privateLinkage->getMethodMetaDataRealRegister();

   // restore java stack pointer
   generateRXInstruction(codeGen, TR::InstOpCode::getLoadOpCode(), callNode, javaStackPointerRealRegister,
                         new (trHeapMemory()) TR::MemoryReference(methodMetaDataVirtualRegister, (int32_t)fej9->thisThreadGetJavaSPOffset(), codeGen));
   }

void
TR::J9S390zLinuxSystemLinkage::doNotKillSpecialRegsForBuildArgs (TR::Linkage *linkage, bool isFastJNI, int64_t &killMask)
   {
   TR::S390PrivateLinkage * privateLinkage = (TR::S390PrivateLinkage *) cg()->getLinkage(TR_Private);
   privateLinkage->doNotKillSpecialRegsForBuildArgs(linkage, isFastJNI, killMask);
   }

void
TR::J9S390zLinuxSystemLinkage::addSpecialRegDepsForBuildArgs(TR::Node * callNode, TR::RegisterDependencyConditions * dependencies, int32_t& from, int32_t step)
   {
   TR::S390PrivateLinkage * privateLinkage = (TR::S390PrivateLinkage *) cg()->getLinkage(TR_Private);
   privateLinkage->addSpecialRegDepsForBuildArgs(callNode, dependencies, from, step);
   }

int64_t
TR::J9S390zLinuxSystemLinkage::addFECustomizedReturnRegDependency(int64_t killMask, TR::Linkage* linkage, TR::DataType resType, TR::RegisterDependencyConditions * dependencies)
   {
   TR::S390PrivateLinkage * privateLinkage = (TR::S390PrivateLinkage *) cg()->getLinkage(TR_Private);
   killMask = privateLinkage->addFECustomizedReturnRegDependency(killMask, linkage, resType, dependencies);
   return killMask;
   }

int32_t
TR::J9S390zLinuxSystemLinkage::storeExtraEnvRegForBuildArgs(TR::Node * callNode, TR::Linkage* linkage, TR::RegisterDependencyConditions * dependencies,
      bool isFastJNI, int32_t stackOffset, int8_t gprSize, uint32_t &numIntegerArgs)
   {
   TR::S390PrivateLinkage * privateLinkage = (TR::S390PrivateLinkage *) cg()->getLinkage(TR_Private);
   stackOffset = privateLinkage->storeExtraEnvRegForBuildArgs(callNode, linkage, dependencies, isFastJNI, stackOffset, gprSize, numIntegerArgs);
   return stackOffset;
   }
