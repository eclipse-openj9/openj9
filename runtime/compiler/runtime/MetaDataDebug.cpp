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

#include "j9.h"
#include "codegen/CodeGenerator.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/Instruction.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "env/CompilerEnv.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "ras/InternalFunctions.hpp"
#include "ras/Logger.hpp"
#include "runtime/MethodMetaData.h"
#include "env/VMJ9.h"
#include "env/CompilerEnv.hpp"
#include "env/j9method.h"

#if defined(TR_HOST_POWER)
#define INTERNAL_PTR_REG_MASK 0x00040000
#else
#define INTERNAL_PTR_REG_MASK 0x80000000
#endif

void
TR_Debug::printJ9JITExceptionTableDetails(
      OMR::Logger *log,
      J9JITExceptionTable *data,
      J9JITExceptionTable *dbgextRemotePtr)
   {
   uintptr_t startPC = (uintptr_t)data->startPC;
   log->printf("J9JITExceptionTable [%p]\n", data);
   log->printf("CP=[%p], slots=[%p], NumExcpRanges=[%p], size=[%p]\n",
               data->constantPool, data->slots, data->numExcptionRanges, data->size);
   log->printf("startPC=     [%p]\n", data->startPC);
   log->printf("endWarmPC=   [%p]\n",data->endWarmPC);
   log->printf("startColdPC= [%p]\n",data->startColdPC);
   log->printf("endPC=       [%p]\n",data->endPC);
   log->printf("hotness=     [%d]\n",data->hotness);
   log->printf("scalarTempSlots=%d, objectTempSlots=%d\n", data->scalarTempSlots, data->objectTempSlots);
   log->printf("prologuePushes=%d, tempOffset=%d\n", data->prologuePushes, data->tempOffset);
   log->printf("registerSaveDescription=[%p]\n", data->registerSaveDescription);
   log->printf("totalFrameSize=%d { Real Frame Size: %d }\n", data->totalFrameSize, (data->totalFrameSize + 1) * TR::Compiler->om.sizeofReferenceAddress());
   log->printf("bodyInfo= [%p]\n", data->bodyInfo);
   }

