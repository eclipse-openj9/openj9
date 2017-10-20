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

#include <algorithm>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include "jilconsts.h"
#include "jitprotos.h"
#include "j9cfg.h"
#include "j9comp.h"
#include "j9consts.h"
#include "j9list.h"
#include "j9port.h"
#include "cache.h"
#include "codegen/AheadOfTimeCompile.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/GCStackMap.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/PicHelpers.hpp"
#include "codegen/Snippet.hpp"
#include "compile/Compilation.hpp"
#include "compile/OSRData.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/jittypes.h"
#include "env/CompilerEnv.hpp"
#include "exceptions/DataCacheError.hpp"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "runtime/ArtifactManager.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/MethodMetaData.h"
#include "runtime/asmprotos.h"
#include "trj9/env/VMJ9.h"
#include "trj9/env/j9fieldsInfo.h"
#include "trj9/env/j9method.h"
#include "trj9/control/CompilationThread.hpp"
#include "trj9/control/CompilationRuntime.hpp"
#include "trj9/runtime/HWProfiler.hpp"

TR_MetaDataStats metaDataStats;

struct TR_StackAtlasStats
   {
#if defined(DEBUG)
   // HACK 1GAN1V9 :: OSE Runtime cannot link with destructors defined
   ~TR_StackAtlasStats();
#endif

   uint32_t _counter;
   uint32_t _maxSlots;
   uint32_t _totalSlots;
   } stackAtlasStats;

static uint8_t * allocateGCData(TR_J9VMBase * vm, uint32_t numBytes, TR::Compilation *comp)
   {
   uint8_t *gcData = NULL;
   uint32_t size = 0;
   bool shouldRetryAllocation;
   gcData = vm->allocateDataCacheRecord(numBytes, comp, vm->isAOT_DEPRECATED_DO_NOT_USE(), &shouldRetryAllocation,
                                        J9_JIT_DCE_STACK_ATLAS, &size);
   if (!gcData)
      {
      if (shouldRetryAllocation)
         {
         // force a retry
         comp->failCompilation<J9::RecoverableDataCacheError>("Failed to allocate GC Data");
         }
      comp->failCompilation<J9::DataCacheError>("Failed to allocate GC Data");
      }
   if (debug("metaDataStats"))
      metaDataStats._gcDataSize += size;

   return gcData;
   }

///////////////////////////////////////////////////////////////////////////
//  Meta Data Creation
///////////////////////////////////////////////////////////////////////////

#if defined(DEBUG)
   // HACK 1GAN1V9 :: OSE Runtime cannot link with destructors defined
TR_MetaDataStats::~TR_MetaDataStats()
   {
   if (!debug("metaDataStats"))
      return;

   uint32_t totalMetaData = _exceptionDataSize + _tableSize + _inlinedCallDataSize + _gcDataSize + _relocationSize;

   printf("\nMetaDataStats\n");
   printf("number of methods jitted:      %10d\n", _counter);
   printf("tot code size:                 %10d\n", _codeSize);
   printf("av. code size:                 %10d\n", _codeSize / _counter);
   printf("max code size:                 %10d\n", _maxCodeSize);
   printf("tot meta size:                 %10d\n", totalMetaData);
   printf("av. meta data size:            %10d\n", totalMetaData / _counter);
   printf("   av. exception data size:    %10d\n", _exceptionDataSize / _counter);
   printf("   av. table size:             %10d\n", _tableSize / _counter);
   printf("   av. inlined call data size: %10d\n", _inlinedCallDataSize / _counter);
   printf("   av. gc data size:           %10d\n", _gcDataSize / _counter);
   printf("   av. relocation size:        %10d\n", _relocationSize / _counter);
   fflush(stdout);
   }
#endif

#if defined(DEBUG)
TR_StackAtlasStats::~TR_StackAtlasStats()
   {
   if (!debug("stackAtlasStats"))
      return;

   printf("\nStack Atlas Stats\n");
   printf("number of methods jitted:      %d\n", _counter);
   printf("av. # slots * 10:              %d\n", (_totalSlots * 10) / _counter);
   printf("max # slots:                   %d\n", _maxSlots);
   fflush(stdout);
   }

#endif


static void
createByteCodeInfoRange(
      TR_GCStackMap *map,
      uint8_t *location,
      bool fourByteOffsets,
      TR::GCStackAtlas *trStackAtlas,
      TR::Compilation *comp)
   {
   uint32_t lowCode = map->getLowestCodeOffset();

   if (fourByteOffsets)
      {
      *(uint32_t *)location = lowCode;
      location += 4;
      }
   else
      {
      *(uint16_t *)location = lowCode;
      if (comp->isAlignStackMaps())
         location += 4;
      else
         location += 2;
      }

   TR_ByteCodeInfo byteCodeInfo = map->getByteCodeInfo();
   byteCodeInfo.setDoNotProfile(1); // indicate that this map entry does not contain a stack map
   if (map == trStackAtlas->getParameterMap())
      {
      byteCodeInfo.setInvalidCallerIndex(); // hacky fix to make sure parameter map caller index is correct
      }

   memcpy(location, &byteCodeInfo, sizeof(TR_ByteCodeInfo));
   location += sizeof(int32_t);
   }


static void
createExceptionTable(
      TR_MethodMetaData * data,
      TR_ExceptionTableEntryIterator & exceptionIterator,
      bool fourByteOffsets,
      TR::Compilation *comp)
   {
   uint8_t * cursor = (uint8_t *)data + sizeof(TR_MethodMetaData);

   for (TR_ExceptionTableEntry * e = exceptionIterator.getFirst(); e; e = exceptionIterator.getNext())
      {
      if (fourByteOffsets)
         {
         *(uint32_t *)cursor = e->_instructionStartPC, cursor += 4;
         *(uint32_t *)cursor = e->_instructionEndPC, cursor += 4;
         *(uint32_t *)cursor = e->_instructionHandlerPC, cursor += 4;
         *(uint32_t *)cursor = e->_catchType, cursor += 4;
         if (comp->fej9()->isAOT_DEPRECATED_DO_NOT_USE())
            *(uintptrj_t *)cursor = (uintptrj_t)e->_byteCodeInfo.getCallerIndex(), cursor += sizeof(uintptrj_t);
         else
            *(uintptrj_t *)cursor = (uintptrj_t)e->_method->resolvedMethodAddress(), cursor += sizeof(uintptrj_t);
         }
      else
         {
         TR_ASSERT(e->_catchType <= 0xFFFF, "the cp index for the catch type requires 17 bits!");

         *(uint16_t *)cursor = e->_instructionStartPC, cursor += 2;
         *(uint16_t *)cursor = e->_instructionEndPC, cursor += 2;
         *(uint16_t *)cursor = e->_instructionHandlerPC, cursor += 2;
         *(uint16_t *)cursor = e->_catchType, cursor += 2;
         }

      // Ensure that InstructionBoundaries are initialized properly.
      //
      TR_ASSERT(e->_instructionStartPC != UINT_MAX, "Uninitialized startPC in exception table entry: %p",  e);
      TR_ASSERT(e->_instructionEndPC != UINT_MAX, "Uninitialized endPC in exception table entry: %p",  e);

      if (comp->getOption(TR_FullSpeedDebug) && !debug("dontEmitFSDInfo"))
         {
         *(uint32_t *)cursor = e->_byteCodeInfo.getByteCodeIndex();
         cursor += 4;
         }
      }
   }


// This method is used to calculate the size (in number of bytes)
// that this internal pointer map will require in the J9 GC map
// format. This is based on the following GC map format desired by
// J9 for internal pointers :
//
// <Number of distinct base array temps> (say 2)
// <Index of base array temp1>
// <Number of internal pointers derived from base array temp1> (say 3)
// <Index of internal ptr1 derived from base array temp1>
// <Index of internal ptr2 derived from base array temp1>
// <Index of internal ptr3 derived from base array temp1>
// <Index of base array temp2>
// <Number of internal pointers derived from base array temp2> (say 1)
// <Index of internal ptr1 derived from base array temp2>
//
// Similar scheme is followed for register based internal pointer maps
// with index of internal pointer temp replaced by register number in that case.
//
static int32_t
calculateMapSize(
      TR_InternalPointerMap *map,
      TR::Compilation *comp)
   {
   if (!map)
      {
      return 0;
      }

   int32_t size = 1; // first byte holding number of distinct base array temps

   int32_t numDistinctPinningArrays = 0;
   int32_t numInternalPtrs = 0;
   //
   // The loop below finds the number of distinct pinning array temps
   // by iterating through the various internal pointer/base array temp pairs
   // created by the JIT.
   //
   List<TR_InternalPointerPair> seenInternalPtrPairs(comp->trMemory());
   ListIterator<TR_InternalPointerPair> internalPtrIt(&map->getInternalPointerPairs());
   for (TR_InternalPointerPair *internalPtrPair = internalPtrIt.getFirst(); internalPtrPair; internalPtrPair = internalPtrIt.getNext())
      {
      bool seenPinningArrayBefore = false;
      ListIterator<TR_InternalPointerPair> seenInternalPtrIt(&seenInternalPtrPairs);
      for (TR_InternalPointerPair *seenInternalPtrPair = seenInternalPtrIt.getFirst(); seenInternalPtrPair && (seenInternalPtrPair != internalPtrPair); seenInternalPtrPair = seenInternalPtrIt.getNext())
         {
         if (internalPtrPair->getPinningArrayPointer() == seenInternalPtrPair->getPinningArrayPointer())
            {
            seenPinningArrayBefore = true;
            break;
            }
         }

      if (!seenPinningArrayBefore)
         {
         seenInternalPtrPairs.add(internalPtrPair);
         numDistinctPinningArrays++;
         }

      numInternalPtrs++;
      }

   map->setNumDistinctPinningArrays(numDistinctPinningArrays);


   size = size + (2*numDistinctPinningArrays); // 2 bytes for each distinct base array temp;
                                               // first byte for the index of the base array temp
                                               // second byte for the number of derived internal ptrs

   size = size + numInternalPtrs;              // 1 byte for each internal pointer
   map->setSize(size);                         // store for later use

   return size;
   }


