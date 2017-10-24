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

#include "z/codegen/S390StackCheckFailureSnippet.hpp"

#include "codegen/AheadOfTimeCompile.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/GCStackMap.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/SnippetGCMap.hpp"
#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "z/codegen/TRSystemLinkage.hpp"

TR::S390StackCheckFailureSnippet::S390StackCheckFailureSnippet
                        (TR::CodeGenerator        *cg,
                            TR::Node                 *node,
                            TR::LabelSymbol           *restartlab,
                            TR::LabelSymbol           *snippetlab,
                            TR::SymbolReference      *helper,
                            uint32_t                 frameSize)
   : TR::Snippet(cg, node, snippetlab, false),
       _reStartLabel(restartlab),
       _destination(helper),
       _frameSize(frameSize),
       _argSize(0)
   {
   }

uint8_t *
TR::S390StackCheckFailureSnippet::emitSnippetBody()
   {
   uint8_t * cursor = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(cursor);
   uint8_t * callSite = cursor;
   uint8_t * returnLocationInSnippet;
   uint32_t rEP = (uint32_t) cg()->getEntryPointRegister() - 1;
   bool is64BitTarget = TR::Compiler->target.is64Bit();
   TR::Linkage * linkage = cg()->getS390Linkage();
   TR::Compilation* comp = cg()->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());

   TR_ASSERT(_reStartLabel != NULL, "Missing restart label in StackCheckFailureSnippet \n");

   bool requireRAStore = !linkage->getRaContextRestoreNeeded();   //  If we don't restore the RA in the epilog
                                                                  //  we need to do so in the stack ext code.

   //If TR_PrologTuning is set then we need to save RS on the stack for stack walker
   static bool prologTuning = (feGetEnv("TR_PrologTuning")!=NULL);
   if (prologTuning)
      requireRAStore = true;

   bool requireRALoad  = requireRAStore;

   // These are assumption checks to make sure our offsets to data constants at the end are correct.
   intptrj_t startOfHelperBASR = -1;
   intptrj_t locOfHelperAddr = -1;
   intptrj_t offsetHelperAddr = 0;

   intptrj_t startOfRetBASR = -1;
   intptrj_t locOfRetAddr = -1;
   intptrj_t offsetRetAddr = 0;

   //-----------------------------------------------------------------------------------//
   // This section of code:
   //     1.  Load the frame size into r0
   //     2.  Unwind the stack by frame size (i.e. rSP = rSP + _frameSize
   //
   // 3 options here:
   //   1.  if _frameSize <=  (12-bits RX instruction)
   //       - Generate:
   //                   -->Non-Trex<-- ST/STG r14, _frameSize(r5)
   //                   LHI/LGHI  r0,_frameSize.
   //                   AHI/AGHI  r5,_frameSize.
   //
   //   2.  if _frameSize <  (16-bits RI instruction) but > 12 bits.
   //       - Generate: AHI/AGHI  r5, _framesize
   //                   LHI/LGHI  r0, _framesize.
   //
   //   3.  Otherwise
   //       - Generate: BRAS      rEP, 4/6      // 1-cycle AGI with L.
   //                   DC        <_frameSize>
   //                   A/AG      r5, 0(rEP)
   //                   L/LG      r0, 0(rEP)

   if (getFrameSize() < MAXDISP)                                                         // --- Under 12-bit frame ---
      {
      // If we need to save RA onto the stack, generate it now to avoid AGI delays.
      if (requireRAStore)
         {
         if (is64BitTarget)                                                              // --> Non-Trex <-- ST/STG  r14, _frameSize(r5)
            {
            *(int32_t *) cursor = 0xe3e05000 + getFrameSize();   // STG r14, _frameSize(r5)
            cursor += sizeof(int32_t);
            *(int16_t *) cursor = 0x0024;
            cursor += sizeof(int16_t);
            }
         else
            {
            *(int32_t *) cursor = 0x50e05000 + getFrameSize();   // ST  r14, _frameSize(r5)
            cursor += sizeof(int32_t);
            }
         requireRAStore = false;
         }

      *(int32_t *) cursor = ((is64BitTarget)?0xa7090000:0xa7080000) + getFrameSize();    // LHI/LGHI r0, _frameSize
      cursor += sizeof(int32_t);
      *(int32_t *) cursor = ((is64BitTarget)?0xa75b0000:0xa75a0000) + getFrameSize();    // AHI/AGHI r5, _frameSize
      cursor += sizeof(int32_t);
      }                                                                                  // -- End of Under 12 Frame ---
   else if (getFrameSize() < MAX_IMMEDIATE_VAL)                                          // --- Under 16 Frame ------
      {                                                                                  // Unwind Stack Frame
      *(int32_t *) cursor = ((is64BitTarget)?0xa75b0000:0xa75a0000) + getFrameSize();    // AHI/AGHI r5, _frameSize
      cursor += sizeof(int32_t);

      *(int32_t *) cursor = ((is64BitTarget)?0xa7090000:0xa7080000) + getFrameSize();    // LHI/LGHI r0, _frameSize
      cursor += sizeof(int32_t);
      }                                                                                  // -- End of Under 16 Frame ---
   else                                                                                  // ---  Big Frame ---
      {
      *(int32_t *) cursor = 0xa7050000 + (rEP << 20) + (2 + TR::Compiler->om.sizeofReferenceAddress()/2);        // BRAS     rEP, 4/6
      cursor += sizeof(int32_t);
      startOfHelperBASR = (intptrj_t)cursor;
      *(intptrj_t *) cursor = (intptrj_t) getFrameSize();                                // DC       <_frameSize>
      cursor += TR::Compiler->om.sizeofReferenceAddress();

      // Unwind the stack frame.
      if (is64BitTarget)                                                                 // A/AG     r5, 0(rEP)
         {                                                                               // L/LG     r0, 0(rEP)
         // AG r5, 0(rEP)
         *(int32_t *) cursor = 0xe3500000 + (rEP << 12);   // Unwind 64-Bit stack.
         cursor += sizeof(int32_t);
         *(int16_t *) cursor = 0x0008;
         cursor += sizeof(int16_t);

         // LG r0, 0(rEP)
         *(int32_t *) cursor = 0xe3000000 + (rEP << 12);   // Load 64-bit framesize.
         cursor += sizeof(int32_t);
         *(int16_t *) cursor = 0x0004;
         cursor += sizeof(int16_t);
         }
      else
         {
         *(int32_t *) cursor = 0x5a500000 + (rEP << 12);   // Unwind 32-bit stack.
         cursor += sizeof(int32_t);
         *(int32_t *) cursor = 0x58000000 + (rEP << 12);   // Load 32-bit framesize.
         cursor += sizeof(int32_t);
         }
      }                                                                                  // --- End of Big Frame ---

   // If Full Speed Debug, save the arguments back onto the stack.
   if (TR::comp()->getOption(TR_FullSpeedDebug))
      {
      uint8_t *prevCursor = cursor;
      cursor = (uint8_t *) cg()->getS390Linkage()->saveArguments(cursor, true, true, getFrameSize());
      _argSize = cursor - prevCursor;
      }
