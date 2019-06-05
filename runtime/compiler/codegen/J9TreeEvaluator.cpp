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

#include "codegen/TreeEvaluator.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/J9WatchedInstanceFieldSnippet.hpp"
#include "codegen/J9WatchedStaticFieldSnippet.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/VMJ9.h"
#include "il/symbol/StaticSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "runtime/RuntimeAssumptions.hpp"
#include "runtime/J9Profiler.hpp"
#include "runtime/J9ValueProfiler.hpp"
#include "util_api.h"

void
J9::TreeEvaluator::rdWrtbarHelperForFieldWatch(TR::Node *node, TR::CodeGenerator *cg, TR::Register *sideEffectRegister, TR::Register *valueReg)
   {
   bool isWrite = node->getOpCode().isWrtBar();

   TR_ASSERT_FATAL(J9ClassHasWatchedFields >= std::numeric_limits<uint16_t>::min() && J9ClassHasWatchedFields <= std::numeric_limits<uint16_t>::max(), "Expecting value of J9ClassHasWatchedFields to be within 16 bits. Currently it's %d(%p).", J9ClassHasWatchedFields, J9ClassHasWatchedFields);
   // Populate a data snippet with the required information so we can call a VM helper to report the fieldwatch event.
   J9Method *owningMethod = reinterpret_cast<J9Method *>(node->getOwningMethod());
   int32_t bcIndex = node->getByteCodeInfo().getByteCodeIndex();
   TR::SymbolReference *symRef = node->getSymbolReference();
   bool isUnresolved = symRef->isUnresolved();
   TR::Snippet *dataSnippet = NULL;
   if (symRef->getSymbol()->isStatic())
      {
      void *fieldAddress = isUnresolved ? reinterpret_cast<void *>(-1) : symRef->getSymbol()->getStaticSymbol()->getStaticAddress();
      J9Class *fieldClass = isUnresolved ? NULL : reinterpret_cast<J9Class *>(symRef->getOwningMethod(cg->comp())->getDeclaringClassFromFieldOrStatic(cg->comp(), symRef->getCPIndex()));
      dataSnippet = new (cg->trHeapMemory()) TR::J9WatchedStaticFieldSnippet (cg, node, owningMethod, bcIndex, fieldAddress, fieldClass);
      }
   else
      {
      dataSnippet = new (cg->trHeapMemory()) TR::J9WatchedInstanceFieldSnippet(cg, node, owningMethod, bcIndex, isUnresolved ? -1 : symRef->getOffset() - TR::Compiler->om.objectHeaderSizeInBytes());
      }
   cg->addSnippet(dataSnippet);

   // If unresolved, then we generate instructions to populate the data snippet's fields correctly at runtime.
   // Note: We also call the VM Helper routine to fill in the data snippet's fields if this is an AOT compilation.
   // Once the infrastructure to support AOT during fieldwatch is enabled and functionally correct, we can remove is check.
   if (node->getSymbolReference()->isUnresolved() || cg->comp()->compileRelocatableCode() /* isAOTCompile */)
      {
      // Resolve and populate dataSnippet fields.
      TR::TreeEvaluator::generateFillInDataBlockSequenceForUnresolvedField(cg, node, dataSnippet, isWrite, sideEffectRegister);
      }
   // Generate instructions to call the VM helper and report the fieldwatch event.
   TR::TreeEvaluator::generateTestAndReportFieldWatchInstructions(cg, node, dataSnippet, isWrite, sideEffectRegister, valueReg);
   }

TR::Register *
J9::TreeEvaluator::bwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Register *valueReg = cg->evaluate(node->getFirstChild());
   TR::Node *sideEffectNode = node->getSecondChild();
   TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, valueReg);
      }

   // Note: The reference count for valueReg's node is not decremented here because the 
   // store evaluator also uses it and so it will evaluate+decrement it. Thus we must skip decrementing here
   // to avoid double decrementing.
   cg->decReferenceCount(sideEffectNode);
   return TR::TreeEvaluator::bstoreEvaluator(node, cg);
   }

TR::Register *
J9::TreeEvaluator::bwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Register *valueReg = cg->evaluate(node->getSecondChild());
   TR::Node *sideEffectNode = node->getThirdChild();
   TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, valueReg);
      }

   // Note: The reference count for valueReg's node is not decremented here because the 
   // store evaluator also uses it and so it will evaluate+decrement it. Thus we must skip decrementing here
   // to avoid double decrementing.
   cg->decReferenceCount(sideEffectNode);
   return TR::TreeEvaluator::bstoreEvaluator(node, cg);
   }

TR::Register *
J9::TreeEvaluator::swrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Register *valueReg = cg->evaluate(node->getFirstChild());
   TR::Node *sideEffectNode = node->getSecondChild();
   TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, valueReg);
      }

   // Note: The reference count for valueReg's node is not decremented here because the 
   // store evaluator also uses it and so it will evaluate+decrement it. Thus we must skip decrementing here
   // to avoid double decrementing.
   cg->decReferenceCount(sideEffectNode);
   return TR::TreeEvaluator::sstoreEvaluator(node, cg);
   }

TR::Register *
J9::TreeEvaluator::swrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Register *valueReg = cg->evaluate(node->getSecondChild());
   TR::Node *sideEffectNode = node->getThirdChild();
   TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, valueReg);
      }

   // Note: The reference count for valueReg's node is not decremented here because the 
   // store evaluator also uses it and so it will evaluate+decrement it. Thus we must skip decrementing here
   // to avoid double decrementing.
   cg->decReferenceCount(sideEffectNode);
   return TR::TreeEvaluator::sstoreEvaluator(node, cg);
   }

TR::Register *
J9::TreeEvaluator::iwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Register *valueReg = cg->evaluate(node->getFirstChild());
   TR::Node *sideEffectNode = node->getSecondChild();
   TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, valueReg);
      }

   // Note: The reference count for valueReg's node is not decremented here because the 
   // store evaluator also uses it and so it will evaluate+decrement it. Thus we must skip decrementing here
   // to avoid double decrementing.
   cg->decReferenceCount(sideEffectNode);
   return TR::TreeEvaluator::istoreEvaluator(node, cg);
   }

TR::Register *
J9::TreeEvaluator::iwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Register *valueReg = cg->evaluate(node->getSecondChild());
   TR::Node *sideEffectNode = node->getThirdChild();
   TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, valueReg);
      }

   // Note: The reference count for valueReg's node is not decremented here because the 
   // store evaluator also uses it and so it will evaluate+decrement it. Thus we must skip decrementing here
   // to avoid double decrementing.
   cg->decReferenceCount(sideEffectNode);
   return TR::TreeEvaluator::istoreEvaluator(node, cg);
   }

TR::Register *
J9::TreeEvaluator::lwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Register *valueReg = cg->evaluate(node->getFirstChild());
   TR::Node *sideEffectNode = node->getSecondChild();
   TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, valueReg);
      }

   // Note: The reference count for valueReg's node is not decremented here because the 
   // store evaluator also uses it and so it will evaluate+decrement it. Thus we must skip decrementing here
   // to avoid double decrementing.
   cg->decReferenceCount(sideEffectNode);
   return TR::TreeEvaluator::lstoreEvaluator(node, cg);
   }

