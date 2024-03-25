/*******************************************************************************
 * Copyright IBM Corp. and others 2020
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef ABS_INTERPRETER_INCL
#define ABS_INTERPRETER_INCL

#include "optimizer/abstractinterpreter/IDTBuilder.hpp"
#include "optimizer/abstractinterpreter/AbsStackMachineState.hpp"
#include "il/Block.hpp"
#include "ilgen/J9ByteCodeIterator.hpp"
#include "infra/ILWalk.hpp"
#include <vector>


namespace TR {

/*
 * The abstract interpreter for Java Bytecode.
 * It interprets the whole method.
 */
class AbsInterpreter
   {
   public:
   AbsInterpreter(TR::ResolvedMethodSymbol* callerMethodSymbol, TR::CFG* cfg, TR::AbsVisitor* vistor, TR::vector<TR::AbsValue*, TR::Region&>* arguments, TR::Region& region, TR::Compilation* comp);

   /**
    * @brief start to interpret the method.
    * Method without loop will be interpreted by walking the CFG blocks.
    * Method with loops will be interpreted using structrual analysis.
    *
    * @return true if the whole method is successfully interpreted. false otherwise
    */
   bool interpret();

   TR::InliningMethodSummary* getInliningMethodSummary() { return _inliningMethodSummary; }

   TR::AbsValue* getReturnValue() { return _returnValue; }

   private:

   TR::Compilation* comp() {  return _comp; }
   TR::Region& region() {  return _region;  }

   TR::ValuePropagation *vp();

   typedef TR::typed_allocator<std::pair<TR::Block * const, TR::AbsStackMachineState*>, TR::Region&> BlockStateMapAllocator;
   typedef std::less<TR::Block*> BlockStateMapComparator;
   std::map<TR::Block*, TR::AbsStackMachineState*, BlockStateMapComparator, BlockStateMapAllocator> _blockStateMap;

   TR::AbsStackMachineState* getState(TR::Block* block);
   void setState(TR::Block* block, TR::AbsStackMachineState* state);

   void transferBlockStatesFromPredeccesors(TR::Block* block, bool insideLoop);

   bool interpretStructure(TR_Structure* structure, bool insideLoop, bool lastTimeThrough);
   bool interpretRegionStructure(TR_RegionStructure* regionStructure, bool insideLoop, bool lastTimeThrough);
   bool interpretBlockStructure(TR_BlockStructure* blockStructure, bool insideLoop, bool lastTimeThrough);

   bool interpretBlock(TR::Block* block, bool insideLoop, bool lastTimeThrough);

   TR::AbsVisitor* _visitor;

   TR::vector<TR::AbsValue*, TR::Region&>* _arguments;

   TR::InliningMethodSummary* _inliningMethodSummary;
   TR::AbsValue* _returnValue;
   TR_J9ByteCodeIterator _bci;
   int32_t _callerIndex;
   TR::ResolvedMethodSymbol* _callerMethodSymbol;
   TR_ResolvedMethod* _callerMethod;
   TR::CFG* _cfg;
   TR::Region& _region;
   TR::Compilation* _comp;
   TR::ValuePropagation* _vp;
   };

/*
 * The interpreter for a Basic Block.
 */
class AbsBlockInterpreter
   {
   public:
   AbsBlockInterpreter(TR::Block* block,
                        TR::AbsStackMachineState* state,
                        bool insideLoop,
                        bool lastTimeThrough,
                        TR::ResolvedMethodSymbol* callerMethodSymbol,
                        TR_J9ByteCodeIterator& bci,
                        TR::AbsValue** returnValue,
                        TR::InliningMethodSummary* summary,
                        TR::AbsVisitor* visitor,
                        TR::ValuePropagation* vp,
                        TR::Compilation* comp,
                        TR::Region& region);

   TR::AbsStackMachineState* setStartBlockState(TR::vector<TR::AbsValue*, TR::Region&>* args);
   bool interpret();

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

   TR::Block* getBlock() { return _block; }
   int32_t getBlockStartIndex() { return _block->getBlockBCIndex(); }
   int32_t getBlockEndIndex() { return _block->getBlockBCIndex() + _block->getBlockSize(); }
   TR::AbsStackMachineState* getState() { return _state; }

   bool insideLoop() { return _insideLoop; }
   bool lastTimeThrough() { return _lastTimeThrough; }

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


   TR::AbsValue* popFromValue(TR::AbsStackMachineState* state, TR::DataType fromType);
   void conversionI2b(TR::DataType fromType);
   void conversionI2c(TR::DataType fromType);
   void conversionI2s(TR::DataType fromType);
   void conversionI2l(TR::DataType fromType);
   void conversionI2f(TR::DataType fromType);
   void conversionI2d(TR::DataType fromType);
   void conversionL2i(TR::DataType fromType);
   void conversionL2f(TR::DataType fromType);
   void conversionL2d(TR::DataType fromType);
   void conversionD2i(TR::DataType fromType);
   void conversionD2f(TR::DataType fromType);
   void conversionD2l(TR::DataType fromType);
   void conversionF2i(TR::DataType fromType);
   void conversionF2d(TR::DataType fromType);
   void conversionF2l(TR::DataType fromType);

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

   TR_CallSite* getCallSite(TR::MethodSymbol::Kinds kind, int32_t bcIndex, int32_t cpIndex, TR::Method* calleeMethod, TR::vector<TR::AbsValue*, TR::Region&>& args);

   /*** Methods for creating different types of abstract values ***/
   TR::AbsValue* createObject(TR_OpaqueClassBlock* opaqueClass);

   TR::AbsValue* createNullObject();
   TR::AbsValue* createNonNullObject(TR_OpaqueClassBlock* opaqueClass);

   TR::AbsValue* createArrayObject(TR_OpaqueClassBlock* arrayClass, bool isNonNull, int32_t lengthLow, int32_t lengthHigh, int32_t elementSize);
   TR::AbsValue* createStringObject(TR::SymbolReference* symRef, bool isNonNull);

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

   bool isIntConst(TR::AbsValue* v);
   bool isIntRange(TR::AbsValue* v);
   bool isInt(TR::AbsValue* v);

   bool isLongConst(TR::AbsValue* v);
   bool isLongRange(TR::AbsValue* v);
   bool isLong(TR::AbsValue* v);

   int32_t arrayElementSize(const char *signature);

   TR::Compilation* comp() { return _comp; }
   TR::Region& region() { return _region; }
   TR::ValuePropagation* vp() { return _vp; }
   bool _insideLoop;
   bool _lastTimeThrough;
   int32_t _callerIndex;
   TR::Block* _block;
   TR::AbsStackMachineState* _state;
   TR::AbsValue** _returnValue;
   TR::Compilation* _comp;
   TR::Region& _region;
   TR::ValuePropagation* _vp;
   TR::ResolvedMethodSymbol* _callerMethodSymbol;
   TR_ResolvedMethod* _callerMethod;
   TR_J9ByteCodeIterator& _bci;
   TR::AbsVisitor* _visitor;
   TR::InliningMethodSummary* _inliningMethodSummary;
   };

class RegionIterator
   {
   public:
   RegionIterator(TR_RegionStructure *region, TR::Region &mem);

   TR_StructureSubGraphNode *getCurrent();
   void next();

   private:
   void addSuccessors(TR_StructureSubGraphNode *from);

   TR_RegionStructure *_region;
   TR::deque<TR_StructureSubGraphNode*, TR::Region &> _nodes;
   TR_BitVector _seen;
   int32_t _current;
   };
}

#endif
