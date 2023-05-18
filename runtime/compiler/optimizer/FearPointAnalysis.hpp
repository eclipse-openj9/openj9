/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/
#include "optimizer/DataFlowAnalysis.hpp"

namespace TR { class NodeChecklist; }

class TR_FearPointAnalysis : public TR_BackwardUnionSingleBitContainerAnalysis
   {
   public:

   TR_FearPointAnalysis(TR::Compilation *comp, TR::Optimizer *optimizer, TR_Structure *,
      TR_BitVector &fearGeneratingNodes, bool topLevelFearOnly=false, bool trace=false);

   virtual Kind getKind();
   virtual TR_FearPointAnalysis *asFearPointAnalysis();

   virtual int32_t getNumberOfBits();
   virtual bool supportsGenAndKillSets();
   virtual void initializeGenAndKillSetInfo();
   virtual void analyzeNode(TR::Node *, vcount_t, TR_BlockStructure *, TR_SingleBitContainer *);
   virtual void analyzeTreeTopsInBlockStructure(TR_BlockStructure *);
   virtual bool postInitializationProcessing();
   TR_SingleBitContainer *generatedFear(TR::Node *node);

   static bool virtualGuardKillsFear(TR::Compilation *comp, TR::Node *virtualGuardNode);

   private:
   void computeFear(TR::Compilation *comp, TR::Node *node, TR::NodeChecklist &checklist);
   void computeFearFromBitVector(TR::Compilation *comp);
   TR_SingleBitContainer **_fearfulNodes;
   TR_BitVector &_fearGeneratingNodes;
   TR_SingleBitContainer _EMPTY;
   bool _topLevelFearOnly;
   bool _trace;

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   bool confirmFearFromBitVector(TR::Node *node);
#endif
   };