void
TR_Debug::print(
      OMR::Logger *log,
      J9JITExceptionTable *data,
      TR_ResolvedMethod *feMethod,
      bool fourByteOffsets)
   {
   uintptr_t startPC = (uintptr_t)data->startPC;

   printJ9JITExceptionTableDetails(log, data);

   TR::GCStackAtlas *sa = _comp->cg()->getStackAtlas();
   J9JITStackAtlas *j9StackAtlas = (J9JITStackAtlas *)sa->getAtlasBits();

   int32_t sizeOfStackAtlas = 0;
   int32_t *offsetInfo = 0;
   if (sa)
      offsetInfo = printStackAtlas(log, startPC, sa->getAtlasBits(), sa->getNumberOfSlotsMapped(), fourByteOffsets, &sizeOfStackAtlas, data->totalFrameSize);

   TR_ASSERT( sizeOfStackAtlas, "size of stack atlas cannot be 0\n");

   // print exception table entries
   //
   int32_t i, numExceptionRanges = data->numExcptionRanges & 0x3FFF;
   bool fourByteExceptionRanges = (data->numExcptionRanges & 0x8000) != 0;

   if (numExceptionRanges)
      log->printf("\n<exceptionTable offsetBytes=\"%d\">\n", fourByteExceptionRanges ? 4 : 2);

   char * cursor = (char *)data + sizeof(J9JITExceptionTable);
   for (i = 0; i < numExceptionRanges; ++i)
      {
      if (fourByteExceptionRanges)
         {
         // { RTSJ Support begins
         log->printf("<range start=\"%08x\" ",   *(uint32_t *)cursor), cursor += 4;
         // } RTSJ Support ends
         log->printf("end=\"%08x\" ",     *(uint32_t *)cursor), cursor += 4;
         log->printf("handler=\"%08x\" ", *(uint32_t *)cursor), cursor += 4;
         log->printf("catchType=\"%08x\" ", *(uint32_t *)cursor), cursor += 4;

         J9Method *method = *(J9Method **)cursor;
         if (_comp->fej9()->isAOT_DEPRECATED_DO_NOT_USE())
            {
            uintptr_t callerIndex = *(uintptr_t *)cursor;
            log->printf("caller index=\"%08x\" ", callerIndex);
            TR_InlinedCallSite * inlinedCallSite = ((TR_InlinedCallSite *)data->inlinedCalls) + callerIndex;
            method = (J9Method *) inlinedCallSite->_methodInfo;
            }

         if (_comp->target().is64Bit())
            {
            log->printf("method=\"%016llx\" ", method);
            cursor += 8;
            }
         else
            {
            log->printf("method=\"%08x\" ", method);
            cursor += 4;
            }
         }
      else
         {
         // { RTSJ Support begins
         log->printf("<range start=\"%04x\" ",   *(uint16_t *)cursor), cursor += 2;
         // } RTSJ Support ends
         log->printf("end=\"%04x\" ",     *(uint16_t *)cursor), cursor += 2;
         log->printf("handler=\"%04x\" ", *(uint16_t *)cursor), cursor += 2;
         log->printf("catchType=\"%04x\"", *(uint16_t *)cursor), cursor += 2;
         }

      if (_comp->getOption(TR_FullSpeedDebug))
         {
         log->printf(" byteCodeIndex=\"%08x\"", *(uint32_t *)cursor);
         cursor += 4;
         }

      log->prints("/>\n");
      }

   if (numExceptionRanges)
      log->prints("</exceptionTable>\n");

   if (sa->getNumberOfSlotsMapped())
      {
      log->prints("\n\nMethod liveMonitor mask: ");
      uint8_t * maskBits = (uint8_t *)data->gcStackAtlas + sizeof(J9JITStackAtlas);
      printStackMapInfo(log, maskBits, sa->getNumberOfSlotsMapped(), 0, offsetInfo);
      log->prints("\n\n");
      }

   // print inlined call site information
   //

   // calculate number of inlined call sites.
   int32_t sizeOfInlinedCallSites, numInlinedCallSites;
   if (fourByteExceptionRanges)
      {
      if (_comp->getOption(TR_FullSpeedDebug))
         {
         sizeOfInlinedCallSites = data->size - ( sizeof(J9JITExceptionTable) + numExceptionRanges * 24);
         }
      else
         {
         sizeOfInlinedCallSites = data->size - ( sizeof(J9JITExceptionTable) + numExceptionRanges * 20);
         }
      }
   else
      {
      if (_comp->getOption(TR_FullSpeedDebug))
         {
         sizeOfInlinedCallSites = data->size - ( sizeof(J9JITExceptionTable) + numExceptionRanges * 12);
         }
      else
         {
         sizeOfInlinedCallSites = data->size - ( sizeof(J9JITExceptionTable) + numExceptionRanges * 8);
         }
      }

   sizeOfInlinedCallSites -= j9StackAtlas->numberOfMapBytes;  // space for the outerscope live monitor mask

   if (_usesSingleAllocMetaData)
      sizeOfInlinedCallSites -= sizeOfStackAtlas;

   numInlinedCallSites = sizeOfInlinedCallSites / (sizeof(TR_InlinedCallSite) + j9StackAtlas->numberOfMapBytes);
   uint8_t * callSiteCursor = (uint8_t *)data->inlinedCalls;
   if (numInlinedCallSites && callSiteCursor)
      {
      log->prints("\nInlined call site array:\n");
      for (i = 0; i < numInlinedCallSites; i++)
         {
         TR_InlinedCallSite * inlinedCallSite = (TR_InlinedCallSite *)callSiteCursor;

         log->printf("\nOwning method: %p\n", inlinedCallSite->_methodInfo);
         log->printf("ByteCodeInfo: <_callerIndex=%d, byteCodeIndex=%d>, _isSameReceiver=%d, _doNotProfile=%d\n",
              inlinedCallSite->_byteCodeInfo.getCallerIndex(), inlinedCallSite->_byteCodeInfo.getByteCodeIndex(),
              inlinedCallSite->_byteCodeInfo.isSameReceiver(), inlinedCallSite->_byteCodeInfo.doNotProfile() );

         callSiteCursor += sizeof(TR_InlinedCallSite);

         if (inlinedCallSite->_byteCodeInfo.isSameReceiver())
            {
            log->prints("liveMonitor mask: ");
            uint8_t * maskBits = callSiteCursor;
            printStackMapInfo(log, maskBits, sa->getNumberOfSlotsMapped(), 0, offsetInfo);
            log->prints("\n");
            }

         callSiteCursor += j9StackAtlas->numberOfMapBytes;
         }
      }

   log->prints("\n\n");
   log->flush();
   }

