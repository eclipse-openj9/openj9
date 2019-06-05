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

#ifndef methodmetadata_h
#define methodmetadata_h

/* @ddr_namespace: map_to_type=J9StackWalkFlags */

#ifdef J9VM_INTERP_STACKWALK_TRACING
#define jitExceptionHandlerSearch jitExceptionHandlerSearchVerbose
#define jitWalkStackFrames jitWalkStackFramesVerbose
#define jitGetStackMapFromPC jitGetStackMapFromPCVerbose
#define jitGetInlinerMapFromPC jitGetInlinerMapFromPCVerbose
#define jitGetMapsFromPC jitGetMapsFromPCVerbose
#define jitGetExceptionTableFromPC jitGetExceptionTableFromPCVerbose
#define jitCalleeSavedRegisterList jitCalleeSavedRegisterListVerbose
#define walkFrame walkFrameVerbose

#define getStackMapFromJitPC getStackMapFromJitPCVerbose
#define getStackAllocMapFromJitPC getStackAllocMapFromJitPCVerbose
#define getFirstInlineRange getFirstInlineRangeVerbose
#define getNextInlineRange getNextInlineRangeVerbose
#define walkJITFrameSlotsForInternalPointers walkJITFrameSlotsForInternalPointersVerbose
#define jitAddSpilledRegistersForDataResolve jitAddSpilledRegistersForDataResolveVerbose
#define jitAddSpilledRegisters jitAddSpilledRegistersVerbose
#define aotFixEndian aotFixEndianVerbose
#define aotWideExceptionEntriesFixEndian aotWideExceptionEntriesFixEndianVerbose
#define aot2ByteExceptionEntriesFixEndian aot2ByteExceptionEntriesFixEndianVerbose
#define aotExceptionEntryFixEndian aotExceptionEntryFixEndianVerbose
#define aotStackAtlasFixEndian aotStackAtlasFixEndianVerbose
#define aotMethodMetaDataFixEndian aotMethodMetaDataFixEndianVerbose
#define getObjectArgScanCursor getObjectArgScanCursorVerbose
#define getObjectTempScanCursor getObjectTempScanCursorVerbose
#define hasSyncObjectTemp hasSyncObjectTempVerbose
#define getSyncObjectTempScanCursor getSyncObjectTempScanCursorVerbose
#define getNextDescriptionBit getNextDescriptionBitVerbose
#define get32BitFirstExceptionDataField get32BitFirstExceptionDataFieldVerbose
#define get16BitFirstExceptionDataField get16BitFirstExceptionDataFieldVerbose
#define get32BitNextExceptionTableEntry get32BitNextExceptionTableEntryVerbose
#define get32BitNextExceptionTableEntryFSD get32BitNextExceptionTableEntryFSDVerbose
#define get16BitNextExceptionTableEntry get16BitNextExceptionTableEntryVerbose
#define get16BitNextExceptionTableEntryFSD get16BitNextExceptionTableEntryFSDVerbose
#define getJit32BitInterpreterPC getJit32BitInterpreterPCVerbose
#define getJit16BitInterpreterPC getJit16BitInterpreterPCVerbose
#define getJitDescriptionCursor getJitDescriptionCursorVerbose
#define get32BitByteCodeIndexFromExceptionTable get32BitByteCodeIndexFromExceptionTableVerbose
#define get16BitByteCodeIndexFromExceptionTable get16BitByteCodeIndexFromExceptionTableVerbose
#define getJitFirstInlinerMap getJitFirstInlinerMapVerbose
#define getJitNextInlinerMap getJitNextInlinerMapVerbose
#define getJitInlineDepth getJitInlineDepthVerbose
#define getJitFirstInlinedMethod getJitFirstInlinedMethodVerbose
#define getJitNextInlinedMethod getJitNextInlinedMethodVerbose
#define getVariablePortionInternalPtrRegMap getVariablePortionInternalPtrRegMapVerbose
#define getVariableLengthSizeOfInternalPtrRegMap getVariableLengthSizeOfInternalPtrRegMapVerbose
#define getNumberOfPinningArrays getNumberOfPinningArraysVerbose
#define getFirstPinningArray getFirstPinningArrayVerbose
#define getNextPinningArray getNextPinningArrayVerbose
#define getNumberOfInternalPtrs getNumberOfInternalPtrsVerbose
#define getFirstInternalPtr getFirstInternalPtrVerbose
#define getNextInternalPtr getNextInternalPtrVerbose
#define getFirstStackMap getFirstStackMapVerbose
#define getVariableLengthSizeOfInternalPtrMap getVariableLengthSizeOfInternalPtrMapVerbose
#define getIndexOfFirstInternalPtr getIndexOfFirstInternalPtrVerbose
#define getOffsetOfFirstInternalPtr getOffsetOfFirstInternalPtrVerbose
#define getNumberOfPinningArraysOfInternalPtrMap getNumberOfPinningArraysOfInternalPtrMapVerbose
#define getFirstInternalPtrMapPinningArrayCursor getFirstInternalPtrMapPinningArrayCursorVerbose
#define getNextInternalPtrMapPinningArrayCursor getNextInternalPtrMapPinningArrayCursorVerbose
#define getJitRegisterMap getJitRegisterMapVerbose
#define getNextDecriptionCursor getNextDecriptionCursorVerbose
#define getJitStackSlots getJitStackSlotsVerbose
#define getJitLiveMonitors getJitLiveMonitorsVerbose
#define getMonitorMask getMonitorMaskVerbose
#define getTempBase getTempBaseVerbose

