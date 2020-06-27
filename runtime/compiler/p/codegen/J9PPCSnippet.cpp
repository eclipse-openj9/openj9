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

#include "p/codegen/J9PPCSnippet.hpp"

#include <stdint.h>
#include "j9.h"
#include "thrdsup.h"
#include "thrtypes.h"
#include "codegen/CodeGenerator.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "codegen/SnippetGCMap.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/DataTypes.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "p/codegen/PPCEvaluator.hpp"
#include "p/codegen/PPCInstruction.hpp"
#include "p/codegen/GenerateInstructions.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheManager.hpp"

#if defined(TR_HOST_POWER)
extern uint32_t getPPCCacheLineSize();
#else
uint32_t getPPCCacheLineSize()
   {
   return 32;
   }
#endif

TR::PPCReadMonitorSnippet::PPCReadMonitorSnippet(
   TR::CodeGenerator   *codeGen,
   TR::Node            *monitorEnterNode,
   TR::Node            *monitorExitNode,
   TR::LabelSymbol      *recurCheckLabel,
   TR::LabelSymbol      *monExitCallLabel,
   TR::LabelSymbol      *restartLabel,
   TR::InstOpCode::Mnemonic       loadOpCode,
   int32_t             loadOffset,
   TR::Register        *objectClassReg)
   : _monitorEnterHelper(monitorEnterNode->getSymbolReference()),
     _recurCheckLabel(recurCheckLabel),
     _loadOpCode(loadOpCode),
     _loadOffset(loadOffset),
     _objectClassReg(objectClassReg),
     TR::PPCHelperCallSnippet(codeGen, monitorExitNode, monExitCallLabel, monitorExitNode->getSymbolReference(), restartLabel)
   {
   recurCheckLabel->setSnippet(this);
   // Helper call, preserves all registers
   //
   gcMap().setGCRegisterMask(0xFFFFFFFF);
   }

uint8_t *TR::PPCReadMonitorSnippet::emitSnippetBody()
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());


   // The 32-bit code for the snippet looks like:
   // recurCheckLabel:
   //    rlwinm  monitorReg, monitorReg, 0, LOCK_THREAD_PTR_MASK
   //    cmp     cndReg, metaReg, monitorReg
   //    bne     cndReg, slowPath
   //    <load>
   //    b       restartLabel
   // slowPath:
   //    bl   monitorEnterHelper
   //    <load>
   //    bl   monitorExitHelper
   //    b    restartLabel;

   // for 64-bit the rlwinm is replaced with:
   //    rldicr  threadReg, monitorReg, 0, (long) LOCK_THREAD_PTR_MASK

   TR::RegisterDependencyConditions *deps = getRestartLabel()->getInstruction()->getDependencyConditions();

   TR::RealRegister *metaReg  = cg()->getMethodMetaDataRegister();
   TR::RealRegister *monitorReg  = cg()->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(0)->getRealRegister());
   TR::RealRegister *cndReg  = cg()->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(2)->getRealRegister());
   TR::RealRegister *loadResultReg  = cg()->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(3)->getRealRegister());
   bool                isResultCollectable = deps->getPostConditions()->getRegisterDependency(3)->getRegister()->containsCollectedReference();
   TR::RealRegister *loadBaseReg  = cg()->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(4)->getRealRegister());
   TR::Compilation *comp = cg()->comp();
   TR::InstOpCode opcode;

   uint8_t *buffer = cg()->getBinaryBufferCursor();

   _recurCheckLabel->setCodeLocation(buffer);

   if (comp->target().is64Bit())
      {
      opcode.setOpCodeValue(TR::InstOpCode::rldicr);
      buffer = opcode.copyBinaryToBuffer(buffer);
      monitorReg->setRegisterFieldRA((uint32_t *)buffer);
      monitorReg->setRegisterFieldRS((uint32_t *)buffer);
      // sh = 0
      // assumption here that thread pointer is in upper bits, so MB = 0
      // ME = 32 + LOCK_LAST_RECURSION_BIT_NUMBER - 1
      int32_t ME = 32 + LOCK_LAST_RECURSION_BIT_NUMBER - 1;
      int32_t me_field_encoding = (ME >> 5) | ((ME & 0x1F) << 1);
      *(int32_t *)buffer |= (me_field_encoding << 5);
      }
   else
      {
      opcode.setOpCodeValue(TR::InstOpCode::rlwinm);
      buffer = opcode.copyBinaryToBuffer(buffer);
      monitorReg->setRegisterFieldRA((uint32_t *)buffer);
      monitorReg->setRegisterFieldRS((uint32_t *)buffer);
      // sh = 0
      // assumption here that thread pointer is in upper bits, so MB = 0
      // ME = LOCK_LAST_RECURSION_BIT_NUMBER - 1
      *(int32_t *)buffer |= ((LOCK_LAST_RECURSION_BIT_NUMBER - 1) << 1);
      }
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(TR::InstOpCode::Op_cmp);
   buffer = opcode.copyBinaryToBuffer(buffer);
   cndReg->setRegisterFieldRT((uint32_t *)buffer);
   metaReg->setRegisterFieldRA((uint32_t *)buffer);
   monitorReg->setRegisterFieldRB((uint32_t *)buffer);
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(TR::InstOpCode::bne);
   buffer = opcode.copyBinaryToBuffer(buffer);
   cndReg->setRegisterFieldBI((uint32_t *)buffer);
   *(int32_t *)buffer |= 12;
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(_loadOpCode);
   buffer = opcode.copyBinaryToBuffer(buffer);
   loadResultReg->setRegisterFieldRT((uint32_t *)buffer);
   loadBaseReg->setRegisterFieldRA((uint32_t *)buffer);
   *(int32_t *)buffer |= _loadOffset & 0xFFFF;
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(TR::InstOpCode::b);
   buffer = opcode.copyBinaryToBuffer(buffer);
   *(int32_t *)buffer |= (getRestartLabel()->getCodeLocation()-buffer) & 0x03FFFFFC;
   buffer += PPC_INSTRUCTION_LENGTH;

   intptr_t helperAddress = (intptr_t)getMonitorEnterHelper()->getSymbol()->castToMethodSymbol()->getMethodAddress();
   if (cg()->directCallRequiresTrampoline(helperAddress, (intptr_t)buffer))
      {
      helperAddress = TR::CodeCacheManager::instance()->findHelperTrampoline(getMonitorEnterHelper()->getReferenceNumber(), (void *)buffer);
      TR_ASSERT_FATAL(comp->target().cpu.isTargetWithinIFormBranchRange(helperAddress, (intptr_t)buffer), "Helper address is out of range");
      }

   opcode.setOpCodeValue(TR::InstOpCode::bl);
   buffer = opcode.copyBinaryToBuffer(buffer);

   if (comp->compileRelocatableCode())
      {
      cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(buffer,(uint8_t *)getMonitorEnterHelper(),TR_HelperAddress, cg()),
                                __FILE__, __LINE__, getNode());
      }

   *(int32_t *)buffer |= (helperAddress - (intptr_t)buffer) & 0x03FFFFFC;
   buffer += PPC_INSTRUCTION_LENGTH;

   gcMap().registerStackMap(buffer, cg());

   opcode.setOpCodeValue(_loadOpCode);
   buffer = opcode.copyBinaryToBuffer(buffer);
   loadResultReg->setRegisterFieldRT((uint32_t *)buffer);
   loadBaseReg->setRegisterFieldRA((uint32_t *)buffer);
   *(int32_t *)buffer |= _loadOffset & 0xFFFF;
   buffer += PPC_INSTRUCTION_LENGTH;

   // this will call jitMonitorExit and return to the restart label
   cg()->setBinaryBufferCursor(buffer);

   // Defect 101811
   TR_GCStackMap *exitMap = gcMap().getStackMap()->clone(cg()->trMemory());
   exitMap->setByteCodeInfo(getNode()->getByteCodeInfo());
   if (isResultCollectable)
      exitMap->setRegisterBits(cg()->registerBitMask((int)deps->getPostConditions()->getRegisterDependency(3)->getRealRegister()));

   // Throw away entry map
   gcMap().setStackMap(exitMap);
   buffer = TR::PPCHelperCallSnippet::emitSnippetBody();

   return buffer;
   }

