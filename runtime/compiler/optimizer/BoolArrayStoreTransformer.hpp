/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#ifndef BoolArrayStoreTransformer_h
#define BoolArrayStoreTransformer_h

#include <set>
#include <vector>
#include "il/symbol/ParameterSymbol.hpp"
#include "compile/Compilation.hpp"
#include "infra/Checklist.hpp"
#include "infra/List.hpp"

class TR_BoolArrayStoreTransformer
   {
   public:

   typedef TR::typed_allocator<TR::Node*, TR::Region &> NodeAllocator;
   typedef std::set<TR::Node*, std::less<TR::Node*>, NodeAllocator> NodeSet;

   TR_BoolArrayStoreTransformer(NodeSet *bstoreiNodes, NodeSet *boolArrayTypeNodes);
   void perform();

   typedef TR::typed_allocator<TR_YesNoMaybe, TR::Region &> TypeInfoAllocator;
   typedef std::vector<TR_YesNoMaybe, TypeInfoAllocator> TypeInfo;

   TypeInfo * processBlock(TR::Block *block, TypeInfo *typeInfo);
   static bool isAnyDimensionBoolArrayNode(TR::Node *node);
   static bool isAnyDimensionByteArrayNode(TR::Node *node);
   static bool isAnyDimensionBoolArrayParm(TR::ParameterSymbol *symbol);
   static bool isAnyDimensionByteArrayParm(TR::ParameterSymbol *symbol);
   static bool isBoolArrayNode(TR::Node *node, bool parmAsAuto = true);
   static bool isByteArrayNode(TR::Node *node, bool parmAsAuto = true);
   static bool isBoolArrayParm(TR::ParameterSymbol *symbol);
   static bool isByteArrayParm(TR::ParameterSymbol *symbol);
   static int getArrayDimension(TR::Node *node, bool boolType, bool parmAsAuto = true);
   static int getArrayDimension(const char * signature, int length, bool boolType);
   void findLoadAddressAutoAndFigureOutType(TR::Node *node, TypeInfo * typeInfo, TR::NodeChecklist &boolArrayNodes, TR::NodeChecklist &byteArrayNodes, TR::NodeChecklist &loadAutoNodes);
   void mergeTypeInfo(TypeInfo *first, TypeInfo *second);
   void collectLocals(TR_Array<List<TR::SymbolReference>> *autosListArray);
   void findBoolArrayStoreNodes();
   void transformBoolArrayStoreNodes();
   void transformUnknownTypeArrayStore();
   bool hasBoolArrayAutoOrCheckCast() { return _hasBoolArrayAutoOrCheckCast;}
   bool hasByteArrayAutoOrCheckCast() { return _hasByteArrayAutoOrCheckCast;}
   void setHasBoolArrayAutoOrCheckCast() { _hasBoolArrayAutoOrCheckCast = true;}
   void setHasByteArrayAutoOrCheckCast() { _hasByteArrayAutoOrCheckCast = true;}
   void setHasVariantArgs() { _hasVariantArgs = true;}
   uint32_t _numLocals;
   TR::Compilation *comp() { return _comp; }

   private:
   TR::Compilation *_comp;
   NodeSet *_bstoreiUnknownArrayTypeNodes;
   NodeSet *_bstoreiBoolArrayTypeNodes;
   bool _hasBoolArrayAutoOrCheckCast;
   bool _hasByteArrayAutoOrCheckCast;
   bool _hasVariantArgs;
   int32_t _NumOfBstoreiNodesToVisit;
   };

#endif
