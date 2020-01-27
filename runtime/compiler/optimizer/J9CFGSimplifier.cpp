/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#include "compile/Method.hpp"
#include "optimizer/CFGSimplifier.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/TransformUtil.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"

#define OPT_DETAILS "O^O CFG SIMPLIFICATION: "

bool J9::CFGSimplifier::simplifyIfPatterns(bool needToDuplicateTree)
   {
   static char *enableCFGSimplification = feGetEnv("TR_enableCFGSimplificaiton");
   if (enableCFGSimplification == NULL)
      return false;

   return OMR::CFGSimplifier::simplifyIfPatterns(needToDuplicateTree)
          || simplifyResolvedRequireNonNull(needToDuplicateTree)
          || simplifyUnresolvedRequireNonNull(needToDuplicateTree)
          ;
   }

// Look for pattern of the form:
//
// ifacmpeq block_A
//   ... some ref
//   aconst NULL
//
// OR
//
// ifacmpne block_A
//   ... some ref
//   aconst NULL
//
// Where block_A looks like:
// ResolveCHK
//    loadaddr
// treetop
//    new
//       => loadaddr
// ResolveAndNULLCHK
//    call java/lang/NullPointerException.<init>();
//       => new
// NULLCHK
//    athrow
//       => new

bool J9::CFGSimplifier::simplifyUnresolvedRequireNonNull(bool needToDuplicateTree)
   {
   static char *disableSimplifyExplicitNULLTest = feGetEnv("TR_disableSimplifyExplicitNULLTest");
   static char *disableSimplifyUnresolvedRequireNonNull = feGetEnv("TR_disableSimplifyUnresolvedRequireNonNull");
   if (disableSimplifyExplicitNULLTest != NULL || disableSimplifyUnresolvedRequireNonNull != NULL)
      return false;

   if (comp()->getOSRMode() == TR::involuntaryOSR)
         return false;

   if (trace())
      traceMsg(comp(), "Start simplifyUnresolvedRequireNonNull\n");

   // This block must end in an ifacmpeq or ifacmpne against aconst NULLa
   TR::TreeTop *compareTreeTop = getLastRealTreetop(_block);
   TR::Node *compareNode       = compareTreeTop->getNode();
   if (compareNode->getOpCodeValue() != TR::ifacmpeq
       && compareNode->getOpCodeValue() != TR::ifacmpne)
      return false;

   if (trace())
      traceMsg(comp(), "   Found an ifacmp[eq/ne] n%dn\n", compareNode->getGlobalIndex());

   if (compareNode->getSecondChild()->getOpCodeValue() != TR::aconst
       || compareNode->getSecondChild()->getAddress() != 0)
      return false;

   // _next1 is fall through so grab the block where the value is NULL
   TR::Block *nullBlock = compareNode->getOpCodeValue() == TR::ifacmpeq ? _next2 : _next1;
   TR::Block *nonnullBlock = compareNode->getOpCodeValue() == TR::ifacmpeq ? _next1 : _next2;

   if (trace())
      traceMsg(comp(), "  Matched nullBlock %d\n", nullBlock->getNumber());

   TR::TreeTop *nullBlockCursor = nullBlock->getEntry()->getNextTreeTop();

   if (nullBlockCursor->getNode()->getOpCodeValue() != TR::ResolveCHK
       || nullBlockCursor->getNode()->getFirstChild()->getOpCodeValue() != TR::loadaddr)
      return false;

   if (trace())
      traceMsg(comp(), "   Match ResolveCHK of loadaddr\n");

   TR::Node *loadaddr = nullBlockCursor->getNode()->getFirstChild();
   nullBlockCursor = nullBlockCursor->getNextTreeTop();

   if (nullBlockCursor->getNode()->getOpCodeValue() != TR::treetop
       || nullBlockCursor->getNode()->getFirstChild()->getOpCodeValue() != TR::New
       || nullBlockCursor->getNode()->getFirstChild()->getFirstChild() != loadaddr)
      return false;

   TR::Node *exceptionNode = nullBlockCursor->getNode()->getFirstChild();

   if (trace())
      traceMsg(comp(), "   Matched new of loadaddr\n");

   nullBlockCursor = nullBlockCursor->getNextTreeTop();

   // optionally match pending push store
   if (nullBlockCursor->getNode()->getOpCodeValue() == TR::astore
       && nullBlockCursor->getNode()->getFirstChild() == exceptionNode
       && nullBlockCursor->getNode()->getSymbol()->isPendingPush())
      nullBlockCursor = nullBlockCursor->getNextTreeTop();

   if (nullBlockCursor->getNode()->getOpCodeValue() != TR::ResolveAndNULLCHK
       || nullBlockCursor->getNode()->getFirstChild()->getOpCodeValue() != TR::call
       || nullBlockCursor->getNode()->getFirstChild()->getFirstChild() != exceptionNode)
      return false;

   TR::Node *initCall = nullBlockCursor->getNode()->getFirstChild();

   if (trace())
      traceMsg(comp(), "   Matched call node %d\n", initCall->getGlobalIndex());

   if (!initCall->getSymbolReference()->isUnresolved())
      return false;

   TR::Method *calleeMethod = initCall->getSymbol()->castToMethodSymbol()->getMethod();
   if (trace())
      traceMsg(comp(), "   Matched calleeMethod %s %s %s\n", calleeMethod->classNameChars(), calleeMethod->nameChars(), calleeMethod->signatureChars());
   if (strncmp(calleeMethod->nameChars(), "<init>", 6) != 0
       || strncmp(calleeMethod->classNameChars(), "java/lang/NullPointerException", 30) != 0
       || strncmp(calleeMethod->signatureChars(), "()V", 3) != 0)
      return false;


   if (trace())
      traceMsg(comp(), "   Matched NPE init\n");

   nullBlockCursor = nullBlockCursor->getNextTreeTop();
   if ((nullBlockCursor->getNode()->getOpCodeValue() != TR::NULLCHK
        && nullBlockCursor->getNode()->getOpCodeValue() != TR::treetop)
       || nullBlockCursor->getNode()->getFirstChild()->getOpCodeValue() != TR::athrow
       || nullBlockCursor->getNode()->getFirstChild()->getFirstChild() != exceptionNode)
      return false;

   if (trace())
      traceMsg(comp(), "   Matched throw\n");

   TR::Node *throwNode = nullBlockCursor->getNode()->getFirstChild();

   nullBlockCursor = nullBlockCursor->getNextTreeTop();
   if (nullBlockCursor != nullBlock->getExit())
      return false;

   if (!performTransformation(comp(), "%sReplace ifacmpeq/ifacmpne of NULL node [%p] to throw of an NPE exception with NULLCHK\n", OPT_DETAILS, compareNode))
      return false;

   _cfg->invalidateStructure();

   TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "cfgSimpNULLCHK/unresolvedNonNull/(%s)", comp()->signature()));

   TR::Block *checkBlock = _block;
   if (hasExceptionPoint(_block, compareTreeTop))
      checkBlock = _block->split(compareTreeTop, _cfg, true, false);

   if (!nullBlock->getExceptionSuccessors().empty())
      {
      for (auto itr = nullBlock->getExceptionSuccessors().begin(), end = nullBlock->getExceptionSuccessors().end(); itr != end; ++itr)
         {
         _cfg->addExceptionEdge(checkBlock, (*itr)->getTo());
         }
      }

   TR::Node *passthroughNode = TR::Node::create(throwNode, TR::PassThrough, 1);
   passthroughNode->setAndIncChild(0, compareNode->getFirstChild());
   TR::SymbolReference *symRef = comp()->getSymRefTab()->findOrCreateNullCheckSymbolRef(comp()->getMethodSymbol());
   TR::Node *nullchkNode = TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, passthroughNode, symRef);
   if (trace())
      traceMsg(comp(), "End simplifyUnresolvedRequireNonNull. Generated NULLCHK node n%dn\n", nullchkNode->getGlobalIndex());
   TR::TreeTop *nullchkTree = TR::TreeTop::create(comp(), nullchkNode);
   checkBlock->getEntry()->insertAfter(nullchkTree);

   _cfg->removeEdge(checkBlock, nullBlock);
   TR::TransformUtil::removeTree(comp(), compareTreeTop);

   if (checkBlock->getNextBlock() != nonnullBlock)
      {
      TR::Node *gotoNode = TR::Node::create(nullchkNode, TR::Goto, 0);
      gotoNode->setBranchDestination(nonnullBlock->getEntry());
      checkBlock->append(TR::TreeTop::create(comp(), gotoNode));
      }

   return true;
   }