TR::Register *
J9::TreeEvaluator::lwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Register *valueReg = cg->evaluate(node->getSecondChild());
   TR::Node *sideEffectNode = node->getThirdChild();
   TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, valueReg);
      }

   // Note: The reference count for valueReg's node is not decremented here because the 
   // store evaluator also uses it and so it will evaluate+decrement it. Thus we must skip decrementing here
   // to avoid double decrementing.
   cg->decReferenceCount(sideEffectNode);
   return TR::TreeEvaluator::lstoreEvaluator(node, cg);
   }

TR::Register *
J9::TreeEvaluator::frdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Node *sideEffectNode = node->getFirstChild();
   TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, NULL);
      }

   cg->decReferenceCount(sideEffectNode);
   return TR::TreeEvaluator::floadEvaluator(node, cg);
   } 

TR::Register *
J9::TreeEvaluator::frdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Register *sideEffectRegister = cg->evaluate(node->getFirstChild());

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, NULL);
      }
   // Note: For indirect rdbar nodes, the first child (sideEffectNode) is also used by the 
   // load evaluator. The load evaluator will also evaluate+decrement it. In order to avoid double
   // decrementing the node we skip doing it here and let the load evaluator do it.
   return TR::TreeEvaluator::floadEvaluator(node, cg);
   }

TR::Register *
J9::TreeEvaluator::drdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Node *sideEffectNode = node->getFirstChild();
   TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, NULL);
      }

   cg->decReferenceCount(sideEffectNode);
   return TR::TreeEvaluator::dloadEvaluator(node, cg);
   }

TR::Register *
J9::TreeEvaluator::drdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Register *sideEffectRegister = cg->evaluate(node->getFirstChild());

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, NULL);
      }

   // Note: For indirect rdbar nodes, the first child (sideEffectNode) is also used by the 
   // load evaluator. The load evaluator will also evaluate+decrement it. In order to avoid double
   // decrementing the node we skip doing it here and let the load evaluator do it.
   return TR::TreeEvaluator::dloadEvaluator(node, cg);
   }

TR::Register *
J9::TreeEvaluator::brdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Node *sideEffectNode = node->getFirstChild();
   TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, NULL);
      }

   cg->decReferenceCount(sideEffectNode);
   return TR::TreeEvaluator::bloadEvaluator(node, cg);
   }

TR::Register *
J9::TreeEvaluator::brdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Register *sideEffectRegister = cg->evaluate(node->getFirstChild());

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, NULL);
      }

   // Note: For indirect rdbar nodes, the first child (sideEffectNode) is also used by the 
   // load evaluator. The load evaluator will also evaluate+decrement it. In order to avoid double
   // decrementing the node we skip doing it here and let the load evaluator do it.
   return TR::TreeEvaluator::bloadEvaluator(node, cg);
   }

TR::Register *
J9::TreeEvaluator::srdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Node *sideEffectNode = node->getFirstChild();
   TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, NULL);
      }

   cg->decReferenceCount(sideEffectNode);
   return TR::TreeEvaluator::sloadEvaluator(node, cg);
   }

TR::Register *
J9::TreeEvaluator::srdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Register *sideEffectRegister = cg->evaluate(node->getFirstChild());

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, NULL);
      }

   // Note: For indirect rdbar nodes, the first child (sideEffectNode) is also used by the 
   // load evaluator. The load evaluator will also evaluate+decrement it. In order to avoid double
   // decrementing the node we skip doing it here and let the load evaluator do it.
   return TR::TreeEvaluator::sloadEvaluator(node, cg);
   }

TR::Register *
J9::TreeEvaluator::lrdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Node *sideEffectNode = node->getFirstChild();
   TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, NULL);
      }

   cg->decReferenceCount(sideEffectNode);
   return TR::TreeEvaluator::lloadEvaluator(node, cg);
   }

TR::Register *
J9::TreeEvaluator::lrdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Register *sideEffectRegister = cg->evaluate(node->getFirstChild());

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, NULL);
      }

   // Note: For indirect rdbar nodes, the first child (sideEffectNode) is also used by the 
   // load evaluator. The load evaluator will also evaluate+decrement it. In order to avoid double
   // decrementing the node we skip doing it here and let the load evaluator do it.
   return TR::TreeEvaluator::lloadEvaluator(node, cg);
   }

bool J9::TreeEvaluator::getIndirectWrtbarValueNode(TR::CodeGenerator *cg, TR::Node *node, TR::Node*& sourceChild, bool incSrcRefCount)
   {
   TR_ASSERT_FATAL(node->getOpCode().isIndirect() && node->getOpCode().isWrtBar(), "getIndirectWrtbarValueNode expects indirect wrtbar nodes only n%dn (%p)\n", node->getGlobalIndex(), node);
   bool usingCompressedPointers = false;
   sourceChild = node->getSecondChild();

   if (cg->comp()->useCompressedPointers() && (node->getSymbolReference()->getSymbol()->getDataType() == TR::Address) &&
         (node->getSecondChild()->getDataType() != TR::Address))
      {
      // pattern match the sequence
      //     awrtbari f     awrtbari f         <- node
      //       aload O       aload O
      //     value           l2i
      //                       lshr
      //                         lsub        <- translatedNode
      //                           a2l
      //                             value   <- sourceChild
      //                           lconst HB
      //                         iconst shftKonst
      //
      // -or- if the field is known to be null
      // awrtbari f
      //    aload O
      //    l2i
      //      a2l
      //        value  <- sourceChild
      //
      TR::Node *translatedNode = sourceChild;
      if (translatedNode->getOpCodeValue() == TR::l2i)
         {
         translatedNode = translatedNode->getFirstChild();
         }
      if (translatedNode->getOpCode().isRightShift())
         {
         TR::Node *shiftAmountChild = translatedNode->getSecondChild();
         TR_ASSERT_FATAL(TR::Compiler->om.compressedReferenceShiftOffset() == shiftAmountChild->getConstValue(),
                "Expect shift amount in the compressedref conversion sequence to be %d but get %d for indirect wrtbar node n%dn (%p)\n",
                TR::Compiler->om.compressedReferenceShiftOffset(), shiftAmountChild->getConstValue(), node->getGlobalIndex(), node);

         translatedNode = translatedNode->getFirstChild();
         }

      if (translatedNode->getOpCode().isSub() ||
           TR::Compiler->vm.heapBaseAddress() == 0 || sourceChild->isNull()) /* i.e. usingLowMemHeap */
         {
         usingCompressedPointers = true;
         }

      if (usingCompressedPointers)
         {
         while ((sourceChild->getNumChildren() > 0) && (sourceChild->getOpCodeValue() != TR::a2l))
            {
            sourceChild = sourceChild->getFirstChild();
            }
         if (sourceChild->getOpCodeValue() == TR::a2l)
            {
            sourceChild = sourceChild->getFirstChild();
            }

         // Artificially bump up the refCount on the value so
         // that different registers are allocated for the actual
         // and compressed values. This is done so that the VMwrtbarEvaluator
         // uses the uncompressed value. We only need to do this when the caller
         // is evaluating the actual write barrier.
         if (incSrcRefCount)
            {
            sourceChild->incReferenceCount();
            }
         }
      }
   return usingCompressedPointers;
   }