// This method creates the internal ptr data structure (common to entire method;
// so hung off the stack atlas). It contains information specifying the base array
// temps and which internal pointers are derived from each base array temp
// in the format described above.
//
// This method creates the internal ptr data structure (common to entire method;
// so hung off the stack atlas). It contains information specifying the base array
// temps and which internal pointers are derived from each base array temp
// in the format described above.
//
static uint8_t *
createInternalPtrStackMapInJ9Format(
      TR_FrontEnd *vm,
      TR_InternalPointerMap * map,
      TR::GCStackAtlas *trStackAtlas,
      TR::CodeGenerator * cg,
      uint8_t * location,
      TR::Compilation *comp)
   {
   if (!map)
      return NULL;

   TR_J9VMBase *fej9 = (TR_J9VMBase *)vm;

   int32_t numPinningArraysOnlyForInternalPtrRegs = trStackAtlas->getPinningArrayPtrsForInternalPtrRegs().getSize();

   // Calculate the number of bytes occupied by this map
   //
   uint32_t mapSize = calculateMapSize(map, comp) + 2*numPinningArraysOnlyForInternalPtrRegs;

   uint8_t *startOfMapData = location;

   int32_t indexOfFirstInternalPtr = trStackAtlas->getIndexOfFirstInternalPointer();

   //printf("JIT Address %x\n", location);
   //*((int32_t *) location) = (int16_t) trStackAtlas->getNumberOfInternalPtrs();
   //printf("Number of internal pointers = %d\n", (*((int16_t *) location)));
   location += sizeof(intptrj_t);

   // Fill in the data now.
   //
   // First fill in how many bytes the variable internal pointer
   // portion of the map will occupy.
   //
   if (debug("traceIP"))
      printf("JIT Address %x\n", location);
   *location = (uint8_t) mapSize;
   if (debug("traceIP"))
      printf("Internal ptr map size = %d\n", *location);
   if (comp->isAlignStackMaps())
      location+=2;
   else
      location++;


   if (debug("traceIP"))
      printf("JIT Address %x\n", location);
   *((int16_t *) location) = (int16_t) indexOfFirstInternalPtr;
   if (debug("traceIP"))
      printf("Index of first internal pointer = %d\n", (*((int16_t *) location)));
   location += 2;

   int32_t offsetOfFirstInternalPtr = trStackAtlas->getOffsetOfFirstInternalPointer();
   if (debug("traceIP"))
      printf("JIT Address %x\n", location);
   *((int16_t *) location) = (int16_t) offsetOfFirstInternalPtr;
   if (debug("traceIP"))
      printf("Offset of first internal pointer = %d\n", (*((int16_t *) location)));
   location += 2;

   // Fill in how many distinct pinning array temps exist
   //
   if (debug("traceIP"))
      printf("JIT Address %x\n", location);
   *location = (uint8_t) (map->getNumDistinctPinningArrays() + numPinningArraysOnlyForInternalPtrRegs);
   if (debug("traceIP"))
      printf("Number of distinct pinning arrays = %d\n", *location);
   location++;

   // Fill in the information for each distinct pinning array temp
   //
   int32_t totalPointers = 0;
   ListElement<TR_InternalPointerPair> *currElement = map->getInternalPointerPairs().getListHead();
   for (;currElement; currElement = currElement->getNextElement())
       {
       // Index of current base array temp
       //
       *location = (uint8_t) (currElement->getData()->getPinningArrayPointer()->getGCMapIndex() - indexOfFirstInternalPtr);
       if (debug("traceIP"))
          printf("JIT : pinning array ptr index %d firstptr %d\n", currElement->getData()->getPinningArrayPointer()->getGCMapIndex(), indexOfFirstInternalPtr);
       // Skip over the byte corresponding to number of derived pointers
       // per base array; to be filled in later.
       //
       location += 2;

       // Index of this internal pointer temp
       //
       *location = (uint8_t) (currElement->getData()->getInternalPointerAuto()->getGCMapIndex() - indexOfFirstInternalPtr);
       if (debug("traceIP"))
          printf("JIT : location %x first internal ptr index %d firstptr %d\n", location, currElement->getData()->getInternalPointerAuto()->getGCMapIndex(), indexOfFirstInternalPtr);
       location++;
       int32_t numInternalPtrsForThisPinningArray = 1;
       //
       // Search for the remaining internal pointer temps
       // derived from this base pinning array temp and fill out
       // those indices as well.
       //
       ListElement<TR_InternalPointerPair> *prevElement = currElement;
       ListElement<TR_InternalPointerPair> *searchElement = currElement->getNextElement();
       while (searchElement)
         {
         TR_InternalPointerPair *internalPtrPair = searchElement->getData();
         if (searchElement->getData()->getPinningArrayPointer() == currElement->getData()->getPinningArrayPointer())
            {
            *location = (uint8_t) (internalPtrPair->getInternalPointerAuto()->getGCMapIndex() - indexOfFirstInternalPtr);
            if (debug("traceIP"))
               printf("JIT : location %x internal ptr index %d firstptr %d\n", location, internalPtrPair->getInternalPointerAuto()->getGCMapIndex(), indexOfFirstInternalPtr);
            location++;
            numInternalPtrsForThisPinningArray++;
            searchElement = searchElement->getNextElement();
            prevElement->setNextElement(searchElement);
            }
         else
            {
            prevElement = searchElement;
            searchElement = searchElement->getNextElement();
            }
         }

       // Fill in the byte skipped earlier that contains the
       // number of internal pointers created out of this base array temp.
       //
       *((location - numInternalPtrsForThisPinningArray) - 1) = numInternalPtrsForThisPinningArray;
       if (debug("traceIP"))
          printf("In VMJ9 numInternalPtrs = %d and memory %x\n", numInternalPtrsForThisPinningArray, (location - numInternalPtrsForThisPinningArray));
       totalPointers = totalPointers + 1 + numInternalPtrsForThisPinningArray;
       }

   // Fill in the bytes for those pinning array slots that are ONLY used
   // by derived internal pointers in registers (not used by any temp);
   // This required so that GC finds and adjusts these pinning array pointers at
   // program points in the method where the pinning array pointer does NOT have
   // any internal pointer derived from it.
   //
   ListIterator<TR::AutomaticSymbol> autoIt(&trStackAtlas->getPinningArrayPtrsForInternalPtrRegs());
   TR::AutomaticSymbol *autoSymbol;
   for (autoSymbol = autoIt.getFirst(); autoSymbol != NULL; autoSymbol = autoIt.getNext())
      {
      *location = (uint8_t) (autoSymbol->getGCMapIndex() - indexOfFirstInternalPtr);
      location++;
      *location = 0;
      location++;
      totalPointers++;
      }

   if (totalPointers >= fej9->maxInternalPlusPinningArrayPointers(comp))
      {
      comp->failCompilation<TR::ExcessiveComplexity>("Failed to create Internal Ptr Stack Map in J9 Format");
      }
   //printf("Special map : Start of map data %x end of map data %x\n", startOfMapData, location);

   return startOfMapData;
   }


