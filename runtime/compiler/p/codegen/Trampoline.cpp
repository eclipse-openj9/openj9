/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/jittypes.h"
#include "env/CompilerEnv.hpp"
#include "p/codegen/PPCTableOfConstants.hpp"
#include "runtime/J9Runtime.hpp"
#include "env/VMJ9.h"

namespace TR { class PersistentInfo; }

extern "C"
   {
   extern   int __j9_smp_flag;
   };

void * ppcPicTrampInit(TR_FrontEnd *vm, TR::PersistentInfo * persistentInfo)
   {
   void *retVal = 0;

#ifdef TR_HOST_POWER
   if (TR::Compiler->target.isSMP())
      __j9_smp_flag = -1;
   else
      __j9_smp_flag = 0;
#endif

#ifdef TR_TARGET_64BIT
   TR_J9VMBase *fej9 = (TR_J9VMBase *)vm;
   if (!fej9->isAOT_DEPRECATED_DO_NOT_USE() &&  // don't init TOC if it is jar2jxe AOT compile
       !TR::Options::getCmdLineOptions()->getOption(TR_DisableTOC))
      {
      retVal = TR_PPCTableOfConstants::initTOC(fej9, persistentInfo, 0);
      }
#endif

   return retVal;
   }

