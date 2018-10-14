/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/MonitorState.hpp"
#include "il/Block.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "infra/Array.hpp"
#include "infra/Cfg.hpp"
#include "infra/List.hpp"
#include "infra/TRCfgEdge.hpp"
#include "infra/TRCfgNode.hpp"
#include "infra/Stack.hpp"



int32_t
J9::SetMonitorStateOnBlockEntry::addSuccessors(
      TR::CFGNode * cfgNode,
      TR_Stack<TR::SymbolReference *> * monitorStack,
      bool traceIt,
      bool dontPropagateMonitor,
      MonitorInBlock monitorType,
      int32_t callerIndex,
      bool walkOnlyExceptionSuccs)
   {
   if (traceIt)
      traceMsg(comp(),
       "\tIn SMSOBE::addSuccessors for cfgNode %d, monitorStack %p dontPropagateMonitor %d monitorType = %d callerIndex %d walkOlyExceptionSuccs %d\n"
            , cfgNode->getNumber(),monitorStack,dontPropagateMonitor,monitorType,callerIndex,walkOnlyExceptionSuccs);

   bool firstSuccessor = true;

   // to reiterate:
   // 3 cases to consider when propagating the monitorStack
   // 1. MonitorEnter
   // Normal Edge       -> propagate
   // Exception Edge    ->
   //   if syncMethodMonitor from callerIndex=-1
   //                   -> propagate
   //   else            -> do not propagate
   //
   // 2. MonitorExit (at this stage the monitor has been temporarily pushed on the monitorStack)
   // Normal Edge       -> pop before propagate
   // Exception Edge    ->
   //   check if the exception successor of the current block has the same caller index or not
   //   if same caller index, then:
   //   a) in a synchronized method, the catch block will eventually unlock the monitor. so we assume
   //   that if the blocks are in the same method, then the monitor will be unlocked by the catch block
   //   b) for a synchronized block, all blocks within the region (including the block with the monexit)
   //   will always branch to the "catch-all" block inserted by the JIT and not to a user catch block.
   //   the catch-all block will unlock the monitor and rethrow the exception.
   //   so in both these cases, we want to push the monitor along the exception successor because the monitor
   //   will be unlocked eventually.
   //
   //   if not the same caller index, then:
   //   a) the monexit block and the exception successor are in different methods (ie. the monexit - regardless of
   //   sync method or sync block, came from an inlined method and the successor is in the caller).
   //   in this case, we dont want to push the monitor along the exception successor because the exception successor
   //   has no idea that it needs to unlock the monitor.
   //
   //                   -> pop before propagate / push before propagate in the case described above
   //   else            -> propagate the monitorStack (with the monitor pushed on it temporarily) ie. do not pop
   //
   // 3. NoMonitor      -> propagate the monitorStack as is
   //
   // return value
   // -1 : default value
   //  1 : if the monitorStack was propagated with the monitor pushed temporarily
   //  0 : otherwise
   //
   int32_t returnValue = -1;

   TR_SuccessorIterator succs(cfgNode);
   for (TR::CFGEdge *edge = succs.getFirst(); edge; edge = succs.getNext())
      {
      TR::Block * succBlock = toBlock(edge->getTo());
      // skip the exception successors for now in this walk
      // they will be processed later
      //
      if (walkOnlyExceptionSuccs && !succBlock->isCatchBlock())
         continue;

      if (succBlock->getEntry())
         {
         bool addInfo = true;
         if (monitorType == MonitorEnter)
            {
            if (traceIt)
               traceMsg(comp(), "\tIn J9::SetMonitorStateOnBlockEntry::addSuccessors monitorType = MonitorEnter  block %d\n", succBlock->getNumber());
            if (succBlock->isCatchBlock() && dontPropagateMonitor)
               {
               returnValue = 0;
               addInfo = false;
               }
            }

         if (monitorType == MonitorExit)
            {
            if (walkOnlyExceptionSuccs)
               {
               if (callerIndex != succBlock->getEntry()->getNode()->getByteCodeInfo().getCallerIndex())
                  {
                  returnValue = 0;
                  addInfo = false;
                  }
               else
                  returnValue = 1; // push the monitor along this exception edge
               }
            else if (succBlock->isCatchBlock()) // already processed during exception successors walk
               continue;
            }

         if (traceIt)
            traceMsg(comp(), "process succBlock %d propagate (t/f: %d) isCatchBlock=%d monitorType=%d callerIndex=%d entryCallerIndex=%d\n", succBlock->getNumber(), addInfo, succBlock->isCatchBlock(), monitorType, callerIndex, succBlock->getEntry()->getNode()->getByteCodeInfo().getCallerIndex());

         bool popMonitor = false;
         if (monitorStack)
            {
            // pop the last element of the stack if dontPropagateMonitor is true
            //
            if (!addInfo &&
                !monitorStack->isEmpty())
               {
               popMonitor = true;
               }

            if (succBlock->getVisitCount() != _visitCount)
               {
               TR_Stack<TR::SymbolReference *> *newMonitorStack = new (trHeapMemory()) TR_Stack<TR::SymbolReference *>(*monitorStack);

               if (traceIt)
                  traceMsg(comp(), "\tIn SMSOnBE::addSuccesors  created newMonitorStack %p and monitorStack %p\n", newMonitorStack,monitorStack);

               if (popMonitor)
                  {
                  if (traceIt)
                     traceMsg(comp(), "popping monitor symRef=%d before propagation\n", newMonitorStack->top()->getReferenceNumber());
                  newMonitorStack->pop();
                  }

               if (_liveMonitorStacks->find(succBlock->getNumber()) != _liveMonitorStacks->end())
                  {
                  _liveMonitorStacks->erase(succBlock->getNumber());
                  }
               (*_liveMonitorStacks)[succBlock->getNumber()] = newMonitorStack;
               if (traceIt)
                  traceMsg(comp(), "adding monitorstack to successor %d (%p size %d)\n", succBlock->getNumber(), newMonitorStack, newMonitorStack->size());
               }
            else
               {
               // the block has been propagated already but we want to verify the monitor state is consistent
               // skip osr blocks here because the monitor state of osrBlocks don't have to be consistent
               if (!succBlock->isOSRCatchBlock() && !succBlock->isOSRCodeBlock())
                  {
                  if (!isMonitorStateConsistentForBlock(succBlock, monitorStack, popMonitor))
                     comp()->cg()->setLmmdFailed();
                  else
                     {
                     if (traceIt)
                        traceMsg(comp(), "verified block_%d monitorState is consistent\n", succBlock->getNumber());
                     }
                  }
               continue;
               }
            }
         else if (succBlock->getVisitCount() == _visitCount)
            {
            if(!succBlock->isOSRCatchBlock() && !succBlock->isOSRCodeBlock())
               {
               if (!isMonitorStateConsistentForBlock(succBlock, monitorStack, popMonitor))
                  comp()->cg()->setLmmdFailed();
               else if (traceIt)
                  traceMsg(comp(), "verified block_%d monitorState is consistent\n", succBlock->getNumber());
               }
            continue;
            }

         if (traceIt)
            traceMsg(comp(), "\tIn SMSOnBE::addSuccessors adding block %d to blocksToVisit\n", succBlock->getNumber());
         _blocksToVisit.push(succBlock);
         }
      }

   return returnValue;
   }

