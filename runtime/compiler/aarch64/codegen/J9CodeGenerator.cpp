/*******************************************************************************
 * Copyright IBM Corp. and others 2019
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "codegen/AheadOfTimeCompile.hpp"
#include "codegen/ARM64JNILinkage.hpp"
#include "codegen/ARM64PrivateLinkage.hpp"
#include "codegen/ARM64Recompilation.hpp"
#include "codegen/ARM64SystemLinkage.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "compile/Compilation.hpp"
#include "runtime/CodeCacheManager.hpp"

extern void TEMPORARY_initJ9ARM64TreeEvaluatorTable(TR::CodeGenerator *cg);

J9::ARM64::CodeGenerator::CodeGenerator(TR::Compilation *comp) :
      J9::CodeGenerator(comp)
   {
   /**
    * Do not add CodeGenerator initialization logic here.
    * Use the \c initialize() method instead.
    */
   }

void
J9::ARM64::CodeGenerator::initialize()
   {
   self()->J9::CodeGenerator::initialize();

   TR::CodeGenerator *cg = self();
   TR::Compilation *comp = cg->comp();

   cg->setAheadOfTimeCompile(new (cg->trHeapMemory()) TR::AheadOfTimeCompile(cg));

   /*
    * "Statically" initialize the FE-specific tree evaluator functions.
    * This code only needs to execute once per JIT lifetime.
    */
   static bool initTreeEvaluatorTable = false;
   if (!initTreeEvaluatorTable)
      {
      TEMPORARY_initJ9ARM64TreeEvaluatorTable(cg);
      initTreeEvaluatorTable = true;
      }

   cg->setSupportsInliningOfTypeCoersionMethods();
   cg->setSupportsDivCheck();
   if (!comp->getOption(TR_FullSpeedDebug))
      cg->setSupportsDirectJNICalls();

   cg->setSupportsPrimitiveArrayCopy();
   cg->setSupportsReferenceArrayCopy();

   static char *disableMonitorCacheLookup = feGetEnv("TR_disableMonitorCacheLookup");
   if (!disableMonitorCacheLookup)
      {
      comp->setOption(TR_EnableMonitorCacheLookup);
      }

   static bool disableInlineVectorizedMismatch = feGetEnv("TR_disableInlineVectorizedMismatch") != NULL;
   if (cg->getSupportsArrayCmpLen() &&
#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
         !TR::Compiler->om.isOffHeapAllocationEnabled() &&
#endif /* J9VM_GC_SPARSE_HEAP_ALLOCATION */
         !disableInlineVectorizedMismatch)
      {
      cg->setSupportsInlineVectorizedMismatch();
      }
   if ((!TR::Compiler->om.canGenerateArraylets()) && (!comp->getOption(TR_DisableSIMDStringHashCode)) && !TR::Compiler->om.isOffHeapAllocationEnabled())
      {
      cg->setSupportsInlineStringHashCode();
      }
   if ((!TR::Compiler->om.canGenerateArraylets()) && (!comp->getOption(TR_DisableFastStringIndexOf)) && !TR::Compiler->om.isOffHeapAllocationEnabled())
      {
      cg->setSupportsInlineStringIndexOf();
      }
   static bool disableInlineStringLatin1Inflate = feGetEnv("TR_disableInlineStringLatin1Inflate") != NULL;
   if ((!TR::Compiler->om.canGenerateArraylets()) && (!disableInlineStringLatin1Inflate) && !TR::Compiler->om.isOffHeapAllocationEnabled())
      {
      cg->setSupportsInlineStringLatin1Inflate();
      }
   if (comp->fej9()->hasFixedFrameC_CallingConvention())
      cg->setHasFixedFrameC_CallingConvention();
   }

