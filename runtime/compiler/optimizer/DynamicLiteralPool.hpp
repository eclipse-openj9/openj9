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

#ifndef DYNAMICLITERALPOOL_INCL
#define DYNAMICLITERALPOOL_INCL

#include <stddef.h>
#include <stdint.h>
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"


namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }


class TR_DynamicLiteralPool : public TR::Optimization
   {
   public:
   TR_DynamicLiteralPool(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_DynamicLiteralPool(manager);
      }

   virtual int32_t perform();
   virtual int32_t performOnBlock(TR::Block *);
   virtual const char * optDetailString() const throw();

   int32_t process(TR::TreeTop *, TR::TreeTop *);

   TR::SymbolReference * getLitPoolAddressSym()
      {
      if (_litPoolAddressSym==NULL) initLiteralPoolBase();
      return _litPoolAddressSym;
      }
   TR::Node * getAloadFromCurrentBlock(TR::Node *parent)
      {
      if (_aloadFromCurrentBlock==NULL)
         {
         setAloadFromCurrentBlock(TR::Node::createWithSymRef(parent, TR::aload, 0, getLitPoolAddressSym()));
         dumpOptDetails(comp(), "New aload needed, it is: %p!\n", _aloadFromCurrentBlock);
         }
      else
         {
         dumpOptDetails(comp(), "Can re-use aload %p!\n",_aloadFromCurrentBlock);
         }
      return _aloadFromCurrentBlock;
      }
   void setAloadFromCurrentBlock(TR::Node *aloadNode)  {_aloadFromCurrentBlock = aloadNode;}

   TR::Node *getVMThreadAloadFromCurrentBlock(TR::Node *parent);
   void setVMThreadAloadFromCurrentBlock(TR::Node *aloadNode)  {_vmThreadAloadFromCurrentBlock = aloadNode;}

   int32_t getNumChild() {return  _numChild;}
   void    setNumChild(int32_t n) {_numChild=n;}

   private:

   TR::SymbolReference * _litPoolAddressSym;
   TR::Block           * _currentBlock;
   TR::Node            * _aloadFromCurrentBlock;
   TR::Node            * _vmThreadAloadFromCurrentBlock;
   bool                 _changed;
   int32_t             _numChild;

   void initLiteralPoolBase();
   bool processBlock(TR::Block *block, vcount_t visitCount);
   bool visitTreeTop(TR::TreeTop *, TR::Node *grandParent, TR::Node *parent, TR::Node *node, vcount_t visitCount);
   bool transformLitPoolConst(TR::Node *grandParent, TR::Node *parent, TR::Node *child);
   bool transformConstToIndirectLoad(TR::Node *parent, TR::Node *child);
   bool transformStaticSymRefToIndirectLoad(TR::TreeTop *, TR::Node *parent, TR::Node * & child);
   bool transformNeeded(TR::Node *grandParent, TR::Node *parent, TR::Node *child);
   bool addNewAloadChild(TR::Node *node);
   bool handleNodeUsingSystemStack(TR::TreeTop *, TR::Node *parent, TR::Node *node, vcount_t visitCount);
   bool handleNodeUsingVMThread(TR::TreeTop *, TR::Node *parent, TR::Node *node, vcount_t visitCount);
   };

#endif