#define getJitConstantPool getJitConstantPoolVerbose
#define getJitRamMethod getJitRamMethodVerbose
#define getJittedMethodStartPC getJittedMethodStartPCVerbose
#define getJittedMethodEndPC getJittedMethodEndPCVerbose
#define getJitTotalFrameSize getJitTotalFrameSizeVerbose
#define getJitSlots getJitSlotsVerbose
#define getJitScalarTempSlots getJitScalarTempSlotsVerbose
#define getJitObjectTempSlots getJitObjectTempSlotsVerbose
#define getJitProloguePushes getJitProloguePushesVerbose
#define getJitTempOffset getJitTempOffsetVerbose
#define getJitNumberOfExceptionRanges getJitNumberOfExceptionRangesVerbose
#define getJitExceptionTableSize getJitExceptionTableSizeVerbose
#define getJitGCStackAtlas getJitGCStackAtlasVerbose
#define getJitInlinedCallInfo getJitInlinedCallInfoVerbose
#define getJitInternalPointerMap getJitInternalPointerMapVerbose
#define getJitNumberOfMaps getJitNumberOfMapsVerbose
#define getJitNumberOfMapBytes getJitNumberOfMapBytesVerbose
#define getJitParmBaseOffset getJitParmBaseOffsetVerbose
#define getJitNumberOfParmSlots getJitNumberOfParmSlotsVerbose
#define getJitLocalBaseOffset getJitLocalBaseOffsetVerbose
#define getJit32BitTableEntryStartPC getJit32BitTableEntryStartPCVerbose
#define getJit32BitTableEntryEndPC getJit32BitTableEntryEndPCVerbose
#define getJit32BitTableEntryHandlerPC getJit32BitTableEntryHandlerPCVerbose
#define getJit32BitTableEntryCatchType getJit32BitTableEntryCatchTypeVerbose
#define getJit32BitTableEntryRamMethod getJit32BitTableEntryRamMethodVerbose
#define getJit16BitTableEntryStartPC getJit16BitTableEntryStartPCVerbose
#define getJit16BitTableEntryEndPC getJit16BitTableEntryEndPCVerbose
#define getJit16BitTableEntryHandlerPC getJit16BitTableEntryHandlerPCVerbose
#define getJit16BitTableEntryCatchType getJit16BitTableEntryCatchTypeVerbose
#define hasBytecodePC hasBytecodePCVerbose
#define hasWideExceptions hasWideExceptionsVerbose
#define hasFourByteOffset hasFourByteOffsetVerbose

