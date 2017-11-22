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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#pragma csect(CODE,"TRJ9MRBase#C")
#pragma csect(STATIC,"TRJ9MRBase#S")
#pragma csect(TEST,"TRJ9MRBase#T")

#include "codegen/MemoryReference.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Machine.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/S390Register.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/symbol/AutomaticSymbol.hpp"

#include <stddef.h>                                 // for size_t
#include <stdint.h>                                 // for int32_t, etc
#include <string.h>                                 // for NULL, strcmp
#include "codegen/CodeGenerator.hpp"                // for CodeGenerator, etc
#include "codegen/ConstantDataSnippet.hpp"
#include "codegen/FrontEnd.hpp"                     // for TR_FrontEnd, etc
#include "codegen/InstOpCode.hpp"                   // for InstOpCode, etc
#include "codegen/Instruction.hpp"                  // for Instruction, etc
#include "codegen/Linkage.hpp"                      // for Linkage
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"                      // for MAXDISP, Machine, etc
#include "codegen/MemoryReference.hpp"              // for MemoryReference, etc
#include "codegen/RealRegister.hpp"                 // for RealRegister, etc
#include "codegen/Register.hpp"                     // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterPair.hpp"                 // for RegisterPair
#include "codegen/Relocation.hpp"
#include "codegen/Snippet.hpp"                      // for TR::S390Snippet, etc
#include "codegen/TreeEvaluator.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/Compilation.hpp"                  // for Compilation
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/sparsrbit.h"
#include "env/CompilerEnv.hpp"
#include "env/ObjectModel.hpp"                      // for ObjectModel
#include "env/StackMemoryRegion.hpp"                // for TR::StackMemoryRegion
#include "env/TRMemory.hpp"
#include "env/defines.h"                            // for TR_HOST_64BIT
#include "env/jittypes.h"                           // for intptrj_t, uintptrj_t
#include "il/Block.hpp"                             // for Block
#include "il/DataTypes.hpp"                         // for DataTypes, etc
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                             // for ILOpCode
#include "il/Node.hpp"                              // for Node, etc
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                            // for Symbol
#include "il/SymbolReference.hpp"                   // for SymbolReference, etc
#include "il/TreeTop.hpp"                           // for TreeTop
#include "il/TreeTop_inlines.hpp"                   // for TreeTop::getNode
#include "il/symbol/AutomaticSymbol.hpp"            // for AutomaticSymbol
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"               // for StaticSymbol
#include "infra/Array.hpp"                          // for TR_Array
#include "infra/Assert.hpp"                         // for TR_ASSERT
#include "infra/Bit.hpp"                            // for trailingZeroes
#include "infra/Flags.hpp"                          // for flags32_t
#include "infra/List.hpp"                           // for List, etc
#include "ras/Debug.hpp"                            // for TR_DebugBase
#include "z/codegen/EndianConversion.hpp"           // for boi
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390Instruction.hpp"

void recursivelyIncrementReferenceCount(TR::Node *node, rcount_t increment, List<TR::Node> & incrementedNodesList, List<TR::Node> &nodesAlreadyEvaluatedBeforeFoldingList,
   TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_ASSERT( increment > 0,"recursivelyIncrementReferenceCount only valid for positive increments\n");
   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\t\t\trecAdjust node - %s (%p) and add to list, increment %d: refCount %d->%d\n",
         node->getOpCode().getName(),node,increment,node->getReferenceCount(),node->getReferenceCount()+increment);
   incrementedNodesList.add(node);
   node->setReferenceCount(node->getReferenceCount()+increment);

   if (node->getRegister() && node->getRegister()->getOpaquePseudoRegister())
      {
      TR_OpaquePseudoRegister *pseudoReg = node->getRegister()->getOpaquePseudoRegister();
      TR_StorageReference *storageReference = pseudoReg->getStorageReference();
      TR_ASSERT( storageReference,"the pseudoReg should have a non-null storage reference\n");
      if (storageReference->isTemporaryBased())
         storageReference->incrementTemporaryReferenceCount();
      }
   else if (node->getOpCode().hasSymbolReference() && node->getSymbolReference() &&
            node->getSymbolReference()->isTempVariableSizeSymRef() && node->getSymbolReference()->getSymbol() &&
            node->getSymbolReference()->getSymbol()->isVariableSizeSymbol())
      {
      TR_ASSERT( node->getOpCodeValue()==TR::loadaddr,"temporary symbol references should only be attached to loadaddr nodes\n");
      TR::AutomaticSymbol *sym = node->getSymbolReference()->getSymbol()->castToVariableSizeSymbol();
      if (comp->cg()->traceBCDCodeGen())
         traceMsg(comp,"\tincrement temporary #%d (sym %p -- from loadaddr node %p) reference count %d->%d\n",
            node->getSymbolReference()->getReferenceNumber(),sym,node,sym->getReferenceCount(),sym->getReferenceCount()+1);
      sym->setReferenceCount(sym->getReferenceCount()+1);
      }
   if (node->getRegister() == NULL)
      {
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\t\t\tnode has no register so do recurse\n");
      for (int32_t childCount = node->getNumChildren()-1; childCount >= 0; childCount--)
         recursivelyIncrementReferenceCount(node->getChild(childCount), increment, incrementedNodesList, nodesAlreadyEvaluatedBeforeFoldingList, cg);
      }
   else
      {
      nodesAlreadyEvaluatedBeforeFoldingList.add(node);
      if (cg->traceBCDCodeGen())
      traceMsg(comp,"\t\t\tnode has a register so do not recurse\n");
      }
   return;
   }