static
void traceInstanceOfOrCheckCastProfilingInfo(TR::CodeGenerator *cg, TR::Node *node, TR_OpaqueClassBlock *castClass)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR_ByteCodeInfo bcInfo = node->getByteCodeInfo();
   TR_ValueProfileInfoManager *valueProfileInfo = TR_ValueProfileInfoManager::get(comp);

   if (!valueProfileInfo)
      {
      return;
      }

   TR_AddressInfo * valueInfo = static_cast<TR_AddressInfo*>(valueProfileInfo->getValueInfo(bcInfo, comp, AddressInfo, TR_ValueProfileInfoManager::justInterpreterProfileInfo));
   if (!valueInfo || valueInfo->getNumProfiledValues() == 0)
      {
      return;
      }

   traceMsg(comp, "%s:\n", __func__);

   TR_ScratchList<TR_ExtraAddressInfo> valuesSortedByFrequency(comp->trMemory());

   valueInfo->getSortedList(comp, &valuesSortedByFrequency);

   ListIterator<TR_ExtraAddressInfo> sortedValuesIt(&valuesSortedByFrequency);
   for (TR_ExtraAddressInfo *profiledInfo = sortedValuesIt.getFirst(); profiledInfo != NULL; profiledInfo = sortedValuesIt.getNext())
      {
      TR_OpaqueClassBlock *profiledClass = fej9->getProfiledClassFromProfiledInfo(profiledInfo);

      traceMsg(comp, "%s:\tProfiled class [" POINTER_PRINTF_FORMAT "] (%u/%u)\n",
               node->getOpCode().getName(), profiledClass, profiledInfo->_frequency, valueInfo->getTotalFrequency());

      if (!profiledClass)
         continue;

      if (comp->getPersistentInfo()->isObsoleteClass(profiledClass, fej9))
         {
         traceMsg(comp, "%s:\tProfiled class [" POINTER_PRINTF_FORMAT "] is obsolete\n",
                  node->getOpCode().getName(), profiledClass);
         continue;
         }

      bool isInstanceOf = instanceOfOrCheckCastNoCacheUpdate((J9Class *)profiledClass, (J9Class *)castClass);
      traceMsg(comp, "%s:\tProfiled class [" POINTER_PRINTF_FORMAT "] is %san instance of cast class\n",
               node->getOpCode().getName(), profiledClass, isInstanceOf ? "" : "not ");
      }
   }
/**   \brief Generates an array of profiled classes with the boolean representing if the profiled class is instanceOf cast class or not
 **   \param profiledClassList 
 **      An array of InstanceOfOrCheckCasrProfiledClasses structure passed from the main evaluator to fill up with profiled classes info
 **   \param topClassProbability
 **      float pointer passed from main evaluator, we update the value with the probability for the top profiled class to be castClass
 **   \param maxProfiledClass
 **      An int denoting how many profiled classes we want
 **/
static
uint32_t getInstanceOfOrCheckCastTopProfiledClass(TR::CodeGenerator *cg, TR::Node *node, TR_OpaqueClassBlock *castClass, J9::TreeEvaluator::InstanceOfOrCheckCastProfiledClasses *profiledClassList, bool *topClassWasCastClass, uint32_t maxProfiledClass, float *topClassProbability)
   {
   TR::Compilation *comp = cg->comp();

   if (comp->getOption(TR_TraceCG))
      {
      static bool traceProfilingInfo = feGetEnv("TR_traceInstanceOfOrCheckCastProfilingInfo") != NULL;
      if (traceProfilingInfo)
         traceInstanceOfOrCheckCastProfilingInfo(cg, node, castClass);
      }

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR_ByteCodeInfo bcInfo = node->getByteCodeInfo();
   TR_ValueProfileInfoManager *valueProfileInfo = TR_ValueProfileInfoManager::get(comp);

   if (!valueProfileInfo)
      {
      return 0;
      }

   TR_AddressInfo * valueInfo = static_cast<TR_AddressInfo*>(valueProfileInfo->getValueInfo(bcInfo, comp, AddressInfo, TR_ValueProfileInfoManager::justInterpreterProfileInfo));
   if (!valueInfo || valueInfo->getNumProfiledValues() == 0)
      {
      return 0;
      }

   if (topClassWasCastClass)
      *topClassWasCastClass = false;

   TR_ScratchList<TR_ExtraAddressInfo> valuesSortedByFrequency(comp->trMemory());

   valueInfo->getSortedList(comp, &valuesSortedByFrequency);

   float totalFrequency = valueInfo->getTotalFrequency();

   ListIterator<TR_ExtraAddressInfo> sortedValuesIt(&valuesSortedByFrequency);
   uint32_t numProfiledClasses = 0;
   for (TR_ExtraAddressInfo *profiledInfo = sortedValuesIt.getFirst(); profiledInfo != NULL && numProfiledClasses < maxProfiledClass ; profiledInfo = sortedValuesIt.getNext())
      {
      TR_OpaqueClassBlock *tempProfiledClass = fej9->getProfiledClassFromProfiledInfo(profiledInfo);

      if (!tempProfiledClass)
         continue;
      
      // Skip unloaded classes.
      //
      if (comp->getPersistentInfo()->isObsoleteClass(tempProfiledClass, fej9))
         {
         if (comp->getOption(TR_TraceCG))
            {
            traceMsg(comp, "%s: Profiled class [" POINTER_PRINTF_FORMAT "] is obsolete, skipping\n",
                     node->getOpCode().getName(), tempProfiledClass);
            }
         continue;
         }

      // For checkcast, skip classes that will fail the cast, not much value in optimizing for those cases.
      // We also don't want to pollute the cast class cache with a failing class for the same reason.
      //
      bool isInstanceOf = instanceOfOrCheckCastNoCacheUpdate((J9Class *)tempProfiledClass, (J9Class *)castClass);
      if (node->getOpCode().isCheckCast() && !isInstanceOf)
         {
         if (comp->getOption(TR_TraceCG))
            {
            traceMsg(comp, "%s: Profiled class [" POINTER_PRINTF_FORMAT "] is not an instance of cast class, skipping\n",
                     node->getOpCode().getName(), tempProfiledClass);
            }
         continue;
         }

      // If the cast class is the top class skip it and return the next highest class if the caller requested it by providing an output param.
      //
      if (tempProfiledClass == castClass && topClassWasCastClass && numProfiledClasses == 0)
         {
         if (comp->getOption(TR_TraceCG))
            {
            traceMsg(comp, "%s: Profiled class [" POINTER_PRINTF_FORMAT "] is the cast class, informing caller and skipping\n",
                     node->getOpCode().getName(), tempProfiledClass);
            }
         *topClassWasCastClass = true;
         *topClassProbability = profiledInfo->_frequency / totalFrequency;
         continue;
         }

      // For AOT compiles with a SymbolValidationManager, skip any classes which cannot be verified.
      //
      if (comp->compileRelocatableCode() && comp->getOption(TR_UseSymbolValidationManager))
         if (!comp->getSymbolValidationManager()->addProfiledClassRecord(tempProfiledClass))
            continue;

      float frequency = profiledInfo->_frequency / totalFrequency;
      if ( frequency >= TR::Options::getMinProfiledCheckcastFrequency() )
         {
         // We have a winner.
         //
         profiledClassList[numProfiledClasses].profiledClass = tempProfiledClass;
         profiledClassList[numProfiledClasses].isProfiledClassInstanceOfCastClass = isInstanceOf;
         profiledClassList[numProfiledClasses].frequency = frequency;
         numProfiledClasses++;
         }
      else
         {
         // Profiled Class is sorted by frequency so if the frequency is less than the Minimum don't bother with generating another profiled Class
         break;
         }
      }
   return numProfiledClasses;
   }

/**   \brief Generates an array of profiled classes with the boolean representing if the profiled class is instanceOf cast class or not
 **   \param profiledClassList 
 **      An array of InstanceOfOrCheckCasrProfiledClasses structure passed from the main evaluator to fill up with profiled classes info
 **   \param numberOfProfiledClass
 **      An int pointer passed from main evaluator, we update the value with the number of classes we get from profilef info
 **   \param maxProfiledClass
 **      An int denoting how many profiled classes we want
 **   \param topClassProbability
 **      Probability of having topClass to be castClass. 
 **   \param topClassWasCastClass
 **      Boolean pointer which will be set to true or false depending on if top profiled class was cast class or not
 **/
uint32_t J9::TreeEvaluator::calculateInstanceOfOrCheckCastSequences(TR::Node *instanceOfOrCheckCastNode, InstanceOfOrCheckCastSequences *sequences, TR_OpaqueClassBlock **compileTimeGuessClass, TR::CodeGenerator *cg, InstanceOfOrCheckCastProfiledClasses *profiledClassList, uint32_t *numberOfProfiledClass, uint32_t maxProfiledClass, float * topClassProbability, bool *topClassWasCastClass)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR_ASSERT(instanceOfOrCheckCastNode->getOpCode().isCheckCast() || instanceOfOrCheckCastNode->getOpCodeValue() == TR::instanceof, "Unexpected node opcode");

   TR::Node *objectNode = instanceOfOrCheckCastNode->getFirstChild();
   TR::Node *castClassNode = instanceOfOrCheckCastNode->getSecondChild();

   bool isInstanceOf = instanceOfOrCheckCastNode->getOpCodeValue() == TR::instanceof;
   bool mayBeNull = !instanceOfOrCheckCastNode->isReferenceNonNull() && !objectNode->isNonNull();

   // By default maxOnsiteCacheSlotForInstanceOf is set to 0 which means cache is disable.
   // To enable test pass JIT option maxOnsiteCacheSlotForInstanceOf=<number_of_slots>
   bool createDynamicCacheTests = cg->comp()->getOptions()->getMaxOnsiteCacheSlotForInstanceOf() > 0;


   uint32_t i = 0;
   uint32_t numProfiledClasses = 0;

   if (castClassNode->getReferenceCount() > 1)
      {
      sequences[i++] = EvaluateCastClass;
      }

   TR::SymbolReference *castClassSymRef = castClassNode->getSymbolReference();

   // Object is known to be null, usually removed by the optimizer, but in case they're not we know the result of the cast/instanceof.
   //
   if (objectNode->isNull())
      {
      sequences[i++] = isInstanceOf ? GoToFalse : GoToTrue;
      }
   // Cast class is unresolved, not a lot of room to be fancy here.
   //
   else if (castClassSymRef->isUnresolved())
      {
      if (cg->comp()->getOption(TR_TraceCG))
         traceMsg(cg->comp(),"Cast Class unresolved\n");
      if (mayBeNull)
         sequences[i++] = NullTest;
      sequences[i++] = ClassEqualityTest;
      // There is a possibility of attempt to cast object to another class and having cache on that object updated by helper.
      // Before going to helper checking the cache.
      sequences[i++] = CastClassCacheTest;
      if (createDynamicCacheTests)
         sequences[i++] = DynamicCacheObjectClassTest;
      sequences[i++] = HelperCall;
      }
   // Cast class is a runtime variable, still not a lot of room to be fancy.
   //
   else if (!OMR::TreeEvaluator::isStaticClassSymRef(castClassSymRef))
      {
      traceMsg(cg->comp(),"Cast Class runtimeVariable\n");
      TR_ASSERT(isInstanceOf, "Expecting instanceof when cast class is a runtime variable");
      if (mayBeNull)
         sequences[i++] = NullTest;
      sequences[i++] = ClassEqualityTest;
      sequences[i++] = CastClassCacheTest;
      // On Z, We were having support for Super Class Test for dynamic Cast Class so adding it here. It can be guarded if Power/X do not need it.
      if (cg->supportsInliningOfIsInstance() && 
         instanceOfOrCheckCastNode->getOpCodeValue() == TR::instanceof && 
         instanceOfOrCheckCastNode->getSecondChild()->getOpCodeValue() != TR::loadaddr)
         sequences[i++] = SuperClassTest;
      if (createDynamicCacheTests)
         sequences[i++] = DynamicCacheDynamicCastClassTest;
      sequences[i++] = HelperCall;
      }

   // Cast class is a compile-time constant, we can generate better code in this case.
   //
   else
      {

      TR::StaticSymbol   *castClassSym = castClassSymRef->getSymbol()->getStaticSymbol();
      TR_OpaqueClassBlock *castClass = (TR_OpaqueClassBlock *)castClassSym->getStaticAddress();
      J9UTF8              *castClassName = J9ROMCLASS_CLASSNAME(((J9Class *)castClass)->romClass);
      // Cast class is a primitive (implies this is an instanceof, since you can't cast an object to a primitive).
      // Usually removed by the optimizer, but in case they're not we know they'll always fail, no object can be of a primitive type.
      // We don't even need to do a null test unless it's a checkcastAndNULLCHK node.
      //
      if (TR::Compiler->cls.isPrimitiveClass(cg->comp(), castClass))
         {
         TR_ASSERT(isInstanceOf, "Expecting instanceof when cast class is a primitive");
         if (instanceOfOrCheckCastNode->getOpCodeValue() == TR::checkcastAndNULLCHK)
            sequences[i++] = NullTest;
         sequences[i++] = GoToFalse;
         }
      // Cast class is java/lang/Object.
      // Usually removed by the optimizer, if not we know everything is an Object, instanceof on null is the only thing that needs to be checked.
      //
      else if (cg->comp()->getObjectClassPointer() == castClass)
         {
         if (isInstanceOf)
            {
            if (mayBeNull)
               sequences[i++] = NullTest;
            sequences[i++] = GoToTrue;
            }
         else
            {
            if (instanceOfOrCheckCastNode->getOpCodeValue() == TR::checkcastAndNULLCHK)
               sequences[i++] = NullTest;
            sequences[i++] = GoToTrue;
            }
         }
      else
         {
         if (mayBeNull)
            {
            sequences[i++] = NullTest;
            }

         TR_OpaqueClassBlock *topProfiledClass = NULL;

         // If the caller doesn't provide the output param don't bother with profiling.
         //
         if (profiledClassList)
            {
            *numberOfProfiledClass = getInstanceOfOrCheckCastTopProfiledClass(cg, instanceOfOrCheckCastNode, castClass, profiledClassList, topClassWasCastClass, maxProfiledClass, topClassProbability);
            numProfiledClasses = *numberOfProfiledClass;
            if (cg->comp()->getOption(TR_TraceCG))
               {
               for (int i=0; i<numProfiledClasses; i++)
                  {
                  J9UTF8 *profiledClassName = J9ROMCLASS_CLASSNAME(((J9Class *)profiledClassList[i].profiledClass)->romClass);
                  traceMsg(cg->comp(), "%s:Interpreter profiling instance class: [" POINTER_PRINTF_FORMAT "] %.*s, probability=%.1f\n",
                     instanceOfOrCheckCastNode->getOpCode().getName(), profiledClassList[i].profiledClass, J9UTF8_LENGTH(profiledClassName), J9UTF8_DATA(profiledClassName), profiledClassList[i].frequency);
                  }
               }
            }

         TR_OpaqueClassBlock *singleImplementerClass = NULL;

         // If the caller doesn't provide the output param don't bother with guessing.
         //
         if (compileTimeGuessClass && (TR::Compiler->cls.isInterfaceClass(cg->comp(), castClass) || TR::Compiler->cls.isAbstractClass(cg->comp(), castClass)))
            {
            // Figuring out that an interface/abstract class has a single concrete implementation is not as useful for instanceof as it is for checkcast.
            // For checkcast we expect the cast to succeed and the single concrete implementation is the logical class to do a quick up front test against.
            // For instanceof false can be a common result and the single concrete implementation doesn't tell us anything special.
            //
            if (!isInstanceOf)
               {
               singleImplementerClass = cg->comp()->getPersistentInfo()->getPersistentCHTable()->findSingleConcreteSubClass(castClass, cg->comp());
               if (cg->comp()->getOption(TR_TraceCG))
                  {
                  if (singleImplementerClass)
                     {
                     J9UTF8 *singleImplementerClassName = J9ROMCLASS_CLASSNAME(((J9Class *)singleImplementerClass)->romClass);
                     traceMsg(cg->comp(), "%s: Single implementer for interface/abstract class: [" POINTER_PRINTF_FORMAT "] %.*s\n",
                              instanceOfOrCheckCastNode->getOpCode().getName(), singleImplementerClass, J9UTF8_LENGTH(singleImplementerClassName), J9UTF8_DATA(singleImplementerClassName));
                     }
                  }
               }
            *compileTimeGuessClass = singleImplementerClass;
            }

         if (TR::Compiler->cls.isClassArray(cg->comp(), castClass))
            {
            if (TR::Compiler->cls.isReferenceArray(cg->comp(), castClass))
               {
               TR_OpaqueClassBlock *componentClass = fej9->getComponentClassFromArrayClass(castClass);
               TR_OpaqueClassBlock *leafClass = fej9->getLeafComponentClassFromArrayClass(castClass);

               // Cast class is a single dim array of java/lang/Object, all we need to do is check if the object is an array of non-primitives.
               //
               if (cg->comp()->getObjectClassPointer() == componentClass && componentClass == leafClass)
                  {
                  sequences[i++] = ArrayOfJavaLangObjectTest;
                  sequences[i++] = GoToFalse;
                  }
               // Cast class is a single dim array of a final class, all we need to do is check if the object is of this type, anything else is false.
               //
               else if (fej9->isClassFinal(componentClass) && componentClass == leafClass)
                  {
                  sequences[i++] = ClassEqualityTest;
                  sequences[i++] = GoToFalse;
                  }
               // Cast class is an array of some non-final class or multiple dimensions.
               //
               else
                  {
                  if (numProfiledClasses  > 0)
                     {
                     if (!*topClassWasCastClass)
                        {
                        sequences[i++] = ProfiledClassTest;
                        sequences[i++] = CastClassCacheTest;
                        }
                     else
                        {
                        sequences[i++] = ClassEqualityTest;
                        sequences[i++] = ProfiledClassTest;
                        sequences[i++] = CastClassCacheTest;
                        }
                     }
                  else
                     {
                     sequences[i++] = ClassEqualityTest;
                     sequences[i++] = CastClassCacheTest;
                     }

                  sequences[i++] = HelperCall;
                  }
               }
            // Cast class is an array of some primitive, all we need to do is check if the object is of this type, anything else is false.
            //
            else
               {
               sequences[i++] = ClassEqualityTest;
               sequences[i++] = GoToFalse;
               }
            }
         // Cast class is an interface, we can skip the class equality test.
         //
         else if (TR::Compiler->cls.isInterfaceClass(cg->comp(), castClass))
            {
            if (singleImplementerClass)
               {
               sequences[i++] = CompileTimeGuessClassTest;
               }
            else if (numProfiledClasses  > 0)
               {
               sequences[i++] = ProfiledClassTest;
               sequences[i++] = CastClassCacheTest;
               }
            else
               {
               sequences[i++] = CastClassCacheTest;
               }
            if (createDynamicCacheTests)
               sequences[i++] = DynamicCacheObjectClassTest;
            sequences[i++] = HelperCall;
            }
         // Cast class is an abstract class, we can skip the class equality test, a superclass test is enough.
         //
         else if (fej9->isAbstractClass(castClass))
            {
            // Don't bother with the cast class cache, it's not updated by the VM when the cast can be determined via a superclass test.
            //
            if (singleImplementerClass)
               sequences[i++] = CompileTimeGuessClassTest;
            else if (numProfiledClasses  > 0)
               sequences[i++] = ProfiledClassTest;

            sequences[i++] = SuperClassTest;
            sequences[i++] = GoToFalse;
            }
         // Cast class is a final class, all we need to do is check if the object is of this type, anything else is false.
         //
         else if (fej9->isClassFinal(castClass))
            {
            sequences[i++] = ClassEqualityTest;
            sequences[i++] = GoToFalse;
            }
         // Arbitrary concrete class.
         //
         else
            {
            // Don't bother with the cast class cache, it's not updated by the VM when the cast can be determined via a superclass test.
            //
            if (numProfiledClasses  > 0)
               {
               if (!*topClassWasCastClass)
                  {
                  sequences[i++] = ProfiledClassTest;
                  sequences[i++] = ClassEqualityTest;
                  }
               else
                  {
                  sequences[i++] = ClassEqualityTest;
                  sequences[i++] = ProfiledClassTest;
                  }
               }
            else
               {
               sequences[i++] = ClassEqualityTest;
               }

            sequences[i++] = SuperClassTest;
            sequences[i++] = GoToFalse;
            }
         }
      }

   // Insert LoadObjectClass and/or EvaluateCastClass where required.
   //
   bool objectClassLoaded = false;
   bool castClassEvaluated = false;

   for (uint32_t j = 0; j < i; ++j)
      {
      InstanceOfOrCheckCastSequences s = sequences[j];
      if (s == EvaluateCastClass)
         {
         castClassEvaluated = true;
         }
      else if (s == LoadObjectClass)
         {
         objectClassLoaded = true;
         }
      else
         {
         if (s == ProfiledClassTest ||
             s == CompileTimeGuessClassTest ||
             s == ArrayOfJavaLangObjectTest ||
             s == ClassEqualityTest ||
             s == SuperClassTest ||
             s == CastClassCacheTest)
            {
            if (!objectClassLoaded)
               {
               memmove(&sequences[j + 1], &sequences[j], (i - j) * sizeof(InstanceOfOrCheckCastSequences));
               sequences[j] = LoadObjectClass;
               i += 1;
               objectClassLoaded = true;
               }
            }
         // Even though the helper call also needs the cast class, we expect to generate the helper call out of line
         // and can load it there. Here we just want to know whether the cast class is needed in the main line sequence.
         //
         if (s == ClassEqualityTest ||
             s == SuperClassTest ||
             s == CastClassCacheTest)
            {
            if (!castClassEvaluated)
               {
               // Ideally we would insert EvaluateCastClass at sequences[j], however
               // if the cast class needs to be evaluated it needs to be done before carrying out any tests
               // in order to prevent evaluation inside internal control flow. Therefore,
               // we insert it at the front of the list rather than at 'j'.
               const uint32_t insertAt = 0;
               memmove(&sequences[insertAt + 1], &sequences[insertAt], (i - insertAt) * sizeof(InstanceOfOrCheckCastSequences));
               sequences[insertAt] = EvaluateCastClass;
               i += 1;
               castClassEvaluated = true;
               }
            }
         }
      }

   TR_ASSERT(sequences[i - 1] == HelperCall ||
             sequences[i - 1] == GoToTrue ||
             sequences[i - 1] == GoToFalse,
             "Expecting last sequence to be a terminal sequence (that provides a definitive answer)");

   return i;
   }

