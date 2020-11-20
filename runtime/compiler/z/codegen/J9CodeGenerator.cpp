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

//On zOS XLC linker can't handle files with same name at link time
//This workaround with pragma is needed. What this does is essentially
//give a different name to the codesection (csect) for this file. So it
//doesn't conflict with another file with same name.

#pragma csect(CODE,"TRJ9ZCGBase#C")
#pragma csect(STATIC,"TRJ9ZCGBase#S")
#pragma csect(TEST,"TRJ9ZCGBase#T")

#include <algorithm>
#include "env/CompilerEnv.hpp"
#include "codegen/AheadOfTimeCompile.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/S390CHelperLinkage.hpp"
#include "codegen/S390PrivateLinkage.hpp"
#include "env/VMJ9.h"
#include "env/jittypes.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "z/codegen/J9SystemLinkageLinux.hpp"
#include "z/codegen/J9SystemLinkagezOS.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390Recompilation.hpp"
#include "z/codegen/S390Register.hpp"
#include "z/codegen/ReduceSynchronizedFieldLoad.hpp"

#define OPT_DETAILS "O^O CODE GENERATION: "

extern void TEMPORARY_initJ9S390TreeEvaluatorTable(TR::CodeGenerator *cg);

//Forward declarations
bool nodeMightClobberAccumulatorBeforeUse(TR::Node *);

J9::Z::CodeGenerator::CodeGenerator(TR::Compilation *comp) :
      J9::CodeGenerator(comp)
   {
   /**
    * Do not add CodeGenerator initialization logic here.
    * Use the \c initialize() method instead.
    */
   }

void
J9::Z::CodeGenerator::initialize()
   {
   self()->J9::CodeGenerator::initialize();

   TR::CodeGenerator *cg = self();
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   cg->setAheadOfTimeCompile(new (cg->trHeapMemory()) TR::AheadOfTimeCompile(cg));

   // Java specific runtime helpers
   cg->symRefTab()->createSystemRuntimeHelper(TR_S390jitMathHelperConvertLongToFloat);
   cg->symRefTab()->createSystemRuntimeHelper(TR_S390induceRecompilation);

   // Enable Direct to JNI calls unless we're mimicking interpreter stack frames.
   if (!comp->getOption(TR_FullSpeedDebug))
      cg->setSupportsDirectJNICalls();

   if (cg->getSupportsVectorRegisters() && !comp->getOption(TR_DisableSIMDStringCaseConv))
      cg->setSupportsInlineStringCaseConversion();

   if (cg->getSupportsVectorRegisters() && !comp->getOption(TR_DisableFastStringIndexOf) &&
       !TR::Compiler->om.canGenerateArraylets())
      {
      cg->setSupportsInlineStringIndexOf();
      }

   if (cg->getSupportsVectorRegisters() && !comp->getOption(TR_DisableSIMDStringHashCode) &&
       !TR::Compiler->om.canGenerateArraylets())
      {
      cg->setSupportsInlineStringHashCode();
      }

   // See comment in `handleHardwareReadBarrier` implementation as to why we cannot support CTX under CS
   if (cg->getSupportsTM() && TR::Compiler->om.readBarrierType() == gc_modron_readbar_none)
      {
      cg->setSupportsInlineConcurrentLinkedQueue();
      }

   // Similar to AOT, array translate instructions are not supported for remote compiles because instructions such as
   // TRTO allocate lookup tables in persistent memory that cannot be relocated.
   if (comp->isOutOfProcessCompilation())
      {
      cg->resetSupportsArrayTranslateTRxx();
      }

   // Let's turn this on.  There is more work needed in the opt
   // to catch the case where the BNDSCHK is inserted after
   //
   cg->setDisableNullCheckOfArrayLength();

   // Enable Range splitter by default.
   if (!comp->getOption(TR_DisableLiveRangeSplitter))
      comp->setOption(TR_EnableRangeSplittingGRA);

   // Disable SS Optimization that generates better SS instruction memory references.
   // Issue in Java because of symref in AOT case.  See RTC 31738 for details.
   comp->setOption(TR_DisableSSOpts);

   // Invoke Class.newInstanceImpl() from the JIT directly
   cg->setSupportsNewInstanceImplOpt();

   // Still being set in the S390CodeGenerator constructor, as zLinux sTR requires this.
   //cg->setSupportsJavaFloatSemantics();

   // Enable this only on Java, as there is a possibility that optimizations driven by this
   // flag will generate calls to helper routines.
#if defined(J9VM_OPT_JITSERVER)
   // The TRT instruction generated by the arrayTranslateAndTestEvaluator is not relocatable. Thus, to
   // attain functional correctness we don't enable this support for remote compilations.
   if (!comp->isOutOfProcessCompilation())
#endif /* defined(J9VM_OPT_JITSERVER) */
      {
      cg->setSupportsArrayTranslateAndTest();
      }

   // Enable compaction of local stack slots.  i.e. variables with non-overlapping live ranges
   // can share the same slot.
   cg->setSupportsCompactedLocals();

   // Enable Implicit NULL Checks on zLinux.  On zOS, page zero is readable, so we need explicit checks.
   cg->setSupportsImplicitNullChecks(comp->target().isLinux() && cg->getHasResumableTrapHandler() && !comp->getOption(TR_DisableZImplicitNullChecks));

   // Enable Monitor cache lookup for monent/monexit
   static char *disableMonitorCacheLookup = feGetEnv("TR_disableMonitorCacheLookup");
   if (!disableMonitorCacheLookup)
      comp->setOption(TR_EnableMonitorCacheLookup);

   // Enable high-resolution timer
   cg->setSupportsCurrentTimeMaxPrecision();

   // Defect 109299 : PMR 14649,999,760 / CritSit AV8426
   // Turn off use of hardware clock on zLinux for calculating currentTimeMillis() as user can adjust time on their system.
   //
   // Hardware clock, however, can be used for calculating System.nanoTime() on zLinux
   // since java/lang/System.nanoTime() returns an arbitrary number, rather than the current time
   // (see the java/lang/System.nanoTime() spec for details).
   if (comp->target().isZOS())
      cg->setSupportsMaxPrecisionMilliTime();

   // Support BigDecimal Long Lookaside versioning optimizations.
   if (!comp->getOption(TR_DisableBDLLVersioning))
      cg->setSupportsBigDecimalLongLookasideVersioning();

   // RI support
   if (comp->getOption(TR_HWProfilerDisableRIOverPrivateLinkage)
       && comp->getPersistentInfo()->isRuntimeInstrumentationEnabled()
       && comp->target().cpu.isAtLeast(OMR_PROCESSOR_S390_ZEC12)
       && comp->target().cpu.supportsFeature(OMR_FEATURE_S390_RI))
      {
      cg->setSupportsRuntimeInstrumentation();
      cg->setEnableRIOverPrivateLinkage(false);  // Disable RI over private linkage, since RION/OFF will be controlled over J2I / I2J.
      }

   /*
    * "Statically" initialize the FE-specific tree evaluator functions.
    * This code only needs to execute once per JIT lifetime.
    */
   static bool initTreeEvaluatorTable = false;
   if (!initTreeEvaluatorTable)
      {
      TEMPORARY_initJ9S390TreeEvaluatorTable(cg);
      initTreeEvaluatorTable = true;
      }

   cg->getS390Linkage()->initS390RealRegisterLinkage();

   const bool accessStaticsIndirectly =
         comp->getOption(TR_DisableDirectStaticAccessOnZ) ||
         (comp->compileRelocatableCode() && !comp->getOption(TR_UseSymbolValidationManager));

   cg->setAccessStaticsIndirectly(accessStaticsIndirectly);

   if (comp->fej9()->hasFixedFrameC_CallingConvention())
      {
      cg->setHasFixedFrameC_CallingConvention();
      }

   cg->setIgnoreDecimalOverflowException(false);
   }

bool
J9::Z::CodeGenerator::callUsesHelperImplementation(TR::Symbol *sym)
   {
   return sym && (!self()->comp()->getOption(TR_DisableInliningOfNatives) &&
                        sym->castToMethodSymbol()->getMandatoryRecognizedMethod() == TR::java_lang_invoke_ComputedCalls_dispatchJ9Method);
   }

TR::Linkage *
J9::Z::CodeGenerator::createLinkage(TR_LinkageConventions lc)
   {
   TR::Linkage * linkage;
   switch (lc)
      {
      case TR_CHelper:
         linkage = new (self()->trHeapMemory()) J9::Z::CHelperLinkage(self());
         break;
      case TR_Helper:
         linkage = new (self()->trHeapMemory()) J9::Z::HelperLinkage(self());
         break;

      case TR_Private:
         linkage = new (self()->trHeapMemory()) J9::Z::PrivateLinkage(self());
         break;

      case TR_J9JNILinkage:
         linkage = new (self()->trHeapMemory()) J9::Z::JNILinkage(self());
         break;

      case TR_System:
         if (self()->comp()->target().isLinux())
            linkage = new (self()->trHeapMemory()) J9::Z::zLinuxSystemLinkage(self());
         else
            linkage = new (self()->trHeapMemory()) J9::Z::zOSSystemLinkage(self());
         break;

      default :
         TR_ASSERT(0, "\nTestarossa error: Illegal linkage convention %d\n", lc);
      }

   self()->setLinkage(lc, linkage);
   return linkage;
   }

bool
J9::Z::CodeGenerator::doInlineAllocate(TR::Node *node)
   {
   TR_OpaqueClassBlock * classInfo = 0;
   if (self()->comp()->suppressAllocationInlining()) return false;
   TR::ILOpCodes opCode = node->getOpCodeValue();

   if ((opCode!=TR::anewarray) && (opCode!=TR::newarray) && (opCode!=TR::New))
      return false;


   int32_t   objectSize = self()->comp()->canAllocateInline(node, classInfo);
   if (objectSize < 0) return false;

   return true;
   }

bool
J9::Z::CodeGenerator::constLoadNeedsLiteralFromPool(TR::Node *node)
   {
   if (node->isClassUnloadingConst() || node->getType().isIntegral() || node->getType().isAddress())
      {
      return false;
      }
   else
      {
      return true;  // Floats/Doubles require literal pool
      }
   }

TR::Recompilation *
J9::Z::CodeGenerator::allocateRecompilationInfo()
   {
   TR::Compilation *comp = self()->comp();
   if(comp->getJittedMethodSymbol()->isJNI() &&
      !comp->getOption(TR_FullSpeedDebug))
     {
     traceMsg(comp, "\n====== THIS METHOD IS VIRTUAL JNI THUNK. IT WILL NOT BE RECOMPILED====\n");
     return NULL;
     }
   else
     {
     return TR_S390Recompilation::allocate(comp);
     }
   }

void
J9::Z::CodeGenerator::lowerTreesPreChildrenVisit(TR::Node* parent, TR::TreeTop * treeTop, vcount_t visitCount)
   {
   J9::CodeGenerator::lowerTreesPreChildrenVisit(parent, treeTop, visitCount);

   if (parent->getOpCodeValue() == TR::BCDCHK)
      {
      // sometimes TR::pdModifyPrecision will be inserted
      // just under BCDCHK, we have to remove it.
      TR::Node * chkChild = parent->getFirstChild();
      if (chkChild->getOpCodeValue() == TR::pdModifyPrecision)
         {
         TR::Node * pdopNode = chkChild->getFirstChild();
         pdopNode->incReferenceCount();
         chkChild->recursivelyDecReferenceCount();
         parent->setChild(0, pdopNode);
         }
      }

#if defined(SUPPORT_DFP)
   // Conversion from DFP to packed generates some ugly code. It is currently more efficient to go from
   // DFP to zoned to packed on systems supporting CZDT (arch(10) and higher).

   // If we have a truncating dd2zd node, we can use CZDT to truncate, but that will generate
   // an overflow exception unless decimal overflow is suppressed. If it is not, we generate
   // a non-truncating CZDT followed by an MVC. It seems more efficient to do the truncation
   // in DFP, so this code inserts a truncating dfpModifyPrecision operation below the dd2zd
   // and sets the source precision on the dd2zd to the truncated value.
   if (parent != NULL &&
       parent->getOpCode().isConversion() &&
       parent->getType().isAnyZoned() &&
       parent->getFirstChild()->getType().isDFP() &&
       parent->isTruncating())
      {
      TR::Node *ddMod = TR::Node::create(parent, TR::ILOpCode::modifyPrecisionOpCode(parent->getFirstChild()->getDataType()), 1);
      ddMod->setDFPPrecision(parent->getDecimalPrecision());
      ddMod->setChild(0, parent->getFirstChild());
      parent->setAndIncChild(0, ddMod);
      parent->setSourcePrecision(parent->getDecimalPrecision());
      }
#endif
   }

void
J9::Z::CodeGenerator::lowerTreesPostChildrenVisit(TR::Node * parent, TR::TreeTop * treeTop, vcount_t visitCount)
   {
   J9::CodeGenerator::lowerTreesPostChildrenVisit(parent, treeTop, visitCount);

   // J9, Z
   //
   if (self()->codegenSupportsLoadlessBNDCheck() &&
      parent->getOpCode().isBndCheck() &&
      (parent->getFirstChild()->getOpCode().isLoadVar() ||
      parent->getSecondChild()->getOpCode().isLoadVar()))
      {
      TR::Node * memChild = parent->getFirstChild()->getOpCode().isLoadVar()?parent->getFirstChild():parent->getSecondChild();

      if (memChild->getVisitCount() != self()->comp()->getVisitCount() && memChild->getReferenceCount() > 1 && performTransformation(self()->comp(), "%sRematerializing memref child %p from BNDCheck node\n", OPT_DETAILS, memChild))
         {
         memChild->decReferenceCount();
         TR::Node *newNode = TR::Node::copy(memChild);
         newNode->setReferenceCount(1);
         parent->setChild(parent->findChildIndex(memChild), newNode);
         }
      }
   }


void
J9::Z::CodeGenerator::lowerTreeIfNeeded(
      TR::Node *node,
      int32_t childNumberOfNode,
      TR::Node *parent,
      TR::TreeTop *tt)
   {
   TR::Compilation *comp = self()->comp();
   J9::CodeGenerator::lowerTreeIfNeeded(node, childNumberOfNode, parent, tt);

   if (self()->yankIndexScalingOp() &&
       (node->getOpCodeValue() == TR::aiadd || node->getOpCodeValue() == TR::aladd ) )
      {
      // 390 sees a lot of scaling ops getting stuck between BNDSchk and array read/write
      // causing heavy AGIs.  This transformation pulls the scaling opp up a tree to unpin it.
      //

      // Looking for trees that look like this:

      // BNDCHK / BNDCHKwithSpineCHK
      //   iiload
      //     ==>aRegLoad
      //   iiload
      //     ==>aRegLoad

      // iaload
      //   aiadd    <=====  You are here
      //     ==>aRegLoad
      //     isub
      //       imul   <=== Find this node and anchor it up above the BNDCHK
      //         ==>iiload
      //         iconst 4
      //       iconst -16

      TR::TreeTop* prevPrevTT = NULL;
      TR::TreeTop* prevTT = tt->getPrevTreeTop();

      while ( prevTT                                                &&
             (prevTT->getNode()->getOpCodeValue() == TR::iRegStore ||
              prevTT->getNode()->getOpCodeValue() == TR::aRegStore ||
              prevTT->getNode()->getOpCodeValue() == TR::asynccheck ||
              ((prevTT->getNode()->getOpCodeValue() == TR::treetop) &&
               (!prevTT->getNode()->getFirstChild()->getOpCode().hasSymbolReference() ||
                prevTT->getNode()->getFirstChild()->getOpCode().isLoad()))))
         {
         prevTT = prevTT->getPrevTreeTop();
         }

      // Pull scaling op up above the arrayStoreCheck as performing the scaling op right before the store is a horrible AGI.
      if (tt->getPrevTreeTop() &&
          tt->getNode()->getOpCodeValue() == TR::ArrayStoreCHK &&
          node->getSecondChild()->getNumChildren() >= 2)
         {
         // The general tree that we are matching is:
         //  aladd        <=====  You are here
         //    ==>iaload
         //    lsub
         //      lmul     <=====  Find this node and anchor it up above the ArrayStoreCHK
         //        i2l
         //          ==>iRegLoad
         //
         //  However, with internal pointers, there may or may not be an isub/lsub for arrayheader.  If there is no
         //  arrayheader isub/lsub, we will see a tree as such:
         //
         //  aladd (internal ptr)       <=====  You are here
         //    ==>iaload
         //    lshl     <=====  Find this node and anchor it up above the ArrayStoreCHK
         //      i2l
         //        ==>iRegLoad
         //
         // As such, we will check the second child of the aiadd/aladd, and see if it's the mul/shift operation.
         // If not, we'll get the subsequent first child.
         TR::Node* mulNode = node->getSecondChild();

         if (mulNode->getOpCodeValue() != TR::imul && mulNode->getOpCodeValue() != TR::ishl &&
             mulNode->getOpCodeValue() != TR::lmul && mulNode->getOpCodeValue() != TR::lshl)
            mulNode = node->getSecondChild()->getFirstChild();

         if ((mulNode->getOpCodeValue() == TR::imul || mulNode->getOpCodeValue() == TR::ishl || mulNode->getOpCodeValue() == TR::lmul || mulNode->getOpCodeValue() == TR::lshl) &&
             (performTransformation(comp, "%sYank mul above ArrayStoreChk [%p] \n", OPT_DETAILS, node)))
            {
            TR::TreeTop * ttNew = TR::TreeTop::create(comp, TR::Node::create(TR::treetop, 1, mulNode));
            tt->getPrevTreeTop()->insertAfter(ttNew);
            }
         }
      else if (prevTT                                           &&
          (prevPrevTT = prevTT->getPrevTreeTop())               &&
           prevTT->getNode()->getOpCode().isBndCheck()          &&
           node->getSecondChild()->getNumChildren() >= 2              )
         {
         // The general tree that we are matching is:
         //  aladd        <=====  You are here
         //    ==>iaload
         //    lsub
         //      lmul     <=====  Find this node and anchor it up above the BNDCHK
         //        i2l
         //          ==>iRegLoad
         //
         //  However, with internal pointers, there may or may not be an isub/lsub for arrayheader.  If there is no
         //  arrayheader isub/lsub, we will see a tree as such:
         //
         //  aladd (internal ptr)       <=====  You are here
         //    ==>iaload
         //    lshl     <=====  Find this node and anchor it up above the BNDCHK
         //      i2l
         //        ==>iRegLoad
         //
         // As such, we will check the second child of the aiadd/aladd, and see if it's the mul/shift operation.
         // If not, we'll get the subsequent first child.
         TR::Node* mulNode = node->getSecondChild();

         if (mulNode->getOpCodeValue() != TR::imul && mulNode->getOpCodeValue() != TR::ishl &&
             mulNode->getOpCodeValue() != TR::lmul && mulNode->getOpCodeValue() != TR::lshl)
            mulNode = node->getSecondChild()->getFirstChild();

         TR::Node *prevNode = prevTT->getNode();
         TR::Node *bndchkIndex = prevNode->getOpCode().isSpineCheck() ?
                                   prevNode->getChild(3) :              // TR::BNDCHKwithSpineCHK
                                   prevNode->getSecondChild();          // TR::BNDCHK

         bool doIt = false;

         doIt |= ((mulNode->getOpCodeValue() == TR::imul || mulNode->getOpCodeValue() == TR::ishl)                     &&
                 (mulNode->getFirstChild() == bndchkIndex)); // Make sure the BNDCHK is for this ind var

         doIt |= ((mulNode->getOpCodeValue() == TR::lmul || mulNode->getOpCodeValue() == TR::lshl) &&
                 (mulNode->getFirstChild()->getOpCodeValue() == TR::i2l &&       // 64-bit memrefs have an extra iu2l
                 // Make sure the BNDCHKxxx is for this ind var
                 (mulNode->getFirstChild() == bndchkIndex ||
                  mulNode->getFirstChild()->getFirstChild() == bndchkIndex ||
                  (bndchkIndex->getNumChildren() >= 1 &&
                   mulNode->getFirstChild() == bndchkIndex->getFirstChild())) ));

         if (doIt && performTransformation(comp, "%sYank mul [%p] \n", OPT_DETAILS, node))
            {
            TR::TreeTop * ttNew = TR::TreeTop::create(comp, TR::Node::create(TR::treetop, 1, mulNode));
            prevPrevTT->insertAfter(ttNew);
            }
         }

      }

   // J9, Z
   //
   // On zseries, convert aconst to iaload of aconst 0 and move it to its own new treetop
   if (comp->target().cpu.isZ() && !self()->profiledPointersRequireRelocation() &&
         node->getOpCodeValue() == TR::aconst && node->isClassUnloadingConst())
      {
      TR::Node * dummyNode = TR::Node::create(node, TR::aconst, 0);
      TR::Node *constCopy;
      TR::SymbolReference *intShadow;

      dumpOptDetails(comp, "transforming unloadable aconst %p \n", node);

      constCopy =TR::Node::copy(node);
      intShadow = self()->symRefTab()->findOrCreateGenericIntShadowSymbolReference((intptr_t)constCopy);
      intShadow->setLiteralPoolAddress();

      TR::Node::recreate(node, TR::aloadi);
      node->setNumChildren(1);
      node->setSymbolReference(intShadow);
      node->setAndIncChild(0,dummyNode);


      tt->getPrevTreeTop()->insertAfter(TR::TreeTop::create(comp,TR::Node::create(TR::treetop, 1, node)));
      node->decReferenceCount();
      parent->setAndIncChild(childNumberOfNode, node);
      }

   // J9, Z
   //
   if (comp->target().cpu.isZ() && node->getOpCodeValue() == TR::aloadi && node->isUnneededIALoad())
      {
      ListIterator<TR_Pair<TR::Node, int32_t> > listIter(&_ialoadUnneeded);
      TR_Pair<TR::Node, int32_t> *ptr;
      uintptr_t temp;
      int32_t updatedTemp;
      for (ptr = listIter.getFirst(); ptr; ptr = listIter.getNext())
         {
         temp = (uintptr_t)ptr->getValue();
         updatedTemp = (int32_t) temp;
         if (ptr->getKey() == node && temp != node->getReferenceCount())
            {
            node->setUnneededIALoad(false);
            break;
            }
         }
      }

   }

TR::S390EyeCatcherDataSnippet *
J9::Z::CodeGenerator::CreateEyeCatcher(TR::Node * node)
   {
   // 88448:  Cold Eyecatcher is used for padding of endPC so that Return Address for exception snippets will never equal the endPC.
   TR::S390EyeCatcherDataSnippet * eyeCatcherSnippet = new (self()->trHeapMemory()) TR::S390EyeCatcherDataSnippet(self(),node);
   _snippetDataList.push_front(eyeCatcherSnippet);
   return eyeCatcherSnippet;
   }

/**
 * Input reg can be NULL (when called for a store node or other type that does not return a register)
 */
void
J9::Z::CodeGenerator::widenUnicodeSignLeadingSeparate(TR::Node *node, TR_PseudoRegister *reg, int32_t endByte, int32_t bytesToClear, TR::MemoryReference *targetMR)
   {
   TR_ASSERT(node->getType().isAnyUnicode(),"widenUnicodeSignLeadingSeparate is only valid for unicode types (type = %s)\n",node->getDataType().toString());
   TR_ASSERT( targetMR->rightAlignMemRef() || targetMR->leftAlignMemRef(),"widenUnicodeSignLeadingSeparate is only valid for aligned memory references\n");
   if (bytesToClear > 0)
      {
      TR_StorageReference *storageRef = reg ? reg->getStorageReference() : NULL;
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\twidenUnicodeSignLeadingSeparate: node %p, reg %s targetStorageRef #%d, endByte %d, bytesToClear %d\n",
            node,reg?self()->getDebug()->getName(reg):"0",storageRef?storageRef->getReferenceNumber():0,endByte,bytesToClear);
      targetMR = reuseS390LeftAlignedMemoryReference(targetMR, node, storageRef, self(), endByte);
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\tgen MVC of size 2 to move unicode leading separate sign code left by %d bytes to the widened left aligned position\n",bytesToClear);
      TR::MemoryReference *originalSignCodeMR = generateS390LeftAlignedMemoryReference(*targetMR, node, bytesToClear, self(), endByte);
      int32_t mvcSize = 2;
      generateSS1Instruction(self(), TR::InstOpCode::MVC, node,
                             mvcSize-1,
                             targetMR,
                             originalSignCodeMR);

      self()->genZeroLeftMostUnicodeBytes(node, reg, endByte - TR::DataType::getUnicodeSignSize(), bytesToClear, targetMR);
      }
   }

#define TR_MAX_UNPKU_SIZE 64
/**
 * Input reg can be NULL (when called for a store node or other type that does not return a register)
 */
void
J9::Z::CodeGenerator::genZeroLeftMostUnicodeBytes(TR::Node *node, TR_PseudoRegister *reg, int32_t endByte, int32_t bytesToClear, TR::MemoryReference *targetMR)
   {
   TR_ASSERT(node->getType().isAnyUnicode(),"genZeroLeftMostUnicodeDigits is only valid for unicode types (type = %d)\n",node->getDataType().toString());
   TR_ASSERT(targetMR->rightAlignMemRef() || targetMR->leftAlignMemRef(),"genZeroLeftMostUnicodeBytes is only valid for aligned memory references\n");

   bool evaluatedPaddingAnchor = false;
   TR::Node *paddingAnchor = NULL;
   if (bytesToClear > 0)
      {
      TR_StorageReference *storageRef = reg ? reg->getStorageReference() : NULL;
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\tgenZeroLeftMostUnicodeBytes: node %p, reg %s targetStorageRef #%d, endByte %d, bytesToClear %d\n",
            node,reg?self()->getDebug()->getName(reg):"0",storageRef?storageRef->getReferenceNumber():0,endByte,bytesToClear);

      // zero 16 bytes (the fixed UNPKU source size) followed by a left aligned UNPKU of bytesToClear length to get 0030 repeated as the left most digits.
      // less efficient than the MVC literal copy above but doesn't require any extra storage as it is in-place
      int32_t tempSize = self()->getPackedToUnicodeFixedSourceSize();
      TR_StorageReference *tempStorageReference = TR_StorageReference::createTemporaryBasedStorageReference(tempSize, self()->comp());
      tempStorageReference->setTemporaryReferenceCount(1);
      TR::MemoryReference *tempMR = generateS390LeftAlignedMemoryReference(node, tempStorageReference, self(), tempSize, true, true); // enforceSSLimits=true, isNewTemp=true

      TR_ASSERT(bytesToClear <= TR_MAX_UNPKU_SIZE,"expecting bytesToClear (%d) <= TR_MAX_UNPKU_SIZE (%d)\n",bytesToClear,TR_MAX_UNPKU_SIZE);
      self()->genZeroLeftMostPackedDigits(node, NULL, tempSize, tempSize*2, tempMR);

      int32_t unpkuCount = ((bytesToClear-1)/TR_MAX_UNPKU_SIZE)+1;
      for (int32_t i = 0; i < unpkuCount; i++)
         {
         int32_t unpkuSize = std::min(bytesToClear,TR_MAX_UNPKU_SIZE);
         int32_t destOffset = i*TR_MAX_UNPKU_SIZE;
         if (self()->traceBCDCodeGen())
            traceMsg(self()->comp(),"\tgen %d of %d UNPKUs with dest size of %d destOffset of %d and fixed source size %d\n",i+1,unpkuCount,unpkuSize,destOffset,tempSize);
         generateSS1Instruction(self(), TR::InstOpCode::UNPKU, node,
                                unpkuSize-1,
                                generateS390LeftAlignedMemoryReference(*targetMR, node, destOffset, self(), endByte),
                                generateS390LeftAlignedMemoryReference(*tempMR, node, 0, self(), tempSize));
         bytesToClear-=unpkuSize;
         }
      tempStorageReference->decrementTemporaryReferenceCount();
      }
   if (!evaluatedPaddingAnchor)
      self()->processUnusedNodeDuringEvaluation(paddingAnchor);
   }

/**
 * Input reg can be NULL (when called for a store node or other type that does not return a register)
 */
void
J9::Z::CodeGenerator::widenZonedSignLeadingSeparate(TR::Node *node, TR_PseudoRegister *reg, int32_t endByte, int32_t bytesToClear, TR::MemoryReference *targetMR)
   {
   TR_ASSERT(node->getDataType() == TR::ZonedDecimalSignLeadingSeparate,
      "widenZonedSignLeadingSeparate is only valid for TR::ZonedDecimalSignLeadingSeparate (type=%s)\n",node->getDataType().toString());
   TR_ASSERT( targetMR->rightAlignMemRef() || targetMR->leftAlignMemRef(),"widenZonedSignLeadingSeparate is only valid for aligned memory references\n");
   if (bytesToClear > 0)
      {
      TR_StorageReference *storageRef = reg ? reg->getStorageReference() : NULL;
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\twidenZonedSignLeadingSeparate: node %p, reg %s targetStorageRef #%d, endByte %d, bytesToClear %d\n",
            node,reg?self()->getDebug()->getName(reg):"0",storageRef?storageRef->getReferenceNumber():0,endByte,bytesToClear);
      targetMR = reuseS390LeftAlignedMemoryReference(targetMR, node, storageRef, self(), endByte);
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\tgen MVC of size 1 to move zoned leading separate sign code left by %d bytes to the widened left aligned position\n",bytesToClear);
      TR::MemoryReference *originalSignCodeMR = generateS390LeftAlignedMemoryReference(*targetMR, node, bytesToClear, self(), endByte);
      int32_t mvcSize = 1;
      generateSS1Instruction(self(), TR::InstOpCode::MVC, node,
                             mvcSize-1,
                             targetMR,
                             originalSignCodeMR);
      self()->genZeroLeftMostZonedBytes(node, reg, endByte - TR::DataType::getZonedSignSize(), bytesToClear, targetMR);
      }
   }

/**
 * Input reg can be NULL (when called for a store node or other type that does not return a register)
 */
void
J9::Z::CodeGenerator::widenZonedSignLeadingEmbedded(TR::Node *node, TR_PseudoRegister *reg, int32_t endByte, int32_t bytesToClear, TR::MemoryReference *targetMR)
   {
   TR_ASSERT(node->getDataType() == TR::ZonedDecimalSignLeadingEmbedded,
      "widenZonedSignLeadingEmbedded is only valid for TR::ZonedDecimalSignLeadingEmbedded (type=%s)\n",node->getDataType().toString());
   TR_ASSERT( targetMR->rightAlignMemRef() || targetMR->leftAlignMemRef(),"widenZonedSignLeadingEmbedded is only valid for aligned memory references\n");
   if (bytesToClear > 0)
      {
      TR_StorageReference *storageRef = reg ? reg->getStorageReference() : NULL;
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\twidenZonedSignLeadingEmbedded: node %p, reg %s targetStorageRef #%d, endByte %d, bytesToClear %d\n",
            node,reg?self()->getDebug()->getName(reg):"0",storageRef?storageRef->getReferenceNumber():0,endByte,bytesToClear);
      self()->genZeroLeftMostZonedBytes(node, reg, endByte, bytesToClear, targetMR);
      targetMR = reuseS390LeftAlignedMemoryReference(targetMR, node, storageRef, self(), endByte);
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\tgen MVZ of size 1 to move leading sign code left by %d bytes to the widened left aligned position\n",bytesToClear);
      TR::MemoryReference *originalSignCodeMR = generateS390LeftAlignedMemoryReference(*targetMR, node, bytesToClear, self(), endByte);
      int32_t mvzSize = 1;
      generateSS1Instruction(self(), TR::InstOpCode::MVZ, node,
                             mvzSize-1,
                             targetMR,
                             generateS390LeftAlignedMemoryReference(*originalSignCodeMR, node, 0, self(), originalSignCodeMR->getLeftMostByte()));
         {
         if (self()->traceBCDCodeGen()) traceMsg(self()->comp(),"\tgenerate OI 0xF0 to force original leading sign code at offset=bytesToClear=%d\n",bytesToClear);
         generateSIInstruction(self(), TR::InstOpCode::OI, node, originalSignCodeMR, TR::DataType::getZonedCode());
         }
      }
   }

void
J9::Z::CodeGenerator::genZeroLeftMostZonedBytes(TR::Node *node, TR_PseudoRegister *reg, int32_t endByte, int32_t bytesToClear, TR::MemoryReference *targetMR)
   {
   TR_ASSERT(node->getType().isAnyZoned(),"genZeroLeftMostZonedBytes is only valid for zoned types (type = %s)\n",node->getDataType().toString());
   TR_ASSERT(targetMR->rightAlignMemRef() || targetMR->leftAlignMemRef(),"genZeroLeftMostZonedBytes is only valid for aligned memory references\n");
   TR::Node *paddingAnchor = NULL;
   bool evaluatedPaddingAnchor = false;
   if (bytesToClear > 0)
      {
      TR_StorageReference *storageRef = reg ? reg->getStorageReference() : NULL;
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\tgenZeroLeftMostZoneBytes: (%s) %p, reg %s targetStorageRef #%d, endByte %d, bytesToClear %d\n",
            node->getOpCode().getName(),node,reg?self()->getDebug()->getName(reg):"0",storageRef?storageRef->getReferenceNumber():0,endByte,bytesToClear);

         {
         targetMR = reuseS390LeftAlignedMemoryReference(targetMR, node, storageRef, self(), endByte);

         generateSIInstruction(self(), TR::InstOpCode::MVI, node, targetMR, TR::DataType::getZonedZeroCode());
         if (bytesToClear > 2)
            {
            int32_t overlapMVCSize = bytesToClear-1;
            generateSS1Instruction(self(), TR::InstOpCode::MVC, node,
                                   overlapMVCSize-1,
                                   generateS390LeftAlignedMemoryReference(*targetMR, node, 1, self(), targetMR->getLeftMostByte()),
                                   generateS390LeftAlignedMemoryReference(*targetMR, node, 0, self(), targetMR->getLeftMostByte()));
            }
         }
      if (reg)
         reg->addRangeOfZeroBytes(endByte-bytesToClear, endByte);
      }

   if (!evaluatedPaddingAnchor)
      self()->processUnusedNodeDuringEvaluation(paddingAnchor);
   }

bool
J9::Z::CodeGenerator::alwaysGeneratesAKnownCleanSign(TR::Node *node)
   {
   switch (node->getOpCodeValue())
      {
      case TR::ud2pd:
         return true;
      default:
         return false;
      }
   return false;
   }

bool
J9::Z::CodeGenerator::alwaysGeneratesAKnownPositiveCleanSign(TR::Node *node)
   {
   switch (node->getOpCodeValue())
      {
      case TR::ud2pd:
         return true;
      default:
         return false;
      }
   return false;
   }

TR_RawBCDSignCode
J9::Z::CodeGenerator::alwaysGeneratedSign(TR::Node *node)
   {
   switch (node->getOpCodeValue())
      {
      case TR::ud2pd:
         return raw_bcd_sign_0xc;
      default:
         return raw_bcd_sign_unknown;
      }
   return raw_bcd_sign_unknown;
   }

TR_OpaquePseudoRegister *
J9::Z::CodeGenerator::allocateOpaquePseudoRegister(TR::DataType dt)
   {
   TR_OpaquePseudoRegister *temp =  new (self()->trHeapMemory()) TR_OpaquePseudoRegister(dt, self()->comp());
   self()->addAllocatedRegister(temp);
   if (self()->getDebug())
      self()->getDebug()->newRegister(temp);
   return temp;
   }


TR_OpaquePseudoRegister *
J9::Z::CodeGenerator::allocateOpaquePseudoRegister(TR_OpaquePseudoRegister *reg)
   {
   TR_OpaquePseudoRegister *temp =  new (self()->trHeapMemory()) TR_OpaquePseudoRegister(reg, self()->comp());
   self()->addAllocatedRegister(temp);
   if (self()->getDebug())
      self()->getDebug()->newRegister(temp);
   return temp;
   }


TR_PseudoRegister *
J9::Z::CodeGenerator::allocatePseudoRegister(TR_PseudoRegister *reg)
   {
   TR_PseudoRegister *temp =  new (self()->trHeapMemory()) TR_PseudoRegister(reg, self()->comp());
   self()->addAllocatedRegister(temp);
   if (self()->getDebug())
      self()->getDebug()->newRegister(temp);
   return temp;
   }

/**
 * OPR in this context is OpaquePseudoRegister
 */
TR_OpaquePseudoRegister *
J9::Z::CodeGenerator::evaluateOPRNode(TR::Node * node)
   {
   bool isBCD = node->getType().isBCD();
   bool isAggr = node->getType().isAggregate();
   TR_ASSERT(isBCD || isAggr,"evaluateOPRNode node %s (%p) must be BCD/Aggr type\n",node->getOpCode().getName(),node);
   TR::Register *reg = isBCD ? self()->evaluateBCDNode(node) : self()->evaluate(node);
   TR_OpaquePseudoRegister *opaquePseudoReg = reg->getOpaquePseudoRegister();
   TR_ASSERT(opaquePseudoReg,"reg must be some type of opaquePseudoRegister on node %s (%p)\n",node->getOpCode().getName(),node);
   return opaquePseudoReg;
   }

void
J9::Z::CodeGenerator::freeUnusedTemporaryBasedHint(TR::Node *node)
   {
   TR_StorageReference *hint = node->getOpCode().canHaveStorageReferenceHint() ? node->getStorageReferenceHint() : NULL;
   if (hint && hint->isTemporaryBased() && hint->getTemporaryReferenceCount() == 0)
      {
      self()->pendingFreeVariableSizeSymRef(hint->getTemporarySymbolReference());
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\tfreeing (pending) unused hint symRef #%d (%s) on %s (%p)\n",
            hint->getReferenceNumber(),
            self()->getDebug()->getName(hint->getTemporarySymbol()),
            node->getOpCode().getName(),
            node);
      }
   }

