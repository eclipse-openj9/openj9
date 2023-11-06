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

#include "runtime/RelocationRuntimeLogger.hpp"

#include "jitprotos.h"
#include "jilconsts.h"
#include "jvminit.h"
#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "j9cp.h"
#include "j9protos.h"
#include "rommeth.h"
#include "env/FrontEnd.hpp"
#include "control/CompilationThread.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "runtime/MethodMetaData.h"
#include "runtime/CodeRuntime.hpp"
#include "runtime/J9CodeCache.hpp"
#include "runtime/J9Runtime.hpp"
#include "runtime/RelocationRecord.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/RelocationTarget.hpp"

// RelocationRuntimeLogger class
static const char *headerTag = "relocatableDataRT";

TR_RelocationRuntimeLogger::TR_RelocationRuntimeLogger(TR_RelocationRuntime *reloRuntime)
   {
   _reloRuntime = reloRuntime;

   _jitConfig = reloRuntime->jitConfig();
   _logLocked = false;
   _headerWasLocked = false;
   _reloStartTime = 0;
   setupOptions(reloRuntime->options());
   _verbose = ((jitConfig()->javaVM->verboseLevel) & VERBOSE_RELOCATIONS) != 0;
   _verbose = _logEnabled;
   }

void
TR_RelocationRuntimeLogger::setupOptions(TR::Options *options)
   {
   _logEnabled = false;
   _logLevel = 0;
   if (options)
      {
      _logLevel = options->getAotrtDebugLevel();
      //_logLevel = TR::Options::getAOTCmdLineOptions()->getAotrtDebugLevel();
      _logEnabled = (_logLevel > 0);
      }
   }

void
TR_RelocationRuntimeLogger::debug_printf(const char *format, ...)
   {
   va_list args;
   char outputBuffer[512];

   J9JavaVM *javaVM = jitConfig()->javaVM;
   PORT_ACCESS_FROM_PORT(javaVM->portLibrary);
   va_start(args, format);
   j9str_vprintf(outputBuffer, 512, format, args);
   va_end(args);

   rtlogPrintLocked(jitConfig(), reloRuntime()->fej9()->_compInfoPT, outputBuffer);
   }

void
TR_RelocationRuntimeLogger::printf(const char *format, ...)
   {
   va_list args;
   char outputBuffer[512];

   J9JavaVM *javaVM = jitConfig()->javaVM;
   PORT_ACCESS_FROM_PORT(javaVM->portLibrary);

   va_start(args, format);
   j9str_vprintf(outputBuffer, 512, format, args);
   va_end(args);

   rtlogPrintLocked(jitConfig(), reloRuntime()->fej9()->_compInfoPT, outputBuffer);
   }

void
TR_RelocationRuntimeLogger::relocatableDataHeader()
   {
   if (logEnabled())
      {
      _headerWasLocked = lockLog();
      startTag(headerTag);
      method(true);
      }
   }

void
TR_RelocationRuntimeLogger::relocatableDataFooter()
   {
   if (logEnabled())
      {
      endTag(headerTag);
      unlockLog(_headerWasLocked);
      }
   }

void
TR_RelocationRuntimeLogger::method(bool newLine)
   {
   if (reloRuntime()->method() == NULL)
      return;

   const char *patternString = "%.*s.%.*s%.*s";
   if (newLine)
      patternString = "%.*s.%.*s%.*s\n";

   U_8 * className = J9UTF8_DATA(J9ROMCLASS_CLASSNAME(J9_CLASS_FROM_CP(reloRuntime()->ramCP())->romClass));
   int classNameLen = J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(J9_CLASS_FROM_CP(reloRuntime()->ramCP())->romClass));
   U_8 *methodName = J9UTF8_DATA(J9ROMMETHOD_NAME(J9_ROM_METHOD_FROM_RAM_METHOD(reloRuntime()->method())));
   int methodNameLen = J9UTF8_LENGTH(J9ROMMETHOD_NAME(J9_ROM_METHOD_FROM_RAM_METHOD(reloRuntime()->method())));
   U_8 *signature = J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(J9_ROM_METHOD_FROM_RAM_METHOD(reloRuntime()->method())));
   int signatureLen= J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(J9_ROM_METHOD_FROM_RAM_METHOD(reloRuntime()->method())));

   bool wasLocked = lockLog();
   rtlogPrintf(jitConfig(), reloRuntime()->fej9()->_compInfoPT, patternString,
               classNameLen, className, methodNameLen, methodName, signatureLen, signature);
   unlockLog(wasLocked);
   }

