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

#ifndef SWITCHANALYZER_HPP
#define SWITCHANALYZER_HPP

#include <limits.h>
#include <stdint.h>
#include "env/FilePointerDecl.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "infra/Link.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"

class TR_BitVector;
class TR_FrontEnd;
namespace TR { class CFG; }
namespace TR { class Block; }
namespace TR { class Node; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }

namespace TR {

class SwitchAnalyzer : public TR::Optimization
   {
   public:
   SwitchAnalyzer(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR::SwitchAnalyzer(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   void analyze(TR::Node *node, TR::Block *block);

   enum NodeKind { Unique, Range, Dense };

   class SwitchInfo : public TR_Link<SwitchInfo>
      {
      public:

      SwitchInfo(TR_Memory * m)
	 : _kind(Dense), _count(0), _freq(0), _cost(0), _min(INT_MAX), _max(INT_MIN)
	    { _chain = new (m->trHeapMemory()) TR_LinkHead<SwitchInfo>(); }

      SwitchInfo(CASECONST_TYPE value, TR::TreeTop *target, int32_t cost)
	 : _kind(Unique), _count(1), _freq(0), _min(value), _max(value), _target(target),
	   _cost(cost) {}

      bool operator >(SwitchInfo &other);
      void print(TR_FrontEnd *, TR::FILE *pOutFile, int32_t indent);

      NodeKind _kind;
      float    _freq;
      int32_t  _count;
      int32_t  _cost;
      CASECONST_TYPE  _min;
      CASECONST_TYPE  _max;
      union
	 {
	 TR::TreeTop              *_target; // range and unique
	 TR_LinkHead<SwitchInfo> *_chain;  // dense
	 };
      };

   void print(TR_FrontEnd *, TR::FILE *pOutFile);
   void chainInsert(TR_LinkHead<SwitchInfo> *chain, SwitchInfo *info);
   void denseInsert(SwitchInfo *dense, SwitchInfo *info);
   void denseMerge (SwitchInfo *to, SwitchInfo *from);

   SwitchInfo *getConsecutiveUniques(SwitchInfo *start);

   void findDenseSets(TR_LinkHead<SwitchInfo> *chain);
   bool mergeDenseSets(TR_LinkHead<SwitchInfo> *chain);
   TR_LinkHead<SwitchInfo> *gather(TR_LinkHead<SwitchInfo> *chain);

   int32_t countMajorsInChain(TR_LinkHead<SwitchInfo> *chain);
   SwitchInfo *getLastInChain(TR_LinkHead<SwitchInfo> *chain);

   void emit(TR_LinkHead<SwitchInfo> *chain, TR_LinkHead<SwitchInfo> *bound, TR_LinkHead<SwitchInfo> *earlyUniques);

   TR::Block *binSearch(SwitchInfo *startNode, SwitchInfo *endNode, int32_t numMajors,
		       CASECONST_TYPE rangeLeft, CASECONST_TYPE rangeRight);
   TR::Block *addIfBlock(TR::ILOpCodes opCode, CASECONST_TYPE val, TR::TreeTop *dest);
   TR::Block *linearSearch(SwitchInfo *start);
   SwitchInfo *sortedListByFrequency(SwitchInfo *start);

   TR::Block *addGotoBlock(TR::TreeTop *dest);

   TR::Block *addTableBlock(SwitchInfo *info);

   int32_t *setupFrequencies(TR::Node *node);
   TR::Block *checkIfDefaultIsDominant(SwitchInfo *start);
   TR::Block *peelOffTheHottestValue(TR_LinkHead<SwitchInfo> *chain);

   bool keepAsUnique(SwitchInfo *info, int32_t itemNumber);

   void printInfo(TR_FrontEnd *, TR::FILE *pOutFile, TR_LinkHead<SwitchInfo> *head);
   private:
   void fixUpUnsigned(TR_LinkHead<SwitchInfo> *chain);
   TR::CFG *_cfg;


   // These variables must be zero-initialized in analyze
   TR::Node  *_switch;
   TR::TreeTop *_switchTree;
   TR::TreeTop *_defaultDest;
   TR::Block *_block;
   TR::Block *_nextBlock;
   TR::SymbolReference *_temp;
   bool  _signed;
   bool  _isInt64;

   TR_BitVector *_blocksGeneratedByMe;


   int32_t _costMem;
   //int32_t _costRelative;

   float   _minDensity;
   int32_t _binarySearchBound;
   int32_t _smallDense;

   int32_t _costRange;
   int32_t _costUnique;
   int32_t _costDense;

   bool    _haveProfilingInfo;
   };

}

#endif
