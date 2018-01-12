/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#ifndef LIVEVARS_INCL
#define LIVEVARS_INCL

#include <stdint.h>                           // for int32_t
#include "optimizer/Optimization.hpp"         // for Optimization
#include "optimizer/OptimizationManager.hpp"  // for OptimizationManager

class TR_BitVector;
namespace TR { class Block; }

class TR_LocalLiveVariablesForGC : public TR::Optimization
   {
public:
   TR_LocalLiveVariablesForGC(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_LocalLiveVariablesForGC(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

private:
   void findGCPointInBlock(TR::Block *block, TR_BitVector &localsToBeInitialized);
   int32_t      _numLocals;
   };

class TR_GlobalLiveVariablesForGC : public TR::Optimization
   {
public:
   TR_GlobalLiveVariablesForGC(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_GlobalLiveVariablesForGC(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   };

#endif