static void
createStackMap(
      TR_GCStackMap *map,
      TR::CodeGenerator *cg,
      uint8_t *location,
      bool fourByteOffsets,
      TR::GCStackAtlas *trStackAtlas,
      uint32_t numberOfMapBytes,
      TR::Compilation *comp)
   {
   uint32_t lowCode = map->getLowestCodeOffset();

   // Set the high order bit on the normal register map
   // to denote that this instruction has internal pointers
   // in registers and hence will need the special internal ptr format
   // register map.
   //
   // NOTE : This format (bit) is only agreed upon between OTI
   // and the JIT on IA32; the bit may be different on PPC or
   // other architectures.
   //
   if (map->getInternalPointerMap())
      {
      map->setRegisterBits((1<<cg->getInternalPtrMapBit()));
      if (debug("traceIP"))
         {
         printf("JIT address %x (lowCode %x) method %s with 4 byte offsets (%d) for register map %d (GC map %x ip map %x)\n", location, lowCode, comp->signature(), fourByteOffsets, map->getRegisterMap(), map, map->getInternalPointerMap());
         fflush(stdout);
         }
      }
   else
      map->resetRegistersBits((1<<cg->getInternalPtrMapBit()));

   uintptrj_t startLocation = (uintptrj_t) location;

   if (fourByteOffsets)
      {
      *(uint32_t *)location = lowCode;
      location += 4;
      }
   else
      {
      *(uint16_t *)location = lowCode;
      if (comp->isAlignStackMaps())
         location += 4;
      else
         location += 2;
      }

   TR_ByteCodeInfo byteCodeInfo = map->getByteCodeInfo();

   byteCodeInfo.setDoNotProfile(0); // indicate that this map entry contains a stack map

   if (map == trStackAtlas->getParameterMap())
      byteCodeInfo.setInvalidCallerIndex(); // hacky fix to make sure parameter map caller index is correct
   if (comp->getCurrentMethod()->isJNINative())
      byteCodeInfo.setInvalidByteCodeIndex();

   memcpy(location, &byteCodeInfo, sizeof(TR_ByteCodeInfo));
   location += sizeof(int32_t);

#ifdef TR_HOST_S390
   //traceMsg(comp, "hprmap %p : %x location %p\n", map, location, map->getHighWordRegisterMap());
   *(int32_t *)location = map->getHighWordRegisterMap();
   location += sizeof(int32_t);
#endif

   ///traceMsg(comp, "map %p rsd %x location %p\n", map, location, map->getRegisterSaveDescription());
   *(int32_t *)location = map->getRegisterSaveDescription();
   location += sizeof(int32_t);

   *(int32_t *)location = map->getRegisterMap();
   location += sizeof(int32_t);

   if (map->getInternalPointerMap())
      {
      TR_InternalPointerMap *internalPtrMap = map->getInternalPointerMap();
      int32_t indexOfFirstInternalPtr = trStackAtlas->getIndexOfFirstInternalPointer();

      // Fill in the data now.
      //
      // First fill in the length of the internal pointer variable portion
      // of the map.
      //
      *location = (uint8_t) internalPtrMap->getSize();

      if (debug("traceIP"))
         printf("JIT Address %x Internal ptr map size = %d\n", location, *location);

      location++;

      // Fill in how many distinct pinning array temps exist
      //
      *location = (uint8_t) internalPtrMap->getNumDistinctPinningArrays();

      if (debug("traceIP"))
         printf("JIT Address %x Num pinning arrays = %d\n", location, *location);

      location++;

      // Fill in the information for each distinct pinning array temp
      //
      ListElement<TR_InternalPointerPair> *currElement = map->getInternalPointerMap()->getInternalPointerPairs().getListHead();
      for (;currElement; currElement = currElement->getNextElement())
         {
         //
         // Fill in the index for this pinning array temp
         //
         *location = (uint8_t) (currElement->getData()->getPinningArrayPointer()->getGCMapIndex() - indexOfFirstInternalPtr);
         if (debug("traceIP"))
            printf("JIT Address %x Pinning array = %d\n", location, *location);
         //
         // Skip over the byte corresponding to number of derived register pointers
         // per base array; to be filled in later.
         //
         location += 2;
         //
         // Fill in the register number for this internal pointer register
         //
         *location = (uint8_t) currElement->getData()->getInternalPtrRegNum();
         if (debug("traceIP"))
            printf("JIT : internal ptr reg num %d address %x\n", *location, location);
         location++;

         int32_t numInternalPtrsForThisPinningArray = 1;
         //
         // Search for the remaining internal pointer registers
         // derived from this base pinning array temp and fill out
         // those register numbers as well.
         //
         ListElement<TR_InternalPointerPair> *prevElement = currElement;
         ListElement<TR_InternalPointerPair> *searchElement = currElement->getNextElement();
         while (searchElement)
            {
            TR_InternalPointerPair *internalPtrPair = searchElement->getData();
            if (searchElement->getData()->getPinningArrayPointer() == currElement->getData()->getPinningArrayPointer())
               {
               *location = (uint8_t) internalPtrPair->getInternalPtrRegNum();
               if (debug("traceIP"))
                  printf("JIT : internal ptr reg num %d address %x\n", *location, location);
               location++;
               numInternalPtrsForThisPinningArray++;
               searchElement = searchElement->getNextElement();
               prevElement->setNextElement(searchElement);
               }
            else
               {
               prevElement = searchElement;
               searchElement = searchElement->getNextElement();
               }
            }

          // Fill in the byte skipped earlier that contains the
          // number of internal pointers created out of this base array temp.
          //
         *((location - numInternalPtrsForThisPinningArray) - 1) = numInternalPtrsForThisPinningArray;
         if (debug("traceIP"))
            printf("JIT Address %x Num internal ptrs for this array = %d\n", ((location - numInternalPtrsForThisPinningArray) - 1),numInternalPtrsForThisPinningArray);
         }
      }

   int32_t mapSize = map->getMapSizeInBytes();
   if (mapSize)
      memcpy(location, map->getMapBits(), mapSize);

   if (map->getLiveMonitorBits())
      {
      location[numberOfMapBytes - 1] |= 128;
      location += numberOfMapBytes;
      memcpy(location, map->getLiveMonitorBits(), mapSize);
      }
   else
      location[numberOfMapBytes - 1] &= 127;

   if (debug("traceIP"))
      {
      printf("Start of map %x end of map %x (lowCode %x)\n", startLocation, (location+mapSize), lowCode);
      diagnostic("Map bits are being emitted at %x\n", location);
      diagnostic("Number of slots mapped %d\n", map->getNumberOfSlotsMapped());
      int mapBytes = (map->getNumberOfSlotsMapped() + 7) >> 3;
      int bits = 0;
      for (int i = 0; i < mapBytes; ++i)
         {
         uint8_t mapByte = map->getMapBits()[i];
         for (int j = 0; j < 8; ++j)
            if (bits < map->getNumberOfSlotsMapped())
               {
               diagnostic("Bit : %d is %d", j, mapByte & 1);
               mapByte >>= 1;
               bits++;
               }
         }

      bits = 0;
      diagnostic("\nNumber of slots mapped %d\n", map->getNumberOfSlotsMapped());
      int descriptionBytes = (map->getNumberOfSlotsMapped() + 7) >> 3;
      uint8_t *traceLocation = location;
      for (int k = 0; k < descriptionBytes; k++)
         {
         U_8 descriptionByte = *traceLocation;
         traceLocation++;
         for (int l = 0; l < 8; l++)
            {
            if (bits < map->getNumberOfSlotsMapped())
               {
               diagnostic("Bit : %d is %d\n", l, (descriptionByte & 0x1 ? 1 : 0));
               descriptionByte = descriptionByte >> 1;
               bits++;
               }
            }
         }
      }//debug
   }

static bool
mapsAreIdentical(
      TR_GCStackMap *mapCursor,
      TR_GCStackMap *nextMapCursor,
      TR::GCStackAtlas *trStackAtlas,
      TR::Compilation *comp)
   {
   if (!comp->getOption(TR_FullSpeedDebug) && // Must keep maps with different bytecode info even if GC info is identical
       nextMapCursor &&
       nextMapCursor != trStackAtlas->getParameterMap() &&
       mapCursor != trStackAtlas->getParameterMap() &&
       mapCursor->getMapSizeInBytes() == nextMapCursor->getMapSizeInBytes() &&
       mapCursor->getRegisterMap() == nextMapCursor->getRegisterMap() &&
       !memcmp(mapCursor->getMapBits(), nextMapCursor->getMapBits(), mapCursor->getMapSizeInBytes()) &&
#ifdef TR_HOST_S390
       (mapCursor->getHighWordRegisterMap() == nextMapCursor->getHighWordRegisterMap()) &&
#endif
       (comp->getOption(TR_DisableShrinkWrapping) ||
        (mapCursor->getRegisterSaveDescription() == nextMapCursor->getRegisterSaveDescription())) &&
       (comp->getOption(TR_DisableLiveMonitorMetadata) ||
        ((mapCursor->getLiveMonitorBits() != 0) == (nextMapCursor->getLiveMonitorBits() != 0) &&
         (mapCursor->getLiveMonitorBits() == 0 ||
          !memcmp(mapCursor->getLiveMonitorBits(), nextMapCursor->getLiveMonitorBits(), mapCursor->getMapSizeInBytes())))) &&
       ((!nextMapCursor->getInternalPointerMap() && !mapCursor->getInternalPointerMap()) ||
        (nextMapCursor->getInternalPointerMap() &&
        mapCursor->getInternalPointerMap() &&
        mapCursor->isInternalPointerMapIdenticalTo(nextMapCursor))))
      return true;

   return false;
   }


