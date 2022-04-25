/*******************************************************************************
 * Copyright (c) 2022, 2022 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "control/CompilationThread.hpp"
#include "microjit/x/amd64/AMD64Codegen.hpp"
#include "control/CompilationRuntime.hpp"
#include "compile/Compilation.hpp"
#include "codegen/OMRCodeGenerator.hpp"
#include "env/VerboseLog.hpp"
#include "env/ClassTableCriticalSection.hpp"
#include "runtime/CodeCacheTypes.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheManager.hpp"

#if defined(J9VM_OPT_MICROJIT)

static void
printCompFailureInfo(TR::Compilation * comp, const char * reason)
   {
   if (comp && comp->getOptions()->getAnyOption(TR_TraceAll))
      traceMsg(comp, "\n=== EXCEPTION THROWN (%s) ===\n", reason);
   }

// MicroJIT returns a code size of 0 when it encounters a compilation error
// We must use this to set the correct values and fail compilation, this is
// the same no matter which phase of compilation fails.
// This function will bubble the exception up to the caller after clean up.
static void
testMicroJITCompilationForErrors(
      uintptr_t code_size,
      J9Method *method,
      J9JITConfig *jitConfig,
      J9VMThread *vmThread,
      TR::Compilation *compiler)
   {
   if (0 == code_size)
      {
      J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
      intptr_t trCountFromMJIT = J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod) ? TR_DEFAULT_INITIAL_BCOUNT : TR_DEFAULT_INITIAL_COUNT;
      trCountFromMJIT = (trCountFromMJIT << 1) | 1;
      TR_J9VMBase *fe = TR_J9VMBase::get(jitConfig, vmThread);
      uint8_t *extendedFlags = fe->fetchMethodExtendedFlagsPointer(method);
      *extendedFlags = *extendedFlags | J9_MJIT_FAILED_COMPILE;
      method->extra = reinterpret_cast<void *>(trCountFromMJIT);
      compiler->failCompilation<MJIT::MJITCompilationFailure>("Cannot compile with MicroJIT.");
      }
   }

// This routine should only be called from wrappedCompile
TR_MethodMetaData *
TR::CompilationInfoPerThreadBase::mjit(
   J9VMThread *vmThread,
   TR::Compilation *compiler,
   TR_ResolvedMethod *compilee,
   TR_J9VMBase &vm,
   TR_OptimizationPlan *optimizationPlan,
   TR::SegmentAllocator const &scratchSegmentProvider,
   TR_Memory *trMemory
   )
   {

   PORT_ACCESS_FROM_JITCONFIG(jitConfig);

   TR_MethodMetaData *metaData = NULL;
   J9Method *method = NULL;
   TR::CodeCache *codeCache  = NULL;

   if (_methodBeingCompiled->_priority >= CP_SYNC_MIN)
      ++_compInfo._numSyncCompilations;
   else
      ++_compInfo._numAsyncCompilations;

   if (_methodBeingCompiled->isDLTCompile())
      testMicroJITCompilationForErrors(0, method, jitConfig, vmThread, compiler);

   bool volatile haveLockedClassUnloadMonitor = false; // used for compilation without VM access
   int32_t estimated_size = 0;
   char* buffer = NULL;
   try
      {

      InterruptibleOperation compilingMethodBody(*this);

      TR::IlGeneratorMethodDetails & details = _methodBeingCompiled->getMethodDetails();
      method = details.getMethod();

      TR_J9VMBase *fe = TR_J9VMBase::get(jitConfig, vmThread); // TODO: fe can be replaced by vm and removed from here.
      uint8_t *extendedFlags = fe->fetchMethodExtendedFlagsPointer(method);
      const char* signature = compilee->signature(compiler->trMemory());

      TRIGGER_J9HOOK_JIT_COMPILING_START(_jitConfig->hookInterface, vmThread, method);

      // BEGIN MICROJIT
      TR::FilePointer *logFileFP = getCompilation()->getOutFile();
      MJIT::ByteCodeIterator bcIterator(0, static_cast<TR_ResolvedJ9Method *> (compilee), static_cast<TR_J9VMBase *> (&vm), comp());

      if (TR::Options::canJITCompile())
         {
         TR::Options *options = TR::Options::getJITCmdLineOptions();

         J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
         U_16 maxLength = J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(romMethod));
         if (compiler->getOption(TR_TraceCG))
            trfprintf(logFileFP, "\n------------------------------\nMicroJIT Compiling %s...\n------------------------------\n", signature);
         if ((compilee->isConstructor()))
            testMicroJITCompilationForErrors(0, method, jitConfig, vmThread, compiler); // "MicroJIT does not currently compile constructors"

         bool isStaticMethod = compilee->isStatic();   // To know if the method we are compiling is static or not
         if (!isStaticMethod)
            maxLength += 1;                            // Make space for an extra character for object reference in case of non-static methods

         char typeString[maxLength];
         if (MJIT::nativeSignature(method, typeString))
            return 0;

         // To insert 'L' for object reference with non-static methods
         // before the input arguments starting at index 3.
         // The last iteration sets typeString[3] to typeString[2],
         // which is always MJIT::CLASSNAME_TYPE_CHARACTER, i.e., 'L'
         // and other iterations just move the characters one index down.
         // e.g. xxIIB ->
         if (!isStaticMethod)
            {
            for (int i = maxLength - 1; i > 2 ; i--)
               typeString[i] = typeString[i-1];
            }

         U_16 paramCount = MJIT::getParamCount(typeString, maxLength);
         U_16 actualParamCount = MJIT::getActualParamCount(typeString, maxLength);
         MJIT::ParamTableEntry paramTableEntries[actualParamCount*2];

         for (int i=0; i<actualParamCount*2; i++)
            paramTableEntries[i] = MJIT::initializeParamTableEntry();

         int error_code = 0;
         MJIT::RegisterStack stack = MJIT::mapIncomingParams(typeString, maxLength, &error_code, paramTableEntries, actualParamCount, logFileFP);
         if (error_code)
            testMicroJITCompilationForErrors(0, method, jitConfig, vmThread, compiler);

         MJIT::ParamTable paramTable(paramTableEntries, paramCount, actualParamCount, &stack);
#define MAX_INSTRUCTION_SIZE 15
#define MAX_TEMPLATE_SIZE 32 //instructions NOTE update if we ever make a template bigger than invokestatic, but it'll probably be fine
/* If the preprologue + prologue + cold area are more than 1024 bytes combines,
 *   we've got at least 68 instructions, but probably closer to 150.
 *   That is a lot of data to have outside the method's main line, so we'll assume
 *   this is enough, and adjust later if necessary
 */
