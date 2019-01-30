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

#include "optimizer/OptimizationManager.hpp"
#include "optimizer/OptimizationManager_inlines.hpp"

#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/Method.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "il/DataTypes.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Flags.hpp"
#include "optimizer/Optimizations.hpp"


namespace TR { class Optimizer; }
struct OptimizationStrategy;

J9::OptimizationManager::OptimizationManager(TR::Optimizer *o, OptimizationFactory factory, OMR::Optimizations optNum, const OptimizationStrategy *groupOfOpts)
      : OMR::OptimizationManager(o, factory, optNum, groupOfOpts)
   {
   // set flags if necessary
   switch (self()->id())
      {
      case OMR::dynamicLiteralPool:
         _flags.set(doesNotRequireAliasSets);
         break;
      case OMR::redundantMonitorElimination:
         _flags.set(requiresStructure | requiresLocalsValueNumbering);
         break;
      case OMR::escapeAnalysis:
         _flags.set(requiresStructure | checkStructure | dumpStructure |
                    requiresLocalsUseDefInfo | requiresLocalsValueNumbering | cannotOmitTrivialDefs);
         break;
      case OMR::globalLiveVariablesForGC:
         _flags.set(requiresStructure);
         break;
      case OMR::recompilationModifier:
         if (self()->comp()->getMethodHotness() > cold)
            self()->setRequiresStructure(true);
         break;
      case OMR::dataAccessAccelerator:
         _flags.set(doesNotRequireAliasSets | supportsIlGenOptLevel);
         break;
      case OMR::stringBuilderTransformer:
         _flags.set(doesNotRequireAliasSets | supportsIlGenOptLevel);
         break;
      case OMR::stringPeepholes:
         _flags.set(doesNotRequireAliasSets | supportsIlGenOptLevel);
         break;
      case OMR::switchAnalyzer:
         _flags.set(checkTheCFG);
         break;
      case OMR::idiomRecognition:
         _flags.set(requiresStructure | checkStructure | dumpStructure |
                    requiresLocalsUseDefInfo | requiresLocalsValueNumbering);
         if (self()->comp()->getMethodHotness() >= warm)
            _flags.set(requiresLocalsUseDefInfo | doesNotRequireLoadsAsDefs);
         break;
      case OMR::loopAliasRefiner:
         _flags.set(requiresStructure | checkStructure | dumpStructure);
         self()->setDoesNotRequireAliasSets(false);
         break;
      case OMR::allocationSinking:
         _flags.set(doesNotRequireAliasSets | supportsIlGenOptLevel);
         break;
      case OMR::varHandleTransformer:
         _flags.set(doesNotRequireAliasSets | supportsIlGenOptLevel);
         break;
      case OMR::unsafeFastPath:
         _flags.set(doesNotRequireAliasSets | supportsIlGenOptLevel);
         break;
      case OMR::recognizedCallTransformer:
         _flags.set(doesNotRequireAliasSets | supportsIlGenOptLevel);
         break;
      case OMR::samplingJProfiling:
         _flags.set(requiresStructure | checkStructure | dumpStructure);
         break;
      case OMR::SPMDKernelParallelization:
         _flags.set(requiresLocalsUseDefInfo | doesNotRequireLoadsAsDefs | requiresLocalsValueNumbering);
         break;
      case OMR::osrGuardInsertion:
         _flags.set(doNotSetFrequencies);
         break;
      case OMR::osrGuardRemoval:
         _flags.set(requiresStructure);
         break;
      case OMR::osrExceptionEdgeRemoval:
         _flags.set(doesNotRequireAliasSets);
         break;
      case OMR::jProfilingBlock:
         _flags.set(doesNotRequireAliasSets);
         break;
      case OMR::jProfilingRecompLoopTest:
         _flags.set(requiresStructure);
         break;
      default:
         // do nothing
         break;
      }
   }