void J9::Z::MemoryReference::addInstrSpecificRelocation(TR::CodeGenerator* cg, TR::Instruction* instr, int32_t disp, uint8_t * cursor)
   {
   }

bool
J9::Z::MemoryReference::typeNeedsAlignment(TR::Node *node)
   {
   if (node && node->getType().isBCD())
      return true;
   else
      return OMR::Z::MemoryReference::typeNeedsAlignment(node);
   }

void
J9::Z::MemoryReference::tryForceFolding(TR::Node *& rootLoadOrStore, TR::CodeGenerator *& cg, TR_StorageReference *& storageReference, TR::SymbolReference *& symRef, TR::Symbol *& symbol,
                                        List<TR::Node>& nodesAlreadyEvaluatedBeforeFoldingList)
   {
   if (storageReference)
      {
      bool isImpliedMemoryReference = false;
      
      TR::Compilation *comp = cg->comp();
      TR::Node *storageRefNode = storageReference->getNode();
      bool isIndirect = storageRefNode->getOpCode().isIndirect();
      
      TR_ASSERT(!storageRefNode->getOpCode().isLoadConst(), "storageRefNode %s (%p) const should be BCD or Aggr type\n", storageRefNode->getOpCode().getName(),storageRefNode);
      
      TR_ASSERT(storageRefNode->getOpCode().isLoadVar() || storageRefNode->getOpCode().isStore(), "expecting storageRef node %p to be a loadVar or store\n",storageRefNode);
      
      _symbolReference = storageReference->getSymbolReference();
      
      symRef = _symbolReference;
      _originalSymbolReference = _symbolReference;
      
      symbol = _symbolReference->getSymbol();
      
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\t\tmr storageRef case: setting rootLoadOrStore from %s (%p) to storageRef->node %s (%p) (ref->nodeRefCount %d, symRef #%d (sym=%p), isIndirect %s, isConst %s)\n",
            rootLoadOrStore?rootLoadOrStore->getOpCode().getName():"NULL",
            rootLoadOrStore,
            storageRefNode?storageRefNode->getOpCode().getName():"NULL",
            storageRefNode,
            storageReference->getNodeReferenceCount(),
            _symbolReference->getReferenceNumber(),
            symbol,
            isIndirect?"yes":"no",
            "no");
      
      rootLoadOrStore = storageRefNode;
      if (isIndirect)
         {
         if (storageReference->isNodeBasedHint())
            {
            isImpliedMemoryReference = true;
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\t\tset isImpliedMemoryReference=true as ref isNodeBasedHint=true\n");
            }
         else
            {
            TR_ASSERT(storageReference->getNodeReferenceCount() >= 1,"storageReference->getNodeReferenceCount() should be >=1 and not %d storageRefNode:[%p]\n",storageReference->getNodeReferenceCount(), storageRefNode);
            isImpliedMemoryReference = (storageReference->getNodeReferenceCount() > 1);
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\t\tset isImpliedMemoryReference=%s as ref->getNodeReferenceCount() %s 1\n",
                  isImpliedMemoryReference?"true":"false",isImpliedMemoryReference?">":"==");
            }
         }
      if (!storageReference->isNodeBasedHint())
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\t\tdec nodeRefCount %d->%d on storageRef #%d (storageRefNode %s (%p))\n",
               storageReference->getNodeReferenceCount(),storageReference->getNodeReferenceCount()-1,
               storageReference->getReferenceNumber(),
               storageReference->getNode()->getOpCode().getName(),storageReference->getNode());
         storageReference->decrementNodeReferenceCount();
         }
      TR_ASSERT(rootLoadOrStore->getOpCode().isLoad() || rootLoadOrStore->getOpCode().isStore(),"rootLoadOrStore should be a load or store opcode\n");

      if (isImpliedMemoryReference)
         {
         // An addressHint is an implied reference to an address tree. The nodes in this tree must have their references counts incremented to
         // account for this implied use. Failing to reflect these implied uses could allow an evaluator to incorrectly clobber a result register that is needed in
         // a subsequent implied or explicit use (i.e. an evaluator may perform a refCount==1 codegen optimization when it really should not given the implied use).
         // A drawback of incrementing the reference counts is that populateMemoryReference will never try to fold an address tree into a 390 memory reference when
         // the refCount of the subTree is > 1.
         // Depending on the simplicity of the tree (for example, if it is just a simple add of a base+const) then it is probably still worthwhile attempting to fold
         // the tree.
         // The flag forceFolding is set in these cases to force populateMemoryReference to attempt folding even when the (now) higher refCounts would usually disallow it.
         TR::Node *addressChild = rootLoadOrStore->getFirstChild();
         
         self()->setForceFoldingIfAdvantageous(cg, addressChild);
         
         if (self()->forceFolding() || self()->forceFirstTimeFolding() || addressChild->getOpCodeValue() == TR::loadaddr)
            {
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\t\tisImpliedMemoryReference=true and %s so recInc refCounts for rootLoadOrStore %s (%p) and addressChild %s (%p)\n",
                  self()->forceFirstTimeFolding()?
                     "forceFirstTimeFolding":
                      (self()->forceFolding()?"forceFolding=true":"addressChild is a loadaddr"),
                      rootLoadOrStore->getOpCode().getName(),rootLoadOrStore,
                      addressChild->getOpCode().getName(),addressChild);
            _incrementedNodesList.init();
            nodesAlreadyEvaluatedBeforeFoldingList.init();
            // Increment the reference counts for this tree to reflect this implicit use so:
            //    1. Any evaluators called for this tree will see accurate reference counts that include this implicit use so no
            //       incorrect codegen optimizations are performed.
            //    2. So any folding of the addressChild's children can be performed for this and future implicit uses and for the original use without
            //       underflowing the addressChild's children's referenceCounts (as each implied and actual use would reference the children each time when
            //       folding).
            recursivelyIncrementReferenceCount(addressChild, 1, _incrementedNodesList, nodesAlreadyEvaluatedBeforeFoldingList, cg);
            }
         else
            {
            self()->setForceEvaluation();
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\t\tisImpliedMemoryReference=true and forceFolding=false so increment addressChild %s (%p) refCount %d->%d and setForceEvaluation to true\n",
                  addressChild->getOpCode().getName(),addressChild,addressChild->getReferenceCount(),addressChild->getReferenceCount()+1);
            addressChild->incReferenceCount();
            }
         }
      }
   }