//---------------------------------------------------------------------------------------//
//   At this point, we should have the following conditions satisfied:                   //
//          1.  r0 has _frameSize                                                        //
//          2.  rSP has been unwinded by _frameSize                                      //
//                                                                                       //
//   Requirements for the next section:                                                  //
//          1.  r14 needs to be stored on the stack UNLESS requireRAStore is unset.      //
//          2.  Branch to Helper Address.                                                //
//          3.  reload r14 from stack UNLESS requireRALoad is unset.                     //
//---------------------------------------------------------------------------------------//

   // Since N3 Instructions are supported, we can take advantage of relative long instructions
   // On Non-Trex machines, we also need to save RA onto the stack.
   // Sequence of instructions we generate:
   //          ST/STG r14, 0 (r5)  <-- Non-Trex only.
   //          BRASL  r14, <target Helper addr>
   //          L/LG   r14, 0 (r5)  <-- Non-Trex only.
   if (requireRAStore)                                                                // -->non-Trex<--   ST/STG  r14, 0(r5)
      {
      if (is64BitTarget)
         {
         *(int32_t *) cursor = 0xe3e05000;          // STG r14, 0(r5)
         cursor += sizeof(int32_t);
         *(int16_t *) cursor = 0x0024;
         cursor += sizeof(int16_t);
         }
      else
         {
         *(int32_t *) cursor = 0x50e05000;          // ST  r14, 0 (r5)
         cursor += sizeof(int32_t);
         }
      }