bool
J9::Z::CodeGenerator::storageReferencesMatch(TR_StorageReference *ref1, TR_StorageReference *ref2)
   {
   bool refMatch = false;
   if (ref1->isNodeBased() && (ref1->getNode()->getOpCode().isLoadVar() || ref1->getNode()->getOpCode().isStore()) &&
       ref2->isNodeBased() && (ref2->getNode()->getOpCode().isLoadVar() || ref2->getNode()->getOpCode().isStore()) &&
       self()->loadOrStoreAddressesMatch(ref1->getNode(), ref2->getNode()))
      {
      if (ref1->getNode()->getSize() != ref2->getNode()->getSize())
         {
         if (self()->traceBCDCodeGen())
            traceMsg(self()->comp(),"\tnode based storageRefs match = false : ref1 (#%d) and ref2 (#%d) addresses match but node1 %s (%p) size=%d != node2 %s (%p) size=%d\n",
               ref1->getReferenceNumber(),ref2->getReferenceNumber(),
               ref1->getNode()->getOpCode().getName(),ref1->getNode(),ref1->getNode()->getSize(),
               ref2->getNode()->getOpCode().getName(),ref2->getNode(),ref2->getNode()->getSize());
         refMatch = false;
         }
      else
         {
         if (self()->traceBCDCodeGen())
            traceMsg(self()->comp(),"\tnode based storageRefs match = true : ref1 (#%d) %s (%p) == ref2 (#%d) %s (%p)\n",
               ref1->getReferenceNumber(),ref1->getNode()->getOpCode().getName(),ref1->getNode(),
               ref2->getReferenceNumber(),ref2->getNode()->getOpCode().getName(),ref2->getNode());
         refMatch = true;
         }
      }
   else if (ref1->isTemporaryBased() &&
            ref2->isTemporaryBased() &&
            ref1->getSymbolReference() == ref2->getSymbolReference())
      {
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\ttemp based storageRefs match = true : ref1 (#%d) == ref2 (#%d) match\n",ref1->getReferenceNumber(),ref2->getReferenceNumber());
      refMatch = true;
      }
   return refMatch;
   }

void
J9::Z::CodeGenerator::processUnusedStorageRef(TR_StorageReference *ref)
   {
   if (ref == NULL || !ref->isNodeBased())
      return;

   if (ref->getNodeReferenceCount() == 0)
      return;

   TR::Node *refNode = ref->getNode();
   TR::Node *addrChild = NULL;
   if (refNode->getOpCode().isIndirect() ||
       (ref->isConstantNodeBased() && refNode->getNumChildren() > 0))
      {
      addrChild = refNode->getFirstChild();
      }

   if (self()->traceBCDCodeGen())
      traceMsg(self()->comp(),"\tprocessUnusedStorageRef ref->node %s (%p) with addrChild %s (%p)\n",
         refNode->getOpCode().getName(),refNode,addrChild?addrChild->getOpCode().getName():"NULL",addrChild);

   if (addrChild)
      {
      TR_ASSERT(addrChild->getType().isAddress(),"addrChild %s (%p) not an address type\n",addrChild->getOpCode().getName(),addrChild);
      if (ref->getNodeReferenceCount() == 1)
         {
         if (self()->traceBCDCodeGen())
            traceMsg(self()->comp(),"\t\tstorageRef->nodeRefCount %d == 1 so processUnusedAddressNode %s (%p) (refCount %d)\n",
               ref->getNodeReferenceCount(),addrChild->getOpCode().getName(),addrChild,addrChild->getReferenceCount());
         self()->processUnusedNodeDuringEvaluation(addrChild);
         }
      else if (self()->traceBCDCodeGen())
         {
         traceMsg(self()->comp(),"\t\tstorageRef->nodeRefCount %d > 1 so do not decRefCounts of unusedAddressNode %s (%p) (refCount %d)\n",
            ref->getNodeReferenceCount(),addrChild->getOpCode().getName(),addrChild,addrChild->getReferenceCount());
         }
      }

   if (self()->traceBCDCodeGen())
      traceMsg(self()->comp(),"\tdec storageRef->nodeRefCount %d->%d\n",
         ref->getNodeReferenceCount(),ref->getNodeReferenceCount()-1);

   ref->decrementNodeReferenceCount();
   }

TR_PseudoRegister *
J9::Z::CodeGenerator::allocatePseudoRegister(TR::DataType dt)
   {
   TR_PseudoRegister *temp =  new (self()->trHeapMemory()) TR_PseudoRegister(dt, self()->comp());
   self()->addAllocatedRegister(temp);
   if (self()->getDebug())
      self()->getDebug()->newRegister(temp);
   return temp;
   }

#define TR_ACCUMULATOR_NODE_BUDGET 50

/// canUseSingleStoreAsAnAccumulator does not use visitCounts (as they are
/// already in use at this point) but instead the slightly less / exact
/// getRegister() == NULL checks
///
/// In a pathological case, such as doubly commoned nodes under the same store
/// there is a potential for an exponential number of nodes to / be visited. To
/// guard against this maintain a count of nodes visited under one store and
/// compare against the budget below.
///
/// \note Today, it should be relatively easy to insert a Checklist, which
///       addresses the concern about visit counts above.
template <class TR_AliasSetInterface>
bool
J9::Z::CodeGenerator::canUseSingleStoreAsAnAccumulator(TR::Node *parent, TR::Node *node, TR::Node *store,TR_AliasSetInterface &storeAliases, TR::list<TR::Node*> *conflictingAddressNodes, bool justLookForConflictingAddressNodes, bool isChainOfFirstChildren, bool mustCheckAllNodes)
   {
   TR::Compilation *comp = self()->comp();

   // A note on isChainOfFirstChildren:
   // In RTC 75858, we saw the following trees for the following COBOL statements, where X is packed decimal:
   // COMPUTE X = X - 2.
   // COMPUTE X = 999 - X.
   //
   // pdstore "X"
   //   pdsub
   //     pdconst +999
   //     pdsub
   //       pdload "X"
   //       pdconst 2
   //
   // In this case, canUseSingleStoreAsAnAccumulator is returning true because the pdload of X is the first child of its parent, but it's missing
   // the fact that the parent pdsub is itself a second child. This is resulting in the value of X getting clobbered with +999.
   //
   // To solve this, isChainOfFirstChildren is used. It is set to true initially, and it will only remain true when called for a node's first child
   // if it was already true. In the example above, it would be true for the pdsub and the pdconst +999 and false for any other nodes.
   LexicalTimer foldTimer("canUseSingleStore", comp->phaseTimer());

   if (self()->traceBCDCodeGen())
      traceMsg(comp,"\t\texamining node %s (%p) (usage/budget = %d/%d)\n",node->getOpCode().getName(),node,self()->getAccumulatorNodeUsage(),TR_ACCUMULATOR_NODE_BUDGET);

   if (self()->getAccumulatorNodeUsage() > TR_ACCUMULATOR_NODE_BUDGET)
      {
      if (self()->traceBCDCodeGen())
         traceMsg(comp,"\t\ta^a : disallow useAccum=false as node budget %d exceeded for store %s (%p)\n",
            TR_ACCUMULATOR_NODE_BUDGET,store->getOpCode().getName(),store);
      return false;
      }

   if (!mustCheckAllNodes)
      {
      if (self()->endAccumulatorSearchOnOperation(node))
         {
         if (self()->traceBCDCodeGen())
            traceMsg(comp,"\t\t\tallow -- found node %s (%p) with endSearch = yes\n",node->getOpCode().getName(),node);
         if (conflictingAddressNodes->empty())
            {
            return true;
            }
         else
            {
            // do not have to worry about overlaps but still must descend to look for conflictingAddressNodes
            if (self()->traceBCDCodeGen())
               traceMsg(comp,"\t\tconflictingAddressNodes list is not empty so continue searching for conflictingAddressNodes\n");
            justLookForConflictingAddressNodes = true;
            }
         }
      else if (!justLookForConflictingAddressNodes && nodeMightClobberAccumulatorBeforeUse(node))
         {
         // RTC 75966: In general, we want to check all nodes until we hit a node for which endAccumulatorSearchOnOperation is true
         // (eg. zd2pd; we won't accumulate across a type change). However, if we have already done something that might clobber the
         // destination, we still need to search all nodes. So, mustCheckAllNodes is initially false but will be set to true when we
         // first encounter any node for which endAccumulatorSearchOnOperation is false. If we've already hit such a node, and we're
         // continuing the search to find conflicting address nodes, then mustCheckAllNodes can remain false.
         //
         // pdstore "a"
         //   pdsub
         //     pdconst
         //     zd2pd
         //       zdload "a"
         //
         // Previously, the code would hit the zd2pd and stop, incorrectly accumulating into "a" and potentially clobbering "a" before
         // the pdload was evaluated. Now, we'll set mustCheckAllNodes to true when we hit the pdsub, and the code that won't let us
         // accumulate because the pdload "a" isn't on a chain of first children will kick in, and we won't accumulate to "a".
         if (!mustCheckAllNodes && self()->traceBCDCodeGen())
            traceMsg(comp,"\t\tFound a node that could clobber the accumulator before use; must check all children\n");

         mustCheckAllNodes = true;
         }
      }

   TR::Node *nodeForAliasing = NULL;
   if (!justLookForConflictingAddressNodes)
      {
      // An already evaluated OpaquePseudoRegister may have had its storageReference updated to point to
      // memory different from that on the node itself (e.g. updated by skipCopyOnStore checks in pdstoreEvaluator
      // or to a temp by ssrClobberEvaluate)
      // It is this updated memory that will be used to generate the actual instructions/memoryReferences therefore it is
      // this memory that must be used for the overlap tests
      if (node->getOpaquePseudoRegister())
         {
         TR_StorageReference *storageRef = node->getOpaquePseudoRegister()->getStorageReference();
         if (self()->traceBCDCodeGen())
            traceMsg(comp,"\t\tfound evaluated reg %s : storageRef #%d ",self()->getDebug()->getName(node->getOpaquePseudoRegister()),storageRef->getReferenceNumber());
         if (storageRef->isTemporaryBased())
            {
            if (self()->traceBCDCodeGen()) traceMsg(comp,"(tempBased)\n");
            TR::SymbolReference *tempSymRef = storageRef->getTemporarySymbolReference();
            // the rest of the code below expects a node but there is not one for tempBased storageRefs so construct/reuse one on the fly
            if (_dummyTempStorageRefNode == NULL)
               {
               _dummyTempStorageRefNode = TR::Node::createWithSymRef(node, comp->il.opCodeForDirectLoad(node->getDataType()), 0, tempSymRef);
               }
            else
               {
               TR::Node::recreate(_dummyTempStorageRefNode, comp->il.opCodeForDirectLoad(node->getDataType()));
               _dummyTempStorageRefNode->setSymbolReference(tempSymRef);
               }
            if (node->getType().isBCD())
               _dummyTempStorageRefNode->setDecimalPrecision(node->getDecimalPrecision());
            else
               TR_ASSERT(false,"unexpected type on node %s (%p)\n",node->getOpCode().getName(),node);
            nodeForAliasing = _dummyTempStorageRefNode;
            }
         else if (storageRef->isNonConstantNodeBased())
            {
            if (self()->traceBCDCodeGen()) traceMsg(comp,"(nodeBased storageRefNode %s (%p))\n",storageRef->getNode()->getOpCode().getName(),storageRef->getNode());
            TR_ASSERT(storageRef->getNode()->getOpCode().hasSymbolReference(),"storageRef node %s (%p) should have a symRef\n",
               storageRef->getNode()->getOpCode().getName(),storageRef->getNode());
            nodeForAliasing   = storageRef->getNode();
            }
         else
            {
            if (self()->traceBCDCodeGen()) traceMsg(comp,"(constNodeBased storageRefNode %s (%p))\n",storageRef->getNode()->getOpCode().getName(),storageRef->getNode());
            TR_ASSERT(storageRef->isConstantNodeBased(),"expecting storageRef #%d to be constant node based\n",storageRef->getReferenceNumber());
            }
         }
      else if (node->getOpCodeValue() != TR::loadaddr &&  // no aliasing implications to a simple loadaddr (it is not a deref)
               node->getOpCode().hasSymbolReference())
         {
         nodeForAliasing = node;
         }

      }

   TR::SymbolReference *symRefForAliasing = NULL;
   if (nodeForAliasing)
      symRefForAliasing = nodeForAliasing->getSymbolReference();

   if (self()->traceBCDCodeGen() && nodeForAliasing && symRefForAliasing)
      traceMsg(comp,"\t\tgot nodeForAliasing %s (%p), symRefForAliasing #%d\n",
         nodeForAliasing->getOpCode().getName(),nodeForAliasing,symRefForAliasing?symRefForAliasing->getReferenceNumber():-1);

   bool useAliasing = true;
   if (self()->traceBCDCodeGen() && useAliasing && !storeAliases.isZero(comp) && symRefForAliasing)
      {
      if (comp->getOption(TR_TraceAliases) && !symRefForAliasing->getUseDefAliases().isZero(comp))
         {
         traceMsg(comp, "\t\t\taliases for #%d: ",symRefForAliasing->getReferenceNumber());
         TR::SparseBitVector aliases(comp->allocator());
         symRefForAliasing->getUseDefAliases().getAliases(aliases);
         (*comp) << aliases << "\n";
         }
      traceMsg(comp,"\t\t\tsymRefForAliasing #%d isSet in storeAliases = %s\n",
         symRefForAliasing->getReferenceNumber(),storeAliases.contains(symRefForAliasing->getReferenceNumber(), comp) ? "yes":"no");
      }

   if (symRefForAliasing &&
       loadAndStoreMayOverlap(store, store->getSize(), nodeForAliasing, nodeForAliasing->getSize(), storeAliases)) // if aliases are present node can be of any node type (a call for example)
      {
      // allow expressions like a=a+b but not a=b+a
      if (parent &&
          nodeForAliasing->getOpCode().isLoadVar() &&
          (parent->getOpCode().isBasicPackedArithmetic()) &&
          parent->getFirstChild() == nodeForAliasing &&
          isChainOfFirstChildren &&
          self()->loadOrStoreAddressesMatch(store, nodeForAliasing))
         {
         if (self()->traceBCDCodeGen())
            traceMsg(comp,"\t\t\tallow hint (loadVar case) %s (%p) -- store %s #%d (%p) location = nodeForAliasing %s #%d (%p) location\n",
               parent->getOpCode().getName(),parent,
               store->getOpCode().getName(),store->getSymbolReference()->getReferenceNumber(),store,
               nodeForAliasing->getOpCode().getName(),symRefForAliasing->getReferenceNumber(),nodeForAliasing);

         return true;
         }
      else if (parent &&
               node->getOpaquePseudoRegister() &&
               nodeForAliasing->getOpCode().isStore() &&
               (parent->getOpCode().isBasicPackedArithmetic()) &&
               parent->getFirstChild() == node &&
               isChainOfFirstChildren &&
               self()->loadOrStoreAddressesMatch(store, nodeForAliasing))
         {
         // zdstoreA #y
         //    zdTrMultipleA
         //       zdload #y
         //
         // zdstoreB #y              <- store
         //    zdTrMultipleB         <- parent
         //       ==>zdTrMultipleA   <- node with nodeForAliasing zdstoreA
         if (self()->traceBCDCodeGen())
            traceMsg(comp,"\t\t\tallow hint (storeVar case) %s (%p) -- store %s #%d (%p) location = nodeForAliasing %s #%d (%p) location\n",
               parent->getOpCode().getName(),parent,
               store->getOpCode().getName(),store->getSymbolReference()->getReferenceNumber(),store,
               nodeForAliasing->getOpCode().getName(),symRefForAliasing->getReferenceNumber(),nodeForAliasing);

         return true;
         }
      // Catch this case
      // pdstore #y
      //    pdshr
      //       pdload #y
      // where the store is to the leading bytes of the load.  See RTC 95073
      else if (self()->isAcceptableDestructivePDShiftRight(store, nodeForAliasing))
         {
         if (self()->traceBCDCodeGen())
            traceMsg(comp,"\t\t\tallow hint (pdshr in place case) %s (%p) -- store %s #%d (%p) location = nodeForAliasing %s #%d (%p) location\n",
               parent->getOpCode().getName(),parent,
               store->getOpCode().getName(),store->getSymbolReference()->getReferenceNumber(),store,
               nodeForAliasing->getOpCode().getName(),symRefForAliasing->getReferenceNumber(),nodeForAliasing);

         return true;
         }
      else if (self()->isAcceptableDestructivePDModPrecision(store, nodeForAliasing))
         {
         if (self()->traceBCDCodeGen())
            traceMsg(comp,"\t\t\tallow hint (pdMod in place case) %s (%p) -- store %s #%d (%p) location = nodeForAliasing %s #%d (%p) location\n",
               parent->getOpCode().getName(),parent,
               store->getOpCode().getName(),store->getSymbolReference()->getReferenceNumber(),store,
               nodeForAliasing->getOpCode().getName(),symRefForAliasing->getReferenceNumber(),nodeForAliasing);

         return true;
         }
      else
         {
         if (useAliasing &&   // checking useAliasing here because in the no info case the above loadAndStoreMayOverlap already did the pattern match
             self()->storageMayOverlap(store, store->getSize(), nodeForAliasing, nodeForAliasing->getSize()) == TR_NoOverlap)
            {
            // get a second opinion -- the aliasing says the operations overlap but perhaps it is too conservative
            // so do pattern matching based test to see if the operations are actually disjoint
            if (self()->traceBCDCodeGen())
               traceMsg(comp,"\t\t\tcheck children -- useAccum=true aliasing test failed but pattern match passed for nodeForAliasing %s (%p) with symRefForAliasing #%d\n",
                  nodeForAliasing->getOpCode().getName(),nodeForAliasing,symRefForAliasing->getReferenceNumber());
            }
         else
            {
            if (self()->traceBCDCodeGen())
               traceMsg(comp,"\t\t\tdisallow -- useAccum=false for nodeForAliasing %s (%p) with symRefForAliasing #%d\n",
                  nodeForAliasing->getOpCode().getName(),nodeForAliasing,symRefForAliasing->getReferenceNumber());
            return false;
            }
         }
      }

   // no need to descend below a load if loadAndStoreMayOverlap already has returned false -- we have our answer and there
   // is no overlap -- unless mustCheckAllNodes is true (something higher up could clobber the accumulator before it's used,
   // so make sure no one uses it). Never any need to descend below a node that's already been evaluated.
   if (node->getOpCode().isLoad())
      {
      if (self()->traceBCDCodeGen())
         traceMsg(comp,"\t\t\t%s -- found load %s (%p) under store %s (%p)\n", (mustCheckAllNodes ? "check children" : "allow"),
            node->getOpCode().getName(),node,store->getOpCode().getName(),store);
      if (!mustCheckAllNodes)
         return true;
      }
   else if (node->getRegister())
      {
      if (self()->traceBCDCodeGen())
         traceMsg(comp,"\t\t\tallow -- found base case evaluated reg %s on node %s (%p) under store %s (%p)\n",
            self()->getDebug()->getName(node->getRegister()),node->getOpCode().getName(),node,store->getOpCode().getName(),store);
      return true;
      }
   // Check conflicting address nodes on the parent node too
   if (self()->foundConflictingNode(node, conflictingAddressNodes))
      {
      // If the same unevaluated BCD/Aggr node is present in the address child and the value child then prevent the accum flag from being set
      // The problem is that if the store is used an accum then there will be a circular evaluation as the value child will have to evaluate
      // the address child in order to the get accumulated store address
      if (self()->traceBCDCodeGen())
         traceMsg(comp,"\t\t\ta^a: disallow -- useAccum=false because node %s (%p) was found commoned from address tree on %s (%p)\n",
            node->getOpCode().getName(),node,store->getOpCode().getName(),store);
      return false;
      }

   for (int32_t i = node->getNumChildren() - 1; i >= 0; --i) // recurse from original node and not nodeForAliasing
      {
      TR::Node *child = node->getChild(i);
      if (self()->foundConflictingNode(child, conflictingAddressNodes))
         {
         // If the same unevaluated BCD/Aggr node is present in the address child and the value child then prevent the accum flag from being set
         // The problem is that if the store is used an accum then there will be a circular evaluation as the value child will have to evaluate
         // the address child in order to the get accumulated store address
         if (self()->traceBCDCodeGen())
            traceMsg(comp,"\t\t\ta^a: disallow -- useAccum=false because node %s (%p) was found commoned from address tree on %s (%p)\n",
               child->getOpCode().getName(),child,store->getOpCode().getName(),store);
         return false;
         }
      else
         {
         // If so far we have an unbroken chain of first children, the chain continues if this node is the value child.
         // If this isn't the value child (eg. second operand of an arith op), or the chain was broken, then we definitely
         // can't continue the chain.
         bool continueChainOfFirstChildren = false;
         if (child == node->getValueChild() && isChainOfFirstChildren)
            continueChainOfFirstChildren = true;

         self()->incAccumulatorNodeUsage();
         if (!canUseSingleStoreAsAnAccumulator(node, child, store, storeAliases, conflictingAddressNodes, justLookForConflictingAddressNodes, continueChainOfFirstChildren, mustCheckAllNodes))
            {
            if (!justLookForConflictingAddressNodes && self()->endHintOnOperation(node))
               {
               if (self()->traceBCDCodeGen())
                  traceMsg(comp,"\t\t\ta^a: : endHint mismatch -- node %s (%p)\n",node->getOpCode().getName(),node);
               }
            return false;
            }
         }
      }

   return true;
   }


// Z
bool
J9::Z::CodeGenerator::isAcceptableDestructivePDShiftRight(TR::Node *storeNode, TR::Node * nodeForAliasing)
   {
   TR::Node *shiftNode = NULL;
   TR::Node *loadNode = NULL;

   if (storeNode->getOpCodeValue() != TR::pdstore && storeNode->getOpCodeValue() != TR::pdstorei)
      return false;

   if (storeNode->getValueChild()->getOpCodeValue() == TR::pdshr)
     shiftNode = storeNode->getValueChild();

   if (!shiftNode)
      return false;

   if (shiftNode->getDecimalRound() != 0)
      return false;

   if (shiftNode->getChild(0)->getOpCode().isLoadVar())
      loadNode = shiftNode->getChild(0);

   if (!loadNode)
      return false;

   if (nodeForAliasing && loadNode != nodeForAliasing)
      return false;

   return self()->loadOrStoreAddressesMatch(storeNode, loadNode);

   }


///
///    pdstorei s=5
///        addr+3
///        pdModPrec s=5
///           pdX (with nodeForAliasing address : addr) s=8
///
/// The above IL is truncating the sourceNode (pdX) and storing the result back
/// to the same field right aligned / In this case it is ok to accumulate as an
/// exact right aligned subfield of the source is being operated on
///
bool
J9::Z::CodeGenerator::isAcceptableDestructivePDModPrecision(TR::Node *storeNode, TR::Node *nodeForAliasing)
   {
   return false; // currently disabled as this leads to a completely overlapping MVC that is even slower than going thru a temp
                 // should be re-enabled when redundant MVC removal is complete

   if (storeNode->getOpCodeValue() != TR::pdstore && storeNode->getOpCodeValue() != TR::pdstorei)
      return false;

   if (!nodeForAliasing->getOpCode().isIndirect())
      return false;

   if (storeNode->getValueChild()->getOpCodeValue() != TR::pdModifyPrecision)
      return false;

   TR::Node *modPrecNode = storeNode->getValueChild();
   TR::Node *sourceNode = modPrecNode->getFirstChild();

   bool matchSourceAndAliasingNode = false;
   if (sourceNode == nodeForAliasing)
      {
      matchSourceAndAliasingNode = true;
      }
   else if (sourceNode->getOpaquePseudoRegister() &&
            sourceNode->getOpaquePseudoRegister()->getStorageReference()->isNonConstantNodeBased() &&
            sourceNode->getOpaquePseudoRegister()->getStorageReference()->getNode() == nodeForAliasing)
      {
      matchSourceAndAliasingNode = true;
      }

   if (!matchSourceAndAliasingNode)
      return false;

   int32_t storePrec = storeNode->getDecimalPrecision();
   int32_t modPrec = modPrecNode->getDecimalPrecision();
   int32_t sourcePrec = sourceNode->getDecimalPrecision();

   if (storePrec != modPrec)
      return false;

   if (sourceNode->getSize() != nodeForAliasing->getSize())
      return false;

   if (modPrec >= sourcePrec) // only handling truncations and this is not a truncation
      return false;

   int32_t truncatedBytes = nodeForAliasing->getSize() - storeNode->getSize();

   return self()->validateAddressOneToAddressOffset(truncatedBytes,
                                            nodeForAliasing->getFirstChild(),
                                            nodeForAliasing->getSymbolReference()->getOffset(),
                                            storeNode->getFirstChild(),
                                            storeNode->getSymbolReference()->getOffset(),
                                            NULL,
                                            self()->traceBCDCodeGen()); // _baseLoadsThatAreNotKilled = NULL (not tracking here)
   }

// Z
bool
J9::Z::CodeGenerator::validateAddressOneToAddressOffset(int32_t expectedOffset,
                                                         TR::Node *addr1,
                                                         int64_t addr1ExtraOffset,
                                                         TR::Node *addr2,
                                                         int64_t addr2ExtraOffset,
                                                         TR::list<TR::Node*> *_baseLoadsThatAreNotKilled,
                                                         bool trace) // _baseLoadsThatAreNotKilled can be NULL
   {
   TR_ASSERT(addr1->getType().isAddress(),"addr1 %s (%p) must an address type\n",addr1->getOpCode().getName(),addr1);
   TR_ASSERT(addr2->getType().isAddress(),"addr2 %s (%p) must an address type\n",addr2->getOpCode().getName(),addr2);

   bool canGetOffset = false;
   int32_t addrOffset = 0;
   self()->getAddressOneToAddressTwoOffset(&canGetOffset, addr1, addr1ExtraOffset, addr2, addr2ExtraOffset, &addrOffset, _baseLoadsThatAreNotKilled, trace);
   if (!canGetOffset)
      {
      if (trace)
         traceMsg(self()->comp(),"\tvalidateAddressOneToAddressOffset = false : could not compute offset between addr1 %s (%p) (+%lld) and addr2 %s (%p) (+%lld)\n",
            addr1->getOpCode().getName(),addr1,addr1ExtraOffset,addr2->getOpCode().getName(),addr2,addr2ExtraOffset);
      return false;
      }

   // some examples:
   // pdstorei (highDigitsStore or lowDigitsStore) p=15,s=8
   //    addr1
   // ...
   // The addr2 could be from an MVO:
   // tt
   //    mvo
   //       dstAddr
   //       addr2 = addr1 + expectedOffset (3)
   //
   if (addrOffset != expectedOffset)
      {
      if (trace)
         traceMsg(self()->comp(),"\tvalidateAddressOneToAddressOffset = false : addrOffset %d not the expected value of %d between addr1 %s (%p) (+%lld) and addr2 %s (%p) (+%lld)\n",
            addrOffset,expectedOffset,addr1->getOpCode().getName(),addr1,addr1ExtraOffset,addr2->getOpCode().getName(),addr2,addr2ExtraOffset);
      return false;
      }
   return true;
   }

// _baseLoadsThatAreNotKilled is if the caller is doing its own tracking of loads that are not killed between treetops
// For these loads syntactic address matching of the loads is allowed even if the node pointers themselves are not the same
// That is
//
//    load1 "A"
//
//    intervening treetops checked by caller not to kill "A"
//
//    load2 "A"
//
// load1 and load2 can be matched for symRef and other properties as the caller has checked that "A" is not killed in between the loads
//
// resultOffset = address2 - address1 or equivalently address2 = address1 + resultOffset
//
// Z
void
J9::Z::CodeGenerator::getAddressOneToAddressTwoOffset(bool *canGetOffset,
                                                       TR::Node *addr1,
                                                       int64_t addr1ExtraOffset,
                                                       TR::Node *addr2,
                                                       int64_t addr2ExtraOffset,
                                                       int32_t *offset,
                                                       TR::list<TR::Node*> *_baseLoadsThatAreNotKilled,
                                                       bool trace) // _baseLoadsThatAreNotKilled can be NULL
   {
   TR::Compilation *comp = self()->comp();
   int64_t offset64 = 0;
   *canGetOffset = false;
   *offset=0;
   bool foundOffset = false;

   if (!foundOffset &&
       self()->addressesMatch(addr1, addr2))
      {
      foundOffset = true;
      offset64 = (addr2ExtraOffset - addr1ExtraOffset);
      if (trace)
         traceMsg(comp,"\t\t(addr2 %s (%p) + %lld) = (addr1 %s (%p) + %lld) + offset (%lld) : node matches case\n",
            addr2->getOpCode().getName(),addr2,addr2ExtraOffset,addr1->getOpCode().getName(),addr1,addr1ExtraOffset,offset64);
      }

   if (!foundOffset &&
       self()->isSupportedAdd(addr1) &&
       self()->isSupportedAdd(addr2) &&
       self()->addressesMatch(addr1->getFirstChild(), addr2->getFirstChild()))
      {
      if (addr1->getSecondChild()->getOpCode().isIntegralConst() &&
          addr2->getSecondChild()->getOpCode().isIntegralConst())
         {
         foundOffset = true;
         int64_t addr1Offset = addr1->getSecondChild()->get64bitIntegralValue() + addr1ExtraOffset;
         int64_t addr2Offset = addr2->getSecondChild()->get64bitIntegralValue() + addr2ExtraOffset;
         offset64 = (addr2Offset - addr1Offset);
         if (trace)
            traceMsg(comp,"\t\t(addr2 %s (%p) + %lld) = (addr1 %s (%p) + %lld) + offset (%lld) : both adds case\n",
               addr2->getOpCode().getName(),addr2,addr2ExtraOffset,addr1->getOpCode().getName(),addr1,addr1ExtraOffset,offset64);
         }
      }

   // =>i2a
   //
   // aiadd
   //    =>i2a
   //    iconst 8
   //
   if (!foundOffset &&
       self()->isSupportedAdd(addr2) &&
       addr2->getSecondChild()->getOpCode().isIntegralConst() &&
       self()->addressesMatch(addr1, addr2->getFirstChild()))
      {
      foundOffset = true;
      int64_t addr2Offset = addr2->getSecondChild()->get64bitIntegralValue() + addr2ExtraOffset;
      offset64 = addr2Offset;
      if (trace)
         traceMsg(comp,"\t\t(addr2 %s (%p) + %lld) = addr1 %s (%p) + offset (%lld) : 2nd add case\n",
            addr2->getOpCode().getName(),addr2,addr2ExtraOffset,addr1->getOpCode().getName(),addr1,offset64);
      }

   if (!foundOffset &&
       _baseLoadsThatAreNotKilled &&
       !_baseLoadsThatAreNotKilled->empty() &&
       self()->isSupportedAdd(addr1) &&
       self()->isSupportedAdd(addr2) &&
       addr1->getSecondChild()->getOpCode().isIntegralConst() &&
       addr2->getSecondChild()->getOpCode().isIntegralConst())
      {
      TR::Node *baseLoad1 = self()->getAddressLoadVar(addr1->getFirstChild(), trace);
      TR::Node *baseLoad2 = self()->getAddressLoadVar(addr2->getFirstChild(), trace);

    if (baseLoad1 != NULL && baseLoad2 != NULL &&
          (std::find(_baseLoadsThatAreNotKilled->begin(),_baseLoadsThatAreNotKilled->end(), baseLoad1) !=
                 _baseLoadsThatAreNotKilled->end()) &&
          self()->directLoadAddressMatch(baseLoad1, baseLoad2, trace))
         {
         foundOffset = true;
         int64_t addr1Offset = addr1->getSecondChild()->get64bitIntegralValue() + addr1ExtraOffset;
         int64_t addr2Offset = addr2->getSecondChild()->get64bitIntegralValue() + addr2ExtraOffset;
         offset64 = (addr2Offset - addr1Offset);
         if (trace)
            traceMsg(comp,"\t\t(addr2 %s (%p) + %lld) = (addr1 %s (%p) + %lld) + offset (%lld) : baseLoad1 %s (%p) in notKilledList, both adds case\n",
               addr2->getOpCode().getName(),addr2,addr2ExtraOffset,
               addr1->getOpCode().getName(),addr1,addr1ExtraOffset,
               offset64,
               baseLoad1->getOpCode().getName(),baseLoad1);
         }
      }

   if (!foundOffset ||
       self()->isOutOf32BitPositiveRange(offset64, trace))
      {
      return;
      }

   *canGetOffset = true;
   *offset = (int32_t)offset64;

   return;
   }

TR::Node *
J9::Z::CodeGenerator::getAddressLoadVar(TR::Node *node, bool trace)
   {
   // allow a non truncating address cast from integral types
   if ((node->getOpCodeValue() == TR::i2a || node->getOpCodeValue() == TR::l2a) &&
        (node->getSize() == node->getFirstChild()->getSize()))
      {
      node = node->getFirstChild();
      }

   if (node->getOpCode().isLoadVar())
      return node;
   else
      return NULL;
   }

void
J9::Z::CodeGenerator::addStorageReferenceHints(TR::Node *node)
   {
   TR::list<TR::Node*> leftMostNodesList(getTypedAllocator<TR::Node*>(self()->comp()->allocator()));
   TR::StackMemoryRegion stackMemoryRegion(*self()->trMemory());

   self()->markStoreAsAnAccumulator(node);

   TR::Node *bestNode = NULL;
   if (self()->traceBCDCodeGen())
      traceMsg(self()->comp(),"\n--start-- examining cg treeTop %s (%p)\n",node->getOpCode().getName(),node);
   int32_t storeSize = 0;
   self()->examineNode(NULL, node, bestNode, storeSize, leftMostNodesList);
   if (self()->traceBCDCodeGen())
      traceMsg(self()->comp(),"--end--   examining cg treeTop %s (%p)\n\n",node->getOpCode().getName(),node);

   }


// Z
void
J9::Z::CodeGenerator::examineNode(
      TR::Node *parent,
      TR::Node *node,
      TR::Node *&bestNode,
      int32_t &storeSize,
      TR::list<TR::Node*> &leftMostNodesList)
   {
   TR::Compilation *comp = self()->comp();
   TR::Node *checkNode = node;
   bool isAccumStore = node->getOpCode().canUseStoreAsAnAccumulator();
   bool isLoad = node->getOpCode().isLoad();
   bool endHintOnNode = self()->endHintOnOperation(node) || isLoad;
   bool isConversionToNonAggrOrNonBCD = node->getOpCode().isBCDToNonBCDConversion();

   if (isAccumStore)
      storeSize = node->getSize();

   if (!node->hasBeenVisitedForHints()) // check other nodes using hasBeenVisitedForHints
      {
      if (self()->traceBCDCodeGen())
         traceMsg(comp,"\tvisiting node - %s (%p), bestNode - %s (%p) (endHintOnNode=%s)\n",
            node->getOpCode().getName(),node,bestNode?bestNode->getOpCode().getName():"NULL",bestNode,endHintOnNode?"true":"false");

      node->setHasBeenVisitedForHints();

      bool nodeCanHaveHint = node->getOpCode().canHaveStorageReferenceHint();
      bool isInterestingStore = nodeCanHaveHint || isAccumStore || isConversionToNonAggrOrNonBCD;
      bool isNonOverflowPDShift = node->getOpCode().isPackedShift() && node->getOpCodeValue() != TR::pdshlOverflow;
      bool isSafeWideningConversion =
         TR::ILOpCode::isPackedConversionToWiderType(node->getOpCodeValue()) && node->getDecimalPrecision() <= node->getFirstChild()->getDecimalPrecision();

      if (isInterestingStore &&
          (isNonOverflowPDShift ||
           isSafeWideningConversion ||
           node->getOpCodeValue() == TR::pdModifyPrecision) &&
          (node->getFirstChild()->getReferenceCount() == 1))
         {
         // pdshl/pdModPrec nodes take care of the zeroing the top nibble in the pad byte for the final shifted value (so we can skip clearing
         // the nibble in the intermediate arithmetic result.
         // non-widening pd2zd nodes only select the exact number of digits so the top nibble will be ignored for even precision values
         // If the child has a refCount > 1 then subsequent uses may not also be have a pdshl/pdModPrec/pd2zd parent so we must conservatively clear the nibble right
         // after the arithmetic operation.
         // TODO: if all subsequent uses are also under truncating pdshl/pdModPrec nodes then the clearing can also be skipped -- but finding this out will
         // require more analysis
         node->getFirstChild()->setSkipPadByteClearing(true);
         }

      if (nodeCanHaveHint &&
          bestNode &&
          node->getStorageReferenceSize() > bestNode->getStorageReferenceSize() && // end hint before
          endHintOnNode &&                                                                     // end hint after
          !leftMostNodesList.empty())
         {

         // when the current node will end a hint before and after then tag any nodes above this node with the store hint so it can store into the final receiver
         // pdstore  <- hint
         //    pdshr    <- tag this list node with the pdstore hint
         //       ud2pd <- node (endHintOnNode=true and ud2pd size > store size) -- alloc a new temp
         //
         // when the current node only ends the hint after (such as a zd2pd) then delay calling processNodeList so the zd2pd will also get the store hint
         // pdstore  <- hint
         //    pdshr    <- tag this list node with the pdstore hint
         //       zd2pd <- node (endHintOnNode=true and ud2pd size <= store size) <- tag this list node with the pdstore hint
         //
         if (self()->traceBCDCodeGen())
            traceMsg(comp,"\t\tendHintOnNode=true so call processNodeList before examining ending hint node %p\n",node);
         // processNodeList will reset storeSize so save and restore the current storeSize value so it will persist for the current node
         // pd2ud    <-sets storeSize to 16
         //    zd2pd <-node (should also use storeSize=16)
         // by persisting it for the zd2pd node this operation can initialize up to 16 bytes for its parent
         int32_t savedStoreSize = storeSize;
         self()->processNodeList(bestNode, storeSize, leftMostNodesList);
         storeSize = savedStoreSize;
         }

      TR::ILOpCodes opCode = node->getOpCodeValue();
      if (isInterestingStore)
         {
         // TODO: if a pdstore is to an auto then continually increase the size of this auto so it is the biggest on the left
         //       most subtree (i.e. force it to be the bestNode)
         if ((bestNode == NULL) ||
             (node->getStorageReferenceSize() > bestNode->getStorageReferenceSize()) ||
             (self()->nodeRequiresATemporary(node) && bestNode->getOpCode().isStore() && !self()->isAcceptableDestructivePDShiftRight(bestNode, NULL /* let the function find the load node */)))
            {
            if (!isAccumStore || node->useStoreAsAnAccumulator())
               {
               bestNode = node;
               if (self()->traceBCDCodeGen())
                  {
                  if (isAccumStore)
                     traceMsg(comp,"\t\tfound new store (canUse = %s) bestNode - %s (%p) with actual size %d and storageRefResultSize %d\n",
                        bestNode->useStoreAsAnAccumulator() ? "yes":"no", bestNode->getOpCode().getName(),bestNode, bestNode->getSize(),bestNode->getStorageReferenceSize());
                  else
                     traceMsg(comp,"\t\tfound new non-store bestNode - %s (%p) (isConversionToNonAggrOrNonBCD=%s, isForcedTemp=%s) with actual size %d and storageRefResultSize %d\n",
                        bestNode->getOpCode().getName(),bestNode,isConversionToNonAggrOrNonBCD?"yes":"no",self()->nodeRequiresATemporary(node)?"yes":"no",bestNode->getSize(),bestNode->getStorageReferenceSize());
                  }
               }
            }

         if (!isAccumStore && !isConversionToNonAggrOrNonBCD && !isLoad) // don't add stores or bcd2x or load nodes to the list
            {
            if (self()->traceBCDCodeGen())
               traceMsg(comp,"\t\tadd node - %s (%p) to list\n",node->getOpCode().getName(),node);
            leftMostNodesList.push_front(node);
            }
         }
      // end hints on some nodes so
      //    1) the same storageReference isn't used for both sides of a zd2pd or pd2zd conversion
      //    2) a storageReference for a commoned node is not used 'across' a conversion:
      //             pdadd
      //                i2pd        :: end hint here so the commoned pdshr storageReference is not used for the i2pd/pdadd subexpression
      //                   iadd
      //                      pd2i  :: start new hint here
      //                         ==>pdshr
      if (endHintOnNode)
         {
         self()->processNodeList(bestNode, storeSize, leftMostNodesList);
         switch (node->getOpCodeValue())
            {
            case TR::pd2ud:
            case TR::pd2udsl:
            case TR::pd2udst:
               storeSize = TR::DataType::packedDecimalPrecisionToByteLength(node->getDecimalPrecision()); // i.e. the size of the result in packed bytes (node->getSize is in ud bytes)
               break;
            default:
               break;
            }

         if (self()->traceBCDCodeGen())
            traceMsg(comp,"\t\tendHintOnNode=true for node - %s (%p) setting storeSize to %d\n",node->getOpCode().getName(),node,storeSize);
         }

      // visit value child first for indirect stores so the possible store hint is not lost on the address child
      if (node->getOpCode().isStoreIndirect())
         {
         int32_t valueChildIndex = node->getOpCode().isIndirect() ? 1 : 0;
         self()->examineNode(node, node->getChild(valueChildIndex), bestNode, storeSize, leftMostNodesList);
         for (int32_t i = 0; i < node->getNumChildren(); i++)
            {
            if (i != valueChildIndex)
               self()->examineNode(node, node->getChild(i), bestNode, storeSize, leftMostNodesList);
            }
         }
      else
         {
         for (int32_t i = 0; i < node->getNumChildren(); i++)
            self()->examineNode(node, node->getChild(i), bestNode, storeSize, leftMostNodesList);
         }
      }
   else
      {
      checkNode = parent;
      if (self()->traceBCDCodeGen())
         traceMsg(comp,"\tnot descending node - %s (%p) because it has been visited already\n",node->getOpCode().getName(),node);
      TR_OpaquePseudoRegister *reg = node->getOpCodeValue() == TR::BBStart ? NULL : node->getOpaquePseudoRegister();
      if (reg)
         {
         if (self()->traceBCDCodeGen())
            traceMsg(comp,"\t\tnode - %s (%p) with reg %s is an already evaluated bcd node with refCount=%d\n",
               node->getOpCode().getName(),node,self()->getDebug()->getName(static_cast<TR::Register*>(reg)),node->getReferenceCount());

         if (!reg->getStorageReference()->isTemporaryBased())
            {
            if (self()->traceBCDCodeGen())
               traceMsg(comp,"\t\t\treg storageRef #%d is not a temp so do not update bestNode with node - %s (%p) but do reset reg %s isInit to false\n",
                  reg->getStorageReference()->getReferenceNumber(),node->getOpCode().getName(),node,self()->getDebug()->getName(reg));
            // setting to false here forces the commoned expression to re-initialize the register to the new hint for one of two reasons:
            // 1) functionally required for non-temps as these storage references can not be clobbered (they are program variables or constants)
            // 2) for perf to avoid a clobber evaluate (temp to temp move) of the already initialized reg -- instead begin using the store hint and leave the temp alone
            reg->setIsInitialized(false);
            }
         else if (bestNode && bestNode->getOpCode().isStore() && node->getReferenceCount() >= 1) // use >= 1 so useNewStoreHint can always be used for ZAP widening on initializations
            {
            if (self()->traceBCDCodeGen())
               traceMsg(comp,"\t\t\treg storageRef #%d with a store bestNode so do not update bestNode with node - %s (%p) refCount=%d\n",
                  reg->getStorageReference()->getReferenceNumber(),node->getOpCode().getName(),node,node->getReferenceCount());
            }
         else
            {
            if (self()->traceBCDCodeGen())
               traceMsg(comp,"\t\t\treg storageRef #%d is a final-use (refCount==1) temp so set bestNode to node - %s (%p) reg->isInit=%s (and reuse temp storageRef))\n",
                  reg->getStorageReference()->getReferenceNumber(),node->getOpCode().getName(),node,reg->isInitialized()?"yes":"no");
            if (bestNode)
               storeSize = bestNode->getSize();
            bestNode = node;
            }
         }
      }

   if ((leftMostNodesList.empty()) || (checkNode == leftMostNodesList.front()))       // just finished with a left most path and there are nodes to tag with hint
                      // just finished with a left most but there are no nodes to tag with a hint
      {
      if (self()->traceBCDCodeGen())
         {
         traceMsg(comp,"\t\tdetected the end of a left most path because ");
         if ((!leftMostNodesList.empty()) && (checkNode == leftMostNodesList.front()))
            traceMsg(comp,"checkNode - %s (%p) matches head of list %p\n",checkNode?checkNode->getOpCode().getName():"NULL",checkNode,leftMostNodesList.front());
         else if (leftMostNodesList.empty()) // i.e. bestNode is your only node so you haven't seen any other higher up nodes to add to the list
            traceMsg(comp,"bestNode - %s (%p) is set and the head of list is NULL for node - %s (%p)\n",
               (bestNode ? bestNode->getOpCode().getName():"NULL"),bestNode,node->getOpCode().getName(),node);
         else
            traceMsg(comp,"of an unknown reason for node - %s (%p) (FIXME - add a reason) \n",node->getOpCode().getName(),node);
         }
      if (leftMostNodesList.empty())
         {
         if (self()->traceBCDCodeGen())
            traceMsg(comp,"\tleftMostNodesList is empty so clear bestNode - %s (%p->NULL) for current node - %s (%p)\n",
               bestNode?bestNode->getOpCode().getName():"NULL",bestNode,node->getOpCode().getName(),node);
         bestNode = NULL;
         storeSize = 0;
         }
      else
         {
         if (self()->traceBCDCodeGen())
            traceMsg(comp,"\tcalling processNodeList with bestNode - %s (%p) because leftMostNodesList is not empty for current node - %s (%p)\n",
               bestNode?bestNode->getOpCode().getName():"NULL",bestNode,node->getOpCode().getName(),node);
         self()->processNodeList(bestNode, storeSize, leftMostNodesList);
         }
      }
   }


// Z
void
J9::Z::CodeGenerator::processNodeList(
      TR::Node *&bestNode,
      int32_t &storeSize,
      TR::list<TR::Node*> &leftMostNodesList)
   {
   TR::Compilation *comp = self()->comp();

   if (bestNode)
      {
      bool keepTrackOfSharedNodes = false;
      TR::SymbolReference *memSlotHint = NULL;
      TR_StorageReference *storageRefHint = NULL;
      if (bestNode->getOpaquePseudoRegister())
         {
         TR_OpaquePseudoRegister *reg = bestNode->getOpaquePseudoRegister();
         if (self()->traceBCDCodeGen())
            traceMsg(comp,"\t\tbestNode - %s (%p) already has a register (%s) so use reg->getStorageReference #%d and %s\n",
               bestNode->getOpCode().getName(),bestNode,self()->getDebug()->getName(reg),reg->getStorageReference()->getReferenceNumber(),
               self()->getDebug()->getName(reg->getStorageReference()->getSymbol()));
         if (reg->getStorageReference()->isTemporaryBased() &&
             storeSize > reg->getLiveSymbolSize())
            {
            if (self()->traceBCDCodeGen())
               traceMsg(comp,"\t\treg->getStorageReference #%d is tempBased and requested storeSize %d > regLiveSymSize %d so increase tempSize\n",
                  reg->getStorageReference()->getReferenceNumber(),storeSize,reg->getLiveSymbolSize());
            reg->increaseTemporarySymbolSize(storeSize-reg->getLiveSymbolSize());
            }
         storageRefHint = reg->getStorageReference();
         }
      else if (bestNode->getOpCode().isStore())
         {
         if (self()->traceBCDCodeGen())
            traceMsg(comp,"\t\tbestNode - %s (%p) is a store so create a new node based storage reference #%d\n",
               bestNode->getOpCode().getName(),bestNode,bestNode->getSymbolReference()->getReferenceNumber());
         storageRefHint = TR_StorageReference::createNodeBasedHintStorageReference(bestNode, comp);
         }
      else
         {
         if (!leftMostNodesList.empty())
            {
            int32_t bestNodeSize = bestNode->getStorageReferenceSize();
            int32_t tempSize = std::max(storeSize, bestNodeSize);
            if (self()->traceBCDCodeGen())
               {
               traceMsg(comp,"\t\tbestNode - %s (%p) is a BCD arithmetic or conversion op (isBCDToNonBCDConversion %s) and list is not empty so allocate a new temporary based storage reference\n",
                  bestNode->getOpCode().getName(),bestNode,bestNode->getOpCode().isBCDToNonBCDConversion()?"yes":"no");
               traceMsg(comp,"\t\tsize of temp is max(storeSize,bestNodeSize) = max(%d,%d) = %d\n", storeSize, bestNodeSize, tempSize);
               }
            storageRefHint = TR_StorageReference::createTemporaryBasedStorageReference(tempSize, comp);
            if (tempSize == bestNodeSize)
               {
               keepTrackOfSharedNodes=true;
               if (self()->traceBCDCodeGen())
                  traceMsg(comp,"\t\tsetting keepTrackOfSharedNodes=true because hintSize is based on a non-store operation (bestNode %s - %p)\n",
                     bestNode->getOpCode().getName(),bestNode);
               }
            }
         else if (self()->traceBCDCodeGen())
            {
            traceMsg(comp,"\t\tbestNode %p is a BCD arithmetic or conversion op but list is empty so do not allocate a new temporary based storage reference\n",bestNode);
            }
         }
      for (auto listIt = leftMostNodesList.begin(); listIt != leftMostNodesList.end(); ++listIt)
         {
         TR_ASSERT(!(*listIt)->getOpCode().isStore(),"stores should not be in the list\n");
         if (self()->traceBCDCodeGen())
            traceMsg(comp,"\ttag (*listIt) - %s (%p) with storageRefHint #%d (%s)\n",
               (*listIt)->getOpCode().getName(),*listIt,storageRefHint->getReferenceNumber(),self()->getDebug()->getName(storageRefHint->getSymbol()));
         (*listIt)->setStorageReferenceHint(storageRefHint);
         if (keepTrackOfSharedNodes)
            storageRefHint->addSharedNode(*listIt);


         // If a child node has lower precision than the storage hint make sure its skipPadByteClearing is off
         if (TR::ILOpCode::isPackedConversionToWiderType((*listIt)->getOpCodeValue()))
            {
            TR::Node *firstChild = (*listIt)->getFirstChild();
            if (firstChild->chkSkipPadByteClearing() &&
                storageRefHint->getSymbolSize() > TR::DataType::getSizeFromBCDPrecision((*listIt)->getDataType(), firstChild->getDecimalPrecision()))
               {
               if (self()->traceBCDCodeGen())
                  traceMsg(comp,"\tUnset skipPadByteClearing on node %s (%p): storage ref hint has size %d and converted node has size %d\n",
                           firstChild->getOpCode().getName(),firstChild,storageRefHint->getSymbolSize(),TR::DataType::getSizeFromBCDPrecision((*listIt)->getDataType(), firstChild->getDecimalPrecision()));
               firstChild->setSkipPadByteClearing(false);
               }
            }
         }
      }

   storeSize = 0;
   bestNode = NULL;
   leftMostNodesList.clear();
   }



// Z
void
J9::Z::CodeGenerator::markStoreAsAnAccumulator(TR::Node *node)
   {
   TR::Compilation *comp = self()->comp();
   LexicalTimer foldTimer("markStoreAsAccumulator", comp->phaseTimer());

   if (!node->getOpCode().isStore())
      return;

   if (self()->traceBCDCodeGen())
      traceMsg(comp,"markStoreAsAnAccumulator for node %s (%p) - useAliasing=%s\n",node->getOpCode().getName(),node,"yes");

   TR::list<TR::Node*> conflictingAddressNodes(getTypedAllocator<TR::Node*>(comp->allocator()));

   if (node->getOpCode().canUseStoreAsAnAccumulator())
      {
      TR_UseDefAliasSetInterface aliases = node->getSymbolReference()->getUseDefAliases();

      if (self()->traceBCDCodeGen())
         {
         traceMsg(comp, "\nUseAsAnAccumulator check for store %s (%p) #%d",node->getOpCode().getName(),node,node->getSymbolReference()->getReferenceNumber());
         if (comp->getOption(TR_TraceAliases) && !aliases.isZero(comp))
            {
            traceMsg(comp, ", storeAliases : ");
            TR::SparseBitVector printAliases(comp->allocator());
            aliases.getAliases(printAliases);
            (*comp) << printAliases;
            }
         traceMsg(comp,"\n");
         }

      if (node->getOpCode().isIndirect())
         {
         conflictingAddressNodes.clear();
         if (self()->traceBCDCodeGen())
            traceMsg(comp,"\tlook for conflicting nodes in address subtree starting at %s (%p)\n",node->getFirstChild()->getOpCode().getName(),node->getFirstChild());
         self()->collectConflictingAddressNodes(node, node->getFirstChild(), &conflictingAddressNodes);
         }

      if (self()->traceBCDCodeGen())
         traceMsg(comp,"\n\texamine nodes in value subtree starting at %s [%s]\n",node->getValueChild()->getOpCode().getName(),node->getValueChild()->getName(comp->getDebug()));

      self()->setAccumulatorNodeUsage(0);
      // parent=NULL, justLookForConflictingAddressNodes=false, isChainOfFirstChildren=true, mustCheckAllNodes=false
      bool canUse = self()->canUseSingleStoreAsAnAccumulator(NULL, node->getValueChild(), node, aliases, &conflictingAddressNodes, false, true, false);
      if (self()->traceBCDCodeGen())
         traceMsg(comp,"\tfinal accumulatorNodeUsage = %d/%d\n",self()->getAccumulatorNodeUsage(),TR_ACCUMULATOR_NODE_BUDGET);
      self()->setAccumulatorNodeUsage(0);

      if (canUse &&
          performTransformation(comp, "%sset new UseStoreAsAnAccumulator=true on %s [%s]\n", OPT_DETAILS, node->getOpCode().getName(),node->getName(comp->getDebug())))
         {
         node->setUseStoreAsAnAccumulator(canUse);
         }
      }
   }


/// If true, this node's operation might overwrite an accumulator by evaluating one child before loading
/// the value from another, if we choose to accumulate. (Accumulation may still be safe, but we'll need
/// to investigate all child nodes to be sure).
/// eg.
//
/// pdstore "a"
///   pdsub
///     pdconst
///     zd2pd
///       zdload "a"
///
/// Accumulating to "a" is incorrect here because the pdconst will get evaluated into "a" before the
/// zdload is evaluated, so when we encounter the pdsub, we need to check all children.
///
bool nodeMightClobberAccumulatorBeforeUse(TR::Node *node)
   {
   TR_ASSERT(node != NULL, "NULL node in nodeMightClobberAccumulatorBeforeUse\n");

   if (!node->getType().isBCD())
      return false;

   if (node->getOpCode().isAnyBCDArithmetic())
      return true;

   if (node->getNumChildren() == 1)
      return false;

   if (node->getOpCode().isShift()
       || node->getOpCode().isConversion()
       || node->getOpCode().isSetSign()
       || node->getOpCode().isSetSignOnNode()
       || node->getOpCode().isExponentiation())
      return false;

   return true;
   }

void
J9::Z::CodeGenerator::correctBadSign(TR::Node *node, TR_PseudoRegister *reg, int32_t endByte, TR::MemoryReference *memRef)
   {
   if (reg && reg->hasKnownBadSignCode())
      {
      int32_t sign = 0xf;  // can choose any valid sign here but 0xf will be the cheapest to set
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\tcorrectBadSign node %p: reg %s hasKnownBadSignCode()=true so force sign to a valid sign 0x%x\n",node,self()->getDebug()->getName(reg),sign);
      self()->genSignCodeSetting(node, NULL, endByte, generateS390RightAlignedMemoryReference(*memRef, node, 0, self()), sign, reg, 0, false); // numericNibbleIsZero=false
      }
   }

int32_t
J9::Z::CodeGenerator::genSignCodeSetting(TR::Node *node, TR_PseudoRegister *targetReg, int32_t endByte, TR::MemoryReference *targetMR, int32_t sign, TR_PseudoRegister *srcReg, int32_t digitsToClear, bool numericNibbleIsZero)
   {
   TR::CodeGenerator *cg = self();
   TR::Compilation *comp = cg->comp();
   int32_t digitsCleared = 0;
   int32_t signCodeOffset = TR::DataType::getSignCodeOffset(node->getDataType(), endByte);

   TR_ASSERT(sign == TR::DataType::getIgnoredSignCode() || (sign >= TR::DataType::getFirstValidSignCode() && sign <= TR::DataType::getLastValidSignCode()),"unexpected sign of 0x%x in genSignCodeSetting\n",sign);

   if (sign == TR::DataType::getIgnoredSignCode())
      {
      if (self()->traceBCDCodeGen())
         traceMsg(comp,"\tgenSignCodeSetting: node=%p, sign==ignored case srcReg %s, targetReg %s, srcReg->isSignInit %d, targetReg->isSignInit %d\n",
            node,srcReg?cg->getDebug()->getName(srcReg):"NULL",targetReg?cg->getDebug()->getName(targetReg):"NULL",srcReg?srcReg->signStateInitialized():0,targetReg?targetReg->signStateInitialized():0);
      if (targetReg != srcReg)
         {
         if (targetReg)
            {
            if (srcReg)
               {
               targetReg->transferSignState(srcReg, true); // digitsLost=true -- conservatively set as this may be part of a truncation
               }
            else
               {
               targetReg->setHasKnownBadSignCode();
               if (cg->traceBCDCodeGen())
                  traceMsg(comp,"\tsign==ignored case and srcReg==NULL so setHasKnownBadSignCode=true on targetReg %s\n",cg->getDebug()->getName(targetReg));
               }
            }
         }
      return digitsCleared;
      }

   int32_t srcSign = TR::DataType::getInvalidSignCode();
   if (srcReg)
      {
      if (srcReg->hasKnownOrAssumedSignCode())
         srcSign = srcReg->getKnownOrAssumedSignCode();
      else if (srcReg->hasTemporaryKnownSignCode())
         srcSign = srcReg->getTemporaryKnownSignCode();
      }

   sign = (sign&0xF);
   bool isEffectiveNop = (srcSign == sign);

   if (self()->traceBCDCodeGen())
      traceMsg(comp,"\tgenSignCodeSetting: node=%p, endByte=%d, sign=0x%x, signCodeOffset=%d, srcReg=%s, digitsToClear=%d, numericNibbleIsZero=%s (srcSign=0x%x, hasCleanSign=%s, hasPrefSign=%s, isEffectiveNop=%s)\n",
         node,endByte,sign,signCodeOffset,srcReg ? self()->getDebug()->getName(srcReg):"NULL",digitsToClear,numericNibbleIsZero ?"yes":"no",
         srcSign,srcReg && srcReg->hasKnownOrAssumedCleanSign()?"true":"false",
         srcReg && srcReg->hasKnownOrAssumedPreferredSign()?"true":"false",isEffectiveNop?"yes":"no");

   if (isEffectiveNop)
      {
      if (srcReg && targetReg)
         targetReg->transferSignState(srcReg, true); // digitsLost=true -- conservatively set as this may be part of a truncation
      if (targetReg->signStateInitialized() == false) // when srcSign is from getTemporaryKnownSignCode()
         targetReg->setKnownSignCode(srcSign);
      return digitsCleared;
      }

   TR::MemoryReference *signCodeMR = generateS390LeftAlignedMemoryReference(*targetMR, node, 0, cg, endByte-signCodeOffset);

   // If the sign code is 0xc,0xd,0xe or 0xf then the top two bits are already set so an initial OI is not required and only an NI is required for some sign values
   bool topTwoBitsSet = false;
   bool knownSignIs0xC = false;
   bool knownSignIs0xF = false;
   if (srcReg)
      {
      topTwoBitsSet = srcReg->hasKnownOrAssumedCleanSign() || srcReg->hasKnownOrAssumedPreferredSign();
      if (srcSign != TR::DataType::getInvalidSignCode())
         {
         if (srcSign >= 0xc && srcSign <= 0xf)
            topTwoBitsSet = true;
         knownSignIs0xC = (srcSign == 0xc);
         knownSignIs0xF = (srcSign == 0xf);
         }
      }

   TR::DataType dt = node->getDataType();
   TR_ASSERT(dt == TR::PackedDecimal || dt == TR::ZonedDecimal || dt == TR::ZonedDecimalSignLeadingEmbedded,
      "genSignCodeSetting only valid for embedded sign types and not type %s\n",dt.toString());
   bool isPacked = (dt == TR::PackedDecimal);

   intptr_t litPoolOffset;
   switch (dt)
      {
      case TR::PackedDecimal:
      case TR::ZonedDecimal:
      case TR::ZonedDecimalSignLeadingEmbedded:
         {
         if (isPacked && digitsToClear >= 3)
            {
            int32_t bytesToSet = (digitsToClear+1)/2;
            int32_t leftMostByte = 0;
            TR::InstOpCode::Mnemonic op = TR::InstOpCode::BAD;
            switch (bytesToSet)
               {
               case 2:
               case 3:
                  op = TR::InstOpCode::MVHHI;
                  digitsCleared = 3;
                  leftMostByte = 2;
                  break;
               case 4:
               case 5:
               case 6:
               case 7:
                  op = TR::InstOpCode::MVHI;
                  digitsCleared = 7;
                  leftMostByte = 4;
                  break;
               default:
                  TR_ASSERT(bytesToSet >= 8,"unexpected bytesToSet value (%d) -- should be >= 8\n",bytesToSet);
                  op = TR::InstOpCode::MVGHI;
                  digitsCleared = 15;
                  leftMostByte = 8;
                  break;
               }
            signCodeMR->setLeftMostByte(leftMostByte);
            generateSILInstruction(cg, op, node, signCodeMR, sign);
            if (self()->traceBCDCodeGen())
               traceMsg(comp,"\t\tusing %d byte move imm (%s) for sign setting : set digitsCleared=%d\n",
                  leftMostByte,leftMostByte==8?"MVGHI":(leftMostByte==4)?"MVHI":"MVHHI",digitsCleared);
            }
         else if (numericNibbleIsZero || digitsToClear >= 1)
            {
            generateSIInstruction(cg, TR::InstOpCode::MVI, node, signCodeMR, isPacked ? sign : sign << 4);
            digitsCleared = 1;
            if (self()->traceBCDCodeGen()) traceMsg(comp,"\t\tusing MVI for sign setting : set digitsCleared=1\n");
            }
         else
            {
            if (knownSignIs0xF)
               {
               generateSIInstruction(cg, TR::InstOpCode::NI, node, signCodeMR, isPacked ? (0xF0 | sign) : (0x0F | (sign<<4)));
               }
            else if (topTwoBitsSet && sign == 0xc)
               {
               generateSIInstruction(cg, TR::InstOpCode::NI, node, signCodeMR, isPacked ? 0xFC : 0xCF);
               }
            else if (knownSignIs0xC && sign == 0xd)
               {
               generateSIInstruction(cg, TR::InstOpCode::OI, node, signCodeMR, isPacked ? 0x01 : 0x10);
               }
            else if (sign == 0xf)
               {
               generateSIInstruction(cg, TR::InstOpCode::OI, node, signCodeMR, isPacked ? 0x0F : 0xF0);
               }
            else
               {
                  {
                  generateSIInstruction(cg, TR::InstOpCode::OI, node, signCodeMR, isPacked ? 0x0F : 0xF0);
                  generateSIInstruction(cg, TR::InstOpCode::NI, node, generateS390LeftAlignedMemoryReference(*signCodeMR,
                                                                                                node,
                                                                                                0,
                                                                                                cg,
                                                                                                signCodeMR->getLeftMostByte()),
                                        isPacked ? (0xF0 | sign) : (0x0F | (sign<<4)));
                  }
               }
            }
         }
         break;
      default:
         TR_ASSERT(false,"dt %s not handled yet in genSignCodeSetting\n",node->getDataType().toString());
      }

   if (targetReg)
      targetReg->setKnownSignCode(sign);

   return digitsCleared;
   }

/**
 * Input reg can be NULL (when called for a store node or other type that does not return a register)
 */
void
J9::Z::CodeGenerator::widenBCDValue(TR::Node *node, TR_PseudoRegister *reg, int32_t startByte, int32_t endByte, TR::MemoryReference *targetMR)
   {
   TR_ASSERT(node->getType().isBCD(),
      "widenBCDValue is only valid for BCD types (type=%s)\n",node->getDataType().toString()); TR_ASSERT(targetMR->rightAlignMemRef() || targetMR->leftAlignMemRef(),"widenBCDValue is only valid for aligned memory references\n");
   TR_ASSERT(endByte >= startByte,"endByte (%d) >= startByte (%d) in widenBCDValue\n",endByte,startByte);

   int32_t bytesToClear = endByte - startByte;
   if (bytesToClear > 0)
      {
      switch (node->getDataType())
         {
         case TR::PackedDecimal:
            self()->genZeroLeftMostPackedDigits(node, reg, endByte, bytesToClear*2, targetMR);
            break;
         case TR::ZonedDecimal:
         case TR::ZonedDecimalSignTrailingSeparate:
            self()->genZeroLeftMostZonedBytes(node, reg, endByte, bytesToClear, targetMR);
            break;
         case TR::ZonedDecimalSignLeadingEmbedded:
            self()->widenZonedSignLeadingEmbedded(node, reg, endByte, bytesToClear, targetMR);
            break;
         case TR::ZonedDecimalSignLeadingSeparate:
            self()->widenZonedSignLeadingSeparate(node, reg, endByte, bytesToClear, targetMR);
            break;
         case TR::UnicodeDecimal:
         case TR::UnicodeDecimalSignTrailing:
            self()->genZeroLeftMostUnicodeBytes(node, reg, endByte, bytesToClear, targetMR);
            break;
         case TR::UnicodeDecimalSignLeading:
            self()->widenUnicodeSignLeadingSeparate(node, reg, endByte, bytesToClear, targetMR);
            break;
         default:
            TR_ASSERT(false,"unsupported dataType %s in widenBCDValue\n",node->getDataType().toString());
         }
      }
   }


/**
 * Input reg can be NULL (when called for a store node or other type that does not return a register)
 */
void
J9::Z::CodeGenerator::widenBCDValueIfNeeded(TR::Node *node, TR_PseudoRegister *reg, int32_t startByte, int32_t endByte, TR::MemoryReference *targetMR)
   {
   TR_ASSERT(node->getType().isBCD(),
      "widenBCDValue is only valid for BCD types (type=%s)\n",node->getDataType().toString());
   TR_ASSERT(targetMR->rightAlignMemRef() || targetMR->leftAlignMemRef(),"widenBCDValue is only valid for aligned memory references\n");
   TR_ASSERT(endByte >= startByte,"endByte (%d) >= startByte (%d) in widenBCDValue\n",endByte,startByte);

   int32_t bytesToClear = endByte - startByte;
   if (self()->traceBCDCodeGen())
      traceMsg(self()->comp(),"\twidenBCDValue for node %s (%p) : %d->%d (%d bytes)\n",node->getOpCode().getName(),node,startByte,endByte,bytesToClear);
   if (bytesToClear > 0)
      {
      if (reg && reg->trackZeroDigits())
         self()->clearByteRangeIfNeeded(node, reg, targetMR, startByte, endByte);
      else
         self()->widenBCDValue(node, reg, startByte, endByte, targetMR);
      }
   }

void
J9::Z::CodeGenerator::genZeroLeftMostDigitsIfNeeded(TR::Node *node, TR_PseudoRegister *reg, int32_t endByte, int32_t digitsToClear, TR::MemoryReference *targetMR, bool widenOnLeft)
   {
   TR_ASSERT(reg->trackZeroDigits(),"genZeroLeftMostDigitsIfNeeded only valid for types where trackZeroDigits=true (dt %s)\n",reg->getDataType().toString());
   TR_ASSERT(endByte > 0,"genZeroLeftMostDigitsIfNeeded: endByte %d should be > 0\n",endByte);
   TR_ASSERT(digitsToClear >= 0,"genZeroLeftMostDigitsIfNeeded: digitsToClear %d should be >= 0\n",digitsToClear);
   TR_ASSERT(reg->getDataType() == node->getDataType(),"reg dt (%s) should match node dt (%s) in genZeroLeftMostDigitsIfNeeded\n",reg->getDataType().toString(),node->getDataType().toString());

   if (digitsToClear <= 0)
      return;

   TR_StorageReference *storageReference = reg->getStorageReference();
   TR_ASSERT(storageReference,"storageReference should be non-null at this point\n");
   int32_t endDigit = TR::DataType::getBCDPrecisionFromSize(node->getDataType(), endByte);
   int32_t startDigit = endDigit-digitsToClear;
   // -1 is the sign code position and it can be cleared. The caller is responsible for generating code to set a new and valid sign code.
   if (self()->traceBCDCodeGen())
      traceMsg(self()->comp(),"\tgenZeroLeftMostDigitsIfNeeded %s #%d for node %p: digitsToClear = %d, endByte = %d (digit range is %d->%d), widenOnLeft=%s\n",
         self()->getDebug()->getName(reg),storageReference->getReferenceNumber(),node,digitsToClear,endByte,startDigit,endDigit,widenOnLeft?"yes":"no");
   TR_ASSERT(startDigit >= -1,"genZeroLeftMostDigitsIfNeeded: startDigit %d should be >= -1\n",startDigit);

   // If requested (widenOnLeft=true) then attempt to clear up to the live symbol size to save separate clears being needed later on
   // this would not be legal, for example, when this routine is called to clear an intermediate digit range only
   // where some left most digits have to be preserved -- such as in pdshlEvaluator (via clearAndSetSign) when the moved over sign code is cleared.
   int32_t actualDigitsToClear = reg->getDigitsToClear(startDigit, endDigit);
   int32_t origEndDigit = endDigit;
   // only respect widenOnLeft if the actualDigitsToClear exceeds the widenOnLeftThreshold
   int32_t widenOnLeftThreshold = 0;
   if (node->getType().isAnyPacked())
      {
      // for the half byte type do not increase a single digit clear (i.e. avoid NI -> XC/NI -- just do the NI and leave the XC until later if needed)
      widenOnLeftThreshold = 1;
      }
   else if (node->getType().isAnyZoned() || node->getType().isAnyUnicode())
      {
      // the full byte types use an MVC for the clear so always attempt to widen on the left
      widenOnLeftThreshold = 0;
      }
   else
      {
      TR_ASSERT(false,"unsupported datatype %s in genZeroLeftMostDigitsIfNeededA\n",node->getDataType().toString());
      }
   if (widenOnLeft &&
       actualDigitsToClear > widenOnLeftThreshold &&
       reg->getLiveSymbolSize() > endByte)
      {
      int32_t origEndByte = endByte;
      endByte = reg->getLiveSymbolSize();
      endDigit = TR::DataType::getBCDPrecisionFromSize(node->getDataType(), endByte);
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\ttargetMR->getStorageReference() #%d liveSymSize %d > endByte %d so increase endByte %d->%d (endDigit %d->%d) and retrieve the actualDigitsToClear based on this new endDigit\n",
            targetMR->getStorageReference()->getReferenceNumber(),reg->getLiveSymbolSize(),origEndByte,origEndByte,endByte,origEndDigit,endDigit);
      }

   if (origEndDigit != endDigit)
      actualDigitsToClear = reg->getDigitsToClear(startDigit, endDigit);

   if (actualDigitsToClear)
      {
      int32_t offset = reg->getByteOffsetFromLeftForClear(startDigit, endDigit, actualDigitsToClear, endByte);   // might modify actualDigitsToClear
      switch (node->getDataType())
         {
         case TR::PackedDecimal:
            self()->genZeroLeftMostPackedDigits(node,
                                        reg,
                                        endByte,
                                        actualDigitsToClear,
                                        targetMR,
                                        offset);
            break;
         case TR::ZonedDecimal:
            self()->genZeroLeftMostZonedBytes(node,
                                               reg,
                                               endByte-offset,
                                               actualDigitsToClear,
                                               targetMR);
            break;
         default:
            TR_ASSERT(false,"unsupported datatype %s in genZeroLeftMostDigitsIfNeededB\n",node->getDataType().toString());
            break;
         }
      }
   else
      {
      self()->processUnusedNodeDuringEvaluation(NULL);
      }
   }