// Look for pattern of the form:
//
// ifacmpeq block_A
//   ... some ref
//   aconst NULL
//
// OR
//
// ifacmpne block_A
//   ... some ref
//   aconst NULL
//
// Where block_A looks like:
// treetop
//    new
//       loadaddr java/lang/NullPointerException
// treetop | NULLCHK
//    call java/lang/NullPointerException.<init>();
//       => new
// treetop | NULLCHK
//    athrow
//       => new
//
// Replace the branch with a NULLCHK PassThrough of some ref
//
bool J9::CFGSimplifier::simplifyResolvedRequireNonNull(bool needToDuplicateTree)
   {
   static char *disableSimplifyExplicitNULLTest = feGetEnv("TR_disableSimplifyExplicitNULLTest");
   static char *disableSimplifyResolvedRequireNonNull = feGetEnv("TR_disableSimplifyResolvedRequireNonNull");
   if (disableSimplifyExplicitNULLTest != NULL || disableSimplifyResolvedRequireNonNull != NULL)
      return false;

   if (comp()->getOSRMode() == TR::involuntaryOSR)
         return false;

   if (trace())
      traceMsg(comp(), "Start simplifyResolvedRequireNonNull\n");

   // This block must end in an ifacmpeq or ifacmpne against aconst NULL
   TR::TreeTop *compareTreeTop = getLastRealTreetop(_block);
   TR::Node *compareNode       = compareTreeTop->getNode();
   if (compareNode->getOpCodeValue() != TR::ifacmpeq
       && compareNode->getOpCodeValue() != TR::ifacmpne)
      return false;

   if (trace())
      traceMsg(comp(), "   Found an ifacmp[eq/ne] n%dn\n", compareNode->getGlobalIndex());

   if (compareNode->getSecondChild()->getOpCodeValue() != TR::aconst
       || compareNode->getSecondChild()->getAddress() != 0)
      return false;

   // _next1 is fall through so grab the block where the value is NULL
   TR::Block *nullBlock = compareNode->getOpCodeValue() == TR::ifacmpeq ? _next2 : _next1;
   TR::Block *nonnullBlock = compareNode->getOpCodeValue() == TR::ifacmpeq ? _next1 : _next2;

   traceMsg(comp(), "   Found nullBlock %d\n", nullBlock->getNumber());

   TR::TreeTop *nullBlockCursor = nullBlock->getEntry()->getNextTreeTop();
   if (nullBlockCursor->getNode()->getOpCodeValue() != TR::treetop
       || nullBlockCursor->getNode()->getFirstChild()->getOpCodeValue() != TR::New
       || nullBlockCursor->getNode()->getFirstChild()->getFirstChild()->getOpCodeValue() != TR::loadaddr)
      return false;

   if (trace())
      traceMsg(comp(), "   Matched new tree\n");

   TR::Node *exceptionNode = nullBlockCursor->getNode()->getFirstChild();
   TR::Node *loadaddr = nullBlockCursor->getNode()->getFirstChild()->getFirstChild();
   // check for java/lang/NullPointerException as the loadaddr
   TR_OpaqueClassBlock *NPEclazz = comp()->fej9()->getSystemClassFromClassName("java/lang/NullPointerException", strlen("java/lang/NullPointerException"));
   if (loadaddr->getSymbolReference()->isUnresolved()
       || loadaddr->getSymbolReference()->getSymbol()->castToStaticSymbol()->getStaticAddress() != NPEclazz)
      return false;

   if (trace())
      traceMsg(comp(), "   Matched new tree class\n");

   nullBlockCursor = nullBlockCursor->getNextTreeTop();

   // optionally match pending push store
   if (nullBlockCursor->getNode()->getOpCodeValue() == TR::astore
       && nullBlockCursor->getNode()->getFirstChild() == exceptionNode
       && nullBlockCursor->getNode()->getSymbol()->isPendingPush())
      nullBlockCursor = nullBlockCursor->getNextTreeTop();

   if ((nullBlockCursor->getNode()->getOpCodeValue() != TR::treetop
        && nullBlockCursor->getNode()->getOpCodeValue() != TR::NULLCHK)
       || nullBlockCursor->getNode()->getFirstChild()->getOpCodeValue() != TR::call
       || nullBlockCursor->getNode()->getFirstChild()->getFirstChild() != exceptionNode)
      return false;

   if (trace())
      traceMsg(comp(), "   Matched exceptionNode\n");

   TR::Node *initCall = nullBlockCursor->getNode()->getFirstChild();
   if (initCall->getSymbolReference()->isUnresolved())
      return false;

   TR_ResolvedMethod *calleeMethod = initCall->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod();
   if (trace())
      traceMsg(comp(), "   Matched calleeMethod %s %s %s\n", calleeMethod->classNameChars(), calleeMethod->nameChars(), calleeMethod->signatureChars());
   if (strncmp(calleeMethod->nameChars(), "<init>", 6) != 0
       || strncmp(calleeMethod->classNameChars(), "java/lang/Throwable", 19) != 0
       || strncmp(calleeMethod->signatureChars(), "()V", 3) != 0)
      return false;

   if (trace())
      traceMsg(comp(), "   Matched exceptionNode call\n");

   nullBlockCursor = nullBlockCursor->getNextTreeTop();
   if ((nullBlockCursor->getNode()->getOpCodeValue() != TR::treetop
        && nullBlockCursor->getNode()->getOpCodeValue() != TR::NULLCHK)
       || nullBlockCursor->getNode()->getFirstChild()->getOpCodeValue() != TR::athrow
       || nullBlockCursor->getNode()->getFirstChild()->getFirstChild() != exceptionNode)
      return false;

   if (trace())
      traceMsg(comp(), "   Matched exception throw\n");

   TR::Node *throwNode = nullBlockCursor->getNode()->getFirstChild();

   nullBlockCursor = nullBlockCursor->getNextTreeTop();
   if (nullBlockCursor != nullBlock->getExit())
      return false;

   if (!performTransformation(comp(), "%sReplace ifacmpeq/ifacmpne of NULL node [%p] to throw of an NPE exception with NULLCHK\n", OPT_DETAILS, compareNode))
      return false;

   _cfg->invalidateStructure();

   TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "cfgSimpNULLCHK/resolvedNonNull/(%s)", comp()->signature()));

   TR::Block *checkBlock = _block;
   if (hasExceptionPoint(_block, compareTreeTop))
      checkBlock = _block->split(compareTreeTop, _cfg, true, false);

   if (!nullBlock->getExceptionSuccessors().empty())
      {
      for (auto itr = nullBlock->getExceptionSuccessors().begin(), end = nullBlock->getExceptionSuccessors().end(); itr != end; ++itr)
         {
         _cfg->addExceptionEdge(checkBlock, (*itr)->getTo());
         }
      }

   TR::Node *passthroughNode = TR::Node::create(throwNode, TR::PassThrough, 1);
   passthroughNode->setAndIncChild(0, compareNode->getFirstChild());
   TR::SymbolReference *symRef = comp()->getSymRefTab()->findOrCreateNullCheckSymbolRef(comp()->getMethodSymbol());
   TR::Node *nullchkNode = TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, passthroughNode, symRef);
   if (trace())
      traceMsg(comp(), "End simplifyResolvedRequireNonNull. Generated NULLCHK node n%dn\n", nullchkNode->getGlobalIndex());
   TR::TreeTop *nullchkTree = TR::TreeTop::create(comp(), nullchkNode);
   checkBlock->getEntry()->insertAfter(nullchkTree);

   _cfg->removeEdge(checkBlock, nullBlock);
   TR::TransformUtil::removeTree(comp(), compareTreeTop);

   if (checkBlock->getNextBlock() != nonnullBlock)
      {
      TR::Node *gotoNode = TR::Node::create(nullchkNode, TR::Goto, 0);
      gotoNode->setBranchDestination(nonnullBlock->getEntry());
      checkBlock->append(TR::TreeTop::create(comp(), gotoNode));
      }

   return true;
   }
