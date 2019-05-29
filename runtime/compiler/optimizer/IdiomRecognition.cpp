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

#include "optimizer/IdiomRecognition.hpp"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "compile/Compilation.hpp"
#include "compile/CompilationTypes.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "cs2/bitvectr.h"
#include "env/CompilerEnv.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/Flags.hpp"
#include "infra/HashTab.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/SimpleRegex.hpp"
#include "infra/TRCfgEdge.hpp"
#include "infra/TRCfgNode.hpp"
#include "optimizer/IdiomRecognitionUtils.hpp"
#include "optimizer/LoopCanonicalizer.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/TransformUtil.hpp"
#include "optimizer/UseDefInfo.hpp"
#include "ras/Debug.hpp"

#define OPT_DETAILS "O^O NEWLOOPREDUCER: "
#define VERBOSE 0
#define STRESS_TEST 0
#define ALLOW_FAST_VERSIONED_LOOP 1
#define EXCLUDE_COLD_LOOP (!STRESS_TEST)
#define SHOW_CANDIDATES 0

#define IDIOM_SIZE_FACTOR 15
#define MAX_PREPARED_GRAPH (36+STRESS_TEST*5)
static TR_CISCGraph *preparedCISCGraphs[MAX_PREPARED_GRAPH];
static int32_t numPreparedCISCGraphs;
static TR_Hotness minimumHotnessPrepared;

#define SHOW_BCINDICES 1
#define SHOW_STATISTICS 1

/************************************/
/***********             ************/
/*********** TR_CISCNode ************/
/***********             ************/
/************************************/

void
TR_CISCNode::initializeMembers(uint32_t opc, uint16_t id, int16_t dagId, uint16_t ncfgs, uint16_t nchildren)
   {
   initializeLists();
   _flags.clear();
   setOpcode(opc);
   _id = id;
   _dagId = dagId;
   _numChildren = nchildren;
   _numSuccs = ncfgs;
   _latestDest = 0;
   _otherInfo = 0;
   if (_ilOpCode.isStoreDirect()) setIsStoreDirect();
   switch(opc)
      {
      case TR_ahconst:
         setIsLightScreening();
         // fall through
      case TR_allconst:
      case TR_variable:
      case TR_variableORconst:
      case TR_quasiConst:
      case TR_quasiConst2:
      case TR_arrayindex:
      case TR_arraybase:
         setIsNecessaryScreening();
         break;
      }
   }

//*****************************************************************************************
// It analyzes pOpc and tOpc are equivalent.
// Note that it handles wildcard nodes as shown below.
//*****************************************************************************************
bool
TR_CISCNode::isEqualOpc(TR_CISCNode *t)
   {
   //TR_ASSERT((int)TR::NumIlOps == TR_variable, "assumption for reducing compilation time");
   static_assert((int)TR::NumIlOps == TR_variable,
                 "assumption for reducing compilation time");

   int32_t pOpc = _opcode;
   int32_t tOpc = t->_opcode;

   if (pOpc == tOpc) return true;
   else if (pOpc > TR::NumIlOps) // Please see the above assumption
      {
      switch(pOpc)
         {
         case TR_booltable:
            return (tOpc == TR::Case || t->_ilOpCode.isIf()) && !t->isOutsideOfLoop();
         case TR_variableORconst:
            return (tOpc == TR_variable || t->_ilOpCode.isLoadConst());
         case TR_quasiConst2:
            if (tOpc == TR::iloadi)
               return !t->getHeadOfTrNode()->getSymbol()->isArrayShadowSymbol();        // true if non-array
            // else fall through
         case TR_quasiConst:
            return (tOpc == TR_variable || t->_ilOpCode.isLoadConst() || tOpc == TR::arraylength);
         case TR_iaddORisub:
            return (tOpc == TR::iadd || tOpc == TR::isub);
         case TR_conversion:
            return (t->_ilOpCode.isConversion());
         case TR_ifcmpall:
            return (t->_ilOpCode.isIf());
         case TR_ishrall:
            return (tOpc == TR::ishr || tOpc == TR::iushr);
         case TR_bitop1:
            return (t->_ilOpCode.isAnd() || t->_ilOpCode.isOr() || t->_ilOpCode.isXor());
         case TR_arrayindex:
            return (tOpc == TR_variable || tOpc == TR::iadd);
         case TR_arraybase:
            return (tOpc == TR_variable || tOpc == TR::aloadi);
         case TR_ahconst:
         case TR_allconst:
            return t->_ilOpCode.isLoadConst();
         case TR_inbload:
            return (t->_ilOpCode.isLoadIndirect() && !t->_ilOpCode.isByte());
         case TR_inbstore:
            return (t->_ilOpCode.isStoreIndirect() && !t->_ilOpCode.isByte());
         case TR_indload:
            return (t->_ilOpCode.isLoadIndirect());
         case TR_indstore:
            return (t->_ilOpCode.isStoreIndirect() || tOpc == TR::awrtbari);
         case TR_ibcload:
            return (t->_ilOpCode.isLoadIndirect() && (t->_ilOpCode.isByte() || (t->_ilOpCode.isShort() && t->_ilOpCode.isUnsigned())));
         case TR_ibcstore:
            return (t->_ilOpCode.isStoreIndirect() && (t->_ilOpCode.isByte() || (t->_ilOpCode.isShort() && t->_ilOpCode.isUnsigned())));
         }
      }
   return false;
   }

//*****************************************************************************************
// convert opcode to its name for debug print
//*****************************************************************************************
const char *
TR_CISCNode::getName(TR_CISCOps op, TR::Compilation * comp)
   {
   if (op < (TR_CISCOps)TR::NumIlOps)
      {
      TR::ILOpCode opCode;
      opCode.setOpCodeValue((enum TR::ILOpCodes)op);
      return opCode.getName();
      }
   switch(op)
      {
      case TR_variable: return "Var";
      case TR_booltable: return "booltable";
      case TR_entrynode: return "entrynode";
      case TR_exitnode: return "exitnode";
      case TR_ahconst: return "ahconst";
      case TR_allconst: return "constall";
      case TR_inbload: return "inbload";
      case TR_inbstore: return "inbstore";
      case TR_indload: return "indload";
      case TR_indstore: return "indstore";
      case TR_ibcload: return "ibcload";
      case TR_ibcstore: return "ibcstore";
      case TR_variableORconst: return "variableORconst";
      case TR_quasiConst: return "quasiConst";
      case TR_quasiConst2: return "quasiConst2";
      case TR_iaddORisub: return "iaddORisub";
      case TR_arrayindex: return "arrayindex";
      case TR_arraybase: return "arraybase";
      case TR_conversion: return "conversion";
      case TR_ifcmpall: return "ifcmpall";
      case TR_ishrall: return "ishrall";
      case TR_bitop1: return "bitop1";
      }
   return "Unknown";
   }


//*****************************************************************************************
// Debug print to file
//*****************************************************************************************
void
TR_CISCNode::dump(TR::FILE *pOutFile, TR::Compilation * comp)
   {
   int32_t i;
   char buf[256];
   const char *name = getName((TR_CISCOps)_opcode, comp);
   if (isValidOtherInfo())
      {
      sprintf(buf, "%s %d", name, _otherInfo);
      }
   else
      {
      sprintf(buf, "%s", name);
      }
   traceMsg(comp, "[%p] %3d %2d%c %-11s", this, _id, _dagId, isOutsideOfLoop() ? ' ' : 'L', buf);
   traceMsg(comp, " [");
   for (i = 0; i < _numSuccs; i++)
      {
      traceMsg(comp, "%d",_succs[i]->_id);
      if (i < _numSuccs-1) traceMsg(comp, " ");
      }
   traceMsg(comp, "]");
   traceMsg(comp, " [");
   for (i = 0; i < _numChildren; i++)
      {
      traceMsg(comp, "%d",_children[i]->_id);
      if (i < _numChildren-1) traceMsg(comp, " ");
      }
   traceMsg(comp, "]");

   ListIterator<TR_CISCNode> ci(&_chains);
   TR_CISCNode *n;
   if (!_chains.isEmpty())
      {
      traceMsg(comp, " chains[");
      for (n = ci.getFirst(); n; n = ci.getNext())
         {
         traceMsg(comp, "%d ",n->_id);
         }
      traceMsg(comp, "]");
      }

   if (!_dest.isEmpty())
      {
      traceMsg(comp, " dest=");
      ci.set(&_dest);
      for (n = ci.getFirst(); n; n = ci.getNext())
         {
         traceMsg(comp, "%d ",n->_id);
         }
      }

   if (!_hintChildren.isEmpty())
      {
      traceMsg(comp, " hint=");
      ci.set(&_hintChildren);
      for (n = ci.getFirst(); n; n = ci.getNext())
         {
         traceMsg(comp, "%d ",n->_id);
         }
      }

#if 0
   if (!_preds.isEmpty())
      {
      ci.set(&_preds);
      traceMsg(comp, " preds[");
      for (n = ci.getFirst(); n; n = ci.getNext())
         {
         traceMsg(comp, "%d ",n->_id);
         }
      traceMsg(comp, "]");
      }
#endif

   if (isCISCNodeModified())
      {
      traceMsg(comp, "\t(Modified)");
      }

   if (isOptionalNode())
      {
      traceMsg(comp, "\t(Optional)");
      }

   // Print out the TR_Nodes that correspond with the given CISCNode.
   if (getTrNodeInfo())
      {
      if (!getTrNodeInfo()->isEmpty())
         {
         traceMsg(comp, "\tTR::Node:[");
         ListIterator<TrNodeInfo> ni(getTrNodeInfo());
         for (TrNodeInfo *n = ni.getFirst(); n != NULL; n = ni.getNext())
            {
            traceMsg(comp, "%s,",comp->getDebug()->getName((TR::Node*)n->_node));
            }
         traceMsg(comp, "]");
         }
      }
   traceMsg(comp, "\n");
   }


//*****************************************************************************************
// Debug print to stdout
//*****************************************************************************************
void
TR_CISCNode::printStdout()
   {
   int32_t i;
   char buf[256];
   if (isValidOtherInfo())
      {
      sprintf(buf, "%d %d", _opcode, _otherInfo);
      }
   else
      {
      sprintf(buf, "%d", _opcode);
      }
   printf("[%p] %3d %2d%c %-11s", this, _id, _dagId, isOutsideOfLoop() ? ' ' : 'L', buf);
   printf(" [");
   for (i = 0; i < _numSuccs; i++)
      {
      printf("%d",_succs[i]->_id);
      if (i < _numSuccs-1) printf(" ");
      }
   printf("]");
   printf(" [");
   for (i = 0; i < _numChildren; i++)
      {
      printf("%d",_children[i]->_id);
      if (i < _numChildren-1) printf(" ");
      }
   printf("]");

   ListIterator<TR_CISCNode> ci(&_chains);
   TR_CISCNode *n;
   if (!_chains.isEmpty())
      {
      printf(" chains[");
      for (n = ci.getFirst(); n; n = ci.getNext())
         {
         printf("%d ",n->_id);
         }
      printf("]");
      }

   if (!_dest.isEmpty())
      {
      printf(" dest=");
      ci.set(&_dest);
      for (n = ci.getFirst(); n; n = ci.getNext())
         {
         printf("%d ",n->_id);
         }
      }

   if (!_hintChildren.isEmpty())
      {
      printf(" hint=");
      ci.set(&_hintChildren);
      for (n = ci.getFirst(); n; n = ci.getNext())
         {
         printf("%d ",n->_id);
         }
      }

   if (isCISCNodeModified())
      {
      printf("\t(Modified)");
      }

   if (isOptionalNode())
      {
      printf("\t(Optional)");
      }

   printf("\n");
   }


//*****************************************************************************************
// It replaces the successor and maintains the predecessor of the original successor.
//*****************************************************************************************
void
TR_CISCNode::replaceSucc(uint32_t index, TR_CISCNode *to)
   {
   TR_ASSERT(index < _numSuccs, "TR_CISCNode::replaceSucc index out of range");
   TR_CISCNode *old = _succs[index];
   if (old)
      {
      old->_preds.remove(this);
      }
   setSucc(index, to);
   }


//*****************************************************************************************
// It replaces the child and maintains the parent of the original child.
//*****************************************************************************************
void
TR_CISCNode::replaceChild(uint32_t index, TR_CISCNode *ch)
   {
   TR_ASSERT(index < _numChildren, "TR_CISCNode::replaceChild index out of range");
   TR_CISCNode *org = _children[index];
   if (org)
      {
      org->_parents.remove(this);
      }
   setChild(index, ch);
   }




//*****************************************************************************************
// To sort out leaf nodes, these functions related to checkParent analyze whether ancestors
// of the pattern node and those of the target node are equivalent.
//*****************************************************************************************

#define DEBUG_CHECKPARENTS 0
#define DEPTH_CHECKPARENTS 7

#if 0 // recursive version, just for reference
bool
TR_CISCNode::checkParents(TR_CISCNode *const o, const int8_t level)
   {
#if DEBUG_CHECKPARENTS
   int i;
   for (i = DEPTH_CHECKPARENTS-level; --i >= 0; ) traceMsg(comp(), "  ");
   traceMsg(comp(), "checkParents %d:%d...\n", _id, o->_id);
#endif
   if (level > 0)
      {
      ListElement<TR_CISCNode> *ple;
      for (ple = _parents.getListHead(); ple; ple = ple->getNextElement())
         {
         TR_CISCNode *const pn = ple->getData();
         uint16_t pIndex = 0;
         const bool commutative = pn->isCommutative();
         if (!commutative)
            {
            for (; pIndex < pn->getNumChildren(); pIndex++ )
               {
               if (pn->getChild(pIndex) == this) break;
               }
            }
         TR_ASSERT(pIndex < pn->getNumChildren(), "error!");
         ListElement<TR_CISCNode> *tle;
         const bool isPnParentsEmpty = pn->_parents.isEmpty();
         const bool isPnOptional = pn->isOptionalNode();
         for (tle = o->_parents.getListHead(); tle; tle = tle->getNextElement())
            {
            TR_CISCNode *const tn = tle->getData();
            if (pn->isEqualOpc(tn))
               {
               if ((commutative || tn->getChild(pIndex) == o) &&
                   pn->checkParents(tn, level-1)) goto find;
               }
            else
               {
               if (tn->getIlOpCode().isStoreDirect())
                  {
                  if (tn->getChild(0) == o && !pn->isChildDirectlyConnected())
                     {
                     if (this->checkParents(tn->getChild(1), level-1)) goto find;
                     }
                  }
               else
                  {      /* search one more depth */
                  ListElement<TR_CISCNode> *tcle;
                  for (tcle = tn->_parents.getListHead(); tcle; tcle = tcle->getNextElement())
                     {
                     if (pn->isEqualOpc(tcle->getData()) &&
                         (commutative || tcle->getData()->getChild(pIndex) == tn) &&
                         pn->checkParents(tcle->getData(), level-1)) goto find;
                     }
                  }
               }
            }
         TR_ASSERT(!tle, "error");
         if (isPnOptional)
            {
            if (isPnParentsEmpty ||
                pn->isSkipParentsCheck() ||
                pn->checkParents(o, level-1)) goto find;
            }
#if DEBUG_CHECKPARENTS
         for (i = DEPTH_CHECKPARENTS-level; --i >= 0; ) traceMsg(comp(), "  ");
         traceMsg(comp(), "checkParents %d:%d failed due to pid %d\n", _id, o->_id, pn->_id);
#endif
         return false;

         find:;
         }
      }
#if DEBUG_CHECKPARENTS
   for (i = DEPTH_CHECKPARENTS-level; --i >= 0; ) traceMsg(comp(), "  ");
   traceMsg(comp(), "checkParents %d:%d succeed\n", _id, o->_id);
#endif
   return true;
   }
#endif



//*****************************************************************************************
// It recursively marks dead (negligible).
//*****************************************************************************************
void
TR_CISCNode::deadAllChildren()
   {
   int32_t i;
   if (!getParents()->isSingleton() ||  // this node is multi-referenced or
       _ilOpCode.canRaiseException() || // any side effectable instructions
       _ilOpCode.isCall() ||
       _ilOpCode.isReturn() ||
       _ilOpCode.isStore() ||
       _ilOpCode.isBranch()) return;

   setIsNegligible();
   for (i = _numChildren; --i >= 0; )
      _children[i]->deadAllChildren();
   }



//*****************************************************************************************
// This class is for creating non-recursive version of checkParents.
//*****************************************************************************************
struct TR_StackForCheckParents
   {
   TR_StackForCheckParents() {}
   TR_StackForCheckParents(TR_CISCNode *p, TR_CISCNode *t,
                           ListElement<TR_CISCNode> *ple, ListElement<TR_CISCNode> *tle, ListElement<TR_CISCNode> *tcle,
                           TR_CISCNode *pn, TR_CISCNode *tn,
                           uint16_t pIndex, int8_t level, int8_t label)
      { initialize(p, t, ple, tle, tcle, pn, tn, pIndex, level, label); }

   void initialize(TR_CISCNode *p, TR_CISCNode *t,
                   ListElement<TR_CISCNode> *ple, ListElement<TR_CISCNode> *tle, ListElement<TR_CISCNode> *tcle,
                   TR_CISCNode *pn, TR_CISCNode *tn,
                   uint16_t pIndex, int8_t level, int8_t label)
      {
      _p = p;
      _t = t;
      _ple = ple;
      _tle = tle;
      _tcle = tcle;
      _pn = pn;
      _tn = tn;
      _pIndex = pIndex;
      _level = level;
      _label = label;
      }

   TR_CISCNode *_p;
   TR_CISCNode *_t;
   ListElement<TR_CISCNode> *_ple;
   ListElement<TR_CISCNode> *_tle;
   ListElement<TR_CISCNode> *_tcle;
   TR_CISCNode *_pn;
   TR_CISCNode *_tn;
   uint16_t _pIndex;
   int8_t _level;
   int8_t _label;
   };

//*****************************************************************************************
// It analyzes ancestors of p and those of t are equivalent.
// The maximum search depth can be given by "level".
// Note: There is a recursive version of this function named "checkParents" above, which
// is easier to understand.
//*****************************************************************************************
bool
TR_CISCNode::checkParentsNonRec(TR_CISCNode *p, TR_CISCNode *t, int8_t level, TR::Compilation *comp)
   {
   ListElement<TR_CISCNode> *ple  = NULL;
   ListElement<TR_CISCNode> *tle  = NULL;
   ListElement<TR_CISCNode> *tcle = NULL;
   TR_CISCNode *pn = NULL;
   TR_CISCNode *tn = NULL;
   uint16_t pIndex;
   bool ret;
   TR_StackForCheckParents stackParents[DEPTH_CHECKPARENTS];
   TR_StackForCheckParents *st;
   const int8_t initial_level = level;

   while(true)
      {
      first:
      TR_CISCNode *parm1 = 0;
      ret = true;
#if DEBUG_CHECKPARENTS
      int32_t i;
      for (i = initial_level-level; --i >= 0; ) traceMsg(comp, "  ");
      traceMsg(comp, "checkParents %d:%d...\n", p->_id, t->_id);
#endif
      if (level > 0)
         {
         for (ple = p->_parents.getListHead(); ple; ple = ple->getNextElement())
            {
            pn = ple->getData();
            pIndex = 0;
            if (!pn->isCommutative())
               {
               for (; pIndex < pn->getNumChildren(); pIndex++ )
                  {
                  if (pn->getChild(pIndex) == p) break;
                  }
               }
            TR_ASSERT(pIndex < pn->getNumChildren(), "error!");
            for (tle = t->_parents.getListHead(); tle; tle = tle->getNextElement())
               {
               tn = tle->getData();
               TR_CISCNode *parm2;
               parm1 = 0;
               if (pn->isEqualOpc(tn))
                  {
                  if (pn->isCommutative() || tn->getChild(pIndex) == t)
                     {
                     parm1 = pn;
                     parm2 = tn;
                     }
                  }
               else
                  {
                  if (tn->getIlOpCode().isStoreDirect())
                     {
                     if (tn->getChild(0) == t && !pn->isChildDirectlyConnected())
                        {
                        parm1 = p;
                        parm2 = tn->getChild(1);
                        }
                     }
                  else
                     {      /* search one more depth */
                     for (tcle = tn->_parents.getListHead(); tcle; tcle = tcle->getNextElement())
                        {
                        if (pn->isEqualOpc(tcle->getData()) &&
                            (pn->isCommutative() || tcle->getData()->getChild(pIndex) == tn))
                           {
                           level--;
                           st = stackParents + level;
                           st->initialize(p, t, ple, tle, tcle, pn, tn, pIndex, level+1, 0);
                           p = pn;
                           t = tcle->getData();
                           goto first;

                           label0:;
                           }
                        }
                     }
                  }
               if (parm1)
                  {
                  level--;
                  st = stackParents + level;
                  st->initialize(p, t, ple, tle, tcle, pn, tn, pIndex, level+1, 1);
                  p = parm1;
                  t = parm2;
                  goto first;

                  label1:;
                  }
               }
            TR_ASSERT(!tle, "error");
            if (pn->isOptionalNode())
               {
               if (!pn->_parents.isEmpty() && !pn->isSkipParentsCheck())
                  {
                  level--;
                  st = stackParents + level;
                  st->initialize(p, t, ple, tle, tcle, pn, tn, pIndex, level+1, 2);
                  p = pn;
                  //t = t;
                  goto first;

                  label2:;
                  }
               else
                  goto find;
               }
#if DEBUG_CHECKPARENTS
            for (i = initial_level-level; --i >= 0; ) traceMsg(comp, "  ");
            traceMsg(comp, "checkParents %d:%d failed due to pid %d\n", p->_id, t->_id, pn->_id);
#endif
            ret = false;
            goto final;

            find:;
            }
         }
      ret = true;
#if DEBUG_CHECKPARENTS
      for (i = initial_level-level; --i >= 0; ) traceMsg(comp, "  ");
      traceMsg(comp, "checkParents %d:%d succeed\n", p->_id, t->_id);
#endif
      final:
      if (level == initial_level) break;   /* exit loop */
      st = stackParents + level;

      p = st->_p;
      t = st->_t;
      ple = st->_ple;
      tle = st->_tle;
      tcle = st->_tcle;
      pn = st->_pn;
      tn = st->_tn;
      pIndex = st->_pIndex;
      level = st->_level;
      parm1 = 0;

      if (ret) goto find;
      else
         {
         if (st->_label == 0) goto label0;
         else if (st->_label == 1) goto label1;
         else goto label2;
         }
      }
   return ret;
   }



//*****************************************************************************************
// Swap successors and reverse the opcode of the branch
//*****************************************************************************************
void
TR_CISCNode::reverseBranchOpCodes()
   {
   TR_ASSERT(_opcode < TR::NumIlOps && _ilOpCode.isIf(), "error: not isIf");
   TR_ASSERT(_numSuccs == 2, "error: _numSuccs != 2");
   TR_CISCNode *swap = _succs[0];
   _succs[0] = _succs[1];
   _succs[1] = swap;
   setOpcode(_ilOpCode.getOpCodeForReverseBranch());
   }


//*****************************************************************************************
// Return whether this node and all nodes in the "_chains" are in the same loop body.
//*****************************************************************************************
bool
TR_CISCNode::checkDagIdInChains()
   {
   uint16_t  thisDagID = _dagId;
   ListIterator<TR_CISCNode> ci(&_chains);
   TR_CISCNode *c;
   for (c = ci.getFirst(); c; c = ci.getNext())
      if (c->_dagId != thisDagID) return false;
   return true;
   }

//*****************************************************************************************
// Return TreeTop for getBranchDestination or fallthrough
//*****************************************************************************************
TR::TreeTop *
TR_CISCNode::getDestination(bool isFallThrough)
   {
   TR::TreeTop *ret;
   TR::Node *nRepTrNode = getHeadOfTrNode();
   if (getOpcode() != nRepTrNode->getOpCodeValue())
      {
      TR_ASSERT(getOpcode() == nRepTrNode->getOpCode().getOpCodeForReverseBranch(), "error");
      isFallThrough = !isFallThrough;
      }

   if (isFallThrough)
      {
      for (ret = getHeadOfTreeTop()->getNextTreeTop();
           ret->getNode()->getOpCodeValue() != TR::BBStart;
           ret = ret->getNextTreeTop());
      }
   else
      {
      ret = nRepTrNode->getBranchDestination();
      }
   return ret;
   }




/************************************/
/***********             ************/
/*********** TR_CISCHash ************/
/***********             ************/
/************************************/
TR_CISCNode *TR_CISCHash::find(uint64_t key)
   {
   TR_ASSERT(_numBuckets > 0, "error: _numBuckets == 0!");
   uint32_t index = key % _numBuckets;
   struct HashTableEntry *p;
   for (p = _buckets[index]; p != 0; p = p->_next)
      {
      if (p->_key == key) return p->_node;
      }
   return 0;
   }

bool
TR_CISCHash::add(uint64_t key, TR_CISCNode *node, bool checkExist)
   {
   TR_ASSERT(_numBuckets > 0, "error: _numBuckets == 0!");
   uint32_t index = key % _numBuckets;
   if (checkExist)
      {
      struct HashTableEntry *p;
      for (p = _buckets[index]; p != 0; p = p->_next)
         {
         if (p->_key == key) return false;
         }
      }
   struct HashTableEntry *newEntry = (struct HashTableEntry *)trMemory()->allocateMemory(sizeof(*newEntry), _allocationKind);
   newEntry->_next = _buckets[index];
   newEntry->_key = key;
   newEntry->_node = node;
   _buckets[index] = newEntry;
   return true;
   }







/********************************************/
/***********                     ************/
/*********** TR_CISCGraphAspects ************/
/***********                     ************/
/********************************************/
//*****************************************************************************************
// We used this property to exclude those idioms which are unlikely to be matched against
// the target loop to limit the extra compilation time.
// We can consider the nodes of an idiom, and if a graph is missing any of those nodes,
// we already know no topological embedding exists.
// For each idiom, we prepared a bit-vector that represents the required IL nodes.
// We compare the bit-vector of every idiom graph with that of the target graph to exclude
// the unlikely matched idioms.
//*****************************************************************************************

void
TR_CISCGraphAspects::setLoadAspects(uint32_t val, bool orExistAccess)
   {
   TR_ASSERT(val <= loadMasks, "error!");
   if (orExistAccess && (val & 0xFF)) val |= existAccess;
   set(val);
   }

void
TR_CISCGraphAspects::setStoreAspects(uint32_t val, bool orExistAccess)
   {
   TR_ASSERT(val <= loadMasks, "error!");
   if (orExistAccess && (val & 0xFF)) val |= existAccess;
   set(val << storeShiftCount);
   }

void
TR_CISCGraphAspects::modifyAspects()
   {
   uint32_t load = getLoadAspects();
   uint32_t store = getStoreAspects();
   uint32_t temp = load & store & 0xff;
   if (temp) set(sameTypeLoadStore);
   }

void
TR_CISCGraphAspects::print(TR::Compilation *comp, bool noaspects)
   {
   traceMsg(comp, "CISCGraph%sAspects is %08x\n",noaspects ? "No" : "", getValue());
   }

void
TR_CISCGraphAspectsWithCounts::print(TR::Compilation *comp, bool noaspects)
   {
   traceMsg(comp, "CISCGraph%sAspects is %08x\n",noaspects ? "No" : "", getValue());
   traceMsg(comp, "min counts: if=%d, indirectLoad=%d, indirectStore=%d\n",
           _ifCount, _indirectLoadCount, _indirectStoreCount);
   }

void
TR_CISCGraphAspectsWithCounts::setAspectsByOpcode(int opc)
   {
   switch(opc)
      {
      case TR_inbload:
         setLoadAspects(existAccess | nbValue);
         incIndirectLoadCount();
         break;
      case TR_inbstore:
         setStoreAspects(existAccess | nbValue);
         incIndirectStoreCount();
         break;
      case TR_ibcload:
      case TR_indload:
         setLoadAspects(existAccess);
         incIndirectLoadCount();
         break;
      case TR_ibcstore:
      case TR_indstore:
         setStoreAspects(existAccess);
         incIndirectStoreCount();
         break;
      case TR_ifcmpall:
         incIfCount();
         break;
      case TR::ishr:
      case TR::iushr:
      case TR::lshr:
      case TR::lushr:
         set(shr);
         break;
      case TR::lmulh:
      case TR::imulh:
      case TR::imul:
      case TR::lmul:
         set(mul);
         break;
      case TR::irem:
      case TR::lrem:
         set(reminder);
         break;
      case TR::idiv:
      case TR::ldiv:
         set(division);
         break;
      case TR::BNDCHK:
         set(bndchk);
         break;
      case TR::isub:
         set(isub);
         break;
      case TR::iadd:
         set(iadd);
         break;
      default:
         if (opc < TR::NumIlOps)
            {
            TR::ILOpCode opCode;
            opCode.setOpCodeValue((enum TR::ILOpCodes)opc);
            if (opCode.isLoadIndirect())
               {
               setLoadAspects(existAccess | opCode.getSize());
               incIndirectLoadCount();
               }
            else if (opCode.isStoreIndirect())
               {
               setStoreAspects(existAccess | opCode.getSize());
               incIndirectStoreCount();
               }
            else if (opCode.isCall())
               {
               set(call);
               }
            else if (opCode.isIf() || opCode.isSwitch())
               {
               incIfCount();
               }
            else if (opCode.isAnd() || opCode.isOr() || opCode.isXor())
               {
               set(bitop1);
               }
            }
         break;
      }
   }


/*************************************/
/***********              ************/
/*********** TR_CISCGraph ************/
/***********              ************/
/*************************************/
void
TR_CISCGraph::setEssentialNodes(TR_CISCGraph *tgt)
   {
   ListIterator<TR_CISCNode> ni(tgt->getNodes());
   TR_CISCNode *n;

   for (n = ni.getFirst(); n; n = ni.getNext())
      {
      if (n->getIlOpCode().isStore() ||
          n->getIlOpCode().isCall())
         {
         n->setIsEssentialNode();
         }
      }
   }