TR::UnresolvedDataSnippet *
J9::Z::MemoryReference::createUnresolvedDataSnippet(TR::Node * node, TR::CodeGenerator * cg, TR::SymbolReference * symRef, TR::Register * tempReg, bool isStore)
   {
   TR::UnresolvedDataSnippet * uds;
   TR::Instruction * cursor;

   self()->setUnresolvedSnippet(uds = new (cg->trHeapMemory()) TR::UnresolvedDataSnippet(cg, node, symRef, isStore, false));
   cg->addSnippet(self()->getUnresolvedSnippet());

   // generate branch to the unresolved data snippet
   cursor = generateRegUnresolvedSym(cg, TR::InstOpCode::getLoadOpCode(), node, tempReg, symRef, uds);
   uds->setBranchInstruction(cursor);

   return uds;
   }

TR::UnresolvedDataSnippet *
J9::Z::MemoryReference::createUnresolvedDataSnippetForiaload(TR::Node * node, TR::CodeGenerator * cg, TR::SymbolReference * symRef, TR::Register * tempReg, bool & isStore)
   {
   // Have to catch the case where, on first glance, a putstatic looks
   // like a 'read' since the unresolved ref is on the iaload, not the
   // iistore. The 'right' fix is to set a bit on the sym instead
   //
   TR::Node * rootNode = cg->getCurrentEvaluationTreeTop()->getNode();
   if (rootNode->getOpCode().isResolveCheck() &&
       rootNode->getFirstChild()->getOpCode().isStoreIndirect() &&
       rootNode->getFirstChild()->getFirstChild() == node &&
       !rootNode->getFirstChild()->getSymbolReference()->isUnresolved())
      {
      isStore = true;
      }
   
   TR::UnresolvedDataSnippet * uds = self()->createUnresolvedDataSnippet(node, cg, symRef, tempReg, isStore);
   self()->getUnresolvedSnippet()->createUnresolvedData(cg, _baseNode);
   self()->getUnresolvedSnippet()->getUnresolvedData()->setUnresolvedDataSnippet(self()->getUnresolvedSnippet());
   return uds;
   }