static uint32_t
calculateSizeOfStackAtlas(
      TR_FrontEnd *vm,
      TR::CodeGenerator * cg,
      bool fourByteOffsets,
      uint32_t numberOfSlotsMapped,
      uint32_t numberOfMapBytes,
      TR::Compilation *comp)
   {
   TR::GCStackAtlas * trStackAtlas = cg->getStackAtlas();

   uint32_t mapSize;
   uint32_t sizeOfMapOffset = (comp->isAlignStackMaps() || fourByteOffsets) ? 4 : 2;
#ifdef TR_HOST_S390
   uint32_t sizeOfStackMap = 4*sizeof(U_32) + sizeOfMapOffset; //4 words in header
#else
   uint32_t sizeOfStackMap = 3*sizeof(U_32) + sizeOfMapOffset; //3 words in header
#endif

   uint32_t sizeOfByteCodeInfoMap = sizeof(U_32) + sizeOfMapOffset;

   mapSize = sizeOfStackMap;

   mapSize += numberOfMapBytes;

   // each map splats a TR_ByteCodeInfo (caller index and a bytecode index)

   // Calculate the atlas size; the fixed size portion is the GC stack map
   // and register map. The variable sized portion is the internal
   // pointer information.
   //
   uint32_t atlasSize = sizeof(J9JITStackAtlas);

   atlasSize += numberOfMapBytes; // space for the outerscope live monitor mask

   ListIterator<TR_GCStackMap> mapIterator(&trStackAtlas->getStackMapList());
   TR_GCStackMap *mapCursor = mapIterator.getFirst();

   // Fill in gaps with bogus stack map for tagged information.
   // Also need to create more space for stack maps above
   //
   UDATA methodEndPCOffset = (UDATA)(cg->getCodeEnd() - cg->getCodeStart());
   uint32_t previousLowestCodeOffset = methodEndPCOffset;
   uint32_t internalPtrMapSize = 0;

   while (mapCursor != NULL)
      {
      TR_GCStackMap *nextMapCursor = mapIterator.getNext();

      internalPtrMapSize = 0;
      if (mapCursor->getInternalPointerMap())
         {
         internalPtrMapSize = calculateMapSize(mapCursor->getInternalPointerMap(), comp);
         }

      if (nextMapCursor)
         {
         calculateMapSize(nextMapCursor->getInternalPointerMap(), comp);
         }

      if (mapsAreIdentical(mapCursor, nextMapCursor, trStackAtlas, comp))
         {
         atlasSize += sizeOfByteCodeInfoMap;
         }
      else
         {
         atlasSize += mapSize;
         if (mapCursor->getInternalPointerMap())
            {
            ++atlasSize; // this accounts for the byte holding the length of the map
            }

         atlasSize += internalPtrMapSize; // shouldn't this be inside the previous 'if' statement?

         if (mapCursor->getLiveMonitorBits())
            {
            atlasSize += numberOfMapBytes;
            }
         }

      previousLowestCodeOffset = mapCursor->getLowestCodeOffset();
      mapCursor = nextMapCursor;
      }

   return atlasSize;
   }


static uint32_t calculateSizeOfInternalPtrMap(TR::Compilation* comp)
   {
   TR::CodeGenerator * cg = comp->cg();
   TR::GCStackAtlas * trStackAtlas = cg->getStackAtlas();
   int32_t sizeOfInternalPtrMap = 0;
   TR_InternalPointerMap * map = trStackAtlas->getInternalPointerMap();
   if (map)
      {
      int32_t numPinningArraysOnlyForInternalPtrRegs = trStackAtlas->getPinningArrayPtrsForInternalPtrRegs().getSize();

      // Calculate the number of bytes occupied by this map
      //
      uint32_t mapSize = calculateMapSize(map, comp) + 2*numPinningArraysOnlyForInternalPtrRegs;

      // Allocate the data structure based on the size; note we allocate
      // memory of size+5 because one byte at the beginning of the map
      // holds the size in bytes, and another 2 bytes byte holds the index
      // of the first internal pointer related auto , and another 2 bytes
      // holds the offset of first internal pointer auto
      //

      sizeOfInternalPtrMap = mapSize + 5 + TR::Compiler->om.sizeofReferenceAddress();
      if(comp->isAlignStackMaps())
         sizeOfInternalPtrMap++;
      }
   return sizeOfInternalPtrMap;
   }

static uint32_t calculateSizeOfOSRInfo(TR::Compilation* comp)
   {
   return comp->getOSRCompilationData()->getSizeOfMetaData();
   }

static uint32_t calculateSizeOfGpuPtx(TR::Compilation* comp)
   {
   uint32_t sizeOfGpuPTX = 0;

   ListIterator<char*> ptxIterator(&(comp->getGPUPtxList()));
   char **cursor = ptxIterator.getFirst();
   while (cursor)
      {
      sizeOfGpuPTX += (strlen(*cursor)+1);
      cursor = ptxIterator.getNext();
      }
   return sizeOfGpuPTX;
   }


static void
createMonitorMask(
      uint8_t *callSiteCursor,
      List<TR::RegisterMappedSymbol> *autos,
      int32_t numberOfMapBytes)
   {
   memset(callSiteCursor, 0, numberOfMapBytes);
   if (autos)
      {
      ListIterator<TR::RegisterMappedSymbol> iterator(autos);
      for (TR::RegisterMappedSymbol * a = iterator.getFirst(); a; a = iterator.getNext())
         {
         int32_t bitNumber = a->getGCMapIndex();
         //TODO: add this important assert in the future
         //TR_ASSERT(bitNumber >= 0, "bitNumber is negative\n");
         callSiteCursor[bitNumber >> 3] |= 1 << (bitNumber & 7);
         }
      }
   }


static uint8_t *
createStackAtlas(
      TR_FrontEnd *vm,
      TR::CodeGenerator *cg,
      bool fourByteOffsets,
      uint32_t numberOfSlotsMapped,
      uint32_t numberOfMapBytes,
      TR::Compilation *comp,
      uint8_t *atlasBits,
      uint32_t atlasSizeInBytes)
   {
   TR::GCStackAtlas * trStackAtlas = cg->getStackAtlas();

   trStackAtlas->setAtlasBits(atlasBits);

   uint32_t mapSizeInBytes;
   uint32_t sizeOfMapOffset = (comp->isAlignStackMaps() || fourByteOffsets) ? 4 : 2;

#ifdef TR_HOST_S390
   uint32_t sizeOfStackMapInBytes = 4*sizeof(U_32) + sizeOfMapOffset; // 4 words in header
#else
   uint32_t sizeOfStackMapInBytes = 3*sizeof(U_32) + sizeOfMapOffset; // 3 words in header
#endif

   uint32_t sizeOfByteCodeInfoMap = sizeof(U_32) + sizeOfMapOffset;

   mapSizeInBytes = sizeOfStackMapInBytes;

   mapSizeInBytes += numberOfMapBytes;

   //each map splats a TR_ByteCodeInfo (caller index and a bytecode index)

   // Calculate the atlas size; the fixed size portion is the GC stack map
   // and register map. The variable sized portion is the internal
   // pointer information.
   //
   ListIterator<TR_GCStackMap> mapIterator(&trStackAtlas->getStackMapList());
   TR_GCStackMap *mapCursor = mapIterator.getFirst();

   /* Fill in gaps with bogus stack map for tagged information.
    * Also need to create more space for stack maps above */

   UDATA methodEndPCOffset = (UDATA)(cg->getCodeEnd() - cg->getCodeStart());
   uint32_t previousLowestCodeOffset = methodEndPCOffset;

   J9JITStackAtlas *vmAtlas = (J9JITStackAtlas *)atlasBits;
   vmAtlas->numberOfMaps      = trStackAtlas->getNumberOfMaps();
   vmAtlas->numberOfMapBytes  = numberOfMapBytes;
   vmAtlas->parmBaseOffset    = trStackAtlas->getParmBaseOffset();
   vmAtlas->numberOfParmSlots = trStackAtlas->getNumberOfParmSlotsMapped();
   vmAtlas->localBaseOffset   = trStackAtlas->getLocalBaseOffset();

   if (debug("traceIP"))
      printf("Local base offset = %d\n", trStackAtlas->getLocalBaseOffset());

   // Abort if we have overflowed the fields in vmAtlas.
   //
   if (numberOfMapBytes                           > USHRT_MAX ||
       trStackAtlas->getNumberOfMaps()            > USHRT_MAX ||
       trStackAtlas->getNumberOfParmSlotsMapped() > USHRT_MAX ||
       trStackAtlas->getParmBaseOffset()  < SHRT_MIN || trStackAtlas->getParmBaseOffset()  > SHRT_MAX ||
       trStackAtlas->getLocalBaseOffset() < SHRT_MIN || trStackAtlas->getLocalBaseOffset() > SHRT_MAX)
      {
      comp->failCompilation<TR::CompilationException>("Overflowed fields in vmAtlas");
      }

   TR_Array<List<TR::RegisterMappedSymbol> *> & monitorAutos = comp->getMonitorAutos();
   List<TR::RegisterMappedSymbol> * autos = (monitorAutos.isEmpty() ? 0 : monitorAutos[0]);
   createMonitorMask(atlasBits + sizeof(J9JITStackAtlas), autos, numberOfMapBytes);

   // Maps are in reverse order in list from what we want in the atlas
   // so advance to the address where the last map should go and start
   // building the maps moving back toward the beginning of the atlas.
   // The end of stack atlas data is also the location where internal
   // ptr data will be located
   //
   uint8_t *cursor = atlasBits + atlasSizeInBytes;

   if (trStackAtlas->getStackAllocMap())
      {
      vmAtlas->stackAllocMap = cursor;
      cursor += sizeof(uintptrj_t);
      int32_t mapSizeInBytes = trStackAtlas->getStackAllocMap()->getMapSizeInBytes();
      memcpy(cursor, trStackAtlas->getStackAllocMap()->getMapBits(), mapSizeInBytes);
      cursor += numberOfMapBytes;
      }

   vmAtlas->internalPointerMap = createInternalPtrStackMapInJ9Format(vm, trStackAtlas->getInternalPointerMap(), trStackAtlas, cg, cursor, comp);

   if (trStackAtlas->getStackAllocMap())
      {
      cursor -= (numberOfMapBytes + sizeof(uintptrj_t));
      }

   TR::ResolvedMethodSymbol * methodSymbol = comp->getJittedMethodSymbol();
   TR::AutomaticSymbol * syncObjectTemp = methodSymbol->getSyncObjectTemp() ? methodSymbol->getSyncObjectTemp()->getSymbol()->getAutoSymbol() : 0;
   vmAtlas->paddingTo32 = (U_16)(syncObjectTemp && syncObjectTemp->getGCMapIndex() != -1 ? syncObjectTemp->getOffset() : -1);

   if (debug("stackAtlasStats"))
      {
      ++stackAtlasStats._counter;
      stackAtlasStats._totalSlots += numberOfSlotsMapped;
      stackAtlasStats._maxSlots = std::max(stackAtlasStats._maxSlots, numberOfSlotsMapped);
      }

   mapIterator.reset();
   mapCursor = mapIterator.getFirst();
   previousLowestCodeOffset = methodEndPCOffset;  /* initialize to end PC of method to fill in gap between last
   stack map and the end PC*/

   while (mapCursor != NULL)
      {
      /* move back from the end of the atlas till the current map
      can be fitted in; then pass the cursor to the routine
      that actually creates and fills in the stack map
      */
      TR_GCStackMap *nextMapCursor = mapIterator.getNext();

      /* Fill in gaps with bogus stack map for tagged information.
       * Also need to create more space for stack maps above */

      if (mapsAreIdentical(mapCursor, nextMapCursor, trStackAtlas, comp))
         {
         cursor -= sizeOfByteCodeInfoMap; //GET_SIZEOF_BYTECODEINFO_MAP(fourByteOffsets);
         createByteCodeInfoRange(mapCursor, cursor, fourByteOffsets, trStackAtlas, comp);
         }
      else
         {
         cursor -= mapSizeInBytes;
         if (mapCursor->getInternalPointerMap())
            {
            cursor -= (mapCursor->getInternalPointerMap()->getSize() + 1);
            }

         if (mapCursor->getLiveMonitorBits())
            cursor -= numberOfMapBytes;

         createStackMap(mapCursor, cg, cursor, fourByteOffsets, trStackAtlas, numberOfMapBytes, comp);

         if (vmAtlas->internalPointerMap && mapCursor == trStackAtlas->getParameterMap())
            *((uintptrj_t *) vmAtlas->internalPointerMap) = (uintptrj_t) cursor;
         if (vmAtlas->stackAllocMap && mapCursor == trStackAtlas->getParameterMap())
            *((uintptrj_t *) vmAtlas->stackAllocMap) = (uintptrj_t) cursor;
         }
      previousLowestCodeOffset = mapCursor->getLowestCodeOffset();
      mapCursor = nextMapCursor;
      }

   return atlasBits;
   }