static bool graphsInitialized = false;
// Register all idioms at start up.
//
void
TR_CISCGraph::makePreparedCISCGraphs(TR::Compilation *c)
   {
   if (graphsInitialized)
      return;
   else
      graphsInitialized = true;

   int32_t num = 0;
   bool genTRxx = c->cg()->getSupportsArrayTranslateTRxx();
   bool genSIMD = c->cg()->getSupportsVectorRegisters() && !c->getOption(TR_DisableSIMDArrayTranslate);
   bool genTRTO255 = c->cg()->getSupportsArrayTranslateTRTO255();
   bool genTRTO = c->cg()->getSupportsArrayTranslateTRTO();
   bool genTROTNoBreak = c->cg()->getSupportsArrayTranslateTROTNoBreak();
   bool genTROT = c->cg()->getSupportsArrayTranslateTROT();
   bool genTRT =  c->cg()->getSupportsArrayTranslateAndTest();
   bool genMemcpy = c->cg()->getSupportsReferenceArrayCopy() || c->cg()->getSupportsPrimitiveArrayCopy();
   bool genMemset = c->cg()->getSupportsArraySet();
   bool genMemcmp = c->cg()->getSupportsArrayCmp();
   bool genIDiv2Mul = c->cg()->getSupportsLoweringConstIDiv();
   bool genLDiv2Mul = c->cg()->getSupportsLoweringConstLDiv();
   // FIXME: We need getSupportsCountDecimalDigit() like interface
   // this idiom is only enabled on 390 for the moment
   //
   bool genDecimal = TR::Compiler->target.cpu.isZ();
   bool genBitOpMem = TR::Compiler->target.cpu.isZ();
   bool is64Bit = TR::Compiler->target.is64Bit();
   bool isBig = TR::Compiler->target.cpu.isBigEndian();
   int32_t ctrl = (is64Bit ? CISCUtilCtl_64Bit : 0) | (isBig ? CISCUtilCtl_BigEndian : 0);

   // THESE ARE NOT GUARANTEED OR TESTED TO WORK ON WCODE.
   // Problems encountered include ahSize=0 on WCode leading to hash collision when adding node for make*CISCGraphs.
   if (genMemcmp)
      {
      preparedCISCGraphs[num] =  makeMemCmpGraph(c, ctrl);
      setEssentialNodes(preparedCISCGraphs[num++]);
      preparedCISCGraphs[num] =  makeMemCmpIndexOfGraph(c, ctrl);
      setEssentialNodes(preparedCISCGraphs[num++]);
      preparedCISCGraphs[num] =  makeMemCmpSpecialGraph(c, ctrl);
      setEssentialNodes(preparedCISCGraphs[num++]);
      }
   if (genTRT)
      {
      preparedCISCGraphs[num] =  makeTRTGraph(c, ctrl);
      setEssentialNodes(preparedCISCGraphs[num++]);
      preparedCISCGraphs[num] =  makeTRTGraph2(c, ctrl);
      setEssentialNodes(preparedCISCGraphs[num++]);
      preparedCISCGraphs[num] =  makeTRT4NestedArrayGraph(c, ctrl);
      setEssentialNodes(preparedCISCGraphs[num++]);
      //preparedCISCGraphs[num] =  makeTRT4NestedArrayIfGraph(c, ctrl); setEssentialNodes(preparedCISCGraphs[num]); num++;
      }
   if (genMemset)
      {
      preparedCISCGraphs[num] =  makeMemSetGraph(c, ctrl);
      setEssentialNodes(preparedCISCGraphs[num++]);
#if STRESS_TEST
      preparedCISCGraphs[num] =  makeMixedMemSetGraph(c, ctrl);
      setEssentialNodes(preparedCISCGraphs[num++]);
#endif
      preparedCISCGraphs[num] = makePtrArraySetGraph(c, ctrl);
      setEssentialNodes(preparedCISCGraphs[num++]);
      // Causes perf degradations on Xalan strlen16 opportunities. SRSTU is only better on long strings.
      //preparedCISCGraphs[num] = makeStrlen16Graph(c, ctrl);
      //setEssentialNodes(preparedCISCGraphs[num++]);
      }
   if (genMemcpy)
      {
      preparedCISCGraphs[num] =  makeMemCpyGraph(c, ctrl);
      setEssentialNodes(preparedCISCGraphs[num++]);
      preparedCISCGraphs[num] =  makeMemCpyDecGraph(c, ctrl);
      setEssentialNodes(preparedCISCGraphs[num++]);
      preparedCISCGraphs[num] =  makeMemCpySpecialGraph(c, ctrl);
      setEssentialNodes(preparedCISCGraphs[num++]);
      preparedCISCGraphs[num] =  makeMemCpyByteToCharGraph(c, ctrl);
      setEssentialNodes(preparedCISCGraphs[num++]);
      preparedCISCGraphs[num] =  makeMemCpyByteToCharBndchkGraph(c, ctrl);
      setEssentialNodes(preparedCISCGraphs[num++]);
      preparedCISCGraphs[num] =  makeMemCpyCharToByteGraph(c, ctrl);
      setEssentialNodes(preparedCISCGraphs[num++]);
      preparedCISCGraphs[num] =  makeMEMCPYChar2ByteGraph2(c, ctrl);
      setEssentialNodes(preparedCISCGraphs[num++]);
      preparedCISCGraphs[num] =  makeMEMCPYChar2ByteMixedGraph(c, ctrl);
      setEssentialNodes(preparedCISCGraphs[num++]);
      // disabled for now
#if STRESS_TEST
      preparedCISCGraphs[num] =  makeMEMCPYByte2IntGraph(c, ctrl); setEssentialNodes(preparedCISCGraphs[num]); num++;
      preparedCISCGraphs[num] =  makeMEMCPYInt2ByteGraph(c, ctrl); setEssentialNodes(preparedCISCGraphs[num]); num++;
#endif
      }

   if (genTRTO255 || genTRTO || genSIMD || genTRxx)
      {
      preparedCISCGraphs[num] =  makeCopyingTRTxGraph(c, ctrl, 0);
      setEssentialNodes(preparedCISCGraphs[num++]);
      preparedCISCGraphs[num] =  makeCopyingTRTxGraph(c, ctrl, 1);
      setEssentialNodes(preparedCISCGraphs[num++]);
      preparedCISCGraphs[num] =  makeCopyingTRTxGraph(c, ctrl, 2);
      setEssentialNodes(preparedCISCGraphs[num++]);
      preparedCISCGraphs[num] =  makeCopyingTRTxThreeIfsGraph(c, ctrl);
      setEssentialNodes(preparedCISCGraphs[num++]);
      preparedCISCGraphs[num] =  makeCopyingTRTOInduction1Graph(c, ctrl, 0);
      setEssentialNodes(preparedCISCGraphs[num++]);
      preparedCISCGraphs[num] =  makeCopyingTRTOInduction1Graph(c, ctrl, 1);
      setEssentialNodes(preparedCISCGraphs[num++]);
      preparedCISCGraphs[num] =  makeCopyingTRTOInduction1Graph(c, ctrl, 2);
      setEssentialNodes(preparedCISCGraphs[num++]);

      }

   if (genTROTNoBreak || genTROT || genSIMD || genTRxx)
      {
      preparedCISCGraphs[num] =  makeCopyingTROxGraph(c, ctrl, 0);
      setEssentialNodes(preparedCISCGraphs[num++]);
      preparedCISCGraphs[num] =  makeCopyingTROxGraph(c, ctrl, 1);
      setEssentialNodes(preparedCISCGraphs[num++]);
      }

   if (genTRxx)
      {
      if (c->getOption(TR_EnableCopyingTROTInduction1Idioms))
         {
         preparedCISCGraphs[num] =  makeCopyingTROTInduction1Graph(c, ctrl, 0);
         setEssentialNodes(preparedCISCGraphs[num++]);
         preparedCISCGraphs[num] =  makeCopyingTROTInduction1Graph(c, ctrl, 1);
         setEssentialNodes(preparedCISCGraphs[num++]);
         }
      preparedCISCGraphs[num] =  makeCopyingTROOSpecialGraph(c, ctrl);
      setEssentialNodes(preparedCISCGraphs[num++]);
#if STRESS_TEST
      preparedCISCGraphs[num] =  makeCopyingTRTTSpecialGraph(c, ctrl);
      setEssentialNodes(preparedCISCGraphs[num++]);
#endif
      if (is64Bit)
         {
         preparedCISCGraphs[num] =  makeCopyingTRTOGraphSpecial(c, ctrl);
         setEssentialNodes(preparedCISCGraphs[num++]);
         }
      preparedCISCGraphs[num] =  makeTROTArrayGraph(c, ctrl);
      setEssentialNodes(preparedCISCGraphs[num++]);
      preparedCISCGraphs[num] =  makeTRTOArrayGraph(c, ctrl);
      setEssentialNodes(preparedCISCGraphs[num++]);
      preparedCISCGraphs[num] =  makeTRTOArrayGraphSpecial(c, ctrl);
      setEssentialNodes(preparedCISCGraphs[num++]);
      }
   if (genDecimal)
      {
      // Needs to be modified
      preparedCISCGraphs[num] =  makeCountDecimalDigitIntGraph(c, ctrl, genIDiv2Mul);
      setEssentialNodes(preparedCISCGraphs[num++]);
      preparedCISCGraphs[num] =  makeIntToStringGraph(c, ctrl, genIDiv2Mul);
      setEssentialNodes(preparedCISCGraphs[num++]);
      preparedCISCGraphs[num] =  makeCountDecimalDigitLongGraph(c, ctrl, genLDiv2Mul);
      setEssentialNodes(preparedCISCGraphs[num++]);
#if STRESS_TEST
      preparedCISCGraphs[num] =  makeLongToStringGraph(c, ctrl); setEssentialNodes(preparedCISCGraphs[num]); num++;
#endif
      }
   if (genBitOpMem)
      {
      preparedCISCGraphs[num] =  makeBitOpMemGraph(c, ctrl);
      setEssentialNodes(preparedCISCGraphs[num++]);
      }

   TR_ASSERT(num <= MAX_PREPARED_GRAPH, "incorrect number of graphs!");
   numPreparedCISCGraphs = num;

   // set minimumHotnessPrepared;
   minimumHotnessPrepared = scorching;
   for (;--num >= 0;)
      {
      TR_Hotness hotness = preparedCISCGraphs[num]->getHotness();
      if (minimumHotnessPrepared > hotness)
         minimumHotnessPrepared = hotness;
      }
   }

void
TR_CISCGraph::dump(TR::FILE *pOutFile, TR::Compilation * comp)
   {
   traceMsg(comp, "CISCGraph of %s\n",_titleOfCISC);
   _aspects.print(comp, false);
   _noaspects.print(comp, true);
   ListIterator<TR_CISCNode> ni(getNodes());
   TR_CISCNode *n;

#if 1
   traceMsg(comp, "!! Note !! Showing reverse order for convenience\n");
   TR_ScratchList<TR_CISCNode> reorder(comp->trMemory());
   for (n = ni.getFirst(); n; n = ni.getNext())
      {
      reorder.add(n);
      }
   ni.set(&reorder);
#endif
   traceMsg(comp, " ptr id dagId(L=Loop) succ children (chains) (dest) (hintChildren) (flags) (TRNodeInfo)\n");
   for (n = ni.getFirst(); n; n = ni.getNext())
      {
      n->dump(pOutFile, comp);
      }

   traceMsg(comp, "\nOrder by Data\n");
   ni.set(&_orderByData);
   for (n = ni.getFirst(); n; n = ni.getNext())
      {
      n->dump(pOutFile, comp);
      }
   }


//*****************************************************************************************
// It registers TR::Block, TR::TreeTop, and TR::Node into TR_CISCNode.
// To find TR_CISCNode by TR_Node, it also adds TR_CISCNode into a hash table.
//*****************************************************************************************
void
TR_CISCGraph::addTrNode(TR_CISCNode *n, TR::Block *block, TR::TreeTop *top, TR::Node *trNode)
   {
   n->addTrNode(block, top, trNode);
   bool ret = addTrNode2CISCNode(trNode, n);
   TR_ASSERT(ret, "addTrNode2CISCNode returns failure");
   }


//*****************************************************************************************
// To find TR_CISCNode by opcode, it adds TR_CISCNode into a hash table.
//*****************************************************************************************
void
TR_CISCGraph::addOpc2CISCNode(TR_CISCNode *n)
   {
   if (_opc2CISCNode.getNumBuckets() > 0)
      {
      bool ret;
      bool registerNode = false;
      switch(n->getOpcode())
         {
         case TR::lconst:
            if (!n->isValidOtherInfo()) break;
            // else fall through
         case TR_variable:
         case TR::cconst:
         case TR::sconst:
         case TR::iconst:
         case TR::bconst:
            TR_ASSERT(n->isValidOtherInfo(), "error");
            // fall through
         case TR_booltable:
         case TR_entrynode:
         case TR_exitnode:
         case TR_ahconst:
         case TR_arrayindex:
         case TR_arraybase:
            registerNode = true;
            break;
         }
      if (registerNode)
         {
         ret = addOpc2CISCNode(n->getOpcode(), n->isValidOtherInfo(), n->getOtherInfo(), n);
         TR_ASSERT(ret, "addOpc2CISCNode returns failure");
         }
      }
   }


//*****************************************************************************************
// Add TR_CISCNode to the graph.
// It also adds its aspects, TR::Block, TR::TreeTop, TR_Node, and hash table.
//*****************************************************************************************
void
TR_CISCGraph::addNode(TR_CISCNode *n, TR::Block *block, TR::TreeTop *top, TR::Node *trNode)
   {
   TR_ASSERT((block == 0 && top == 0 && trNode == 0) ||
          (block != 0 && top != 0 && trNode != 0), "error"); // all 0 or all Non-0
   _nodes.add(n);
   if (isRecordingAspectsByOpcode()) _aspects.setAspectsByOpcode(n);
   if (trNode != 0)
      {
      TR_ASSERT(n->getTrNodeInfo()->isEmpty(), "n->getTrNodeInfo() exists???");
      addTrNode(n, block, top, trNode);
      }
   addOpc2CISCNode(n);
   }


//*****************************************************************************************
// Search a store for the node "target" until the node "to"
//*****************************************************************************************
TR_CISCNode *
TR_CISCGraph::searchStore(TR_CISCNode *target, TR_CISCNode *to)
   {
   TR_CISCNode *v = target;
   if (v->isLoadVarDirect()) v = v->getChild(0);
   if (v->getOpcode() != TR_variable) return 0;
   TR_BitVector visited(getNumNodes(), trMemory());

   TR_CISCNode *t = target;
   while (true)
      {
      if (t->isStoreDirect() &&
          t->getChild(1) == v) return t;

      if (t->getNumSuccs() == 0) break;
      visited.set(t->getID());

      t = t->getSucc(0);
      if (t == target || t == to || t == getExitNode() || visited.isSet(t->getID())) return 0;
      }

   return 0;
   }


/**************************************************/
/***********                           ************/
/***********      TR_PCISCGraph        ************/
/*********** (PersistentAlloc version) ************/
/***********                           ************/
/**************************************************/
void
TR_PCISCGraph::addNode(TR_CISCNode *n, TR::Block *block, TR::TreeTop *top, TR::Node *trNode)
   {
   TR_ASSERT(block == 0 && top == 0 && trNode == 0, "error"); // all NULL
   TR_PersistentList<TR_CISCNode> *p = (TR_PersistentList<TR_CISCNode> *)&_nodes;
   p->add(n);
   addOpc2CISCNode(n);
   }


void
TR_CISCGraph::createDagId2NodesTable()
   {
   TR_ASSERT(_numDagIds > 0, "TR_CISCGraph::createDagId2NodesTable(), _numDagIds <= 0???");
   if (!isNoFragmentDagId()) defragDagId();
   uint32_t size = sizeof(*_dagId2Nodes) * _numDagIds;
   _dagId2Nodes = (List<TR_CISCNode> *)trMemory()->allocateMemory(size, heapAlloc);
   memset(_dagId2Nodes, 0, size);
   for (int i = 0; i < _numDagIds; i++) _dagId2Nodes[i].setRegion(trMemory()->heapMemoryRegion());
   ListIterator<TR_CISCNode> nodesLi(&_nodes);
   TR_CISCNode *n;
   for (n = nodesLi.getFirst(); n; n = nodesLi.getNext())
      {
      int32_t dagId = n->getDagID();
      TR_ASSERT(dagId < _numDagIds, "TR_CISCGraph::createDagId2NodesTable(), dagId is out of range");
      _dagId2Nodes[dagId].add(n);
      }
   }

// remove all the BBStart/BBEnd nodes
void
TR_CISCGraph::createOrderByData()
   {
   _orderByData.init();
   ListIterator<TR_CISCNode> nodesLi(&_nodes);
   TR_CISCNode *n;
   for (n = nodesLi.getFirst(); n; n = nodesLi.getNext())
      {
      if (n->getNumChildren() > 0 ||
          !n->getParents()->isEmpty())
         {
         _orderByData.add(n);
         }
      else
         {
         switch(n->getOpcode())
            {
            case TR_entrynode:
            case TR_exitnode:
               _orderByData.add(n);
               break;
            }
         }
      }
   }


void
TR_PCISCGraph::createDagId2NodesTable()
   {
   TR_ASSERT(_numDagIds > 0, "TR_PCISCGraph::createDagId2NodesTable(), _numDagIds <= 0???");
   if (!isNoFragmentDagId()) defragDagId();
   uint32_t size = sizeof(*_dagId2Nodes) * _numDagIds;
   _dagId2Nodes = (TR_PersistentList<TR_CISCNode> *)jitPersistentAlloc(size);
   memset(_dagId2Nodes, 0, size);
   ListIterator<TR_CISCNode> nodesLi(&_nodes);
   TR_CISCNode *n;
   TR_PersistentList<TR_CISCNode> *list;
   for (n = nodesLi.getFirst(); n; n = nodesLi.getNext())
      {
      int32_t dagId = n->getDagID();
      TR_ASSERT(dagId < _numDagIds, "TR_PCISCGraph::createDagId2NodesTable(), dagId is out of range");
      list = (TR_PersistentList<TR_CISCNode> *)_dagId2Nodes + dagId;
      list->add(n);
      }
   }


void
TR_PCISCGraph::createOrderByData()
   {
   TR_PersistentList<TR_CISCNode> *list = (TR_PersistentList<TR_CISCNode> *)&_orderByData;
   ListIterator<TR_CISCNode> nodesLi(&_nodes);
   TR_CISCNode *n;
   for (n = nodesLi.getFirst(); n; n = nodesLi.getNext())
      {
      if (n->getNumChildren() > 0 ||
          !n->getParents()->isEmpty())
         {
            list->add(n);
         }
      else
         {
         switch(n->getOpcode())
            {
            case TR_entrynode:
            case TR_exitnode:
               list->add(n);
               break;
            }
         }
      }
   }


//*****************************************************************************************
// Import UD/DU information of TR::Node to TR_CISCNode._chain
//*****************************************************************************************
void
TR_CISCGraph::importUDchains(TR::Compilation *comp, TR_UseDefInfo *useDefInfo, bool reinitialize)
   {
   ListIterator<TR_CISCNode> nodesLi(&_nodes);
   TR_CISCNode *n;
   const int32_t firstUseIndex = useDefInfo->getFirstUseIndex();

   if (isSetUDDUchains())       // already done before
      {
      if (!reinitialize) return;
      for (n = nodesLi.getFirst(); n; n = nodesLi.getNext())
         {
         n->initChains();
         }
      }

   setIsSetUDDUchains();
   for (n = nodesLi.getFirst(); n; n = nodesLi.getNext())
      {
      if (n->isLoadVarDirect())
         {
         if (n->getHeadOfTrNodeInfo()->_node->getSymbol()->isAutoOrParm())
            {
            /* set UD-chains to TR_CISCNode._chain */
            TR_ASSERT(n->getTrNodeInfo()->isSingleton(), "direct load must correspond to a single TR node");
            TR::Node *trNode = n->getTrNodeInfo()->getListHead()->getData()->_node;
            int32_t useDefIndex = trNode->getUseDefIndex();
            TR_ASSERT(useDefInfo->isUseIndex(useDefIndex), "error!");
            TR_UseDefInfo::BitVector info(comp->allocator());
            useDefInfo->getUseDef(info, useDefIndex);
            TR_ASSERT(!info.IsZero(), "no defs!");
            TR_UseDefInfo::BitVector::Cursor cursor(info);
            for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
               {
               int32_t defIndex = cursor;
               TR_ASSERT(defIndex < useDefInfo->getNumDefsOnEntry() || useDefInfo->isDefIndex(defIndex), "error!");
               TR_CISCNode *defNode = getCISCNode(useDefInfo->getNode(defIndex));
               if (!defNode)
                  {
                  n->addChain(_entryNode, true);
                  }
               else
                  {
                  n->addChain(defNode);
                  }
               }
            }
         }
      else if (n->isStoreDirect())
         {
         /* set DU-chains to TR_CISCNode._chain */
         TR_ASSERT(n->getTrNodeInfo()->isSingleton(), "direct store must correspond to a single TR node");
         TR::Node *trNode = n->getTrNodeInfo()->getListHead()->getData()->_node;
         int32_t useDefIndex = trNode->getUseDefIndex();
         if (useDefIndex == 0) continue;
         TR_ASSERT(useDefInfo->isDefIndex(useDefIndex), "error!");
         TR_UseDefInfo::BitVector info(comp->allocator());
         useDefInfo->getUsesFromDef(info, useDefIndex);
         if (!info.IsZero())
            {
            TR_UseDefInfo::BitVector::Cursor cursor(info);
            for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
               {
               int32_t useIndex = (int32_t) cursor + firstUseIndex;
               TR_ASSERT(useDefInfo->isUseIndex(useIndex), "error!");
               TR_CISCNode *useNode = getCISCNode(useDefInfo->getNode(useIndex));
               if (!useNode)
                  {
                  n->addChain(_exitNode, true);
                  }
               else
                  {
                  n->addChain(useNode);
                  }
               }
            }
         else
            {
            n->setIsNegligible();
            n->getChild(0)->deadAllChildren();
            }
         }
      else
         {
         TR_CISCNode *c, *p, *v;
         ListIterator<TR_CISCNode> ci(n->getHintChildren());
         for (c = ci.getFirst(); c; c = ci.getNext())
            {
            ListIterator<TR_CISCNode> pi(c->getParents());
            for (p = pi.getFirst(); p; p = pi.getNext())
               {
               if (p->isStoreDirect())
                  {
                  v = p->getChild(1);   // variable
                  bool find = false;
                  for (int i = n->getNumChildren(); --i >= 0; )
                     if (n->getChild(i) == v)
                        {
                        find = true;
                        break;
                        }
                  if (find)
                     {
                     // set UD/DU chains
                     p->addChain(n);
                     n->addChain(p);
                     }
                  }
               }
            }
         }
      }
   }



//*****************************************************************************************
// Defragment dagIds and set _noFragmentDagId
//*****************************************************************************************
int32_t
TR_CISCGraph::defragDagId()
   {
   ListIterator<TR_CISCNode> nodesLi(&_nodes);
   TR_CISCNode *n;
   int32_t curId, newId;

   n = nodesLi.getFirst();
   newId = 0;
   curId = n->getDagID();
   for (; n; n = nodesLi.getNext())
      {
      int32_t nDagId = n->getDagID();
      if (curId != nDagId)
         {
         TR_ASSERT(curId < nDagId, "Error!");
         curId = nDagId;
         newId++;
         }
      n->setDagID(newId);
      }
   newId++;
   _numDagIds = newId;
   setNoFragmentDagId();
   return newId;
   }



//*****************************************************************************************
// Set the flag _isOutsideOfLoop to all of the nodes in outside of the loop body
//*****************************************************************************************
void
TR_CISCGraph::setOutsideOfLoopFlag(uint16_t loopBodyDagId)
   {
   ListIterator<TR_CISCNode> li(&_nodes);
   TR_CISCNode *n;
   for (n = li.getFirst(); n; n = li.getNext())
      {
      if (n->getDagID() != loopBodyDagId) n->setOutsideOfLoop();
      }
   }



/***********************************************/
/***********                        ************/
/*********** TR_CFGReversePostOrder ************/
/***********                        ************/
/***********************************************/

// do a reverse post-order walk of the CFG
// The result is stored in _revPost.
//

List<TR::CFGNode> *
TR_CFGReversePostOrder::compute(TR::CFG *cfg)
   {
   createReversePostOrder(cfg, cfg->getEnd());
   return &_revPost;
   }

void
TR_CFGReversePostOrder::createReversePostOrder(TR::CFG *cfg, TR::CFGNode *n)
   {
   TR_StackForRevPost *stack;
   TR_BitVector *visited = new (cfg->comp()->trStackMemory()) TR_BitVector(cfg->getNextNodeNumber(), cfg->comp()->trMemory(), stackAlloc);
   TR_LinkHead<TR_StackForRevPost> stackRevPost;

   visited->set(n->getNumber());
   auto edge = n->getPredecessors().begin();
   while(true)
      {
      bool alreadyVisited = true;
      while(edge != n->getPredecessors().end())
         {
         TR::CFGNode *predBlock = (*edge)->getFrom();
         if (!visited->isSet(predBlock->getNumber()))
            {
            // push the predBlock onto the stack
            //
            stack = new (cfg->comp()->trStackMemory()) TR_StackForRevPost();
            stack->n = n;
            stack->le = ++edge;
            stackRevPost.add(stack);

            n = predBlock;
            visited->set(n->getNumber());
            edge = n->getPredecessors().begin();
            alreadyVisited = false;
            break;
            }
         ++edge;
         }

      if (alreadyVisited)
         {
         // add current block to the final list
         //
         _revPost.append(n);

         // pick next block to be processed
         //
         if (!(stack = stackRevPost.pop()))
            break;
         n = stack->n;
         edge = stack->le;
         }
      }
   }

void
TR_CFGReversePostOrder::dump(TR::Compilation * comp)
   {
   ListIterator<TR::CFGNode> ni(&_revPost);
   TR::CFGNode *n;
   traceMsg(comp, "Generated Reverse post order of CFG: ");
   for (n = ni.getFirst(); n; n = ni.getNext())
      traceMsg(comp, "%d->", n->getNumber());
   traceMsg(comp, "\n");
   }

#if 0
//*****************************************************************************************
// These functions are recursive versions, and they are disabled for now.
//*****************************************************************************************
void
TR_CFGReversePostOrder::initReversePostOrder()
   {
   _visited.init(_cfg->getNextNodeNumber());
   _revPost.init();
   }


void
TR_CFGReversePostOrder::initReversePostOrder(TR::CFG *cfg)
   {
   _cfg = cfg;
   initReversePostOrder();
   }

void
TR_CFGReversePostOrder::createReversePostOrderRec(TR::CFGNode *n)
   {
   _visited.set(n->getNumber());
   for (auto le = n->getPredecessors().begin(); le != n->getPredecessors().end(); ++le)
      {
      TR::CFGNode *from = (*le)->getFrom();
      if (!_visited.isSet(from->getNumber()))
         {
         createReversePostOrderRec(from);
         }
      }
   _revPost.append(n);
   }
#endif



/*********** FOR OPTIMIZATION ************/

/******************************************/
/***********                   ************/
/*********** TR_CISCNodeRegion ************/
/***********                   ************/
/******************************************/
TR_CISCNodeRegion *
TR_CISCNodeRegion::clone()
   {
   TR_CISCNodeRegion *c = new (getRegion()) TR_CISCNodeRegion(_bvnum, getRegion());
   ListElement<TR_CISCNode> *le;
   c->_flags = _flags;
   for (le = _pHead; le; le = le->getNextElement())
      c->append(le->getData());
   return c;
   }


/******************************************/
/***********                   ************/
/*********** TR_NodeDuplicator ************/
/***********                   ************/
/******************************************/
TR::Node *
TR_NodeDuplicator::restructureTree(TR::Node *oldTree, TR::Node *newTree)
   {
   TR_ASSERT(oldTree->getNumChildren() == newTree->getNumChildren(), "error");
   for (int i = 0; i < oldTree->getNumChildren(); i++)
      {
      TR::Node *oldChild = oldTree->getChild(i);
      TR_Pair<TR::Node,TR::Node> *pair;
      ListElement<TR_Pair<TR::Node,TR::Node> > *le;

      // Search for oldChild
      for (le = _list.getListHead(); le; le = le->getNextElement())
         {
         pair = le->getData();
         if (pair->getKey() == oldChild) break;
         }

      if (le)
         {      // found
         newTree->setAndIncChild(i, pair->getValue());
         }
      else
         {
         TR::Node *newChild = newTree->getChild(i);
         pair = new (trHeapMemory()) TR_Pair<TR::Node,TR::Node>(oldChild, newChild);
         _list.add(pair);
         restructureTree(oldChild, newChild);
         }
      }
   return newTree;
   }

TR::Node *
TR_NodeDuplicator::duplicateTree(TR::Node *org)
   {
   TR::Node *newNode = org->duplicateTree();
   return restructureTree(org, newNode);
   }


/******************************************/
/***********                   ************/
/*********** TR_UseTreeTopMap  ************/
/***********                   ************/
/******************************************/

TR_UseTreeTopMap::TR_UseTreeTopMap(TR::Compilation * comp, TR::Optimizer * optimizer)
   : _useToParentMap(comp->trMemory(), stackAlloc)
   {
   _buildAllMap = false;
   _compilation = comp;
   _optimizer = optimizer;
   }

int32_t
TR_UseTreeTopMap::buildAllMap()
   {
   if (_buildAllMap) return 0;
   _info = _optimizer->getUseDefInfo();
   if (0==_info) return 0;
   TR::TreeTop* currentTree = comp()->getStartTree();
   _useToParentMap.init(_info->getTotalNodes());
   comp()->incVisitCount();

   for (; currentTree; currentTree = currentTree->getNextTreeTop())
      {
      buildUseTreeTopMap(currentTree,currentTree->getNode());
      }

   _buildAllMap = true;
   return 1;
   }

/**
 * Build a map of use indices to their parent TreeTops (usedef datastructure doesn't have this)
 */
typedef TR_Pair<TR::Node,TR::TreeTop> UseInfo;
void TR_UseTreeTopMap::buildUseTreeTopMap(TR::TreeTop* treeTop,TR::Node *node)
   {
   vcount_t currCount = comp()->getVisitCount();
   if (currCount == node->getVisitCount()) return;
   node->setVisitCount(currCount);

   for (int32_t childIndex=0;childIndex < node->getNumChildren();++childIndex)
      {
      TR::Node *childNode = node->getChild(childIndex);
      int32_t useIndex = childNode->getUseDefIndex();
      if (_info->isUseIndex(useIndex)) // hash it.
         {
         TR_HashId hashIndex;
         TR_ScratchList<UseInfo> *tlist;
         if (_useToParentMap.locate(useIndex,hashIndex))
            {
            tlist = (TR_ScratchList<UseInfo> *)_useToParentMap.getData(hashIndex);
            }
         else
            {
            tlist = new (comp()->trStackMemory()) TR_ScratchList<UseInfo>(comp()->trMemory());
            _useToParentMap.add(useIndex,hashIndex,tlist);
            }

         UseInfo * useInfo = new (comp()->trStackMemory()) UseInfo(childNode,treeTop);
         tlist->add(useInfo);
         }
      buildUseTreeTopMap(treeTop,childNode);
      }
   }

// findParentTreeTop returns the parent TreeTop that contains the given useNode
TR::TreeTop * TR_UseTreeTopMap::findParentTreeTop(TR::Node *useNode)
   {
   TR_ASSERT(_buildAllMap, "Use Treetop map is not initialized.");
   TR_HashId hashId;
   int32_t useIndex = useNode->getUseDefIndex();
   bool found = _useToParentMap.locate(useIndex,hashId);
   TR_ASSERT(found,"No entry exists for %d %x\n",useIndex,hashId);
   TR_ScratchList<UseInfo> *list= (TR_ScratchList<UseInfo> *)_useToParentMap.getData(hashId);
   ListIterator<UseInfo> listCursor(list);
   for (listCursor.getFirst(); listCursor.getCurrent(); listCursor.getNext())
      {
      UseInfo *useInfoPtr = listCursor.getCurrent();
      if (useNode == useInfoPtr->getKey())
         return useInfoPtr->getValue();
      }
   // Parent treetop may not be found if an earlier transformation has removed the useNode.
   // In that case, we'll conservatively return NULL.
   return NULL;
   }


/*******************************************/
/***********                    ************/
/*********** TR_CISCTransformer ************/
/***********                    ************/
/*******************************************/
TR_CISCTransformer::TR_CISCTransformer(TR::OptimizationManager *manager)
   : TR_LoopTransformer(manager),
     _candidatesForShowing(manager->trMemory()),
     _candidateBBStartEnd(0),
     _backPatchList(manager->trMemory()),
     _beforeInsertions(manager->trMemory()),
     _afterInsertions(manager->trMemory()),
     _bblistPred(manager->trMemory()),
     _bblistBody(manager->trMemory()),
     _bblistSucc(manager->trMemory()),
     _candidatesForRegister(manager->trMemory()),
     _useTreeTopMap(manager->comp(), manager->optimizer()),
     _BitsKeepAliveList(manager->trMemory())
   {
   _afterInsertionsIdiom = (ListHeadAndTail<TR::Node> *) trMemory()->allocateHeapMemory(sizeof(ListHeadAndTail<TR::Node>)*2);
   memset(_afterInsertionsIdiom, 0, sizeof(ListHeadAndTail<TR::Node>)*2);
   for (int32_t i = 0; i < 2; ++i)
      _afterInsertionsIdiom[i].setRegion(trMemory()->heapMemoryRegion());

   _lastCFGNode = 0;
   _backPatchList.init();
   _embeddedForData = _embeddedForCFG = 0;
   _flagsForTransformer.clear();
   _loopStructure = NULL;
   if (SHOW_CANDIDATES) setShowingCandidates();
   // construct the idiom graphs and register them
   //
   // TO_BE_ENABLED
   TR_CISCGraph::makePreparedCISCGraphs(manager->comp());
   }

//*****************************************************************************************
// return whether the structure is a fast versioned loop
//*****************************************************************************************
bool
TR_CISCTransformer::isInsideOfFastVersionedLoop(TR_RegionStructure *l)
   {
   TR_RegionStructure *parent = l;
   while(true)
      {
      if (////parent->getVersionedLoop() &&
          !parent->getEntryBlock()->isCold()
#if 0   // We should optimize a fail path of value profiling
          && !parent->getEntryBlock()->isRare()
#endif
          )
         {
         return true; // It is inside of the fast versioned loop.
         }
      TR_Structure *p = parent->getParent();
      if (!p || !(parent = p->asRegion())) break;
      }
   return false;
   }


// createLoopCandidates populates the given list with natural loop candidates
// which contains structure information and is not cold.  The return value of
// this call dictates whether we found candidates or not.
bool
TR_CISCTransformer::createLoopCandidates(List<TR_RegionStructure> *loopCandidates)
   {
   bool enableTracing = trace();


   loopCandidates->init();
   TR_ScratchList<TR_Structure> whileLoops(trMemory());
   ListAppender<TR_Structure> whileLoopsInnerFirst(&whileLoops);
   TR_ScratchList<TR_Structure> doWhileLoops(trMemory());
   ListAppender<TR_Structure> doWhileLoopsInnerFirst(&doWhileLoops);
   TR_ScratchList<TR_Structure> *candidate;
   comp()->incVisitCount();

   detectWhileLoops(whileLoopsInnerFirst, whileLoops, doWhileLoopsInnerFirst, doWhileLoops, _cfg->getStructure(), true);
   // join both lists so all loop
   // candidates are analyzed
   ListElement<TR_Structure> *last = whileLoops.getLastElement();
   if (last)
      {
      last->setNextElement(doWhileLoops.getListHead());
      candidate = &whileLoops;
      }
   else
      candidate = &doWhileLoops;

   int32_t loopCount = 0;
   if (!candidate->isEmpty())
      {
      if (enableTracing)
         traceMsg(comp(), "createLoopCandidates: Evaluating list of loop candidates.\n");

      ListIterator<TR_Structure> whileLoopsIt(candidate);
      TR_Structure *nextWhileLoop;

      for (nextWhileLoop = whileLoopsIt.getFirst(); nextWhileLoop != 0; nextWhileLoop = whileLoopsIt.getNext())
         {
         TR_RegionStructure *naturalLoop = nextWhileLoop->asRegion();
         TR_ASSERT(naturalLoop && naturalLoop->isNaturalLoop(),"CISCGraph, expecting natural loop");
         if (!naturalLoop || !naturalLoop->isNaturalLoop())
            {
            if (trace() && naturalLoop)
               traceMsg(comp(), "\tRejected loop %d - not a natural loop?\n", naturalLoop->getNumber());
            continue;
            }
         TR_StructureSubGraphNode *entryGraphNode = naturalLoop->getEntry();
         TR_BlockStructure *loopBlockStructure = entryGraphNode->getStructure()->asBlock();
         if (!loopBlockStructure)
            {
            if (enableTracing)
               traceMsg(comp(), "\tRejected loop %d - no block structure.\n", naturalLoop->getNumber());
            continue;
            }
         if (!naturalLoop->containsOnlyAcyclicRegions())
            {
            if (enableTracing)
               traceMsg(comp(), "\tRejected loop %d - not inner most loop.\n", naturalLoop->getNumber());
            continue; // inner most loop
            }
         if (loopBlockStructure->getBlock()->isCold())
            {
            if (enableTracing)
               traceMsg(comp(), "\tRejected loop %d - cold loop.\n", naturalLoop->getNumber());
            continue;     // cold loop
            }
         loopCount++;
         loopCandidates->add(naturalLoop);

         if (enableTracing)
            traceMsg(comp(), "\tAccepted loop %d as candidate.\n", naturalLoop->getNumber());
         }
#if SHOW_STATISTICS
      if (showMesssagesStdout() && loopCount)
         if (comp()->getMethodHotness() == warm || isAfterVersioning()) printf("!! #Loop=%d\n",loopCount);
#endif
      }

   if (enableTracing)
      traceMsg(comp(), "createLoopCandidates: %d loop candidates found.\n", loopCount);

   return !loopCandidates->isEmpty();
   }