void
J9::Z::CodeGenerator::clearByteRangeIfNeeded(TR::Node *node, TR_PseudoRegister *reg, TR::MemoryReference *targetMR, int32_t startByte, int32_t endByte, bool widenOnLeft)
   {
   TR_ASSERT(startByte <= endByte,"clearByteRangeIfNeeded: invalid range of %d->%d\n",startByte,endByte);
   if (startByte >= endByte) return;
   int32_t clearDigits = TR::DataType::bytesToDigits(node->getDataType(), endByte-startByte);
   return self()->genZeroLeftMostDigitsIfNeeded(node, reg, endByte, clearDigits, targetMR, widenOnLeft);
   }

void
J9::Z::CodeGenerator::genZeroLeftMostPackedDigits(TR::Node *node, TR_PseudoRegister *reg, int32_t endByte, int32_t digitsToClear, TR::MemoryReference *targetMR, int32_t memRefOffset)
   {
   TR_ASSERT(targetMR->rightAlignMemRef() || targetMR->leftAlignMemRef(),"genZeroLeftMostPackedDigits is only valid for aligned memory references\n");

   TR_StorageReference *storageRef = reg ? reg->getStorageReference() : NULL;
   targetMR = reuseS390LeftAlignedMemoryReference(targetMR, node, storageRef, self(), endByte);

   if (digitsToClear)
      {
      int32_t fullBytesToClear = digitsToClear/2;
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\tgenZeroLeftMostPackedDigits: node %p, reg %s targetMemSlot #%d, endByte %d, digitsToClear %d (fullBytesToClear %d), memRefOffset %d\n",
            node,reg?self()->getDebug()->getName(reg):"0",reg?reg->getStorageReference()->getReferenceNumber():0,endByte,digitsToClear,fullBytesToClear,memRefOffset);
      if (fullBytesToClear)
         {
         int32_t destOffset = 0;
         if (self()->traceBCDCodeGen())
            traceMsg(self()->comp(),"\t\tgen XC with size %d and mr offset %d (destOffset %d + memRefOffset %d)\n",fullBytesToClear,destOffset+memRefOffset,destOffset,memRefOffset);
         generateSS1Instruction(self(), TR::InstOpCode::XC, node,
                                fullBytesToClear-1,
                                generateS390LeftAlignedMemoryReference(*targetMR, node, destOffset+memRefOffset, self(), targetMR->getLeftMostByte()),     // left justified
                                generateS390LeftAlignedMemoryReference(*targetMR, node, destOffset+memRefOffset, self(), targetMR->getLeftMostByte()));    // left justified
         }
      if (digitsToClear&0x1)
         {
         int32_t destOffset = 0;
            {
            if (self()->traceBCDCodeGen())
               traceMsg(self()->comp(),"\tgen NI for odd clear digits with mr offset %d (fullBytesToClear %d + destOffset %d + memRefOffset %d)\n",fullBytesToClear+destOffset+memRefOffset,fullBytesToClear,destOffset,memRefOffset);
            generateSIInstruction(self(), TR::InstOpCode::NI, node,
                                  generateS390LeftAlignedMemoryReference(*targetMR, node, fullBytesToClear+destOffset+memRefOffset, self(), targetMR->getLeftMostByte()),
                                0x0F);
            }
         }
      int32_t endDigit = (endByte*2)-(memRefOffset*2)-1;  // -1 for the sign code
      if (reg)
         reg->addRangeOfZeroDigits(endDigit-digitsToClear, endDigit);
      }
   }


