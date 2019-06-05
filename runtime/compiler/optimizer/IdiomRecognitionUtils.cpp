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

#include "optimizer/IdiomRecognitionUtils.hpp"

#include <algorithm>
#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
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
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/List.hpp"
#include "infra/TRCfgEdge.hpp"
#include "optimizer/IdiomRecognition.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/TranslateTable.hpp"

/************************************/
/************ Utilities *************/
/************************************/
void
dump256Bytes(uint8_t *t, TR::Compilation * comp)
   {
   int i;
   traceMsg(comp, "  | 0 1 2 3 4 5 6 7 8 9 A B C D E F\n");
   traceMsg(comp, "--+--------------------------------");
   for (i = 0; i < 256; i++)
      {
      if ((i % 16) == 0)
         {
         traceMsg(comp, "\n%02X|",i);
         }
      traceMsg(comp, "%2x",t[i]);
      }
   traceMsg(comp, "\n");
   }


/******************************************************/
/************ Utilities for making idioms *************/
/******************************************************/

//*****************************************************************************************
// create the idiom for "v = src1 op val"
//*****************************************************************************************
TR_PCISCNode *
createIdiomIOP2VarInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, int opcode, TR_PCISCNode *v, TR_PCISCNode *src1, TR_PCISCNode *val)
   {
   TR_PCISCNode *n0, *n1;
   TR_ASSERT(v->getOpcode() == TR_variable || v->getOpcode() == TR::iload, "Error!");
   n0  = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), opcode, v->getOpcode() == TR_variable ?  TR::NoType : TR::Int32, tgt->incNumNodes(),  dagId,   1,   2,   pred); tgt->addNode(n0);
   n1  = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::istore, TR::Int32,  tgt->incNumNodes(),  dagId,   1,   2,   n0);   tgt->addNode(n1);
   n0->setChildren(src1, val);
   n1->setChildren(n0, v->getOpcode() == TR::iload ? v->getChild(0) : v);
   n0->setIsChildDirectlyConnected();
   n1->setIsChildDirectlyConnected();
   n0->setIsSuccDirectlyConnected();
   return n1;
   }

//*****************************************************************************************
// create the idiom for "v = v op val"
//*****************************************************************************************
TR_PCISCNode *
createIdiomIOP2VarInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, int opcode, TR_PCISCNode *v, TR_PCISCNode *val)
   {
   return createIdiomIOP2VarInLoop(tgt, ctrl, dagId, pred, opcode, v, v, val);
   }

//*****************************************************************************************
// create the idiom for "v = src1 - val"
//*****************************************************************************************
TR_PCISCNode *
createIdiomDecVarInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *v, TR_PCISCNode *src1, TR_PCISCNode *subval)
   {
   return createIdiomIOP2VarInLoop(tgt, ctrl, dagId, pred, TR::isub, v, src1, subval);
   }

//*****************************************************************************************
// create the idiom for "v = src1 + val"
//*****************************************************************************************
TR_PCISCNode *
createIdiomIncVarInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *v, TR_PCISCNode *src1, TR_PCISCNode *addval)
   {
   return createIdiomIOP2VarInLoop(tgt, ctrl, dagId, pred, TR::iadd, v, src1, addval);
   }

//*****************************************************************************************
// create the idiom for "v = v - val"
//*****************************************************************************************
TR_PCISCNode *
createIdiomDecVarInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *v, TR_PCISCNode *subval)
   {
   return createIdiomDecVarInLoop(tgt, ctrl, dagId, pred, v, v, subval);
   }

//*****************************************************************************************
// create the idiom for "v = v + val"
//*****************************************************************************************
TR_PCISCNode *
createIdiomIncVarInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *v, TR_PCISCNode *addval)
   {
   return createIdiomIncVarInLoop(tgt, ctrl, dagId, pred, v, v, addval);
   }

//*****************************************************************************************
// create "iconst val" or "lconst val". Use lconst for a 64-bit environment
//*****************************************************************************************
TR_PCISCNode *
createIdiomArrayRelatedConst(TR_PCISCGraph *tgt, int32_t ctrl, uint16_t id, int dagId, int32_t val)
   {
   TR_PCISCNode *ret;
   uint32_t opcode = (ctrl & CISCUtilCtl_64Bit) ? TR::lconst : TR::iconst;
   ret = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), opcode, opcode == TR::lconst ? TR::Int64 : TR::Int32, id, dagId, 0, 0, val);
   tgt->addNode(ret);
   return ret;
   }


//*****************************************************************************************
// create "iconst -sizeof(array header)" or "lconst -sizeof(array header)".
// Use lconst for a 64-bit environment.
//*****************************************************************************************
TR_PCISCNode *
createIdiomArrayHeaderConst(TR_PCISCGraph *tgt, int32_t ctrl, uint16_t id, int dagId, TR::Compilation *c)
   {
   return createIdiomArrayRelatedConst(tgt, ctrl, id, dagId, -(int32_t)TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
   }

//*****************************************************************************************
// when the environment is under 64-bit, it'll insert "i2l" to the node.
//*****************************************************************************************
TR_PCISCNode *
createIdiomI2LIfNecessary(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode **pred, TR_PCISCNode *node)
   {
   TR_PCISCNode *ret = node;
   if ((ctrl & (CISCUtilCtl_64Bit|CISCUtilCtl_NoI2L)) == (CISCUtilCtl_64Bit|0))
      {
      ret = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::i2l, TR::Int64,      tgt->incNumNodes(),  dagId,   1,   1, *pred, node); tgt->addNode(ret);
      *pred = ret;
      }
   return ret;
   }

#if 0
//*****************************************************************************************
// It creates an address tree of "index part" for a byte array. (e.g. index-header)
// It may insert I2L depending on the given flag.
//
// "InLoop" means that it sets the given dagId to all nodes in this subroutine.
//*****************************************************************************************
TR_PCISCNode *
createIdiomByteArrayAddressIndexTreeInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *index, TR_PCISCNode *cmah)
   {
   TR_PCISCNode *n0;
   TR_PCISCNode *parentIndex;
   if (ctrl & CISCUtilCtl_64Bit)
      {
      if (ctrl & CISCUtilCtl_NoI2L)
         {
         n0  = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::lsub, TR::Int64,    tgt->incNumNodes(),  dagId,   1,   2, pred); tgt->addNode(n0);
         parentIndex = n0;
         }
      else
         {
         parentIndex = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::i2l, TR::Int64,     tgt->incNumNodes(),  dagId,   1,   1, pred); tgt->addNode(parentIndex);
         n0  = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::lsub, TR::Int64,    tgt->incNumNodes(),  dagId,   1,   2, parentIndex); tgt->addNode(n0);
         n0->setChild(parentIndex);
         n0->setIsChildDirectlyConnected();
         }
      }
   else
      {
      n0  = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::isub, TR::Int32,    tgt->incNumNodes(),  dagId,   1,   2, pred); tgt->addNode(n0);
      parentIndex = n0;
      }
   parentIndex->setChild(index);
   if ((ctrl & CISCUtilCtl_ChildDirectConnected) || index->getOpcode() == TR_variable || index->getOpcode() == TR_arrayindex)
      parentIndex->setIsChildDirectlyConnected();
   n0->setChild(1, cmah);
   return n0;
   }