/*
 * if recordAll is true, record all result in classArray, skip MinProfiledCheckcastFrequency check
 * if probability is not null, record class' corresponding probability in this array. Used for cost/benefit analysis for profiled check.
 */
uint8_t
J9::TreeEvaluator::interpreterProfilingInstanceOfOrCheckCastInfo(
      TR::CodeGenerator * cg,
      TR::Node * node,
      TR_OpaqueClassBlock **classArray,
      float* probability,
      bool recordAll)
   {
   TR_ByteCodeInfo bcInfo = node->getByteCodeInfo();
   TR::Compilation *comp = cg->comp();
   TR_ValueProfileInfoManager * valueProfileInfo = TR_ValueProfileInfoManager::get(comp);
   static const char *p = feGetEnv("TR_TracePIC");
   bool p1 = p && comp->getOption(TR_TraceCG); // allow per method tracing

   if (!valueProfileInfo)
      return 0;

   TR_AddressInfo * valueInfo = static_cast<TR_AddressInfo*>(valueProfileInfo->getValueInfo(bcInfo, comp, AddressInfo, TR_ValueProfileInfoManager::justInterpreterProfileInfo));
   if (!valueInfo || valueInfo->getNumProfiledValues()==0)
      {
      if (p1) traceMsg(comp, "==TPIC==No IProfiler info on node %p in %s\n", node, comp->signature());
      return 0;
      }

   TR_OpaqueClassBlock *topValue = (TR_OpaqueClassBlock *) valueInfo->getTopValue();
   if (!topValue)
      {
      if (p1) traceMsg(comp, "==TPIC==No topvalue on node %p in %s\n", node, comp->signature());
      return 0;
      }

   if ((recordAll == false) && valueInfo->getTopProbability() < TR::Options::getMinProfiledCheckcastFrequency())
      {
      if (p1) traceMsg(comp, "==TPIC==low top probability on node %p in %s\n", node, comp->signature());
      return 0;
      }

   if (comp->getPersistentInfo()->isObsoleteClass(topValue, cg->fe()))
      {
      if (p1) traceMsg(comp, "==TPIC==%p unloaded on node %p in %s\n", topValue, node, comp->signature());
      return 0;
      }

   uintptrj_t totalFrequency = valueInfo->getTotalFrequency();
   TR_ScratchList<TR_ExtraAddressInfo> valuesSortedByFrequency(comp->trMemory());
   valueInfo->getSortedList(comp, &valuesSortedByFrequency);

   ListIterator<TR_ExtraAddressInfo> sortedValuesIt(&valuesSortedByFrequency);
   TR_ExtraAddressInfo *profiledInfo;
   uint8_t number = 0;
   for (profiledInfo = sortedValuesIt.getFirst(); profiledInfo != NULL; profiledInfo = sortedValuesIt.getNext())
      {
      TR_OpaqueClassBlock * classPointer = (TR_OpaqueClassBlock *)(profiledInfo->_value);
      if (!classPointer)
         continue;

      if (comp->getPersistentInfo()->isObsoleteClass((void *)classPointer, cg->fe()))
         {
         return 0;
         }

      TR_OpaqueClassBlock *thisType = cg->fej9()->getProfiledClassFromProfiledInfo(profiledInfo);
      if (!thisType)
         continue;
      if (p1)
         {
         char *name;
         int32_t len;
         name = comp->fej9()->getClassNameChars(thisType, len);
         traceMsg(comp, "==TPIC==Freq %d (%.2f%%) %.*s @ %p\n", profiledInfo->_frequency, (float) profiledInfo->_frequency/totalFrequency, len, name, thisType); fflush(stdout);
         }
      if ((recordAll == false) && ((float) profiledInfo->_frequency/totalFrequency) < TR::Options::getMinProfiledCheckcastFrequency())
         continue;

      classArray[number] = thisType;
      if (probability)
         {
         probability[number] = ((float)profiledInfo->_frequency)/totalFrequency;
         }
      number++;
      }
   return number;
   }