#if !defined(PUBLIC_BUILD)
   // Generate RIOFF if RI is supported.
   cursor = generateRuntimeInstrumentationOnOffInstruction(cg(), cursor, TR::InstOpCode::RIOFF);
#endif

   // Now we generate a BRASL to target
   *(int16_t *) cursor = 0xC0E5;                                                      // BRASL     r14, <Helper Addr>
   cursor += sizeof(int16_t);

   // Calculate the relative offset to get to helper method.
   // If MCC is not supported, everything should be reachable.
   // If MCC is supported, we will look up the appropriate trampoline, if
   // necessary.
   intptrj_t destAddr = (intptrj_t)(getDestination()->getSymbol()->castToMethodSymbol()->getMethodAddress());

#if defined(TR_TARGET_64BIT) 
#if defined(J9ZOS390)
   if (comp->getOption(TR_EnableRMODE64))
#endif
      {
      if (NEEDS_TRAMPOLINE(destAddr, cursor, cg()))
         {
         // Destination is beyond our reachable jump distance, we'll find the
         // trampoline.
         destAddr = fej9->indexedTrampolineLookup(getDestination()->getReferenceNumber(), (void *)cursor);
         this->setUsedTrampoline(true);
         }
      }
#endif
   TR_ASSERT(CHECK_32BIT_TRAMPOLINE_RANGE(destAddr, cursor), "Helper Call is not reachable.");
   this->setSnippetDestAddr(destAddr);

   *(int32_t *) cursor = (int32_t)((destAddr - (intptrj_t)(cursor - 2)) / 2);
   AOTcgDiag5(cg()->comp(), "cursor=%x destAddr=%x TR_HelperAddress=%x cg=%x %x\n",
   cursor, getDestination()->getSymbol(), TR_HelperAddress, cg(), *((int*) cursor) );
   cg()->addProjectSpecializedRelocation(cursor, (uint8_t*) getDestination(), NULL, TR_HelperAddress,
                             __FILE__, __LINE__, getNode());

   cursor += sizeof(int32_t);
   returnLocationInSnippet = cursor;

#if !defined(PUBLIC_BUILD)
   // Generate RION if RI is supported.
   //temporary disable to allow RI on zlinux
   //cursor = generateRuntimeInstrumentationOnOffInstruction(cg(), cursor, TR::InstOpCode::RION);
#endif

   // If non-Trex, we need to reload the RA from the stack.
   if (requireRALoad)                                                                 // -->non-Trex<--  L/LG     r14, 0(r5)
      {
      if (is64BitTarget)
         {
         *(int32_t *) cursor = 0xe3e05000;          // LG r14, 0(r5)
         cursor += sizeof(int32_t);
         *(int16_t *) cursor = 0x0004;
         cursor += sizeof(int16_t);
         }
      else
         {
         *(int32_t *) cursor = 0x58e05000;          // L  r14, 0 (r5)
         cursor += sizeof(int32_t);
         }
      }