//*****************************************************************************************
// It creates an effective address tree for a byte array. (e.g. base+(index-header))
// It may insert I2L depending on the given flag.
//
// "InLoop" means that it sets the given dagId to all nodes in this subroutine.
//*****************************************************************************************
TR_PCISCNode *
createIdiomByteArrayAddressInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *base, TR_PCISCNode *index, TR_PCISCNode *cmah)
   {
   TR_PCISCNode *n0, *n1;

   n0 = createIdiomByteArrayAddressIndexTreeInLoop(tgt, ctrl, dagId, pred, index, cmah);
   n1  = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), (ctrl & CISCUtilCtl_64Bit) ? TR::aladd : TR::aiadd,  TR::Address, tgt->incNumNodes(),  dagId,   1,   2, n0); tgt->addNode(n1);
   n1->setChildren(base, n0);
   if (base->getOpcode() == TR_variable || base->getOpcode() == TR_arraybase)
      n1->setIsChildDirectlyConnected();
   return n1;
   }
#endif

//*****************************************************************************************
// It creates an address tree of "index part" for a non-byte array. (e.g. index*4-header)
// It may insert I2L depending on the given flag.
//
// "InLoop" means that it sets the given dagId to all nodes in this subroutine.
//*****************************************************************************************
TR_PCISCNode *
createIdiomArrayAddressIndexTreeInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *index, TR_PCISCNode *cmah, TR_PCISCNode *constForMul)
   {
   TR_PCISCNode *n0, *mul;
   if (ctrl & CISCUtilCtl_64Bit)
      {
      TR_PCISCNode *i2l;
      if (ctrl & CISCUtilCtl_NoI2L)
         {
         mul = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::lmul, TR::Int64,     tgt->incNumNodes(),  dagId,   1,   2, pred); tgt->addNode(mul);
         i2l = mul;
         }
      else
         {
         i2l = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::i2l, TR::Int64,     tgt->incNumNodes(),  dagId,   1,   1, pred); tgt->addNode(i2l);
         mul = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::lmul, TR::Int64,    tgt->incNumNodes(),  dagId,   1,   2, i2l); tgt->addNode(mul);
         mul->setIsChildDirectlyConnected();
         mul->setChild(i2l);
         }
      n0  = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::lsub, TR::Int64,    tgt->incNumNodes(),  dagId,   1,   2, mul); tgt->addNode(n0);
      i2l->setChild(index);
      switch(index->getOpcode())
         {
         case TR_variable:
         case TR_arrayindex:
            i2l->setIsChildDirectlyConnected();
            break;
         }
      n0->setIsChildDirectlyConnected();
      }
   else
      {
      mul = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::imul, TR::Int32,    tgt->incNumNodes(),  dagId,   1,   2, pred); tgt->addNode(mul);
      n0  = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::isub, TR::Int32,    tgt->incNumNodes(),  dagId,   1,   2, mul); tgt->addNode(n0);
      mul->setChild(index);
      n0->setIsChildDirectlyConnected();
      switch(index->getOpcode())
         {
         case TR_variable:
         case TR_arrayindex:
            mul->setIsChildDirectlyConnected();
            break;
         }
      }
   mul->setChild(1, constForMul);
   n0->setChildren(mul, cmah);
   return n0;
   }

//*****************************************************************************************
// It creates an effective address tree for a non-byte array. (e.g. base+(index*4-header))
// It may insert I2L depending on the given flag.
//
// "InLoop" means that it sets the given dagId to all nodes in this subroutine.
//*****************************************************************************************
TR_PCISCNode *
createIdiomArrayAddressInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *base, TR_PCISCNode *index, TR_PCISCNode *cmah, TR_PCISCNode *constForMul)
   {
   TR_PCISCNode *n0, *n1;
   n0 = createIdiomArrayAddressIndexTreeInLoop(tgt, ctrl, dagId, pred, index, cmah, constForMul);
   n1  = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), (ctrl & CISCUtilCtl_64Bit) ? TR::aladd : TR::aiadd, TR::Address,   tgt->incNumNodes(),  dagId,   1,   2, n0); tgt->addNode(n1);
   n1->setChildren(base, n0);
   if (base->getOpcode() == TR_variable || base->getOpcode() == TR_arraybase)
      n1->setIsChildDirectlyConnected();
   return n1;
   }

//*****************************************************************************************
// It creates an effective address tree for an array using the given index tree. (e.g. base+indexTree)
//
// "InLoop" means that it sets the given dagId to all nodes in this subroutine.
//*****************************************************************************************
TR_PCISCNode *
createIdiomArrayAddressInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *base, TR_PCISCNode *indexTree)
   {
   TR_PCISCNode *n1;

   n1  = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), (ctrl & CISCUtilCtl_64Bit) ? TR::aladd : TR::aiadd, TR::Address, tgt->incNumNodes(),  dagId,   1,   2, pred); tgt->addNode(n1);
   n1->setChildren(base, indexTree);
   return n1;
   }

#if 0
//*****************************************************************************************
// It creates a byte array load.
// "InLoop" means that it sets the given dagId to all nodes in this subroutine.
//*****************************************************************************************
TR_PCISCNode *
createIdiomByteArrayLoadInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *base, TR_PCISCNode *index, TR_PCISCNode *cmah)
   {
   TR_PCISCNode *n1, *n2;
   n1 = createIdiomByteArrayAddressInLoop(tgt, ctrl, dagId, pred, base, index, cmah);
   n2  = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::bloadi, TR::Int8,  tgt->incNumNodes(),  dagId,   1,   1, n1); tgt->addNode(n2);

   n2->setChild(n1);
   n2->setIsChildDirectlyConnected();
   return n2;
   }

//*****************************************************************************************
// It creates a byte array store from the given address and storeval trees.
// "InLoop" means that it sets the given dagId to all nodes in this subroutine.
//*****************************************************************************************
TR_PCISCNode *
createIdiomByteArrayStoreBodyInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *addr, TR_PCISCNode *storeval)
   {
   TR_PCISCNode *n2, *i2b;
   if (ctrl & CISCUtilCtl_NoConversion)
      {
      i2b = storeval;
      }
   else
      {
      i2b = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), (ctrl & CISCUtilCtl_AllConversion) ? TR_conversion : (TR_CISCOps)TR::i2b,
                                              TR::Int8,
                                              tgt->incNumNodes(),  dagId,   1,   1, pred); tgt->addNode(i2b);
      pred = i2b;
      i2b->setChild(storeval);
      }

   n2  = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::bstorei, TR::Int8, tgt->incNumNodes(),  dagId,   1,   2, pred); tgt->addNode(n2);
   n2->setChildren(addr, i2b);
   return n2;
   }

//*****************************************************************************************
// It creates a byte array store from the given base, index, cmah, and storeval trees.
// "InLoop" means that it sets the given dagId to all nodes in this subroutine.
//*****************************************************************************************
TR_PCISCNode *
createIdiomByteArrayStoreInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *base, TR_PCISCNode *index, TR_PCISCNode *cmah, TR_PCISCNode *storeval)
   {
   TR_PCISCNode *n1, *n2;
   n1 = createIdiomByteArrayAddressInLoop(tgt, ctrl, dagId, pred, base, index, cmah);
   n2 = createIdiomByteArrayStoreBodyInLoop(tgt, ctrl, dagId, n1, n1, storeval);
   n2->setIsChildDirectlyConnected();
   return n2;
   }
#endif