#define MAX_BUFFER_SIZE(maxBCI) (192+(maxBCI*MAX_TEMPLATE_SIZE*MAX_INSTRUCTION_SIZE))
         uint32_t maxBCI = compilee->maxBytecodeIndex();

         MJIT::CodeGenGC mjitCGGC(logFileFP);
         MJIT::CodeGenerator mjit_cg(_jitConfig, vmThread, logFileFP, vm, &paramTable, compiler, &mjitCGGC, comp()->getPersistentInfo(), trMemory, compilee);
         estimated_size = MAX_BUFFER_SIZE(maxBCI);
         buffer = (char*)mjit_cg.allocateCodeCache(estimated_size, &vm, vmThread);
         testMicroJITCompilationForErrors((uintptr_t)buffer, method, jitConfig, vmThread, compiler); // MicroJIT cannot compile if allocation fails.
         codeCache = mjit_cg.getCodeCache();

         // provide enough space for CodeCacheMethodHeader
         char *cursor = buffer;

         buffer_size_t buffer_size = 0;

         char *magicWordLocation, *first2BytesPatchLocation, *samplingRecompileCallLocation;
         buffer_size_t code_size = mjit_cg.generatePrePrologue(cursor, method, &magicWordLocation, &first2BytesPatchLocation, &samplingRecompileCallLocation);

         testMicroJITCompilationForErrors((uintptr_t)code_size, method, jitConfig, vmThread, compiler);

         compiler->cg()->setPrePrologueSize(code_size);
         buffer_size += code_size;
         MJIT_ASSERT(logFileFP, buffer_size < MAX_BUFFER_SIZE(maxBCI), "Buffer overflow after pre-prologue");

#ifdef MJIT_DEBUG
         trfprintf(logFileFP, "\ngeneratePrePrologue\n");
         for (int32_t i = 0; i < code_size; i++)
            trfprintf(logFileFP, "%02x\n", ((unsigned char)cursor[i]) & (unsigned char)0xff);

