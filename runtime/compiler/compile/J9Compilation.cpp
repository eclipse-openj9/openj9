/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#pragma csect(CODE,"TRJ9CompBase#C")
#pragma csect(STATIC,"TRJ9CompBase#S")
#pragma csect(TEST,"TRJ9CompBase#T")

#include "compile/J9Compilation.hpp"

#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/Instruction.hpp"
#include "compile/Compilation.hpp"
#include "compile/Compilation_inlines.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/OptimizationPlan.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/j9method.h"
#include "env/TRMemory.hpp"
#include "env/VMJ9.h"
#include "env/VMAccessCriticalSection.hpp"
#include "env/KnownObjectTable.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "ilgen/IlGenRequest.hpp"
#include "infra/List.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/TransformUtil.hpp"
#include "runtime/RuntimeAssumptions.hpp"
#include "runtime/J9Profiler.hpp"

#include "j9.h"
#include "j9cfg.h"


/*
 * There should be no allocations that use the global operator new, since
 * all allocations should go through the JitMemory allocation routines.
 * To catch cases that we miss, we define global operator new and delete here.
 * (xlC won't link staticly with the -noe flag when we override these.)
 */
bool firstCompileStarted = false;

// TODO disabled to allow for JITaaS
/*
void *operator new(size_t size)
   {
#if defined(DEBUG)
   #if LINUX
   // glibc allocates something at dl_init; check if a method is being compiled to avoid
   // getting assumes at _dl_init
   if (firstCompileStarted)
   #endif
      {
      printf( "\n*** ERROR *** Invalid use of global operator new\n");
      TR_ASSERT(0,"Invalid use of global operator new");
      }
#endif
   return malloc(size);
   }
*/

/**
 * Since we are using arena allocation, heap deletions must be a no-op, and
 * can't be used by JIT code, so we inject an assertion here.
 */

// TODO disabled to allow for JITaaS
/*
void operator delete(void *)
   {
   TR_ASSERT(0, "Invalid use of global operator delete");
   }
*/




uint64_t J9::Compilation::_maxYieldIntervalS = 0;

TR_CallingContext J9::Compilation::_sourceContextForMaxYieldIntervalS = NO_CONTEXT;

TR_CallingContext J9::Compilation::_destinationContextForMaxYieldIntervalS = NO_CONTEXT;

TR_Stats** J9::Compilation::_compYieldStatsMatrix = NULL;


const char * callingContextNames[] = {
   "FBVA_INITIALIZE_CONTEXT",
   "FBVA_ANALYZE_CONTEXT",
   "BBVA_INITIALIZE_CONTEXT",
   "BBVA_ANALYZE_CONTEXT",
   "GRA_ASSIGN_CONTEXT",
   "PRE_ANALYZE_CONTEXT",
   "AFTER_INSTRUCTION_SELECTION_CONTEXT",
   "AFTER_REGISTER_ASSIGNMENT_CONTEXT",
   "AFTER_POST_RA_SCHEDULING_CONTEXT",
   "BEFORE_PROCESS_STRUCTURE_CONTEXT",
   "GRA_FIND_LOOPS_AND_CORRESPONDING_AUTOS_BLOCK_CONTEXT",
   "GRA_AFTER_FIND_LOOP_AUTO_CONTEXT",
   "ESC_CHECK_DEFSUSES_CONTEXT",
   "LAST_CONTEXT"
};


J9::Compilation::Compilation(
      int32_t id,
      J9VMThread *j9vmThread,
      TR_FrontEnd *fe,
      TR_ResolvedMethod *compilee,
      TR::IlGenRequest &ilGenRequest,
      TR::Options &options,
      TR::Region &heapMemoryRegion,
      TR_Memory *m,
      TR_OptimizationPlan *optimizationPlan)
   : OMR::CompilationConnector(
      id,
      j9vmThread->omrVMThread,
      (firstCompileStarted = true, fe),
      compilee,
      ilGenRequest,
      options,
      heapMemoryRegion,
      m,
      optimizationPlan),
   _updateCompYieldStats(
      options.getOption(TR_EnableCompYieldStats) ||
      options.getVerboseOption(TR_VerboseCompYieldStats) ||
      TR::Options::_compYieldStatsHeartbeatPeriod > 0),
   _maxYieldInterval(0),
   _previousCallingContext(NO_CONTEXT),
   _sourceContextForMaxYieldInterval(NO_CONTEXT),
   _destinationContextForMaxYieldInterval(NO_CONTEXT),
   _needsClassLookahead(true),
   _reservedDataCache(NULL),
   _totalNeededDataCacheSpace(0),
   _aotMethodDataStart(NULL),
   _curMethodMetadata(NULL),
   _getImplInlineable(false),
   _vpInfoManager(NULL),
   _bpInfoManager(NULL),
   _methodBranchInfoList(getTypedAllocator<TR_MethodBranchProfileInfo*>(self()->allocator())),
   _externalVPInfoList(getTypedAllocator<TR_ExternalValueProfileInfo*>(self()->allocator())),
   _doneHWProfile(false),
   _hwpInstructions(m),
   _hwpBCMap(m),
   _sideEffectGuardPatchSites(getTypedAllocator<TR_VirtualGuardSite*>(self()->allocator())),
   _j9VMThread(j9vmThread),
   _monitorAutos(m),
   _monitorAutoSymRefsInCompiledMethod(getTypedAllocator<TR::SymbolReference*>(self()->allocator())),
   _classForOSRRedefinition(m),
   _classForStaticFinalFieldModification(m),
   _profileInfo(NULL),
   _skippedJProfilingBlock(false),
   _outOfProcessCompilation(false),
   _remoteCompilation(false)
   {
   _ObjectClassPointer   = fe->getClassFromSignature("Ljava/lang/Object;", 18, compilee);
   _RunnableClassPointer = fe->getClassFromSignature("Ljava/lang/Runnable;", 20, compilee);
   _StringClassPointer   = fe->getClassFromSignature("Ljava/lang/String;", 18, compilee);
   _SystemClassPointer   = fe->getClassFromSignature("Ljava/lang/System;", 18, compilee);
   _ReferenceClassPointer = fe->getClassFromSignature("Ljava/lang/ref/Reference;", 25, compilee);
   _JITHelpersClassPointer = fe->getClassFromSignature("Lcom/ibm/jit/JITHelpers;", 24, compilee);

   _aotClassClassPointer = NULL;
   _aotClassClassPointerInitialized = false;

   _aotGuardPatchSites = new (m->trHeapMemory()) TR::list<TR_AOTGuardSite*>(getTypedAllocator<TR_AOTGuardSite*>(self()->allocator()));

   _aotClassInfo = new (m->trHeapMemory()) TR::list<TR::AOTClassInfo*>(getTypedAllocator<TR::AOTClassInfo*>(self()->allocator()));

   if (_updateCompYieldStats)
      _hiresTimeForPreviousCallingContext = TR::Compiler->vm.getHighResClock(self());

   _profileInfo = new (m->trHeapMemory()) TR_AccessedProfileInfo(heapMemoryRegion);
   }