void
TR::PPCReadMonitorSnippet::print(TR::FILE *pOutFile, TR_Debug *debug)
   {
   TR::Compilation *comp = cg()->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());
   uint8_t *cursor = getRecurCheckLabel()->getCodeLocation();

   debug->printSnippetLabel(pOutFile, getRecurCheckLabel(), cursor, "Read Monitor Snippet");

   TR::RegisterDependencyConditions *deps = getRestartLabel()->getInstruction()->getDependencyConditions();

   TR::Machine *machine = cg()->machine();
   TR::RealRegister *metaReg    = cg()->getMethodMetaDataRegister();
   TR::RealRegister *monitorReg  = machine->getRealRegister(deps->getPostConditions()->getRegisterDependency(1)->getRealRegister());
   TR::RealRegister *condReg  = machine->getRealRegister(deps->getPostConditions()->getRegisterDependency(2)->getRealRegister());
   TR::RealRegister *loadResultReg  = machine->getRealRegister(deps->getPostConditions()->getRegisterDependency(3)->getRealRegister());
   TR::RealRegister *loadBaseReg  = machine->getRealRegister(deps->getPostConditions()->getRegisterDependency(4)->getRealRegister());

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   if (comp->target().is64Bit())
      trfprintf(pOutFile, "rldicr \t%s, %s, 0, " INT64_PRINTF_FORMAT_HEX "\t; Get owner thread value", debug->getName(monitorReg), debug->getName(monitorReg), (int64_t) LOCK_THREAD_PTR_MASK);
   else
      trfprintf(pOutFile, "rlwinm \t%s, %s, 0, 0x%x\t; Get owner thread value", debug->getName(monitorReg), debug->getName(monitorReg), LOCK_THREAD_PTR_MASK);
   cursor+= 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   if (comp->target().is64Bit())
      trfprintf(pOutFile, "cmp8 \t%s, %s, %s\t; Compare VMThread to owner thread", debug->getName(condReg), debug->getName(metaReg), debug->getName(monitorReg));
   else
      trfprintf(pOutFile, "cmp4 \t%s, %s, %s\t; Compare VMThread to owner thread", debug->getName(condReg), debug->getName(metaReg), debug->getName(monitorReg));
   cursor+= 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   int32_t distance = *((int32_t *) cursor) & 0x0000fffc;
   distance = (distance << 16) >> 16;   // sign extend
   trfprintf(pOutFile, "bne %s, " POINTER_PRINTF_FORMAT "\t; Use Helpers", debug->getName(condReg), (intptr_t)cursor + distance);
   cursor+= 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "%s \t%s, [%s, %d]\t; Load", TR::InstOpCode::metadata[getLoadOpCode()].name, debug->getName(loadResultReg), debug->getName(loadBaseReg), getLoadOffset());
   cursor+= 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   distance = *((int32_t *) cursor) & 0x03fffffc;
   distance = (distance << 6) >> 6;   // sign extend
   trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t\t; ", (intptr_t)cursor + distance);
   debug->print(pOutFile, getRestartLabel());
   cursor+= 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   distance = *((int32_t *) cursor) & 0x03fffffc;
   distance = (distance << 6) >> 6;   // sign extend
   trfprintf(pOutFile, "bl \t" POINTER_PRINTF_FORMAT "\t\t; %s", (intptr_t)cursor + distance, debug->getName(getMonitorEnterHelper()));
   if (debug->isBranchToTrampoline(getMonitorEnterHelper(), cursor, distance))
      trfprintf(pOutFile, " Through trampoline");
   cursor+= 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "%s \t%s, [%s, %d]\t; Load", TR::InstOpCode::metadata[getLoadOpCode()].name, debug->getName(loadResultReg), debug->getName(loadBaseReg), getLoadOffset());

   debug->print(pOutFile, (TR::PPCHelperCallSnippet *)this);
   }

uint32_t TR::PPCReadMonitorSnippet::getLength(int32_t estimatedSnippetStart)
   {
   int32_t len = 28;
   len += TR::PPCHelperCallSnippet::getLength(estimatedSnippetStart+len);
   return len;
   }

int32_t TR::PPCReadMonitorSnippet::setEstimatedCodeLocation(int32_t estimatedSnippetStart)
   {
   _recurCheckLabel->setEstimatedCodeLocation(estimatedSnippetStart);
   getSnippetLabel()->setEstimatedCodeLocation(estimatedSnippetStart+28);
   return(estimatedSnippetStart);
   }

TR::PPCAllocPrefetchSnippet::PPCAllocPrefetchSnippet(
   TR::CodeGenerator   *codeGen,
   TR::Node            *node,
   TR::LabelSymbol      *callLabel)
     : TR::Snippet(codeGen, node, callLabel, false)
   {
   }

uint32_t TR::getCCPreLoadedCodeSize()
   {
   uint32_t size = 0;

   // XXX: Can't check if processor supports transient at this point because processor type hasn't been determined
   //      so we have to allocate for the larger of the two scenarios
   if (false)
      {
      const uint32_t linesToPrefetch = TR::Options::_TLHPrefetchLineCount > 0 ? TR::Options::_TLHPrefetchLineCount : 8;
      size += (linesToPrefetch + 1) / 2 * 4;
      }
   else
      {
      static bool l3SkipLines = feGetEnv("TR_l3SkipLines") != NULL;
      const uint32_t linesToLdPrefetch = TR::Options::_TLHPrefetchLineCount > 0 ? TR::Options::_TLHPrefetchLineCount : 4;
      const uint32_t linesToStPrefetch = linesToLdPrefetch * 2;
      size += 4 + (linesToLdPrefetch + 1) / 2 * 4 + 1 + (l3SkipLines ? 2 : 0) + (linesToStPrefetch + 1) / 2 * 4;
      }
   size += 3;

   //if (!TR::CodeGenerator::supportsTransientPrefetch() && !doL1Pref)
   // XXX: Can't check if processor supports transient at this point because processor type hasn't been determined
   //      so we have to allocate for the larger of the two scenarios
   if (false)
      {
      const uint32_t linesToPrefetch = TR::Options::_TLHPrefetchLineCount > 0 ? TR::Options::_TLHPrefetchLineCount : 8;
      size += (linesToPrefetch + 1) / 2 * 4;
      }
   else
      {
      static bool l3SkipLines = feGetEnv("TR_l3SkipLines") != NULL;
      const uint32_t linesToLdPrefetch = TR::Options::_TLHPrefetchLineCount > 0 ? TR::Options::_TLHPrefetchLineCount : 4;
      const uint32_t linesToStPrefetch = linesToLdPrefetch * 2;
      size += 4 + (linesToLdPrefetch + 1) / 2 * 4 + 1 + (l3SkipLines ? 2 : 0) + (linesToStPrefetch + 1) / 2 * 4;
      }
   size += 3;

   //TR_writeBarrier/TR_writeBarrierAndCardMark/TR_cardMark
   size += 12;
   if (TR::Options::getCmdLineOptions()->getGcCardSize() > 0)
      size += 19 + (TR::Compiler->om.writeBarrierType() != gc_modron_wrtbar_cardmark_incremental ? 13 : 10);

#if defined(TR_TARGET_32BIT)
   // If heap base and/or size is constant we can materialize them with 1 or 2 instructions
   // Assume 2 instructions, which means we want space for 1 additional instruction
   // for both TR_writeBarrier and TR_writeBarrierAndCardMark
   if (!TR::Options::getCmdLineOptions()->isVariableHeapBaseForBarrierRange0())
      size += 2;
   if (!TR::Options::getCmdLineOptions()->isVariableHeapSizeForBarrierRange0())
      size += 2;
#endif

   //TR_arrayStoreCHK
   size += 25;

   // Add size for other helpers

   // Eyecatchers, one per helper
   size += TR_numCCPreLoadedCode;

   return TR::alignAllocationSize<8>(size * PPC_INSTRUCTION_LENGTH);
   }

