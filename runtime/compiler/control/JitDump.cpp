/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

#include "control/JitDump.hpp"

#include "codegen/CodeGenerator.hpp"
#include "env/VMJ9.h"
#include "control/MethodToBeCompiled.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "env/ut_j9jit.h"
#include "env/VMAccessCriticalSection.hpp"
#include "ilgen/J9ByteCodeIlGenerator.hpp"
#if defined(J9VM_OPT_JITSERVER)
#include "control/JITServerHelpers.hpp"
#include "runtime/JITServerIProfiler.hpp"
#include "runtime/JITServerStatisticsThread.hpp"
#include "runtime/Listener.hpp"
#endif

UDATA
jitDumpSignalHandler(struct J9PortLibrary *portLibrary, U_32 gpType, void *gpInfo, void *arg)
   {
   TR_VerboseLog::writeLineLocked(TR_Vlog_JITDUMP, "vmThread = %p Recursive crash occurred. Aborting JIT dump.", reinterpret_cast<J9VMThread*>(arg));

   // Returning J9PORT_SIG_EXCEPTION_RETURN will make us come back to the same crashing instruction over and over
   //
   return J9PORT_SIG_EXCEPTION_RETURN;
   }

typedef struct DumpCurrentILParamenters
   {
   DumpCurrentILParamenters(
         TR::Compilation *comp,
         J9VMThread *vmThread,
         J9JITConfig *jitConfig,
         TR::FILE *logFile
      )  :
      _comp(comp),
      _vmThread(vmThread),
      _jitConfig(jitConfig),
      _logFile(logFile)
      {}

   TR::Compilation *_comp;
   J9VMThread      *_vmThread;
   J9JITConfig     *_jitConfig;
   TR::FILE        *_logFile;
   } DumpCurrentILParamenters;

static UDATA
blankDumpCurrentILSignalHandler(struct J9PortLibrary *portLibrary, U_32 gpType, void *gpInfo, void *arg)
   {
   J9VMThread *vmThread = (J9VMThread *) arg;
   TR_VerboseLog::writeLineLocked(TR_Vlog_JITDUMP, "vmThread=%p Crashed while printing out current IL.", vmThread);
   return J9PORT_SIG_EXCEPTION_RETURN;
   }

static void jitDumpFailedBecause(J9VMThread *currentThread, const char* message)
   {
   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseDump))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITDUMP, "JIT dump failed because %s", message);
   Trc_JIT_DumpFail(currentThread, message);
   return;
   }

static void stackWalkEndingBecause(const char* message)
   {
   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseDump))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITDUMP, "stack walk ending because %s", message);
   return;
   }

static UDATA dumpCurrentILProtected(J9PortLibrary *portLib, void * opaqueParameters)
   {
   DumpCurrentILParamenters *p = static_cast<DumpCurrentILParamenters *>(opaqueParameters);
   TR::Compilation *comp       = p->_comp;
   J9VMThread *vmThread        = p->_vmThread;
   J9JITConfig *jitConfig      = p->_jitConfig;
   TR::FILE *logFile           = p->_logFile;

   comp->findOrCreateDebug();

   TR::Options *options = comp->getOptions();
   TR_Debug *dbg = comp->getDebug();
   TR_J9VMBase * fe = TR_J9VMBase::get(jitConfig, vmThread);

   if (logFile != NULL)
      {
      comp->setOutFile(logFile);
      options->setOption(TR_TraceAll);
      options->setOption(TR_TraceKnownObjectGraph);
      dbg->setFile(logFile);

      trfprintf(logFile,"<currentIL>\n");

      // Print Bytecodes
      TR::IlGeneratorMethodDetails & details = comp->ilGenRequest().details();
      TR::ResolvedMethodSymbol *resolvedMethod = comp->getMethodSymbol();
      TR::SymbolReferenceTable *symRefTab = comp->getSymRefTab();
      TR_J9ByteCodeIlGenerator bci(details, resolvedMethod, fe, comp, symRefTab);
      bci.printByteCodes();

      dbg->printMethodHotness();
      comp->dumpMethodTrees("Trees");
      dbg->print(logFile, comp->getSymRefTab());

      int bitMask = J9VMSTATE_JIT_CODEGEN | 0x0000FF00; // 0xFF?? is Codegen Phase
      if ( ((vmThread->omrVMThread->vmState) & bitMask) == bitMask )  // if we are in the Codegen Phase
         {
         dbg->dumpMethodInstrs(logFile, "Post Binary Instructions", false, true);
         dbg->print(logFile,comp->cg()->getSnippetList());
         dbg->dumpMixedModeDisassembly();
         }
      else
         {
         // Tree verification is only valid during optimizations as it relies on consistent node counts which are only
         // valid before codegen, since the codegen will decrement the node counts as part of instruction selection
         comp->verifyTrees(comp->getMethodSymbol());
         comp->verifyBlocks(comp->getMethodSymbol());
         }

      trfprintf(logFile, "</currentIL>\n");
      }

   return 0;
   }