J9::Compilation::~Compilation()
   {
   _profileInfo->~TR_AccessedProfileInfo();
   }

TR_J9VMBase *
J9::Compilation::fej9()
   {
   return (TR_J9VMBase *)self()->fe();
   }

TR_J9VM *
J9::Compilation::fej9vm()
   {
   return (TR_J9VM *)self()->fe();
   }

void
J9::Compilation::updateCompYieldStatistics(TR_CallingContext callingContext)
   {
   // get time of this call
   //
   uint64_t crtTime = TR::Compiler->vm.getHighResClock(self());

   // compute the difference between 2 consecutive calls
   //
   static uint64_t hiresClockResolution = TR::Compiler->vm.getHighResClockResolution();
   uint64_t ticks = crtTime - _hiresTimeForPreviousCallingContext;
   uint64_t diffTime;

   if (hiresClockResolution < 1000000)
      diffTime = (ticks * 1000000)/hiresClockResolution;
   else
      diffTime = ticks / (hiresClockResolution/1000000);

   // update stats for the corresponding cell in the matrix
   // May lead to problems in the future when we add multiple compilation threads
   //
   if (self()->getOption(TR_EnableCompYieldStats))
      _compYieldStatsMatrix[(int32_t)_previousCallingContext][(int32_t)callingContext].update((double)diffTime);

   if (self()->getOptions()->getVerboseOption(TR_VerboseCompYieldStats))
      {
      if (diffTime > _maxYieldInterval)
         {
         _maxYieldInterval = diffTime;
         _sourceContextForMaxYieldInterval = _previousCallingContext;
         _destinationContextForMaxYieldInterval = callingContext;
         }
      }

   if (TR::Options::_compYieldStatsHeartbeatPeriod > 0)
      {
      if (diffTime > _maxYieldIntervalS)
         {
         _maxYieldIntervalS = diffTime;
         _sourceContextForMaxYieldIntervalS = _previousCallingContext;
         _destinationContextForMaxYieldIntervalS = callingContext;
         }
      }

   // prepare for next call
   //
   _hiresTimeForPreviousCallingContext = crtTime;
   _previousCallingContext = callingContext;
   }


void
J9::Compilation::allocateCompYieldStatsMatrix()
   {
   // need to use persistent memory
   _compYieldStatsMatrix = (TR_Stats**)TR::Compilation::jitPersistentAlloc(sizeof(TR_Stats *)*(int32_t)LAST_CONTEXT);

   for (int32_t i=0; i < (int32_t)LAST_CONTEXT; i++)
      {
      _compYieldStatsMatrix[i] = (TR_Stats *)TR::Compilation::jitPersistentAlloc(sizeof(TR_Stats)*(int32_t)LAST_CONTEXT);
      for (int32_t j=0; j < (int32_t)LAST_CONTEXT; j++)
         {
         char buffer[128];
         sprintf(buffer, "%d-%d", i,j);
         _compYieldStatsMatrix[i][j].setName(buffer);
         }
      }
   }

void
J9::Compilation::printCompYieldStats()
   {
   TR_VerboseLog::writeLine(TR_Vlog_PERF,"max yield-to-yield time of %u usec for ", (uint32_t)_maxYieldInterval);
   TR_VerboseLog::write("%s -", J9::Compilation::getContextName(_sourceContextForMaxYieldInterval));
   TR_VerboseLog::write("- %s", J9::Compilation::getContextName(_destinationContextForMaxYieldInterval));
   }

const char *
J9::Compilation::getContextName(TR_CallingContext context)
   {
   if (context == OMR::endOpts || context == TR_CallingContext::NO_CONTEXT)
      return "NO CONTEXT";
   else if (context < OMR::numOpts)
      return TR::Optimizer::getOptimizationName((OMR::Optimizations)context);
   else
      return callingContextNames[context-OMR::numOpts];
   }

void
J9::Compilation::printEntryName(int32_t i, int32_t j)
   {
   fprintf(stderr, "\n%s -", J9::Compilation::getContextName((TR_CallingContext) i));
   fprintf(stderr, "- %s\n", J9::Compilation::getContextName((TR_CallingContext) j));
   }


