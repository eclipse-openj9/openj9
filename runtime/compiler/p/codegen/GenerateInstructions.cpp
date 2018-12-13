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

#include "p/codegen/GenerateInstructions.hpp"

#include <algorithm>                           // for std::max
#include <stdint.h>                            // for int32_t, uint32_t, etc
#include <stdlib.h>                            // for NULL, atoi
#include "codegen/BackingStore.hpp"            // for TR_BackingStore
#include "codegen/CodeGenerator.hpp"           // for CodeGenerator
#include "codegen/FrontEnd.hpp"                // for feGetEnv, TR_FrontEnd
#include "codegen/InstOpCode.hpp"              // for InstOpCode, etc
#include "codegen/Instruction.hpp"             // for Instruction, etc
#include "codegen/MemoryReference.hpp"         // for MemoryReference
#include "codegen/Register.hpp"                // for Register
#include "codegen/Snippet.hpp"                 // for Snippet
#include "compile/Compilation.hpp"             // for Compilation
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/Processors.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"                      // for intptrj_t, uintptrj_t
#include "il/Block.hpp"                        // for Block
#include "il/ILOpCodes.hpp"                    // for ILOpCodes::BBEnd, etc
#include "il/ILOps.hpp"                        // for ILOpCode
#include "il/Node.hpp"                         // for Node
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"                      // for TreeTop
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"                    // for TR_ASSERT
#include "p/codegen/PPCInstruction.hpp"
#include "runtime/J9Runtime.hpp"

TR::Instruction *generateVirtualGuardNOPInstruction(TR::CodeGenerator *cg,  TR::Node *n, TR_VirtualGuardSite *site,
   TR::RegisterDependencyConditions *cond, TR::LabelSymbol *sym, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCVirtualGuardNOPInstruction(n, site, cond, sym, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCVirtualGuardNOPInstruction(n, site, cond, sym, cg);
   }