bool
J9::SetMonitorStateOnBlockEntry::isMonitorStateConsistentForBlock(
      TR::Block *block,
      TR_Stack<TR::SymbolReference *> *newMonitorStack,
      bool popMonitor)
   {
   TR_Stack<TR::SymbolReference *> *oldMonitorStack = _liveMonitorStacks->find(block->getNumber()) != _liveMonitorStacks->end() ?
      (*_liveMonitorStacks)[block->getNumber()] : NULL;
   static const bool traceItEnv = feGetEnv("TR_traceLiveMonitors") ? true : false;
   bool traceIt = traceItEnv || comp()->getOption(TR_TraceLiveMonitorMetadata);

   if (traceIt)
      traceMsg(comp(), "MonitorState block_%d: oldMonitorStack %p newMonitorStack %p popMonitor %d\n", block->getNumber(), oldMonitorStack, newMonitorStack, popMonitor);

   // first step: check if both monitor stacks are empty
   bool oldMonitorStackEmpty = false;
   bool newMonitorStackEmpty = false;

   if (!oldMonitorStack || oldMonitorStack->isEmpty())
      oldMonitorStackEmpty = true;
   if (!newMonitorStack || newMonitorStack->isEmpty()
       || (newMonitorStack->size() == 1 && popMonitor))
      newMonitorStackEmpty = true;
   if (oldMonitorStackEmpty != newMonitorStackEmpty)
      {
      if (traceIt)
         traceMsg(comp(), "MonitorState inconsistent for block_%d: oldMonitorStack isEmpty %d, newMonitorStack isEmpty %d\n", block->getNumber(), oldMonitorStackEmpty, newMonitorStackEmpty);
      return false;
      }
   else if (oldMonitorStackEmpty)
      return true;

   // second step: check if the monitor stacks are the same size
   int32_t oldSize = oldMonitorStack->size();
   int32_t newSize = newMonitorStack->size();
   if (popMonitor)
      newSize--;
   if (newSize != oldSize)
      {
      if (traceIt)
         traceMsg(comp(), "MonitorState inconsistent for block_%d: oldMonitorStack size %d, newMonitorStack size %d\n",block->getNumber(), oldSize, newSize);
      return false;
      }

   // third step: check if all the monitors in both stacks are the same
   for (int i = oldMonitorStack->topIndex(); i>= 0; i--)
      {
      if (newMonitorStack->element(i)->getReferenceNumber() != oldMonitorStack->element(i)->getReferenceNumber())
         {
         if (traceIt)
            traceMsg(comp(), "MonitorState inconsistent for block_%d: oldMonitorStack(%d) symRef=%d, newMonitorStack(%d) symRef=%d\n",block->getNumber(), i, oldMonitorStack->element(i)->getReferenceNumber(), i, newMonitorStack->element(i)->getReferenceNumber());
         return false;
         }
      }
   return true;
   }

