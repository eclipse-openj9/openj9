/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include "p/codegen/PPCInstruction.hpp"

#include "codegen/CodeGenerator.hpp"
#include "codegen/Relocation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/CompilerEnv.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "p/codegen/CallSnippet.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/Runtime.hpp"

uint8_t *TR::PPCDepImmSymInstruction::generateBinaryEncoding()
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());
   TR::Compilation *comp = cg()->comp();
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   intptr_t imm = getAddrImmediate();

   if (getOpCodeValue() == TR::InstOpCode::bl || getOpCodeValue() == TR::InstOpCode::b)
      {
      int32_t refNum = getSymbolReference()->getReferenceNumber();
      TR::ResolvedMethodSymbol *sym = getSymbolReference()->getSymbol()->getResolvedMethodSymbol();
      TR_ResolvedMethod *resolvedMethod = sym == NULL ? NULL : sym->getResolvedMethod();
      TR::LabelSymbol *label = getSymbolReference()->getSymbol()->getLabelSymbol();
      bool callToSelf = comp->isRecursiveMethodTarget(resolvedMethod);

      if (cg()->hasCodeCacheSwitched())
         {
         TR::SymbolReference *calleeSymRef = NULL;

         if (label == NULL)
            {
            calleeSymRef = getSymbolReference();
            }
         else
            {
            if (label->getSnippet() != NULL)
               {
               TR::Snippet *snippet = label->getSnippet();
               if (snippet->getKind() == TR::Snippet::IsCall)
                  {
                  calleeSymRef = ((TR::PPCCallSnippet *)snippet)->getRealMethodSymbolReference();
                  }
               }

            if (calleeSymRef == NULL && getNode() != NULL)
               {
               calleeSymRef = getNode()->getSymbolReference();
               }
            }

         if (calleeSymRef != NULL)
            {
            if (calleeSymRef->getReferenceNumber() >= TR_PPCnumRuntimeHelpers &&
                (calleeSymRef->getReferenceNumber() < cg()->symRefTab()->getNonhelperIndex(TR::SymbolReferenceTable::firstPerCodeCacheHelperSymbol) ||
                 calleeSymRef->getReferenceNumber() > cg()->symRefTab()->getNonhelperIndex(TR::SymbolReferenceTable::lastPerCodeCacheHelperSymbol)))
               {
               fej9->reserveTrampolineIfNecessary(comp, calleeSymRef, true);
               }
            }
         else
            {
            TR_ASSERT(false, "Missing possible re-reservation for trampolines.");
            }
         }

      if (callToSelf)
         {
         intptr_t jitToJitStart = cg()->getLinkage()->entryPointFromCompiledMethod();
         *(int32_t *)cursor |= (reinterpret_cast<uint8_t *>(jitToJitStart) - cursor) & 0x03fffffc;
         }
      else if (label != NULL)
         {
         cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative24BitRelocation(cursor, label));
         ((TR::PPCCallSnippet *)getCallSnippet())->setCallRA(cursor + 4);
         }
      else
         {
         if (comp->target().cpu.isTargetWithinIFormBranchRange(imm, (intptr_t)cursor))
            {
            *(int32_t *)cursor |= (imm - (intptr_t)cursor) & 0x03fffffc;
            }
         else
            {
            intptr_t targetAddress;
            if (refNum < TR_PPCnumRuntimeHelpers)
               {
               targetAddress = TR::CodeCacheManager::instance()->findHelperTrampoline(refNum, (void *)cursor);
               }
            else if (refNum >= cg()->symRefTab()->getNonhelperIndex(TR::SymbolReferenceTable::firstPerCodeCacheHelperSymbol) &&
                     refNum <= cg()->symRefTab()->getNonhelperIndex(TR::SymbolReferenceTable::lastPerCodeCacheHelperSymbol))
               {
               TR_ASSERT(cg()->hasCodeCacheSwitched(), "Expecting per-codecache helper to be unreachable only when codecache was switched");
               TR_CCPreLoadedCode helper = (TR_CCPreLoadedCode)(refNum - cg()->symRefTab()->getNonhelperIndex(TR::SymbolReferenceTable::firstPerCodeCacheHelperSymbol));
               _addrImmediate = (uintptr_t)fej9->getCCPreLoadedCodeAddress(cg()->getCodeCache(), helper, cg());
               targetAddress = (intptr_t)_addrImmediate;
               }
            else
               {
               // Must use the trampoline as the target and not the label
               //
               targetAddress = (intptr_t)fej9->methodTrampolineLookup(comp, getSymbolReference(), (void *)cursor);
               }

            TR_ASSERT_FATAL(comp->target().cpu.isTargetWithinIFormBranchRange(targetAddress, (intptr_t)cursor),
                            "Call target address is out of range");
            *(int32_t *)cursor |= (targetAddress - (intptr_t)cursor) & 0x03fffffc;
            }
         }

      if ((comp->compileRelocatableCode()
          #ifdef J9VM_OPT_JITSERVER
             || comp->isOutOfProcessCompilation()
          #endif
          ) &&
          label == NULL &&
          !callToSelf)
         {
         bool callIsJ2ITransition = runtimeHelperValue(TR_j2iTransition) == getSymbolReference()->getMethodAddress();
         if (sym && !sym->isHelper() && resolvedMethod && !callIsJ2ITransition)
            {
            cg()->addProjectSpecializedRelocation(cursor, (uint8_t *)getSymbolReference()->getMethodAddress(), NULL, TR_MethodCallAddress,
                               __FILE__, __LINE__, getNode());
            }
         else
            {
            cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,(uint8_t *)getSymbolReference(),TR_HelperAddress, cg()),
                               __FILE__, __LINE__, getNode());
            }
         }
      }
   else
      {
      intptr_t distance = imm - (intptr_t)cursor;
      // Place holder only: non-TR::InstOpCode::b[l] usage of this instruction doesn't
      // exist at this moment.
      *(int32_t *)cursor |= distance & 0x03fffffc;
      }

   cursor += 4;
   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }
