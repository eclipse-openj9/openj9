/*******************************************************************************
 * Copyright (c) 2016, 2019 IBM Corp. and others
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

#include "arm/codegen/ARMJNILinkage.hpp"

#include "arm/codegen/ARMInstruction.hpp"
#include "codegen/CallSnippet.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/GCStackMap.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/StackCheckFailureSnippet.hpp"
#include "env/CompilerEnv.hpp"
#include "env/J2IThunk.hpp"
#include "env/VMJ9.h"
#include "env/jittypes.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"

#define LOCK_R14

// TODO: Merge with TR::ARMLinkageProperties
static TR::RealRegister::RegNum _singleArgumentRegisters[] =
   {
   TR::RealRegister::fs0,
   TR::RealRegister::fs1,
   TR::RealRegister::fs2,
   TR::RealRegister::fs3,
   TR::RealRegister::fs4,
   TR::RealRegister::fs5,
   TR::RealRegister::fs6,
   TR::RealRegister::fs7,
   TR::RealRegister::fs8,
   TR::RealRegister::fs9,
   TR::RealRegister::fs10,
   TR::RealRegister::fs11,
   TR::RealRegister::fs12,
   TR::RealRegister::fs13,
   TR::RealRegister::fs14,
   TR::RealRegister::fs15
   };

TR::ARMJNILinkage::ARMJNILinkage(TR::CodeGenerator *cg)
   :TR::ARMPrivateLinkage(cg)
   {
   //Copy out SystemLinkage properties. Assumes no objects in TR::ARMLinkageProperties.
   TR::Linkage *sysLinkage = cg->getLinkage(TR_System);
   const TR::ARMLinkageProperties& sysLinkageProperties = sysLinkage->getProperties();

   _properties = sysLinkageProperties;

   //Set preservedRegisterMapForGC to PrivateLinkage properties.
   TR::Linkage *privateLinkage = cg->getLinkage(TR_Private);
   const TR::ARMLinkageProperties& privateLinkageProperties = privateLinkage->getProperties();

   _properties._preservedRegisterMapForGC = privateLinkageProperties.getPreservedRegisterMapForGC();

   }

int32_t TR::ARMJNILinkage::buildArgs(TR::Node *callNode,
                             TR::RegisterDependencyConditions *dependencies,
                             TR::Register* &vftReg,
                             bool                                isVirtual)
   {
   TR_ASSERT(0, "Should call TR::ARMJNILinkage::buildJNIArgs instead.");
   return 0;
   }

TR::Register *TR::ARMJNILinkage::buildIndirectDispatch(TR::Node  *callNode)
   {
   TR_ASSERT(0, "Calling TR::ARMJNILinkage::buildIndirectDispatch does not make sense.");
   return NULL;
   }

void         TR::ARMJNILinkage::buildVirtualDispatch(TR::Node   *callNode,
                                                    TR::RegisterDependencyConditions *dependencies,
                                                    TR::RegisterDependencyConditions *postDeps,
                                                    TR::Register                        *vftReg,
                                                    uint32_t                           sizeOfArguments)
   {
   TR_ASSERT(0, "Calling TR::ARMJNILinkage::buildVirtualDispatch does not make sense.");
   }

TR::ARMLinkageProperties& TR::ARMJNILinkage::getProperties()
   {
   return _properties;
   }

#if defined(__VFP_FP__) && !defined(__SOFTFP__)
TR::Register *TR::ARMJNILinkage::pushFloatArgForJNI(TR::Node *child)
   {
   // if (isSmall()) return 0;

   TR::Register *pushRegister = cg()->evaluate(child);
   child->setRegister(pushRegister);
   child->decReferenceCount();
   if (pushRegister->getKind() == TR_GPR)
      {
      TR::Register *trgReg = cg()->allocateSinglePrecisionRegister();
      generateTrg1Src1Instruction(cg(), ARMOp_fmsr, child, trgReg, pushRegister);
      return trgReg;
      }

   return pushRegister;
   }

TR::Register *TR::ARMJNILinkage::pushDoubleArgForJNI(TR::Node *child)
   {
   // if (isSmall()) return 0;

   TR::Register *pushRegister = cg()->evaluate(child);
   child->setRegister(pushRegister);
   child->decReferenceCount();
   if (pushRegister->getKind() == TR_GPR)
      {
      TR::Register *trgReg = cg()->allocateRegister(TR_FPR);
      TR::RegisterPair *pair = pushRegister->getRegisterPair();
      generateTrg1Src2Instruction(cg(), ARMOp_fmdrr, child, trgReg, pair->getLowOrder(), pair->getHighOrder());
      return trgReg;
      }
   return pushRegister;
   }
#endif

TR::MemoryReference *TR::ARMJNILinkage::getOutgoingArgumentMemRef(int32_t               totalSize,
                                                                       int32_t               offset,
                                                                       TR::Register          *argReg,
                                                                       TR_ARMOpCodes         opCode,
                                                                       TR::ARMMemoryArgument &memArg)
   {
/* totalSize does not matter */
#ifdef DEBUG_ARM_LINKAGE
printf("JNI: offset %d\n", offset); fflush(stdout);
#endif
   const TR::ARMLinkageProperties &jniLinkageProperties = getProperties();
   int32_t                spOffset = offset + jniLinkageProperties.getOffsetToFirstParm();
   TR::RealRegister    *sp       = cg()->machine()->getRealRegister(jniLinkageProperties.getStackPointerRegister());
   TR::MemoryReference *result   = new (trHeapMemory()) TR::MemoryReference(sp, spOffset, cg());
   memArg.argRegister = argReg;
   memArg.argMemory   = result;
   memArg.opCode      = opCode;
   return result;
   }