// this routine is used to decide if the monitorStack needs to be popped
// the analysis needs to be careful in particular for DLT compiles as there are
// several scenarios as the DLT control could land into the middle of a
// nested (several levels deep) synchronized region
// a) if the monexit is at callerIndex=-1,
//      this means that the monitorStack could be imbalanced
//      i) if synchronized method then the syncObjectTemp would have been used to
//      initialize the hidden slot in the DLT entry (monitorStack size is 1) and
//      then control could branch into the middle of a synchronized region (without
//      ever seeing any monents). At the monexit, the analysis would then try to pop
//      the lone monitor on the stack so prevent this by checking if the stack size is 1.
//      This is done because the special slot needs to be live across the entire method
//
//      ii) an improvement to i) is done at blocks that exit the method. in these cases,
//      the analysis would have encountered the monexits corresponding to the synchronized
//      'this' so it needs to empty the stack
//
//      iii) an exception to i) is when the method is a static synchronized method. in this
//      case, the syncObjectTemp is not used to initialize the monitor slot in the DLT entry
//      (so the monitorStack size is 0). so the analysis should avoid any special checks
//
//      in case i) or iii) fails, this means that the DLT control landed straight into the sync
//      region (with no monitorStack), in this case the analysis needs to ensure that an empty
//      monitorStack is not popped
//
//
// b) if the monexit is not at callerIndex=-1, then this means that the monexit was
// part of a monent-monexit region that would have normally appeared in the method. an imbalance
// in the monitorStack indicates an error, but return the default answer as "yes", the stack
// can be popped.
//
// c) if not DLT, then return the default answer as "yes"
//
//
static bool canPopMonitorStack(
      TR::Compilation *comp,
      TR_Stack<TR::SymbolReference *> * monitorStack,
      TR::Node *node,
      bool blockExitsMethod,
      bool traceIt)
   {
   int32_t callerIndex = node->getByteCodeInfo().getCallerIndex();
   if (comp->isDLT())
      {
      if (callerIndex == -1)
         {
         if (comp->getJittedMethodSymbol()->isSynchronised())
            {
            // We have the special slot set up at DLT entry. Let us avoid resetting
            // the bit for this special slot so that we do not have any wrong GC maps
            // TODO : if we need to empty the monitor stack at the end of the method
            // then pop it off the stack only when we reach a block whose successor is
            // the dummy exit block
            //
            if (monitorStack->size() == 1 &&
                  !comp->getJittedMethodSymbol()->isStatic() &&
                  !blockExitsMethod)
               {
               if (traceIt)
                   traceMsg(comp, "monitorStack is empty (except for special DLT sync object slot) for DLT compile at monexit %p\n", node);
               return false;
               }
            else if (monitorStack->isEmpty())
               {
               if (traceIt)
                  traceMsg(comp, "monitorStack is empty for DLT compile at monexit %p\n", node);
               return false;
               }
            }
         else
            {
            if (monitorStack->isEmpty())
               {
                if (traceIt)
                   traceMsg(comp, "monitorStack is empty for non-synchronized DLT compile at monexit %p\n", node);
                return false;
               }
            }
         }
      else
         {
         // TODO : could add an assert error here : check if the elem to be popped off
         // from the monitor stack for your caller index and assert if there is not
         /*
         if (!monitorStack->isEmpty() &&
               monitorStack->top())
            {
            if (monitorStack->top()->getOwningMethodIndex() != callerIndex)
               traceMsg(comp(), "unbalanced monitorStack, trying to pop %d but top is %d symRef: %d\n", callerIndex, monitorStack->top()->getOwningMethodIndex(), monitorStack->top()->getReferenceNumber());
            TR_ASSERT(monitorStack->top()->getOwningMethodIndex() == callerIndex, "unbalanced monitorStack, trying to pop %d but top is %d\n", callerIndex, monitorStack->top()->getOwningMethodIndex());
            }
         */
         return true;
         }
      }

   return true;
   }

