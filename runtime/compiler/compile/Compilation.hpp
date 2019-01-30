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

#ifndef TR_COMPILATION_INCL
#define TR_COMPILATION_INCL

#include "compile/J9Compilation.hpp"

class TR_FrontEnd;
class TR_Memory;
class TR_OptimizationPlan;
class TR_ResolvedMethod;
namespace TR { class IlGenRequest; }
namespace TR { class Options; }
struct J9VMThread;

namespace TR
{
class OMR_EXTENSIBLE Compilation : public J9::CompilationConnector
   {
   public:

   Compilation(
         int32_t compThreadId,
         J9VMThread *j9vmThread,
         TR_FrontEnd *fe,
         TR_ResolvedMethod *method,
         TR::IlGenRequest &request,
         TR::Options &options,
         TR::Region &heapMemoryRegion,
         TR_Memory *memory,
         TR_OptimizationPlan *optimizationPlan,
         TR_RelocationRuntime *reloRuntime) :
      J9::CompilationConnector(
         compThreadId,
         j9vmThread,
         fe,
         method,
         request,
         options,
         heapMemoryRegion,
         memory,
         optimizationPlan,
         reloRuntime)
      {}

   ~Compilation() {}
   };
}

#endif
