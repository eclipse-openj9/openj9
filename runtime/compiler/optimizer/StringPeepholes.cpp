/*******************************************************************************
 * Copyright (c) 2000, 2021 IBM Corp. and others
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

#include "optimizer/StringPeepholes.hpp"

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/CompilerEnv.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/NodePool.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/Cfg.hpp"
#include "infra/ILWalk.hpp"
#include "infra/List.hpp"
#include "infra/Stack.hpp"
#include "optimizer/Inliner.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/TransformUtil.hpp"
#include "ras/Debug.hpp"
#include "runtime/J9Profiler.hpp"
#include "runtime/J9ValueProfiler.hpp"

#include "env/PersistentCHTable.hpp"
#include "env/PersistentInfo.hpp"

static void replaceCallNode(TR::Node *oldCall, TR::Node *newCall, TR::Node *node, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   int32_t i;
   for (i=0;i<node->getNumChildren();i++)
      {
      TR::Node *child = node->getChild(i);
      if (child == oldCall)
         {
         node->setAndIncChild(i, newCall);
         oldCall->recursivelyDecReferenceCount();
         }
      else
         replaceCallNode(oldCall, newCall, node->getChild(i), visitCount);
      }
   }

static void replaceCallNode(TR::Node *oldCall, TR::Node *newCall, TR::TreeTop *tt, vcount_t visitCount)
   {
   while (tt)
      {
      TR::Node *node = tt->getNode();
      if (node->getOpCodeValue() == TR::BBEnd)
         break;

      replaceCallNode(oldCall, newCall, node, visitCount);

      tt = tt->getNextTreeTop();
      }
   }

// counts the number of times the node uniquely appears in a subtree
//
static uint16_t countNodeOccurrencesInSubTree(TR::Node *root, TR::Node *node, vcount_t visitCount)
   {
   uint16_t count = 0;

   if (root == node)
      return 1;

   if (root->getVisitCount() == visitCount)
      return 0;

   root->setVisitCount(visitCount);

   for (int8_t c = root->getNumChildren() - 1; c >= 0; --c)
      count += countNodeOccurrencesInSubTree(root->getChild(c), node, visitCount);
   return count;
   }

TR_StringPeepholes::TR_StringPeepholes(TR::OptimizationManager *manager)
   : TR::Optimization(manager), _transformedCalls(manager->trMemory())
   {}

void TR_StringPeepholes::genFlush(TR::TreeTop *tt, TR::Node *node)
   {
   if (comp()->cg()->getEnforceStoreOrder())
      {
      TR::Node *flushNode = TR::Node::createAllocationFence(node, node);
      tt->insertAfter(TR::TreeTop::create(comp(), flushNode));
      }
   }

TR::SymbolReference* TR_StringPeepholes::findSymRefForOptMethod (StringpeepholesMethods m)
   {
   if (!optSymRefs[m])
      optSymRefs[m] = MethodEnumToArgsForMethodSymRefFromName(m);

   return optSymRefs[m];
   }

TR::SymbolReference* TR_StringPeepholes::MethodEnumToArgsForMethodSymRefFromName (StringpeepholesMethods m)
   {

   TR_ASSERT(m !=  START_STRING_METHODS &&
          m !=  END_STRINGPEEPHOLES_METHODS , "wrong constant!!");


   static char* classNames [] = {"java/math/BigDecimal",
                                 "java/math/BigDecimal",
                         "java/math/BigDecimal",
                         "java/math/BigDecimal",
                          NULL,
                         "java/lang/String",
                         "java/lang/String",
                         "java/lang/String",
                                 "java/lang/String",
                         "java/lang/String",
                         "java/lang/String"};

   static char* methodNames [] = {"SMAAMSS",
                                  "SMSS",
                          "AAMSS",
                          "MSS",
                                  NULL,
                                  "<init>",
                          "<init>",
                          "<init>",
                          "<init>",
                                  "<init>",
                          "<init>"};

   static char* signatures [] =  {        "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/BigDecimal;IIII)Ljava/math/BigDecimal;",
                                  "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/BigDecimal;II)Ljava/math/BigDecimal;",
                          "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/BigDecimal;III)Ljava/math/BigDecimal;",
                          "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;I)Ljava/math/BigDecimal;",
                                  NULL,
                                  "(Ljava/lang/String;C)V",
                                  "(Ljava/lang/String;Ljava/lang/String;)V",
                                  "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
                                  "(Ljava/lang/String;I)V",
                                  "([BIIZ)V",
                                  "(ILjava/lang/String;ILjava/lang/String;Ljava/lang/String;)V"};

   // TODO: This is a workaround as we switched to using a byte[] backing array in String*. Remove this workaround once obsolete.
   if (m == SPH_String_init_AIIZ)
      {
      if (!fe()->getMethodFromName(classNames[m], methodNames[m], signatures[m]))
         {
         return comp()->getSymRefTab()->methodSymRefFromName(comp()->getMethodSymbol(), classNames[m], methodNames[m], "([CIIZ)V", TR::MethodSymbol::Special);
         }
      }

   if (strlen(methodNames[m]) == 6
       && !strncmp(methodNames[m], "<init>", 6))
      return comp()->getSymRefTab()->methodSymRefFromName(comp()->getMethodSymbol(), classNames[m], methodNames[m], signatures[m], TR::MethodSymbol::Special);

   return comp()->getSymRefTab()->methodSymRefFromName(comp()->getMethodSymbol(), classNames[m], methodNames[m], signatures[m], TR::MethodSymbol::Static);
   }


int32_t TR_StringPeepholes::perform()
   {
   static char *skipitAtWarm = feGetEnv("TR_noPeepholeAtWarm");
   if (comp()->getOption(TR_DisableStringPeepholes)
       || (!comp()->fej9()->doStringPeepholing() && !comp()->getOption(TR_UseSymbolValidationManager))
       || (skipitAtWarm && comp()->getMethodHotness()==warm))
      return 1;

   process(comp()->getStartTree(), NULL);

   TR::TreeTop *tt;
   ListIterator<TR::TreeTop> i(&_transformedCalls);
   for (tt = i.getFirst(); tt; tt = i.getNext())
      {
      if (tt->getNode()->getFirstChild()->getReferenceCount() == 0)
         continue;

      TR_InlineCall newInlineCall(optimizer(), this);
      if (newInlineCall.inlineCall(tt, 0, true))
         {
         optimizer()->setUseDefInfo(NULL);
         optimizer()->setValueNumberInfo(NULL);
         optimizer()->setAliasSetsAreValid(false);
         }
      }

   return 1;
   }


int32_t TR_StringPeepholes::performOnBlock(TR::Block *block)
   {
   TR_ASSERT(false, "performOnBlock should not be called for string peepholes\n");
   if (block->getEntry())
      process(block->getEntry(), block->getEntry()->getExtendedBlockExitTreeTop()->getNextTreeTop());
   return 0;
   }



void TR_StringPeepholes::prePerformOnBlocks()
   {
   _transformedCalls.deleteAll();

   optSymRefs = (TR::SymbolReference**) comp()->trMemory()->allocateStackMemory(sizeof(TR::SymbolReference*) * END_STRINGPEEPHOLES_METHODS);
   memset(optSymRefs, 0, sizeof(TR::SymbolReference*) * END_STRINGPEEPHOLES_METHODS);

   // Determine if the StringBuffer or StringBuilder classes have been redefined
   _stringClassesRedefined = classesRedefined();

   _valueOfISymRef = _valueOfCSymRef = _valueOfJSymRef = _valueOfZSymRef = _valueOfOSymRef = 0;
   _stringSymRef = 0;

   TR_ResolvedMethod *feMethod = comp()->getCurrentMethod();
   _methodSymbol = comp()->getOwningMethodSymbol(feMethod);
   TR_OpaqueClassBlock *stringClass = comp()->getStringClassPointer();
   if (!stringClass)
      return;
   _stringSymRef = getSymRefTab()->findOrCreateClassSymbol(_methodSymbol, -1, stringClass);
   }

const char *
TR_StringPeepholes::optDetailString() const throw()
   {
   return "O^O STRING PEEPHOLES: ";
   }

int32_t TR_StringPeepholes::process(TR::TreeTop *startTree, TR::TreeTop *endTree)
   {
   TR::TreeTop *tt, *exitTree;
   for (tt = startTree; (tt != endTree); tt = exitTree->getNextRealTreeTop())
      {
      TR::Block *block = tt->getNode()->getBlock();
      exitTree = block->getExit();
      processBlock(block);
      }

   return 1;
   }

void TR_StringPeepholes::processBlock(TR::Block *block)
   {
   TR::TreeTop *exit = block->getExit();

   for (TR::TreeTop *tt = block->getEntry();
        tt != exit;
        tt = tt->getNextRealTreeTop())
      {
      TR::Node *node = tt->getNode();

      if (node->getOpCodeValue() == TR::treetop) // jump over TreeTop
         node = node->getFirstChild();

      if (!_stringClassesRedefined && (node->getOpCodeValue() == TR::New))
         {
         TR::Node *classNode = node->getFirstChild();
         int32_t len;
         char *className = TR::Compiler->cls.classNameChars(comp(), classNode->getSymbolReference(), len);
         if (len == 22 && !strncmp(className, "java/lang/StringBuffer",  22))
            {
            if (trace())
               traceMsg(comp(), "--stringbuffer-- in %s\n", comp()->signature());

            TR::TreeTop *prevTree = tt->getPrevTreeTop();
            TR::TreeTop *newTree = detectPattern(block, tt, true);
            if (newTree)
               {
               postProcessTreesForOSR(prevTree->getNextTreeTop(), newTree);
               tt = newTree;
               }
            }
         else if (len == 23 && !strncmp(className, "java/lang/StringBuilder", 23))
            {
            if (trace())
               traceMsg(comp(), "--stringbuilder-- in %s\n", comp()->signature());

            TR::TreeTop *prevTree = tt->getPrevTreeTop();
            TR::TreeTop *newTree = detectPattern(block, tt, false);
            if (newTree)
               {
               postProcessTreesForOSR(prevTree->getNextTreeTop(), newTree);
               tt = newTree;
               }
            }
         }
      else if (comp()->isOutermostMethod() && !optimizer()->isIlGenOpt())
         {
         TR::Node *callNode = node;
         if (!node->getOpCode().isCall() &&
             (node->getNumChildren() > 0))
            callNode = node->getFirstChild();

         if (comp()->cg()->getSupportsBigDecimalLongLookasideVersioning() &&
             callNode->getOpCode().hasSymbolReference() &&
             !callNode->getSymbolReference()->isUnresolved() &&
             (callNode->getOpCodeValue() == TR::call) &&
             !callNode->getSymbolReference()->getSymbol()->castToMethodSymbol()->isHelper())
            {
            TR::ResolvedMethodSymbol *owningMethod = callNode->getSymbolReference()->getOwningMethodSymbol(comp());
            if (((owningMethod->getRecognizedMethod() == TR::java_math_BigDecimal_longString1) ||
                 (owningMethod->getRecognizedMethod() == TR::java_math_BigDecimal_longString1C) ||
                 (owningMethod->getRecognizedMethod() == TR::java_math_BigDecimal_longString2)  ||
                 (owningMethod->getRecognizedMethod() == TR::java_math_BigDecimal_toString) ||
                 (owningMethod->getRecognizedMethod() == TR::java_math_BigDecimal_doToString) ) &&
                checkMethodSignature(callNode->getSymbolReference(), "java/lang/String.<init>("))
               {
               TR::ResolvedMethodSymbol *method = callNode->getSymbolReference()->getSymbol()->castToResolvedMethodSymbol();
               TR_ResolvedMethod *m = method->getResolvedMethod();

               // Change the constructor call only if String compression is disabled. The reason we are changing the constructor call to the package protected String
               // constructor is so that we can share the char[] argument passed in without making a copy internally in the String constructor (we would have to do this
               // if we call the public API to ensure the String remains immutable). However the char[] constructed in BigDecimal is always decompressed, so if String
               // compression is enabled passing such a char[] would force the package protected constructor to create a decompressed String because it does not check
               // explicitly whether all the characters in the char[] are compressible or not. The public constructor would make such a check hence why we do not want
               // to carry out this transformation if String compression is enabled.

               // TODO: Calls to this constructor in the above BigDecimal methods do not exist. We need to refactor these classes to call the new
               // String constructors using byte arrays instead.
               if (strncmp(m->signatureChars(), "([BII)", 6) == 0 && !IS_STRING_COMPRESSION_ENABLED_VM(static_cast<TR_J9VMBase *>(comp()->fe())->getJ9JITConfig()->javaVM) &&
                     findSymRefForOptMethod(SPH_String_init_AIIZ))
                  {
                  if (performTransformation(comp(), "%s Changing the string constructor call node %p to invoke a private constructor\n", optDetailString(), callNode))
                     {
                     // Save the original children as we will be recreating the call node
                     TR::Node *callNodeChild0 = callNode->getChild(0);
                     TR::Node *callNodeChild1 = callNode->getChild(1);
                     TR::Node *callNodeChild2 = callNode->getChild(2);
                     TR::Node *callNodeChild3 = callNode->getChild(3);

                     // Recreate the call node since we need to add an extra child to the constructor which determines whether the byte[] we are passing in is compressed
                     TR::Node::recreateWithoutProperties(callNode, TR::call, 5, findSymRefForOptMethod(SPH_String_init_AIIZ));

                     callNode->setChild(0, callNodeChild0);
                     callNode->setChild(1, callNodeChild1);
                     callNode->setChild(2, callNodeChild2);
                     callNode->setChild(3, callNodeChild3);

                     // char[] originating from BigDecimal is always decompressed
                     callNode->setAndIncChild(4, TR::Node::iconst(0));

                     genFlush(tt, callNode->getFirstChild());

                     _transformedCalls.add(tt);
                     }
                  }
               }
            }

         if (callNode->getOpCode().hasSymbolReference() &&
             !callNode->getSymbolReference()->isUnresolved() &&
             (callNode->getOpCodeValue() == TR::call) &&
             !callNode->getSymbolReference()->getSymbol()->castToMethodSymbol()->isHelper() &&
             (comp()->getMethodHotness() > warm) &&
             comp()->isOutermostMethod() &&
             !optimizer()->isIlGenOpt() &&
             checkMethodSignature(callNode->getSymbolReference(), "java/lang/String.<init>("))
            {
            TR::ResolvedMethodSymbol *method = callNode->getSymbolReference()->getSymbol()->castToResolvedMethodSymbol();
            TR_ResolvedMethod *m = method->getResolvedMethod();
            if ((strncmp(m->signatureChars(), "([CII)", 6)==0))
               {
               if(comp()->getRecompilationInfo() &&
                  performTransformation(comp(), "%smight have simplified string pattern at node [%p] if profiling info was available. Switching to profiling.\n", optDetailString(), callNode))
                 {
                 if (comp()->getMethodHotness() == hot)
                    {
                    if (trace())
                       printf("switching method %s to profiling\n", comp()->signature()); fflush(stdout);
                    optimizer()->switchToProfiling();
                    }
                  }
               }
            }

         if (comp()->cg()->getSupportsBigDecimalLongLookasideVersioning() &&
             comp()->isOutermostMethod() &&
             !optimizer()->isIlGenOpt())
            {
            TR::TreeTop *prevTree = tt->getPrevTreeTop();
            TR::TreeTop *newTree = detectBDPattern(tt, exit, node);
            if (newTree)
               {
               postProcessTreesForOSR(prevTree->getNextTreeTop(), newTree);
               tt = newTree;
               }
            }

         if (comp()->isOptServer() &&
             comp()->cg()->getSupportsBigDecimalLongLookasideVersioning() &&
             comp()->isOutermostMethod() &&
             !optimizer()->isIlGenOpt())
            {
            TR::TreeTop *prevTree = tt->getPrevTreeTop();
            TR::TreeTop *newTree = detectSubMulSetScalePattern(tt, exit, callNode);
            if (newTree)
               {
               postProcessTreesForOSR(prevTree->getNextTreeTop(), newTree);
               tt = newTree;
               }
            }
         }
      }
   }

struct TR_BDChain
   {
   TR_ALLOC(TR_Memory::LocalOpts)

      TR_BDChain(TR::RecognizedMethod recognizedMethod, TR::Node *node, TR::TreeTop *tt, int32_t childNum, TR_BDChain *prev = NULL, TR_BDChain *next = NULL, int32_t scale1 = -1, int32_t scale2 = -1, int32_t resultScale = -1) : _recognizedMethod(recognizedMethod), _node(node), _tt(tt), _childNum(childNum), _prev(prev), _next(next), _scale1(scale1), _scale2(scale2), _resultScale(resultScale)
      {
      if (prev)
         prev->_next = this;

      if (next)
         next->_prev = this;
      }

   TR::Node *_node;
   TR::TreeTop *_tt;
   int32_t _childNum;
   TR_BDChain *_prev;
   TR_BDChain *_next;
   TR::RecognizedMethod _recognizedMethod;
   int32_t _scale1;
   int32_t _scale2;
   int32_t _resultScale;
   };

TR_BDChain *TR_StringPeepholes::detectChain(TR::RecognizedMethod recognizedMethod, TR::TreeTop *cursorTree, TR::Node *cursorNode, TR_BDChain *prevInChain)
   {
   TR::Node *prevNodeInChain = prevInChain->_node;
   TR::Node *chainNode = NULL;
   int32_t childNum = -1;

   if (prevNodeInChain)
      {
      if ((cursorNode->getOpCodeValue() == TR::treetop) || (cursorNode->getOpCodeValue() == TR::NULLCHK))// jump
         cursorNode = cursorNode->getFirstChild();

      if (recognizedMethod == TR::unknownMethod)
         {
         if ((cursorNode->getOpCodeValue() == TR::astore) && (cursorNode->getNumChildren() == 1) &&
             (cursorNode->getFirstChild() == prevNodeInChain))
            {
            chainNode = cursorNode;
            childNum = 1;
            }
         }
      else if ((cursorNode->getOpCodeValue() == TR::acalli) || (cursorNode->getOpCodeValue() == TR::acall))
         {
         TR::SymbolReference *callSymRef = cursorNode->getSymbolReference();
         if (!cursorNode->getSymbolReference()->isUnresolved() &&
             !cursorNode->getSymbolReference()->getSymbol()->castToMethodSymbol()->isHelper())
            {
             if (cursorNode->getSymbolReference()->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == recognizedMethod)
               {
               if (recognizedMethod == TR::java_math_BigDecimal_valueOf)
                  chainNode = cursorNode;
               else
                  {
                  if ((cursorNode->getNumChildren() > 0) &&
                      (cursorNode->getFirstChild() == prevNodeInChain))
                     {
                     chainNode = cursorNode;
                     childNum = 0;
                     }
                  else if ((cursorNode->getNumChildren() > 1) &&
                           (cursorNode->getSecondChild() == prevNodeInChain))
                     {
                     chainNode = cursorNode;
                     childNum = 1;
                     }
                  else if ((cursorNode->getNumChildren() > 2) &&
                           (cursorNode->getChild(2) == prevNodeInChain))
                     {
                     chainNode = cursorNode;
                     childNum = 2;
                     }
                  }
               }
            }
         }
      }

   if (chainNode)
      {
      TR_BDChain *nextInChain = new (trStackMemory()) TR_BDChain(recognizedMethod, chainNode, cursorTree, childNum, prevInChain);
      return nextInChain;
      }

   return NULL;
   }

static TR::RecognizedMethod bdPattern1[] =
   {
   TR::java_math_BigDecimal_valueOf,
   TR::java_math_BigDecimal_subtract,
   TR::java_math_BigDecimal_multiply,
   TR::java_math_BigDecimal_setScale,
   TR::unknownMethod,
   TR::java_math_BigDecimal_valueOf,
   TR::java_math_BigDecimal_add,
   TR::java_math_BigDecimal_add,
   TR::java_math_BigDecimal_multiply,
   TR::java_math_BigDecimal_setScale
   };

static TR::RecognizedMethod bdPattern2[] =
   {
   TR::java_math_BigDecimal_valueOf,
   TR::java_math_BigDecimal_subtract,
   TR::java_math_BigDecimal_multiply,
   TR::java_math_BigDecimal_setScale
   };

static TR::RecognizedMethod bdPattern3[] =
   {
   TR::java_math_BigDecimal_valueOf,
   TR::java_math_BigDecimal_add,
   TR::java_math_BigDecimal_add,
   TR::java_math_BigDecimal_multiply,
   TR::java_math_BigDecimal_setScale
   };

static TR::RecognizedMethod bdPattern4[] =
   {
   TR::java_math_BigDecimal_valueOf_J,
   TR::java_math_BigDecimal_multiply,
   TR::java_math_BigDecimal_setScale
   };

static TR::RecognizedMethod *bdPatterns[TR_StringPeepholes::numBDPatterns] = { bdPattern1, bdPattern2, bdPattern3, bdPattern4 };
static int32_t bdPatternLengths[TR_StringPeepholes::numBDPatterns] = { 10, 4, 5, 3 };


static TR_BDChain *matchBDPattern(TR_BDChain *chain, TR::RecognizedMethod *pattern, int32_t len, TR_ValueProfileInfoManager * profileManager, TR::Compilation *comp, bool trace, bool &obtainProfilingInfo)
   {
   TR_BDChain *cursorChain = chain;
   TR_BDChain *firstInChain = NULL;
   TR_BDChain *lastInChain = NULL;

   bool foundScaleInfo = false;
   bool failedWithoutProfilingInfo = false;
   int32_t curLen = 0;
   while(cursorChain)
      {
      bool matched = false;
      if (trace)
         traceMsg(comp, "BigDecimal (binary op) node %p method %d pattern %d in a chain\n", cursorChain->_node, cursorChain->_recognizedMethod, pattern[curLen]);
     //printf("Found a BigDecimal (binary op) method %d pattern %d in a chain in method %s\n", cursorChain->_recognizedMethod, pattern[curLen], comp->signature()); fflush(stdout);
      if (cursorChain->_recognizedMethod == pattern[curLen])
         {
         matched = true;
         if ((cursorChain->_recognizedMethod == TR::java_math_BigDecimal_add) ||
             (cursorChain->_recognizedMethod == TR::java_math_BigDecimal_subtract) ||
             (cursorChain->_recognizedMethod == TR::java_math_BigDecimal_multiply))
            {
            if (comp->getMethodHotness() != hot)
               {
               matched = false;
               TR::Node *callNode = cursorChain->_node;
               TR_ByteCodeInfo callNodeInfo = callNode->getByteCodeInfo();
               callNodeInfo.setByteCodeIndex(callNodeInfo.getByteCodeIndex()+1);
               TR_BigDecimalValueInfo * valueInfo1 = static_cast<TR_BigDecimalValueInfo*>(profileManager ? profileManager->getValueInfo(callNodeInfo, comp, BigDecimalInfo) : 0);
               callNodeInfo.setByteCodeIndex(callNodeInfo.getByteCodeIndex()+1);
               TR_BigDecimalValueInfo * valueInfo2 = static_cast<TR_BigDecimalValueInfo*>(profileManager ? profileManager->getValueInfo(callNodeInfo, comp, BigDecimalInfo) : 0);

               TR_BigDecimalValueInfo * resultValueInfo = static_cast<TR_BigDecimalValueInfo*>(profileManager ? profileManager->getValueInfo(callNode, comp, BigDecimalInfo) : 0);

               //traceMsg(comp, "Matched a BigDecimal (binary op) method in a chain in method %s with valueInfo %p\n", comp->signature(), valueInfo);
               if (valueInfo1 && (valueInfo1->getTopProbability() == 1.0f) &&
                   valueInfo2 && (valueInfo2->getTopProbability() == 1.0f) &&
                   resultValueInfo && (resultValueInfo->getTopProbability() == 1.0f))
                  {
                  int32_t flag1;
                  int32_t curScale1 = valueInfo1->getTopValue(flag1);

                  int32_t flag2;
                  int32_t curScale2 = valueInfo2->getTopValue(flag2);

                  int32_t resultFlag;
                  int32_t resultScale = resultValueInfo->getTopValue(resultFlag);

                  if ((curScale1 >= 0) && (flag1 == 1) &&
                      (curScale2 >= 0) && (flag2 == 1) &&
                      (resultScale >= 0) && (resultFlag == 1))
                     {
                     foundScaleInfo = true;
                     cursorChain->_scale1 = curScale1;
                     cursorChain->_scale2 = curScale2;
                     cursorChain->_resultScale = resultScale;
                     matched = true;
                     if (trace)
                        {
                        traceMsg(comp, "Matched a BigDecimal (binary op) method with profile info in a chain with scale1 = %d scale2 = %d result scale = %d\n", curScale1, curScale2, resultScale);
                        printf("Matched a BigDecimal (binary op) method with profile info in a chain in method %s with scales = %d and %d result scale = %d\n", comp->signature(), curScale1, curScale2, resultScale); fflush(stdout);
                        }
                     }
                  else if (trace)
                     traceMsg(comp, "0Failed on profile info from %p for a BigDecimal (binary op) method with profile info in a chain with scale1 = %d scale2 = %d\n", TR_PersistentProfileInfo::get(comp), curScale1, curScale2);
                  }
               else if (trace)
                  traceMsg(comp, "1Failed on profile info from %p for a BigDecimal (binary op) method with profile info in a chain \n", TR_PersistentProfileInfo::get(comp));
               }
            else
               {
               failedWithoutProfilingInfo = true;
               }
            }
         }

      if (trace)
         traceMsg(comp, "1 len %d cur %d next %p\n", len, curLen, cursorChain->_next);
      if (matched)
         {
         if (curLen == 0)
            firstInChain = cursorChain;

         if (trace)
            traceMsg(comp, "2 len %d cur %d\n", len, curLen);

         if (curLen == (len-1))
            {
            lastInChain = cursorChain;
            break;
            }

         curLen++;
         }
      else
         {
         if (trace)
            traceMsg(comp, "Failed to match on a big decimal method in a chain at node %p\n", cursorChain->_node);
         curLen = 0;
         }

      cursorChain = cursorChain->_next;
      }

   if (trace)
      traceMsg(comp, "first in chain %p last in chain %p\n", firstInChain, lastInChain);

   if (firstInChain && lastInChain)
      {
      if (failedWithoutProfilingInfo)
         obtainProfilingInfo = true;
      firstInChain->_prev = NULL;
      lastInChain->_next = NULL;
      return firstInChain;
      }

   return NULL;
   }

static TR_BDChain *matchBDPatterns(TR_BDChain *chain, TR_ValueProfileInfoManager * profileManager, TR::Compilation *comp, int32_t &matchedPattern, bool trace, bool &obtainProfilingInfo)
   {
   int32_t i = 0;
   while (i < TR_StringPeepholes::numBDPatterns)
      {
      //traceMsg(comp, "Trying to match pattern %d\n", i);
      TR_BDChain *matchedChain = matchBDPattern(chain, bdPatterns[i], bdPatternLengths[i], profileManager, comp, trace, obtainProfilingInfo);
      if (matchedChain)
         {
         matchedPattern = i;
         return matchedChain;
         }
      i++;
      }

   return NULL;
   }


static void maintainScales(TR_BDChain *chain, int32_t &maxScale, int32_t &minScale)
   {
   int32_t scale1 = chain->_scale1;
   int32_t scale2 = chain->_scale2;
   int32_t resultScale = chain->_resultScale;

   if (scale1 > maxScale)
      maxScale = scale1;

   if (scale2 > maxScale)
      maxScale = scale2;

   if (resultScale > maxScale)
      maxScale = resultScale;

   if (scale1 < minScale)
      minScale = scale1;

   if (scale2 < minScale)
      minScale = scale2;

   if (resultScale < minScale)
      minScale = resultScale;
   }

static bool isRecognizedMethod(TR::Node* node, TR::RecognizedMethod rm) {
   bool result = node->getOpCode().isCall() && !node->getSymbolReference()->isUnresolved();
   if (result) {
      TR::Symbol* sym = node->getSymbolReference()->getSymbol();
      result =
         (sym != NULL) &&
         (sym->getMethodSymbol() != NULL) &&
         (sym->getMethodSymbol()->getRecognizedMethod() == rm);
   }
   return result;
}

TR::TreeTop *TR_StringPeepholes::detectSubMulSetScalePattern(TR::TreeTop *tt, TR::TreeTop *exit, TR::Node *node)
   {
   TR::TreeTop *methodTT = NULL;

   TR::TreeTop *firstTree = tt;
   TR::TreeTop *lastTree = tt;
   TR::Node *subNode = NULL;
   TR::Node *subChild = NULL;
   int32_t subScale = -1;
   TR::Node *mulNode = NULL;
   TR::Node *mulChild = NULL;
   int32_t mulScale = -1;
   bool foundPattern = false;

   if (trace())
      traceMsg(comp(), "Looking for subtract multiply setscale pattern in block %d, staring at tt: %p, node: %p\n", firstTree->getEnclosingBlock()->getNumber(), firstTree, firstTree->getNode());

   for (; !foundPattern && tt != exit; tt = tt->getNextRealTreeTop())
      {
      TR::Node *cursorNode = tt->getNode();

      if ((cursorNode->getOpCodeValue() == TR::treetop) || (cursorNode->getOpCodeValue() == TR::NULLCHK))
         cursorNode = cursorNode->getFirstChild();

      // searching for pattern: subtract, multiply and setScale
      if (cursorNode->getOpCodeValue() == TR::acalli)
         {
         if (!isRecognizedMethod(cursorNode, TR::java_math_BigDecimal_subtract)
             || cursorNode->getReferenceCount() != 2)
            {
            if (trace())
               traceMsg(comp(), "callNode %p is not subtract or its refcount is not 2\n", cursorNode);
            continue;
            }

         subNode = cursorNode;
         subChild = subNode->getChild(subNode->getNumChildren()-1);
         firstTree = tt;

         tt = tt->getNextTreeTop();
         cursorNode = tt->getNode();
         if ((cursorNode->getOpCodeValue() == TR::treetop) || (cursorNode->getOpCodeValue() == TR::NULLCHK))
            cursorNode = cursorNode->getFirstChild();

         // searching for pattern: multiply and setScale
         if (cursorNode->getOpCodeValue() == TR::acalli)
            {
            if (!isRecognizedMethod(cursorNode, TR::java_math_BigDecimal_multiply)
                || cursorNode->getReferenceCount() != 3)
               {
               if (trace())
                  traceMsg(comp(), "callNode %p is not multiply or its refcount is not 3\n", cursorNode);
               continue;
               }

            // check if subtract node is multiply node's second argument
            if (subNode != cursorNode->getChild(cursorNode->getFirstArgumentIndex()+1))
               {
               if (trace())
                  traceMsg(comp(), "multiplyNode %p does not have subtractNode %p as the second child\n", cursorNode, subNode);
               continue;
               }

            mulNode = cursorNode;
            mulChild = mulNode->getChild(mulNode->getNumChildren()-2);

            tt = tt->getNextTreeTop();
            cursorNode = tt->getNode();
            if ((cursorNode->getOpCodeValue() == TR::treetop) || (cursorNode->getOpCodeValue() == TR::NULLCHK))
               cursorNode = cursorNode->getFirstChild();

            // searching for pattern: setScale
            if (cursorNode->getOpCodeValue() == TR::acalli)
               {
               if (!isRecognizedMethod(cursorNode, TR::java_math_BigDecimal_setScale)
                     || cursorNode->getReferenceCount() != 2)
                  {
                  if (trace())
                     traceMsg(comp(), "callNode %p is not setScale or its refcount is not 2\n", cursorNode);
                  continue;
                  }

               // check if setScale nodes's receiver is multiply node
               if (mulNode != cursorNode->getChild(cursorNode->getFirstArgumentIndex()))
                  {
                  if (trace())
                     traceMsg(comp(), "setScaleNode's %p receiver is not multiplyNode %p\n", cursorNode, mulNode);
                  continue;
                  }

               // check if scale is 2
               if (cursorNode->getChild(cursorNode->getNumChildren()-2)->getInt() != 2)
                  {
                  if (trace())
                     traceMsg(comp(), "setScaleNode's %p scale is not 2: %d\n", cursorNode, cursorNode->getChild(cursorNode->getNumChildren()-2)->getInt());
                  continue;
                  }

               // check if round is ROUND_DOWN=1
               if (cursorNode->getChild(cursorNode->getNumChildren()-1)->getInt() != 1)
                  {
                  if (trace())
                     traceMsg(comp(), "setScaleNode's %p rounding is not 1: %d\n", cursorNode, cursorNode->getChild(cursorNode->getNumChildren()-1)->getInt());
                  continue;
                  }

               lastTree = tt;
               foundPattern = true;
               }
               else
                  if (trace())
                     traceMsg(comp(), "callNode %p should be setScale, but is not an iacall\n", cursorNode);
            }
            else
               if (trace())
                  traceMsg(comp(), "callNode %p should be multiply, but is not an iacall\n", cursorNode);
         }
         else
            if (trace())
               traceMsg(comp(), "callNode %p should be subtract, but is not an iacall\n", cursorNode);
      }

   if (foundPattern)
      {
      // get profiling info
      TR_ValueProfileInfoManager * profileManager = TR_ValueProfileInfoManager::get(comp());

      if (comp()->getMethodHotness() > warm &&
          comp()->getMethodHotness() != hot)
         {
         if (trace())
            traceMsg(comp(), "Method hotness: %d\n", comp()->getMethodHotness());

         // get mulscale
         TR_ByteCodeInfo nodeInfo = mulNode->getByteCodeInfo();
         nodeInfo.setByteCodeIndex(nodeInfo.getByteCodeIndex() + 1);
         TR_BigDecimalValueInfo *valueInfo1 = static_cast<TR_BigDecimalValueInfo*>(profileManager ? profileManager->getValueInfo(nodeInfo, comp(), BigDecimalInfo) : 0);
         // restore the index
         nodeInfo.setByteCodeIndex(nodeInfo.getByteCodeIndex() - 1);

         // get subscale
         nodeInfo = subNode->getByteCodeInfo();
         nodeInfo.setByteCodeIndex(nodeInfo.getByteCodeIndex() + 2);
         TR_BigDecimalValueInfo *valueInfo2 = static_cast<TR_BigDecimalValueInfo*>(profileManager ? profileManager->getValueInfo(nodeInfo, comp(), BigDecimalInfo) : 0);
         // restore the index
         nodeInfo.setByteCodeIndex(nodeInfo.getByteCodeIndex() - 2);

         if (valueInfo1 && (valueInfo1->getTopProbability() == 1.0f) &&
             valueInfo2 && (valueInfo2->getTopProbability() == 1.0f))
            {
            int32_t flag1, flag2;
            mulScale = valueInfo1->getTopValue(flag1);
            subScale = valueInfo2->getTopValue(flag2);

            if ((mulScale != 2) || (flag2 != 1) ||
                (subScale < 0) || (subScale > 6) || (flag2 != 1))
               {
               if (trace())
                  traceMsg(comp(), "Failed on profile info from %p for subtract multiply and setscale pattern with sub scale %d, and mul scale %d\n", TR_PersistentProfileInfo::get(comp()), subScale, mulScale);
               return NULL;
               }
            }
         else if (trace())
            {
            traceMsg(comp(), "Failed on profile info from %p for subtract multiply and setscale pattern\n", TR_PersistentProfileInfo::get(comp()));
            return NULL;
            }
         }
      else
         {
         if (comp()->getMethodHotness() == hot && comp()->getRecompilationInfo() &&  performTransformation(comp(), "%smight have simplified big decimal subtract multiply setscale pattern from node [%p] to [%p] if profiling info was available. Switching to profiling.\n", optDetailString(), firstTree->getNode(), lastTree->getNode()))
            {
            if (trace())
               traceMsg(comp(), "Switching method %s to profiling...\n", comp()->signature());
            optimizer()->switchToProfiling();
            }
         return NULL;
         }

      if (trace())
         {
         traceMsg(comp(), "Successfully obtained profile info from %p for subtract multiply and setscale pattern with sub scale %d, and mul scale %d\n", TR_PersistentProfileInfo::get(comp()), subScale, mulScale);
         }

      TR::SymbolReference *callSymRef = comp()->fej9()->findOrCreateMethodSymRef(comp(), comp()->getMethodSymbol(), "java/math/BigDecimal", "java/math/BigDecimal.SMSetScale(Ljava/math/BigDecimal;Ljava/math/BigDecimal;II)Ljava/math/BigDecimal;");
      if (callSymRef &&
          performTransformation(comp(), "%ssimplified big decimal subtract multiply setscale pattern from node [%p] to [%p]\n", optDetailString(), firstTree->getNode(), lastTree->getNode()))
         {
         // do the transformation
         TR::Node *methodCall = TR::Node::createWithSymRef(TR::acall, 4, 4,
               subChild,
               mulChild,
               TR::Node::create(node, TR::iconst, 0, subScale),  //4
               TR::Node::create(node, TR::iconst, 0, mulScale),  //2
               callSymRef);

         if (trace())
            traceMsg(comp(), "method call sym ref: %p, method call: %p\n", callSymRef, methodCall);

         TR::TreeTop *treeAfterLast = lastTree->getNextTreeTop();
         TR::Node *lastCall = lastTree->getNode()->getFirstChild();
         int32_t visitCount = comp()->incVisitCount();
         replaceCallNode(lastCall, methodCall, treeAfterLast, visitCount);

         TR::TreeTop *prevTree = firstTree->getPrevTreeTop();
         TR::Node *treeTop = TR::Node::create(TR::treetop, 1, methodCall);
         methodTT = TR::TreeTop::create(comp(), prevTree, treeTop);

         TR::TreeTop *cursorTree = firstTree;
         while (cursorTree != treeAfterLast)
            {
            TR::TreeTop *nextTree = cursorTree->getNextTreeTop();
            TR::TransformUtil::removeTree(comp(), cursorTree);
            cursorTree = nextTree;
            }

         if (trace())
            traceMsg(comp(), "Replaced big decimal sub mul setscale in method %s with call %p\n", comp()->signature(), methodCall);
         }
      }
   else if (trace())
      {
      traceMsg(comp(), "Failed to find subtract multiply setscale pattern in block %d\n", firstTree->getEnclosingBlock()->getNumber());
      }

   return methodTT;
   }


TR::TreeTop *TR_StringPeepholes::detectBDPattern(TR::TreeTop *tt, TR::TreeTop *exit, TR::Node *node)
   {
   TR::TreeTop *privateBDMethodCall = NULL;

   TR_BDChain *prevInChain = NULL;
   TR_BDChain *firstInChain = NULL;
   if ((node->getOpCodeValue() == TR::acall) &&
        node->getSymbolReference()->getSymbol()->isResolvedMethod() &&
       ((node->getSymbolReference()->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigDecimal_valueOf) ||
        (node->getSymbolReference()->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigDecimal_valueOf_J)))
      {
      prevInChain = new (trStackMemory()) TR_BDChain(node->getSymbolReference()->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod(), node, tt, -1);
      if (!firstInChain)
         firstInChain = prevInChain;

      tt = tt->getNextTreeTop();
      while (tt != exit)
         {
         if (tt->getNode()->getOpCode().isAnchor() ||
             skipNodeUnderOSR(tt->getNode()))
            {
            tt = tt->getNextTreeTop();
            continue;
            }

         TR_BDChain *nextInChain = detectChain(TR::java_math_BigDecimal_add, tt, tt->getNode(), prevInChain);
         if (!nextInChain)
            nextInChain = detectChain(TR::java_math_BigDecimal_subtract, tt, tt->getNode(), prevInChain);
         if (!nextInChain)
            nextInChain = detectChain(TR::java_math_BigDecimal_multiply, tt, tt->getNode(), prevInChain);
         if (!nextInChain)
            nextInChain = detectChain(TR::java_math_BigDecimal_setScale, tt, tt->getNode(), prevInChain);
         if (!nextInChain)
            nextInChain = detectChain(TR::java_math_BigDecimal_valueOf, tt, tt->getNode(), prevInChain);
         if (!nextInChain)
            nextInChain = detectChain(TR::java_math_BigDecimal_valueOf_J, tt, tt->getNode(), prevInChain);
         if (!nextInChain)
            nextInChain = detectChain(TR::unknownMethod, tt, tt->getNode(), prevInChain);

         if (!nextInChain)
            break;
         else
            prevInChain = nextInChain;

         tt = tt->getNextTreeTop();
         }
      }

   TR_ValueProfileInfoManager * profileManager = TR_ValueProfileInfoManager::get(comp());
   int32_t matchedPattern = -1;
   bool obtainProfilingInfo = false;
   TR_BDChain *matchedChain = matchBDPatterns(firstInChain, profileManager, comp(), matchedPattern, trace(), obtainProfilingInfo);

   if (trace())
      traceMsg(comp(), "For sequence starting at %p matched chain = %p\n", node, matchedChain);

   if (matchedChain)
      {
      TR::TreeTop *firstTree = NULL;
      TR::TreeTop *lastTree = NULL;

      TR::Node *valueOf1Node = NULL;
      TR::Node *valueOf2Node = NULL;
      TR::Node *add1Node = NULL;
      TR::Node *add2Node = NULL;
      TR::Node *subtractNode = NULL;
      TR::Node *mulNode = NULL;
      TR::Node *mul1Node = NULL;
      int32_t add1ResultScale = -1;
      int32_t add2ResultScale = -1;

      TR::Node *add1Child = NULL;
      TR::Node *add2Child = NULL;
      TR::Node *subChild = NULL;
      TR::Node *mul1Child = NULL;
      int32_t add1Scale = -1;
      int32_t add2Scale = -1;
      int32_t subScale = -1;
      int32_t mul1Scale = -1;

      bool treesAsExpected = true;
      int32_t maxScale = -1;
      int32_t minScale = INT_MAX;

      TR::Symbol *astoreSym = NULL;

      TR_BDChain *cursorChain = matchedChain;

      for (int chainPos = 0; cursorChain; chainPos++)
         {
         if (!firstTree)
            firstTree = cursorChain->_tt;

         if (cursorChain->_recognizedMethod == TR::java_math_BigDecimal_valueOf)
            {
            TR::Node *valueOfNode = cursorChain->_node;

            if (!valueOf1Node)
               valueOf1Node = valueOfNode;
            else
               valueOf2Node = valueOfNode;

            if (matchedPattern <= 2)
               {
               if ((valueOfNode->getFirstChild()->getOpCodeValue() == TR::lconst) &&
                   (valueOfNode->getSecondChild()->getOpCodeValue() == TR::iconst))
                  {
                  if (valueOfNode->getFirstChild()->getLongInt() != 1)
                     {
                     if (trace())
                        traceMsg(comp(), "Failed at 0\n");
                     treesAsExpected = false;
                     }
                  if (valueOfNode->getSecondChild()->getInt() != 0)
                     {
                     if (trace())
                        traceMsg(comp(), "Failed at 1\n");
                     treesAsExpected = false;
                     }
                  }
               else
                  {
                  if (trace())
                     traceMsg(comp(), "Failed at 2\n");
                  treesAsExpected = false;
                  }
               }
            }

         if (cursorChain->_recognizedMethod == TR::java_math_BigDecimal_valueOf_J)
            {
            valueOf1Node = cursorChain->_node;
            }

         if (cursorChain->_recognizedMethod == TR::java_math_BigDecimal_add)
            {
            maintainScales(cursorChain, maxScale, minScale);
            if (!add1Node)
               {
               add1Node = cursorChain->_node;
               add1ResultScale = cursorChain->_resultScale;
               if (matchedPattern == 0 || matchedPattern == 2)
                  {
                  TR::Node *valueOfNode;

                  if (valueOf2Node)
                     valueOfNode = valueOf2Node;
                  else
                     valueOfNode = valueOf1Node;

                  if ((add1Node->getChild(add1Node->getNumChildren()-2) != valueOfNode)  ||
                      ((add1Node->getOpCode().isCallIndirect() && (valueOfNode->getReferenceCount() != 3)) ||
                      (!add1Node->getOpCode().isCallIndirect() && (valueOfNode->getReferenceCount() != 2))) )
                     {
                     if (trace())
                        traceMsg(comp(), "Failed at 3 at add node %p valueOfNode %p\n", add1Node, valueOfNode);
                     treesAsExpected = false;
                     }
                  else
                     {
                     add1Scale = cursorChain->_scale2;
                     add1Child = add1Node->getChild(add1Node->getNumChildren()-1);
                     }
                  }
               }
            else
               {
               add2Node = cursorChain->_node;
               add2ResultScale = cursorChain->_resultScale;
               if (matchedPattern == 0 || matchedPattern == 2)
                  {
                  if ((add2Node->getChild(add2Node->getNumChildren()-2) != add1Node)  ||
                      ((add2Node->getOpCode().isCallIndirect() && (add1Node->getReferenceCount() != 3)) ||
                      (!add2Node->getOpCode().isCallIndirect() && (add1Node->getReferenceCount() != 2))) )
                     {
                     if (trace())
                        traceMsg(comp(), "Failed at 4\n");
                     treesAsExpected = false;
                     }
                  else
                     {
                     add2Scale = cursorChain->_scale2;
                     add2Child = add2Node->getChild(add2Node->getNumChildren()-1);
                     }
                  }
               }
            }

         if (cursorChain->_recognizedMethod == TR::java_math_BigDecimal_subtract)
            {
            maintainScales(cursorChain, maxScale, minScale);
            subtractNode = cursorChain->_node;
            if (matchedPattern <= 1)
               {
               if ((subtractNode->getChild(subtractNode->getNumChildren()-2) != valueOf1Node) ||
                   ((subtractNode->getOpCode().isCallIndirect() && (valueOf1Node->getReferenceCount() != 3)) ||
                   (!subtractNode->getOpCode().isCallIndirect() && (valueOf1Node->getReferenceCount() != 2))) )
                  {
                  if (trace())
                     traceMsg(comp(), "Failed at 5 sub = %p valueOf = %p\n", subtractNode, valueOf1Node);
                  treesAsExpected = false;
                  }
               else
                  {
                  subScale = cursorChain->_scale2;
                  subChild = subtractNode->getChild(subtractNode->getNumChildren()-1);
                  }
               }
            }

         if (cursorChain->_recognizedMethod == TR::java_math_BigDecimal_multiply)
            {
            maintainScales(cursorChain, maxScale, minScale);
            mulNode = cursorChain->_node;
            TR::Node *mulChild = NULL;
            int32_t mulScale = 0;

            if (matchedPattern <= 1 && chainPos == 2)
               {
               if ((mulNode->getChild(mulNode->getNumChildren()-1) != subtractNode)  ||
                   ((mulNode->getOpCode().isCallIndirect() && (subtractNode->getReferenceCount() > 3)) ||
                   (!mulNode->getOpCode().isCallIndirect() && (subtractNode->getReferenceCount() > 2)))  )
                  {
                  if (trace())
                     traceMsg(comp(), "Failed at 6\n");
                  treesAsExpected = false;
                  }
               else
                  {
                  mulScale = cursorChain->_scale1;
                  mulChild = mulNode->getChild(mulNode->getNumChildren()-2);
                  }
               }

            if ((matchedPattern == 0 && chainPos == 8) || (matchedPattern == 2 && chainPos == 3))
               {
               if ((mulNode->getChild(mulNode->getNumChildren()-1) != add2Node)  ||
                   ((mulNode->getOpCode().isCallIndirect() && (add2Node->getReferenceCount() > 3)) ||
                   (!mulNode->getOpCode().isCallIndirect() && (add2Node->getReferenceCount() > 2))) )
                  {
                  if (trace())
                     traceMsg(comp(), "Failed at 7\n");
                  treesAsExpected = false;
                  }
               else
                  {
                  mulScale = cursorChain->_scale1;
                  mulChild = mulNode->getChild(mulNode->getNumChildren()-2);
                  }
               }

            if ((matchedPattern == 0) && (chainPos == 8) && astoreSym && (astoreSym != mulNode->getChild(mulNode->getNumChildren()-2)->getSymbolReference()->getSymbol()))
               {
               if (trace())
                  traceMsg(comp(), "Failed at 8\n");
               treesAsExpected = false;
               }

            if (matchedPattern == 3)
               {
               if ((mulNode->getChild(mulNode->getNumChildren()-2) != valueOf1Node)  ||
                   ((mulNode->getOpCode().isCallIndirect() && (valueOf1Node->getReferenceCount() > 3)) ||
                   (!mulNode->getOpCode().isCallIndirect() && (valueOf1Node->getReferenceCount() > 2))) )
                  {
                  if (trace())
                     traceMsg(comp(), "Failed at 9\n");
                  treesAsExpected = false;
                  }
               else
                  {
                  mulScale = cursorChain->_scale2;
                  mulChild = mulNode->getChild(mulNode->getNumChildren()-1);
                  }
               }
            if (!mul1Child)
               {
               mul1Scale = mulScale;
               mul1Child = mulChild;
               }
            }

         if (cursorChain->_recognizedMethod == TR::java_math_BigDecimal_setScale)
            {
            TR::Node *setScaleNode = cursorChain->_node;

            if (matchedPattern <= 3)
               {
               if ((setScaleNode->getChild(setScaleNode->getNumChildren()-3) != mulNode)  ||
                   ((setScaleNode->getOpCode().isCallIndirect() && (mulNode->getReferenceCount() != 3)) ||
                   (!setScaleNode->getOpCode().isCallIndirect() && (mulNode->getReferenceCount() != 2))) )
                  {
                  if (trace())
                     traceMsg(comp(), "Failed at 10\n");
                  treesAsExpected = false;
                  }

               TR::Node *child = setScaleNode->getChild(setScaleNode->getNumChildren()-2);
               if ((child->getOpCodeValue() != TR::iconst) || (child->getInt() != 2))
                  {
                  if (trace())
                     traceMsg(comp(), "Failed at 11\n");
                  treesAsExpected = false;
                  }

               child = setScaleNode->getChild(setScaleNode->getNumChildren()-1);
               if ((child->getOpCodeValue() != TR::iconst) || (child->getInt() != 4))
                  {
                  if (trace())
                     traceMsg(comp(), "Failed at 12\n");
                  treesAsExpected = false;
                  }
               }
            }

         if (cursorChain->_recognizedMethod == TR::unknownMethod)
            {
            astoreSym = cursorChain->_node->getSymbolReference()->getSymbol();
            TR::Node *astoreNode = cursorChain->_node;
            if (!astoreSym || !astoreSym->isAuto() || !astoreSym->getAutoSymbol() ||
                !astoreNode || (astoreNode->getNumChildren() != 1) || (astoreNode->getOpCodeValue() != TR::astore))
               {
               if (trace())
                  traceMsg(comp(), "Failed at 13\n");
               treesAsExpected = false;
               }
            }

         lastTree = cursorChain->_tt;
         cursorChain = cursorChain->_next;
         }

      if ((minScale < 0) || (maxScale > 6))
         {
         if (trace())
            traceMsg(comp(), "Failed at 14 with minScale %d and maxScale %d\n", minScale, maxScale);
         if (comp()->getMethodHotness() != hot)
            treesAsExpected = false;
         else
            obtainProfilingInfo = true;
         }

      if (!obtainProfilingInfo && !findSymRefForOptMethod((StringpeepholesMethods)matchedPattern))
         {
         if (trace())
            traceMsg(comp(), "Failed at 15 with no private method found\n");
         treesAsExpected = false;
         }

      //Private methods MSS, SMSS, AAMSS, SMAAMSS are only correct if mul1Scale == 2
      if (!obtainProfilingInfo && (mul1Scale != 2))
         {
         if (trace())
            traceMsg(comp(), "Failed at 16 with mul1Scale != 2\n");
         treesAsExpected = false;
         }

      //Private methods AAMSS, SMAAMSS are only correct if add1Scale == add2Scale
      if (!obtainProfilingInfo && (add1Scale != add2Scale))
         {
         if (trace())
            traceMsg(comp(), "Failed at 17 with add1Scale != add2Scale\n");
         treesAsExpected = false;
         }

      //Private method SMAAMSS has the extra requirement that add1Scale == add2Scale == subScale
      if (!obtainProfilingInfo && (add1Scale != subScale) && (matchedPattern == 3))
         {
         if (trace())
            traceMsg(comp(), "Failed at 18 with (add1Scale != subScale) && (matchedPattern == 3)\n");
         treesAsExpected = false;
         }

      if (treesAsExpected && obtainProfilingInfo)
         {
         if(comp()->getMethodHotness() == hot && comp()->getRecompilationInfo() &&  performTransformation(comp(), "%smight have simplified big decimal pattern from node [%p] to [%p] pattern=%d if profiling info was available. Switching to profiling.\n", optDetailString(), firstTree->getNode(), lastTree->getNode(), matchedPattern))
            optimizer()->switchToProfiling();
         return NULL;
         }
      else if (treesAsExpected &&
          performTransformation(comp(), "%ssimplified big decimal pattern from node [%p] to [%p] pattern=%d\n", optDetailString(), firstTree->getNode(), lastTree->getNode(), matchedPattern))
         {
         TR::TreeTop *prevTree = firstTree;
         TR::TreeTop *treeAfterLast = lastTree->getNextTreeTop();

         TR::SymbolReference *callSymRef = findSymRefForOptMethod((StringpeepholesMethods)matchedPattern);
         TR::Node *methodCall = NULL;

         callSymRef = callSymRef ? getSymRefTab()->findOrCreateMethodSymbol(valueOf1Node->getSymbolReference()->getOwningMethodIndex(), -1, callSymRef->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod(), TR::MethodSymbol::Static) : 0;
         if (matchedPattern == 0)
            {
            methodCall = TR::Node::createWithSymRef(TR::acall, 9, 4,
                                         valueOf1Node, subChild, mul1Child, add1Child, callSymRef);

            methodCall->setAndIncChild(4, add2Child);
            methodCall->setAndIncChild(5, TR::Node::create(node, TR::iconst, 0, subScale));
            methodCall->setAndIncChild(6, TR::Node::create(node, TR::iconst, 0, mul1Scale));
            methodCall->setAndIncChild(7, TR::Node::create(node, TR::iconst, 0, add1Scale));
            methodCall->setAndIncChild(8, TR::Node::create(node, TR::iconst, 0, add2Scale));
            }
         else if (matchedPattern == 1)
            {
            methodCall = TR::Node::createWithSymRef(TR::acall, 5, 5,
                                         valueOf1Node, subChild, mul1Child,
                                         TR::Node::create(node, TR::iconst, 0, subScale),
                                         TR::Node::create(node, TR::iconst, 0, mul1Scale),
                                         callSymRef);
            }
         else if (matchedPattern == 2)
            {
            methodCall = TR::Node::createWithSymRef(TR::acall, 7, 4,
                                         valueOf1Node, add1Child, add2Child, mul1Child, callSymRef);

            methodCall->setAndIncChild(4, TR::Node::create(node, TR::iconst, 0, add1Scale));
            methodCall->setAndIncChild(5, TR::Node::create(node, TR::iconst, 0, add2Scale));
            methodCall->setAndIncChild(6, TR::Node::create(node, TR::iconst, 0, mul1Scale));
            }
         else if (matchedPattern == 3)
            {
            methodCall = TR::Node::createWithSymRef(TR::acall, 3, 3,
                                         valueOf1Node, mul1Child, TR::Node::create(node, TR::iconst, 0, mul1Scale),
                                         callSymRef);
            if (comp()->target().cpu.isPower())
               {
               static bool disableBDPrefetch = (feGetEnv("TR_DisableBDPrefetch") != NULL);
               TR::Node *sourceNode = NULL;
               if (comp()->useCompressedPointers() && (TR::Compiler->om.compressedReferenceShiftOffset() == 0))
                  {
                  if (mul1Child->getOpCodeValue() == TR::l2a &&
                      mul1Child->getFirstChild()->getOpCodeValue() == TR::iu2l &&
                      mul1Child->getFirstChild()->getFirstChild() &&
                      mul1Child->getFirstChild()->getFirstChild()->getOpCodeValue() == TR::iloadi &&
                      mul1Child->getFirstChild()->getFirstChild()->getNumChildren() == 1)
                      {
                      sourceNode = mul1Child->getFirstChild()->getFirstChild();
                      }
                  else if (mul1Child->getOpCodeValue() == TR::aloadi)
                      {
                      sourceNode = mul1Child;
                      }
                  }
               else
                  sourceNode = mul1Child;

               if (!disableBDPrefetch && sourceNode &&
                   comp()->getMethodHotness() >= scorching &&
                   sourceNode->getOpCode().hasSymbolReference() &&
                   sourceNode->getSymbolReference()->getSymbol()->getKind() == TR::Symbol::IsShadow)
                  {
                  TR::Node *targetNode;
                  // For the method where this pattern is found, the first store would be the right place.
                  for (TR::TreeTop *tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
                     {
                     targetNode = tt->getNode();
                     if (targetNode->getOpCodeValue() == TR::compressedRefs) // New for 64bit detection
                        targetNode = targetNode->getFirstChild();

                     if (targetNode->getOpCode().hasSymbolReference() &&
                         targetNode->getSymbolReference() == sourceNode->getSymbolReference() &&
                        (targetNode->getOpCodeValue() == TR::astorei || targetNode->getOpCodeValue() == TR::awrtbari))
                        {

                        int32_t len;
                        TR::SymbolReference *symRef = sourceNode->getSymbolReference();
                        const char *fieldName = symRef->getOwningMethod(comp())->fieldName(symRef->getCPIndex(), len, comp()->trMemory());
                        TR_OpaqueClassBlock *clazz = comp()->fej9()->getSystemClassFromClassName("java/math/BigDecimal", strlen("java/math/BigDecimal"));
                        int32_t scaleOffset = comp()->fej9()->getInstanceFieldOffset(clazz, "scale", strlen("scale"), "I", 1);
                        if (strstr(fieldName, "Ljava/math/BigDecimal") && scaleOffset > 0)
                           {
                           if (comp()->findPrefetchInfo(targetNode)<0)
                              {
                              scaleOffset += comp()->fej9()->getObjectHeaderSizeInBytes();
                              TR_Pair<TR::Node, uint32_t> *prefetchInfo = new (comp()->trHeapMemory()) TR_Pair<TR::Node, uint32_t> (targetNode, (uint32_t *)(intptr_t)scaleOffset);
                              comp()->getNodesThatShouldPrefetchOffset().push_front(prefetchInfo);
                              //printf("Ready to prefetch for the field: %*s, offset %d, in node %p\n", len, fieldName, scaleOffset, targetNode);
                              }
                           }
                        break;
                        }
                     }
                  }
               }
            else if (comp()->target().cpu.isZ())
               {
               TR::Node *pNode = TR::Node::createWithSymRef(node, TR::Prefetch, 4, callSymRef);
               pNode->setAndIncChild(0, mul1Child);

               // TODO: add code to determine offset of instance member scale at JIT Compile Time
               TR::Node * offsetNode = TR::Node::create(node, TR::iconst, 0, 28);
               pNode->setAndIncChild(1, offsetNode);
               TR::Node * size = TR::Node::create(node, TR::iconst, 0, 1);
               pNode->setAndIncChild(2, size);
               TR::Node * type = TR::Node::create(node, TR::iconst, 0, (int32_t)PrefetchLoad);
               pNode->setAndIncChild(3, type);
               TR::Node * tt = TR::Node::create(node, TR::treetop, 1);
               tt->setAndIncChild(0, pNode);
               TR::TreeTop *prefetchTree = TR::TreeTop::create(comp(), tt);
               firstTree->insertBefore(prefetchTree);
               }
            }

         TR::TreeTop *firstTreeToRemove = firstTree->getNextTreeTop();

         TR::Node *lastCall = lastTree->getNode()->getFirstChild();
         vcount_t visitCount = comp()->incVisitCount();
         replaceCallNode(lastCall, methodCall, treeAfterLast, visitCount);

         TR::Node *treeTop = TR::Node::create(TR::treetop, 1, methodCall);
         privateBDMethodCall = TR::TreeTop::create(comp(), prevTree, treeTop);

         TR::TreeTop *cursorTree = firstTreeToRemove;
         while (cursorTree != treeAfterLast)
            {
            TR::TreeTop *nextTree = cursorTree->getNextTreeTop();
            TR::TransformUtil::removeTree(comp(), cursorTree);
            cursorTree = nextTree;
            }

         traceMsg(comp(), " replaced the matched pattern with call %p\n", methodCall);

         if (trace())
            printf("Doing BigDecimal transformation in method %s\n", comp()->signature()); fflush(stdout);
         }
      }

   return privateBDMethodCall;
   }




//-------------------------- detectPattern ----------------------------------
// Collects stats about string peepholes patterns
// tt is the treetop that has the TR::New opcode ( for new(StringBuffer) )
// Will return 0 if it cannot apply the transformation
//---------------------------------------------------------------------------
TR::TreeTop *TR_StringPeepholes::detectPattern(TR::Block *block, TR::TreeTop *tt, bool useStringBuffer)
   {
   TR::TreeTop *newTree   = tt; // make a copy for later use
   TR::TreeTop *startTree = tt->getNextRealTreeTop();
   TR::TreeTop *exitTree  = block->getExit();
   TR::Node    *newBuffer = tt->getNode()->getFirstChild();

   // variables for maintaining status info
   enum {MAX_STRINGS = 5}; // maximum number strings concatenation allowed
   int        initWithString = 0;
   bool       initWithInteger = false;
   TR::TreeTop *appendTree[MAX_STRINGS+1];
   TR::Node    *appendedString[MAX_STRINGS+1];
   char       pattern[MAX_STRINGS+1];  // the type of appended objects S=string O=object U=unknown
   TR::TreeTop *primToStringTree[MAX_STRINGS+1];
   int        stringCount = 0;

   // the following is used when we find an append(I) or something similar that
   // needs conversion to string
   TR::SymbolReference *valueOfSymRef[MAX_STRINGS+1];

   // let's look for StringBuffer.<init> or StringBuilder.<init>
   TR::TreeTop *initTree   = 0;
   vcount_t     visitCount = comp()->incVisitCount();
   tt = searchForInitCall((useStringBuffer ? "java/lang/StringBuffer.<init>(":"java/lang/StringBuilder.<init>("),
                          startTree, // where to start searching
                          exitTree,  // where to stop searching
                          newBuffer, // the node that contains the TR::New
                          visitCount,
                          &initTree);// result returned in this variable

   if (initTree)
      {
      // analyze which type on init we have
      TR::Symbol *symbol = tt->getNode()->getFirstChild()->getSymbolReference()->getSymbol();
      TR_ResolvedMethod *m = symbol->castToResolvedMethodSymbol()->getResolvedMethod();
      if (strncmp(m->signatureChars(), "()V", m->signatureLength())==0)
         {
         }
      else if (strncmp(m->signatureChars(), "(Ljava/lang/String;)V", m->signatureLength())==0)
         {
         initWithString = 1;
         pattern[0] = 'S';
         appendTree[stringCount] = 0;
         primToStringTree[stringCount] = 0;
         appendedString[stringCount] = initTree->getNode()->getFirstChild()->getSecondChild();
         valueOfSymRef[stringCount] = 0;
         stringCount++;
         }
      else if (strncmp(m->signatureChars(), "(I)V", m->signatureLength())==0)
         {
         initWithInteger = true;
         }
      else
         {
         //fprintf(stderr, "Unknown type of <init>\n");
         return 0;
         }
      }
   else // <init> not found (could be unresolved)
      {
      return 0;
      }

   // now search for StringBuffer.append calls that are chained to one another
   TR::TreeTop *lastAppendTree = 0; // updated when we find an append
   TR::Node    *child = newBuffer;

   while (1)
      {
      startTree = tt->getNextRealTreeTop();
      appendedString[stringCount] = 0;
      primToStringTree[stringCount] = 0;
      visitCount = comp()->incVisitCount();
      if (useStringBuffer)
         tt = searchForStringAppend("java/lang/StringBuffer.append(",
                                    startTree, exitTree, TR::acall, child, visitCount,
                                    appendedString + stringCount, primToStringTree + stringCount);
      else
         tt = searchForStringAppend("java/lang/StringBuilder.append(",
                                    startTree, exitTree, TR::acall, child, visitCount,
                                    appendedString + stringCount, primToStringTree + stringCount);
      if (appendedString[stringCount]) // we found it
         {
         appendTree[stringCount] = tt;

         // we could exit here if too many appends are chained
         if (stringCount >= MAX_STRINGS)
            return 0;

         // see which type of append we have
         TR::Symbol *symbol = tt->getNode()->getFirstChild()->getSymbolReference()->getSymbol();
         TR_ASSERT(symbol->isResolvedMethod(), "assertion failure");
         TR::ResolvedMethodSymbol *method = symbol->castToResolvedMethodSymbol();
         TR_ASSERT(method, "assertion failure");
         TR_ResolvedMethod *m = method->getResolvedMethod();

         if (primToStringTree[stringCount])
            {
            pattern[stringCount] = 'I';
            if (!_valueOfISymRef)  // try to find the symbol ref for String.valueOf
               _valueOfISymRef = findSymRefForValueOf("(I)Ljava/lang/String;");
            valueOfSymRef[stringCount] = _valueOfISymRef;
            }
         else if (strncmp(m->signatureChars(), "(Ljava/lang/String;)", 20) == 0)
            {
            pattern[stringCount] = 'S';
            valueOfSymRef[stringCount] = 0; // don't need conversion to string
            }
         else // appending something that needs conversion using valueOf
            {
            TR::SymbolReference *symRefForValueOf = 0;
            // In the following we can compare only (C) because we know that
            // StringBuffer.append returns a StringBuffer.
            //
            char *sig = m->signatureChars();
            TR_ASSERT(m->signatureLength() >= 3, "The minimum signature length should be 3 for ()V");

            if (sig[1]=='C' && sig[2]==')')
               {
               pattern[stringCount] = 'C';
               if (!_valueOfCSymRef)  // try to find the symbol ref for String.valueOf
                  _valueOfCSymRef = findSymRefForValueOf("(C)Ljava/lang/String;");
               symRefForValueOf = _valueOfCSymRef;
               }
            else if (sig[1]=='I' && sig[2]==')')
               {
               pattern[stringCount] = 'I';
               if (!_valueOfISymRef)  // try to find the symbol ref for String.valueOf
                  _valueOfISymRef = findSymRefForValueOf("(I)Ljava/lang/String;");
               symRefForValueOf = _valueOfISymRef;
               }
            else if (strncmp(sig, "(Ljava/lang/Object;)", 20)==0)
               {
               pattern[stringCount] = 'O';
               if (!_valueOfOSymRef)  // try to find the symbol ref for String.valueOf
                  _valueOfOSymRef = findSymRefForValueOf("(Ljava/lang/Object;)Ljava/lang/String;");
               symRefForValueOf = _valueOfOSymRef;
               }
            else if (sig[1]=='Z' && sig[2]==')')
               {
               pattern[stringCount] = 'Z';
               if (!_valueOfZSymRef)  // try to find the symbol ref for String.valueOf
                  _valueOfZSymRef = findSymRefForValueOf("(Z)Ljava/lang/String;");
               symRefForValueOf = _valueOfZSymRef;
               }
            else if (sig[1]=='J' && sig[2]==')')
               {
               pattern[stringCount] = 'J';
               if (!_valueOfJSymRef)  // try to find the symbol ref for String.valueOf
                  _valueOfJSymRef = findSymRefForValueOf("(J)Ljava/lang/String;");
               symRefForValueOf = _valueOfJSymRef;
               }
            else // some other type of append
               {
               //pattern[stringCount] = 'U';
               return 0; // exit abruptly
               }
            if (!symRefForValueOf) // cannot convert the object to a string
               return 0;
            // memorize info needed for conversion
            valueOfSymRef[stringCount] = symRefForValueOf;
            }
         stringCount++;
         }
      else // the chain of appends is broken
         {
         appendTree[stringCount] = 0;
         primToStringTree[stringCount] = 0;
         pattern[stringCount] = 0; // string terminator
         break;
         }
      lastAppendTree = tt;
      child = tt->getNode()->getFirstChild(); // the first node is a NULLCHK and its child is the call
      } // end while

   //traceMsg(comp(), "Reached here for %p count %d init1 %p init2 %p\n", tt->getNode(), stringCount, _initSymRef1, _initSymRef2);

   if (stringCount < 2)
      return 0; // cannot apply StringPeepholes
   if (stringCount > MAX_STRINGS)
      return 0;
   if (stringCount == 2 && !findSymRefForOptMethod(SPH_String_init_SS))
      return 0; // cannot do it because we do not have the appropriate constructor
   if (stringCount == 3 && !findSymRefForOptMethod(SPH_String_init_SSS))
      return 0; // same as above

   if ((stringCount > 3))
      {
      if ((stringCount == 5) &&
          (pattern[0] == 'I') &&
          (pattern[1] == 'S') &&
          (pattern[2] == 'I') &&
          (pattern[3] == 'S') &&
          (pattern[4] == 'S') &&
           primToStringTree[0] && primToStringTree[2] &&
           findSymRefForOptMethod(SPH_String_init_ISISS))
         {
         }
      else
         return 0;
      }


   TR_ASSERT(lastAppendTree, "If stringCount <=2 then we must have found an append");

   // now look for the toString call
   TR::TreeTop *toStringTree = 0;
   visitCount = comp()->incVisitCount();
   tt = searchForToStringCall(lastAppendTree->getNextRealTreeTop(), exitTree,
                              lastAppendTree->getNode()->getFirstChild(),
                              visitCount, &toStringTree, useStringBuffer);
   if (!toStringTree)
      return 0;



   //--- do the transformation ---

   //   ORIGINAL                             AFTER TRANSFORMATIONS
   //treetop                                 will disappear
   //  new
   //    loadaddr [StringBuffer]
   //NULLCHK                                 will disappear
   //  vcall [StringBuffer.<init>]
   //    ==>new
   //NULLCHK                                 treetop
   //  acall [StringBuffer.append]             aload string1
   //    ==>new
   //    aload string1
   //NULLCHK                                 treetop
   //  acall [StringBuffer.append]             aload string2
   //    ==>acall
   //    aload string2
   //NULLCHK                                 treetop
   //  acall[StringBuffer.toString()]          new
   //    ==>acall                                loadaddr [String]
   //                                        treetop
   //                                          vcall [String.<init>]
   //                                            ==>new
   //                                            ==>aload
   //                                            ==>aload
   //
   if (!performTransformation(comp(), "%ssimplified string concatenation starting at node [%p] pattern=%s\n", optDetailString(), newTree->getNode(), pattern))
      return 0;
   //fprintf(stderr, "Applying pattern %s to method %s\n", pattern, comp()->signature());

   // eliminate the appends and leave only loads to the objects
   // referred by the appends
   for (int i = stringCount-1; i >= 0; i--)
      {
      if (appendTree[i])
         {
         removePendingPushOfResult(appendTree[i]);
         appendTree[i]->getNode()->recursivelyDecReferenceCount();
         TR::Node::recreate(appendTree[i]->getNode(), TR::treetop);
         appendTree[i]->getNode()->setNumChildren(1);
         appendTree[i]->getNode()->setAndIncChild(0,appendedString[i]);
         }
      else
         {
         // when initWithString, the first appendTree will be empty
         TR_ASSERT(i==0 && initWithString, "assertion failure");
         }

      if (primToStringTree[i])
         {
         removePendingPushOfResult(primToStringTree[i]);
         TR::Node *firstChild = primToStringTree[i]->getNode()->getFirstChild()->getFirstChild();
         primToStringTree[i]->getNode()->recursivelyDecReferenceCount();
         TR::Node::recreate(primToStringTree[i]->getNode(), TR::treetop);
         primToStringTree[i]->getNode()->setNumChildren(1);
         primToStringTree[i]->getNode()->setAndIncChild(0,firstChild);
         }
      }

   // For string+C and string+I we have a special constructor. Determine if this is the case.
   bool specialCase = (stringCount == 2 && pattern[0] == 'S' &&
                       ((pattern[1] == 'C' && findSymRefForOptMethod(SPH_String_init_SC)) || (pattern[1] == 'I' && findSymRefForOptMethod(SPH_String_init_SI))));

   if (!specialCase)
      {
      if ((stringCount == 5) &&
          (pattern[0] == 'I') &&
          (pattern[1] == 'S') &&
          (pattern[2] == 'I') &&
          (pattern[3] == 'S') &&
          (pattern[4] == 'S') &&
          findSymRefForOptMethod(SPH_String_init_ISISS))
         specialCase = true;
      }


   TR::Node *stringNode = toStringTree->getNode()->getFirstChild();

   // if we found append of something other than a string we need to convert
   // them to strings using String.valueOf
   if (!specialCase)
      for (int j=0; j < stringCount; j++)
         if (valueOfSymRef[j])
            {
            // create a new treetop for String.valueOf()
            //                                                      nodeToConvert
            TR::SymbolReference * symRef = getSymRefTab()->findOrCreateMethodSymbol((appendedString[j] && appendedString[j]->getOpCode().hasSymbolReference()) ? appendedString[j]->getSymbolReference()->getOwningMethodIndex() : JITTED_METHOD_INDEX, -1, valueOfSymRef[j]->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod(), TR::MethodSymbol::Static);
            TR::Node *valueOf = TR::Node::createWithSymRef(stringNode, TR::acall, 1, appendedString[j], symRef);
            TR::TreeTop::create(comp(), appendTree[j],
                               TR::Node::create(stringNode, TR::treetop, 1, valueOf));
            // modify the content of the appendString[i] so that we take the
            // result from the newly created valueOf node
            appendedString[j] = valueOf;
            dumpOptDetails(comp(), "%s added valueOf call [%p]\n", optDetailString(), valueOf);
            }

   // transform the toStringTree into
   // treetop
   //   new
   //     loadaddr (string class)
   TR::Node::recreate(toStringTree->getNode(), TR::treetop);
   stringNode->getFirstChild()->recursivelyDecReferenceCount();
   TR::Node::recreate(stringNode, TR::New);
   stringNode->setNumChildren(1);
   stringNode->setSymbolReference(getSymRefTab()->findOrCreateNewObjectSymbolRef(_methodSymbol));
   stringNode->setAndIncChild(0, TR::Node::createWithSymRef(stringNode, TR::loadaddr, 0, _stringSymRef));
   // create a new tree like
   // treetop
   //   vcall Method <init>
   //     ==>new
   //     ==>aload
   //     ==>aload
   TR::Node *initCall = NULL;

   if (stringCount == 2)
      {
      if (specialCase)
         {
            if (pattern[1] == 'C')
            {
               TR::SymbolReference *initSymRef = findSymRefForOptMethod(SPH_String_init_SC) ? getSymRefTab()->findOrCreateMethodSymbol(stringNode->getSymbolReference()->getOwningMethodIndex(), -1, findSymRefForOptMethod(SPH_String_init_SC)->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod(), TR::MethodSymbol::Special) : 0;
               initCall = TR::Node::createWithSymRef(TR::call, 3, 3,
                                          stringNode, appendedString[0], appendedString[1],
                                          initSymRef);
            }
            else
            {
               TR::SymbolReference *initSymRef3 = findSymRefForOptMethod(SPH_String_init_SI) ? getSymRefTab()->findOrCreateMethodSymbol(stringNode->getSymbolReference()->getOwningMethodIndex(), -1, findSymRefForOptMethod(SPH_String_init_SI)->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod(), TR::MethodSymbol::Special) : 0;
               initCall = TR::Node::createWithSymRef(TR::call, 3, 3,
                                          stringNode, appendedString[0], appendedString[1],
                                          initSymRef3);
            }
         }
      else // use the string + string constructor
         {
         TR::SymbolReference *initSymRef1 = findSymRefForOptMethod(SPH_String_init_SS) ? getSymRefTab()->findOrCreateMethodSymbol(stringNode->getSymbolReference()->getOwningMethodIndex(), -1, findSymRefForOptMethod(SPH_String_init_SS)->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod(), TR::MethodSymbol::Special) : 0;
         initCall = TR::Node::createWithSymRef(TR::call, 3, 3,
                                    stringNode, appendedString[0], appendedString[1],
                                    initSymRef1);
         }
      }
   else if (stringCount == 3)
      {
      TR::SymbolReference *initSymRef2 = findSymRefForOptMethod(SPH_String_init_SSS) ? getSymRefTab()->findOrCreateMethodSymbol(stringNode->getSymbolReference()->getOwningMethodIndex(), -1, findSymRefForOptMethod(SPH_String_init_SSS)->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod(), TR::MethodSymbol::Special) : 0;
      initCall = TR::Node::createWithSymRef(TR::call, 4, 4,
                                 stringNode, appendedString[0], appendedString[1], appendedString[2],
                                 initSymRef2);
      }
   else if (stringCount == 5)
      {
      TR::SymbolReference *initSymRef7 = findSymRefForOptMethod(SPH_String_init_ISISS) ? getSymRefTab()->findOrCreateMethodSymbol(stringNode->getSymbolReference()->getOwningMethodIndex(), -1, findSymRefForOptMethod(SPH_String_init_ISISS)->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod(), TR::MethodSymbol::Special) : 0;
      initCall = TR::Node::createWithSymRef(TR::call, 6, 6,
                                 stringNode, appendedString[0], appendedString[1], appendedString[2], appendedString[3], appendedString[4],
                                 initSymRef7);
      }

   //printf("Before call to create.  initCall = %p",initCall); fflush(stdout);
   TR::Node *treeTop = TR::Node::create(stringNode, TR::treetop, 1, initCall);
   TR::TreeTop *initCallTree = TR::TreeTop::create(comp(), toStringTree, treeTop);
   genFlush(initCallTree, initCall->getFirstChild());

   removePendingPushOfResult(initTree);
   if (!initWithString)
      {
      TR::TransformUtil::removeTree(comp(), initTree);
      }
   else
      {
      // we have to add a load for the string that is attached to the init
      initTree->getNode()->recursivelyDecReferenceCount();
      TR::Node::recreate(initTree->getNode(), TR::treetop);
      initTree->getNode()->setNumChildren(1);
      initTree->getNode()->setAndIncChild(0,appendedString[0]);
      }

   removeAllocationFenceOfNew(newTree);
   removePendingPushOfResult(newTree);
   TR::TransformUtil::removeTree(comp(), newTree);

   dumpOptDetails(comp(), "%s added init call [%p]\n", optDetailString(), initCall);
   return toStringTree;
   }


TR::SymbolReference *TR_StringPeepholes::findSymRefForValueOf(const char *sig)
   {
   // try to find the symbol ref for String.valueOf
   TR_OpaqueClassBlock *stringClass = comp()->getStringClassPointer();

   // It is possible for getStringClassPointer to return NULL in an AOT compilation
   if (!stringClass && comp()->compileRelocatableCode())
      comp()->failCompilation<TR::CompilationException>("StringPeepholes: stringClass is NULL");

   TR_ASSERT_FATAL(stringClass, "stringClass should not be NULL\n");
   TR_ResolvedMethod *method = comp()->fej9()->getResolvedMethodForNameAndSignature(trMemory(), stringClass, "valueOf", sig);
   return method ? getSymRefTab()->findOrCreateMethodSymbol(JITTED_METHOD_INDEX, -1, method, TR::MethodSymbol::Static) : NULL;
   }




// NOTE: this method assumes that the second parm is a c-style constant string literal
//
bool TR_StringPeepholes::checkMethodSignature(TR::SymbolReference *symRef, const char *sig)
   {
   TR::Symbol *symbol = symRef->getSymbol();
   if (!symbol->isResolvedMethod())
      return false;

   TR::ResolvedMethodSymbol *method = symbol->castToResolvedMethodSymbol();
   if (!method) return false;

   if (strncmp(method->getResolvedMethod()->signature(trMemory()), sig, strlen(sig)) == 0)
      return true;
   return false;
   }

/*
 * In postExecutionOSR, there will be several bookkeeping nodes between chained calls.
 * This helper will identify them so that they can be skipped.
 */
