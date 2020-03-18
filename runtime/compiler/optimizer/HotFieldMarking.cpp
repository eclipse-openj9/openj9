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

#include "optimizer/HotFieldMarking.hpp"
#include "env/j9method.h"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "infra/ILWalk.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"

#include <map>

struct SymStats
   {
   int32_t _count;
   int32_t _score;
   int32_t _fieldNameLength;
   char* _fieldName;
   int32_t _fieldSigLength;
   char* _fieldSig;
   TR_OpaqueClassBlock *_clazz;
   SymStats(int32_t count, int32_t score, int32_t fieldNameLength, char* fieldName, int32_t fieldSigLength, char* fieldSig, TR_OpaqueClassBlock *clazz) :
      _count(count), _score(score), _fieldNameLength(fieldNameLength), _fieldName(fieldName), _fieldSigLength(fieldSigLength), _fieldSig(fieldSig), _clazz(clazz) {}
   };

typedef TR::typed_allocator<std::pair<TR::Symbol *, SymStats *>, TR::Region&> SymAggMapAllocator;
typedef std::less<TR::Symbol *> SymAggMapComparator;
typedef std::map<TR::Symbol *, SymStats *, SymAggMapComparator, SymAggMapAllocator> SymAggMap;

static int32_t getReducedFrequency(int32_t currentValue, int32_t count, int32_t newFrequency) {
   return ((currentValue * count) + newFrequency) / (count + 1);
}

int32_t TR_HotFieldMarking::perform()
   {
   if (!comp()->getOption(TR_DisableMarkingOfHotFields))
      {
      if (trace())
         traceMsg(comp(), "Skipping hot field marking since it is disabled\n");
      return 0;
      }

   SymAggMap stats(SymAggMapComparator(), comp()->trMemory()->currentStackRegion());
   TR::Block *block = NULL;
   for (TR::PostorderNodeIterator it(comp()->getStartTree(), comp()); it != NULL; ++it)
      {
      TR::Node * const node = it.currentNode();

      if (node->getOpCodeValue() == TR::BBStart)
         {
         block = node->getBlock();
         }
      else if ((node->getOpCode().isLoadIndirect() || node->getOpCode().isStoreIndirect())
               && node->getOpCode().hasSymbolReference()
               && !node->getSymbolReference()->isUnresolved()
               && node->getSymbolReference()->getSymbol()->isShadow()
               && !node->isInternalPointer()
               && !node->getOpCode().isArrayLength()
              )
         {
         TR::SymbolReference *symRef = node->getSymbolReference();
         if (symRef->getCPIndex() >= 0 && !symRef->getSymbol()->isArrayShadowSymbol())
            {
            auto itr = stats.find(symRef->getSymbol());
            if (itr != stats.end())
               {
               itr->second->_score = getReducedFrequency(itr->second->_score, itr->second->_count, block->getFrequency());
               itr->second->_count += 1;
               continue;
               }

            TR::ResolvedMethodSymbol *rms = comp()->getOwningMethodSymbol(symRef->getOwningMethodIndex());
            TR_ResolvedMethod *method = rms->getResolvedMethod();

            int32_t fieldNameLength = 0;
            char *fieldName = method->fieldNameChars(symRef->getCPIndex(), fieldNameLength);
            int32_t fieldSigLength = 0;
            char *fieldSig = method->fieldSignatureChars(symRef->getCPIndex(), fieldSigLength);
            bool isStatic = false;
            TR_OpaqueClassBlock *containingClass = static_cast<TR_ResolvedJ9Method*>(method)->definingClassFromCPFieldRef(comp(), symRef->getCPIndex(), isStatic);
            if (isStatic)
               continue;

            stats[symRef->getSymbol()] = new (trStackMemory()) SymStats(1, block->getFrequency(), fieldNameLength, fieldName, fieldSigLength, fieldSig, containingClass);
            }
         }
      }

   for (auto itr = stats.begin(), end = stats.end(); itr != end; ++itr)
      {
      int32_t classNameLength = 0;
      char *className = comp()->fej9()->getClassNameChars(itr->second->_clazz, classNameLength);

      int32_t fieldOffset = comp()->fej9()->getInstanceFieldOffset(itr->second->_clazz, itr->second->_fieldName, itr->second->_fieldNameLength, itr->second->_fieldSig, itr->second->_fieldSigLength);
      dumpOptDetails(comp(), "%s notifying hot field %.*s%.*s.%.*s = %d\n", itr->second->_fieldSigLength, itr->second->_fieldSig, classNameLength, className, itr->second->_fieldNameLength, itr->second->_fieldName, itr->second->_score);
      //comp()->fej9()->markHotField(getUtilization(), itr->second->_clazz, fieldOffset, itr->second->_score);
      }
   return 1;
   }

int32_t TR_HotFieldMarking::getUtilization()
   {
   switch (comp()->getMethodHotness())
      {
      case noOpt:
      case cold:
      case warm:
         return 0;
      case hot:
         return 1;
      case veryHot:
      case scorching:
         return 12;
      default:
         TR_ASSERT(false, "Unable handled hotness for utilization calculation");
      }
   return 0;
   }

const char *
TR_HotFieldMarking::optDetailString() const throw()
   {
   return "O^O HOT FIELD MARKING: ";
   }