void
J9::Compilation::printCompYieldStatsMatrix()
   {
   if (!_compYieldStatsMatrix)
      return; // the matrix may not have been allocated (for instance when we give a bad command line option)

   for (int32_t i=0; i < (int32_t)LAST_CONTEXT; i++)
      {
      for (int32_t j=0; j < (int32_t)LAST_CONTEXT; j++)
         {
         TR_Stats *stats = &_compYieldStatsMatrix[i][j];
         if (stats->samples() > 0 && stats->maxVal() > TR::Options::_compYieldStatsThreshold)
            {
            TR::Compilation::printEntryName(i, j);
            stats->report(stderr);
            }
         }
      }
   }


TR::Node *
J9::Compilation::findNullChkInfo(TR::Node *node)
   {
   TR_ASSERT((node->getOpCodeValue() == TR::checkcastAndNULLCHK), "should call this only for checkcastAndNullChk\n");
   TR::Node * newNode = NULL;
   for (auto pair = self()->getCheckcastNullChkInfo().begin(); pair != self()->getCheckcastNullChkInfo().end(); ++pair)
      {
      if ((*pair)->getKey()->getByteCodeIndex() == node->getByteCodeIndex() &&
            (*pair)->getKey()->getCallerIndex() == node->getInlinedSiteIndex())
         {
         newNode = (*pair)->getValue();
         //dumpOptDetails("found bytecodeinfo for node %p as %x [%p]\n", node, newNode->getByteCodeIndex(), newNode);
         break;
         }
      }
   TR_ASSERT(newNode, "checkcastAndNullChk node doesnt have a corresponding null chk bytecodeinfo\n");
   return newNode;
   }


/**
 * Sometimes we start the compilation with an optLevel, but later on,
 * after we get more information, we decide to change it to something else.
 * This method is used to change the optLevel. Note that the optLevel
 * is cached in various data structures and it needs to be kept in sync.
 */
void
J9::Compilation::changeOptLevel(TR_Hotness newOptLevel)
   {
   self()->getOptions()->setOptLevel(newOptLevel);
   self()->getOptimizationPlan()->setOptLevel(newOptLevel);
   if (self()->getRecompilationInfo())
      {
      TR_PersistentJittedBodyInfo *bodyInfo = self()->getRecompilationInfo()->getJittedBodyInfo();
      if (bodyInfo)
         bodyInfo->setHotness(newOptLevel);
      }
   }


bool
J9::Compilation::isConverterMethod(TR::RecognizedMethod rm)
   {
   switch (rm)
      {
      case TR::sun_nio_cs_ISO_8859_1_Encoder_encodeISOArray:
      case TR::java_lang_StringCoding_implEncodeISOArray:
      case TR::sun_nio_cs_ISO_8859_1_Decoder_decodeISO8859_1:
      case TR::sun_nio_cs_US_ASCII_Encoder_encodeASCII:
      case TR::sun_nio_cs_US_ASCII_Decoder_decodeASCII:
      case TR::sun_nio_cs_ext_SBCS_Encoder_encodeSBCS:
      case TR::sun_nio_cs_ext_SBCS_Decoder_decodeSBCS:
      case TR::sun_nio_cs_UTF_8_Encoder_encodeUTF_8:
      case TR::sun_nio_cs_UTF_8_Decoder_decodeUTF_8:
      case TR::sun_nio_cs_UTF_16_Encoder_encodeUTF16Big:
      case TR::sun_nio_cs_UTF_16_Encoder_encodeUTF16Little:
         return true;
      default:
      	return false;
      }

   return false;
   }


//This implicitly checks if method is recognized converter method.
bool
J9::Compilation::canTransformConverterMethod(TR::RecognizedMethod rm)
   {
   TR_ASSERT(self()->isConverterMethod(rm), "not a converter method\n");

   if (self()->getOption(TR_DisableConverterReducer))
      return false;

   bool aot = self()->compileRelocatableCode();
   bool genSIMD = self()->cg()->getSupportsVectorRegisters() && !self()->getOption(TR_DisableSIMDArrayTranslate);
   bool genTRxx = !aot && self()->cg()->getSupportsArrayTranslateTRxx();

   switch (rm)
      {
      case TR::sun_nio_cs_ISO_8859_1_Encoder_encodeISOArray:
      case TR::java_lang_StringCoding_implEncodeISOArray:
         return genTRxx || self()->cg()->getSupportsArrayTranslateTRTO255() || self()->cg()->getSupportsArrayTranslateTRTO() || genSIMD;

      case TR::sun_nio_cs_ISO_8859_1_Decoder_decodeISO8859_1:
         return genTRxx || self()->cg()->getSupportsArrayTranslateTROTNoBreak() || genSIMD;

      case TR::sun_nio_cs_US_ASCII_Encoder_encodeASCII:
      case TR::sun_nio_cs_UTF_8_Encoder_encodeUTF_8:
         return genTRxx || self()->cg()->getSupportsArrayTranslateTRTO() || genSIMD;

      case TR::sun_nio_cs_US_ASCII_Decoder_decodeASCII:
      case TR::sun_nio_cs_UTF_8_Decoder_decodeUTF_8:
         return genTRxx || self()->cg()->getSupportsArrayTranslateTROT() || genSIMD;

      case TR::sun_nio_cs_ext_SBCS_Encoder_encodeSBCS:
         return genTRxx && self()->cg()->getSupportsTestCharComparisonControl();

      case TR::sun_nio_cs_ext_SBCS_Decoder_decodeSBCS:
         return genTRxx;

      // devinmp: I'm not sure whether these could be transformed in AOT, but
      // they haven't been so far.
      case TR::sun_nio_cs_UTF_16_Encoder_encodeUTF16Little:
         return !aot && self()->cg()->getSupportsEncodeUtf16LittleWithSurrogateTest();

      case TR::sun_nio_cs_UTF_16_Encoder_encodeUTF16Big:
         return !aot && self()->cg()->getSupportsEncodeUtf16BigWithSurrogateTest();

      default:
          return false;
      }
   }


