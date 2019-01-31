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

#pragma csect(CODE,"TRJ9ZCGPhase#C")
#pragma csect(STATIC,"TRJ9ZCGPhase#S")
#pragma csect(TEST,"TRJ9ZCGPhase#T")

#include "codegen/CodeGenPhase.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/InMemoryLoadStoreMarking.hpp"
#include "codegen/ReduceSynchronizedFieldLoad.hpp"
#include "codegen/UncommonBCDCHKAddressNode.hpp"

// to decide if asyncchecks should be inserted at method exits
#define BYTECODESIZE_THRESHOLD_FOR_ASYNCCHECKS  300

void
J9::Z::CodeGenPhase::performInMemoryLoadStoreMarkingPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   InMemoryLoadStoreMarking obj = InMemoryLoadStoreMarking(cg);
   obj.perform();
   }

void
J9::Z::CodeGenPhase::performReduceSynchronizedFieldLoadPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   ReduceSynchronizedFieldLoad obj = ReduceSynchronizedFieldLoad(cg);

   if (obj.perform())
      {
      if (cg->comp()->getOption(TR_TraceCG) || cg->comp()->getOption(TR_TraceTrees))
         {
         cg->comp()->dumpMethodTrees("Post Reduce Synchronized Field Load Trees");
         }
      }
   }

void
J9::Z::CodeGenPhase::performUncommonBCDCHKAddressNodePhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   UncommonBCDCHKAddressNode obj = UncommonBCDCHKAddressNode(cg);
   obj.perform();
   }

int
J9::Z::CodeGenPhase::getNumPhases()
   {
   return static_cast<int>(TR::CodeGenPhase::LastJ9ZPhase);
   }

const char *
J9::Z::CodeGenPhase::getName()
   {
   return TR::CodeGenPhase::getName(_currentPhase);
   }

const char *
J9::Z::CodeGenPhase::getName(TR::CodeGenPhase::PhaseValue phase)
   {
   switch (phase)
      {
      case InMemoryLoadStoreMarkingPhase:
         return "InMemoryLoadStoreMarkingPhase";
      case ReduceSynchronizedFieldLoadPhase:
         return "ReduceSynchronizedFieldLoadPhase";
      case UncommonBCDCHKAddressNodePhase:
         return "UncommonBCDCHKAddressNodePhase";
      default:
	      return J9::CodeGenPhase::getName(phase);
      }
   }

