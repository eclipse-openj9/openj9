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

#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "env/CompilerEnv.hpp"
#include "il/DataTypes.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/NodePool.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "p/codegen/GenerateInstructions.hpp"
#include "p/codegen/PPCEvaluator.hpp"
#include "p/codegen/PPCInstruction.hpp"
#include "env/VMJ9.h"

extern TR::Instruction *loadConstant(TR::CodeGenerator *cg, TR::Node * node, int32_t value,
                              TR::Register *trgReg, TR::Instruction *cursor, bool isPicSite);
extern TR::Instruction *loadConstant(TR::CodeGenerator *cg, TR::Node * node, int64_t value,
                              TR::Register *trgReg, TR::Instruction *cursor, bool isPicSite);

extern bool loadAndEvaluateAsDouble(
      TR::Node* node,
      TR::Register*& reg,
      TR::CodeGenerator* cg)
   {
   if (node->getOpCodeValue() == TR::lloadi &&
      !node->getRegister() && node->getReferenceCount() == 1)
      {
      // change the opcode to indirect load double
      TR::Node::recreate(node, TR::dloadi);

      // evaluate into an FPR
      reg = cg->evaluate(node);
      cg->decReferenceCount(node);
      return true;
      }
   return false;
   }