bool
J9::Compilation::useCompressedPointers()
   {
   //FIXME: probably have to query the GC as well
   return (TR::Compiler->target.is64Bit() && TR::Options::useCompressedPointers());
   }


bool
J9::Compilation::useAnchors()
   {
   return (self()->useCompressedPointers());
   }


bool
J9::Compilation::hasBlockFrequencyInfo()
   {
   return TR_BlockFrequencyInfo::get(self()) != NULL;
   }

bool
J9::Compilation::isShortRunningMethod(int32_t callerIndex)
   {
      {
      const char *sig = NULL;
      if (callerIndex > -1)
         {
         // this should be more reliable, but needs verification as equivalent
         sig = self()->getInlinedResolvedMethod(callerIndex)->signature(self()->trMemory());
         }
      else
         sig = self()->signature();

      if (sig &&
          ((strncmp("java/lang/String.", sig, 17) == 0) ||
           (strncmp("java/util/HashMap.", sig, 18) == 0)   ||
           (strncmp("java/util/TreeMap.", sig, 18) == 0) ||
           (strncmp("java/math/DivisionLong.", sig, 23) == 0) ||
           (strncmp("com/ibm/xml/xlxp2/scan/util/XMLString.", sig, 38) == 0) ||
           (strncmp("com/ibm/xml/xlxp2/scan/util/SymbolMap.", sig, 38) == 0) ||
           (strncmp("java/util/Random.next(I)I",sig,25) == 0) ||
           (strncmp("java/util/zip/ZipFile.safeToUseModifiedUTF8", sig, 43) == 0) ||
           (strncmp("java/util/HashMap$HashIterator.", sig, 31) == 0) ||
           (strncmp("sun/misc/FloatingDecimal.readJavaFormatString", sig, 45) == 0)
          )
         )
         {
         return true;
         }
      }
   return false;
   }

bool
J9::Compilation::isRecompilationEnabled()
   {
   if (self()->isDLT())
      {
      return false;
      }

   // Don't do recompilation on JNI virtual thunk methods
   //
   if (self()->getCurrentMethod()->isJNINative())
      return false;

   return self()->allowRecompilation();
   }

bool
J9::Compilation::isJProfilingCompilation()
   {
   return self()->getRecompilationInfo() ? self()->getRecompilationInfo()->getJittedBodyInfo()->getUsesJProfiling() : false;
   }

// See if it is OK to remove this allocation node to e.g. merge it with others
// or allocate it locally on a stack frame.
// If so, return the allocation size if the size is constant, or zero if the
// size is variable.
// If not, return -1.
//
int32_t
J9::Compilation::canAllocateInlineOnStack(TR::Node* node, TR_OpaqueClassBlock* &classInfo)
   {
   if (self()->compileRelocatableCode())
      return -1;

   if (node->getOpCodeValue() == TR::New)
      {
      J9Class* clazz = self()->fej9vm()->getClassForAllocationInlining(self(), node->getFirstChild()->getSymbolReference());

      if (clazz == NULL)
         return -1;

      if (TR::Compiler->cls.isClassSpecialForStackAllocation((TR_OpaqueClassBlock *)clazz))
         return -1;
      }
   return self()->canAllocateInline(node, classInfo);
   }


bool
J9::Compilation::canAllocateInlineClass(TR_OpaqueClassBlock *block)
   {
   if (block == NULL)
      return false;

   return self()->fej9()->canAllocateInlineClass(block);
   }


