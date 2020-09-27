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
#include "optimizer/abstractinterpreter/AbsStackMachineState.hpp"
#include "il/Block.hpp"
#include "ilgen/J9ByteCodeIterator.hpp"
#include "infra/ILWalk.hpp"


namespace J9 {
   
/*
 * The abstract interpreter for Java Bytecode.
 * It interprets the method and simluates the JVM state.
 */
class AbsInterpreter : public TR_J9ByteCodeIterator, public TR::ReversePostorderSnapshotBlockIterator
   {
   public:
   AbsInterpreter(TR::ResolvedMethodSymbol* callerMethodSymbol, TR::CFG* cfg, TR::AbsVisitor* vistor, TR::AbsArguments* arguments, TR::Region& region, TR::Compilation* comp);

   /**
    * @brief start to interpret the method.
    * It walks the CFG's basic blocks in reverse post order and interprets the bytecode in each block,
    * updating the block's abstract state correspondingly.
    * 
    * @return true if the whole method is successfully interpreted. false otherwise
    */
   bool interpret();

   TR::InliningMethodSummary* getInliningMethodSummary() { return _inliningMethodSummary; }

   TR::AbsValue* getReturnValue() { return _returnValue; }

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

   TR::ValuePropagation *vp();

   void moveToNextBlock();

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

   /*** Methods for creating different types of abstract values ***/
   TR::AbsValue* createObject(TR_OpaqueClassBlock* opaqueClass, TR_YesNoMaybe isNonNull);

   TR::AbsValue* createNullObject();

   TR::AbsValue* createArrayObject(TR_OpaqueClassBlock* arrayClass, TR_YesNoMaybe isNonNull, int32_t lengthLow, int32_t lengthHigh, int32_t elementSize);
   TR::AbsValue* createStringObject(TR::SymbolReference* symRef, TR_YesNoMaybe isNonNull);

   TR::AbsValue* createIntConst(int32_t value);
   TR::AbsValue* createLongConst(int64_t value);

   TR::AbsValue* createIntRange(int32_t low, int32_t high);
   TR::AbsValue* createLongRange(int64_t low, int64_t high);

   TR::AbsValue* createTopInt();
   TR::AbsValue* createTopLong();

   TR::AbsValue* createTopDouble();
   TR::AbsValue* createTopFloat();

   TR::AbsValue* createTopObject();

   bool isNullObject(TR::AbsValue* v);
   bool isNonNullObject(TR::AbsValue* v);

   bool isArrayObject(TR::AbsValue* v);
   bool isObject(TR::AbsValue* v);

   bool isIntConst(TR::AbsValue* v);
   bool isIntRange(TR::AbsValue* v);
   bool isInt(TR::AbsValue* v);

   bool isLongConst(TR::AbsValue* v);
   bool isLongRange(TR::AbsValue* v);
   bool isLong(TR::AbsValue* v);

   TR::AbsVisitor* _visitor;

   TR::AbsArguments* _arguments;

   TR::InliningMethodSummary* _inliningMethodSummary;
   TR::AbsValue* _returnValue;
   
   int32_t _callerIndex;
   TR::ResolvedMethodSymbol* _callerMethodSymbol;
   TR_ResolvedMethod* _callerMethod;
   TR::CFG* _cfg;

   bool *_blockStartIndexFlags;  //marks if the index is the start index of any basic block

   TR::Region& _region;
   TR::Compilation* _comp;
   TR::ValuePropagation* _vp;
   };

}

#endif