void
J9::Z::MemoryReference::createUnresolvedSnippetWithNodeRegister(TR::Node * node, TR::CodeGenerator * cg, TR::SymbolReference * symRef, TR::Register *& writableLiteralPoolRegister)
   {
   TR::Register * tempReg = node->getRegister();
   if (tempReg == NULL)
      {
      if (TR::Compiler->target.is64Bit())
         tempReg = node->setRegister(cg->allocate64bitRegister());
      else
         tempReg = node->setRegister(cg->allocateRegister());
      }
   else if (tempReg->getKind() == TR_FPR)
      {
      if (TR::Compiler->target.is64Bit())
         tempReg = cg->allocate64bitRegister();
      else
         tempReg = cg->allocateRegister();
      }
   else if (tempReg->getKind() == TR_VRF)
      {
      tempReg = cg->allocateRegister(TR_VRF);
      }

   self()->createUnresolvedDataSnippet(node, cg, symRef, tempReg, false);

   if (node->getOpCodeValue() == TR::loadaddr)
      {
      if (cg->isLiteralPoolOnDemandOn())
         {
         writableLiteralPoolRegister = cg->allocateRegister();
         generateLoadLiteralPoolAddress(cg, node, writableLiteralPoolRegister);
         cg->stopUsingRegister(writableLiteralPoolRegister);
         }
      else
         {
         writableLiteralPoolRegister = cg->getLitPoolRealRegister();
         }
      }

   self()->setBaseRegister(tempReg, cg);
   _baseNode = node;
   }

void
J9::Z::MemoryReference::createUnresolvedDataSnippetForBaseNode(TR::CodeGenerator * cg, TR::Register * writableLiteralPoolRegister)
   {
   self()->getUnresolvedSnippet()->createUnresolvedData(cg, _baseNode);
   self()->getUnresolvedSnippet()->getUnresolvedData()->setUnresolvedDataSnippet(self()->getUnresolvedSnippet());
   self()->setBaseRegister(writableLiteralPoolRegister, cg);
   }

void
J9::Z::MemoryReference::createPatchableDataInLitpool(TR::Node * node, TR::CodeGenerator * cg, TR::Register * tempReg, TR::UnresolvedDataSnippet * uds)
   {
   // create a patchable data in litpool
   TR::S390WritableDataSnippet * litpool = cg->CreateWritableConstant(node);
   litpool->setUnresolvedDataSnippet(uds);

   if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z10))
      {
      litpool->resetNeedLitPoolBasePtr();
      TR::S390RILInstruction * LRLinst;
      LRLinst = (TR::S390RILInstruction *) generateRILInstruction(cg, TR::InstOpCode::getLoadRelativeLongOpCode(), node, tempReg, reinterpret_cast<uintptrj_t*>(0xBABE), 0);
      uds->setDataReferenceInstruction(LRLinst);
      LRLinst->setSymbolReference(uds->getDataSymbolReference());
      LRLinst->setTargetSnippet(litpool);
      LRLinst->setTargetSymbol(uds->getDataSymbol());
      TR_Debug * debugObj = cg->getDebug();

      if (debugObj)
         {
         debugObj->addInstructionComment(LRLinst, "LoadLitPoolEntry");
         }
      }
   else
      {
      auto dataMemoryReference = generateS390MemoryReference(litpool, cg, 0, node);

      generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, tempReg, dataMemoryReference);
      }

   self()->setBaseRegister(tempReg, cg);
   }

bool
J9::Z::MemoryReference::symRefHasTemporaryNegativeOffset()
   {
   return self()->getStorageReference() && self()->getStorageReference()->getSymbolReference() && self()->getStorageReference()->getSymbolReference()->hasTemporaryNegativeOffset();
   }