TR_OpaqueClassBlock *
J9::TreeEvaluator::interpreterProfilingInstanceOfOrCheckCastInfo(TR::CodeGenerator * cg, TR::Node * node)
   {
   TR::Compilation *comp= cg->comp();
   TR_ByteCodeInfo bcInfo = node->getByteCodeInfo();
   TR_ValueProfileInfoManager * valueProfileInfo = TR_ValueProfileInfoManager::get(comp);

   if (!valueProfileInfo)
      return NULL;

   TR_AddressInfo * valueInfo = static_cast<TR_AddressInfo*>(valueProfileInfo->getValueInfo(bcInfo, comp, AddressInfo, TR_ValueProfileInfoManager::justInterpreterProfileInfo));
   if (!valueInfo || valueInfo->getNumProfiledValues()==0)
      {
      return NULL;
      }

   TR_OpaqueClassBlock *topValue = (TR_OpaqueClassBlock *) valueInfo->getTopValue();
   if (!topValue)
      {
      return NULL;
      }

   if (valueInfo->getTopProbability() < TR::Options::getMinProfiledCheckcastFrequency())
      return NULL;

   if (comp->getPersistentInfo()->isObsoleteClass(topValue, cg->fe()))
      {
      return NULL;
      }

   return topValue;
   }


bool
J9::TreeEvaluator::checkcastShouldOutlineSuperClassTest(TR::Node *node, TR::CodeGenerator *cg)
    {
    // caller should always first call instanceOfOrCheckCastNeedSuperTest
   TR::Node            *castClassNode    = node->getSecondChild();
   TR::MethodSymbol    *helperSym        = node->getSymbol()->castToMethodSymbol();
   TR::SymbolReference *castClassSymRef  = castClassNode->getSymbolReference();
   TR_ByteCodeInfo bcInfo = node->getByteCodeInfo();
   TR::Compilation *comp= cg->comp();

   TR_ValueProfileInfoManager * valueProfileInfo = TR_ValueProfileInfoManager::get(comp);

   if (castClassSymRef->isUnresolved())
      {
      return false;
      }

   if (!TR::TreeEvaluator::isStaticClassSymRef(castClassSymRef))
      {
      // We could theoretically do a super test on something with no sym, but it would require significant
      // changes to platform code. The benefit is little at this point (shows up from reference arraycopy reductions)
      return false;
      }

   TR::StaticSymbol    *castClassSym = castClassSymRef->getSymbol()->getStaticSymbol();

   if (!valueProfileInfo)
      return false;

   TR_AddressInfo * valueInfo = static_cast<TR_AddressInfo*>(valueProfileInfo->getValueInfo(bcInfo, comp, AddressInfo, TR_ValueProfileInfoManager::justInterpreterProfileInfo));

   if (!valueInfo || valueInfo->getNumProfiledValues()==0)
      {
      return false;
      }

   TR_OpaqueClassBlock *topValue = (TR_OpaqueClassBlock *) valueInfo->getTopValue();

   if (!topValue)
      {
      return false;
      }

   if (valueInfo->getTopProbability() < TR::Options::getMinProfiledCheckcastFrequency())
      return false;

   if (comp->getPersistentInfo()->isObsoleteClass(topValue, cg->fe()))
      {
      return false;
      }

   if (topValue == (TR_OpaqueClassBlock *) castClassSym->getStaticAddress())
        {
        return true;
        }

   return false;
    }