static void dumpCurrentIL(TR::Compilation *comp, J9VMThread *vmThread, J9JITConfig *jitConfig, TR::FILE *logFile )
   {
   /* Acquire VM Access */
   bool alreadyHaveVMAccess = ((vmThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS) != 0);
   bool haveAcquiredVMAccess = false;
   if (!alreadyHaveVMAccess)
      if (0 == vmThread->javaVM->internalVMFunctions->internalTryAcquireVMAccessWithMask(vmThread, J9_PUBLIC_FLAGS_HALT_THREAD_ANY_NO_JAVA_SUSPEND))
         haveAcquiredVMAccess = true;

   PORT_ACCESS_FROM_JITCONFIG(jitConfig);

   DumpCurrentILParamenters p(
      comp,
      vmThread,
      jitConfig,
      logFile
      );

   UDATA result = 0;

#if defined(J9VM_PORT_SIGNAL_SUPPORT)
   U_32 flags = J9PORT_SIG_FLAG_MAY_RETURN |
                J9PORT_SIG_FLAG_SIGSEGV | J9PORT_SIG_FLAG_SIGFPE |
                J9PORT_SIG_FLAG_SIGILL  | J9PORT_SIG_FLAG_SIGBUS;

   static char *noSignalWrapper = feGetEnv("TR_NoSignalWrapper");

   if (!noSignalWrapper && j9sig_can_protect(flags))
      {
      UDATA protectedResult;

      protectedResult = j9sig_protect((j9sig_protected_fn)dumpCurrentILProtected, static_cast<void *>(&p),
                                         (j9sig_handler_fn)blankDumpCurrentILSignalHandler, vmThread,
                                         flags, &result);
      }
   else
#endif
      result = dumpCurrentILProtected(privatePortLibrary, &p);


   /* Release VM Access */
   if (!alreadyHaveVMAccess)
      if (haveAcquiredVMAccess)
         vmThread->javaVM->internalVMFunctions->internalReleaseVMAccess(vmThread);
   }

#define STACK_WALK_DEPTH 16

// struct to remember a method for JIT dump
typedef struct TR_MethodToBeCompiledForDump {
   J9Method   *_method;
   void       *_oldStartPC;
   TR_Hotness  _optLevel;
} TR_MethodToBeCompiledForDump;