// prepare the loop for transformation
//
TR::Block *
TR_CISCTransformer::addPreHeaderIfNeeded(TR_RegionStructure *region)
   {
   TR::Block *loopEntry = region->getEntry()->getStructure()->asBlock()->getBlock();
   TR::Block *preHeader = NULL;
   for (auto e = loopEntry->getPredecessors().begin(); e != loopEntry->getPredecessors().end(); ++e)
      {
      TR::Block *predBlock = toBlock((*e)->getFrom());
      // ignore backedges
      if (region->contains(predBlock->getStructureOf(), region->getParent()))
         continue;

      if (predBlock->getStructureOf() &&
            predBlock->getStructureOf()->isLoopInvariantBlock())
         {
         preHeader = predBlock;
         break;
         }
      }

   if (!preHeader)
      {
      preHeader = TR::Block::createEmptyBlock(loopEntry->getEntry()->getNode(), comp(), loopEntry->getFrequency(), loopEntry);
      _cfg->addNode(preHeader);
      TR::Block *prevBlock = loopEntry->getPrevBlock();
      if (prevBlock)
         prevBlock->getExit()->join(preHeader->getEntry());
      preHeader->getExit()->join(loopEntry->getEntry());
      _cfg->addEdge(preHeader, loopEntry);

      // iterate again and fixup the preds to branch to the
      // new pre-header
      //
      TR_ScratchList<TR::CFGEdge> removedEdges(trMemory());
      TR::CFGEdge *e = NULL;
      for (auto e = loopEntry->getPredecessors().begin(); e != loopEntry->getPredecessors().end(); ++e)
         {
         TR::Block *predBlock = toBlock((*e)->getFrom());
         // ignore backedges
         if (region->contains(predBlock->getStructureOf(), region->getParent()))
            continue;

         traceMsg(comp(), "fixing predecessor %d\n", predBlock->getNumber());
         removedEdges.add(*e);
         _cfg->addEdge(predBlock, preHeader);
         TR::Node *branchNode = predBlock->getExit()->getPrevRealTreeTop()->getNode();
         if (branchNode->getOpCode().isBranch() || branchNode->getOpCode().isBranch())
            {
            if (branchNode->getBranchDestination()->getNode()->getBlock() == loopEntry)
               {
               branchNode->setBranchDestination(preHeader->getEntry());
               }
            }
         else if (branchNode->getOpCode().isSwitch())
            {
            for (int32_t i = branchNode->getCaseIndexUpperBound() - 1; i > 0; --i)
               {
               if (branchNode->getChild(i)->getBranchDestination()->getNode()->getBlock() == loopEntry)
                  branchNode->getChild(i)->setBranchDestination(preHeader->getEntry());
               }
            }
         }
      ListIterator<TR::CFGEdge> rIt(&removedEdges);
      for (e = rIt.getFirst(); e; e = rIt.getNext())
         {
         _cfg->removeEdge(e);
         }
      traceMsg(comp(), "added preheader block_%d\n", preHeader->getNumber());
      }

   return preHeader;
   }


// return the predecessor block of the loop entry
//
TR::Block *
TR_CISCTransformer::findPredecessorBlockOfLoopEntry(TR_RegionStructure *loop)
   {
   TR::Block *loopEntry = loop->getEntry()->getStructure()->asBlock()->getBlock();
   if (true /*pred.isDoubleton()*/ )
      {
      for (auto edge = loopEntry->getPredecessors().begin(); edge != loopEntry->getPredecessors().end(); ++edge)
         {
         TR::Block *from = toBlock((*edge)->getFrom());
         if ((from->getSuccessors().size() == 1) &&
             from->getParentStructureIfExists(_cfg) != loop)
            return from;
         }
      }
   return 0;
   }


//*****************************************************************************************
// analyze whether the loop is frequently iterated.
//*****************************************************************************************
void
TR_CISCTransformer::analyzeHighFrequencyLoop(TR_CISCGraph *graph,
                                             TR_RegionStructure *naturalLoop)
   {
   if (trace())
      traceMsg(comp(), "\tAnalyzing if loop is frequently iterated\n");
   bool isInsideOfFastVersioned = isInsideOfFastVersionedLoop(naturalLoop);
   bool highFrequency = true;
#if !STRESS_TEST // If STRESS_TEST is true, BB frequency checks are ignored.
   TR::Block *loopEntry;
   int32_t loopEntryFrequency = -1;
   ListIterator<TR::Block> bi(&_bblistBody);
   for (loopEntry = bi.getFirst(); loopEntry; loopEntry = bi.getNext())
      {
      if (loopEntryFrequency < loopEntry->getFrequency())
         loopEntryFrequency = loopEntry->getFrequency();
      }
   if (trace()) traceMsg(comp(), "\t\tLoop Frequency=%d\n",loopEntryFrequency); // the freq of the loop entry
   if (loopEntryFrequency <= 0)
      {
#if ALLOW_FAST_VERSIONED_LOOP
      highFrequency = isInsideOfFastVersioned;
#else
      highFrequency = false;
#endif
      }
   else
      {
      TR::Block *outer = findPredecessorBlockOfLoopEntry(naturalLoop);
      if (!outer || outer->getFrequency() < 0)
         {
         // If there is no freq information for the predecessor BB of the loop
         if (_bblistSucc.isSingleton())
            {
            // Try the successor block if it is single.
            outer = _bblistSucc.getListHead()->getData();
            if (outer->getFrequency() > loopEntryFrequency) outer = 0;
            }
         if (!outer || outer->getFrequency() < 0)
            {
            // Use the freq of the method entry block for the reference.
            outer = _cfg->getStart()->getSuccessors().front()->getTo()->asBlock();
            }
         }
      if (outer)
         {
         int32_t outerFrequency = outer->getFrequency();
         if (outerFrequency < 1) outerFrequency = 1;
         if (trace()) traceMsg(comp(), "\t\tOuter block %d: Frequency=%d Inner/Outer Ratio:(%f)\n",outer->getNumber(),outerFrequency, (double)loopEntryFrequency/(double)outerFrequency);
         if (loopEntryFrequency < outerFrequency * cg()->arrayTranslateAndTestMinimumNumberOfIterations()) highFrequency = false;
         }
      }
#endif
   if (trace()) traceMsg(comp(), "\t\thighFrequency=%d\n",highFrequency);
   graph->setHotness(comp()->getMethodHotness(), highFrequency);
   graph->setInsideOfFastVersioned(isInsideOfFastVersioned);
   }




//*****************************************************************************************
// Tree Simplification for the Idiom Recognition
//*****************************************************************************************
/*
ificmpxx
  isub
    iconst A
    iload  B
  iconst   C
     |
     V
ificmpyy (yy is the swapChildrenOpCodes of xx)
  iload    B
  iconst   A-C
-------------
ificmpxx
  iadd
    iconst A
    iload  B
  iconst   C
     |
     V
ificmpxx
  iload    B
  iconst   C-A
-------------
ificmplt
  isub
    iload  A
    iload  B
  iconst 1
     |
     V
ificmple
  isub
    iload  A
    iload  B
  iconst 0
     |
     V
ificmpge
  iload    B
  iload    A
*/
void
TR_CISCTransformer::easyTreeSimplification(TR::Node *const node)
   {
   bool modified = false;
   if (node->getOpCode().isIf())
      {
      TR::Node *iconstC = node->getChild(1);
      if (iconstC->getOpCodeValue() != TR::iconst || iconstC->getReferenceCount() > 1) return;
      if (node->getOpCodeValue() == TR::ificmplt &&
          iconstC->getInt() == 1)
         {
         traceMsg(comp(), "\t\teasyTreeSimplification: Node: %p converted from ificmplt with 1 to ifcmple with 0", node);
         TR::Node::recreate(node, TR::ificmple);
         iconstC->setInt(0);
         }

      TR::Node *addOrSub = node->getChild(0);
      if (!addOrSub->getOpCode().isAdd() && !addOrSub->getOpCode().isSub()) return;
      if (addOrSub->getReferenceCount() > 1) return;

      TR::Node *iloadB  = addOrSub->getChild(1);
      if (iloadB->getOpCodeValue() != TR::iload) return;
      if (iloadB->getReferenceCount() > 1) return;

      TR::Node *iconstA = addOrSub->getChild(0);
      if (iconstA->getOpCodeValue() == TR::iconst)
         {
         if (addOrSub->getOpCode().isSub())
            {
            TR::Node::recreate(node, node->getOpCode().getOpCodeForSwapChildren());
            node->setAndIncChild(0, iloadB);
            iconstC->setInt(iconstA->getInt() - iconstC->getInt());
            }
         else
            {
            TR_ASSERT(addOrSub->getOpCode().isSub(), "error");
            node->setAndIncChild(0, iloadB);
            iconstC->setInt(iconstC->getInt() - iconstA->getInt());
            }
         addOrSub->recursivelyDecReferenceCount();
         modified = true;
         }
      else if (iconstA->getOpCodeValue() == TR::iload)
         {
         TR::Node *iloadA = iconstA;
         if (iloadA->getReferenceCount() > 1 ||
             !addOrSub->getOpCode().isSub()) return;
         if (node->getOpCodeValue() == TR::ificmple &&
             iconstC->getInt() == 0)
            {
            TR::Node::recreate(node, TR::ificmpge);
            node->setChild(0, iloadB);
            node->setChild(1, iloadA);
            modified = true;
            }
         }
      }
   if (modified && trace())
      traceMsg(comp(), "\t\teasyTreeSimplification: The tree %p is simplified.\n", node);
   }




//*****************************************************************************************
// analyze the tree "top" and add each node
//*****************************************************************************************
TR_CISCNode *
TR_CISCTransformer::addAllSubNodes(TR_CISCGraph *const graph, TR::Block *const block, TR::TreeTop *const top,
                                   TR::Node *const parent, TR::Node *const node, const int32_t dagId)
   {
   //IdiomRecognition doesn't know how to handle rdbar/wrtbar for now
   if (comp()->incompleteOptimizerSupportForReadWriteBarriers() &&
       (node->getOpCode().isReadBar() || node->getOpCode().isWrtBar()))
      return 0;
   int32_t i;
   int32_t numChildren = node->getNumChildren();
   TR_ScratchList<TR_CISCNode> childList(trMemory());
   vcount_t curVisit = comp()->getVisitCount();

   if (node->getVisitCount() == curVisit)       // already visited
      {
      TR_CISCNode *findCisc = graph->getCISCNode(node);
      return findCisc;
      }
   else
      {
      if (isAfterVersioning() || comp()->getMethodHotness() == warm)
         easyTreeSimplification(node);
      node->setVisitCount(curVisit);
      TR_CISCNode *newCisc = NULL;
      const int32_t opcode = node->getOpCodeValue();
      TR::DataType nodeDataType = node->getDataType();

      //bool ret;
      bool isReplaceChild = true;
      const bool isSwitch = (opcode == TR::lookup || opcode == TR::table);

      int32_t numCreateChildren;
      if (isSwitch)
      	 {
      	 numCreateChildren = 1;
      	 numChildren = node->getCaseIndexUpperBound();
      	 }
      else
      	 {
      	 numCreateChildren = numChildren;    // for ops other than switches, process the children first
      	 }

      if (parent &&
          ((parent->getOpCodeValue() == TR::aiadd && node->getOpCodeValue() == TR::isub && node->getChild(0)->getOpCodeValue() != TR::imul) ||
           (parent->getOpCodeValue() == TR::aladd && node->getOpCodeValue() == TR::lsub && node->getChild(0)->getOpCodeValue() != TR::lmul)))
         {
         // In the internal graph representation, I explicitly represent an index as "index * 1"
         //                 for a byte memory access in order to merge byte and non-byte idioms.
         // Example:
         //   aiadd
         //     aload
         //     isub
         //       iload
         //       iconst xx
         //      |
         //      V
         //   aiadd
         //     aload
         //     isub
         //       imul
         //         iload
         //         iconst 1
         //       iconst xx

         TR_ASSERT(numCreateChildren == 2, "error");
         TR_CISCNode *childCisc = addAllSubNodes(graph, block, top, node, node->getChild(0), dagId);
         if (!childCisc) return 0;

         bool is64bit = node->getOpCodeValue() == TR::lsub;
         int opcodeConst = is64bit ? TR::lconst : TR::iconst;
         TR::DataType opcodeDataType = is64bit ? TR::Int64 : TR::Int32;

         TR_CISCNode *const1 = graph->getCISCNode(opcodeConst, true, 1);
         if (!const1)
            {
            const1 = new (trHeapMemory()) TR_CISCNode(trMemory(), opcodeConst, opcodeDataType, graph->incNumNodes(), 0, 0, 0, 1);
            const1->setNewCISCNode();
            graph->addNode(const1, 0, 0, 0);
            }

         TR_CISCNode *mul = new (trHeapMemory()) TR_CISCNode(trMemory(),
                                                             is64bit ? TR::lmul : TR::imul,
                                                             is64bit ? TR::Int64 : TR::Int32,
                                                             graph->incNumNodes(), dagId, 1, 2);
         mul->setNewCISCNode();
         mul->setChildren(childCisc, const1);
         graph->addNode(mul, 0, 0, 0);
         childCisc = mul;
         childList.add(childCisc);
         if (_lastCFGNode)
            {
            _lastCFGNode->setSucc(0, mul);     // _lastCFGNode must be the predecessor of newCisc.
            _lastCFGNode = mul;
            }

         childCisc = addAllSubNodes(graph, block, top, node, node->getChild(1), dagId);
         if (!childCisc) return 0;
         childList.add(childCisc);
         }
      else
         {
         for (i = 0; i < numCreateChildren; i++)
            {
            TR_CISCNode *childCisc = addAllSubNodes(graph, block, top, node, node->getChild(i), dagId);
            if (!childCisc) return 0;
            childList.add(childCisc);
            }
         }

      if (node->getOpCode().isLoadVarDirect()) // direct loads (e.g. iload)
         {
         TR_ASSERT(childList.isEmpty(), "Loads must have no child");
         int32_t refNum = node->getSymbolReference()->getReferenceNumber();
         TR_CISCNode *variable = graph->getCISCNode(TR_variable, true, refNum);
         if (!variable)
            {
            variable = new (trHeapMemory()) TR_CISCNode(trMemory(), TR_variable, TR::NoType, graph->incNumNodes(), 0,
                                       0, 0, refNum);
            variable->addTrNode(block, top, node);
            graph->addNode(variable);
            }

         numChildren = 1;
         newCisc = new (trHeapMemory()) TR_CISCNode(trMemory(), opcode, nodeDataType, graph->incNumNodes(), dagId,
                                   1, numChildren);
         newCisc->setIsNegligible();
         newCisc->setIsLoadVarDirect();
         childList.add(variable);

         }
      else if (node->getOpCode().isLoadConst()) // constants (e.g. iconst)
         {
         int32_t val;
         bool isConst = true;
         switch(opcode)
            {
            case TR::iconst:
               val = node->getInt();
               break;
            case TR::cconst:
               val = node->getConst<uint16_t>();
               break;
            case TR::sconst:
               val = node->getShortInt();
               break;
            case TR::bconst:
               val = node->getByte();
               break;
            case TR::lconst:
               val = node->getLongIntLow();
               if ((int64_t)val != node->getLongInt())
                  isConst = false;
               break;
            default:
               isConst = false;
               break;
            }

         if (isConst)
            {
            newCisc = graph->getCISCNode(opcode, true, val);
            if (newCisc)        // if the same node is already added
               graph->addTrNode(newCisc, block, top, node);
            else
               {
               newCisc = new (trHeapMemory()) TR_CISCNode(trMemory(), opcode, nodeDataType, graph->incNumNodes(), 0,
                                         0, 0, val);
               graph->addNode(newCisc, block, top, node);
               }
            return newCisc;
            }
         newCisc = new (trHeapMemory()) TR_CISCNode(trMemory(), opcode, nodeDataType, graph->incNumNodes(), dagId,
                              1, numChildren);
         }
      else if (node->getOpCode().isStoreDirect())
         {
         if (childList.isSingleton()) // for normal direct store (e.g. istore)
            {
            isReplaceChild = false;
            TR_CISCNode *child = childList.getListHead()->getData();
            int refNum = node->getSymbolReference()->getReferenceNumber();
            TR_CISCNode *variable = graph->getCISCNode(TR_variable, true, refNum);
            if (!variable)
               {
               variable = new (trHeapMemory()) TR_CISCNode(trMemory(), TR_variable, TR::NoType, graph->incNumNodes(), 0,
                                                           0, 0, refNum);
               variable->addTrNode(block, top, node);
               graph->addNode(variable);
               }
            if (!child->isInterestingConstant()) child->setDest(variable);

            numChildren = 2;
            newCisc = new (trHeapMemory()) TR_CISCNode(trMemory(), opcode, nodeDataType, graph->incNumNodes(), dagId,
                                                       1, numChildren);
            newCisc->setIsStoreDirect();
            childList.add(variable);
            }
         else
            {
            newCisc = new (trHeapMemory()) TR_CISCNode(trMemory(), opcode, nodeDataType, graph->incNumNodes(), dagId,
                                                       1, numChildren);
            }
         }
      else
         {
         if (node->getOpCode().isCall() &&
             graph->isRecordingAspectsByOpcode()) return 0;
         int32_t nSucc = node->getOpCode().isIf() ? 2 : (isSwitch ? node->getCaseIndexUpperBound()-1 : 1);
         if (opcode == TR::Case)
            {
            TR_ASSERT(numChildren == 0, "TR::Case: numChildren != 0 ???");
            numChildren = 1;
            newCisc = new (trHeapMemory()) TR_CISCNode(trMemory(), opcode, nodeDataType, graph->incNumNodes(), dagId,
                                      nSucc, numChildren);
            childList.add(graph->getCISCNode(top->getNode()->getChild(0)));
            }
         else
            {
            newCisc = new (trHeapMemory()) TR_CISCNode(trMemory(), opcode, nodeDataType, graph->incNumNodes(), dagId,
                                      nSucc, numChildren);
            }
         switch(newCisc->getOpcode())
            {
            case TR::BBStart:
            case TR::BBEnd:
               newCisc->setOtherInfo(block->getNumber());
               // fall through
            case TR::Goto:
            case TR::asynccheck:
            case TR::allocationFence:
            case TR::treetop:
            case TR::table:
            case TR::lookup:
            case TR::PassThrough:
               newCisc->setIsNegligible();      // These opcodes are negligible.
               break;
            case TR::compressedRefs:
               // compressed refs are really just another kind of treetop so treat them accordingly
               {
               static const bool anchorIsNegligible = feGetEnv("TR_disableIRNegligibleCompressedRefs") == NULL;
               if (anchorIsNegligible)
                  newCisc->setIsNegligible();
               }
               break;
            }

         if (node->getOpCode().isBranch())
            {
            _backPatchList.add(newCisc);        // The branch destination will be set later.
            }
         }

      if (_lastCFGNode)
         {
         _lastCFGNode->setSucc(0, newCisc);     // _lastCFGNode must be the predecessor of newCisc.
         }

      if (newCisc->getNumSuccs() > 0) _lastCFGNode = newCisc;
      graph->addNode(newCisc, block, top, node);// add newCisc

      if (isSwitch)     // When the node is a switch, children will be added later for analyzing easier.
         {
         for (i = 1; i < numChildren; i++)
            {
            TR_CISCNode *childCisc = addAllSubNodes(graph, block, top, node, node->getChild(i), dagId);
            if (!childCisc) return 0;
            childList.add(childCisc);
            switch (opcode)
               {
               case TR::lookup:
                  if (i >= 1)
                     {
                     newCisc->setSucc(i-1, childCisc);
                     if (i >= 2)
                        childCisc->setOtherInfo(node->getChild(i)->getCaseConstant());
                     }
                  break;
               case TR::table:
                  if (i >= 1)
                     {
                     newCisc->setSucc(i-1, childCisc);
                     if (i >= 2)
                        childCisc->setOtherInfo(i-2);
                     }
                  break;
               }
            }
         }

      ListIterator<TR_CISCNode> ciscLi(&childList);
      TR_CISCNode *child = ciscLi.getFirst();
      if (isReplaceChild)
         {
         for (i = numChildren; --i >= 0; child = ciscLi.getNext())
            {
            if (child->getFirstDest())
               {
               newCisc->addHint(child);
               child = child->getFirstDest();
               }
            newCisc->setChild(i, child);
            }
         }
      else
         {
         for (i = numChildren; --i >= 0; child = ciscLi.getNext()) newCisc->setChild(i, child);
         }
      return newCisc;
      }
   }



//*****************************************************************************************
// Convert TR::Block to TR_CISCGraph
//*****************************************************************************************
bool
TR_CISCTransformer::makeCISCGraphForBlock(TR_CISCGraph *graph, TR::Block *const block, int32_t dagId)
   {
   if (trace())
      traceMsg(comp(), "\t\tmakeCISCGraphForBlock: Building CISCGraph for block %d.\n", block->getNumber());

   TR::TreeTop *top = block->getEntry();
   TR::TreeTop *end = block->getExit();

   if (!top) return true;
   while(true)
      {
      if (!addAllSubNodes(graph, block, top, NULL, top->getNode(), dagId))
         {
         if (trace())
            traceMsg(comp(), "\t\tFailed to create CISCNode for Node %p in block %d : %p\n", top->getNode(), block->getNumber(), block);
         return false;
         }

      if (top == end) break;
      top = top->getNextTreeTop();
      }
   if (_lastCFGNode)
      {
      if (!_backPatchList.find(_lastCFGNode))
         _backPatchList.add(_lastCFGNode);
      _lastCFGNode = 0;
      }
   return true;
   }


//*****************************************************************************************
// Resolve unknown destinations of branches
// If the destination is not included in the region, it will set "exitNode" to the destination.
//*****************************************************************************************
void
TR_CISCTransformer::resolveBranchTargets(TR_CISCGraph *graph, TR_CISCNode *exitNode)
   {
   ListIterator<TR_CISCNode> ci(&_backPatchList);
   TR_CISCNode *p;
   for (p = ci.getFirst(); p != 0; p = ci.getNext()) // each element of _backPatchList
      {
      TR_ASSERT(p->getTrNodeInfo()->isSingleton(), "each branch node must correspond to a single TR node");
      TR::Node *trNode = p->getHeadOfTrNodeInfo()->_node;
      TR_CISCNode *destCisc;
      TR::Node *destNode;

      if (trNode->getOpCode().isBranch())
         {
         destNode = trNode->getBranchDestination()->getNode();
         destCisc = graph->getCISCNode(destNode);
         if (!destCisc) destCisc = exitNode;            // set exitNode if the destination is outside
         p->setSucc(p->getNumSuccs()-1, destCisc);
         }
      else
         {
         // set fallthrough block
         destCisc = exitNode;             // set exitNode as default
         if (trNode->getOpCodeValue() == TR::BBEnd)
            {
            TR::TreeTop *nextTreeTop = trNode->getBlock()->getExit()->getNextTreeTop();
            if (nextTreeTop)
               {
               destNode = nextTreeTop->getNode();
               destCisc = graph->getCISCNode(destNode);
               if (!destCisc) destCisc = exitNode;             // set exitNode if the destination is outside
               }
            }
         p->setSucc(0, destCisc);
         }
      }

   for (p = ci.getFirst(); p != 0; p = ci.getNext())
      {
      uint32_t numSuccs = p->getNumSuccs();
      if (numSuccs >= 2)        // To exclude 0 and 1 quickly (reduce compilation time)
         {
         if (numSuccs == 2)     // Typical case in "numSuccs >= 2" (reduce compilation time)
            {
            TR_CISCNode *succ0 = p->getSucc(0);
            TR_CISCNode *succ1 = p->getSucc(1);
            if (succ0->getOpcode() == TR::BBEnd) p->setSucc(0, (succ0 = succ0->getSucc(0)));
            if (succ1->getOpcode() == TR::BBEnd) p->setSucc(1, (succ1 = succ1->getSucc(0)));
            if (p->getHeadOfTrNodeInfo()->_node->getOpCode().isIf())
               {
               if (succ0->getOpcode() == TR_exitnode ||            // if the fallthrough edge goes to exitNode, or
                   (p->getDagID() == succ1->getDagID() &&          // if the fallthrough edge goes to another dagId
                    p->getDagID() != succ0->getDagID()))           // and the branch edge goes to the same dagid
                  {
                  p->reverseBranchOpCodes();                       // swap the fallthrough and branch edges for canonicalization
                  }
               }
            }
         else
            {
            uint32_t idx;
            for (idx = 0; idx < numSuccs; idx++)
               {
               TR_CISCNode *s = p->getSucc(idx);
               if (s->getOpcode() == TR::BBEnd) p->setSucc(idx, s->getSucc(0));
               }
            TR_ASSERT(!p->getHeadOfTrNodeInfo()->_node->getOpCode().isIf(), "error");
            }
         }
      }
   }




uint16_t
TR_CISCTransformer::renumberDagId(TR_CISCGraph *graph, int32_t tempMaxDagId, int32_t bodyDagId)
   {
   int32_t newDagId = 0, newBodyDagId = -1;
   List<TR_CISCNode> *orgNodes = graph->getNodes();
   ListElement<TR_CISCNode> *cur = NULL, *next = NULL, *appendCursor = NULL, *newListTop = NULL;
   // renumber the dagIds of the nodes in the graph
   // initially, the exitNode, entryNode and treetop (eg. loads, stores, add/sub, ifcmp)
   // nodes in the loop are assigned ids of 3, 0, 2 resp.
   // for eg., the dagIds of nodes may look like:
   //  0L iconst -16  [] []
   //  2L isub        [] [40 41]
   //  2L aiadd       [] [39 42]
   //  0L bconst 1    [] []
   //  2L ibstore     [] [43 44]
   //  2L iadd        [] [40 22]
   //  2L istore      [] [46 4]
   //  2L ificmpge    [] [4 6]
   //  2L BBEnd       [] []
   //  3L exitnode    [] []
   //
   // the loop walks the list of nodes, reassigning ids so that exitNode gets 0, entryNode
   // gets 2 and all nodes in the loop (e.g. with 2L) get id 1. the children of these nodes
   // will get unique dagIds. note, we need 3 ptrs as we might need to hop over some nodes
   // during any iteration of the for loop
#if 0
   for (int32_t dId = tempMaxDagId; dId >= 0; dId--)
      {
      cur = orgNodes->getListHead();
      ListElement<TR_CISCNode> *prev = 0;
      while (cur)
         {
         next = cur->getNextElement();
         if (cur->getData()->getDagID() == dId)
            {
            cur->getData()->setDagID(newDagId);
           // if node belongs to outside of the loop,
           // give it a unique id, otherwise it belongs to the scc
            if (dId != bodyDagId)
               newDagId++;
            else
               newBodyDagId = newDagId;
            cur->setNextElement(0);
            if (!appendCursor)
               {
               newListTop = cur;
               appendCursor = cur;
               }
            else
               {
               appendCursor->setNextElement(cur);
               appendCursor = cur;
               }
            if (!prev)
               orgNodes->setListHead(next);
            else
               prev->setNextElement(next);
            }
         else
            prev = cur;
         cur = next;
         }
      if (dId == bodyDagId)
         {
         newBodyDagId = newDagId;
         newDagId++;
         }
      }
#else
   for (int32_t dId = tempMaxDagId; dId >= 0; dId--)
      {
      while (true)
         {
         cur = orgNodes->getListHead();
         if (!cur || cur->getData()->getDagID() != dId) break;
         orgNodes->setListHead(cur->getNextElement());
         cur->getData()->setDagID(newDagId);
         if (dId != bodyDagId)
            newDagId++;                   // outside of the loop body
         else
            newBodyDagId = newDagId;      // inside of the loop body
         cur->setNextElement(0);
         if (!appendCursor)
            {
            newListTop = cur;
            appendCursor = cur;
            }
         else
            {
            appendCursor->setNextElement(cur);
            appendCursor = cur;
            }
         }
      if (cur)
         {
         while (true)
            {
            next = cur->getNextElement();
            if (!next) break;
            if (next->getData()->getDagID() == dId)
               {
               cur->setNextElement(next->getNextElement());
               next->getData()->setDagID(newDagId);
               if (dId != bodyDagId)
                  newDagId++;                   // outside of the loop body
               else
                  newBodyDagId = newDagId;      // inside of the loop body
               next->setNextElement(0);
               if (!appendCursor)
                  {
                  newListTop = next;
                  appendCursor = next;
                  }
               else
                  {
                  appendCursor->setNextElement(next);
                  appendCursor = next;
                  }
               }
            else
               {
               cur = next;
               }
            }
         }
      if (dId == bodyDagId)
         {
         newBodyDagId = newDagId;
         newDagId++;
         }
      }
#endif
   TR_ASSERT(orgNodes->isEmpty(), "there are elements in orgNodes");
   orgNodes->setListHead(newListTop);
   graph->setNoFragmentDagId();
   graph->setNumDagIds(newDagId);
   return (uint16_t)newBodyDagId;
   }



// make our internal graph representation from the input IL code.
//
TR_CISCGraph *
TR_CISCTransformer::makeCISCGraph(List<TR::Block> *pred,
                                  List<TR::Block> *body,
                                  List<TR::Block> *succ)
   {
   int32_t dagId = 1, bodyDagId;
   TR_CISCGraph *graph = new (trHeapMemory()) TR_CISCGraph(trMemory(), comp()->signature());
   TR::Block *block;
   ListIterator<TR::Block> bi(pred);
   //ListElement<TR_CISCNode> *head;

   graph->setRecordingAspectsByOpcode(false);   // Stop recording aspects outside of the loop
   _backPatchList.init();
   comp()->incVisitCount();

   // make entry node
   TR_CISCNode *newCisc = new (trHeapMemory()) TR_CISCNode(trMemory(), TR_entrynode, TR::NoType, graph->incNumNodes(), dagId, 1, 0);
   graph->setEntryNode(newCisc);
   graph->addNode(newCisc);
   _lastCFGNode = newCisc;

   static const bool includePreds = feGetEnv("TR_idiomIncludePreds") != NULL;
   if (includePreds)
      {
      if (trace())
         traceMsg(comp(), "\tmakeCISCGraph: Building CISCGraph for Predecessor Blocks.\n");
      for (block = bi.getFirst(); block != 0; block = bi.getNext())
         {
         if (!makeCISCGraphForBlock(graph, block, dagId)) return 0;
         }
      }

   dagId++;
   bodyDagId = dagId;
   bi.set(body);
   if (trace())
      traceMsg(comp(), "\tmakeCISCGraph: Building CISCGraph for Loop Body Blocks.\n");
   graph->setRecordingAspectsByOpcode(true);    // Start recording aspects inside of the loop
   for (block = bi.getFirst(); block != 0; block = bi.getNext())
      {
      if (!makeCISCGraphForBlock(graph, block, dagId)) return 0;
      }
   graph->setRecordingAspectsByOpcode(false);   // Stop recording aspects outside of the loop


   dagId++;
#if 0
   bi.set(succ);
   for (block = bi.getFirst(); block != 0; block = bi.getNext())
      {
      if (!makeCISCGraphForBlock(graph, block, dagId)) return 0;
      }
#endif

   TR_CISCNode *exitNode = new (trHeapMemory()) TR_CISCNode(trMemory(), TR_exitnode, TR::NoType, graph->incNumNodes(), dagId, 0, 0);
   graph->addNode(exitNode);
   graph->setExitNode(exitNode);
   if (_lastCFGNode)
      {
      _lastCFGNode->setSucc(0, exitNode);
      _lastCFGNode = 0;
      }

   // add "iconst -ahsize" if nonexistent
   int32_t ahsize = -(int32_t)TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
   uint32_t opcode = TR::Compiler->target.is64Bit() ? TR::lconst : TR::iconst;
   TR::DataType nodeDataType = TR::Compiler->target.is64Bit() ? TR::Int64 : TR::Int32;

   if (!graph->getCISCNode(opcode, true, ahsize))
      {
      graph->addNode(new (trHeapMemory()) TR_CISCNode(trMemory(), opcode, nodeDataType, graph->incNumNodes(), 0, 0, 0, ahsize));
      }

   bodyDagId = renumberDagId(graph, dagId, bodyDagId);
   resolveBranchTargets(graph, exitNode);
   // setup the dagIds2Nodes table & a list containing
   // all the nodes except BBStarts and BBEnds
   graph->createInternalData(bodyDagId);
   graph->modifyTargetGraphAspects();

   return graph;
   }



