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

#include "runtime/MethodMetaData.h"

#include <assert.h>
#include <limits.h>
#include "j9.h"
#include "jitprotos.h"
#include "j9protos.h"
#include "rommeth.h"
#include "env/jittypes.h"

#ifndef J9VM_OUT_OF_PROCESS
#define FASTWALK 1
#endif /* ndef J9VM_OUT_OF_PROCESS */

#define FASTWALK_CACHESIZE 2

static JITINLINE void initializeIterator(TR_MapIterator * i, void * methodMetaData)
   {
   i->_methodMetaData = (J9TR_MethodMetaData *)methodMetaData;
   i->_stackAtlas = (J9JITStackAtlas *) i->_methodMetaData->gcStackAtlas;
   i->_currentStackMap = NULL;
   i->_currentInlineMap = NULL;
   i->_nextMap = (U_8 *) getFirstStackMap(i->_stackAtlas);
   i->_mapIndex = 0;
   }

static JITINLINE void initializeIteratorWithSpecifiedMap(TR_MapIterator * i, J9TR_MethodMetaData * methodMetaData, U_8 * map, U_32 mapCount)
   {
   i->_methodMetaData = methodMetaData;
   i->_stackAtlas = (J9JITStackAtlas *) i->_methodMetaData->gcStackAtlas;
   i->_currentStackMap = NULL;
   i->_currentInlineMap = NULL;
   i->_nextMap = map;
   i->_mapIndex = mapCount;
   }

static JITINLINE U_8 * getNextMap(TR_MapIterator * i, UDATA fourByteOffsets)
   {
   i->_currentMap = i->_nextMap;

   if (i->_currentMap)
      {
      i->_currentInlineMap = i->_currentMap;
      if (!IS_BYTECODEINFO_MAP(fourByteOffsets, i->_currentMap))
         i->_currentStackMap = i->_currentMap;
      i->_rangeStartOffset = GET_LOW_PC_OFFSET_VALUE(fourByteOffsets, i->_currentMap);
      if (++i->_mapIndex < i->_stackAtlas->numberOfMaps)
         {
         GET_NEXT_STACK_MAP(fourByteOffsets, i->_currentMap, i->_stackAtlas, i->_nextMap);
         i->_rangeEndOffset = GET_LOW_PC_OFFSET_VALUE(fourByteOffsets, i->_nextMap) - 1;
         }
      else
         {
         i->_nextMap = 0;
         i->_rangeEndOffset = i->_methodMetaData->endPC - i->_methodMetaData->startPC - 1;
         }
      }

   return i->_currentMap;
   }

/*
 * Get the next full stack map, skipping over maps that are just inlineMaps/bytecodeinfos.
 */
static JITINLINE U_8 * getNextStackMap(TR_MapIterator * i, U_32 * mapCount, UDATA fourByteOffsets)
   {
   if (getNextMap(i, fourByteOffsets))
      { ++*mapCount; }
   while (i->_currentMap && IS_BYTECODEINFO_MAP(fourByteOffsets, i->_currentMap))
      {
      getNextMap(i, fourByteOffsets);
      ++*mapCount;
      }
   return i->_currentMap;
   }

typedef struct TR_MapTableEntry {
   UDATA _lowCodeOffset;
   UDATA _stackMapOffset;
   U_32  _mapCount;
} TR_MapTableEntry;

typedef struct TR_StackMapTable {
   U_32 _tableSize;
#ifdef FASTWALK_CACHESIZE
   /**
    * Multiple threads can update _cache[index] at the same time.
    * Need to declare as volatile to prevent thread-unsafe compiler opts.
    */
   volatile U_32 _cache[FASTWALK_CACHESIZE];
#endif /* FASTWALK_CACHESIZE */
   TR_MapTableEntry _table[1];
} TR_StackMapTable;

typedef struct TR_PersistentJittedBodyInfo {
   int32_t _dummy1; /* must be at offset 0                 */
   void *  _dummy2; /* must be at offset 4 (8 for 64bit)   */
   void *  _dummy3; /* must be at offset 8 (16 for 64bit)  */
   void *_mapTable; /* must be at offset 12 (24 for 64bit) */
} TR_PersistentJittedBodyInfo;

static const U_32 TR_StackMapTable_magicNumber = 0xABCDEFAB;

static JITINLINE TR_StackMapTable * initializeMapTable(J9JavaVM * javaVM, J9TR_MethodMetaData * metaData, UDATA fourByteOffsets)
   {
   /* How big should the table be before we give up on linear search? */
   const U_32 threshold = 6;

   J9JITStackAtlas * stackAtlas = 0;
   U_8 * addressOfFirstMap = 0;
   U_32 mapCount = 0;
   U_32 concreteMapCount = 0;
   TR_StackMapTable * mapTable = 0;
   TR_MapIterator i;

   assert(metaData);
   stackAtlas = (J9JITStackAtlas *)metaData->gcStackAtlas;
   assert(stackAtlas);

   initializeIterator(&i, metaData);
   while(getNextStackMap(&i, &mapCount, fourByteOffsets))
      { ++concreteMapCount; }

   if (i._stackAtlas->numberOfMaps > threshold)
      {
      PORT_ACCESS_FROM_JAVAVM(javaVM);
      mapTable = (TR_StackMapTable *) j9mem_allocate_memory(concreteMapCount * sizeof(TR_MapTableEntry) + sizeof(TR_StackMapTable), J9MEM_CATEGORY_JIT);

      if (mapTable)
         {
         size_t index = 0;

         mapTable->_tableSize = concreteMapCount;

#ifdef FASTWALK_CACHESIZE
         for (index = 0; index < FASTWALK_CACHESIZE; ++index)
            { mapTable->_cache[index] = 0; }
#endif /* FASTWALK_CACHESIZE */

         initializeIterator(&i, metaData);
         addressOfFirstMap = i._nextMap;

         mapCount = 0;
         for (index = 0; getNextStackMap(&i, &mapCount, fourByteOffsets); ++index)
            {
            mapTable->_table[index]._lowCodeOffset = i._rangeStartOffset;
            mapTable->_table[index]._stackMapOffset = (UDATA) (i._currentMap - addressOfFirstMap);
            mapTable->_table[index]._mapCount = mapCount - 1;
            }
         assert(index == concreteMapCount);
         assert(index == mapTable->_tableSize);

         mapTable->_table[index]._lowCodeOffset = -1;
         mapTable->_table[index]._stackMapOffset = 0;
         mapTable->_table[index]._mapCount = TR_StackMapTable_magicNumber;
         assert(mapTable->_table[index]._mapCount == TR_StackMapTable_magicNumber);
         FLUSH_MEMORY(1); /* Usually we'd use isSMP but that's a C++ function not available here */
         ((TR_PersistentJittedBodyInfo *)metaData->bodyInfo)->_mapTable = mapTable;
         }
      }

   return mapTable;
   }

static JITINLINE TR_StackMapTable * findOrCreateMapTable(J9JavaVM * javaVM, J9TR_MethodMetaData * metaData, UDATA fourByteOffsets)
   {
   TR_StackMapTable * mapTablePtr = 0;
   assert(metaData);

   if (metaData->bodyInfo &&
       (javaVM->phase == J9VM_PHASE_NOT_STARTUP || 0 == (javaVM->jitConfig->runtimeFlags & J9JIT_QUICKSTART))) // save footprint during startup in Xquickstart mode
      {
      mapTablePtr = ((TR_PersistentJittedBodyInfo *)metaData->bodyInfo)->_mapTable; /* cache it */
      if (mapTablePtr == (TR_StackMapTable *)-1) /* if nobody wrote to it yet */
        { mapTablePtr = initializeMapTable(javaVM, metaData, fourByteOffsets); }
#if defined(TR_HOST_64BIT)
      if (((U_32)((UDATA)mapTablePtr) == (U_32)-1) || ((U_32)((UDATA)mapTablePtr >> 32) == (U_32)-1)) /* check upper and lower word */
         { mapTablePtr = 0; } /* give up the optimization */
#endif
      }

   return mapTablePtr;
   }

static JITINLINE void * currentInlineMap(TR_MapIterator * i)
   {
   return i->_currentInlineMap;
   }

static JITINLINE void * currentStackMap(TR_MapIterator * i)
   {
   return i->_currentStackMap;
   }

static U_32 matchingRange(TR_MapIterator * i, UDATA offset)
   {
   return i->_rangeStartOffset <= offset && offset <= i->_rangeEndOffset;
   }

static JITINLINE void findMapsAtPC(TR_MapIterator * i, UDATA offsetPC, void * * stackMap, void * * inlineMap, UDATA fourByteOffsets)
   {
   while (getNextMap(i, fourByteOffsets))
      {
      if (i->_rangeStartOffset <= offsetPC && offsetPC <= i->_rangeEndOffset)
         {
         *stackMap = currentStackMap(i);
         *inlineMap = currentInlineMap(i);
         break;
         }
      }
   }

static JITINLINE int compareMapTableEntries(UDATA key, TR_MapTableEntry * ent)
   {
   /* Position (ent+1) is always valid, because there's an extra entry at
    * the end of the table that isn't part of this search.
    */
   if (key < ent->_lowCodeOffset)  return -1;
   if (key >= (ent + 1)->_lowCodeOffset) return 1;
   return 0;
   }

static JITINLINE void * mapSearch(UDATA key, TR_MapTableEntry * base, size_t num)
   {
   size_t delta = OMR_MAX(num / 2, 1);
   size_t pos = delta;
   while(1)
      {
      TR_MapTableEntry * pivot = base + pos;
      delta = OMR_MAX(delta / 2, 1);
      switch (compareMapTableEntries(key, pivot))
         {
         case -1:
            pos -= delta;
            if (pos == 0)
               return base; /* Don't walk past the first entry */
            break;
         case 0:
            return pivot;
            break;
         case 1:
            pos += delta;
            if (pos >= num)
               return base + num - 1; /* Don't walk past the last entry */
            break;
         default:
            assert(0);
            break;
         }
      }
   assert(0);

   /* We never reach this statement, but we need it to squelch compiler warnings: */
   return 0;
   }

#if defined(DEBUG)
static void printMetaData(J9TR_MethodMetaData * methodMetaData)
   {
   int index = 0;
   TR_MapIterator iter;
   UDATA fourByteOffsets = HAS_FOUR_BYTE_OFFSET(methodMetaData);

   if (methodMetaData->flags & JIT_METADATA_IS_STUB)
      {
      printf("Stub Metadata 0x%p, nothing to dump\n", methodMetaData);
      return;
      }
   
   printf("Metadata dump:\n");
   initializeIterator(&iter, methodMetaData);
   for (index = 0; index < iter._stackAtlas->numberOfMaps; ++index)
      {
      getNextMap(&iter, fourByteOffsets);
      printf("index: %d map address: %p rangeStartOffset: %x rangeEndOffset: %x is inlineMap? %s\n", index, iter._currentMap, iter._rangeStartOffset, iter._rangeEndOffset, IS_BYTECODEINFO_MAP(fourByteOffsets, iter._currentMap) ? "yes" : "no");
      }
   }

static void printMapTable(TR_StackMapTable * stackMapTable, U_8 * addressOfFirstMap)
   {
   int index = 0;

   printf("\nMapTable dump:\n");
   printf("MapTable size is: %lu\n", stackMapTable->_tableSize);
   for (index = 0; index <= stackMapTable->_tableSize; ++index)
      {
      printf("index: %d map address: %p lowCodeOffset: %x stackMapOffset: %x mapCount: %u\n", index, stackMapTable->_table[index]._stackMapOffset + addressOfFirstMap, stackMapTable->_table[index]._lowCodeOffset, stackMapTable->_table[index]._stackMapOffset, stackMapTable->_table[index]._mapCount);
      }
   }

static void fastwalkDebug(J9TR_MethodMetaData * methodMetaData, UDATA offsetPC, TR_StackMapTable * stackMapTable, TR_MapTableEntry * mapTableEntry)
   {
   if (methodMetaData && (methodMetaData->flags & JIT_METADATA_IS_STUB))
      printf("Stub Metadata 0x%p, nothing to dump\n", methodMetaData);
   else
      {
      UDATA fourByteOffsets = HAS_FOUR_BYTE_OFFSET(methodMetaData);
      J9JITStackAtlas * stackAtlas = (J9JITStackAtlas *) methodMetaData->gcStackAtlas;
      void * stackMap1 = 0;
      void * inlineMap1 = 0;
      void * stackMap2 = 0;
      void * inlineMap2 = 0;
      TR_MapIterator iter1, iter2;
      U_8 * addressOfFirstMap = 0;

      /* Find the stack map and the inline map using a linear search. */
      initializeIterator(&iter1, methodMetaData);
      addressOfFirstMap = iter1._nextMap;
      findMapsAtPC(&iter1, offsetPC, &stackMap1, &inlineMap1, fourByteOffsets);

      /* Find the stack map and the inline map using the fastwalk method. */
      initializeIteratorWithSpecifiedMap(&iter2, methodMetaData, (U_8 *) (getFirstStackMap(stackAtlas) + mapTableEntry->_stackMapOffset), mapTableEntry->_mapCount);
      findMapsAtPC(&iter2, offsetPC, &stackMap2, &inlineMap2, fourByteOffsets);

      /* Ensure the maps found by each method are the same. */
      if ((stackMap1 != stackMap2) || (inlineMap1 != inlineMap2))
         {
         printf("FASTWALK DEBUG:\n");
         printf("stackMap found by linear walk is %p\n", stackMap1);
         printf("stackMap found by fastwalk method is %p\n", stackMap2);
         printf("inlineMap found by linear walk is %p\n", inlineMap1);
         printf("inlineMap found by fastwalk method is %p\n", inlineMap2);

         printMetaData(methodMetaData);
         printMapTable(stackMapTable, addressOfFirstMap);

         assert(stackMap1 == stackMap2);
         assert(inlineMap1 == inlineMap2);
         }
      }
   }