bool TR_StringPeepholes::skipNodeUnderOSR(TR::Node *node)
   {
   bool result = false;
   if (comp()->getOption(TR_EnableOSR)
      && comp()->isOSRTransitionTarget(TR::postExecutionOSR))
      {
      if (comp()->getMethodSymbol()->isOSRRelatedNode(node))
         {
         result = true;
         }
      }

   if (!result &&
       node->getOpCodeValue() == TR::treetop &&
       node->getFirstChild()->isPotentialOSRPointHelperCall())
      {
      result = true;
      }

   if (result && trace())
      traceMsg(comp(), "Skipping OSR node [%p] n%dn\n", node, node->getGlobalIndex());

   return result;
   }

/*
 * In postExecutionOSR, the result of a call will always be stored into a pending push temp.
 * Several calls will be removed under this optimization, so this helper will find and remove
 * the matching store.
 */
void TR_StringPeepholes::removePendingPushOfResult(TR::TreeTop *callTreeTop)
   {
   if (comp()->isOSRTransitionTarget(TR::postExecutionOSR) && comp()->getOption(TR_EnableOSR))
      {
      TR::TreeTop *cursor = callTreeTop->getNextTreeTop();
      while (cursor && comp()->getMethodSymbol()->isOSRRelatedNode(cursor->getNode()))
         {
         if (cursor->getNode()->getFirstChild() == callTreeTop->getNode()->getFirstChild())
            TR::TransformUtil::removeTree(comp(), cursor);
         cursor = cursor->getNextTreeTop();
         }
      }
   }

