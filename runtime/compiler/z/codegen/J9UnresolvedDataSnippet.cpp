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

#pragma csect(CODE,"J9ZUnresolvedDataSnippet#C")
#pragma csect(STATIC,"J9ZUnresolvedDataSnippet#S")
#pragma csect(TEST,"J9ZUnresolvedDataSnippet#T")


#include "codegen/UnresolvedDataSnippet.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/Relocation.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/IO.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "il/symbol/StaticSymbol_inlines.hpp"
#include "infra/Assert.hpp"
#include "ras/Debug.hpp"
#include "runtime/J9Runtime.hpp"
#include "env/CompilerEnv.hpp"

namespace TR { class S390WritableDataSnippet; }

J9::Z::UnresolvedDataSnippet::UnresolvedDataSnippet(
      TR::CodeGenerator *cg,
      TR::Node *node,
      TR::SymbolReference *symRef,
      bool isStore,
      bool canCauseGC) :
   J9::UnresolvedDataSnippet(cg, node, symRef, isStore, canCauseGC),
      _branchInstruction(NULL),
      _dataReferenceInstruction(NULL),
      _dataSymbolReference(symRef),
      _unresolvedData(NULL),
      _memoryReference(NULL),
      _isStore(isStore)
   {
   TR_ASSERT( (symRef != NULL), " UDS: _dataSymbolRef is NULL !");
   }


uint8_t *
J9::Z::UnresolvedDataSnippet::getAddressOfDataReference()
   {
   return (uint8_t *) (_branchInstruction->getNext())->getBinaryEncoding();
   }


TR::Instruction *
J9::Z::UnresolvedDataSnippet::setDataReferenceInstruction(TR::Instruction *i)
   {
   // For instance data snippets, we need to guarantee that the branch
   // is right before the load/store it needs to update.
   if (isInstanceData())
      {
      // Return address register may be used if unresolved offset is > 4k.
      // PicBuilder will patch the base register of the load/store with RA register,
      // which will contain the sum of original base register and resolved offset.
      // Since this introduces some internal control flow, we need to ensure all registers
      // used in the control flow are added as dependencies.
      // We can have up to 6 dependencies:
      //    1. Branch RA register.
      //    2. Base Register for MR.
      //    3. Index Register for MR.
      //    4-6 "target" register.  2-3 dependencies requires for STM/LM.

      TR::Instruction* brInstr = getBranchInstruction();
      if (brInstr != NULL && brInstr->getNext() != i)
         {
         // Remove _branchInstruction
         brInstr->getPrev()->setNext(brInstr->getNext());
         brInstr->getNext()->setPrev(brInstr->getPrev());

         // Insert before current instruction
         brInstr->setPrev(i->getPrev());
         brInstr->setNext(i);
         brInstr->getPrev()->setNext(brInstr);
         i->setPrev(brInstr);
         }

      TR::RegisterDependencyConditions* dependencies = new (cg()->trHeapMemory()) TR::RegisterDependencyConditions(0, 6, cg());

      // The instruction after the branch should be our memory access instruction.
      TR::Instruction *memRefInstr = i;

      TR::MemoryReference* memRef = memRefInstr->getMemoryReference();
      TR_ASSERT(memRef != NULL, "Unexpected instruction for unresolved data sequence.\n");

      TR::Register* baseReg = memRef->getBaseRegister();
      TR::Register* indexReg = memRef->getIndexRegister();

      if (baseReg != NULL && baseReg->getRealRegister() == NULL)  // BaseReg may be a real Reg GPR0.
         dependencies->addPostCondition(baseReg, TR::RealRegister::AssignAny);
      if (indexReg != NULL && indexReg != baseReg)
         dependencies->addPostCondition(indexReg, TR::RealRegister::AssignAny);

      TR::Register* targetReg = memRefInstr->getRegisterOperand(1);
      if (targetReg != NULL && targetReg != baseReg && targetReg != indexReg)
         {
         // Handle Register Pairs
         TR::RegisterPair* targetRegPair = targetReg->getRegisterPair();
         if (targetRegPair != NULL)
            {
            // Force any register pairs used by LM/STM into GPR0:GPR1, to avoid potential issues
            // between register pairs and real register dependencies.
            dependencies->addPostCondition(targetRegPair->getHighOrder(), TR::RealRegister::GPR0);
            dependencies->addPostCondition(targetRegPair->getLowOrder(), TR::RealRegister::GPR1);
            }
         else
            {
            dependencies->addPostCondition(targetReg, TR::RealRegister::AssignAny);
            }
         }
      // Return Address Register may be used to patch long offsets.
      TR::RegisterDependencyConditions* brDependencies = brInstr->getDependencyConditions();
      TR::Register* returnReg = brDependencies->searchPostConditionRegister(cg()->getReturnAddressRegister());
      TR_ASSERT(returnReg != NULL, "Expect return address register in reg deps.");
      dependencies->addPostCondition(returnReg, cg()->getReturnAddressRegister());

      memRefInstr->setDependencyConditions(dependencies);
      }
   return _dataReferenceInstruction = i;
   }