#endif /* defined(DEBUG) */

void jitGetMapsFromPC(J9JavaVM * javaVM, J9TR_MethodMetaData * methodMetaData, UDATA jitPC, void * * stackMap, void * * inlineMap)
   {
   TR_MapIterator i;
   TR_StackMapTable * stackMapTable = 0;

   /* We subtract one from the jitPC as jitPCs that come in are always pointing at
    * the beginning of the next instruction (ie: the return address from the child call).
    * In the case of trap handlers, the GP handler has already bumped the PC forward by
    * one, expecting this -1 to happen.
    *
    * TODO: Stack maps should just have the right PC values in them in the first
    * place -- namely the ones you find when you do a stack walk -- eliminating the
    * need to lie about the PC here.
    */
   UDATA offsetPC = jitPC - methodMetaData->startPC - 1;

   UDATA fourByteOffsets = HAS_FOUR_BYTE_OFFSET(methodMetaData);

   J9JITStackAtlas * stackAtlas = (J9JITStackAtlas *) methodMetaData->gcStackAtlas;

   *stackMap = 0;
   *inlineMap = 0;

   if (methodMetaData->flags & JIT_METADATA_IS_STUB)
      return;
   
   if (!stackAtlas)
      return;

#ifdef FASTWALK

   stackMapTable = findOrCreateMapTable(javaVM, methodMetaData, fourByteOffsets);

   if (stackMapTable)
      {
      TR_MapTableEntry * mapTableEntry = 0;
      int index = 0;

      assert(stackMapTable->_table);
      assert(stackMapTable->_tableSize > 0);
      assert(stackMapTable->_table[stackMapTable->_tableSize]._mapCount == TR_StackMapTable_magicNumber);

#if (FASTWALK_CACHESIZE > 0)
      for (; index < FASTWALK_CACHESIZE; ++index)
         {
         TR_MapTableEntry * cachedEntry = &stackMapTable->_table[stackMapTable->_cache[index]];

         assert(cachedEntry);

         if (!compareMapTableEntries(offsetPC, cachedEntry))
            {
            initializeIteratorWithSpecifiedMap(&i, methodMetaData, (U_8 *) (getFirstStackMap(stackAtlas) + cachedEntry->_stackMapOffset), cachedEntry->_mapCount);

            findMapsAtPC(&i, offsetPC, stackMap, inlineMap, fourByteOffsets);

#if (FASTWALK_CACHESIZE > 1)
            /*
             * Move the contents of the most-recently hit cache position to
             * _cache[0]
             */
            if (index != 0)
               {
               U_32 tmp = stackMapTable->_cache[index];
               stackMapTable->_cache[index] = stackMapTable->_cache[0];
               stackMapTable->_cache[0] = tmp;
               }
#endif /* (FASTWALK_CACHESIZE > 1) */
            return;
            }
         }
#endif /* (FASTWALK_CACHESIZE > 0) */

      mapTableEntry = mapSearch(offsetPC, stackMapTable->_table, stackMapTable->_tableSize - 1);

      assert(mapTableEntry);
      assert(mapTableEntry >= stackMapTable->_table);

      initializeIteratorWithSpecifiedMap(&i, methodMetaData, (U_8 *) (getFirstStackMap(stackAtlas) + mapTableEntry->_stackMapOffset), mapTableEntry->_mapCount);

#if (FASTWALK_CACHESIZE > 1)
      /*
       * Make space in _cache[0].
       */
      for (index = FASTWALK_CACHESIZE - 1; index; --index)
         { stackMapTable->_cache[index] = stackMapTable->_cache[index - 1]; }
#endif /* (FASTWALK_CACHESIZE > 1) */

#ifdef FASTWALK_CACHESIZE
      /*
       * This stackmap isn't in the cache, so add it.
       */
      stackMapTable->_cache[0] = (U_32) (mapTableEntry - stackMapTable->_table);
#endif /* def FASTWALK_CACHESIZE */

#if defined(DEBUG)
      fastwalkDebug(methodMetaData, offsetPC, stackMapTable, mapTableEntry);
#endif /* defined(DEBUG) */

      }
   else
#endif /* def FASTWALK */
      {
      initializeIterator(&i, methodMetaData);
      }

   /* try to speed the loop up by by versioning it based on the value of 'fourByteOffsets'. Assume that the compiler
    * will inline the call and do constant propagation
    */

   findMapsAtPC(&i, offsetPC, stackMap, inlineMap, fourByteOffsets);

   if (!*stackMap)
      {
      /* OH OH!  That hack above to subtract one from the PC seems to have
       * caused us to miss the proper GC map.  try again with the original PC.
       */
      initializeIterator(&i, methodMetaData);
      findMapsAtPC(&i, offsetPC+1, stackMap, inlineMap, fourByteOffsets);
      }
   }

void * jitGetInlinerMapFromPC(J9JavaVM * javaVM, J9TR_MethodMetaData * methodMetaData, UDATA jitPC)
   {
   void * stackMap, * inlineMap;
   jitGetMapsFromPC(javaVM, methodMetaData, jitPC, &stackMap, &inlineMap);
   return inlineMap;
   }

void * getStackMapFromJitPC(J9JavaVM * javaVM, J9TR_MethodMetaData * methodMetaData, UDATA jitPC)
   {
   void * stackMap, * inlineMap;
   jitGetMapsFromPC(javaVM, methodMetaData, jitPC, &stackMap, &inlineMap);
   return stackMap;
   }



void * getStackAllocMapFromJitPC(J9JavaVM * javaVM, J9TR_MethodMetaData * methodMetaData, UDATA jitPC, void *curStackMap)
   {
   void * stackMap, ** stackAllocMap;

   if (!methodMetaData->gcStackAtlas)
      return NULL;

   if (curStackMap)
      stackMap = curStackMap;
   else
      stackMap = getStackMapFromJitPC(javaVM, methodMetaData, jitPC);

   stackAllocMap = (void **)((J9JITStackAtlas *) methodMetaData->gcStackAtlas)->stackAllocMap;
   if (stackAllocMap)
      {
      uintptrj_t returnValue;

      if (*((uint8_t **) stackAllocMap) == ((uint8_t *) stackMap))
         return NULL;

      returnValue = ((uintptrj_t) stackAllocMap) + sizeof(uintptrj_t);

      return ((void *) returnValue);
      }

   return NULL;
   }



static U_32 sizeOfInlinedCallSiteArrayElement(J9JITExceptionTable * methodMetaData)
   {
   return sizeof(TR_InlinedCallSite) + ((J9JITStackAtlas *)methodMetaData->gcStackAtlas)->numberOfMapBytes;
   }


U_8 * getInlinedCallSiteArrayElement(J9TR_MethodMetaData * methodMetaData, int cix)
   {
   U_8 * inlinedCallSiteArray = getJitInlinedCallInfo(methodMetaData);
   if (inlinedCallSiteArray)
      return inlinedCallSiteArray + (cix * sizeOfInlinedCallSiteArrayElement(methodMetaData));

   return NULL;
   }



U_8 isUnloadedInlinedMethod(J9Method *m)
   {

#if (defined(TR_HOST_POWER) || defined(TR_HOST_ARM))
   if (((UDATA) m & 0x1))
#else
   if (~((UDATA)m) == 0) /* d142150: check for m==-1 in a way that works on ZOS */
#endif
      return 1;

   return 0;

   }



U_32 isPatchedValue(J9Method *m)
   {
#if (defined(TR_HOST_POWER) || defined(TR_HOST_ARM))
   if (((UDATA) m & 0x1))
#else
   if (~((UDATA)m) == 0) /* d142150: check for m==-1 in a way that works on ZOS */
#endif
      return 1;

   return 0;
   }



void markClassesInInlineRanges(void * methodMetaData, J9StackWalkState * walkState)
   {
   J9Method * savedMethod = walkState->method;
   J9ConstantPool * savedCP = walkState->constantPool;
   U_32 numCallSites = getNumInlinedCallSites(methodMetaData);
   U_32 i = 0;
   for (i=0; i<numCallSites; i++)
      {
      U_8 *inlinedCallSite = getInlinedCallSiteArrayElement(methodMetaData, i);
      J9Method *inlinedMethod = getInlinedMethod(inlinedCallSite);
      if (!isPatchedValue(inlinedMethod))
         {
         walkState->method = READ_METHOD(inlinedMethod);
         walkState->constantPool = UNTAGGED_METHOD_CP(walkState->method);
         /*
         walkState->bytecodePCOffset = (IDATA) getCurrentByteCodeIndexAndIsSameReceiver(walkState->jitInfo, inlineMap, inlinedCallSite, NULL);
#ifdef J9VM_INTERP_STACKWALK_TRACING
         jitPrintFrameType(walkState, "JIT inline");
#endif
         */
         WALK_METHOD_CLASS(walkState);
         }
      }

   walkState->method = savedMethod;
   walkState->constantPool = savedCP;
   }




static void setInlineRangeEndOffset(TR_MapIterator * i, I_32 callerIndex, UDATA * endOffset)
   {
   UDATA fourByteOffsets = HAS_FOUR_BYTE_OFFSET(i->_methodMetaData);

   while (getNextMap(i, fourByteOffsets))
      {
      *endOffset = i->_rangeEndOffset;

      if (!i->_nextMap || ((TR_ByteCodeInfo *)getByteCodeInfoFromStackMap(i->_methodMetaData, i->_nextMap))->_callerIndex != callerIndex)
         break;
      }
   }

void * getFirstInlineRange(TR_MapIterator * i, void * methodMetaData, UDATA * startOffset, UDATA * endOffset)
   {
   initializeIterator(i, (J9TR_MethodMetaData *)methodMetaData);
   if (!i->_nextMap)
      return NULL;

   *startOffset = 0;

   setInlineRangeEndOffset(i, -1, endOffset);

   return i->_currentInlineMap;
   }

void * getNextInlineRange(TR_MapIterator * i, UDATA * startOffset, UDATA * endOffset)
   {
   I_32 callerIndex;
   if (!i->_nextMap)
      return 0;

   *startOffset = i->_rangeEndOffset + 1;

   callerIndex = ((TR_ByteCodeInfo *)getByteCodeInfoFromStackMap(i->_methodMetaData, i->_nextMap))->_callerIndex;

   setInlineRangeEndOffset(i, callerIndex, endOffset);

   return i->_currentInlineMap;
   }

JITINLINE static J9JIT32BitExceptionTableEntry * getNext32BitExceptionDataField(J9JIT32BitExceptionTableEntry * handlerCursor, UDATA bytecodePCBytes)
   {
   return (J9JIT32BitExceptionTableEntry *) (((U_8 *) (handlerCursor + 1)) + bytecodePCBytes);
   }


JITINLINE static J9JIT16BitExceptionTableEntry * getNext16BitExceptionDataField(J9JIT16BitExceptionTableEntry * handlerCursor, UDATA bytecodePCBytes)
   {
   return (J9JIT16BitExceptionTableEntry *) (((U_8 *) (handlerCursor + 1)) + bytecodePCBytes);
   }

/*  Hash table for jit exception handler, record an entry to state that for current pc position,
 *  there is no handler for the thrownClass
 */



/*Below is a mirror struct with the one in HookHelpers.cpp.
 *Please apply to any modification to both methods.
 */

#define JIT_EXCEPTION_HANDLER_CACHE_SIZE 256
typedef struct TR_jitExceptionHandlerCache
   {
    UDATA pc;
    J9Class * thrownClass;
   } TR_jitExceptionHandlerCache;



#define JIT_EXCEPTION_HANDLER_CACHE_DIMENSION 8
#ifdef TR_TARGET_64BIT
#define JIT_EXCEPTION_HANDLER_CACHE_HASH_VALUE 17446744073709553729U
#define BIT_IN_INTERGER 64
#else
#define JIT_EXCEPTION_HANDLER_CACHE_HASH_VALUE 4102541685U
#define BIT_IN_INTERGER 32
#endif