#endif
         cursor += code_size;

         // start point should point to prolog
         char *prologue_address = cursor;

         // generate debug breakpoint
         if (comp()->getOption(TR_EntryBreakPoints))
            {
            code_size = mjit_cg.generateDebugBreakpoint(cursor);
            cursor += code_size;
            buffer_size += code_size;
            }

         char *jitStackOverflowPatchLocation = NULL;

         mjit_cg.setPeakStackSize(romMethod->maxStack * mjit_cg.getPointerSize());
         char *firstInstructionLocation = NULL;

         code_size = mjit_cg.generatePrologue(cursor, method, &jitStackOverflowPatchLocation, magicWordLocation, first2BytesPatchLocation, samplingRecompileCallLocation, &firstInstructionLocation, &bcIterator);
         testMicroJITCompilationForErrors((uintptr_t)code_size, method, jitConfig, vmThread, compiler);

         TR::GCStackAtlas *atlas = mjit_cg.getStackAtlas();
         compiler->cg()->setStackAtlas(atlas);
         compiler->cg()->setMethodStackMap(atlas->getLocalMap());
         // TODO: Find out why setting this correctly causes the startPC to report the jitToJit startPC and not the interpreter entry point
         // compiler->cg()->setJitMethodEntryPaddingSize((uint32_t)(firstInstructionLocation-cursor));
         compiler->cg()->setJitMethodEntryPaddingSize((uint32_t)(0));

         buffer_size += code_size;
         MJIT_ASSERT(logFileFP, buffer_size < MAX_BUFFER_SIZE(maxBCI), "Buffer overflow after prologue");

         if (compiler->getOption(TR_TraceCG))
            {
            trfprintf(logFileFP, "\ngeneratePrologue\n");
            for (int32_t i = 0; i < code_size; i++)
               trfprintf(logFileFP, "%02x\n", ((unsigned char)cursor[i]) & (unsigned char)0xff);
            }

         cursor += code_size;

         // GENERATE BODY
         bcIterator.setIndex(0);

         TR_Debug dbg(getCompilation());
         getCompilation()->setDebug(&dbg);

         if (compiler->getOption(TR_TraceCG))
            trfprintf(logFileFP, "\n%s\n", signature);

         code_size = mjit_cg.generateBody(cursor, &bcIterator);

         testMicroJITCompilationForErrors((uintptr_t)code_size, method, jitConfig, vmThread, compiler);

         buffer_size += code_size;
         MJIT_ASSERT(logFileFP, buffer_size < MAX_BUFFER_SIZE(maxBCI), "Buffer overflow after body");

#ifdef MJIT_DEBUG
         trfprintf(logFileFP, "\ngenerateBody\n");
         for (int32_t i = 0; i < code_size; i++)
            trfprintf(logFileFP, "%02x\n", ((unsigned char)cursor[i]) & (unsigned char)0xff);
#endif

         cursor += code_size;
         // END GENERATE BODY

         // GENERATE COLD AREA
         code_size = mjit_cg.generateColdArea(cursor, method, jitStackOverflowPatchLocation);

         testMicroJITCompilationForErrors((uintptr_t)code_size, method, jitConfig, vmThread, compiler);
         buffer_size += code_size;
         MJIT_ASSERT(logFileFP, buffer_size < MAX_BUFFER_SIZE(maxBCI), "Buffer overflow after cold-area");

#ifdef MJIT_DEBUG
         trfprintf(logFileFP, "\ngenerateColdArea\n");
         for (int32_t i = 0; i < code_size; i++)
            trfprintf(logFileFP, "%02x\n", ((unsigned char)cursor[i]) & (unsigned char)0xff);

         trfprintf(logFileFP, "\nfinal method\n");
         for (int32_t i = 0; i < buffer_size; i++)
            trfprintf(logFileFP, "%02x\n", ((unsigned char)buffer[i]) & (unsigned char)0xff);
