/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "infra/Checklist.hpp"                // fir NodeChecklist
#include "runtime/J9Profiler.hpp"
#include "optimizer/Structure.hpp"
/**
 * Class TR_JProfilingRecompLoopTest
 * ===================================
 *
 * When jProfiling in compilations are enabled, we have only one recompilation
 * test that counts the method invocation to trip up recompilation. For work loads
 * where we are spending a lot of time in loops, we are hitting a risk where we
 * are running into compiled method body with profiling trees if we just rely on
 * method invocations. This optimization scans the method body and puts 
 * recompilation test after asynccheck node to consider number of times a loop
 * is running.
 */
class TR_JProfilingRecompLoopTest : public TR::Optimization
   {
   public:
   // While doing the first walk over treetop, we collect  TreeTop after which we put recompilation test, corresponding Block and loop nesting depth
   // Following data structures are used to keep this information  
   typedef TR::typed_allocator<std::pair<std::pair<TR::TreeTop*, TR::Block*>, int32_t>, TR::Region &> RecompilationTestLocationInfoAllocator;
   typedef std::deque<std::pair<std::pair<TR::TreeTop*, TR::Block*>, int32_t>, RecompilationTestLocationInfoAllocator> RecompilationTestLocationsInfo;
   TR_JProfilingRecompLoopTest(TR::OptimizationManager *manager)
      : TR::Optimization(manager)
      {}
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_JProfilingRecompLoopTest(manager);
      }
   virtual int32_t perform();
   virtual const char *optDetailString() const throw();
   void addRecompilationTests(TR::Compilation *comp, RecompilationTestLocationsInfo &testLocations);
   bool isByteCodeInfoInCurrentTestLocationList(TR_ByteCodeInfo &bci, TR::list<TR_ByteCodeInfo, TR::Region&> &addedLocationBCIList);
   static int32_t maxLoopRecompilationThreshold;
   };