#define getByteCodeInfoFromStackMap getByteCodeInfoFromStackMapVerbose
#define getNumInlinedCallSites getNumInlinedCallSitesVerbose
#define getFirstInlinedCallSiteWithByteCodeInfo getFirstInlinedCallSiteWithByteCodeInfoVerbose
#define getFirstInlinedCallSite getFirstInlinedCallSiteVerbose
#define getNextInlinedCallSite getNextInlinedCallSiteVerbose
#define hasMoreInlinedMethods hasMoreInlinedMethodsVerbose
#define getInlinedMethod getInlinedMethodVerbose
#define getByteCodeIndex getByteCodeIndexVerbose
#define getCallerIndex getCallerIndexVerbose
#define getByteCodeInfo getByteCodeInfoVerbose
#define getJitInlineDepthFromCallSite getJitInlineDepthFromCallSiteVerbose

#define markClassesInInlineRanges markClassesInInlineRangesVerbose
#define isUnloadedInlinedMethod isUnloadedInlinedMethodVerbose

#define getByteCodeIndexFromStackMap getByteCodeIndexFromStackMapVerbose
#define getCurrentByteCodeIndexAndIsSameReceiver getCurrentByteCodeIndexAndIsSameReceiverVerbose
#define getJitPCOffsetFromExceptionHandler getJitPCOffsetFromExceptionHandlerVerbose

#define getJitRecompilationResolvePushes getJitRecompilationResolvePushesVerbose
#define getJitStaticMethodResolvePushes getJitStaticMethodResolvePushesVerbose
#define getJitVirtualMethodResolvePushes getJitVirtualMethodResolvePushesVerbose
#define getJitDataResolvePushes getJitDataResolvePushesVerbose
#define getJitSlotsBeforeSavesInDataResolve getJitSlotsBeforeSavesInDataResolveVerbose

#define getInlinedCallSiteArrayElement getInlinedCallSiteArrayElementVerbose

#define jitReportDynamicCodeLoadEvents jitReportDynamicCodeLoadEventsVerbose
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "j9.h"
#include "j9consts.h"
#include "stackwalk.h"

struct J9JITExceptionTable;
struct J9JITStackAtlas;

typedef J9JITExceptionTable J9TR_MethodMetaData;
typedef J9JITStackAtlas J9TR_StackAtlas;

typedef struct TR_MapIterator
   {
   UDATA                 _rangeStartOffset;
   UDATA                 _rangeEndOffset;

   J9TR_MethodMetaData * _methodMetaData;
   J9JITStackAtlas *     _stackAtlas;
   U_8 *                 _currentMap;
   U_8 *                 _currentStackMap;
   U_8 *                 _currentInlineMap;
   U_8 *                 _nextMap;
   U_32                  _mapIndex;
   } TR_MapIterator;

   struct GpuMetaData
      {
      char* methodSignature;    //pointer to method signature
      int numPtxKernels;        //total number of PTX kernels in the method
      int maxNumCachedDevices;  //maximum number of devices that CUmodules can be cached for
      int* lineNumberArray;     //pointer to an array containing the source line number of each gpu kernel
      char** ptxArray;          //PTX code is stored as a char* string. This is an array that points to each entry.
      void* cuModuleArray;      //Array of cached CUmodules. One entry per PTX kernel and device combination
      };

#if defined(DEBUG)
#define JITINLINE
#else /* DEBUG */
#if defined(WINDOWS)
#define JITINLINE __forceinline
#elif ((__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1)) && !defined(J9VM_GC_REALTIME)
#define JITINLINE inline __attribute((always_inline))
#else
#define JITINLINE __inline
#endif /* WINDOWS */
#endif

/* @ddr_namespace: map_to_type=MethodMetaDataConstants */

#if defined(TR_HOST_POWER)
#define INTERNAL_PTR_REG_MASK 0x00040000
#else
#define INTERNAL_PTR_REG_MASK 0x80000000
#endif

