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
#include "codegen/CodeGenerator.hpp"
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
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "ras/DebugCounter.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/RelocationRecord.hpp"
#include "runtime/SymbolValidationManager.hpp"

J9::Power::AheadOfTimeCompile::AheadOfTimeCompile(TR::CodeGenerator *cg) :
         J9::AheadOfTimeCompile(NULL, cg->comp()),
         _cg(cg)
   {
   }

void J9::Power::AheadOfTimeCompile::processRelocations()
   {
   for (auto iterator = getRelocationList().begin(); iterator != getRelocationList().end(); ++iterator)
      {
      (*iterator)->mapRelocation(_cg);
      }

   J9::AheadOfTimeCompile::processRelocations();
   }

bool
J9::Power::AheadOfTimeCompile::initializePlatformSpecificAOTRelocationHeader(TR::IteratedExternalRelocation *relocation,
                                                                             TR_RelocationTarget *reloTarget,
                                                                             TR_RelocationRecord *reloRecord,
                                                                             uint8_t targetKind)
   {
   bool platformSpecificReloInitialized = true;
   TR::Compilation *comp = self()->comp();
   TR_J9VMBase *fej9 = comp->fej9();
   TR_SharedCache *sharedCache = fej9->sharedCache();
   uint8_t * aotMethodCodeStart = (uint8_t *) comp->getRelocatableMethodCodeStart();

   switch (targetKind)
      {
      case TR_ClassAddress:
         {
         TR_RelocationRecordClassAddress *caRecord = reinterpret_cast<TR_RelocationRecordClassAddress *>(reloRecord);
         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation*) relocation->getTargetAddress();

         TR::SymbolReference *symRef = reinterpret_cast<TR::SymbolReference *>(recordInfo->data1);
         uintptr_t inlinedSiteIndex = reinterpret_cast<uintptr_t>(recordInfo->data2);
         uint8_t flags = static_cast<uint8_t>(recordInfo->data3);

         void *constantPool = symRef->getOwningMethod(comp)->constantPool();
         inlinedSiteIndex = self()->findCorrectInlinedSiteIndex(constantPool, inlinedSiteIndex);

         caRecord->setReloFlags(reloTarget, flags);
         caRecord->setInlinedSiteIndex(reloTarget, inlinedSiteIndex);
         caRecord->setConstantPool(reloTarget, reinterpret_cast<uintptr_t>(constantPool));
         caRecord->setCpIndex(reloTarget, symRef->getCPIndex());
         }
         break;

      case TR_DataAddress:
         {
         TR_RelocationRecordDataAddress *daRecord = reinterpret_cast<TR_RelocationRecordDataAddress *>(reloRecord);
         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation*) relocation->getTargetAddress();

         TR::SymbolReference *symRef = reinterpret_cast<TR::SymbolReference *>(recordInfo->data1);
         uintptr_t inlinedSiteIndex = reinterpret_cast<uintptr_t>(recordInfo->data2);
         uint8_t flags = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(recordInfo->data3));

         void *constantPool = symRef->getOwningMethod(comp)->constantPool();
         inlinedSiteIndex = self()->findCorrectInlinedSiteIndex(constantPool, inlinedSiteIndex);

         daRecord->setReloFlags(reloTarget, flags);
         daRecord->setInlinedSiteIndex(reloTarget, inlinedSiteIndex);
         daRecord->setConstantPool(reloTarget, reinterpret_cast<uintptr_t>(constantPool));
         daRecord->setCpIndex(reloTarget, symRef->getCPIndex());
         daRecord->setOffset(reloTarget, symRef->getOffset());
         }
         break;

      case TR_AbsoluteHelperAddress:
         {
         TR_RelocationRecordAbsoluteHelperAddress *ahaRecord = reinterpret_cast<TR_RelocationRecordAbsoluteHelperAddress *>(reloRecord);
         TR::SymbolReference *symRef = reinterpret_cast<TR::SymbolReference *>(relocation->getTargetAddress());
         uint8_t flags = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(relocation->getTargetAddress2()));

         ahaRecord->setHelperID(reloTarget, static_cast<uint32_t>(symRef->getReferenceNumber()));
         ahaRecord->setReloFlags(reloTarget, flags);
         }
         break;

      case TR_AbsoluteMethodAddressOrderedPair:
         {
         TR_RelocationRecordMethodAddress *maRecord = reinterpret_cast<TR_RelocationRecordMethodAddress *>(reloRecord);

         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation *) relocation->getTargetAddress();
         uint8_t flags = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(recordInfo->data3));

         maRecord->setReloFlags(reloTarget, flags);
         }
         break;

      case TR_FixedSequenceAddress:
         {
         TR_RelocationRecordWithOffset *rwoRecord = reinterpret_cast<TR_RelocationRecordWithOffset *>(reloRecord);

         TR::LabelSymbol *table = reinterpret_cast<TR::LabelSymbol *>(relocation->getTargetAddress());
         uint8_t flags = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(relocation->getTargetAddress2()));
         uint8_t *codeLocation = table->getCodeLocation();

         rwoRecord->setReloFlags(reloTarget, flags);
         if (comp->target().is64Bit())
            {
            rwoRecord->setOffset(reloTarget, static_cast<uintptr_t>(codeLocation - aotMethodCodeStart));
            }
         else
            {
            TR_ASSERT_FATAL(false, "Creating TR_FixedSeqAddress/TR_FixedSeq2Address relo for 32-bit target");
            }
         }
         break;

      case TR_FixedSequenceAddress2:
         {
         TR_RelocationRecordWithOffset *rwoRecord = reinterpret_cast<TR_RelocationRecordWithOffset *>(reloRecord);
         uint8_t flags = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(relocation->getTargetAddress2()));

         rwoRecord->setReloFlags(reloTarget, flags);
         if (comp->target().is64Bit())
            {
            TR_ASSERT_FATAL(relocation->getTargetAddress(), "target address is NULL");

            uintptr_t offset = relocation->getTargetAddress()
                     ? static_cast<uintptr_t>(relocation->getTargetAddress() - aotMethodCodeStart)
                     : 0x0;

            rwoRecord->setOffset(reloTarget, offset);
            }
         else
            {
            TR_ASSERT_FATAL(0, "Creating TR_LoadAddress/TR_LoadAddressTempReg relo for 32-bit target");
            }
         }
         break;

      case TR_ArrayCopyHelper:
      case TR_ArrayCopyToc:
      case TR_BodyInfoAddressLoad:
      case TR_CatchBlockCounter:
      case TR_StartPC:
      case TR_RecompQueuedFlag:
         {
         TR_RelocationRecord *rRecord = reinterpret_cast<TR_RelocationRecord *>(reloRecord);
         uint8_t flags;

         if (comp->target().is64Bit())
            {
            flags = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(relocation->getTargetAddress2()));
            }
         else
            {
            TR_RelocationRecordInformation *recordInfo = reinterpret_cast<TR_RelocationRecordInformation *>(relocation->getTargetAddress());
            flags = static_cast<uint8_t>(recordInfo->data3);
            }

         rRecord->setReloFlags(reloTarget, flags);
         }
         break;

      case TR_RamMethodSequence:
         {
         TR_RelocationRecordRamSequence *rsRecord = reinterpret_cast<TR_RelocationRecordRamSequence *>(reloRecord);
         uint8_t flags;

         if (comp->target().is64Bit())
            {
            flags = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(relocation->getTargetAddress2()));

            TR_ASSERT_FATAL(relocation->getTargetAddress(), "target address is NULL");

            uintptr_t offset = relocation->getTargetAddress()
                     ? static_cast<uintptr_t>(relocation->getTargetAddress() - aotMethodCodeStart)
                     : 0x0;

            rsRecord->setOffset(reloTarget, offset);
            }
         else
            {
            TR_RelocationRecordInformation *recordInfo = reinterpret_cast<TR_RelocationRecordInformation *>(relocation->getTargetAddress());
            flags = static_cast<uint8_t>(recordInfo->data3);

            // Skip Offset
            }

         rsRecord->setReloFlags(reloTarget, flags);
         }
         break;

      case TR_ArbitraryClassAddress:
         {
         TR_RelocationRecordArbitraryClassAddress *acaRecord = reinterpret_cast<TR_RelocationRecordArbitraryClassAddress *>(reloRecord);

         // ExternalRelocation data is as expected for TR_ClassAddress
         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation *)relocation->getTargetAddress();

         auto symRef = (TR::SymbolReference *)recordInfo->data1;
         auto sym = symRef->getSymbol()->castToStaticSymbol();
         auto j9class = (TR_OpaqueClassBlock *)sym->getStaticAddress();
         uint8_t flags = (uint8_t)recordInfo->data3;
         uintptr_t inlinedSiteIndex = self()->findCorrectInlinedSiteIndex(symRef->getOwningMethod(comp)->constantPool(), recordInfo->data2);

         uintptr_t classChainIdentifyingLoaderOffsetInSharedCache = sharedCache->getClassChainOffsetIdentifyingLoader(j9class);
         const AOTCacheClassChainRecord *classChainRecord = NULL;
         uintptr_t classChainOffsetInSharedCache = self()->getClassChainOffset(j9class, classChainRecord);

         acaRecord->setReloFlags(reloTarget, flags);
         acaRecord->setInlinedSiteIndex(reloTarget, inlinedSiteIndex);
         acaRecord->setClassChainIdentifyingLoaderOffsetInSharedCache(reloTarget, classChainIdentifyingLoaderOffsetInSharedCache,
                                                                      self(), classChainRecord);
         acaRecord->setClassChainForInlinedMethod(reloTarget, classChainOffsetInSharedCache, self(), classChainRecord, j9class);
         }
         break;

      case TR_GlobalValue:
         {
         TR_RelocationRecordGlobalValue *gvRecord = reinterpret_cast<TR_RelocationRecordGlobalValue *>(reloRecord);

         uintptr_t gv;
         uint8_t flags;

         if (comp->target().is64Bit())
            {
            gv = reinterpret_cast<uintptr_t>(relocation->getTargetAddress());
            flags = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(relocation->getTargetAddress2()));
            }
         else
            {
            TR_RelocationRecordInformation *recordInfo = reinterpret_cast<TR_RelocationRecordInformation*>(relocation->getTargetAddress());
            gv = recordInfo->data1;
            flags = static_cast<uint8_t>(recordInfo->data3);
            }

         gvRecord->setReloFlags(reloTarget, flags);
         gvRecord->setOffset(reloTarget, gv);
         }
         break;

      case TR_DiscontiguousSymbolFromManager:
         {
         TR_RelocationRecordDiscontiguousSymbolFromManager *dsfmRecord = reinterpret_cast<TR_RelocationRecordDiscontiguousSymbolFromManager *>(reloRecord);

         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation*) relocation->getTargetAddress();

         uint8_t *symbol = (uint8_t *)recordInfo->data1;
         uint16_t symbolID = comp->getSymbolValidationManager()->getSymbolIDFromValue(static_cast<void *>(symbol));

         uint16_t symbolType = (uint16_t)recordInfo->data2;

         uint8_t flags = (uint8_t) recordInfo->data3;

         dsfmRecord->setSymbolID(reloTarget, symbolID);
         dsfmRecord->setSymbolType(reloTarget, static_cast<TR::SymbolType>(symbolType));
         dsfmRecord->setReloFlags(reloTarget, flags);
         }
         break;

      case TR_HCR:
         {
         TR_RelocationRecordHCR *hcrRecord = reinterpret_cast<TR_RelocationRecordHCR *>(reloRecord);

         uintptr_t gv = reinterpret_cast<uintptr_t>(relocation->getTargetAddress());
         uint8_t flags = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(relocation->getTargetAddress2()));

         hcrRecord->setReloFlags(reloTarget, flags);
         hcrRecord->setOffset(reloTarget, gv);
         }
         break;

      default:
         platformSpecificReloInitialized = false;
      }

   return platformSpecificReloInitialized;
   }

