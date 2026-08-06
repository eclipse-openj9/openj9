/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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

#if defined(J9ZOS390)
// On zOS XLC linker can't handle files with same name at link time
// This workaround with pragma is needed. What this does is essentially
// give a different name to the codesection (csect) for this file. So it
// doesn't conflict with another file with same name.

#pragma csect(CODE, "TRJ9CGPhase#C")
#pragma csect(STATIC, "TRJ9CGPhase#S")
#pragma csect(TEST, "TRJ9CGPhase#T")
#endif

#include "codegen/CodeGenPhase.hpp"
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "il/Block.hpp"
#include "optimizer/SequentialStoreSimplifier.hpp"
#include "env/VMJ9.h"

#ifdef TR_TARGET_S390
#include "codegen/InMemoryLoadStoreMarking.hpp"
#endif

// to decide if asyncchecks should be inserted at method exits
#define BYTECODESIZE_THRESHOLD_FOR_ASYNCCHECKS 300

void J9::CodeGenPhase::reportPhase(PhaseValue phase)
{
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(_cg->comp()->fe());
    fej9->reportCodeGeneratorPhase(phase);
    _currentPhase = phase;
}

int J9::CodeGenPhase::getNumPhases() { return static_cast<int>(TR::CodeGenPhase::LastJ9Phase); }

void J9::CodeGenPhase::performBinaryEncodingPhase(TR::CodeGenerator *cg, TR::CodeGenPhase *phase)
{
    OMR::CodeGenPhase::performBinaryEncodingPhase(cg, phase);
    TR::Compilation *comp = cg->comp();
    if (comp->getOptimizationPlan()->insertPatchableJProfiling() && comp->getRecompilationInfo() != NULL) {
        uintptr_t key = reinterpret_cast<uintptr_t>(comp->getRecompilationInfo()->getJittedBodyInfo());
        TR_BlockFrequencyInfo *info = TR_BlockFrequencyInfo::getCurrent(comp);
        if (info != NULL) {
            TR::list<TR::Instruction *> instrList = cg->getJProfilingCounterBumpInstructionList();
            if (!instrList->empty()) {
                TR::JProfBFPatchSites *sites = new (comp->trPersistentMemory())
                    TR::JProfBFPatchSites(comp->trPersistentMemory(), instrList->size());
                for (auto iter = instrList->begin(); itre != instrList->end(); ++iter) {
                    uint8_t *location = (*iter)->getBinaryEncoding();
                    uint8_t instrLength = (*iter)->getBinaryLength();
                    // Need to pass the length of the instruction here.
                    sites->add(location, instrLength);
                }
                TR_JProfBlockFrequencyCounterSites *patchSites = TR_JProfBlockFrequencyCounterSites::make(comp->fe(),
                    comp->trPersistentMemory(), key, sites, comp->getMetadataAssumptionList());
                info->addJProfBlockFrequencyCounterPatchSites(patchSites);
            }
        }
        TR_ValueProfileInfo *valueInfo = TR_ValueProfileInfo::getCurrent(comp);
        if (valueInfo != NULL) {
            TR::list<TR::Instruction *> instrList = cg->getJProfValueBranchInstrList();
            if (instrList != NULL && !instrList->empty()) {
                TR::PatchSites *sites
                    = new (comp->trPersistentMemory()) TR::PatchSites(comp->trPersistentMemory(), instrList->size());
                for (auto iter = instrList->begin(); iter != instrList->end(); ++iter) {
                    uint8_t *location = (*iter)->getBinaryEncoding();
                    sites->add(location, 0);
                }
                TR_JProfValueSites *valueProfSites = TR_JProfValueSites::make(comp->fe(), comp->trPersistentMemory(),
                    key, sites, comp->getMetadataAssumptionList());
                valueInfo->addJProfValueSites(valueProfSites);
            }
        }
    }
}

void J9::CodeGenPhase::performFixUpProfiledInterfaceGuardTestPhase(TR::CodeGenerator *cg, TR::CodeGenPhase *phase)
{
    cg->fixUpProfiledInterfaceGuardTest();
}

void J9::CodeGenPhase::performAllocateLinkageRegistersPhase(TR::CodeGenerator *cg, TR::CodeGenPhase *phase)
{
    TR::Compilation *comp = cg->comp();
    if (!comp->getOption(TR_DisableLinkageRegisterAllocation))
        cg->allocateLinkageRegisters();
}

void J9::CodeGenPhase::performPopulateOSRBufferPhase(TR::CodeGenerator *cg, TR::CodeGenPhase *phase)
{
    phase->reportPhase(PopulateOSRBufferPhase);
    cg->populateOSRBuffer();
}

void J9::CodeGenPhase::performMoveUpArrayLengthStoresPhase(TR::CodeGenerator *cg, TR::CodeGenPhase *phase)
{
    phase->reportPhase(MoveUpArrayLengthStoresPhase);
    cg->moveUpArrayLengthStores(cg->comp()->getStartBlock()->getEntry());
}

void J9::CodeGenPhase::performInsertEpilogueYieldPointsPhase(TR::CodeGenerator *cg, TR::CodeGenPhase *phase)
{
    TR::Compilation *comp = cg->comp();
    phase->reportPhase(InsertEpilogueYieldPointsPhase);

    // insert asyncchecks for non-loopy large methods that contain no calls
    // (important for sunflow where the second hottest method is one such)
    //
    // FIXME: the value for methodContainsCalls is not computed until
    // after the main tree traversal (below). However, we can't move
    // this code after the loop because the inserted yield points need
    // to be lowered by the same loop.
    //
    if ((comp->getCurrentMethod()->maxBytecodeIndex() >= BYTECODESIZE_THRESHOLD_FOR_ASYNCCHECKS)
        && !comp->mayHaveLoops() && comp->getCurrentMethod()->convertToMethod()->methodType() == TR::Method::J9
        && // FIXME: enable for ruby and python
        comp->getOSRMode() != TR::involuntaryOSR) {
        cg->insertEpilogueYieldPoints();
    }
}

void J9::CodeGenPhase::performCompressedReferenceRematerializationPhase(TR::CodeGenerator *cg, TR::CodeGenPhase *)
{
    cg->compressedReferenceRematerialization();
}

const char *J9::CodeGenPhase::getName() { return TR::CodeGenPhase::getName(_currentPhase); }

const char *J9::CodeGenPhase::getName(TR::CodeGenPhase::PhaseValue phase)
{
    switch (phase) {
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
        case IdentifyUnneededByteConvsPhase:
            return "IdentifyUnneededByteConvsPhase";
        case FixUpProfiledInterfaceGuardTest:
            return "FixUpProfiledInterfaceGuardTest";
        default:
            return OMR::CodeGenPhaseConnector::getName(phase);
    }
}

void J9::CodeGenPhase::performIdentifyUnneededByteConvsPhase(TR::CodeGenerator *cg, TR::CodeGenPhase *phase)
{
    cg->identifyUnneededByteConvNodes();
}

