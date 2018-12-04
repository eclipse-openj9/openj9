/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include "optimizer/StringBuilderTransformer.hpp"

#include <math.h>
#include "env/jittypes.h"
#include "env/CompilerEnv.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "env/VMJ9.h"
#include "il/Block.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "ras/DebugCounter.hpp"

#if defined (_MSC_VER) && _MSC_VER < 1900
#define snprintf _snprintf
#endif

#define OPT_DETAILS "O^O STRINGBUILDER TRANSFORMER: "

static const char* StringBuilderClassName = "java/lang/StringBuilder";

/** \note
 *     This optimization is disabled for AOT compilations due to a functional issue. Consider an AOT compilation of the
 *     following FireTruck.toString()Ljava/lang/String; method:
 *
 *     \code
 *     class FireTruck
 *     {
 *        // ...
 *
 *        public String toString()
 *        {
 *           return "Firetruck " + getSerialNumber();
 *        }
 *     }
 *     \endcode
 *
 *     During an AOT compilation of this method the StringBuilderTransformer optimization would pattern match the
 *     String concatenation sequence and would replace the java/lang/StringBuilder.<init>()V call with a call to the
 *     overloaded java/lang/StringBuilder.<init>(I)V method. Now consider a scenario in which the inliner subsequently
 *     chose to inline this new constructor call. Because this is an AOT compilation we have to place down a guard for
 *     this inlined call site and generate a relocation record for it. The guard generated will be an
 *     TR_InlinedStaticMethodWithNopGuard which guards against java/lang/StringBuilder class modifications.
 *
 *     The issue that arises in the AOT compilation is that the original bytecode for the original java/lang/
 *     StringBuilder.<init>()V call is an invokespecial bytecode with a constant pool index of the java/lang/
 *     StringBuilder.<init>()V in the constant pool of FireTruck.toString()Ljava/lang/String;. Because we modified the
 *     target of the call to the overloaded constructor we may not have a constant pool entry for this overloaded
 *     constructor so we are not able to properly create the relocation record for the TR_InlinedStaticMethodWithNopGuard
 *     because we do not have a valid constant pool index for the new constructor overload call.
 *
 *     Because this constant pool index is non-existent we are not able to correctly validate the inlined call site in
 *     an AOT compilation where the StringBuilderTransformer optimization was performed. The amount of work involved to
 *     add support for these types of relocations is greater than the amount of work involved in writing this
 *     optimization. Because of this tradeoff we chose to disable this optimization for AOT compilations.
 *
 *     TODO: Add AOT support for this optimization. Note that this may be a heroic effort.
 *     TODO: For correctness under HCR, the modified call should be wrapped in an HCR guard, in the event that
 *     StringBuilder is redefined.
 */
int32_t TR_StringBuilderTransformer::perform()
   {
   if (comp()->getOption(TR_DisableStringBuilderTransformer) || (comp()->compileRelocatableCode() && !comp()->getOption(TR_UseSymbolValidationManager)))
      {
      return 0;
      }

   for (TR::AllBlockIterator iter(optimizer()->getMethodSymbol()->getFlowGraph(), comp()); iter.currentBlock() != NULL; ++iter)
      {
      TR::Block* block = iter.currentBlock();

      performOnBlock(block);
      }

   return 1;
   }

