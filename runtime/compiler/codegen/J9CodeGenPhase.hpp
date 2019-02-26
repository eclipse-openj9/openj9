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

#ifndef J9_CODEGEN_PHASE
#define J9_CODEGEN_PHASE

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_CODEGEN_PHASE_CONNECTOR
#define J9_CODEGEN_PHASE_CONNECTOR
   namespace J9 { class CodeGenPhase; }
   namespace J9 { typedef J9::CodeGenPhase CodeGenPhaseConnector; }
#endif

#include "codegen/OMRCodeGenPhase.hpp"


namespace J9
{

class OMR_EXTENSIBLE CodeGenPhase: public OMR::CodeGenPhaseConnector
   {
   protected:

   CodeGenPhase(TR::CodeGenerator * cg): OMR::CodeGenPhaseConnector(cg) {}

   public:

   void reportPhase(PhaseValue phase);
   static void performFixUpProfiledInterfaceGuardTestPhase(TR::CodeGenerator *cg, TR::CodeGenPhase *);
   static void performAllocateLinkageRegistersPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *);
   static void performPopulateOSRBufferPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *);
   static void performMoveUpArrayLengthStoresPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *);
   static void performInsertEpilogueYieldPointsPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *);
   static void performCompressedReferenceRematerializationPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *);
   static void performSplitWarmAndColdBlocksPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *);
   static void performIdentifyUnneededByteConvsPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *);
   static void performLateSequentialConstantStoreSimplificationPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *);


   // override base class implementation because new phases are being added
   static int getNumPhases();
   const char * getName();
   static const char* getName(PhaseValue phase);

   };


}

#endif