#define GET_BYTECODEINFO_VALUE(fourByteOffset, stackMap) (*((U_32 *)((U_8 *)stackMap + SIZEOF_MAP_OFFSET(fourByteOffset))))

#define GET_REGISTER_MAP_CURSOR(fourByteOffset, stackMap) ((U_8 *)stackMap + SIZEOF_MAP_OFFSET(fourByteOffset) + 2*sizeof(U_32))
#define GET_SIZEOF_STACK_MAP_HEADER(fourByteOffset) 3*sizeof(U_32) + SIZEOF_MAP_OFFSET(fourByteOffset)
#define GET_REGISTER_SAVE_DESCRIPTION_CURSOR(fourByteOffset, stackMap) ((U_8 *)stackMap + SIZEOF_MAP_OFFSET(fourByteOffset) + sizeof(U_32))
#define ADDRESS_OF_REGISTERMAP(fourByteOffset, stackMap) ((U_8 *)stackMap + SIZEOF_MAP_OFFSET(fourByteOffset) + 2*sizeof(U_32))

#define GET_LOW_PC_OFFSET_VALUE(fourByteOffset, stackMap) (fourByteOffset ? *((U_32 *)ADDRESS_OF_LOW_PC_OFFSET_IN_STACK_MAP(fourByteOffset, stackMap)) : *((U_16 *)ADDRESS_OF_LOW_PC_OFFSET_IN_STACK_MAP(fourByteOffset, stackMap)))

#define GET_SIZEOF_BYTECODEINFO_MAP(fourByteOffset) sizeof(U_32) + SIZEOF_MAP_OFFSET(fourByteOffset)

#define ADDRESS_OF_BYTECODEINFO_IN_STACK_MAP(fourByteOffset, stackMap) ((U_8 *)stackMap + SIZEOF_MAP_OFFSET(fourByteOffset))
#define ADDRESS_OF_LOW_PC_OFFSET_IN_STACK_MAP(fourByteOffset, stackMap) stackMap

/* { RTSJ Support begins */
#define RANGE_NEEDS_FOUR_BYTE_OFFSET(r)   (((r) >= (USHRT_MAX   )) ? 1 : 0)
#define HAS_FOUR_BYTE_OFFSET(md) (((md)->flags & JIT_METADATA_GC_MAP_32_BIT_OFFSETS) ? 1 : 0)
/* } RTSJ Support ends */
#define SIZEOF_MAP_OFFSET(fourByteOffset) ((alignStackMaps || fourByteOffset) ? 4 : 2)

#define IS_BYTECODEINFO_MAP(fourByteOffset, stackMap) (((struct TR_ByteCodeInfo *)ADDRESS_OF_BYTECODEINFO_IN_STACK_MAP(fourByteOffset, stackMap))->_doNotProfile ? 1: 0)

#define GET_NEXT_STACK_MAP(fourByteOffset, stackMap, atlas, nextStackMap) \
   { \
   nextStackMap = stackMap; \
   if (IS_BYTECODEINFO_MAP(fourByteOffset, stackMap)) \
      nextStackMap += GET_SIZEOF_BYTECODEINFO_MAP(fourByteOffset); \
   else \
      { \
      nextStackMap = GET_REGISTER_MAP_CURSOR(fourByteOffset, stackMap); \
      if ((*(U_32*)nextStackMap & INTERNAL_PTR_REG_MASK) && atlas->internalPointerMap) \
         nextStackMap += (*(nextStackMap + 4) + 1);   \
      nextStackMap += 3 + atlas->numberOfMapBytes; \
      if (((*nextStackMap) & 128) != 0) \
         nextStackMap += atlas->numberOfMapBytes; \
      ++nextStackMap; \
      } \
   }