//*****************************************************************************************
// It creates a char array load.
// "InLoop" means that it sets the given dagId to all nodes in this subroutine.
//*****************************************************************************************
TR_PCISCNode *
createIdiomCharArrayLoadInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *base, TR_PCISCNode *index, TR_PCISCNode *cmah, TR_PCISCNode *const2)
   {
   return createIdiomArrayLoadInLoop(tgt, ctrl, dagId, pred, TR::cloadi, TR::Int16, base, index, cmah, const2);
   }

//*****************************************************************************************
// It creates a non-byte array load.
// "InLoop" means that it sets the given dagId to all nodes in this subroutine.
//*****************************************************************************************
TR_PCISCNode *
createIdiomArrayLoadInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, int opcode, TR::DataType dataType, TR_PCISCNode *base, TR_PCISCNode *index, TR_PCISCNode *cmah, TR_PCISCNode *mulconst)
   {
   TR_PCISCNode *n1, *n2;
   n1 = createIdiomArrayAddressInLoop(tgt, ctrl, dagId, pred, base, index, cmah, mulconst);
   n2  = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), opcode, dataType, tgt->incNumNodes(),  dagId,   1,   1, n1); tgt->addNode(n2);

   n2->setChild(n1);
   n2->setIsChildDirectlyConnected();
   return n2;
   }

//*****************************************************************************************
// It creates a non-byte array store from the given address and storeval trees.
// "InLoop" means that it sets the given dagId to all nodes in this subroutine.
//*****************************************************************************************
TR_PCISCNode *
createIdiomArrayStoreBodyInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, int opcode, TR::DataType dataType, TR_PCISCNode *addr, TR_PCISCNode *storeval)
   {
   TR_PCISCNode *n2, *i2c;
   if (ctrl & CISCUtilCtl_NoConversion)
      {
      i2c = storeval;
      }
   else
      {
      i2c = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(),
                                              (opcode == TR::cstorei) ? (TR_CISCOps)TR::i2s : TR_conversion,
                                              (opcode == TR::cstorei) ? TR::Int16 : TR::NoType,
                                              tgt->incNumNodes(),  dagId,   1,   1, pred); tgt->addNode(i2c);
      i2c->setIsOptionalNode();
      pred = i2c;
      i2c->setChild(storeval);
      }
   n2  = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), opcode, dataType, tgt->incNumNodes(),  dagId,   1,   2, pred); tgt->addNode(n2);
   n2->setChildren(addr, i2c);
   return n2;
   }

//*****************************************************************************************
// It creates a char array store from the given address and storeval trees.
// "InLoop" means that it sets the given dagId to all nodes in this subroutine.
//*****************************************************************************************
TR_PCISCNode *
createIdiomCharArrayStoreBodyInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *addr, TR_PCISCNode *storeval)
   {
   return createIdiomArrayStoreBodyInLoop(tgt, ctrl, dagId, pred, TR::cstorei, TR::Int16, addr, storeval);
   }

//*****************************************************************************************
// It creates a char array store from the given base, index, cmah, and storeval trees.
// "InLoop" means that it sets the given dagId to all nodes in this subroutine.
//*****************************************************************************************
TR_PCISCNode *
createIdiomArrayStoreInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, int opcode, TR::DataType dataType,  TR_PCISCNode *base, TR_PCISCNode *index, TR_PCISCNode *cmah, TR_PCISCNode *const2, TR_PCISCNode *storeval)
   {
   TR_PCISCNode *n1, *n2;
   n1 = createIdiomArrayAddressInLoop(tgt, ctrl, dagId, pred, base, index, cmah, const2);
   n2 = createIdiomArrayStoreBodyInLoop(tgt, ctrl, dagId, n1, opcode, dataType, n1, storeval);
   n2->setIsChildDirectlyConnected();
   return n2;
   }

//*****************************************************************************************
// It creates a char array store from the given base, index, cmah, and storeval trees.
// "InLoop" means that it sets the given dagId to all nodes in this subroutine.
//*****************************************************************************************
TR_PCISCNode *
createIdiomCharArrayStoreInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *base, TR_PCISCNode *index, TR_PCISCNode *cmah, TR_PCISCNode *const2, TR_PCISCNode *storeval)
   {
   return createIdiomArrayStoreInLoop(tgt, ctrl, dagId, pred, TR::cstorei, TR::Int16, base, index, cmah, const2, storeval);
   }

//*****************************************************************************************
// It creates a byte array load particularly for "sun/io/CharToByteSingleByte.JITintrinsicConvert".
// The base address is given via java.nio.DirectByteBuffer.
//
// "InLoop" means that it sets the given dagId to all nodes in this subroutine.
//*****************************************************************************************
TR_PCISCNode *
createIdiomByteDirectArrayLoadInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *base, TR_PCISCNode *index)
   {
   TR_PCISCNode *n2;
   TR_PCISCNode *n0;
   TR_PCISCNode *parentIndex;
   if (ctrl & CISCUtilCtl_64Bit)
      {
      if (ctrl & CISCUtilCtl_NoI2L)
         {
         n0  = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::ladd, TR::Int64,    tgt->incNumNodes(),  dagId,   1,   2, pred); tgt->addNode(n0);
         parentIndex = n0;
         }
      else
         {
         parentIndex = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::i2l, TR::Int64,     tgt->incNumNodes(),  dagId,   1,   1, pred); tgt->addNode(parentIndex);
         n0  = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::ladd, TR::Int64,    tgt->incNumNodes(),  dagId,   1,   2, parentIndex); tgt->addNode(n0);
         n0->setChild(parentIndex);
         n0->setIsChildDirectlyConnected();
         }
      parentIndex->setChild(index);
      if ((ctrl & CISCUtilCtl_ChildDirectConnected) || index->getOpcode() == TR_variable || index->getOpcode() == TR_arrayindex)
         parentIndex->setIsChildDirectlyConnected();
      }
   else
      {
      parentIndex = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::l2i, TR::Int32,      tgt->incNumNodes(),  dagId,   1,   1, pred); tgt->addNode(parentIndex);
      parentIndex->setChild(base);
      parentIndex->setIsChildDirectlyConnected();
      base = parentIndex;
      n0  = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::iadd, TR::Int32,     tgt->incNumNodes(),  dagId,   1,   2, parentIndex); tgt->addNode(n0);
      n0->setChild(index);
      if ((ctrl & CISCUtilCtl_ChildDirectConnected) || index->getOpcode() == TR_variable || index->getOpcode() == TR_arrayindex)
         n0->setIsChildDirectlyConnected();
      }
   n0->setChild(1, base);
   n2  = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::bloadi, TR::Int8,   tgt->incNumNodes(),  dagId,   1,   1, n0); tgt->addNode(n2);

   n2->setChild(n0);
   n2->setIsChildDirectlyConnected();
   return n2;
   }