// Stack frame iterator. Iterates until STACK_WALK_DEPTH frames or the top are reached.
static UDATA logStackIterator(J9VMThread *currentThread, J9StackWalkState *walkState)
   {
   Trc_JIT_DumpWalkingFrame(currentThread);

   // stop iterating if the walk state is null
   if (!walkState)
      {
      stackWalkEndingBecause("got a null walkState");
      return J9_STACKWALK_STOP_ITERATING;
      }

   // get user data from walk state
   TR_MethodToBeCompiledForDump* jittedMethodsOnStack = (TR_MethodToBeCompiledForDump *) walkState->userData1;
   int *currentMethodIndex                            = (int *) walkState->userData2;

   // also stop iterating if passed user data is null
   if (currentMethodIndex == 0 || jittedMethodsOnStack == 0)
      {
      stackWalkEndingBecause("one or both user data are null");
      return J9_STACKWALK_STOP_ITERATING;
      }

   // also stop iterating if enough frames have been reached
   if ((*currentMethodIndex) >= STACK_WALK_DEPTH)
      {
      stackWalkEndingBecause("reached limit on number of methods to recompile");
      return J9_STACKWALK_STOP_ITERATING;
      }

   // if the frame has jit metadata, then it belongs to a JITed method
   if (walkState->jitInfo)
      {
      // NOTE: method is the one (J9Method, a.k.a. TR_OpaqueMethodBlock) that gets
      //       passed to IlGeneratorMethodDetails
      TR_ASSERT(walkState->method, "Found method metadata on the stack, but method is null.");

      // get method's body info (can be null)
      TR_PersistentJittedBodyInfo* bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC((void*) walkState->jitInfo->startPC);

      // get global configuration options
      TR::Options *options = TR::Options::getCmdLineOptions();

      // get global opt level
      // NOTE: getOptLevel returns -1 if it is not set
      TR_Hotness globalOptLevel = (TR_Hotness) (-1);
      if (options)
         globalOptLevel = (TR_Hotness) options->getFixedOptLevel();

      // add the method to our list ONLY if level can be determined; it can be determined
      // either from body info (if it exists), or from fixed level (if it was set)
      if (bodyInfo || (globalOptLevel != (-1)))
         {
         // set the method
         jittedMethodsOnStack[*currentMethodIndex]._method = walkState->method;

         // set the method's oldStartPC:
         //    if bodyInfo exists, then we can do a recompilation (use startPCAfterPreviousCompile)
         //    if not, then we can't, and we do a first-time compilation (use 0)
         if (bodyInfo)
            jittedMethodsOnStack[*currentMethodIndex]._oldStartPC = bodyInfo->getStartPCAfterPreviousCompile();
         else
            jittedMethodsOnStack[*currentMethodIndex]._oldStartPC = 0;

         // set the optLevel
         if (bodyInfo)
            jittedMethodsOnStack[*currentMethodIndex]._optLevel = bodyInfo->getHotness();
         else
            // NOTE: global optLevel is not -1, since we checked for that above
            jittedMethodsOnStack[*currentMethodIndex]._optLevel = globalOptLevel;

         // advance to the next method in the list
         *currentMethodIndex = (*(currentMethodIndex) + 1);
         }
      }

   return J9_STACKWALK_KEEP_ITERATING;
   }

/// Recompiles a method for the JIT dump
static TR_CompilationErrorCode recompileMethodForLog(
   J9VMThread         *vmThread,
   J9Method           *ramMethod,
   TR::CompilationInfo *compInfo,
   TR_J9VMBase        *frontendOfThread,
   TR_Hotness          optimizationLevel,
   bool                profilingCompile,
   void               *oldStartPC,
   TR::FILE *logFile
   )
   {
   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseDump))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITDUMP, "recompiling a method for log: %p", ramMethod);

   Trc_JIT_DumpCompilingMethod(vmThread, ramMethod, optimizationLevel, oldStartPC);

   // the request to use Log should be passed to the compilation, via optimizationPlan
   // then the option object created should use this to open the log; thus must create a new optimization plan
   // the right optlevel would be set during the Options setting
   TR_OptimizationPlan *plan = TR_OptimizationPlan::alloc(optimizationLevel);
   if (!plan)
      return compilationFailure;

   if (profilingCompile)
      plan->setInsertInstrumentation(true);

   // pass the log file to the compilation
   plan->setLogCompilation(logFile);

   bool successfullyQueued = false;

   trfprintf(logFile, "<logRecompilation>\n");

   // actually request the compilation
   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseDump))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITDUMP, "compileMethod() about to issued synchronously");
   TR_CompilationErrorCode compErrCode;

   // Set the VM state of the crashed thread so the diagnostic thread can use consume it
   compInfo->setVMStateOfCrashedThread(vmThread->omrVMThread->vmState);

   // create a compilation request
   // NOTE: operator new() is overridden, and takes a storage object as a parameter
   // TODO: this is indiscriminately compiling as J9::DumpMethodRequest, which is wrong;
   //       should be fixed by checking if the method is indeed DLT, and compiling DLT if so
      {
      J9::JitDumpMethodDetails details(ramMethod);
      compInfo->compileMethod(vmThread, details, oldStartPC, TR_no, &compErrCode, &successfullyQueued, plan);
      }

   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseDump))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITDUMP, "Crashing thread returned from compileMethod() with rc = %d", compErrCode);

   trfprintf(logFile, "</logRecompilation>\n");

   // if the request failed, get rid of the optimization plan we made
   if (!successfullyQueued)
      TR_OptimizationPlan::freeOptimizationPlan(plan);

   return compErrCode;
   }