// If new tree is going to be removed, the allocationfence on the new node
// has to be removed as well
void TR_StringPeepholes::removeAllocationFenceOfNew(TR::TreeTop *newTreeTop)
   {
   TR::TreeTop *cursor = newTreeTop->getNextTreeTop();
   if (cursor
       && cursor->getNode()->getOpCodeValue() == TR::allocationFence
       && cursor->getNode()->getAllocation() == newTreeTop->getNode()->getFirstChild())
      {
      TR::TransformUtil::removeTree(comp(), cursor);
      }
   }

/*
 * In HCR, these optimizations cannot be performed if the classes have already been redefined.
 * StringBuffer and StringBuilder calls will be removed and replaced, whilst Integer calls will
 * be used for conversions to strings.
 */
bool TR_StringPeepholes::classesRedefined()
   {
   if (!comp()->getOption(TR_EnableHCR))
      return false;

   TR_OpaqueClassBlock *clazz = comp()->fej9()->getSystemClassFromClassName("java/lang/StringBuffer", strlen("java/lang/StringBuffer"));
   if (classRedefined(clazz))
      return true;

   clazz = comp()->fej9()->getSystemClassFromClassName("java/lang/StringBuilder", strlen("java/lang/StringBuilder"));
   if (classRedefined(clazz))
      return true;

   clazz = comp()->fej9()->getSystemClassFromClassName("java/lang/Integer", strlen("java/lang/Integer"));
   if (classRedefined(clazz))
      return true;

   return false;
   }