int32_t TR_StringBuilderTransformer::performOnBlock(TR::Block* block)
   {
   uint32_t expectedReferenceCount = 3;
   if (comp()->isOSRTransitionTarget(TR::postExecutionOSR))
      expectedReferenceCount = 4;

   for (TR::TreeTopIterator iter(block->getEntry(), comp()); iter != block->getExit(); ++iter)
      {
      TR::Node* currentNode = iter.currentNode();

      // TR::New will be the child of a treetop node
      if (currentNode->getOpCodeValue() == TR::treetop)
         {
         currentNode = currentNode->getFirstChild();
         }

      if (currentNode->getOpCodeValue() == TR::New && currentNode->getReferenceCount() == expectedReferenceCount)
         {
         TR::Node* loadaddrNode = currentNode->getFirstChild();

         int32_t classNameLength = 0;

         const char* className = TR::Compiler->cls.classNameChars(comp(), loadaddrNode->getSymbolReference(), classNameLength);

         // Find the 'new java/lang/StringBuilder' node
         if (classNameLength == strlen(StringBuilderClassName) && strncmp(className, StringBuilderClassName, classNameLength) == 0)
            {
            if (trace())
               {
               traceMsg(comp(), "[0x%p] Found new java/lang/StringBuilder node.\n", currentNode);
               }

            ++iter;

            if (iter.currentNode()->getOpCodeValue() == TR::allocationFence)
               {
               ++iter;
               }

            TR::Node* initNode = findStringBuilderInit(iter, currentNode);

            if (initNode != NULL)
               {
               List<TR_Pair<TR::Node*, TR::RecognizedMethod> > appendArguments (trMemory());

               if (findStringBuilderChainedAppendArguments(iter, currentNode, appendArguments) != NULL)
                  {
                  int32_t capacity = computeHeuristicStringBuilderInitCapacity(appendArguments);

                  if (performTransformation(comp(), "%sTransforming java/lang/StringBuilder.<init>()V call at node [0x%p] to java/lang/StringBuilder.<init>(I)V with capacity = %d\n", OPT_DETAILS, initNode, capacity))
                     {
                     static const bool collectAppendStatistics = feGetEnv("TR_StringBuilderTransformerCollectAppendStatistics") != NULL;
                     static const bool collectAllocationStatistics = feGetEnv("TR_StringBuilderTransformerCollectAllocationStatistics") != NULL;
                     static const bool collectAllocationBacktraces = feGetEnv("TR_StringBuilderTransformerCollectAllocationBacktraces") != NULL;
                     static const bool collectAppendObjectTypes = feGetEnv("TR_StringBuilderTransformerCollectAppendObjectTypes") != NULL;
                     static const char* overrideInitialCapacity = feGetEnv("TR_StringBuilderTransformerOverrideInitialCapacity");

                     if (collectAppendStatistics || collectAllocationStatistics || collectAllocationBacktraces || collectAppendObjectTypes || overrideInitialCapacity != NULL)
                        {
                        TR::SymbolReference* newInitSymRef = getSymRefTab()->methodSymRefFromName(comp()->getMethodSymbol(), "java/lang/StringBuilder", "<init>", "(IZZZZ)V", TR::MethodSymbol::Static);

                        if (overrideInitialCapacity != NULL)
                           {
                           capacity = atoi(overrideInitialCapacity);
                           }

                        TR::Node::recreateWithoutProperties(initNode, TR::call, 6, newInitSymRef);

                        initNode->setChild(0, currentNode);

                        initNode->setAndIncChild(1, TR::Node::iconst(capacity));
                        initNode->setAndIncChild(2, TR::Node::iconst(static_cast<int32_t>(collectAppendStatistics)));
                        initNode->setAndIncChild(3, TR::Node::iconst(static_cast<int32_t>(collectAllocationStatistics)));
                        initNode->setAndIncChild(4, TR::Node::iconst(static_cast<int32_t>(collectAllocationBacktraces)));
                        initNode->setAndIncChild(5, TR::Node::iconst(static_cast<int32_t>(collectAppendObjectTypes)));
                        }
                     else
                        {
                        TR::SymbolReference* newInitSymRef = getSymRefTab()->methodSymRefFromName(comp()->getMethodSymbol(), "java/lang/StringBuilder", "<init>", "(I)V", TR::MethodSymbol::Static);

                        TR::Node::recreateWithoutProperties(initNode, TR::call, 2, newInitSymRef);

                        initNode->setChild(0, currentNode);

                        initNode->setAndIncChild(1, TR::Node::iconst(capacity));
                        }

                     TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "StringBuilderTransformer/Succeeded/%d/%s", capacity, comp()->signature()));
                     }
                  }
               }
            }
         }
      }

   return 1;
   }

/** \details
 *     We attempt to pattern match the following canonical tree sequence:
 *
 *     \code
 *     n5n       treetop
 *     n4n         new  jitNewObject
 *     n3n           loadaddr  java/lang/StringBuilder
 *     n7n       NULLCHK
 *     n6n         call  java/lang/StringBuilder.<init>()V
 *     n4n           ==>new
 *     \endcode
 *
 *     Note that we do not actually traverse any treetops. We only peek inside of the current iterator.
 */