#define JIT_EXCEPTION_HANDLER_CACHE_HASH_RESULT(key) \
        ((key*JIT_EXCEPTION_HANDLER_CACHE_HASH_VALUE) >> (BIT_IN_INTERGER-JIT_EXCEPTION_HANDLER_CACHE_DIMENSION))


void JITINLINE setJitExceptionHandlerCache(TR_jitExceptionHandlerCache* jitExceptionHandlerCache, UDATA pc, J9Class * thrownClass)
   {
    jitExceptionHandlerCache[JIT_EXCEPTION_HANDLER_CACHE_HASH_RESULT(pc)].pc=pc;
    jitExceptionHandlerCache[JIT_EXCEPTION_HANDLER_CACHE_HASH_RESULT(pc)].thrownClass=thrownClass;
    return;
   }

JITINLINE TR_jitExceptionHandlerCache getJitExceptionHandlerCache(TR_jitExceptionHandlerCache* jitExceptionHandlerCache, UDATA pc)
   {
   return jitExceptionHandlerCache[JIT_EXCEPTION_HANDLER_CACHE_HASH_RESULT(pc)];
   }

UDATA jitExceptionHandlerSearch(J9VMThread * currentThread, J9StackWalkState * walkState)
   {
   /* Helper function, called for each JIT stack frame.  Assumptions:
    * 1) current thread and walk state are passed as parameters.
    * 2) walkState->userData1 is a flags field that controls the walk (input)
    * 3) walkState->userData2 is the target PC (output)
    * 4) walkState->userData3 is a cookie describing the frame type (output)
    * 5) walkState->userData4 is the thrown class (input)
    */

      UDATA numberOfRanges;
      TR_jitExceptionHandlerCache * searchCache=NULL;
      TR_jitExceptionHandlerCache cacheResult;
      searchCache=(TR_jitExceptionHandlerCache *)(currentThread->jitExceptionHandlerCache);
      if(searchCache)
         {
        cacheResult = getJitExceptionHandlerCache(searchCache,(UDATA) walkState->pc);
         if (cacheResult.pc == (UDATA) walkState->pc && cacheResult.thrownClass == (J9Class *)walkState->userData4)
            {
            return J9_STACKWALK_KEEP_ITERATING;
            }
         }
      else
         {
         PORT_ACCESS_FROM_JAVAVM(currentThread->javaVM);
         currentThread->jitExceptionHandlerCache  =
            j9mem_allocate_memory(JIT_EXCEPTION_HANDLER_CACHE_SIZE * sizeof (TR_jitExceptionHandlerCache),J9MEM_CATEGORY_VM);
         if(currentThread->jitExceptionHandlerCache)
            {
            searchCache=(TR_jitExceptionHandlerCache *)(currentThread->jitExceptionHandlerCache);
            memset((TR_jitExceptionHandlerCache *)(currentThread->jitExceptionHandlerCache),0,
               JIT_EXCEPTION_HANDLER_CACHE_SIZE * sizeof (TR_jitExceptionHandlerCache));
            }
         }
   numberOfRanges = getJitNumberOfExceptionRanges(walkState->jitInfo);

   if (numberOfRanges)
      {
      UDATA bytecodePCBytes = (hasBytecodePC(walkState->jitInfo)) ? sizeof(U_32) : 0;
      UDATA deltaPC;
      J9Class * thrownClass = (J9Class *) walkState->userData4;
      UDATA (* isExceptionTypeCaughtByHandler) (J9VMThread *, J9Class *, J9ConstantPool *, UDATA, J9StackWalkState *) =
         walkState->walkThread->javaVM->internalVMFunctions->isExceptionTypeCaughtByHandler;

      /* we subtract one from the jitPC as jitPCs that come in are always pointing at the beginning of the next instruction
       * (ie: the return address from the child call).  In the case of trap handlers, the GP handler has already bumped the PC
       * forward by one, expecting this -1 to happen.
       */

      deltaPC = ((UDATA) walkState->pc) - getJittedMethodStartPC(walkState->jitInfo) - 1;

      if (hasWideExceptions(walkState->jitInfo))
         {
         J9JIT32BitExceptionTableEntry * handlerCursor = get32BitFirstExceptionDataField(walkState->jitInfo);

         numberOfRanges &= ~(J9_JIT_METADATA_WIDE_EXCEPTIONS | J9_JIT_METADATA_HAS_BYTECODE_PC);
         while (numberOfRanges)
            {
            if ((deltaPC >= getJit32BitTableEntryStartPC(handlerCursor)) && (deltaPC < getJit32BitTableEntryEndPC(handlerCursor)))
               {
               if (
                  /* { RTSJ Support begins */
                   isExceptionTypeCaughtByHandler(walkState->walkThread, (J9Class *)walkState->userData4, UNTAGGED_METHOD_CP(handlerCursor->ramMethod), handlerCursor->catchType, walkState))
                  /* } RTSJ Support ends */
                  {
                  if (bytecodePCBytes)
                     {
                     walkState->userData1 = (void *)(intptr_t)*get32BitByteCodeIndexFromExceptionTable(walkState->jitInfo);
                     }
                  walkState->userData2 = (void *) (getJittedMethodStartPC(walkState->jitInfo) + getJit32BitTableEntryHandlerPC(handlerCursor));
                  walkState->restartPoint = walkState->walkThread->javaVM->jitConfig->runJITHandler;
                  walkState->userData3 = (void *) J9_EXCEPT_SEARCH_JIT_HANDLER;
                  return J9_STACKWALK_STOP_ITERATING;
                  }
               }
            handlerCursor = getNext32BitExceptionDataField(handlerCursor, bytecodePCBytes);
            --numberOfRanges;
            }
         }
      else
         {
         J9JIT16BitExceptionTableEntry * handlerCursor =  get16BitFirstExceptionDataField(walkState->jitInfo);
         numberOfRanges &= ~(J9_JIT_METADATA_WIDE_EXCEPTIONS | J9_JIT_METADATA_HAS_BYTECODE_PC);
         while (numberOfRanges)
            {
            if ((deltaPC >= getJit16BitTableEntryStartPC(handlerCursor)) && (deltaPC < getJit16BitTableEntryEndPC(handlerCursor)))
               {
               if (
                   isExceptionTypeCaughtByHandler(walkState->walkThread, (J9Class *)walkState->userData4, walkState->constantPool, handlerCursor->catchType, walkState))
                  /* } RTSJ Support ends */
                  {
                  if (bytecodePCBytes)
                     {
                     walkState->userData1 = (void *)(intptr_t)*get16BitByteCodeIndexFromExceptionTable(walkState->jitInfo);
                     }
                  walkState->userData2 = (void *) (getJittedMethodStartPC(walkState->jitInfo) + getJit16BitTableEntryHandlerPC(handlerCursor));
                  walkState->restartPoint = walkState->walkThread->javaVM->jitConfig->runJITHandler;
                  walkState->userData3 = (void *) J9_EXCEPT_SEARCH_JIT_HANDLER;
                  return J9_STACKWALK_STOP_ITERATING;
                  }
               }
            handlerCursor = getNext16BitExceptionDataField(handlerCursor, bytecodePCBytes);
            --numberOfRanges;
            }
         }
      }
   if(searchCache && !currentThread->javaVM->jitConfig->fsdEnabled)
      setJitExceptionHandlerCache(searchCache, (UDATA) walkState->pc, (J9Class *)walkState->userData4);
   return J9_STACKWALK_KEEP_ITERATING;
   }

JITINLINE J9JIT32BitExceptionTableEntry * get32BitFirstExceptionDataField(J9TR_MethodMetaData * methodMetaData)
   {
   return (J9JIT32BitExceptionTableEntry *) (methodMetaData + 1);
   }

JITINLINE J9JIT16BitExceptionTableEntry * get16BitFirstExceptionDataField(J9TR_MethodMetaData * methodMetaData)
   {
   return (J9JIT16BitExceptionTableEntry *) (methodMetaData + 1);
   }

U_8 * getJit32BitInterpreterPC(U_8 * bytecodes, J9JIT32BitExceptionTableEntry * handlerCursor)
   {
   return bytecodes + *((U_32 *) (handlerCursor + 1));
   }

JITINLINE U_8 * getJit16BitInterpreterPC(U_8 * bytecodes, J9JIT16BitExceptionTableEntry * handlerCursor)
   {
   return bytecodes + *((U_32 *) (handlerCursor + 1));
   }

JITINLINE J9JIT32BitExceptionTableEntry * get32BitNextExceptionTableEntry(J9JIT32BitExceptionTableEntry * handlerCursor)
   {
   return (J9JIT32BitExceptionTableEntry *) (((U_8 *) (handlerCursor + 1)) + sizeof(U_32));
   }

JITINLINE J9JIT32BitExceptionTableEntry * get32BitNextExceptionTableEntryFSD(J9JIT32BitExceptionTableEntry * handlerCursor, UDATA fullSpeedDebug)
   {
   if (fullSpeedDebug)
      {
      return (J9JIT32BitExceptionTableEntry *) (((U_8 *) (handlerCursor + 1)) + sizeof(U_32));
      }

   return (J9JIT32BitExceptionTableEntry *) (((U_8 *) (handlerCursor + 1)));
   }

JITINLINE J9JIT16BitExceptionTableEntry * get16BitNextExceptionTableEntry(J9JIT16BitExceptionTableEntry * handlerCursor)
   {
   return (J9JIT16BitExceptionTableEntry *) (((U_8 *) (handlerCursor + 1)) + sizeof(U_32));
   }

JITINLINE J9JIT16BitExceptionTableEntry * get16BitNextExceptionTableEntryFSD(J9JIT16BitExceptionTableEntry * handlerCursor, int fullSpeedDebug)
   {
   if (fullSpeedDebug)
      {
      return (J9JIT16BitExceptionTableEntry *) (((U_8 *) (handlerCursor + 1)) + sizeof(U_32));
      }
   return (J9JIT16BitExceptionTableEntry *) (((U_8 *) (handlerCursor + 1)));
   }


void aotFixEndian(UDATA j9dst_priv)
   {
   UDATA j9src_priv;
   j9src_priv = (UDATA)j9dst_priv;
   ((U_8 *)&j9dst_priv)[0] = ((U_8 *)&j9src_priv)[3];
   ((U_8 *)&j9dst_priv)[1] = ((U_8 *)&j9src_priv)[2];
   ((U_8 *)&j9dst_priv)[2] = ((U_8 *)&j9src_priv)[1];
   ((U_8 *)&j9dst_priv)[3] = ((U_8 *)&j9src_priv)[0];
   }

void aotWideExceptionEntriesFixEndian(J9JITExceptionTable * methodMetaData)
   {
#if defined(TR_HOST_POWER) && defined(J9VM_ENV_LITTLE_ENDIAN)
#define J9_AOT_FIX_ENDIAN(j9dst_priv) \
       { \
       UDATA j9src_priv; \
       j9src_priv = (UDATA)j9dst_priv; \
       ((U_8 *)&j9dst_priv)[0] = ((U_8 *)&j9src_priv)[3]; \
       ((U_8 *)&j9dst_priv)[1] = ((U_8 *)&j9src_priv)[2]; \
       ((U_8 *)&j9dst_priv)[2] = ((U_8 *)&j9src_priv)[1]; \
       ((U_8 *)&j9dst_priv)[3] = ((U_8 *)&j9src_priv)[0]; \
       }
#define J9_AOT_FIX_ENDIAN_INDIRECT(j9dst_priv) \
       { \
       UDATA j9src_priv; \
       j9src_priv = *(UDATA *)j9dst_priv; \
       ((U_8 *)j9dst_priv)[0] = ((U_8 *)&j9src_priv)[3]; \
       ((U_8 *)j9dst_priv)[1] = ((U_8 *)&j9src_priv)[2]; \
       ((U_8 *)j9dst_priv)[2] = ((U_8 *)&j9src_priv)[1]; \
       ((U_8 *)j9dst_priv)[3] = ((U_8 *)&j9src_priv)[0]; \
       }

    UDATA numExcptionRanges = ((UDATA)methodMetaData->numExcptionRanges) & ~(J9_JIT_METADATA_WIDE_EXCEPTIONS | J9_JIT_METADATA_HAS_BYTECODE_PC);
    J9JIT32BitExceptionTableEntry *excptEntry32 = get32BitFirstExceptionDataField(methodMetaData);

    while(numExcptionRanges > 0)
       {
       UDATA hasBytecodePC;
       J9_AOT_FIX_ENDIAN(excptEntry32->startPC)
       J9_AOT_FIX_ENDIAN(excptEntry32->endPC)
       J9_AOT_FIX_ENDIAN(excptEntry32->handlerPC)
       J9_AOT_FIX_ENDIAN(excptEntry32->catchType)
       J9_AOT_FIX_ENDIAN(excptEntry32->ramMethod)
       hasBytecodePC = ((UDATA)methodMetaData->numExcptionRanges) & J9_JIT_METADATA_HAS_BYTECODE_PC;
       if (hasBytecodePC)/*((UDATA)methodMetaData->numExcptionRanges) & J9_JIT_METADATA_HAS_BYTECODE_PC )*/
          {
          J9_AOT_FIX_ENDIAN_INDIRECT(get32BitByteCodeIndexFromExceptionTable(methodMetaData))/*excptEntry32)*/
  /*        excptEntry32 = (J9JIT32BitExceptionTableEntry *) (((U_8 *) excptEntry32) + sizeof(U_32));*/
          }
       excptEntry32 = get32BitNextExceptionTableEntryFSD(excptEntry32, hasBytecodePC);
       numExcptionRanges--;
       }
#undef J9_AOT_FIX_ENDIAN
#undef J9_AOT_FIX_ENDIAN_INDIRECT
#endif /*(TR_HOST_POWER) && defined(J9VM_ENV_LITTLE_ENDIAN)*/
   }