#ifdef __LITTLE_ENDIAN__
#define CCEYECATCHER(a, b, c, d) (((a) << 0) | ((b) << 8) | ((c) << 16) | ((d) << 24))
#else
#define CCEYECATCHER(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | ((d) << 0))
#endif

static void performCCPreLoadedBinaryEncoding(uint8_t *buffer, TR::CodeGenerator *cg)
   {
   cg->setBinaryBufferStart(buffer);
   cg->setBinaryBufferCursor(buffer);
   for (TR::Instruction *i = cg->getFirstInstruction(); i != NULL; i = i->getNext())
      {
      i->estimateBinaryLength(cg->getBinaryBufferCursor() - cg->getBinaryBufferStart());
      cg->setBinaryBufferCursor(i->generateBinaryEncoding());
      }
   }

static uint8_t* initializeCCPreLoadedPrefetch(uint8_t *buffer, void **CCPreLoadedCodeTable, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node *n = cg->getFirstInstruction()->getNode();

   // Prefetch helper; prefetches a number of lines and returns directly to JIT code
   // In:
   //   r8 = object ptr
   // Out:
   //   r8 = object ptr
   // Clobbers:
   //   r10, r11
   //   cr0

   cg->setFirstInstruction(NULL);
   cg->setAppendInstruction(NULL);

   TR::Instruction *eyecatcher = generateImmInstruction(cg, TR::InstOpCode::dd, n, CCEYECATCHER('C', 'C', 'H', 5));

   TR::LabelSymbol *entryLabel = generateLabelSymbol(cg);
   TR::Instruction *entry = generateLabelInstruction(cg, TR::InstOpCode::label, n, entryLabel);
   TR::Instruction *cursor = entry;

   TR::Register *metaReg = cg->getMethodMetaDataRegister();
   TR::Register *r8 = cg->machine()->getRealRegister(TR::RealRegister::gr8);
   TR::Register *r10 = cg->machine()->getRealRegister(TR::RealRegister::gr10);
   TR::Register *r11 = cg->machine()->getRealRegister(TR::RealRegister::gr11);
   TR::Register *cr0 = cg->machine()->getRealRegister(TR::RealRegister::cr0);

   static bool    doL1Pref = feGetEnv("TR_doL1Prefetch") != NULL;
   const uint32_t ppcCacheLineSize = getPPCCacheLineSize();
   uint32_t       helperSize;

   if (!TR::CodeGenerator::supportsTransientPrefetch() && !doL1Pref)
      {
      const uint32_t linesToPrefetch = TR::Options::_TLHPrefetchLineCount > 0 ? TR::Options::_TLHPrefetchLineCount : 8;
      const uint32_t restartAfterLines = TR::Options::_TLHPrefetchBoundaryLineCount > 0 ?  TR::Options::_TLHPrefetchBoundaryLineCount : 8;
      const uint32_t skipLines = TR::Options::_TLHPrefetchStaggeredLineCount > 0  ? TR::Options::_TLHPrefetchStaggeredLineCount : 4;
      helperSize = (linesToPrefetch + 1) / 2 * 4;

      TR_ASSERT_FATAL( (skipLines + 1) * ppcCacheLineSize <= UPPER_IMMED, "tlhPrefetchStaggeredLineCount (%u) is too high. Will cause imm field to overflow.", skipLines);
      TR_ASSERT_FATAL( restartAfterLines * ppcCacheLineSize <= UPPER_IMMED, "tlhPrefetchBoundaryLineCount (%u) is too high. Will cause imm field to overflow.", restartAfterLines);

      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r8, skipLines * ppcCacheLineSize, cursor);
      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r11, r8, (skipLines + 1) * ppcCacheLineSize, cursor);
      cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtst, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r10, 4, cg), cursor);
      cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtst, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r11, 4, cg), cursor);

      for (uint32_t i = 2; i < linesToPrefetch; i += 2)
         {
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r10, ppcCacheLineSize * 2, cursor);
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r11, r11, ppcCacheLineSize * 2, cursor);
         cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtst, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r10, 4, cg), cursor);
         cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtst, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r11, 4, cg), cursor);
         }

      cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, n, r11, restartAfterLines * ppcCacheLineSize, cursor);
      cursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, tlhPrefetchFTA), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          r11, cursor);
      }
   else
      {
      // Transient version
      static const char *s = feGetEnv("TR_l3SkipLines");
      static uint32_t l3SkipLines = s ? atoi(s) : 0;
      const uint32_t linesToLdPrefetch = TR::Options::_TLHPrefetchLineCount > 0 ? TR::Options::_TLHPrefetchLineCount : 4;
      const uint32_t linesToStPrefetch = linesToLdPrefetch * 2;
      const uint32_t restartAfterLines = TR::Options::_TLHPrefetchBoundaryLineCount > 0 ?  TR::Options::_TLHPrefetchBoundaryLineCount : 4;
      const uint32_t skipLines = TR::Options::_TLHPrefetchStaggeredLineCount > 0  ? TR::Options::_TLHPrefetchStaggeredLineCount : 4;
      helperSize = 4 + (linesToLdPrefetch + 1) / 2 * 4 + 1 + (l3SkipLines ? 2 : 0) + (linesToStPrefetch + 1) / 2 * 4;

      TR_ASSERT_FATAL( (skipLines + 1) * ppcCacheLineSize <= UPPER_IMMED, "tlhPrefetchStaggeredLineCount (%u) is too high. Will cause imm field to overflow.", skipLines);
      TR_ASSERT_FATAL( restartAfterLines * ppcCacheLineSize <= UPPER_IMMED, "tlhPrefetchBoundaryLineCount (%u) is too high. Will cause imm field to overflow.", restartAfterLines);

      cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r10,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, debugEventData3), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          cursor);
      cursor = generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, n, cr0, r10, 0, cursor);
      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xori, n, r10, r10, 1, cursor);
      cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, n, r11, restartAfterLines * ppcCacheLineSize, cursor);
      cursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, tlhPrefetchFTA), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          r11, cursor);
      cursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, debugEventData3), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          r10, cursor);

      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r8, skipLines * ppcCacheLineSize, cursor);
      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r11, r8, (skipLines + 1) * ppcCacheLineSize, cursor);
      cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtt, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r10, 4, cg), cursor);
      cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtt, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r11, 4, cg), cursor);

      for (uint32_t i = 2; i < linesToLdPrefetch; i += 2)
         {
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r10, ppcCacheLineSize * 2, cursor);
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r11, r11, ppcCacheLineSize * 2, cursor);
         cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtt, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r10, 4, cg), cursor);
         cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtt, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r11, 4, cg), cursor);
         }

      cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beqlr, n, NULL, cr0, cursor);

      if (l3SkipLines > 0)
         {
         TR_ASSERT_FATAL( ppcCacheLineSize * l3SkipLines <= UPPER_IMMED, "TR_l3SkipLines (%u) is too high. Will cause imm field to overflow.", l3SkipLines);
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r10, ppcCacheLineSize * l3SkipLines, cursor);
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r11, r11, ppcCacheLineSize * l3SkipLines, cursor);
         }

      for (uint32_t i = 0; i < linesToStPrefetch; i += 2)
         {
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r10, ppcCacheLineSize * 2, cursor);
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r11, r11, ppcCacheLineSize * 2, cursor);
         cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtstt, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r10, 4, cg), cursor);
         cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtstt, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r11, 4, cg), cursor);
         }
      }

   cursor = generateInstruction(cg, TR::InstOpCode::blr, n, cursor);

   performCCPreLoadedBinaryEncoding(buffer, cg);

   helperSize += 3;
   TR_ASSERT(cg->getBinaryBufferCursor() - entryLabel->getCodeLocation() == helperSize * PPC_INSTRUCTION_LENGTH,
           "Per-codecache prefetch helper, unexpected size");

   CCPreLoadedCodeTable[TR_AllocPrefetch] = entryLabel->getCodeLocation();

   return cg->getBinaryBufferCursor() - PPC_INSTRUCTION_LENGTH;
   }

