/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include "z/codegen/J9SystemLinkagezOS.hpp"

#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/GCStackMap.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/S390PrivateLinkage.hpp"
#include "compile/Compilation.hpp"
#include "env/CHTable.hpp"
#include "env/CompilerEnv.hpp"
#include "env/VMJ9.h"
#include "env/jittypes.h"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "runtime/Runtime.hpp"
#include "runtime/RuntimeAssumptions.hpp"
#include "z/codegen/CallSnippet.hpp"
#include "z/codegen/OpMemToMem.hpp"
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390HelperCallSnippet.hpp"
#include "z/codegen/S390J9CallSnippet.hpp"
#include "z/codegen/S390StackCheckFailureSnippet.hpp"

////////////////////////////////////////////////////////////////////////////////
//  J9::Z::zOSSystemLinkage Implementations
////////////////////////////////////////////////////////////////////////////////
J9::Z::zOSSystemLinkage::zOSSystemLinkage(TR::CodeGenerator * cg)
   : TR::S390zOSSystemLinkage(cg)
   {
   }

/////////////////////////////////////////////////////////////////////////////////
// J9::Z::zOSSystemLinkage::generateInstructionsForCall - Front-end
//  customization of callNativeFunction
////////////////////////////////////////////////////////////////////////////////
void
J9::Z::zOSSystemLinkage::generateInstructionsForCall(TR::Node * callNode, TR::RegisterDependencyConditions * deps, intptr_t targetAddress,
      TR::Register * methodAddressReg, TR::Register * javaLitOffsetReg, TR::LabelSymbol * returnFromJNICallLabel,
      TR::Snippet * callDataSnippet, bool isJNIGCPoint)
   {
   TR::S390JNICallDataSnippet * jniCallDataSnippet = static_cast<TR::S390JNICallDataSnippet *>(callDataSnippet);
   TR::CodeGenerator * codeGen = cg();
   TR::Compilation *comp = codeGen->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(codeGen->fe());
   // privateLinkage refers to linkage of caller
   J9::Z::PrivateLinkage * privateLinkage = static_cast<J9::Z::PrivateLinkage *>(cg()->getLinkage(TR_Private));

   TR::Instruction * gcPoint;
   TR::Register * javaStackRegister = privateLinkage->getStackPointerRealRegister();
   TR::Register * systemStackRegister = deps->searchPreConditionRegister(getNormalStackPointerRegister());

   if (systemStackRegister == NULL || (!cg()->supportsJITFreeSystemStackPointer()))
      systemStackRegister = getNormalStackPointerRealRegister();

   TR::Register * systemCAARegister = deps->searchPostConditionRegister(getCAAPointerRegister());
   TR::Register * systemEntryPointRegister = deps->searchPostConditionRegister(getEntryPointRegister());
   TR::Register * systemReturnAddressRegister = deps->searchPostConditionRegister(getReturnAddressRegister());
   TR::Register * systemEnvironmentRegister = javaStackRegister;
   TR::Register * javaLitPoolRegister = privateLinkage->getLitPoolRealRegister();
   bool passLitPoolReg = false;

   if (codeGen->isLiteralPoolOnDemandOn())
      {
      passLitPoolReg = true;
      }

   if (((TR::RealRegister *)javaLitPoolRegister)->getState() != TR::RealRegister::Locked)
      {
      javaLitPoolRegister = deps->searchPostConditionRegister(privateLinkage->getLitPoolRegister());
      }

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
   // get the address of the function descriptor
   if (fej9->needRelocationsForHelpers()
         && !(callNode->getSymbol()->isResolvedMethod() || jniCallDataSnippet))
      {
      generateRegLitRefInstruction(cg(), TR::InstOpCode::getLoadOpCode(), callNode, methodAddressReg, (uintptr_t) callNode->getSymbolReference(),
      TR_HelperAddress, NULL, NULL, NULL);
      }
   else if (!callNode->getSymbol()->isResolvedMethod()  || !jniCallDataSnippet) // An unresolved method means a helper being called  using system linkage
      {
      if (codeGen->needClassAndMethodPointerRelocations() && callNode->isPreparedForDirectJNI())
         {
         TR_ExternalRelocationTargetKind reloType;
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
         generateRegLitRefInstruction(cg(), TR::InstOpCode::getLoadOpCode(), callNode, methodAddressReg, (uintptr_t) targetAddress, reloType, NULL, NULL, NULL);
         }
      else
         genLoadAddressConstant(codeGen, callNode, targetAddress, methodAddressReg, NULL, NULL, javaLitPoolRegister);
      }
   else
      {
      generateRXInstruction(codeGen, TR::InstOpCode::getLoadOpCode(), callNode, methodAddressReg,
                            generateS390MemoryReference(jniCallDataSnippet->getBaseRegister(),
                                                        jniCallDataSnippet->getTargetAddressOffset(), codeGen));
      }

   //save litpool reg GPR6
   generateRRInstruction(codeGen, TR::InstOpCode::getLoadRegOpCode(), callNode, javaLitOffsetReg, systemEntryPointRegister);

   if (comp->target().is64Bit())
      {
      //Load Environment Pointer in R5 and entry point of the JNI function in R6
      generateRSInstruction(codeGen, TR::InstOpCode::getLoadMultipleOpCode(), callNode, systemEnvironmentRegister,
               systemEntryPointRegister, generateS390MemoryReference(methodAddressReg, 0, codeGen));
      }
   else
      {
      int32_t J9TR_CAA_save_offset = fej9->getCAASaveOffset();
      generateRXInstruction(codeGen, TR::InstOpCode::getLoadOpCode(), callNode, systemCAARegister,
               generateS390MemoryReference(systemStackRegister, J9TR_CAA_save_offset, codeGen));
      //Load Environment Pointer in R5 and entry point of the JNI function in R6
      generateRSInstruction(codeGen, TR::InstOpCode::getLoadMultipleOpCode(), callNode, systemEnvironmentRegister,
               systemEntryPointRegister, generateS390MemoryReference(methodAddressReg, 16, codeGen));
      }
   // call the JNI function
   TR::Register * methodMetaDataVirtualRegister = privateLinkage->getMethodMetaDataRealRegister();

   if (cg()->supportsJITFreeSystemStackPointer())
      {
      auto* systemSPOffsetMR = generateS390MemoryReference(methodMetaDataVirtualRegister, static_cast<int32_t>(fej9->thisThreadGetSystemSPOffset()), codeGen);
      generateSILInstruction(codeGen, TR::InstOpCode::getMoveHalfWordImmOpCode(), callNode, systemSPOffsetMR, 0);
      }

   /**
    * NOP padding is needed because returning from XPLINK functions skips the XPLink eyecatcher and
    * always return to a point that's 2 or 4 bytes after the return address.
    *
    * In 64 bit XPLINK, the caller returns with a 'branch relative on condition' instruction with a 2 byte offset:
    *
    *   0x47F07002                    B        2(,r7)
    *
    * In 31-bit XPLINK, this offset is 4-byte.
    *
    * As a result of this, JIT'ed code that does XPLINK calls needs 2 or 4-byte NOP paddings to ensure entry to valid instruction.
    *
    * The BASR and NOP padding must stick together and can't have reverse spills in the middle.
    * Hence, splitting the dependencies to avoid spill instructions.
    */
   TR::RegisterDependencyConditions* callPreDeps = new (self()->trHeapMemory()) TR::RegisterDependencyConditions(deps->getPreConditions(), NULL, deps->getAddCursorForPre(), 0, codeGen);
   TR::RegisterDependencyConditions* callPostDeps = new (self()->trHeapMemory()) TR::RegisterDependencyConditions(NULL, deps->getPostConditions(), 0, deps->getAddCursorForPost(), codeGen);

   gcPoint = generateRRInstruction(codeGen, TR::InstOpCode::BASR, callNode, systemReturnAddressRegister, systemEntryPointRegister, callPreDeps);
   if (isJNIGCPoint)
         gcPoint->setNeedsGCMap(0x00000000);

   TR::Instruction * cursor = generateS390LabelInstruction(codeGen, TR::InstOpCode::LABEL, callNode, returnFromJNICallLabel);

   cursor = genCallNOPAndDescriptor(cursor, callNode, callNode, TR_XPLinkCallType_BASR);
   cursor->setDependencyConditions(callPostDeps);

   if (cg()->supportsJITFreeSystemStackPointer())
      {
      generateRXInstruction(codeGen, TR::InstOpCode::getStoreOpCode(), callNode, systemStackRegister,
         new (trHeapMemory()) TR::MemoryReference(methodMetaDataVirtualRegister, (int32_t)fej9->thisThreadGetSystemSPOffset(), codeGen));
      if (getStackPointerRealRegister()->getState() != TR::RealRegister::Locked)
         codeGen->stopUsingRegister(systemStackRegister);
      }

   //restore litpool reg GPR6
   generateRRInstruction(codeGen, TR::InstOpCode::getLoadRegOpCode(), callNode, systemEntryPointRegister, javaLitOffsetReg);
   }

