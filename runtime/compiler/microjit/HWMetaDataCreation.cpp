/*******************************************************************************
 * Copyright (c) 2022, 2022 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include <stdint.h>
#include "il/Block.hpp"
#include "il/J9Node_inlines.hpp"
#include "infra/Cfg.hpp"
#include "microjit/MethodBytecodeMetaData.hpp"

#if defined(J9VM_OPT_MJIT_Standalone)
/* do nothing */
#else

namespace MJIT
{

static void
setupNode(
      TR::Node *node,
      uint32_t bcIndex,
      TR_ResolvedMethod *feMethod,
      TR::Compilation *comp)
   {
   node->getByteCodeInfo().setDoNotProfile(0);
   node->setByteCodeIndex(bcIndex);
   node->setInlinedSiteIndex(-10);
   node->setMethod(feMethod->getPersistentIdentifier());
   }

static TR::Block *
makeBlock(
      TR::Compilation *comp,
      TR_ResolvedMethod *feMethod,
      uint32_t start,
      uint32_t end,
      TR::CFG &cfg)
   {
   TR::TreeTop *startTree = TR::TreeTop::create(comp, TR::Node::create(NULL, TR::BBStart, 0));
   TR::TreeTop *endTree = TR::TreeTop::create(comp, TR::Node::create(NULL, TR::BBEnd, 0));
   startTree->join(endTree);
   TR::Block *block = TR::Block::createBlock(startTree, endTree, cfg);
   block->setBlockBCIndex(start);
   block->setNumber(cfg.getNextNodeNumber());
   setupNode(startTree->getNode(), start, feMethod, comp);
   setupNode(endTree->getNode(), end, feMethod, comp);
   cfg.addNode(block);
   return block;
   }

static void
join(
      TR::CFG &cfg,
      TR::Block *src,
      TR::Block *dst,
      bool isBranchDest)
   {
   TR::CFGEdge *e = cfg.addEdge(src, dst);

   // TODO: add case for switch
   if (isBranchDest)
      {
      src->getExit()->getNode()->setBranchDestination(dst->getEntry());
      if (dst->getEntry()->getNode()->getByteCodeIndex() < src->getExit()->getNode()->getByteCodeIndex())
         src->setBranchesBackwards();
      }
   else
      {
      src->getExit()->join(dst->getEntry());
      }
   src->addSuccessor(e);
   dst->addPredecessor(e);
   }

class BytecodeRange
   {

   public:
      int32_t _start;
      int32_t _end;
      BytecodeRange(int32_t start)
         :_start(start)
         ,_end(-1)
         {}
      inline bool isClosed()
         {
         return _end > _start;
         }
      BytecodeRange() = delete;
   };

class BytecodeRangeList
   {

   private:
      BytecodeRange *_range;

   public:
      TR_ALLOC(TR_Memory::UnknownType); // TODO: MicroJIT: create a memory object type for this and get it added to OMR
      BytecodeRangeList *prev;
      BytecodeRangeList *next;
      BytecodeRangeList(BytecodeRange *range)
         :_range(range)
         ,next(NULL)
         ,prev(NULL)
         {}
      inline uint32_t getStart()
         {
         return _range->_start;
         }
      inline uint32_t getEnd()
         {
         return _range->_end;
         }
      inline BytecodeRangeList *insert(BytecodeRangeList *newListEntry)
         {
         if (this == newListEntry || next == newListEntry)
            return this;
         while (_range->_start < newListEntry->getStart())
            {
            if (next)
               {
               next->insert(newListEntry);
               return this;
               }
            else
               { // end of list
               next = newListEntry;
               newListEntry->prev = this;
               newListEntry->next = NULL;
               return this;
               }
            }
         MJIT_ASSERT_NO_MSG(_range->_start != newListEntry->getStart());
         if (prev)
            prev->next = newListEntry;
         newListEntry->next = this;
         newListEntry->prev = prev;
         prev = newListEntry;
         return newListEntry;
         }
   };

class BytecodeIndexList
   {

   private:
      BytecodeIndexList *_prev;
      BytecodeIndexList *_next;
      TR::CFG &_cfg;