//---------------------------------------------------------------------------------------//
//   Requirements for the next section:                                                  //
//          1.  Readjust the stack frame by _frameSize                                   //
//          2.  Branch to Return Address of main line code.                              //
//---------------------------------------------------------------------------------------//

      // Restore the stack pointer.  r0 is saved and restored in the helper.
      if (is64BitTarget)                                                                  // SR/SGR  r5, r0
         {
         *(int32_t *)cursor = 0xb9090050;
         cursor += sizeof(int32_t);
         }
      else
         {
         *(int16_t *)cursor = 0x1b50;
         cursor += sizeof(int16_t);
         }

      // Branch back to main line code.
      //We only require BRCL <return address>.  It is guaranteed to be reachable.
      intptrj_t returnAddr = (intptrj_t)_reStartLabel->getCodeLocation();             // BRCL   <Return Addr>
      *(int16_t *) cursor = 0xC0F4;
      cursor += sizeof(int16_t);
      *(int32_t *) cursor = (int32_t)((returnAddr - (intptrj_t)(cursor - 2)) / 2);
      cursor += sizeof(int32_t);
      // Set the Stack Atlas.
      setStackAtlasHelper();
      gcMap().registerStackMap (returnLocationInSnippet, cg());
      return cursor;
   }


void
TR::S390StackCheckFailureSnippet::setStackAtlasHelper()
   {
   TR::Linkage * linkage = cg()->getS390Linkage();
   TR::Compilation *comp = cg()->comp();
   TR::ResolvedMethodSymbol * bodySymbol = comp->getJittedMethodSymbol();
   ListIterator<TR::ParameterSymbol> paramIterator(&(bodySymbol->getParameterList()));
   TR::ParameterSymbol * paramCursor;
   paramIterator.reset();
   paramCursor = paramIterator.getFirst();

   TR::GCStackAtlas * atlas = cg()->getStackAtlas();
   if (atlas)
      {
      // only the arg references are live at this point
      uint32_t numberOfParmSlots = atlas->getNumberOfParmSlotsMapped();
      TR_GCStackMap * map = new (cg()->trHeapMemory(), numberOfParmSlots) TR_GCStackMap(numberOfParmSlots);

      map->copy(atlas->getParameterMap());
      while (paramCursor != NULL)
         {
         int32_t intRegArgIndex = paramCursor->getLinkageRegisterIndex();
         if (intRegArgIndex >= 0 && (paramCursor->isReferencedParameter() || comp->getOption(TR_FullSpeedDebug)) && paramCursor->isCollectedReference())
            {
            // In full speed debug all the parameters are passed in the stack for this case
            // but will also reside in the registers
            if (!comp->getOption(TR_FullSpeedDebug))
               {
               map->resetBit(paramCursor->getGCMapIndex());
               }
            map->setRegisterBits(cg()->registerBitMask((int32_t) linkage->getIntegerArgumentRegister(intRegArgIndex)));
            }
         paramCursor = paramIterator.getNext();
         }

      // set the GC map
      gcMap().setStackMap(map);
      atlas->setParameterMap(map);
      }
   }