void
TR_RelocationRuntimeLogger::exceptionTable()
   {
   J9JITExceptionTable *data = reloRuntime()->exceptionTable();
   TR::CompilationInfoPerThread *compInfoPT = reloRuntime()->fej9()->_compInfoPT;

   bool wasLocked = lockLog();

   rtlogPrintf(jitConfig(), compInfoPT, "%-12s",   "startPC");
   rtlogPrintf(jitConfig(), compInfoPT, "%-12s",   "endPC");
   rtlogPrintf(jitConfig(), compInfoPT, "%-8s",    "size");
   rtlogPrintf(jitConfig(), compInfoPT, "%-14s",   "gcStackAtlas");
   rtlogPrintf(jitConfig(), compInfoPT, "%-12s\n", "bodyInfo");

   rtlogPrintf(jitConfig(), compInfoPT, "%-12p",   reinterpret_cast<void *>(data->startPC));
   rtlogPrintf(jitConfig(), compInfoPT, "%-12p",   reinterpret_cast<void *>(data->endPC));
   rtlogPrintf(jitConfig(), compInfoPT, "%-8x",    data->size);
   rtlogPrintf(jitConfig(), compInfoPT, "%-14p",   data->gcStackAtlas);
   rtlogPrintf(jitConfig(), compInfoPT, "%-12p\n", data->bodyInfo);

   rtlogPrintf(jitConfig(), compInfoPT, "%-12s\n", "inlinedCalls");
   rtlogPrintf(jitConfig(), compInfoPT, "%-12p\n", data->inlinedCalls);

   unlockLog(wasLocked);
   }

void
TR_RelocationRuntimeLogger::metaData()
   {
   if (logEnabled())
      {
      J9JavaVM *javaVM = jitConfig()->javaVM;
      const char *metaDataTag = "relocatableDataMetaDataRT";

      bool wasLocked = lockLog();
      startTag(metaDataTag);
      method(true);
      exceptionTable();
      endTag(metaDataTag);
      unlockLog(wasLocked);
      }
   }

void
TR_RelocationRuntimeLogger::relocationDump()
   {
   J9JavaVM *javaVM = jitConfig()->javaVM;
   PORT_ACCESS_FROM_JAVAVM(javaVM);
   _reloStartTime = (UDATA) j9time_usec_clock();
   }

void
TR_RelocationRuntimeLogger::relocationTime()
   {
   if (verbose())
      {
      J9JavaVM *javaVM = jitConfig()->javaVM;
      PORT_ACCESS_FROM_JAVAVM(javaVM);
      UDATA reloEndTime = j9time_usec_clock();

      bool wasLocked = lockLog();
      method(false);

      rtlogPrintf(jitConfig(), reloRuntime()->fej9()->_compInfoPT,
                  " <%p-%p> ",
                  reinterpret_cast<void *>(reloRuntime()->exceptionTable()->startPC),
                  reinterpret_cast<void *>(reloRuntime()->exceptionTable()->endPC));

      rtlogPrintf(jitConfig(), reloRuntime()->fej9()->_compInfoPT,
                  " Time: %d usec\n",
                  static_cast<uint32_t>(reloEndTime-_reloStartTime));

      unlockLog(wasLocked);
      }
   }

void
TR_RelocationRuntimeLogger::versionMismatchWarning()
   {
   const char *warningMessageFormat = "AOT major/minor versions don't match the ones of running JVM: aotMajorVersion %d jvmMajorVersion %d aotMinorVersion %d jvmMinorVersion %d   ";
   if (verbose())
      {
      bool wasLocked = lockLog();
      rtlogPrintf(jitConfig(), reloRuntime()->fej9()->_compInfoPT, warningMessageFormat,
                  reloRuntime()->aotMethodHeaderEntry()->majorVersion,
                  TR_AOTMethodHeader_MajorVersion,
                  reloRuntime()->aotMethodHeaderEntry()->minorVersion,
                  TR_AOTMethodHeader_MinorVersion);
      unlockLog(wasLocked);

      method(true);
      }
   }

void
TR_RelocationRuntimeLogger::maxCodeOrDataSizeWarning()
   {
   if (verbose())
      {
      PORT_ACCESS_FROM_JAVAVM(jitConfig()->javaVM);
      j9tty_printf(PORTLIB, "WARNING: Reached max size of runtime code cache or data cache!!! ");
      if (logEnabled())
         method(true);
      }
   }


// returns true if this call actually locked the log, false if it was already locked
bool
TR_RelocationRuntimeLogger::lockLog()
   {
   TR::CompilationInfoPerThread *compInfoPT = reloRuntime()->fej9()->_compInfoPT;
   if (!_logLocked && !(compInfoPT && compInfoPT->getRTLogFile()))
      {
      JITRT_LOCK_LOG(jitConfig());
      _logLocked = true;
      return true;
      }
   return false;
   }


// Should pass in the value returned by the corresponding call to lockLog
void
TR_RelocationRuntimeLogger::unlockLog(bool shouldUnlock)
   {
   if (shouldUnlock)
      {
      //TR_ASSERT(_logLocked, "Runtime log unlock request but not holding lock");
      JITRT_UNLOCK_LOG(jitConfig());
      _logLocked = false;
      }
   }

void
TR_RelocationRuntimeLogger::startTag(const char *tag)
   {
   rtlogPrintf(jitConfig(), reloRuntime()->fej9()->_compInfoPT, "<%s>\n", tag);
   }

void
TR_RelocationRuntimeLogger::endTag(const char *tag)
   {
   rtlogPrintf(jitConfig(), reloRuntime()->fej9()->_compInfoPT, "</%s>\n", tag);
   }