void
J9::Z::CodeGenerator::initializeStorageReference(TR::Node *node,
                                                 TR_OpaquePseudoRegister *destReg,
                                                 TR::MemoryReference *destMR,
                                                 int32_t destSize,
                                                 TR::Node *srcNode,
                                                 TR_OpaquePseudoRegister *srcReg,
                                                 TR::MemoryReference *sourceMR,
                                                 int32_t sourceSize,
                                                 bool performExplicitWidening,
                                                 bool alwaysLegalToCleanSign,
                                                 bool trackSignState)
   {
   TR::Compilation *comp = self()->comp();
   if (self()->traceBCDCodeGen())
      traceMsg(comp,"\tinitializeStorageReference for %s (%p), destReg %s, srcReg %s, sourceSize %d, destSize %d, performExplicitWidening=%s, trackSignState=%s\n",
         node->getOpCode().getName(),node,
         destReg ? self()->getDebug()->getName(destReg):"NULL",srcReg ? self()->getDebug()->getName(srcReg):"NULL",sourceSize,destSize,performExplicitWidening?"yes":"no",trackSignState?"yes":"no");

   TR_ASSERT( srcReg,"expecting a non-null srcReg in initializeStorageReference\n");
   TR_ASSERT( srcReg->getStorageReference(),"expecting a non-null srcReg->storageRef in initializeStorageReference\n");

   TR::CodeGenerator *cg = self();
   // if a non-null destReg does not have a memory slot set then the addRangeOfZeroBytes/addRangeOfZeroDigits calls will
   // not be able to query the symbol size
   TR_ASSERT( !destReg || destReg->getStorageReference(),"a non-null destReg must have a storageReference set\n");
   bool isBCD = node->getType().isBCD();
   TR_ASSERT(!isBCD || sourceSize <= TR_MAX_MVC_SIZE,"sourceSize %d > max %d for node %p\n",sourceSize,TR_MAX_MVC_SIZE,node);
   TR_ASSERT(!isBCD || destSize <= TR_MAX_MVC_SIZE,"destSize %d > max %d for node %p\n",destSize,TR_MAX_MVC_SIZE,node);
   TR_PseudoRegister *srcPseudoReg = srcReg->getPseudoRegister();
   TR_PseudoRegister *destPseudoReg = destReg ? destReg->getPseudoRegister() : NULL;

   // widening and truncations only supported for pseudoRegisters
   TR_ASSERT(srcPseudoReg || destSize == sourceSize,"destSize %d != sourceSize %d for opaquePseudoReg on node %p\n",destSize,sourceSize,node);
   TR_ASSERT(destPseudoReg == NULL || srcPseudoReg == NULL || (srcPseudoReg && destPseudoReg),"both src and dest must be pseudoRegisters for node %p\n",node);
   TR_ASSERT(!isBCD || srcPseudoReg,"srcPseudoReg should be set for BCD node %p\n",node);

   if (sourceMR == NULL)
      {
      sourceMR = isBCD ?
         generateS390RightAlignedMemoryReference(srcNode, srcReg->getStorageReference(), cg) :
         generateS390MemRefFromStorageRef(srcNode, srcReg->getStorageReference(), cg);
      }

   int32_t mvcSize = std::min(sourceSize, destSize);
   TR_StorageReference *dstStorageRef = destMR->getStorageReference();
   TR_StorageReference *srcStorageRef = sourceMR->getStorageReference();
   TR_ASSERT(dstStorageRef,"dstStorageRef should be non-NULL\n");
   TR_ASSERT(srcStorageRef,"srcStorageRef should be non-NULL\n");

   if (!self()->storageReferencesMatch(dstStorageRef, srcStorageRef))
      {
      int32_t bytesToClear = (destSize > sourceSize) ? srcReg->getBytesToClear(sourceSize, destSize) : 0;
      bool srcCastedToBCD = srcReg->getStorageReference()->isNodeBased() && srcReg->getStorageReference()->getNode()->castedToBCD();

      if (self()->traceBCDCodeGen())
         traceMsg(comp,"\t\tnode %p : srcReg %s (hasBadSign %s) on srcNode %p has bytes %d->%d %salready clear (bytesToClear=%d), srcCastedToBCD=%d\n",
            node,self()->getDebug()->getName(srcReg),srcPseudoReg ? (srcPseudoReg->hasKnownBadSignCode()?"yes":"no") : "no",
            srcNode,sourceSize,destSize,bytesToClear==0?"":"not ",bytesToClear,srcCastedToBCD);

      if (destSize > sourceSize &&
          bytesToClear == 0)
         {
         mvcSize = destSize;
         if (destReg)
            destReg->addRangeOfZeroBytes(sourceSize,destSize);
         if (self()->traceBCDCodeGen())
            traceMsg(comp,"\tincrease mvcSize %d->%d to account for already cleared %d bytes\n",sourceSize,mvcSize,bytesToClear);
         }

      if (self()->traceBCDCodeGen())
         traceMsg(comp,"\t\tgen MVC/memcpy to initialize storage reference with size = %d\n",mvcSize);

      TR::MemoryReference *initDstMR = NULL;
      TR::MemoryReference *initSrcMR = NULL;
      if (isBCD)
         {
         initDstMR = generateS390RightAlignedMemoryReference(*destMR, node, 0, cg);
         initSrcMR = generateS390RightAlignedMemoryReference(*sourceMR, srcNode, 0, cg);
         }
      else
         {
         initDstMR = generateS390MemoryReference(*destMR, 0, cg);
         initSrcMR = generateS390MemoryReference(*sourceMR, 0, cg);
         }
      self()->genMemCpy(initDstMR, node, initSrcMR, srcNode, mvcSize);
      }

   if (isBCD && performExplicitWidening && (destSize > sourceSize))
      self()->widenBCDValueIfNeeded(node, destPseudoReg, sourceSize, destSize, destMR);

   if (destPseudoReg)
      {
      TR_ASSERT(srcPseudoReg,"srcPseudoReg must be non-NULL if destPseudoReg is non-NULL on node %p\n",node);
      // the destReg can be refined further by the caller but for now set it to a conservative value
      int32_t targetPrecision = 0;
      if (destSize >= sourceSize)
         targetPrecision  = srcPseudoReg->getDecimalPrecision();
      else
         targetPrecision = TR::DataType::getBCDPrecisionFromSize(node->getDataType(), destSize);
      destPseudoReg->setDecimalPrecision(targetPrecision);
      if (self()->traceBCDCodeGen())
         traceMsg(comp,"\tset destReg targetPrecision to %d (from %s for node dt %s)\n",
            targetPrecision,destSize >= sourceSize?"srcReg precision":"destSize",node->getDataType().toString());
      }
   if (destReg)
      destReg->setIsInitialized();
   }

TR_StorageReference *
J9::Z::CodeGenerator::initializeNewTemporaryStorageReference(TR::Node *node,
                                                             TR_OpaquePseudoRegister *destReg,
                                                             int32_t destSize,
                                                             TR::Node *srcNode,
                                                             TR_OpaquePseudoRegister *srcReg,
                                                             int32_t sourceSize,
                                                             TR::MemoryReference *sourceMR,
                                                             bool performExplicitWidening,
                                                             bool alwaysLegalToCleanSign,
                                                             bool trackSignState)
   {
   if (self()->traceBCDCodeGen())
      traceMsg(self()->comp(),"\tinitializeNewTemporaryStorageReference for node %p, destReg %s, srcNode %p, srcReg %s (with size %d), sourceSize %d, destSize %d\n",
         node,destReg ? self()->getDebug()->getName(destReg):"NULL",srcNode,srcReg ? self()->getDebug()->getName(srcReg):"NULL",srcReg?srcReg->getSize():0,sourceSize,destSize);

   TR_StorageReference *tempStorageReference = TR_StorageReference::createTemporaryBasedStorageReference(destSize, self()->comp());
   if (destReg)
      destReg->setStorageReference(tempStorageReference, node);
   else
      tempStorageReference->setTemporaryReferenceCount(1);

   TR_ASSERT(srcReg,"expecting a non-null srcReg in initializeNewTemporaryStorageReference for srcNode %p\n",srcNode);

   TR::MemoryReference *destMR = NULL;
   if (srcReg->getPseudoRegister())
      destMR = generateS390RightAlignedMemoryReference(node, tempStorageReference, self(), true, true); // enforceSSLimits=true, isNewTemp=true
   else
      destMR = generateS390MemRefFromStorageRef(node, tempStorageReference, self());

   self()->initializeStorageReference(node,
                              destReg,
                              destMR,
                              destSize,
                              srcNode,
                              srcReg,
                              sourceMR,
                              sourceSize,
                              performExplicitWidening,
                              alwaysLegalToCleanSign,
                              trackSignState);
   if (destReg == NULL)
      tempStorageReference->setTemporaryReferenceCount(0);
   return tempStorageReference;
   }

TR_OpaquePseudoRegister *
J9::Z::CodeGenerator::privatizePseudoRegister(TR::Node *node, TR_OpaquePseudoRegister *reg, TR_StorageReference *storageRef, size_t sizeOverride)
   {
   TR::CodeGenerator *cg = self();
   TR::Compilation *comp = cg->comp();
   size_t regSize = reg->getSize();
   if (self()->traceBCDCodeGen())
      {
      if (sizeOverride != 0 && sizeOverride != regSize)
         traceMsg(comp,"\tsizeOverride=%d : use this as the size for privatizing reg %s (instead of regSize %d)\n",sizeOverride,cg->getDebug()->getName(reg),reg->getSize());
      else
         traceMsg(comp,"\tsizeOverride=0 : use reg %s regSize %d as the size for privatizing\n",cg->getDebug()->getName(reg),reg->getSize());
      }
   size_t size = sizeOverride == 0 ? regSize : sizeOverride;
   bool isBCD = node->getType().isBCD();
   TR_StorageReference *tempStorageReference = TR_StorageReference::createTemporaryBasedStorageReference(size, comp);
   tempStorageReference->setIsSingleUseTemporary();
   TR::MemoryReference *origSrcMR = NULL;
   TR::MemoryReference *copyMR = NULL;
   if (isBCD)
      {
      origSrcMR = generateS390RightAlignedMemoryReference(node, storageRef, cg);
      copyMR = generateS390RightAlignedMemoryReference(node, tempStorageReference, cg, true, true); // enforceSSLimits=true, isNewTemp=true
      }
   else
      {
      origSrcMR = generateS390MemRefFromStorageRef(node, storageRef, cg);
      copyMR = generateS390MemRefFromStorageRef(node, tempStorageReference, cg); // enforceSSLimits=true
      }

   if (self()->traceBCDCodeGen())
      traceMsg(comp,"\ta^a : gen memcpy of size = %d to privatize node %s (%p) with storageRef #%d (%s) to #%d (%s) on line_no=%d\n",
         size,node->getOpCode().getName(),node,
         storageRef->getReferenceNumber(),self()->getDebug()->getName(storageRef->getSymbol()),
         tempStorageReference->getReferenceNumber(),self()->getDebug()->getName(tempStorageReference->getSymbol()),
         comp->getLineNumber(node));

   // allocate a new register so any storageRef dep state (like leftAlignedZeroDigits) is cleared (as the mempcy isn't going transfer these over to copyMR)
   TR_OpaquePseudoRegister *tempRegister = isBCD ? cg->allocatePseudoRegister(reg->getPseudoRegister()) : cg->allocateOpaquePseudoRegister(reg);
   tempRegister->setStorageReference(tempStorageReference, NULL); // node==NULL as the temp refCounts are explicitly being managed as the temp will only live for this evaluator
   tempRegister->setIsInitialized();

   cg->genMemCpy(copyMR, node, origSrcMR, node, size);

   return tempRegister;
   }

TR_OpaquePseudoRegister*
J9::Z::CodeGenerator::privatizePseudoRegisterIfNeeded(TR::Node *parent, TR::Node *child, TR_OpaquePseudoRegister *childReg)
   {
   TR::Compilation *comp = self()->comp();
   TR_OpaquePseudoRegister *outReg = childReg;
   TR_StorageReference *hint = parent->getStorageReferenceHint();
   if (hint && hint->isNodeBased())
      {
      TR::Node *hintNode = hint->getNode();
      TR_StorageReference *childStorageRef = childReg->getStorageReference();
      if (self()->traceBCDCodeGen())
         traceMsg(comp,"\tprivatizePseudoRegisterIfNeeded for %s (%p) with hint %s (%p) (isInMemoryCopyProp=%s) and child %s (%p) (child storageRef isNonConstNodeBased=%s)\n",
            parent->getOpCode().getName(),parent,
            hintNode->getOpCode().getName(),hintNode,hintNode->isInMemoryCopyProp()?"yes":"no",
            child->getOpCode().getName(),child,
            childStorageRef ? (childStorageRef->isNonConstantNodeBased() ? "yes":"no") : "null");
      if (childStorageRef &&
          childStorageRef->isNonConstantNodeBased() &&
          hintNode->getOpCode().hasSymbolReference())
         {
         TR::Node *childStorageRefNode = childStorageRef->getNode();
         // see comment in pdstoreEvaluator for isUsingStorageRefFromAnotherStore and childRegHasDeadOrIgnoredBytes
         bool isUsingStorageRefFromAnotherStore = childStorageRefNode->getOpCode().isStore() && childStorageRefNode != hintNode;
         bool childRegHasDeadOrIgnoredBytes = childReg->getRightAlignedIgnoredBytes() > 0;
         if (self()->traceBCDCodeGen())
            traceMsg(comp,"\tisInMemoryCopyProp=%s, isUsingStorageRefFromAnotherStore=%s, childRegHasDeadOrIgnoredBytes=%s : childStorageRef %s (%p), hintNode %s (%p)\n",
               hintNode->isInMemoryCopyProp() ? "yes":"no",
               isUsingStorageRefFromAnotherStore ? "yes":"no",
               childRegHasDeadOrIgnoredBytes ? "yes":"no",
               childStorageRefNode->getOpCode().getName(),childStorageRefNode,
               hintNode->getOpCode().getName(),hintNode);
         if (hintNode->isInMemoryCopyProp() || isUsingStorageRefFromAnotherStore || childRegHasDeadOrIgnoredBytes)
            {
            bool useAliasing = true;
            if (self()->traceBCDCodeGen())
               traceMsg(comp,"\tcheck overlap between store hint %s (%p) and childStorageRefNode %s (%p)\n",
                  hintNode->getOpCode().getName(),hintNode,childStorageRefNode->getOpCode().getName(),childStorageRefNode);
            if (self()->loadAndStoreMayOverlap(hintNode,
                                       hintNode->getSize(),
                                       childStorageRefNode,
                                       childStorageRefNode->getSize()))
               {
               bool needsPrivitization = true;
               if (self()->traceBCDCodeGen())
                  traceMsg(comp,"\toverlap=true (from %s test) -- privatize the source memref to a temp memref\n",useAliasing?"aliasing":"pattern");
               if (useAliasing && // checking useAliasing here because in the no info case the above loadAndStoreMayOverlap already did the pattern match
                   self()->storageMayOverlap(hintNode, hintNode->getSize(), childStorageRefNode, childStorageRefNode->getSize()) == TR_NoOverlap)
                  {
                  // get a second opinion -- the aliasing says the operations overlap but perhaps it is too conservative
                  // so do pattern matching based test to see if the operations are actually disjoint
                  if (self()->traceBCDCodeGen())
                     traceMsg(comp,"\t\t but overlap=false (from 2nd opinion pattern test) -- set needsPrivitization to false\n");
                  needsPrivitization = false;
                  }

               if (needsPrivitization)
                  {
                  if (self()->traceBCDCodeGen())
                     {
                     if (hintNode->isInMemoryCopyProp())
                        traceMsg(comp,"\ta^a : privatize needed due to isInMemoryCopyProp hintNode %s (%p) on line_no=%d (privatizeCase)\n",
                           hintNode->getOpCode().getName(),hintNode,comp->getLineNumber(hintNode));
                     if (isUsingStorageRefFromAnotherStore)
                        traceMsg(comp,"\ta^a : privatize needed due to isUsingStorageRefFromAnotherStore childStorageRefNode %s (%p) on line_no=%d (privatizeCase)\n",
                           childStorageRefNode->getOpCode().getName(),childStorageRefNode,comp->getLineNumber(hintNode));
                     if (childRegHasDeadOrIgnoredBytes)
                        traceMsg(comp,"\ta^a : privatize needed due to childRegHasDeadOrIgnoredBytes valueReg %s child %s (%p) on line_no=%d (privatizeCase)\n",
                           self()->getDebug()->getName(childReg),child->getOpCode().getName(),child,comp->getLineNumber(hintNode));
                     }

                  outReg = self()->privatizePseudoRegister(child, childReg, childStorageRef);
                  TR_ASSERT(!comp->getOption(TR_EnablePerfAsserts),"gen overlap copy for hintNode %s (%p) on line_no=%d (privatePseudoCase)\n",
                     hintNode->getOpCode().getName(),hintNode,comp->getLineNumber(hintNode));
                  }
               }
            else
               {
               if (self()->traceBCDCodeGen())
                  traceMsg(comp,"\toverlap=false (from %s test) -- do not privatize the source memref\n",useAliasing?"aliasing":"pattern");
               }
            }
         else
            {
            if (self()->traceBCDCodeGen())
               traceMsg(comp,"y^y : temp copy saved isInMemoryCopyProp = false on %s (%p) (privatizeCase)\n",hintNode->getOpCode().getName(),hintNode);
            }
         }
      }
   return outReg;
   }

