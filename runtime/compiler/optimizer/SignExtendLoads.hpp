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

#ifndef SIGN_EXTEND_LOADS_INCL
#define SIGN_EXTEND_LOADS_INCL

#include "optimizer/Optimization.hpp"

#include <stdint.h>
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/jittypes.h"
#include "optimizer/OptimizationManager.hpp"

namespace TR { class Node; }
template <class T> class TR_ScratchList;

class TR_SignExtendLoads : public TR::Optimization
   {
   public:
   TR_SignExtendLoads(TR::OptimizationManager *manager)
      : TR::Optimization(manager)
      {
      setTrace(comp()->getOption(TR_TraceSEL));
      }
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_SignExtendLoads(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   private:
   bool gatheri2lNodes(TR::Node* parent,TR::Node *node, TR_ScratchList<TR::Node> & i2lList,
                       TR_ScratchList<TR::Node> & userI2lList,bool);

   friend struct HashTable;

   // Hash table mapping of nodes to lists of referring nodes
   //
   struct HashTableEntry
      {
      HashTableEntry          *_next;
      TR::Node                 *_node;
      TR_ScratchList<TR::Node> *_referringList;
      };

   struct HashTable
      {
      int32_t          _numBuckets;
      HashTableEntry **_buckets;
      };

   int32_t nodeHashBucket(TR::Node *node)
               {return ((uintptrj_t)node >> 2) % _sharedNodesHash._numBuckets;}

   void    InitializeHashTable();
   TR_ScratchList<TR::Node> * getListFromHash(TR::Node *node);
   void    addListToHash(TR::Node *node,TR_ScratchList<TR::Node> *nodeList);
   void    addNodeToHash(TR::Node *node,TR::Node *parent);
   void    Propagatei2lNode(TR::Node *i2lNode,TR::Node *parent,int32_t childIndex,bool);
   void    ProcessNodeList(TR_ScratchList<TR::Node> &list,bool isAddressCalc);
   void    Insertl2iNode(TR::Node *targetNode);
   bool    ConvertSubTreeToLong(TR::Node *parent,TR::Node *node,bool changeNode);
   void    emptyHashTable();
   void    Inserti2lNode(TR::Node *targetNode,TR::Node* newI2LNode);
   void    ReplaceI2LNode(TR::Node *i2lNode,TR::Node *newNode);

   HashTable _sharedNodesHash;
   };
extern bool shouldEnableSEL(TR::Compilation *c);
#endif
