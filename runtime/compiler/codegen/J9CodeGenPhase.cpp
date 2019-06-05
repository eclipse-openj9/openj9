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

//On zOS XLC linker can't handle files with same name at link time
//This workaround with pragma is needed. What this does is essentially
//give a different name to the codesection (csect) for this file. So it
//doesn't conflict with another file with same name.

#pragma csect(CODE,"TRJ9CGPhase#C")
#pragma csect(STATIC,"TRJ9CGPhase#S")
#pragma csect(TEST,"TRJ9CGPhase#T")


#include "codegen/CodeGenPhase.hpp"
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "il/Block.hpp"
#include "optimizer/SequentialStoreSimplifier.hpp"
#include "env/VMJ9.h"

#ifdef TR_TARGET_S390
#include "codegen/InMemoryLoadStoreMarking.hpp"
#endif

// to decide if asyncchecks should be inserted at method exits
#define BYTECODESIZE_THRESHOLD_FOR_ASYNCCHECKS  300

void
J9::CodeGenPhase::reportPhase(PhaseValue phase)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(_cg->comp()->fe());
   fej9->reportCodeGeneratorPhase(phase);
   _currentPhase = phase;
   }

int
J9::CodeGenPhase::getNumPhases()
   {
   return static_cast<int>(TR::CodeGenPhase::LastJ9Phase);
   }

void 
J9::CodeGenPhase::performFixUpProfiledInterfaceGuardTestPhase(TR::CodeGenerator *cg, TR::CodeGenPhase *phase)
   {
   cg->fixUpProfiledInterfaceGuardTest();
   }

void
J9::CodeGenPhase::performAllocateLinkageRegistersPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   TR::Compilation * comp = cg->comp();
   if (!comp->getOption(TR_DisableLinkageRegisterAllocation))
      cg->allocateLinkageRegisters();
   }


void
J9::CodeGenPhase::performPopulateOSRBufferPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   phase->reportPhase(PopulateOSRBufferPhase);
   cg->populateOSRBuffer();
   }


void
J9::CodeGenPhase::performMoveUpArrayLengthStoresPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   phase->reportPhase(MoveUpArrayLengthStoresPhase);
   cg->moveUpArrayLengthStores(cg->comp()->getStartBlock()->getEntry());
   }

void
J9::CodeGenPhase::performInsertEpilogueYieldPointsPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   TR::Compilation * comp = cg->comp();
   phase->reportPhase(InsertEpilogueYieldPointsPhase);

   // insert asyncchecks for non-loopy large methods that contain no calls
   // (important for sunflow where the second hottest method is one such)
   //
   // FIXME: the value for methodContainsCalls is not computed until
   // after the main tree traversal (below). However, we can't move
   // this code after the loop because the inserted yield points need
   // to be lowered by the same loop.
   //
   if ((comp->getCurrentMethod()->maxBytecodeIndex() >= BYTECODESIZE_THRESHOLD_FOR_ASYNCCHECKS) &&
       !comp->mayHaveLoops() &&
       comp->getCurrentMethod()->convertToMethod()->methodType() == TR_Method::J9 &&  // FIXME: enable for ruby and python
       comp->getOSRMode() != TR::involuntaryOSR)
      {
      cg->insertEpilogueYieldPoints();
      }
   }

void
J9::CodeGenPhase::performCompressedReferenceRematerializationPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *)
   {
   cg->compressedReferenceRematerialization();
   }

void
J9::CodeGenPhase::performSplitWarmAndColdBlocksPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *)
   {
   cg->splitWarmAndColdBlocks();
   }

const char *
J9::CodeGenPhase::getName()
   {
   return TR::CodeGenPhase::getName(_currentPhase);
   }

const char *
J9::CodeGenPhase::getName(TR::CodeGenPhase::PhaseValue phase)
   {
   switch (phase)
      {
      case AllocateLinkageRegisters:
         return "AllocateLinkageRegisters";
      case PopulateOSRBufferPhase:
         return "PopulateOSRBuffer";
      case MoveUpArrayLengthStoresPhase:
         return "MoveUpArrayLengthStores";
      case InsertEpilogueYieldPointsPhase:
         return "InsertEpilogueYieldPoints";
      case CompressedReferenceRematerializationPhase:
         return "CompressedReferenceRematerialization";
      case SplitWarmAndColdBlocksPhase:
         return "SplitWarmAndColdBlocks";
      case IdentifyUnneededByteConvsPhase:
	      return "IdentifyUnneededByteConvsPhase";
      case LateSequentialConstantStoreSimplificationPhase:
         return "LateSequentialConstantStoreSimplification";
      case FixUpProfiledInterfaceGuardTest:
         return "FixUpProfiledInterfaceGuardTest";
      default:
         return OMR::CodeGenPhaseConnector::getName(phase);
      }
   }

void
J9::CodeGenPhase::performIdentifyUnneededByteConvsPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   cg->identifyUnneededByteConvNodes();
   }

void
J9::CodeGenPhase::performLateSequentialConstantStoreSimplificationPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   TR::Compilation* comp = cg->comp();
   cg->setOptimizationPhaseIsComplete();
   }