TR::Node* TR_StringBuilderTransformer::findStringBuilderInit(TR::TreeTopIterator iter, TR::Node* newNode)
   {
   // It is necessary to skip trees added for OSR book keeping purposes between the allocation and the call. There
   // could be several treetops.
   if (comp()->isOSRTransitionTarget(TR::postExecutionOSR))
      {
      bool foundNewReference = false;
      TR_ByteCodeInfo &bci = iter.currentNode()->getByteCodeInfo();
      while (comp()->getMethodSymbol()->isOSRRelatedNode(iter.currentNode(), bci))
         {
         if (trace())
            traceMsg(comp(), "[0x%p] Skipping OSR bookkeeping node.\n", iter.currentNode());
         if (newNode == iter.currentNode()->getFirstChild())
            foundNewReference = true;
         ++iter;
         }

      // The call node should have the same bytecodeinfo as the related nodes and the additional reference should have
      // been found
      TR_ByteCodeInfo &callBCI = iter.currentNode()->getByteCodeInfo();
      if (!foundNewReference
          || callBCI.getByteCodeIndex() != bci.getByteCodeIndex()
          || callBCI.getCallerIndex() != bci.getCallerIndex())
         return NULL;
      }

   TR::Node* NULLCHKNode = iter.currentNode();

   if (NULLCHKNode->getOpCodeValue() == TR::NULLCHK)
      {
      TR::Node* callNode = NULLCHKNode->getFirstChild();

      if (callNode->getOpCodeValue() == TR::call && callNode->getFirstChild() == newNode)
         {
         OMR::ResolvedMethodSymbol* callSymbol = callNode->getSymbol()->getResolvedMethodSymbol();

         if (callSymbol != NULL && callSymbol->getRecognizedMethod() == TR::java_lang_StringBuilder_init)
            {
            if (trace())
               {
               traceMsg(comp(), "[0x%p] Found java/lang/StringBuilder.<init>()V call node.\n", callNode);
               }

            return callNode;
            }
         }
      }

   if (trace())
      {
      traceMsg(comp(), "[0x%p] Could not find java/lang/StringBuilder.<init>()V call on new node.\n", newNode);
      }

   TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "StringBuilderTransformer/Failed/CouldNotLocateInit/%s", comp()->signature()));

   return NULL;
   }

/** \details
 *     We attempt to pattern match the following canonical tree sequence:
 *
 *     \code
 *     n10n      NULLCHK
 *     n9n         acall  java/lang/StringBuilder.append(Ljava/lang/String;)Ljava/lang/StringBuilder;
 *     n4n           ==>new
 *     n8n           aload
 *     n13n      NULLCHK
 *     n12n        acall  java/lang/StringBuilder.append(Ljava/lang/String;)Ljava/lang/StringBuilder;
 *     n9n           ==>acall
 *     n11n          aload
 *     .
 *     .
 *     .
 *     n26n      NULLCHK
 *     n25n        acall  java/lang/StringBuilder.append(I)Ljava/lang/StringBuilder;
 *     n19n          ==>acall
 *     n24n          iload
 *     n28n      NULLCHK
 *     n27n        acall  java/lang/StringBuilder.toString()Ljava/lang/String;
 *     n25n          ==>acall
 *     \endcode
 *
 *     Note that the result of each acall is a StringBuilder object which is the receiver of the next acall node. The
 *     sequence is terminated by a call to StringBuilder.toString() within the same block. Furthermore there can be
 *     arbitrary NULLCHK trees in between the calls to StringBuilder.append(...) as long as the chain is not broken.
 *     This notion of chaining along with node reference counts ensures that the StringBuilder object  is not used
 *     anywhere outside the chained StringBuilder.append(...) sequence.
 */