int32_t *
TR_Debug::printStackAtlas(
      OMR::Logger *log,
      uintptr_t startPC,
      uint8_t * mapBits,
      int32_t numberOfSlotsMapped,
      bool fourByteOffsets,
      int32_t *sizeOfStackAtlas,
      int32_t frameSize)
   {
   J9JITStackAtlas *stackAtlas = (J9JITStackAtlas *) mapBits;
   int32_t *offsetInfo = (int32_t *) _comp->trMemory()->allocateHeapMemory(sizeof(int32_t)*numberOfSlotsMapped);
   memset(offsetInfo, 0, sizeof(int32_t)*numberOfSlotsMapped);

   uint16_t indexOfFirstInternalPtr = printStackAtlasDetails(log, startPC, mapBits, numberOfSlotsMapped, fourByteOffsets, sizeOfStackAtlas, frameSize, offsetInfo);
   mapBits += sizeof(J9JITStackAtlas) + stackAtlas->numberOfMapBytes;

   //Map Info
   for (U_32 j = 0; j < stackAtlas->numberOfMaps; j++)
      {
      log->printf("    stackmap location: %p\n", mapBits);
      TR_ByteCodeInfo *byteCodeInfo = NULL;
      mapBits = printMapInfo(log, startPC, mapBits, numberOfSlotsMapped, fourByteOffsets, sizeOfStackAtlas, byteCodeInfo, indexOfFirstInternalPtr, offsetInfo);
      }

   return offsetInfo;
   }