uint8_t *
J9::Z::UnresolvedDataSnippet::emitSnippetBody()
   {
   TR::Compilation *comp = cg()->comp();
   uint8_t * cursor = cg()->getBinaryBufferCursor();
   TR::SymbolReference * glueRef;

   //Grab the snippet's start point
   getSnippetLabel()->setCodeLocation(cursor);

   //  setup the address of the picbuilder function

   if (getDataSymbol()->getShadowSymbol() != NULL)
      {
      if (resolveForStore())
         {
         glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_S390interpreterUnresolvedInstanceDataStoreGlue, false, false, false);
         }
      else
         {
         glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_S390interpreterUnresolvedInstanceDataGlue, false, false, false);
         }
      }
   else if (getDataSymbol()->isClassObject())
      {
      if (getDataSymbol()->addressIsCPIndexOfStatic())
         {
         glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_S390interpreterUnresolvedClassGlue2, false, false, false);
         }
      else
         {
         glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_S390interpreterUnresolvedClassGlue, false, false, false);
         }
      }
   else if (getDataSymbol()->isConstString())
      {
      glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_S390interpreterUnresolvedStringGlue, false, false, false);
      }
   else if (getDataSymbol()->isConstMethodType())
      {
      glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_interpreterUnresolvedMethodTypeGlue, false, false, false);
      }
   else if (getDataSymbol()->isConstMethodHandle())
      {
      glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_interpreterUnresolvedMethodHandleGlue, false, false, false);
      }
   else if (getDataSymbol()->isCallSiteTableEntry())
      {
      glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_interpreterUnresolvedCallSiteTableEntryGlue, false, false, false);
      }
   else if (getDataSymbol()->isMethodTypeTableEntry())
      {
      glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_interpreterUnresolvedMethodTypeTableEntryGlue, false, false, false);
      }
   else if (getDataSymbol()->isConstantDynamic())
      {
      glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_S390jitResolveConstantDynamicGlue, false, false, false);
      }
   else // must be static data
      {
      if (resolveForStore())
         {
         glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_S390interpreterUnresolvedStaticDataStoreGlue, false, false, false);
         }
      else
         {
         glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_S390interpreterUnresolvedStaticDataGlue, false, false, false);
         }
      }

#if !defined(PUBLIC_BUILD)
   // Generate RIOFF if RI is supported.
   cursor = generateRuntimeInstrumentationOnOffInstruction(cg(), cursor, TR::InstOpCode::RIOFF);