   public:
      TR_ALLOC(TR_Memory::UnknownType); //MicroJIT TODO: create a memory object type for this and get it added to OMR
      uint32_t _index;
      BytecodeIndexList(uint32_t index, BytecodeIndexList *prev, BytecodeIndexList *next, TR::CFG &cfg)
         :_index(index)
         ,_prev(prev)
         ,_next(next)
         ,_cfg(cfg)
         {}
      BytecodeIndexList() = delete;
      BytecodeIndexList *appendBCI(uint32_t i)
         {
         if (i == _index)
            return this;
         if (_next)
            return _next->appendBCI(i);
         BytecodeIndexList *newIndex = new (_cfg.getInternalRegion()) BytecodeIndexList(i, this, _next, _cfg);
         _next = newIndex;
         return newIndex;
         }
      BytecodeIndexList *getBCIListEntry(uint32_t i)
         {
         if (i == _index)
            {
            return this;
            }
         else if (i < _index)
            {
            return _prev->getBCIListEntry(i);
            }
         else if (_next && _next->_index <= i)
            { // (i > _index)
            return _next->getBCIListEntry(i);
            }
         else
            {
            BytecodeIndexList *newIndex = new (_cfg.getInternalRegion()) BytecodeIndexList(i, this, _next, _cfg);
            return newIndex;
            }
         }
      inline uint32_t getPreviousIndex()
         {
         return _prev->_index;
         }
      inline BytecodeIndexList *getPrev()
         {
         return _prev;
         }
      BytecodeIndexList *getTail()
         {
         if (_next)
            return _next->getTail();
         return this;
         }
      inline void cutNext()
         {
         if (!_next)
            return;
         _next->_prev = NULL;
         _next = NULL;
         }
   };

class BlockList
   {

   private:
      BlockList *_next;
      BlockList *_prev;
      BytecodeRangeList *_successorList;
      uint32_t _successorCount;
      TR::CFG &_cfg;
      BlockList** _tail;
      BytecodeIndexList *_bcListHead;
      BytecodeIndexList *_bcListCurrent;
   public:
      TR_ALLOC(TR_Memory::UnknownType) // TODO: MicroJIT: create a memory object type for this and get it added to OMR
      BytecodeRange _range;
      TR::Block *_block;

      BlockList(TR::CFG &cfg, uint32_t start, BlockList** tail)
         :_next(NULL)
         ,_prev(NULL)
         ,_successorList(NULL)
         ,_successorCount(0)
         ,_cfg(cfg)
         ,_tail(tail)
         ,_bcListHead(NULL)
         ,_bcListCurrent(NULL)
         ,_range(start)
         ,_block(NULL)
         {
         _bcListHead = new (_cfg.getInternalRegion()) BytecodeIndexList(start, NULL, NULL, _cfg);
         _bcListCurrent = _bcListHead;
         }

      inline void addBCIndex(uint32_t index)
         {
         _bcListCurrent = _bcListCurrent->appendBCI(index);
         }

      inline void setBCListHead(BytecodeIndexList *list)
         {
         _bcListHead = list;
         }

      inline void setBCListCurrent(BytecodeIndexList *list)
         {
         _bcListCurrent = list;
         }

      inline void close()
         {
         _range._end = _bcListCurrent->_index;
         }

      inline void insertAfter(BlockList *bl)
         {
         MJIT_ASSERT_NO_MSG(bl);
         bl->_next = _next;
         bl->_prev = this;
         if (_next)
            _next->_prev = bl;
         _next = bl;
         if (this == *_tail)
            *_tail = bl;
         }

      inline void addSuccessor(BytecodeRange *successor)
         {
         BytecodeRangeList *brl = new (_cfg.getInternalRegion()) BytecodeRangeList(successor);
         if (_successorList)
            _successorList = _successorList->insert(brl);
         else
            _successorList = brl;
         _successorCount++;
         // Until we support switch and other multiple end point bytecodes this should never go over 2
         MJIT_ASSERT_NO_MSG(_successorCount > 0 && _successorCount <= 2);
         }

      BlockList *getBlockListByStartIndex(uint32_t index)
         {
         // NOTE always start searching from a block with a lower start than the index
         MJIT_ASSERT_NO_MSG(_range._start <= index);

         // If this is the target block return it
         if (_range._start == index)
            return this;
         if (_range._start < index && _next && _next->_range._start > index)
            return this;

         // If the branch is after this block, and there is more list to search, then continue searching
         if (_next)
            return _next->getBlockListByStartIndex(index);

         // There was no block in the BlockList for this index
         return NULL;
         }