TR::Node* TR_StringBuilderTransformer::findStringBuilderChainedAppendArguments(TR::TreeTopIterator iter, TR::Node* newNode, List<TR_Pair<TR::Node*, TR::RecognizedMethod> >& appendArguments)
   {
   TR::Node* stringBuilderReceiver = newNode;

   // Under postExecution OSR, there will be an additional reference to the call result
   // This accounts for it and ensures it is found. A reference is only expected after the
   // initial call, so the flag is initially set to true.
   uint32_t expectedReferenceCount = 2;
   bool foundOSRReference = true;
   if (comp()->isOSRTransitionTarget(TR::postExecutionOSR))
      expectedReferenceCount = 3;

   while (iter != NULL)
      {
      TR::Node* currentNode = iter.currentNode();

      if (currentNode->getNumChildren() == 1 &&
          currentNode->getFirstChild()->isPotentialOSRPointHelperCall())
         {
         if (trace())
            {
            traceMsg(comp(), "Skipping potentialOSRPointHelper call n%dn [0x%p].\n", currentNode->getGlobalIndex(), currentNode);
            }
         ++iter;
         continue;
         }

      if (currentNode->getOpCodeValue() == TR::NULLCHK)
         {
         TR::Node* acallNode = currentNode->getFirstChild();

         if (acallNode->getOpCodeValue() == TR::acall && acallNode->getFirstChild() == stringBuilderReceiver)
            {
            if (trace())
               {
               traceMsg(comp(), "[0x%p] Examining acall node.\n", acallNode);
               }

            OMR::ResolvedMethodSymbol* acallSymbol = acallNode->getSymbol()->getResolvedMethodSymbol();

            if (acallSymbol != NULL)
               {
               TR::RecognizedMethod recogniedMethod = acallSymbol->getRecognizedMethod();

               switch (recogniedMethod)
                  {
                  case TR::java_lang_StringBuilder_append_bool:
                  case TR::java_lang_StringBuilder_append_char:
                  case TR::java_lang_StringBuilder_append_double:
                  case TR::java_lang_StringBuilder_append_float:
                  case TR::java_lang_StringBuilder_append_int:
                  case TR::java_lang_StringBuilder_append_long:
                  case TR::java_lang_StringBuilder_append_String:
                  case TR::java_lang_StringBuilder_append_Object:
                     {
                     // The reference count for the new call and the flag foundOSRReference for the prior call are checked here
                     if (acallNode->getReferenceCount() == expectedReferenceCount && foundOSRReference)
                        {

                        if (trace())
                           {
                           traceMsg(comp(), "[0x%p] Adding argument of java/lang/StringBuilder.append acall node.\n", acallNode);
                           }

                        appendArguments.add(new (trHeapMemory()) TR_Pair<TR::Node*, TR::RecognizedMethod> (acallNode->getSecondChild(), recogniedMethod));

                        // The result of this append call is chained to the next so update the current receiver
                        stringBuilderReceiver = acallNode;
                        foundOSRReference = !comp()->isOSRTransitionTarget(TR::postExecutionOSR);
                        }
                     else if (!foundOSRReference)
                        {
                        if (trace())
                           {
                           traceMsg(comp(), "[0x%p] Invalid reference count at acall node due to missing OSR bookkeeping.\n", stringBuilderReceiver);
                           }

                        TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "StringBuilderTransformer/Failed/InvalidReferenceCountMissingBookkeeping/%s", comp()->signature()));

                        return NULL;
                        }
                     else
                        {
                        if (trace())
                           {
                           traceMsg(comp(), "[0x%p] Invalid reference count at acall node.\n", acallNode);
                           }

                        TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "StringBuilderTransformer/Failed/InvalidReferenceCount/%s", comp()->signature()));

                        return NULL;
                        }
                     }
                     break;

                  case TR::java_lang_StringBuilder_toString:
                     {
                     // Verify that the OSR reference was found for the final append call
                     if (foundOSRReference)
                        {
                        if (trace())
                           {
                           traceMsg(comp(), "[0x%p] Found java/lang/StringBuilder.toString acall node.\n", acallNode);
                           }

                        return acallNode;
                        }
                     else
                        {
                        if (trace())
                           {
                           traceMsg(comp(), "[0x%p] Invalid reference count at acall node due to missing OSR bookkeeping for final append.\n", stringBuilderReceiver);
                           }

                        TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "StringBuilderTransformer/Failed/InvalidReferenceCountMissingBookkeeping/%s", comp()->signature()));

                        return NULL;
                        }
                     }
                     break;

                  default:
                     {
                     if (trace())
                        {
                        traceMsg(comp(), "[0x%p] java/lang/StringBuilder.append chain broken at node.\n", acallNode);
                        }

                     TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "StringBuilderTransformer/Failed/AppendChainBroken/%s", comp()->signature()));

                     return NULL;
                     }
                  }
               }
            else
               {
               if (trace())
                  {
                  traceMsg(comp(), "[0x%p] Unresolved acall node.\n", acallNode);
                  }

               TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "StringBuilderTransformer/Failed/UnresolvedACall/%s", comp()->signature()));

               return NULL;
               }
            }
         }
      else if (comp()->getMethodSymbol()->isOSRRelatedNode(currentNode) && currentNode->getFirstChild() == stringBuilderReceiver)
         foundOSRReference = true;

      ++iter;
      }

   if (trace())
      {
      traceMsg(comp(), "[0x%p] NULLCHK chain broken at node.\n", iter.currentNode());
      }

   TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "StringBuilderTransformer/Failed/ToStringNotFound/%s", comp()->signature()));

   return NULL;
   }

/** \details
 *     If the StringBuilder.append(...) argument is a compile time constant we statically determine number of chars
 *     needed to represent the argument by doing the String conversion at compile time. If the argument is not a
 *     compile time constant (such as a parameter or unresolved static) we use a heuristic approach to estimating the
 *     size of the object.
 *
 *     The heuristic used in the non-constant case is hard coded per object type and was determined using the
 *     statistics collection mechanisms implemented by this optimization. See TR_StringBuilderTransformer Environment
 *     Variables section for more details.
 */