#endif

   // TODO: We could use LRL / LGRL here but the JIT does not guarantee that the Data Constant be 4 / 8 byte aligned,
   // so we cannot make use of these instructions in general. We should explore the idea of aligning 4 / 8 byte data
   // constants in the literal pool to a(n) 4 / 8 byte boundary such that these instructions can be exploited.

   //  Snippet body
   *(int16_t *) cursor = 0x0de0;                              // BASR   r14,0
   cursor += 2;

   if (TR::Compiler->target.is64Bit())
      {
      // LG     r14,8(,r14)
      *(uint32_t *) cursor = 0xe3e0e008;
      cursor += 4;
      *(uint16_t *) cursor = 0x0004;
      cursor += 2;
      }
   else
      {
      *(int32_t *) cursor = 0x58e0e006;                       // L      r14,6(,r14)
      cursor += 4;
      }

   *(int16_t *) cursor = 0x0dee;                              // BASR   r14,r14
   cursor += 2;

   // PicBuilder function address
   *(uintptrj_t *) cursor = (uintptrj_t) glueRef->getMethodAddress();
   AOTcgDiag1(comp, "add TR_AbsoluteHelperAddress cursor=%x\n", cursor);
   cg()->addProjectSpecializedRelocation(cursor, (uint8_t *)glueRef, NULL, TR_AbsoluteHelperAddress,
                             __FILE__, __LINE__, getNode());
   cursor += sizeof(uintptrj_t);

   // code cache RA
   *(uintptrj_t *) cursor = (uintptrj_t) (getBranchInstruction()->getNext())->getBinaryEncoding();
   AOTcgDiag1(comp, "add TR_AbsoluteMethodAddress cursor=%x\n", cursor);
   cg()->addProjectSpecializedRelocation(cursor, NULL, NULL, TR_AbsoluteMethodAddress,
                             __FILE__, __LINE__, getNode());
   cursor += sizeof(uintptrj_t);

   // cp
   if (getDataSymbolReference()->getSymbol()->isCallSiteTableEntry())
      {
      *(int32_t *)cursor = getDataSymbolReference()->getSymbol()->castToCallSiteTableEntrySymbol()->getCallSiteIndex();
      }
   else if (getDataSymbol()->isMethodTypeTableEntry())
      {
      *(int32_t *)cursor = getDataSymbolReference()->getSymbol()->castToMethodTypeTableEntrySymbol()->getMethodTypeIndex();
      }
   else // constant pool index
      {
      *(int32_t *)cursor = getDataSymbolReference()->getCPIndex();
      }

   cursor += 4;

   // address of constant pool
   *(uintptrj_t *) cursor = (uintptrj_t) getDataSymbolReference()->getOwningMethod(comp)->constantPool();
   AOTcgDiag1(comp, "add TR_ConstantPool cursor=%x\n", cursor);
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, *(uint8_t **)cursor, getNode() ? (uint8_t *)(intptr_t)getNode()->getInlinedSiteIndex() : (uint8_t *)-1, TR_ConstantPool, cg()),
                             __FILE__, __LINE__, getNode());
   cursor += sizeof(uintptrj_t);

   // referencing instruction that needs patching
   if (getDataReferenceInstruction() != NULL)
      {
      *(uintptrj_t *) cursor = (uintptrj_t) (getDataReferenceInstruction()->getBinaryEncoding());
      }
   else
      {
      *(uintptrj_t *) cursor = (uintptrj_t) (getBranchInstruction()->getNext())->getBinaryEncoding();
      }
   AOTcgDiag1(comp, "add TR_AbsoluteMethodAddress cursor=%x\n", cursor);
   cg()->addProjectSpecializedRelocation(cursor, NULL, NULL, TR_AbsoluteMethodAddress,
                             __FILE__, __LINE__, getNode());
   cursor += sizeof(uintptrj_t);

   // Literal Pool Address to patch.
   *(uintptrj_t *) cursor = 0x0;
   setLiteralPoolPatchAddress(cursor);
   AOTcgDiag1(comp, "add TR_AbsoluteMethodAddress cursor=%x\n", cursor);
   cg()->addProjectSpecializedRelocation(cursor, NULL, NULL, TR_AbsoluteMethodAddress,
                             __FILE__, __LINE__, getNode());
   cursor += sizeof(uintptrj_t);


   // Instance data load and stores require patching of the displacement field
   // of the respective load / store instruction.  In the event that the
   // resolved offset is greater than displacement, we provide an out-of-line
   // instruction sequence to allow for large displacement calculation.
   // The sequence that is generated is:
   //         DC     Address of Offset
   //         DC     Offset to patch BRCL to helper
   //         BASR    R14, 0
   //         L/LGF   R14, Offset(R14)
   //         AR/AGR  R14, Rbase
   //         BRCL    Return to mainline code
   //         -- Padding for word alignment --
   //         DC      _ResolvedOffset  // 32-bits
   //  where Load/Store is the original load/store instruction in mainline
   //  sequence, and Rtarget, Rbase and Rindex are its respective real registers.
   //  The sequence will return to the point after the mainline instruction.
   if (isInstanceData()) {
      TR::Instruction* loadStore = getDataReferenceInstruction();
      TR_ASSERT( getDataReferenceInstruction() != NULL, "Expected data reference instruction.");
      TR::MemoryReference* mr = loadStore->getMemoryReference();
      TR_ASSERT(mr != NULL , "Memory Reference expected.");
      TR::RealRegister::RegNum base  = (mr->getBaseRegister() ? toRealRegister(mr->getBaseRegister())->getRegisterNumber() : TR::RealRegister::NoReg);

      // Get PC for relative load of the resolved offset.
      uint8_t* offsetMarker = cursor;                        // DC Address of Offset
      cursor += sizeof(intptrj_t);

      // Get PC for relative load of the resolved offset.
      // We need to use getNext - 6 because the branch instruction may have padding before it.
      *(int32_t *) cursor = (int32_t)((cursor + sizeof(int32_t) - getBranchInstruction()->getNext()->getBinaryEncoding() + 6) / 2);
      cursor += sizeof(int32_t);

      // Get PC for relative load of the resolved offset.
      *(int16_t *) cursor = 0x0de0;                              // BASR R14, 0
      cursor += sizeof(int16_t);

      // Load the resolved offset into R14.
      // Add the resolved offset into base register.
      uint8_t* offsetLoad = cursor;
      if (TR::Compiler->target.is64Bit())
         {
         *(int32_t *) cursor = 0xe3e0e000;                       // 64Bit: LGF R14, (R14)
         cursor += sizeof(int32_t);
         *(int16_t *) cursor = 0x0014;
         cursor += sizeof(int16_t);

         *(int32_t *) cursor = 0xb90800e0;                       // 64Bit: AGR R14, Rbase
         TR::RealRegister::setRegisterField((uint32_t*)cursor, 0, base);
         cursor += sizeof(int32_t);
         }
      else
         {
         *(int32_t *) cursor = 0x58e0e000;                       // 31Bit: L   R14,6(R14)
         cursor += sizeof(int32_t);

         *(uint32_t *) cursor = (int32_t)0x1ae00000;             // 31Bit: AR  R14, Rbase
         TR::RealRegister::setRegisterField((uint32_t*)cursor, 4, base);
         cursor += sizeof(int16_t);
         }

      uint8_t*  returnAddress = (getBranchInstruction()->getNext())->getBinaryEncoding();
      // Return to instruction after load.
      *(int16_t *) cursor = 0xC0F4;                              // BRCL return address
      cursor += sizeof(int16_t);
      *(int32_t *) cursor = (int32_t)((returnAddress - (cursor - 2)) / 2);
      cursor += sizeof(int32_t);

      // Padding to ensure resolved offset slot is aligned.
      if ((uintptrj_t)cursor % 4 != 0)
         cursor += sizeof(int16_t);

      *(int32_t *) cursor = (int32_t)0x0badbeef;                 // DC Offset

      // Store pointer to resolved offset slot
      // code cache RA
      *(uintptrj_t *) offsetMarker = (uintptrj_t) cursor;
      AOTcgDiag1(comp, "add TR_AbsoluteMethodAddress offsetMarker=%x\n", offsetMarker);
      cg()->addProjectSpecializedRelocation(offsetMarker, NULL, NULL, TR_AbsoluteMethodAddress,
                                __FILE__, __LINE__, getNode());

      *(int32_t *) offsetLoad |= (cursor-offsetLoad);

      cursor += sizeof(int32_t);

   }

   return cursor;
   }