void aot2ByteExceptionEntriesFixEndian(J9JITExceptionTable * methodMetaData)
    {
#if defined(TR_HOST_POWER) && defined(J9VM_ENV_LITTLE_ENDIAN)
#define J9_AOT_FIX_ENDIAN_HALF(j9dst_priv) \
       { \
       UDATA j9src_priv; \
       j9src_priv = (UDATA)j9dst_priv; \
       ((U_8 *)&j9dst_priv)[0] = ((U_8 *)&j9src_priv)[1]; \
       ((U_8 *)&j9dst_priv)[1] = ((U_8 *)&j9src_priv)[0]; \
       }

#define J9_AOT_FIX_ENDIAN_INDIRECT(j9dst_priv) \
       { \
       UDATA j9src_priv; \
       j9src_priv = *(UDATA *)j9dst_priv; \
       ((U_8 *)j9dst_priv)[0] = ((U_8 *)&j9src_priv)[3]; \
       ((U_8 *)j9dst_priv)[1] = ((U_8 *)&j9src_priv)[2]; \
       ((U_8 *)j9dst_priv)[2] = ((U_8 *)&j9src_priv)[1]; \
       ((U_8 *)j9dst_priv)[3] = ((U_8 *)&j9src_priv)[0]; \
       }
    UDATA numExcptionRanges = ((UDATA)methodMetaData->numExcptionRanges) & ~(J9_JIT_METADATA_WIDE_EXCEPTIONS | J9_JIT_METADATA_HAS_BYTECODE_PC);
    J9JIT16BitExceptionTableEntry *excptEntry16 = get16BitFirstExceptionDataField(methodMetaData); /*(J9JIT16BitExceptionTableEntry *)(methodMetaData + 1);*/


    while(numExcptionRanges > 0)
       {
       UDATA hasBytecodePC;
       J9_AOT_FIX_ENDIAN_HALF(excptEntry16->startPC)
       J9_AOT_FIX_ENDIAN_HALF(excptEntry16->endPC)
       J9_AOT_FIX_ENDIAN_HALF(excptEntry16->handlerPC)
       J9_AOT_FIX_ENDIAN_HALF(excptEntry16->catchType)
/*     ++excptEntry16;*/
       hasBytecodePC = ((UDATA)methodMetaData->numExcptionRanges) & J9_JIT_METADATA_HAS_BYTECODE_PC;
       if (hasBytecodePC) /*((UDATA)methodMetaData->numExcptionRanges) & J9_JIT_METADATA_HAS_BYTECODE_PC ) */
          {
          J9_AOT_FIX_ENDIAN_INDIRECT(get16BitByteCodeIndexFromExceptionTable(methodMetaData));/*excptEntry16);*/
/*         excptEntry16 = (J9JIT16BitExceptionTableEntry *) (((U_8 *) excptEntry16) + sizeof(U_32));*/
          }
       excptEntry16 = get16BitNextExceptionTableEntryFSD(excptEntry16, hasBytecodePC);
       numExcptionRanges--;
       }
#undef J9_AOT_FIX_ENDIAN_HALF
#undef J9_AOT_FIX_ENDIAN_INDIRECT
#endif /*(TR_HOST_POWER) && defined(J9VM_ENV_LITTLE_ENDIAN)*/
    }

void aotExceptionEntryFixEndian(J9JITExceptionTable * methodMetaData)
   {
#define J9_AOT_FIX_ENDIAN(j9dst_priv) \
    { \
        UDATA j9src_priv; \
        j9src_priv = (UDATA)j9dst_priv; \
        ((U_8 *)&j9dst_priv)[0] = ((U_8 *)&j9src_priv)[3]; \
        ((U_8 *)&j9dst_priv)[1] = ((U_8 *)&j9src_priv)[2]; \
        ((U_8 *)&j9dst_priv)[2] = ((U_8 *)&j9src_priv)[1]; \
        ((U_8 *)&j9dst_priv)[3] = ((U_8 *)&j9src_priv)[0]; \
    }
#define J9_AOT_FIX_ENDIAN_INDIRECT(j9dst_priv) \
    { \
        UDATA j9src_priv; \
        j9src_priv = *(UDATA *)j9dst_priv; \
        ((U_8 *)j9dst_priv)[0] = ((U_8 *)&j9src_priv)[3]; \
        ((U_8 *)j9dst_priv)[1] = ((U_8 *)&j9src_priv)[2]; \
        ((U_8 *)j9dst_priv)[2] = ((U_8 *)&j9src_priv)[1]; \
        ((U_8 *)j9dst_priv)[3] = ((U_8 *)&j9src_priv)[0]; \
    }
#define J9_AOT_FIX_ENDIAN_HALF(j9dst_priv) \
    { \
        UDATA j9src_priv; \
        j9src_priv = (UDATA)j9dst_priv; \
        ((U_8 *)&j9dst_priv)[0] = ((U_8 *)&j9src_priv)[1]; \
        ((U_8 *)&j9dst_priv)[1] = ((U_8 *)&j9src_priv)[0]; \
    }


   if (methodMetaData->numExcptionRanges)
      {
      UDATA numExcptionRanges = ((UDATA)methodMetaData->numExcptionRanges) & ~(J9_JIT_METADATA_WIDE_EXCEPTIONS | J9_JIT_METADATA_HAS_BYTECODE_PC);
      if ( ((UDATA)methodMetaData->numExcptionRanges) & J9_JIT_METADATA_WIDE_EXCEPTIONS )
         {
         /* 4 byte exception range entries */
         J9JIT32BitExceptionTableEntry *excptEntry32 = (J9JIT32BitExceptionTableEntry *)(methodMetaData + 1);
         while(numExcptionRanges > 0)
            {
            J9_AOT_FIX_ENDIAN(excptEntry32->startPC)
            J9_AOT_FIX_ENDIAN(excptEntry32->endPC)
            J9_AOT_FIX_ENDIAN(excptEntry32->handlerPC)
            J9_AOT_FIX_ENDIAN(excptEntry32->catchType)
            J9_AOT_FIX_ENDIAN(excptEntry32->ramMethod)
            ++excptEntry32;
            if ( ((UDATA)methodMetaData->numExcptionRanges) & J9_JIT_METADATA_HAS_BYTECODE_PC )
               {
               J9_AOT_FIX_ENDIAN_INDIRECT(excptEntry32);
               excptEntry32 = (J9JIT32BitExceptionTableEntry *) (((U_8 *) excptEntry32) + sizeof(U_32));
               }
            numExcptionRanges--;
            }
         }
      else
         {
         /* 2 byte exception range entries */
         J9JIT16BitExceptionTableEntry *excptEntry16 = (J9JIT16BitExceptionTableEntry *)(methodMetaData + 1);
         while(numExcptionRanges > 0)
            {
            J9_AOT_FIX_ENDIAN_HALF(excptEntry16->startPC)
            J9_AOT_FIX_ENDIAN_HALF(excptEntry16->endPC)
            J9_AOT_FIX_ENDIAN_HALF(excptEntry16->handlerPC)
            J9_AOT_FIX_ENDIAN_HALF(excptEntry16->catchType)
            ++excptEntry16;
            if ( ((UDATA)methodMetaData->numExcptionRanges) & J9_JIT_METADATA_HAS_BYTECODE_PC )
               {
               J9_AOT_FIX_ENDIAN_INDIRECT(excptEntry16);
               excptEntry16 = (J9JIT16BitExceptionTableEntry *) (((U_8 *) excptEntry16) + sizeof(U_32));
               }
            numExcptionRanges--;
            }
         }
      }
#undef J9_AOT_FIX_ENDIAN
#undef J9_AOT_FIX_ENDIAN_INDIRECT
#undef J9_AOT_FIX_ENDIAN_HALF
   }


U_32 getNumInlinedCallSites(J9JITExceptionTable * methodMetaData)
   {
   U_32 sizeOfInlinedCallSites, numInlinedCallSites = 0;
   U_32 numExceptionRanges = methodMetaData->numExcptionRanges & 0x3FFF;
   U_32 fourByteExceptionRanges = methodMetaData->numExcptionRanges & 0x8000;

   if (methodMetaData->inlinedCalls)
      {
#if 0
      if (fourByteExceptionRanges)
         {
         sizeOfInlinedCallSites = methodMetaData->size - ( sizeof(J9JITExceptionTable) + numExceptionRanges * 20);
         }
      else
         {
         sizeOfInlinedCallSites = methodMetaData->size - ( sizeof(J9JITExceptionTable) + numExceptionRanges * 8);
         }
      if (hasBytecodePC(methodMetaData))
         {
         sizeOfInlinedCallSites -= numExceptionRanges * 4;
         }
#endif

      sizeOfInlinedCallSites = (U_32) ((UDATA) methodMetaData->gcStackAtlas - (UDATA) methodMetaData->inlinedCalls);

      numInlinedCallSites = sizeOfInlinedCallSites / sizeOfInlinedCallSiteArrayElement(methodMetaData);
      }
   return numInlinedCallSites;
   }


static void aotByteCodeInfoFixEndian(TR_ByteCodeInfo * byteCodeInfo)
   {
   int   doNotProf = 0, callerInd;
   char * newByteCodeInfo;
   TR_ByteCodeInfo bci;
   bci._doNotProfile = (byteCodeInfo)->_doNotProfile;
   doNotProf = bci._doNotProfile & 0x00000001;
   bci._callerIndex = (byteCodeInfo)->_callerIndex;
   bci._byteCodeIndex = (byteCodeInfo)->_byteCodeIndex;
   newByteCodeInfo = (char *)byteCodeInfo;
   newByteCodeInfo[0] = doNotProf << 7;

   callerInd = bci._callerIndex & 0x00001fff;
   newByteCodeInfo[0] |= (callerInd>>6) & 0x7f;
   newByteCodeInfo[1] = (callerInd & 0x3f) << 2;

   callerInd = bci._byteCodeIndex & 0x0003ffff;
   newByteCodeInfo[1] |= (callerInd>>16) & 0x3;
   newByteCodeInfo[2] = (callerInd>>8) & 0xff;
   newByteCodeInfo[3] = (callerInd & 0xff);
   }