void AOTRAS_traceMetaData(TR_J9VMBase *vm, TR_MethodMetaData *data, TR::Compilation *comp)
   {
   traceMsg(comp, "<relocatableDataMetaDataCG>\n");
   J9JITDataCacheHeader *aotMethodHeader = (J9JITDataCacheHeader *)comp->getAotMethodDataStart();
   TR_AOTMethodHeader *aotMethodHeaderEntry =  (TR_AOTMethodHeader *)(aotMethodHeader + 1);

   traceMsg(comp, "%s\n", comp->signature());
   traceMsg(comp, "%-12s", "startPC");
   traceMsg(comp, "%-12s", "endPC");
   traceMsg(comp, "%-8s", "size");
   traceMsg(comp, "%-14s", "gcStackAtlas");
   traceMsg(comp, "%-12s\n", "bodyInfo");

   traceMsg(comp, "%-12x", data->startPC);
   traceMsg(comp, "%-12x", data->endPC);
   traceMsg(comp, "%-8x", data->size);
   traceMsg(comp, "%-14x", data->gcStackAtlas);
   traceMsg(comp, "%-12x\n", data->bodyInfo);

   traceMsg(comp, "%-12s", "CodeStart");
   traceMsg(comp, "%-12s", "DataStart");
   traceMsg(comp, "%-10s", "CodeSize");
   traceMsg(comp, "%-10s", "DataSize");
   traceMsg(comp, "%-12s\n", "inlinedCalls");

   traceMsg(comp, "%-12x",aotMethodHeaderEntry->compileMethodCodeStartPC);
   traceMsg(comp, "%-12x",aotMethodHeaderEntry->compileMethodDataStartPC);
   traceMsg(comp, "%-10x",aotMethodHeaderEntry->compileMethodCodeSize);
   traceMsg(comp, "%-10x",aotMethodHeaderEntry->compileMethodDataSize);
   traceMsg(comp, "%-12x\n", data->inlinedCalls);

   traceMsg(comp, "</relocatableDataMetaDataCG>\n");
   }

static int32_t calculateExceptionsSize(
   TR::Compilation* comp,
   TR_ExceptionTableEntryIterator& exceptionIterator,
   bool& fourByteExceptionTableEntries,
   uint32_t& numberOfExceptionRangesWithBits)
   {
   uint32_t exceptionsSize = 0;
   uint32_t numberOfExceptionRanges = exceptionIterator.size();
   numberOfExceptionRangesWithBits = numberOfExceptionRanges;
   if (numberOfExceptionRanges)
      {
      if (numberOfExceptionRanges > 0x3FFF)
         return -1; // our meta data representation only has 14 bits for the number of exception ranges

      if (!fourByteExceptionTableEntries)
         for (TR_ExceptionTableEntry * e = exceptionIterator.getFirst(); e; e = exceptionIterator.getNext())
            if (e->_catchType > 0xFFFF || !e->_method->isSameMethod(comp->getCurrentMethod()))
               { fourByteExceptionTableEntries = true; break; }

      int32_t entrySize;
      if (fourByteExceptionTableEntries)
         {
         entrySize = sizeof(J9JIT32BitExceptionTableEntry);
         numberOfExceptionRangesWithBits |= 0x8000;
         }
      else
         entrySize = sizeof(J9JIT16BitExceptionTableEntry);

      if (comp->getOption(TR_FullSpeedDebug))
         {
         numberOfExceptionRangesWithBits |= 0x4000;
         entrySize += 4;
         }

      exceptionsSize = numberOfExceptionRanges * entrySize;
      }
   return exceptionsSize;
   }