void * getStackMapFromJitPC(J9JavaVM * javaVM, J9TR_MethodMetaData * exceptionTable, UDATA jitPC);
void * getStackAllocMapFromJitPC(J9JavaVM * javaVM, J9TR_MethodMetaData * exceptionTable, UDATA jitPC, void *curStackMap);
UDATA jitExceptionHandlerSearch(J9VMThread * currentThread, J9StackWalkState * walkState);
void * jitGetInlinerMapFromPC(J9JavaVM * javaVM, J9JITExceptionTable * exceptionTable, UDATA jitPC);
void jitGetMapsFromPC(J9JavaVM * javaVM, J9JITExceptionTable * exceptionTable, UDATA jitPC, void * * inlineMap, void * * stackMap);
void * getFirstInlineRange(TR_MapIterator * i, void * methodMetaData, UDATA * startOffset, UDATA * endOffset);
void * getNextInlineRange(TR_MapIterator * i, UDATA * startOffset, UDATA * endOffset);
void walkJITFrameSlotsForInternalPointers(J9StackWalkState * walkState,  U_8 ** jitDescriptionCursor, UDATA * scanCursor, void *stackMap, J9JITStackAtlas *gcStackAtlas);
void jitAddSpilledRegistersForDataResolve(J9StackWalkState * walkState);
void jitAddSpilledRegisters(J9StackWalkState * walkState, void *stackMap);

void aotFixEndian(UDATA j9dst_priv);
void aotWideExceptionEntriesFixEndian(J9JITExceptionTable *exceptionTable);
void aot2ByteExceptionEntriesFixEndian(J9JITExceptionTable *exceptionTable);

void aotExceptionEntryFixEndian(J9JITExceptionTable *exceptionTable);
void aotStackAtlasFixEndian(J9JITStackAtlas *stackAtlas, J9JITExceptionTable *exceptionTable);
void aotMethodMetaDataFixEndian(J9JITExceptionTable *exceptionTable);

UDATA * getObjectArgScanCursor(J9StackWalkState *walkState);
UDATA * getObjectTempScanCursor(J9StackWalkState *walkState);
I_32    hasSyncObjectTemp(J9StackWalkState *walkState);
UDATA * getSyncObjectTempScanCursor(J9StackWalkState *walkState);
U_8 getNextDescriptionBit(U_8 ** jitDescriptionCursor);
JITINLINE J9JIT32BitExceptionTableEntry * get32BitFirstExceptionDataField(J9TR_MethodMetaData* metaData);
JITINLINE J9JIT16BitExceptionTableEntry * get16BitFirstExceptionDataField(J9TR_MethodMetaData* metaData);
JITINLINE J9JIT32BitExceptionTableEntry * get32BitNextExceptionTableEntry(J9JIT32BitExceptionTableEntry* handlerCursor);
JITINLINE J9JIT32BitExceptionTableEntry * get32BitNextExceptionTableEntryFSD(J9JIT32BitExceptionTableEntry* handlerCursor, UDATA fullSpeedDebug);
JITINLINE J9JIT16BitExceptionTableEntry * get16BitNextExceptionTableEntry(J9JIT16BitExceptionTableEntry* handlerCursor);
JITINLINE J9JIT16BitExceptionTableEntry * get16BitNextExceptionTableEntryFSD(J9JIT16BitExceptionTableEntry* handlerCursor, int fullSpeedDebug);

U_8 * getJit32BitInterpreterPC(U_8* bytecodes, J9JIT32BitExceptionTableEntry * handlerCursor);
U_8 * getJit16BitInterpreterPC(U_8* bytecodes, J9JIT16BitExceptionTableEntry * handlerCursor);
U_8 * getJitDescriptionCursor(void * stackMap, J9StackWalkState *walkState);

