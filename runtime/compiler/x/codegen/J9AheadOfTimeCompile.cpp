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

#include "codegen/AheadOfTimeCompile.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/Instruction.hpp"
#include "compile/AOTClassInfo.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/VirtualGuard.hpp"
#include "env/CHTable.hpp"
#include "env/ClassLoaderTable.hpp"
#include "env/CompilerEnv.hpp"
#include "env/SharedCache.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "ras/DebugCounter.hpp"
#include "runtime/CodeCacheConfig.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/RelocationRecord.hpp"
#include "runtime/SymbolValidationManager.hpp"

void J9::X86::AheadOfTimeCompile::processRelocations()
   {
   TR::Compilation *comp = _cg->comp();

   if (comp->target().is64Bit()
       && TR::CodeCacheManager::instance()->codeCacheConfig().needsMethodTrampolines()
       && _cg->getPicSlotCount())
      {
      _cg->addExternalRelocation(
         TR::ExternalRelocation::create(
            NULL,
            (uint8_t *)(uintptr_t)_cg->getPicSlotCount(),
            TR_PicTrampolines,
            _cg),
         __FILE__,
         __LINE__,
         NULL);
      }

   J9::AheadOfTimeCompile::processRelocations();
   }

bool
J9::X86::AheadOfTimeCompile::initializePlatformSpecificAOTRelocationHeader(TR::IteratedExternalRelocation *relocation,
                                                                           TR_RelocationTarget *reloTarget,
                                                                           TR_RelocationRecord *reloRecord,
                                                                           uint8_t targetKind)
   {
   bool platformSpecificReloInitialized = true;

   switch (targetKind)
      {
      case TR_PicTrampolines:
         {
         TR_RelocationRecordPicTrampolines *ptRecord = reinterpret_cast<TR_RelocationRecordPicTrampolines *>(reloRecord);

         TR_ASSERT(self()->comp()->target().is64Bit(), "TR_PicTrampolines not supported on 32-bit");
         uint32_t numTrampolines = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(relocation->getTargetAddress()));
         ptRecord->setNumTrampolines(reloTarget, numTrampolines);
         }
        break;

      default:
         platformSpecificReloInitialized = false;
      }

   return platformSpecificReloInitialized;
   }