static uint8_t* initializeCCPreLoadedNonZeroPrefetch(uint8_t *buffer, void **CCPreLoadedCodeTable, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node *n = cg->getFirstInstruction()->getNode();

   // NonZero TLH Prefetch helper; prefetches a number of lines and returns directly to JIT code
   // In:
   //   r8 = object ptr
   // Out:
   //   r8 = object ptr
   // Clobbers:
   //   r10, r11
   //   cr0

   cg->setFirstInstruction(NULL);
   cg->setAppendInstruction(NULL);

   TR::Instruction *eyecatcher = generateImmInstruction(cg, TR::InstOpCode::dd, n, CCEYECATCHER('C', 'C', 'H', 6));

   TR::LabelSymbol *entryLabel = generateLabelSymbol(cg);
   TR::Instruction *entry = generateLabelInstruction(cg, TR::InstOpCode::label, n, entryLabel);
   TR::Instruction *cursor = entry;

   TR::Register *metaReg = cg->getMethodMetaDataRegister();
   TR::Register *r8 = cg->machine()->getRealRegister(TR::RealRegister::gr8);
   TR::Register *r10 = cg->machine()->getRealRegister(TR::RealRegister::gr10);
   TR::Register *r11 = cg->machine()->getRealRegister(TR::RealRegister::gr11);
   TR::Register *cr0 = cg->machine()->getRealRegister(TR::RealRegister::cr0);

   static bool    doL1Pref = feGetEnv("TR_doL1Prefetch") != NULL;
   const uint32_t ppcCacheLineSize = getPPCCacheLineSize();
   uint32_t       helperSize;

   if (!TR::CodeGenerator::supportsTransientPrefetch() && !doL1Pref)
      {
      const uint32_t linesToPrefetch = TR::Options::_TLHPrefetchLineCount > 0 ? TR::Options::_TLHPrefetchLineCount : 8;
      const uint32_t restartAfterLines = TR::Options::_TLHPrefetchBoundaryLineCount > 0 ?  TR::Options::_TLHPrefetchBoundaryLineCount : 8;
      const uint32_t skipLines = TR::Options::_TLHPrefetchStaggeredLineCount > 0  ? TR::Options::_TLHPrefetchStaggeredLineCount : 4;
      helperSize = (linesToPrefetch + 1) / 2 * 4;

      TR_ASSERT_FATAL( (skipLines + 1) * ppcCacheLineSize <= UPPER_IMMED, "tlhPrefetchStaggeredLineCount (%u) is too high. Will cause imm field to overflow.", skipLines);
      TR_ASSERT_FATAL( restartAfterLines * ppcCacheLineSize <= UPPER_IMMED, "tlhPrefetchBoundaryLineCount (%u) is too high. Will cause imm field to overflow.", restartAfterLines);

      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r8, skipLines * ppcCacheLineSize, cursor);
      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r11, r8, (skipLines + 1) * ppcCacheLineSize, cursor);
      cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtst, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r10, 4, cg), cursor);
      cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtst, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r11, 4, cg), cursor);

      for (uint32_t i = 2; i < linesToPrefetch; i += 2)
         {
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r10, ppcCacheLineSize * 2, cursor);
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r11, r11, ppcCacheLineSize * 2, cursor);
         cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtst, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r10, 4, cg), cursor);
         cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtst, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r11, 4, cg), cursor);
         }

      cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, n, r11, restartAfterLines * ppcCacheLineSize, cursor);
      cursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, nonZeroTlhPrefetchFTA), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          r11, cursor);
      }
   else
      {
      // Transient version
      static const char *s = feGetEnv("TR_l3SkipLines");
      static uint32_t l3SkipLines = s ? atoi(s) : 0;
      const uint32_t linesToLdPrefetch = TR::Options::_TLHPrefetchLineCount > 0 ? TR::Options::_TLHPrefetchLineCount : 4;
      const uint32_t linesToStPrefetch = linesToLdPrefetch * 2;
      const uint32_t restartAfterLines = TR::Options::_TLHPrefetchBoundaryLineCount > 0 ?  TR::Options::_TLHPrefetchBoundaryLineCount : 4;
      const uint32_t skipLines = TR::Options::_TLHPrefetchStaggeredLineCount > 0  ? TR::Options::_TLHPrefetchStaggeredLineCount : 4;
      helperSize = 4 + (linesToLdPrefetch + 1) / 2 * 4 + 1 + (l3SkipLines ? 2 : 0) + (linesToStPrefetch + 1) / 2 * 4;

      TR_ASSERT_FATAL( (skipLines + 1) * ppcCacheLineSize <= UPPER_IMMED, "tlhPrefetchStaggeredLineCount (%u) is too high. Will cause imm field to overflow.", skipLines);
      TR_ASSERT_FATAL( restartAfterLines * ppcCacheLineSize <= UPPER_IMMED, "tlhPrefetchBoundaryLineCount (%u) is too high. Will cause imm field to overflow.", restartAfterLines);

      cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r10,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, debugEventData3), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          cursor);
      cursor = generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, n, cr0, r10, 0, cursor);
      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xori, n, r10, r10, 1, cursor);
      cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, n, r11, restartAfterLines * ppcCacheLineSize, cursor);
      cursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, nonZeroTlhPrefetchFTA), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          r11, cursor);
      cursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, debugEventData3), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          r10, cursor);

      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r8, skipLines * ppcCacheLineSize, cursor);
      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r11, r8, (skipLines + 1) * ppcCacheLineSize, cursor);
      cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtt, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r10, 4, cg), cursor);
      cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtt, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r11, 4, cg), cursor);

      for (uint32_t i = 2; i < linesToLdPrefetch; i += 2)
         {
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r10, ppcCacheLineSize * 2, cursor);
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r11, r11, ppcCacheLineSize * 2, cursor);
         cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtt, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r10, 4, cg), cursor);
         cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtt, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r11, 4, cg), cursor);
         }

      cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beqlr, n, NULL, cr0, cursor);

      if (l3SkipLines > 0)
         {
         TR_ASSERT_FATAL( ppcCacheLineSize * l3SkipLines <= UPPER_IMMED, "TR_l3SkipLines (%u) is too high. Will cause imm field to overflow.", l3SkipLines);
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r10, ppcCacheLineSize * l3SkipLines, cursor);
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r11, r11, ppcCacheLineSize * l3SkipLines, cursor);
         }

      for (uint32_t i = 0; i < linesToStPrefetch; i += 2)
         {
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r10, ppcCacheLineSize * 2, cursor);
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r11, r11, ppcCacheLineSize * 2, cursor);
         cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtstt, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r10, 4, cg), cursor);
         cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtstt, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r11, 4, cg), cursor);
         }
      }

   cursor = generateInstruction(cg, TR::InstOpCode::blr, n, cursor);

   performCCPreLoadedBinaryEncoding(buffer, cg);

   helperSize += 3;
   TR_ASSERT(cg->getBinaryBufferCursor() - entryLabel->getCodeLocation() == helperSize * PPC_INSTRUCTION_LENGTH,
           "Per-codecache prefetch helper, unexpected size");

   CCPreLoadedCodeTable[TR_NonZeroAllocPrefetch] = entryLabel->getCodeLocation();

   return cg->getBinaryBufferCursor() - PPC_INSTRUCTION_LENGTH;
   }

static TR::Instruction* genZeroInit(TR::CodeGenerator *cg, TR::Node *n, TR::Register *objStartReg, TR::Register *objEndReg, TR::Register *needsZeroInitCondReg,
                                   TR::Register *iterReg, TR::Register *zeroReg, TR::Register *condReg, uint32_t initOffset, TR::Instruction *cursor)
   {
   TR_ASSERT_FATAL_WITH_NODE(n, initOffset <= UPPER_IMMED, "initOffset (%u) is too big to fit in a signed immediate field.", initOffset);
   // Generates 24 instructions (+6 if DEBUG)
#if defined(DEBUG)
   // Fill the object with junk to make sure zero-init is working
      {
      TR::LabelSymbol *loopStartLabel = generateLabelSymbol(cg);

      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, iterReg, objStartReg, initOffset, cursor);
      cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, n, zeroReg, -1, cursor);

      cursor = generateLabelInstruction(cg, TR::InstOpCode::label, n, loopStartLabel, cursor);
      // XXX: This can be improved to use std on 64-bit, but we have to adjust for the size not being 8-byte aligned
      cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(iterReg, 0, 4, cg),
                                          zeroReg, cursor);
      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, iterReg, iterReg, 4, cursor);
      cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, condReg, iterReg, objEndReg, cursor);
      cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, n, loopStartLabel, condReg, cursor);
      }