      BlockList *getOrMakeBlockListByBytecodeIndex(uint32_t index)
         {
         // NOTE always start searching from a block with a lower start than the index
         MJIT_ASSERT_NO_MSG(_range._start <= index);

         // If this is the target block return it
         if (_range._start == index)
            return this;

         // If the branch targets the middle of this block (which may be under construction) split it on that index
         if (_range.isClosed() && index > _range._start && index <= _range._end)
            return splitBlockListOnIndex(index);
         if (!_range.isClosed() && _next && index > _range._start && index < _next->_range._start)
            return splitBlockListOnIndex(index);

         // If the branch is after this block, and there is more list to search, then continue searching
         if (_next && index > _next->_range._start)
            return _next->getOrMakeBlockListByBytecodeIndex(index);

         // The target isn't before this block, isn't this block, isn't in this block, and there is no more list to search.
         // Time to make a new block and add it to the list
         BlockList *target = new (_cfg.getInternalRegion()) BlockList(_cfg, index, _tail);
         insertAfter(target);
         return target;
         }

      BlockList *splitBlockListOnIndex(uint32_t index)
         {
         BlockList *target = new (_cfg.getInternalRegion()) BlockList(_cfg, index, _tail);
         insertAfter(target);

         BytecodeIndexList *splitPoint = _bcListHead->getBCIListEntry(index);
         target->setBCListHead(splitPoint);
         target->setBCListCurrent(splitPoint->getTail());

         _range._end = splitPoint->getPreviousIndex();
         _bcListCurrent = splitPoint->getPrev();
         _bcListCurrent->cutNext();

         return target;
         }

      BlockList *getNext()
         {
         return _next;
         }

      BlockList *getPrev()
         {
         return _prev;
         }

      inline uint32_t getSuccessorCount()
         {
         return _successorCount;
         }

      inline void getDestinations(uint32_t *destinations)
         {
         BytecodeRangeList *range = _successorList;
         for (uint32_t i=0; i<_successorCount; i++)
            {
            destinations[i] = range->getStart();
            range = _successorList->next;
            }
         }
   };

class CFGCreator
   {

   private:
      BlockList *_head;
      BlockList *_current;
      BlockList *_tail;
      bool _newBlock;
      bool _fallThrough;
      uint32_t _blockCount;
      TR::CFG &_cfg;
      TR_ResolvedMethod *_compilee;
      TR::Compilation *_comp;

   public:
      CFGCreator(TR::CFG &cfg, TR_ResolvedMethod *compilee, TR::Compilation *comp)
         :_head(NULL)
         ,_current(NULL)
         ,_tail(NULL)
         ,_newBlock(false)
         ,_fallThrough(true)
         ,_blockCount(0)
         ,_cfg(cfg)
         ,_compilee(compilee)
         ,_comp(comp)
         {}

      void addBytecodeIndexToCFG(TR_J9ByteCodeIterator *bci)
         {
         // Just started parsing, make the block list.
         if (!_head)
            {
            _head = new (_cfg.getInternalRegion()) BlockList(_cfg, bci->currentByteCodeIndex(), &_tail);
            _tail = _head;
            _current = _head;
            }

         // Given current state and current bytecode, update BlockList
         BlockList *nextBlock = _head->getBlockListByStartIndex(bci->currentByteCodeIndex());

         // If we have a block for this (possibly from a forward jump)
         // we should use that block, and close the current one.
         // If the current block is the right block to work on, then don't close it.
         if (nextBlock && nextBlock != _current)
            {
            _current->close();
            _current = _head->getBlockListByStartIndex(bci->currentByteCodeIndex());
            // If we had thought we needed a new block, override it
            _newBlock = false;
            _fallThrough = true;
            }

         if (_newBlock)
            { // Current block has ended in a branch, this is a new block now
            nextBlock = new (_cfg.getInternalRegion()) BlockList(_cfg, bci->currentByteCodeIndex(), &_tail);
            _current->insertAfter(nextBlock);
            if(_fallThrough)
               _current->addSuccessor(&(nextBlock->_range));
            _newBlock = false;
            _fallThrough = true;
            _current = nextBlock;
            }
         else if (bci->isBranch())
            { // Get the target block, make it if it isn't there already
            _current->_range._end = bci->currentByteCodeIndex();
            nextBlock = _head->getOrMakeBlockListByBytecodeIndex(bci->branchDestination(bci->currentByteCodeIndex()));
            if (bci->current() == J9BCgoto || bci->current() == J9BCgotow)
               _fallThrough = false;
            _current->addSuccessor(&(nextBlock->_range));
            _newBlock = true;
            }
         else
            { // Still working on current block
              // TODO: determine if there are any edge cases that need to be filled in here.
            }
         _current->addBCIndex(bci->currentByteCodeIndex());
         }