bool TR_StringPeepholes::classRedefined(TR_OpaqueClassBlock *clazz)
   {
   // The class in question may not have been loaded yet. For example in JDK11 the JCL removed most (all?) uses of the
   // StringBuffer class, thus this class is never loaded until `main` is reached. If the class is not loaded it could
   // not have been redefined, so we return false here.
   if (clazz == NULL)
      {
      return false;
      }

   TR_PersistentClassInfo *clazzInfo = comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(clazz, fe());
   return !clazzInfo || clazzInfo->classHasBeenRedefined();
   }

TR::TreeTop *TR_StringPeepholes::searchForStringAppend(const char *sig, TR::TreeTop *tt, TR::TreeTop *exitTree,
                                                      TR::ILOpCodes opCode, TR::Node *newBuffer, vcount_t visitCount, TR::Node **string, TR::TreeTop **prim)
   {
   for (;tt != exitTree; tt = tt->getNextRealTreeTop())
      {
      TR::Node *node = tt->getNode();

      if (skipNodeUnderOSR(node))
         {
         if (trace())
            traceMsg(comp(), "Skipping OSR node [%p] when searching for append\n", node);
         continue;
         }

      if (node->getNumChildren() == 1 &&
          node->getFirstChild()->getOpCodeValue() == opCode)
         {
         if (checkMethodSignature(node->getFirstChild()->getSymbolReference(), sig))
            {
            TR::Node *call = node->getFirstChild();
            if (call->getFirstChild() == newBuffer)
               *string = call->getSecondChild();
            return tt;
            }
         else
            {
            char *sig2 = "java/lang/Integer.toString(I)";

            // Expected reference count for the Integer.toString may change if pending pushes are being
            // generated
            int refCount = 2;
            bool seenRef = true;
            if (comp()->isOSRTransitionTarget(TR::postExecutionOSR) && comp()->getOption(TR_EnableOSR))
               {
               refCount = 3;
               seenRef = false;
               }

            if ((node->getFirstChild()->getReferenceCount() == refCount) &&
                checkMethodSignature(node->getFirstChild()->getSymbolReference(), sig2))
               {
               TR_ASSERT(node->getFirstChild()->getNumChildren() == 1, "The number of children of java/lang/Integer.toString(I) should be equal to 1");

               TR::TreeTop *cursor = tt->getNextRealTreeTop();
               TR::Node *prevCall = node->getFirstChild();
               while (skipNodeUnderOSR(cursor->getNode()))
                  {
                  if (trace())
                     traceMsg(comp(), "Skipping OSR node [%p] when searching for append with integer\n", node);

                  // Check the additional reference to Integer.toString is seen
                  if (cursor->getNode()->getOpCode().isStoreDirect() && cursor->getNode()->getFirstChild() == prevCall)
                     seenRef = true;

                  cursor = cursor->getNextRealTreeTop();
                  }

               node = cursor->getNode();
               if (seenRef &&
                   node->getNumChildren() == 1 &&
                   node->getFirstChild()->getOpCodeValue() == opCode)
                  {
                  if (checkMethodSignature(node->getFirstChild()->getSymbolReference(), sig))
                     {
                     TR::Node *call = node->getFirstChild();
                     if (call->getFirstChild() == newBuffer)
                        {
                        *string = prevCall->getFirstChild();
                        *prim = tt;
                        }
                     return cursor;
                     }
                  }
               }
            }
         }

      if (countNodeOccurrencesInSubTree(node, newBuffer, visitCount) > 0) return tt;
      }

   return tt;
   }