TR_PseudoRegister*
J9::Z::CodeGenerator::privatizeBCDRegisterIfNeeded(TR::Node *parent, TR::Node *child, TR_OpaquePseudoRegister *childReg)
   {
   TR_OpaquePseudoRegister *reg = self()->privatizePseudoRegisterIfNeeded(parent, child, childReg);
   TR_PseudoRegister *pseudoReg = reg->getPseudoRegister();
   TR_ASSERT(pseudoReg,"pseudoReg should not be NULL after privatizing of child %p\n",child);
   return pseudoReg;
   }

TR_StorageReference *
J9::Z::CodeGenerator::privatizeStorageReference(TR::Node *node, TR_OpaquePseudoRegister *reg, TR::MemoryReference *memRef)
   {
   TR::Compilation *comp = self()->comp();

   // Copy a node-based storageReference with a refCount > 1 to a temporary as the underlying symRef may be killed before the next commoned reference
   // to the node.
   // The flag skipCopyOnLoad is set in lowerTrees to prevent unnecessary copies when the symRef is known not to be killed for any commoned reference.
   TR_StorageReference *storageRef = reg->getStorageReference();
   TR_StorageReference *tempStorageRef = NULL;
   bool isPassThruCase = node != storageRef->getNode();
   if (self()->traceBCDCodeGen())
      traceMsg(comp,"privatizeStorageReference: %s (%p) refCount %d :: storageRef #%d, storageRefNode %s (%p) nodeRefCount %d, isNodeBased %s\n",
         node->getOpCode().getName(),
         node,
         node->getReferenceCount(),
         storageRef->getReferenceNumber(),
         storageRef->getNode()?storageRef->getNode()->getOpCode().getName():"NULL",
         storageRef->getNode(),
         storageRef->isNodeBased()?storageRef->getNodeReferenceCount():-99,
         storageRef->isNodeBased()?"yes":"no");

   bool force = comp->getOption(TR_ForceBCDInit) && node->getOpCode().isBCDLoad();
   if (force ||
       (storageRef->isNodeBased() &&
       node->getReferenceCount() > 1 &&
       !node->skipCopyOnLoad()))
      {
      if (self()->traceBCDCodeGen())
         {
         traceMsg(comp,"\tnode %p (%s) with skipCopyOnLoad=false does need to be privatized for node based storageRef node %p (%s-based) (force=%s)\n",
            node,node->getOpCode().getName(),storageRef->getNode(),storageRef->getNode()->getOpCode().isStore()?"store":"load",force?"yes":"no");
         traceMsg(comp,"\tb^b : gen memcpy of size = %d to privatizeStorageReference node %s (%p) with storageRef #%d (%s) on line_no=%d\n",
            reg->getSize(),node->getOpCode().getName(),node,
            storageRef->getReferenceNumber(),self()->getDebug()->getName(storageRef->getSymbol()),
            comp->getLineNumber(node));
         }

      if (force && storageRef->getNodeReferenceCount() == 1)
         storageRef->incrementNodeReferenceCount(); // prevent nodeRefCount underflow (dec'd for init and on setStorageRef call)

      if (memRef == NULL)
         {
         if (reg->getPseudoRegister())
            memRef = generateS390RightAlignedMemoryReference(node, storageRef, self());
         else
            memRef = generateS390MemRefFromStorageRef(node, storageRef, self());
         }

      if (reg->getSize() == 0)
         {
         TR_ASSERT(false,"register should have its size initialized before calling privatizeStorageReference\n");

         if (reg->getPseudoRegister())
            reg->getPseudoRegister()->setDecimalPrecision(node->getDecimalPrecision());
         else
            reg->setSize(node->getSize());
         }
      tempStorageRef = self()->initializeNewTemporaryStorageReference(node, reg, reg->getSize(), node, reg, reg->getSize(), memRef, false, false, false); // performExplicitWidening=false, alwaysLegalToCleanSign=false, trackSignState=false
      }
   else if (self()->traceBCDCodeGen())
      {
      traceMsg(comp,"\t%s (%p) does NOT need to be privatised because isTemp (%s) and/or refCount %d <= 1 and/or skipCopyOnLoad=true (flag is %s)\n",
         node->getOpCode().getName(),node,storageRef->isTemporaryBased()?"yes":"no",node->getReferenceCount(),node->skipCopyOnLoad()?"true":"false");
      }
   return tempStorageRef;
   }

/**
 * A binary coded decimal value may have had its storageReference size reduced
 * (by a pdshr for example) and/or have implied left most zeroes.  This routine
 * will ensure the storageReference is at least resultSize and zero digits are
 * explicitly generated up and including clearSize.  This full materialization
 * is required in several cases such as before calls or when used in an
 * instruction that requires a fixed size temp (like UNPKU in pd2ud or
 * CVB/CVBG)
 */
TR::MemoryReference *
J9::Z::CodeGenerator::materializeFullBCDValue(TR::Node *node,
                                              TR_PseudoRegister *&reg,
                                              int32_t resultSize,
                                              int32_t clearSize,
                                              bool updateStorageReference,
                                              bool alwaysEnforceSSLimits)
   {
   TR::Compilation *comp = self()->comp();

   int32_t regSize = reg->getSize();
   if (self()->traceBCDCodeGen())
      traceMsg(comp,"\tmaterializeFullBCDValue evaluated %s (%p) (nodeSize %d, requested resultSize %d) to reg %s (regSize %d), clearSize=%d, updateStorageReference=%s\n",
         node->getOpCode().getName(),node,node->getStorageReferenceSize(),resultSize,self()->getDebug()->getName(reg),regSize,clearSize,updateStorageReference?"yes":"no");

   TR_ASSERT(clearSize >= 0,"invalid clearSize %d for node %p\n",clearSize,node);
   if (clearSize == 0)
      {
      clearSize = resultSize;
      if (self()->traceBCDCodeGen())
         traceMsg(comp,"\tspecific clearSize not requested : set clearSize=resultSize=%d\n",resultSize);
      }
   else
      {
      // enforce this condition : regSize <= clearSize <= resultSize
      TR_ASSERT(clearSize <= resultSize,"clearSize %d should be <= resultSize %d on node %p\n",clearSize,resultSize,node);
      if (self()->traceBCDCodeGen())
         traceMsg(comp,"\tupdate clearSize %d to max(clearSize, regSize) = max(%d,%d) = %d\n",clearSize,clearSize,regSize,std::max(clearSize, regSize));
      clearSize = std::max(clearSize, regSize);
      }

   TR::MemoryReference *memRef = NULL;
   if (regSize < resultSize &&
       reg->getLiveSymbolSize() >= resultSize &&
       reg->getBytesToClear(regSize, clearSize) == 0)
      {
      if (self()->traceBCDCodeGen())
         traceMsg(comp,"\tbytes regSize->clearSize (%d->%d) are already clear -- no work to do to materializeFullBCDValue\n",regSize,clearSize);
      memRef = reuseS390RightAlignedMemoryReference(memRef, node, reg->getStorageReference(), self(), alwaysEnforceSSLimits);
      }
   else if (regSize < resultSize)
      {
      if (self()->traceBCDCodeGen())
         traceMsg(comp,"\treg->getSize() < resultSize (%d < %d) so check liveSymSize on reg\n",regSize,resultSize);
      int32_t liveSymSize = reg->getLiveSymbolSize();
      int32_t bytesToClear = clearSize-regSize;
      bool enforceSSLimitsForClear = alwaysEnforceSSLimits || bytesToClear > 1;

      if (reg->isInitialized() &&
          reg->getStorageReference()->isReadOnlyTemporary() &&
          liveSymSize > regSize &&
          reg->getBytesToClear(regSize, clearSize) > 0)
         {
         // 1  pd2i
         // 1     pdModPrec p=3,s=2 <- (node) passThrough + initialized (setAsReadOnly due to lazy clobber evaluate)
         // 2        pdX p=8,s=5    <- initialized and refCount > 1 (used again)
         //
         // Have to clobber evaluate in this case so the clearing of firstRegSize (2) to sourceSize (8) does not destroy
         // the 6 upper bytes required by the commoned reference to pdX
         memRef = reuseS390RightAlignedMemoryReference(memRef, node, reg->getStorageReference(), self(), enforceSSLimitsForClear);
         TR_OpaquePseudoRegister *opaqueReg = self()->ssrClobberEvaluate(node, memRef);
         reg = opaqueReg->getPseudoRegister();
         TR_ASSERT(reg,"reg should be set for node %p\n",node);
         }

      if (reg->isInitialized() && reg->trackZeroDigits() && liveSymSize >= resultSize)
         {
         if (self()->traceBCDCodeGen())
            traceMsg(comp,"\treg->getLiveSymbolSize() >= resultSize (%d >= %d) so call clearByteRangeIfNeeded\n",liveSymSize,resultSize);
         memRef = reuseS390RightAlignedMemoryReference(memRef, node, reg->getStorageReference(), self(), enforceSSLimitsForClear);
         self()->clearByteRangeIfNeeded(node, reg, memRef, regSize, clearSize);
         }
      else if (reg->isInitialized() && reg->trackZeroDigits() && reg->getStorageReference()->isTemporaryBased())
         {
         if (self()->traceBCDCodeGen())
            traceMsg(comp,"\treg->getLiveSymbolSize() < resultSize (%d < %d) so call increaseTemporarySymbolSize but first check for already cleared bytes\n",liveSymSize,resultSize);
         //int32_t bytesToClear = clearSize-regSize;                      // e.g. clearSize=16, regSize=3 so bytesToClear=13, liveSymSize=15
         int32_t alreadyClearedBytes = 0;
         int32_t endByteForClearCheck = 0;
         if (clearSize > liveSymSize)                                   // 16 > 15
            endByteForClearCheck = liveSymSize;                         // endByteForClearCheck = 15
         else
            endByteForClearCheck = clearSize;

         if (reg->getBytesToClear(regSize, endByteForClearCheck) == 0)  // increaseTemporarySymbolSize resets leftAlignedZeroDigits so check cleared bytes first
            alreadyClearedBytes = endByteForClearCheck-regSize;         // endByteForClearCheck=15,regSize=3 so alreadyClearedBytes=12

         if (self()->traceBCDCodeGen())
            traceMsg(comp,"\tfound %d alreadyClearedBytes : adjust bytesToClear %d -> %d\n",alreadyClearedBytes,bytesToClear,bytesToClear-alreadyClearedBytes);
         bytesToClear-=alreadyClearedBytes;                             // bytesToClear = bytesToClear-alreadyClearedBytes = 13-12 = 1
         if (bytesToClear < 0)
            {
            TR_ASSERT(false,"bytesToClear should always be >=0 and not %d\n",bytesToClear);
            bytesToClear = clearSize-regSize;
            }
         int32_t savedLeftAlignedZeroDigits = reg->getLeftAlignedZeroDigits();
         reg->increaseTemporarySymbolSize(resultSize - liveSymSize); // also resets leftAlignedZeroDigits

         // create memRef after temp size increase so correct TotalSizeForAlignment is set
         memRef = reuseS390RightAlignedMemoryReference(memRef, node, reg->getStorageReference(), self(), enforceSSLimitsForClear);
         int32_t startByte = clearSize-bytesToClear;
         int32_t endByte = clearSize;
         self()->widenBCDValue(node, reg, startByte, endByte, memRef);
         if (clearSize == resultSize)
            {
            // bytesToClear may have been reduced to less than resultSize-regSize if the source already had some cleared bytes
            // in this case the already cleared bytes should also be transferred to the size increased temporary
            int32_t newLeftAlignedZeroDigits = TR::DataType::bytesToDigits(reg->getDataType(), resultSize-regSize); // (16-3)*2 = 26
            if (TR::DataType::getDigitSize(reg->getDataType()) == HalfByteDigit && reg->isEvenPrecision() && reg->isLeftMostNibbleClear())
               newLeftAlignedZeroDigits++;
            reg->setLeftAlignedZeroDigits(newLeftAlignedZeroDigits);
            if (self()->traceBCDCodeGen())
               traceMsg(comp,"\tset leftAlignedZeroDigits to %d after temporarySymbolSize increase\n",newLeftAlignedZeroDigits);
            }
         else // if not clearing all the new bytes than the zero digits will not be left aligned
            {
            // TODO: when actual zero ranges are tracked can transfer the range on the reg from before the increaseTemporarySymbolSize
            // to now in the clearSize < resultSize case
            if (self()->traceBCDCodeGen() && savedLeftAlignedZeroDigits > 0)
               traceMsg(comp,"x^x : missed transferring savedLeftAlignedZeroDigits %d on matFull, node %p\n",savedLeftAlignedZeroDigits,node);
            }
         }
      else
         {
         if (self()->traceBCDCodeGen())
            traceMsg(comp,"\tstorageReference #%d is not tempBased (or is not packed) and reg->getLiveSymbolSize() < resultSize (%d < %d) so alloc a new temporary reference\n",
               reg->getStorageReference()->getReferenceNumber(),liveSymSize,resultSize);
         memRef = reuseS390RightAlignedMemoryReference(memRef, node, reg->getStorageReference(), self(), enforceSSLimitsForClear);
         TR_PseudoRegister *destReg = NULL;
         if (updateStorageReference)
            destReg = reg;
         bool clearWidenedBytes = clearSize == resultSize;
         TR_StorageReference *tempStorageRef = self()->initializeNewTemporaryStorageReference(node,
                                                                                      destReg,
                                                                                      resultSize,
                                                                                      node,
                                                                                      reg,
                                                                                      reg->getSize(),
                                                                                      memRef,
                                                                                      clearWidenedBytes, // performExplicitWidening
                                                                                      false,             // alwaysLegalToCleanSign
                                                                                      false);            // trackSignState=false
         if (destReg == NULL)
            tempStorageRef->setTemporaryReferenceCount(1);

         // pass in isNewTemp=true for the memref gen below so any deadBytes on the node's register are *not* counted for this new temporary
         // (these deadBytes should only be counted for the source memRef created just above)
         memRef = generateS390RightAlignedMemoryReference(node, tempStorageRef, self(), true, true); // enforceSSLimits=true, isNewTemp=true

         if (!clearWidenedBytes && clearSize > regSize)
            self()->widenBCDValue(node, destReg, regSize, clearSize, memRef);

         if (destReg == NULL)
            tempStorageRef->setTemporaryReferenceCount(0);
         self()->pendingFreeVariableSizeSymRef(tempStorageRef->getTemporarySymbolReference()); // free after this treetop has been evaluated if the refCount is still 0 at that point
         }
      }
   memRef = reuseS390RightAlignedMemoryReference(memRef, node, reg->getStorageReference(), self(), alwaysEnforceSSLimits);
   return memRef;
   }

bool topBitIsZero(uint8_t byte)
   {
   return (byte & 0x80) == 0;
   }

bool topBitIsOne(uint8_t byte)
   {
   return (byte & 0x80) == 0x80;
   }

#define TR_TWO_BYTE_TABLE_SIZE 17
static uint8_t zeroTable[TR_TWO_BYTE_TABLE_SIZE] =
   {
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0
   };

static uint8_t oneTable[TR_TWO_BYTE_TABLE_SIZE] =
   {
   0xFF,
   0xFF,
   0xFF,
   0xFF,
   0xFF,
   0xFF,
   0xFF,
   0xFF,
   0xFF,
   0xFF,
   0xFF,
   0xFF,
   0xFF,
   0xFF,
   0xFF,
   0xFF,
   0xFF
   };

TR::MemoryReference *getNextMR(TR::MemoryReference *baseMR, TR::Node *node, intptr_t offset, size_t destLeftMostByte, bool isBCD, TR::CodeGenerator *cg)
   {
   if (isBCD)
      return generateS390LeftAlignedMemoryReference(*baseMR, node, offset, cg, destLeftMostByte);
   else
      return generateS390MemoryReference(*baseMR, offset, cg);
   }

bool checkMVHI(char *lit, int32_t offset)
   {
   if (memcmp(lit+offset,zeroTable,2) == 0 && topBitIsZero(lit[offset+2]))    // zero extend 0x7FFF to lit value of 0x00007FFF
      return true;
   else if (memcmp(lit+offset,oneTable,2) == 0 && topBitIsOne(lit[offset+2])) // sign extend 0xFFF to lit value of 0xffffFFFF
      return true;
   else
      return false;
   }

bool checkMVGHI(char *lit, int32_t offset)
   {
   if (memcmp(lit+offset,zeroTable,6) == 0 && topBitIsZero(lit[offset+6]))    // zero extend 0x7FFF to lit value of 0x00000000 00007FFF
      return true;
   else if (memcmp(lit+offset,oneTable,6) == 0 && topBitIsOne(lit[offset+6])) // sign extend 0xFFFF to lit value of 0xffffFFFF ffffFFFF
      return true;
   else
      return false;
   }

void genMVI(TR::MemoryReference *destMR, TR::Node *node, uint8_t value, TR::CodeGenerator *cg)
   {
   if (cg->traceBCDCodeGen())
      traceMsg(cg->comp(),"\tgen MVI 0x%02x\n",value);
   generateSIInstruction(cg, TR::InstOpCode::MVI, node, destMR, value);
   }

void genMVHHI(TR::MemoryReference *destMR, TR::Node *node, int16_t value, TR::CodeGenerator *cg)
   {
   if (cg->traceBCDCodeGen())
      traceMsg(cg->comp(),"\tgen MVHHI 0x%04x\n",(uint16_t)value);
   generateSILInstruction(cg, TR::InstOpCode::MVHHI, node, destMR, value);
   }

void genMVHI(TR::MemoryReference *destMR, TR::Node *node, int16_t value, TR::CodeGenerator *cg)
   {
   if (cg->traceBCDCodeGen())
      traceMsg(cg->comp(),"\tgen MVHI 0x%04x\n",(uint16_t)value);
   generateSILInstruction(cg, TR::InstOpCode::MVHI, node, destMR, value);
   }

void genMVGHI(TR::MemoryReference *destMR, TR::Node *node, int16_t value, TR::CodeGenerator *cg)
   {
   if (cg->traceBCDCodeGen())
      traceMsg(cg->comp(),"\tgen MVGHI 0x%04x\n",(uint16_t)value);
   generateSILInstruction(cg, TR::InstOpCode::MVGHI, node, destMR, value);
   }



/**
 * This method must be kept in sync with cases handled by useMoveImmediateCommon below
 */
bool
J9::Z::CodeGenerator::canCopyWithOneOrTwoInstrs(char *lit, size_t size)
   {
   if (size < 1 || size >= TR_TWO_BYTE_TABLE_SIZE)
      {
      return false;
      }

   bool canCopy = false;
   switch (size)
      {
      case 0:
         canCopy = false;
         break;
      case 1:  // MVI
      case 2:  // MVI/MVI or MVHHI
      case 3:  // MVHHI/MVI
         canCopy = true;
         break;
      case 4:  // MVHHI/MVHHI (always) or MVHI (value <= 0x7FFF)
         canCopy = true;
         break;
      case 5:  // MVHI/MVI (MVHI 0,1,2,3 bytes value <= 0x7FFF) or MVI/MVHI (MVHI 1,2,3,4 bytes value <= 0x7FFF)
         if (checkMVHI(lit,0) || checkMVHI(lit,1))
            canCopy = true;
         break;
      case 6:  // MVHI/MVHHI (MVHI 0,1,2,3 bytes value <= 0x7FFF)  or MVHHI/MVHI (MVHI 2,3,4,5 bytes value <= 0x7FFF)
         if (checkMVHI(lit,0) || checkMVHI(lit,2))
            canCopy = true;
         break;
      case 7:
         canCopy = false;
         break;
      case 8:  // MVGHI (value <= 0x7FFF) or MVHI/MVHI (e.g. 0x00007FFF FFFFffff or vice-versa)
         if (checkMVGHI(lit,0))
            canCopy = true;
         else if (checkMVHI(lit,0) && checkMVHI(lit,4))
            canCopy = true;
         break;
      case 9:  // MVGHI/MVI (MVGHI <= 0x7FFF)
      case 10: // MVGHI/MVHHI (MVGHI <= 0x7FFF)
         if (checkMVGHI(lit,0))
            canCopy = true;
         break;
      case 11:
         canCopy = false;
         break;
      case 12: // MVGHI/MVHI  (MVGHI and MVHI value both <= 0x7FFF)
         if (checkMVGHI(lit,0) && checkMVHI(lit,8))
            {
            canCopy = true;
            }
         break;
      case 13:
      case 14:
      case 15:
         canCopy=false;
         break;
      case 16: // MVGHI/MVGHI (both MVGHI values <= 0x7FFF)
         if (checkMVGHI(lit,0) && checkMVGHI(lit,8))
            {
            canCopy = true;
            }
         break;
      default:
         canCopy = false;
         break;
      }
   return canCopy;
   }


/**
 * This method must be kept in sync with cases handled by canCopyWithOneOrTwoInstrs above
 */
bool
J9::Z::CodeGenerator::useMoveImmediateCommon(TR::Node *node,
                                             char *srcLiteral,
                                             size_t srcSize,
                                             TR::Node *srcNode,
                                             size_t destSize,
                                             intptr_t destBaseOffset,
                                             size_t destLeftMostByte,
                                             TR::MemoryReference *inputDestMR)
   {
   TR::CodeGenerator *cg = self();
   size_t size = destSize;
   char *lit = srcLiteral;
   bool isBCD = node->getType().isBCD();

   TR::MemoryReference *destMR = getNextMR(inputDestMR, node, destBaseOffset, destLeftMostByte, isBCD, cg);

   switch (size)
      {
      case 0:
         TR_ASSERT(false,"copySize %d not supported on node %p\n",size,node);
         break;
      case 1:  // MVI
         genMVI(destMR, node, lit[0], cg);
         break;
      case 2:  // MVI/MVI or MVHHI
         {
         genMVHHI(destMR, node, (lit[0]<<8)|lit[1], cg);
         break;
         }
      case 3:  // MVHHI/MVI
         genMVHHI(destMR, node, (lit[0]<<8)|lit[1], cg);
         genMVI(getNextMR(destMR, node, 2, destLeftMostByte, isBCD, cg), node, lit[2], cg);
         break;
      case 4:  // MVHHI/MVHHI (always) or MVHI (value <= 0x7FFF)
         if (checkMVHI(lit,0))
            {
            genMVHI(destMR, node, (lit[2]<<8)|lit[3], cg);
            }
         else
            {
            genMVHHI(destMR, node, (lit[0]<<8)|lit[1], cg);
            genMVHHI(getNextMR(destMR, node, 2, destLeftMostByte, isBCD, cg), node, (lit[2]<<8)|lit[3], cg);
            }
         break;
      case 5:
         if (checkMVHI(lit,0))
            {
            // MVHI/MVI (MVHI 0,1,2,3 bytes value <= 0x7FFF)
            genMVHI(destMR, node, (lit[2]<<8)|lit[3], cg);
            genMVI(getNextMR(destMR, node, 4, destLeftMostByte, isBCD, cg), node, lit[4], cg);
            }
         else
            {
            // MVI/MVHI (MVHI 1,2,3,4 bytes value <= 0x7FFF)
            TR_ASSERT(checkMVHI(lit,1),"checkMVHI should be true\n");
            genMVI(destMR, node, lit[0], cg);
            genMVHI(getNextMR(destMR, node, 1, destLeftMostByte, isBCD, cg), node, (lit[3]<<8)|lit[4], cg);
            }
         break;
      case 6:
         if (checkMVHI(lit,0))
            {
            // MVHI/MVHHI (MVHI 0,1,2,3 bytes value <= 0x7FFF)
            genMVHI(destMR, node, (lit[2]<<8)|lit[3], cg);
            genMVHHI(getNextMR(destMR, node, 4, destLeftMostByte, isBCD, cg), node, (lit[4]<<8)|lit[5], cg);
            }
         else
            {
            // MVHHI/MVHI (MVHI 2,3,4,5 bytes value <= 0x7FFF)
            TR_ASSERT(checkMVHI(lit,2),"checkMVHI should be true\n");
            genMVHHI(destMR, node, (lit[0]<<8)|lit[1], cg);
            genMVHI(getNextMR(destMR, node, 2, destLeftMostByte, isBCD, cg), node, (lit[4]<<8)|lit[5], cg);
            }
         break;
      case 7:
         TR_ASSERT(false,"copySize %d not supported on node %p\n",size,node);
         break;
      case 8:  // MVGHI (value <= 0x7FFF)
         if (checkMVGHI(lit,0))
            {
            genMVGHI(destMR, node, (lit[6]<<8)|lit[7], cg);
            }
         else
            {
            TR_ASSERT(checkMVHI(lit,0) && checkMVHI(lit,4),"checkMVHI+checkMVHI should be true\n");
            genMVHI(destMR, node, (lit[2]<<8)|lit[3], cg);
            genMVHI(getNextMR(destMR, node, 4, destLeftMostByte, isBCD, cg), node, (lit[6]<<8)|lit[7], cg);
            }
         break;
      case 9:  // MVGHI/MVI (MVGHI <= 0x7FFF)
         genMVGHI(destMR, node, (lit[6]<<8)|lit[7], cg);
         genMVI(getNextMR(destMR, node, 8, destLeftMostByte, isBCD, cg), node, lit[8], cg);
         break;
      case 10: // MVGHI/MVHHI (MVGHI <= 0x7FFF)
         genMVGHI(destMR, node, (lit[6]<<8)|lit[7], cg);
         genMVHHI(getNextMR(destMR, node, 8, destLeftMostByte, isBCD, cg), node, (lit[8]<<8)|lit[9], cg);
         break;
      case 11:
         TR_ASSERT(false,"copySize %d not supported on node %p\n",size,node);
         break;
      case 12: // MVGHI/MVHI  (MVGHI and MVHI value both <= 0x7FFF)
         genMVGHI(destMR, node, (lit[6]<<8)|lit[7], cg);
         genMVHI(getNextMR(destMR, node, 8, destLeftMostByte, isBCD, cg), node, (lit[10]<<8)|lit[11], cg);
         break;
      case 13:
      case 14:
      case 15:
         TR_ASSERT(false,"copySize %d not supported on node %p\n",size,node);
         break;
      case 16: // MVGHI/MVGHI (both MVGHI values <= 0x7FFF)
         genMVGHI(destMR, node, (lit[6]<<8)|lit[7], cg);
         genMVGHI(getNextMR(destMR, node, 8, destLeftMostByte, isBCD, cg), node, (lit[14]<<8)|lit[15], cg);
         break;
      default:
         TR_ASSERT(false,"copySize %d not supported on node %p\n",size,node);
         break;
      }

   return true;
   }