uint16_t
TR_Debug::printStackAtlasDetails(
      OMR::Logger *log,
      uintptr_t startPC,
      uint8_t *mapBits,
      int32_t numberOfSlotsMapped,
      bool fourByteOffsets,
      int32_t *sizeOfStackAtlas,
      int32_t frameSize,
      int32_t *offsetInfo)
   {
   J9JITStackAtlas *stackAtlas = (J9JITStackAtlas *) mapBits;

   log->prints("\nStack Atlas:\n");
   log->printf("  numberOfSlotsMapped=%d\n", numberOfSlotsMapped);
   log->printf("  numberOfMaps=%d\n", stackAtlas->numberOfMaps);
   log->printf("  numberOfMapBytes=%d\n", stackAtlas->numberOfMapBytes);
   log->printf("  parmBaseOffset=%d\n", stackAtlas->parmBaseOffset);
   log->printf("  numberOfParmSlots=%d\n", stackAtlas->numberOfParmSlots);
   log->printf("  localBaseOffset=%d\n", stackAtlas->localBaseOffset);
   log->printf("  syncObjectTempOffset=%d\n", (int16_t)stackAtlas->paddingTo32);

   mapBits += sizeof(J9JITStackAtlas);
   *sizeOfStackAtlas = sizeof(J9JITStackAtlas);

   uint16_t indexOfFirstInternalPtr = 0;
   if (stackAtlas->internalPointerMap)
      {
      log->prints("      variable length internal pointer stack map portion exists\n");
      uint8_t *internalPtrMapCursor = (uint8_t *) stackAtlas->internalPointerMap;
      internalPtrMapCursor += sizeof(intptr_t);

      uint8_t variableLengthSize = *((uint8_t *)internalPtrMapCursor);
      log->printf("        size of internal pointer stack map = %d\n", variableLengthSize);
      internalPtrMapCursor += 1;
      if (_comp->isAlignStackMaps())
         internalPtrMapCursor += 1;

      indexOfFirstInternalPtr = *((uint16_t *)internalPtrMapCursor);
      log->printf("        index of first internal pointer = %d\n", indexOfFirstInternalPtr);
      internalPtrMapCursor += 2;

      uint16_t offsetOfFirstInternalPtr = *((uint16_t *)internalPtrMapCursor);
      log->printf("        offset of first internal pointer = %d\n", offsetOfFirstInternalPtr);
      internalPtrMapCursor += 2;

      uint8_t numPinningArrays = *((uint8_t *)internalPtrMapCursor);
      log->printf("        number of distinct pinning arrays = %d\n", numPinningArrays);
      internalPtrMapCursor++;

      uint8_t i = 0;
      while (i < numPinningArrays)
         {
         log->printf("          pinning array : %d\n", (indexOfFirstInternalPtr+(*internalPtrMapCursor)));
         internalPtrMapCursor++;

         uint8_t numInternalPointers = *internalPtrMapCursor;
         internalPtrMapCursor++;
         log->printf("          number of internal pointers in stack slots for this pinning array = %d\n", numInternalPointers);

         uint8_t j = 0;
         while (j < numInternalPointers)
            {
            log->printf("            internal pointer stack slot : %d\n", (indexOfFirstInternalPtr+(*internalPtrMapCursor)));
            internalPtrMapCursor++;
            j++;
            }

         i++;
         }

      *sizeOfStackAtlas = *sizeOfStackAtlas + 1 + variableLengthSize;
      }

   if (stackAtlas->stackAllocMap)
       {
       log->printf("\nStack alloc map location : %p ", stackAtlas->stackAllocMap);

       uint8_t *localStackAllocMap = (uint8_t *) dxMallocAndRead(sizeof(intptr_t), stackAtlas->stackAllocMap);

       log->printf("\n  GC map at stack overflow check : %p", localStackAllocMap);
       log->prints("\n  Stack alloc map bits : ");

       uint8_t *mapBits = (uint8_t *) ((uintptr_t) localStackAllocMap + sizeof(uintptr_t));
       printStackMapInfo(log, mapBits, numberOfSlotsMapped, sizeOfStackAtlas, NULL);

       log->prints("\n");
       }

   //Offset info
   int32_t size = TR::Compiler->om.sizeofReferenceAddress();
   int32_t numParms = stackAtlas->numberOfParmSlots;
   int32_t offset;

   log->prints("\nOffset info: \n");
   for (int32_t i = 0; i < numParms; i++)
      {
      offset = frameSize * size + stackAtlas->parmBaseOffset + i * size;
      offsetInfo[i] = offset;
      log->printf("Parm: \tGC Map Index: %i,\tOffset: %i (0x%x)\n", i, offset, offset);
      }

   for (int32_t j = numParms; j < numberOfSlotsMapped; j++)
      {
      offset = frameSize * size + stackAtlas->localBaseOffset + (j - numParms) * size;
      offsetInfo[j] = offset;
      log->printf("Local: \tGC Map Index: %i,\tOffset: %i (0x%x)\n", j, offset, offset);
      }

   return indexOfFirstInternalPtr;
   }