// This code was previously in canAllocateInlineOnStack. However, it is required by code gen to
// inline heap allocations. The only difference, for now, is that inlined heap allocations
// are being enabled for AOT, but stack allocations are not (yet).
//
int32_t
J9::Compilation::canAllocateInline(TR::Node* node, TR_OpaqueClassBlock* &classInfo)
   {

   // Can't skip the allocation if we are generating JVMPI hooks, since
   // JVMPI needs to know about the allocation.
   //
   if (self()->suppressAllocationInlining() || !self()->fej9vm()->supportAllocationInlining(self(), node))
      return -1;

   // Pending inline allocation support on platforms for variable new
   //
   if (node->getOpCodeValue() == TR::variableNew || node->getOpCodeValue() == TR::variableNewArray)
      return -1;

   int32_t              size;
   TR::Node          * classRef;
   TR::SymbolReference * classSymRef;
   TR::StaticSymbol    * classSym;
   J9Class            * clazz;

   bool isRealTimeGC = self()->getOptions()->realTimeGC();

   bool generateArraylets = self()->generateArraylets();

   if (node->getOpCodeValue() == TR::New)
      {

      classRef    = node->getFirstChild();
      classSymRef = classRef->getSymbolReference();

      classSym    = classSymRef->getSymbol()->getStaticSymbol();

      // Check if the class can be inlined allocation.
      // The class has to be resolved, initialized, concrete, etc.
      clazz = self()->fej9vm()->getClassForAllocationInlining(self(), classSymRef);
      if (!self()->canAllocateInlineClass(reinterpret_cast<TR_OpaqueClassBlock*> (clazz)))
         return -1;

      classInfo = self()->fej9vm()->getClassOffsetForAllocationInlining(clazz);

      return self()->fej9()->getAllocationSize(classSym, reinterpret_cast<TR_OpaqueClassBlock*> (clazz));
      }

   int32_t elementSize;
   if (node->getOpCodeValue() == TR::newarray)
      {
      TR_ASSERT(node->getSecondChild()->getOpCode().isLoadConst(), "Expecting const child \n");

      int32_t arrayClassIndex = node->getSecondChild()->getInt();

      // TEMP HACK: getClassFromNewArrayType returns null under AOT because primitive array classes
      // are not in the SCC. However, here we require that the array class is actually resolved.
      // VP requires that getClassFromNewArrayType returns null under AOT for some reason so we cannot
      // just always resolve it.
      if (TR::CompilationInfo::getStream())
         {
         clazz = (J9Class *)self()->fej9()->getClassFromNewArrayType(arrayClassIndex);
         }
      else
         {
         struct J9Class ** arrayClasses = &self()->fej9()->getJ9JITConfig()->javaVM->booleanArrayClass;
         clazz = arrayClasses[arrayClassIndex - 4];
         }

      if (node->getFirstChild()->getOpCodeValue() != TR::iconst)
         {
         classInfo = self()->fej9vm()->getPrimitiveArrayAllocationClass(clazz);
         return 0;
         }

      // Make sure the number constant of elements requested is within reasonable bounds
      //
      TR_ASSERT(node->getFirstChild()->getOpCode().isLoadConst(), "Expecting const child \n");
      size = node->getFirstChild()->getInt();
      if (size < 0 || size > 0x000FFFFF)
         return -1;

      classInfo = self()->fej9vm()->getPrimitiveArrayAllocationClass(clazz);

      elementSize = TR::Compiler->om.getSizeOfArrayElement(node);
      }
   else if (node->getOpCodeValue() == TR::anewarray)
      {
      classRef      = node->getSecondChild();
      classSymRef   = classRef->getSymbolReference();

      // Can't skip the allocation if the class is unresolved
      //
      clazz = self()->fej9vm()->getClassForAllocationInlining(self(), classSymRef);
      if (clazz == NULL)
         return -1;

      auto classOffset = self()->fej9()->getArrayClassFromComponentClass(self()->fej9()->convertClassPtrToClassOffset(clazz));
      clazz = TR::Compiler->cls.convertClassOffsetToClassPtr(classOffset);
      if (!clazz)
         return -1;

      if (node->getFirstChild()->getOpCodeValue() != TR::iconst)
         {
         classInfo = self()->fej9vm()->getClassOffsetForAllocationInlining(clazz);
         return 0;
         }

      // Make sure the number of elements requested is in reasonable bounds
      //
      TR_ASSERT(node->getFirstChild()->getOpCode().isLoadConst(), "Expecting const child \n");
      size = node->getFirstChild()->getInt();
      if (size < 0 || size > 0x000FFFFF)
         return -1;

      classInfo = self()->fej9vm()->getClassOffsetForAllocationInlining(clazz);

      if (self()->useCompressedPointers())
         elementSize = TR::Compiler->om.sizeofReferenceField();
      else
         elementSize = (int32_t)(TR::Compiler->om.sizeofReferenceAddress());
      }


   TR_ASSERT(node->getOpCodeValue() == TR::newarray ||
          node->getOpCodeValue() == TR::anewarray, "unexpected allocation node");

   size *= elementSize;

   if (TR::Compiler->om.useHybridArraylets() && TR::Compiler->om.isDiscontiguousArray(size))
      {
      if (self()->getOption(TR_TraceCG))
         traceMsg(self(), "cannot inline array allocation @ node %p because size %d is discontiguous\n", node, size);
      return -1;
      }
   else if (!isRealTimeGC && size == 0)
      {
#if (defined(TR_HOST_S390) && defined(TR_TARGET_S390)) || (defined(TR_TARGET_X86) && defined(TR_HOST_X86)) || (defined(TR_TARGET_POWER) && defined(TR_HOST_POWER))
      size = TR::Compiler->om.discontiguousArrayHeaderSizeInBytes();
      if (self()->getOption(TR_TraceCG))
         traceMsg(self(), "inline array allocation @ node %p for size 0\n", node);
#else
      if (self()->getOption(TR_TraceCG))
         traceMsg(self(), "cannot inline array allocation @ node %p because size 0 is discontiguous\n", node);
      return -1;
#endif
      }
   else if (generateArraylets)
      {
      size += self()->fej9()->getArrayletFirstElementOffset(elementSize, self());
      }
   else
      {
      size += TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
      }

   if (node->getOpCodeValue() == TR::newarray || self()->useCompressedPointers())
      {
      size = (int32_t)((size+(TR::Compiler->om.sizeofReferenceAddress()-1)) & ~(TR::Compiler->om.sizeofReferenceAddress()-1));
      }

   if (isRealTimeGC &&
       ((size < 0) || (size > self()->fej9()->getMaxObjectSizeForSizeClass())))
      return -1;

   TR_ASSERT(size != -1, "unexpected array size");

   return size >= J9_GC_MINIMUM_OBJECT_SIZE ? size : J9_GC_MINIMUM_OBJECT_SIZE;
   }


TR::KnownObjectTable *
J9::Compilation::getOrCreateKnownObjectTable()
   {
   if (!_knownObjectTable && !self()->getOption(TR_DisableKnownObjectTable))
      {
      _knownObjectTable = new (self()->trHeapMemory()) TR::KnownObjectTable(self());
      }

   return _knownObjectTable;
   }


void
J9::Compilation::freeKnownObjectTable()
   {
   if (_knownObjectTable)
      {
      TR::VMAccessCriticalSection freeKnownObjectTable(self()->fej9());

      J9VMThread *thread = self()->fej9()->vmThread();
      TR_ASSERT(thread, "assertion failure");

      TR_ArrayIterator<uintptrj_t> i(&_knownObjectTable->_references);
      for (uintptrj_t *ref = i.getFirst(); !i.pastEnd(); ref = i.getNext())
         thread->javaVM->internalVMFunctions->j9jni_deleteLocalRef((JNIEnv*)thread, (jobject)ref);
      }

   _knownObjectTable = NULL;
   }


bool
J9::Compilation::compileRelocatableCode()
   {
   return self()->fej9()->isAOT_DEPRECATED_DO_NOT_USE();
   }