void aotStackAtlasFixEndian(J9JITStackAtlas * stackAtlas, J9JITExceptionTable * methodMetaData)
   {
#define J9_AOT_FIX_ENDIAN(j9dst_priv) \
    { \
        UDATA j9src_priv; \
        j9src_priv = (UDATA)j9dst_priv; \
        ((U_8 *)&j9dst_priv)[0] = ((U_8 *)&j9src_priv)[3]; \
        ((U_8 *)&j9dst_priv)[1] = ((U_8 *)&j9src_priv)[2]; \
        ((U_8 *)&j9dst_priv)[2] = ((U_8 *)&j9src_priv)[1]; \
        ((U_8 *)&j9dst_priv)[3] = ((U_8 *)&j9src_priv)[0]; \
    }
#define J9_AOT_FIX_ENDIAN_HALF(j9dst_priv) \
    { \
        UDATA j9src_priv; \
        j9src_priv = (UDATA)j9dst_priv; \
        ((U_8 *)&j9dst_priv)[0] = ((U_8 *)&j9src_priv)[1]; \
        ((U_8 *)&j9dst_priv)[1] = ((U_8 *)&j9src_priv)[0]; \
    }

   /* Fix the register map in each stack map */

   U_8 * stackMap =  (U_8 *) getFirstStackMap(stackAtlas);
   UDATA numberOfMaps;
   UDATA fourByteOffsets = ( (methodMetaData->endPC - methodMetaData->startPC) > USHRT_MAX )? 1 : 0;
   UDATA * lowPCOffset, * byteCodeInfo;
   U_8 * nextStackMap;
   U_32 numInlinedCallSites, i;
   U_8 *internalPtrMapCursor;

   for (numberOfMaps = (UDATA)stackAtlas->numberOfMaps; numberOfMaps; numberOfMaps--)
      {
      GET_NEXT_STACK_MAP(fourByteOffsets, stackMap, stackAtlas, nextStackMap);

      lowPCOffset = (UDATA *)ADDRESS_OF_LOW_PC_OFFSET_IN_STACK_MAP(fourByteOffsets, stackMap);
      byteCodeInfo = (UDATA *)ADDRESS_OF_BYTECODEINFO_IN_STACK_MAP(fourByteOffsets, stackMap);

      if (!IS_BYTECODEINFO_MAP(fourByteOffsets, stackMap))
         {
         UDATA *registerMap = (UDATA *)ADDRESS_OF_REGISTERMAP(fourByteOffsets, stackMap);
         J9_AOT_FIX_ENDIAN(*registerMap);
         }

      aotByteCodeInfoFixEndian((TR_ByteCodeInfo *)byteCodeInfo);

      if (fourByteOffsets)
         {
         J9_AOT_FIX_ENDIAN(*lowPCOffset);
         }
      else
         {
         J9_AOT_FIX_ENDIAN_HALF(*lowPCOffset);
         }

      stackMap = nextStackMap;
      }

   /* Fix endian of inlined call site array */
   numInlinedCallSites = getNumInlinedCallSites(methodMetaData);

   if (numInlinedCallSites)
      {
      U_8 * callSiteCursor = methodMetaData->inlinedCalls;
      for (i = 0; i < numInlinedCallSites; ++i)
         {
         TR_InlinedCallSite * inlinedCallSite = (TR_InlinedCallSite *)callSiteCursor;
         J9_AOT_FIX_ENDIAN(inlinedCallSite->_methodInfo)
         aotByteCodeInfoFixEndian(&inlinedCallSite->_byteCodeInfo);
         callSiteCursor += sizeOfInlinedCallSiteArrayElement(methodMetaData);
         }
      }

   /* Fix internal pointer maps */
   internalPtrMapCursor = stackAtlas->internalPointerMap;
   if (internalPtrMapCursor)
      {
      J9_AOT_FIX_ENDIAN(*internalPtrMapCursor); /* Fix endian of parameter map */
      internalPtrMapCursor += sizeof(UDATA) + (alignStackMaps ? 2 : 1);
      J9_AOT_FIX_ENDIAN_HALF(*internalPtrMapCursor);  /* Fix endian of index of first internal ptr */
      internalPtrMapCursor += 2;
      J9_AOT_FIX_ENDIAN_HALF(*internalPtrMapCursor);  /* Fix endian of offset of first internal ptr */
      }

/* Fix the stack atlas (we fix the padding so that if it is removed we'll find out) */
   J9_AOT_FIX_ENDIAN(stackAtlas->internalPointerMap)
   J9_AOT_FIX_ENDIAN_HALF(stackAtlas->numberOfMaps)
   J9_AOT_FIX_ENDIAN_HALF(stackAtlas->numberOfMapBytes)
   J9_AOT_FIX_ENDIAN_HALF(stackAtlas->parmBaseOffset)
   J9_AOT_FIX_ENDIAN_HALF(stackAtlas->numberOfParmSlots)
   J9_AOT_FIX_ENDIAN_HALF(stackAtlas->localBaseOffset)
   J9_AOT_FIX_ENDIAN_HALF(stackAtlas->paddingTo32)
#undef J9_AOT_FIX_ENDIAN
#undef J9_AOT_FIX_ENDIAN_HALF
   }

void aotMethodMetaDataFixEndian(J9JITExceptionTable * methodMetaData)
   {
#define J9_AOT_FIX_ENDIAN(j9dst_priv) \
    { \
        UDATA j9src_priv; \
        j9src_priv = (UDATA)j9dst_priv; \
        ((U_8 *)&j9dst_priv)[0] = ((U_8 *)&j9src_priv)[3]; \
        ((U_8 *)&j9dst_priv)[1] = ((U_8 *)&j9src_priv)[2]; \
        ((U_8 *)&j9dst_priv)[2] = ((U_8 *)&j9src_priv)[1]; \
        ((U_8 *)&j9dst_priv)[3] = ((U_8 *)&j9src_priv)[0]; \
    }
#define J9_AOT_FIX_ENDIAN_HALF(j9dst_priv) \
    { \
        UDATA j9src_priv; \
        j9src_priv = (UDATA)j9dst_priv; \
        ((U_8 *)&j9dst_priv)[0] = ((U_8 *)&j9src_priv)[1]; \
        ((U_8 *)&j9dst_priv)[1] = ((U_8 *)&j9src_priv)[0]; \
    }

/* Ignore the inlined call map, since we have none in AOT (for now) */

/* Fix fields within the method meta data structure */
   J9_AOT_FIX_ENDIAN(methodMetaData->constantPool)
   J9_AOT_FIX_ENDIAN(methodMetaData->ramMethod)
   J9_AOT_FIX_ENDIAN(methodMetaData->startPC)
   J9_AOT_FIX_ENDIAN(methodMetaData->endPC)
   J9_AOT_FIX_ENDIAN(methodMetaData->endWarmPC)
   J9_AOT_FIX_ENDIAN(methodMetaData->startColdPC)

   J9_AOT_FIX_ENDIAN(methodMetaData->hotness)
   J9_AOT_FIX_ENDIAN(methodMetaData->totalFrameSize)
   J9_AOT_FIX_ENDIAN_HALF(methodMetaData->slots)
   J9_AOT_FIX_ENDIAN_HALF(methodMetaData->scalarTempSlots)
   J9_AOT_FIX_ENDIAN_HALF(methodMetaData->objectTempSlots)
   J9_AOT_FIX_ENDIAN_HALF(methodMetaData->prologuePushes)
   J9_AOT_FIX_ENDIAN_HALF(methodMetaData->tempOffset)
   J9_AOT_FIX_ENDIAN_HALF(methodMetaData->numExcptionRanges)
   J9_AOT_FIX_ENDIAN(methodMetaData->size)
   J9_AOT_FIX_ENDIAN(methodMetaData->registerSaveDescription)
   J9_AOT_FIX_ENDIAN(methodMetaData->gcStackAtlas)
   J9_AOT_FIX_ENDIAN(methodMetaData->inlinedCalls)
#undef J9_AOT_FIX_ENDIAN
#undef J9_AOT_FIX_ENDIAN_HALF
    }

UDATA * getObjectArgScanCursor(J9StackWalkState * walkState)
   {
   return (UDATA *) (((U_8 *) walkState->bp) + ((J9JITStackAtlas *)walkState->jitInfo->gcStackAtlas)->parmBaseOffset);
   }


UDATA * getObjectTempScanCursor(J9StackWalkState * walkState)
   {
   return (UDATA *) (((U_8 *) walkState->bp) + ((J9JITStackAtlas *)walkState->jitInfo->gcStackAtlas)->localBaseOffset);
   }

I_32 hasSyncObjectTemp(J9StackWalkState * walkState)
   {
   return (I_16)((J9JITStackAtlas *)walkState->jitInfo->gcStackAtlas)->paddingTo32 == -1 ? 0 : 1;
   }

UDATA * getSyncObjectTempScanCursor(J9StackWalkState * walkState)
   {
   return (UDATA *) (((U_8 *) walkState->bp) + (I_16)((J9JITStackAtlas *)walkState->jitInfo->gcStackAtlas)->paddingTo32);
   }

U_8 getNextDescriptionBit(U_8 ** jitDescriptionCursor)
   {
   return *((*jitDescriptionCursor)++);
   }

void walkJITFrameSlotsForInternalPointers(J9StackWalkState * walkState,  U_8 ** jitDescriptionCursor, UDATA * scanCursor, void * stackMap, J9JITStackAtlas * gcStackAtlas)
   {
   UDATA registerMap;
   U_8 numInternalPtrMapBytes, numDistinctPinningArrays, i;
   UDATA parmStackMap;
   void * internalPointerMap = gcStackAtlas->internalPointerMap;
   U_8 *tempJitDescriptionCursor = (U_8 *) internalPointerMap;
   I_16 indexOfFirstInternalPtr;
   I_16 offsetOfFirstInternalPtr;
   U_8 internalPointersInRegisters = 0;

   /* If this PC offset corresponds to the parameter map, it means
      zero initialization of internal pointers/pinning arrays has not
      been done yet as execution is still in the prologue sequence
      that does the stack check failure check (can result in GC). Avoid
      examining internal pointer map in this case with uninitialized
      internal pointer autos, as calling GC in this case is problematic.
   */

   parmStackMap = *((UDATA *) tempJitDescriptionCursor);
   if (parmStackMap == ((UDATA) stackMap))
      return;

   registerMap = getJitRegisterMap(walkState->jitInfo, stackMap);

   /* Skip over the parameter map address (used in above compare)
   */
   tempJitDescriptionCursor += sizeof(UDATA);

#ifdef J9VM_INTERP_STACKWALK_TRACING
   swPrintf(walkState, 6, "Address %p\n", REMOTE_ADDR(tempJitDescriptionCursor));
#endif
   numInternalPtrMapBytes = *((tempJitDescriptionCursor)++);

   if (alignStackMaps)
      ++tempJitDescriptionCursor;

#ifdef J9VM_INTERP_STACKWALK_TRACING
   swPrintf(walkState, 6, "Num internal ptr map bytes %d\n", numInternalPtrMapBytes);
#endif

   indexOfFirstInternalPtr = (I_16) *((U_16 *) tempJitDescriptionCursor);

#ifdef J9VM_INTERP_STACKWALK_TRACING
   swPrintf(walkState, 6, "Address %p\n", REMOTE_ADDR(tempJitDescriptionCursor));
#endif
   tempJitDescriptionCursor += 2;


#ifdef J9VM_INTERP_STACKWALK_TRACING
    swPrintf(walkState, 6,"Index of first internal ptr %d\n", indexOfFirstInternalPtr);
#endif


   offsetOfFirstInternalPtr = (I_16) *((U_16 *) tempJitDescriptionCursor);

#ifdef J9VM_INTERP_STACKWALK_TRACING
   swPrintf(walkState, 6, "Address %p\n", REMOTE_ADDR(tempJitDescriptionCursor));
#endif


   tempJitDescriptionCursor += 2;


#ifdef J9VM_INTERP_STACKWALK_TRACING
   swPrintf(walkState, 6, "Offset of first internal ptr %d\n", offsetOfFirstInternalPtr);
#endif


#ifdef J9VM_INTERP_STACKWALK_TRACING
   swPrintf(walkState, 6, "Address %p\n", REMOTE_ADDR(tempJitDescriptionCursor));
#endif
   numDistinctPinningArrays = *((tempJitDescriptionCursor)++);

#ifdef J9VM_INTERP_STACKWALK_TRACING
   swPrintf(walkState, 6, "Num distinct pinning arrays %d\n", numDistinctPinningArrays);
#endif


   i = 0;
   if ((registerMap & INTERNAL_PTR_REG_MASK) && (registerMap != (UDATA)0xFADECAFE))
      internalPointersInRegisters = 1;


   while (i < numDistinctPinningArrays)
      {
      U_8 currPinningArrayIndex = *(tempJitDescriptionCursor++);
      U_8 numInternalPtrsForArray = *(tempJitDescriptionCursor++);
      J9Object ** currPinningArrayCursor = (J9Object **) (((U_8 *) walkState->bp) + (offsetOfFirstInternalPtr + (((U_16) currPinningArrayIndex * sizeof(UDATA)))));
      J9Object *oldPinningArrayAddress = *((J9Object **) currPinningArrayCursor);
      J9Object * newPinningArrayAddress;
      IDATA displacement = 0;


#ifdef J9VM_INTERP_STACKWALK_TRACING
      swPrintf(walkState, 6, "Before object slot walk &address : %p address : %p bp %p offset of first internal ptr %d\n", REMOTE_ADDR(currPinningArrayCursor), oldPinningArrayAddress, REMOTE_ADDR(walkState->bp), offsetOfFirstInternalPtr);
#endif
      walkState->objectSlotWalkFunction(walkState->walkThread, walkState, currPinningArrayCursor, REMOTE_ADDR(currPinningArrayCursor));
      newPinningArrayAddress = *((J9Object **) currPinningArrayCursor);
      displacement = (IDATA) (((UDATA)newPinningArrayAddress) - ((UDATA)oldPinningArrayAddress));
      ++(walkState->slotIndex);

#ifdef J9VM_INTERP_STACKWALK_TRACING
      swPrintf(walkState, 6, "After object slot walk for pinning array with &address : %p old address %p new address %p displacement %p\n", REMOTE_ADDR(currPinningArrayCursor), oldPinningArrayAddress, newPinningArrayAddress, displacement);
#endif


#ifdef J9VM_INTERP_STACKWALK_TRACING
      swPrintf(walkState, 6, "For pinning array %d num internal pointer stack slots %d\n", currPinningArrayIndex, numInternalPtrsForArray);
#endif


       /* If base array was moved by a non zero displacement
       */
#if defined(J9VM_INTERP_STACKWALK_TRACING) && !defined(J9VM_OUT_OF_PROCESS)
      if ((displacement != 0) || (walkState->walkThread->javaVM->runtimeFlags & J9_RUNTIME_SNIFF_AND_WHACK))
#else
      if (displacement != 0)
#endif
         {
         U_8 j = 0;

         while (j < numInternalPtrsForArray)
            {
            U_8 internalPtrAuto = *(tempJitDescriptionCursor++);
            J9Object ** currInternalPtrCursor = (J9Object **) (((U_8 *) walkState->bp) + (offsetOfFirstInternalPtr + (((U_16) internalPtrAuto) * sizeof(UDATA))));


#ifdef J9VM_INTERP_STACKWALK_TRACING
            swPrintf(walkState, 6, "For pinning array %d internal pointer auto %d old address %p displacement %p\n", currPinningArrayIndex, internalPtrAuto, *currInternalPtrCursor, displacement);
#endif

            MARK_SLOT_AS_OBJECT(walkState, currInternalPtrCursor);

            /*
               If the internal pointer is non null and the base pinning array object
               was moved by a non zero displacement, then adjust the internal pointer
            */
            if (*currInternalPtrCursor != 0)
               {
               IDATA internalPtrAddress = (IDATA) *currInternalPtrCursor;
               internalPtrAddress += displacement;


#ifdef J9VM_INTERP_STACKWALK_TRACING
               swPrintf(walkState, 6, "For pinning array %d internal pointer auto %d new address %p\n", currPinningArrayIndex, internalPtrAuto, internalPtrAddress);
#endif
               *currInternalPtrCursor = (J9Object *) internalPtrAddress;
               }
            ++j;
            }

         if (internalPointersInRegisters)
            {
            U_8 *tempJitDescriptionCursorForRegs;
            U_8 numInternalPtrRegMapBytes, numDistinctPinningArrays;
            U_8 j, k;

            registerMap = registerMap & J9SW_REGISTER_MAP_MASK;

#ifdef J9VM_INTERP_STACKWALK_TRACING
            swPrintf(walkState, 6, "\tJIT-RegisterMap = %p\n", registerMap);
#endif
            tempJitDescriptionCursorForRegs = (U_8 *)stackMap;

            /* Skip the register map and register save description */
            tempJitDescriptionCursorForRegs += 8;

            if (((walkState->jitInfo->endPC - walkState->jitInfo->startPC) >= 65535) || (alignStackMaps))
               {
               tempJitDescriptionCursorForRegs += 8;
               }
            else
               {
               tempJitDescriptionCursorForRegs += 6;
               }
            numInternalPtrRegMapBytes = *((tempJitDescriptionCursorForRegs)++);
            numDistinctPinningArrays = *((tempJitDescriptionCursorForRegs)++);
            k = 0;
            while (k < numDistinctPinningArrays)
               {
               U_8 currPinningArrayIndexForRegister = *(tempJitDescriptionCursorForRegs++);
               U_8 numInternalPtrRegsForArray = *(tempJitDescriptionCursorForRegs++);
               j = 0;

               if (currPinningArrayIndexForRegister == currPinningArrayIndex)
                  {
                  UDATA ** mapCursor;

#ifdef J9SW_REGISTER_MAP_WALK_REGISTERS_LOW_TO_HIGH
                  mapCursor = (UDATA **) &(walkState->registerEAs);
#else
                  mapCursor = ((UDATA **) &(walkState->registerEAs)) + J9SW_POTENTIAL_SAVED_REGISTERS - 1;
#endif

                  while (j < numInternalPtrRegsForArray)
                     {
                     U_8 internalPtrRegNum = *(tempJitDescriptionCursorForRegs++);
                     J9Object ** internalPtrObject;
                     IDATA internalPtrAddress;


#ifdef J9SW_REGISTER_MAP_WALK_REGISTERS_LOW_TO_HIGH
                     internalPtrObject = *((J9Object ***) (mapCursor + (internalPtrRegNum - 1)));
#else
                     internalPtrObject = *((J9Object ***) (mapCursor - (internalPtrRegNum)));
#endif


                     internalPtrAddress = (IDATA) (*internalPtrObject);


#ifdef J9VM_INTERP_STACKWALK_TRACING
                     swPrintf(walkState, 6, "Original internal pointer reg address %p\n", internalPtrAddress);
#endif

                     MARK_SLOT_AS_OBJECT(walkState, internalPtrObject);

                     if (internalPtrAddress != 0)
                        internalPtrAddress += displacement;


#ifdef J9VM_INTERP_STACKWALK_TRACING
                     swPrintf(walkState, 6, "Adjusted internal pointer reg to be address %p (disp %p)\n", internalPtrAddress, displacement);
#endif

                     *internalPtrObject = (J9Object *) internalPtrAddress;
                     ++j;
                     }


                  break;
                  }
               else
                  tempJitDescriptionCursorForRegs += numInternalPtrRegsForArray; /* skip over requisite number of bytes */

               ++k;
               }
            }
         }
      else
         tempJitDescriptionCursor += numInternalPtrsForArray; /* skip over requisite number of bytes */

      ++i;
      }
   }