static bool needToPushMonitor(TR::Compilation *comp, TR::Block *block, bool traceIt)
   {
   // this routine is needed to decide if the monitor needs to be temporarily
   // pushed back onto the stack for the successor
   // for a sequence such as this for a synchronized block:
   // synchronized {
   // ...
   // }
   // return
   // BBEnd
   //
   // older javac includes the return in the exception range (for the catch-all block)
   // when MethodEnter/MethodExit hooks are enabled, we split the blocks as follows:
   // synchronized {
   // ...
   // }
   // check the hook bit
   // BBEnd
   // BBStart <-- new block after split
   // return
   // BBEnd
   // ...
   // since the return is included in the exception range, the new blocks (after the split)
   // also have exception successors to the catch-all block. this results in unbalanced
   // monitors because we would have popped the monitorStack at the block containing the monexit
   // new javac seem to correctly exclude the return from the exception range
   // To support this case, walk the successors of the current block and check if the successor
   // has identical exception successors.
   //
   // A new case.  When a transaction exists, the monexitfence is in a separate block from the
   // monexit and the tfinish.  So the two blocks will generally have an exception edge with the same destination block
   // So for this case don't return true.  *Shudder*

   bool retval = false;
   for (auto e = block->getSuccessors().begin(); e != block->getSuccessors().end(); ++e)
      {
      TR::Block *succ = (*e)->getTo()->asBlock();
      if (comp->getFlowGraph()->compareExceptionSuccessors(block, succ) == 0)
         {
         if (traceIt)
            traceMsg(comp, "found identical exception successors for block %d and succ %d\n", block->getNumber(), succ->getNumber());

         retval = true;
         for (TR::TreeTop *tt = succ->getEntry(); tt != succ->getExit() ; tt = tt->getNextTreeTop())
            {
            TR::Node *aNode = tt->getNode();

            if (aNode && (aNode->getOpCodeValue() == TR::tfinish || aNode->getOpCodeValue() == TR::monexit ||
                          ((aNode->getOpCodeValue() == TR::treetop|| aNode->getOpCodeValue() == TR::NULLCHK) && aNode->getFirstChild()->getOpCodeValue() == TR::monexit )
                          ))
               {
               if(traceIt)
                  traceMsg(comp, "overriding identical exception decision because node %p in block %d is either monexit or tfinish",aNode,succ->getNumber());
               retval = false;
               break;
               }
            }
         break;
         }
      }
   return retval;
   }

