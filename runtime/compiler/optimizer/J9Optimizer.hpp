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

#ifndef J9_OPTIMIZER_INCL
#define J9_OPTIMIZER_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_OPTIMIZER_CONNECTOR
#define J9_OPTIMIZER_CONNECTOR
namespace J9 { class Optimizer; }
namespace J9 { typedef J9::Optimizer OptimizerConnector; }
#endif

#include "optimizer/OMROptimizer.hpp"

#include <stddef.h>
#include <stdint.h>

namespace TR { class Compilation; }
namespace TR { class Optimizer; }
namespace TR { class ResolvedMethodSymbol; }
struct OptimizationStrategy;
class TR_J9InlinerPolicy;
class TR_J9InlinerUtil;

namespace J9
{

class Optimizer : public OMR::OptimizerConnector
   {
   public:

   Optimizer(TR::Compilation *comp, TR::ResolvedMethodSymbol *methodSymbol, bool isIlGen,
         const OptimizationStrategy *strategy = NULL, uint16_t VNType = 0);

   OMR_InlinerPolicy *getInlinerPolicy();
   OMR_InlinerUtil *getInlinerUtil();

   bool switchToProfiling(uint32_t f, uint32_t c);
   bool switchToProfiling();

   static const OptimizationStrategy *optimizationStrategy(TR::Compilation *c);
   static ValueNumberInfoBuildType valueNumberInfoBuildType();

   private:

   TR::Optimizer *self();
   };

}

#endif