U_8 * getJitDescriptionCursor(void * stackMap, J9StackWalkState * walkState) {
   return 0; /* deprecated */
   }

U_8 * getVariablePortionInternalPtrRegMap(U_8 * mapBits, int fourByteOffsets)
   {
   return mapBits + GET_SIZEOF_STACK_MAP_HEADER(fourByteOffsets);
   }

U_8 getVariableLengthSizeOfInternalPtrRegMap(U_8 * internalPtrMapLocation)
   {
   return *internalPtrMapLocation;
   }

U_8 getNumberOfPinningArrays(U_8 * internalPtrMapLocation)
   {
   return *(internalPtrMapLocation + 1);
   }

U_8 * getFirstPinningArray(U_8 * internalPtrMapLocation)
   {
   return (internalPtrMapLocation + 2);
   }

U_8 * getNextPinningArray(U_8 * pinningArrayCursor)
   {
   U_8 numberOfInternalPtrs = getNumberOfInternalPtrs(pinningArrayCursor);
   return (pinningArrayCursor + numberOfInternalPtrs + 2);
   }

U_8 getNumberOfInternalPtrs(U_8 * pinningArrayCursor)
   {
   return *(pinningArrayCursor + 1);
   }

U_8 * getFirstInternalPtr(U_8 * pinningArrayCursor)
   {
   return (pinningArrayCursor + 2);
   }

U_8 * getNextInternalPtr(U_8 * internalPtrCursor)
   {
   return (internalPtrCursor + 1);
   }

static U_32 getStackMapRegisterMap(U_8 * mapBits)
   {
   return *(U_32 *)mapBits;
   }

JITINLINE U_8 * getFirstStackMap(J9TR_StackAtlas * stackAtlas)
   {
   return (U_8*)stackAtlas + sizeof(J9TR_StackAtlas) + stackAtlas->numberOfMapBytes;
   }

JITINLINE U_32 * get32BitByteCodeIndexFromExceptionTable(J9TR_MethodMetaData * methodMetaData)
   {
   return (U_32 *)((char *)methodMetaData + sizeof(J9JITExceptionTable) + sizeof(J9JIT32BitExceptionTableEntry));
   }

JITINLINE U_32 * get16BitByteCodeIndexFromExceptionTable(J9TR_MethodMetaData * methodMetaData)
   {
   return (U_32 *)((char *)methodMetaData + sizeof(J9JITExceptionTable) + sizeof(J9JIT16BitExceptionTableEntry));
   }

U_8 getVariableLengthSizeOfInternalPtrMap(U_8 * internalPtrMapLocation)
   {
   return *((U_8 *)internalPtrMapLocation + sizeof(UDATA *));
   }

U_16 getIndexOfFirstInternalPtr(U_8 * internalPtrMapLocation)
   {
   if (alignStackMaps)
      return *(U_16 *)((U_8 *)internalPtrMapLocation + sizeof(UDATA *) + sizeof(U_16));
   return *(U_16 *)((U_8 *)internalPtrMapLocation + sizeof(UDATA *) + sizeof(U_8));
   }

U_16 getOffsetOfFirstInternalPtr(U_8 * internalPtrMapLocation)
   {
   if (alignStackMaps)
      return *(U_16 *)((U_8 *)internalPtrMapLocation + sizeof(UDATA *) + sizeof(U_16) + sizeof(U_16));
   return *(U_16 *)((U_8 *)internalPtrMapLocation + sizeof(UDATA *) + sizeof(U_8) + sizeof(U_16));
   }

U_8 getNumberOfPinningArraysOfInternalPtrMap(U_8 * internalPtrMapLocation)
   {
   if (alignStackMaps)
      return *((U_8 *)internalPtrMapLocation + sizeof(UDATA *) + sizeof(U_16) + 2*sizeof(U_16));
   return *((U_8 *)internalPtrMapLocation + sizeof(UDATA *) + sizeof(U_8) + 2*sizeof(U_16));
   }

U_8 * getFirstInternalPtrMapPinningArrayCursor(U_8 * internalPtrMapLocation)
   {
   if (alignStackMaps)
      return ((U_8 *)internalPtrMapLocation + sizeof(UDATA *) + 3*sizeof(U_8) + 2*sizeof(U_16));
   return ((U_8 *)internalPtrMapLocation + sizeof(UDATA *) + 2*sizeof(U_8) + 2*sizeof(U_16));
   }

U_8 * getNextInternalPtrMapPinningArrayCursor(U_8 * internalPtrMapPinningArrayCursor)
   {
   U_8 numberOfInternalPtrs = getNumberOfInternalPtrs(internalPtrMapPinningArrayCursor);
   return (internalPtrMapPinningArrayCursor + numberOfInternalPtrs + 2);
   }

U_32 getJitRegisterMap(J9TR_MethodMetaData * methodMetaData, void * stackMap)
   {
   return *((U_32*)GET_REGISTER_MAP_CURSOR(HAS_FOUR_BYTE_OFFSET(methodMetaData), stackMap));
   }

U_8 * getJitStackSlots(J9TR_MethodMetaData * metaData, void * stackMap)
   {
   U_8 * cursor = GET_REGISTER_MAP_CURSOR(HAS_FOUR_BYTE_OFFSET(metaData), stackMap);
   if ((*(U_32*)cursor & INTERNAL_PTR_REG_MASK) && getJitInternalPointerMap((J9JITStackAtlas *)getJitGCStackAtlas(metaData)))
      {
      cursor += (*(cursor + 4) + 1);
      }
   cursor += 4;
   return cursor;
   }

U_8 * getNextDecriptionCursor(J9TR_MethodMetaData * metaData, void * stackMap, U_8 * jitDescriptionCursor)
   {
   return getJitStackSlots(metaData, stackMap); /* deprecated ... use getJitStackSlots */
   }

U_8 * getJitLiveMonitors(J9TR_MethodMetaData * metaData, void * stackMap)
   {
   U_8 * cursor = getJitStackSlots(metaData, stackMap);
   U_16 mapBytes = getJitNumberOfMapBytes(getJitGCStackAtlas(metaData));
   cursor += mapBytes - 1;
   if ((*cursor & 128) == 0)
      return NULL;

   return cursor + 1;
   }

void jitAddSpilledRegistersForDataResolve(J9StackWalkState * walkState)
   {
   UDATA * slotCursor = walkState->unwindSP + getJitSlotsBeforeSavesInDataResolve();
   UDATA ** mapCursor = (UDATA **) &(walkState->registerEAs);
   UDATA i;

   for (i = 0; i < J9SW_POTENTIAL_SAVED_REGISTERS; ++i)
      {
      *mapCursor++ = slotCursor++;
      }

#ifdef J9VM_INTERP_STACKWALK_TRACING
   swPrintf(walkState, 2, "\t%d slots skipped before scalar registers\n", getJitSlotsBeforeSavesInDataResolve());
   jitPrintRegisterMapArray(walkState, "DataResolve");
#endif
   }