int32_t TR::ARMJNILinkage::buildJNIArgs(TR::Node *callNode,
                          TR::RegisterDependencyConditions *dependencies,
                          TR::Register* &vftReg,
                          bool passReceiver,
                          bool passEnvArg)
   {
   TR::Compilation *comp = TR::comp();
   TR::CodeGenerator  *codeGen      = cg();
   TR::ARMMemoryArgument *pushToMemory = NULL;
   const TR::ARMLinkageProperties &jniLinkageProperties = getProperties();

   bool bigEndian = TR::Compiler->target.cpu.isBigEndian();
   int32_t   i;
   uint32_t  numIntegerArgRegIndex = 0;
   uint32_t  numFloatArgRegIndex = 0;
   uint32_t  numDoubleArgRegIndex = 0;
   uint32_t  numSingleArgRegIndex = 0; // if nonzero, use the second half of VFP register
   uint32_t  numMemArgs = 0;
   uint32_t  stackOffset = 0;
   uint32_t  numIntArgRegs = jniLinkageProperties.getNumIntArgRegs();
   uint32_t  numFloatArgRegs = jniLinkageProperties.getNumFloatArgRegs();
   bool      isEABI = TR::Compiler->target.isEABI();
   uint32_t  firstArgumentChild = callNode->getFirstArgumentIndex();
   TR::DataType callNodeDataType = callNode->getDataType();
   TR::DataType resType = callNode->getType();
   TR::MethodSymbol *callSymbol = callNode->getSymbol()->castToMethodSymbol();

   if (passEnvArg)
      numIntegerArgRegIndex = 1;

   if (!passReceiver)
      {
      // Evaluate as usual if necessary, but don't actually pass it to the callee
      TR::Node *firstArgChild = callNode->getChild(firstArgumentChild);
      if (firstArgChild->getReferenceCount() > 1)
         {
         switch (firstArgChild->getDataType())
            {
            case TR::Int32:
               pushIntegerWordArg(firstArgChild);
               break;
            case TR::Int64:
               pushLongArg(firstArgChild);
               break;
            case TR::Address:
               pushAddressArg(firstArgChild);
               break;
            default:
               TR_ASSERT( false, "Unexpected first child type");
            }
         }
      else
         codeGen->recursivelyDecReferenceCount(firstArgChild);
      firstArgumentChild += 1;
      }

   /* Step 1 - figure out how many arguments are going to be spilled to memory i.e. not in registers */
   for (i = firstArgumentChild; i < callNode->getNumChildren(); i++)
      {
      switch (callNode->getChild(i)->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Address:
#if !defined(__ARM_PCS_VFP)
         case TR::Float:
#endif
            if (numIntegerArgRegIndex >= numIntArgRegs)
               {
               numMemArgs++;
               stackOffset += 4;
               }
            numIntegerArgRegIndex++;
            break;
         case TR::Int64:
#if !defined(__ARM_PCS_VFP)
         case TR::Double:
#endif
            if (isEABI)
               {
                // The MVL CEE 3.0 system linkage requires that an unaligned
                // long argument consume an extra slot of padding
               if (numIntegerArgRegIndex + 1 >= numIntArgRegs)
                  {
                  /* if numIntegerArgRegIndex >= numIntArgReg - 1, we do not need to care about the register slot padding. */
                  numIntegerArgRegIndex += 2;
                  numMemArgs += 2;
                  /* but we need to care about the stack alignment instead. */
                  stackOffset = (stackOffset + 12) & (~7);
                  }
               else
                  {
                  if (numIntegerArgRegIndex & 1)
                     numIntegerArgRegIndex += 3; // an unaligned long argument consume an extra slot of padding
                  else
                     numIntegerArgRegIndex += 2;
                  }
               }
            else
               {
               if (numIntegerArgRegIndex + 1 == numIntArgRegs)
                  {
                  numMemArgs++;
                  stackOffset += 4;
                  }
               else if (numIntegerArgRegIndex + 1 > numIntArgRegs)
                  {
                  numMemArgs += 2;
                  stackOffset += 8;
                  }

               numIntegerArgRegIndex += 2;
               }
            break;
#if defined(__ARM_PCS_VFP) // -mfloat-abi=hard
         case TR::Float:
            if ((numFloatArgRegIndex < numFloatArgRegs) || (numFloatArgRegIndex == numFloatArgRegs && numSingleArgRegIndex != 0))
               {
               if (numSingleArgRegIndex != 0)
                  {
                  numSingleArgRegIndex = 0;
                  }
               else
                  {
                  numSingleArgRegIndex = numFloatArgRegIndex * 2 + 1;
                  numFloatArgRegIndex++;
                  }
               }
            else
               {
               numMemArgs++;
               stackOffset += 4;

               numFloatArgRegIndex++;
               if (numSingleArgRegIndex != 0)
                  {
                  numSingleArgRegIndex = 0;
                  }
               }
            break;
         case TR::Double:
            if (isEABI)
               {
                // The MVL CEE 3.0 system linkage requires that an unaligned
                // long argument consume an extra slot of padding
                // A VFP register can hold 64bit double value.
               if (numFloatArgRegIndex >= numFloatArgRegs)
                  {
                  numMemArgs++;
                  /* 8byte stack alignment. */
                  stackOffset = (stackOffset + 12) & (~7);
                  }
               }
            else
               {
               if (numFloatArgRegIndex >= numFloatArgRegs)
                  {
                  numMemArgs++;
                  stackOffset += 8;
                  }
               }
            numFloatArgRegIndex++;
            break;
#endif // ifndef __VFP_FP__
         }
      }

	#ifdef DEBUG_ARM_LINKAGE
	// const char *sig = callNode->getSymbol()->getResolvedMethodSymbol()->signature();
	const char *sig = "CALL";
	printf("%s: numIntegerArgRegIndex %d numMemArgs %d\n", sig,  numIntegerArgRegIndex, numMemArgs); fflush(stdout);
	#endif


   int32_t numStackParmSlots = 0;
   // From here, down, all stack memory allocations will expire / die when the function returns.
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());
   if (numMemArgs > 0)
      {

      pushToMemory = new(trStackMemory()) TR::ARMMemoryArgument[numMemArgs];

      // For direct-to-JNI calls, instead of pushing
      // arguments, buy the necessary stack slots up front.
      // On EABI, keep the C stack 8-byte aligned on entry to native callee.
      numStackParmSlots = (isEABI ? ((stackOffset + 4) & (~7)) : stackOffset);
#ifdef DEBUG_ARM_LINKAGE
printf("subtracting %d slots from SP\n", numStackParmSlots); fflush(stdout);
#endif
      TR::RealRegister *sp = codeGen->machine()->getRealRegister(jniLinkageProperties.getStackPointerRegister());
      uint32_t base, rotate;
      if (constantIsImmed8r(numStackParmSlots, &base, &rotate))
         {
         generateTrg1Src1ImmInstruction(codeGen, ARMOp_sub, callNode, sp, sp, base, rotate);
         }
      else
         {
         TR::Register *tmpReg = codeGen->allocateRegister();
         armLoadConstant(callNode, numStackParmSlots, tmpReg, codeGen);
         generateTrg1Src2Instruction(codeGen, ARMOp_sub, callNode, sp, sp, tmpReg);
         codeGen->stopUsingRegister(tmpReg);
         }
      }

   if (passEnvArg)
      numIntegerArgRegIndex = 1;

   numFloatArgRegIndex = 0;
   numSingleArgRegIndex = 0;
   stackOffset = 0;

   int32_t memArg  = 0;
   for (i = firstArgumentChild; i < callNode->getNumChildren(); i++)
      {
      TR::Node               *child = callNode->getChild(i);
      TR::DataType         childType = child->getDataType();
      TR::Register           *reg;
      TR::Register           *tempReg;
      TR::MemoryReference    *tempMR;

      switch (childType)
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Address: // have to do something for GC maps here
#if !defined(__ARM_PCS_VFP)
         case TR::Float:
#endif
            if (childType == TR::Address)
               reg = pushJNIReferenceArg(child);
            else
               reg = pushIntegerWordArg(child);

            if (numIntegerArgRegIndex < numIntArgRegs)
               {
               if ((childType != TR::Address) && !cg()->canClobberNodesRegister(child, 0))
                  {
                  /* If the reg is shared by another node, we copy it to tempReg, so that the reg is not reused after returning the call. */
                  if (reg->containsCollectedReference())
                     tempReg = codeGen->allocateCollectedReferenceRegister();
                  else
                     tempReg = codeGen->allocateRegister();
                  generateTrg1Src1Instruction(codeGen, ARMOp_mov, child, tempReg, reg);
                  reg = tempReg;
                  }
               if (numIntegerArgRegIndex == 0 &&
                  (resType.isAddress() || resType.isInt32() || resType.isInt64()))
                  {
                  TR::Register *resultReg;
                  if (resType.isAddress())
                     resultReg = codeGen->allocateCollectedReferenceRegister();
                  else
                     resultReg = codeGen->allocateRegister();
                  dependencies->addPreCondition(reg, TR::RealRegister::gr0);
                  dependencies->addPostCondition(resultReg, TR::RealRegister::gr0);
                  }
               else if (numIntegerArgRegIndex == 1 && resType.isInt64())
                  {
                  TR::Register *resultReg = codeGen->allocateRegister();
                  dependencies->addPreCondition(reg, TR::RealRegister::gr1);
                  dependencies->addPostCondition(resultReg, TR::RealRegister::gr1);
                  }
               else
                  {
                  TR::addDependency(dependencies, reg, jniLinkageProperties.getIntegerArgumentRegister(numIntegerArgRegIndex), TR_GPR, codeGen);
                  }
               }
            else
               {
#ifdef DEBUG_ARM_LINKAGE
printf("pushing 32-bit arg %d %d\n", numIntegerArgRegIndex, memArg); fflush(stdout);
#endif
               tempMR = getOutgoingArgumentMemRef(0, stackOffset, reg, ARMOp_str, pushToMemory[memArg++]);
               stackOffset += 4;
               }
            numIntegerArgRegIndex++;
            break;
         case TR::Int64:
#if !defined(__ARM_PCS_VFP)
         case TR::Double:
#endif
            if (isEABI)
               {
               if (numIntegerArgRegIndex & 1)
                  {
#ifdef DEBUG_ARM_LINKAGE
printf("skipping one argument slot\n"); fflush(stdout);
#endif
                  numIntegerArgRegIndex++;
                  }
               }
            reg = pushLongArg(child);

            if (numIntegerArgRegIndex < numIntArgRegs)
               {
               if (!cg()->canClobberNodesRegister(child, 0))
                  {
                  tempReg = codeGen->allocateRegister();

                  if (bigEndian)
                     {
                     generateTrg1Src1Instruction(codeGen, ARMOp_mov, child, tempReg, reg->getRegisterPair()->getHighOrder());
                     reg = codeGen->allocateRegisterPair(reg->getRegisterPair()->getLowOrder(), tempReg);
                     }
                  else
                     {
                     generateTrg1Src1Instruction(codeGen, ARMOp_mov, child, tempReg, reg->getRegisterPair()->getLowOrder());
                     reg = codeGen->allocateRegisterPair(tempReg, reg->getRegisterPair()->getHighOrder());
                     }
                  }

               dependencies->addPreCondition(bigEndian ? reg->getRegisterPair()->getHighOrder() : reg->getRegisterPair()->getLowOrder(), jniLinkageProperties.getIntegerArgumentRegister(numIntegerArgRegIndex));
               if ((numIntegerArgRegIndex == 0) && resType.isAddress())
                  dependencies->addPostCondition(codeGen->allocateCollectedReferenceRegister(), TR::RealRegister::gr0);
               else
                  dependencies->addPostCondition(codeGen->allocateRegister(), jniLinkageProperties.getIntegerArgumentRegister(numIntegerArgRegIndex));

               if (numIntegerArgRegIndex + 1 < numIntArgRegs)
                  {
                  if (!cg()->canClobberNodesRegister(child, 0))
                     {
                     tempReg = codeGen->allocateRegister();
                     if (bigEndian)
                        {
                        generateTrg1Src1Instruction(codeGen, ARMOp_mov, child, tempReg, reg->getRegisterPair()->getLowOrder());
                        reg->getRegisterPair()->setLowOrder(tempReg, codeGen);
                        }
                     else
                        {
                        generateTrg1Src1Instruction(codeGen, ARMOp_mov, child, tempReg, reg->getRegisterPair()->getHighOrder());
                        reg->getRegisterPair()->setHighOrder(tempReg, codeGen);
                        }
                     }
                  dependencies->addPreCondition(bigEndian ? reg->getRegisterPair()->getLowOrder() : reg->getRegisterPair()->getHighOrder(), jniLinkageProperties.getIntegerArgumentRegister(numIntegerArgRegIndex + 1));
                  dependencies->addPostCondition(codeGen->allocateRegister(), jniLinkageProperties.getIntegerArgumentRegister(numIntegerArgRegIndex + 1));
                  }
               else
                  {
#ifdef DEBUG_ARM_LINKAGE
printf("pushing %s word of 64-bit arg %d %d\n", bigEndian ? "low" : "high", numIntegerArgRegIndex, memArg); fflush(stdout);
#endif
                  tempMR = getOutgoingArgumentMemRef(0, stackOffset, bigEndian ? reg->getRegisterPair()->getLowOrder() : reg->getRegisterPair()->getHighOrder(), ARMOp_str, pushToMemory[memArg++]);
                  stackOffset += 4;
                  }
               }
            else
               {
#ifdef DEBUG_ARM_LINKAGE
printf("pushing 64-bit JNI arg %d %d %d\n", numIntegerArgs, memArg, totalSize); fflush(stdout);
#endif
               if (isEABI)
                  stackOffset = (stackOffset + 4) & (~7);
               tempMR = getOutgoingArgumentMemRef(0, stackOffset, bigEndian ? reg->getRegisterPair()->getHighOrder() : reg->getRegisterPair()->getLowOrder(), ARMOp_str, pushToMemory[memArg++]);
               tempMR = getOutgoingArgumentMemRef(0, stackOffset + 4, bigEndian ? reg->getRegisterPair()->getLowOrder() : reg->getRegisterPair()->getHighOrder(), ARMOp_str, pushToMemory[memArg++]);
               stackOffset += 8;
               }
            numIntegerArgRegIndex += 2;
            break;
#if defined(__ARM_PCS_VFP) // -mfloat-abi=hard
         case TR::Float:
            reg = pushFloatArgForJNI(child);
            if ((numFloatArgRegIndex < numFloatArgRegs) || (numFloatArgRegIndex == numFloatArgRegs && numSingleArgRegIndex != 0))
               {
               if (!cg()->canClobberNodesRegister(child, 0))
                  {
                  tempReg = codeGen->allocateSinglePrecisionRegister();
                  generateTrg1Src1Instruction(codeGen, ARMOp_fcpys, child, tempReg, reg);
                  reg = tempReg;
                  }
               if ((numFloatArgRegIndex == 0) && resType.isFloatingPoint())
                  {
                  TR::Register *resultReg;
                  if (resType.getDataType() == TR::Float)
                      resultReg = codeGen->allocateSinglePrecisionRegister();
                   else
                      resultReg = codeGen->allocateRegister(TR_FPR);

                  dependencies->addPreCondition(reg, TR::RealRegister::fs0);
                  dependencies->addPostCondition(resultReg, TR::RealRegister::fp0);
                  }
               else
                  {
                  uint32_t regIdx = (numSingleArgRegIndex != 0) ? numSingleArgRegIndex : numFloatArgRegIndex * 2;
                  dependencies->addPreCondition(reg, _singleArgumentRegisters[regIdx]);
                  if (numSingleArgRegIndex == 0)
                     {
                     dependencies->addPostCondition(reg, jniLinkageProperties.getFloatArgumentRegister(numFloatArgRegIndex));
                     }
                  }

               if (numSingleArgRegIndex != 0)
                  {
                  numSingleArgRegIndex = 0;
                  }
               else
                  {
                  numSingleArgRegIndex = numFloatArgRegIndex * 2 + 1;
                  numFloatArgRegIndex++;
                  }
               }
            else
               {
               tempMR = getOutgoingArgumentMemRef(0, stackOffset, reg, ARMOp_fsts, pushToMemory[memArg++]);
               stackOffset += 4;

               numFloatArgRegIndex++;
               if (numSingleArgRegIndex != 0)
                  {
                  numSingleArgRegIndex = 0;
                  }
               }
            break;
         case TR::Double:
            reg = pushDoubleArgForJNI(child);
            if (numFloatArgRegIndex < numFloatArgRegs)
               {
               if (!cg()->canClobberNodesRegister(child, 0))
                  {
                  tempReg = cg()->allocateRegister(TR_FPR);
                  generateTrg1Src1Instruction(codeGen, ARMOp_fcpyd, child, tempReg, reg);
                  reg = tempReg;
                  }
               if ((numFloatArgRegIndex == 0) && resType.isFloatingPoint())
                  {
                  TR::Register *resultReg;
                  if (resType.getDataType() == TR::Float)
                      resultReg = codeGen->allocateSinglePrecisionRegister();
                   else
                      resultReg = codeGen->allocateRegister(TR_FPR);

                  dependencies->addPreCondition(reg, TR::RealRegister::fp0);
                  dependencies->addPostCondition(resultReg, TR::RealRegister::fp0);
                  }
               else
                  TR::addDependency(dependencies, reg, jniLinkageProperties.getFloatArgumentRegister(numFloatArgRegIndex), TR_FPR, codeGen);
               }
            else
               {
               if (isEABI)
                   stackOffset = (stackOffset + 4) & (~7);
               tempMR = getOutgoingArgumentMemRef(0, stackOffset, reg, ARMOp_fstd, pushToMemory[memArg++]);
               stackOffset += 8;
               }
            numFloatArgRegIndex++;
            break;
#endif
         }
      }

   for (i = 0; i < jniLinkageProperties.getNumIntArgRegs(); i++)
      {
      TR::RealRegister::RegNum realReg = (TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::gr0 + i);
      if (passEnvArg && (realReg == TR::RealRegister::gr0))
         continue;
      if (!dependencies->searchPreConditionRegister(realReg))
         {
         if (realReg == jniLinkageProperties.getIntegerArgumentRegister(0) && resType.isAddress())
            {
            dependencies->addPreCondition(codeGen->allocateRegister(), TR::RealRegister::gr0);
            dependencies->addPostCondition(codeGen->allocateCollectedReferenceRegister(), TR::RealRegister::gr0);
            }
         else if ((realReg == TR::RealRegister::gr1) && resType.isInt64())
            {
            dependencies->addPreCondition(codeGen->allocateRegister(), TR::RealRegister::gr1);
            dependencies->addPostCondition(codeGen->allocateRegister(), TR::RealRegister::gr1);
            }
         else
            {
            TR::addDependency(dependencies, NULL, realReg, TR_GPR, codeGen);
            }
         }
      }

   /* d0-d8 are argument registers and not preserved across the call. */
   for (i = 0; i < jniLinkageProperties.getNumFloatArgRegs(); i++)
      {
      TR::RealRegister::RegNum realReg = (TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::fp0 + i);
      if (!dependencies->searchPreConditionRegister(realReg))
         {
         TR::RealRegister::RegNum singleReg1, singleReg2;
         singleReg1 = (TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::fs0 + i*2);
         singleReg2 = (TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::fs0 + i*2 + 1);
         if (!dependencies->searchPreConditionRegister(singleReg1))
            {
            TR_ASSERT(!dependencies->searchPreConditionRegister(singleReg2), "Wrong dependency for single precision register.\n");
            if (realReg == jniLinkageProperties.getFloatArgumentRegister(0) && (resType.getDataType() == TR::Float))
               {
               dependencies->addPreCondition(codeGen->allocateRegister(TR_FPR), TR::RealRegister::fp0);
               dependencies->addPostCondition(codeGen->allocateSinglePrecisionRegister(), TR::RealRegister::fp0);
               }
            else
               {
               TR::addDependency(dependencies, NULL, realReg, TR_FPR, codeGen);
               }
            }
         }
      }

   // add dependencies for other volatile registers; for virtual calls,
   // dependencies on gr11 and gr14 have already been added above