//***************************************************************************************
// Main driver for TR_CISCTransformer
//***************************************************************************************
int32_t TR_CISCTransformer::perform()
   {

   //TO_BE_ENABLED
   ///return 0;

   static int enable = -1;
   if (enable < 0)
      {
      char *p = feGetEnv("DISABLE_CISC");
      enable = p ? 0 : 1;
      }
   static int disableLoopNumber = -1;
   if (disableLoopNumber == -1)
      {
      char *p = feGetEnv("DISABLE_LOOP_NUMBER");
      disableLoopNumber = p ? atoi(p) : -2;
      }
   static int showStdout = -1;
   if (showStdout == -1)
      {
      char *p = feGetEnv("traceCISCVerbose");
      showStdout = p ? 1 : 0;
      }

   //FIXME: add TR_EnableIdiomRecognitionWarm to options
   int32_t methodHotness =  scorching;
                           //_compilation->getOption(TR_EnableIdiomRecognitionWarm) ? scorching :
                           //_compilation->getMethodHotness();
   //FIXME: remove this code if it works

   TR::Recompilation *recompInfo = comp()->getRecompilationInfo();
   if (!comp()->mayHaveLoops() ||
       methodHotness < minimumHotnessPrepared ||
       comp()->getProfilingMode() == JitProfiling ||    // if this method is now profiled
       !enable)
      return 0;

   _useDefInfo = optimizer()->getUseDefInfo();
   if (_useDefInfo == 0)
      return 0;

   // Required beyond the scope of the stack memory region
   int32_t cost = 0;

   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   _cfg = comp()->getFlowGraph();
   _rootStructure = _cfg->getStructure();
   _nodesInCycle = new (trStackMemory()) TR_BitVector(_cfg->getNextNodeNumber(), trMemory(), stackAlloc);
   _isGenerateI2L = TR::Compiler->target.is64Bit();
   _showMesssagesStdout = (VERBOSE || showStdout);

   // make loop candidates
   List<TR_RegionStructure> loopCandidates(trMemory());

   if (createLoopCandidates(&loopCandidates))
      {
      TR_CFGReversePostOrder revPost(trMemory());
      ListIterator<TR::CFGNode> revPostIterator(revPost.compute(_cfg));
      if (trace())
         revPost.dump(comp());

      bool modified = false;
      if (showMesssagesStdout()) printf("\nStarting CISCTransformer %s, %s\n",
                                        comp()->getHotnessName(comp()->getMethodHotness()),
                                        comp()->signature());
      if (trace())
         {
         traceMsg(comp(), "Starting CISCTransformer\n");
         comp()->dumpMethodTrees("Trees before transforming CISC instructions");
         }

      ListIterator<TR_RegionStructure> loopIt(&loopCandidates);

      TR_RegionStructure *nextLoop;

      List<TR::Block> bblistPred(comp()->trMemory());
      List<TR::Block> bblistBody(comp()->trMemory());
      List<TR::Block> bblistSucc(comp()->trMemory());
      List<TR_BitsKeepAliveInfo> BitsKeepAliveList(comp()->trMemory());
      _BitsKeepAliveList = BitsKeepAliveList;

      int32_t numNodes = _cfg->getNextNodeNumber();
      TR_BitVector cfgBV(numNodes, trMemory(), stackAlloc);
      TR::CFGNode *cfgnode;
      int loopNumber = 0;

      for (nextLoop = loopIt.getFirst(); nextLoop != 0; nextLoop = loopIt.getNext())
         {
         // make bb list for each loop
         TR::Block *block;
         bblistPred.init();
         bblistBody.init();
         bblistSucc.init();

         // add predecessors of loop header as long as there is no merge point. (i.e. extended BB)
         TR::Block *predBlock = findPredecessorBlockOfLoopEntry(nextLoop);
         if (predBlock && predBlock->getEntry())
            {
            TR_RegionStructure *region = NULL;

            // Get the parent of the region the outer region
            TR_Structure* blockStructure = predBlock->getStructureOf();
            if (blockStructure)
               region = (TR_RegionStructure*) blockStructure->getParent();

            if (region != NULL)
               {
               TR::CFGNode* cfgStart = _cfg->getStart();
               while(predBlock != cfgStart)
                  {
                  if (predBlock->getNumberOfRealTreeTops() > 300)
                     {
                     if (trace())
                        traceMsg(comp(), "Skip the predecessor %d, because it has too many TreeTops (%d).\n", predBlock->getNumber(), predBlock->getNumberOfRealTreeTops());
                     break; // To reduce unnecessary compilation time
                     }

                  bblistPred.add(predBlock);

                  TR::CFGEdgeList pred = predBlock->getPredecessors();

                  if (!(pred.size() == 1))  // Check if there exists more than one predecessor ==> merge point.
                     break;

                  predBlock = toBlock(pred.front()->getFrom());

                  // Get the parent of the region the outer region
                  TR_RegionStructure* predBlockRegion = NULL;
                  blockStructure = predBlock->getStructureOf();
                  if (blockStructure)
                     {
                     predBlockRegion = (TR_RegionStructure*) blockStructure->getParent();
                     }

                  // Check to see if predBlock is the same.
                  if (region != predBlockRegion)
                     {
                     if (trace())
                         traceMsg(comp(), "Skip the predecessor block_%d, because it is within another region/loop.\n",predBlock->getNumber());
                     break;
                     }
                  }
               }
            }


         // add BBs of loop body
         TR_RegionStructure::Cursor si(*nextLoop);
         TR_StructureSubGraphNode *node;
         cfgBV.empty();
         for (node = si.getFirst(); node != 0; node = si.getNext())
            {
            cfgBV.set(node->getNumber());
            }

         ListAppender<TR::Block> appender(&bblistBody);
         block = nextLoop->getEntry()->getStructure()->asBlock()->getBlock();
         for (cfgnode = revPostIterator.getFirst(); cfgnode; cfgnode = revPostIterator.getNext())
            {
            if (block == cfgnode->asBlock())
               break;
            }

         if (cfgnode == 0)
            continue;

         for (; cfgnode; cfgnode = revPostIterator.getNext())
            {
            int32_t bbnum = cfgnode->getNumber();
            if (cfgBV.isSet(bbnum))
               {
               block = cfgnode->asBlock();
               TR_ASSERT(block->getEntry() != 0, "assuming block->getEntry() != 0");
               appender.add(block);
               cfgBV.reset(bbnum);
               if (cfgBV.isEmpty())
                  break;
               }
            }

         if (!cfgBV.isEmpty())
            {
            for (cfgnode = revPostIterator.getFirst(); cfgnode; cfgnode = revPostIterator.getNext())
               {
               int32_t bbnum = cfgnode->getNumber();
               if (cfgBV.isSet(bbnum))
                  {
                  block = cfgnode->asBlock();
                  TR_ASSERT(block->getEntry() != 0, "assuming block->getEntry() != 0");
                  appender.add(block);
                  cfgBV.reset(bbnum);
                  if (cfgBV.isEmpty())
                     break;
                  }
               }
            }

         // add exit blocks of the loop
         ListIterator<TR::CFGEdge> ei(&nextLoop->getExitEdges());
         TR::CFGEdge *edge;
         for (edge = ei.getFirst(); edge != 0; edge = ei.getNext())
            {
            TR::CFGNode *from = edge->getFrom();
            TR::CFGNode *to = edge->getTo();
            int32_t toNum = to->getNumber();
            if (!toStructureSubGraphNode(from)->getStructure()->asBlock()) continue;
            from = toStructureSubGraphNode(from)->getStructure()->asBlock()->getBlock();
            // get the corresponding cfg edge
            //
            auto edgeFrom = from->getSuccessors().begin();
            for (; edgeFrom != from->getSuccessors().end(); ++edgeFrom)
               {
               block = toBlock((*edgeFrom)->getTo());
               if (block->getNumber() == toNum)
                  break;
               }
            if (edgeFrom != from->getSuccessors().end())
               {
               TR_ASSERT(block->getNumber() == toNum, "error block->getNumber() != toNum");
               if (block->getEntry() &&
                   !bblistSucc.find(block) &&
                   !bblistPred.find(block))
                  bblistSucc.add(block);
               }
            }

         _bblistPred = bblistPred;
         _bblistBody = bblistBody;
         _bblistSucc = bblistSucc;

         if (trace())
            {
            traceMsg(comp(), "Loop %d.\n\tAnalyzed predecessor, body and successor blocks.\n", nextLoop->getNumber());
            traceMsg(comp(), "\t\tPredecessors blocks:");
            ListIterator<TR::Block> bi(&bblistPred);
            for (block = bi.getFirst(); block != 0; block = bi.getNext())
               traceMsg(comp(), " %d:[%p]",block->getNumber(),block);
            traceMsg(comp(), "\n");

            traceMsg(comp(), "\t\tBody blocks:");
            bi.set(&bblistBody);
            for (block = bi.getFirst(); block != 0; block = bi.getNext())
               traceMsg(comp(), " %d:[%p]",block->getNumber(),block);
            traceMsg(comp(), "\n");

            traceMsg(comp(), "\t\tSuccessors blocks:");
            bi.set(&bblistSucc);
            for (block = bi.getFirst(); block != 0; block = bi.getNext())
               traceMsg(comp(), " %d:[%p]",block->getNumber(),block);
            traceMsg(comp(), "\n");
            }

         // Iterate through the loop body blocks to remove Bits.keepAlive() or Reference.reachabilityFence() calls.
         // This is a NOP function inserted into NIO libraries to keep the NIO object and its native
         // ptr alive until after the native pointer accesses.
         removeBitsKeepAliveCalls(&bblistBody);


         // make our internal graph representation
         TR_CISCGraph *graph;
         graph = makeCISCGraph(&bblistPred, &bblistBody, &bblistSucc);
         if (!graph)
            {
            if (trace()) traceMsg(comp(), "Loop %d.  Failed to make CISC Graph.\n", nextLoop->getNumber());
            restoreBitsKeepAliveCalls();
            continue;
            }
         if (loopNumber++ == disableLoopNumber)
            {
            restoreBitsKeepAliveCalls();
            continue; // for debug purpose
            }

         analyzeHighFrequencyLoop(graph, nextLoop); // Analyze if frequently iterated loop.

         setCurrentLoop(nextLoop);

         if (trace())
            graph->dump(comp()->getOutFile(), comp());

         bool modifiedThisLoop = false;
         _candidatesForShowing.init();
         for (int32_t i = 0; i < numPreparedCISCGraphs; i++)        // for each idiom
            {
            TR_CISCGraph *prepared = preparedCISCGraphs[i];
            if (prepared)
               {
               if (computeTopologicalEmbedding(prepared, graph))// Analyze matching for the idiom
                  {
                  modified = true;                              // IL is modified
                  modifiedThisLoop = true;
                  if (trace())
                     {
                     traceMsg(comp(), "Transformed %s\n", prepared->getTitle());
                     comp()->dumpMethodTrees("Trees after transforming CISC instruction");
                     }
                  break;
                  }
               }
            }
         // Restore the keepAliveCalls
         restoreBitsKeepAliveCalls();

         if (!modifiedThisLoop) showCandidates();
         }


      if (modified)
         {
         _cfg->setStructure(0);
         // Use/def info and value number info are now bad.
         //
         optimizer()->setUseDefInfo(NULL);
         optimizer()->setValueNumberInfo(0);

         if (trace())
            {
            traceMsg(comp(), "Ending CISCTransformer\n");
            comp()->dumpFlowGraph();
            comp()->dumpMethodTrees("Trees after transforming CISC instructions");
            }
         }

      if (showMesssagesStdout()) printf("Exiting CISCTransformer\n");
      cost = 1;
      }
   } // scope for stack memory region

   manager()->incNumPassesCompleted();
   return cost;
   }

const char *
TR_CISCTransformer::optDetailString() const throw()
   {
   return "O^O IDIOM RECOGNITION: ";
   }

// check if the block is really in the loop body
//
bool
TR_CISCTransformer::isBlockInLoopBody(TR::Block *block)
   {
   ListIterator<TR::Block> bi(&_bblistBody);
   for (TR::Block *b = bi.getFirst(); b; b = bi.getNext())
      {
      if (block->getNumber() == b->getNumber())
         return true;
      }
   return false;
   }


void
TR_CISCTransformer::showEmbeddedData(char *title, uint8_t *data)
   {
   int32_t i, j;
   traceMsg(comp(), "%s\n    ",title);
   for (j = 0; j < _numPNodes; j++)
      {
      traceMsg(comp(), "%3d",j);
      }
   traceMsg(comp(), "\n  --");
   for (j = 0; j < _numPNodes; j++)
      {
      traceMsg(comp(), "---");
      }
   traceMsg(comp(), "\n");
   for (i = 0; i < _numTNodes; i++)
      {
      traceMsg(comp(), "%3d:",i);
      for (j = 0; j < _numPNodes; j++)
         {
         uint8_t this_result = data[idx(j, i)];
         if (this_result == _Unknown || this_result == _NotEmbed)
            traceMsg(comp(), "|  ");
         else
            traceMsg(comp(), "| %X",data[idx(j, i)]);
         }
      traceMsg(comp(), "\n");
      }
   }


//***************************************************************************************
// It returns the result whether parents of p correspond to those of t.
// It also checks whether the expression is commutative.
//***************************************************************************************
bool
TR_CISCTransformer::checkParents(TR_CISCNode *p, TR_CISCNode *t, uint8_t *result, bool *inLoop, bool *optionalParents)
   {
   ListIterator<TR_CISCNode> pi(p->getParents());
   ListIterator<TR_CISCNode> ti(t->getParents());
   TR_CISCNode *pn, *tn;
   bool isTargetInsideOfLoop = false;
   bool allOptionalParents = true;
   for (pn = pi.getFirst(); pn; pn = pi.getNext())      // for each parent of p
      {
      uint32_t pnOpc = pn->getOpcode();
      uint32_t tmpIdx = idx(pn->getID(), 0);
      int32_t pIndex = 0;
      const bool commutative = pn->isCommutative();
      const bool isPnInsideOfLoop = !pn->isOutsideOfLoop();
      if (!commutative)
         {
         for (pIndex = pn->getNumChildren(); --pIndex >= 0; )
            {
            if (pn->getChild(pIndex) == p) break;
            }
         }
      TR_ASSERT(pIndex >= 0, "error!");
      for (tn = ti.getFirst(); tn; tn = ti.getNext())   // for each parent of t
         {
         if (isPnInsideOfLoop && tn->isOutsideOfLoop())
            continue;
         if (pn->isEqualOpc(tn))
            {
            if (result[tmpIdx + tn->getID()] == _Embed &&
                (commutative || tn->getChild(pIndex) == t)) break;
            }
         else
            {
            if (tn->getIlOpCode().isLoadVarDirect())    /* search one more depth */
               {
               ListIterator<TR_CISCNode> tci(tn->getParents());
               TR_CISCNode *tcn;
               for (tcn = tci.getFirst(); tcn; tcn = tci.getNext())
                  {
                  if (pn->isEqualOpc(tcn) &&
                      result[tmpIdx + tcn->getID()] == _Embed &&
                      (commutative || tcn->getChild(pIndex) == tn)) break;
                  }
               if (tcn) break;
               }
            }
         }
      if (!tn)
         {
         if (!pn->isOptionalNode())
            {
            return false;
            }
         else
            {
            if (!pn->getParents()->isEmpty() && !pn->isSkipParentsCheck())
               {
               bool nextInLoop = false;
               bool nextOptionalParents = false;
               if (checkParents(pn, t, result, &nextInLoop, &nextOptionalParents))
                  {
                  if (!nextOptionalParents) allOptionalParents = false;
                  if (nextInLoop) isTargetInsideOfLoop = true;
                  }
               else
                  {
                  return false;
                  }
               }
            }
         }
      else
         {
         if (!pn->isOptionalNode()) allOptionalParents = false;
         if (!tn->isOutsideOfLoop()) isTargetInsideOfLoop = true;
         }
      }
   *optionalParents = allOptionalParents;
   *inLoop = isTargetInsideOfLoop;
   return true;
   }


//***************************************************************************************
// It computes embedding information for an input data dependence graph.
// Because this graph consists of a directed acyclic graph (DAG), it handles only a DAG.
// It uses the topological embedding algorithm (dag_embed) by walking data dependence graph
// from leaf to root and find the candidate nodes.
// Note that we need to find candidates among the leaf nodes in the data dependence graph.
// The function checkParentsNonRec() finds candidates of leaf nodes by analyzing their ancestors.
// When there are multiple candidates of a leaf, we will exclude those candidates
// which are unlikely to be matched to the leaf.
// In this case, we will perform the topological embedding again.
//***************************************************************************************
bool
TR_CISCTransformer::computeEmbeddedForData()
   {
   uint8_t *const result = _embeddedForData;
   bool ret = false;
   bool skipScreening = false;
   const bool enableWriteBarrierConversion = TR::Compiler->om.writeBarrierType() == gc_modron_wrtbar_none;

   memset(result, 0, _sizeResult);
   TR_CISCNode *p, *t;
   ListElement<TR_CISCNode> *const plistHead = _P->getOrderByData()->getListHead();
   ListElement<TR_CISCNode> *const tlistHead = _T->getOrderByData()->getListHead();
   ListElement<TR_CISCNode> *ple, *tle;
   uint32_t i;

   while(true)  // This loop is for narrowing down candidates of a leaf.
      {
      // Perform topological embedding algorithm (dagEmbed)
      for (ple = plistHead; ple; ple = ple->getNextElement())
         {
         p = ple->getData();
         const uint32_t pOpc = p->getOpcode();
         const uint32_t tmpIdx = idx(p->getID(), 0);
         const int32_t pOtherInfo = p->getOtherInfo();
         const uint16_t numPChi = p->getNumChildren();
         const bool isVariable = (pOpc == TR_variable);
         const bool isBoolTable = (pOpc == TR_booltable);
         const bool isAllowWrtbar = enableWriteBarrierConversion && (pOpc == TR_inbstore || pOpc == TR_indstore);
         const bool isCheckOtherInfo = p->isInterestingConstant();
         const bool isChildDirectlyConnected = p->isChildDirectlyConnected();
         const bool isNecessaryScreening = p->isNecessaryScreening();
         const bool commutative = p->isCommutative();
         const bool checkLoopInvariant =
            pOpc == TR_variableORconst
            || pOpc == TR_quasiConst
            || pOpc == TR_quasiConst2
            || pOpc == TR_arraybase;
         const bool isOptionalNode = p->isOptionalNode();
         bool existEmbed = false;
         tle = tlistHead;
         while(true)
            {
            t = tle->getData();
            const uint16_t numTChi = t->getNumChildren();
            const uint32_t tOpc = t->getOpcode();
            const uint32_t index = tmpIdx + t->getID();
            tle = tle->getNextElement();
            bool isEmbed = true;

            // check degree
            if (numPChi != numTChi && numPChi != 0)
               {
               if ((!isBoolTable || numTChi < 1) &&
                   (!isAllowWrtbar || numTChi < 2))
                  isEmbed = false;
               }

            if (isEmbed)
               {
               if (skipScreening &&
                   isNecessaryScreening)
                  {
                  existEmbed = true;
                  isEmbed = (result[index] == _Embed);
                  }
               else if ((isEmbed = p->isEqualOpc(t)) == true)
                  {
                  // if its a constant, check if values match
                  if (isCheckOtherInfo &&
                      pOtherInfo != t->getOtherInfo())
                     {
                     isEmbed = false;
                     }
                  else if (isNecessaryScreening)
                     {
                     if (checkLoopInvariant)
                        {
                        if (tOpc == TR_variable)
                           {
                           ListIterator<TR_CISCNode> parenti(t->getParents());
                           TR_CISCNode *parent;
                           for (parent = parenti.getFirst(); parent; parent = parenti.getNext())
                              {
                              if (!parent->isOutsideOfLoop() &&
                                  parent->getIlOpCode().isStoreDirect())
                                 {
                                 if (trace()) traceMsg(comp(), "pID%d: tID%d isn't loop invariant because of %d\n", p->getID(), t->getID(), parent->getID());
                                 isEmbed = false;
                                 break;
                                 }
                              }
                           }
                        }

                     // finds candidates of leaf nodes by analyzing their ancestors.
                     //isEmbed = p->checkParents(t, DEPTH_CHECKPARENTS);
                     //FIXME: enable the recursive routine
                     //
                     if (isEmbed) isEmbed = TR_CISCNode::checkParentsNonRec(p, t, DEPTH_CHECKPARENTS, comp());
                     }
                  }

               if (isEmbed)
                  {
                  if (p->isStoreDirect())
                     {
                     isEmbed = false;
                     TR_ASSERT(numPChi == 2, "error");
                     uint8_t chiData = result[idx(p->getChild(0)->getID(), t->getChild(0)->getID())];
                     if (chiData == _Embed || (chiData == _Desc && !isChildDirectlyConnected))
                        {
                        chiData = result[idx(p->getChild(1)->getID(), t->getChild(1)->getID())];
                        if (chiData == _Embed)  // allow only _Embed
                           isEmbed = true;
                        }
                     }
                  else
                     {
                     // For commutative expressions, we try to swap operands for the comparison.
                     if (commutative && numPChi == 2)
                        {
                        TR_CISCNode *pch0 = p->getChild(0);
                        TR_CISCNode *tch0 = t->getChild(0);
                        TR_CISCNode *tch1 = t->getChild(1);
                        uint32_t pIdx0 = idx(pch0->getID(), 0);
                        uint8_t chiData00 = result[pIdx0 + tch0->getID()];
                        uint8_t chiData01 = result[pIdx0 + tch1->getID()];
                        if ((chiData00 == _Embed || chiData01 == _Embed) ||
                            ((chiData00 == _Desc) &&
                             (!isChildDirectlyConnected ||
                              (tch0->isLoadVarDirect() &&
                               _Embed == result[pIdx0 + tch0->getChild(0)->getID()]))) ||
                            ((chiData01 == _Desc) &&
                             (!isChildDirectlyConnected ||
                              (tch1->isLoadVarDirect() &&
                               _Embed == result[pIdx0 + tch1->getChild(0)->getID()]))))
                           {
                           // OK!
                           TR_CISCNode *pch1 = p->getChild(1);
                           uint32_t pIdx1 = idx(pch1->getID(), 0);
                           uint8_t chiData10 = result[pIdx1 + tch0->getID()];
                           uint8_t chiData11 = result[pIdx1 + tch1->getID()];
                           if ((chiData10 == _Embed || chiData11 == _Embed) ||
                               ((chiData10 == _Desc) &&
                                (!isChildDirectlyConnected ||
                                 (tch0->isLoadVarDirect() &&
                                  _Embed == result[pIdx1 + tch0->getChild(0)->getID()]))) ||
                               ((chiData11 == _Desc) &&
                                (!isChildDirectlyConnected ||
                                 (tch1->isLoadVarDirect() &&
                                  _Embed == result[pIdx1 + tch1->getChild(0)->getID()]))))
                              {
                              // OK!
                              }
                           else
                              {
                              isEmbed = false;
                              }
                           }
                        else
                           {
                           isEmbed = false;
                           }
                        }
                     else
                        {
                        for (i = 0; i < numPChi; i++)
                           {
                           TR_CISCNode *pch = p->getChild(i);
                           TR_CISCNode *tch = t->getChild(i);
                           uint8_t chiData = result[idx(pch->getID(), tch->getID())];
                           if (chiData == _Embed ||
                               ((chiData == _Desc) &&
                                (!isChildDirectlyConnected ||
                                 (tch->isLoadVarDirect() &&
                                  _Embed == result[idx(pch->getID(), tch->getChild(0)->getID())]))))
                              {
                              // OK!
                              }
                           else
                              {
                              isEmbed = false;
                              break;
                              }
                           }
                        }
                     }
                  }
               }

            if (isEmbed)
               {
               TR_ASSERT(index == idx(p->getID(), t->getID()), "error");
               result[index] = _Embed;
               existEmbed = true;
               }
            else
               {
               if (numTChi == 1)                // for reducing compilation time
                  {
                  if (tOpc != TR::arraylength)   // The type of the child (ref) is different from the type of destination (int).
                     {
                     uint8_t chiData = result[tmpIdx + t->getChild(0)->getID()];
                     isEmbed = isDescOrEmbed(chiData);
                     }
                  }
               else if (numTChi == 2)           // for reducing compilation time
                  {
                  uint16_t idTChi0 = t->getChild(0)->getID();
                  uint16_t idTChi1 = t->getChild(1)->getID();
                  uint8_t chiData0 = result[tmpIdx + idTChi0];
                  uint8_t chiData1 = result[tmpIdx + idTChi1];
                  isEmbed = isDescOrEmbed(chiData0) | isDescOrEmbed(chiData1);
                  }
               else                             // Natural code
                  {
                  for (i = 0; i < numTChi; i++)
                     {
                     uint8_t chiData = result[tmpIdx + t->getChild(i)->getID()];
                     if (isDescOrEmbed(chiData))
                        {
                        isEmbed = true;
                        break;
                        }
                     }
                  }

               if (isOptionalNode && !isEmbed)
                  {
                  for (i = 0; i < numPChi; i++)
                     {
                     uint8_t chiData = result[idx(p->getChild(i)->getID(), t->getID())];
                     if (chiData == _Desc ||  chiData == _Embed)
                        {
                        isEmbed = true;
                        break;
                        }
                     }
                  }
               TR_ASSERT(index == idx(p->getID(), t->getID()), "error");
               if (isEmbed)
                  {
                  result[index] = _Desc;
                  if (t->isStoreDirect())
                     {
                     int32_t childIdx = tmpIdx + t->getChild(1)->getID();
                     result[childIdx] |= _Desc;
                     }
                  }
               else
                  result[index] = _NotEmbed;
               }
            if (!tle) break;
            }
         if (!existEmbed && !isOptionalNode) // cannot find any nodes corresponding to p
            {
            if (trace())
               {
               traceMsg(comp(), "data dag embedding failed for node %d.\n", p->getID());
               showEmbeddedData("Result of _embeddedForData", result);
               }
            return false;
            }
         }
      // Finish topological embedding algorithm (dagEmbed)


      // From here, I'd like to exclude those candidates
      // which are unlikely to be matched to each leaf.

      //showEmbeddedData("before screening1", result);
      skipScreening = true;
      bool modifyEmbeddedResult = false;
      TR_ScratchList<TR_CISCNode> singleList(comp()->trMemory()), multiList(comp()->trMemory());

      // This loop tries to exclude candidates by analyzing parents
      // It also creates two lists singleList and multiList.
      // * singleList has a pattern leaf node corresponding to a single target node
      // * multiList has a pattern leaf node corresponding to multiple target nodes
      for (ple = plistHead; ple; ple = ple->getNextElement())
         {
         p = ple->getData();
         if (!p->isNecessaryScreening()) continue;
         const bool lightScreening = p->isLightScreening();
         const uint32_t tmpIdx = idx(p->getID(), 0);
         int32_t count = 0;
         for (tle = tlistHead; tle; tle = tle->getNextElement())
            {
            t = tle->getData();
            if (result[tmpIdx + t->getID()] == _Embed)
               {
               bool inLoop = false;
               bool allOptionalParents = false;
               if (!(!checkParents(p, t, result, &inLoop, &allOptionalParents) || !(inLoop || lightScreening)))
                  {
                  count ++;
                  }
               }
            }
         bool checkOptionalParents = (count == 0);
         if (trace() && count == 0) traceMsg(comp(), "screening1: count=%d for p:%d\n",count,p->getID());
         count = 0;
         for (tle = tlistHead; tle; tle = tle->getNextElement())
            {
            t = tle->getData();
            if (result[tmpIdx + t->getID()] == _Embed)
               {
               bool inLoop = false;
               bool allOptionalParents = false;
               if (!checkParents(p, t, result, &inLoop, &allOptionalParents) || !(inLoop || lightScreening ||
                                                                                  (checkOptionalParents && allOptionalParents)))
                  {
                  modifyEmbeddedResult = true;
                  result[tmpIdx + t->getID()] = _NotEmbed;
                  if (trace()) traceMsg(comp(), "screening1: set _NotEmbed to (%d, %d)\n",p->getID(),t->getID());
                  }
               else
                  {
                  count ++;
                  }
               }
            }
         if (count == 0)
            {
            if (!p->isOptionalNode())
               {
               if (trace())
                  {
                  traceMsg(comp(), "fail!! pID=%d.\n", p->getID());
                  showEmbeddedData("Result of _embeddedForData", result);
                  }
               return false;
               }
            }
         else if (count == 1)
            {
            singleList.add(p);
            }
         else if (count >= 2)
            {
            multiList.add(p);
            }
         }


      // This loop tries to exclude those candidates that already included in singleList.
      //showEmbeddedData("before screening2", result);
      if (!multiList.isEmpty())
         {
         ListIterator<TR_CISCNode> mi(&multiList);
         ListIterator<TR_CISCNode> si(&singleList);
         TR_CISCNode *s, *m;
         for (m = mi.getFirst(); m; m = mi.getNext())
            {
            TR_ASSERT(m->isNecessaryScreening(), "error!");
            if (m->isLightScreening()) continue;
            const uint32_t tmpMIdx = idx(m->getID(), 0);
            const int32_t mOpcode = m->getOpcode();
            bool thisScreening = false;
            // Try a set of the same opcode
            for (s = si.getFirst(); s; s = si.getNext())
               {
               if (mOpcode == s->getOpcode())
                  {
                  const uint32_t tmpSIdx = idx(s->getID(), 0);
                  uint32_t tID;
                  for (tID = 0; tID < _numTNodes; tID++ )
                     {
                     if (result[tmpSIdx + tID] == _Embed) break;
                     }
                  if (result[tmpMIdx + tID] == _Embed)
                     {
                     modifyEmbeddedResult = true;
                     thisScreening = true;
                     result[tmpMIdx + tID] = _NotEmbed;
                     if (trace()) traceMsg(comp(), "screening2, sameOpcode: set _NotEmbed to (%d, %d)\n",m->getID(),tID);
                     }
                  }
               }
            bool changeToSingle = false;
            if (thisScreening)
               {
               uint32_t tID;
               int32_t count = 0;
               for (tID = 0; tID < _numTNodes; tID++ )
                  {
                  if (result[tmpMIdx + tID] == _Embed) count++;
                  }
               if (count <= 1) changeToSingle = true;
               }
            // Try a set of different opcodes
            if (!changeToSingle)
               {
               for (s = si.getFirst(); s; s = si.getNext())
                  {
                  if (mOpcode != s->getOpcode())
                     {
                     const uint32_t tmpSIdx = idx(s->getID(), 0);
                     uint32_t tID;
                     for (tID = 0; tID < _numTNodes; tID++ )
                        {
                        if (result[tmpSIdx + tID] == _Embed) break;
                        }
                     if (result[tmpMIdx + tID] == _Embed)
                        {
                        modifyEmbeddedResult = true;
                        thisScreening = true;
                        result[tmpMIdx + tID] = _NotEmbed;
                        if (trace()) traceMsg(comp(), "screening2, others: set _NotEmbed to (%d, %d)\n",m->getID(),tID);
                        }
                     }
                  }
               }
            }
         }
      if (!modifyEmbeddedResult) break;
      }

   for (ple = plistHead; ple; ple = ple->getNextElement())
      {
      p = ple->getData();
      if (!p->isNecessaryScreening()) continue;
      const bool lightScreening = p->isLightScreening();
      const uint32_t tmpIdx = idx(p->getID(), 0);
      for (tle = tlistHead; tle; tle = tle->getNextElement())
         {
         t = tle->getData();
         if (result[tmpIdx + t->getID()] == _Embed)
            {
            bool inLoop = false;
            bool allOptionalParents = false;
            if (!checkParents(p, t, result, &inLoop, &allOptionalParents) || !(inLoop || lightScreening))
               {
               result[tmpIdx + t->getID()] = _NotEmbed;
               if (trace()) traceMsg(comp(), "screening3: set _NotEmbed to (%d, %d)\n",p->getID(),t->getID());
               }
            }
         }
      }

   if (trace())
      showEmbeddedData("Result of _embeddedForData", result);
   return true;
   }





//***************************************************************************************
// It corresponds to the algorithm "dag_embed()" in Fu's paper. (p.385)
// I relaxed degree checking for TR_booltable to find switch-case statements.
//***************************************************************************************
bool
TR_CISCTransformer::dagEmbed(TR_CISCNode *np, TR_CISCNode *nt)
   {
   uint8_t *const result = _embeddedForCFG;
   const uint32_t numPSucc = np->getNumSuccs();
   const uint32_t numTSucc = nt->getNumSuccs();
   bool isEmbed = false;
   const uint32_t tmpIdx = idx(np->getID(), 0);
   const uint32_t index = tmpIdx + nt->getID();
   uint32_t i;

   if (_embeddedForData[index] == _Embed &&
       ((numPSucc == numTSucc) || (numPSucc == 0)))
      {
      const bool isSuccDirectlyConnected = np->isSuccDirectlyConnected();
      isEmbed = true;
      uint8_t *const result = _embeddedForCFG;
      if (np->getOpcode() == TR_booltable)
         {
         TR_ASSERT(numPSucc == 2, "error!!");
         uint16_t idPSucc0 = np->getSucc(0)->getID();
         uint16_t idTSucc0 = nt->getSucc(0)->getID();
         uint16_t idPSucc1 = np->getSucc(1)->getID();
         uint16_t idTSucc1 = nt->getSucc(1)->getID();
         uint8_t succData01 = result[idx(idPSucc0, idTSucc1)];
         uint8_t succData10 = result[idx(idPSucc1, idTSucc0)];
         if (isDescOrEmbed(succData01) & isDescOrEmbed(succData10))
            {
            nt->reverseBranchOpCodes();
            }
         }
      for (i = 0; i < numPSucc; i++)
         {
         uint16_t idPSucc = np->getSucc(i)->getID();
         uint16_t idTSucc = nt->getSucc(i)->getID();
         uint8_t succData = result[idx(idPSucc, idTSucc)];
         if ((succData != _Desc || isSuccDirectlyConnected) && succData != _Embed)
            {
            isEmbed = false;
            break;
            }
         }
      }
   if (isEmbed)
      {
      result[index] = _Embed;
      return true;
      }
   else
      {
      TR_ASSERT(index == idx(np->getID(), nt->getID()), "error");
      if (numTSucc == 1)                 // for reducing compilation time
         {
         uint8_t succData = result[tmpIdx + nt->getSucc(0)->getID()];
         result[index] = isDescOrEmbed(succData) ? _Desc : _NotEmbed;
         }
      else if (numTSucc == 0)            // for reducing compilation time
         {
         result[index] = _NotEmbed;
         }
      else                               // Natural code
         {
         for (i = 0; i < numTSucc; i++)
            {
            uint16_t idTSucc = nt->getSucc(i)->getID();
            uint8_t succData = result[tmpIdx + idTSucc];
            if (isDescOrEmbed(succData))
               {
               isEmbed = true;
               break;
               }
            }
         result[index] = isEmbed ? _Desc : _NotEmbed;
         }
      }
   return false;
   }


