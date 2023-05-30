/*******************************************************************************
 * Copyright IBM Corp. and others 2022
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

#ifndef TR_COMPILEBEFORECHECKPOINT_INCL
#define TR_COMPILEBEFORECHECKPOINT_INCL

#if defined(J9VM_OPT_CRIU_SUPPORT)

#include <set>
#include "infra/TRlist.hpp"

namespace TR { class Region; }
namespace TR { class CompilationInfo; }
class TR_J9VMBase;
struct J9VMThread;

namespace TR
{

class CompileBeforeCheckpoint
   {
   public:
      typedef std::set<TR_OpaqueMethodBlock*,
                       std::less<TR_OpaqueMethodBlock*>,
                       TR::typed_allocator<TR_OpaqueMethodBlock*, TR::Region&>
                      > TR_MethodsSet;

      CompileBeforeCheckpoint(TR::Region &region, J9VMThread *vmThread, TR_J9VMBase *fej9, TR::CompilationInfo *compInfo);

      void collectAndCompileMethodsBeforeCheckpoint();
      void addMethodForCompilationBeforeCheckpoint(TR_OpaqueMethodBlock *method) { return addMethodToList(method); }

   private:
      void addMethodToList(TR_OpaqueMethodBlock *method);
      void collectMethodsForCompilationBeforeCheckpoint() {};
      void queueMethodsForCompilationBeforeCheckpoint();

      TR::Region &_region;
      J9VMThread *_vmThread;
      TR_J9VMBase *_fej9;
      TR::CompilationInfo *_compInfo;
      TR_MethodsSet _methodsSet;
   };

}
#endif // #if defined(J9VM_OPT_CRIU_SUPPORT)

#endif // TR_COMPILEBEFORECHECKPOINT_INCL