uint32_t
TR::S390StackCheckFailureSnippet::getLength(int32_t)
   {
   int32_t size = 0;
   bool is64BitTarget = TR::Compiler->target.is64Bit();
   TR::Linkage * linkage = cg()->getS390Linkage();

   //  We need the extra RA store/reload in two cases:
   //   1) Non-TREX hardware
   //   2) avoiding the RAREG context restore.
   // #2 cannot be safely determined at length estimation time, so we
   // have ot be pessimistic and assume that it will happen.
   //
   bool requireRAStore = true;

   if (requireRAStore)
      {
      size += (is64BitTarget)?12:8;   // ST/STG + L/LG
      }

   if (getFrameSize() < MAXDISP)
      {
      size += 8;                      // LHI/LGHI + AHI/AGHI
      }
   else if (getFrameSize() < MAX_IMMEDIATE_VAL)
      {
      size += 8;                      // LHI/LGHI + AHI/AGHI
      }
   else
      {
      size += (is64BitTarget)?24:16;  // BRAS + DC<Framesize> + A/AG + L/LG
      }

   size += 12;                     // BRASL + BRCL
   size += (is64BitTarget)?4:2;       // SGR

   // Full Speed Debug.
   if (cg()->comp()->getOption(TR_FullSpeedDebug))
      {

      // 81909: For 64 bit: max is 3 GPRs ( STG - 6 bytes * 3 = 18 ) + 4 FPRs ( STD - 4 bytes * 4 = 16 ) = 34 total
      // For 31 bit: max is 3 GPRs ( ST - 4 bytes * 3 = 12 ) + 4 FPRs ( STE - 4 bytes * 4 = 16 ) = 28 total
      size += (is64BitTarget)?34:28;
      }

#if !defined(PUBLIC_BUILD)
   // RI Hooks
   //temporary only RIOFF to allow RI on zlinux
   size += getRuntimeInstrumentationOnOffInstructionLength(cg());
   //size += 2 * getRuntimeInstrumentationOnOffInstructionLength(cg());
#endif

   return size;
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390StackCheckFailureSnippet * snippet)
   {
   if (pOutFile == NULL)
      {
      return;
      }

   TR::Linkage   *linkage      = _cg->getLinkage();

   uint8_t * bufferPos = snippet->getSnippetLabel()->getCodeLocation();
   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, "Stack Check Failure/Overflow  Snippet",
      getName(snippet->getDestination()));

   bool is64BitTarget = TR::Compiler->target.is64Bit();
   bool requireRAStore = !linkage->getRaContextRestoreNeeded();
   bool requireRALoad  = requireRAStore;

   int32_t frameSize = snippet->getFrameSize();

   if (frameSize < MAXDISP)
      {
      // If we need to save RA onto the stack, generate it now to avoid AGI delays.
      if (requireRAStore)
         {
         if (is64BitTarget)
            {
            printPrefix(pOutFile, NULL, bufferPos, 6);
            trfprintf(pOutFile, "STG \tGPR14,%d(,GPR5)",frameSize);
            bufferPos += 6;
            }
         else
            {
            printPrefix(pOutFile, NULL, bufferPos, 4);
            trfprintf(pOutFile, "ST   \tGPR14,%d(,GPR5)",frameSize);
            bufferPos += 4;
            }
         requireRAStore = false;
         }


      if (is64BitTarget)
         {
         printPrefix(pOutFile, NULL, bufferPos, 4);
         trfprintf(pOutFile, "LGHI \tGPR0,%d", frameSize);
         bufferPos += 4;

         printPrefix(pOutFile, NULL, bufferPos, 4);
         trfprintf(pOutFile, "AGHI \tGPR5,%d", frameSize);
         bufferPos += 4;
         }
      else
         {
         printPrefix(pOutFile, NULL, bufferPos, 4);
         trfprintf(pOutFile, "LHI \tGPR0,%d", frameSize);
         bufferPos += 4;

         printPrefix(pOutFile, NULL, bufferPos, 4);
         trfprintf(pOutFile, "AHI \tGPR5,%d", frameSize);
         bufferPos += 4;
         }
      }
   else if (frameSize < MAX_IMMEDIATE_VAL)
      {
      if (is64BitTarget)
         {
         printPrefix(pOutFile, NULL, bufferPos, 4);
         trfprintf(pOutFile, "AGHI \tGPR5,%d", frameSize);
         bufferPos += 4;

         printPrefix(pOutFile, NULL, bufferPos, 4);
         trfprintf(pOutFile, "LGHI \tGPR0,%d", frameSize);
         bufferPos += 4;
         }
      else
         {
         printPrefix(pOutFile, NULL, bufferPos, 4);
         trfprintf(pOutFile, "AHI \tGPR5,%d", frameSize);
         bufferPos += 4;

         printPrefix(pOutFile, NULL, bufferPos, 4);
         trfprintf(pOutFile, "LHI \tGPR0,%d", frameSize);
         bufferPos += 4;
         }
      }
   else
      {
      trfprintf(pOutFile, "...Large stack detected...");

      printPrefix(pOutFile, NULL, bufferPos, 4);
      trfprintf(pOutFile, "BRAS \tGPR_EP, *+%d <%p>", 4 + sizeof(intptrj_t), bufferPos + 4 + sizeof(intptrj_t));
      bufferPos += 4;

      printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
      trfprintf(pOutFile, "DC \t0x%p \t\t# Frame Size", snippet->getFrameSize());
      bufferPos += sizeof(intptrj_t);

      if (is64BitTarget)
         {
         printPrefix(pOutFile, NULL, bufferPos, 6);
         trfprintf(pOutFile, "AG   \tGPR5, 0(,GPR_EP)");
         bufferPos += 6;

         printPrefix(pOutFile, NULL, bufferPos, 6);
         trfprintf(pOutFile, "LG   \tGPR0, 0(,GPR_EP)");
         bufferPos += 6;
         }
      else
         {
         printPrefix(pOutFile, NULL, bufferPos, 4);
         trfprintf(pOutFile, "A   \tGPR5, 0(,GPR_EP)");
         bufferPos += 4;

         printPrefix(pOutFile, NULL, bufferPos, 4);
         trfprintf(pOutFile, "L   \tGPR0, 0(,GPR_EP)");
         bufferPos += 4;
         }
      }

   if (_comp->getOption(TR_FullSpeedDebug))
      {
      // TODO: need to print out the actual instructions
      printPrefix(pOutFile, NULL, bufferPos, snippet->getSizeOfArguments());
      trfprintf(pOutFile, "Store Arguments on Stack for FSD: %d",snippet->getSizeOfArguments());
      bufferPos += snippet->getSizeOfArguments();
      }

   if (requireRAStore)
      {
      if (is64BitTarget)
         {
         printPrefix(pOutFile, NULL, bufferPos, 6);
         trfprintf(pOutFile, "STG \tGPR14, 0(,GPR5)");
         bufferPos += 6;
         }
      else
         {
         printPrefix(pOutFile, NULL, bufferPos, 4);
         trfprintf(pOutFile, "ST    \tGPR14, 0(,GPR5)");
         bufferPos += 4;
         }
      }

   bufferPos = printRuntimeInstrumentationOnOffInstruction(pOutFile, bufferPos, false); // RIOFF

   printPrefix(pOutFile, NULL, bufferPos, 6);
   trfprintf(pOutFile, "BRASL \tGPR14, <%p>\t# Branch to Helper Method %s",
                       snippet->getSnippetDestAddr(),
                       snippet->usedTrampoline()?"- Trampoline Used.":"");
   bufferPos += 6;

   bufferPos = printRuntimeInstrumentationOnOffInstruction(pOutFile, bufferPos, true); // RION

   if (requireRALoad)
      {
      if (is64BitTarget)
         {
         printPrefix(pOutFile, NULL, bufferPos, 6);
         trfprintf(pOutFile, "LG    \tGPR14, 0(,GPR5)");
         bufferPos += 6;
         }
      else
         {
         printPrefix(pOutFile, NULL, bufferPos, 4);
         trfprintf(pOutFile, "L     \tGPR14, 0(,GPR5)");
         bufferPos += 4;
         }
      }

   if (is64BitTarget)
      {
      printPrefix(pOutFile, NULL, bufferPos, 4);
      trfprintf(pOutFile, "SGR \tGPR5, GPR0");
      bufferPos += 4;
      }
   else
      {
      printPrefix(pOutFile, NULL, bufferPos, 2);
      trfprintf(pOutFile, "SR  \tGPR5, GPR0");
      bufferPos += 2;
      }

   printPrefix(pOutFile, NULL, bufferPos, 6);
   trfprintf(pOutFile, "BRCL \t<%p>\t# Return to Main Code",
                    snippet->getReStartLabel()->getCodeLocation());
   }