#endif

   TR::LabelSymbol *unrolledLoopStartLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *residueLoopStartLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *doneZeroInitLabel = generateLabelSymbol(cg);

   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beqlr, n, NULL, needsZeroInitCondReg, cursor);

   cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, iterReg, objStartReg, initOffset, cursor);
   // Use the zero reg temporarily to calculate unrolled loop iterations, equal to (stop - start) >> 5
   cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, n, zeroReg, iterReg, objEndReg, cursor);
   cursor = generateShiftRightLogicalImmediate(cg, n, zeroReg, zeroReg, 5, cursor);
   cursor = generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, n, condReg, zeroReg, 0, cursor);
   cursor = generateSrc1Instruction(cg, TR::InstOpCode::mtctr, n, zeroReg);
   cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, n, zeroReg, 0, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, n, residueLoopStartLabel, condReg, cursor);

   cursor = generateLabelInstruction(cg, TR::InstOpCode::label, n, unrolledLoopStartLabel, cursor);
   for (int i = 0; i < 32; i += 4)
      cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(iterReg, i, 4, cg),
                                          zeroReg, cursor);
   cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, iterReg, iterReg, 32, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bdnz, n, unrolledLoopStartLabel, /* Not used */ condReg, cursor);

   cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, condReg, iterReg, objEndReg, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bge, n, doneZeroInitLabel, condReg, cursor);

   // Residue loop
   cursor = generateLabelInstruction(cg, TR::InstOpCode::label, n, residueLoopStartLabel, cursor);
   cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, n,
                                       new (cg->trHeapMemory()) TR::MemoryReference(iterReg, 0, 4, cg),
                                       zeroReg, cursor);
   cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, iterReg, iterReg, 4, cursor);
   cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, condReg, iterReg, objEndReg, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, n, residueLoopStartLabel, condReg, cursor);

   cursor = generateLabelInstruction(cg, TR::InstOpCode::label, n, doneZeroInitLabel, cursor);

   return cursor;
   }

static uint8_t* initializeCCPreLoadedWriteBarrier(uint8_t *buffer, void **CCPreLoadedCodeTable, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR::Node *n = cg->getFirstInstruction()->getNode();

   // Write barrier
   // In:
   //   r3 = dst object
   //   r4 = src object
   // Out:
   //   none
   // Clobbers:
   //   r3-r6, r11
   //   cr0, cr1

   cg->setFirstInstruction(NULL);
   cg->setAppendInstruction(NULL);

   TR::Instruction *eyecatcher = generateImmInstruction(cg, TR::InstOpCode::dd, n, CCEYECATCHER('C', 'C', 'H', 2));

   TR::LabelSymbol *entryLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *helperTrampolineLabel = generateLabelSymbol(cg);
   helperTrampolineLabel->setCodeLocation((uint8_t *)TR::CodeCacheManager::instance()->findHelperTrampoline(TR_writeBarrierStoreGenerational, buffer));
   TR::Instruction *entry = generateLabelInstruction(cg, TR::InstOpCode::label, n, entryLabel);
   TR::Instruction *cursor = entry;
   TR::InstOpCode::Mnemonic Op_lclass = TR::InstOpCode::Op_load;
   if (TR::Compiler->om.compressObjectReferences())
      Op_lclass = TR::InstOpCode::lwz;
   const TR::InstOpCode::Mnemonic rememberedClassMaskOp = J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST > UPPER_IMMED ||
                                               J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST < LOWER_IMMED ? TR::InstOpCode::andis_r : TR::InstOpCode::andi_r;
   const uint32_t      rememberedClassMask = rememberedClassMaskOp == TR::InstOpCode::andis_r ?
                                             J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST >> 16 : J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST;

   TR::Register *metaReg = cg->getMethodMetaDataRegister();
   TR::Register *r3 = cg->machine()->getRealRegister(TR::RealRegister::gr3);
   TR::Register *r4 = cg->machine()->getRealRegister(TR::RealRegister::gr4);
   TR::Register *r5 = cg->machine()->getRealRegister(TR::RealRegister::gr5);
   TR::Register *r6 = cg->machine()->getRealRegister(TR::RealRegister::gr6);
   TR::Register *r11 = cg->machine()->getRealRegister(TR::RealRegister::gr11);
   TR::Register *cr0 = cg->machine()->getRealRegister(TR::RealRegister::cr0);
   TR::Register *cr1 = cg->machine()->getRealRegister(TR::RealRegister::cr1);

   TR_ASSERT_FATAL((J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST <= UPPER_IMMED && J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST >= LOWER_IMMED) ||
           (J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST & 0xffff) == 0, "Expecting J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST to fit in immediate field");
   TR_ASSERT_FATAL( rememberedClassMask <= 0xFFFF, "Expecting rememberedClassMask (%u) to fit in an unsigned immediate field.", rememberedClassMask);

   const bool constHeapBase = !comp->getOptions()->isVariableHeapBaseForBarrierRange0();
   const bool constHeapSize = !comp->getOptions()->isVariableHeapSizeForBarrierRange0();
   intptr_t heapBase;
   intptr_t heapSize;

   if (comp->target().is32Bit() && !comp->compileRelocatableCode() && constHeapBase)
      {
      heapBase = comp->getOptions()->getHeapBaseForBarrierRange0();
      cursor = loadAddressConstant(cg, false, n, heapBase, r5, cursor);
      }
   else
      cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r5,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(struct J9VMThread, heapBaseForBarrierRange0), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          cursor);
   if (comp->target().is32Bit() && !comp->compileRelocatableCode() && constHeapSize)
      {
      heapSize = comp->getOptions()->getHeapSizeForBarrierRange0();
      cursor = loadAddressConstant(cg, false, n, heapSize, r6, cursor);
      }
   else
      cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r6,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(struct J9VMThread, heapSizeForBarrierRange0), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          cursor);
   cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, n, r11, r5, r3, cursor);
   cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr0, r11, r6, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bgelr, n, NULL, cr0, cursor);
   cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, n, r11, r5, r4, cursor);
   cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr0, r11, r6, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bltlr, n, NULL, cr0, cursor);
   cursor = generateTrg1MemInstruction(cg,Op_lclass, n, r11,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r3, TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceField(), cg),
                                       cursor);
   cursor = generateTrg1Src1ImmInstruction(cg, rememberedClassMaskOp, n, r11, r11, cr0, rememberedClassMask, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bnelr, n, NULL, cr0, cursor);
   cursor = generateLabelInstruction(cg, TR::InstOpCode::b, n, helperTrampolineLabel, cursor);

   performCCPreLoadedBinaryEncoding(buffer, cg);

   const uint32_t helperSize = 12 +
      (comp->target().is32Bit() && !comp->compileRelocatableCode() && constHeapBase && heapBase > UPPER_IMMED && heapBase < LOWER_IMMED ? 1 : 0) +
      (comp->target().is32Bit() && !comp->compileRelocatableCode() && constHeapSize && heapSize > UPPER_IMMED && heapSize < LOWER_IMMED ? 1 : 0);
   TR_ASSERT(cg->getBinaryBufferCursor() - entryLabel->getCodeLocation() == helperSize * PPC_INSTRUCTION_LENGTH,
           "Per-codecache write barrier, unexpected size");

   CCPreLoadedCodeTable[TR_writeBarrier] = entryLabel->getCodeLocation();

   return cg->getBinaryBufferCursor() - PPC_INSTRUCTION_LENGTH;
   }