void
J9::Z::zOSSystemLinkage::setupRegisterDepForLinkage(TR::Node * callNode, TR_DispatchType dispatchType,
   TR::RegisterDependencyConditions * &deps, int64_t & killMask, TR::SystemLinkage * systemLinkage,
   TR::Node * &GlobalRegDeps, bool &hasGlRegDeps, TR::Register ** methodAddressReg, TR::Register * &javaLitOffsetReg)
   {
   // call j9 private linkage specialization
   J9::Z::PrivateLinkage * privateLinkage = static_cast<J9::Z::PrivateLinkage *>(cg()->getLinkage(TR_Private));
   privateLinkage->setupRegisterDepForLinkage(callNode, dispatchType, deps, killMask, systemLinkage, GlobalRegDeps, hasGlRegDeps, methodAddressReg, javaLitOffsetReg);
   }

void
J9::Z::zOSSystemLinkage::setupBuildArgForLinkage(TR::Node * callNode, TR_DispatchType dispatchType, TR::RegisterDependencyConditions * deps, bool isFastJNI,
      bool isPassReceiver, int64_t & killMask, TR::Node * GlobalRegDeps, bool hasGlRegDeps, TR::SystemLinkage * systemLinkage)
   {
   J9::Z::PrivateLinkage * privateLinkage = static_cast<J9::Z::PrivateLinkage *>(cg()->getLinkage(TR_Private));
   privateLinkage->setupBuildArgForLinkage(callNode, dispatchType, deps, isFastJNI, isPassReceiver, killMask, GlobalRegDeps, hasGlRegDeps, systemLinkage);
   }