int32_t TR_StringBuilderTransformer::computeHeuristicStringBuilderInitCapacity(List<TR_Pair<TR::Node*, TR::RecognizedMethod> >& appendArguments)
   {
   int32_t capacity = 0;

   ListIterator<TR_Pair<TR::Node*, TR::RecognizedMethod> > iter(&appendArguments);

   for (TR_Pair<TR::Node*, TR::RecognizedMethod>* pair = iter.getFirst(); pair != NULL; pair = iter.getNext())
      {
      TR::Node* argument = pair->getKey();

      switch (pair->getValue())
         {
         case TR::java_lang_StringBuilder_append_bool:
            {
            if (argument->getOpCodeValue() == TR::iconst)
               {
               capacity += argument->getInt() == 1 ? 4 : 5;
               }
            else
               {
               capacity += 5;
               }
            }
            break;

         case TR::java_lang_StringBuilder_append_char:
            {
            ++capacity;
            }
            break;

         case TR::java_lang_StringBuilder_append_double:
            {
            if (argument->getOpCodeValue() == TR::dconst)
               {
               char printed[32];

               // The C Standard Library guarantees the printed array to be null-terminated except on certain versions of MSVC++
               snprintf(printed, sizeof(printed) / sizeof(printed[0]), "%g", argument->getDouble());

#if defined (_MSC_VER) && _MSC_VER < 1900
               // Explicitly null terminate as null termination is not guaranteed
               printed[(sizeof(printed) / sizeof(printed[0])) - 1] = '\0';
#endif

               capacity += strlen(printed);
               }
            else
               {
               capacity += 5;
               }
            }
            break;

         case TR::java_lang_StringBuilder_append_float:
            {
            if (argument->getOpCodeValue() == TR::fconst)
               {
               char printed[32];

               // The C Standard Library guarantees the printed array to be null-terminated except on certain versions of MSVC++
               snprintf(printed, sizeof(printed) / sizeof(printed[0]), "%g", argument->getFloat());

#if defined (_MSC_VER) && _MSC_VER < 1900
               // Explicitly null terminate as null termination is not guaranteed
               printed[(sizeof(printed) / sizeof(printed[0])) - 1] = '\0';
#endif

               capacity += strlen(printed);
               }
            else
               {
               capacity += 5;
               }
            }
            break;

         case TR::java_lang_StringBuilder_append_int:
            {
            if (argument->getOpCodeValue() == TR::iconst)
               {
               int32_t value = argument->getInt();

               if (value == 0)
                  {
                  ++capacity;
                  }
               else if (value > 0)
                  {
                  capacity += floor(log10(static_cast<double>(+value))) + 1;
                  }
               else
                  {
                  capacity += floor(log10(static_cast<double>(-value))) + 2;
                  }
               }
            else
               {
               capacity += 4;
               }
            }
            break;

         case TR::java_lang_StringBuilder_append_long:
            {
            if (argument->getOpCodeValue() == TR::iconst)
               {
               int64_t value = argument->getLongInt();

               if (value == 0)
                  {
                  ++capacity;
                  }
               else if (value > 0)
                  {
                  capacity += floor(log10(static_cast<double>(+value))) + 1;
                  }
               else
                  {
                  capacity += floor(log10(static_cast<double>(-value))) + 2;
                  }
               }
            else
               {
               capacity += 8;
               }
            }
            break;

         case TR::java_lang_StringBuilder_append_String:
            {
            if (argument->getOpCodeValue() == TR::aload)
               {
               TR::Symbol* symbol = argument->getSymbol();

               if (symbol->isConstString())
                  {
                  TR::SymbolReference* symRef = argument->getSymbolReference();

                  if (!symRef->isUnresolved())
                     {
                     TR::VMAccessCriticalSection stringBuildAppendStringCriticalSection(comp()->fej9(),
                                                                                         TR::VMAccessCriticalSection::tryToAcquireVMAccess,
                                                                                         comp());

                     if (stringBuildAppendStringCriticalSection.hasVMAccess())
                        {
                        uintptrj_t stringObjectLocation = (uintptrj_t)symbol->castToStaticSymbol()->getStaticAddress();
                        uintptrj_t stringObject = comp()->fej9()->getStaticReferenceFieldAtAddress(stringObjectLocation);
                        capacity += comp()->fe()->getStringUTF8Length(stringObject);

                        break;
                        }
                     }
                  }
               }

            capacity += 16;
            }
            break;

         case TR::java_lang_StringBuilder_append_Object:
            {
            capacity += 7;
            }
            break;

         default:
            {
            TR_ASSERT(false, "Unreachable case reached.");
            }
         }

      if (trace())
         {
         traceMsg(comp(), "[0x%p] Added capacity for node. Current capacity = %d.\n", argument, capacity);
         }
      }

   return capacity;
   }
