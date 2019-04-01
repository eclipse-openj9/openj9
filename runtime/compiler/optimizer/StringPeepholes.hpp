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

#ifndef STRINGPEEPHOLES_INCL
#define STRINGPEEPHOLES_INCL

#include <stddef.h>
#include <stdint.h>
#include "codegen/RecognizedMethods.hpp"
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/List.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"

namespace TR { class ResolvedMethodSymbol; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }
struct TR_BDChain;

class TR_StringPeepholes : public TR::Optimization
   {
   public:
   TR_StringPeepholes(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_StringPeepholes(manager);
      }

   virtual int32_t perform();
   virtual int32_t performOnBlock(TR::Block *);
   virtual void prePerformOnBlocks();
   virtual const char * optDetailString() const throw();
   int32_t process(TR::TreeTop *, TR::TreeTop *);

   private:
   void genFlush(TR::TreeTop *, TR::Node *node);
   enum StringpeepholesMethods
      {
	   SPH_BigDecimal_SMAAMSS,
	   SPH_BigDecimal_SMSS,
	   SPH_BigDecimal_AAMSS,
	   SPH_BigDecimal_MSS,
	   START_STRING_METHODS, //separator
       SPH_String_init_SC, //_initSymRef
	   SPH_String_init_SS, //_initSymRef1
	   SPH_String_init_SSS, //_initSymRef2
	   SPH_String_init_SI, //_initSymRef3
	   SPH_String_init_AIIZ, //_initSymRef4
	   SPH_String_init_ISISS, //_initSymRef7
	   START_BIG_DECIMAL_HELPER_METHODS, //separator
	   SPH_DecimalFormatHelper_formatAsDouble,
	   SPH_DecimalFormatHelper_formatAsFloat,
	   END_STRINGPEEPHOLES_METHODS
      };

   TR::SymbolReference **optSymRefs;

   TR::SymbolReference* findSymRefForOptMethod (StringpeepholesMethods m);
   TR::SymbolReference* MethodEnumToArgsForMethodSymRefFromName (StringpeepholesMethods m);

   void processBlock(TR::Block *block);
   TR::TreeTop *detectPattern(TR::Block *block, TR::TreeTop *tt, bool useStringBuffer);
   TR::TreeTop *detectBDPattern(TR::Block *block, TR::TreeTop *tt, TR::Node *node);
   TR::TreeTop *detectFormatPattern(TR::TreeTop *tt, TR::TreeTop *exit, TR::Node *node);
   TR::TreeTop *detectSubMulSetScalePattern(TR::TreeTop *tt, TR::TreeTop *exit, TR::Node *node);
   TR::SymbolReference *findSymRefForValueOf(const char *sig);

   bool checkMethodSignature(TR::SymbolReference *symRef, const char *sig);
   TR::TreeTop *searchForStringAppend(const char *sig, TR::TreeTop *tt, TR::TreeTop *exitTree,
                                     TR::ILOpCodes opCode, TR::Node *newBuffer, vcount_t visitCount,
                                     TR::Node **string, TR::TreeTop **prim);
   TR::TreeTop *searchForInitCall(const char *sig, TR::TreeTop *tt, TR::TreeTop *exitTree,
                                 TR::Node *newBuffer, vcount_t visitCount, TR::TreeTop **initTree);
   TR::TreeTop *searchForToStringCall(TR::TreeTop *tt, TR::TreeTop *exitTree,
                                     TR::Node *newBuffer, vcount_t visitCount,
                                     TR::TreeTop **toStringTree, bool useStringBuffer);

   bool skipNodeUnderOSR(TR::Node *node);
   bool invalidNodeUnderOSR(TR::Node *node);
   void removePendingPushOfResult(TR::TreeTop *callTreeTop);
   void removeAllocationFenceOfNew(TR::TreeTop *newTreeTop);
   void postProcessTreesForOSR(TR::TreeTop *startTree, TR::TreeTop *endTree);

   bool _stringClassesRedefined;
   bool classesRedefined();
   bool classRedefined(TR_OpaqueClassBlock *clazz);

   TR::ResolvedMethodSymbol *_methodSymbol;
   TR::SymbolReference      *_stringSymRef;


   /*
   TR::SymbolReference      *_initSymRef; // String + Char constructor
   TR::SymbolReference      *_initSymRef1;// String + String constructor
   TR::SymbolReference      *_initSymRef2;// String + String + String constructor
   TR::SymbolReference      *_initSymRef3;// String + Integer constructor
   TR::SymbolReference      *_initSymRef4;// Constructor with parm types int, int, String

   TR::SymbolReference      *_initSymRef5;// String Constructor with parm types String, int, int
   TR::SymbolReference      *_initSymRef6;// String Constructor with parm types String, char, int

   TR::SymbolReference      *_initSymRef7;// String Constructor with parm types int, String, int, String, String
   */

   TR::SymbolReference      *_valueOfISymRef;// String.valueOf(I)
   TR::SymbolReference      *_valueOfCSymRef;// String.valueOf(C)
   TR::SymbolReference      *_valueOfJSymRef;// String.valueOf(J)
   TR::SymbolReference      *_valueOfZSymRef;// String.valueOf(Z)
   TR::SymbolReference      *_valueOfOSymRef;// String.valueOf(Ljava/lang/Object;)

   TR_ScratchList<TR::TreeTop> _transformedCalls;

   TR::TreeTop *detectBDPattern(TR::TreeTop *tt, TR::TreeTop *exit, TR::Node *node);
   TR_BDChain *detectChain(TR::RecognizedMethod recognizedMethod, TR::TreeTop *cursorTree, TR::Node *cursorNode, TR_BDChain *prevInChain);

   public:
   static const int32_t numBDPatterns = 4;

   //TR::SymbolReference *_privateMethodSymRefs[numBDPatterns];
   //TR::SymbolReference *_decFormatEntryPoint[2];

   //TR::SymbolReference       *_privateMethodSymRef1;
   //TR::SymbolReference       *_privateMethodSymRef2;
   //TR::SymbolReference       *_privateMethodSymRef3;
   };

#endif