static uint8_t* initializeCCPreLoadedWriteBarrierAndCardMark(uint8_t *buffer, void **CCPreLoadedCodeTable, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR::Node *n = cg->getFirstInstruction()->getNode();

   // Write barrier and card mark
   // In:
   //   r3 = dst object
   //   r4 = src object
   // Out:
   //   none
   // Clobbers:
   //   r3-r6, r11
   //   cr0, cr1

   cg->setFirstInstruction(NULL);
   cg->setAppendInstruction(NULL);

   TR::Instruction *eyecatcher = generateImmInstruction(cg, TR::InstOpCode::dd, n, CCEYECATCHER('C', 'C', 'H', 3));

   TR::LabelSymbol *entryLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *doneCardMarkLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *helperTrampolineLabel = generateLabelSymbol(cg);
   helperTrampolineLabel->setCodeLocation((uint8_t *)TR::CodeCacheManager::instance()->findHelperTrampoline(TR_writeBarrierStoreGenerationalAndConcurrentMark, buffer));
   TR::Instruction *entry = generateLabelInstruction(cg, TR::InstOpCode::label, n, entryLabel);
   TR::Instruction *cursor = entry;
   TR::InstOpCode::Mnemonic Op_lclass = TR::InstOpCode::Op_load;
   if (TR::Compiler->om.compressObjectReferences())
      Op_lclass = TR::InstOpCode::lwz;
   const TR::InstOpCode::Mnemonic cmActiveMaskOp = J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE > UPPER_IMMED ||
                                        J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE < LOWER_IMMED ? TR::InstOpCode::andis_r : TR::InstOpCode::andi_r;
   const uint32_t      cmActiveMask = cmActiveMaskOp == TR::InstOpCode::andis_r ?
                                      J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE >> 16 : J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE;
   const TR::InstOpCode::Mnemonic rememberedClassMaskOp = J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST > UPPER_IMMED ||
                                               J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST < LOWER_IMMED ? TR::InstOpCode::andis_r : TR::InstOpCode::andi_r;
   const uint32_t      rememberedClassMask = rememberedClassMaskOp == TR::InstOpCode::andis_r ?
                                             J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST >> 16 : J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST;
   const uintptr_t     cardTableShift = comp->target().is64Bit() ?
                                        trailingZeroes((uint64_t)comp->getOptions()->getGcCardSize()) :
                                        trailingZeroes((uint32_t)comp->getOptions()->getGcCardSize());

   TR::Register *metaReg = cg->getMethodMetaDataRegister();
   TR::Register *r3 = cg->machine()->getRealRegister(TR::RealRegister::gr3);
   TR::Register *r4 = cg->machine()->getRealRegister(TR::RealRegister::gr4);
   TR::Register *r5 = cg->machine()->getRealRegister(TR::RealRegister::gr5);
   TR::Register *r6 = cg->machine()->getRealRegister(TR::RealRegister::gr6);
   TR::Register *r11 = cg->machine()->getRealRegister(TR::RealRegister::gr11);
   TR::Register *cr0 = cg->machine()->getRealRegister(TR::RealRegister::cr0);
   TR::Register *cr1 = cg->machine()->getRealRegister(TR::RealRegister::cr1);

   TR_ASSERT_FATAL((J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE <= UPPER_IMMED && J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE >= LOWER_IMMED) ||
           (J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE & 0xffff) == 0, "Expecting J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE to fit in immediate field");
   TR_ASSERT_FATAL((J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST <= UPPER_IMMED && J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST >= LOWER_IMMED) ||
           (J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST & 0xffff) == 0, "Expecting J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST to fit in immediate field");
   TR_ASSERT_FATAL( cmActiveMask <= 0xFFFF, "Expecting cmActiveMask (%u) to fit in an unsigned immediate field.", cmActiveMask);
   TR_ASSERT_FATAL( rememberedClassMask <= 0xFFFF, "Expecting rememberedClassMask (%u) to fit in an unsigned immediate field.", rememberedClassMask);

   const bool constHeapBase = !comp->getOptions()->isVariableHeapBaseForBarrierRange0();
   const bool constHeapSize = !comp->getOptions()->isVariableHeapSizeForBarrierRange0();
   intptr_t heapBase;
   intptr_t heapSize;

   if (comp->target().is32Bit() && !comp->compileRelocatableCode() && constHeapBase)
      {
      heapBase = comp->getOptions()->getHeapBaseForBarrierRange0();
      cursor = loadAddressConstant(cg, false, n, heapBase, r5, cursor);
      }
   else
      cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r5,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(struct J9VMThread, heapBaseForBarrierRange0), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          cursor);
   if (comp->target().is32Bit() && !comp->compileRelocatableCode() && constHeapSize)
      {
      heapSize = comp->getOptions()->getHeapSizeForBarrierRange0();
      cursor = loadAddressConstant(cg, false, n, heapSize, r6, cursor);
      }
   else
      cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r6,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(struct J9VMThread, heapSizeForBarrierRange0), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          cursor);
   cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, n, r11, r5, r3, cursor);
   cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr0, r11, r6, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bgelr, n, NULL, cr0, cursor);
   cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, n, r5, r5, r4, cursor);
   cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr1, r5, r6, cursor);
   cursor = generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, n, r6,
                                       new (cg->trHeapMemory()) TR::MemoryReference(metaReg,  offsetof(struct J9VMThread, privateFlags), 4, cg),
                                       cursor);
   cursor = generateTrg1Src1ImmInstruction(cg, cmActiveMaskOp, n, r6, r6, cr0, cmActiveMask, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, n, doneCardMarkLabel, cr0, cursor);
   cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r6,
                                       new (cg->trHeapMemory()) TR::MemoryReference(metaReg,  offsetof(struct J9VMThread, activeCardTableBase), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                       cursor);
   if (comp->target().is64Bit())
      cursor = generateShiftRightLogicalImmediateLong(cg, n, r11, r11, cardTableShift, cursor);
   else
      cursor = generateShiftRightLogicalImmediate(cg, n, r11, r11, cardTableShift, cursor);
   cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, n, r5, 1, cursor);
   cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stbx, n,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r6, r11, 1, cg),
                                       r5, cursor);
   cursor = generateLabelInstruction(cg, TR::InstOpCode::label, n, doneCardMarkLabel, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bltlr, n, NULL, cr1, cursor);
   cursor = generateTrg1MemInstruction(cg,Op_lclass, n, r11,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r3, TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceField(), cg),
                                       cursor);
   cursor = generateTrg1Src1ImmInstruction(cg, rememberedClassMaskOp, n, r11, r11, cr0, rememberedClassMask, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bnelr, n, NULL, cr0, cursor);
   cursor = generateLabelInstruction(cg, TR::InstOpCode::b, n, helperTrampolineLabel, cursor);

   performCCPreLoadedBinaryEncoding(buffer, cg);

   const uint32_t helperSize = 19 +
      (comp->target().is32Bit() && !comp->compileRelocatableCode() && constHeapBase && heapBase > UPPER_IMMED && heapBase < LOWER_IMMED ? 1 : 0) +
      (comp->target().is32Bit() && !comp->compileRelocatableCode() && constHeapSize && heapSize > UPPER_IMMED && heapSize < LOWER_IMMED ? 1 : 0);
   TR_ASSERT(cg->getBinaryBufferCursor() - entryLabel->getCodeLocation() == helperSize * PPC_INSTRUCTION_LENGTH,
           "Per-codecache write barrier with card mark, unexpected size");

   CCPreLoadedCodeTable[TR_writeBarrierAndCardMark] = entryLabel->getCodeLocation();

   return cg->getBinaryBufferCursor() - PPC_INSTRUCTION_LENGTH;
   }

