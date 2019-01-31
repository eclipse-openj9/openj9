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

#ifndef IN_MEM_LOAD_STORE_MARK_INCL
#define IN_MEM_LOAD_STORE_MARK_INCL

#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/Node.hpp"
#include "infra/List.hpp"

namespace TR { class TreeTop; }

class InMemoryLoadStoreMarking
   {
   public:
   TR_ALLOC(TR_Memory::CodeGenerator);
   InMemoryLoadStoreMarking(TR::CodeGenerator* cg): cg(cg),
                                      _BCDAggrLoadList(cg->comp()->trMemory()),
                                      _BCDAggrStoreList(cg->comp()->trMemory()),
                                      _BCDConditionalCleanLoadList(cg->comp()->trMemory())
   {
   _comp = cg->comp();
   }

   void perform();

   private:

   TR::CodeGenerator * cg;
   TR::Compilation *_comp;
   List<TR::Node>                    _BCDAggrLoadList;
   List<TR::Node>                    _BCDAggrStoreList;

   // _BCDConditionalCleanLoadList contains the commoned BCD load nodes that can have skipCopyOnLoad set on
   // them under these particular conditions:
   //
   // pdstore "a" clean
   //    pdload "a"
   //
   //    =>pdload "a"
   //
   // Can set skipCopyOnLoad on pdload "a" iff all commoned references have parents who do not rely
   // on a clean sign (other cleans, setsign, pd arith etc)...
   // Any stores aliased to "a" are explicitly checking for perfect overlap (a 'must' vs a 'may' alias)

   List<TR::Node>                    _BCDConditionalCleanLoadList;

   // NOTE: update _TR_NodeListTypeNames when adding new node list types
   enum TR_NodeListTypes
      {
      LoadList                   = 0,
      ConditionalCleanLoadList   = 1,
      StoreList                  = 2,
      NodeList_NumTypes,
      UnknownNodeList            = NodeList_NumTypes
      };

   TR::Compilation *comp() { return _comp; }

   void examineFirstReference(TR::Node *node, TR::TreeTop *tt);
   void examineCommonedReference(TR::Node *child, TR::Node *parent, TR::TreeTop *tt);
   bool isConditionalCleanLoad(TR::Node *listNode, TR::Node *defNode);
   void addToConditionalCleanLoadList(TR::Node *listNode);
   void refineConditionalCleanLoadList(TR::Node *child, TR::Node *parent);
   void handleLoadLastReference(TR::Node *node, List<TR::Node> &nodeList, TR_NodeListTypes listType);
   void processBCDAggrNodeList(List<TR::Node> &nodeList, TR::Node *defNode, TR_NodeListTypes listType);
   void handleDef(TR::Node*, TR::TreeTop*);
   void visitChildren(TR::Node*, TR::TreeTop*, vcount_t);
   bool allListsAreEmpty();
   void clearAllLists();
   void clearLoadLists();

   static char *getName(TR_NodeListTypes s)
      {
      if (s < NodeList_NumTypes)
         return _TR_NodeListTypeNames[s];
      else
         return (char*)"UnknownNodeListType";
      }

   static char *_TR_NodeListTypeNames[NodeList_NumTypes];
   };

#endif
