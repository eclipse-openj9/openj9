/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

#include "codegen/CodeGenerator.hpp"
#include "codegen/Relocation.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/VMJ9.h"
#include "il/MethodSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/forward_list.hpp"
#include "infra/Link.hpp"
#include "objectfmt/GlobalFunctionCallData.hpp"
#include "objectfmt/JitCodeObjectFormat.hpp"
#include "runtime/Runtime.hpp"

namespace TR { class Instruction; }

void
J9::X86::AMD64::JitCodeObjectFormat::registerJNICallSite(TR::GlobalFunctionCallData &data)
   {
   // Register the instruction that loads the method address
   //
   TR::ResolvedMethodSymbol *resolvedMethodSymbol = data.globalMethodSymRef->getSymbol()->castToResolvedMethodSymbol();
   data.cg->getJNICallSites().push_front(new (data.cg->trHeapMemory()) TR_Pair<TR_ResolvedMethod, TR::Instruction>(resolvedMethodSymbol->getResolvedMethod(), data.out_loadInstr));
   }


uint8_t *
J9::X86::AMD64::JitCodeObjectFormat::encodeGlobalFunctionCall(TR::GlobalFunctionCallData &data)
   {
   uint8_t *buffer = J9::JitCodeObjectFormat::encodeGlobalFunctionCall(data);

   TR::CodeGenerator *cg = data.cg;
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->comp()->fe());

   if (fej9->needRelocationsForHelpers())
      {
      cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(
         data.out_encodedMethodAddressLocation,
         (uint8_t *)data.globalMethodSymRef,
         TR_HelperAddress,
         cg),
         __FILE__, __LINE__, data.callNode);
      }

   return buffer;
   }