static uint8_t* initializeCCPreLoadedCardMark(uint8_t *buffer, void **CCPreLoadedCodeTable, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node *n = cg->getFirstInstruction()->getNode();

   // Card mark
   // In:
   //   r3 = dst object
   // Out:
   //   none
   // Clobbers:
   //   r4-r5, r11
   //   cr0

   cg->setFirstInstruction(NULL);
   cg->setAppendInstruction(NULL);

   TR::Instruction *eyecatcher = generateImmInstruction(cg, TR::InstOpCode::dd, n, CCEYECATCHER('C', 'C', 'H', 4));

   TR::LabelSymbol *entryLabel = generateLabelSymbol(cg);
   TR::Instruction *entry = generateLabelInstruction(cg, TR::InstOpCode::label, n, entryLabel);
   TR::Instruction *cursor = entry;
   const TR::InstOpCode::Mnemonic cmActiveMaskOp = J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE > UPPER_IMMED ||
                                        J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE < LOWER_IMMED ? TR::InstOpCode::andis_r : TR::InstOpCode::andi_r;
   const uint32_t      cmActiveMask = cmActiveMaskOp == TR::InstOpCode::andis_r ?
                                      J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE >> 16 : J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE;
   const uintptr_t     cardTableShift = comp->target().is64Bit() ?
                                        trailingZeroes((uint64_t)comp->getOptions()->getGcCardSize()) :
                                        trailingZeroes((uint32_t)comp->getOptions()->getGcCardSize());

   TR::Register *metaReg = cg->getMethodMetaDataRegister();
   TR::Register *r3 = cg->machine()->getRealRegister(TR::RealRegister::gr3);
   TR::Register *r4 = cg->machine()->getRealRegister(TR::RealRegister::gr4);
   TR::Register *r5 = cg->machine()->getRealRegister(TR::RealRegister::gr5);
   TR::Register *r11 = cg->machine()->getRealRegister(TR::RealRegister::gr11);
   TR::Register *cr0 = cg->machine()->getRealRegister(TR::RealRegister::cr0);

   TR_ASSERT_FATAL((J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE <= UPPER_IMMED && J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE >= LOWER_IMMED) ||
                       (J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE & 0xffff) == 0, "Expecting J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE to fit in immediate field");
   TR_ASSERT_FATAL( cmActiveMask <= 0xFFFF, "Expecting cmActiveMask (%u) to fit in an unsigned immediate field.", cmActiveMask);

   cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r5,
                                       new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(struct J9VMThread, heapBaseForBarrierRange0), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                       cursor);
   cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r4,
                                       new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(struct J9VMThread, heapSizeForBarrierRange0), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                       cursor);
   cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, n, r5, r5, r3, cursor);
   cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr0, r5, r4, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bgelr, n, NULL, cr0, cursor);
   // Incremental (i.e. balanced) always dirties the card
   if (TR::Compiler->om.writeBarrierType() != gc_modron_wrtbar_cardmark_incremental)
      {
      cursor = generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, n, r4,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg,  offsetof(struct J9VMThread, privateFlags), 4, cg),
                                          cursor);
      cursor = generateTrg1Src1ImmInstruction(cg, cmActiveMaskOp, n, r4, r4, cr0, cmActiveMask, cursor);
      cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beqlr, n, NULL, cr0, cursor);
      }
   cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r4,
                                       new (cg->trHeapMemory()) TR::MemoryReference(metaReg,  offsetof(struct J9VMThread, activeCardTableBase), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                       cursor);
   if (comp->target().is64Bit())
      cursor = generateShiftRightLogicalImmediateLong(cg, n, r5, r5, cardTableShift, cursor);
   else
      cursor = generateShiftRightLogicalImmediate(cg, n, r5, r5, cardTableShift, cursor);
   cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, n, r11, 1, cursor);
   cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stbx, n,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r4, r5, 1, cg),
                                       r11, cursor);
   cursor = generateInstruction(cg, TR::InstOpCode::blr, n, cursor);

   performCCPreLoadedBinaryEncoding(buffer, cg);

   const uint32_t helperSize = TR::Compiler->om.writeBarrierType() != gc_modron_wrtbar_cardmark_incremental ? 13 : 10;
   TR_ASSERT(cg->getBinaryBufferCursor() - entryLabel->getCodeLocation() == helperSize * PPC_INSTRUCTION_LENGTH,
           "Per-codecache card mark, unexpected size");

   CCPreLoadedCodeTable[TR_cardMark] = entryLabel->getCodeLocation();

   return cg->getBinaryBufferCursor() - PPC_INSTRUCTION_LENGTH;
   }

static uint8_t* initializeCCPreLoadedArrayStoreCHK(uint8_t *buffer, void **CCPreLoadedCodeTable, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR::Node *n = cg->getFirstInstruction()->getNode();

   // Array store check
   // In:
   //   r3 = dst object
   //   r4 = src object
   //   r11 = root class (j/l/Object)
   // Out:
   //   none
   // Clobbers:
   //   r5-r7, r11
   //   cr0

   cg->setFirstInstruction(NULL);
   cg->setAppendInstruction(NULL);

   TR::Instruction *eyecatcher = generateImmInstruction(cg, TR::InstOpCode::dd, n, CCEYECATCHER('C', 'C', 'H', 7));

   TR::LabelSymbol *entryLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *skipSuperclassTestLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *helperTrampolineLabel = generateLabelSymbol(cg);
   helperTrampolineLabel->setCodeLocation((uint8_t *)TR::CodeCacheManager::instance()->findHelperTrampoline(TR_typeCheckArrayStore, buffer));
   TR::Instruction *entry = generateLabelInstruction(cg, TR::InstOpCode::label, n, entryLabel);
   TR::Instruction *cursor = entry;
   TR::InstOpCode::Mnemonic Op_lclass = TR::InstOpCode::Op_load;
   if (TR::Compiler->om.compressObjectReferences())
      Op_lclass = TR::InstOpCode::lwz;

   TR::Register *r3 = cg->machine()->getRealRegister(TR::RealRegister::gr3);
   TR::Register *r4 = cg->machine()->getRealRegister(TR::RealRegister::gr4);
   TR::Register *r5 = cg->machine()->getRealRegister(TR::RealRegister::gr5);
   TR::Register *r6 = cg->machine()->getRealRegister(TR::RealRegister::gr6);
   TR::Register *r7 = cg->machine()->getRealRegister(TR::RealRegister::gr7);
   TR::Register *r11 = cg->machine()->getRealRegister(TR::RealRegister::gr11);
   TR::Register *cr0 = cg->machine()->getRealRegister(TR::RealRegister::cr0);

   cursor = generateTrg1MemInstruction(cg,Op_lclass, n, r5,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r3, TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceField(), cg),
                                       cursor);
   cursor = generateTrg1MemInstruction(cg,Op_lclass, n, r6,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r4, TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceField(), cg),
                                       cursor);
   cursor = TR::TreeEvaluator::generateVFTMaskInstruction(cg, n, r5, cursor);
   cursor = TR::TreeEvaluator::generateVFTMaskInstruction(cg, n, r6, cursor);
   cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r5,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r5, offsetof(J9ArrayClass, componentType), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                       cursor);

   cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr0, r5, r6, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beqlr, n, NULL, cr0, cursor);
   cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r7,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r6, offsetof(J9Class, castClassCache), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                       cursor);
   cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr0, r5, r7, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beqlr, n, NULL, cr0, cursor);
   cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr0, r5, r11, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beqlr, n, NULL, cr0, cursor);

   cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r7,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r5, offsetof(J9Class, romClass), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                       cursor);
   cursor = generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, n, r7,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r7, offsetof(J9ROMClass, modifiers), 4, cg),
                                       cursor);
   cursor = generateShiftRightLogicalImmediate(cg, n, r7, r7, 1, cursor);
   TR_ASSERT_FATAL(!(((J9AccClassArray | J9AccInterface) >> 1) & ~0xffff),
           "Expecting shifted ROM class modifiers to fit in immediate");
   cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, n, r7, r7, cr0, (J9AccClassArray | J9AccInterface) >> 1, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, n, skipSuperclassTestLabel, cr0, cursor);

   cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r7,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r6, offsetof(J9Class, classDepthAndFlags), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                       cursor);
   TR_ASSERT_FATAL(!(J9AccClassDepthMask & ~0xffff),
           "Expecting class depth mask to fit in immediate");
   cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, n, r7, r7, cr0, J9AccClassDepthMask, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, n, skipSuperclassTestLabel, cr0, cursor);
   cursor = generateTrg1MemInstruction(cg,Op_lclass, n, r7,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r6, offsetof(J9Class, superclasses), TR::Compiler->om.sizeofReferenceField(), cg),
                                       cursor);
   cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r7,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r7, 0, TR::Compiler->om.sizeofReferenceAddress(), cg),
                                       cursor);
   cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr0, r5, r7, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beqlr, n, NULL, cr0, cursor);

   cursor = generateLabelInstruction(cg, TR::InstOpCode::label, n, skipSuperclassTestLabel, cursor);
   cursor = generateLabelInstruction(cg, TR::InstOpCode::b, n, helperTrampolineLabel, cursor);

   performCCPreLoadedBinaryEncoding(buffer, cg);

   const uint32_t helperSize = 25;
   TR_ASSERT(cg->getBinaryBufferCursor() - entryLabel->getCodeLocation() == helperSize * PPC_INSTRUCTION_LENGTH,
           "Per-codecache array store check, unexpected size");

   CCPreLoadedCodeTable[TR_arrayStoreCHK] = entryLabel->getCodeLocation();

   return cg->getBinaryBufferCursor() - PPC_INSTRUCTION_LENGTH;
   }