uint32_t
J9::Z::UnresolvedDataSnippet::getLength(int32_t  estimatedSnippetStart)
   {
   uint32_t length = (TR::Compiler->target.is64Bit() ? (14 + 5 * sizeof(uintptrj_t)) : (12 + 5 * sizeof(uintptrj_t)));
   // For instance snippets, we have the out-of-line sequence
   if (isInstanceData())
      length += (TR::Compiler->target.is64Bit()) ? 36 : 28;

#if !defined(PUBLIC_BUILD)
   length += getRuntimeInstrumentationOnOffInstructionLength(cg());
#endif

   return length;
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::UnresolvedDataSnippet * snippet)
   {

   uint8_t * bufferPos = snippet->getSnippetLabel()->getCodeLocation();
   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, "Unresolved Data Snippet");

   bufferPos = printRuntimeInstrumentationOnOffInstruction(pOutFile, bufferPos, false); // RIOFF

   // TODO: We could use LRL / LGRL here but the JIT does not guarantee that the Data Constant be 4 / 8 byte aligned,
   // so we cannot make use of these instructions in general. We should explore the idea of aligning 4 / 8 byte data
   // constants in the literal pool to a(n) 4 / 8 byte boundary such that these instructions can be exploited.

   printPrefix(pOutFile, NULL, bufferPos, 2);
   trfprintf(pOutFile, "BASR \tGPR14, 0");
   bufferPos += 2;

   if (TR::Compiler->target.is64Bit())
      {
      printPrefix(pOutFile, NULL, bufferPos, 6);
      trfprintf(pOutFile, "LG   \tGPR14, 6(,GPR14)");
      bufferPos += 6;
      }
   else
      {
      printPrefix(pOutFile, NULL, bufferPos, 4);
      trfprintf(pOutFile, "L   \tGPR14, 6(,GPR14)");
      bufferPos += 4;
      }

   printPrefix(pOutFile, NULL, bufferPos, 2);
   trfprintf(pOutFile, "BASR \tGPR14, GPR14");
   bufferPos += 2;

   TR::SymbolReference * glueRef;
   if (snippet->getDataSymbol()->getShadowSymbol())
      {
      if (snippet->resolveForStore())
         {
         glueRef = _cg->getSymRef(TR_S390interpreterUnresolvedInstanceDataStoreGlue);
         }
      else
         {
         glueRef = _cg->getSymRef(TR_S390interpreterUnresolvedInstanceDataGlue);
         }
      }
   else if (snippet->getDataSymbol()->isClassObject())
      {
      if (snippet->getDataSymbol()->addressIsCPIndexOfStatic())
         {
         glueRef = _cg->getSymRef(TR_S390interpreterUnresolvedClassGlue2);
         }
      else
         {
         glueRef = _cg->getSymRef(TR_S390interpreterUnresolvedClassGlue);
         }
      }
   else if (snippet->getDataSymbol()->isConstString())
      {
      glueRef = _cg->getSymRef(TR_S390interpreterUnresolvedStringGlue);
      }
   else if (snippet->getDataSymbol()->isConstMethodHandle())
      {
      glueRef = _cg->getSymRef(TR_interpreterUnresolvedMethodHandleGlue);
      }
   else if (snippet->getDataSymbol()->isConstMethodType())
      {
      glueRef = _cg->getSymRef(TR_interpreterUnresolvedMethodTypeGlue);
      }
   else if (snippet->getDataSymbol()->isCallSiteTableEntry())
      {
      glueRef = _cg->getSymRef(TR_interpreterUnresolvedCallSiteTableEntryGlue);
      }
   else if (snippet->getDataSymbol()->isMethodTypeTableEntry())
      {
      glueRef = _cg->getSymRef(TR_interpreterUnresolvedMethodTypeTableEntryGlue);
      }
   else if (snippet->getDataSymbol()->isConstantDynamic())
      {
      glueRef = _cg->getSymRef(TR_S390jitResolveConstantDynamicGlue);
      }
   else // the data symbol is static
      {
      if (snippet->resolveForStore())
         {
         glueRef = _cg->getSymRef(TR_S390interpreterUnresolvedStaticDataStoreGlue);
         }
      else
         {
         glueRef = _cg->getSymRef(TR_S390interpreterUnresolvedStaticDataGlue);
         }
      }


   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC    \t%s", getName(glueRef));
   bufferPos += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC    \t%p \t# Return Address",
            (intptrj_t) (snippet->getBranchInstruction()->getNext())->getBinaryEncoding());
   bufferPos += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "DC    \t0x%08x \t# Constant Pool Index", snippet->getDataSymbolReference()->getCPIndex());
   bufferPos += 4;

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC    \t0x%p \t# Address Of Constant Pool",
            (intptrj_t) getOwningMethod(snippet->getDataSymbolReference())->constantPool());
   bufferPos += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   uintptrj_t addr;

   if (snippet->getDataReferenceInstruction() != NULL)
      {
      addr = (uintptrj_t) (snippet->getDataReferenceInstruction()->getBinaryEncoding());
      }
   else
      {
      addr = (uintptrj_t) (snippet->getBranchInstruction()->getNext())->getBinaryEncoding();
      }
   trfprintf(pOutFile, "DC    \t0x%p \t# Address Of Ref. Instruction", addr);

   bufferPos += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC   \t0x%p \t# Address Of Literal Pool Slot",
           (intptrj_t)(snippet->getLiteralPoolSlot()));
   bufferPos += sizeof(intptrj_t);

   // Snippet has out-of-line sequence for large offsets for instance data
   if (snippet->isInstanceData())
      {
      printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
      trfprintf(pOutFile, "DC   \t0x%p \t# Address to large offset slot",
            (intptrj_t)((intptrj_t*)bufferPos));
      bufferPos += sizeof(intptrj_t);

      printPrefix(pOutFile, NULL, bufferPos, sizeof(int32_t));
      trfprintf(pOutFile, "DC   \t0x%p \t# Displacement from helper branch to out-of-line sequence",
            (int32_t)((intptrj_t)bufferPos + sizeof(int32_t) - (intptrj_t)snippet->getBranchInstruction()->getBinaryEncoding() +6));
      bufferPos += sizeof(int32_t);

      printPrefix(pOutFile, NULL, bufferPos, 2);
      trfprintf(pOutFile, "BASR \tGPR14, 0");
      bufferPos += 2;

      if (TR::Compiler->target.is64Bit())
         {
         printPrefix(pOutFile, NULL, bufferPos, 6);
         trfprintf(pOutFile, "LGF   \tGPR14, Offset(,GPR14)");
         bufferPos += 6;
         printPrefix(pOutFile, NULL, bufferPos, 4);
         trfprintf(pOutFile, "AGR   \tGPR14, GPRbase");
         bufferPos += 4;
         }
      else
         {
         printPrefix(pOutFile, NULL, bufferPos, 4);
         trfprintf(pOutFile, "L   \tGPR14, Offset(,GPR14)");
         bufferPos += 4;
         printPrefix(pOutFile, NULL, bufferPos, 2);
         trfprintf(pOutFile, "AR  \tGPR14, GPRbase");
         bufferPos += 2;
         }

      printPrefix(pOutFile, NULL, bufferPos, 6);
      trfprintf(pOutFile, "BRCL \t<%p>\t# Return to Main Code",
                             snippet->getBranchInstruction()->getNext()->getBinaryEncoding());
      bufferPos += 6;

      if ((uintptrj_t)bufferPos % 4 != 0)
         {
         printPrefix(pOutFile, NULL, bufferPos, 2);
         trfprintf(pOutFile, "\t\t# 2 byte padding");
         bufferPos += 2;
         }

      printPrefix(pOutFile, NULL, bufferPos, sizeof(int32_t));
      trfprintf(pOutFile, "DC   \t0x%p \t# Offset slot",
            (int32_t)(*(int32_t*)bufferPos));
      bufferPos += sizeof(int32_t);
      }
   }

bool
J9::Z::UnresolvedDataSnippet::isInstanceData()
   {
   return getDataSymbol()->getShadowSymbol() != NULL;
   }


TR::S390WritableDataSnippet *
J9::Z::UnresolvedDataSnippet::createUnresolvedData(
      TR::CodeGenerator *cg,
      TR::Node * n)
   {
   _unresolvedData = cg->CreateWritableConstant(n);
   return _unresolvedData;
   }