      void buildCFG()
         {
         // Make the blocks
         BlockList *temp = _head;
         while (temp != NULL)
            {
            uint32_t start = temp->_range._start;
            uint32_t end = temp->_range._end;
            temp->_block = makeBlock(_comp, _compilee, start, end, _cfg);
            temp = temp->getNext();
            }

         // Use successor lists to build CFG
         temp = _head;
         while (temp != NULL)
            {
            uint32_t successorCount = temp->getSuccessorCount();
            uint32_t destinations[successorCount];
            temp->getDestinations(destinations);
            for (uint32_t i=0; i<successorCount; i++)
               {
               // This needs to be replaced with something to handle all cases or a method call when switches are implemented.
               BlockList *dst = _head->getBlockListByStartIndex(destinations[i]);
               join(_cfg, temp->_block, dst->_block, (dst == temp->getNext()));
               }
            temp = temp->getNext();
            }
         _cfg.setStart(_head->_block);
         }
   };

static TR::Optimizer *createOptimizer(TR::Compilation *comp, TR::ResolvedMethodSymbol *methodSymbol)
   {
   return new (comp->trHeapMemory()) TR::Optimizer(comp, methodSymbol, true, J9::Optimizer::microJITOptimizationStrategy(comp));
   }

MJIT::InternalMetaData
createInternalMethodMetadata(
      MJIT::ByteCodeIterator *bci,
      MJIT::LocalTableEntry *localTableEntries,
      U_16 entries,
      int32_t offsetToFirstLocal,
      TR_ResolvedMethod *compilee,
      TR::Compilation *comp,
      MJIT::ParamTable *paramTable,
      uint8_t pointerSize,
      bool *resolvedAllCallees)
   {
   int32_t totalSize = 0;

   int32_t localIndex = 0; // Indexes are 0 based and positive
   int32_t lastParamSlot = 0;
   int32_t gcMapOffset = 0; // Offset will always be greater than 0
   U_16 size = 0; // Sizes are all multiples of 8 and greater than 0
   bool isRef = false; // No good default here, both options are valid.

   uint16_t maxCalleeArgsSize = 0;

   // TODO: Create the CFG before here and pass reference
   bool profilingCompilation = comp->getOption(TR_EnableJProfiling) || comp->getProfilingMode() == JProfiling;

   TR::CFG *cfg;
   if (profilingCompilation)
      cfg = new (comp->trHeapMemory()) TR::CFG(comp, compilee->findOrCreateJittedMethodSymbol(comp));

   CFGCreator creator(*cfg, compilee, comp);
   ParamTableEntry entry;
   for (int i=0; i<paramTable->getParamCount(); i+=entry.slots)
      {
      MJIT_ASSERT_NO_MSG(paramTable->getEntry(i, &entry));
      size = entry.slots*pointerSize;
      localTableEntries[i] = makeLocalTableEntry(i, totalSize + offsetToFirstLocal, size, entry.isReference);
      totalSize += size;
      lastParamSlot += localTableEntries[i].slots;
      }

   for (TR_J9ByteCode bc = bci->first(); bc != J9BCunknown; bc = bci->next())
      {
      /* It's okay to overwrite entries here.
      * We are not storing whether the entry is used for a
      * load or a store because it could be both. Identifying entries
      * that we have already created would require zeroing the
      * array before use and/or remembering which entries we've created.
      * Both might be as much work as just recreating entries, depending
      * on how many times a local is stored/loaded during a method.
      * This may be worth doing later, but requires some research first.
      *
      * TODO: Arguments are not guaranteed to be used in the bytecode of a method
      * e.g. class MyClass { public int ret3() {return 3;} }
      * The above method is not a static method (arg 0 is an object reference) but the BC
      * will be [iconst_3, return].
      * We must find a way to take the parameter table in and create the required entries
      * in the local table.
      */

      if (profilingCompilation)
         creator.addBytecodeIndexToCFG(bci);
      gcMapOffset = 0;
      isRef = false;
      switch (bc)
         {
         MakeEntry:
            gcMapOffset = offsetToFirstLocal + totalSize;
            localTableEntries[localIndex] = makeLocalTableEntry(localIndex, gcMapOffset, size, isRef);
            totalSize += size;
            break;
         case J9BCiload:
         case J9BCistore:
         case J9BCfstore:
         case J9BCfload:
            localIndex = (int32_t)bci->nextByte();
            size = pointerSize;
            goto MakeEntry;
         case J9BCiload0:
         case J9BCistore0:
         case J9BCfload0:
         case J9BCfstore0:
            localIndex = 0;
            size = pointerSize;
            goto MakeEntry;
         case J9BCiload1:
         case J9BCistore1:
         case J9BCfstore1:
         case J9BCfload1:
            localIndex = 1;
            size = pointerSize;
            goto MakeEntry;
         case J9BCiload2:
         case J9BCistore2:
         case J9BCfstore2:
         case J9BCfload2:
            localIndex = 2;
            size = pointerSize;
            goto MakeEntry;
         case J9BCiload3:
         case J9BCistore3:
         case J9BCfstore3:
         case J9BCfload3:
            localIndex = 3;
            size = pointerSize;
            goto MakeEntry;
         case J9BClstore:
         case J9BClload:
         case J9BCdstore:
         case J9BCdload:
            localIndex = (int32_t)bci->nextByte();
            size = pointerSize*2;
            goto MakeEntry;
         case J9BClstore0:
         case J9BClload0:
         case J9BCdload0:
         case J9BCdstore0:
            localIndex = 0;
            size = pointerSize*2;
            goto MakeEntry;
         case J9BClstore1:
         case J9BCdstore1:
         case J9BCdload1:
         case J9BClload1:
            localIndex = 1;
            size = pointerSize*2;
            goto MakeEntry;
         case J9BClstore2:
         case J9BClload2:
         case J9BCdstore2:
         case J9BCdload2:
            localIndex = 2;
            size = pointerSize*2;
            goto MakeEntry;
         case J9BClstore3:
         case J9BClload3:
         case J9BCdstore3:
         case J9BCdload3:
            localIndex = 3;
            size = pointerSize*2;
            goto MakeEntry;
         case J9BCaload:
         case J9BCastore:
            localIndex = (int32_t)bci->nextByte();
            size = pointerSize;
            isRef = true;
            goto MakeEntry;
         case J9BCaload0:
         case J9BCastore0:
            localIndex = 0;
            size = pointerSize;
            isRef = true;
            goto MakeEntry;
         case J9BCaload1:
         case J9BCastore1:
            localIndex = 1;
            size = pointerSize;
            isRef = true;
            goto MakeEntry;
         case J9BCaload2:
         case J9BCastore2:
            localIndex = 2;
            size = pointerSize;
            isRef = true;
            goto MakeEntry;
         case J9BCaload3:
         case J9BCastore3:
            localIndex = 3;
            size = pointerSize;
            isRef = true;
            goto MakeEntry;
         case J9BCinvokestatic:
            {
            int32_t cpIndex = (int32_t)bci->next2Bytes();
            bool isUnresolvedInCP;
            TR_ResolvedMethod *resolved = compilee->getResolvedStaticMethod(comp, cpIndex, &isUnresolvedInCP);
            if (!resolved)
               {
               *resolvedAllCallees = false;
               break;
               }
            J9Method *ramMethod = static_cast<TR_ResolvedJ9Method*>(resolved)->ramMethod();
            J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);
            // TODO replace this with a more accurate count. This is a clear upper bound;
            uint16_t calleeArgsSize = romMethod->argCount * pointerSize * 2; // assume 2 slot for now
            maxCalleeArgsSize = (calleeArgsSize > maxCalleeArgsSize) ? calleeArgsSize : maxCalleeArgsSize;
            }
         default:
            break;
         }
      }

   if(profilingCompilation)
      {
      creator.buildCFG();
      comp->getMethodSymbol()->setFlowGraph(cfg);
      TR::Optimizer *optimizer = createOptimizer(comp, comp->getMethodSymbol());
      comp->setOptimizer(optimizer);
      optimizer->optimize();
      }

   MJIT::LocalTable localTable(localTableEntries, entries);

   MJIT::InternalMetaData internalMetaData(localTable, cfg, maxCalleeArgsSize);
   return internalMetaData;
   }

} // namespace MJIT
#endif /* TR_MJIT_Interop */