uint8_t *
TR_Debug::printMapInfo(
      OMR::Logger *log,
      uintptr_t startPC,
      uint8_t * mapBits,
      int32_t numberOfSlotsMapped,
      bool fourByteOffsets,
      int32_t *sizeOfStackAtlas,
      TR_ByteCodeInfo *byteCodeInfo,
      uint16_t indexOfFirstInternalPtr,
      int32_t offsetInfo[],
      bool nummaps)
   {
   uint32_t lowOffset, registerMap;

   if (fourByteOffsets)
      {
      lowOffset = *((U_32 *)mapBits);
      mapBits += sizeof(U_32);
      if (byteCodeInfo == NULL)
         {
         byteCodeInfo = (TR_ByteCodeInfo *)mapBits;
         }
      mapBits += sizeof(U_32);
      *sizeOfStackAtlas = *sizeOfStackAtlas + 2 * sizeof(U_32);
      }
   else
      {
      lowOffset = *((U_16 *)mapBits);
      if (_comp->isAlignStackMaps())
         {
         mapBits += sizeof(U_32);
         *sizeOfStackAtlas = *sizeOfStackAtlas + sizeof(U_32);
         }
      else
         {
         mapBits += sizeof(U_16);
         *sizeOfStackAtlas = *sizeOfStackAtlas + sizeof(U_16);
         }

      if (byteCodeInfo == NULL)
         {
         byteCodeInfo = (TR_ByteCodeInfo *)mapBits;
         }

      mapBits += sizeof(U_32);
      *sizeOfStackAtlas = *sizeOfStackAtlas + sizeof(U_32);
      }

   if (!nummaps)
      {
      log->printf("    map range: starting at [%p]\n", (startPC + lowOffset));
      log->printf("      lowOffset: %08X\n", lowOffset);
      log->printf("      byteCodeInfo: <_callerIndex=%d, byteCodeIndex=%d>, _isSameReceiver=%d, _doNotProfile=%d\n",
         byteCodeInfo->getCallerIndex(), byteCodeInfo->getByteCodeIndex(), byteCodeInfo->isSameReceiver(), byteCodeInfo->doNotProfile());
      }

   if (byteCodeInfo->doNotProfile())
      {
      log->prints("      ByteCodeInfo Map\n");
      }
   else
      {
      if (!nummaps)
         {
         log->printf("      registerSaveDescription: starting at [%08X] { %08X }\n", mapBits, *((U_32 *)mapBits));
         }
      mapBits += sizeof(U_32);
      *sizeOfStackAtlas = *sizeOfStackAtlas + sizeof(U_32);

      registerMap = *((U_32 *)mapBits);
      mapBits += sizeof(U_32);
      *sizeOfStackAtlas = *sizeOfStackAtlas + sizeof(U_32);

      if (!nummaps)
         {
         log->printf("      registers: %08X\t{ ", registerMap);
         }
      uint32_t descriptionByte = registerMap;
      for (int32_t l = 0; l < sizeof(descriptionByte)*8; l++)
         {
         if (descriptionByte & 1)
            {
            const char *regName = getRealRegisterName(l);

            if (!nummaps)
               log->printf("%i:%s ", l, regName);
            }
         descriptionByte = descriptionByte >> 1;
         }

      if (!nummaps)
         {
         log->prints("}\n");
         }

      if ((*(uint32_t *)byteCodeInfo ==0x00000000) && (registerMap ==0xFADECAFE) && !nummaps)
         log->prints("      This is a dummy map\n");
      else
         {
         if (registerMap & INTERNAL_PTR_REG_MASK)
            {
            uint8_t *internalPtrMapCursor = mapBits;
            if (!nummaps)
               {
               log->prints("      variable length internal pointer register map portion exists\n");
               log->printf("        size of internal pointer register map = %d\n", *internalPtrMapCursor);
               }

            uint8_t variableLengthSize = *internalPtrMapCursor;
            internalPtrMapCursor++;
            if (!nummaps)
               log->printf("        number of pinning arrays for internal pointers in regs now = %d\n", *internalPtrMapCursor);

            uint8_t numPinningArrays = *internalPtrMapCursor;
            internalPtrMapCursor++;
            uint8_t i = 0;
            while (i < numPinningArrays)
               {
               if (!nummaps)
                  log->printf("          pinning array : %d\n", (indexOfFirstInternalPtr+(*internalPtrMapCursor)));
               internalPtrMapCursor++;

               uint8_t numInternalPointers = *internalPtrMapCursor;
               internalPtrMapCursor++;
               if (!nummaps)
                  log->printf("          number of internal pointers in registers for this pinning array = %d\n", numInternalPointers);

               uint8_t j = 0;
               while (j < numInternalPointers)
                  {
                  if (!nummaps)
                     log->printf("            internal pointer register number : %d\n", *internalPtrMapCursor);
                  internalPtrMapCursor++;
                  j++;
                  }
               i++;
               }

            mapBits += (1+variableLengthSize);
            *sizeOfStackAtlas = *sizeOfStackAtlas + 1 + variableLengthSize;
            }

         if (!nummaps)
            log->prints("      stack map: ");

         printStackMapInfo(log, mapBits, numberOfSlotsMapped, sizeOfStackAtlas, offsetInfo, nummaps);

         if (!nummaps)
            log->prints("\n");

         // check is there's live monitor meta data and if so, bump the pointers.
         // todo: print out the live monitor meta data
         //
         if (*(mapBits - 1) & 128)
            {
            if (!nummaps)
               log->prints("liveMonitor map: ");

            printStackMapInfo(log, mapBits, numberOfSlotsMapped, sizeOfStackAtlas, offsetInfo, nummaps);

            if (!nummaps)
               log->prints("\n");
            }

         if (_comp->isAlignStackMaps())
            {
            mapBits += ((uintptr_t)mapBits & 3) ? (4 - ((uintptr_t)mapBits & 3)) : 0;
            *sizeOfStackAtlas = *sizeOfStackAtlas + ((uintptr_t)mapBits & 3) ? (4 - ((uintptr_t)mapBits & 3)) : 0;
            }
         }
      }

   if (!nummaps)
      log->prints("\n");

   return mapBits;
   }

