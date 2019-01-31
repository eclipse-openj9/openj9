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
#ifndef OSRGUARDINSERTION_INCL
#define OSRGUARDINSERTION_INCL

#include <stdint.h>
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "control/RecompilationInfo.hpp"
#include "optimizer/HCRGuardAnalysis.hpp"

namespace TR { class Block; }

class TR_OSRGuardInsertion : public TR::Optimization
   {
   public:
   TR_OSRGuardInsertion(TR::OptimizationManager *manager)
      : TR::Optimization(manager)
      {}
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_OSRGuardInsertion(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   private:
   static const uint32_t defaultRematBlockLimit = 3;

   void removeHCRGuards(TR_BitVector &fearGeneratingNodes, TR_HCRGuardAnalysis* guardAnalysis);
   void removeRedundantPotentialOSRPointHelperCalls(TR_HCRGuardAnalysis* guardAnalysis);
   void cleanUpPotentialOSRPointHelperCalls();
   int32_t insertOSRGuards(TR_BitVector &fearGeneratingNodes);
   void performRemat(TR::TreeTop *osrPoint, TR::TreeTop *osrGuard, TR::TreeTop *rematDest);
   void generateTriggeringRecompilationTrees(TR::TreeTop *osrGuard, TR_PersistentMethodInfo::InfoBits reason);
   void collectFearFromOSRFearPointHelperCalls(TR_BitVector &fearGeneratingNodes, TR_HCRGuardAnalysis* guardAnalysis);
   void cleanUpOSRFearPoints();
   };
#endif