#endif

         cursor += code_size;

         // As the body is finished, mark its profile info as active so that the JProfiler thread will inspect it

         // TODO: after adding profiling support, uncomment this.
         // if (bodyInfo && bodyInfo->getProfileInfo())
         //    {
         //    bodyInfo->getProfileInfo()->setActive();
         //    }

         // Put a metaData pointer into the Code Cache Header(s).
         compiler->cg()->setBinaryBufferCursor((uint8_t*)(cursor));
         compiler->cg()->setBinaryBufferStart((uint8_t*)(buffer));
         if (compiler->getOption(TR_TraceCG))
            trfprintf(logFileFP, "Compiled method binary buffer finalized [" POINTER_PRINTF_FORMAT " : " POINTER_PRINTF_FORMAT "] for %s @ %s", ((void*)buffer), ((void*)cursor), compiler->signature(), compiler->getHotnessName());

         metaData = createMJITMethodMetaData(vm, compilee, compiler);
         if (!metaData)
            {
            if (TR::Options::getVerboseOption(TR_VerboseCompilationDispatch))
               TR_VerboseLog::writeLineLocked(TR_Vlog_DISPATCH, "Failed to create metadata for %s @ %s", compiler->signature(), compiler->getHotnessName());
            compiler->failCompilation<J9::MetaDataCreationFailure>("Metadata creation failure");
            }
         if (TR::Options::getVerboseOption(TR_VerboseCompilationDispatch))
            TR_VerboseLog::writeLineLocked(TR_Vlog_DISPATCH, "Successfully created metadata [" POINTER_PRINTF_FORMAT "] for %s @ %s", metaData, compiler->signature(), compiler->getHotnessName());
         setMetadata(metaData);
         uint8_t *warmMethodHeader = compiler->cg()->getBinaryBufferStart() - sizeof(OMR::CodeCacheMethodHeader);
         memcpy(warmMethodHeader + offsetof(OMR::CodeCacheMethodHeader, _metaData), &metaData, sizeof(metaData));
         // FAR: should we do postpone this copying until after CHTable commit?
         metaData->runtimeAssumptionList = *(compiler->getMetadataAssumptionList());
         TR::ClassTableCriticalSection chTableCommit(&vm);
         TR_ASSERT(!chTableCommit.acquiredVMAccess(), "We should have already acquired VM access at this point.");
         TR_CHTable *chTable = compiler->getCHTable();

         if (prologue_address && compiler->getOption(TR_TraceCG))
            trfprintf(logFileFP, "\nMJIT:%s\n", compilee->signature(compiler->trMemory()));

         compiler->cg()->getCodeCache()->trimCodeMemoryAllocation(buffer, (cursor-buffer));

         // END MICROJIT
         if (compiler->getOption(TR_TraceCG))
            trfprintf(logFileFP, "\n------------------------------\nMicroJIT Compiled %s Successfully!\n------------------------------\n", signature);

         }

      logCompilationSuccess(vmThread, vm, method, scratchSegmentProvider, compilee, compiler, metaData, optimizationPlan);

      TRIGGER_J9HOOK_JIT_COMPILING_END(_jitConfig->hookInterface, vmThread, method);
      }
   catch (const std::exception &e)
      {
      const char *exceptionName;

#if defined(J9ZOS390)
      // Compiling with -Wc,lp64 results in a crash on z/OS when trying
      // to call the what() virtual method of the exception.
      exceptionName = "std::exception";
#else
      exceptionName = e.what();
#endif

      printCompFailureInfo(compiler, exceptionName);
      processException(vmThread, scratchSegmentProvider, compiler, haveLockedClassUnloadMonitor, exceptionName);
      if (codeCache)
         {
         if (estimated_size && buffer)
            {
            codeCache->trimCodeMemoryAllocation(buffer, 1);
            }
         TR::CodeCacheManager::instance()->unreserveCodeCache(codeCache);
         }
      metaData = 0;
      }

   // At this point the compilation has either succeeded and compilation cannot be
   // interrupted anymore, or it has failed. In either case _compilationShouldBeInterrupted flag
   // is not needed anymore
   setCompilationShouldBeInterrupted(0);

   // We should not have the classTableMutex at this point
   TR_ASSERT_FATAL(!TR::MonitorTable::get()->getClassTableMutex()->owned_by_self(),
                  "Should not still own classTableMutex");

   // Increment the number of JIT compilations (either successful or not)
   // performed by this compilation thread
   incNumJITCompilations();

   return metaData;
   }
#endif /* J9VM_OPT_MICROJIT */