int32_t
J9::Compilation::maxInternalPointers()
   {
   if (self()->getOption(TR_DisableInternalPointers))
      return 0;
   else
      return 128;
   }


void
J9::Compilation::addHWPInstruction(TR::Instruction *instruction,
                                         TR_HWPInstructionInfo::type instructionType,
                                         void *data)
   {
   if (!self()->getPersistentInfo()->isRuntimeInstrumentationEnabled())
      return;

   TR::Node *node = instruction->getNode();

   switch (instructionType)
      {
      case TR_HWPInstructionInfo::callInstructions:
      case TR_HWPInstructionInfo::indirectCallInstructions:
         TR_ASSERT(node->getOpCode().isCall(), "Unknown instruction for HW profiling");
         break;
      case TR_HWPInstructionInfo::returnInstructions:
      case TR_HWPInstructionInfo::valueProfileInstructions:
         break;
      default:
         TR_ASSERT(false, "Unknown instruction for HW profiling");
      }

   TR_HWPInstructionInfo hwpInstructionInfo = {(void*)instruction,
                                               data,
                                               instructionType};

   _hwpInstructions.add(hwpInstructionInfo);
   }


void
J9::Compilation::addHWPCallInstruction(TR::Instruction *instruction, bool indirectCall, TR::Instruction *prev)
   {
   if (indirectCall)
      self()->addHWPInstruction(instruction, TR_HWPInstructionInfo::indirectCallInstructions, (void*)prev);
   else
      self()->addHWPInstruction(instruction, TR_HWPInstructionInfo::callInstructions);
   }


void
J9::Compilation::addHWPReturnInstruction(TR::Instruction *instruction)
   {
   self()->addHWPInstruction(instruction, TR_HWPInstructionInfo::returnInstructions);
   }


void
J9::Compilation::addHWPValueProfileInstruction(TR::Instruction *instruction)
   {
   self()->addHWPInstruction(instruction, TR_HWPInstructionInfo::valueProfileInstructions);
   }


void
J9::Compilation::verifyCompressedRefsAnchors()
   {
   vcount_t visitCount = self()->incVisitCount();

   TR::TreeTop *tt;
   for (tt = self()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      self()->verifyCompressedRefsAnchors(NULL, node, tt, visitCount);
      }
   }

void
J9::Compilation::verifyCompressedRefsAnchors(TR::Node *parent, TR::Node *node,
                                                   TR::TreeTop *tt, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   // check stores
   //
   if (node->getOpCode().isLoadIndirect() ||
         (node->getOpCode().isStoreIndirect() &&
            !node->getOpCode().isWrtBar()))
      {
      if (node->getSymbolReference()->getSymbol()->getDataType() == TR::Address &&
            node->getOpCode().isRef())
         TR_ASSERT(0, "indirect store %p not lowered!\n", node);
      }

   // check children for loads/stores
   //
   for (int32_t i = node->getNumChildren()-1; i >= 0; i--)
      {
      TR::Node *child = node->getChild(i);
      self()->verifyCompressedRefsAnchors(node, child, tt, visitCount);
      }
   }

bool
J9::Compilation::verifyCompressedRefsAnchors(bool anchorize)
   {
   bool status = true;

   vcount_t visitCount = self()->incVisitCount();
   TR::list<TR_Pair<TR::Node, TR::TreeTop> *> nodesList(getTypedAllocator<TR_Pair<TR::Node, TR::TreeTop> *>(self()->allocator()));
   TR::TreeTop *tt;
   for (tt = self()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      TR::Node *n = tt->getNode();
      self()->verifyCompressedRefsAnchors(NULL, n, tt, visitCount, nodesList);
      }

   // create anchors if required
   if (anchorize)
      {
      TR_Pair<TR::Node, TR::TreeTop> *info;
      // all non-null tt fields indicate some loads/stores were found
      // with no corresponding anchors
      //
      for (auto info = nodesList.begin(); info != nodesList.end(); ++info)
         {
         TR::TreeTop *tt = (*info)->getValue();
         if (tt)
            {
            TR::Node *n = (*info)->getKey();
            dumpOptDetails(self(), "No anchor found for load/store [%p]\n", n);
            if (TR::TransformUtil::fieldShouldBeCompressed(n, self()))
               {
               status = false;
               dumpOptDetails(self(), "placing anchor at [%p]\n", tt->getNode());
               TR::TreeTop *newTT = TR::TreeTop::create(self(),
                                                      TR::Node::createCompressedRefsAnchor( n),
                                                      NULL, NULL);
#if 0 ///#ifdef DEBUG
               TR_ASSERT(0, "No anchor found for load/store [%p]", n);
#else
               // place anchor after tt if its a check
               // otherwise before
               //
               TR::TreeTop *next = tt->getNextTreeTop();
               if (tt->getNode()->getOpCode().isCheck())
                  {
                  tt->join(newTT);
                  newTT->join(next);
                  }
               else
                  {
                  TR::TreeTop *prev = tt->getPrevTreeTop();
                  prev->join(newTT);
                  // Previously, the below path only applied to store nodes (hence
                  // the isTreeTop() check). However, it's now been made to apply to
                  // void-type nodes as well. This is to account for nodes such as
                  // TR::arrayset. Specifically, in the case where the child to be set
                  // in an arrayset node is an indirect reference (e.g static String),
                  // we need to treat the arrayset node as an indirect store (and compress
                  // the reference accordingly)
                  if (n->getOpCode().isTreeTop() || n->getOpCode().isVoid())
                     {
                     newTT->join(next);

                     // In the case where the void node's (e.g TR::arrayset) parent is
                     // not itself (e.g it's a TR::treetop), we anchor the arrayset node and it's children
                     // under a compressedRefs node and remove the original arrayset tree
                     // found under TR::treetop. The reference count of the arrayset node is
                     // incremented when we create the compressedRefs anchor, but not when
                     // we 'remove' the TR::treetop node. Hence we must recursively decrement
                     // here.
                     if (n != tt->getNode())
                        {
                        for (int i = 0; i < tt->getNode()->getNumChildren(); i++)
                           tt->getNode()->getChild(i)->recursivelyDecReferenceCount();
                        }
                     }
                  else
                     newTT->join(tt);
                  }
               status = true;
#endif
               }
            else
               dumpOptDetails(self(), "field at [%p] need not be compressed\n", n);
            }
         else
            dumpOptDetails(self(), "Anchor found for load/store [%p]\n", (*info)->getKey());
         }
      }
   return status;
   }