//*****************************************************************************************
// It creates the idiom for "divide by 10" or the corresponding multiplication idiom
// based on the given flag.
//
// "InLoop" means that it sets the given dagId to all nodes in this subroutine.
//*****************************************************************************************
TR_PCISCNode *
createIdiomIDiv10InLoop(TR_PCISCGraph *tgt, int32_t ctrl, bool isDiv2Mul, int dagId, TR_PCISCNode *pred, TR_PCISCNode *src1, TR_PCISCNode *src2, TR_PCISCNode *c2, TR_PCISCNode *c31)
   {
   TR_PCISCNode *ret;
   if (isDiv2Mul)
      {
      TR_PCISCNode *nmul= new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::imulh, TR::Int32    ,tgt->incNumNodes(),  1,   1,   2,   pred, src1, src2);   tgt->addNode(nmul);
      TR_PCISCNode *nshr= new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::ishr, TR::Int32     ,tgt->incNumNodes(),  1,   1,   2,   nmul, nmul, c2);   tgt->addNode(nshr);
      TR_PCISCNode *ushr= new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::iushr, TR::Int32    ,tgt->incNumNodes(),  1,   1,   2,   nshr, src1, c31);   tgt->addNode(ushr);   // optional
      ret = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::iadd, TR::Int32     ,tgt->incNumNodes(),  1,   1,   2,   ushr, nshr, ushr);   tgt->addNode(ret);                 // optional
      ushr->setIsOptionalNode();
      ushr->setSkipParentsCheck();
      ret->setIsOptionalNode();
      }
   else
      {
      ret = new (PERSISTENT_NEW) TR_PCISCNode(tgt->trMemory(), TR::idiv, TR::Int32     ,tgt->incNumNodes(),  1,   1,   2,   pred, src1, src2);   tgt->addNode(ret);
      }
   return ret;
   }


//*****************************************************************************************
// Search the node "target" starting from "start" within this basic block
//*****************************************************************************************
bool
searchNodeInBlock(TR_CISCNode *start,
                  TR_CISCNode *target)
   {
   TR_CISCNode *t = start;

   while (t->getNumSuccs() == 1 && t->getPreds()->isSingleton())
      {
      if (t == target) return true;
      t = t->getSucc(0);
      if (t == start) break;    // maybe, this condition never happen.
      }

   return false;
   }


//*****************************************************************************************
// Check if all successors of t will reach any node included in pBV.
//*****************************************************************************************
bool
checkSuccsSet(TR_CISCTransformer *const trans,
              TR_CISCNode *t,
              TR_BitVector *const pBV)
   {
   List<TR_CISCNode> *T2P = trans->getT2P();
   TR_CISCNode *p;

   while (t->getNumSuccs() == 1)
      {
      t = t->getSucc(0);
      if (!t->isNegligible())
         {
         ListIterator<TR_CISCNode> li(T2P + t->getID());
         for (p = li.getFirst(); p; p = li.getNext()) if (pBV->isSet(p->getID())) return true;   // found the pattern node in pBV
         return false; // cannot find the pattern node.
         }
      }

   for (int i = t->getNumSuccs(); --i >= 0; )
      {
      TR_CISCNode *tn = t->getSucc(i);
      if (!tn->isNegligible())
         {
         ListIterator<TR_CISCNode> li(T2P + tn->getID());
         for (p = li.getFirst(); p; p = li.getNext()) if (pBV->isSet(p->getID())) break;   // found the pattern node in pBV
         if (!p) return false; // cannot find the pattern node.
         }
      else
         {
         if (!checkSuccsSet(trans, tn, pBV)) return false;
         }
      }
   return true;
   }


//*****************************************************************************************
// It searches an iload in the tree "q", and it returns the iload
//*****************************************************************************************
static TR_CISCNode *
searchIload(TR_CISCNode *q, bool allowArrayIndex)
   {
   while(true)
      {
      bool fail = false;
      if (q->getOpcode() == TR::i2l)
         {
         q = q->getChild(0);      // allow only TR::iload or TR_variable
         fail = true;
         }
      if (q->getOpcode() == TR::iload || q->getOpcode() == TR_variable) return q;
      if (allowArrayIndex && q->getOpcode() == TR_arrayindex) return q;
      if (fail || q->getOpcode() == TR::lload || q->getNumChildren() < 1) return 0;
      q = q->getChild(0);
      }
   return 0;
   }


//*****************************************************************************************
// It searches an array load in the tree "top", and it returns the type of an array load,
// a base variable, and an index variable.
//*****************************************************************************************
bool
getThreeNodesForArray(TR_CISCNode *top, TR_CISCNode **ixloadORstore, TR_CISCNode **aload, TR_CISCNode **iload, bool allowArrayIndex)
   {
   TR_CISCNode *p;

   // search ixloadORstore
   p = top;
   while(true)
      {
      if (p->getNumChildren() == 0) return false;
      if (p->getIlOpCode().isLoadIndirect() || p->getIlOpCode().isStoreIndirect() ||
          p->getOpcode() == TR_inbload || p->getOpcode() == TR_inbstore ||
          p->getOpcode() == TR_indload || p->getOpcode() == TR_indstore ||
          p->getOpcode() == TR_ibcload || p->getOpcode() == TR_ibcstore)
         {
         *ixloadORstore = p;
         break;
         }
      p = p->getChild(0);
      }

   p = p->getChild(0);

   TR_CISCNode *q;
   switch(p->getOpcode())
      {
      case TR::aladd:
      case TR::aiadd:
         // search aload
         q = p->getChild(0);
         while(true)
            {
            if (q->getOpcode() == TR::aload || q->getOpcode() == TR_variable || q->getOpcode() == TR_arraybase)
               {
               *aload = q;
               break;
               }
            if (q->getNumChildren() != 1) return false;
            q = q->getChild(0);
            }

         // search iload
         if ((q = searchIload(p->getChild(1), allowArrayIndex)) == 0) return false;
         *iload = q;
         break;

      case TR::ladd:
      case TR::iadd:
         // search iload
         if (q = searchIload(p->getChild(1), allowArrayIndex))
            {
            *iload = q;
            q = p->getChild(0);
            }
         else if (q = searchIload(p->getChild(0), allowArrayIndex))
            {
            *iload = q;
            q = p->getChild(1);
            }
         else
            {
            return false;
            }

         // search aload
         while(true)
            {
            if (q->getOpcode() == TR::lload || q->getOpcode() == TR_variable)
               {
               *aload = q;
               break;
               }
            if (q->getOpcode() == TR::iload || q->getNumChildren() != 1) return false;
            q = q->getChild(0);
            }
         break;

      default:
         return false;
      }
   return true;
   }


//*****************************************************************************************
// It returns isDcrement and modification length by analyzing if-opcode
//*****************************************************************************************
bool
testExitIF(int opcode, bool *isDecrement, int32_t *modLength, int32_t *modStartIdx)
   {
   switch(opcode)
      {
      case TR::ificmplt:
         if (isDecrement) *isDecrement = true;
         if (modLength) *modLength = 1;
         if (modStartIdx) *modStartIdx = 0;
         return true;
      case TR::ificmple:
         if (isDecrement) *isDecrement = true;
         if (modLength) *modLength = 0;
         if (modStartIdx) *modStartIdx = 1;
         return true;
      case TR::ificmpgt:
         if (isDecrement) *isDecrement = false;
         if (modLength) *modLength = 1;
         if (modStartIdx) *modStartIdx = 0;
         return true;
      case TR::ificmpge:
         if (isDecrement) *isDecrement = false;
         if (modLength) *modLength = 0;
         if (modStartIdx) *modStartIdx = 0;
         return true;
      }
   return false;
   }


/********************************************************************/
/************ Utilities for analyzing or making TR::Node *************/
/********************************************************************/

//*****************************************************************************************
// It returns the n'th child is "iconst value".
//*****************************************************************************************
bool
testIConst(TR_CISCNode *n, int idx, int32_t value)
   {
   TR_CISCNode *ch = n->getChild(idx);
   return (ch->getOpcode() == TR::iconst && ch->getOtherInfo() == value);
   }