//***************************************************************************************
// It corresponds to the algorithm "cycle_embed()" in Fu's paper. (pp.387-388)
// I relaxed degree checking for TR_booltable to find switch-case statements.
//***************************************************************************************
bool
TR_CISCTransformer::cycleEmbed(uint16_t dagP, uint16_t dagT)
   {
   const List<TR_CISCNode> *dagId2NodesP = _P->getDagId2Nodes();
   const List<TR_CISCNode> *dagId2NodesT = _T->getDagId2Nodes();
   List<TR_CISCNode> dagPList = dagId2NodesP[dagP];
   List<TR_CISCNode> dagTList = dagId2NodesT[dagT];
   ListIterator<TR_CISCNode> pi(&dagPList);
   ListIterator<TR_CISCNode> ti(&dagTList);
   uint8_t *result = _embeddedForCFG;
   uint8_t *const embeddedForData = _embeddedForData;
   uint32_t i;

   memset(_EM, 0, _sizeResult);
   memset(_DE, 0, _sizeDE);
   TR_CISCNode *np, *nt;
   bool isEmbed;
   for (np = pi.getFirst(); np; np = pi.getNext())
      {
      const uint16_t pId = np->getID();
      const uint32_t tmpIdx = idx(pId, 0);
      const uint32_t numPSucc = np->getNumSuccs();
      const bool isSuccDirectlyConnected = np->isSuccDirectlyConnected();
      const bool isBoolTable = (np->getOpcode() == TR_booltable);
      for (nt = ti.getFirst(); nt; nt = ti.getNext())
         {
         const uint32_t numTSucc = nt->getNumSuccs();
         const uint32_t index = tmpIdx + nt->getID();
         isEmbed = false;
         bool isLabelSame = (embeddedForData[index] == _Embed);
         uint32_t tOpc = nt->getOpcode();
         if (isLabelSame)
            {
            if ((numPSucc == numTSucc) || (numPSucc == 0))
               {
               isEmbed = true;
               if (isBoolTable)
                  {
                  TR_ASSERT(numPSucc == 2, "error!!");
                  uint16_t idPSucc0 = np->getSucc(0)->getID();
                  uint16_t idTSucc0 = nt->getSucc(0)->getID();
                  uint16_t idPSucc1 = np->getSucc(1)->getID();
                  uint16_t idTSucc1 = nt->getSucc(1)->getID();
                  uint8_t succData01 = result[idx(idPSucc0, idTSucc1)];
                  uint8_t succData10 = result[idx(idPSucc1, idTSucc0)];
                  if (isDescOrEmbed(succData01) & isDescOrEmbed(succData10))
                     {
                     nt->reverseBranchOpCodes();
                     }
                  }
               for (i = 0; i < numPSucc; i++)
                  {
                  uint8_t succData = result[idx(np->getSucc(i)->getID(), nt->getSucc(i)->getID())];
                  if ((succData != _Desc || isSuccDirectlyConnected) && succData != _Embed)
                     {
                     isEmbed = false;
                     break;
                     }
                  }
               }
            else if (isBoolTable)
               {
               if (tOpc == TR::Case)
                  {
                  i = (nt->isValidOtherInfo() ? 1 : 0);
                  uint8_t succData = result[idx(np->getSucc(i)->getID(), nt->getSucc(0)->getID())];
                  isEmbed = !((succData != _Desc || isSuccDirectlyConnected) && succData != _Embed);
                  }
               }
            }
         uint8_t chkEmbed;
         if (isEmbed)
            chkEmbed = _Embed;
         else
            {
            if (!isLabelSame)
               chkEmbed = _NotEmbed;
            else if (numPSucc != numTSucc)
               {
               chkEmbed = _NotEmbed;
               if (isBoolTable)
                  {
                  chkEmbed = _Cond;
                  if (tOpc == TR::Case)
                     {
                     i = (nt->isValidOtherInfo() ? 1 : 0);
                     uint8_t succData = result[idx(np->getSucc(i)->getID(), nt->getSucc(0)->getID())];
                     if (succData == _NotEmbed) chkEmbed = _NotEmbed;
                     }
                  }
               }
            else
               {
               chkEmbed = _Cond;
               for (i = 0; i < numPSucc; i++)
                  {
                  uint8_t succData = result[idx(np->getSucc(i)->getID(), nt->getSucc(i)->getID())];
                  if (succData == _NotEmbed)
                     {
                     chkEmbed = _NotEmbed;
                     break;
                     }
                  }
               }
            }
         _EM[index] = chkEmbed;
         if (chkEmbed == _Embed || chkEmbed == _Cond)
            _DE[pId] = 1;
         else
            {
            for (i = 0; i < numTSucc; i++)
               {
               uint8_t succData = result[tmpIdx + nt->getSucc(i)->getID()];
               if (isDescOrEmbed(succData))
                  {
                  _DE[pId] = 1;
                  break;
                  }
               }
            }
         }
      }

   for (np = pi.getFirst(); np; np = pi.getNext())
      {
      if (_DE[np->getID()] == 0)
         {
         for (np = pi.getFirst(); np; np = pi.getNext())
            {
            const uint32_t tmpIdx = idx(np->getID(), 0);
            for (nt = ti.getFirst(); nt; nt = ti.getNext())
               {
               result[tmpIdx + nt->getID()] = _NotEmbed; // set NotEmbed to all elements
               }
            }
         }
      }

   bool ret = true;
   for (np = pi.getFirst(); np; np = pi.getNext())
      {
      const uint32_t tmpIdx = idx(np->getID(), 0);
      bool existEmbed = false;
      for (nt = ti.getFirst(); nt; nt = ti.getNext())
         {
         const uint32_t index = tmpIdx + nt->getID();
         uint8_t chkEmbed = _EM[index];
         if (chkEmbed == _Embed || chkEmbed == _Cond)
            {
            result[index] = _Embed;
            existEmbed = true;
            }
         else
            {
            result[index] = _Desc;
            }
         }
      if (!existEmbed && !np->isOptionalNode())
         {
         ret = false;
         }
      }
   return ret;
   }



//***************************************************************************************
// It computes embedding information for an input CFG.
// Because CFG consists of DAGs and a cycle, it will handle them.
// It uses the result of computeEmbeddedForData() to find candidate nodes
// whose label is the same as that of each node in the idiom graph.
// It uses the topological embedding algorithm by walking the CFG edges from exit to entry.
// At the time, we traverse nodes based on the order of the DagIDs,
// which basically represent a post order of basic blocks.
//***************************************************************************************
bool
TR_CISCTransformer::computeEmbeddedForCFG()
   {
   TR_ASSERT(_embeddedForData != 0, "error");
   uint8_t *const result = _embeddedForCFG;
   memset(result, 0, _sizeResult);
   uint16_t dagP, dagT;
   uint16_t numDagIdsP = _P->getNumDagIds();
   uint16_t numDagIdsT = _T->getNumDagIds();
   const List<TR_CISCNode> *dagId2NodesP = _P->getDagId2Nodes();
   const List<TR_CISCNode> *dagId2NodesT = _T->getDagId2Nodes();
   TR_CISCNode *np, *nt;

   for (dagP = 0; dagP < numDagIdsP; dagP++)
      {
      List<TR_CISCNode> dagPList = dagId2NodesP[dagP];
      ListIterator<TR_CISCNode> pi(&dagPList);
      bool existEmbed = false;
      for (dagT = 0; dagT < numDagIdsT; dagT++)
         {
         List<TR_CISCNode> dagTList = dagId2NodesT[dagT];
         TR_ASSERT(!dagTList.isEmpty(), "empty dagId");
         if (dagTList.isSingleton())
            {
            nt = dagTList.getListHead()->getData();
            for (np = pi.getFirst(); np; np = pi.getNext())
               if (dagEmbed(np, nt)) existEmbed = true;
            }
         else
            {
            if (cycleEmbed(dagP, dagT)) existEmbed = true;
            }
         }
      if (!existEmbed)
         {
         if (trace())
            {
            traceMsg(comp(), "computeEmbeddedForCFG: Cannot find embedded nodes for dagP:%d\n",dagP);
            showEmbeddedData("Result of _embeddedForCFG", result);
            }
         return false;
         }
      }
   if (trace())
      showEmbeddedData("Result of _embeddedForCFG", result);
   return true;
   }


//***************************************************************************************
// It creates P2T and T2P tables from embedding information.
// (P and T denote Pattern and Target, respectively.)
// We can use them to find target nodes from pattern nodes, and vice versa.
//***************************************************************************************
bool
TR_CISCTransformer::makeLists()
   {
   TR_CISCNode *p, *t;
   ListIterator<TR_CISCNode> pi(_P->getNodes());
   ListIterator<TR_CISCNode> ti(_T->getOrderByData());
   uint8_t *const result = _embeddedForCFG;
   uint8_t *const embeddedForData = _embeddedForData;
   bool modify = false;

   memset(_P2T, 0, _sizeP2T);
   memset(_T2P, 0, _sizeT2P);

   int i;
   for (i = 0; i < _numPNodes; i++) _P2T[i].setRegion(trMemory()->heapMemoryRegion());
   for (i = 0; i < _numTNodes; i++) _T2P[i].setRegion(trMemory()->heapMemoryRegion());

   for (p = pi.getFirst(); p; p = pi.getNext())
      {
      const bool isEssential = p->isEssentialNode();
      const bool isSuccDirectlyConnected = p->isSuccDirectlyConnected();
      const uint32_t numPSucc = p->getNumSuccs();
      uint32_t pID = p->getID();
      List<TR_CISCNode> *pList = _P2T + pID;
      const uint32_t tmpIdx = idx(pID, 0);
      for (t = ti.getFirst(); t; t = ti.getNext())
         {
         uint32_t tID = t->getID();
         if (result[tmpIdx+tID] == _Embed)
            {
            bool isEmbed = true;
            if (isSuccDirectlyConnected)
               {
               for (uint32_t i = 0; i < numPSucc; i++)
                  {
                  if (result[idx(p->getSucc(i)->getID(), t->getSucc(i)->getID())] != _Embed)
                     {
                     isEmbed = false;
                     break;
                     }
                  }
               }
            if (isEmbed)
               {
               if (trace() &&
                   !_T2P[tID].isEmpty())
                  {
                  traceMsg(comp(), "makeLists: tID:%d corresponds to multiple nodes\n",tID);
                  }
               if (isEssential) t->setIsEssentialNode();
               pList->add(t);
               if (numPSucc == 0) t->setIsNegligible();
               _T2P[tID].add(p);
               }
            else
               {
               modify = true;
               result[tmpIdx+tID] = _Desc;
               embeddedForData[tmpIdx+tID] = _Desc;
               }
            }
         }
      if (pList->isMultipleEntry() &&
          p->getOpcode() == TR_variable)
         {
         if (!p->isOptionalNode())
            {
            if (trace()) traceMsg(comp(), "makeLists: pid:%d a variable corresponds to multiple nodes\n",p->getID());
            return false;       /* a variable corresponds to multiple nodes */
            }
         }
      }
   if (modify)
      {
      if (trace())
         showEmbeddedData("Result of _embeddedForCFG after makeLists", result);
      }
   return true;
   }



//*****************************************************************************
// Analyze relationships for parents and children
//*****************************************************************************
int32_t
TR_CISCTransformer::analyzeConnectionOnePairChild(TR_CISCNode *const p, TR_CISCNode *const t,
                                                  TR_CISCNode *const pn, TR_CISCNode *tn)
   {
   uint8_t *result = _embeddedForData;
   const uint32_t tmpIdx = idx(pn->getID(), 0);
   int32_t successCount = 0;
   TR_CISCNode *tnBefore = t;
   while(true)  // we may need to analyze descendant for tn because of negligible nodes (e.g. iload)
      {
      uint8_t chiData = result[tmpIdx + tn->getID()];
      if (chiData == _Embed)
         {
         // the connectivity of p and pn (child) is the same as that of t and tn (child)
         successCount++;
         tn->setIsParentSimplyConnected();
         break;
         }
      else if (chiData != _Desc || !tn->isNegligible() || tn->getNumSuccs() != 1)
         {
         bool success = false;
         if (tnBefore->isLoadVarDirect())
            {
            ListIterator<TR_CISCNode> defI;
            ListIterator<TR_CISCNode> useI;
            defI.set(tnBefore->getChains());
            TR_ASSERT(defI.getFirst(), "error");
            TR_CISCNode *d;
            success = true;
            // Check if t and tn are connected via a variable
            for (d = defI.getFirst(); d; d = defI.getNext())
               {
               if (d->getOpcode() == TR_entrynode)
                  {
                  success = false;
                  continue;
                  }
               TR_ASSERT(d->isStoreDirect(), "error");
               TR_CISCNode *childOfStore = d->getChild(0);
               if (_Embed != result[tmpIdx + childOfStore->getID()])
                  success = false; // Need to handle this store
               else
                  {
                  // Check if all uses are appropriate.
                  bool validThisStore = true;
                  bool includeExitNode = false;
                  if (!d->isNegligible())
                     {
                     useI.set(d->getChains());
                     TR_ASSERT(useI.getFirst(), "error");
                     TR_CISCNode *u;
                     List<TR_CISCNode> *pParents = p->getChild(0)->getParents();
                     ListIterator<TR_CISCNode> pParentsI(pParents);
                     for (u = useI.getFirst(); u; u = useI.getNext())
                        {
                        if (tnBefore == u) continue; // short-cut path
                        if (u->getDagID() != d->getDagID())
                           {
                           includeExitNode = true;
                           continue;
                           }
                        List<TR_CISCNode> *uParents = u->getParents();
                        TR_CISCNode *uParent;
                        TR_CISCNode *pParent;

                        // check all stored data are valid.
                        ListIterator<TR_CISCNode> uParentsI(uParents);
                        bool isEmbed = true;
                        for (uParent = uParentsI.getFirst(); uParent; uParent = uParentsI.getNext())
                           {
                           isEmbed = false;
                           for (pParent = pParentsI.getFirst(); pParent; pParent = pParentsI.getNext())
                              {
                              if (_Embed == result[idx(pParent->getID(), uParent->getID())])
                                 {
                                 isEmbed = true;
                                 break;
                                 }
                              }
                           if (!isEmbed) break;
                           }
                        if (!isEmbed)
                           {
                           success = validThisStore = false;
                           break;
                           }
                        }
                     }
                  if (validThisStore)
                     {
                     if (!includeExitNode) d->setIsNegligible();
                     childOfStore->setIsParentSimplyConnected();
                     }
                  }
               }
            }
         else if (tn->getOpcode() == TR_variable)
            {
            success = false;
            ListIterator<TR_CISCNode> hi(t->getHintChildren());
            TR_CISCNode *n;
            for (n = hi.getFirst(); n; n = hi.getNext()) // n is a right-hand side expression of a store
               {
               if (_Embed == result[tmpIdx + n->getID()])
                  {
                  n->setIsParentSimplyConnected();
                  success = true;
                  break;
                  }
               }

            // If we cannot use any hint, we'll look at a neighbor store instruction.
            if (!success)
               {
               List<TR_CISCNode> *preds = tnBefore->getPreds();
               while(preds->isSingleton())
                  {
                  n = preds->getListHead()->getData();
                  if (n->isStoreDirect() &&
                      n->getChild(1) == tnBefore &&
                      _Embed == result[tmpIdx + n->getChild(0)->getID()])
                     {
                     n->getChild(0)->setIsParentSimplyConnected();
                     success = true;
                     break;
                     }
                  preds = n->getPreds();
                  }
               }
            }
         if (success)
            {
            successCount++;
            }
         break;
         }
      if (tn->getNumChildren() == 0) break;
      tnBefore = tn;
      tn = tn->getChild(0);
      }
   return successCount;
   }



//*****************************************************************************
// Analyze relationships for parents, children, predecessors, successors.
// They are represented to four flags.
//*****************************************************************************
void
TR_CISCTransformer::analyzeConnectionOnePair(TR_CISCNode *const p, TR_CISCNode *const t)
   {
   int32_t i, /*j,*/ num;
   uint8_t *result;
   TR_CISCNode *tn, *pn;
   int32_t successCount;
   const bool isBoolTable = (p->getOpcode() == TR_booltable);
   const bool isCmpAll = (p->getOpcode() == TR_ifcmpall);

   result = _embeddedForData;
   num = p->getNumChildren();
   TR_ASSERT(t->getNumChildren() == num ||
          p->getOpcode() == TR_arrayindex ||
          p->getOpcode() == TR_arraybase ||
          p->getOpcode() == TR_quasiConst ||
          p->getOpcode() == TR_quasiConst2 ||
          p->getOpcode() == TR_booltable ||
          p->getOpcode() == TR_inbstore ||
          p->getOpcode() == TR_indstore, "error");
   if (p->getParents()->isEmpty() ||
       t->getParents()->isEmpty() ||
       t->getOpcode() == TR::Case ||
       t->getOpcode() == TR::awrtbari) t->setIsParentSimplyConnected();

   // Analyze connectivities for children and parents
   if (num == 0)
      {
      t->setIsChildSimplyConnected();
      }
   else
      {
      successCount = 0;
      for (i = 0; i < num; i++) // for each child
         {
         const bool commutative = p->isCommutative();
         pn = p->getChild(i);
         while (pn->isOptionalNode() &&
                _P2T[pn->getID()].isEmpty() &&
                pn->getNumChildren() > 0)
             {
             pn = pn->getChild(0);
             }
         const uint32_t tmpIdx = idx(pn->getID(), 0);

         while(true)
            {
            int32_t thisCount;
            if (commutative && num == 2)
               {
               if ((thisCount = analyzeConnectionOnePairChild(p, t, pn, t->getChild(i))) ||
                   (thisCount = analyzeConnectionOnePairChild(p, t, pn, t->getChild(1-i))))
                  {
                  successCount += thisCount;
                  break;
                  }
               }
            else
               {
               if (thisCount = analyzeConnectionOnePairChild(p, t, pn, t->getChild(i)))
                  {
                  successCount += thisCount;
                  break;
                  }
               }
            if (pn->isOptionalNode() &&
                pn->getNumChildren() > 0)
               {
               pn = pn->getChild(0);    // Try a child
               }
            else
               {
               break;   // fail
               }
            }
         }
      if (successCount == num)
         {
         t->setIsChildSimplyConnected();
         }
      }


   // Analyzing Predecessors and Successors
   result = _embeddedForCFG;
   num = t->getNumSuccs();
   if (t->getPreds()->isEmpty() || p->getPreds()->isEmpty()) t->setIsPredSimplyConnected();
   if (num == 0 || p->getNumSuccs() == 0)
      {
      t->setIsSuccSimplyConnected();
      }
   else if (p->getNumSuccs() == num)    // typical case
      {
      successCount = 0;
      for (i = 0; i < num; i++)         // for each successor
         {
         pn = p->getSucc(i);
         while (pn->isOptionalNode() &&
                _P2T[pn->getID()].isEmpty() &&
                pn->getNumSuccs() > 0)
            {
            pn = pn->getSucc(0);        // skip optional nodes
            }

         while(true)
            {
            tn = t->getSucc(i);
            const uint32_t tmpIdx = idx(pn->getID(), 0);

            while(true)
               {
               uint8_t chiData = result[tmpIdx + tn->getID()];
               if (chiData == _Embed)
                  {
                  // the connectivity of p and pn (succ) is the same as that of t and tn (succ)
                  successCount++;
                  tn->setIsPredSimplyConnected();
                  break;
                  }
               else if (chiData != _Desc || !tn->isNegligible() || tn->getNumSuccs() != 1)
                  {
                  if (isBoolTable || isCmpAll)
                     {
                     if (_Embed == result[idx(p->getID(), tn->getID())])
                        {
                        successCount++;
                        tn->setIsPredSimplyConnected();
                        }
                     }
                  break;
                  }
               tn = tn->getSucc(0);
               }

            if (!tn->isPredSimplyConnected() &&
                pn->isOptionalNode())
               {
               while (pn->isOptionalNode() &&
                      pn->getNumSuccs() > 0)
                  {
                  pn = pn->getSucc(0);        // skip optional nodes
                  }
               continue; // retry!
               }
            break; // Usually, this loop doesn't iterate.
            }
         }
      if (successCount == num)
         {
         t->setIsSuccSimplyConnected();
         }
      }
   else if (isBoolTable)
      {
      if (t->getOpcode() == TR::Case)
         {
         int32_t i = (t->isValidOtherInfo() ? 1 : 0);
         pn = p->getSucc(i);
         tn = t->getSucc(0);
         while(true)
            {
            uint8_t chiData = result[idx(pn->getID(), tn->getID())];
            if (chiData == _Embed)
               {
               tn->setIsPredSimplyConnected();
               t->setIsSuccSimplyConnected();
               break;
               }
            else if (chiData != _Desc || !tn->isNegligible() || tn->getNumSuccs() != 1)
               {
               if (_Embed == result[idx(p->getID(), tn->getID())])
                  {
                  tn->setIsPredSimplyConnected();
                  t->setIsSuccSimplyConnected();
                  }
               break;
               }
            tn = tn->getSucc(0);
            }
         }
      }
   }


void
TR_CISCTransformer::showT2P()
   {
   if (trace())
      {
      TR_CISCNode *p, *t;
      int32_t dagT, numDagIdsT = _T->getNumDagIds();
      List<TR_CISCNode> *dagId2NodesT = _T->getDagId2Nodes();
      for (dagT = numDagIdsT; --dagT >= 0;)
         {
         ListIterator<TR_CISCNode> ti(dagId2NodesT + dagT);
         for (t = ti.getFirst(); t; t = ti.getNext())
            {
            uint32_t tID = t->getID();
            traceMsg(comp(), "%3d:",tID);
            if (!_T2P[tID].isEmpty())
               {
               //TR_ASSERT(_T2P[tID].isSingleton(), "it may be error (not sure).");
               ListIterator<TR_CISCNode> pi(_T2P + tID);
               for (p = pi.getFirst(); p; p = pi.getNext())
                  {
                  uint32_t pID = p->getID();
                  traceMsg(comp(), " %2d",pID);
                  }
               traceMsg(comp(), " %c%c%c%c", t->isSuccSimplyConnected() ? 'S' : 'x',
                                   t->isPredSimplyConnected() ? 'P' : 'x',
                                   t->isParentSimplyConnected() ? 'B' : 'x',
                                   t->isChildSimplyConnected() ? 'C' : 'x'
                      );
               if (t->isNegligible()) traceMsg(comp(), "\t(negligible)");
               traceMsg(comp(), "\n");
               }
            else
               {
               if (t->isNegligible())
                  {
                  traceMsg(comp(), " negligible\n"); // negligible
                  }
               else
                  {
                  t->dump(comp()->getOutFile(), comp());
                  }
               }
            }
         }
      }
   }


//*****************************************************************************
// Analyze connectivities between pattern nodes and target nodes
//*****************************************************************************
void
TR_CISCTransformer::analyzeConnection()
   {
   TR_CISCNode *p, *t;
   ListIterator<TR_CISCNode> pi(_P->getNodes());
   SpecialNodeTransformerPtr specialTransformer = _P->getSpecialNodeTransformer();
   int count = 0;

   _T->setListsDuplicator();
   while(true)
      {
      for (p = pi.getFirst(); p; p = pi.getNext()) // for each pattern node
         {
         uint32_t pID = p->getID();
         ListIterator<TR_CISCNode> ti(_P2T + pID);
         for (t = ti.getFirst(); t; t = ti.getNext()) // for each target node corresponding to p
            {
            analyzeConnectionOnePair(p, t);
            }
         }

      if (!specialTransformer ||
          !specialTransformer(this)) break;
      if (++count > 10) break;
      }

   showT2P();
   }


//*****************************************************************************
// Analyze whether each candidate of array header constant is appropriate compared to the idiom.
// Because the array header size is sometimes modified by constant folding
// e.g. When AH is -24 for a[i], AH is modified to -25 for a[i+1]
// If the analysis fails, it'll invalidate that node.
//*****************************************************************************
void
TR_CISCTransformer::analyzeArrayHeaderConst()
   {
   int32_t i = 0;
   while(true) // check all TR_ahconst
      {
      TR_CISCNode *p = _P->getCISCNode(TR_ahconst, true, i);
      if (!p) break; // no more TR_ahconst

      TR_ASSERT(p->getOpcode() == TR_ahconst, "error");
      int32_t pid = p->getID();
      ListIterator<TR_CISCNode> p2ti(_P2T + pid);
      TR_CISCNode *t;
      int32_t ahsize = -(int32_t)TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
      uint8_t *const embeddedForCFG = _embeddedForCFG;
      uint8_t *const embeddedForData = _embeddedForData;
      const uint32_t tmpIdx = idx(pid, 0);
      bool modify = false;
      for (t = p2ti.getFirst(); t; t = p2ti.getNext()) // for each target node
         {
         TR_ASSERT(t->isValidOtherInfo(), "error");
         int32_t val = t->getOtherInfo();
         bool invalidate = false;
         if (val != ahsize)
            {
            ListIterator<TR_CISCNode> parentTi(t->getParents());
            TR_CISCNode *parent;
            for (parent = parentTi.getFirst(); parent; parent = parentTi.getNext())
               {
               // Condition 1:
               // sub           - parent
               //   load        - loadNode
               //     variable  - variableNode
               //   ahconst     - t
               if (!parent->getIlOpCode().isSub())
                  {
                  invalidate = true;
                  break;
                  }
               else
                  {
                  invalidate = true;
                  TR_CISCNode *loadNode;
                  TR_CISCNode *variableNode;
                  TR_CISCNode *subNode = 0;
                  TR_CISCNode *constNode = 0;
                  TR_CISCNode *storeNode = 0;
                  TR_CISCNode *i2lNode = 0;

                  loadNode = parent->getChild(0);
                  if (loadNode->getOpcode() == TR::i2l)
                     {
                     i2lNode = loadNode;
                     loadNode = loadNode->getChild(0);
                     }
                  if (loadNode->getOpcode() == TR_variable)
                     {
                     // Fail. Not implemeted yet.
                     }
                  else
                     {
                     // OK, Condition 1 is satisfied

                     // Next, check Condition 2
                     // istore v
                     //   isub
                     //     iload v
                     //     iconst b
                     variableNode = loadNode->getChild(0);
                     ListIterator<TR_CISCNode> loadPi(loadNode->getParents());
                     bool found = false;
                     for (subNode = loadPi.getFirst(); subNode; subNode = loadPi.getNext())
                        {
                        if (parent != subNode && subNode->getIlOpCode().isSub())
                           {
                           constNode = subNode->getChild(1);
                           if (constNode->isValidOtherInfo() &&
                               constNode->getIlOpCode().isLoadConst() &&
                               constNode->getOtherInfo() + ahsize == val)
                              {
                              ListIterator<TR_CISCNode> subPi(subNode->getParents());
                              for (storeNode = subPi.getFirst(); storeNode; storeNode = subPi.getNext())
                                 {
                                 if (storeNode->getChild(1) == variableNode)
                                    {
                                    // Condition 2 is satisfied
                                    invalidate = false;
                                    found = true;
                                    break;
                                    ////goto find;
                                    }
                                 }
                              if (found)
                                 break;
                              }
                           }
                        }
                     }
                  ////find:;

                  if (invalidate) break;
                  else
                     {
                     TR_CISCNode *newConstNode = _T->getCISCNode(t->getOpcode(), true, ahsize);
                     if (newConstNode)
                        {
                        if (i2lNode)
                           {
                           parent->replaceChild(0, i2lNode);
                           i2lNode->replaceChild(0, variableNode);
                           i2lNode->setCISCNodeModified();
                           }
                        else
                           {
                           parent->replaceChild(0, variableNode);
                           }
                        parent->replaceChild(1, newConstNode);
                        parent->setCISCNodeModified();
                        const int32_t index = tmpIdx + newConstNode->getID();
                        embeddedForCFG[index] = _Embed;
                        embeddedForData[index] = _Embed;
                        modify = true;
                        }
                     }
                  }
               }
            }

         if (invalidate)
            {
            const int32_t tid = t->getID();
            const int32_t index = tmpIdx + tid;
            if (trace())
               {
               traceMsg(comp(), "tid:%d (pid:%d) is invalidated because of failure of analyzeArrayHeaderConst\n",
                      tid,pid);
               }
            embeddedForCFG[index] = _NotEmbed;
            embeddedForData[index] = _NotEmbed;
            }
         }
      if (modify && trace())
         {
         _T->dump(comp()->getOutFile(), comp());
         }
      i++;
      }
   }


void
TR_CISCTransformer::showCISCNodeRegion(TR_CISCNodeRegion *r, TR::Compilation * comp)
   {
   ListIterator<TR_CISCNode> ni;
   TR_CISCNode *n;

   if (r->isIncludeEssentialNode()) traceMsg(comp, "(E) ");
   ni.set((ListHeadAndTail<TR_CISCNode>*)r);
   for (n = ni.getFirst(); n; n = ni.getNext())
      {
      traceMsg(comp, "%d->",n->getID());
      }
   traceMsg(comp, "\n");
   }


void
TR_CISCTransformer::showCISCNodeRegions(List<TR_CISCNodeRegion> *regions, TR::Compilation * comp)
   {
   ListIterator<TR_CISCNodeRegion> ri(regions);
   TR_CISCNodeRegion *r;

   for (r = ri.getFirst(); r; r = ri.getNext())
      {
      showCISCNodeRegion(r, comp);
      }
   }



//*****************************************************************************
// It removes first several nodes from the region r to correct the alignment
//*****************************************************************************
bool
TR_CISCTransformer::alignTopOfRegion(TR_CISCNodeRegion *r)
   {
   ListElement<TR_CISCNode> *le;
   ListElement<TR_CISCNode> *firstNegligible = 0;
   TR_CISCNode *pTop = _P->getEntryNode()->getSucc(0);
   TR_CISCNode *t;

   // determine the top node pTop of the idiom (skip optional nodes)
   while (true)
      {
      t = getP2TRep(pTop);
      if (t == 0)
         {
         if (!pTop->isOptionalNode())
            {
            if (trace()) traceMsg(comp(), "alignTopOfRegion failed. There is no target node corresponding to %d.  Check for nodes in broken region listings above and x in SPBC listing.\n",pTop->getID());
            return false;
            }
         }
      else
         {
         if (!pTop->isOptionalNode() || r->isIncluded(t)) break;
         ListIterator<TR_CISCNode> ci(_P2T + pTop->getID());
         for (t = ci.getFirst(); t; t = ci.getNext())
            {
            if (r->isIncluded(t)) break;
            }
         if (t) break;
         }
      pTop = pTop->getSucc(0);
      }

   if (trace()) traceMsg(comp(), "alignTopOfRegion: (pTop, t) is (%d, %d)\n", pTop->getID(), t->getID());

   // remove nodes (from start to pTop) from the region r
   for (le = r->getListHead(); le; le = le->getNextElement())
      {
      t = le->getData();
      ListIterator<TR_CISCNode> ci(_T2P + t->getID());
      TR_CISCNode *p;
      bool pHasCFG = false;
      for (p = ci.getFirst(); p; p = ci.getNext())
         {
         if (p == pTop)
            {
            r->setListHead(firstNegligible ? firstNegligible : le);
            return true;
            }
         if (p->getNumSuccs() > 0 || !p->getPreds()->isEmpty())
            pHasCFG = true;
         }
      if (t->isNegligible() || !pHasCFG)
         {
         if (!firstNegligible && t->getOpcode() != TR::BBEnd) firstNegligible = le;
         }
      else
         {
         firstNegligible = 0;
         }
      }
   if (trace()) traceMsg(comp(), "alignTopOfRegion failed. Cannot find pTop:%d in the region.\n",pTop->getID());
   return false;
   }


//*****************************************************************************
// Check whether all nodes in the idiom _P are included in the region r.
// It uses a bit vector to analyze above.
//*****************************************************************************
bool
TR_CISCTransformer::areAllNodesIncluded(TR_CISCNodeRegion *r)
   {
   ListIterator<TR_CISCNode> ni;
   TR_CISCNode *t;
   TR_BitVector bv(_P->getNumNodes(), trMemory(), stackAlloc);
   ni.set(_P->getNodes());
   // Set the IDs of required nodes in the idiom _P to the bit vector bv.
   for (t = ni.getFirst(); t; t = ni.getNext())
      {
      if ((t->getNumSuccs() > 0 || !t->getPreds()->isEmpty()) && !t->isOptionalNode())
         {
         switch(t->getOpcode())
            {
            case TR_entrynode:
            case TR_exitnode:
               break;
            default:
               bv.set(t->getID());
               break;
            }
         }
      }

   // Reset the ID of the idiom nodes corresponding to each target node in the region r.
   ni.set((ListHeadAndTail<TR_CISCNode>*)r);
   for (t = ni.getFirst(); t; t = ni.getNext())
      {
      ListIterator<TR_CISCNode> ci(_T2P + t->getID());
      TR_CISCNode *p;
      for (p = ci.getFirst(); p; p = ci.getNext())
         bv.reset(p->getID());
      }

   // If the bit vector bv is empty, alll nodes are included in the region r.
   if (trace())
      {
      if (!bv.isEmpty())
         {
         traceMsg(comp(), "Cannot find pNodes: ");
         bv.print(comp(), comp()->getOutFile());
         traceMsg(comp(), "\n");
         }
      }
   return bv.isEmpty();
   }