void
J9::Z::zOSSystemLinkage::performCallNativeFunctionForLinkage(TR::Node * callNode, TR_DispatchType dispatchType, TR::Register * &javaReturnRegister, TR::SystemLinkage * systemLinkage,
      TR::RegisterDependencyConditions * &deps, TR::Register * javaLitOffsetReg, TR::Register * methodAddressReg, bool isJNIGCPoint)
   {
   // call base class implementation first
   OMR::Z::Linkage::performCallNativeFunctionForLinkage(callNode, dispatchType, javaReturnRegister, systemLinkage, deps, javaLitOffsetReg, methodAddressReg, isJNIGCPoint);

   // get javaStack Real Register
   TR::CodeGenerator * codeGen = cg();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(codeGen->fe());
   J9::Z::PrivateLinkage * privateLinkage = static_cast<J9::Z::PrivateLinkage *>(cg()->getLinkage(TR_Private));
   TR::RealRegister * javaStackPointerRealRegister = privateLinkage->getStackPointerRealRegister();

   // get methodMetaDataVirtualRegister
   TR::Register * methodMetaDataVirtualRegister = privateLinkage->getMethodMetaDataRealRegister();

   // restore java stack pointer
   generateRXInstruction(codeGen, TR::InstOpCode::getLoadOpCode(), callNode, javaStackPointerRealRegister,
                         new (trHeapMemory()) TR::MemoryReference(methodMetaDataVirtualRegister, (int32_t)fej9->thisThreadGetJavaSPOffset(), codeGen));
   }

void
J9::Z::zOSSystemLinkage::doNotKillSpecialRegsForBuildArgs (TR::Linkage *linkage, bool isFastJNI, int64_t &killMask)
   {
   J9::Z::PrivateLinkage * privateLinkage = static_cast<J9::Z::PrivateLinkage *>(cg()->getLinkage(TR_Private));
   privateLinkage->doNotKillSpecialRegsForBuildArgs(linkage, isFastJNI, killMask);
   }

void
J9::Z::zOSSystemLinkage::addSpecialRegDepsForBuildArgs(TR::Node * callNode, TR::RegisterDependencyConditions * dependencies, int32_t& from, int32_t step)
   {
   J9::Z::PrivateLinkage * privateLinkage = static_cast<J9::Z::PrivateLinkage *>(cg()->getLinkage(TR_Private));
   privateLinkage->addSpecialRegDepsForBuildArgs(callNode, dependencies, from, step);
   }

int32_t
J9::Z::zOSSystemLinkage::storeExtraEnvRegForBuildArgs(TR::Node * callNode, TR::Linkage* linkage, TR::RegisterDependencyConditions * dependencies,
      bool isFastJNI, int32_t stackOffset, int8_t gprSize, uint32_t &numIntegerArgs)
   {
   J9::Z::PrivateLinkage * privateLinkage = static_cast<J9::Z::PrivateLinkage *>(cg()->getLinkage(TR_Private));
   stackOffset = privateLinkage->storeExtraEnvRegForBuildArgs(callNode, linkage, dependencies, isFastJNI, stackOffset, gprSize, numIntegerArgs);
   return stackOffset;
   }

int64_t
J9::Z::zOSSystemLinkage::addFECustomizedReturnRegDependency(int64_t killMask, TR::Linkage* linkage, TR::DataType resType, TR::RegisterDependencyConditions * dependencies)
   {
   J9::Z::PrivateLinkage * privateLinkage = static_cast<J9::Z::PrivateLinkage *>(cg()->getLinkage(TR_Private));
   killMask = privateLinkage->addFECustomizedReturnRegDependency(killMask, linkage, resType, dependencies);
   return killMask;
   }