TR::Linkage *
J9::ARM64::CodeGenerator::createLinkage(TR_LinkageConventions lc)
   {
   TR::Linkage *linkage;
   switch (lc)
      {
      case TR_Private:
         linkage = new (self()->trHeapMemory()) J9::ARM64::PrivateLinkage(self());
         break;
      case TR_System:
         linkage = new (self()->trHeapMemory()) TR::ARM64SystemLinkage(self());
         break;
      case TR_CHelper:
      case TR_Helper:
         linkage = new (self()->trHeapMemory()) J9::ARM64::HelperLinkage(self(), lc);
         break;
      case TR_J9JNILinkage:
         linkage = new (self()->trHeapMemory()) J9::ARM64::JNILinkage(self());
         break;
      default :
         linkage = new (self()->trHeapMemory()) TR::ARM64SystemLinkage(self());
         TR_ASSERT_FATAL(false, "Unexpected linkage convention");
      }

   self()->setLinkage(lc, linkage);
   return linkage;
   }

TR::Recompilation *
J9::ARM64::CodeGenerator::allocateRecompilationInfo()
   {
   return TR_ARM64Recompilation::allocate(self()->comp());
   }

uint32_t
J9::ARM64::CodeGenerator::encodeHelperBranchAndLink(TR::SymbolReference *symRef, uint8_t *cursor, TR::Node *node, bool omitLink)
   {
   TR::CodeGenerator *cg = self();
   uintptr_t target = (uintptr_t)symRef->getMethodAddress();

   if (cg->directCallRequiresTrampoline(target, (intptr_t)cursor))
      {
      target = TR::CodeCacheManager::instance()->findHelperTrampoline(symRef->getReferenceNumber(), (void *)cursor);

      TR_ASSERT_FATAL(cg->comp()->target().cpu.isTargetWithinUnconditionalBranchImmediateRange(target, (intptr_t)cursor),
                      "Target address is out of range");
      }

   cg->addExternalRelocation(
      TR::ExternalRelocation::create(
         cursor,
         (uint8_t *)symRef,
         TR_HelperAddress,
         cg),
      __FILE__,
      __LINE__,
      node);

   uintptr_t distance = target - (uintptr_t)cursor;
   return TR::InstOpCode::getOpCodeBinaryEncoding(omitLink ? (TR::InstOpCode::b) : (TR::InstOpCode::bl)) | ((distance >> 2) & 0x3ffffff); /* imm26 */
   }

void
J9::ARM64::CodeGenerator::generateBinaryEncodingPrePrologue(TR_ARM64BinaryEncodingData &data)
   {
   TR::Compilation *comp = self()->comp();
   TR::Node *startNode = comp->getStartTree()->getNode();

   data.recomp = comp->getRecompilationInfo();
   data.cursorInstruction = self()->getFirstInstruction();
   data.i2jEntryInstruction = data.cursorInstruction;

   if (data.recomp != NULL)
      {
      data.recomp->generatePrePrologue();
      }
   else
      {
      if (comp->getOption(TR_FullSpeedDebug) || comp->getOption(TR_SupportSwitchToInterpreter))
         {
         self()->generateSwitchToInterpreterPrePrologue(NULL, startNode);
         }
      else
         {
         TR::ResolvedMethodSymbol *methodSymbol = comp->getMethodSymbol();
         /* save the original JNI native address if a JNI thunk is generated */
         /* thunk is not recompilable, nor does it support FSD */
         if (methodSymbol->isJNI())
            {
            uintptr_t methodAddress = reinterpret_cast<uintptr_t>(methodSymbol->getResolvedMethod()->startAddressForJNIMethod(comp));
            uint32_t low = methodAddress & static_cast<uint32_t>(0xffffffff);
            uint32_t high = (methodAddress >> 32) & static_cast<uint32_t>(0xffffffff);
            TR::Instruction *cursor = new (self()->trHeapMemory()) TR::ARM64ImmInstruction(TR::InstOpCode::dd, startNode, low, NULL, self());
            generateImmInstruction(self(), TR::InstOpCode::dd, startNode, high, cursor);
            }
         }
      }
   }