void
TR_Debug::printStackMapInfo(
      OMR::Logger *log,
      uint8_t *&mapBits,
      int32_t numberOfSlotsMapped,
      int32_t *sizeOfStackAtlas,
      int32_t *offsetInfo,
      bool nummaps)
   {
   int32_t *collectedOffsets = (int32_t *) _comp->trMemory()->allocateHeapMemory(sizeof(int32_t)*numberOfSlotsMapped);
   memset(collectedOffsets, 0, sizeof(int32_t)*numberOfSlotsMapped);

   int32_t descriptionBytes = (numberOfSlotsMapped + 7 + 1) >> 3;
   int32_t bits = 0;

   for (int32_t k = 0; k < descriptionBytes; k++)
      {
      U_8 descriptionByte = *mapBits++;
      if (sizeOfStackAtlas)
         *sizeOfStackAtlas = *sizeOfStackAtlas + sizeof(U_8);

      for (int32_t l = 0; l < 8; l++)
         {
         if (bits < numberOfSlotsMapped)
            {
            if (!nummaps)
               log->printf("%d", (descriptionByte & 0x1 ? 1 : 0));

            if (descriptionByte & 0x1)
               collectedOffsets[bits] = 1;

            descriptionByte = descriptionByte >> 1;
            bits++;
            }
         }
      }

   if (offsetInfo != NULL)
      {
      if (!nummaps)
         log->prints("\t{ ");

      for (int32_t i = 0; i < numberOfSlotsMapped; i++)
         {
         if (collectedOffsets[i] && !nummaps)
            {
            log->printf("%d ", offsetInfo[i]);
            }
         }

      if (!nummaps)
         log->printc('}');
      }
   }

extern "C" void jitBytecodePrintFunction(void *userData, char *format, ...)
   {
   va_list args;
   char outputBuffer[512];

   va_start(args, format);
   vsnprintf(outputBuffer, sizeof(outputBuffer), format, args);
   va_end(args);

   OMR::Logger *log = reinterpret_cast<OMR::Logger *>(userData);
   log->prints(outputBuffer);
   }

void
TR_Debug::printByteCodeStack(OMR::Logger *log, int32_t parentStackIndex, uint16_t byteCodeIndex, size_t *indentLen)
   {
#if defined(J9VM_OPT_JITSERVER)
   if (_comp->isOutOfProcessCompilation() || _comp->isRemoteCompilation())
      return;
#endif

   if (!_comp->fej9()->isAOT_DEPRECATED_DO_NOT_USE())
      {
      J9Method * ramMethod;
      void *bcPrintFunc = (void *)jitBytecodePrintFunction;

      if (parentStackIndex == -1)
         {
         log->printf(" \\\\ %s\n", _comp->getCurrentMethod()->signature(comp()->trMemory(), heapAlloc));
         ramMethod = (J9Method *)_comp->getCurrentMethod()->resolvedMethodAddress();
         }
      else
         {
         TR_InlinedCallSite & site = _comp->getInlinedCallSite(parentStackIndex);
         printByteCodeStack(log, site._byteCodeInfo.getCallerIndex(), site._byteCodeInfo.getByteCodeIndex(), indentLen);
         ramMethod = (J9Method *)site._methodInfo;
         }

      #ifdef J9VM_ENV_LITTLE_ENDIAN
         uint32_t flags = BCT_LittleEndianOutput;
      #else
         uint32_t flags = BCT_BigEndianOutput;
      #endif

      log->prints(" \\\\");
      j9bcutil_dumpBytecodes(((TR_J9VMBase *)_comp->fej9())->_portLibrary,
                             J9_CLASS_FROM_METHOD(ramMethod)->romClass,
                             J9_BYTECODE_START_FROM_RAM_METHOD(ramMethod),
                             byteCodeIndex, byteCodeIndex, flags, bcPrintFunc, log, *indentLen);
      *indentLen += 3;
      }
   }