static void
populateBodyInfo(
      TR::Compilation *comp,
      TR_J9VMBase *vm,
      TR_MethodMetaData *data)
   {
   TR::Recompilation * recompInfo = comp->getRecompilationInfo();

   // If recompilation is supported, add persistent method info and body info
   // into persistent metadata
   //
   if (recompInfo)
      {
      if (vm->isAOT_DEPRECATED_DO_NOT_USE())
         {
         // The allocation for the Persistent Method Info and the Persistent Jitted Body Info used to be allocated with the exception table.
         // Exception tables are now being reaped on method recompilation.  As these need to be persistent, we need to allocate them seperately.
         // Later, we should probably reconsider what the structure for this whole method is and where it all belongs.
         //
         bool retryCompilation = false;
         uint32_t bytesAllocated = 0;
         uint32_t bytesRequested = sizeof(TR_PersistentMethodInfo) + sizeof(TR_PersistentJittedBodyInfo);
         uint8_t *persistentInfo = vm->allocateDataCacheRecord(
            bytesRequested,
            comp,
            true,
            &retryCompilation,
            J9_JIT_DCE_AOT_PERSISTENT_INFO,
            &bytesAllocated
         );

         if (!persistentInfo)
            {
            if (retryCompilation)
               {
               comp->failCompilation<J9::RecoverableDataCacheError>("Failed to allocate persistent info");
               }
            comp->failCompilation<J9::DataCacheError>("Failed to allocate persistent info");
            }

         memset(persistentInfo, 0, bytesAllocated);

         uint8_t *locationPersistentJittedBodyInfo = persistentInfo; // AOT data cache allocations should already be pointer aligned.
         uint8_t *locationPersistentMethodInfo = persistentInfo + sizeof(TR_PersistentJittedBodyInfo);

         TR_PersistentJittedBodyInfo *bodyInfoSrc = recompInfo->getJittedBodyInfo();
         TR_PersistentMethodInfo *methodInfoSrc = recompInfo->getMethodInfo();
         methodInfoSrc->setIsInDataCache(true);
         data->bodyInfo = locationPersistentJittedBodyInfo;
         TR_PersistentJittedBodyInfo *newBodyInfo = (TR_PersistentJittedBodyInfo *)locationPersistentJittedBodyInfo;

         // Generate and copy body/method info into the meta data
         memcpy(locationPersistentJittedBodyInfo, bodyInfoSrc, sizeof(TR_PersistentJittedBodyInfo));
         recompInfo->setJittedBodyInfo(newBodyInfo);

         memcpy(locationPersistentMethodInfo, methodInfoSrc, sizeof(TR_PersistentMethodInfo));
         recompInfo->setMethodInfo((TR_PersistentMethodInfo *)locationPersistentMethodInfo);
         newBodyInfo->setMethodInfo((TR_PersistentMethodInfo *)locationPersistentMethodInfo);

         J9JITDataCacheHeader *aotMethodHeader = (J9JITDataCacheHeader *)comp->getAotMethodDataStart();
         TR_AOTMethodHeader *aotMethodHeaderEntry =  (TR_AOTMethodHeader *)(aotMethodHeader + 1);
         aotMethodHeaderEntry->offsetToPersistentInfo = ((char *) persistentInfo - sizeof(J9JITDataCacheHeader) - (char *)aotMethodHeader);

         //Free the old copies of body/method info
         TR_Memory::jitPersistentFree(bodyInfoSrc);
         TR_Memory::jitPersistentFree(methodInfoSrc);
         }
      else
         {
         data->bodyInfo = recompInfo->getJittedBodyInfo();
         }
      }
   else
      {
      if (vm->isAOT_DEPRECATED_DO_NOT_USE())
         {
         J9JITDataCacheHeader *aotMethodHeader = (J9JITDataCacheHeader *)comp->getAotMethodDataStart();
         TR_AOTMethodHeader *aotMethodHeaderEntry =  (TR_AOTMethodHeader *)(aotMethodHeader + 1);
         aotMethodHeaderEntry->offsetToPersistentInfo = 0;
         }

      data->bodyInfo = NULL;
      }

   }

static void populateOSRInfo(TR::Compilation* comp, TR_MethodMetaData* data, uint32_t osrInfoOffset)
   {
   uint32_t offset = 0;
   if (comp->getOption(TR_EnableOSR))
      {
      data->osrInfo = (uint8_t*)data + osrInfoOffset;
      comp->getOSRCompilationData()->writeMetaData((uint8_t*)data->osrInfo);
      }
   else
      data->osrInfo = NULL;
   }

static void populateGpuPtx(TR::Compilation* comp, char** gpuPtxArrayCursor, char *gpuPtxArrayEntryLocation)
   {
   ListIterator<char*> ptxIterator(&(comp->getGPUPtxList()));
   char **cursor = ptxIterator.getFirst();
   while (cursor)
      {
      *gpuPtxArrayCursor = gpuPtxArrayEntryLocation;
      gpuPtxArrayCursor++;
      strcpy(gpuPtxArrayEntryLocation, *cursor);
      gpuPtxArrayEntryLocation += (strlen(*cursor)+1);
      cursor = ptxIterator.getNext();
      }
   }

static void populateGpuLineNumbers(TR::Compilation* comp, int* gpuLineNumberArrayCursor)
   {
   ListIterator<int32_t> lineNumberIterator(&(comp->getGPUKernelLineNumberList()));
   int32_t *cursor = lineNumberIterator.getFirst();
   while (cursor)
      {
      *gpuLineNumberArrayCursor = *cursor;
      gpuLineNumberArrayCursor++;
      cursor = lineNumberIterator.getNext();
      }
   }

static void populateInlineCalls(
   TR::Compilation* comp, TR_J9VMBase* vm,
   TR_MethodMetaData* data, uint8_t* callSiteCursor,
   uint32_t numberOfMapBytes)
   {
   TR_Array<List<TR::RegisterMappedSymbol> *> & monitorAutos = comp->getMonitorAutos();
   for (int32_t i = 0; i < comp->getNumInlinedCallSites(); ++i)
      {
      TR_InlinedCallSite * inlinedCallSite = &comp->getInlinedCallSite(i);

      List<TR::RegisterMappedSymbol> * autos = (i + 1 < monitorAutos.size()) ? monitorAutos[i+1] : 0;
      bool hasLiveMonitors = (i + 1 < monitorAutos.size() && monitorAutos[i+1]);

      // _isSameReceiver is an overloaded bit that in this context means that the live monitor data for this
      // inlined call site contains some nonzero bits
      //
      if (autos != 0)
         inlinedCallSite->_byteCodeInfo.setIsSameReceiver(1);

      if (vm->isAOT_DEPRECATED_DO_NOT_USE())
         {
         inlinedCallSite->_methodInfo = (TR_OpaqueMethodBlock*)-1;
         }
      memcpy(callSiteCursor, inlinedCallSite, sizeof(TR_InlinedCallSite) );
      if (comp->getOption(TR_AOT) && (comp->getOption(TR_TraceRelocatableDataCG) || comp->getOption(TR_TraceRelocatableDataDetailsCG) || comp->getOption(TR_TraceReloCG)))
      	 {
      	 traceMsg(comp, "inlineIdx %d, callSiteCursor %p, inlinedCallSite->methodInfo = %p\n", i, callSiteCursor, inlinedCallSite->_methodInfo);
      	 }

      if (!vm->isAOT_DEPRECATED_DO_NOT_USE()) // For AOT, we should only have returned resolved info about a method if the method came from same class loaders.
         {
         J9Class *j9clazz = (J9Class *) J9_CLASS_FROM_CP(((J9RAMConstantPoolItem *) J9_CP_FROM_METHOD(((J9Method *) inlinedCallSite->_methodInfo))));
         TR_OpaqueClassBlock *clazzOfInlinedMethod = ((TR_J9VMBase*) comp->fej9())->convertClassPtrToClassOffset(j9clazz);
         if (comp->fej9()->isUnloadAssumptionRequired(clazzOfInlinedMethod, comp->getCurrentMethod()))
            {
            if (comp->getOption(TR_AOT) && comp->getOption(TR_TraceRelocatableDataDetailsCG))
               {
               TR_OpaqueClassBlock *clazzOfCallerMethod =  comp->getCurrentMethod()->classOfMethod();
               traceMsg(comp, "createClassUnloadPicSite: clazzOfInlinedMethod %p, loader = %p, clazzOfCallerMethod %p, loader = %p, callsiteCursor %p\n",
                       clazzOfInlinedMethod,
                       comp->fej9()->getClassLoader(clazzOfInlinedMethod),
                       clazzOfCallerMethod,
                       comp->fej9()->getClassLoader(clazzOfCallerMethod),
                       callSiteCursor);
               }
#if (defined(TR_HOST_64BIT) && defined(TR_HOST_POWER))
            createClassUnloadPicSite((void*) j9clazz, (void*) (callSiteCursor+(TR::Compiler->target.cpu.isBigEndian()?4:0)), 4, comp->getMetadataAssumptionList());
#else
            createClassUnloadPicSite((void*) j9clazz, (void*) callSiteCursor, sizeof(uintptrj_t), comp->getMetadataAssumptionList());
#endif
            }
         }

      callSiteCursor += sizeof(TR_InlinedCallSite);

      createMonitorMask(callSiteCursor, autos, numberOfMapBytes);

      callSiteCursor += numberOfMapBytes;
      }

   uint32_t maxInlineDepth = comp->getMaxInlineDepth();

   if (maxInlineDepth > vm->_jitConfig->maxInlineDepth)
      {
      vm->_jitConfig->maxInlineDepth = maxInlineDepth;
      }
   }