//*****************************************************************************
// If moveTo is 0, the region ("from" through "to") in the list l will be moved to the last.
// Otherwise, the region will be moved to before moveTo.
//*****************************************************************************
void
TR_CISCTransformer::moveCISCNodesInList(List<TR_CISCNode> *l, TR_CISCNode *from, TR_CISCNode *to, TR_CISCNode *moveTo)
   {
#if 0
   if (showMesssagesStdout())
      {
      printf("moveCISCNodesInList: %s\n",_T->getTitle());
      }
#endif
   if (trace())
      {
      traceMsg(comp(), "moveCISCNodesInList: r_from:%p(%d) r_to:%p(%d) moveTo:%p(%d)\n",from,from->getID(),to,to->getID(),moveTo,moveTo->getID());
      }

   ListElement<TR_CISCNode> *before = 0, *beforeFrom = 0, *beforeMoveTo = 0, *fromLe = 0, *toLe = 0, *moveToLe = 0, *le;
   for (le = l->getListHead(); le; le = le->getNextElement())
      {
      TR_CISCNode *n = le->getData();
      if (n == from)
         {
         beforeFrom = before;
         fromLe = le;
         }
      if (n == to)
         {
         TR_ASSERT(fromLe != 0, "error! fromLe must be found first.");
         toLe = le;
         }
      if (n == moveTo)
         {
         beforeMoveTo = before;
         moveToLe = le;
         }
      before = le;
      }
   if (moveTo == 0)
      {
      beforeMoveTo = before;
      }
   else
      {
      TR_ASSERT(moveToLe != 0, "error");
      if (moveToLe == 0) return;  // the case if the assertion failed
      }
   TR_ASSERT(fromLe != 0 && toLe != 0, "error");
   if (fromLe == 0 || toLe == 0) return;  // the case if the assertion failed
   if (toLe == beforeMoveTo) return;    // Already moved

   if (!beforeFrom)
      l->setListHead(toLe->getNextElement());
   else
      beforeFrom->setNextElement(toLe->getNextElement());

   toLe->setNextElement(moveToLe);

   if (!beforeMoveTo)
      l->setListHead(fromLe);
   else
      beforeMoveTo->setNextElement(fromLe);
   }


//*****************************************************************************
// If moveTo is 0, the region ("from" through "to") will be moved to the last
// of the dagId2Nodes[from->getDagID()].
// Otherwise, the region will be moved to before moveTo.
// It assumes that all three nodes "from", "to", and "moveTo" (if non-null)
// have the same dagId.
// It maintains the following lists:
// * _T->_dagId2Nodes[from->getDagID()]
// * _T->_nodes
// * _T->_orderByData
//*****************************************************************************
void
TR_CISCTransformer::moveCISCNodes(TR_CISCNode *from, TR_CISCNode *to, TR_CISCNode *moveTo, char *debugStr)
   {
   if (showMesssagesStdout())
      {
      printf("moveCISCNodes: %s %s\n",_T->getTitle(), debugStr ? debugStr : "");
      }

   int32_t dagId = from->getDagID();
   TR_ASSERT(dagId == to->getDagID(), "from->getDagID() and to->getDagID() must be same!");
   TR_ASSERT(!moveTo || dagId == moveTo->getDagID(), "from->getDagID() and moveTo->getDagID() must be same!");
   List<TR_CISCNode> *dagList = _T->getDagId2Nodes()+dagId;

   TR_CISCNode *prevOrg, *nextOrg;
   TR_CISCNode *prevDst, *nextDst, *succDst;

   TR_ASSERT(from->getPreds()->isSingleton(), "assumption error!");
   prevOrg = from->getHeadOfPredecessors();
   nextOrg = to->getSucc(0);

   ListElement<TR_CISCNode> *beforeLastDagIdElement = 0;
   ListElement<TR_CISCNode> *lastDagIdElement = dagList->getListHead();
   if (!moveTo)
      {
      while(lastDagIdElement->getNextElement())
         {
         beforeLastDagIdElement = lastDagIdElement;
         lastDagIdElement = lastDagIdElement->getNextElement();
         }
      prevDst = lastDagIdElement->getData();
      if (prevDst->getOpcode() == TR::BBEnd)
         {
         moveTo = nextDst = prevDst;
         TR_ASSERT(beforeLastDagIdElement, "error!");
         prevDst = beforeLastDagIdElement->getData();
         }
      else
         {
         nextDst = prevDst->getSucc(0);
         }
      }
   else
      {
      while(lastDagIdElement)
         {
         if (lastDagIdElement->getData() == moveTo) break;
         beforeLastDagIdElement = lastDagIdElement;
         lastDagIdElement = lastDagIdElement->getNextElement();
         }
      nextDst = moveTo;
      TR_ASSERT(beforeLastDagIdElement, "error!");
      prevDst = beforeLastDagIdElement->getData();
      }
   succDst = prevDst->getSucc(0);

   // Modify the successor of each node
   prevOrg->replaceSucc(0, nextOrg);
   prevDst->replaceSucc(0, from);
   to->replaceSucc(0, succDst);

   // Modify three lists
   if (to->getNumChildren() != 0 || !to->getParents()->isEmpty())       // if "to" has a child or a parent.
      {
      TR_CISCNode *fromData = from, *nextDstData = nextDst;
      while(fromData->getNumChildren() == 0 && fromData->getParents()->isEmpty()) fromData = fromData->getSucc(0);
      while(nextDstData->getNumChildren() == 0 && nextDstData->getParents()->isEmpty() && nextDstData->getOpcode() != TR_exitnode) nextDstData = nextDstData->getSucc(0);
      moveCISCNodesInList(_T->getOrderByData(), fromData, to, nextDstData);
      }

   moveCISCNodesInList(dagList, from, to, moveTo);
   moveCISCNodesInList(_T->getNodes(), to, from, prevDst);      // Note: _nodes is the reverse post order.
   }


//*****************************************************************************
// Based on four relationships (parents, children, predecessors, successors),
// we extract the region that matches the idiom graph.
// Return the region in which all nodes in the idiom graph are included
//*****************************************************************************
TR_CISCNodeRegion *
TR_CISCTransformer::extractMatchingRegion()
   {
   TR_CISCNodeRegion *lists = new (trHeapMemory()) TR_CISCNodeRegion(_numTNodes, comp()->trMemory()->heapMemoryRegion());
   TR_ScratchList<TR_CISCNodeRegion> regions(comp()->trMemory());
   TR_CISCNode *t;
   ListElement<TR_CISCNode> *firstNegligible = 0;
   int32_t dagT, numDagIdsT = _T->getEntryNode()->getDagID() + 1;
   List<TR_CISCNode> *dagId2NodesT = _T->getDagId2Nodes();
   bool empty = true;

   bool isSingleLoopBody = _bblistBody.isSingleton();

   // Collect regions in which data dependence of nodes are equivalent to the idiom and
   // there is no additional node.
   for (dagT = numDagIdsT; --dagT >= 0;)        // From entry to exit
      {
      List<TR_CISCNode> *TList = dagId2NodesT + dagT;
      ListElement<TR_CISCNode> *element;
      for (element = TList->getListHead(); element; element = element->getNextElement())
         {
         t = element->getData();
         uint32_t tID = t->getID();
         bool isEmbed = false;
         if (!_T2P[tID].isEmpty() && (t->isDataConnected() || t->isNegligible()))
            {
            // TODO: We may need more checks !!!
            isEmbed = true;
            if (t->getIlOpCode().isIf() && !t->isOutsideOfLoop())
               if (!t->isPredSimplyConnected() && !isSingleLoopBody)
                  {
                  isEmbed = false;
                  if (showMesssagesStdout())
                     {
                     printf("!!!!!!!!!!!!!! Predecessor of tID %d is different from that of idiom.\n",tID);
                     }
                  if (trace())
                     {
                     traceMsg(comp(), "Predecessor of tID %d is different from that of idiom.\n",tID);
                     }
                  }
            }
         if (isEmbed)
            {
            // The node t is an appropriate node!
            if (empty && firstNegligible)
               {
               // Add all of the negligible nodes before the node corresponding to a pattern node.
               ListElement<TR_CISCNode> *tmp_ele;
               TR_CISCNode *neg;
               for (tmp_ele = firstNegligible; true; tmp_ele = tmp_ele->getNextElement())
                  {
                  neg = tmp_ele->getData();
                  if (!neg->isNegligible() || !_T2P[neg->getID()].isEmpty()) break;
                  lists->append(neg);
                  }
               TR_ASSERT(neg == t, "error!");
               }
            empty = false;
            lists->append(t);
            }
         else
            {
            // The node t is an inappropriate node!
            if (!empty)
               {
               if (!t->isNegligible() || !_T2P[t->getID()].isEmpty())
                  {
                  // add "lists" to "regions" and clear it
                  empty = true;
                  firstNegligible = 0;
                  regions.add(lists);
                  lists = new (trHeapMemory()) TR_CISCNodeRegion(_numTNodes, comp()->trMemory()->heapMemoryRegion());
                  }
               else
                  {
                  // It can be added
                  lists->append(t);
                  }
               }
            else
               {
               if (t->isNegligible() && _T2P[t->getID()].isEmpty())
                  {
                  // Register the first negligible node after the inappropriate node.
                  if (!firstNegligible) firstNegligible = element;
                  }
               else
                  {
                  // Clear the first negligible node
                  firstNegligible = 0;
                  }
               }
            }
         }
      firstNegligible = 0;
      }
   if (!empty)
      {
      regions.add(lists);
      }
   if (trace())
      {
      traceMsg(comp(), "Before alignTopOfRegion\n");
      showCISCNodeRegions(&regions, comp());
      }

   ListIterator<TR_CISCNodeRegion> ri(&regions);
   ListIterator<TR_CISCNode> ni;
   TR_CISCNodeRegion *r, *ret = 0;
   const bool showingCandidates = isShowingCandidates();
   for (r = ri.getFirst(); r; r = ri.getNext())
      {
      if (r->isIncludeEssentialNode())
         {
         if (showingCandidates) _candidatesForRegister.add(r->clone());
         if (alignTopOfRegion(r)) // Remove nodes from the region r to correct the alignment
            {
            if (areAllNodesIncluded(r)) // If all idiom nodes are included in r
               {
               ret = r;
               break;
               }
            }
         }
      }
   if (trace())
      {
      traceMsg(comp(), "After alignTopOfRegion\n");
      showCISCNodeRegions(&regions, comp());
      traceMsg(comp(), "extractMatchingRegion ret=0x%x\n",ret);
      }

   return ret;
   }


//*****************************************************************************
// It analyzes whether all blocks of the loop body are included in the _candidateRegion
// It also creates _candidateBBStartEnd, which has all of TR::BBStart and TR::BBEnd nodes in the region.
//*****************************************************************************
bool
TR_CISCTransformer::verifyCandidate()
   {
   ListIterator<TR_CISCNode> ci(_candidateRegion);
   TR_CISCNode *cn;
   ListHeadAndTail<TR_CISCNode> *listBB = new (trHeapMemory()) ListHeadAndTail<TR_CISCNode>(trMemory());
   ListElement <TR_CISCNode> *le;

   // Create the list of TR::BBStart and TR::BBEnd nodes in the region.
   for (cn = ci.getFirst(); cn; cn = ci.getNext())
      {
      switch(cn->getOpcode())
         {
         case TR::BBStart:
         case TR::BBEnd:
            listBB->append(cn);
            break;
         }
      }

   le = listBB->getListHead();
   ListIterator<TR::Block> bi(&_bblistBody);
   TR::Block *b;
   for (b = bi.getFirst(); b; b = bi.getNext())
      {
      while (true)
         {
         if (!le)
            {
            if (trace()) traceMsg(comp(), "Cannot find TR::BBStart of block_%d in the region\n",b->getNumber());
            return false;         // Cannot find b in listBB
            }
         cn = le->getData();
         if (cn->getOpcode() == TR::BBStart && cn->getHeadOfTrNodeInfo()->_node->getBlock() == b)
            {
            le = le->getNextElement();
            if (!le) return false;      // Cannot find TR::BBEnd
            cn = le->getData();
            if (cn->getOpcode() != TR::BBEnd || cn->getHeadOfTrNodeInfo()->_node->getBlock() != b)
               return false;            // Cannot find TR::BBEnd
            le = le->getNextElement();
            break;
            }
         le = le->getNextElement();
         }
      }

   _candidateBBStartEnd = listBB;
   return true;
   }



//*****************************************************************************************
// Return the number of target nodes corresponding to p
// Count only within a loop if inLoop is true (default is false).
//*****************************************************************************************
int
TR_CISCTransformer::countP2T(TR_CISCNode *p, bool inLoop)
   {
   uint32_t pID = p->getID();
   List<TR_CISCNode> *list = _P2T + pID;
   if (list->isEmpty())
      {
      return 0;
      }
   else
      {
      ListIterator<TR_CISCNode> ni(list);
      TR_CISCNode *t;
      int count = 0;
      if (inLoop)
         {
         for (t = ni.getFirst(); t; t = ni.getNext()) if (!t->isOutsideOfLoop()) count++;
         }
      else
         {
         for (t = ni.getFirst(); t; t = ni.getNext()) count++;
         }
      return count;
      }
   }



//*****************************************************************************************
// Return a representative target node corresponding to p
// 0 for no-existence
//*****************************************************************************************
TR_CISCNode *
TR_CISCTransformer::getP2TRep(TR_CISCNode *p)
   {
   uint32_t pID = p->getID();
   List<TR_CISCNode> *list = _P2T + pID;
   if (list->isEmpty())
      {
      return 0;
      }
   else
      {
      return list->getListHead()->getData();
      }
   }



//*****************************************************************************************
// Return a representative target node *in the cycle* corresponding to p
// 0 for no-existence
//*****************************************************************************************
TR_CISCNode *
TR_CISCTransformer::getP2TRepInLoop(TR_CISCNode *p, TR_CISCNode *exclude)
   {
   uint32_t pID = p->getID();
   List<TR_CISCNode> *list = _P2T + pID;
   if (list->isEmpty())
      {
      return 0;
      }
   else
      {
      ListIterator<TR_CISCNode> ni(list);
      TR_CISCNode *t;
      for (t = ni.getFirst(); t; t = ni.getNext())
         {
         if (!t->isOutsideOfLoop() && t != exclude) return t;
         }
      return 0;
      }
   }



//*****************************************************************************************
// Return a target node *in the cycle* corresponding to p if the target node is only one.
// 0 for others
//*****************************************************************************************
TR_CISCNode *
TR_CISCTransformer::getP2TInLoopIfSingle(TR_CISCNode *p)
   {
   uint32_t pID = p->getID();
   List<TR_CISCNode> *list = _P2T + pID;
   if (list->isEmpty())
      {
      return 0;
      }
   else
      {
      ListIterator<TR_CISCNode> ni(list);
      TR_CISCNode *t;
      TR_CISCNode *ret = 0;
      for (t = ni.getFirst(); t; t = ni.getNext())
         {
         if (!t->isOutsideOfLoop())
            {
            if (ret) return 0;
            ret = t;
            }
         }
      return ret;
      }
   }



//*****************************************************************************************
// It is similar to getP2TInLoopIfSingle.
// If the given pattern node is "optional", we can skip it.
//*****************************************************************************************
TR_CISCNode *
TR_CISCTransformer::getP2TInLoopAllowOptionalIfSingle(TR_CISCNode *p)
   {
   TR_CISCNode *t;
   while(true)
      {
      if (t = getP2TInLoopIfSingle(p)) return t;
      if (!p->isOptionalNode()) return 0;
      p = p->getChild(0);
      if (!p) return 0;
      }
   }


//*****************************************************************************************
// Return TR::TreeTop, TR::Node, and TR::Block corresponding to the first node of the region _candidateRegion
//*****************************************************************************************
bool
TR_CISCTransformer::findFirstNode(TR::TreeTop **retTree, TR::Node **retNode, TR::Block **retBlock)
   {
   ListIterator<TR_CISCNode> ci(_candidateRegion);
   TR_CISCNode *cn        = NULL;
   TR::Node    *trNode    = NULL;
   TR::TreeTop *trTreeTop = NULL;
   TR::Block   *block     = NULL;

   for (cn = ci.getFirst(); cn; cn = ci.getNext())
      {
      if (cn->getOpcode() == TR_entrynode) continue;
      if (cn->isNewCISCNode()) continue;
      if (trace() && !cn->getTrNodeInfo()->isSingleton())
         traceMsg(comp(), "!cn->getTrNodeInfo()->isSingleton(): %d\n",cn->getID());
      TR_ASSERT(cn->getTrNodeInfo()->isSingleton(), "it must correspond to a single TR node");
      struct TrNodeInfo *info = cn->getHeadOfTrNodeInfo();
      trNode = info->_node;
      if (trNode->getOpCodeValue() == TR::BBEnd) continue;
      if(cn->getOpcode() == TR::BBStart)
         {
         block = trNode->getBlock();

         trTreeTop = info->_treeTop;
         trTreeTop = trTreeTop->getNextTreeTop();
         trNode = trTreeTop->getNode();
         if (trNode->getOpCodeValue() != TR::BBEnd)
            break;
         }
      else
         {
         trTreeTop = info->_treeTop;
         if (trTreeTop->getNode() == trNode)
            {
            if (!block)
               {
               cn = _candidateBBStartEnd->getHeadData();
               if(cn->getOpcode() == TR::BBEnd)
                  {
                  block = cn->getHeadOfTrNodeInfo()->_node->getBlock();
                  }
               }
            break;
            }
         }
      }

   TR_ASSERT(trNode->getOpCodeValue() != TR::BBEnd, "Assumption failed!");
   *retTree = trTreeTop;
   *retNode = trNode;
   *retBlock = block;
   if (trace()) traceMsg(comp(), "First node in candidate region - node: %p block_%d: %p\n",trNode, block->getNumber(), block);
   return true;
   }


//*****************************************************************************************
// It adds the edge from srcBlock to the destBlock, only if succList does not have it.
//*****************************************************************************************
void
TR_CISCTransformer::addEdge(TR::CFGEdgeList *succList, TR::Block *srcBlock, TR::Block *destBlock)
   {
   for (auto edge = succList->begin(); edge != succList->end(); ++edge)
      {
      TR::Block * dest = toBlock((*edge)->getTo());
      TR::Block * src = toBlock((*edge)->getFrom());
      if (src == srcBlock && dest == destBlock)
         {
         return; // already exists!
         }
      }
   _cfg->addEdge(TR::CFGEdge::createEdge(srcBlock,  destBlock, trMemory()));
   return;
   }


//*****************************************************************************************
// It removes the edge (from srcBlock to the destBlock) from the CFG.
//*****************************************************************************************
void
TR_CISCTransformer::removeEdge(List<TR::CFGEdge> *succList, TR::Block *srcBlock, TR::Block *destBlock)
   {
   ListIterator<TR::CFGEdge> succIt(succList);
   TR::CFGEdge * edge;

   for (edge = succIt.getCurrent(); edge != 0; edge = succIt.getNext())
      {
      TR::Block * dest = toBlock(edge->getTo());
      TR::Block * src = toBlock(edge->getFrom());
      if (src == srcBlock && dest == destBlock)
         {
         _cfg->removeEdge(edge);
         }
      }
   return;
   }


//*****************************************************************************************
// It removes the edges (from srcBlock to all blocks except for exceptDestBlock) from the CFG.
//*****************************************************************************************
void
TR_CISCTransformer::removeEdgesExceptFor(TR::CFGEdgeList *succList, TR::Block *srcBlock, TR::Block *exceptDestBlock)
   {
   for (auto edge = succList->begin(); edge != succList->end();)
      {
      TR::Block * dest = toBlock((*edge)->getTo());
      TR::Block * src = toBlock((*edge)->getFrom());
      if (src == srcBlock && dest != exceptDestBlock)
         {
         _cfg->removeEdge(*(edge++));
         }
      else
    	  ++edge;
      }
   return;
   }


//*****************************************************************************************
// Set the edge from srcBlock to destBlock into the CFG.
//*****************************************************************************************
void
TR_CISCTransformer::setEdge(TR::CFGEdgeList *succList, TR::Block *srcBlock, TR::Block *destBlock)
   {
   addEdge(succList, srcBlock, destBlock);
   removeEdgesExceptFor(succList, srcBlock, destBlock);
   }


//*****************************************************************************************
// Set two edges from srcBlock to destBlock0 and destBlock1 into the CFG.
//*****************************************************************************************
void
TR_CISCTransformer::setEdges(TR::CFGEdgeList *succList, TR::Block *srcBlock, TR::Block *destBlock0, TR::Block *destBlock1)
   {
   bool existEdge0, existEdge1;
   existEdge0 = existEdge1 = false;
   int32_t count0, count1;

   for (auto edge = succList->begin(); edge != succList->end(); ++edge)
      {
      TR::Block * dest = toBlock((*edge)->getTo());
      TR::Block * src = toBlock((*edge)->getFrom());
      if (src == srcBlock)
         {
         if (dest == destBlock0) existEdge0 = true;
         else if (dest == destBlock1) existEdge1 = true;
         }
      }

   if (!existEdge1) addEdge(succList, srcBlock, destBlock1);
   if (!existEdge0) addEdge(succList, srcBlock, destBlock0);

   count0 = count1 = 0;
   for (auto edge = succList->begin(); edge != succList->end();)
      {
      TR::Block * dest = toBlock((*edge)->getTo());
      TR::Block * src = toBlock((*edge)->getFrom());
      if (src == srcBlock)
         {
         if (dest == destBlock0)
            {
            if (++count0 >= 2) _cfg->removeEdge(*(edge++));
            else ++edge;
            }
         else if (dest == destBlock1)
            {
            if (++count1 >= 2) _cfg->removeEdge(*(edge++));
            else ++edge;
            }
         else
            {
            _cfg->removeEdge(*(edge++));
            }
         }
      else
    	  ++edge;
      }
   }


//*****************************************************************************
// It analyzes whether the successor of the target loop is single.
// Even if there are multiple successors for the loop, it tries to analyze
// whether they can be merged to a single successor.
// It returns:
//  * non-null: the single successor block
//  * null: multiple successors
//*****************************************************************************
TR::Block *
TR_CISCTransformer::analyzeSuccessorBlock(TR::Node *ignoreTree)
   {
   TR::Block *target = 0;
   if (_bblistSucc.isSingleton())
      {
      target = _bblistSucc.getListHead()->getData();    // obvious
      }
   else
      {
      ListIterator<TR::Block> bbi1(&_bblistSucc), bbi2(&_bblistSucc);
      TR::Block *b1, *b2;

      // Analyze successors will be merged to a single block without any additional instruction
      for (b1 = bbi1.getFirst(); b1; b1 = bbi1.getNext())
         {
         target = 0;
         for (b2 = bbi2.getFirst(); b2; b2 = bbi2.getNext())
            {
            if (b1 != b2)
               {
               TR::Node *gotonode = b2->getFirstRealTreeTop()->getNode();
               if (gotonode->getOpCodeValue() == TR::Goto &&
                   gotonode->getBranchDestination()->getNode()->getBlock() == b1)
                  {
                  target = b1;
                  }
               else if (gotonode->getOpCodeValue() == TR::BBEnd &&
                        b2->getNextBlock() == b1)
                  {
                  target = b2;
                  }
               else
                  {
                  target = 0;
                  break;
                  }
               }
            }
         if (target) break;
         }
      if (!target)
         {
         for (b1 = bbi1.getFirst(); b1; b1 = bbi1.getNext())
            {
            TR::Block *gotoTarget = skipGoto(b1, ignoreTree);
            if (!target)
               {
               target = gotoTarget;
               }
            else
               {
               if (target != gotoTarget)
                  {
                  target = 0;
                  break;
                  }
               }
            }
         }

      // Especially for two successors, I'll analyze more deeply.
   /*
      if (!target && _bblistSucc.isDoubleton())
         {
         b1 = bbi1.getFirst();
         b2 = bbi1.getNext();
         TR::Block *cur1 = b1, *cur2 = b2;
         while(true)
            {
            cur1 = skipGoto(cur1, ignoreTree);
            cur2 = skipGoto(cur2, ignoreTree);
            if (cur1 == cur2)
               {
               target = b1;
               break;
               }
            if (!compareBlockTrNodeTree(cur1, cur2)) break;
            if (!(cur1 = getSuccBlockIfSingle(cur1))) break;
            if (!(cur2 = getSuccBlockIfSingle(cur2))) break;
            }
         }
   */
      }

   if (trace())
      {
      if (target == 0)
         traceMsg(comp(), "!! TR_CISCTransformer::analyzeSuccessorBlock returns 0!\n");
      }

   return target;
   }


//*****************************************************************************************
// It sets one successor "target" to the block.
//*****************************************************************************************
void
TR_CISCTransformer::setSuccessorEdge(TR::Block *block, TR::Block *target)
   {
   if (target == 0)
      target = analyzeSuccessorBlock();

   TR_ASSERT(target != 0, "target must be non-null!!");

   TR::Node *gotonode = block->getLastRealTreeTop()->getNode();
   if (gotonode->getOpCodeValue() != TR::Goto)
      {
      TR::TreeTop * branchAroundTreeTop = TR::TreeTop::create(comp(), TR::Node::create(gotonode, TR::Goto, 0, target->getEntry()));
      block->getLastRealTreeTop()->join(branchAroundTreeTop);
      branchAroundTreeTop->join(block->getExit());
      }
   setEdge(&block->getSuccessors(), block, target);
   }


//*****************************************************************************************
// Search for the TR::Block in _bblistSucc except for target0 and target1
//*****************************************************************************************
TR::Block *
TR_CISCTransformer::searchOtherBlockInSuccBlocks(TR::Block *target0, TR::Block *target1)
   {
   ListIterator<TR::Block> bbi1(&_bblistSucc);
   TR::Block *b;
   TR::Block *ret = 0;
   for (b = bbi1.getFirst(); b; b = bbi1.getNext())
      {
      if (b == target0 || b == target1)
         continue;
      if (ret)
         return 0;   // Failure - Found two non-target0/target1 blocks.
      ret = b;
      }
   return ret;
   }


//*****************************************************************************************
// Search for the TR::Block in _bblistSucc except for target0
//*****************************************************************************************
TR::Block *
TR_CISCTransformer::searchOtherBlockInSuccBlocks(TR::Block *target0)
   {
   if (_bblistSucc.isDoubleton())
      {
      ListIterator<TR::Block> bbi1(&_bblistSucc);
      TR::Block *first = bbi1.getFirst();
      TR::Block *second = bbi1.getNext();
      if (first == target0)
         return second;
      else if (second == target0)
         return first;
      }
   return 0;
   }


//*****************************************************************************************
// It sets two successors "target0" and "target1" to the block.
//*****************************************************************************************
TR::Block *
TR_CISCTransformer::setSuccessorEdges(TR::Block *block, TR::Block *target0, TR::Block *target1)
   {
   TR::TreeTop * oldNext = block->getExit()->getNextTreeTop();
   if (target0 == 0 || target1 == 0)      // automatic detection
      {
      if (target0 == 0)
         target0 = searchOtherBlockInSuccBlocks(target1);
      else
         target1 = searchOtherBlockInSuccBlocks(target0);
      TR_ASSERT(target0 && target1, "error");
      }
   if (trace())
      {
      traceMsg(comp(), "setSuccessorEdges for block_%d [%p]: tgt0=%d tgt1=%d\n", block->getNumber(), block, target0->getNumber(),target1->getNumber());
      }

   if (!oldNext ||
         oldNext->getNode()->getBlock() != target0)
      {
      TR::Node *lastnode = block->getLastRealTreeTop()->getNode();
      TR::Block * gotoBlock = TR::Block::createEmptyBlock(lastnode, comp(), block->getFrequency(), block);
      _cfg->addNode(gotoBlock);
      TR::TreeTop * gotoEntry = gotoBlock->getEntry();
      TR::TreeTop * gotoExit = gotoBlock->getExit();
      TR::TreeTop * branchTreeTop = TR::TreeTop::create(comp(), TR::Node::create(lastnode, TR::Goto, 0, target0->getEntry()));
      gotoEntry->insertAfter(branchTreeTop);

      block->getExit()->join(gotoEntry);
      gotoExit->join(oldNext);

      _cfg->setStructure(0);
      _cfg->addEdge(TR::CFGEdge::createEdge(gotoBlock,  target0, trMemory()));
      setEdges(&block->getSuccessors(), block, gotoBlock, target1);
      return gotoBlock;
      }
   else
      {
      setEdges(&block->getSuccessors(), block, target0, target1);
      return block;
      }
   }


//*****************************************************************************************
// It returns:
//  * 0: if successor is not single
//  * non-null: the successor block
//*****************************************************************************************
TR::Block *
TR_CISCTransformer::getSuccBlockIfSingle(TR::Block *block)
   {
   if (!(block->getSuccessors().size() == 1)) return 0;
   TR::CFGEdge *edge = block->getSuccessors().front();
   return toBlock(edge->getTo());
   }



//*****************************************************************************************
// It searches for a combination of predecessors of "block" and _bblistPred.
//*****************************************************************************************
TR::Block *
TR_CISCTransformer::searchPredecessorOfBlock(TR::Block *block)
   {
   for (auto edge = block->getPredecessors().begin(); edge != block->getPredecessors().end(); ++edge)
      {
      TR::Block *from = toBlock((*edge)->getFrom());
      if (_bblistPred.find(from))
         {
         return from;     // find
         }
      }
   return NULL;
   }


//*****************************************************************************************
// It decides whether we generate versioning code and modifies the target blocks.
// It returns the block that fast code will be appended.
//*****************************************************************************************
TR::Block *
TR_CISCTransformer::modifyBlockByVersioningCheck(TR::Block *block, TR::TreeTop *startTop, TR::Node *lengthNode, List<TR::Node> *guardList)
   {
   uint16_t versionLength = _P->getVersionLength();
   List<TR::Node> guardListLocal(trMemory());
   // Create versioning if necessary
   if (versionLength >= 1)      // Skip if versionLength is less than 1.
      {
      if (guardList == 0) guardList = &guardListLocal;
      ListAppender<TR::Node> appender(guardList);
      if (lengthNode->getOpCodeValue() == TR::i2l) lengthNode = lengthNode->getAndDecChild(0);
      if (lengthNode->getOpCode().isLong())
         {
         TR::Node *lconst = TR::Node::create(lengthNode, TR::lconst, 0, 0);
         lconst->setLongInt(versionLength);
         appender.add(TR::Node::createif(TR::iflcmple, lengthNode, lconst));
         }
      else
         {
         TR_ASSERT(lengthNode->getOpCode().isInt(), "Error");
         appender.add(TR::Node::createif(TR::ificmple, lengthNode,
                                     TR::Node::create(lengthNode, TR::iconst, 0, versionLength)));
         }
      }
   return modifyBlockByVersioningCheck(block, startTop, guardList);
   }



//*****************************************************************************************
// It decides whether we generate versioning code and modifies the target blocks.
// It returns the block that fast code will be appended.
//*****************************************************************************************
TR::Block *
TR_CISCTransformer::modifyBlockByVersioningCheck(TR::Block *block, TR::TreeTop *startTop, List<TR::Node> *guardList)
   {
   TR::CFG *cfg = comp()->getFlowGraph();
   TR::Block *fastpath;

   // Create versioning if necessary
   if (guardList && !guardList->isEmpty())
      {
      cfg->setStructure(0);
      fastpath = TR::Block::createEmptyBlock(startTop->getNode(), comp(), block->getFrequency(), block);
      TR::Block *slowpad;
      TR::Node *cmp;
      TR::Block *orgPrevBlock = 0;
      ListIterator<TR::Node> guardI(guardList);
      TR::Block *firstBlock = 0;
      TR::Block *lastBlock = 0;

      // Append versioning check
      // Result: orgPrevBlock->block->fastpath->slowpad

      if (block->getFirstRealTreeTop() == startTop)
         {
         // search the entry pad
         orgPrevBlock = searchPredecessorOfBlock(block);
         }

      // Insert the fastpath + versioning tress in between the previous block and current block, unless:
      //     1.  We do not find a previous block.
      //     2.  The previous block does not fall-through into current block (i.e. reached by taken branch).
      // In these two cases, we will just split the current block, and insert the fastpath + versioning trees
      // in between.
      if (!orgPrevBlock || orgPrevBlock->getNextBlock() != block)
         {
         orgPrevBlock = block;
         slowpad = block->split(startTop, cfg, true);
         }
      else
         {
         slowpad = block;
         }

      TR::TreeTop * orgPrevTreeTop = orgPrevBlock->getExit();
      TR::Node *lastOrgPrevRealNode = orgPrevBlock->getLastRealTreeTop()->getNode();
      TR::TreeTop * orgNextTreeTop = orgPrevTreeTop->getNextTreeTop();
      if (orgNextTreeTop)
         {
         TR::Block * orgNextBlock = orgNextTreeTop->getNode()->getBlock();
         cfg->insertBefore(fastpath, orgNextBlock);
         }
      else
         {
         cfg->addNode(fastpath);
         }

      firstBlock = fastpath;
      for (cmp = guardI.getFirst(); cmp; cmp = guardI.getNext())
         {
         block = TR::Block::createEmptyBlock(startTop->getNode(), comp(), block->getFrequency(), block);
         if (!lastBlock) lastBlock = block;
         TR_ASSERT(cmp->getOpCode().isIf(), "Not implemeted yet");
         cmp->setBranchDestination(slowpad->getEntry());
         block->append(TR::TreeTop::create(comp(), cmp));
         cfg->insertBefore(block, firstBlock);
         firstBlock = block;
         }

      orgPrevTreeTop->join(firstBlock->getEntry());
      cfg->addEdge(orgPrevBlock, firstBlock);
      cfg->removeEdge(orgPrevBlock, slowpad);

      if (trace()) traceMsg(comp(), "modifyBlockByVersioningCheck: orgPrevBlock=%d firstBlock=%d lastBlock=%d fastpath=%d slowpad=%d orgNextTreeTop=%x\n",
                          orgPrevBlock->getNumber(), firstBlock->getNumber(), lastBlock->getNumber(), fastpath->getNumber(), slowpad->getNumber(), orgNextTreeTop);

      if (lastOrgPrevRealNode->getOpCode().getOpCodeValue() == TR::Goto)
         {
         TR_ASSERT(lastOrgPrevRealNode->getBranchDestination() == slowpad->getEntry(), "Error");
         lastOrgPrevRealNode->setBranchDestination(firstBlock->getEntry());
         }
      }
   else
      {
      // Generate no versioning code
      TR::TreeTop *lastRealTT = block->getLastRealTreeTop();
      if (lastRealTT->getNode()->getOpCodeValue() == TR::Goto)
         {
         if (startTop != lastRealTT)
            {
            TR::TreeTop *last = removeAllNodes(startTop, lastRealTT);
            last->join(lastRealTT);
            }
         block->split(lastRealTT, cfg);
         }
      else
         {
         TR::TreeTop *last = removeAllNodes(startTop, block->getExit());
         last->join(block->getExit());
         }
      fastpath = block;
      }
   return fastpath;
   }