void jitAddSpilledRegisters(J9StackWalkState * walkState, void * stackMap)
   {
   UDATA savedGPRs = 0, saveOffset = 0, * saveCursor = 0, lowestRegister = 0;
   UDATA ** mapCursor = (UDATA **) &(walkState->registerEAs);
   J9TR_MethodMetaData * md = walkState->jitInfo;
   UDATA registerSaveDescription = walkState->jitInfo->registerSaveDescription;

#if defined(TR_HOST_X86)
   UDATA prologuePushes = (UDATA) getJitProloguePushes(walkState->jitInfo);
   U_8 i = 1;

   if (prologuePushes)
      {
      UDATA * saveCursor = walkState->bp - (((UDATA) getJitScalarTempSlots(walkState->jitInfo)) + ((UDATA) getJitObjectTempSlots(walkState->jitInfo)) + prologuePushes);

      registerSaveDescription &= J9SW_REGISTER_MAP_MASK;
      do
         {
         if (registerSaveDescription & 1)
            {
            *mapCursor = saveCursor++;
            }

         ++i;
         ++mapCursor;
         registerSaveDescription >>= 1;
         }
      while (registerSaveDescription);
      }
#elif defined(TR_HOST_POWER)
   /*
    * see PPCLinkage for a description of the RSD
    * the save offset is located from bits 18-32
    * so first mask it off to get the bit vector
    * corresponding to the saved GPRS
    */
   savedGPRs = registerSaveDescription & 0x1FFFF;
   saveOffset = (registerSaveDescription >> 17) & 0x7FFF;
   lowestRegister = 15; /* gpr15 is the first saved GPR, so move 15 spaces */

   saveCursor = (UDATA *) (((U_8 *) walkState->bp) - saveOffset);

   mapCursor += lowestRegister; /* access gpr15 in the vm register state */
   U_8 i = lowestRegister + 1;
   do
      {
      if (savedGPRs & 1)
         {
         *mapCursor = saveCursor++;
         }

      ++i;
      ++mapCursor;
      savedGPRs >>= 1;
      }
   while (savedGPRs != 0);

#elif defined(TR_HOST_ARM) || defined(TR_HOST_S390)
   savedGPRs = registerSaveDescription & 0xFFFF;

   if (savedGPRs)
      {
      UDATA saveOffset = 0xFFFF&(registerSaveDescription >> 16); /* UDATA is a  ptr, so clean high word */
                                                                 /* for correctness on 64bit platforms  */
      UDATA * saveCursor = (UDATA *) (((U_8 *) walkState->bp) - saveOffset);
      U_8 i = 1;
      do
         {
         if (savedGPRs & 1)
            {
            *mapCursor = saveCursor++;
            }

         ++i;
         ++mapCursor;
         savedGPRs >>= 1;
         }
      while (savedGPRs != 0);
      }
#elif defined(TR_HOST_ARM64)
   // TODO: Implement this
   assert(0);
#else
#error Unknown TR_HOST type
#endif


#ifdef J9VM_INTERP_STACKWALK_TRACING
   jitPrintRegisterMapArray(walkState, "Frame");
#endif
   }

JITINLINE J9ConstantPool * getJitConstantPool(J9TR_MethodMetaData * md)
   {
   return md->constantPool;
   }

JITINLINE J9Method * getJitRamMethod(J9TR_MethodMetaData * md)
   {
   return md->ramMethod;
   }

JITINLINE UDATA getJittedMethodStartPC(J9TR_MethodMetaData * md)
   {
   return md->startPC;
   }

JITINLINE UDATA getJittedMethodEndPC(J9TR_MethodMetaData * md)
   {
   return md->endPC;
   }

I_16 getJitTotalFrameSize(J9TR_MethodMetaData * md)
   {
   return (I_16) md->totalFrameSize;
   }

I_16 getJitSlots(J9TR_MethodMetaData * md)
   {
   return md->slots;
   }

I_16 getJitScalarTempSlots(J9TR_MethodMetaData * md)
   {
   return md->scalarTempSlots;
   }

I_16 getJitObjectTempSlots(J9TR_MethodMetaData * md)
   {
   return md->objectTempSlots;
   }

JITINLINE U_16 getJitProloguePushes(J9TR_MethodMetaData * md)
   {
   return md->prologuePushes;
   }

JITINLINE I_16 getJitTempOffset(J9TR_MethodMetaData * md)
   {
   return md->tempOffset;
   }

JITINLINE U_16 getJitNumberOfExceptionRanges(J9TR_MethodMetaData * md)
   {
   return md->numExcptionRanges;
   }

JITINLINE I_32 getJitExceptionTableSize(J9TR_MethodMetaData * md)
   {
   return md->size;
   }

void * getJitGCStackAtlas(J9TR_MethodMetaData * md)
   {
   return md->gcStackAtlas;
   }

void * getJitInlinedCallInfo(J9TR_MethodMetaData * md)
   {
   return md->inlinedCalls;
   }

U_8 * getJitInternalPointerMap(J9TR_StackAtlas * sa)
   {
   return sa->internalPointerMap;
   }

U_16 getJitNumberOfMaps(J9TR_StackAtlas * sa)
   {
   return sa->numberOfMaps;
   }

U_16 getJitNumberOfMapBytes(J9TR_StackAtlas * sa)
   {
   return sa->numberOfMapBytes;
   }

I_16 getJitParmBaseOffset(J9TR_StackAtlas * sa)
   {
   return sa->parmBaseOffset;
   }

U_16 getJitNumberOfParmSlots(J9TR_StackAtlas * sa)
   {
   return sa->numberOfParmSlots;
   }

JITINLINE I_16 getJitLocalBaseOffset(J9TR_StackAtlas * sa)
   {
   return sa->localBaseOffset;
   }

JITINLINE U_32 getJit32BitTableEntryStartPC(J9JIT32BitExceptionTableEntry * te)
   {
   return te->startPC;
   }

JITINLINE U_32 getJit32BitTableEntryEndPC(J9JIT32BitExceptionTableEntry * te) {
   return te->endPC;
   }

JITINLINE U_32 getJit32BitTableEntryHandlerPC(J9JIT32BitExceptionTableEntry * te)
   {
   return te->handlerPC;
   }

JITINLINE U_32 getJit32BitTableEntryCatchType(J9JIT32BitExceptionTableEntry * te)
   {
   return te->catchType;
   }

JITINLINE J9Method * getJit32BitTableEntryRamMethod(J9JIT32BitExceptionTableEntry * te)
   {
   return te->ramMethod;
   }

JITINLINE U_16 getJit16BitTableEntryStartPC(J9JIT16BitExceptionTableEntry * te)
   {
   return te->startPC;
   }

JITINLINE U_16 getJit16BitTableEntryEndPC(J9JIT16BitExceptionTableEntry * te)
   {
   return te->endPC;
   }

JITINLINE U_16 getJit16BitTableEntryHandlerPC(J9JIT16BitExceptionTableEntry * te)
   {
   return te->handlerPC;
   }

JITINLINE U_16 getJit16BitTableEntryCatchType(J9JIT16BitExceptionTableEntry * te)
   {
   return te->catchType;
   }

JITINLINE UDATA hasBytecodePC(J9TR_MethodMetaData * md)
   {
   return md->numExcptionRanges & J9_JIT_METADATA_HAS_BYTECODE_PC;
   }

JITINLINE UDATA hasWideExceptions(J9TR_MethodMetaData * md)
   {
   return md->numExcptionRanges & J9_JIT_METADATA_WIDE_EXCEPTIONS;
   }

UDATA hasFourByteOffset(J9TR_MethodMetaData * md)
   {
   return md->flags & JIT_METADATA_GC_MAP_32_BIT_OFFSETS;
   }

UDATA * getTempBase(UDATA * bp, J9TR_MethodMetaData * methodMetaData)
   {
   return ((UDATA *) (((U_8 *) bp) + getJitLocalBaseOffset((J9JITStackAtlas *) getJitGCStackAtlas(methodMetaData)))) + getJitTempOffset(methodMetaData);
   }

JITINLINE void * getByteCodeInfoFromStackMap(J9TR_MethodMetaData * methodMetaData, void * stackMap)
   {
   return (void *)ADDRESS_OF_BYTECODEINFO_IN_STACK_MAP(HAS_FOUR_BYTE_OFFSET(methodMetaData), stackMap);
   }


/* This is the method vm can call to retrieve the bytecode index given a pointer to the TR_InlinedCallSite*/
JITINLINE UDATA getByteCodeIndex(void * inlinedCallSite)
   {
   TR_ByteCodeInfo * byteCodeInfo = (TR_ByteCodeInfo *)getByteCodeInfo(inlinedCallSite);
   return byteCodeInfo->_byteCodeIndex;
   }

UDATA hasMoreInlinedMethods(void * inlinedCallSite)
   {
   TR_ByteCodeInfo * byteCodeInfo = (TR_ByteCodeInfo *)getByteCodeInfo(inlinedCallSite);
   return byteCodeInfo->_callerIndex < 0 ? 0 : 1;
   }

void * getInlinedMethod(void * inlinedCallSite)
   {
   return ((TR_InlinedCallSite *)inlinedCallSite)->_methodInfo;
   }


static void *getNotUnloadedInlinedCallSiteArrayElement(J9TR_MethodMetaData * methodMetaData, int cix)
   {
   void *inlinedCallSite = getInlinedCallSiteArrayElement(methodMetaData, cix);
   while (isUnloadedInlinedMethod(getInlinedMethod(inlinedCallSite)))
      {
      inlinedCallSite = getNextInlinedCallSite(methodMetaData, inlinedCallSite);
      if (!inlinedCallSite)
          break;
      }
   return inlinedCallSite;
   }

void * getNextInlinedCallSite(J9TR_MethodMetaData * methodMetaData, void * inlinedCallSite)
   {
   if (hasMoreInlinedMethods(inlinedCallSite))
      {
      return getNotUnloadedInlinedCallSiteArrayElement(methodMetaData, ((TR_ByteCodeInfo *)getByteCodeInfo(inlinedCallSite))->_callerIndex);
      }
   return NULL;
   }


void * getFirstInlinedCallSiteWithByteCodeInfo(J9TR_MethodMetaData * methodMetaData, void * stackMap, void * byteCodeInfo)
   {
   int cix;
   if (byteCodeInfo == 0)
       byteCodeInfo = ADDRESS_OF_BYTECODEINFO_IN_STACK_MAP(HAS_FOUR_BYTE_OFFSET(methodMetaData), stackMap);
   cix = ((TR_ByteCodeInfo *) byteCodeInfo)->_callerIndex;
   if (cix < 0)
      return NULL;

   return getNotUnloadedInlinedCallSiteArrayElement(methodMetaData, cix);
   }

void * getFirstInlinedCallSite(J9TR_MethodMetaData * methodMetaData, void * stackMap)
   {
   return getFirstInlinedCallSiteWithByteCodeInfo(methodMetaData, stackMap, 0);
   }


U_8 * getMonitorMask(J9TR_StackAtlas * stackAtlas, void * inlinedCallSite)
   {
   if (inlinedCallSite)
      {
      TR_ByteCodeInfo * byteCodeInfo = (TR_ByteCodeInfo *)getByteCodeInfo(inlinedCallSite);

      /* _isSameReceiver is an overloaded bit that in this context means that the live monitor data for this
       * inlined call site contains some nonzero bits
       */
      if (byteCodeInfo->_isSameReceiver)
         return (U_8*)inlinedCallSite + sizeof(TR_InlinedCallSite);

      return 0;
      }

   return (U_8 *)stackAtlas + sizeof(J9TR_StackAtlas);
   }

JITINLINE void * getByteCodeInfo(void * inlinedCallSite)
   {
   TR_ByteCodeInfo * bci = NULL;
   bci = &(((TR_InlinedCallSite *)inlinedCallSite)->_byteCodeInfo);
   return (void *)bci;
   }

UDATA getJitInlineDepthFromCallSite(J9TR_MethodMetaData * methodMetaData, void * inlinedCallSite)
   {
   UDATA inlineDepth = 0;
   do
      {
      ++inlineDepth;
      inlinedCallSite = getNextInlinedCallSite(methodMetaData, inlinedCallSite);
      }
   while (inlinedCallSite);
   return inlineDepth;
   }

UDATA getByteCodeIndexFromStackMap(J9TR_MethodMetaData * methodMetaData, void * stackMap)
   {
   /*
    * void * byteCodeInfo = getByteCodeInfoFromStackMap(methodMetaData, stackMap);
    */
   void * byteCodeInfo = ADDRESS_OF_BYTECODEINFO_IN_STACK_MAP(HAS_FOUR_BYTE_OFFSET(methodMetaData), stackMap);
   return ((TR_ByteCodeInfo *)byteCodeInfo)->_byteCodeIndex;
   }

