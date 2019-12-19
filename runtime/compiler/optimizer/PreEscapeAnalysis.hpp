/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#ifndef PREESCAPEANALYSIS_INCL
#define PREESCAPEANALYSIS_INCL

#include <stdint.h>                           // for int32_t
#include "optimizer/Optimization.hpp"         // for Optimization
#include "optimizer/OptimizationManager.hpp"  // for OptimizationManager

namespace TR { class Block; class Node; }

// TODO:  Bring Doxygen comments up to date
/**
 * The \c TR_PreEscapeAnalysis optimization looks for calls to
 * \c OSRInductionHelper and adds a fake \c prepareForOSR call
 * that references all live auto symrefs and pending pushes.  Any object
 * that ends up as a candidate for stack allocation that appears to be
 * used by such a fake \c prepareForOSR call can be heapified at that
 * point.
 *
 * During \ref TR_EscapeAnalysis (EA) itself, those references on
 * \c prepareForOSR calls are marked as ignoreable for the purposes
 * of determining whether an object can be stack allocated.
 *
 * After EA, the \ref TR_PostEscapeAnalysis pass will remove the fake
 * calls to \c prepareForOSR.
 *
 * \see TR_EscapeAnalysis,TR_PostEscapeAnalysis
 */
class TR_PreEscapeAnalysis : public TR::Optimization
   {
   public:
   TR_PreEscapeAnalysis(TR::OptimizationManager *manager)
      : TR::Optimization(manager)
      {}
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_PreEscapeAnalysis(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();
   };

#endif