bool
J9::Z::CodeGenerator::inlineSmallLiteral(size_t srcSize, char *srcLiteral, size_t destSize, bool trace)
   {
   TR::Compilation *comp = self()->comp();

   bool inlineLiteral = false;
   if (srcSize != destSize)
      {
      inlineLiteral = false;
      if (trace)
         traceMsg(comp,"\t\tinlineLiteral=false : srcSize %d != destSize %d\n",srcSize,destSize);
      }
   else if (srcSize == 1)
      {
      inlineLiteral = true;
      if (trace)
         traceMsg(comp,"\t\tinlineLiteral=true : srcSize == 1 (destSize %d)\n",destSize);
      }
   else if (destSize <= 2)
      {
      inlineLiteral = true;
      if (trace)
         traceMsg(comp,"\t\tinlineLiteral=true : destSize %d <= 2 (srcSize %d)\n",destSize,srcSize);
      }
   else if (self()->canCopyWithOneOrTwoInstrs(srcLiteral, srcSize))
      {
      inlineLiteral = true;
      if (trace)
         traceMsg(comp,"\t\tinlineLiteral=true : canCopyWithOneOrTwoInstrs = true (srcSize %d, destSize %d)\n",srcSize,destSize);
      }
   else
      {
      inlineLiteral = false;
      if (trace)
         traceMsg(comp,"\t\tinlineSmallLiteral=false : unhandled case (srcSize %d, destSize %d)\n",srcSize,destSize);
      }
   return inlineLiteral;
   }


bool
J9::Z::CodeGenerator::checkFieldAlignmentForAtomicLong()
   {
   TR_OpaqueClassBlock * classBlock = self()->comp()->fej9()->getSystemClassFromClassName("java/util/concurrent/atomic/AtomicLong", 38, true);

   // TR_J9SharedCacheVM::getSystemClassFromClassName can return 0 when it's impossible to relocate a J9Class later for AOT loads.
   if (!classBlock)
      return false;

   char* fieldName = "value";
   int32_t fieldNameLen = 5;
   char * fieldSig = "J";
   int32_t fieldSigLen = 1;
   int32_t intOrBoolOffset = self()->fe()->getObjectHeaderSizeInBytes() + self()->fej9()->getInstanceFieldOffset(classBlock, fieldName, fieldNameLen, fieldSig, fieldSigLen);
   return (intOrBoolOffset & 0x3) == 0;
   }


TR_PseudoRegister *
J9::Z::CodeGenerator::evaluateBCDNode(TR::Node * node)
   {
   TR_ASSERT(node->getType().isBCD(),"evaluateBCDNode only valid for binary coded decimal types\n");
   bool isFirstTime = node->getRegister() == NULL;
   TR::Register *reg = self()->evaluate(node);
   TR_PseudoRegister *pseudoReg = reg->getPseudoRegister();
   TR_ASSERT(pseudoReg,"pseudoReg should not be NULL after evaluation of node %p\n",node);
   if (isFirstTime)
      {
      if (node->getOpCode().canHaveStorageReferenceHint() &&
          node->getStorageReferenceHint() &&
          node->getStorageReferenceHint()->isTemporaryBased())
         {
         if (self()->traceBCDCodeGen())
            traceMsg(self()->comp(),"evaluateBCDNode: found temp based hint #%d on %s (%p)\n",
               node->getStorageReferenceHint()->getReferenceNumber(),
               node->getOpCode().getName(),
               node);
         node->getStorageReferenceHint()->removeSharedNode(node);
         }
      // to prevent refCount underflow on the padding address node can only use this tree on the first reference to a node
      if (node->getOpCode().canHavePaddingAddress())
         {
         if (self()->traceBCDCodeGen())
            traceMsg(self()->comp(),"evaluateBCDNode: set UsedPaddingAnchorAddress flag to true on %s (%p)\n",
               node->getOpCode().getName(),
               node);
         }
      }
//   TR_ASSERT(pseudoReg->signStateInitialized(),"sign state for node %p register not initialized\n",node);
   return pseudoReg;
   }

void
J9::Z::CodeGenerator::addAllocatedRegister(TR_PseudoRegister * temp)
   {
   uint32_t idx = _registerArray.add(temp);
   temp->setIndex(idx);
   self()->startUsingRegister(temp);
   }


/**
 * These routines return the minimum precision and size values for a packed arithmetic node so the corresponding
 * hardware instruction (AP,SP,MP,DP) can be legally encode
 */
uint32_t
J9::Z::CodeGenerator::getPDMulEncodedSize(TR::Node *pdmul, TR_PseudoRegister *multiplicand, TR_PseudoRegister *multiplier)
   {
   TR_ASSERT(pdmul->getType().isAnyPacked(), "getPDMulEncodedSize only valid for packed types and not type %s\n",pdmul->getDataType().toString());
   return multiplicand->getSize() + multiplier->getSize();
   }

uint32_t
J9::Z::CodeGenerator::getPDMulEncodedSize(TR::Node *pdmul)
   {
   TR_ASSERT(pdmul->getType().isAnyPacked(), "getPDMulEncodedSize only valid for packed types and not type %s\n",pdmul->getDataType().toString());
   return pdmul->getFirstChild()->getSize() + pdmul->getSecondChild()->getSize();
   }

uint32_t
J9::Z::CodeGenerator::getPDMulEncodedSize(TR::Node *pdmul, int32_t exponent)
   {
   TR_ASSERT(pdmul->getType().isAnyPacked(), "getPDMulEncodedSize only valid for packed types and not type %s\n",pdmul->getDataType().toString());
   return pdmul->getFirstChild()->getSize() * exponent;
   }

int32_t
J9::Z::CodeGenerator::getPDMulEncodedPrecision(TR::Node *pdmul, TR_PseudoRegister *multiplicand, TR_PseudoRegister *multiplier)
   {
   TR_ASSERT(pdmul->getType().isAnyPacked(), "getPDMulEncodedPrecision only valid for packed types and not type %s\n",pdmul->getDataType().toString());
   return TR::DataType::byteLengthToPackedDecimalPrecisionFloor(self()->getPDMulEncodedSize(pdmul, multiplicand, multiplier));
   }

int32_t
J9::Z::CodeGenerator::getPDMulEncodedPrecision(TR::Node *pdmul)
   {
   TR_ASSERT(pdmul->getType().isAnyPacked(), "getPDMulEncodedPrecision only valid for packed types and not type %s\n",pdmul->getDataType().toString());
   return TR::DataType::byteLengthToPackedDecimalPrecisionFloor(self()->getPDMulEncodedSize(pdmul));
   }

int32_t
J9::Z::CodeGenerator::getPDMulEncodedPrecision(TR::Node *pdmul, int32_t exponent)
   {
   TR_ASSERT(pdmul->getType().isAnyPacked(), "getPDMulEncodedPrecision only valid for packed types and not type %s\n",pdmul->getDataType().toString());
   return TR::DataType::byteLengthToPackedDecimalPrecisionFloor(self()->getPDMulEncodedSize(pdmul, exponent));
   }

uint32_t
J9::Z::CodeGenerator::getPackedToDecimalFloatFixedSize()
   {
   if (self()->supportsFastPackedDFPConversions())
      return TR_PACKED_TO_DECIMAL_FLOAT_SIZE_ARCH11;
   return TR_PACKED_TO_DECIMAL_FLOAT_SIZE;
   }

uint32_t
J9::Z::CodeGenerator::getPackedToDecimalDoubleFixedSize()
   {
   if (self()->supportsFastPackedDFPConversions())
      return TR_PACKED_TO_DECIMAL_DOUBLE_SIZE_ARCH11;
   return TR_PACKED_TO_DECIMAL_DOUBLE_SIZE;
   }

uint32_t
J9::Z::CodeGenerator::getPackedToDecimalLongDoubleFixedSize()
   {
   if (self()->supportsFastPackedDFPConversions())
      return TR_PACKED_TO_DECIMAL_LONG_DOUBLE_SIZE_ARCH11;
   return TR_PACKED_TO_DECIMAL_LONG_DOUBLE_SIZE;
   }

uint32_t
J9::Z::CodeGenerator::getDecimalFloatToPackedFixedSize()
   {
   if (self()->supportsFastPackedDFPConversions())
      return TR_DECIMAL_FLOAT_TO_PACKED_SIZE_ARCH11;
   return TR_DECIMAL_FLOAT_TO_PACKED_SIZE;
   }

uint32_t
J9::Z::CodeGenerator::getDecimalDoubleToPackedFixedSize()
   {
   if (self()->supportsFastPackedDFPConversions())
      return TR_DECIMAL_DOUBLE_TO_PACKED_SIZE_ARCH11;
   return TR_DECIMAL_DOUBLE_TO_PACKED_SIZE;
   }

uint32_t
J9::Z::CodeGenerator::getDecimalLongDoubleToPackedFixedSize()
   {
   if (self()->supportsFastPackedDFPConversions())
      return TR_DECIMAL_LONG_DOUBLE_TO_PACKED_SIZE_ARCH11;
   return TR_DECIMAL_LONG_DOUBLE_TO_PACKED_SIZE;
   }

/**
 * Motivating example for the packedAddSubSize
 * pdsub p=3,s=2     // correct answer is 12111-345 truncated to 3 digits = (11)766
 *    pdload p=5,s=3 //  12111
 *    pdload p=3,s=3 //    345
 * If an SP of size=2 is used then the answer will be 111-345 = -234 instead of 766 as SP/AP are destructive operations
 * so for AP/SP the encoded firstOp/result size must be at least as big as the first operand.
 */
uint32_t
J9::Z::CodeGenerator::getPDAddSubEncodedSize(TR::Node *node)
   {
   TR_ASSERT( node->getType().isAnyPacked() && node->getFirstChild()->getType().isAnyPacked(),"getPackedAddSubSize only valid for packed types\n");
   return std::max(node->getSize(), node->getFirstChild()->getSize());
   }

int32_t
J9::Z::CodeGenerator::getPDAddSubEncodedPrecision(TR::Node *node)
   {
   TR_ASSERT( node->getType().isAnyPacked() && node->getFirstChild()->getType().isAnyPacked(),"getPackedAddSubPrecision only valid for packed types\n");
   return std::max(node->getDecimalPrecision(), node->getFirstChild()->getDecimalPrecision());
   }

uint32_t
J9::Z::CodeGenerator::getPDAddSubEncodedSize(TR::Node *node, TR_PseudoRegister *firstReg)
   {
   TR_ASSERT( node->getType().isAnyPacked() && firstReg->getDataType().isAnyPacked(),"getPackedAddSubSize only valid for packed types\n");
   return std::max<uint32_t>(node->getSize(), firstReg->getSize());
   }

int32_t
J9::Z::CodeGenerator::getPDAddSubEncodedPrecision(TR::Node *node, TR_PseudoRegister *firstReg)
   {
   TR_ASSERT( node->getType().isAnyPacked() && firstReg->getDataType().isAnyPacked(),"getPackedAddSubPrecision only valid for packed types\n");
   return std::max<int32_t>(node->getDecimalPrecision(), firstReg->getDecimalPrecision());
   }

bool
J9::Z::CodeGenerator::supportsPackedShiftRight(int32_t resultPrecision, TR::Node *shiftSource, int32_t shiftAmount)
   {
   bool isSupported = false;
   int32_t maxPrecision = TR::DataType::getMaxPackedDecimalPrecision();
   int32_t sourceDigits = shiftSource->getDecimalPrecision();
   int32_t shiftedPrecision = sourceDigits - shiftAmount;
   if (resultPrecision <= maxPrecision)
      {
      isSupported = true;  // fits in an MVO or SRP (and all SS2/SS3 instructions)
      }
   else if (shiftedPrecision <= maxPrecision)
      {
      isSupported = true;  // fits in an MVO or SRP (and all SS2/SS3 instructions)
      }
   else if (isEven(shiftAmount))
      {
      isSupported = true;  // uses MVN to move just the sign code so no restriction on length
      }

   if (self()->traceBCDCodeGen())
      traceMsg(self()->comp(),"%ssupportsPackedShiftRight = %s : shiftSource %s (%p) p=%d by shiftAmount=%d -> shiftedPrec=%d (resultPrec %d) on line_no=%d (offset=%06X)\n",
         isSupported?"":"t^t : ",isSupported?"yes":"no",shiftSource->getOpCode().getName(),shiftSource,
         sourceDigits,shiftAmount,shiftedPrecision,resultPrecision,
         self()->comp()->getLineNumber(shiftSource),self()->comp()->getLineNumber(shiftSource));

   return isSupported;
   }

int32_t
J9::Z::CodeGenerator::getPDDivEncodedPrecision(TR::Node *node)
   {
   TR_ASSERT(node->getOpCodeValue() == TR::pddiv || node->getOpCodeValue() == TR::pdrem,
      "getPackedDividendPrecision only valid for pddiv/pdrem\n");
   return self()->getPDDivEncodedPrecisionCommon(node,
                                         node->getFirstChild()->getDecimalPrecision(),
                                         node->getSecondChild()->getDecimalPrecision(),
                                         node->getSecondChild()->isEvenPrecision());
   }

int32_t
J9::Z::CodeGenerator::getPDDivEncodedPrecision(TR::Node *node, TR_PseudoRegister *dividendReg, TR_PseudoRegister *divisorReg)
   {
   TR_ASSERT(node->getOpCodeValue() == TR::pddiv || node->getOpCodeValue() == TR::pdrem,
      "getPackedDividendPrecision only valid for pddiv/pdrem\n");
   return self()->getPDDivEncodedPrecisionCommon(node,
                                         dividendReg->getDecimalPrecision(),
                                         divisorReg->getDecimalPrecision(),
                                         divisorReg->isEvenPrecision());
   }


int32_t
J9::Z::CodeGenerator::getPDDivEncodedPrecisionCommon(TR::Node *node, int32_t dividendPrecision, int32_t divisorPrecision, bool isDivisorEvenPrecision)
   {
   int32_t basePrecision = dividendPrecision;
   int32_t quotientAdjust = 1;   // always subtract off second sign code when computing the quotient precision
   if (isDivisorEvenPrecision)
      quotientAdjust++;          // adjust for the pad nibble
   return basePrecision+divisorPrecision+quotientAdjust;
   }

uint32_t
J9::Z::CodeGenerator::getPDDivEncodedSize(TR::Node *node)
   {
   TR_ASSERT(node->getOpCodeValue() == TR::pddiv || node->getOpCodeValue() == TR::pdrem,
      "getPDDivEncodedSize only valid for pddiv/pdrem\n");
   return TR::DataType::packedDecimalPrecisionToByteLength(self()->getPDDivEncodedPrecision(node));
   }

uint32_t
J9::Z::CodeGenerator::getPDDivEncodedSize(TR::Node *node, TR_PseudoRegister *dividendReg, TR_PseudoRegister *divisorReg)
   {
   TR_ASSERT(node->getOpCodeValue() == TR::pddiv || node->getOpCodeValue() == TR::pdrem,
      "getPDDivEncodedSize only valid for pddiv/pdrem\n");
   return TR::DataType::packedDecimalPrecisionToByteLength(self()->getPDDivEncodedPrecision(node, dividendReg, divisorReg));
   }

bool
J9::Z::CodeGenerator::canGeneratePDBinaryIntrinsic(TR::ILOpCodes opCode, TR::Node * op1PrecNode, TR::Node * op2PrecNode, TR::Node * resultPrecNode)
   {
   if(!op2PrecNode->getOpCode().isLoadConst() || !op1PrecNode->getOpCode().isLoadConst() || !resultPrecNode->getOpCode().isLoadConst())
      return false;

   int32_t max = TR::DataType::getMaxPackedDecimalPrecision();

   int32_t op1Prec = op1PrecNode->getInt();
   int32_t op2Prec = op2PrecNode->getInt();
   int32_t resultPrec = resultPrecNode->getInt();

   if(op1Prec > max || op2Prec > max || resultPrec > max)
      return false;

   int32_t op1Size = TR::DataType::packedDecimalPrecisionToByteLength(op1Prec);
   int32_t op2Size = TR::DataType::packedDecimalPrecisionToByteLength(op2Prec);
   int32_t resultSize = TR::DataType::packedDecimalPrecisionToByteLength(resultPrec);

   switch(opCode)
      {
      case TR::pdadd:
      case TR::pdsub:
      case TR::pdmul:
         if(op2Prec > 15)
            return false;
         if(resultSize < (op1Size + op2Size))
            return false;
         break;
      case TR::pddiv:
      case TR::pdrem:
         if(op2Size >= op1Size)
            return false;
         if(op2Prec > 15 || op1Prec > 31 || (op1Prec-op2Prec) > 29)
            return false;
         break;
      default:
         TR_ASSERT(0, "not implemented yet");
         return false;
      }

   return true;
   }

void
J9::Z::CodeGenerator::incRefCountForOpaquePseudoRegister(TR::Node * node)
   {
   if (node->getOpaquePseudoRegister())
      {
      TR_OpaquePseudoRegister *reg = node->getOpaquePseudoRegister();
      TR_StorageReference *ref = reg->getStorageReference();
      if (ref && ref->isNodeBased() && ref->getNodeReferenceCount() > 0)
         {
         if (self()->traceBCDCodeGen())
            self()->comp()->getDebug()->trace("\tnode %s (%p) with storageRef #%d (%s): increment nodeRefCount %d->%d when artificially incrementing ref count\n",
               node->getOpCode().getName(),node,ref->getReferenceNumber(),self()->comp()->getDebug()->getName(ref->getSymbol()),ref->getNodeReferenceCount(),ref->getNodeReferenceCount()+1);
         ref->incrementNodeReferenceCount();
         }
      }
   }

TR::Instruction* J9::Z::CodeGenerator::generateVMCallHelperSnippet(TR::Instruction* cursor, TR::LabelSymbol* vmCallHelperSnippetLabel)
   {
   TR::Compilation* comp = self()->comp();

   // Associate all generated instructions with the first node
   TR::Node* node = comp->getStartTree()->getNode();

   cursor = generateS390LabelInstruction(self(), TR::InstOpCode::LABEL, node, vmCallHelperSnippetLabel, cursor);

   TR::Instruction* vmCallHelperSnippetLabelInstruction = cursor;

   // Store all arguments to the stack for access by the interpreted method
   J9::Z::PrivateLinkage *privateLinkage = static_cast<J9::Z::PrivateLinkage *>(self()->getLinkage());
   cursor = static_cast<TR::Instruction*>(privateLinkage->saveArguments(cursor, false, true));

   // Load the EP register with the address of the next instruction
   cursor = generateRRInstruction(self(), TR::InstOpCode::BASR, node, self()->getEntryPointRealRegister(), self()->machine()->getRealRegister(TR::RealRegister::GPR0), cursor);

   TR::Instruction* basrInstruction = cursor;

   // Displacement will be updated later once we know the offset
   TR::MemoryReference* j9MethodAddressMemRef = generateS390MemoryReference(self()->getEntryPointRealRegister(), 0, self());

   // Load the address of the J9Method corresponding to this JIT compilation
   cursor = generateRXInstruction(self(), TR::InstOpCode::getLoadOpCode(), node, self()->machine()->getRealRegister(TR::RealRegister::GPR1), j9MethodAddressMemRef, cursor);

   // Displacement will be updated later once we know the offset
   TR::MemoryReference* vmCallHelperAddressMemRef = generateS390MemoryReference(self()->getEntryPointRealRegister(), 0, self());

   // Load the address of the VM call helper
   cursor = generateRXInstruction(self(), TR::InstOpCode::getLoadOpCode(), node, self()->getEntryPointRealRegister(), vmCallHelperAddressMemRef, cursor);

   // Call the VM call helper
   cursor = generateS390BranchInstruction(self(), TR::InstOpCode::BCR, node, TR::InstOpCode::COND_BCR, self()->getEntryPointRealRegister(), cursor);

   const int32_t offsetFromEPRegisterValueToVMCallHelperAddress = CalcCodeSize(basrInstruction->getNext(), cursor);

   vmCallHelperAddressMemRef->setOffset(offsetFromEPRegisterValueToVMCallHelperAddress);

   TR::ResolvedMethodSymbol* methodSymbol = comp->getJittedMethodSymbol();

   TR::SymbolReference* helperSymRef = self()->symRefTab()->findOrCreateRuntimeHelper(TR_j2iTransition);

   // AOT relocation for the helper address
   TR::S390EncodingRelocation* encodingRelocation = new (self()->trHeapMemory()) TR::S390EncodingRelocation(TR_AbsoluteHelperAddress, helperSymRef);

   AOTcgDiag3(comp, "Add encodingRelocation = %p reloType = %p symbolRef = %p\n", encodingRelocation, encodingRelocation->getReloType(), encodingRelocation->getSymbolReference());

   const intptr_t vmCallHelperAddress = reinterpret_cast<intptr_t>(helperSymRef->getMethodAddress());

   // Encode the address of the VM call helper
   if (comp->target().is64Bit())
      {
      cursor = generateDataConstantInstruction(self(), TR::InstOpCode::DC, node, UPPER_4_BYTES(vmCallHelperAddress), cursor);
      cursor->setEncodingRelocation(encodingRelocation);

      cursor = generateDataConstantInstruction(self(), TR::InstOpCode::DC, node, LOWER_4_BYTES(vmCallHelperAddress), cursor);
      }
   else
      {
      cursor = generateDataConstantInstruction(self(), TR::InstOpCode::DC, node, vmCallHelperAddress, cursor);
      cursor->setEncodingRelocation(encodingRelocation);
      }

   const int32_t offsetFromEPRegisterValueToJ9MethodAddress = CalcCodeSize(basrInstruction->getNext(), cursor);

   j9MethodAddressMemRef->setOffset(offsetFromEPRegisterValueToJ9MethodAddress);
   TR::SymbolReference *methodSymRef = new (self()->trHeapMemory()) TR::SymbolReference(self()->symRefTab(), methodSymbol);
   encodingRelocation = new (self()->trHeapMemory()) TR::S390EncodingRelocation(TR_RamMethod, methodSymRef);

   AOTcgDiag2(comp, "Add encodingRelocation = %p reloType = %p\n", encodingRelocation, encodingRelocation->getReloType());

   const intptr_t j9MethodAddress = reinterpret_cast<intptr_t>(methodSymbol->getResolvedMethod()->resolvedMethodAddress());

   if (comp->target().is64Bit())
      {
      cursor = generateDataConstantInstruction(self(), TR::InstOpCode::DC, node, UPPER_4_BYTES(j9MethodAddress), cursor);
      cursor->setEncodingRelocation(encodingRelocation);

      cursor = generateDataConstantInstruction(self(), TR::InstOpCode::DC, node, LOWER_4_BYTES(j9MethodAddress), cursor);
      }
   else
      {
      cursor = generateDataConstantInstruction(self(), TR::InstOpCode::DC, node, j9MethodAddress, cursor);
      cursor->setEncodingRelocation(encodingRelocation);
      }

   if (comp->getOption(TR_EnableHCR))
      {
      comp->getStaticHCRPICSites()->push_front(cursor);
      }

   int32_t padSize = CalcCodeSize(vmCallHelperSnippetLabelInstruction, cursor) % TR::Compiler->om.sizeofReferenceAddress();

   if (padSize != 0)
      {
      padSize = TR::Compiler->om.sizeofReferenceAddress() - padSize;
      }

   // Align to the size of the reference field to ensure alignment of subsequent sections for atomic patching
   cursor = self()->insertPad(node, cursor, padSize, false);

   return cursor;
   }

bool J9::Z::CodeGenerator::canUseRelativeLongInstructions(int64_t value)
   {
   if (self()->comp()->isOutOfProcessCompilation())
      {
      return false;
      }
   return OMR::CodeGeneratorConnector::canUseRelativeLongInstructions(value);
   }

