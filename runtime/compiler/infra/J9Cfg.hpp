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

#ifndef J9_CFG_INCL
#define J9_CFG_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_CFG_CONNECTOR
#define J9_CFG_CONNECTOR
namespace J9 { class CFG; }
namespace J9 { typedef J9::CFG CFGConnector; }
#endif

#include "infra/OMRCfg.hpp"

#include <stddef.h>
#include <stdint.h>
#include "cs2/listof.h"
#include "env/TRMemory.hpp"
#include "il/Node.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"
#include "infra/TRCfgEdge.hpp"
#include "infra/TRCfgNode.hpp"

class TR_BitVector;
class TR_BlockCloner;
class TR_BlockFrequencyInfo;
class TR_ExternalProfiler;
class TR_RegionStructure;
class TR_Structure;
namespace TR { class Block; }
namespace TR { class CFG; }
namespace TR { class Compilation; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class TreeTop; }
template <class T> class TR_Array;

namespace J9
{

class CFG : public OMR::CFGConnector
   {
public:

   CFG(TR::Compilation *c, TR::ResolvedMethodSymbol *m) :
         OMR::CFGConnector(c, m),
      _externalProfiler(NULL)
      {
      }

   /**
    * Set up profiling frequencies for nodes and edges, normalized to the
    * maxBlockCount in TR::Recompilation.
    *
    * Returns true if profiling information was available and used.
    */
   bool setFrequencies();

   void setBlockAndEdgeFrequenciesBasedOnStructure();
   TR_BitVector *setBlockAndEdgeFrequenciesBasedOnJITProfiler();
   void setBlockFrequenciesBasedOnInterpreterProfiler();
   void computeInitialBlockFrequencyBasedOnExternalProfiler(TR::Compilation *comp);
   void propagateFrequencyInfoFromExternalProfiler(TR_ExternalProfiler *profiler);
   void getInterpreterProfilerBranchCountersOnDoubleton(TR::CFGNode *cfgNode, int32_t *taken, int32_t *nottaken);
   void setSwitchEdgeFrequenciesOnNode(TR::CFGNode *node, TR::Compilation *comp);
   void setBlockFrequency(TR::CFGNode *node, int32_t frequency, bool addFrequency = false);
   int32_t scanForFrequencyOnSimpleMethod(TR::TreeTop *tt, TR::TreeTop *endTT);

   bool hasBranchProfilingData() { return _externalProfiler ? true : false; }
   void getBranchCountersFromProfilingData(TR::Node *node, TR::Block *block, int32_t *taken, int32_t *notTaken);

   bool emitVerbosePseudoRandomFrequencies();

protected:

   TR_ExternalProfiler *_externalProfiler;
   };

}

#endif