#ifndef LOCK_R14
   TR::addDependency(dependencies, NULL, TR::RealRegister::gr14, TR_GPR, codeGen);
#endif

   for (i = 0; i < numMemArgs; i++)
      {
#ifdef DEBUG_ARM_LINKAGE
printf("pushing mem arg %d of %d (in reg %x) to [sp + %d]... ", i, numMemArgs, pushToMemory[i].argRegister, pushToMemory[i].argMemory->getOffset()); fflush(stdout);
#endif
      TR::Register *aReg = pushToMemory[i].argRegister;
      generateMemSrc1Instruction(codeGen,
                                 pushToMemory[i].opCode,
                                 callNode,
                                 pushToMemory[i].argMemory,
                                 pushToMemory[i].argRegister);
      codeGen->stopUsingRegister(aReg);
#ifdef DEBUG_ARM_LINKAGE
printf("done\n"); fflush(stdout);
#endif
      }
   //Returns the size of parameter buffers allocated on stack
   return numStackParmSlots;

   }


TR::Register *TR::ARMJNILinkage::buildDirectDispatch(TR::Node *callNode)
   {
   TR::CodeGenerator           *codeGen    = cg();
   const TR::ARMLinkageProperties &jniLinkageProperties = getProperties();
   TR::Linkage *privateLinkage = codeGen->getLinkage(TR_Private);
   const TR::ARMLinkageProperties &privateLinkageProperties = privateLinkage->getProperties();

   TR::RegisterDependencyConditions *deps =
      new (trHeapMemory()) TR::RegisterDependencyConditions(jniLinkageProperties.getNumberOfDependencyGPRegisters() + jniLinkageProperties.getNumFloatArgRegs()*2,
                                             jniLinkageProperties.getNumberOfDependencyGPRegisters() + jniLinkageProperties.getNumFloatArgRegs()*2, trMemory());

   TR::SymbolReference      *callSymRef     = callNode->getSymbolReference();
   TR::ResolvedMethodSymbol *calleeSym      = callSymRef->getSymbol()->castToResolvedMethodSymbol();
   TR_ResolvedMethod     *resolvedMethod = calleeSym->getResolvedMethod();

   codeGen->machine()->setLinkRegisterKilled(true);
   codeGen->setHasCall();

   // buildArgs() will set up dependencies for the volatile registers, except gr0.
   TR::Register* vftReg = NULL;
   int32_t spSize = buildJNIArgs(callNode, deps, vftReg, true, true);

   // kill all values in non-volatile registers so that the
   // values will be in a stack frame in case GC looks for them
   TR::addDependency(deps, NULL, TR::RealRegister::gr4, TR_GPR, codeGen);
   TR::addDependency(deps, NULL, TR::RealRegister::gr5, TR_GPR, codeGen);
   TR::addDependency(deps, NULL, TR::RealRegister::gr6, TR_GPR, codeGen);
   TR::addDependency(deps, NULL, TR::RealRegister::gr9, TR_GPR, codeGen);
   TR::addDependency(deps, NULL, TR::RealRegister::gr10, TR_GPR, codeGen);
   TR::addDependency(deps, NULL, TR::RealRegister::gr11, TR_GPR, codeGen);

   // set up dependency for the return register
   TR::Register *gr0Reg = deps->searchPreConditionRegister(TR::RealRegister::gr0);
   TR::Register *returnRegister = NULL;
   TR::Register *lowReg = NULL, *highReg;

   switch (callNode->getOpCodeValue())
      {
      case TR::icall:
      case TR::acall:
#if !defined(__VFP_FP__) || defined(__SOFTFP__)
      case TR::fcall:
#endif
         if (callNode->getDataType() == TR::Address)
            {
            if (!gr0Reg)
               {
               gr0Reg = codeGen->allocateRegister();
               returnRegister = codeGen->allocateCollectedReferenceRegister();
               deps->addPreCondition(gr0Reg, TR::RealRegister::gr0);
               deps->addPostCondition(returnRegister, TR::RealRegister::gr0);
               }
            else
               {
               returnRegister = deps->searchPostConditionRegister(TR::RealRegister::gr0);
               }
            }
         else
            {
            if (!gr0Reg)
               {
               gr0Reg = codeGen->allocateRegister();
               returnRegister = gr0Reg;
               TR::addDependency(deps, gr0Reg, TR::RealRegister::gr0, TR_GPR, codeGen);
               }
            else
               {
               returnRegister = deps->searchPostConditionRegister(TR::RealRegister::gr0);
               }
            }
         break;
      case TR::lcall:
#if !defined(__VFP_FP__) || defined(__SOFTFP__)
      case TR::dcall:
#endif
         {
         if(TR::Compiler->target.cpu.isBigEndian())
            {
            if (!gr0Reg)
               {
               gr0Reg = codeGen->allocateRegister();
               TR::addDependency(deps, gr0Reg, TR::RealRegister::gr0, TR_GPR, codeGen);
               highReg = gr0Reg;
               }
            else
               {
               highReg = deps->searchPostConditionRegister(TR::RealRegister::gr0);
               }
            lowReg = deps->searchPostConditionRegister(TR::RealRegister::gr1);
            }
         else
            {
            if (!gr0Reg)
               {
                gr0Reg = codeGen->allocateRegister();
                TR::addDependency(deps, gr0Reg, TR::RealRegister::gr0, TR_GPR, codeGen);
                lowReg = gr0Reg;
               }
            else
               {
                lowReg = deps->searchPostConditionRegister(TR::RealRegister::gr0);
               }
            highReg = deps->searchPostConditionRegister(TR::RealRegister::gr1);
            }
         returnRegister = codeGen->allocateRegisterPair(lowReg, highReg);
         break;
         }
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
      case TR::fcall:
      case TR::dcall:
         returnRegister = deps->searchPostConditionRegister(jniLinkageProperties.getFloatReturnRegister(0));
         if (!gr0Reg)
            {
            gr0Reg = codeGen->allocateRegister();
            TR::addDependency(deps, gr0Reg, TR::RealRegister::gr0, TR_GPR, codeGen);
            }
         break;
#endif
      case TR::call:
          if (!gr0Reg)
             {
             gr0Reg = codeGen->allocateRegister();
             TR::addDependency(deps, gr0Reg, TR::RealRegister::gr0, TR_GPR, codeGen);
             }
         returnRegister = NULL;
         break;
      default:
          if (!gr0Reg)
             {
             gr0Reg = codeGen->allocateRegister();
             TR::addDependency(deps, gr0Reg, TR::RealRegister::gr0, TR_GPR, codeGen);
             }
         returnRegister = NULL;
         TR_ASSERT(0, "Unknown direct call opode.\n");
      }

   deps->stopAddingConditions();

   // build stack frame on the Java stack
   TR::MemoryReference *tempMR;

   TR::Machine      *machine  = codeGen->machine();
   TR::RealRegister *metaReg  = codeGen->getMethodMetaDataRegister();
   TR::RealRegister *stackPtr = machine->getRealRegister(privateLinkageProperties.getStackPointerRegister());//JavaSP
   TR::RealRegister *instrPtr = machine->getRealRegister(TR::RealRegister::gr15);
   TR::RealRegister *gr13Reg  = machine->getRealRegister(TR::RealRegister::gr13);
   TR::Register        *gr4Reg   = deps->searchPreConditionRegister(TR::RealRegister::gr4);
   TR::Register        *gr5Reg   = deps->searchPreConditionRegister(TR::RealRegister::gr5);

   // Force spilling the volatile FPRs
   int32_t i;
   TR::LabelSymbol *spillLabel = generateLabelSymbol(codeGen);
   TR::RegisterDependencyConditions *spillDeps = new (trHeapMemory()) TR::RegisterDependencyConditions(0, jniLinkageProperties.getNumFloatArgRegs()*2, codeGen->trMemory());
   for (i = 0; i < jniLinkageProperties.getNumFloatArgRegs()*2; i++)
      {
      spillDeps->addPostCondition(codeGen->allocateRegister(TR_FPR), (TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::fs0 + i));
      }
   generateLabelInstruction(codeGen, ARMOp_label, callNode, spillLabel, spillDeps);

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   // mask out the magic bit that indicates JIT frames below
   generateTrg1ImmInstruction(codeGen, ARMOp_mov, callNode, gr5Reg, 0, 0);
   tempMR = new (trHeapMemory()) TR::MemoryReference(metaReg, fej9->thisThreadGetJavaFrameFlagsOffset(), codeGen);
   generateMemSrc1Instruction(codeGen, ARMOp_str, callNode, tempMR, gr5Reg);

   // push tag bits (savedA0 slot)
   // if the current method is simply a wrapper for the JNI call, hide the call-out stack frame
   uintptrj_t tagBits = fej9->constJNICallOutFrameSpecialTag();
   if (resolvedMethod == comp()->getCurrentMethod())
      {
      tagBits |= fej9->constJNICallOutFrameInvisibleTag();
      }
   armLoadConstant(callNode, tagBits, gr4Reg, codeGen);
   tempMR = new (trHeapMemory()) TR::MemoryReference(stackPtr, -((int)sizeof(uintptrj_t)), codeGen);
   tempMR->setImmediatePreIndexed();
   generateMemSrc1Instruction(codeGen, ARMOp_str, callNode, tempMR, gr4Reg);

   // skip unused savedPC slot and push return address (savedCP slot)
   //
   TR::LabelSymbol *returnAddrLabel               = generateLabelSymbol(codeGen);
   generateLabelInstruction(codeGen, ARMOp_add, callNode, returnAddrLabel, NULL, gr4Reg, instrPtr);
   tempMR = new (trHeapMemory()) TR::MemoryReference(stackPtr, -2 * ((int)sizeof(uintptrj_t)), codeGen);
   tempMR->setImmediatePreIndexed();
   generateMemSrc1Instruction(codeGen, ARMOp_str, callNode, tempMR, gr4Reg);

   // push frame flags
   intParts flags((int32_t)fej9->constJNICallOutFrameFlags());
   TR_ASSERT((flags.getValue() & ~0x7FFF0000) == 0, "JNI call-out frame flags have more than 15 bits");
   generateTrg1Src1Instruction(codeGen, ARMOp_mov, callNode, gr4Reg, new (trHeapMemory()) TR_ARMOperand2(flags.getByte3(), 24));
   generateTrg1Src2Instruction(codeGen, ARMOp_add, callNode, gr4Reg, gr4Reg, new (trHeapMemory()) TR_ARMOperand2(flags.getByte2(), 16));
   tempMR = new (trHeapMemory()) TR::MemoryReference(stackPtr, -((int)TR::Compiler->om.sizeofReferenceAddress()), codeGen);
   tempMR->setImmediatePreIndexed();
   generateMemSrc1Instruction(codeGen, ARMOp_str, callNode, tempMR, gr4Reg);

   // push the RAM method for the native
   intParts ramMethod((int32_t)resolvedMethod->resolvedMethodAddress());
   generateTrg1Src1Instruction(codeGen, ARMOp_mov, callNode, gr4Reg, new (trHeapMemory()) TR_ARMOperand2(ramMethod.getByte3(), 24));
   generateTrg1Src2Instruction(codeGen, ARMOp_add, callNode, gr4Reg, gr4Reg, new (trHeapMemory()) TR_ARMOperand2(ramMethod.getByte2(), 16));
   generateTrg1Src2Instruction(codeGen, ARMOp_add, callNode, gr4Reg, gr4Reg, new (trHeapMemory()) TR_ARMOperand2(ramMethod.getByte1(), 8));
   generateTrg1Src2Instruction(codeGen, ARMOp_add, callNode, gr4Reg, gr4Reg, new (trHeapMemory()) TR_ARMOperand2(ramMethod.getByte0(), 0));
   tempMR = new (trHeapMemory()) TR::MemoryReference(stackPtr, -((int)TR::Compiler->om.sizeofReferenceAddress()), codeGen);
   tempMR->setImmediatePreIndexed();
   generateMemSrc1Instruction(codeGen, ARMOp_str, callNode, tempMR, gr4Reg);

   // store the Java SP
   tempMR = new (trHeapMemory()) TR::MemoryReference(metaReg, fej9->thisThreadGetJavaSPOffset(), codeGen);
   generateMemSrc1Instruction(codeGen, ARMOp_str, callNode, tempMR, stackPtr);

   // store the PC and literals values indicating the call-out frame
   intParts frameType((int32_t)fej9->constJNICallOutFrameType());
   TR_ASSERT((frameType.getValue() & ~0xFFFF) == 0, "JNI call-out frame type has more than 16 bits");
   generateTrg1Src1Instruction(codeGen, ARMOp_mov, callNode, gr4Reg, new (trHeapMemory()) TR_ARMOperand2(frameType.getByte1(), 8));
   generateTrg1Src2Instruction(codeGen, ARMOp_add, callNode, gr4Reg, gr4Reg, new (trHeapMemory()) TR_ARMOperand2(frameType.getByte0(), 0));
   tempMR = new (trHeapMemory()) TR::MemoryReference(metaReg, fej9->thisThreadGetJavaPCOffset(), codeGen);
   generateMemSrc1Instruction(codeGen, ARMOp_str, callNode, tempMR, gr4Reg);
   tempMR = new (trHeapMemory()) TR::MemoryReference(metaReg, fej9->thisThreadGetJavaLiteralsOffset(), codeGen);
   generateMemSrc1Instruction(codeGen, ARMOp_str, callNode, tempMR, gr5Reg);

   // the Java arguments for the native method are all in place already; now
   // pass the vmThread pointer as the hidden first argument to a JNI call
   generateTrg1Src1Instruction(codeGen, ARMOp_mov, callNode, gr0Reg, metaReg);

   // release VM access (go to internalReleaseVMAccess directly)
   TR::ResolvedMethodSymbol *callerSym    = comp()->getJittedMethodSymbol();
   TR::SymbolReferenceTable *symRefTab    = comp()->getSymRefTab();
   TR::SymbolReference      *helperSymRef = symRefTab->findOrCreateReleaseVMAccessSymbolRef(callerSym);

   TR::ARMMultipleMoveInstruction *instr;
   instr = new (trHeapMemory()) TR::ARMMultipleMoveInstruction(ARMOp_stmdb, callNode, gr13Reg, 0x0f, codeGen);
   instr->setWriteBack();

   //AOT relocation is handled in TR::ARMImmSymInstruction::generateBinaryEncoding()
   TR::Instruction *gcPoint = generateImmSymInstruction(codeGen, ARMOp_bl, callNode, (uint32_t)helperSymRef->getMethodAddress(), NULL, helperSymRef);
   gcPoint->ARMNeedsGCMap(~(jniLinkageProperties.getPreservedRegisterMapForGC()));
   instr = new (trHeapMemory()) TR::ARMMultipleMoveInstruction(ARMOp_ldmia, callNode, gr13Reg, 0x0f, codeGen);
   instr->setWriteBack();

   // split dependencies to prevent register assigner from inserting code. Any generated
   // spills/loads would be incorrect as the stack pointer has not been fixed up yet.
   TR::RegisterDependencyConditions *postDeps = deps->clone(cg());
   deps->setNumPostConditions(0, trMemory());
   postDeps->setNumPreConditions(0, trMemory());
   // get the target method address and dispatch JNI method directly
   uintptrj_t methodAddress = (uintptrj_t)resolvedMethod->startAddressForJNIMethod(comp());
   //AOT relocation is handled in TR::ARMImmSymInstruction::generateBinaryEncoding()
   gcPoint = generateImmSymInstruction(codeGen, ARMOp_bl, callNode, methodAddress, deps, callSymRef);
   codeGen->getJNICallSites().push_front(new (trHeapMemory()) TR_Pair<TR_ResolvedMethod, TR::Instruction>(calleeSym->getResolvedMethod(), gcPoint));
   gcPoint->ARMNeedsGCMap(jniLinkageProperties.getPreservedRegisterMapForGC());

   generateLabelInstruction(codeGen, ARMOp_label, callNode, returnAddrLabel);

   // JNI methods may not return a full register in some cases so we need to
   // sign- or zero-extend the narrower integer return types properly.
   switch (resolvedMethod->returnType())
      {
      case TR::Int8:
        if (resolvedMethod->returnTypeIsUnsigned())
           generateTrg1Src1ImmInstruction(codeGen, ARMOp_and, callNode, returnRegister, returnRegister, 0xFF, 0);
        else
           {
           generateShiftLeftImmediate(codeGen, callNode, returnRegister, returnRegister, 24);
           generateShiftRightArithmeticImmediate(codeGen, callNode, returnRegister, returnRegister, 24);
           }
        break;
      case TR::Int16:
         if (resolvedMethod->returnTypeIsUnsigned())
            {
            generateTrg1ImmInstruction(codeGen, ARMOp_mvn, callNode, gr4Reg, 0xFF, 0);
            generateTrg1Src2Instruction(codeGen, ARMOp_and, callNode, returnRegister, returnRegister,
                                        new (trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSRImmed, gr4Reg, 16));
            }
         else
            {
            generateShiftLeftImmediate(codeGen, callNode, returnRegister, returnRegister, 16);
            generateShiftRightArithmeticImmediate(codeGen, callNode, returnRegister, returnRegister, 16);
            }
         break;
      }

   // if a particular flavour of arm-linux does not support soft-float then any floating point return value will
   // be in f0 and must be moved to the expected gpr(s).
   if (codeGen->hasHardFloatReturn())
      {
      switch (resolvedMethod->returnType())
         {
         case TR::Float:
            {
            TR::RealRegister *fpReg   = machine->getRealRegister(jniLinkageProperties.getFloatReturnRegister());
            fpReg->setAssignedRegister(fpReg);
            tempMR = new (trHeapMemory()) TR::MemoryReference(metaReg, fej9->thisThreadGetFloatTemp1Offset(), codeGen);
            generateMemSrc1Instruction(codeGen, ARMOp_fsts, callNode, tempMR, fpReg);
            generateTrg1MemInstruction(codeGen, ARMOp_ldr, callNode, returnRegister, tempMR);
            break;
            }
         case TR::Double:
            {
            TR::RealRegister *fdReg   = machine->getRealRegister(jniLinkageProperties.getDoubleReturnRegister());
            fdReg->setAssignedRegister(fdReg);
            TR_ASSERT(fej9->thisThreadGetFloatTemp2Offset() - fej9->thisThreadGetFloatTemp1Offset() == 4,"floatTemp1 and floatTemp2 not contiguous");
            tempMR = new (trHeapMemory()) TR::MemoryReference(metaReg, fej9->thisThreadGetFloatTemp1Offset(), codeGen);
            generateMemSrc1Instruction(codeGen, ARMOp_fstd, callNode, tempMR, fdReg);
            bool bigEndian = TR::Compiler->target.cpu.isBigEndian();
            generateTrg1MemInstruction(codeGen, ARMOp_ldr, callNode, bigEndian ? returnRegister->getHighOrder() : returnRegister->getLowOrder(), tempMR);
            tempMR = new (trHeapMemory()) TR::MemoryReference(metaReg, fej9->thisThreadGetFloatTemp2Offset(), codeGen);
            generateTrg1MemInstruction(codeGen, ARMOp_ldr, callNode, bigEndian ? returnRegister->getLowOrder() : returnRegister->getHighOrder(), tempMR);
            break;
            }
         }
      }

   // restore the system stack pointer (r13)
   if (spSize > 0)
      {
      TR::RealRegister *sp = machine->getRealRegister(jniLinkageProperties.getStackPointerRegister());
      uint32_t base, rotate;
      if (constantIsImmed8r(spSize, &base, &rotate))
         {
         generateTrg1Src1ImmInstruction(codeGen, ARMOp_add, callNode, sp, sp, base, rotate);
         }
      else
         {
         TR::Register *tmpReg = codeGen->allocateRegister();
         armLoadConstant(callNode, spSize, gr4Reg, codeGen);
         generateTrg1Src2Instruction(codeGen, ARMOp_add, callNode, sp, sp, gr4Reg);
         }
      }

   // re-acquire VM access
   helperSymRef = symRefTab->findOrCreateAcquireVMAccessSymbolRef(callerSym);
   //AOT relocation is handled in TR::ARMImmSymInstruction::generateBinaryEncoding()
   gcPoint = generateImmSymInstruction(codeGen, ARMOp_bl, callNode, (uint32_t)helperSymRef->getMethodAddress(), NULL, helperSymRef);
   gcPoint->ARMNeedsGCMap(1 << (jniLinkageProperties.getIntegerReturnRegister() - TR::RealRegister::FirstGPR));

   // JNI methods return objects with an extra level of indirection (unless
   // the result is NULL) so we need to dereference the return register;
   // This dereference must be done after vm access is re-acquired so the underlying
   // object is not moved by gc.
   if (resolvedMethod->returnType() == TR::Address)
      {
      tempMR = new (trHeapMemory()) TR::MemoryReference(returnRegister, 0, codeGen);
      generateSrc1ImmInstruction(codeGen, ARMOp_cmp, callNode, returnRegister, 0, 0);
      gcPoint = generateTrg1MemInstruction(codeGen, ARMOp_ldr, callNode, returnRegister, tempMR);
      gcPoint->setConditionCode(ARMConditionCodeNE);
      }

   // restore stack pointer and deal with possibly grown stack
   tempMR = new (trHeapMemory()) TR::MemoryReference(metaReg, fej9->thisThreadGetJavaLiteralsOffset(), codeGen);
   generateTrg1MemInstruction(codeGen, ARMOp_ldr, callNode, gr4Reg, tempMR);
   tempMR = new (trHeapMemory()) TR::MemoryReference(metaReg, fej9->thisThreadGetJavaSPOffset(), codeGen);
   generateTrg1MemInstruction(codeGen, ARMOp_ldr, callNode, stackPtr, tempMR);
   generateTrg1Src2Instruction(codeGen, ARMOp_add, callNode, stackPtr, stackPtr, gr4Reg);

   // see if the reference pool was used and, if used, clean it up; otherwise we can
   // leave a bunch of pinned garbage behind that screws up the GC quality forever
   tempMR = new (trHeapMemory()) TR::MemoryReference(stackPtr, fej9->constJNICallOutFrameFlagsOffset(), codeGen);
   generateTrg1MemInstruction(codeGen, ARMOp_ldr, callNode, gr4Reg, tempMR);

   uint32_t flagValue = fej9->constJNIReferenceFrameAllocatedFlags();
   uint32_t base, rotate;
   if (constantIsImmed8r(flagValue, &base, &rotate))
      {
      generateSrc1ImmInstruction(codeGen, ARMOp_tst, callNode, gr4Reg, base, rotate);
      }
   else
      {
      armLoadConstant(callNode, flagValue, gr5Reg, codeGen);
      generateSrc2Instruction(codeGen, ARMOp_tst, callNode, gr4Reg, gr5Reg);
      }

   helperSymRef = codeGen->symRefTab()->findOrCreateRuntimeHelper(TR_ARMjitCollapseJNIReferenceFrame, false, false, false);
   //AOT relocation is handled in TR::ARMImmSymInstruction::generateBinaryEncoding()
   generateImmSymInstruction(codeGen, ARMOp_bl, callNode, (uint32_t)helperSymRef->getMethodAddress(), NULL, helperSymRef, NULL, NULL, ARMConditionCodeEQ);

   // restore the JIT frame
   generateTrg1Src1ImmInstruction(codeGen, ARMOp_add, callNode, stackPtr, stackPtr, 20, 0);

   // check exceptions
   tempMR = new (trHeapMemory()) TR::MemoryReference(metaReg, fej9->thisThreadGetCurrentExceptionOffset(), codeGen);
   generateTrg1MemInstruction(codeGen, ARMOp_ldr, callNode, gr4Reg, tempMR);
   generateSrc1ImmInstruction(codeGen, ARMOp_cmp, callNode, gr4Reg, 0, 0);
   helperSymRef = symRefTab->findOrCreateThrowCurrentExceptionSymbolRef(callerSym);
   //AOT relocation is handled in TR::ARMImmSymInstruction::generateBinaryEncoding()
   gcPoint = generateImmSymInstruction(codeGen, ARMOp_bl, callNode, (uint32_t)helperSymRef->getMethodAddress(), NULL, helperSymRef, NULL, NULL, ARMConditionCodeNE);
   gcPoint->ARMNeedsGCMap(1 << (jniLinkageProperties.getIntegerReturnRegister() - TR::RealRegister::FirstGPR));

   TR::LabelSymbol *doneLabel = generateLabelSymbol(codeGen);
   generateLabelInstruction(codeGen, ARMOp_label, callNode, doneLabel, postDeps);

   return callNode->setRegister(returnRegister);
   }