TR::TreeTop *TR_StringPeepholes::searchForInitCall(const char *sig, TR::TreeTop *tt,
                                                  TR::TreeTop *exitTree, TR::Node *newBuffer,
                                                  vcount_t visitCount, TR::TreeTop **initTree)
   {
   for (;tt != exitTree; tt = tt->getNextRealTreeTop())
      {
      TR::Node *node = tt->getNode();

      if (skipNodeUnderOSR(node))
         {
         if (trace())
            traceMsg(comp(), "Skipping OSR node [%p] when searching for init\n", node);
         continue;
         }

      if (node->getNumChildren() == 1 &&
          node->getFirstChild()->getOpCodeValue() == TR::call)
         {
         if (checkMethodSignature(node->getFirstChild()->getSymbolReference(), sig))
            {
            TR::Node *call = node->getFirstChild();
            if (call->getFirstChild() == newBuffer)
               *initTree = tt;
            return tt;
            }
         }

      if (countNodeOccurrencesInSubTree(node, newBuffer, visitCount) > 0) return tt;
      }

   return tt;
   }


TR::TreeTop *TR_StringPeepholes::searchForToStringCall(TR::TreeTop *tt, TR::TreeTop *exitTree,
                                                      TR::Node *newBuffer, vcount_t visitCount,
                                                      TR::TreeTop **toStringTree, bool useStringBuffer)
   {
   for (;tt != exitTree; tt = tt->getNextRealTreeTop())
      {
      TR::Node *node = tt->getNode();

      if (skipNodeUnderOSR(node))
         {
         if (trace())
            traceMsg(comp(), "Skipping OSR node [%p] when searching for toString\n", node);
         continue;
         }

      if (node->getNumChildren() == 1 &&
          node->getFirstChild()->getOpCodeValue() == TR::acall)
         {
         if (checkMethodSignature(node->getFirstChild()->getSymbolReference(),
                                  (useStringBuffer ?
                                   "java/lang/StringBuffer.toString()Ljava/lang/String;" :
                                   "java/lang/StringBuilder.toString()Ljava/lang/String;")))
            {
            TR::Node *call = node->getFirstChild();
            if (call->getFirstChild() == newBuffer)
               *toStringTree = tt;
            return tt;
            }
         }

      if (countNodeOccurrencesInSubTree(node, newBuffer, visitCount) > 0) return tt;
      }

   return tt;
   }

void TR_StringPeepholes::postProcessTreesForOSR(TR::TreeTop *startTree, TR::TreeTop *endTree)
   {
   if (comp()->supportsInduceOSR() &&
       comp()->isOSRTransitionTarget(TR::postExecutionOSR) &&
       comp()->getOSRMode() == TR::voluntaryOSR)
      {
      if (trace())
         {
         TR::Node* startNode = startTree->getNode();
         TR::Node* endNode = endTree->getNode();
         traceMsg(comp(), "Post process Trees from %p n%dn to %p n%dn for OSR\n", startNode, startNode->getGlobalIndex(), endNode, endNode->getGlobalIndex());
         }

      TR::TransformUtil::removePotentialOSRPointHelperCalls(comp(), startTree, endTree);
      TR::TransformUtil::prohibitOSROverRange(comp(), startTree, endTree);
      }
   }