void
J9::Z::MemoryReference::setMemRefAndGetUnresolvedData(TR::Snippet *& snippet)
   {
   self()->getUnresolvedSnippet()->setMemoryReference(self());
   snippet = self()->getUnresolvedSnippet()->getUnresolvedData();
   }

TR::MemoryReference *
reuseS390MemRefFromStorageRef(TR::MemoryReference *baseMR, int32_t offset, TR::Node *node, TR_StorageReference *storageReference, TR::CodeGenerator *cg, bool enforceSSLimits)
   {
   if (baseMR == NULL)
      {
      baseMR = generateS390MemRefFromStorageRef(node, storageReference, cg, enforceSSLimits);
      baseMR->addToOffset(offset);
      }
   else
      {
      baseMR = reuseS390MemoryReference(baseMR, offset, node, cg, enforceSSLimits);
      }
   return baseMR;
   }
   

/**
 * When isNewTemp=true then do not transfer any deadBytes from the node/reg as this is a memref for brand new tempStorageRef -- node is still needed
 * in this case to attach on any instructions for line number lookup
 */
TR::MemoryReference *
generateS390MemRefFromStorageRef(TR::Node *node, TR_StorageReference *storageReference, TR::CodeGenerator * cg, bool enforceSSLimits, bool isNewTemp)
   {
   TR::Compilation *comp = cg->comp();
   TR_ASSERT( storageReference,"must specify a storageReference when creating an aligned memory reference\n");
   TR::MemoryReference *memRef = NULL;
   // A memRef created for an indirect load may have its symRef replaced with its child's symRef (when the child is a loadaddr for example)
   // When it comes to right aligning the memRef the symbol size of the indirect load itself is required and *not* the symbol size of the child
   // So in these cases cache the indirect load symbol size on the memRef for later use when right aligning.
   // One example of this in a indirect load off the loadaddr of comp()->getWCodeMainLitSymRef(). In this case the symbol size reflects all the constants present.
   // Another example is an indirect load off of a loadaddr auto. The auto symbol size may be larger than the indirect load size being performed.
   if (storageReference->isTemporaryBased())
      {
      memRef = new (cg->trHeapMemory()) TR::MemoryReference(node, storageReference->getTemporarySymbolReference(), cg, storageReference);
      }
   else
      {
      memRef = new (cg->trHeapMemory()) TR::MemoryReference(node, cg, false, storageReference);
      memRef->setFixedSizeForAlignment(storageReference->getSymbolSize());
      }

   if (!isNewTemp &&
       !storageReference->isSingleUseTemporary() &&
       node &&
       node->getOpaquePseudoRegister() &&
       node->getOpaquePseudoRegister()->getRightAlignedDeadAndIgnoredBytes() > 0)
      {
      int32_t regDeadAndIgnoredBytes = node->getOpaquePseudoRegister()->getRightAlignedDeadAndIgnoredBytes();
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tgenerateS390AlignedMemoryReference: adjust memRef->_offset for regDeadAndIgnoredBytes (%d->%d) from node %s (%p) and reg %s\n",
            memRef->getOffset(),memRef->getOffset()-regDeadAndIgnoredBytes,node->getOpCode().getName(),node,cg->getDebug()->getName(node->getOpaquePseudoRegister()));
      memRef->addToTemporaryNegativeOffset(node, -regDeadAndIgnoredBytes, cg);
      }

   if (storageReference->isTemporaryBased() && storageReference->getSymbolReference()->hasTemporaryNegativeOffset())
      {
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tgenerateS390AlignedMemoryReference mr %p: call addToTemporaryNegativeOffset flag for storageRef->symRef #%d (offset = %d) and node %s (%p) to memRef\n",
            memRef,storageReference->getReferenceNumber(),storageReference->getSymbolReference()->getOffset(),
            node->getOpCode().getName(),node);
      memRef->enforceSSFormatLimits(node, cg, NULL);
      memRef->setHasTemporaryNegativeOffset();
      }

   if (cg->traceBCDCodeGen() && storageReference->isTemporaryBased() && !storageReference->isSingleUseTemporary())
      {
      if (storageReference->getTemporaryReferenceCount() == 0)
         traceMsg(comp,"**ERROR**: using an already freed temp #%d sym %p (node %p storageRef %p)\n",
            storageReference->getReferenceNumber(),storageReference->getTemporarySymbol(),node,storageReference);
      else if (!storageReference->getTemporarySymbol()->isReferenced())
         traceMsg(comp,"**ERROR**: using an unreferenced temp #%d sym %p (node %p storageRef %p)\n",
            storageReference->getReferenceNumber(),storageReference->getTemporarySymbol(),node,storageReference);
      }

   TR_ASSERT(!storageReference->isTemporaryBased() ||
          storageReference->isSingleUseTemporary() || // refCount is always zero for these
          storageReference->getTemporaryReferenceCount() > 0,
         "using an already freed temporary symbol reference\n");

   TR_ASSERT(!storageReference->isTemporaryBased() ||
           !storageReference->isSingleUseTemporary() || // refCount is always zero for these
           storageReference->getTemporaryReferenceCount() == 0,
         "single use temps must have a refCount of 0 and not %d\n",storageReference->getTemporaryReferenceCount());

   TR_ASSERT(!storageReference->isTemporaryBased() ||
           storageReference->getTemporarySymbol()->isReferenced(),
         "using a temporary symbol that is marked as unreferenced\n");

   // enforcing the ss limits early means that each consuming instruction does not have to consolidate an index register or a large offset to a new base register
   if (enforceSSLimits)
      memRef->enforceSSFormatLimits(node, cg, NULL);

   //Make sure that the listing symref doesn't stay on the memref if the storage ref has a different symbol
   if (memRef->getListingSymbolReference())
      {
      if (storageReference->isTemporaryBased())
         {
         memRef->setListingSymbolReference(NULL);
         }
      else if (storageReference->isConstantNodeBased())
         {
         memRef->setListingSymbolReference(NULL);
         }
      else if (storageReference->isNonConstantNodeBased())
         {
         if (memRef->getListingSymbolReference() != storageReference->getNode()->getSymbolReference())
            {
            memRef->setListingSymbolReference(NULL);
            }
         }
      }

   return memRef;
   }