TR::Instruction *
J9::ARM64::CodeGenerator::generateSwitchToInterpreterPrePrologue(TR::Instruction *cursor, TR::Node *node)
   {
   TR::Compilation *comp = self()->comp();
   TR::Register *x8 = self()->machine()->getRealRegister(TR::RealRegister::x8);
   TR::Register *lr = self()->machine()->getRealRegister(TR::RealRegister::x30); // link register
   TR::Register *xzr = self()->machine()->getRealRegister(TR::RealRegister::xzr); // zero register
   TR::ResolvedMethodSymbol *methodSymbol = comp->getJittedMethodSymbol();
   TR::SymbolReference *revertToInterpreterSymRef = self()->symRefTab()->findOrCreateRuntimeHelper(TR_ARM64revertToInterpreterGlue);
   uintptr_t ramMethod = (uintptr_t)methodSymbol->getResolvedMethod()->resolvedMethodAddress();
   TR::SymbolReference *helperSymRef = self()->symRefTab()->findOrCreateRuntimeHelper(TR_j2iTransition);
   uintptr_t helperAddr = (uintptr_t)helperSymRef->getMethodAddress();

   // x8 must contain the saved LR; see Recompilation.s
   // cannot use generateMovInstruction() here
   cursor = new (self()->trHeapMemory()) TR::ARM64Trg1Src2Instruction(TR::InstOpCode::orrx, node, x8, xzr, lr, cursor, self());
   cursor = self()->getLinkage()->saveParametersToStack(cursor);
   cursor = generateImmSymInstruction(self(), TR::InstOpCode::bl, node,
                                      (uintptr_t)revertToInterpreterSymRef->getMethodAddress(),
                                      new (self()->trHeapMemory()) TR::RegisterDependencyConditions(0, 0, self()->trMemory()),
                                      revertToInterpreterSymRef, NULL, cursor);
   cursor = generateRelocatableImmInstruction(self(), TR::InstOpCode::dd, node, (uintptr_t)ramMethod, TR_RamMethod, cursor);

   if (comp->getOption(TR_EnableHCR))
      comp->getStaticHCRPICSites()->push_front(cursor);

   cursor = generateRelocatableImmInstruction(self(), TR::InstOpCode::dd, node, (uintptr_t)helperAddr, TR_AbsoluteHelperAddress, helperSymRef, cursor);
   // Used in FSD to store an instruction
   cursor = generateImmInstruction(self(), TR::InstOpCode::dd, node, 0, cursor);

   return cursor;
   }

bool
J9::ARM64::CodeGenerator::supportsInliningOfIsInstance()
   {
   return !self()->comp()->getOption(TR_DisableInlineIsInstance);
   }

bool
J9::ARM64::CodeGenerator::supportsInliningOfIsAssignableFrom()
   {
   static const bool disableInliningOfIsAssignableFrom = feGetEnv("TR_disableInlineIsAssignableFrom") != NULL;
   return !disableInliningOfIsAssignableFrom;
   }

bool
J9::ARM64::CodeGenerator::suppressInliningOfRecognizedMethod(TR::RecognizedMethod method)
   {
   TR::Compilation *comp = self()->comp();
   static const bool disableCRC32 = feGetEnv("TR_aarch64DisableCRC32") != NULL;

   if (method == TR::java_lang_Math_min_F ||
       method == TR::java_lang_Math_max_F ||
       method == TR::java_lang_Math_fma_D ||
       method == TR::java_lang_Math_fma_F ||
       method == TR::java_lang_StrictMath_fma_D ||
       method == TR::java_lang_StrictMath_fma_F)
      {
      return true;
      }
   if (method == TR::java_util_zip_CRC32C_updateBytes ||
       method == TR::java_util_zip_CRC32C_updateDirectByteBuffer)
      {
      return (!TR::Compiler->om.canGenerateArraylets() && !TR::Compiler->om.isOffHeapAllocationEnabled()) && comp->target().cpu.supportsFeature(OMR_FEATURE_ARM64_CRC32) && (!disableCRC32);
      }
   return false;
   }

bool
J9::ARM64::CodeGenerator::callUsesHelperImplementation(TR::Symbol *sym)
   {
   return sym && (!self()->comp()->getOption(TR_DisableInliningOfNatives) &&
          sym->castToMethodSymbol()->getMandatoryRecognizedMethod() == TR::java_lang_invoke_ComputedCalls_dispatchJ9Method);
   }