static TR_Pair<TR::Node, TR::TreeTop> *findCPtrsInfo(TR::list<TR_Pair<TR::Node, TR::TreeTop> *> &haystack,
                                                      TR::Node *needle)
   {
   for (auto info = haystack.begin(); info != haystack.end(); ++info)
      {
      if ((*info)->getKey() == needle)
         return *info;
      }
   return NULL;
   }


void
J9::Compilation::verifyCompressedRefsAnchors(TR::Node *parent, TR::Node *node,
                                                   TR::TreeTop *tt, vcount_t visitCount,
                                                   TR::list<TR_Pair<TR::Node, TR::TreeTop> *> &nodesList)
   {
   if (node->getVisitCount() == visitCount)
      return;

   // process loads/stores that are references
   //
   if ((node->getOpCode().isLoadIndirect() || node->getOpCode().isStoreIndirect()) &&
         node->getSymbolReference()->getSymbol()->getDataType() == TR::Address ||
            (node->getOpCodeValue() == TR::arrayset && node->getSecondChild()->getDataType() == TR::Address))
      {
      TR_Pair<TR::Node, TR::TreeTop> *info = findCPtrsInfo(nodesList, node);

      // check if the load/store is already under an anchor
      // if so, this load/store will be lowered correctly
      //
      if (parent && parent->getOpCodeValue() == TR::compressedRefs)
         {
         // set tt value to null to indicate success
         //
         if (info)
            info->setValue(NULL);

         // donot process this node again
         //
         node->setVisitCount(visitCount);
         }
      else
         {
         // either encountered the load/store for the first time in which
         // case record it,
         // -or-
         // its referenced multiple times in which case do nothing until
         // an anchor is found
         //
         if (!info)
            {
            // add node, tt to the nodesList
            TR_Pair<TR::Node, TR::TreeTop> *newVal = new (self()->trStackMemory()) TR_Pair<TR::Node, TR::TreeTop> (node, tt);
            nodesList.push_front(newVal);
            }
         }
      }
   else
      node->setVisitCount(visitCount);

   // process the children
   //
   for (int32_t i = node->getNumChildren()-1; i >=0; i--)
      {
      TR::Node *child = node->getChild(i);
      self()->verifyCompressedRefsAnchors(node, child, tt, visitCount, nodesList);
      }
   }


TR_VirtualGuardSite *
J9::Compilation::addSideEffectNOPSite()
   {
   TR_VirtualGuardSite *site = new /* (PERSISTENT_NEW)*/ (self()->trHeapMemory()) TR_VirtualGuardSite;
   _sideEffectGuardPatchSites.push_front(site);
   return site;
   }


TR_AOTGuardSite *
J9::Compilation::addAOTNOPSite()
   {
   TR_AOTGuardSite *site = new /* (PERSISTENT_NEW)*/ (self()->trHeapMemory()) TR_AOTGuardSite;
   _aotGuardPatchSites->push_front(site);
   return site;
   }


bool
J9::Compilation::isGeneratedReflectionMethod(TR_ResolvedMethod * method)
   {

   if (!method) return false;

   if (strstr(method->signature(self()->trMemory()), "sun/reflect/GeneratedMethodAccessor"))
      return true;

   return false;
   }


bool
J9::Compilation::compilationShouldBeInterrupted(TR_CallingContext callingContext)
   {
   return self()->fej9()->compilationShouldBeInterrupted(self(), callingContext);
   }


void
J9::Compilation::reportILGeneratorPhase()
   {
   self()->fej9()->reportILGeneratorPhase();
   }


void
J9::Compilation::reportAnalysisPhase(uint8_t id)
   {
   self()->fej9()->reportAnalysisPhase(id);
   }


void
J9::Compilation::reportOptimizationPhase(OMR::Optimizations opts)
   {
   self()->fej9()->reportOptimizationPhase(opts);
   }


void
J9::Compilation::reportOptimizationPhaseForSnap(OMR::Optimizations opts)
   {
   self()->fej9()->reportOptimizationPhaseForSnap(opts, self());
   }


TR::Compilation::CompilationPhase
J9::Compilation::saveCompilationPhase()
   {
   return self()->fej9()->saveCompilationPhase();
   }


void
J9::Compilation::restoreCompilationPhase(TR::Compilation::CompilationPhase phase)
   {
   self()->fej9()->restoreCompilationPhase(phase);
   }

void
J9::Compilation::addMonitorAuto(TR::RegisterMappedSymbol * a, int32_t callerIndex)
   {
   TR_Array<List<TR::RegisterMappedSymbol> *> & monitorAutos = self()->getMonitorAutos();
   List<TR::RegisterMappedSymbol> * autos = monitorAutos[callerIndex + 1];
   if (!autos)
      monitorAutos[callerIndex + 1] = autos = new (self()->trHeapMemory()) List<TR::RegisterMappedSymbol>(self()->trMemory());

   autos->add(a);
   }