TR::MemoryReference *
generateS390RightAlignedMemoryReference(TR::Node *node, TR_StorageReference *storageReference, TR::CodeGenerator * cg, bool enforceSSLimits, bool isNewTemp)
   {
   TR_ASSERT(!node->getType().isAggregate(),"do not use aligned memrefs for aggrs on node %p\n",node);
   TR::MemoryReference *mr = generateS390MemRefFromStorageRef(node, storageReference, cg, enforceSSLimits, isNewTemp);
   mr->setRightAlignMemRef();
   return mr;
   }

TR::MemoryReference *
generateS390LeftAlignedMemoryReference(TR::Node *node, TR_StorageReference *storageReference, TR::CodeGenerator * cg, int32_t leftMostByte, bool enforceSSLimits, bool isNewTemp)
   {
   TR_ASSERT(!node->getType().isAggregate(),"do not use aligned memrefs for aggrs on node %p\n",node);
   TR::MemoryReference *mr = generateS390MemRefFromStorageRef(node, storageReference, cg, enforceSSLimits, isNewTemp);
   mr->setLeftAlignMemRef(leftMostByte);
   return mr;
   }

TR::MemoryReference *
reuseS390LeftAlignedMemoryReference(TR::MemoryReference *baseMR, TR::Node *node, TR_StorageReference *storageReference, TR::CodeGenerator *cg, int32_t leftMostByte, bool enforceSSLimits)
   {
   TR_ASSERT(!node->getType().isAggregate(),"do not use aligned memrefs for aggrs on node %p\n",node);
   if (baseMR == NULL)
      baseMR = generateS390LeftAlignedMemoryReference(node, storageReference, cg, leftMostByte, enforceSSLimits);
   else if (baseMR->getMemRefUsedBefore())
      baseMR = generateS390LeftAlignedMemoryReference(*baseMR, node, 0, cg, leftMostByte, enforceSSLimits);
   else
      baseMR->setLeftAlignMemRef(leftMostByte);
   return baseMR;
   }

TR::MemoryReference *
reuseS390RightAlignedMemoryReference(TR::MemoryReference *baseMR, TR::Node *node, TR_StorageReference *storageReference, TR::CodeGenerator *cg, bool enforceSSLimits)
   {
   TR_ASSERT(!node->getType().isAggregate(),"do not use aligned memrefs for aggrs on node %p\n",node);
   if (baseMR == NULL)
      baseMR = generateS390RightAlignedMemoryReference(node, storageReference, cg, enforceSSLimits);
   else if (baseMR->getMemRefUsedBefore())
      baseMR = generateS390RightAlignedMemoryReference(*baseMR, node, 0, cg, enforceSSLimits);
   else
      baseMR->setRightAlignMemRef();
   return baseMR;
   }