bool
J9::TreeEvaluator::loadLookaheadAfterHeaderAccess(TR::Node *node, int32_t &fieldOffset, TR::CodeGenerator *cg)
   {
   TR::Node *object = node->getFirstChild();

   TR::TreeTop *currTree = cg->getCurrentEvaluationTreeTop()->getNextTreeTop();
   while (currTree)
      {
      TR::Node *currNode = currTree->getNode();
      if (currNode->getOpCodeValue() == TR::aloadi || currNode->getOpCodeValue() == TR::iloadi)
         {
         if (currNode->getFirstChild() == object)
            {
            int displacement = 0;
            TR::Symbol *sym = currNode->getSymbolReference()->getSymbol();
            if (sym)
               {
               if (sym->isRegisterMappedSymbol() &&
                   sym->getRegisterMappedSymbol()->getOffset() != 0)
                  {
                  displacement = sym->getRegisterMappedSymbol()->getOffset();
                  }
               }


            fieldOffset = displacement + currNode->getSymbolReference()->getOffset();
            return true;
            }
         }
      else if (currNode->getNumChildren() > 0 &&
               currNode->getFirstChild()->getNumChildren() > 0 &&
               (currNode->getFirstChild()->getOpCodeValue() == TR::aloadi || currNode->getFirstChild()->getOpCodeValue() == TR::iloadi))
         {
         if (currNode->getFirstChild()->getFirstChild() == object)
            {
            int displacement = 0;
            TR::Symbol *sym = currNode->getFirstChild()->getSymbolReference()->getSymbol();
            if (sym)
               {
               if (sym->isRegisterMappedSymbol() &&
                   sym->getRegisterMappedSymbol()->getOffset() != 0)
                  {
                  displacement = sym->getRegisterMappedSymbol()->getOffset();
                  }
               }

            fieldOffset = displacement + currNode->getFirstChild()->getSymbolReference()->getOffset();
            return true;
            }
         }
      currTree = currTree->getNextTreeTop();
      }

   return false;
   }


// only need a helper call if the class is not super and not final, otherwise
// it can be determined without a call-out
bool J9::TreeEvaluator::instanceOfNeedHelperCall(bool testCastClassIsSuper, bool isFinalClass)
   {
   return !testCastClassIsSuper && !isFinalClass;
   }


bool J9::TreeEvaluator::instanceOfOrCheckCastIsJavaLangObjectArray(TR::Node * node, TR::CodeGenerator *cg)
   {
   TR::Node            *castClassNode    = node->getSecondChild();
   TR::SymbolReference *castClassSymRef  = castClassNode->getSymbolReference();

   if (!TR::TreeEvaluator::isStaticClassSymRef(castClassSymRef))
      {
      return false;
      }

   TR::StaticSymbol    *castClassSym = castClassSymRef->getSymbol()->getStaticSymbol();

   if (castClassSymRef->isUnresolved())
      {
      return false;
      }
   else
      {
      TR_OpaqueClassBlock * clazz;
      if (castClassSym &&
          (clazz = (TR_OpaqueClassBlock *) castClassSym->getStaticAddress()) &&
          TR::Compiler->cls.isClassArray(cg->comp(), clazz))
         {
         TR_OpaqueClassBlock *objectClass = cg->fej9()->getSystemClassFromClassName("java/lang/Object", 16);
         clazz = cg->fej9()->getComponentClassFromArrayClass(clazz);
         return (objectClass) && objectClass == clazz;
         }
      }
   return false;
   }


TR_OpaqueClassBlock *
J9::TreeEvaluator::getCastClassAddress(TR::Node * castClassNode)
   {
   TR::SymbolReference * symRef = castClassNode->getSymbolReference();
   if (!TR::TreeEvaluator::isStaticClassSymRef(symRef))
      {
      return NULL;
      }

   TR::Symbol * symbol = symRef->getSymbol();

   if (symRef->isUnresolved())
      {
      return NULL; // node is unresolved - class address not known yet
      }
   TR_OpaqueClassBlock * staticAddressValue = (TR_OpaqueClassBlock*) symbol->castToStaticSymbol()->getStaticAddress();
   return staticAddressValue;
   }


/*
 * return true if instanceof/checkcast's checking class is final array
 */
bool
J9::TreeEvaluator::instanceOfOrCheckCastIsFinalArray(TR::Node * node, TR::CodeGenerator *cg)
   {
   TR::Node            *castClassNode    = node->getSecondChild();
   TR::SymbolReference *castClassSymRef  = castClassNode->getSymbolReference();

   if (!TR::TreeEvaluator::isStaticClassSymRef(castClassSymRef))
      {
         return false;
      }

   TR::StaticSymbol    *castClassSym = castClassSymRef->getSymbol()->getStaticSymbol();

   if (castClassSymRef->isUnresolved())
      {
      return false;
      }
   else
      {
      TR_OpaqueClassBlock * clazz;
      // If the class is a regular class (i.e., not an interface nor an array) and
      // not known to be a final class, an inline superclass test can be generated.
      // If the helper does not preserve all the registers there will not be
      // enough registers to do the superclass test inline.
      // Also, don't generate the superclass test if optimizing for space.
      //
      if (castClassSym &&
          (clazz = (TR_OpaqueClassBlock *) castClassSym->getStaticAddress()) &&
          TR::Compiler->cls.isClassArray(cg->comp(), clazz) &&
          (clazz = cg->fej9()->getLeafComponentClassFromArrayClass(clazz)) &&
          (cg->fej9()->isClassFinal(clazz) || TR::Compiler->cls.isPrimitiveClass(cg->comp(), clazz))
          )
         return true;
      }
   return false;
   }


void
J9::TreeEvaluator::evaluateLockForReservation(TR::Node *node, bool *reservingLock, bool *normalLockPreservingReservation, TR::CodeGenerator *cg)
   {
   static const char *allPreserving = feGetEnv("TR_AllLocksPreserving");
   TR::Compilation *comp= cg->comp();

   if (!node->isSyncMethodMonitor())
      {
      *reservingLock = false;
      *normalLockPreservingReservation = false;
      return;
      }

   if (comp->getOption(TR_ReserveAllLocks))
      {
      *reservingLock = true;
      *normalLockPreservingReservation = false;
      return;
      }

   if (allPreserving)
      {
      *reservingLock = false;
      *normalLockPreservingReservation = true;
      return;
      }

   TR_OpaqueMethodBlock *owningMethod = node->getOwningMethod();
   TR_OpaqueClassBlock *classPointer = cg->fej9()->getClassOfMethod(owningMethod);
   TR_PersistentClassInfo * persistentClassInfo =
      comp->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(classPointer, comp);

   if (persistentClassInfo && persistentClassInfo->isReservable())
      {
      if (comp->getMethodHotness() >= warm)
         *reservingLock = true;
      else
         *normalLockPreservingReservation = true;
      }
   }



