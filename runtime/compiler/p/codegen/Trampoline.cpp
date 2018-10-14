/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#include <stddef.h>                           // for NULL
#include <stdint.h>                           // for uint8_t, int32_t, etc
#include "codegen/CodeGenerator.hpp"          // for CodeGenerator
#include "codegen/FrontEnd.hpp"               // for TR_FrontEnd
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"        // for TR::Options
#include "env/jittypes.h"                     // for uintptrj_t
#include "env/CompilerEnv.hpp"
#include "p/codegen/PPCTableOfConstants.hpp"  // for TR_PPCTableOfConstants
#include "runtime/Runtime.hpp"
#include "env/VMJ9.h"

namespace TR { class PersistentInfo; }

#if defined(TR_TARGET_64BIT)
#define TRAMPOLINE_SIZE       28
#define OFFSET_IPIC_TO_CALL   36
#else
#define TRAMPOLINE_SIZE       16
#define OFFSET_IPIC_TO_CALL   32
#endif

extern "C"
   {
   extern   int __j9_smp_flag;
   int32_t  ppcTrampolineInitByCodeCache(TR_FrontEnd *, uint8_t *, uintptrj_t);
   };

#ifdef TR_HOST_POWER
extern void     ppcCodeSync(uint8_t *, uint32_t);
#endif

#if defined(AIXPPC)
extern FILE    *j2Profile;
extern void     j2Prof_trampolineReport(uint8_t *startP, uint8_t *endP, int32_t num);
extern int32_t  j2Prof_initialize();
#endif


void * ppcPicTrampInit(TR_FrontEnd *vm, TR::PersistentInfo * persistentInfo)
   {
   void *retVal = 0;

#ifdef TR_HOST_POWER
   if (TR::Compiler->target.isSMP())
      __j9_smp_flag = -1;
   else
      __j9_smp_flag = 0;
#endif

#if defined(AIXPPC)
   if (TR::Options::getCmdLineOptions()->getOption(TR_CreatePCMaps))
      j2Prof_initialize();
#endif

#ifdef TR_TARGET_64BIT
   TR_J9VMBase *fej9 = (TR_J9VMBase *)vm;
   if (!fej9->isAOT_DEPRECATED_DO_NOT_USE()) // don't init TOC if it is jar2jxe AOT compile
      {
      retVal = TR_PPCTableOfConstants::initTOC(fej9, persistentInfo, 0);
      }
#endif

   return retVal;
   }

