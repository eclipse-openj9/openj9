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

#if !defined(J9WINCE)
#ifdef WINDOWS
#include <windows.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#endif

#include "j9.h"
#include "codegen/FrontEnd.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "runtime/MethodMetaData.h"
#include "env/VMJ9.h"

// To transfer control to VM during OSR
extern "C" {
void _prepareForOSR(uintptrj_t vmThreadArg, int32_t currentInlinedSiteIndex, int32_t slotData)
   {
   const bool details = TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseOSRDetails);
   const bool trace   = TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseOSR) || details;

   J9VMThread *vmThread = (J9VMThread *)vmThreadArg;
   int numSymsThatShareSlot = (slotData >> 16) & 0xFFF;
   int totalNumSlots = slotData & 0xFFFF;
   J9JITConfig *jitConfig = vmThread->javaVM->jitConfig;
   J9JITExceptionTable *metaData = jitConfig->jitGetExceptionTableFromPC(vmThread, (UDATA) vmThread->osrBuffer->jitPC);
   UDATA jitPCOffset = (UDATA) vmThread->osrBuffer->jitPC - metaData->startPC;
   J9OSRFrame *osrFrame = (J9OSRFrame*)((uint8_t*)vmThread->osrBuffer + vmThread->osrFrameIndex);

   if ((trace && (numSymsThatShareSlot > 0)) || details)
      {
      UDATA bytecodePCOffset = osrFrame->bytecodePCOffset;
      TR_VerboseLog::writeLineLocked(TR_Vlog_OSR,
         "%x prepareForOSR at %p (startPC %p +%d) at %d:%x numSharingSyms:%d totalSlots:%d vmThread=%p", (int)vmThreadArg,
         metaData->startPC+jitPCOffset, metaData->startPC, (int)jitPCOffset,
         currentInlinedSiteIndex, (int)bytecodePCOffset, numSymsThatShareSlot, totalNumSlots, vmThread);

      TR_VerboseLog::vlogAcquire();
      TR_VerboseLog::writeLine(TR_Vlog_OSRD, "%X   Jitted body:    %.*s.%.*s%.*s", (int)vmThreadArg,
         J9UTF8_LENGTH(metaData->className),       J9UTF8_DATA(metaData->className),
         J9UTF8_LENGTH(metaData->methodName),      J9UTF8_DATA(metaData->methodName),
         J9UTF8_LENGTH(metaData->methodSignature), J9UTF8_DATA(metaData->methodSignature));

      if (details)
         {
         TR_J9VMBase * fe = (TR_J9VMBase *)jitConfig->compilationInfo;
         char inlinedMethodSignature[1000];
         TR_OpaqueMethodBlock *inlinedMethod = NULL;
         if (currentInlinedSiteIndex != -1)
            {
            inlinedMethod = (TR_OpaqueMethodBlock*)getInlinedMethod(getInlinedCallSiteArrayElement(metaData, currentInlinedSiteIndex));
            fe->printTruncatedSignature(inlinedMethodSignature, sizeof(inlinedMethodSignature), inlinedMethod);
            }

         if (inlinedMethod)
            TR_VerboseLog::writeLine(TR_Vlog_OSRD, "%X   Inlined method: %s", (int)vmThreadArg, inlinedMethodSignature);
         TR_VerboseLog::writeLine(TR_Vlog_OSRD, "%X   osrBuffer=%p osrFrame=%p, osrReturnAddress=%p osrScratchBuffer=%p osrJittedFrameCopy=%p", (int)vmThreadArg,
            vmThread->osrBuffer, osrFrame, vmThread->osrReturnAddress, vmThread->osrScratchBuffer, vmThread->osrJittedFrameCopy);
         TR_VerboseLog::writeLine(TR_Vlog_OSRD, "%X     OSRBuffer: numberOfFrames=%d jitPC=%p", (int)vmThreadArg,
            vmThread->osrBuffer->numberOfFrames, vmThread->osrBuffer->jitPC);
         TR_VerboseLog::writeLine(TR_Vlog_OSRD, "%X     OSRFrame: j9method=%p bytecodePC=%x numberOfLocals=%d maxStack=%d pendingStackHeight=%d monitorEnterRecords=%p", (int)vmThreadArg,
            osrFrame->method, osrFrame->bytecodePCOffset + osrFrame->method->bytecodes, (int)osrFrame->numberOfLocals, (int)osrFrame->maxStack, (int)osrFrame->pendingStackHeight, osrFrame->monitorEnterRecords);
         void **arg0EA = (void**)(osrFrame+1) + osrFrame->numberOfLocals + osrFrame->maxStack - 1;
         int32_t i;
         for (i = osrFrame->pendingStackHeight-1; i >= 0; --i)
            TR_VerboseLog::writeLine(TR_Vlog_OSRD, "%X       stack %2d: %p", (int)vmThreadArg, i, arg0EA[-i - osrFrame->numberOfLocals]);
         for (i = osrFrame->numberOfLocals-1; i >= 0; --i)
            TR_VerboseLog::writeLine(TR_Vlog_OSRD, "%X       local %2d: %p", (int)vmThreadArg, i, arg0EA[-i]);
         }

      TR_VerboseLog::vlogRelease();
      }


   if (numSymsThatShareSlot > 0) // this is a condition that depends on the current inlined method
      {
      int32_t* osrMetaData = (int32_t*) metaData->osrInfo;
      osrMetaData += 2; //skip size of section 0 and maxScratchBufferSize
      int32_t numberOfMappings = *osrMetaData; osrMetaData++;
      if (details)
         TR_VerboseLog::writeLineLocked(TR_Vlog_OSRD, "%X   %d mappings", (int)vmThreadArg, numberOfMappings);

      //if there are any symbols sharing slots at this OSR point
      //SD: With a high probability, this condition is true when the above condition (numSymsThtaShareSlot > 0)
      // is true. So it might be safe to remove the following if statement.
      if (numberOfMappings > 0) // this is a condition that depends on the whole compilation
         {
         //find the last instructionPC that is smaller than or equal to jitPC (i.e., the first instructionPC
         //that is larger than jitPC - 1)
         int32_t* previousMapping = NULL;
         int32_t i;
         //jitPC larger than 2^32 - 1 is not supported
         TR_ASSERT(sizeof(UDATA) == 4 || jitPCOffset <= INT_MAX, "jitPC doesn't fit in int32\n");
         for (i = 0; i < numberOfMappings; i++)
            {
            int32_t instructionPC = *osrMetaData; osrMetaData++;
            if (jitPCOffset < instructionPC)
               {
               if (details)
                  TR_VerboseLog::writeLineLocked(TR_Vlog_OSRD, "%X   Found mapping @%d > %d", (int)vmThreadArg, instructionPC, jitPCOffset);
               break;
               }
            else
               {
               previousMapping = osrMetaData;
               //skipping through the info of all symbols to get to beginning of the next mapping
               int32_t numSymbols = *osrMetaData; osrMetaData++;
               osrMetaData += numSymbols*4;
               if (details)
                  TR_VerboseLog::writeLineLocked(TR_Vlog_OSRD, "%X     Skip mapping @%d <= %d with %d symbols", (int)vmThreadArg, instructionPC, jitPCOffset, numSymbols);
               }
            }
         TR_ASSERT(i > 0, "jitPC %x is smaller than first instructionPC, startPC=%p\n",
                 jitPCOffset, metaData->startPC);

         osrMetaData = previousMapping;
         int32_t numSymbols = *osrMetaData; osrMetaData++;
         int32_t numSymbolsForThisCallerIndex = 0;

         if (details)
            TR_VerboseLog::writeLineLocked(TR_Vlog_OSRD, "%X   Copying %d symbols", (int)vmThreadArg, numSymbols);
         for (i = 0; i < numSymbols; i++)
            {
            int32_t inlinedSiteIndex = *osrMetaData; osrMetaData++;
            int32_t osrFrameDataOffset = *osrMetaData; osrMetaData++;
            int32_t scratchBufferOffset = *osrMetaData; osrMetaData++;
            int32_t symSize = *osrMetaData; osrMetaData++;

            if (inlinedSiteIndex != currentInlinedSiteIndex) continue;


            numSymbolsForThisCallerIndex++;
            if (scratchBufferOffset != -1)
               {
               uint8_t* dataAtScrBuffer = (uint8_t*)vmThread->osrScratchBuffer + scratchBufferOffset;
               if (details)
                  {
                  TR_VerboseLog::vlogAcquire();
                  TR_VerboseLog::writeLine(TR_Vlog_OSRD, "%X     Symbol #%d osrFrameDataOffset=%d scratchBufferOffset=%d size=%d data:", (int)vmThreadArg,
                     i, osrFrameDataOffset, scratchBufferOffset, symSize);
                  switch (symSize)
                     {
                     case 4:
                        TR_VerboseLog::write("0x%08x", *(uint32_t*)(dataAtScrBuffer));
                        break;
                     case 8:
                        TR_VerboseLog::write(UINT64_PRINTF_FORMAT_HEX, *(uint64_t*)(dataAtScrBuffer));
                        break;
                     default:
                        // TODO: Dump binary
                        break;
                     }
                  TR_VerboseLog::vlogRelease();
                  }
               memcpy(
                  (uint8_t*)(osrFrame) + osrFrameDataOffset,
                  dataAtScrBuffer,
                  symSize);
               }
            else
               {
               if (details)
                  {
                  TR_VerboseLog::writeLineLocked(TR_Vlog_OSRD, "%X     Symbol #%d osrFrameDataOffset=%d size=%d data:Zeros", (int)vmThreadArg,
                     i, osrFrameDataOffset, symSize);
                  }
               memset(
                  (uint8_t*)(osrFrame) + osrFrameDataOffset,
                  0, symSize);
               }
            }
         TR_ASSERT(numSymbolsForThisCallerIndex <= numSymsThatShareSlot,
                 "the number of symbols we are going to write (%d) is more than the number of symbols that share slots (%d)\n",
                 numSymbolsForThisCallerIndex, numSymsThatShareSlot);
         }
      }
   else
      {
      if (details)
         TR_VerboseLog::writeLineLocked(TR_Vlog_OSRD, "%X     No slot-sharing symbols", (int)vmThreadArg);
      }
   if (0)  // TODO
      {
      uint8_t* osrBufferPtr = (uint8_t*)vmThread->osrBuffer + vmThread->osrFrameIndex;
      printf("contents of OSR buffer (%p):\n", osrBufferPtr);
      int numIntsToPrint =  (totalNumSlots * sizeof(UDATA) + sizeof(J9OSRFrame)) / 4 ;
      for (int i = 0; i < numIntsToPrint; i++)
         {
         printf("%.8x ", *(uint32_t*)(osrBufferPtr + 4*i));
         if ((i % 4) == 3 || (i == numIntsToPrint -1 ))
            printf("\n");
         }
      printf("\n");
      fflush(stdout);
      }
   if (details)
      TR_VerboseLog::writeLineLocked(TR_Vlog_OSRD, "%X   prepareForOSR returning", (int)vmThreadArg);
   }
}