void
J9::Compilation::addAsMonitorAuto(TR::SymbolReference* symRef, bool dontAddIfDLT)
   {
   symRef->getSymbol()->setHoldsMonitoredObject();
   int32_t siteIndex = self()->getCurrentInlinedSiteIndex();
   if (!self()->isPeekingMethod())
      {
      self()->addMonitorAuto(symRef->getSymbol()->castToRegisterMappedSymbol(), siteIndex);
      if (!dontAddIfDLT)
         {
         if (siteIndex == -1)
            self()->getMonitorAutoSymRefsInCompiledMethod()->push_front(symRef);
         }
      else
         {
         // only add the symref into the list for initialization when not in DLT and not peeking.
         // in DLT, we already use the corresp. slot to store the locked object from the interpreter
         // so initializing the symRef later in the block can overwrite the first store.
         if (!self()->isDLT() && siteIndex == -1)
            self()->getMonitorAutoSymRefsInCompiledMethod()->push_front(symRef);
         }
      }
   }

TR_OpaqueClassBlock *
J9::Compilation::getClassClassPointer(bool isVettedForAOT)
   {
   if (!isVettedForAOT)
      return _ObjectClassPointer ? self()->fe()->getClassClassPointer(_ObjectClassPointer) : 0;

   if (_aotClassClassPointerInitialized)
      return _aotClassClassPointer;

   _aotClassClassPointerInitialized = true;

   bool jlObjectVettedForAOT = true;
   TR_OpaqueClassBlock *jlObject = self()->fej9()->getClassFromSignature(
      "Ljava/lang/Object;",
      18,
      self()->getCurrentMethod(),
      jlObjectVettedForAOT);

   if (jlObject == NULL)
      return NULL;

   TR_OpaqueClassBlock *jlClass = self()->fe()->getClassClassPointer(jlObject);
   if (jlClass == NULL)
      return NULL;

   TR_ResolvedJ9Method *method = (TR_ResolvedJ9Method*)self()->getCurrentMethod();
   if (!method->validateArbitraryClass(self(), (J9Class*)jlClass))
      return NULL;

   _aotClassClassPointer = jlClass;
   return jlClass;
   }

/*
 * Adds the provided TR_OpaqueClassBlock to the set of those to trigger OSR Guard patching
 * on a redefinition.
 * Cheaper implementation would be a set, not an array.
 */
void
J9::Compilation::addClassForOSRRedefinition(TR_OpaqueClassBlock *clazz)
   {
   for (uint32_t i = 0; i < _classForOSRRedefinition.size(); ++i)
      if (_classForOSRRedefinition[i] == clazz)
         return;

   _classForOSRRedefinition.add(clazz);
   }

/*
 * Adds the provided TR_OpaqueClassBlock to the set of those to trigger OSR Guard patching
 * on a static final field modification.
 */
void
J9::Compilation::addClassForStaticFinalFieldModification(TR_OpaqueClassBlock *clazz)
   {
   // Class redefinition can also modify static final fields
   self()->addClassForOSRRedefinition(clazz);

   for (uint32_t i = 0; i < _classForStaticFinalFieldModification.size(); ++i)
      if (_classForStaticFinalFieldModification[i] == clazz)
         return;

   _classForStaticFinalFieldModification.add(clazz);
   }

/*
 * Controls if pending push liveness is stashed during IlGen to reduce OSRLiveRange
 * overhead.
 */
bool
J9::Compilation::pendingPushLivenessDuringIlgen()
   {
   static bool enabled = (feGetEnv("TR_DisablePendingPushLivenessDuringIlGen") == NULL);
   if (self()->getOSRMode() == TR::involuntaryOSR)
      return false;
   else return enabled;
   }

bool
J9::Compilation::supportsQuadOptimization()
   {
   if (self()->isDLT() || self()->getOption(TR_FullSpeedDebug))
      return false;
   return true;
   }


bool
J9::Compilation::notYetRunMeansCold()
   {
   if (self()->getOptimizer() && !(self()->getOptimizer()->isIlGenOpt()))
      return false;

   TR_ResolvedMethod *currentMethod = self()->getJittedMethodSymbol()->getResolvedMethod();

   intptrj_t initialCount = currentMethod->hasBackwardBranches() ?
                             self()->getOptions()->getInitialBCount() :
                             self()->getOptions()->getInitialCount();

   if (currentMethod->convertToMethod()->isBigDecimalMethod() ||
       currentMethod->convertToMethod()->isBigDecimalConvertersMethod())
       initialCount = 0;

    switch (currentMethod->getRecognizedMethod())
       {
       case TR::com_ibm_jit_DecimalFormatHelper_formatAsDouble:
       case TR::com_ibm_jit_DecimalFormatHelper_formatAsFloat:
          initialCount = 0;
          break;
       default:
          break;
       }

    if (currentMethod->containingClass() == self()->getStringClassPointer())
       {
       if (currentMethod->isConstructor())
          {
          char *sig = currentMethod->signatureChars();
          if (!strncmp(sig, "([CIIII)", 8) ||
              !strncmp(sig, "([CIICII)", 9) ||
              !strncmp(sig, "(II[C)", 6))
             initialCount = 0;
          }
       else
          {
          char *sig = "isRepeatedCharCacheHit";
          if (strncmp(currentMethod->nameChars(), sig, strlen(sig)) == 0)
             initialCount = 0;
          }
       }

   if (
      self()->isDLT()
      || (initialCount < TR_UNRESOLVED_IMPLIES_COLD_COUNT)
      || ((self()->getOption(TR_UnresolvedAreNotColdAtCold) && self()->getMethodHotness() == cold) || self()->getMethodHotness() < cold)
      || currentMethod->convertToMethod()->isArchetypeSpecimen()
      || (  self()->getCurrentMethod()
         && self()->getCurrentMethod()->convertToMethod()->isArchetypeSpecimen())
      )
      return false;
   else
      return true;
   }