UDATA getCurrentByteCodeIndexAndIsSameReceiver(J9TR_MethodMetaData * methodMetaData, void * stackMap, void * currentInlinedCallSite, UDATA * isSameReceiver)
   {
   TR_ByteCodeInfo * byteCodeInfo = (TR_ByteCodeInfo *)getByteCodeInfoFromStackMap(methodMetaData, stackMap);

   if (currentInlinedCallSite)
      {
      void * inlinedCallSite = getFirstInlinedCallSiteWithByteCodeInfo(methodMetaData, stackMap, byteCodeInfo);
      if (inlinedCallSite != currentInlinedCallSite)
         {
         void * previousInlinedCallSite;
         do
            {
            previousInlinedCallSite = inlinedCallSite;
            inlinedCallSite = getNextInlinedCallSite(methodMetaData, inlinedCallSite);
            }
         while (inlinedCallSite != currentInlinedCallSite);
         byteCodeInfo = (TR_ByteCodeInfo *)getByteCodeInfo(previousInlinedCallSite);
         }
      }
   else if (byteCodeInfo->_callerIndex != -1)
      {
      void * inlinedCallSite = getFirstInlinedCallSiteWithByteCodeInfo(methodMetaData, stackMap, byteCodeInfo);
      void * prevInlinedCallSite = inlinedCallSite;
      while (inlinedCallSite && hasMoreInlinedMethods(inlinedCallSite))
         {
         prevInlinedCallSite = inlinedCallSite;
         inlinedCallSite = getNextInlinedCallSite(methodMetaData, inlinedCallSite);
         }
      byteCodeInfo = (TR_ByteCodeInfo *)getByteCodeInfo(prevInlinedCallSite);
      if (inlinedCallSite)
         byteCodeInfo = (TR_ByteCodeInfo *)getByteCodeInfo(inlinedCallSite);

      }

   if (isSameReceiver != 0)
      *isSameReceiver = byteCodeInfo->_isSameReceiver;
   return byteCodeInfo->_byteCodeIndex;
   }

UDATA getJitPCOffsetFromExceptionHandler(J9TR_MethodMetaData * methodMetaData, void *jitPC)
   {
   UDATA numberOfRanges = methodMetaData->numExcptionRanges;
   UDATA relativePC = (UDATA) jitPC - methodMetaData->startPC;

   if (numberOfRanges & J9_JIT_METADATA_WIDE_EXCEPTIONS)
      {
      J9JIT32BitExceptionTableEntry * handlerCursor = (J9JIT32BitExceptionTableEntry *) (methodMetaData + 1);

      numberOfRanges &= ~(J9_JIT_METADATA_WIDE_EXCEPTIONS | J9_JIT_METADATA_HAS_BYTECODE_PC);
      while (numberOfRanges)
         {
         if (relativePC == handlerCursor->handlerPC)
            {
            U_32 pcOffset = *((U_32 *) (handlerCursor + 1));
            return (UDATA)pcOffset;
            }
         handlerCursor = (J9JIT32BitExceptionTableEntry *) (((U_8 *) (handlerCursor + 1)) + sizeof(U_32));
         --numberOfRanges;
         }
      }
   else
      {
      J9JIT16BitExceptionTableEntry * handlerCursor = (J9JIT16BitExceptionTableEntry *) (methodMetaData + 1);
      numberOfRanges &= ~(J9_JIT_METADATA_WIDE_EXCEPTIONS | J9_JIT_METADATA_HAS_BYTECODE_PC);
      while (numberOfRanges)
         {
         if (relativePC == handlerCursor->handlerPC)
            {
            U_32 pcOffset = *((U_32 *) (handlerCursor + 1));
            return (UDATA)pcOffset;
            }
         handlerCursor = (J9JIT16BitExceptionTableEntry *) (((U_8 *) (handlerCursor + 1)) + sizeof(U_32));
         --numberOfRanges;
         }
      }
   return 0;
   }

/* Number of slots pushed between the unwindSP of the JIT frame and the pushed register arguments for recompilation resolves.

   Include the slot for the pushed return address (if any) for the call from the codecache to the picbuilder.
   Do not include the slot for return address for call from picbuilder to resolve helper.
*/

UDATA getJitRecompilationResolvePushes()
   {
#if defined(TR_HOST_X86) && defined(TR_HOST_64BIT)
   /* AMD64 recompilation resolve shape
      0:  rcx (arg register)
      1:  rdx (arg register)
      2:  rsi (arg register)
      3:  rax (arg register)
      4:  8 XMMs (1 8-byte slot each)                    <== unwindSP points here
      12: return PC (caller of recompiled method)
      13: <last argument to method>                      <== unwindSP should point here"
   */
   return 9;
#elif defined(TR_HOST_X86)
   /* IA32 recompilation resolve shape
      0:  return address (picbuilder)
      1:  return PC (caller of recompiled method)
      2:  old start address
      3:  method
      4:  EAX (contains receiver for virtual)            <== unwindSP points here
      5:  EDX
      6:  return PC (caller of recompiled method)
      7:  <last argument to method>                      <== unwindSP should point here"
   */
   return 3;
#elif defined(TR_HOST_S390)
   /* 390 recompilation resolve shape
      0:  r3 (arg register)
      1:  r2 (arg register)
      2:  r1 (arg register)
      3:  ??                                             <== unwindSP points here
      4:  ??
      5:  ??
      6:  64 bytes FP register saves
      7:  temp1
      8:  temp2
      9:  temp3
      XX: <linkage area>                                 <== unwindSP should point here
   */
   return 7 + (64 / sizeof(UDATA));
#elif defined(TR_HOST_POWER)
   /* PPC recompilation resolve shape
      0:  r10 (arg register)
      1:  r9 (arg register)
      2:  r8 (arg register)
      3:  r7 (arg register)
      4:  r6 (arg register)
      5:  r5 (arg register)
      6:  r4 (arg register)
      7:  r3 (arg register)
      8:  r0                                             <== unwindSP points here
      9:  r12
      10: r12
      XX: <linkage area>                                 <== unwindSP should point here
   */
   return 3;
#elif defined(TR_HOST_ARM)
   /* ARM recompilation resolve shape
      0:  r3 (arg register)
      1:  r2 (arg register)
      2:  r1 (arg register)
      3:  r0 (arg register)
      4:  r4 return PC (caller of recompiled method)     <== unwindSP points here
      5:  old startPC
      XX: (linkage area)                                 <== unwindSP should point here
   */
   return 2;
#else
   return 0;
#endif
   }

/* Number of slots pushed between the unwindSP of the JIT frame and the pushed resolve arguments for static/special method resolves.

   Include the slot for the pushed return address (if any) for the call from the codecache to the picbuilder.
   Do not include the slot for return address for call from picbuilder to resolve helper.
*/

UDATA getJitStaticMethodResolvePushes()
   {
#if defined(TR_HOST_X86) && defined(TR_HOST_64BIT)
   /* AMD64 static resolve shape
      0: ret addr to picbuilder
      1: code cache return address		<==== unwindSP points here
      2: last arg									<==== unwindSP should point here
   */
   return 1;
#elif defined(TR_HOST_X86)
   /* IA32 static resolve shape
      0: return address (picbuilder)
      1: jitEIP
      2: constant pool
      3: cpIndex
      4: return address (code cache)		<== unwindSP points here
      5: <last argument to method>			<== unwindSP should point here
   */
   return 1;
#else
   return 0;
#endif
   }

/* Number of slots pushed between the unwindSP of the JIT frame and the pushed register arguments for virtual/interface method resolves.

   Include the slot for the pushed return address (if any) for the call from the codecache to the picbuilder.
   Do not include the slot for return address for call from picbuilder to resolve helper.
*/

UDATA getJitVirtualMethodResolvePushes()
   {
#if defined(TR_HOST_X86) && defined(TR_HOST_64BIT)
   /* AMD64 virtual resolve shape
      0: ret addr to picbuilder
      1: arg3
      2: arg2
      3: arg1
      4: arg0
      5: saved RDI								<==== unwindSP points here
      6: code cache return address
      7: last arg									<==== unwindSP should point here
   */
   return 2;
#elif defined(TR_HOST_X86)
   /* IA32 virtual resolve shape
      0: return address (picbuilder)
      1: indexAndLiteralsEA
      2: jitEIP
      3: saved eax									<== unwindSP points here
      4: saved edx
      5: saved edi
      6: return address (code cache)
      7: <last argument to method>			<== unwindSP should point here
   */
   return 4;
#elif defined(TR_HOST_POWER)
   /* PPC doesn't save anything extra */
   return 0;
#else
   return 0;
#endif
   }

/* The number of slots used by the picbuilder to push state required in data resolves */

UDATA getJitDataResolvePushes()
   {
#if defined(TR_HOST_X86) && defined(TR_HOST_64BIT)
   /* AMD64 data resolve shape
      16 slots of XMM registers
      16 slots of integer registers
      1 slot flags
      1 slot call from snippet
      1 slot call from codecache to snippet
   */
   return 35;
#elif defined(TR_HOST_X86)
   /* IA32 data resolve shape
      20 slots FP saves
      7 slots integer registers
      1 slot saved flags
      1 slot return address in snippet
      1 slot literals
      1 slot cpIndex
      1 slot call from codecache to snippet
   */
   return 32;
#elif defined(TR_HOST_ARM)
   /* ARM data resolve shape
      12 slots saved integer registers
   */
   return 12;
#elif defined(TR_HOST_S390)
   /* 390 data resolve shape
      16 integer registers
   */
#ifdef J9VM_JIT_32BIT_USES64BIT_REGISTERS
   return 32;
#else
   return 16;
#endif
#elif defined(TR_HOST_POWER)
   /* PPC data resolve shape
      32 integer registers
      CR
   */
   return 33;
#else
   return 0;
#endif
   }

/* Number of slots of data pushed after the integer registers during data resolves */

UDATA getJitSlotsBeforeSavesInDataResolve()
   {
#if defined(TR_HOST_X86) && defined(TR_HOST_64BIT)
   /* AMD64 data resolve shape
      16 slots of XMM registers
      16 slots of scalar registers
      1 slot flags
      1 slot call from snippet
      1 slot call from codecache to snippet
  */
   return 16;
#elif defined(TR_HOST_X86)
   /* IA32 data resolve shape
      20 slots FP saves
      7 slots integer registers
      1 slot saved flags
      1 slot return address in snippet
      1 slot literals
      1 slot cpIndex
      1 slot call from codecache to snippet
   */
   return 20;
#else
   return 0;
#endif
   }

uint8_t* getBeginningOfOSRSection(J9JITExceptionTable *metaData, uint32_t sectionIndex)
   {
   uint8_t* returnVal = (uint8_t*)metaData->osrInfo;
   uint32_t i;
   for (i = 0; i < sectionIndex; i++)
      {
      returnVal += *(uint32_t*)returnVal; /*the first uint32_t of a section is the section's length*/
      }
   return returnVal;
   }

/* returns OSR start address */
void* preOSR(J9VMThread* currentThread, J9JITExceptionTable *metaData, void *pc)
   {
   UDATA osrBlockOffset;
   uint8_t* callerIndex2OSRCatchBlock;
   uint32_t numberOfMapping, mapEntrySize;
   void* stackMap, *inlineMap;
   TR_ByteCodeInfo* bcInfo;
   assert(metaData);
   assert(metaData->osrInfo);

   jitGetMapsFromPC(currentThread->javaVM, metaData, (UDATA) pc, &stackMap, &inlineMap);
   bcInfo = (TR_ByteCodeInfo*) getByteCodeInfoFromStackMap(metaData, inlineMap);
/*
   printf("offset=%08X bytecode.caller=%d bytecode.index=%x\n",
      (UDATA)pc, bcInfo->_callerIndex, bcInfo->_byteCodeIndex);
*/
   callerIndex2OSRCatchBlock = getBeginningOfOSRSection(metaData, 1);
   callerIndex2OSRCatchBlock += sizeof(uint32_t); /*skip over the size of the section*/
   numberOfMapping = *(uint32_t*)callerIndex2OSRCatchBlock; callerIndex2OSRCatchBlock += sizeof(uint32_t);

   mapEntrySize = sizeof(uint32_t); /*size of a single offset PC (osrCatchBlockAddress)*/
   osrBlockOffset = *(uint32_t*)(callerIndex2OSRCatchBlock + (mapEntrySize *(bcInfo->_callerIndex+1)));

   return (void*) (osrBlockOffset + metaData->startPC);
   }

/* returns non-zero for "must force decompile" */
UDATA postOSR(J9VMThread* currentThread, J9JITExceptionTable *metaData, void *pc)
   {
   /*always return true for now*/
   return 1;
   }

/* returns 0 for FSD, non-zero for OSR */
UDATA usesOSR(J9VMThread* currentThread, J9JITExceptionTable *metaData)
   {
   assert(metaData != NULL);
   return metaData->osrInfo?1:0;
   }

UDATA osrScratchBufferSize(J9VMThread* currentThread, J9JITExceptionTable *metaData, void *pc)
   {
   uint8_t* instruction2SharedSlotMap;
   uint32_t maxScratchBufferSize;
   assert(metaData);
   assert(metaData->osrInfo);
   instruction2SharedSlotMap = getBeginningOfOSRSection(metaData, 0);
   /* skip over the size of the section*/
   instruction2SharedSlotMap += sizeof(uint32_t);
   maxScratchBufferSize = *(uint32_t*)instruction2SharedSlotMap;
   return (UDATA) maxScratchBufferSize;
   }


/* New hook for getting a JIT exception table.  This will be used as a pivot to help assimilate the implementation into the JIT. */
J9JITExceptionTable *
jitGetMetaDataFromPC(J9VMThread* currentThread, UDATA pc)
   {
   return jitGetExceptionTableFromPC(currentThread, pc);
   }