void J9::SetMonitorStateOnBlockEntry::set(bool& lmmdFailed, bool traceIt)
   {
   addSuccessors(comp()->getFlowGraph()->getStart(), 0, traceIt);
   static bool traceInitMonitorsForExceptionAfterMonexit = feGetEnv("TR_traceInitMonitorsForExceptionAfterMonexit")? true: false;

   while (!_blocksToVisit.isEmpty())
      {
      TR::Block * block = _blocksToVisit.pop();
      if (block->getVisitCount() == _visitCount)
         continue;
      block->setVisitCount(_visitCount);

      if (traceIt)
         traceMsg(comp(), "block to process: %d\n", block->getNumber());

      TR_Stack<TR::SymbolReference *> *monitorStack =
         (_liveMonitorStacks->find(block->getNumber()) != _liveMonitorStacks->end()) ?
         (*_liveMonitorStacks)[block->getNumber()] :
         NULL;

      if (traceIt && monitorStack && !monitorStack->isEmpty())
         traceMsg(comp(), "top of the stack symRef=%d, and size=%d\n", monitorStack->top()->getReferenceNumber(), monitorStack->size());
      else if (traceIt)
         traceMsg(comp(), "monitor stack is empty\n");

      bool blockHasMonent = false;
      bool blockHasMonexit = false;

      bool blockExitsMethod = false;
      TR_SuccessorIterator succs(block);
      for (TR::CFGEdge *edge = succs.getFirst(); edge; edge = succs.getNext())
         {
         if (edge->getTo()->getNumber() == comp()->getFlowGraph()->getEnd()->getNumber())
            blockExitsMethod = true;
         }

      bool isSyncMethodMonent = false;
      bool isSyncMethodMonexit = false;
      TR::SymbolReference *monitorStackTop = NULL;
      int32_t callerIndex = -1;
      int32_t monitorPoppedForExceptionSucc = 1;
      int32_t monitorEnterStore = 0;
      int32_t monitorExitFence= 0;
      for (TR::TreeTop * tt = block->getEntry(); ; tt = tt->getNextTreeTop())
         {
         TR::Node * node = tt->getNode();
         if (node->getOpCodeValue() == TR::treetop || node->getOpCodeValue() == TR::NULLCHK)
            node = node->getFirstChild();

         TR::ILOpCodes opCode = node->getOpCodeValue();

         if ((node->getOpCode().isStore() &&            //only monents are represented by this store now
               node->getSymbol()->holdsMonitoredObject() &&
               !node->isLiveMonitorInitStore()))
            {
            //problem with lmmd only occurs when the exception successor is not 0
            if(!block->getExceptionSuccessors().empty())
               monitorEnterStore++;

            callerIndex = node->getByteCodeInfo().getCallerIndex();
            if (monitorStack)
               {
               monitorStack = new (trHeapMemory()) TR_Stack<TR::SymbolReference *>(*monitorStack);
               if (traceIt)
                  traceMsg(comp(), "adding monitor to stack symbol=%p symRef=%d (size=%d) (node %p)\n", node->getSymbol(), node->getSymbolReference()->getReferenceNumber(), monitorStack->size()+1,node);
               }
            else
               {
               monitorStack = new (trHeapMemory()) TR_Stack<TR::SymbolReference *>(trMemory());
               if (traceIt)
                  traceMsg(comp(), "adding monitor to fresh stack symbol=%p symRef=%d (size=%d) (node %p)\n", node->getSymbol(), node->getSymbolReference()->getReferenceNumber(), monitorStack->size()+1,node);
               }

            monitorStack->push(node->getSymbolReference());
            blockHasMonent = true;

            // if the callerIndex of the node is *not* -1, then this node
            // came from a synchronized method that was inlined. in this case,
            // don't push the monitor info along exception successors for this block.
            // the typical pattern should be:
            // ...
            // astore <holdsMonitoredObject>
            // monent
            // BBEnd   // exception successor of this block will not unlock the object (it
            //         // actually belongs to the caller
            // BBStart //start of inlined method, these blocks will have the catchall block
            //         //that unlocks the monitor
            //
            if (node->getSymbolReference()->holdsMonitoredObjectForSyncMethod() &&
                  (callerIndex == -1))
               isSyncMethodMonent = true;
            }
         else if ( (node->getOpCode().getOpCodeValue() == TR::monexitfence) &&
                    monitorStack && !monitorStack->isEmpty() &&
                    canPopMonitorStack(comp(), monitorStack, node, blockExitsMethod, traceIt))
            {
            if(!block->getExceptionSuccessors().empty())
               monitorExitFence++;

            // The check for this assume was moved in the if statement above.
            // JCK has tests for unbalanced monitor exits and we would crash during compilation
            // if we tried to pop a non-existent monitor off the stack.
            // TR_ASSERT(!monitorStack->isEmpty(), "monitor stack is empty at block %d node %p\n",
            //         block->getNumber(), node);
            //
            monitorStackTop = monitorStack->top();
            if (monitorStackTop && monitorStackTop->holdsMonitoredObjectForSyncMethod())
               isSyncMethodMonexit = true;
            blockHasMonexit = true;
            callerIndex = node->getByteCodeInfo().getCallerIndex();

            ///traceMsg(comp(), "blockHasMonexit = %d isSyncMethodMonitor = %d\n", blockHasMonexit, isSyncMethodMonitor);
            // process all the exception successors at this point
            // the normal successors will be processed at the end of the block
            //
            monitorPoppedForExceptionSucc = addSuccessors(block, monitorStack, traceIt, false /*not used*/, MonitorExit, callerIndex, true /*walkOnlyExceptionSuccs*/);
            // monexit
            if (monitorStack->topIndex() == 0)
               {
               monitorStack = new (trHeapMemory()) TR_Stack<TR::SymbolReference *>(*monitorStack);
               if (traceIt)
                  traceMsg(comp(), "popping monitor off stack symRef=%d, BEFORE pop size=%d, ", monitorStack->top()->getReferenceNumber(), monitorStack->size());
               monitorStack->pop();
               if (traceIt)
                  traceMsg(comp(), "AFTER size=%d\n", monitorStack->size());
               }

            else
               {
               monitorStack = new (trHeapMemory()) TR_Stack<TR::SymbolReference *>(*monitorStack);
               if (traceIt)
                  traceMsg(comp(), "popping monitor off stack symRef=%d, BEFORE pop size=%d, ", monitorStack->top()->getReferenceNumber(), monitorStack->size());
               monitorStack->pop();
               if (traceIt)
                  traceMsg(comp(), "AFTER size=%d\n", monitorStack->size());
               }
            }
         else if(node->getOpCode().getOpCodeValue() != TR::monexit && node->exceptionsRaised())
            {
            if (monitorExitFence > 0)
               {
               auto edge = block->getExceptionSuccessors().begin();
               for (; edge != block->getExceptionSuccessors().end(); ++edge)
                  {
                  TR::Block * succBlock = toBlock((*edge)->getTo());
                  if (node->getByteCodeInfo().getCallerIndex() ==
                        succBlock->getEntry()->getNode()->getByteCodeInfo().getCallerIndex())
                     {
                     if (traceInitMonitorsForExceptionAfterMonexit)
                           traceMsg(comp(), "block_%d has exceptions after monexit with catch block in the same method %s\n", block->getNumber(), comp()->signature());
                     lmmdFailed = true;
                     break;
                     }
                  }
               }
            }

         if (tt == block->getExit())
            {
            bool dontPropagateMonitor = false; // so propagate it by default!
            MonitorInBlock monitorType = NoMonitor;
            if ((monitorExitFence+monitorEnterStore)>= 2)
               {
               if (traceIt)
                  traceMsg(comp(), "block_%d has monitorEnterStore=%d monitorExitFence=%d\n", block->getNumber(), monitorEnterStore, monitorExitFence);
               lmmdFailed = true;
               }

            if (blockHasMonent)
               {
               // the monitorStack will contain the monitor to be pushed
               // along the successors at this point
               // a) if the edge is a normal edge, then just propagate the stack
               // b) if the edge is an exception edge, then only push the monitor on
               // the stack if it is a syncMethodMonitor. otherwise don't propagate
               // the monitor to the exception successors (this is because if there is
               // an exception successor of a block containing the monent and control to
               // the exception is reached, this means that the monitor is not locked, ie
               // there is no monexit to pop the stack)
               //
               monitorType = MonitorEnter;
               dontPropagateMonitor = !isSyncMethodMonent;
               }

            if (blockHasMonexit)
               {
               // the monitorStack will be popped under 2 conditions:
               // a) if the successor is a normal edge (ie. not an exception edge)
               // b) if the successor is an exception edge, then pop the stack *only* if the
               // monexit came from a synchronized method (check the callerIndex on the catch block
               // and the monexit)
               //
               // isSyncMethodMonexit will control b)
               //
               monitorType = MonitorExit;
               dontPropagateMonitor = isSyncMethodMonexit;

               if ((monitorPoppedForExceptionSucc > 0) &&
                     monitorStackTop &&
                     needToPushMonitor(comp(), block, traceIt))
                  {
                  if (traceIt)
                     traceMsg(comp(), "pushing monexit symRef=%d back temporarily\n", monitorStackTop->getReferenceNumber());
                  monitorStack->push(monitorStackTop);
                  }
               }

            if (traceIt)
               traceMsg(comp(), "blockHasMonent=%d blockHasMonexit=%d dontPropagateMonitor=%d callerIndex=%d monitorPoppedForExceptionSucc=%d\n", blockHasMonent, blockHasMonexit, dontPropagateMonitor, callerIndex, monitorPoppedForExceptionSucc);

            addSuccessors(block, monitorStack, traceIt, dontPropagateMonitor, monitorType, callerIndex);
            break;
            }
         }
      }

   static bool disableCountingMonitors = feGetEnv("TR_disableCountingMonitors")? true: false;
   if (lmmdFailed && !disableCountingMonitors)
      {
      TR_Array<List<TR::RegisterMappedSymbol> *> & monitorAutos = comp()->getMonitorAutos();
      for (int32_t i=0; i<monitorAutos.size(); i++)
         {
         List<TR::RegisterMappedSymbol> *autos = monitorAutos[i];
         if (autos)
            {
            ListIterator<TR::RegisterMappedSymbol> iterator(autos);
            for (TR::RegisterMappedSymbol * a = iterator.getFirst(); a; a = iterator.getNext())
               {
               TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "lmmdFailed/(%s)", comp()->signature()));
               a->setUninitializedReference();
               }
            }
         }
      }
   }

