/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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
#ifndef JPROFILINGVALUE_INCL
#define JPROFILINGVALUE_INCL

#include <stdint.h>
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "infra/Checklist.hpp"                // fir NodeChecklist

#include "runtime/J9ValueProfiler.hpp"

class TR_JProfilingValue : public TR::Optimization
   {
   public:

   TR_JProfilingValue(TR::OptimizationManager *manager)
      : TR::Optimization(manager)
      {}
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_JProfilingValue(manager);
      }

   virtual int32_t perform();
   virtual const char *optDetailString() const throw();

   void lowerCalls();
   void removeRedundantProfilingValueCalls();
   void addProfiling(TR::Node *address, TR::TreeTop *tt);
   void addVFTProfiling(TR::Node *address, TR::TreeTop *tt, bool addNullCheck);
   void performOnNode(TR::Node *node, TR::TreeTop *tt, TR::NodeChecklist *checklist);

   static bool addProfilingTrees(
      TR::Compilation *comp,
      TR::TreeTop *insertionPoint,
      TR::Node *value,
      TR_AbstractHashTableProfilerInfo *table,
      TR::Node *optionalTest = NULL,
      bool extendBlocks = true,
      bool trace = false);

   private:
   static TR::Node *computeHash(TR::Compilation *comp, TR_AbstractHashTableProfilerInfo *table, TR::Node *value, TR::Node *baseAddr);

   // Helpers for inserting the profiling trees
   static void replaceNode(TR::Node* check, TR::Node* origNode, TR::Node *newNode, TR::NodeChecklist &checklist);
   static void replaceNode(TR::Compilation *comp, TR::TreeTop *blockStart, TR::TreeTop *replaceStart,
      TR::Node *origNode, TR::Node *newNode);

   static void setProfilingCode(TR::Node *node, TR::NodeChecklist &checklist);

   static TR::Node *loadValue(TR::Compilation *comp, TR::DataType dataType, TR::Node *base, TR::Node *index = NULL,
      TR::Node *offset = NULL);
   static TR::Node *storeNode(TR::Compilation *comp, TR::Node *value, TR::SymbolReference* &symRef);
   static TR::Node *createHelperCall(TR::Compilation *comp, TR::Node *value, TR::Node *table);
   static TR::Node *incrementMemory(TR::Compilation *comp, TR::DataType counterType, TR::Node *address);

   static TR::Node *effectiveAddress(TR::DataType dataType, TR::Node *base, TR::Node *index = NULL, TR::Node *offset = NULL);
   static TR::Node *systemConst(TR::Node *example, uint64_t value);
   static TR::Node *convertType(TR::Node *index, TR::DataType dataType, bool zeroExtend = true);
   };

#endif