J9ConstantPool * getJitConstantPool(J9TR_MethodMetaData * md);
J9Method * getJitRamMethod(J9TR_MethodMetaData * md);
JITINLINE UDATA  getJittedMethodStartPC(J9TR_MethodMetaData * md);
JITINLINE UDATA  getJittedMethodEndPC(J9TR_MethodMetaData * md);
I_16   getJitTotalFrameSize(J9TR_MethodMetaData * md);
I_16   getJitSlots(J9TR_MethodMetaData *md);
I_16   getJitScalarTempSlots(J9TR_MethodMetaData * md);
I_16   getJitObjectTempSlots(J9TR_MethodMetaData * md);
JITINLINE U_16   getJitProloguePushes(J9TR_MethodMetaData * md);
JITINLINE I_16   getJitTempOffset(J9TR_MethodMetaData * md);
JITINLINE U_16   getJitNumberOfExceptionRanges(J9TR_MethodMetaData * md);
JITINLINE I_32   getJitExceptionTableSize(J9TR_MethodMetaData * md);
void * getJitGCStackAtlas(J9TR_MethodMetaData * md);
void * getJitInlinedCallInfo(J9TR_MethodMetaData * md);

U_8* getJitInternalPointerMap(J9TR_StackAtlas * sa);
U_16 getJitNumberOfMaps(J9TR_StackAtlas * sa);
U_16 getJitNumberOfMapBytes(J9TR_StackAtlas * sa);
I_16 getJitParmBaseOffset(J9TR_StackAtlas * sa);
U_16 getJitNumberOfParmSlots(J9TR_StackAtlas * sa);
JITINLINE I_16 getJitLocalBaseOffset(J9TR_StackAtlas * sa);

JITINLINE U_32 getJit32BitTableEntryStartPC(J9JIT32BitExceptionTableEntry * te);
JITINLINE U_32 getJit32BitTableEntryEndPC(J9JIT32BitExceptionTableEntry * te);
JITINLINE U_32 getJit32BitTableEntryHandlerPC(J9JIT32BitExceptionTableEntry * te);
JITINLINE U_32 getJit32BitTableEntryCatchType(J9JIT32BitExceptionTableEntry * te);
JITINLINE J9Method * getJit32BitTableEntryRamMethod(J9JIT32BitExceptionTableEntry * te);

JITINLINE U_16 getJit16BitTableEntryStartPC(J9JIT16BitExceptionTableEntry * te);
JITINLINE U_16 getJit16BitTableEntryEndPC(J9JIT16BitExceptionTableEntry * te);
JITINLINE U_16 getJit16BitTableEntryHandlerPC(J9JIT16BitExceptionTableEntry * te);
JITINLINE U_16 getJit16BitTableEntryCatchType(J9JIT16BitExceptionTableEntry * te);

JITINLINE UDATA hasBytecodePC(J9TR_MethodMetaData * md);
JITINLINE UDATA hasWideExceptions(J9TR_MethodMetaData * md);

JITINLINE U_32 * get32BitByteCodeIndexFromExceptionTable(J9TR_MethodMetaData * exceptionTable);
JITINLINE U_32 * get16BitByteCodeIndexFromExceptionTable(J9TR_MethodMetaData * exceptionTable);

U_8 * getVariablePortionInternalPtrRegMap(U_8 * mapBits, int fourByteOffsets);
U_8 getVariableLengthSizeOfInternalPtrRegMap(U_8 * internalPtrMapLocation);
U_8 getNumberOfPinningArrays(U_8 * internalPtrMapLocation);
U_8 * getFirstPinningArray(U_8 * internalPtrMapLocation);
U_8 * getNextPinningArray(U_8 * pinningArrayCursor);
U_8 getNumberOfInternalPtrs(U_8 * pinningArrayCursor);
U_8 * getFirstInternalPtr(U_8 * pinningArrayCursor);

U_8 * getNextInternalPtr(U_8 * pinningArrayCursor);

JITINLINE U_8 * getFirstStackMap(J9TR_StackAtlas * stackAtlas);