//*****************************************************************************************
// It clones the loop body.
//*****************************************************************************************
TR::Block *
TR_CISCTransformer::cloneLoopBodyForPeel(TR::Block **firstBlock, TR::Block **lastBlock, TR_CISCNode *cmpifCISCNode)
   {
   TR::CFG *cfg = comp()->getFlowGraph();
   cfg->setStructure(0);
   TR_BlockCloner cloner(cfg);
   *firstBlock = cloner.cloneBlocks(_bblistBody.getListHead()->getData(),_bblistBody.getLastElement()->getData());
   *lastBlock  = cloner.getLastClonedBlock();
   if (cmpifCISCNode)
      {
      struct TrNodeInfo *repNode = cmpifCISCNode->getHeadOfTrNodeInfo();
      TR::Block *modifyBlock = cloner.getToBlock(repNode->_block);
      TR_ASSERT(modifyBlock != repNode->_block, "error");
      TR::Node *modifyNode = modifyBlock->getLastRealTreeTop()->getNode();
      TR_ASSERT(modifyNode->getOpCode().isIf(), "error");
      TR::Node::recreate(modifyNode, (TR::ILOpCodes)cmpifCISCNode->getOpcode());
      modifyNode->setBranchDestination(cmpifCISCNode->getDestination());
      }
   return *firstBlock;
   }

//*****************************************************************************************
// It appends blocks after "block".
//*****************************************************************************************
TR::Block *
TR_CISCTransformer::appendBlocks(TR::Block *block, TR::Block *firstBlock, TR::Block *lastBlock)
   {
   TR::CFG *cfg = comp()->getFlowGraph();
   cfg->setStructure(0);
   TR::Block *ret;

   TR::TreeTop *orgNextTreeTop = block->getExit()->getNextTreeTop();
   if (orgNextTreeTop)
      {
      TR::Block * orgNextBlock = orgNextTreeTop->getEnclosingBlock();
      ret = TR::Block::createEmptyBlock(block->getExit()->getNode(), comp(), block->getFrequency(), block);
      cfg->insertBefore(ret, orgNextBlock);
      }
   else
      {
      TR_ASSERT(block->getLastRealTreeTop()->getNode()->getOpCode().isBranch(), "error");
      ret = block->split(block->getLastRealTreeTop(), cfg);
      }
   cfg->join(block, firstBlock);
   cfg->join(lastBlock, ret);
   setSuccessorEdge(block, firstBlock);
   return ret;
   }


//*****************************************************************************************
// Check whether the node is a dead store by using useDefInfo
//*****************************************************************************************
bool
TR_CISCTransformer::isDeadStore(TR::Node *node)
   {
   if (node->getOpCode().isStoreDirect())
      {
      if (!node->getSymbol()->isAutoOrParm())
         return false;

      TR_UseDefInfo *useDefInfo = _useDefInfo;
      const int32_t firstUseIndex = useDefInfo->getFirstUseIndex();
      int32_t useDefIndex = node->getUseDefIndex();
      //TR_ASSERT(useDefInfo->isDefIndex(useDefIndex), "error!");
      if (!useDefInfo->isDefIndex(useDefIndex)) return false;
      if (useDefInfo->getUsesFromDefIsZero(useDefIndex)) return true;
      }
   return false;
   }


//*****************************************************************************************
// It basically skips blocks containing only a goto statement.
// It can additionally skip nodes for dead stores and the node specified by "ignoreTree"
//*****************************************************************************************
TR::Block *
TR_CISCTransformer::skipGoto(TR::Block *block, TR::Node *ignoreTree)
   {
   while(true)
      {
      TR::TreeTop *treeTop = block->getFirstRealTreeTop();
      TR::Node *gotonode;
      while(true)
         {
         gotonode = treeTop->getNode();
         if (!isDeadStore(gotonode) &&
             (ignoreTree == 0 || !compareTrNodeTree(gotonode, ignoreTree))) break;
         treeTop = treeTop->getNextRealTreeTop();
         }
      TR::ILOpCodes opcode = gotonode->getOpCodeValue();
      if (opcode == TR::Goto)
         {
         block = gotonode->getBranchDestination()->getNode()->getBlock();
         }
      else if (opcode == TR::BBEnd)
         {
         treeTop = treeTop->getNextRealTreeTop();
         block = treeTop->getNode()->getBlock();
         }
      else
         {
         break;
         }
      }
   return block;
   }


//*****************************************************************************************
// It searches the "target" node in the tree from "top".
// Return values are stored into *retParent and *retChildNum.
//*****************************************************************************************
bool
TR_CISCTransformer::searchNodeInTrees(TR::Node *top, TR::Node *target, TR::Node **retParent, int *retChildNum)
   {
   int i;
   for (i = top->getNumChildren(); --i >= 0; )
      {
      if (compareTrNodeTree(top->getChild(i), target))
         {
         if (retParent) *retParent = top;
         if (retChildNum) *retChildNum = i;
         return true;
         }
      }
   for (i = top->getNumChildren(); --i >= 0; )
      {
      if (searchNodeInTrees(top->getChild(i), target, retParent, retChildNum)) return true;
      }
   return false;
   }


//*****************************************************************************************
// Analyze whether node trees a and b are equivalent.
//*****************************************************************************************
bool
TR_CISCTransformer::compareTrNodeTree(TR::Node *a, TR::Node *b)
   {
   if (a == b) return true;
   if (a->getOpCodeValue() != b->getOpCodeValue()) return false;
   if (a->getOpCode().hasSymbolReference() &&
         (a->getSymbolReference()->getReferenceNumber() != b->getSymbolReference()->getReferenceNumber()))
      return false;

   if (a->getOpCode().isLoadConst())
      {
      switch(a->getOpCodeValue())
         {
         case TR::iuconst:
         case TR::iconst:
            if (a->getUnsignedInt() != b->getUnsignedInt()) return false;
            break;
         case TR::luconst:
         case TR::lconst:
            if (a->getUnsignedLongInt() != b->getUnsignedLongInt()) return false;
            break;
         case TR::aconst:
            if (a->getAddress() != b->getAddress()) return false;
            break;
         case TR::fconst:
            if (a->getFloat() != b->getFloat()) return false;
            break;
         case TR::dconst:
            if (a->getDouble() != b->getDouble()) return false;
            break;
         case TR::bconst:
         case TR::buconst:
            if (a->getUnsignedByte() != b->getUnsignedByte()) return false;
            break;
         case TR::sconst:
            if (a->getShortInt() != b->getShortInt()) return false;
            break;
         case TR::cconst:
            if (a->getConst<uint16_t>() != b->getConst<uint16_t>()) return false;
            break;
         default:
            return false;
         }
      }
   int32_t numChild = a->getNumChildren();
   if (numChild != b->getNumChildren()) return false;
   if (numChild == 2 && a->getOpCode().isCommutative())
      {
      if ((!compareTrNodeTree(a->getChild(0), b->getChild(0)) ||
           !compareTrNodeTree(a->getChild(1), b->getChild(1))) &&
          (!compareTrNodeTree(a->getChild(0), b->getChild(1)) ||
           !compareTrNodeTree(a->getChild(1), b->getChild(0))))
         return false;
      }
   else
      {
      int32_t i;
      for (i = 0; i < numChild; i++)
         {
         if (!compareTrNodeTree(a->getChild(i), b->getChild(i))) return false;
         }
      }
   return true;
   }


//*****************************************************************************************
// Analyze whether node trees in blocks a and b are equivalent.
//*****************************************************************************************
bool
TR_CISCTransformer::compareBlockTrNodeTree(TR::Block *a, TR::Block *b)
   {
   if (a == b) return true;
   TR::TreeTop *ttA = a->getFirstRealTreeTop();
   TR::TreeTop *ttB = b->getFirstRealTreeTop();
   TR::TreeTop *lastA = a->getLastRealTreeTop();
   while(true)
      {
      if (!compareTrNodeTree(ttA->getNode(), ttB->getNode())) return false;
      if (ttA == lastA) break;
      ttA = ttA->getNextRealTreeTop();
      if (ttA->getNode()->getOpCodeValue() == TR::Goto) break;
      ttB = ttB->getNextRealTreeTop();
      if (ttB->getNode()->getOpCodeValue() == TR::Goto) break;
      }
   return true;
   }


//*****************************************************************************************
// Append nodes in the list _beforeInsertions into the block.
// Restriction: It can create a single new block automatically, but it doesn't create multiple ones.
//*****************************************************************************************
TR::Block *
TR_CISCTransformer::insertBeforeNodes(TR::Block *block)
   {
   ListIterator<TR::Node> ni(&_beforeInsertions);
   TR::Node *n, *last = 0;
   int32_t count = 0;
   for (n = ni.getFirst(); n; n = ni.getNext())
      {
      // Insert nodes into given block
      TR::TreeTop *top = TR::TreeTop::create(comp(), n);
      block->getLastRealTreeTop()->join(top);
      top->join(block->getExit());
      last = n;
      count++;
      }
   if (trace()) traceMsg(comp(), "insertBeforeNodes added %d node(s) to block_%d [%p]\n", count, block->getNumber(), block);
   if (last && last->getOpCode().isBranch())
      {
      TR::CFG *cfg = comp()->getFlowGraph();
      TR::TreeTop *orgNext = block->getExit()->getNextTreeTop();
      TR::Block *newBlock = TR::Block::createEmptyBlock(last, comp(), block->getFrequency(), block);
      cfg->setStructure(0);
      cfg->addNode(newBlock);
      newBlock->getExit()->join(orgNext);
      block->getExit()->join(newBlock->getEntry());

      cfg->addSuccessorEdges(newBlock);
      bool isRemove = true;

      TR::Block * orgNextBlock = orgNext->getNode()->getBlock();
      TR::Block * branchDestinationBlock = 0;
      if (last->getOpCode().isIf())
         branchDestinationBlock = last->getBranchDestination()->getEnclosingBlock();
      // Copy edges to avoid removing necessary blocks
      for (auto edge = block->getSuccessors().begin(); edge != block->getSuccessors().end(); ++edge)
         {
         TR::Block *to = toBlock((*edge)->getTo());
         if (to != branchDestinationBlock &&
             to != orgNextBlock)
            {
            if (trace()) traceMsg(comp(), "insertBeforeNodes added the edge (%d, %d).\n",newBlock->getNumber(),to->getNumber());
            addEdge(&newBlock->getSuccessors(), newBlock, to);
            }
         }

      if (last->getOpCode().isIf())
         {
         setSuccessorEdges(block, newBlock, branchDestinationBlock);
         if (orgNext->getNode()->getBlock() == branchDestinationBlock)
            isRemove = false;
         }
      else
         {
         setSuccessorEdge(block, newBlock);
         }

      if (isRemove)
         cfg->removeEdge(block, orgNext->getNode()->getBlock());
      if (trace()) traceMsg(comp(), "insertBeforeNodes created block_%d [%p]\n", newBlock->getNumber(), newBlock);
      block = newBlock;
      }
   return block;
   }

//*****************************************************************************************
// Search for store to a given symref in the insert before list
// @param symRefNumberToBeMatched  The symbol reference number to be matched.
// @return The TR::Node for the store with same symbol reference number.  NULL if not found.
//*****************************************************************************************
TR::Node *
TR_CISCTransformer::findStoreToSymRefInInsertBeforeNodes(int32_t symRefNumberToBeMatched)
   {
   ListIterator<TR::Node> ni(&_beforeInsertions);
   TR::Node *n = NULL;

   for (n = ni.getFirst(); n; n = ni.getNext())
      {
      if (n->getOpCode().isStore() && n->getOpCode().hasSymbolReference() && n->getSymbolReference()->getReferenceNumber() == symRefNumberToBeMatched)
         return n;
      }

   return NULL;
   }

//*****************************************************************************************
// Prepend/Append nodes in the list l into the block.
// Restriction: It creates no new block automatically.
//*****************************************************************************************
TR::Block *
TR_CISCTransformer::insertAfterNodes(TR::Block *block, List<TR::Node> *l, bool prepend)
   {
   ListIterator<TR::Node> ni(l);
   TR::Node *n;
   int32_t count = 0;
   if (prepend)
      {
      TR::TreeTop *last, *orgNext;
      last = block->getEntry();
      orgNext = last->getNextTreeTop();
      for (n = ni.getFirst(); n; n = ni.getNext())
         {
         TR::TreeTop *top = TR::TreeTop::create(comp(), n);
         last->join(top);
         last = top;
         count++;
         }
      last->join(orgNext);
      }
   else
      {
      for (n = ni.getFirst(); n; n = ni.getNext())
         {
         TR::TreeTop *top = TR::TreeTop::create(comp(), n);
         block->append(top);
         count++;
         }
      }
   if (trace()) traceMsg(comp(), "insertAfterNodes adds %d node(s)\n", count);
   return block;
   }

//*****************************************************************************************
// Add nodes for idiom independent transformations
//*****************************************************************************************
TR::Block *
TR_CISCTransformer::insertAfterNodes(TR::Block *block, bool prepend)
   {
   return insertAfterNodes(block, &_afterInsertions, prepend);
   }

// Add nodes for idiom specific transformations
TR::Block *
TR_CISCTransformer::insertAfterNodesIdiom(TR::Block *block, int32_t pos, bool prepend)
   {
   return insertAfterNodes(block, _afterInsertionsIdiom + pos, prepend);
   }


// Remove all nodes from TreeTops from "start" to "end"
TR::TreeTop *
TR_CISCTransformer::removeAllNodes(TR::TreeTop *start, TR::TreeTop *end)
   {
   TR::TreeTop *ret = start->getPrevTreeTop();
#if 0
   for (; start != end; start = start->getNextTreeTop())
      {
      start->getNode()->removeAllChildren();
      start->setNode(0);
      }
#endif
   TR::TreeTop *next;
   for (; start != end; start = next)
      {
      next = start->getNextTreeTop();
      TR::TransformUtil::removeTree(comp(), start);
      if (next == end)
         break;
      }
   return ret;
   }


//*****************************************************************************
// These functions create function tables for TRT or TRxx instructions.
//*****************************************************************************
bool
TR_CISCTransformer::analyzeBoolTable(TR_BitVector **bv, TR::TreeTop **retSameExit, TR_CISCNode *boolTable, TR_BitVector *defBV, TR_CISCNode *defNode, TR_CISCNode *ignoreNode, int32_t bvoffset, int32_t allocBVSize)
   {
   List<TR_CISCNode> *P2T = _P2T;
   List<TR_CISCNode> *T2P = _T2P;
   TR_CISCGraph *P = _P;
   TR_CISCGraph *T = _T;
   ListIterator<TR_CISCNode> ni(_candidateRegion);
   TR::TreeTop *exitTreeTop;
   bool initExitTreeTop;
   TR_CISCNode * exitnode;
   TR_CISCNode *n;
   int32_t i;
   TR_BitVector takenBV(allocBVSize, trMemory(), stackAlloc), ntakenBV(allocBVSize, trMemory(), stackAlloc), tmpBV(allocBVSize, trMemory(), stackAlloc);
   TR_BitVector orgBV(allocBVSize, trMemory(), stackAlloc);

   //
   // Perform a forward dataflow analysis to compute exit conditions of the loop
   //
   for (i = T->getNumNodes(); --i >= 0; )
      bv[i] = new (trStackMemory()) TR_BitVector(allocBVSize, trMemory(), stackAlloc);
   exitnode = T->getExitNode();
   exitTreeTop = 0;
   initExitTreeTop = false;
   int loopCount = 0;
   bool doAgain = true;
   while(doAgain)
      {
      if (loopCount++ > 10)
         {
         TR_ASSERT(false, "analyzeBoolTable: infinite loop!\n");
         return false;
         }
      doAgain = false;
      for (n = ni.getFirst(); n; n = ni.getNext())
         {
         int32_t pos;
         uint32_t tID = n->getID();
         TR_CISCNode *p = getT2PheadRep(tID);
         bool doPropagateSuccs = true;
         if (analyzeT2P(n, defNode) & _T2P_MatchMask)
            {
            p = defNode;
            if (bv[tID]->isEmpty()) *bv[tID] = *defBV;
            }
         else if (p == boolTable)
            {
            if (n->getOpcode() == TR::Case)
               {
               if (!n->isValidOtherInfo())
                  {
                  // default case
                  TR_ASSERT(n->getParents()->isSingleton(), "error!!!");
                  TR_CISCNode *swbody = n->getHeadOfParents();
                  takenBV = *bv[tID];
                  for (i = swbody->getNumChildren(); --i >= 2; )
                     {
                     pos = swbody->getChild(i)->getOtherInfo()+bvoffset;
                     takenBV.reset(pos);
                     }
                  }
               else
                  {
                  pos = n->getOtherInfo()+bvoffset;
                  takenBV.empty();
                  takenBV.set(pos);
                  if (!n->isOutsideOfLoop())
                     {
                     TR::TreeTop *thisDestination = n->getDestination();
                     if (!initExitTreeTop)
                        {
                        initExitTreeTop = true;
                        exitTreeTop = thisDestination;
                        }
                     if (exitTreeTop != thisDestination)
                        {
                        if (trace() && exitTreeTop)
                           {
                           traceMsg(comp(), "Succ(0) is not exit node. ID:%d (TR::Case)\n", n->getID());
                           }
                        exitTreeTop = 0;
                        }
                     }
                  }

               if (doAgain)
                  {
                  *bv[n->getSucc(0)->getID()] |= takenBV;
                  }
               else
                  {
                  int id;
                  id = n->getSucc(0)->getID();
                  orgBV = *bv[id];
                  *bv[id] |= takenBV;
                  if (!(orgBV == *bv[id])) doAgain = true;
                  }

               doPropagateSuccs = false; // already done
               }
            else
               {
               TR_CISCNode *child1 = n->getChild(1);
               while (!child1->isInterestingConstant())
                  {
                  child1 = child1->getNodeIfSingleChain();
                  if (!child1)
                     {
                     if (trace()) traceMsg(comp(), "analyzeBoolTable failed for %p. (no single chain)\n", n->getChild(1));
                     return false;
                     }
                  if (!child1->isStoreDirect())
                     {
                     if (trace()) traceMsg(comp(), "analyzeBoolTable failed for %p. (%p is not store)\n", n->getChild(1), child1);
                     return false;
                     }
                  child1 = child1->getChild(0);
                  }
               pos = child1->getOtherInfo()+bvoffset;

               switch(n->getOpcode())
                  {
                  case TR::ifbcmpeq:
                  case TR::ifsucmpeq:
                  case TR::ificmpeq:
                     takenBV.empty();
                     ntakenBV = *bv[tID];
                     if (bv[tID]->isSet(pos))
                        {
                        takenBV.set(pos);
                        ntakenBV.reset(pos);
                        }
                     break;
                  case TR::ifbcmpne:
                  case TR::ifsucmpne:
                  case TR::ificmpne:
                     takenBV = *bv[tID];
                     ntakenBV.empty();
                     if (bv[tID]->isSet(pos))
                        {
                        takenBV.reset(pos);
                        ntakenBV.set(pos);
                        }
                     break;
                  case TR::ifbcmplt:
                  case TR::ifsucmplt:
                  case TR::ificmplt:
                     takenBV = *bv[tID];
                     ntakenBV = takenBV;
                     tmpBV.empty();
                     tmpBV.setAll(0, pos-1);
                     takenBV &= tmpBV;
                     ntakenBV -= tmpBV;
                     break;
                  case TR::ifbcmpge:
                  case TR::ifsucmpge:
                  case TR::ificmpge:
                     takenBV = *bv[tID];
                     ntakenBV = takenBV;
                     tmpBV.empty();
                     tmpBV.setAll(0, pos-1);
                     ntakenBV &= tmpBV;
                     takenBV -= tmpBV;
                     break;
                  case TR::ifbcmpgt:
                  case TR::ifsucmpgt:
                  case TR::ificmpgt:
                     takenBV = *bv[tID];
                     ntakenBV = takenBV;
                     tmpBV.empty();
                     tmpBV.setAll(0, pos);
                     ntakenBV &= tmpBV;
                     takenBV -= tmpBV;
                     break;
                  case TR::ifbcmple:
                  case TR::ifsucmple:
                  case TR::ificmple:
                     takenBV = *bv[tID];
                     ntakenBV = takenBV;
                     tmpBV.empty();
                     tmpBV.setAll(0, pos);
                     takenBV &= tmpBV;
                     ntakenBV -= tmpBV;
                     break;
                  default:
                     TR_ASSERT(false, "not implemented yet");
                     // not implemented yet
                     return false;
                  }

               if (doAgain)
                  {
                  *bv[n->getSucc(0)->getID()] |= ntakenBV;
                  *bv[n->getSucc(1)->getID()] |= takenBV;
                  }
               else
                  {
                  int id;
                  id = n->getSucc(0)->getID();
                  orgBV = *bv[id];
                  *bv[id] |= ntakenBV;
                  if (!(orgBV == *bv[id])) doAgain = true;

                  id = n->getSucc(1)->getID();
                  orgBV = *bv[id];
                  *bv[id] |= takenBV;
                  if (!(orgBV == *bv[id])) doAgain = true;
                  }

               doPropagateSuccs = false; // already done

               if (!n->isOutsideOfLoop())
                  {
                  TR::TreeTop *thisDestination = n->getDestination();
                  if (!initExitTreeTop)
                     {
                     if (trace())
                        traceMsg(comp(), "analyzeBoolTable - Delimiter checking node %d targets treetop: %p block_%d: %p\n",
                                n->getID(), thisDestination, thisDestination->getEnclosingBlock()->getNumber(), thisDestination->getEnclosingBlock());
                     initExitTreeTop = true;
                     exitTreeTop = thisDestination;
                     }
                  if (exitTreeTop != thisDestination)
                     {
                     if (trace() && exitTreeTop)
                        traceMsg(comp(), "analyzeBoolTable - found conflicting successors.  Delimiter checking node %d targets treetop: %p (!= %p) block_%d: %p\n",
                                n->getID(), thisDestination, exitTreeTop, thisDestination->getEnclosingBlock()->getNumber(), thisDestination->getEnclosingBlock());

                     exitTreeTop = NULL;
                     }
                  }
               }
            }
         else if (p == ignoreNode)
            {
            doPropagateSuccs = false; // ignore
            }
         else
            {
            if (p &&
                p->getNumSuccs() >= 2)
               {
               for (i = p->getNumSuccs(); --i >= 0; )
                  {
                  if (p->getSucc(i)->getOpcode() == TR_exitnode)
                     {
                     doPropagateSuccs = false; // ignore
                     break;
                     }
                  }
               }
            }

         if (doPropagateSuccs)
            {
            if (doAgain)
               {
               for (i = n->getNumSuccs(); --i >= 0; )
                  *bv[n->getSucc(i)->getID()] |= *bv[tID];
               }
            else
               {
               for (i = n->getNumSuccs(); --i >= 0; )
                  {
                  int id = n->getSucc(i)->getID();
                  orgBV = *bv[id];
                  *bv[id] |= *bv[tID];
                  if (!(orgBV == *bv[id])) doAgain = true;
                  }
               }
            }
         }
      }

   if (retSameExit)
      {
      *retSameExit = exitTreeTop;
      }
   return true;
   }


#define BYTEBVOFFSET (128)
#define ALLOCBYTEBVSIZE (128+256)
int32_t
TR_CISCTransformer::analyzeByteBoolTable(TR_CISCNode *boolTable, uint8_t *table256, TR_CISCNode *ignoreNode, TR::TreeTop **retSameExit)
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   List<TR_CISCNode> *P2T = _P2T;
   List<TR_CISCNode> *T2P = _T2P;
   TR_CISCGraph *P = _P;
   TR_CISCGraph *T = _T;
   //int32_t i;
   //TR::TreeTop *exitTreeTop;

   //
   // initialize
   //
   memset(table256, 0, 256);
   if (!boolTable || !getP2TRepInLoop(boolTable)) return 0;     // # of delimiter is zero

   TR_BitVector **bv, defBV(ALLOCBYTEBVSIZE, trMemory(), stackAlloc);
   uint32_t size = sizeof(*bv) * T->getNumNodes();
   TR_ASSERT(boolTable->getOpcode() == TR_booltable, "error!");
   TR_CISCNode *defNode = boolTable->getChild(0);
   TR_CISCNode *defTargetNode = getP2TRepInLoop(defNode);
   bv = (TR_BitVector **)trMemory()->allocateMemory(size, stackAlloc);
   memset(bv, 0, size);

   switch((defTargetNode ? defTargetNode : defNode)->getOpcode())
      {
      case TR::b2i:
         if (defNode->isOptionalNode()) defNode = defNode->getChild(0);
         // fall through
      case TR::bloadi:
         defBV.setAll(-128+BYTEBVOFFSET, 127+BYTEBVOFFSET);
         break;
      case TR::bu2i:
         defBV.setAll(   0+BYTEBVOFFSET, 255+BYTEBVOFFSET);
         break;
      default:
         TR_ASSERT(false, "not implemented yet");
         // not implemented yet
         return -1;     // error
      }

   if (!analyzeBoolTable(bv, retSameExit, boolTable, &defBV, defNode, ignoreNode, BYTEBVOFFSET, ALLOCBYTEBVSIZE)) return -1; // error

   TR_BitVectorIterator bvi(*bv[T->getExitNode()->getID()]);
   int32_t count = 0;

   // Create the function table from the BitVector of the exit node
   while (bvi.hasMoreElements())
      {
      int32_t nextElement = bvi.getNextElement() - BYTEBVOFFSET;
      if (nextElement < 0) nextElement += 256;
      TR_ASSERT(0 <= nextElement && nextElement < 256, "error!!!");
      table256[nextElement] = nextElement ? nextElement : 1;
      count ++;
      }
   if (trace())
      {
      static int traceByteBoolTable = -1;
      if (traceByteBoolTable < 0)
         {
         char *p = feGetEnv("traceBoolTable");
         traceByteBoolTable = p ? 1 : 0;
         }
      if (1 > count || count > 255 || traceByteBoolTable)
         {
         traceMsg(comp(), "analyzeByteBoolTable: count is %d\n",count);
         ListIterator<TR_CISCNode> pi(_candidateRegion);
         TR_CISCNode *pn;
         traceMsg(comp(), "Predecessors of the exit node:\n ID:count\n");
         for (pn = pi.getFirst(); pn; pn = pi.getNext())
            {
            int32_t id = pn->getID();
            if (getT2PheadRep(id) == boolTable)
               {
               traceMsg(comp(), "%3d:%3d:",id,bv[id]->elementCount());
               bv[id]->print(comp());
               traceMsg(comp(), "\n");
               }
            }
         }
      }
   //TR_ASSERT(1 <= count && count <= 255, "maybe error!!");
   return count;
   }


#define CHARBVOFFSET (0)
#define ALLOCCHARBVSIZE (65536)
int32_t
TR_CISCTransformer::analyzeCharBoolTable(TR_CISCNode *boolTable, uint8_t *table65536, TR_CISCNode *ignoreNode, TR::TreeTop **retSameExit)
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   List<TR_CISCNode> *P2T = _P2T;
   List<TR_CISCNode> *T2P = _T2P;
   TR_CISCGraph *P = _P;
   TR_CISCGraph *T = _T;
   //int32_t i;

   //
   // initialize
   //
   memset(table65536, 0, 65536);
   if (!boolTable || !getP2TRepInLoop(boolTable)) return 0;     // # of delimiter is zero

   TR_BitVector **bv, defBV(ALLOCCHARBVSIZE, trMemory(), stackAlloc);
   uint32_t size = sizeof(*bv) * T->getNumNodes();
   TR_ASSERT(boolTable->getOpcode() == TR_booltable, "error!");
   TR_CISCNode *defNode = boolTable->getChild(0);
   TR_CISCNode *defTargetNode = getP2TRepInLoop(defNode);
   bv = (TR_BitVector **)trMemory()->allocateMemory(size, stackAlloc);
   memset(bv, 0, size);

   switch((defTargetNode ? defTargetNode : defNode)->getOpcode())
      {
      case TR::su2i:
         if (defNode->isOptionalNode()) defNode = defNode->getChild(0);
         // fall through
      case TR::cloadi:
         defBV.setAll(0, 65535);
         break;
      default:
         TR_ASSERT(false, "not implemented yet");
         // not implemented yet
         return -1;     // error
      }

   if (!analyzeBoolTable(bv, retSameExit, boolTable, &defBV, defNode, ignoreNode, CHARBVOFFSET, ALLOCCHARBVSIZE)) return -1;    // error

   TR_BitVectorIterator bvi(*bv[T->getExitNode()->getID()]);
   int32_t count = 0;

   // Create the function table from the BitVector of the exit node
   while (bvi.hasMoreElements())
      {
      int32_t nextElement = bvi.getNextElement() - CHARBVOFFSET;
      TR_ASSERT(0 <= nextElement && nextElement < 65536, "error!!!");
      table65536[nextElement] = 1;
      count ++;
      }
   if (trace())
      {
      static char *traceCharBoolTable = feGetEnv("traceBoolTable");

      if (count < 1 || count > 65535 || traceCharBoolTable)
         {
         traceMsg(comp(), "analyzeByteBoolTable: count is %d\n",count);
         ListIterator<TR_CISCNode> pi(_candidateRegion);
         TR_CISCNode *pn;
         traceMsg(comp(), "Predecessors of the exit node:\n ID:count\n");
         for (pn = pi.getFirst(); pn; pn = pi.getNext())
            {
            int32_t id = pn->getID();
            if (getT2PheadRep(id) == boolTable)
               {
               traceMsg(comp(), "%3d:%3d:", id, bv[id]->elementCount());
               bv[id]->print(comp());
               traceMsg(comp(), "\n");
               }
            }
         }
      }

   return count;
   }


//*****************************************************************************
// It sets the cold flags to the all blocks in _bblistBody.
//*****************************************************************************
void
TR_CISCTransformer::setColdLoopBody()
   {
   ListIterator<TR::Block> bi(&_bblistBody);
   TR::Block *b;
   for (b = bi.getFirst(); b; b = bi.getNext())
      {
      b->setIsCold();
      b->setFrequency(-1);
      }
   }


//*****************************************************************************
// It get minimum and maximum of byte code index within the list l.
// Note: Please initialize minIndex and maxIndex in caller!!
// The return value means whether the result includes inlined region.
//*****************************************************************************
bool
TR_CISCTransformer::getBCIndexMinMax(List<TR_CISCNode> *l, int32_t *_minIndex, int32_t *_maxIndex, int32_t *_minLN, int32_t *_maxLN, bool allowInlined)
   {
   int32_t minIndex = *_minIndex;
   int32_t maxIndex = *_maxIndex;
   int32_t minLN = *_minLN;
   int32_t maxLN = *_maxLN;
   ListIterator<TR_CISCNode> ni(l);
   TR_CISCNode *n;
   TR::Node *tn;
   bool includeInline = false;
   for (n = ni.getFirst(); n; n = ni.getNext())
      {
      ListElement<TrNodeInfo> *le = n->getTrNodeInfo()->getListHead();
      if (le)
         {
         tn = le->getData()->_node;
         bool go = true;
         if (tn->getInlinedSiteIndex() != -1)
            {
            if (allowInlined)
               includeInline = true;
            else
               go = false;
            }
         if (go)
            {
            int bcIndex = tn->getByteCodeIndex();
            if (minIndex > bcIndex) minIndex = bcIndex;
            if (maxIndex < bcIndex) maxIndex = bcIndex;
            int LN = comp()->getLineNumber(tn);
            if (minLN > LN) minLN = LN;
            if (maxLN < LN) maxLN = LN;
            }
         }
      }
   *_minIndex = minIndex;
   *_maxIndex = maxIndex;
   *_minLN = minLN;
   *_maxLN = maxLN;
   return includeInline;
   }


