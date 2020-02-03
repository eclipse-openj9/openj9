/*******************************************************************************
 * Copyright (c) 2022, 2022 IBM Corp. and others
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
#include "microjit/x/amd64/AMD64CodegenGC.hpp"

#include <stdint.h>
#include <string.h>
#include "env/StackMemoryRegion.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/GCStackMap.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/ObjectModel.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AutomaticSymbol.hpp"
#include "il/Node.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/IGNode.hpp"
#include "infra/InterferenceGraph.hpp"
#include "infra/List.hpp"
#include "microjit/SideTables.hpp"
#include "microjit/utils.hpp"
#include "ras/Debug.hpp"


MJIT::CodeGenGC::CodeGenGC(TR::FilePointer *logFileFP)
   :_logFileFP(logFileFP)
{}

TR::GCStackAtlas*
MJIT::CodeGenGC::createStackAtlas(TR::Compilation *comp, MJIT::ParamTable *paramTable, MJIT::LocalTable *localTable)
   {

   // --------------------------------------------------------------------------------
   // Construct the parameter map for mapped reference parameters
   //
   intptr_t stackSlotSize = TR::Compiler->om.sizeofReferenceAddress();
   U_16 paramCount = paramTable->getParamCount();
   U_16 ParamSlots = paramTable->getTotalParamSize()/stackSlotSize;
   TR_GCStackMap *parameterMap = new (comp->trHeapMemory(), paramCount) TR_GCStackMap(paramCount);

   int32_t firstMappedParmOffsetInBytes = -1;
   MJIT::ParamTableEntry entry;
   for(int i=0; i<paramCount;){
      MJIT_ASSERT(_logFileFP, paramTable->getEntry(i, &entry), "Bad index for table entry");
      if(entry.notInitialized) {
         i++;
         continue;
      }
      if(!entry.notInitialized && entry.isReference){
         firstMappedParmOffsetInBytes = entry.gcMapOffset;
         break;
      }
      i += entry.slots;
   }

   for(int i=0; i<paramCount;){
      MJIT_ASSERT(_logFileFP, paramTable->getEntry(i, &entry), "Bad index for table entry");
      if(entry.notInitialized) {
         i++;
         continue;
      }
      if(!entry.notInitialized && entry.isReference){
         int32_t entryOffsetInBytes = entry.gcMapOffset;
         parameterMap->setBit(((entryOffsetInBytes-firstMappedParmOffsetInBytes)/stackSlotSize));
         if(!entry.onStack){
            parameterMap->setRegisterBits(TR::RealRegister::gprMask((TR::RealRegister::RegNum)entry.regNo));
         }
      }
      i += entry.slots;
   }

   // --------------------------------------------------------------------------------
   // Construct the local map for mapped reference locals.
   // This does not duplicate items mapped from the parameter table.
   //
   firstMappedParmOffsetInBytes =
      (firstMappedParmOffsetInBytes >= 0) ? firstMappedParmOffsetInBytes : 0;
   U_16 localCount = localTable->getLocalCount();
   TR_GCStackMap *localMap = new (comp->trHeapMemory(), localCount) TR_GCStackMap(localCount);
   localMap->copy(parameterMap);
   for(int i=paramCount; i<localCount;){
      MJIT_ASSERT(_logFileFP, localTable->getEntry(i, &entry), "Bad index for table entry");
      if(entry.notInitialized) {
         i++;
         continue;
      }
      if(!entry.notInitialized && entry.offset == -1 && entry.isReference){
         int32_t entryOffsetInBytes = entry.gcMapOffset;
         localMap->setBit(((entryOffsetInBytes-firstMappedParmOffsetInBytes)/stackSlotSize));
         if(!entry.onStack){
            localMap->setRegisterBits(TR::RealRegister::gprMask((TR::RealRegister::RegNum)entry.regNo));
         }
      }
      i += entry.slots;
   }

   // --------------------------------------------------------------------------
   // Now create the stack atlas
   //
   TR::GCStackAtlas * atlas = new (comp->trHeapMemory()) TR::GCStackAtlas(paramCount, localCount, comp->trMemory());
   atlas->setParmBaseOffset(firstMappedParmOffsetInBytes);
   atlas->setParameterMap(parameterMap);
   atlas->setLocalMap(localMap);
   // MJIT does not create stack allocated objects
   atlas->setStackAllocMap(NULL);
   // MJIT must initialize all locals which are not parameters
   atlas->setNumberOfSlotsToBeInitialized(localCount - paramCount);
   atlas->setIndexOfFirstSpillTemp(localCount);
   // MJIT does not use internal pointers
   atlas->setInternalPointerMap(NULL);
   // MJIT does not mimic interpreter frame shape
   atlas->setNumberOfPendingPushSlots(0);
   atlas->setNumberOfPaddingSlots(0);
   return atlas;
   }