omr_error_t
runJitdump(char *label, J9RASdumpContext *context, J9RASdumpAgent *agent)
   {
   J9VMThread *crashedThread = context->javaVM->internalVMFunctions->currentVMThread(context->javaVM);

   Trc_JIT_DumpStart(crashedThread);

#if defined(J9VM_OPT_JITSERVER)
   if (context && context->javaVM && context->javaVM->jitConfig)
      {
      J9JITConfig *jitConfig = context->javaVM->jitConfig;
      TR::CompilationInfo *compInfo = TR::CompilationInfo::get(context->javaVM->jitConfig);
      if (compInfo)
         {
         static char * isPrintJITServerMsgStats = feGetEnv("TR_PrintJITServerMsgStats");
         if (isPrintJITServerMsgStats)
            JITServerHelpers::printJITServerMsgStats(jitConfig, compInfo);
         if (feGetEnv("TR_PrintJITServerCHTableStats"))
            JITServerHelpers::printJITServerCHTableStats(jitConfig, compInfo);
         if (feGetEnv("TR_PrintJITServerIPMsgStats"))
            {
            if (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
               {
               TR_J9VMBase * vmj9 = (TR_J9VMBase *)(TR_J9VMBase::get(context->javaVM->jitConfig, 0));
               JITServerIProfiler *iProfiler = (JITServerIProfiler *)vmj9->getIProfiler();
               iProfiler->printStats();
               }
            }
         }
      }
#endif

   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseDump))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITDUMP, "JIT dump initiated. Crashed vmThread=%p", crashedThread);

   // if either one of the args is null, we can't really do anything
   if (crashedThread == 0 || label == 0)
      {
      jitDumpFailedBecause(crashedThread, "one or both arguments are null");
      return OMR_ERROR_NONE;
      }

   // if VM is gone, can't do anything either
   if (crashedThread->javaVM == 0)
      {
      jitDumpFailedBecause(crashedThread, "VM pointer is null");
      return OMR_ERROR_NONE;
      }

   // get a hold of jitConfig in order to later get compinfo and frontend
   J9JITConfig * jitConfig = crashedThread->javaVM->jitConfig;

   // if jitConfig is gone, then we can't do anything either
   if (jitConfig == 0)
      {
      jitDumpFailedBecause(crashedThread, "jitConfig is null");
      return OMR_ERROR_NONE;
      }
   // get global compinfo
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get(jitConfig);
   if (!compInfo)
      {
      jitDumpFailedBecause(crashedThread, "compInfo is null");
      return OMR_ERROR_NONE;
      }

   // Must be able to allocate a frontend for this thread
   TR_J9VMBase *frontendOfThread = TR_J9VMBase::get(jitConfig, crashedThread);
   if (!frontendOfThread)
      {
      jitDumpFailedBecause(crashedThread, "thread's frontend is missing");
      return OMR_ERROR_NONE;
      }

   // get global configuration options
   TR::Options *options = TR::Options::getCmdLineOptions();
   if (!options)
      {
      jitDumpFailedBecause(crashedThread, "No cmdLineOptions available");
      return OMR_ERROR_NONE;
      }


   // open log file, using the postfixed timestamp if specified
   TR::FILE *logFile;
   char tmp[1025];
   label = frontendOfThread->getFormattedName(tmp, 1025, label, NULL, false);
   logFile = trfopen(label, "ab", false);

   trfprintf(logFile,
      "<?xml version=\"1.0\" standalone=\"no\"?>\n"
      "<jitDump>\n"
      );

