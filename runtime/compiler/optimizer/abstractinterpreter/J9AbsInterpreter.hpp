/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef J9_ABS_INTERPRETER_INCL
#define J9_ABS_INTERPRETER_INCL

#include "optimizer/abstractinterpreter/IDTBuilder.hpp"
#include "optimizer/abstractinterpreter/AbsState.hpp"
#include "il/Block.hpp"
#include "ilgen/J9ByteCodeIterator.hpp"
#include "infra/ILWalk.hpp"

/*
 * The abstract interpreter for Java Bytecode.
 * It interprets the method and simluates the JVM state.
 * 
 */
class J9AbsInterpreter : public TR_J9ByteCodeIterator, public TR::ReversePostorderSnapshotBlockIterator
   {
   public:
   J9AbsInterpreter(TR::ResolvedMethodSymbol* callerMethodSymbol, TR::CFG* cfg, AbsVisitor* vistor, AbsArguments* arguments, TR::Region& region, TR::Compilation* comp);

   /**
    * @brief start to interpret the method.
    * It walks the CFG's basic blocks in reverse post order and interprets the bytecode in each block,
    * updating the block's abstract state correspondingly.
    * 
    * @return bool
    * 
    * The return value indicates whether the abstract interpretation is successful or not.
    */
   bool interpret();

   InliningMethodSummary* getInliningMethodSummary() { return _inliningMethodSummary; }

   AbsValue* getReturnValue() { return _returnValue; }

   void setCallerIndex(int32_t callerIndex) { _callerIndex = callerIndex; }
   
   private:
   enum BinaryOperator
      {
      plus, minus, mul, div, rem,
      and_, or_, xor_
      };

   enum UnaryOperator
      {
      neg
      };

   enum ShiftOperator
      {
      shl,
      shr,
      ushr
      };

   enum ConditionalBranchOperator
      {
      eq, ge, gt, le, lt, ne,
      null, nonnull,
      cmpeq, cmpge, cmpgt, cmple, cmplt, cmpne
      };

   enum ComparisonOperator //fcmpl, fcmpg, dcmpl, dcmpg. The 'g' and 'l' here are ommitted since float and double are not being modeled. 
      {
      cmp
      };

   TR::Compilation* comp() {  return _comp; }
   TR::Region& region() {  return _region;  }

   OMR::ValuePropagation *vp();

   void moveToNextBasicBlock();

   void setStartBlockState();

   void transferBlockStatesFromPredeccesors();
   
   bool interpretByteCode();
      
   /** For interpreting bytecode and updating abstract state **/

   void constant(int32_t i);
   void constant(int64_t l);
   void constant(float f);
   void constant(double d);
   void constantNull();
   
   void load(TR::DataType type, int32_t index);
   void store(TR::DataType type, int32_t index);

   void arrayLoad(TR::DataType type);
   void arrayStore(TR::DataType type);

   void ldc(bool wide);

   void binaryOperation(TR::DataType type, BinaryOperator op);
   void unaryOperation(TR::DataType type, UnaryOperator op);

   void pop(int32_t size);
   void swap();
   void dup(int32_t size, int32_t delta);

   void nop();

   void shift(TR::DataType type, ShiftOperator op);

   void conversion(TR::DataType fromType, TR::DataType toType);

   void comparison(TR::DataType type, ComparisonOperator op);

   void goto_(int32_t label);
   void conditionalBranch(TR::DataType type, int32_t label, ConditionalBranchOperator op);

   void return_(TR::DataType type, bool oneBit=false);

   void new_();
   void newarray();
   void anewarray();
   void multianewarray(int32_t dimension);

   void arraylength();

   void instanceof();
   void checkcast();

   void get(bool isStatic);
   void put(bool isStatic);

   void iinc(int32_t index, int32_t incVal);

   void monitor(bool kind); //true: enter, false: exit
   void switch_(bool kind); //true: lookup, false: table

   void athrow();

   void invoke(TR::MethodSymbol::Kinds kind);

   TR::SymbolReference* getSymbolReference(int32_t cpIndex, TR::MethodSymbol::Kinds kind);

   TR_CallSite* getCallSite(TR::MethodSymbol::Kinds kind, int32_t bcIndex, int32_t cpIndex);

   AbsVisitor* _visitor;

   AbsArguments *_arguments;

   InliningMethodSummary* _inliningMethodSummary;
   AbsValue* _returnValue;
   
   int32_t _callerIndex;
   TR::ResolvedMethodSymbol* _callerMethodSymbol;
   TR_ResolvedMethod* _callerMethod;
   TR::CFG* _cfg;

   bool *_blockStartIndexFlags;  //marks if the index is the start index of any basic block

   TR::Region& _region;
   TR::Compilation* _comp;
   OMR::ValuePropagation* _vp;
   };



#endif