void TR::createCCPreLoadedCode(uint8_t *CCPreLoadedCodeBase, uint8_t *CCPreLoadedCodeTop, void **CCPreLoadedCodeTable, TR::CodeGenerator *cg)
   {
   /* If you modify this make sure you update CCPreLoadedCodeSize above as well */

   // We temporarily clobber the first and append instructions so we can use high level codegen to generate pre-loaded code
   // So save the original values here and restore them when done
   TR::Compilation *comp = cg->comp();
   if (comp->getOptions()->realTimeGC())
      return;

   TR::Instruction *curFirst = cg->getFirstInstruction();
   TR::Instruction *curAppend = cg->getAppendInstruction();
   uint8_t *curBinaryBufferStart = cg->getBinaryBufferStart();
   uint8_t *curBinaryBufferCursor = cg->getBinaryBufferCursor();

   uint8_t *buffer = (uint8_t *)CCPreLoadedCodeBase;

   buffer = initializeCCPreLoadedPrefetch(buffer, CCPreLoadedCodeTable, cg);
   buffer = initializeCCPreLoadedNonZeroPrefetch(buffer + PPC_INSTRUCTION_LENGTH, CCPreLoadedCodeTable, cg);
   buffer = initializeCCPreLoadedWriteBarrier(buffer + PPC_INSTRUCTION_LENGTH, CCPreLoadedCodeTable, cg);
   if (comp->getOptions()->getGcCardSize() > 0)
      {
      buffer = initializeCCPreLoadedWriteBarrierAndCardMark(buffer + PPC_INSTRUCTION_LENGTH, CCPreLoadedCodeTable, cg);
      buffer = initializeCCPreLoadedCardMark(buffer + PPC_INSTRUCTION_LENGTH, CCPreLoadedCodeTable, cg);
      }
   buffer = initializeCCPreLoadedArrayStoreCHK(buffer + PPC_INSTRUCTION_LENGTH, CCPreLoadedCodeTable, cg);

   // Other Code Cache Helper Initialization will go here

   TR_ASSERT(buffer <= (uint8_t*)CCPreLoadedCodeTop, "Exceeded CodeCache Helper Area");

   // Apply all of our relocations now before we sync
   TR::list<TR::Relocation*> &relocs = cg->getRelocationList();
   auto iterator = relocs.begin();
   while (iterator != relocs.end())
      {
      if ((*iterator)->getUpdateLocation() >= CCPreLoadedCodeBase &&
          (*iterator)->getUpdateLocation() <= CCPreLoadedCodeTop)
         {
         (*iterator)->apply(cg);
         iterator = relocs.erase(iterator);
         }
      else
    	  ++iterator;
      }

#if defined(TR_HOST_POWER)
   ppcCodeSync((uint8_t *)CCPreLoadedCodeBase, buffer - (uint8_t *)CCPreLoadedCodeBase + 1);
#endif

   cg->setFirstInstruction(curFirst);
   cg->setAppendInstruction(curAppend);
   cg->setBinaryBufferStart(curBinaryBufferStart);
   cg->setBinaryBufferCursor(curBinaryBufferCursor);

   }

uint8_t *TR::PPCAllocPrefetchSnippet::emitSnippetBody()
   {
   TR::Compilation *comp = cg()->comp();
   uint8_t *buffer = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(buffer);
   TR::InstOpCode opcode;

   if (comp->getOptions()->realTimeGC())
      return NULL;

   TR_ASSERT((uintptr_t)((cg()->getCodeCache())->getCCPreLoadedCodeAddress(TR_AllocPrefetch, cg())) != 0xDEADBEEF,
         "Invalid addr for code cache helper");
   intptr_t distance = (intptr_t)(cg()->getCodeCache())->getCCPreLoadedCodeAddress(TR_AllocPrefetch, cg())
                           - (intptr_t)buffer;
   opcode.setOpCodeValue(TR::InstOpCode::b);
   buffer = opcode.copyBinaryToBuffer(buffer);
   *(int32_t *)buffer |= distance & 0x03FFFFFC;
   return buffer+PPC_INSTRUCTION_LENGTH;
   }

void
TR::PPCAllocPrefetchSnippet::print(TR::FILE *pOutFile, TR_Debug * debug)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());
   uint8_t *cursor = getSnippetLabel()->getCodeLocation();

   debug->printSnippetLabel(pOutFile, getSnippetLabel(), cursor, "Allocation Prefetch Snippet");

   int32_t  distance;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   distance = *((int32_t *) cursor) & 0x03fffffc;
   distance = (distance << 6) >> 6;   // sign extend
   trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t\t", (intptr_t)cursor + distance);
   }

uint32_t TR::PPCAllocPrefetchSnippet::getLength(int32_t estimatedCodeStart)
   {

   if (cg()->comp()->getOptions()->realTimeGC())
      return 0;

   return PPC_INSTRUCTION_LENGTH;
   }

TR::PPCNonZeroAllocPrefetchSnippet::PPCNonZeroAllocPrefetchSnippet(
   TR::CodeGenerator   *codeGen,
   TR::Node            *node,
   TR::LabelSymbol      *callLabel)
     : TR::Snippet(codeGen, node, callLabel, false)
   {
   }

uint8_t *TR::PPCNonZeroAllocPrefetchSnippet::emitSnippetBody()
   {
   TR::Compilation *comp = cg()->comp();
   uint8_t *buffer = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(buffer);
   TR::InstOpCode opcode;

   if (comp->getOptions()->realTimeGC())
      return NULL;

   TR_ASSERT((uintptr_t)((cg()->getCodeCache())->getCCPreLoadedCodeAddress(TR_NonZeroAllocPrefetch, cg())) != 0xDEADBEEF,
         "Invalid addr for code cache helper");
   intptr_t distance = (intptr_t)(cg()->getCodeCache())->getCCPreLoadedCodeAddress(TR_NonZeroAllocPrefetch, cg())
                           - (intptr_t)buffer;
   opcode.setOpCodeValue(TR::InstOpCode::b);
   buffer = opcode.copyBinaryToBuffer(buffer);
   *(int32_t *)buffer |= distance & 0x03FFFFFC;
   return buffer+PPC_INSTRUCTION_LENGTH;
   }

void
TR::PPCNonZeroAllocPrefetchSnippet::print(TR::FILE *pOutFile, TR_Debug * debug)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());
   uint8_t *cursor = getSnippetLabel()->getCodeLocation();

   debug->printSnippetLabel(pOutFile, getSnippetLabel(), cursor, "Non Zero TLH Allocation Prefetch Snippet");

   int32_t  distance;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   distance = *((int32_t *) cursor) & 0x03fffffc;
   distance = (distance << 6) >> 6;   // sign extend
   trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t\t", (intptr_t)cursor + distance);
   }

uint32_t TR::PPCNonZeroAllocPrefetchSnippet::getLength(int32_t estimatedCodeStart)
   {

   if (cg()->comp()->getOptions()->realTimeGC())
      return 0;

   return PPC_INSTRUCTION_LENGTH;
   }