//
//the routine that sequences the creation of the meta-data for the method
//
TR_MethodMetaData *
createMethodMetaData(
   TR_J9VMBase & vmArg,
   TR_ResolvedMethod *vmMethod,
   TR::Compilation *comp
   )
   {
   TR_J9VMBase * vm = &vmArg;
   TR_ExceptionTableEntryIterator exceptionIterator(comp);
   TR::ResolvedMethodSymbol * methodSymbol = comp->getJittedMethodSymbol();
   TR::CodeGenerator * cg = comp->cg();
   TR::GCStackAtlas * trStackAtlas = cg->getStackAtlas();

   TR_Debug *dbg = (TR_Debug *)comp->getDebug();
   if (dbg)
      {
      dbg->setSingleAllocMetaData(true);
      }

   bool fourByteOffsets = RANGE_NEEDS_FOUR_BYTE_OFFSET(cg->getCodeLength());

   uint32_t tableSize = sizeof(TR_MethodMetaData);

   // --------------------------------------------------------------------------
   // Computing the size of the exception table
   // fourByteExceptionTableEntries and numberOfExceptionRangesWithBits will be
   // computed in calculateExceptionSize
   //
   bool fourByteExceptionTableEntries = fourByteOffsets;
   uint32_t numberOfExceptionRangesWithBits = 0;
   int32_t exceptionsSize = calculateExceptionsSize(
         comp,
         exceptionIterator,
         fourByteExceptionTableEntries,
         numberOfExceptionRangesWithBits);

   if (exceptionsSize == -1)
      {
      return NULL;
      }

   tableSize += exceptionsSize;
   uint32_t exceptionTableSize = tableSize; //Size of the meta data header and exception entries

   // --------------------------------------------------------------------------
   // Computing the size of the inlinedCall
   //
   uint32_t numberOfSlotsMapped = trStackAtlas->getNumberOfSlotsMapped();
   uint32_t numberOfMapBytes = (numberOfSlotsMapped + 1 + 7) >> 3; // + 1 to indicate whether there's a live monitor map
   if (comp->isAlignStackMaps())
      {
      numberOfMapBytes = (numberOfMapBytes + 3) & ~3;
      }

   // Space for the number of entries
   //
   uint32_t inlinedCallSize = comp->getNumInlinedCallSites() * (sizeof(TR_InlinedCallSite) + numberOfMapBytes);
   tableSize += inlinedCallSize;

   if (debug("metaDataStats"))
      {
      ++metaDataStats._counter;
      metaDataStats._exceptionDataSize += exceptionsSize;
      metaDataStats._tableSize += sizeof(TR_MethodMetaData);
      metaDataStats._inlinedCallDataSize += inlinedCallSize;
      }

   // Add size of stack atlas to allocate
   //
   int32_t sizeOfStackAtlasInBytes = calculateSizeOfStackAtlas(
         vm,
         cg,
         fourByteOffsets,
         numberOfSlotsMapped,
         numberOfMapBytes,
         comp);

   tableSize += sizeOfStackAtlasInBytes;

   // Add size of internal ptr data structure to allocate
   //
   tableSize += calculateSizeOfInternalPtrMap(comp);

   // Escape analysis change for compressed pointers
   //
   TR_GCStackAllocMap * stackAllocMap = trStackAtlas->getStackAllocMap();
   if (stackAllocMap)
      {
      tableSize += numberOfMapBytes + sizeof(uintptrj_t);
      }

   int32_t osrInfoOffset = -1;
   if (comp->getOption(TR_EnableOSR))
      {
      osrInfoOffset = tableSize;
      tableSize += calculateSizeOfOSRInfo(comp);
      }

   int32_t gpuMetaDataOffset = -1;
   int32_t gpuMethodSignatureOffset = -1;
   int32_t gpuLineNumberArrayOffset = -1;
   int32_t gpuPtxArrayOffset = -1;
   int32_t gpuPtxArrayEntryOffset = -1;
   int32_t gpuCuModuleArrayOffset = -1;

   int32_t maxNumCachedDevices = 8; // max # of GPGPU cards that JIT compile can cache CUmodule for
   int32_t sizeOfCUmodule = 0;

#ifdef ENABLE_GPU
   int getCUmoduleSize();
   sizeOfCUmodule = getCUmoduleSize(); //get size of the CUmodule object from the CUDA library. Only used when the GPU is used
#endif

   if (comp->getGPUPtxCount() > 0)
      {
      //Struct of data and pointers to GPU metadata.
      gpuMetaDataOffset = tableSize;
      tableSize += sizeof(GpuMetaData);

      gpuMethodSignatureOffset = tableSize;
      tableSize += strlen(comp->signature())+1;

      gpuLineNumberArrayOffset = tableSize;
      tableSize += comp->getGPUPtxCount() * sizeof(int);

      gpuPtxArrayOffset = tableSize;
      tableSize += comp->getGPUPtxCount() * sizeof(char*);

      gpuPtxArrayEntryOffset = tableSize;
      tableSize += calculateSizeOfGpuPtx(comp);

      gpuCuModuleArrayOffset = tableSize;
      tableSize += sizeOfCUmodule * comp->getGPUPtxCount() * maxNumCachedDevices;
      }


   int32_t bytecodePCToIAMapOffset = -1;
   if (comp->getPersistentInfo()->isRuntimeInstrumentationEnabled() &&
       TR::Options::getCmdLineOptions()->getOption(TR_EnableHardwareProfileIndirectDispatch) &&
       TR::Options::getCmdLineOptions()->getOption(TR_EnableMetadataBytecodePCToIAMap))
      {
      // Array of TR_HWPBytecodePCToIAMap structs
      // The first element is a special; it contains the size of the array and an eyecatcher
      bytecodePCToIAMapOffset = tableSize;
      uint32_t memoryUsedByMetadataMapping = (comp->getHWPBCMap()->size() + 1) * sizeof(TR_HWPBytecodePCToIAMap);
      tableSize += memoryUsedByMetadataMapping;
      }

   /* Legend of the info stored at "data". From top to bottom, the address increases

      Exception Info
      --------------
      Inline Info
      --------------
      Stack Atlas
      --------------
      Stack alloc map (if exists)
      --------------
      internal pointer map

   */

   TR_MethodMetaData * data = (TR_MethodMetaData*) vmMethod->allocateException(tableSize, comp);

   populateBodyInfo(comp, vm, data);

   data->startPC = (UDATA)comp->cg()->getCodeStart();
   data->endPC = (UDATA)comp->cg()->getCodeEnd();
   data->startColdPC = (UDATA)0;
   data->endWarmPC = data->endPC;
   data->codeCacheAlloc = (UDATA)comp->cg()->getBinaryBufferStart();

   data->flags = 0;
   if (fourByteOffsets)
      data->flags |= JIT_METADATA_GC_MAP_32_BIT_OFFSETS;

   data->hotness = comp->getMethodHotness();
   data->totalFrameSize = comp->cg()->getFrameSizeInBytes()/TR::Compiler->om.sizeofReferenceAddress();
   data->slots = vmMethod->numberOfParameterSlots();
   data->scalarTempSlots = methodSymbol->getScalarTempSlots();
   data->objectTempSlots = methodSymbol->getObjectTempSlots();
   data->prologuePushes = methodSymbol->getProloguePushSlots();
   data->numExcptionRanges = numberOfExceptionRangesWithBits;
   data->tempOffset = comp->cg()->getStackAtlas()->getNumberOfPendingPushSlots();
   data->size = tableSize;

   data->gcStackAtlas = createStackAtlas(
         vm,
         comp->cg(),
         fourByteOffsets,
         numberOfSlotsMapped,
         numberOfMapBytes,
         comp,
         ((uint8_t *)data + exceptionTableSize + inlinedCallSize),
         sizeOfStackAtlasInBytes);

   if (comp->cg()->getShrinkWrappingDone())
      {
      traceMsg(comp, "lowestSavedRegister %x\n", comp->cg()->getLowestSavedRegister());
      data->registerSaveDescription = J9TR_SHRINK_WRAP;

      // Stash the lowest register saved in the prologue in the last byte of the
      // rsd hung off the method's metadata. this is consulted during the stackwalk
      // so that the right stack slots are checked
      //
      // note: on x86, the preserved registers are not necessarily allocated in sequence,
      // so the info looks like this:
      // data->registerSaveDescription = 0x....| abcd
      // the low 16bits are used to store a bitvector of registers that are shrinkwrapped
      // in this method. this bitvector is consulted during the stackwalk to determine
      // which stack slots need to be checked
      //
      // on ppc and s390, the registers are always allocated in sequence, so just store
      // the lowest number in the low byte
      //
      data->registerSaveDescription |= (comp->cg()->getLowestSavedRegister() & 0xFFFF);
      }
   else
      {
      data->registerSaveDescription = comp->cg()->getRegisterSaveDescription();
      }

#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT)
   if (vm->isAOT_DEPRECATED_DO_NOT_USE())
      {
      TR::CodeCache * codeCache = comp->getCurrentCodeCache(); // MCT

      /* Align code caches */
      codeCache->alignWarmCodeAlloc(3);

      J9JITDataCacheHeader *aotMethodHeader = (J9JITDataCacheHeader *)comp->getAotMethodDataStart();
      TR_AOTMethodHeader *aotMethodHeaderEntry =  (TR_AOTMethodHeader *)(aotMethodHeader + 1);

      // Performed just after TR_AOTMethodHeader has been allocated in  TR::CompilationInfo::wrappedCompile():
      // - aotMethodHeaderEntry->majorVersion = TR_AOTMethodHeader_MajorVersion      // AOT code and data major version
      // - aotMethodHeaderEntry->minorVersion  = TR_AOTMethodHeader_MinorVersion      // AOT code and data minor version

      //  Sections that have not been allocated will have the offset 0. At this point at least exception table (method meta data ) was allocated.

      aotMethodHeaderEntry->offsetToExceptionTable = ((char *) data - sizeof(J9JITDataCacheHeader) - (char *)aotMethodHeader);

      if ((comp->cg()->getAheadOfTimeCompile()->getRelocationData() - (uint8_t *)aotMethodHeader) > 0 )
         {
         aotMethodHeaderEntry->offsetToRelocationDataItems = (comp->cg()->getAheadOfTimeCompile()->getRelocationData() - (uint8_t *)aotMethodHeader);
         }
      else
         {
         aotMethodHeaderEntry->offsetToRelocationDataItems = 0;
         }

      aotMethodHeaderEntry->compileMethodCodeStartPC = (UDATA)comp->getAotMethodCodeStart();
      aotMethodHeaderEntry->compileMethodDataStartPC = (UDATA)comp->getAotMethodDataStart();
      aotMethodHeaderEntry->compileMethodCodeSize = (UDATA)codeCache->getWarmCodeAlloc() - (UDATA)comp->getAotMethodCodeStart();

      // For AOT we should have a reserved dataCache
      //
      TR_DataCache *dataCache = (TR_DataCache*)comp->getReservedDataCache();
      TR_ASSERT(dataCache, "Must have a reserved dataCache for AOT compilations");
      aotMethodHeaderEntry->compileMethodDataSize = (UDATA)dataCache->getCurrentHeapAlloc() - (UDATA)comp->getAotMethodDataStart();

      // Set some flags in the AOTMethodHeader
      //
      if (!vm->isMethodExitTracingEnabled(vmMethod->getPersistentIdentifier()) &&
          !vm->canMethodExitEventBeHooked())
         {
         aotMethodHeaderEntry->flags |= TR_AOTMethodHeader_IsNotCapableOfMethodExitTracing;
         }

      if (!vm->isMethodEnterTracingEnabled(vmMethod->getPersistentIdentifier()) &&
          !vm->canMethodEnterEventBeHooked())
         {
         aotMethodHeaderEntry->flags |= TR_AOTMethodHeader_IsNotCapableOfMethodEnterTracing;
         }

      // totalAllocated space is in comp object
      TR_ASSERT(comp->getTotalNeededDataCacheSpace() == aotMethodHeaderEntry->compileMethodDataSize, "Size missmatach");
      }
#endif

   createExceptionTable(data, exceptionIterator, fourByteExceptionTableEntries, comp);

   uint8_t *callSiteCursor = (uint8_t *)data + exceptionTableSize;
   if (inlinedCallSize)
      {
      data->inlinedCalls = (void*)callSiteCursor;
      }
   else
      {
      data->inlinedCalls = NULL;
      }

   // AOT RAS output if tracerelocatableData[Details]CG requested
   //
   if (vm->isAOT_DEPRECATED_DO_NOT_USE() &&
      (comp->getOption(TR_TraceRelocatableDataCG) || comp->getOption(TR_TraceRelocatableDataDetailsCG)))
      {
      AOTRAS_traceMetaData(vm,data,comp);
      }

   populateInlineCalls(comp, vm, data, callSiteCursor, numberOfMapBytes);

   if (!(vm->_jitConfig->runtimeFlags & J9JIT_TOSS_CODE) && !vm->isAOT_DEPRECATED_DO_NOT_USE())
      {
      TR_TranslationArtifactManager *artifactManager = TR_TranslationArtifactManager::getGlobalArtifactManager();
      TR_TranslationArtifactManager::CriticalSection updateMetaData;

      if ( !(artifactManager->insertArtifact( static_cast<J9JITExceptionTable *>(data) ) ) )
         {
         // Insert trace point here for insertion failure
         }
      if (vm->isAnonymousClass( (TR_OpaqueClassBlock*) ((TR_ResolvedJ9Method*)vmMethod)->constantPoolHdr()))
         {
         J9Class *j9clazz = ((TR_ResolvedJ9Method*)vmMethod)->constantPoolHdr();
         J9CLASS_EXTENDED_FLAGS_SET(j9clazz, J9ClassContainsJittedMethods);
         data->prevMethod = NULL;
         data->nextMethod = j9clazz->jitMetaDataList;
         if (j9clazz->jitMetaDataList)
            j9clazz->jitMetaDataList->prevMethod = data;
         j9clazz->jitMetaDataList = data;
         }
      else
         {
         J9ClassLoader * classLoader = ((TR_ResolvedJ9Method*)vmMethod)->getClassLoader();
         classLoader->flags |= J9CLASSLOADER_CONTAINS_JITTED_METHODS;
         data->prevMethod = NULL;
         data->nextMethod = classLoader->jitMetaDataList;
         if (classLoader->jitMetaDataList)
            classLoader->jitMetaDataList->prevMethod = data;
         classLoader->jitMetaDataList = data;
         }
      }

   populateOSRInfo(comp, data, osrInfoOffset);

   if (comp->getGPUPtxCount() > 0)
      {
      /* Legend of the info stored at "data->gpucode". From top to bottom, the address increases
         GpuMetaData.methodSignature;     //pointer to method signature
         GpuMetaData.numPtxKernels;       //total number of PTX kernels in the method
         GpuMetaData.maxNumCachedDevices; //maximum number of devices that CUmodules can be cached for
         GpuMetaData.lineNumberArray;     //pointer to an array containing the source line number of each gpu kernel
         GpuMetaData.ptxArray;            //PTX code is stored as a char* string. This is an array that points to each entry.
         GpuMetaData.cuModuleArray;       //Array of cached CUmodules. One entry per PTX kernel and device combination
         --------------
         method signature (0 termination)
         --------------
         gpu kernel line number list
           int *
           ... repeat comp->getGPUPtxCount() times ...
         --------------
         gpu ptx list
           char * (ptr to ptx string)
           ... repeat comp->getGPUPtxCount() times ...
           char [] ptx string
           ... repeat comp->getGPUPtxCount() times ...
         --------------
         gpu cumodule list (grouped by deviceID)
           CUmodule ptr (for ptxSourceID = 0, deviceID = 0)
           CUmodule ptr (for ptxSourceID = 1, deviceID = 0)
           ... repeat comp->getGPUPtxCount() times ...
           CUmodule ptr (for ptxSourceID = 0, deviceID = 1)
           ... repeat comp->getGPUPtxCount() times ...
           CUmodule ptr (for ptxSourceID = 0, deviceID = 2)
           ... repeat comp->getGPUPtxCount() times ...
           CUmodule ptr (for ptxSourceID = 0, deviceID = 3)
           ... repeat comp->getGPUPtxCount() times ...
           CUmodule ptr (for ptxSourceID = 0, deviceID = 4)
           ... repeat comp->getGPUPtxCount() times ...
           CUmodule ptr (for ptxSourceID = 0, deviceID = 5)
           ... repeat comp->getGPUPtxCount() times ...
           CUmodule ptr (for ptxSourceID = 0, deviceID = 6)
           ... repeat comp->getGPUPtxCount() times ...
           CUmodule ptr (for ptxSourceID = 0, deviceID = 7)
           ... repeat comp->getGPUPtxCount() times ...
      */

      GpuMetaData* gpuMetaDataLocation = (GpuMetaData*)((uint8_t*)data + gpuMetaDataOffset);
      data->gpuCode = (void*)gpuMetaDataLocation;

      char* gpuMethodSignatureLocation = (char*)((uint8_t*)data + gpuMethodSignatureOffset);
      gpuMetaDataLocation->methodSignature = gpuMethodSignatureLocation;
      strcpy(gpuMethodSignatureLocation, comp->signature());

      gpuMetaDataLocation->numPtxKernels = comp->getGPUPtxCount();

      gpuMetaDataLocation->maxNumCachedDevices = maxNumCachedDevices;

      int* gpuLineNumberArrayLocation = (int*)((uint8_t*)data + gpuLineNumberArrayOffset);
      gpuMetaDataLocation->lineNumberArray = gpuLineNumberArrayLocation;
      populateGpuLineNumbers(comp, gpuLineNumberArrayLocation);

      char** gpuPtxArrayLocation = (char**)((uint8_t*)data + gpuPtxArrayOffset);
      char* gpuPtxArrayEntryLocation = (char*)((uint8_t*)data + gpuPtxArrayEntryOffset);
      gpuMetaDataLocation->ptxArray = gpuPtxArrayLocation;
      populateGpuPtx(comp, gpuPtxArrayLocation, gpuPtxArrayEntryLocation);

      void* gpuCuModuleArrayLocation = (void*)((uint8_t*)data + gpuCuModuleArrayOffset);
      gpuMetaDataLocation->cuModuleArray = gpuCuModuleArrayLocation;
      memset(gpuCuModuleArrayLocation, 0, sizeOfCUmodule * comp->getGPUPtxCount() * maxNumCachedDevices);
      }

   if (comp->getPersistentInfo()->isRuntimeInstrumentationEnabled() &&
       TR::Options::getCmdLineOptions()->getOption(TR_EnableHardwareProfileIndirectDispatch) &&
       TR::Options::getCmdLineOptions()->getOption(TR_EnableMetadataBytecodePCToIAMap))
      {
      void *bytecodePCToIAMapLocation = (void *)((uint8_t*)data + bytecodePCToIAMapOffset);
      data->riData = bytecodePCToIAMapLocation;
      }

   if (comp->getOption(TR_TraceCG) && comp->getOutFile() != NULL)
      {
      comp->getDebug()->print(data, vmMethod, fourByteOffsets);
      }

   return data;
   }