TR::Instruction* J9::Z::CodeGenerator::generateVMCallHelperPrePrologue(TR::Instruction* cursor)
   {
   TR::Compilation* comp = self()->comp();

   // Associate all generated instructions with the first node
   TR::Node* node = comp->getStartTree()->getNode();

   TR::LabelSymbol* vmCallHelperSnippetLabel = generateLabelSymbol(self());

   cursor = self()->generateVMCallHelperSnippet(cursor, vmCallHelperSnippetLabel);

   cursor = generateS390BranchInstruction(self(), TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, vmCallHelperSnippetLabel, cursor);

   // The following 4 bytes are used for various patching sequences that overwrite the JIT entry point with a 4 byte
   // branch (BRC) to some location. Before patching in the branch we must save the 4 bytes at the JIT entry point
   // to this location so that we can later reverse the patching at JIT entry point if needed.
   cursor = generateDataConstantInstruction(self(), TR::InstOpCode::DC, node, 0xdeafbeef, cursor);

   // Generated a pad for the body info address to keep offsets in PreprologueConst.hpp constant for simplicity
   if (comp->target().is64Bit())
      {
      cursor = generateDataConstantInstruction(self(), TR::InstOpCode::DC, node, 0x00000000, cursor);
      cursor = generateDataConstantInstruction(self(), TR::InstOpCode::DC, node, 0x00000000, cursor);
      }
   else
      {
      cursor = generateDataConstantInstruction(self(), TR::InstOpCode::DC, node, 0x00000000, cursor);
      }

   return cursor;
   }

bool
J9::Z::CodeGenerator::suppressInliningOfRecognizedMethod(TR::RecognizedMethod method)
   {
   TR::Compilation *comp = self()->comp();

   if (self()->isMethodInAtomicLongGroup(method))
      return true;

   if (!comp->compileRelocatableCode() && !comp->getOption(TR_DisableDFP) && comp->target().cpu.supportsFeature(OMR_FEATURE_S390_DFP))
      {
      if (method == TR::java_math_BigDecimal_DFPIntConstructor ||
          method == TR::java_math_BigDecimal_DFPLongConstructor ||
          method == TR::java_math_BigDecimal_DFPLongExpConstructor ||
          method == TR::java_math_BigDecimal_DFPAdd ||
          method == TR::java_math_BigDecimal_DFPSubtract ||
          method == TR::java_math_BigDecimal_DFPMultiply ||
          method == TR::java_math_BigDecimal_DFPDivide ||
          method == TR::java_math_BigDecimal_DFPScaledAdd ||
          method == TR::java_math_BigDecimal_DFPScaledSubtract ||
          method == TR::java_math_BigDecimal_DFPScaledMultiply ||
          method == TR::java_math_BigDecimal_DFPScaledDivide ||
          method == TR::java_math_BigDecimal_DFPRound ||
          method == TR::java_math_BigDecimal_DFPSetScale ||
          method == TR::java_math_BigDecimal_DFPCompareTo ||
          method == TR::java_math_BigDecimal_DFPSignificance ||
          method == TR::java_math_BigDecimal_DFPExponent ||
          method == TR::java_math_BigDecimal_DFPBCDDigits ||
          method == TR::java_math_BigDecimal_DFPUnscaledValue ||
          method == TR::java_math_BigDecimal_DFPConvertPackedToDFP ||
          method == TR::java_math_BigDecimal_DFPConvertDFPToPacked)
         {
         return true;
         }

      if (method == TR::com_ibm_dataaccess_DecimalData_DFPConvertPackedToDFP ||
          method == TR::com_ibm_dataaccess_DecimalData_DFPConvertDFPToPacked)
         {
         return true;
         }
      }

   if (self()->getSupportsVectorRegisters()){
      if (method == TR::java_lang_Math_fma_D ||
          method == TR::java_lang_StrictMath_fma_D)
         {
         return true;
         }
      if (comp->target().cpu.supportsFeature(OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_1) &&
            (method == TR::java_lang_Math_fma_F ||
             method == TR::java_lang_StrictMath_fma_F))
         {
         return true;
         }
   }

   if (method == TR::java_lang_Integer_highestOneBit ||
       method == TR::java_lang_Integer_numberOfLeadingZeros ||
       method == TR::java_lang_Integer_numberOfTrailingZeros ||
       method == TR::java_lang_Long_highestOneBit ||
       method == TR::java_lang_Long_numberOfLeadingZeros ||
       method == TR::java_lang_Long_numberOfTrailingZeros)
      {
      return true;
      }

   if (method == TR::java_util_concurrent_atomic_AtomicBoolean_getAndSet ||
       method == TR::java_util_concurrent_atomic_AtomicInteger_getAndAdd ||
       method == TR::java_util_concurrent_atomic_AtomicInteger_getAndIncrement ||
       method == TR::java_util_concurrent_atomic_AtomicInteger_getAndDecrement ||
       method == TR::java_util_concurrent_atomic_AtomicInteger_getAndSet ||
       method == TR::java_util_concurrent_atomic_AtomicInteger_addAndGet ||
       method == TR::java_util_concurrent_atomic_AtomicInteger_decrementAndGet ||
       method == TR::java_util_concurrent_atomic_AtomicInteger_incrementAndGet ||
       method == TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_incrementAndGet ||
       method == TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_decrementAndGet ||
       method == TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_addAndGet ||
       method == TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndIncrement ||
       method == TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndDecrement ||
       method == TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndAdd)
      {
      return true;
      }

   // Transactional Memory
   if (self()->getSupportsInlineConcurrentLinkedQueue())
      {
      if (method == TR::java_util_concurrent_ConcurrentLinkedQueue_tmOffer ||
          method == TR::java_util_concurrent_ConcurrentLinkedQueue_tmPoll ||
          method == TR::java_util_concurrent_ConcurrentLinkedQueue_tmEnabled)
          {
          return true;
          }
      }

   return false;
   }

/* extern TreeEvaluator functions */
extern TR::Register* inlineCurrentTimeMaxPrecision(TR::CodeGenerator* cg, TR::Node* node);
extern TR::Register* inlineSinglePrecisionSQRT(TR::Node *node, TR::CodeGenerator *cg);
extern TR::Register* VMinlineCompareAndSwap( TR::Node *node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic casOp, bool isObj);
extern TR::Register* inlineAtomicOps(TR::Node *node, TR::CodeGenerator *cg, int8_t size, TR::MethodSymbol *method, bool isArray = false);
extern TR::Register* inlineAtomicFieldUpdater(TR::Node *node, TR::CodeGenerator *cg, TR::MethodSymbol *method);
extern TR::Register* inlineKeepAlive(TR::Node *node, TR::CodeGenerator *cg);
extern TR::Register* inlineConcurrentLinkedQueueTMOffer(TR::Node *node, TR::CodeGenerator *cg);
extern TR::Register* inlineConcurrentLinkedQueueTMPoll(TR::Node *node, TR::CodeGenerator *cg);

extern TR::Register* inlineStringHashCode(TR::Node *node, TR::CodeGenerator *cg, bool isCompressed);
extern TR::Register* inlineUTF16BEEncodeSIMD(TR::Node *node, TR::CodeGenerator *cg);
extern TR::Register* inlineUTF16BEEncode    (TR::Node *node, TR::CodeGenerator *cg);

extern TR::Register *inlineHighestOneBit(TR::Node *node, TR::CodeGenerator *cg, bool isLong);
extern TR::Register *inlineNumberOfLeadingZeros(TR::Node *node, TR::CodeGenerator * cg, bool isLong);
extern TR::Register *inlineNumberOfTrailingZeros(TR::Node *node, TR::CodeGenerator *cg, int32_t subfconst);
extern TR::Register *inlineTrailingZerosQuadWordAtATime(TR::Node *node, TR::CodeGenerator *cg);

extern TR::Register *inlineBigDecimalConstructor(TR::Node *node, TR::CodeGenerator *cg, bool isLong, bool exp);
extern TR::Register *inlineBigDecimalBinaryOp(TR::Node * node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, bool scaled);
extern TR::Register *inlineBigDecimalScaledDivide(TR::Node * node, TR::CodeGenerator *cg);
extern TR::Register *inlineBigDecimalDivide(TR::Node * node, TR::CodeGenerator *cg);
extern TR::Register *inlineBigDecimalRound(TR::Node * node, TR::CodeGenerator *cg);
extern TR::Register *inlineBigDecimalCompareTo(TR::Node * node, TR::CodeGenerator * cg);
extern TR::Register *inlineBigDecimalUnaryOp(TR::Node * node, TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op);
extern TR::Register *inlineBigDecimalSetScale(TR::Node * node, TR::CodeGenerator * cg);
extern TR::Register *inlineBigDecimalUnscaledValue(TR::Node * node, TR::CodeGenerator * cg);
extern TR::Register *inlineBigDecimalFromPackedConverter(TR::Node * node, TR::CodeGenerator * cg);
extern TR::Register *inlineBigDecimalToPackedConverter(TR::Node * node, TR::CodeGenerator * cg);

extern TR::Register *toUpperIntrinsic(TR::Node * node, TR::CodeGenerator * cg, bool isCompressedString);
extern TR::Register *toLowerIntrinsic(TR::Node * node, TR::CodeGenerator * cg, bool isCompressedString);

extern TR::Register* inlineVectorizedStringIndexOf(TR::Node* node, TR::CodeGenerator* cg, bool isCompressed);

extern TR::Register *inlineIntrinsicIndexOf(TR::Node* node, TR::CodeGenerator* cg, bool isLatin1);

extern TR::Register *inlineDoubleMax(TR::Node *node, TR::CodeGenerator *cg);
extern TR::Register *inlineDoubleMin(TR::Node *node, TR::CodeGenerator *cg);

extern TR::Register *inlineMathFma(TR::Node *node, TR::CodeGenerator *cg);

#define IS_OBJ      true
#define IS_NOT_OBJ  false

bool isKnownMethod(TR::MethodSymbol * methodSymbol)
   {
   return methodSymbol &&
          (methodSymbol->getRecognizedMethod() == TR::java_lang_Math_sqrt ||
           methodSymbol->getRecognizedMethod() == TR::java_lang_StrictMath_sqrt ||
           methodSymbol->getRecognizedMethod() == TR::java_lang_Class_isAssignableFrom);
   }

bool
J9::Z::CodeGenerator::inlineDirectCall(
      TR::Node *node,
      TR::Register *&resultReg)
   {
   TR::CodeGenerator *cg = self();
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   TR::MethodSymbol * methodSymbol = node->getSymbol()->getMethodSymbol();

   // If the method to be called is marked as an inline method, see if it can
   // actually be generated inline.
   //
   if (comp->getSymRefTab()->isNonHelper(node->getSymbolReference(), TR::SymbolReferenceTable::currentTimeMaxPrecisionSymbol))
      {
      resultReg = inlineCurrentTimeMaxPrecision(cg, node);
      return true;
      }
   else if (comp->getSymRefTab()->isNonHelper(node->getSymbolReference(), TR::SymbolReferenceTable::singlePrecisionSQRTSymbol))
      {
      resultReg = inlineSinglePrecisionSQRT(node, cg);
      return true;
      }
   else if (comp->getSymRefTab()->isNonHelper(node->getSymbolReference(), TR::SymbolReferenceTable::synchronizedFieldLoadSymbol))
      {
      ReduceSynchronizedFieldLoad::inlineSynchronizedFieldLoad(node, cg);
      return true;
      }

   static const char * enableTRTRE = feGetEnv("TR_enableTRTRE");
   switch (methodSymbol->getRecognizedMethod())
      {
      case TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z:
         // In Java9 this can be either the jdk.internal JNI method or the sun.misc Java wrapper.
         // In Java8 it will be sun.misc which will contain the JNI directly.
         // We only want to inline the JNI methods, so add an explicit test for isNative().
         if (!methodSymbol->isNative())
            break;

         if ((!TR::Compiler->om.canGenerateArraylets() || node->isUnsafeGetPutCASCallOnNonArray()) && node->isSafeForCGToFastPathUnsafeCall())
            {
            resultReg = VMinlineCompareAndSwap(node, cg, TR::InstOpCode::CS, IS_NOT_OBJ);
            return true;
            }

      case TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z:
         // As above, we only want to inline the JNI methods, so add an explicit test for isNative()
         if (!methodSymbol->isNative())
            break;

         if (comp->target().is64Bit() && (!TR::Compiler->om.canGenerateArraylets() || node->isUnsafeGetPutCASCallOnNonArray()) && node->isSafeForCGToFastPathUnsafeCall())
            {
            resultReg = VMinlineCompareAndSwap(node, cg, TR::InstOpCode::CSG, IS_NOT_OBJ);
            return true;
            }
         // Too risky to do Long-31bit version now.
         break;

      case TR::sun_misc_Unsafe_compareAndSwapObject_jlObjectJjlObjectjlObject_Z:
         // As above, we only want to inline the JNI methods, so add an explicit test for isNative()
         if (!methodSymbol->isNative())
            break;

         if ((!TR::Compiler->om.canGenerateArraylets() || node->isUnsafeGetPutCASCallOnNonArray()) && node->isSafeForCGToFastPathUnsafeCall())
            {
            resultReg = VMinlineCompareAndSwap(node, cg, (comp->useCompressedPointers() ? TR::InstOpCode::CS : TR::InstOpCode::getCmpAndSwapOpCode()), IS_OBJ);
            return true;
            }
         break;

      case TR::java_util_concurrent_atomic_Fences_reachabilityFence:
      case TR::java_util_concurrent_atomic_Fences_orderAccesses:
      case TR::java_util_concurrent_atomic_Fences_orderReads:
      case TR::java_util_concurrent_atomic_Fences_orderWrites:
         cg->decReferenceCount(node->getChild(0));
         break;

      case TR::java_util_concurrent_atomic_AtomicBoolean_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndAdd:
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndIncrement:
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndDecrement:
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicInteger_addAndGet:
      case TR::java_util_concurrent_atomic_AtomicInteger_incrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicInteger_decrementAndGet:
         resultReg = inlineAtomicOps(node, cg, 4, methodSymbol);
         return true;
         break;

      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndAdd:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndIncrement:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndDecrement:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_addAndGet:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_incrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_decrementAndGet:
         resultReg = inlineAtomicOps(node, cg, 4, methodSymbol, true);
         return true;
         break;

      case TR::java_util_concurrent_atomic_AtomicLong_addAndGet:
      case TR::java_util_concurrent_atomic_AtomicLong_getAndAdd:
      case TR::java_util_concurrent_atomic_AtomicLong_incrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicLong_getAndIncrement:
      case TR::java_util_concurrent_atomic_AtomicLong_decrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicLong_getAndDecrement:
         if (cg->checkFieldAlignmentForAtomicLong() && comp->target().cpu.isAtLeast(OMR_PROCESSOR_S390_Z196))
            {
            // TODO: I'm not sure we need the z196 restriction here given that the function already checks for z196 and
            // has a compare and swap fallback path
            resultReg = inlineAtomicOps(node, cg, 8, methodSymbol);
            return true;
            }
         break;

      case TR::java_util_concurrent_atomic_AtomicLongArray_addAndGet:
      case TR::java_util_concurrent_atomic_AtomicLongArray_getAndAdd:
      case TR::java_util_concurrent_atomic_AtomicLongArray_incrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicLongArray_getAndIncrement:
      case TR::java_util_concurrent_atomic_AtomicLongArray_decrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicLongArray_getAndDecrement:
         if (cg->checkFieldAlignmentForAtomicLong() && comp->target().cpu.isAtLeast(OMR_PROCESSOR_S390_Z196))
            {
            // TODO: I'm not sure we need the z196 restriction here given that the function already checks for z196 and
            // has a compare and swap fallback path
            resultReg = inlineAtomicOps(node, cg, 8, methodSymbol);
            return true;
            }
         break;

      case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_incrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_decrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_addAndGet:
      case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndIncrement:
      case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndDecrement:
      case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndAdd:
         if (cg->getSupportsAtomicLoadAndAdd())
            {
            resultReg = inlineAtomicFieldUpdater(node, cg, methodSymbol);
            return true;
            }
         break;

      case TR::java_nio_Bits_keepAlive:
      case TR::java_lang_ref_Reference_reachabilityFence:
         resultReg = inlineKeepAlive(node, cg);
         return true;

      case TR::java_util_concurrent_ConcurrentLinkedQueue_tmOffer:
         if (cg->getSupportsInlineConcurrentLinkedQueue())
            {
            resultReg = inlineConcurrentLinkedQueueTMOffer(node, cg);
            return true;
            }
         break;

      case TR::java_util_concurrent_ConcurrentLinkedQueue_tmPoll:
         if (cg->getSupportsInlineConcurrentLinkedQueue())
            {
            resultReg = inlineConcurrentLinkedQueueTMPoll(node, cg);
            return true;
            }
         break;
       // HashCode routine for Compressed and Decompressed String Shares lot of code so combining them.
      case TR::java_lang_String_hashCodeImplDecompressed:
         if (cg->getSupportsInlineStringHashCode())
            {
            return resultReg = inlineStringHashCode(node, cg, false);
            }
         break;

      case TR::java_lang_String_hashCodeImplCompressed:
         if (cg->getSupportsInlineStringHashCode())
            {
            return resultReg = inlineStringHashCode(node, cg, true);
            }
        break;

      case TR::com_ibm_jit_JITHelpers_transformedEncodeUTF16Big:
         return resultReg = comp->getOption(TR_DisableUTF16BEEncoder) ? inlineUTF16BEEncodeSIMD(node, cg)
                                                                      : inlineUTF16BEEncode    (node, cg);
         break;

      default:
         break;

      }

   switch (methodSymbol->getRecognizedMethod())
      {
      case TR::java_lang_Integer_highestOneBit:
         resultReg = inlineHighestOneBit(node, cg, false);
         return true;
      case TR::java_lang_Integer_numberOfLeadingZeros:
         resultReg = inlineNumberOfLeadingZeros(node, cg, false);
         return true;
      case TR::java_lang_Integer_numberOfTrailingZeros:
         resultReg = inlineNumberOfTrailingZeros(node, cg, 32);
         return true;
      case TR::java_lang_Long_highestOneBit:
         resultReg = inlineHighestOneBit(node, cg, true);
         return true;
      case TR::java_lang_Long_numberOfLeadingZeros:
         resultReg = inlineNumberOfLeadingZeros(node, cg, true);
         return true;
      case TR::java_lang_Long_numberOfTrailingZeros:
         resultReg = inlineNumberOfTrailingZeros(node, cg, 64);
         return true;
      default:
         break;
      }

#ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION
   if (self()->inlineCryptoMethod(node, resultReg))
      {
      return true;
      }
#endif

   if (cg->getSupportsInlineStringCaseConversion())
      {
      switch (methodSymbol->getRecognizedMethod())
         {
         case TR::com_ibm_jit_JITHelpers_toUpperIntrinsicUTF16:
            resultReg = toUpperIntrinsic(node, cg, false);
            return true;
         case TR::com_ibm_jit_JITHelpers_toUpperIntrinsicLatin1:
            resultReg = toUpperIntrinsic(node, cg, true);
            return true;
         case TR::com_ibm_jit_JITHelpers_toLowerIntrinsicUTF16:
            resultReg = toLowerIntrinsic(node, cg, false);
            return true;
         case TR::com_ibm_jit_JITHelpers_toLowerIntrinsicLatin1:
            resultReg = toLowerIntrinsic(node, cg, true);
            return true;
         default:
            break;
         }
      }

   if (cg->getSupportsInlineStringIndexOf())
      {
      switch (methodSymbol->getRecognizedMethod())
         {
         case TR::com_ibm_jit_JITHelpers_intrinsicIndexOfLatin1:
            resultReg = inlineIntrinsicIndexOf(node, cg, true);
            return true;
         case TR::com_ibm_jit_JITHelpers_intrinsicIndexOfUTF16:
            resultReg = inlineIntrinsicIndexOf(node, cg, false);
            return true;
         case TR::java_lang_StringLatin1_indexOf:
         case TR::com_ibm_jit_JITHelpers_intrinsicIndexOfStringLatin1:
               resultReg = inlineVectorizedStringIndexOf(node, cg, false);
               return true;
         case TR::java_lang_StringUTF16_indexOf:
         case TR::com_ibm_jit_JITHelpers_intrinsicIndexOfStringUTF16:
               resultReg = inlineVectorizedStringIndexOf(node, cg, true);
               return true;
         default:
            break;
         }
      }

      if (!comp->getOption(TR_DisableSIMDDoubleMaxMin) && cg->getSupportsVectorRegisters())
         {
         switch (methodSymbol->getRecognizedMethod())
            {
            case TR::java_lang_Math_max_D:
               resultReg = inlineDoubleMax(node, cg);
               return true;
            case TR::java_lang_Math_min_D:
               resultReg = inlineDoubleMin(node, cg);
               return true;
            default:
               break;
            }
         }
      if (cg->getSupportsVectorRegisters())
         {
         switch (methodSymbol->getRecognizedMethod())
            {
            case TR::java_lang_Math_fma_D:
            case TR::java_lang_StrictMath_fma_D:
               resultReg = inlineMathFma(node, cg);
               return true;

            case TR::java_lang_Math_fma_F:
            case TR::java_lang_StrictMath_fma_F:
               if (comp->target().cpu.supportsFeature(OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_1))
                  {
                  resultReg = inlineMathFma(node, cg);
                  return true;
                  }
               break;
            default:
               break;
            }
         }

   if (!comp->compileRelocatableCode() && !comp->getOption(TR_DisableDFP) &&
       comp->target().cpu.supportsFeature(OMR_FEATURE_S390_DFP))
      {
      TR_ASSERT( methodSymbol, "require a methodSymbol for DFP on Z\n");
      if (methodSymbol)
         {
         switch(methodSymbol->getMandatoryRecognizedMethod())
            {
            case TR::java_math_BigDecimal_DFPIntConstructor:
               resultReg = inlineBigDecimalConstructor(node, cg, false, false);
               return true;
            case TR::java_math_BigDecimal_DFPLongConstructor:
               resultReg = inlineBigDecimalConstructor(node, cg, true, false);
               return true;
            case TR::java_math_BigDecimal_DFPLongExpConstructor:
               resultReg = inlineBigDecimalConstructor(node, cg, true, true);
               return true;
            case TR::java_math_BigDecimal_DFPAdd:
               resultReg = inlineBigDecimalBinaryOp(node, cg, TR::InstOpCode::ADTR, false);
               return true;
            case TR::java_math_BigDecimal_DFPSubtract:
               resultReg = inlineBigDecimalBinaryOp(node, cg, TR::InstOpCode::SDTR, false);
               return true;
            case TR::java_math_BigDecimal_DFPMultiply:
               resultReg = inlineBigDecimalBinaryOp(node, cg, TR::InstOpCode::MDTR, false);
               return true;
            case TR::java_math_BigDecimal_DFPScaledAdd:
               resultReg = inlineBigDecimalBinaryOp(node, cg, TR::InstOpCode::ADTR, true);
               return true;
            case TR::java_math_BigDecimal_DFPScaledSubtract:
               resultReg = inlineBigDecimalBinaryOp(node, cg, TR::InstOpCode::SDTR, true);
               return true;
            case TR::java_math_BigDecimal_DFPScaledMultiply:
               resultReg = inlineBigDecimalBinaryOp(node, cg, TR::InstOpCode::MDTR, true);
               return true;
            case TR::java_math_BigDecimal_DFPRound:
               resultReg = inlineBigDecimalRound(node, cg);
               return true;
            case TR::java_math_BigDecimal_DFPSignificance:
               resultReg = inlineBigDecimalUnaryOp(node, cg, TR::InstOpCode::ESDTR);
               return true;
            case TR::java_math_BigDecimal_DFPExponent:
               resultReg = inlineBigDecimalUnaryOp(node, cg, TR::InstOpCode::EEDTR);
               return true;
            case TR::java_math_BigDecimal_DFPCompareTo:
               resultReg = inlineBigDecimalCompareTo(node, cg);
               return true;
            case TR::java_math_BigDecimal_DFPBCDDigits:
               resultReg = inlineBigDecimalUnaryOp(node, cg, TR::InstOpCode::CUDTR);
               return true;
            case TR::java_math_BigDecimal_DFPUnscaledValue:
               resultReg = inlineBigDecimalUnscaledValue(node, cg);
               return true;
            case TR::java_math_BigDecimal_DFPSetScale:
               resultReg = inlineBigDecimalSetScale(node, cg);
               return true;
            case TR::java_math_BigDecimal_DFPDivide:
               resultReg = inlineBigDecimalDivide(node, cg);
               return true;
            case TR::java_math_BigDecimal_DFPConvertPackedToDFP:
            case TR::com_ibm_dataaccess_DecimalData_DFPConvertPackedToDFP:
               resultReg = inlineBigDecimalFromPackedConverter(node, cg);
               return true;
            case TR::java_math_BigDecimal_DFPConvertDFPToPacked:
            case TR::com_ibm_dataaccess_DecimalData_DFPConvertDFPToPacked:
               resultReg = inlineBigDecimalToPackedConverter(node, cg);
               return true;
            default:
               break;
            }
         }
      }

   TR::MethodSymbol * symbol = node->getSymbol()->castToMethodSymbol();
   if ((symbol->isVMInternalNative() || symbol->isJITInternalNative()) || isKnownMethod(methodSymbol))
      {
      if (TR::TreeEvaluator::VMinlineCallEvaluator(node, false, cg))
         {
         resultReg = node->getRegister();
         return true;
         }
      }

   // No method specialization was done.
   //
   resultReg = NULL;
   return false;
   }

/**
* Check if arithmetic operations with a constant requires entry in the literal pool.
*/
bool
J9::Z::CodeGenerator::arithmeticNeedsLiteralFromPool(TR::Node *node)
   {
   int64_t value = getIntegralValue(node);
   return value > GE_MAX_IMMEDIATE_VAL || value < GE_MIN_IMMEDIATE_VAL;
   }


bool
J9::Z::CodeGenerator::supportsTrapsInTMRegion()
   {
   return self()->comp()->target().isZOS();
   }