//*****************************************************************************************
// It inserts i2l based on the given flag.
//*****************************************************************************************
TR::Node*
createI2LIfNecessary(TR::Compilation *comp, bool is64bit, TR::Node *child)
   {
   return is64bit ? TR::Node::create(TR::i2l, 1, child) : child;
   }


//*****************************************************************************************
// It converts from store to load.
//*****************************************************************************************
TR::Node*
createLoad(TR::Node *baseNode)
   {
   return baseNode->getOpCode().isStoreDirect() ? TR::Node::createLoad(baseNode, baseNode->getSymbolReference()) :
                                                  baseNode->duplicateTree();
   }


//*****************************************************************************************
// It creates a load instruction, and it inserts i2l based on the given flag.
//*****************************************************************************************
TR::Node*
createLoadWithI2LIfNecessary(TR::Compilation *comp, bool is64bit, TR::Node *indexNode)
   {
   TR_ASSERT(indexNode->getOpCode().hasSymbolReference(), "parameter error");
   TR::Node *iload = createLoad(indexNode);
   return createI2LIfNecessary(comp, is64bit, iload);
   }


//*****************************************************************************************
// It creates a node of the constant value of the array header size.
//*****************************************************************************************
TR::Node*
createArrayHeaderConst(TR::Compilation *comp, bool is64bit, TR::Node *baseNode)
   {
   TR::Node *c2;
   if (is64bit)
      {
      c2 = TR::Node::create(baseNode, TR::lconst, 0);
      c2->setLongInt(-(int32_t)TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
      }
   else
      {
      c2 = TR::Node::create(baseNode, TR::iconst, 0, -(int32_t)TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
      }
   return c2;
   }


//*****************************************************************************************
// It creates an effective address tree for the top of the given array.
//*****************************************************************************************
TR::Node*
createArrayTopAddressTree(TR::Compilation *comp, bool is64bit, TR::Node *baseNode)
   {
   TR::Node *top, *c2;
   TR::Node *aload = createLoad(baseNode);
   if (is64bit)
      {
      top = TR::Node::create(baseNode, TR::aladd, 2);
      c2 = TR::Node::create(baseNode, TR::lconst, 0);
      c2->setLongInt((int32_t)TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
      }
   else
      {
      top = TR::Node::create(baseNode, TR::aiadd, 2);
      c2 = TR::Node::create(baseNode, TR::iconst, 0, (int32_t)TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
      }
   top->setAndIncChild(0, aload);
   top->setAndIncChild(1, c2);
   return top;
   }


//*****************************************************************************************
// It converts from store to load, and it inserts i2l based on the given flag.
//*****************************************************************************************
TR::Node*
convertStoreToLoadWithI2LIfNecessary(TR::Compilation *comp, bool is64bit, TR::Node *indexNode)
   {
   return (indexNode->getOpCode().isStoreDirect()) ?
      createLoadWithI2LIfNecessary(comp, is64bit, indexNode) :
      createI2LIfNecessary(comp, is64bit, indexNode->getReferenceCount() ?
                                          indexNode->duplicateTree() : indexNode);
   }


//*****************************************************************************************
// It converts from store to load.
//*****************************************************************************************
TR::Node*
convertStoreToLoad(TR::Compilation *comp, TR::Node *indexNode)
   {
   return (indexNode->getOpCode().isStoreDirect()) ?
      TR::Node::createLoad(indexNode, indexNode->getSymbolReference()) :
      indexNode->getReferenceCount() ? indexNode->duplicateTree() : indexNode;
   }


//*****************************************************************************************
// It creates the byte size from an element size. (i.e. ret = index * multiply)
//*****************************************************************************************
TR::Node*
createBytesFromElement(TR::Compilation *comp, bool is64bit, TR::Node *indexNode, int multiply)
   {
   TR::Node *top, *c2;
   top = convertStoreToLoadWithI2LIfNecessary(comp, is64bit, indexNode);

   if (is64bit)
      {
      if (multiply > 1)
         {
         c2 = TR::Node::create(indexNode, TR::lconst, 0); // c2 is used temporary
         c2->setLongInt(multiply);
         top = TR::Node::create(TR::lmul, 2, top, c2);
         }
      }
   else
      {
      if (multiply > 1)
         {
         c2 = TR::Node::create(indexNode, TR::iconst, 0, multiply); // c2 is used temporary
         top = TR::Node::create(TR::imul, 2, top, c2);
         }
      }
   return top;
   }


//*****************************************************************************************
// It creates an index address tree for the given indexNode and size.
//*****************************************************************************************
TR::Node*
createIndexOffsetTree(TR::Compilation *comp, bool is64bit, TR::Node *indexNode, int multiply)
   {
   TR::Node *top, *c1, *c2;
   c1 = createBytesFromElement(comp, is64bit, indexNode, multiply);

   if (is64bit)
      {
      c2 = TR::Node::create(indexNode, TR::lconst, 0);
      c2->setLongInt(-(int32_t)TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
      top = TR::Node::create(indexNode, TR::lsub, 2);
      }
   else
      {
      c2 = TR::Node::create(indexNode, TR::iconst, 0, -(int32_t)TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
      top = TR::Node::create(indexNode, TR::isub, 2);
      }
   top->setAndIncChild(0, c1);
   top->setAndIncChild(1, c2);
   return top;
   }


//*****************************************************************************************
// It creates an effective address tree for the given baseNode, indexNode and size.
//*****************************************************************************************
TR::Node*
createArrayAddressTree(TR::Compilation *comp, bool is64bit, TR::Node *baseNode, TR::Node *indexNode, int multiply)
   {
   if (indexNode->getOpCodeValue() == TR::iconst && indexNode->getInt() == 0)
      {
      return createArrayTopAddressTree(comp, is64bit, baseNode);
      }
   else
      {
      TR::Node *top, *c2;
      TR::Node *aload = createLoad(baseNode);
      c2 = createIndexOffsetTree(comp, is64bit, indexNode, multiply);
      top = TR::Node::create(baseNode, is64bit ? TR::aladd : TR::aiadd, 2);
      top->setAndIncChild(0, aload);
      top->setAndIncChild(1, c2);
      return top;
      }
   }


//*****************************************************************************************
// It creates an array load tree for the given load type, baseNode, indexNode and size.
//*****************************************************************************************
TR::Node*
createArrayLoad(TR::Compilation *comp, bool is64bit, TR::Node *ixload, TR::Node *baseNode, TR::Node *indexNode, int multiply)
   {
   TR::Node *top, *c1;

   if (comp->useCompressedPointers() &&
          (ixload->getDataType() == TR::Address))
      multiply = multiply >> 1;

   c1 = createArrayAddressTree(comp, is64bit, baseNode, indexNode, multiply);
   TR::SymbolReference *symref=ixload->getSymbolReference();
   top = TR::Node::createWithSymRef(ixload, ixload->getOpCodeValue(), 1, symref);
   top->setAndIncChild(0, c1);
   return top;
   }


//*****************************************************************************************
// It replaces the array index in the address tree.
//*****************************************************************************************
TR::Node *
replaceIndexInAddressTree(TR::Compilation *comp, TR::Node *tree, TR::SymbolReference *symRef, TR::Node *newNode)
   {
   TR::Node *orgTree = tree;
   if (tree->getOpCode().isIndirect()) tree = tree->getChild(0);
   if (tree->getOpCode().isAdd())
      {
      tree = tree->getChild(1);
      while(true)
         {
         TR::Node *parent = tree;
         if (tree->getOpCodeValue() == TR::iadd)
            {
            TR::Node *ch1 = tree->getChild(1);
            if (ch1->getOpCodeValue() == TR::iload)
               {
               if (ch1->getSymbolReference() == symRef)
                  {
                  parent->getAndDecChild(1);
                  parent->setAndIncChild(1, newNode);
                  return orgTree;
                  }
               }
            }

         tree = tree->getChild(0);
         if (!tree) break;
         if (tree->getOpCodeValue() == TR::iload)
            {
            if (tree->getSymbolReference() == symRef)
               {
               parent->getAndDecChild(0);
               parent->setAndIncChild(0, newNode);
               return orgTree;
               }
            else
               {
               return NULL;
               }
            }
         }
      }
   return NULL;
   }

//*****************************************************************************************
// It modifies the array header constant in the given tree
//*****************************************************************************************
TR::Node *
modifyArrayHeaderConst(TR::Compilation *comp, TR::Node *tree, int32_t offset)
   {
   if (offset == 0) return tree;
   if (!tree->getOpCode().isAdd()) tree = tree->getFirstChild();
   if (tree->getOpCodeValue() == TR::aiadd || tree->getOpCodeValue() == TR::aladd)
      {
      TR::Node *addOrSubNode = tree->getSecondChild();
      TR::Node *constLoad = addOrSubNode->getSecondChild();
      if (addOrSubNode->getOpCode().isSub())
         {
         offset = -offset;
         }
      else if (!addOrSubNode->getOpCode().isAdd())
         {
         return NULL;   // failed
         }
      switch(constLoad->getOpCodeValue())
         {
         case TR::iconst:
            constLoad->setInt(constLoad->getInt() + offset);
            return constLoad;
            break;
         case TR::lconst:
            constLoad->setLongInt(constLoad->getLongInt() + offset);
            return constLoad;
            break;
         default:
            return NULL;
         }
      }
   return NULL;
   }

//*****************************************************************************************
// It creates a table load node especially for function tables for TRT or TRxx
//*****************************************************************************************
TR::Node*
createTableLoad(TR::Compilation *comp, TR::Node* repNode, uint8_t inputSize, uint8_t outputSize, void *array, bool dispTrace)
   {
   TR_SetTranslateTable setTable(comp, inputSize, outputSize, array,
                                 TR_TranslateTable::tableSize(inputSize, outputSize));
   TR::SymbolReference *tableSymRef = setTable.createSymbolRef();
   if (dispTrace)
      setTable.dumpTable();
   return TR::Node::createWithSymRef(repNode, TR::loadaddr, 0, tableSymRef);
   }

//*****************************************************************************************
// It basically creates "ch1 op2 ch2", but it performs a simple constant folding.
//*****************************************************************************************
TR::Node*
createOP2(TR::Compilation *comp, TR::ILOpCodes op2, TR::Node* ch1, TR::Node* ch2)
   {
   TR::Node* op2Node;
   bool done = false;
   if (ch2->getOpCodeValue() == TR::iconst)
      {
      int value = ch2->getInt();
      int32_t newVal;
      switch(op2)
         {
         case TR::iadd:
         case TR::isub:
            if (value == 0)
               {
               op2Node = ch1;
               done = true;
               }
            else if (ch1->getOpCodeValue() == TR::iconst)
               {
               newVal =  (op2 == TR::iadd) ? (ch1->getInt() + ch2->getInt()) :
                                            (ch1->getInt() - ch2->getInt());
               op2Node = TR::Node::create(ch1, TR::iconst, 0, newVal);
               done = true;
               }
            break;
         case TR::imul:
         case TR::idiv:
            if (value == 1)
               {
               op2Node = ch1;
               done = true;
               }
            else if (ch1->getOpCodeValue() == TR::iconst)
               {
               if (ch2->getInt() == 0 && op2 == TR::idiv)
                  break;  // divide by 0
               newVal =  (op2 == TR::imul) ? (ch1->getInt() * ch2->getInt()) :
                                            (ch1->getInt() / ch2->getInt());
               op2Node = TR::Node::create(ch1, TR::iconst, 0, newVal);
               done = true;
               }
            break;
         default:
         	break;
         }
      }
   if (!done) op2Node = TR::Node::create(op2, 2, ch1, ch2);
   return op2Node;
   }

//*****************************************************************************************
// It creates like the following tree "storeNode = ch1 op2 ch2"
//*****************************************************************************************
TR::Node*
createStoreOP2(TR::Compilation *comp, TR::Node* storeNode, TR::ILOpCodes op2, TR::Node* ch1, TR::Node* ch2)
   {
   TR::Node* op2Node = createOP2(comp, op2, ch1, ch2);
   storeNode->setAndIncChild(0, op2Node);
   return storeNode;
   }

//*****************************************************************************************
// It creates like the following tree "storeNode = ch1 op2 ch2" from the given symbol references.
//*****************************************************************************************
TR::Node*
createStoreOP2(TR::Compilation *comp, TR::SymbolReference* store, TR::ILOpCodes op2, TR::SymbolReference* ch1, TR::Node* ch2, TR::Node *rep)
   {
   return createStoreOP2(comp,
                         TR::Node::createWithSymRef(rep, comp->il.opCodeForDirectStore(store->getSymbol()->getDataType()), 1, store),
                         op2,
                         TR::Node::createWithSymRef(rep, comp->il.opCodeForDirectLoad(ch1->getSymbol()->getDataType()), 0, ch1),
                         ch2);
   }

//*****************************************************************************************
// It creates like the following tree "storeNode = ch1 op2 ch2" from the given symbol references.
//*****************************************************************************************
TR::Node*
createStoreOP2(TR::Compilation *comp, TR::SymbolReference* store, TR::ILOpCodes op2, TR::SymbolReference* ch1, TR::SymbolReference* ch2, TR::Node *rep)
   {
   return createStoreOP2(comp, store, op2, ch1,
                         TR::Node::createWithSymRef(rep, comp->il.opCodeForDirectLoad(ch2->getSymbol()->getDataType()), 0, ch2),
                         rep);
   }

//*****************************************************************************************
// It creates like the following tree "storeNode = ch1 op2 ch2" from the given symbol
// references and constant value.
//*****************************************************************************************
TR::Node*
createStoreOP2(TR::Compilation *comp, TR::SymbolReference* store, TR::ILOpCodes op2, TR::SymbolReference* ch1, int ch2Const, TR::Node *rep)
   {
   return createStoreOP2(comp, store, op2, ch1,
                         TR::Node::create( rep, TR::iconst, 0, ch2Const),
                         rep);
   }


//*****************************************************************************************
// It creates "min(x, y)", whose tree is x+(((y-x)>>31)&(y-x)).
//*****************************************************************************************
TR::Node*
createMin(TR::Compilation *comp, TR::Node* x, TR::Node* y)
   {
   if (x->getOpCodeValue() == TR::iconst &&
       y->getOpCodeValue() == TR::iconst)
      {
      return TR::Node::create(x, TR::iconst, 0, std::min(x->getInt(), y->getInt()));
      }
   else
      {
      TR::Node *y_x   = TR::Node::create(TR::isub, 2, y, x);       // y-x
      TR::Node *shift = TR::Node::create(TR::ishr, 2, y_x,
                                       TR::Node::create(y_x, TR::iconst, 0, 31));    // y_x >> 31
      TR::Node *andop = TR::Node::create(TR::iand, 2, shift, y_x); // shift & y_x
      TR::Node *ret   = TR::Node::create(TR::iadd, 2, x, andop);   // x + andop
      return ret;
      }
   }


//*****************************************************************************************
// It creates "max(x, y)", whose tree is x-(((x-y)>>31)&(x-y)).
//*****************************************************************************************
TR::Node*
createMax(TR::Compilation *comp, TR::Node* x, TR::Node* y)
   {
   if (x->getOpCodeValue() == TR::iconst &&
       y->getOpCodeValue() == TR::iconst)
      {
      return TR::Node::create(x, TR::iconst, 0, std::max(x->getInt(), y->getInt()));
      }
   else
      {
      TR::Node *x_y   = TR::Node::create(TR::isub, 2, x, y);       // x-y
      TR::Node *shift = TR::Node::create(TR::ishr, 2, x_y,
                                       TR::Node::create(x_y, TR::iconst, 0, 31));    // x_y >> 31
      TR::Node *andop = TR::Node::create(TR::iand, 2, shift, x_y); // shift & x_y
      TR::Node *ret   = TR::Node::create(TR::isub, 2, x, andop);   // x - andop
      return ret;
      }
   }


//*****************************************************************************************
// It will return a constant load in the target graph corresponding to the pattern node mulConst.
// If mulConst is NULL, *elementSize will be set to 1.
// Otherwise, *elementSize will be set to the value of the constant, and *multiplier will be set
// to that TR::Node.
//*****************************************************************************************
bool
getMultiplier(TR_CISCTransformer *trans, TR_CISCNode *mulConst, TR::Node **multiplier, int *elementSize, TR::DataType srcNodeType)
   {
   TR::Node *mulFactorNode = 0;
   if (mulConst)
      {
      TR_CISCNode *mulConstT = trans->getP2TRep(mulConst);
      if (!mulConstT->isNewCISCNode())
         mulFactorNode = mulConstT->getHeadOfTrNodeInfo()->_node;
      }
   if (mulFactorNode)
      {
      TR_ASSERT(mulFactorNode->getOpCode().isLoadConst(), "error!");
      switch(mulFactorNode->getOpCodeValue())
         {
         case TR::iconst:
            *elementSize = mulFactorNode->getInt();
            *multiplier = mulFactorNode;
            return true;
         case TR::lconst:
            *elementSize = (int)mulFactorNode->getLongInt();
            *multiplier = mulFactorNode;
            return true;
         default:
            TR_ASSERT(false, "error!");
            return false;
         }
      }
   else
      {
      *multiplier = NULL;
      *elementSize = 1;
      }
   return true;
   }


//*****************************************************************************************
// It will get each representative target node using P2T table for each pattern node.
// The number of pattern nodes is given by "count".
// The result (several TR::Nodes) is set to "array".
//*****************************************************************************************
void
getP2TTrRepNodes(TR_CISCTransformer *trans, TR::Node** array, int count)
   {
   TR_CISCGraph *P = trans->getP();
   ListIterator<TR_CISCNode> pi(P->getOrderByData());
   TR_CISCNode *pn;
   int index = 0;
   for (pn = pi.getFirst(); pn && index < count; pn = pi.getNext(), index++)
      {
      TR_CISCNode *tn = trans->getP2TRepInLoop(pn);
      if (!tn) tn = trans->getP2TRep(pn);
      TR::Node *candidate = NULL;
      if (tn)
         {
         ListIterator<TrNodeInfo> li(tn->getTrNodeInfo());
         candidate = tn->getHeadOfTrNodeInfo()->_node;
         TrNodeInfo *p;
         for (p = li.getFirst(); p; p = li.getNext())
            {
            if (!p->_node->getOpCode().isStoreDirect())
               {
               candidate = p->_node;
               break;
               }
            }
         if (candidate->getOpCode().isStoreDirect())
            {
            ListIterator<TR_CISCNode> pi(tn->getParents());
            TR_CISCNode *p;
            bool noLoad = true;
            for (p = pi.getFirst(); p; p = pi.getNext())
               {
               if (p->isLoadVarDirect()) noLoad = false;
               }
            if (noLoad)
               {
               for (p = pi.getFirst(); p; p = pi.getNext())
                  {
                  if (!p->isOutsideOfLoop() &&
                      p->isStoreDirect() &&
                      p->isNegligible())
                     {
                     trans->getBeforeInsertionList()->add(candidate->duplicateTree());
                     break;
                     }
                  }
               }
            }
         }
      array[index] = candidate;
      }
   }

//*****************************************************************************************
// Get first two representative target nodes.
//*****************************************************************************************
void
getP2TTrRepNodes(TR_CISCTransformer *trans, TR::Node** n1, TR::Node** n2)
   {
   TR::Node* array[2];
   getP2TTrRepNodes(trans, array, 2);
   if (n1) *n1 = array[0];
   if (n2) *n2 = array[1];
   }

//*****************************************************************************************
// Get first three representative target nodes.
//*****************************************************************************************
void
getP2TTrRepNodes(TR_CISCTransformer *trans, TR::Node** n1, TR::Node** n2, TR::Node** n3)
   {
   TR::Node* array[3];
   getP2TTrRepNodes(trans, array, 3);
   if (n1) *n1 = array[0];
   if (n2) *n2 = array[1];
   if (n3) *n3 = array[2];
   }

//*****************************************************************************************
// Get first four representative target nodes.
//*****************************************************************************************
void
getP2TTrRepNodes(TR_CISCTransformer *trans, TR::Node** n1, TR::Node** n2, TR::Node** n3, TR::Node** n4)
   {
   TR::Node* array[4];
   getP2TTrRepNodes(trans, array, 4);
   if (n1) *n1 = array[0];
   if (n2) *n2 = array[1];
   if (n3) *n3 = array[2];
   if (n4) *n4 = array[3];
   }

//*****************************************************************************************
// Get first five representative target nodes.
//*****************************************************************************************
void
getP2TTrRepNodes(TR_CISCTransformer *trans, TR::Node** n1, TR::Node** n2, TR::Node** n3, TR::Node** n4, TR::Node** n5)
   {
   TR::Node* array[5];
   getP2TTrRepNodes(trans, array, 5);
   if (n1) *n1 = array[0];
   if (n2) *n2 = array[1];
   if (n3) *n3 = array[2];
   if (n4) *n4 = array[3];
   if (n5) *n5 = array[4];
   }

//*****************************************************************************************
// Get first six representative target nodes.
//*****************************************************************************************
void
getP2TTrRepNodes(TR_CISCTransformer *trans, TR::Node** n1, TR::Node** n2, TR::Node** n3, TR::Node** n4, TR::Node** n5, TR::Node** n6)
   {
   TR::Node* array[6];
   getP2TTrRepNodes(trans, array, 6);
   if (n1) *n1 = array[0];
   if (n2) *n2 = array[1];
   if (n3) *n3 = array[2];
   if (n4) *n4 = array[3];
   if (n5) *n5 = array[4];
   if (n6) *n6 = array[5];
   }

//*****************************************************************************************
// Get first seven representative target nodes.
//*****************************************************************************************
void
getP2TTrRepNodes(TR_CISCTransformer *trans, TR::Node** n1, TR::Node** n2, TR::Node** n3, TR::Node** n4, TR::Node** n5, TR::Node** n6, TR::Node** n7)
   {
   TR::Node* array[7];
   getP2TTrRepNodes(trans, array, 7);
   if (n1) *n1 = array[0];
   if (n2) *n2 = array[1];
   if (n3) *n3 = array[2];
   if (n4) *n4 = array[3];
   if (n5) *n5 = array[4];
   if (n6) *n6 = array[5];
   if (n7) *n7 = array[6];
   }

//*****************************************************************************************
// Confirm whether only the table[0] is zero and other entries (table[1 to 256]) are non-zero
//*****************************************************************************************
bool
isFitTRTFunctionTable(uint8_t *table)
   {
   if (*table) return false;
   for (int i = 1; i < 256; i++)
      if (table[i] == 0) return false;
   return true;
   }


//*****************************************************************************************
// create table alignment check node for arraytranslate
//*****************************************************************************************
TR::Node*
createTableAlignmentCheck(TR::Compilation *comp, TR::Node *tableNode, bool isByteSource, bool isByteTarget, bool tableBackedByRawStorage)
   {
   int32_t mask = comp->cg()->arrayTranslateTableRequiresAlignment(isByteSource, isByteTarget);
   TR::Node * compareNode = NULL;
   if (mask != 0 && mask != 7)
      {
      if (TR::Compiler->target.is64Bit())
         {
         TR::Node * zeroNode = TR::Node::create(tableNode, TR::lconst);
         zeroNode->setLongInt(0);
         TR::Node * pageNode = TR::Node::create(tableNode, TR::lconst);
         pageNode->setLongInt(mask);
         TR::Node * dupTableNode = tableNode->duplicateTree();
         if (!tableBackedByRawStorage)
            {
            TR::Node * hdrSizeNode = TR::Node::create(tableNode, TR::lconst, 0);
            hdrSizeNode->setLongInt((int32_t)TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
            dupTableNode = TR::Node::create(TR::ladd, 2, dupTableNode, hdrSizeNode);
            }
         TR::Node * andNode = TR::Node::create(TR::land, 2, dupTableNode, pageNode);

         compareNode = TR::Node::createif(TR::iflcmpne, zeroNode, andNode);
         }
      else
         {
         TR::Node * zeroNode = TR::Node::create(tableNode, TR::iconst, 0, 0);
         TR::Node * pageNode = TR::Node::create(tableNode, TR::iconst, 0, mask);
         TR::Node * dupTableNode = tableNode->duplicateTree();
         if (!tableBackedByRawStorage)
            {
            TR::Node * hdrSizeNode = TR::Node::create(tableNode, TR::iconst, 0, TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
            dupTableNode = TR::Node::create(TR::iadd, 2, dupTableNode, hdrSizeNode);
            }
         TR::Node * andNode = TR::Node::create(TR::iand, 2, dupTableNode, pageNode);

         compareNode = TR::Node::createif(TR::ificmpne, zeroNode, andNode);
         }
      }
   return compareNode;
   }


//*****************************************************************************************
// Sort each entry of the given list
//*****************************************************************************************
List<TR_CISCNode>*
sortList(List<TR_CISCNode>* input, List<TR_CISCNode>* output, List<TR_CISCNode>* order, bool reverse)
   {
   if (input->isSingleton())
      {
      TR_CISCNode *n = input->getListHead()->getData();
      if (order->find(n))
         output->add(n);
      }
   else
      {
      ListIterator<TR_CISCNode> li(order);
      TR_CISCNode *n = li.getFirst();
      if (!reverse)
         {
         ListAppender<TR_CISCNode> appender(output);
         for (; n; n = li.getNext())
            {
            if (input->find(n)) appender.add(n);
            }
         }
      else
         {
         for (; n; n = li.getNext())
            {
            if (input->find(n)) output->add(n);
            }
         }
      }
   return output;
   }

//*****************************************************************************************
// Checks if loop preheader block is last block in method.
//*****************************************************************************************
bool isLoopPreheaderLastBlockInMethod(TR::Compilation *comp, TR::Block *block, TR::Block **predBlock)
   {

   // If already a loop invariant block, we likely have the preheader.
   if (block->getStructureOf() && block->getStructureOf()->isLoopInvariantBlock())
      {
      if (predBlock)
         *predBlock = block;
      if (block->getExit()->getNextTreeTop() == NULL)
         {
         traceMsg(comp, "Preheader block_%d [%p] is last block in method.\n", block->getNumber(), block);
         return true;
         }
      }
   else
      {
      // Find the loop preheader block and check its next treetop.
      for (auto edge = block->getPredecessors().begin(); edge != block->getPredecessors().end(); ++edge)
         {
         TR::Block *source = toBlock ((*edge)->getFrom());

         if (source->getStructureOf() &&
             source->getStructureOf()->isLoopInvariantBlock())
            {
            if (predBlock)
               *predBlock = source;
            if (source->getExit()->getNextTreeTop() == NULL)
               {
               traceMsg(comp, "Preheader block_%d [%p] to block_%d [%p] is last block in method.\n", source->getNumber(), source, block->getNumber(), block);
               return true;
               }
            }
         }
      }
   return false;
   }


//*****************************************************************************************
// Search for specific symref within a given subtree.  If targetNode is specified, the node with the matching symref
// will be replaced with the targetNode.
// @param curNode The parent node to recursively search
// @param symRefNumberToBeMatched The symbol reference number to be matched
// @return The load TR::Node* with the matching symbol reference number.  NULL if not found.
//*****************************************************************************************
TR::Node*
findLoadWithMatchingSymRefNumber(TR::Node *curNode, int32_t symRefNumberToBeMatched)
   {
   TR::Node *node = NULL;
   // Recursively iterate through each children node and look for nodes with the matching symref to replace.
   for (int32_t i = 0; i < curNode->getNumChildren(); i++)
      {
      TR::Node *childNode = curNode->getChild(i);

      if (childNode->getOpCode().isLoad() && childNode->getOpCode().hasSymbolReference() && (childNode->getSymbolReference()->getReferenceNumber() == symRefNumberToBeMatched))
         {
         node = childNode;
         break;
         }
      else
         {
         node = findLoadWithMatchingSymRefNumber(childNode, symRefNumberToBeMatched);
         if (node != NULL)
            break;
         }
      }
   return node;
   }


//*****************************************************************************************
// Search for specific symref within a given subtree.  If targetNode is specified, the node with the matching symref
// will be replaced with the targetNode.
//*****************************************************************************************
bool
findAndOrReplaceNodesWithMatchingSymRefNumber(TR::Node *curNode, TR::Node *targetNode, int32_t symRefNumberToBeMatched)
   {
   bool isFound = false;

   // Recursively iterate through each children node and look for nodes with the matching symref to replace.
   for (int32_t i = 0; i < curNode->getNumChildren(); i++)
      {
      TR::Node *childNode = curNode->getChild(i);

      if (childNode->getOpCode().hasSymbolReference() && (childNode->getSymbolReference()->getReferenceNumber() == symRefNumberToBeMatched))
         {
         if (targetNode != NULL)
            curNode->setAndIncChild(i,targetNode);
         isFound = true;
         }
      else
         {
         isFound = findAndOrReplaceNodesWithMatchingSymRefNumber(childNode, targetNode, symRefNumberToBeMatched) || isFound;
         }
      }
   return isFound;
   }