#if defined(J9VM_OPT_JITSERVER)
   if (context && context->javaVM && context->javaVM->jitConfig)
      {
      J9JITConfig *jitConfig = context->javaVM->jitConfig;
      TR::CompilationInfo *compInfo = TR::CompilationInfo::get(context->javaVM->jitConfig);
      if (compInfo &&
          compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
         {
         jitDumpFailedBecause(crashedThread, "Skipped jitdump at the JITServer");
         trfprintf(logFile, "Skipped jitdump at the JITServer\n");
         trfclose(logFile);
         return OMR_ERROR_NONE;
         }
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   
   // to avoid deadlock, release compilation monitor until we are no longer holding it
   while (compInfo->getCompilationMonitor()->owned_by_self())
      compInfo->releaseCompMonitor(crashedThread);

   // Release other monitors as well. In particular CHTable and classUnloadMonitor must not be held
   while (TR::MonitorTable::get()->getClassTableMutex()->owned_by_self())
      frontendOfThread->releaseClassTableMutex(false);

   //FIXME: how do I detect that someone is holding the classUnloadMonitor

   // get crashed thread's own compinfo
   TR::CompilationInfoPerThread *threadCompInfo = compInfo->getCompInfoForThread(crashedThread);

   // Crashes on the diagnostic thread should not be processed
   if (threadCompInfo && threadCompInfo->isDiagnosticThread())
      {
      jitDumpFailedBecause(crashedThread, "detected recursive crash");
      trfprintf(logFile, "Detected recursive crash. No log created.\n");
      trfclose(logFile);
      return OMR_ERROR_NONE;
      }

   // get the method currently being compiled
   TR_MethodToBeCompiled *currentMethodBeingCompiled = 0;
   if (threadCompInfo)
      currentMethodBeingCompiled = threadCompInfo->getMethodBeingCompiled();

   /*
    * at this stage, we know that we are good to orchestrate a dump
    */

   if (options->getOption(TR_VerboseDump))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITDUMP, "dump agent obtained necessary information to perform dump");

   // if we are currently compiling a method, wake everyone waiting for it to compile
   if (currentMethodBeingCompiled && currentMethodBeingCompiled->getMonitor())
      {
      currentMethodBeingCompiled->getMonitor()->enter();
      currentMethodBeingCompiled->getMonitor()->notifyAll();
      currentMethodBeingCompiled->getMonitor()->exit();
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseDump))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITDUMP, "Notified all waiting threads");
      }

   // disable all non-essential compilations
   compInfo->getPersistentInfo()->setDisableFurtherCompilation(true);
   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseDump))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITDUMP, "Disabled further compilation");

   // get the dump thread
   TR::CompilationInfoPerThread *recompilationThreadInfo = compInfo->getCompilationInfoForDumpThread();
   J9VMThread                  *recompilationThread = NULL;
   if (recompilationThreadInfo)
      recompilationThread = recompilationThreadInfo->getCompilationThread();

   // purge the compilation queue if a thread was found
   if (recompilationThread)
      {
      if (options->getVerboseOption(TR_VerboseDump))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITDUMP, "Diagnostic compilation thread available - purging compilation queue");
      compInfo->acquireCompMonitor(crashedThread);
      compInfo->purgeMethodQueue(compilationFailure); // compilationFailure is a TR_CompilationErrorCode
      compInfo->releaseCompMonitor(crashedThread);
      }

   try
      {
      // if our compinfo is null, we are an application thread
      if (threadCompInfo == 0)
         {
         if (options->getVerboseOption(TR_VerboseDump))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITDUMP, "Crashed in application thread");
         trfprintf(logFile, "#INFO: Crashed in application thread %p.\n", crashedThread);

         // only bother doing anything if we have a healthy compilation thread available
         if (recompilationThread)
            {
            // make space for methods to be recompiled
            // FIXME: this is on the stack... is the stack big enough?
            int currentMethodIndex = 0;
            TR_MethodToBeCompiledForDump jittedMethodsOnStack[STACK_WALK_DEPTH] = { 0 };

            // set up the stack walk object
            J9StackWalkState walkState;

            walkState.userData1 = (void *)jittedMethodsOnStack;
            walkState.userData2 = (void *)&currentMethodIndex;
            walkState.walkThread = crashedThread;
            walkState.skipCount = 0;
            walkState.maxFrames = STACK_WALK_DEPTH;
            walkState.flags = (
               // J9_STACKWALK_LINEAR |
               // J9_STACKWALK_START_AT_JIT_FRAME |
               // J9_STACKWALK_INCLUDE_NATIVES |
               // J9_STACKWALK_HIDE_EXCEPTION_FRAMES |
               // J9_STACKWALK_ITERATE_HIDDEN_JIT_FRAMES |
               J9_STACKWALK_VISIBLE_ONLY |
               J9_STACKWALK_SKIP_INLINES |
               J9_STACKWALK_COUNT_SPECIFIED |
               J9_STACKWALK_ITERATE_FRAMES
               );
            walkState.frameWalkFunction = logStackIterator;

            /*
             * NOTE [March 6th, 2013]:
             *
             *    Graham Chapman said:
             *
             *    This will make the stack walker jump back to the last
             *    interpreter transition point if a bad return address is found,
             *    rather than asserting.  You'll miss a bunch of frames, but
             *    there's really nothing better to be done in that case.
             */
            walkState.errorMode = J9_STACKWALK_ERROR_MODE_IGNORE;

            // actually walk the stack, adding all JITed methods to the queue
            compInfo->acquireCompMonitor(crashedThread);
            crashedThread->javaVM->walkStackFrames(crashedThread, &walkState);
            compInfo->releaseCompMonitor(crashedThread);

            if (options->getVerboseOption(TR_VerboseDump))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITDUMP, "found %d JITed methods on Java stack", currentMethodIndex);
            trfprintf(logFile, "#INFO: Found %d JITed methods on Java stack.\n", currentMethodIndex);

            // resume the compilation thread
            recompilationThreadInfo->resumeCompilationThread();

            // compile our methods
            for (int i = 0; i < currentMethodIndex; i++)
               {
               // skip if method is somehow null
               if (!(jittedMethodsOnStack[i]._method))
                  continue;

               TR_CompilationErrorCode compErrCode;
               compErrCode = recompileMethodForLog(
                  crashedThread,
                  jittedMethodsOnStack[i]._method,
                  compInfo,
                  frontendOfThread,
                  jittedMethodsOnStack[i]._optLevel,
                  false,
                  jittedMethodsOnStack[i]._oldStartPC,
                  logFile
               );
               } // for

            if (currentMethodIndex == 0)
               trfprintf(logFile, "#INFO: DUMP FAILED: no methods to recompile\n");

            if (options->getVerboseOption(TR_VerboseDump))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITDUMP, "recompilations complete");

            } // if recompilationThread
         else
            {
            trfprintf(logFile, "#INFO: DUMP FAILED: no diagnostic thread\n");
            jitDumpFailedBecause(crashedThread, "no thread available to compile for dump");
            }

         } // if threadcompinfo

      // if our compinfo is not null, we are a compilation thread
      else
         {
         if (options->getVerboseOption(TR_VerboseDump))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITDUMP, "crashed in compilation thread");
         trfprintf(logFile, "#INFO: Crashed in compilation thread %p.\n", crashedThread);

         // get current compilation
         TR::Compilation *comp = threadCompInfo->getCompilation();

         // if the compilation is in progress, dump interesting things from it and then recompile
         if (comp)
            {
            if (options->getVerboseOption(TR_VerboseDump))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITDUMP, "Found compilation object");

            // dump IL of current compilation
            dumpCurrentIL(comp, crashedThread, jitConfig, logFile);

            // if there was an available compilation thread, recompile the current method
            if (recompilationThread)
               {
               // only proceed to recompile if the method is a regular Java method
               if (currentMethodBeingCompiled &&
                  currentMethodBeingCompiled->getMethodDetails().isOrdinaryMethod())
                  {
                  // resume the healthy compilation thread
                  recompilationThreadInfo->resumeCompilationThread();  // TODO: Postpone this so that the thread does not get to sleep again
                  if (options->getVerboseOption(TR_VerboseDump))
                     TR_VerboseLog::writeLineLocked(TR_Vlog_JITDUMP, "Resuming DiagCompThread");

                  // get old start PC if method was available
                  void *oldStartPC = 0;
                  if (currentMethodBeingCompiled)
                     oldStartPC = currentMethodBeingCompiled->_oldStartPC;

                  {
                     // The current thread is a compilation thread which may or may not currently hold VM access. The request
                     // for recompilation to generate the jitdump will be performed synchronously for which the code path we
                     // will take will be the same as if we were an application thread. We will take the synchronous request
                     // path in `compileOnSeparateThread` which ends up releasing VM access prior to waiting on the diagnostic
                     // thread to finish the queued compile. To avoid deadlocks we must acquire VM access here.
                     TR::VMAccessCriticalSection requestSynchronousCompilation(TR_J9VMBase::get(jitConfig, crashedThread));

                     // request the compilation
                     TR_CompilationErrorCode compErrCode;
                     compErrCode = recompileMethodForLog(
                        crashedThread,
                        (J9Method *)(comp->getCurrentMethod()->getPersistentIdentifier()),
                        compInfo,
                        frontendOfThread,
                        (TR_Hotness)comp->getOptLevel(),
                        comp->isProfilingCompilation(),
                        oldStartPC,
                        logFile
                     );
                  }

                  if (options->getVerboseOption(TR_VerboseDump))
                     TR_VerboseLog::writeLineLocked(TR_Vlog_JITDUMP, "recompilation complete");
                  }
               else
                  {
                  trfprintf(logFile, "#INFO: DUMP FAILED: not recompiling DLT method\n");
                  jitDumpFailedBecause(crashedThread, "method was not a OrdinaryMethod");
                  }

               } // if recompilationThread
            else
               {
               trfprintf(logFile, "#INFO: DUMP FAILED: no diagnostic thread\n");
               jitDumpFailedBecause(crashedThread, "no thread available to compile for dump");
               }

            } // if comp
         else
            {
            trfprintf(logFile, "#INFO: DUMP FAILED: no compilation in progress to redo\n");
            jitDumpFailedBecause(crashedThread, "found no in-progress compilation to redo");
            }

         } // if threadcompinfo
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

      trfprintf(logFile, "\n=== EXCEPTION THROWN (%s) ===\n", exceptionName);
      }

   trfprintf(logFile, "</jitDump>\n");

   // flush and close log file
   trfflush(logFile);
   trfclose(logFile);

   // re-enable all non-essential compilations
   compInfo->getPersistentInfo()->setDisableFurtherCompilation(false);

   if (options && options->getVerboseOption(TR_VerboseDump))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITDUMP, "JIT dump complete");

   return OMR_ERROR_NONE;
   }