//*****************************************************************************
// It shows candidates of input code that cannot be transformed to idioms.
//*****************************************************************************
void
TR_CISCTransformer::showCandidates()
   {
   if (!isShowingCandidates()) return;
   FILE *fp = stderr;
   int32_t minIndex = _candidatesForShowing.getMinBCIndex();
   int32_t maxIndex = _candidatesForShowing.getMaxBCIndex();
   int32_t minLN = _candidatesForShowing.getMinLineNumber();
   int32_t maxLN = _candidatesForShowing.getMaxLineNumber();
   if (minIndex <= maxIndex)
      {
      ListIterator<TR_CISCGraph> idiomI(_candidatesForShowing.getListIdioms());
      TR_CISCGraph *p;
      fprintf(fp, "!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
      fprintf(fp, "Candidate is found for ");
      int count = 0;
      for (p = idiomI.getFirst(); p; p = idiomI.getNext())
         {
         if (count != 0) fprintf(fp, ",");
         fprintf(fp, "%s", p->getTitle());
         count ++;
         }
      fprintf(fp, " (%s) in %s",
              comp()->getHotnessName(comp()->getMethodHotness()),
              _T->getTitle());
#if SHOW_BCINDICES
      fprintf(fp, "\t bcindex is %d - %d, linenumber is %d - %d.", minIndex, maxIndex, minLN, maxLN);
#endif
      fprintf(fp, "\n");
      }
   }



//*****************************************************************************
// It registers candidates of input code that cannot be transformed to idioms.
//*****************************************************************************
void
TR_CISCTransformer::registerCandidates()
   {
   if (!isShowingCandidates()) return;
   ListIterator<TR_CISCNodeRegion> ri(&_candidatesForRegister);
   TR_CISCNodeRegion *r;
   int32_t minIndex, maxIndex;
   int32_t minLN, maxLN;
   minIndex = 0x7fffffff;
   maxIndex = -minIndex;
   minLN = 0x7fffffff;
   maxLN = -minIndex;
   for (r = ri.getFirst(); r; r = ri.getNext())
      {
      getBCIndexMinMax(r, &minIndex, &maxIndex, &minLN, &maxLN, false);
      }
   if (minIndex <= maxIndex) _candidatesForShowing.add(_P, minIndex, maxIndex, minLN, maxLN);
   }


//*****************************************************************************
// It returns the following conditions:
//  * _T2P_NULL: There is no pattern nodes corresponding to the target node t.
//               (regardless whether p is null or non-null)
//
// If p is non-null,
//  * _T2P_NotMatch: Cannot find any relationships between t and p
//  * _T2P_MatchAndSingle: The node t corresponds to ONLY the node p. (= _T2P_MatchMask | _T2P_Single)
//  * _T2P_MatchAndMultiple: The node t corresponds to the node p,    (= _T2P_MatchMask | _T2P_Multiple)
//                           but there is another candidate.
// If p is null,
//  * _T2P_Single: There is a single pattern node corresponding to the node t.
//  * _T2P_Multiple: There are multiple pattern nodes corresponding to the node t.
//*****************************************************************************
CISCT2PCond
TR_CISCTransformer::analyzeT2P(TR_CISCNode *t, TR_CISCNode *p)
   {
   //CISCT2PCond ret;
   int32_t tid = t->getID();
   List<TR_CISCNode> *l = _T2P + tid;
   TR_CISCNode *t2p;
   if (l->isEmpty())
      {
      return _T2P_NULL;
      }
   else if (l->isSingleton())
      {
      t2p = l->getListHead()->getData();
      if (!p) return _T2P_Single;
      return (p == t2p) ? _T2P_MatchAndSingle : _T2P_NotMatch;
      }
   else
      {
      if (!p) return _T2P_Multiple;
      ListIterator<TR_CISCNode> t2pi(l);
      for (t2p = t2pi.getFirst(); t2p; t2p = t2pi.getNext())
         {
         if (p == t2p) return _T2P_MatchAndMultiple;
         }
      return _T2P_NotMatch;
      }
   }


//*****************************************************************************
// It analyzes whether the pattern node TR_arrayindex corresponds to
// an induction variable V or V + something in an input graph.
//*****************************************************************************
bool
TR_CISCTransformer::analyzeOneArrayIndex(TR_CISCNode *arrayindex, TR::SymbolReference *inductionVariableSymRef)
   {
   List<TR_CISCNode> *l = getP2T() + arrayindex->getID();
   if (l->isEmpty())
      {
      if (arrayindex->isOptionalNode()) return true;
      else return false;
      }
   else if (!l->isSingleton()) return false;
   TR_CISCNode *t = l->getListHead()->getData();
   if (t->getOpcode() == TR::iadd)       // check induction variable + something
      {
      bool ret = false;
      TR_CISCNode *c;
      c = t->getChild(0);
      if (c->getOpcode() == TR::iload)
         {
         TR::SymbolReference *symref = c->getHeadOfTrNodeInfo()->_node->getSymbolReference();
         if (symref == inductionVariableSymRef) ret = true;
         }
      if (!ret)
         {
         c = t->getChild(1);
         if (c->getOpcode() == TR::iload)
            {
            TR::SymbolReference *symref = c->getHeadOfTrNodeInfo()->_node->getSymbolReference();
            if (symref == inductionVariableSymRef) ret = true;
            }
         }
      if (!ret) return false;
      }
   else if (t->getOpcode() != TR_variable)
      {
      return false;
      }
   return true;
   }


// Check if all of the node TR_arrayindex correspond to V or V+something
bool
TR_CISCTransformer::analyzeArrayIndex(TR::SymbolReference *inductionVariableSymRef)
   {
   // check each array index
   for (int32_t i = 0; ; i++)
      {
      TR_CISCNode *arrayindex = _P->getCISCNode(TR_arrayindex, true, i);
      if (!arrayindex) break;   // end

      if (!analyzeOneArrayIndex(arrayindex, inductionVariableSymRef)) return false;
      }
   return true;
   }


// Count valid nodes where the node TR_arrayindex corresponds to V or V+something
int32_t
TR_CISCTransformer::countGoodArrayIndex(TR::SymbolReference *inductionVariableSymRef)
   {
   int32_t ret = 0;
   // check each array index
   for (int32_t i = 0; ; i++)
      {
      TR_CISCNode *arrayindex = _P->getCISCNode(TR_arrayindex, true, i);
      if (!arrayindex)
         {
         if (i == 0) ret = -1; // no TR_arrayindex in the pattern
         break;   // end
         }

      if (analyzeOneArrayIndex(arrayindex, inductionVariableSymRef)) ret ++;
      }
   return ret;
   }


//*****************************************************************************
// It performs very simple optimizations using UD/DU chains.
// Currently, it performs:
//   (1) redundant BNDCHK elimination. necessary for very early phase, such as in the earlyGlobalOpts phase
//*****************************************************************************
bool
TR_CISCTransformer::simpleOptimization()
   {
   TR_ASSERT(_T->isSetUDDUchains(), "please call simpleOptimization() after executing importUDchains()!");
   ListIterator<TR_CISCNode> ni(_T->getOrderByData());
   TR_CISCNode *n, *ch, *def;
   List<TR_CISCNode> *l;
   TR_CISCNode quasiConst2(trMemory(), TR_quasiConst2, TR::NoType, 0, 0, 0, 0);

   for (n = ni.getFirst(); n; n = ni.getNext())
      {
      if (!n->isNegligible())
         {
         switch(n->getOpcode())
            {
            case TR::BNDCHK:
               // check whether the size is greater or equal to 256
               //       and the array index comes from TR::bu2i.
               ch = n->getChild(0);
               if (ch->getOpcode() == TR::iconst)
                  {
                  if (ch->getOtherInfo() >= 256)
                     {
                     ch = n->getChild(1);
                     switch(ch->getOpcode())
                        {
                        case TR::iload:
                           if (def = ch->getNodeIfSingleChain())
                              {
                              if ((def->getNumChildren() > 0) && def->getChild(0))  // There is a bug where def is a TR_entrynode, so getChild(0) is null.
                                 {
                                 switch(def->getChild(0)->getOpcode())
                                    {
                                    case TR::bu2i:
                                       n->setIsNegligible(); // because the range is 0 - 255
                                       break;
                                    }
                                 }
                              }
                           break;
                        case TR::bu2i:
                           n->setIsNegligible(); // because the range is 0 - 255
                           break;
                        }
                     }
                  }
               break;

            default:
               if (!n->isOutsideOfLoop())
                  {
                  if (n->isStoreDirect())
                     {
                     ListIterator<TR_CISCNode> useI(n->getChains());
                     bool includeExitNode = false;
                     for (ch = useI.getFirst(); ch; ch = useI.getNext())
                        {
                        if (ch->getDagID() != n->getDagID())
                           {
                           includeExitNode = true;
                           break;
                           }
                        }
                     if (!includeExitNode) n->setIsNegligible();
                     }
                  }
               if (!n->isNegligible())  // Still need to analysis?
                  {
                  if (quasiConst2.isEqualOpc(n) &&
                      n->getParents()->isSingleton())
                     {
                     TR_CISCNode *parent = n->getHeadOfParents();
                     if (parent->getOpcode() == TR::iadd)
                        {
                        l = _T2P + parent->getID();
                        if (l->isSingleton() &&
                            l->getListHead()->getData()->getOpcode() == TR_arrayindex)
                           {
                           n->setIsNegligible();
                           }
                        }
                     }
                  }
               break;
            }
         }
      }
   return true;
   }


//*****************************************************************************
// get the hash value of TR_CISCNodeRegion
//*****************************************************************************
uint64_t
TR_CISCTransformer::getHashValue(TR_CISCNodeRegion *r)
   {
   uint64_t ret = 0;
   int count = 0;
   ListIterator<TR_CISCNode> ri(r);
   TR_CISCNode *n;
   for (n = ri.getFirst(); n; n = ri.getNext())
      {
      int i = count % 74;
      int bigshift = i % 5;
      int smallshift = i / 5;
      int shiftcount = bigshift * 10 + smallshift;
      ret += (uint64_t)n->getOpcode() << (uint64_t)shiftcount;
      count++;
      }
   return ret;
   }


//*****************************************************************************
// It analyzes whether we can convert the loop ArrayCmpLen in String.compareTo
// to either ArrayCmpSign or ArrayCmp.
//*****************************************************************************
bool
TR_CISCTransformer::canConvertArrayCmpSign(TR::Node *storeNode, List<TR::TreeTop> *compareIfs, bool *canConvertToArrayCmp)
   {
   static int disable = -1;
   if (disable < 0)
      {
      char *p = feGetEnv("DISABLE_CONVERTCMPSIGN");
      disable = p ? 1 : 0;
      }
   if (disable) return false;
   static int disableCMP = -1;
   if (disableCMP < 0)
      {
      char *p = feGetEnv("DISABLE_CONVERTCMP");
      disableCMP = p ? 1 : 0;
      }

   TR_ASSERT(storeNode->getOpCode().isStoreDirect(), "error");
   TR_UseDefInfo *useDefInfo = getUseDefInfo();
   int32_t useDefIndex = storeNode->getUseDefIndex();
   if (useDefIndex == 0) return true;
   TR_ASSERT(useDefInfo->isDefIndex(useDefIndex), "error!");
   TR_UseDefInfo::BitVector info(comp()->allocator());
   useDefInfo->getUsesFromDef(info, useDefIndex);
   if (!info.IsZero())
      {
      TR_UseDefInfo::BitVector::Cursor cursor(info);
      bool convertToArrayCmp = true;
      for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
         {
         int32_t useIndex = (int32_t) cursor + useDefInfo->getFirstUseIndex();
         TR_ASSERT(useDefInfo->isUseIndex(useIndex), "error!");
         TR::Node *useNode = useDefInfo->getNode(useIndex);
         if (useNode->getReferenceCount() > 1)
            {
            if (trace()) traceMsg(comp(), "canConvertArrayCmpSign failed because ReferenceCount > 1. %p\n",useNode);
            return false;
            }
         TR::Node *parentNode = NULL;
         int32_t retChildNum = -1;
         TR_CISCNode *useCISCNode = _T->getCISCNode(useNode);
         TR::TreeTop *foundTT = NULL;
         if (useCISCNode)
            {
            if (useCISCNode->getParents()->isSingleton())
               {
               TR_CISCNode *parentCISCNode = useCISCNode->getHeadOfParents();
               if (parentCISCNode->getTrNodeInfo()->isSingleton())
                  {
                  parentNode = parentCISCNode->getHeadOfTrNode();
                  foundTT = parentCISCNode->getHeadOfTreeTop();
                  if (parentNode->getChild(0) == useNode)
                     retChildNum = 0;
                  else if (parentNode->getChild(1) == useNode)
                     retChildNum = 1;
                  else
                     parentNode = 0;
                  }
               }
            }
         else
            {
            _useTreeTopMap.buildAllMap();
            foundTT = _useTreeTopMap.findParentTreeTop(useNode);
            if (NULL == foundTT || !searchNodeInTrees(foundTT->getNode(), useNode, &parentNode, &retChildNum))
               {
               if (trace()) traceMsg(comp(), "canConvertArrayCmpSign failed because searchNodeInTrees failed. UseNode: %p with corresponding TreeTop: %p\n",useNode, foundTT);
               return false;
               }
            }
         if (!parentNode)
            {
            if (trace()) traceMsg(comp(), "canConvertArrayCmpSign failed because parentNode is NULL. %p\n",useNode);
            return false;
            }
         TR_ASSERT(foundTT, "error!");
         if (parentNode->getOpCode().isStoreDirect())
            {
            if (!canConvertArrayCmpSign(parentNode, compareIfs, &convertToArrayCmp))
               {
               if (trace()) traceMsg(comp(), "canConvertArrayCmpSign failed because canConvertArrayCmpSign(p) failed. %p\n",useNode);
               return false;
               }
            }
         else if (parentNode->getOpCode().isBooleanCompare())
            {
            TR_ASSERT(retChildNum == 0 || retChildNum == 1, "error");
            TR::Node *theOtherChild = parentNode->getChild(1-retChildNum);
            if (theOtherChild->getInt() != 0 ||
                theOtherChild->getOpCodeValue() != TR::iconst)
               {
               if (trace()) traceMsg(comp(), "canConvertArrayCmpSign failed because theOtherChild is not iconst 0. %p\n",useNode);
               return false;
               }
            if (compareIfs) compareIfs->add(foundTT);
            switch(parentNode->getOpCodeValue())
               {
               case TR::icmpeq:
               case TR::icmpne:
               case TR::ificmpeq:
               case TR::ificmpne:
                  break; // OK!
               default:
                  if (trace())
                     {
                     traceMsg(comp(), "convertArrayCmp failed because parentNode is %s. %x\n",
                             parentNode->getOpCode().getName(),
                             useNode,parentNode);
                     }
                  convertToArrayCmp = false;
                  break;
               }
            }
         else
            {
            if (trace())
               {
               traceMsg(comp(), "canConvertArrayCmpSign failed because unhandled opcode %s. %x %x\n",
                       parentNode->getOpCode().getName(),useNode,parentNode);
               }
            return false;
            }
         }
      if (canConvertToArrayCmp) *canConvertToArrayCmp = convertToArrayCmp;
      }
   if (disableCMP) *canConvertToArrayCmp = false;
#if VERBOSE
   if (*canConvertToArrayCmp)
      printf("!!! canConvertToArrayCmp %s\n",_T->getTitle());
   else
      printf("!!! canConvertToArrayCmpSign %s\n",_T->getTitle());
#endif
   return true;
   }



bool
TR_CISCTransformer::computeTopologicalEmbedding(TR_CISCGraph *P, TR_CISCGraph *T)
   {
   TR::SimpleRegex *disabledPatterns = comp()->getOptions()->getDisabledIdiomPatterns();
   if (disabledPatterns && TR::SimpleRegex::match(disabledPatterns, P->getTitle()))
      {
      if (trace())
         traceMsg(comp(), "%s is disabled by disabledIdiomPatterns={}\n", P->getTitle());
      return false;
      }

   //FIXME: improve this
   if (!T->testAllAspects(P))
      {
      if (trace())
         traceMsg(comp(), "%s is skipped since graph properties do not match (%08x)\n", P->getTitle(),P->getAspectsValue());
      return false;                     // No need to analyze
      }
   if (T->testAnyNoAspects(P))
      {
      if (trace())
         traceMsg(comp(), "%s is skipped due to existence of testAnyNoAspects (%08x)\n",P->getTitle(),P->getNoAspectsValue());
      return false;                     // No need to analyze
      }
   if (!T->meetMinCounts(P))
      {
      if (trace())
         traceMsg(comp(), "%s is skipped due to failure of meetMinCounts (%d %d %d)\n",P->getTitle(),
                P->getAspects()->getIfCount(), P->getAspects()->getIndirectLoadCount(), P->getAspects()->getIndirectStoreCount());
      return false;                     // No need to analyze
      }

   // avoid analyzing very large graphs
   if (T->getNumNodes() >= IDIOM_SIZE_FACTOR*P->getNumNodes())
      {
      if (trace())
         traceMsg(comp(), "%s is skipped due to loop being very large\n", P->getTitle());
      return false;  // No need to analyze
      }

#if !STRESS_TEST
   //FIXME: add TR_EnableIdiomRecognitionWarm to options
   if (1)//!_compilation->getOption(TR_EnableIdiomRecognitionWarm))
      {
      if (T->getHotness() < P->getHotness())
         {
         if (trace())
            traceMsg(comp(), "%s is skipped due to hotness\n",P->getTitle());
         return false;                     // No need to analyze
         }
      }
   bool incorrectBBFreqLevel = (T->getHotness() == veryHot);
   if (!incorrectBBFreqLevel)
      {
      if (P->isHighFrequency() && !T->isHighFrequency())
         {
         if (trace())
            traceMsg(comp(), "%s is skipped due to the rarely iterated loop (!isHighFrequency)\n",P->getTitle());
         return false;     // No need to analyze
         }
      }
   if (T->getHotness() > warm)  // Because IR is called only one time at the warm level, skip this check.
      {
      if (isAfterVersioning() ? P->isInhibitAfterVersioning() :
                                P->isInhibitBeforeVersioning())
         {
         if (trace())
            traceMsg(comp(), "%s is skipped due to loop versioning check\n",P->getTitle());
         return false;     // No need to analyze
         }
      }
#endif



   if (trace())
      {
      traceMsg(comp(), "loopid %d: ", _bblistBody.getListHead()->getData()->getNumber());
      P->dump(comp()->getOutFile(), comp());
      }
   _P = P;
   _T = T;
   _numPNodes = P->getNumNodes();
   _numTNodes = T->getNumNodes();
   initTopologicalEmbedding();

   // Step 1 computes embedding information for an input data dependence graph.
   //
   if (trace())
      traceMsg(comp(), "Computing embedding info for idiom %s in loop %d\n", P->getTitle(), _bblistBody.getListHead()->getData()->getNumber());
   if (showMesssagesStdout()) printf("Idiom: loop %d, %s\n",_bblistBody.getListHead()->getData()->getNumber(),
                                     P->getTitle());
   _sizeResult = _numPNodes * _numTNodes * sizeof(*_embeddedForData);
   _embeddedForData = (uint8_t*)trMemory()->allocateMemory(_sizeResult, stackAlloc);
   if (!computeEmbeddedForData()) return false; // It cannot find all of the idiom nodes.
   if (showMesssagesStdout()) printf("find1 %s\n", P->getTitle());
   if (trace())
      traceMsg(comp(), "Detected IL nodes in loop for idiom %s\n", P->getTitle());

   // Step 2 computes embedding information for an input control flow graph.
   //
   _embeddedForCFG = (uint8_t*)trMemory()->allocateMemory(_sizeResult, stackAlloc);
   _sizeDE = _numPNodes * sizeof(*_DE);
   _EM = (uint8_t*)trMemory()->allocateMemory(_sizeResult, stackAlloc);
   _DE = (uint8_t*)trMemory()->allocateMemory(_sizeDE, stackAlloc);
   if (!computeEmbeddedForCFG()) return false; // It cannot find all of the idiom nodes.
   if (showMesssagesStdout()) printf("find2 %s\n", P->getTitle());
   if (trace())
      traceMsg(comp(), "finished topological embedding for idiom %s\n", P->getTitle());

   // Step 3 creates P2T and T2P tables from embedding information.
   // P and T denote Pattern and Target, respectively.
   // We can use them to find target nodes from pattern nodes, and vice versa.
   //
   _sizeP2T = _numPNodes * sizeof(*_P2T);
   _P2T = (List<TR_CISCNode> *)trMemory()->allocateMemory(_sizeP2T, stackAlloc);
   _sizeT2P = _numTNodes * sizeof(*_T2P);
   _T2P = (List<TR_CISCNode> *)trMemory()->allocateMemory(_sizeT2P, stackAlloc);
   if (!makeLists()) return false; // a variable corresponds to multiple nodes
   if (showMesssagesStdout()) printf("find3 %s\n", P->getTitle());

   // Step 4 transforms the target graph if necessary and
   // checks that both graphs are exactly matched.
   //
   _candidatesForShowing.init();

   // Import UD/DU information of TR::Node to TR_CISCNode._chain
   //
   T->importUDchains(comp(), _useDefInfo);

   // It performs very simple optimizations using UD/DU chains.
   // Currently, it performs:
   //   (1) redundant BNDCHK elimination.
   simpleOptimization();
   if (trace())
      {
      T->dump(comp()->getOutFile(), comp());
      }

   // Analyze whether each candidate of array header constant is appropriate compared to the idiom.
   // Because the array header size is sometimes modified by constant folding
   // e.g. When AH is -24 for a[i], AH is modified to -25 for a[i+1]
   // If the analysis fails, it'll invalidate that node.
   //
   if (P->isRequireAHconst()) analyzeArrayHeaderConst();

   // Analyze relationships for parents, children, predecessors, successors.
   // They are represented to four flags.
   //
   analyzeConnection();

   // Based on above four relationships, we extract the region that matches the idiom graph.
   // If all nodes in the idiom graph are not included in the region, it returns 0.
   //
   _candidateRegion = extractMatchingRegion();
   if (!_candidateRegion ||
       !verifyCandidate() || // all blocks of the loop body are not included in the _candidateRegion
       embeddingHasConflictingBranches())
      {
      if (trace()) traceMsg(comp(), "computeTopologicalEmbedding: Graph transformations failed. (step 3)\n\n");
      registerCandidates();
      _T->restoreListsDuplicator();
      return false;
      }
   if (showMesssagesStdout()) printf("find4 %s\n", P->getTitle());

   //***************************************************************************************
   // Start to transform actual code (TR::Block, TR::TreeTop, TR::Node, and so on...)
   //***************************************************************************************
   resetFlags();
   TransformerPtr transformer = P->getTransformer();
   if (performTransformation(comp(), "%sReducing loop %d to %s\n", OPT_DETAILS, _bblistBody.getListHead()->getData()->getNumber(),
                                         P->getTitle()) && !transformer(this))
      {
      if (trace()) traceMsg(comp(), "computeTopologicalEmbedding: IL Transformer failed. (step 4)\n\n");
      registerCandidates();
      _T->restoreListsDuplicator();
      return false;             // The transformation fails
      }

   if (trace() || showMesssagesStdout())
      {
      char *bcinfo = "";
#if SHOW_BCINDICES
      char tmpbuf[256];
      int32_t minIndex, maxIndex;
      int32_t minLN, maxLN;
      minIndex = 0x7fffffff;
      maxIndex = -minIndex;
      minLN = 0x7fffffff;
      maxLN = -minIndex;
      bool inlined = getBCIndexMinMax(_candidateRegion, &minIndex, &maxIndex, &minLN, &maxLN, true);
      if (minIndex <= maxIndex)
         {
         sprintf(tmpbuf, ", bcindex %d - %d linenumber %d - %d%s.", minIndex, maxIndex, minLN, maxLN, inlined ? " (inlined)" : "");
         bcinfo = tmpbuf;
         }
#endif
#if SHOW_STATISTICS
      if (showMesssagesStdout())
         printf("!! Hash=0x%llx %s %s\n", getHashValue(_candidateRegion), P->getTitle(), T->getTitle());
#endif

      if (trace()) traceMsg(comp(), "***** Transformed *****, %s, %s, %s, loop:%d%s\n",
                          comp()->getHotnessName(comp()->getMethodHotness()),
                          P->getTitle(), T->getTitle(),
                          _bblistBody.getListHead()->getData()->getNumber(),
                           bcinfo);
      if (showMesssagesStdout()) printf("== Transformed == %s, %s, %s, loop:%d%s\n",
                                        comp()->getHotnessName(comp()->getMethodHotness()),
                                        P->getTitle(), T->getTitle(),
                                        _bblistBody.getListHead()->getData()->getNumber(),
                                        bcinfo);
      }

   TR::DebugCounter::incStaticDebugCounter(comp(),
      TR::DebugCounter::debugCounterName(comp(),
         "idiomRecognition.matched/%s/(%s)/%s/loop=%d",
         P->getTitle(),
         comp()->signature(),
         comp()->getHotnessName(comp()->getMethodHotness()),
         _bblistBody.getListHead()->getData()->getNumber()));

   return true;
   }

static void
traceConflictingBranches(
   TR_CISCTransformer *opt,
   TR_CISCNode *pn,
   List<TR_CISCNode> *matches)
   {
   if (!opt->trace())
      return;

   TR::Compilation *comp = opt->comp();
   traceMsg(comp, "Pattern node %d (%s) has conflicting branches:",
      pn->getID(),
      TR_CISCNode::getName((TR_CISCOps)pn->getOpcode(), comp));

   bool first = true;
   ListIterator<TR_CISCNode> ti(matches);
   for (TR_CISCNode *tn = ti.getFirst(); tn != NULL; tn = ti.getNext())
      {
      traceMsg(comp, "%s %d (%s)",
         first ? "" : ",",
         tn->getID(),
         TR_CISCNode::getName((TR_CISCOps)tn->getOpcode(), comp));
      first = false;
      }

   traceMsg(comp, "\n");
   }

/**
 * Determine whether there is a branch in the pattern with multiple matches.
 *
 * Such branches obscure the control flow of the target loop, which otherwise
 * should have to closely match the control flow seen in the pattern.
 *
 * Booltable nodes are exempt because the condition may be expressed via a
 * series of conditionals.
 *
 * Additionally, a branch may have matches ahead of the loop, but since these
 * have no bearing on control flow within the loop, they are allowed. An
 * in-loop match is moved to the front of _P2T, ahead of any outside matches.
 *
 * \return true if there is a troublesome branch
 */
bool
TR_CISCTransformer::embeddingHasConflictingBranches()
   {
   static const char * const disableEnv =
      feGetEnv("TR_disableIdiomRecognitionConflictingBranchTest");
   static bool disable = disableEnv != NULL && disableEnv[0] != '\0';
   if (disable)
      return false;

   List<TR_CISCNode> * const dagNodes = _P->getDagId2Nodes();
   const int32_t dagCount = _P->getNumDagIds();
   for (int32_t dag = 0; dag < dagCount; dag++)
      {
      ListIterator<TR_CISCNode> pi(&dagNodes[dag]);
      for (TR_CISCNode *pn = pi.getFirst(); pn != NULL; pn = pi.getNext())
         {
         uint32_t op = pn->getOpcode();
         bool isIf =
            op == (uint32_t)TR_ifcmpall
            || (op < (uint32_t)TR::NumIlOps && pn->getIlOpCode().isIf());

         if (!isIf)
            continue;

         TR_CISCNode *inLoopMatch = NULL;
         List<TR_CISCNode> *matches = &_P2T[pn->getID()];
         ListIterator<TR_CISCNode> ti(matches);
         for (TR_CISCNode *tn = ti.getFirst(); tn != NULL; tn = ti.getNext())
            {
            if (getCandidateRegion()->isIncluded(tn))
               {
               if (inLoopMatch != NULL)
                  {
                  traceConflictingBranches(this, pn, matches);
                  TR::DebugCounter::incStaticDebugCounter(comp(),
                     TR::DebugCounter::debugCounterName(comp(),
                        "idiomRecognition.rejected/branchConflict/%s/(%s)/%s/loop=%d",
                        _P->getTitle(),
                        comp()->signature(),
                        comp()->getHotnessName(comp()->getMethodHotness()),
                        _bblistBody.getListHead()->getData()->getNumber()));
                  return true;
                  }
               inLoopMatch = tn;
               }
            }
         if (inLoopMatch != NULL && matches->getHeadData() != inLoopMatch)
            {
            // move it to the front
            matches->remove(inLoopMatch);
            matches->addAfter(inLoopMatch, NULL);
            }
         }
      }

   return false;
}

// Iterate through the loop body blocks to remove Bits.keepAlive() and Reference.reachabilityFence() calls.
// The keepAlive() and Reference.reachabilityFence() calls are NOP functions inserted into NIO libraries to keep
// the NIO object and its native ptr alive until after the native pointer accesses.
// Reference.reachabilityFence is a newly introduced Java 9 public API, we will remove this call only if the caller comes
// from the java.nio package, i.e. only for Reference.reachabilityFence calls replacing existing Bits.keepAlive calls
// in the java.nio package. For all other non-nio callers, we will conservatively not remove the call to not break existing
// Idiom Recognition code.

bool
TR_CISCTransformer::removeBitsKeepAliveCalls(List<TR::Block> *body)
   {
   if (trace())
      traceMsg(comp(), "\tScanning for java/nio/Bits.keepAlive(Ljava/lang/Object;)V calls.\n");
   ListIterator<TR::Block> bi(body);
   TR::Block *block = NULL;
   bool foundCall = false;

   _BitsKeepAliveList.init();

   // Iterate through loop body blcoks
   for (block = bi.getFirst(); block != 0; block = bi.getNext())
      {
      for (TR::TreeTop *tt = block->getEntry(); tt != block->getExit(); tt = tt->getNextTreeTop())
         {
         TR::Node* node = tt->getNode();

         // Look for the following tree:
         //   treetop
         //      vcall #481[0x74fd1dc0]  static Method[java/nio/Bits.keepAlive(Ljava/lang/Object;)V]
         //          aload #423[0x00694234]  Parm[<parm 1 Ljava/nio/ByteBuffer;>]   <flags:"0x4" (X!=0 )/>
         if (node->getOpCodeValue() == TR::treetop)
            {
            node = node->getChild(0);
            if (node->getOpCode().isCall())
               {
               TR::MethodSymbol *methodSym = node->getSymbol()->getMethodSymbol();
               // If the call is Bits.keepAlive() which is package private hence can only be called from the nio package,
               // or the call is Reference.reachabilityFence() which is public _and_ provided the caller is from the nio package,
               // only then we will remove the call node.
               if (methodSym->getRecognizedMethod() == TR::java_nio_Bits_keepAlive
                  || ((methodSym->getRecognizedMethod() == TR::java_lang_ref_Reference_reachabilityFence)
                     && (!strncmp(comp()->fe()->sampleSignature(node->getOwningMethod(), 0, 0, comp()->trMemory()), "java/nio/", 9))))
                  {
                  if (trace())
                     traceMsg(comp(), "\t\tRemoving KeepAlive call found in block %d [%p] @ Node: %p\n",block->getNumber(), block, node);
                  foundCall = true;

                  TR_BitsKeepAliveInfo *info = new (comp()->trStackMemory()) TR_CISCTransformer::TR_BitsKeepAliveInfo(block, tt, tt->getPrevTreeTop());
                  _BitsKeepAliveList.add(info);

                  // Disconnect treetop from list.
                  tt->getPrevTreeTop()->setNextTreeTop(tt->getNextTreeTop());
                  tt->getNextTreeTop()->setPrevTreeTop(tt->getPrevTreeTop());
                  }
               }
            }
         }
      }
   return foundCall;
   }

// Insert cloned copies of Bits.keepAlive() call into the fast path code of the reduced loop.
void
TR_CISCTransformer::insertBitsKeepAliveCalls(TR::Block * block)
   {
   if (trace())
      traceMsg(comp(), "\tInserting java/nio/Bits.keepAlive(Ljava/lang/Object;)V calls into reduced loop.\n");

   ListIterator<TR_BitsKeepAliveInfo> bi(&_BitsKeepAliveList);

   for (TR_BitsKeepAliveInfo *info = bi.getFirst(); info != NULL; info = bi.getNext())
      {
      TR::TreeTop * tt = info->_treeTop;

      // Clone the call node
      TR::Node *callNode = TR::Node::copy(tt->getNode()->getChild(0));
      callNode->decReferenceCount();
      callNode->getChild(0)->incReferenceCount();

      // Uncommon the child
      callNode->setChild(0,callNode->getChild(0)->uncommon());
      TR::Node *treetopNode = TR::Node::create(TR::treetop, 1, callNode);
      block->append(TR::TreeTop::create(comp(), treetopNode));

      if (trace())
         {
         TR::TreeTop * prev = info->_prevTreeTop;
         TR::Block * keepAliveBlock = info->_block;
         traceMsg(comp(), "\t\tInserting KeepAlive call clone node: %p from block %d [%p] node: %p into block :%d %p\n",callNode, keepAliveBlock->getNumber(), keepAliveBlock, tt->getNode(), block->getNumber(), block);
         }
      }
   }

// Restore any Bits.keepAlive() calls to their original locations
void
TR_CISCTransformer::restoreBitsKeepAliveCalls()
   {
   if (trace())
      traceMsg(comp(), "\tRestoring for java/nio/Bits.keepAlive(Ljava/lang/Object;)V calls.\n");
   ListIterator<TR_BitsKeepAliveInfo> bi(&_BitsKeepAliveList);

   for (TR_BitsKeepAliveInfo *info = bi.getFirst(); info != NULL; info = bi.getNext())
      {
      TR::TreeTop * tt = info->_treeTop;
      TR::TreeTop * prev = info->_prevTreeTop;
      TR::Block * block = info->_block;

      if (trace())
         traceMsg(comp(), "\t\tInserting KeepAlive call found in block %d [%p] @ Node: %p\n",block->getNumber(), block, tt->getNode());
      prev->insertAfter(tt);
      }
   }