U_8 getVariableLengthSizeOfInternalPtrMap(U_8 * internalPtrMapLocation);
U_16 getIndexOfFirstInternalPtr(U_8 * internalPtrMapLocation);
U_16 getOffsetOfFirstInternalPtr(U_8 * internalPtrMapLocation);
U_8 getNumberOfPinningArraysOfInternalPtrMap(U_8 * internalPtrMapLocation);
U_8 * getFirstInternalPtrMapPinningArrayCursor(U_8 * internalPtrMapLocation);
U_8 * getNextInternalPtrMapPinningArrayCursor(U_8 * internalPtrMapPinningArrayCursor);
UDATA hasFourByteOffset(J9TR_MethodMetaData * md);

U_32 getJitRegisterMap(J9TR_MethodMetaData *md, void * stackMap);
U_8 * getNextDecriptionCursor(J9TR_MethodMetaData * md, void * stackMap, U_8 * jitDescriptionCursor);
U_8 * getJitStackSlots(J9TR_MethodMetaData * metadata, void * stackMap);
U_8 * getJitLiveMonitors(J9TR_MethodMetaData * metaData, void * stackMap);
U_8 * getMonitorMask(J9TR_StackAtlas *, void * inlinedCallSite);

UDATA * getTempBase(UDATA * bp, J9TR_MethodMetaData * metaData);

JITINLINE void * getByteCodeInfoFromStackMap(J9TR_MethodMetaData * metaData, void * stackMap);

U_32 getNumInlinedCallSites(J9JITExceptionTable * methodMetaData);
void * getFirstInlinedCallSiteWithByteCodeInfo(J9TR_MethodMetaData * methodMetaData, void * stackMap, void * byteCodeInfo);
void * getFirstInlinedCallSite(J9TR_MethodMetaData * metaData, void * stackMap);
void * getNextInlinedCallSite(J9TR_MethodMetaData * metaData, void *inlinedCallSite);
UDATA hasMoreInlinedMethods(void * inlinedCallSite);
void * getInlinedMethod(void * inlinedCallSite);

void markClassesInInlineRanges(void * methodMetaData, J9StackWalkState * walkState);
U_8 isUnloadedInlinedMethod(J9Method *method);

U_32 isPatchedValue(J9Method *m);

UDATA etByteCodeIndex(void *inlinedCallSite);
JITINLINE void * getByteCodeInfo(void *inlinedCallSite);

UDATA getJitInlineDepthFromCallSite(J9TR_MethodMetaData *metaData, void *inlinedCallSite);
UDATA getByteCodeIndexFromStackMap(J9TR_MethodMetaData *metaData, void *stackMap);

UDATA getCurrentByteCodeIndexAndIsSameReceiver(J9TR_MethodMetaData *metaData, void *stackMap, void *inlinedCallSite, UDATA *isSameReceiver);

UDATA getJitPCOffsetFromExceptionHandler(J9TR_MethodMetaData *metaData, void *jitPC);

UDATA getJitRecompilationResolvePushes(void);
UDATA getJitStaticMethodResolvePushes(void);
UDATA getJitVirtualMethodResolvePushes(void);
UDATA getJitDataResolvePushes(void);
UDATA getJitSlotsBeforeSavesInDataResolve(void);

U_8 * getInlinedCallSiteArrayElement(J9TR_MethodMetaData * methodMetaData, int cix);

/* OSR -- On-Stack Replacement */
/* returns OSR start address */
void* preOSR  (J9VMThread* currentThread, J9JITExceptionTable *metaData, void *pc);
/* returns non-zero for "must force decompile" */
UDATA postOSR (J9VMThread* currentThread, J9JITExceptionTable *metaData, void *pc);
/* returns 0 for FSD, non-zero for OSR */
UDATA usesOSR (J9VMThread* currentThread, J9JITExceptionTable *metaData);
/* returns the size required for the scratch buffer */
UDATA osrScratchBufferSize(J9VMThread* currentThread, J9JITExceptionTable *metaData, void *pc);

#if (defined(TR_HOST_ARM))
#define alignStackMaps 1
#else
#define alignStackMaps 0
#endif

J9JITExceptionTable * jitGetMetaDataFromPC(J9VMThread* currentThread, UDATA pc);

#ifdef __cplusplus
}
#endif

#endif