static TR::Node *scanForMonitorExitNode(TR::TreeTop *firstTree)
   {
   TR::TreeTop *currTree = firstTree;
   while (currTree)
      {
      TR::Node *currNode = currTree->getNode();
      if (currNode->getOpCodeValue() == TR::monexit)
         {
         if (currNode->isSyncMethodMonitor())
            return currNode;
         else
              return NULL;
         }
      else if (currNode->getNumChildren() > 0 &&
               currNode->getFirstChild()->getNumChildren() > 0 &&
               currNode->getFirstChild()->getOpCodeValue() == TR::monexit)
         {
         if (currNode->getFirstChild()->isSyncMethodMonitor())
            return currNode->getFirstChild();
         else
              return NULL;
         }

      if ((currNode->getOpCodeValue() == TR::monent ||
          currNode->exceptionsRaised() != 0        ||
          currNode->canCauseGC()                   ||
          currNode->getOpCode().isBranch()))
         {
         return NULL;
         }

      currTree = currTree->getNextTreeTop();
      }

   return NULL;
   }


bool
J9::TreeEvaluator::isPrimitiveMonitor(TR::Node *monNode, TR::CodeGenerator *cg)
   {
   // TODO: enable primitive monitors once we have the standard ones working
   // The primitive monitors will need separate monexit helper where the
   // recursive count is incremented in case FLC is set. The state of RES and FLC set
   // while the count is 0 is illegal.
   static const char *allReservingPrimitive = feGetEnv("TR_AllLocksReservingPrimitive");
   static const char *noReservingPrimitiveLocks = feGetEnv("TR_NoReservingPrimitiveLocks");

   if (allReservingPrimitive)
      return true;

   if (noReservingPrimitiveLocks)
      return false;

   TR::Node *object = monNode->getFirstChild();

   TR_ASSERT(monNode->getOpCodeValue() == TR::monent, "Primitive monitor region check should only be done on monitor enter node");

   TR::Node *guardExit = NULL;

   TR::TreeTop *currTree = cg->getCurrentEvaluationTreeTop()->getNextTreeTop();
   while (currTree)
      {
      TR::Node *currNode = currTree->getNode();
      if (currNode->getOpCodeValue() == TR::monexit)
         {
         if (currNode->getFirstChild() == object)
            {
            monNode->setPrimitiveLockedRegion();
            currNode->setPrimitiveLockedRegion();

            if (guardExit != NULL)
                {
               guardExit->setPrimitiveLockedRegion();
               }
            return true;
            }
         else
            return false;
         }
      else if (currNode->getNumChildren() > 0 &&
               currNode->getFirstChild()->getNumChildren() > 0 &&
               currNode->getFirstChild()->getOpCodeValue() == TR::monexit)
         {
         if (currNode->getFirstChild()->getFirstChild() == object)
            {
            monNode->setPrimitiveLockedRegion();
            currNode->getFirstChild()->setPrimitiveLockedRegion();

            if (guardExit != NULL)
                {
               guardExit->setPrimitiveLockedRegion();
               }
            return true;
            }
         else
            return false;
         }

      if ((currNode->getOpCodeValue() == TR::monent ||
          currNode->exceptionsRaised() != 0        ||
          currNode->canCauseGC()                   ||
          currNode->getOpCode().isBranch()         ||
          (currNode->getOpCodeValue() == TR::BBStart && !currNode->getBlock()->isExtensionOfPreviousBlock())
          ))
         {
         if ((currNode->getOpCode().isIf() && currNode->isNonoverriddenGuard()))
              {
              guardExit = scanForMonitorExitNode( currNode->getBranchDestination());

              if (!guardExit && monNode->isSyncMethodMonitor())
                  return false;
              }
         else
            return false;
         }

      currTree = currTree->getNextTreeTop();
      }

   return false;
   }

bool
J9::TreeEvaluator::isDummyMonitorEnter(TR::Node *monNode, TR::CodeGenerator *cg)
   {
   TR::Node *object = monNode->getFirstChild();

   TR_ASSERT(monNode->getOpCodeValue() == TR::monent, "Primitive monitor region check should only be done on monitor enter node");

   TR::Node *guardExit = NULL;

   TR::TreeTop *currTree = cg->getCurrentEvaluationTreeTop()->getNextTreeTop();
   TR::Node *currNode = currTree->getNode();


   if (currNode->getOpCode().isIf() && currNode->isNonoverriddenGuard() && monNode->isSyncMethodMonitor())
      {
      guardExit = scanForMonitorExitNode(currNode->getBranchDestination());

      if (!guardExit)
         return false;

      currTree = currTree->getNextTreeTop();
      }

   if (currTree)
      {
      currNode = currTree->getNode();
      if (currNode->getOpCodeValue() == TR::monexit)
         {
         if (currNode->getFirstChild() == object)
            {
            return true;
            }
         else
            return false;
         }
      else if (currNode->getNumChildren() > 0 &&
               currNode->getFirstChild()->getNumChildren() > 0 &&
               currNode->getFirstChild()->getOpCodeValue() == TR::monexit)
         {
         if (currNode->getFirstChild()->getFirstChild() == object)
            {
            return true;
            }
         else
            return false;
         }
      }

   return false;
   }

bool
J9::TreeEvaluator::isDummyMonitorExit(TR::Node *monNode, TR::CodeGenerator *cg)
   {
   TR::Node *object = monNode->getFirstChild();

   TR_ASSERT(monNode->getOpCodeValue() == TR::monexit, "Primitive monitor region check should only be done on monitor exit node");

   TR::TreeTop *currTree = cg->getCurrentEvaluationTreeTop()->getPrevTreeTop();
   TR::Node *currNode = currTree->getNode();

   if (currNode->getOpCode().isIf() && currNode->isNonoverriddenGuard() && monNode->isSyncMethodMonitor())
      {
      currTree = currTree->getPrevTreeTop();
      }

   if (currTree)
      {
      currNode = currTree->getNode();
      if (currNode->getOpCodeValue() == TR::monent)
         {
         if (currNode->getFirstChild() == object)
            {
            return true;
            }
         else
            return false;
         }
      else if (currNode->getNumChildren() > 0 &&
               currNode->getFirstChild()->getNumChildren() > 0 &&
               currNode->getFirstChild()->getOpCodeValue() == TR::monent)
         {
         if (currNode->getFirstChild()->getFirstChild() == object)
            {
            return true;
            }
         else
            return false;
         }
      }

   return false;
   }


// Helper Functions for BNDCHKwithSpineCHK

// Evaluate all subtrees whose first reference is under the root node and whose
// last reference is not.
//
// Evaluate every subtree S of a node N that meets the following criteria:
//
//    (1) the first reference to S is in a subtree of N, and
//    (2) not all references to S appear under N
//
// All subtrees will be evaluated as soon as they are discovered.
//
//
// TODO: do not pre-evaluate nodes under a spinechk if all other references are also under
//       a spinechk (there could be some complications here though).
//
void J9::TreeEvaluator::preEvaluateEscapingNodesForSpineCheck(TR::Node *root, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::initializeStrictlyFutureUseCounts(root, cg->comp()->incVisitCount(), cg);
   TR::TreeEvaluator::evaluateNodesWithFutureUses(root, cg);
   }
