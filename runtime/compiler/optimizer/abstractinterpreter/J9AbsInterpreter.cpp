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

#include "optimizer/abstractinterpreter/J9AbsInterpreter.hpp"
#include "optimizer/J9CallGraph.hpp"

J9::AbsInterpreter::AbsInterpreter(TR::ResolvedMethodSymbol* callerMethodSymbol, TR::CFG* cfg, TR::AbsVisitor* vistor, TR::AbsArguments* arguments, TR::Region& region, TR::Compilation* comp):
      TR_J9ByteCodeIterator(callerMethodSymbol, static_cast<TR_ResolvedJ9Method*>(callerMethodSymbol->getResolvedMethod()), static_cast<TR_J9VMBase*>(comp->fe()), comp),
      TR::ReversePostorderSnapshotBlockIterator(cfg, comp),
      _callerMethodSymbol(callerMethodSymbol),
      _callerMethod(callerMethodSymbol->getResolvedMethod()),
      _callerIndex(-1),
      _cfg(cfg),
      _visitor(vistor),
      _arguments(arguments),
      _region(region),
      _comp(comp),
      _vp(NULL),
      _returnValue(NULL),
      _inliningMethodSummary(new (region) TR::InliningMethodSummary(region))
   {
   _blockStartIndexFlags = new (_region) bool[maxByteCodeIndex()];
   memset(_blockStartIndexFlags, 0, maxByteCodeIndex());

   TR::AllBlockIterator blockIt(cfg, comp);

   //Marks the bytecode index which is the start index of any block
   while (blockIt.currentBlock())
      {
      if (blockIt.currentBlock() != cfg->getStart()->asBlock() && blockIt.currentBlock() != cfg->getEnd()->asBlock())
         {
         _blockStartIndexFlags[blockIt.currentBlock()->getBlockBCIndex()] = true;
         }
      blockIt.stepForward();
      }
      
   }

TR::ValuePropagation* J9::AbsInterpreter::vp()
   {
   if (!_vp)
      {
      TR::OptimizationManager* manager = comp()->getOptimizer()->getOptimization(OMR::globalValuePropagation);
      _vp = (TR::ValuePropagation*) manager->factory()(manager);
      _vp->initialize();
      }
   return _vp;
   }

bool J9::AbsInterpreter::interpret()
   {
   if (comp()->getOption(TR_TraceAbstractInterpretation))
      traceMsg(comp(), "\nStarting to abstract interpret method %s ...\n", _callerMethod->signature(comp()->trMemory()));

   setStartBlockState();
   moveToNextBlock();
   
   while (currentBlock()) //walk the CFG blocks
      {
      //Check if the current block has been already interpreted
      while (currentBlock() 
         && ((_blockStartIndexFlags[currentByteCodeIndex()] == true && currentByteCodeIndex() != currentBlock()->getBlockBCIndex()) //if we arrive at the start index of another block
               || currentByteCodeIndex() > maxByteCodeIndex() // if we arrive at the end of the whole method
               || current() == J9BCunknown)) 
         {
         if (comp()->getOption(TR_TraceAbstractInterpretation) && currentBlock()->getAbsState())
            {
            traceMsg(comp(), "\nBlock: #%d of method %s finishes abstract interpretation\n", currentBlock()->getNumber(), _callerMethod->signature(comp()->trMemory()));
            static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState())->print(comp());
            }
         
         moveToNextBlock(); //We may move to the end (currentBlock() is NULL)
         }
      
      if (currentBlock()) 
         {
         if (currentBlock()->getAbsState())
            {
            bool success = interpretByteCode();

            if (!success) // Should abort interpreting the whole method immediately if any bytecode fails to be interpreted.
               {
               if (comp()->getOption(TR_TraceAbstractInterpretation))
                  traceMsg(comp(), "\nFail to abstract interpret method %s ...\n", _callerMethod->signature(comp()->trMemory()));
               return false;
               } 
               
            }
         else 
            {
            //dead code block, does not have an state
            if (comp()->getOption(TR_TraceAbstractInterpretation))
               traceMsg(comp(), "\nBlock in dead code area. Do not interpret\n");
            }

         next(); //move to next bytecode
         }
      }

   if (comp()->getOption(TR_TraceAbstractInterpretation))
      traceMsg(comp(), "\nSuccessfully abstract interpret method %s ...\n", _callerMethod->signature(comp()->trMemory()));

   if (comp()->getOption(TR_TraceBISummary))
      _inliningMethodSummary->trace(comp());

   return true;
   }

void J9::AbsInterpreter::moveToNextBlock()
   {
   TR_ASSERT_FATAL(currentBlock(), "Cannot move to next block since CFG walk has already ended");

   stepForward(); //go to next block

   if (currentBlock() == _cfg->getEnd()->asBlock()) //End Block
      stepForward(); // skip the end block

   if (currentBlock())
      {
      if (comp()->getOption(TR_TraceAbstractInterpretation))
         traceMsg(comp(), "\nStart to abstract interpret Block: #%d of method %s ...\n", currentBlock()->getNumber(), _callerMethod->signature(comp()->trMemory()));
      transferBlockStatesFromPredeccesors(); //transfer CFG abstract states to the current block
      setIndex(currentBlock()->getBlockBCIndex()); //set the start index of the bytecode iterator
      }
   }

//Set the abstract state of the START block of CFG
void J9::AbsInterpreter::setStartBlockState()
   {  
   TR::AbsStackMachineState* state = new (region()) TR::AbsStackMachineState(region());

   uint32_t paramPos = 0; 
   uint32_t slotIndex = 0;

   // if we have arguments passed from the caller, setting the parameters using these arguments.
   //
   if (_arguments) 
      {
      for (size_t i = 0; i < _arguments->size(); i++, paramPos++, slotIndex++)
         {
         TR::AbsVPValue* arg = static_cast<TR::AbsVPValue*>(_arguments->at(i));
         TR::DataType dataType = arg->getDataType();

         TR::AbsVPValue* param = new (region()) TR::AbsVPValue(vp(), arg->getConstraint(), dataType);
         param->setParamPosition(paramPos);

         if (i == 0 && !_callerMethod->isStatic())
            param->setImplicitParam();

         state->set(slotIndex, param);

         switch (dataType)
            {
            case TR::Int8:
            case TR::Int16:
            case TR::Int32:
            case TR::Float:
            case TR::Address:
               break;
            case TR::Double:
               slotIndex++;
               state->set(slotIndex, createTopDouble()); //take the second half of 64-bit
               break;
            case TR::Int64:
               slotIndex++;
               state->set(slotIndex, createTopLong());
               break;

            default:
               TR_ASSERT_FATAL(false, "Invalid type");
            }
         }
      }

   // if not, setting the parameters as TOP. 
   //
   else 
      {
      //set the implicit parameter
      if (!_callerMethod->isStatic())
         {
         TR_OpaqueClassBlock *classBlock = _callerMethod->containingClass();
         TR::AbsValue* value = createObject(classBlock, TR_yes);
         value->setParamPosition(paramPos++);
         value->setImplicitParam();
         state->set(slotIndex++, value);
         }

      //set the explicit parameters
      for (TR_MethodParameterIterator *pi = _callerMethod->getParameterIterator(*comp()); !pi->atEnd(); pi->advanceCursor(), slotIndex++, paramPos++)
         {
         TR::DataType dataType = pi->getDataType();
         TR::AbsValue* param = NULL;

         switch (dataType)
            {
            case TR::Int8:
            case TR::Int16:
            case TR::Int32:
               param = createTopInt();
               param->setParamPosition(paramPos);
               state->set(slotIndex, param);
               break;
            
            case TR::Int64:
               param = createTopLong();
               param->setParamPosition(paramPos);
               state->set(slotIndex, param);
               slotIndex++;
               state->set(slotIndex, createTopLong());
               break;
            
            case TR::Double:
               param = createTopDouble();
               param->setParamPosition(paramPos);
               state->set(slotIndex, param);
               slotIndex++;
               state->set(slotIndex, createTopDouble());
               break;
            
            case TR::Float:
               param = createTopFloat();
               param->setParamPosition(paramPos);
               state->set(slotIndex, param);
               break;
            
            case TR::Aggregate:
               {
               TR_OpaqueClassBlock *classBlock = pi->getOpaqueClass();
               if (pi->isArray())
                  {
                  int32_t arrayType = comp()->fe()->getNewArrayTypeFromClass(classBlock);
                  int32_t elemetSize = arrayType == 7 || arrayType == 11 ? 8 : 4; //7: double, 11: long
                  param = createArrayObject(classBlock, TR_maybe, 0, INT32_MAX, elemetSize);
                  param->setParamPosition(paramPos);
                  state->set(slotIndex, param);
                  }
               else
                  {
                  param = createObject(classBlock, TR_maybe);
                  param->setParamPosition(paramPos);
                  state->set(slotIndex, param);
                  }
               break;
               }

            default:
               TR_ASSERT_FATAL(false, "Uncatched type");
               break;
            }
         }
      }

   _cfg->getStart()->asBlock()->setAbsState(state);

   if (comp()->getOption(TR_TraceAbstractInterpretation))
      {
      traceMsg(comp(), "Start Block state of method %s:\n", _callerMethod->signature(comp()->trMemory()));
      static_cast<TR::AbsStackMachineState*>(_cfg->getStart()->asBlock()->getAbsState())->print(comp());
      }
                  
   }

void J9::AbsInterpreter::transferBlockStatesFromPredeccesors()
   {
   TR::Block* block = currentBlock();

   /*** Case 1: No predecessors. Do nothing ***/
   if (block->getPredecessors().size() == 0)
      {
      return;
      }
      
   /*** Case 2: Have only one predecessor ***/
   if (block->getPredecessors().size() == 1)
      {
      TR::Block* parentBlock = block->getPredecessors().front()->getFrom()->asBlock();
      
      /*** Case 2.1: Parent is not interpreted. This may happen if this block is in a dead code area. Do nothing ***/
      if (parentBlock->getAbsState() == NULL)
         {
         return;
         }
      /*** Case 2.2: Parent is interpreted. Copy parent's state and pass it to the current block ***/
      else 
         {
         TR::AbsState* parentState = parentBlock->getAbsState();
         TR::AbsState* copiedState = parentState->clone(region()); //copy

         block->setAbsState(copiedState);
         return;
         }
      }

   /*** Case 3: Have more than one predecessors ***/
   if (block->getPredecessors().size() > 1) 
      {
      bool allPredecessorsInterpreted = true;

      TR::Block *oneInterpretedBlock = NULL;

      for (auto e = block->getPredecessors().begin(); e != block->getPredecessors().end(); e++)
         {
         TR::Block* predBlock = (*e)->getFrom()->asBlock();

         if (predBlock->getAbsState() == NULL) //any block is not interpreted
            {
            allPredecessorsInterpreted = false;
            if (oneInterpretedBlock)
               break;
            }
         else  //any block is interpreted
            {
            oneInterpretedBlock = predBlock;
            }
         }

      /*** Case 3.1: All predecessors are interpreted. Merge their states ***/
      if (allPredecessorsInterpreted)
         {
         TR::Block* firstPredBlock = (*block->getPredecessors().begin())->getFrom()->asBlock();
         TR::AbsState* state = firstPredBlock->getAbsState()->clone(region());
         for (auto e = ++block->getPredecessors().begin(); e != block->getPredecessors().end(); e ++)
            {
            TR::Block* predBlock = (*e)->getFrom()->asBlock();
            state->merge(predBlock->getAbsState());
            }

         block->setAbsState(state);
         return;
         }

      /*** Case 3.2: Not all predecessors are interpreted. Have back-edges. Set the state to TOP ***/
      //NOTE: This case (back-edge) will be handled in the future. 
      if (!allPredecessorsInterpreted && oneInterpretedBlock)
         {
         TR::AbsState* copiedState = oneInterpretedBlock->getAbsState()->clone(region());

         copiedState->setToTop();
         block->setAbsState(copiedState);
         return;
         }

      /*** Case 3.3: None of the predecessors is interpreted. Block is in dead code area. Do nothing ***/
      if (!allPredecessorsInterpreted && !oneInterpretedBlock)
         {
         return;
         }
      }

   TR_ASSERT_FATAL(false, "Uncatched case");
   }



bool J9::AbsInterpreter::interpretByteCode()
   {
   switch(current())
      {
      case J9BCnop: nop(); break;

      case J9BCaconstnull: constantNull(); break;

      //iconst_x
      case J9BCiconstm1: constant((int32_t)-1); break;
      case J9BCiconst0: constant((int32_t)0); break;
      case J9BCiconst1: constant((int32_t)1); break;
      case J9BCiconst2: constant((int32_t)2); break;
      case J9BCiconst3: constant((int32_t)3); break;
      case J9BCiconst4: constant((int32_t)4); break;
      case J9BCiconst5: constant((int32_t)5); break;

      //lconst_x
      case J9BClconst0: constant((int64_t)0); break;
      case J9BClconst1: constant((int64_t)1); break;

      //fconst_x
      case J9BCfconst0: constant((float)0); break;
      case J9BCfconst1: constant((float)1); break;
      case J9BCfconst2: constant((float)2); break;

      //dconst_x
      case J9BCdconst0: constant((double)0); break;
      case J9BCdconst1: constant((double)1); break;

      //x_push
      case J9BCbipush: constant((int32_t)nextByteSigned()); break;
      case J9BCsipush: constant((int32_t)next2BytesSigned()); break;

      //ldc_x
      case J9BCldc: ldc(false); break;
      case J9BCldcw: ldc(true); break;
      case J9BCldc2lw: ldc(true); break;
      case J9BCldc2dw: ldc(true); break; 

      //iload_x
      case J9BCiload: load(TR::Int32, nextByte()); break;
      case J9BCiload0: load(TR::Int32, 0); break;
      case J9BCiload1: load(TR::Int32, 1); break;
      case J9BCiload2: load(TR::Int32, 2); break;
      case J9BCiload3: load(TR::Int32, 3); break;
      case J9BCiloadw: load(TR::Int32, next2Bytes()); break;

      //lload_x
      case J9BClload: load(TR::Int64, nextByte()); break;
      case J9BClload0: load(TR::Int64, 0); break;
      case J9BClload1: load(TR::Int64, 1); break;
      case J9BClload2: load(TR::Int64, 2); break;
      case J9BClload3: load(TR::Int64, 3); break;
      case J9BClloadw: load(TR::Int64, next2Bytes()); break;

      //fload_x
      case J9BCfload: load(TR::Float, nextByte()); break;
      case J9BCfload0: load(TR::Float, 0); break;
      case J9BCfload1: load(TR::Float, 1); break;
      case J9BCfload2: load(TR::Float, 2); break;
      case J9BCfload3: load(TR::Float, 3); break;
      case J9BCfloadw: load(TR::Float, next2Bytes()); break;

      //dload_x
      case J9BCdload: load(TR::Double, nextByte()); break;
      case J9BCdload0: load(TR::Double, 0); break;
      case J9BCdload1: load(TR::Double, 1); break;
      case J9BCdload2: load(TR::Double, 2); break;
      case J9BCdload3: load(TR::Double, 3); break;
      case J9BCdloadw: load(TR::Double, next2Bytes()); break;

      //aload_x
      case J9BCaload: load(TR::Address, nextByte()); break;
      case J9BCaload0: load(TR::Address, 0); break;
      case J9BCaload1: load(TR::Address, 1); break;
      case J9BCaload2: load(TR::Address, 2); break;
      case J9BCaload3: load(TR::Address, 3); break;
      case J9BCaloadw: load(TR::Address, next2Bytes()); break;

      //x_aload
      case J9BCiaload: arrayLoad(TR::Int32); break;
      case J9BClaload: arrayLoad(TR::Int64); break;
      case J9BCfaload: arrayLoad(TR::Float); break;
      case J9BCaaload: arrayLoad(TR::Address); break;
      case J9BCdaload: arrayLoad(TR::Double); break;
      case J9BCcaload: arrayLoad(TR::Int16); break;
      case J9BCsaload: arrayLoad(TR::Int16); break;
      case J9BCbaload: arrayLoad(TR::Int8); break;

      //istore_x
      case J9BCistore: store(TR::Int32, nextByte()); break;
      case J9BCistore0: store(TR::Int32, 0); break;
      case J9BCistore1: store(TR::Int32, 1); break;
      case J9BCistore2: store(TR::Int32, 2); break;
      case J9BCistore3: store(TR::Int32, 3); break;
      case J9BCistorew: store(TR::Int32, next2Bytes()); break;

      //lstore_x
      case J9BClstore: store(TR::Int64, nextByte()); break;
      case J9BClstore0: store(TR::Int64, 0); break;
      case J9BClstore1: store(TR::Int64, 1); break;
      case J9BClstore2: store(TR::Int64, 2); break;
      case J9BClstore3: store(TR::Int64, 3); break;
      case J9BClstorew: store(TR::Int64, next2Bytes()); break; 

      //fstore_x
      case J9BCfstore: store(TR::Float, nextByte()); break;
      case J9BCfstore0: store(TR::Float, 0); break;
      case J9BCfstore1: store(TR::Float, 1); break;
      case J9BCfstore2: store(TR::Float, 2); break;
      case J9BCfstore3: store(TR::Float, 3); break;
      case J9BCfstorew: store(TR::Float, next2Bytes()); break; 
      
      //dstore_x
      case J9BCdstore: store(TR::Double, nextByte()); break;
      case J9BCdstorew: store(TR::Double, next2Bytes()); break; 
      case J9BCdstore0: store(TR::Double, 0); break;
      case J9BCdstore1: store(TR::Double, 1); break;
      case J9BCdstore2: store(TR::Double, 2); break;
      case J9BCdstore3: store(TR::Double, 3); break;

      //astore_x
      case J9BCastore: store(TR::Address, nextByte()); break;
      case J9BCastore0: store(TR::Address, 0); break;
      case J9BCastore1: store(TR::Address, 1); break;
      case J9BCastore2: store(TR::Address, 2); break;
      case J9BCastore3: store(TR::Address, 3); break;
      case J9BCastorew: store(TR::Address, next2Bytes()); break;

      //x_astore
      case J9BCiastore: arrayStore(TR::Int32); break;
      case J9BCfastore: arrayStore(TR::Float); break;
      case J9BCbastore: arrayStore(TR::Int8); break;
      case J9BCdastore: arrayStore(TR::Double); break;
      case J9BClastore: arrayStore(TR::Int64); break;
      case J9BCsastore: arrayStore(TR::Int16); break;
      case J9BCcastore: arrayStore(TR::Int16); break;
      case J9BCaastore: arrayStore(TR::Address); break;

      //pop_x
      case J9BCpop: pop(1); break;
      case J9BCpop2: pop(2); break;

      //dup_x
      case J9BCdup: dup(1, 0); break;
      case J9BCdupx1: dup(1, 1); break;
      case J9BCdupx2: dup(1, 2); break;
      case J9BCdup2: dup(2, 0); break;
      case J9BCdup2x1: dup(2, 1); break;
      case J9BCdup2x2: dup(2, 2); break;

      case J9BCswap: swap(); break;

      //x_add
      case J9BCiadd: binaryOperation(TR::Int32, BinaryOperator::plus); break;
      case J9BCdadd: binaryOperation(TR::Double, BinaryOperator::plus); break;
      case J9BCfadd: binaryOperation(TR::Float, BinaryOperator::plus); break;
      case J9BCladd: binaryOperation(TR::Int64, BinaryOperator::plus); break;

      //x_sub
      case J9BCisub: binaryOperation(TR::Int32, BinaryOperator::minus); break;
      case J9BCdsub: binaryOperation(TR::Double, BinaryOperator::minus); break;
      case J9BCfsub: binaryOperation(TR::Float, BinaryOperator::minus); break;
      case J9BClsub: binaryOperation(TR::Int64, BinaryOperator::minus); break;

      //x_mul
      case J9BCimul: binaryOperation(TR::Int32, BinaryOperator::mul); break;
      case J9BClmul: binaryOperation(TR::Int64, BinaryOperator::mul); break;
      case J9BCfmul: binaryOperation(TR::Float, BinaryOperator::mul); break;
      case J9BCdmul: binaryOperation(TR::Double, BinaryOperator::mul); break;

      //x_div
      case J9BCidiv: binaryOperation(TR::Int32, BinaryOperator::div); break;
      case J9BCddiv: binaryOperation(TR::Double, BinaryOperator::div); break;
      case J9BCfdiv: binaryOperation(TR::Float, BinaryOperator::div); break;
      case J9BCldiv: binaryOperation(TR::Int64, BinaryOperator::div); break;

      //x_rem
      case J9BCirem: binaryOperation(TR::Int32, BinaryOperator::rem); break;
      case J9BCfrem: binaryOperation(TR::Float, BinaryOperator::rem); break;
      case J9BClrem: binaryOperation(TR::Int64, BinaryOperator::rem); break;
      case J9BCdrem: binaryOperation(TR::Double, BinaryOperator::rem); break;

      //x_neg
      case J9BCineg: unaryOperation(TR::Int32, UnaryOperator::neg); break;
      case J9BCfneg: unaryOperation(TR::Float, UnaryOperator::neg); break;
      case J9BClneg: unaryOperation(TR::Int64, UnaryOperator::neg); break;
      case J9BCdneg: unaryOperation(TR::Double, UnaryOperator::neg); break;

      //x_sh_x
      case J9BCishl: shift(TR::Int32, ShiftOperator::shl); break;
      case J9BCishr: shift(TR::Int32, ShiftOperator::shr); break;
      case J9BCiushr: shift(TR::Int32, ShiftOperator::ushr); break;
      case J9BClshl: shift(TR::Int64, ShiftOperator::shl); break;
      case J9BClshr: shift(TR::Int64, ShiftOperator::shr); break;
      case J9BClushr: shift(TR::Int64, ShiftOperator::ushr); break;

      //x_and
      case J9BCiand: binaryOperation(TR::Int32, BinaryOperator::and_); break;
      case J9BCland: binaryOperation(TR::Int64, BinaryOperator::and_); break;

      //x_or
      case J9BCior: binaryOperation(TR::Int32, BinaryOperator::or_); break;
      case J9BClor: binaryOperation(TR::Int64, BinaryOperator::or_); break;

      //x_xor
      case J9BCixor: binaryOperation(TR::Int32, BinaryOperator::xor_); break;
      case J9BClxor: binaryOperation(TR::Int64, BinaryOperator::xor_); break;

      //iinc_x 

      case J9BCiinc: iinc(nextByte(), nextByteSigned()); break;
      case J9BCiincw: iinc(next2Bytes(), next2BytesSigned()); break;

      //i2_x
      case J9BCi2b: conversion(TR::Int32, TR::Int8); break;
      case J9BCi2c: conversion(TR::Int32, TR::Int16); break;
      case J9BCi2d: conversion(TR::Int32, TR::Double); break;
      case J9BCi2f: conversion(TR::Int32, TR::Float); break;
      case J9BCi2l: conversion(TR::Int32, TR::Int64); break;
      case J9BCi2s: conversion(TR::Int32, TR::Int16); break;
      
      //l2_x
      case J9BCl2d: conversion(TR::Int64, TR::Double); break;
      case J9BCl2f: conversion(TR::Int64, TR::Float); break;
      case J9BCl2i: conversion(TR::Int64, TR::Int32); break;

      //d2_x
      case J9BCd2f: conversion(TR::Double, TR::Float); break;
      case J9BCd2i: conversion(TR::Double, TR::Int32); break;
      case J9BCd2l: conversion(TR::Double, TR::Int64); break;

      //f2_x
      case J9BCf2d: conversion(TR::Float, TR::Double); break;
      case J9BCf2i: conversion(TR::Float, TR::Int32); break;
      case J9BCf2l: conversion(TR::Float, TR::Int64); break;

      //x_cmp_x
      case J9BCdcmpl: comparison(TR::Double, ComparisonOperator::cmp); break;
      case J9BCdcmpg: comparison(TR::Double, ComparisonOperator::cmp); break;
      case J9BCfcmpl: comparison(TR::Float, ComparisonOperator::cmp); break;
      case J9BCfcmpg: comparison(TR::Float, ComparisonOperator::cmp); break;
      case J9BClcmp: comparison(TR::Int64, ComparisonOperator::cmp); break;

      //if_x
      case J9BCifeq: conditionalBranch(TR::Int32, next2BytesSigned(), ConditionalBranchOperator::eq); break;
      case J9BCifge: conditionalBranch(TR::Int32, next2BytesSigned(), ConditionalBranchOperator::ge); break;
      case J9BCifgt: conditionalBranch(TR::Int32, next2BytesSigned(), ConditionalBranchOperator::gt); break;
      case J9BCifle: conditionalBranch(TR::Int32, next2BytesSigned(), ConditionalBranchOperator::le); break;
      case J9BCiflt: conditionalBranch(TR::Int32, next2BytesSigned(), ConditionalBranchOperator::lt); break;
      case J9BCifne: conditionalBranch(TR::Int32, next2BytesSigned(), ConditionalBranchOperator::ne); break;

      //if_x_null
      case J9BCifnonnull: conditionalBranch(TR::Address, next2BytesSigned(), ConditionalBranchOperator::nonnull); break;
      case J9BCifnull: conditionalBranch(TR::Address, next2BytesSigned(), ConditionalBranchOperator::null); break;

      //ificmp_x
      case J9BCificmpeq: conditionalBranch(TR::Int32, next2BytesSigned(), ConditionalBranchOperator::cmpeq); break;
      case J9BCificmpge: conditionalBranch(TR::Int32, next2BytesSigned(), ConditionalBranchOperator::cmpge); break;
      case J9BCificmpgt: conditionalBranch(TR::Int32, next2BytesSigned(), ConditionalBranchOperator::cmpgt); break;
      case J9BCificmple: conditionalBranch(TR::Int32, next2BytesSigned(), ConditionalBranchOperator::cmple); break;
      case J9BCificmplt: conditionalBranch(TR::Int32, next2BytesSigned(), ConditionalBranchOperator::cmplt); break;
      case J9BCificmpne: conditionalBranch(TR::Int32, next2BytesSigned(), ConditionalBranchOperator::cmpne); break;

      //ifacmp_x
      case J9BCifacmpeq: conditionalBranch(TR::Address, next2BytesSigned(), ConditionalBranchOperator::cmpeq); break; 
      case J9BCifacmpne: conditionalBranch(TR::Address, next2BytesSigned(), ConditionalBranchOperator::cmpne); break; 

      //goto_x
      case J9BCgoto: goto_(next2BytesSigned()); break;
      case J9BCgotow: goto_(next4BytesSigned()); break;

      //x_switch
      case J9BClookupswitch: switch_(true); break;
      case J9BCtableswitch: switch_(false); break;

      //get_x
      case J9BCgetfield: get(false); break;
      case J9BCgetstatic: get(true); break;

      //put_x
      case J9BCputfield: put(false); break;
      case J9BCputstatic: put(true); break;

      //x_newarray
      case J9BCnewarray: newarray(); break;
      case J9BCanewarray: anewarray(); break;
      case J9BCmultianewarray: multianewarray(nextByte(3)); break;

      //monitor_x
      case J9BCmonitorenter: monitor(true); break;
      case J9BCmonitorexit: monitor(false); break;
      
      case J9BCnew: new_(); break;

      case J9BCarraylength: arraylength(); break;

      case J9BCathrow: athrow(); break;
      
      case J9BCcheckcast: checkcast(); break;

      case J9BCinstanceof: instanceof(); break;

      case J9BCwide: 
         {
         int32_t wopcode = nextByte();
         TR_J9ByteCode wbc = convertOpCodeToByteCodeEnum(wopcode);

         switch (wbc)
            {
            case J9BCiinc: iinc(next2Bytes(2), next2BytesSigned(4)); break;
            
            case J9BCiload: load(TR::Int32, next2Bytes(2)); break;
            case J9BClload: load(TR::Int64, next2Bytes(2)); break;
            case J9BCfload: load(TR::Float, next2Bytes(2)); break;
            case J9BCdload: load(TR::Double, next2Bytes(2)); break;
            case J9BCaload: load(TR::Address, next2Bytes(2)); break;

            case J9BCistore: store(TR::Int32, next2Bytes(2)); break;
            case J9BClstore: store(TR::Int64, next2Bytes(2)); break;
            case J9BCfstore: store(TR::Float, next2Bytes(2)); break;
            case J9BCdstore: store(TR::Double, next2Bytes(2)); break;
            case J9BCastore: store(TR::Address, next2Bytes(2)); break;
            default: 
               break;
            }
         break;
         }

      //invoke_x
      case J9BCinvokedynamic:
         if (comp()->getOption(TR_TraceAbstractInterpretation)) 
            traceMsg(comp(), "Encounter invokedynamic. Abort abstract interpreting method %s.\n", _callerMethodSymbol->signature(comp()->trMemory())); 
            return false; 
            break;
      case J9BCinvokeinterface: invoke(TR::MethodSymbol::Kinds::Interface); break;
      case J9BCinvokeinterface2: /*how should we handle invokeinterface2? */ break;
      case J9BCinvokespecial: invoke(TR::MethodSymbol::Kinds::Special); break;
      case J9BCinvokestatic: invoke(TR::MethodSymbol::Kinds::Static); break;
      case J9BCinvokevirtual: invoke(TR::MethodSymbol::Kinds::Virtual); break;
      case J9BCinvokespecialsplit: invoke(TR::MethodSymbol::Kinds::Special); break;
      case J9BCinvokestaticsplit: invoke(TR::MethodSymbol::Kinds::Static); break;
   
      //return_x: to be implemented in the future
      case J9BCReturnC: return_(TR::Int16); break;
      case J9BCReturnS: return_(TR::Int16); break;
      case J9BCReturnB: return_(TR::Int8); break;
      case J9BCReturnZ: return_(TR::Int32, true); break; //mask the lowest bit and return
      case J9BCgenericReturn: return_(_callerMethod->returnType()); break; 
      
      default:
      break;
      }

   return true; //This bytecode is successfully interpreted
   }

void J9::AbsInterpreter::return_(TR::DataType type, bool oneBit)
   {
   if (type == TR::NoType)
      return;

   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());
   if (type.isDouble() || type.isInt64())
      state->pop();

   TR::AbsValue* value = state->pop();
   TR_ASSERT_FATAL(type == TR::Int16 || type == TR::Int8 ? value->getDataType() == TR::Int32 : value->getDataType() == type, "Unexpected type");
   }

void J9::AbsInterpreter::constant(int32_t i)
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());
   TR::AbsValue* value = createIntConst(i);
   state->push(value);
   }

void J9::AbsInterpreter::constant(int64_t l)
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());
   TR::AbsValue* value1 = createLongConst(l);
   TR::AbsValue* value2 = createTopLong();
   state->push(value1);
   state->push(value2);
   }

void J9::AbsInterpreter::constant(float f)
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());
   TR::AbsValue* floatConst = createTopFloat();
   state->push(floatConst);
   }

void J9::AbsInterpreter::constant(double d)
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());
   TR::AbsValue *value1 = createTopDouble();
   TR::AbsValue *value2 = createTopDouble();
   state->push(value1);
   state->push(value2);
   }

void J9::AbsInterpreter::constantNull()
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());
   TR::AbsValue* value = createNullObject();
   state->push(value);
   }

void J9::AbsInterpreter::ldc(bool wide)
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());

   int32_t cpIndex = wide ? next2Bytes() : nextByte();
   TR::DataType type = _callerMethod->getLDCType(cpIndex);

   switch (type)
      {
      case TR::Int32:
         {
         int32_t intVal = _callerMethod->intConstant(cpIndex);
         constant((int32_t)intVal);
         break;
         }
      case TR::Int64:
         {
         int64_t longVal = _callerMethod->longConstant(cpIndex);
         constant((int64_t)longVal);
         break;
         }
      case TR::Double:
         {
         constant((double)0); //the value does not matter here since doubles are modeled as TOP
         break;
         }
      case TR::Float:
         {
         constant((float)0); //same for float
         break;
         }
      case TR::Address:
         {
         bool isString = _callerMethod->isStringConstant(cpIndex);
         if (isString) 
            {
            TR::SymbolReference *symbolReference = comp()->getSymRefTab()->findOrCreateStringSymbol(_callerMethodSymbol, cpIndex);
            if (symbolReference->isUnresolved())
               {
               state->push(createTopObject());
               }
            else  //Resolved
               {
               TR::AbsValue *stringVal = createStringObject(symbolReference, TR_yes);
               state->push(stringVal);
               }
            }
         else  //Class
            {
            TR_OpaqueClassBlock* classBlock = _callerMethod->getClassFromConstantPool(comp(), cpIndex);
            TR::AbsValue* value = createObject(classBlock, TR_yes);
            state->push(value);
            }
         break;
         }
      default:
         TR_ASSERT_FATAL(false, "Invalid type");   
         break;
      }
   }

void J9::AbsInterpreter::load(TR::DataType type, int32_t index)
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());
   
   switch (type)
      {
      case TR::Int32:
      case TR::Float:
      case TR::Address:
         {
         TR::AbsValue *value = state->at(index);
         TR_ASSERT_FATAL(value->getDataType() == type, "Unexpected type");
         state->push(value);
         break;
         }
      case TR::Int64:
      case TR::Double:
         {
         TR::AbsValue *value1 = state->at(index);
         TR_ASSERT_FATAL(value1->getDataType() == type, "Unexpected type");
         TR::AbsValue *value2 = state->at(index + 1);
         state->push(value1);
         state->push(value2);
         break;
         }
      default:
         TR_ASSERT_FATAL(false, "Invalid type");
         break;
      }
   }

void J9::AbsInterpreter::store(TR::DataType type, int32_t index)
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());

   switch (type)
      {
      case TR::Int32:
      case TR::Float:
      case TR::Address:
         {
         TR::AbsValue* value = state->pop();
         TR_ASSERT_FATAL(value->getDataType() == type, "Unexpected type");
         state->set(index, value);
         break;
         }
      case TR::Int64:
      case TR::Double:
         {
         TR::AbsValue* value2 = state->pop();
         TR::AbsValue* value1 = state->pop();
         TR_ASSERT_FATAL(value1->getDataType() == type, "Unexpected type");
         state->set(index, value1);
         state->set(index+1, value2);
         break;
         }
      default:
         TR_ASSERT_FATAL(false, "Invalid type");
         break;
      }
   }

void J9::AbsInterpreter::arrayLoad(TR::DataType type)
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());

   TR::AbsValue *index = state->pop();
   TR_ASSERT_FATAL(index->getDataType() == TR::Int32, "Unexpected type");

   TR::AbsValue *arrayRef = state->pop();
   TR_ASSERT_FATAL(arrayRef->getDataType() == TR::Address, "Unexpected type");

   switch (type)
      {
      case TR::Double:
         {
         TR::AbsValue *value1 = createTopDouble();
         TR::AbsValue *value2 = createTopDouble();
         state->push(value1);
         state->push(value2);
         break;
         }
      case TR::Float:
         {
         TR::AbsValue* value = createTopFloat();
         state->push(value);
         break;
         }
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
         {
         TR::AbsValue *value = createTopInt();
         state->push(value);
         break;
         }
      case TR::Int64:
         {
         TR::AbsValue *value1 = createTopLong();
         TR::AbsValue *value2 = createTopLong();
         state->push(value1);
         state->push(value2);
         break;
         }
      case TR::Address:
         {
         TR::AbsValue* value = createTopObject();
         state->push(value);
         break;
         }
      default:
         TR_ASSERT_FATAL(false, "Invalid type");  
         break;
      }
   }

void J9::AbsInterpreter::arrayStore(TR::DataType type)
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());

   if (type.isDouble() || type.isInt64())
      state->pop(); //dummy

   TR::AbsValue* value = state->pop();
   TR_ASSERT_FATAL(type == TR::Int8 || type == TR::Int16 ? value->getDataType() == TR::Int32 : value->getDataType() == type, "Unexpected type");

   TR::AbsValue *index = state->pop();
   TR_ASSERT_FATAL(index->getDataType() == TR::Int32, "Unexpected type");

   TR::AbsValue *arrayRef = state->pop();
   TR_ASSERT_FATAL(arrayRef->getDataType() == TR::Address, "Unexpected type");

   //heap is being not modeled
   switch (type)
      {
      case TR::Double:
      case TR::Float:
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
      case TR::Int64:
      case TR::Address:
         break;
      default:
         TR_ASSERT_FATAL(false, "Invalid type");  
         break;
      }
   }

void J9::AbsInterpreter::binaryOperation(TR::DataType type, BinaryOperator op)
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());

   if (type.isDouble() || type.isInt64())
      state->pop(); //dummy
      
   TR::AbsValue* value2 = state->pop();
   TR_ASSERT_FATAL(value2->getDataType() == type, "Unexpected type");

   if (type.isDouble() || type.isInt64())
      state->pop(); //dummy
      
   TR::AbsValue* value1 = state->pop();
   TR_ASSERT_FATAL(value1->getDataType() == type, "Unexpected type");

   switch (type)
      {
      // float and double are not modeled
      case TR::Float:
         {
         TR::AbsValue *result = createTopFloat();
         state->push(result);
         break;
         }

      case TR::Double:
         {
         TR::AbsValue *result1 = createTopDouble();
         TR::AbsValue *result2 = createTopDouble();
         state->push(result1);
         state->push(result2);
         break;
         }

      //The following types are modeled
      case TR::Int32:
         {
         if (isIntConst(value1) && isIntConst(value2)) //both int const
            {
            int32_t intVal1 = static_cast<TR::AbsVPValue*>(value1)->getConstraint()->asIntConst()->getInt();
            int32_t intVal2 = static_cast<TR::AbsVPValue*>(value2)->getConstraint()->asIntConst()->getInt();

            if (intVal2 == 0 && op == BinaryOperator::div) //divide by zero exception
               {
               state->push(createTopInt());
               break;
               }

            int32_t resultVal;

            switch (op)
               {
               case BinaryOperator::plus: 
                  resultVal = intVal1 + intVal2;
                  break;
               case BinaryOperator::minus: 
                  resultVal = intVal1 - intVal2;
                  break;

               case BinaryOperator::mul:
                  resultVal = intVal1 * intVal2;
                  break;

               case BinaryOperator::div:
                  resultVal = intVal1 / intVal2;
                  break;

               case BinaryOperator::rem:
                  resultVal = intVal1 % intVal2;
                  break;

               case BinaryOperator::and_:
                  resultVal = intVal1 & intVal2;
                  break;

               case BinaryOperator::or_:
                  resultVal = intVal1 | intVal2;
                  break;

               case BinaryOperator::xor_:
                  resultVal = intVal1 ^ intVal2;
                  break;

               default:
                  TR_ASSERT_FATAL(false, "Invalid binary op type");
                  break;
               }

            TR::AbsValue* result = createIntConst(resultVal);
            state->push(result);
            break;
            }
         else  //not const
            {
            state->push(createTopInt());
            break;
            }
         }

      case TR::Int64:
         {
         if (isLongConst(value1) && isLongConst(value2)) //both long const
            {
            int64_t longVal1 = static_cast<TR::AbsVPValue*>(value1)->getConstraint()->asLongConst()->getLong();
            int64_t longVal2 = static_cast<TR::AbsVPValue*>(value2)->getConstraint()->asLongConst()->getLong();

            if (longVal2 == 0 && op == BinaryOperator::div) //divide by zero exception
               {
               TR::AbsValue* result1 = createTopLong();
               TR::AbsValue* result2 = createTopLong();
               state->push(result1);
               state->push(result2);
               break;
               }

            int64_t resultVal;
            switch (op)
               {
               case BinaryOperator::plus: 
                  resultVal = longVal1 + longVal2;
                  break;
               case BinaryOperator::minus: 
                  resultVal = longVal1 - longVal2;
                  break;

               case BinaryOperator::mul:
                  resultVal = longVal1 * longVal2;
                  break;

               case BinaryOperator::div:
                  resultVal = longVal1 / longVal2;
                  break;

               case BinaryOperator::rem:
                  resultVal = longVal1 % longVal2;
                  break;

               case BinaryOperator::and_:
                  resultVal = longVal1 & longVal2;
                  break;

               case BinaryOperator::or_:
                  resultVal = longVal1 | longVal2;
                  break;

               case BinaryOperator::xor_:
                  resultVal = longVal1 ^ longVal2;
                  break;

               default:
                  TR_ASSERT_FATAL(false, "Invalid binary op type");
                  break;
               }
            
            TR::AbsValue* result1 = createLongConst(resultVal);
            TR::AbsValue* result2 = createTopLong();
            state->push(result1);
            state->push(result2);
            break;
            }
         else  //not const
            {
            TR::AbsValue* result1 = createTopLong();
            TR::AbsValue* result2 = createTopLong();
            state->push(result1);
            state->push(result2);
            break;
            }
         }
      default:
         TR_ASSERT_FATAL(false, "Invalid type");
         break;
      }
   }

void J9::AbsInterpreter::unaryOperation(TR::DataType type, UnaryOperator op)
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());
   if (type.isDouble() || type.isInt64())
      state->pop();
   
   TR::AbsValue* value = state->pop();
   TR_ASSERT_FATAL(value->getDataType() == type, "Unexpected type");

   switch (type)
      {
      case TR::Float:
         {
         TR::AbsValue* result = createTopFloat();
         state->push(result);
         break;
         }

      case TR::Double:
         {
         TR::AbsValue* result1 = createTopDouble();
         TR::AbsValue* result2 = createTopDouble();
         state->push(result1);
         state->push(result2);
         break;
         }

      case TR::Int32:
         {
         if (isIntConst(value)) //const int
            {
            int32_t intVal = static_cast<TR::AbsVPValue*>(value)->getConstraint()->asIntConst()->getInt();
            TR::AbsValue* result = createIntConst(-intVal);
            state->push(result);
            break;
            }
         else if (isIntRange(value)) //range int
            {
            int32_t intValLow = static_cast<TR::AbsVPValue*>(value)->getConstraint()->asIntRange()->getLowInt();
            int32_t intValHigh = static_cast<TR::AbsVPValue*>(value)->getConstraint()->asIntRange()->getHighInt();

            if (intValLow == INT32_MIN) //neg INT_MIN = INT_MIN
               {
               state->push(createTopInt());
               break;
               }

            TR::AbsValue* result = createIntRange(-intValHigh, -intValLow);
            state->push(result);
            break;
            }
         else  //other cases
            {
            TR::AbsValue* result = createTopInt();
            state->push(result);
            break;
            }
         break;
         }

      case TR::Int64:
         {
         if (isLongConst(value))
            {
            int64_t longVal = static_cast<TR::AbsVPValue*>(value)->getConstraint()->asLongConst()->getLong();
            TR::AbsValue* result1 = createLongConst(-longVal);
            TR::AbsValue* result2 = createTopLong();
            state->push(result1);
            state->push(result2);
            break;
            }
         else if (isLongRange(value))
            {
            int64_t longValLow = static_cast<TR::AbsVPValue*>(value)->getConstraint()->asLongRange()->getLowLong();
            int64_t longValHigh = static_cast<TR::AbsVPValue*>(value)->getConstraint()->asLongRange()->getHighLong();

            if (longValLow == LONG_MIN)
               {
               state->push(createTopLong());
               state->push(createTopLong());
               break;
               }

            TR::AbsValue* result1 = createLongRange(-longValHigh, -longValLow);
            TR::AbsValue* result2 = createTopLong();
            state->push(result1);
            state->push(result2);
            break;
            }
         else 
            {
            TR::AbsValue* result1 = createTopLong();
            TR::AbsValue* result2 = createTopLong();
            state->push(result1);
            state->push(result2);
            break;
            }
         break;
         }
      
      default:
         TR_ASSERT_FATAL(false, "Invalid data type");
         break;
      }
   }

void J9::AbsInterpreter::pop(int32_t size)
   {
   TR_ASSERT_FATAL(size >0 && size <= 2, "Invalid pop size");
   for (int32_t i = 0; i < size; i ++)
      {
      static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState())->pop();
      }
   }

void J9::AbsInterpreter::nop()
   {
   }

void J9::AbsInterpreter::swap()
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());
   TR::AbsValue* value1 = state->pop();
   TR::AbsValue* value2 = state->pop();
   state->push(value1);
   state->push(value2);
   }

void J9::AbsInterpreter::dup(int32_t size, int32_t delta)  
   {
   TR_ASSERT_FATAL(size > 0 && size <= 2, "Invalid dup size");
   TR_ASSERT_FATAL(delta >= 0 && size <=2, "Invalid dup delta");

   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());

   TR::AbsValue* temp[size + delta];

   for (int32_t i = 0; i < size + delta; i ++)
      temp[i] = state->pop();
   
   for (int32_t i = size - 1 ; i >= 0 ; i --)
      state->push(temp[i]->clone(region())); //copy the top X values of the stack

   for (int32_t i = size + delta - 1; i >= 0 ; i --)
      state->push(temp[i]); //push the values back to stack
   }

void J9::AbsInterpreter::shift(TR::DataType type, ShiftOperator op)
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());

   TR::AbsValue* shiftAmount = state->pop();

   if (type.isInt64())
      state->pop();
      
   TR::AbsValue* value = state->pop();

   switch (type)
      {
      case TR::Int32:
         {
         if (isIntConst(value) && isIntConst(shiftAmount))
            {
            int32_t intVal = static_cast<TR::AbsVPValue*>(value)->getConstraint()->asIntConst()->getInt();
            int32_t shiftAmountVal = static_cast<TR::AbsVPValue*>(shiftAmount)->getConstraint()->asIntConst()->getInt();
            int32_t resultVal;
            switch (op)
               {
               case ShiftOperator::shl:
                  resultVal = intVal << shiftAmountVal;
                  break;
               case ShiftOperator::shr:
                  resultVal = intVal >> shiftAmountVal;
                  break;
               case ShiftOperator::ushr:
                  resultVal = (uint32_t)intVal >> shiftAmountVal; 
                  break;
               default:
                  TR_ASSERT_FATAL(false, "Invalid shift operator");
                  break;
               }
            TR::AbsValue* result = createIntConst(resultVal);
            state->push(result);
            break;
            }
         else 
            {
            TR::AbsValue* result = createTopInt();
            state->push(result);
            break;  
            }
         break;
         }
         
      case TR::Int64:
         {
         if (isLongConst(value) && isIntConst(shiftAmount))
            {
            int64_t longVal = static_cast<TR::AbsVPValue*>(value)->getConstraint()->asLongConst()->getLong();
            int32_t shiftAmountVal = static_cast<TR::AbsVPValue*>(shiftAmount)->getConstraint()->asIntConst()->getInt();
            int64_t resultVal;
            switch (op)
               {
               case ShiftOperator::shl:
                  resultVal = longVal << shiftAmountVal;
                  break;
               case ShiftOperator::shr:
                  resultVal = longVal >> shiftAmountVal;
                  break;
               case ShiftOperator::ushr:
                  resultVal = (uint64_t)longVal >> shiftAmountVal; 
                  break;
               default:
                  TR_ASSERT_FATAL(false, "Invalid shift operator");
                  break;
               }
            TR::AbsValue* result1 = createLongConst(resultVal);
            TR::AbsValue* result2 = createTopLong();
            state->push(result1);
            state->push(result2);
            break;
            }
         else 
            {
            TR::AbsValue* result1 = createTopLong();
            TR::AbsValue* result2 = createTopLong();
            state->push(result1);
            state->push(result2);
            break;
            }
         break;
         }
      default:
         TR_ASSERT_FATAL(false, "Invalid type");
         break;
      }
   }

void J9::AbsInterpreter::conversion(TR::DataType fromType, TR::DataType toType)
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());

   if (fromType.isDouble() || fromType.isInt64())
      state->pop(); //dummy
      
   TR::AbsValue* value = state->pop();
   TR_ASSERT_FATAL(value->getDataType() == fromType, "Unexpected type");

   switch (fromType)
      {
      /*** Convert from int to X ***/
      case TR::Int32:
         {
         switch (toType)
            {
            case TR::Int8: //i2b
               {
               TR::AbsValue* result = isIntConst(value) ? 
                  createIntConst((int8_t)static_cast<TR::AbsVPValue*>(value)->getConstraint()->asIntConst()->getInt())
                  :
                  createTopInt();
               
               state->push(result);
               break;
               }

            case TR::Int16: //i2c or i2s
               {
               TR::AbsValue* result = isIntConst(value) ? 
                  createIntConst((int16_t)static_cast<TR::AbsVPValue*>(value)->getConstraint()->asIntConst()->getInt())
                  :
                  createTopInt();
               
               state->push(result);
               break;
               }
            
            case TR::Int64: //i2l
               {
               TR::AbsValue* result1 = isIntConst(value) ?
                  createLongConst(static_cast<TR::AbsVPValue*>(value)->getConstraint()->asIntConst()->getInt())
                  :
                  createTopLong();
               
               TR::AbsValue* result2 = createTopLong();
               state->push(result1);
               state->push(result2);
               break;
               }

            case TR::Float: //i2f
               {
               TR::AbsValue* result = createTopFloat();
               state->push(result);
               break;
               }

            case TR::Double: //i2d
               {
               TR::AbsValue* result1 = createTopDouble();
               TR::AbsValue* result2 = createTopDouble();
               state->push(result1);
               state->push(result2);
               break;
               }
            
            default:
               TR_ASSERT_FATAL(false, "Invalid toType");
               break;
            }
         break;
         }

      /*** Convert from long to X ***/
      case TR::Int64:
         {
         switch (toType)
            {            
            case TR::Int32: //l2i
               {
               TR::AbsValue* result = isLongConst(value) ?
                  createIntConst((int32_t)static_cast<TR::AbsVPValue*>(value)->getConstraint()->asLongConst()->getLong())
                  :
                  createTopInt();

               state->push(result);
               break;
               }

            case TR::Float: //l2f
               {
               TR::AbsValue* result = createTopFloat();
               state->push(result);
               break;
               }

            case TR::Double: //l2d
               {
               TR::AbsValue* result1 = createTopDouble();
               TR::AbsValue* result2 = createTopDouble();
               state->push(result1);
               state->push(result2);
               break;
               }
            
            default:
               TR_ASSERT_FATAL(false, "Invalid toType");
               break;
            }
         break;
         }

      /*** Convert from double to X ***/
      case TR::Double:
         {
         switch (toType)
            {            
            case TR::Int32: //d2i
               {
               TR::AbsValue* result = createTopInt();
               state->push(result);
               break;
               }

            case TR::Float: //d2f
               {
               TR::AbsValue* result = createTopFloat();
               state->push(result);
               break;
               }

            case TR::Int64: //d2l
               {
               TR::AbsValue* result1 = createTopLong();
               TR::AbsValue* result2 = createTopLong();
               state->push(result1);
               state->push(result2);
               break;
               }
            
            default:
               TR_ASSERT_FATAL(false, "Invalid toType");
               break;
            }
         break;
         }

         /*** Convert from float to X ***/
         case TR::Float:
            {
            switch (toType)
               {            
               case TR::Int32: //f2i
                  {
                  TR::AbsValue* result = createTopInt();
                  state->push(result);
                  break;
                  }

               case TR::Double: //f2d
                  {
                  TR::AbsValue* result1 = createTopDouble();
                  TR::AbsValue* result2 = createTopDouble();
                  state->push(result1);
                  state->push(result2);
                  break;
                  }

               case TR::Int64: //f2l
                  {
                  TR::AbsValue* result1 = createTopLong();
                  TR::AbsValue* result2 = createTopLong();
                  state->push(result1);
                  state->push(result2);
                  break;
                  }
               
               default:
                  TR_ASSERT_FATAL(false, "Invalid toType");
                  break;
               }
            break;
            }

         default:
            TR_ASSERT_FATAL(false, "Invalid fromType");
            break;
      }
   }
   

void J9::AbsInterpreter::comparison(TR::DataType type, ComparisonOperator op)
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());

   if (type.isDouble() || type.isInt64())
      state->pop();

   TR::AbsValue* value2 = state->pop();
   TR_ASSERT_FATAL(value2->getDataType() == type, "Unexpected type");

   if (type.isDouble() || type.isInt64())
      state->pop();

   TR::AbsValue* value1 = state->pop();
   TR_ASSERT_FATAL(value1->getDataType() == type, "Unexpected type");

   switch(type)
      {
      case TR::Float:
      case TR::Double:
         {
         TR::AbsValue* result = createIntRange(-1, 1);
         state->push(result);
         break;
         }
      case TR::Int64:
         {
         if (isLong(value1) && isLong(value2)) //long
            {
            if (isLongConst(value1) && isLongConst(value2) // ==
               && static_cast<TR::AbsVPValue*>(value1)->getConstraint()->asLongConst()->getLong() == static_cast<TR::AbsVPValue*>(value2)->getConstraint()->asLongConst()->getLong()) 
               {
               TR::AbsValue* result = createIntConst(0);
               state->push(result);
               break;
               }
            else if (static_cast<TR::AbsVPValue*>(value1)->getConstraint()->asLongConstraint()->getLowLong() > static_cast<TR::AbsVPValue*>(value2)->getConstraint()->asLongConstraint()->getHighLong()) // >
               {
               TR::AbsValue* result = createIntConst(1);
               state->push(result);
               break;
               }
            else if (static_cast<TR::AbsVPValue*>(value1)->getConstraint()->asLongConstraint()->getHighLong() < static_cast<TR::AbsVPValue*>(value2)->getConstraint()->asLongConstraint()->getLowLong()) // <
               {
               TR::AbsValue* result = createIntConst(-1);
               state->push(result);
               break;
               }
            else 
               {
               TR::AbsValue* result = createIntRange(-1, 1);
               state->push(result);
               break;
               }
            }
         else 
            {
            TR::AbsValue* result = createIntRange(-1,1);
            state->push(result);
            break;
            }
         }

      default:
         TR_ASSERT_FATAL(false, "Invalid type");
         break;
      }
   }

void J9::AbsInterpreter::goto_(int32_t label)
   {
   }

void J9::AbsInterpreter::conditionalBranch(TR::DataType type, int32_t label, ConditionalBranchOperator op)
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());

   switch(op)
      {
      /*** ifnull ***/
      case ConditionalBranchOperator::null:
         {
         TR::AbsValue* value = state->pop();
         TR_ASSERT_FATAL(value->getDataType() == TR::Address, "Unexpected type");
         
         if (value->isParameter() && !value->isImplicitParameter())
            {
            TR::PotentialOptimizationVPPredicate* p1 = 
               new (region()) TR::PotentialOptimizationVPPredicate(TR::VPNullObject::create(vp()), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::BranchFolding, vp());
            TR::PotentialOptimizationVPPredicate* p2 = 
               new (region()) TR::PotentialOptimizationVPPredicate(TR::VPNonNullObject::create(vp()), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::BranchFolding, vp());

            _inliningMethodSummary->addPotentialOptimizationByArgument(p1, value->getParamPosition());
            _inliningMethodSummary->addPotentialOptimizationByArgument(p2, value->getParamPosition());
            }
         
         switch (type)
            {
            case TR::Address:
               break;
            default:
               TR_ASSERT_FATAL(false, "Invalid type");
               break;
            }

         break;
         }
      
      /*** ifnonnull ***/
      case ConditionalBranchOperator::nonnull:
         {
         TR::AbsValue* value = state->pop();
         TR_ASSERT_FATAL(value->getDataType() == TR::Address, "Unexpected type");

         if (value->isParameter() && !value->isImplicitParameter())
            {
            TR::PotentialOptimizationVPPredicate* p1 = 
               new (region()) TR::PotentialOptimizationVPPredicate(TR::VPNullObject::create(vp()), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::BranchFolding, vp());
            TR::PotentialOptimizationVPPredicate* p2 = 
               new (region()) TR::PotentialOptimizationVPPredicate(TR::VPNonNullObject::create(vp()), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::BranchFolding, vp());

            _inliningMethodSummary->addPotentialOptimizationByArgument(p1, value->getParamPosition());
            _inliningMethodSummary->addPotentialOptimizationByArgument(p2, value->getParamPosition());
            }

         switch (type)
            {
            case TR::Address:
               break;
            default:
               TR_ASSERT_FATAL(false, "Invalid type");
               break;
            }

         break;
         }

      /*** ifeq ***/
      case ConditionalBranchOperator::eq:
         {
         TR::AbsValue* value = state->pop();
         TR_ASSERT_FATAL(value->getDataType() == TR::Int32, "Unexpected type");

         if (value->isParameter())
            {
            TR::PotentialOptimizationVPPredicate* p1 = 
               new (region()) TR::PotentialOptimizationVPPredicate(TR::VPIntConst::create(vp(), 0), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::BranchFolding, vp());
            TR::PotentialOptimizationVPPredicate* p2 = 
               new (region()) TR::PotentialOptimizationVPPredicate(TR::VPIntRange::create(vp(), INT_MIN, -1), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::BranchFolding, vp());
            TR::PotentialOptimizationVPPredicate* p3 = 
               new (region()) TR::PotentialOptimizationVPPredicate(TR::VPIntRange::create(vp(), 1, INT_MAX), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::BranchFolding, vp());

            _inliningMethodSummary->addPotentialOptimizationByArgument(p1, value->getParamPosition());
            _inliningMethodSummary->addPotentialOptimizationByArgument(p2, value->getParamPosition());
            _inliningMethodSummary->addPotentialOptimizationByArgument(p3, value->getParamPosition());
            }

         switch (type)
            {
            case TR::Int32:
               break;
            default:
               TR_ASSERT_FATAL(false, "Invalid type");
               break;
            }
         break;
         }

      /*** ifne ***/
      case ConditionalBranchOperator::ne:
         {
         TR::AbsValue* value = state->pop();
         TR_ASSERT_FATAL(value->getDataType() == TR::Int32, "Unexpected type");

         if (value->isParameter())
            {
            TR::PotentialOptimizationVPPredicate* p1 = 
               new (region()) TR::PotentialOptimizationVPPredicate(TR::VPIntConst::create(vp(), 0), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::BranchFolding, vp());
            TR::PotentialOptimizationVPPredicate* p2 = 
               new (region()) TR::PotentialOptimizationVPPredicate(TR::VPIntRange::create(vp(), INT_MIN, -1), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::BranchFolding, vp());
            TR::PotentialOptimizationVPPredicate* p3 = 
               new (region()) TR::PotentialOptimizationVPPredicate(TR::VPIntRange::create(vp(), 1, INT_MAX), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::BranchFolding, vp());

            _inliningMethodSummary->addPotentialOptimizationByArgument(p1, value->getParamPosition());
            _inliningMethodSummary->addPotentialOptimizationByArgument(p2, value->getParamPosition());
            _inliningMethodSummary->addPotentialOptimizationByArgument(p3, value->getParamPosition());
            }

         switch (type)
            {
            case TR::Int32:
               break;
            default:
               TR_ASSERT_FATAL(false, "Invalid type");
               break;
            }
         break;
         }

      /*** ifge ***/
      case ConditionalBranchOperator::ge:
         {
         TR::AbsValue* value = state->pop();
         TR_ASSERT_FATAL(value->getDataType() == TR::Int32, "Unexpected type");

         if (value->isParameter())
            {
            if (isInt(value))
               {
               TR::PotentialOptimizationVPPredicate* p1 = 
                  new (region()) TR::PotentialOptimizationVPPredicate(TR::VPIntRange::create(vp(), INT_MIN, -1), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::BranchFolding, vp());
               TR::PotentialOptimizationVPPredicate* p2 = 
                  new (region()) TR::PotentialOptimizationVPPredicate(TR::VPIntRange::create(vp(), 0, INT_MAX), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::BranchFolding, vp());
            
               _inliningMethodSummary->addPotentialOptimizationByArgument(p1, value->getParamPosition());
               _inliningMethodSummary->addPotentialOptimizationByArgument(p2, value->getParamPosition());
               }
            }

         switch (type)
            {
            case TR::Int32:
               break;
            default:
               TR_ASSERT_FATAL(false, "Invalid type");
               break;
            }
         break;
         }

      /*** ifgt ***/
      case ConditionalBranchOperator::gt:
         {
         TR::AbsValue* value = state->pop();
         TR_ASSERT_FATAL(value->getDataType() == TR::Int32, "Unexpected type");
         
         if (value->isParameter())
            {
            TR::PotentialOptimizationVPPredicate* p1 = 
               new (region()) TR::PotentialOptimizationVPPredicate(TR::VPIntRange::create(vp(), INT_MIN, 0), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::BranchFolding, vp());
            TR::PotentialOptimizationVPPredicate* p2 = 
               new (region()) TR::PotentialOptimizationVPPredicate(TR::VPIntRange::create(vp(), 1, INT_MAX), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::BranchFolding, vp());
            
            _inliningMethodSummary->addPotentialOptimizationByArgument(p1, value->getParamPosition());
            _inliningMethodSummary->addPotentialOptimizationByArgument(p2, value->getParamPosition());
            }

         switch (type)
            {
            case TR::Int32:
               break;
            default:
               TR_ASSERT_FATAL(false, "Invalid type");
               break;
            }
         break;
         }

      /*** ifle ***/
      case ConditionalBranchOperator::le:
         {
         TR::AbsValue* value = state->pop();
         TR_ASSERT_FATAL(value->getDataType() == TR::Int32, "Unexpected type");

         if (value->isParameter())
            {
            TR::PotentialOptimizationVPPredicate* p1 = 
               new (region()) TR::PotentialOptimizationVPPredicate(TR::VPIntRange::create(vp(), INT_MIN, 0), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::BranchFolding, vp());
            TR::PotentialOptimizationVPPredicate* p2 = 
               new (region()) TR::PotentialOptimizationVPPredicate(TR::VPIntRange::create(vp(), 1, INT_MAX), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::BranchFolding, vp());
            
            _inliningMethodSummary->addPotentialOptimizationByArgument(p1, value->getParamPosition());
            _inliningMethodSummary->addPotentialOptimizationByArgument(p2, value->getParamPosition());
            }

         switch (type)
            {
            case TR::Int32:
               break;
            default:
               TR_ASSERT_FATAL(false, "Invalid type");
               break;
            }
         break;
         }

      /*** iflt ***/
      case ConditionalBranchOperator::lt:
         {
         TR::AbsValue* value = state->pop();
         TR_ASSERT_FATAL(value->getDataType() == TR::Int32, "Unexpected type");

         if (value->isParameter())
            {
            TR::PotentialOptimizationVPPredicate* p1 = 
                  new (region()) TR::PotentialOptimizationVPPredicate(TR::VPIntRange::create(vp(), INT_MIN, -1), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::BranchFolding, vp());
            TR::PotentialOptimizationVPPredicate* p2 = 
               new (region()) TR::PotentialOptimizationVPPredicate(TR::VPIntRange::create(vp(), 0, INT_MAX), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::BranchFolding, vp());
         
            _inliningMethodSummary->addPotentialOptimizationByArgument(p1, value->getParamPosition());
            _inliningMethodSummary->addPotentialOptimizationByArgument(p2, value->getParamPosition());
            }

         switch (type)
            {
            case TR::Int32:
               break;
            default:
               TR_ASSERT_FATAL(false, "Invalid type");
               break;
            }
         break;
         }
      
      /*** if_cmpeq ***/
      case ConditionalBranchOperator::cmpeq:
         {
         TR::AbsValue* value2 = state->pop();
         TR::AbsValue* value1 = state->pop();
         TR_ASSERT_FATAL(value1->getDataType() == type, "Unexpected type");
         TR_ASSERT_FATAL(value2->getDataType() == type, "Unexpected type");
   
         switch (type)
            {
            case TR::Int32:
            case TR::Address:
               break;
            default:
               TR_ASSERT_FATAL(false, "Invalid type");
               break;
            }
         break;
         }

      /*** if_cmpne ***/
      case ConditionalBranchOperator::cmpne:
         {
         TR::AbsValue* value2 = state->pop();
         TR::AbsValue* value1 = state->pop();

         TR_ASSERT_FATAL(value2->getDataType() == type, "Unexpected type");
         TR_ASSERT_FATAL(value1->getDataType() == type, "Unexpected type");
   
         switch (type)
            {
            case TR::Int32:
            case TR::Address:
               break;
            default:
               TR_ASSERT_FATAL(false, "Invalid type");
               break;
            }
         break;
         }

      /*** if_cmpge ***/
      case ConditionalBranchOperator::cmpge:
         {
         TR::AbsValue* value2 = state->pop();
         TR::AbsValue* value1 = state->pop();

         TR_ASSERT_FATAL(value2->getDataType() == type, "Unexpected type");
         TR_ASSERT_FATAL(value1->getDataType() == type, "Unexpected type");
   
         switch (type)
            {
            case TR::Int32:
               break;
            default:
               TR_ASSERT_FATAL(false, "Invalid type");
               break;
            }
         break;
         }

      /*** if_cmpgt ***/
      case ConditionalBranchOperator::cmpgt:
         {
         TR::AbsValue* value2 = state->pop();
         TR::AbsValue* value1 = state->pop();
   
         switch (type)
            {
            case TR::Int32:
               break;
            default:
               TR_ASSERT_FATAL(false, "Invalid type");
               break;
            }
         break;
         }

      /*** if_cmple ***/
      case ConditionalBranchOperator::cmple:
         {
         TR::AbsValue* value2 = state->pop();
         TR::AbsValue* value1 = state->pop();

         TR_ASSERT_FATAL(value2->getDataType() == type, "Unexpected type");
         TR_ASSERT_FATAL(value1->getDataType() == type, "Unexpected type");
   
         switch (type)
            {
            case TR::Int32:
               break;
            default:
               TR_ASSERT_FATAL(false, "Invalid type");
               break;
            }
         break;
         }

      /*** if_cmplt ***/
      case ConditionalBranchOperator::cmplt:
         {
         TR::AbsValue* value2 = state->pop();
         TR::AbsValue* value1 = state->pop();

         TR_ASSERT_FATAL(value2->getDataType() == type, "Unexpected type");
         TR_ASSERT_FATAL(value1->getDataType() == type, "Unexpected type");
   
         switch (type)
            {
            case TR::Int32:
               break;
            default:
               TR_ASSERT_FATAL(false, "Invalid type");
               break;
            }
         break;
         }

      default:
         TR_ASSERT_FATAL(false, "Invalid conditional branch operator");
         break;
      }
   }

void J9::AbsInterpreter::new_()
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());

   int32_t cpIndex = next2Bytes();
   TR_OpaqueClassBlock* type = _callerMethod->getClassFromConstantPool(comp(), cpIndex);
   TR::AbsValue* value = createObject(type, TR_yes);
   state->push(value);
   }

void J9::AbsInterpreter::multianewarray(int32_t dimension)
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());

   uint16_t cpIndex = next2Bytes();

   TR_OpaqueClassBlock* arrayType = _callerMethod->getClassFromConstantPool(comp(), cpIndex);

   //get the outer-most length
   for (int i = 0; i < dimension-1; i++)
      {
      state->pop();
      }

   TR::AbsValue* length = state->pop(); 
   TR_ASSERT_FATAL(length->getDataType() == TR::Int32, "Unexpected type");

   if (isInt(length))
      {
      TR::AbsValue* array = createArrayObject(
                           arrayType,
                           TR_yes,
                           static_cast<TR::AbsVPValue*>(length)->getConstraint()->asIntConstraint()->getLowInt(),
                           static_cast<TR::AbsVPValue*>(length)->getConstraint()->asIntConstraint()->getHighInt(),
                           4
                           );
      state->push(array);
      return;
      }

   TR::AbsValue* array = createArrayObject(
                        arrayType,
                        TR_yes,
                        0,
                        INT32_MAX,
                        4
                        );
   state->push(array);
   }

void J9::AbsInterpreter::newarray()
   {
   TR::AbsStackMachineState *state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());

   /**
    * aType
    * 4: boolean
    * 5: char
    * 6: float
    * 7: double
    * 8: byte
    * 9: short
    * 10: int
    * 11: long
    */
   int32_t aType = nextByte();
   int32_t elementSize = aType == 7 || aType == 11 ? 8 : 4;
   
   TR_OpaqueClassBlock* arrayType = comp()->fe()->getClassFromNewArrayType(aType);

   TR::AbsValue *length = state->pop();
   TR_ASSERT_FATAL(length->getDataType() == TR::Int32, "Unexpected type");

   if (isInt(length))
      {
      TR::AbsValue* value = createArrayObject(
                                             arrayType, 
                                             TR_yes, 
                                             static_cast<TR::AbsVPValue*>(length)->getConstraint()->getLowInt(), 
                                             static_cast<TR::AbsVPValue*>(length)->getConstraint()->getHighInt(), 
                                             elementSize
                                             );
      state->push(value);
      return;
      }

   TR::AbsValue* value = createArrayObject(
                                          arrayType, 
                                          TR_yes, 
                                          0, 
                                          INT32_MAX, 
                                          elementSize
                                          );
   state->push(value);
   }

void J9::AbsInterpreter::anewarray()
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());

   int32_t cpIndex = next2Bytes();

   TR_OpaqueClassBlock* arrayType = _callerMethod->getClassFromConstantPool(comp(), cpIndex);

   TR::AbsValue *length = state->pop();
   TR_ASSERT_FATAL(length->getDataType() == TR::Int32, "Unexpected type");

   if (isInt(length))
      {
      TR::AbsValue* value = createArrayObject(
                                             arrayType, 
                                             TR_yes, 
                                             static_cast<TR::AbsVPValue*>(length)->getConstraint()->asIntConstraint()->getLowInt(), 
                                             static_cast<TR::AbsVPValue*>(length)->getConstraint()->asIntConstraint()->getHighInt(),
                                             4
                                             );
      state->push(value);
      return;
      }

   TR::AbsValue* value = createArrayObject(arrayType, TR_yes, 0, INT32_MAX, 4);
   state->push(value);
   }

void J9::AbsInterpreter::arraylength()
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());

   TR::AbsValue* arrayRef = state->pop();
   TR_ASSERT_FATAL(arrayRef->getDataType() == TR::Address, "Unexpected type");

   if (arrayRef->isParameter() && !arrayRef->isImplicitParameter())
      {
      TR::PotentialOptimizationVPPredicate* p1 = 
         new (region()) TR::PotentialOptimizationVPPredicate(TR::VPNullObject::create(vp()), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::NullCheckFolding, vp());
      TR::PotentialOptimizationVPPredicate* p2 = 
         new (region()) TR::PotentialOptimizationVPPredicate(TR::VPNonNullObject::create(vp()), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::NullCheckFolding, vp());

      _inliningMethodSummary->addPotentialOptimizationByArgument(p1, arrayRef->getParamPosition());
      _inliningMethodSummary->addPotentialOptimizationByArgument(p2, arrayRef->getParamPosition());
      }
     
   if (isArrayObject(arrayRef))
      {
      TR::VPArrayInfo* info = static_cast<TR::AbsVPValue*>(arrayRef)->getConstraint()->getArrayInfo();
      TR::AbsValue* result = NULL;

      if (info->lowBound() == info->highBound())
         {
         result = createIntConst(info->lowBound());
         }
      else
         {
         result = createIntRange(info->lowBound(), info->highBound());
         }
      state->push(result);
      return;
      }
   
   TR::AbsValue *result = createIntRange(0, INT32_MAX);
   state->push(result);
   }

void J9::AbsInterpreter::instanceof()
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());

   TR::AbsValue *objectRef = state->pop();
   TR_ASSERT_FATAL(objectRef->getDataType() == TR::Address, "Unexpected type");

   int32_t cpIndex = next2Bytes();
   TR_OpaqueClassBlock *castClass = _callerMethod->getClassFromConstantPool(comp(), cpIndex); //The cast class to be compared with

   //Add to the inlining summary
   if (objectRef->isParameter())
      {
      TR::PotentialOptimizationVPPredicate* p1 = 
         new (region()) TR::PotentialOptimizationVPPredicate(TR::VPNullObject::create(vp()), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::InstanceOfFolding, vp());

      _inliningMethodSummary->addPotentialOptimizationByArgument(p1, objectRef->getParamPosition());
      
      if (castClass)
         {
         TR::PotentialOptimizationVPPredicate* p2 = 
            new (region()) TR::PotentialOptimizationVPPredicate(TR::VPResolvedClass::create(vp(), castClass), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::InstanceOfFolding, vp());

         _inliningMethodSummary->addPotentialOptimizationByArgument(p2, objectRef->getParamPosition());
         }
      
      }

   if (isNullObject(objectRef)) //instanceof null
      {
      TR::AbsValue* result = createIntConst(0); //false
      state->push(result);
      return;
      }

   if (isObject(objectRef) && isNonNullObject(objectRef))
      {
      if (castClass && static_cast<TR::AbsVPValue*>(objectRef)->getConstraint()->getClass())
         {
         TR_YesNoMaybe yesNoMaybe = comp()->fe()->isInstanceOf(
                                                      static_cast<TR::AbsVPValue*>(objectRef)->getConstraint()->getClass(), 
                                                      castClass, 
                                                      static_cast<TR::AbsVPValue*>(objectRef)->getConstraint()->isFixedClass(), 
                                                      true);
         if(yesNoMaybe == TR_yes) //Instanceof must be true;
            {
            state->push(createIntConst(1));
            return;
            } 
         else if (yesNoMaybe == TR_no) //Instanceof must be false;
            {
            state->push(createIntConst(0));
            return;
            }
         }
      }

   state->push(createIntRange(0, 1));
   return;
   }

void J9::AbsInterpreter::checkcast() 
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());

   TR::AbsValue *objRef = state->pop();
   TR_ASSERT_FATAL(objRef->getDataType() == TR::Address, "Unexpected type");

   int32_t cpIndex = next2Bytes();
   TR_OpaqueClassBlock* castClass = _callerMethod->getClassFromConstantPool(comp(), cpIndex);

   //adding to method summary
   if (objRef->isParameter() && !objRef->isImplicitParameter() )
      {
      TR::PotentialOptimizationVPPredicate* p1 = 
         new (region()) TR::PotentialOptimizationVPPredicate(TR::VPNullObject::create(vp()), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::CheckCastFolding, vp());

      _inliningMethodSummary->addPotentialOptimizationByArgument(p1, objRef->getParamPosition());
      
      if (castClass)
         {
         TR::PotentialOptimizationVPPredicate* p2 = 
            new (region()) TR::PotentialOptimizationVPPredicate(TR::VPResolvedClass::create(vp(), castClass), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::CheckCastFolding, vp());

         _inliningMethodSummary->addPotentialOptimizationByArgument(p2, objRef->getParamPosition());
         }
      }

   if (isNullObject(objRef)) //Check cast null object, always succeed
      {
      state->push(objRef);
      return;
      }

   if (isObject(objRef) && isNonNullObject(objRef))
      {
      if (castClass && static_cast<TR::AbsVPValue*>(objRef)->getConstraint()->getClass())
         {
         TR_YesNoMaybe yesNoMaybe = comp()->fe()->isInstanceOf(
                                          static_cast<TR::AbsVPValue*>(objRef)->getConstraint()->getClass(), 
                                          castClass,
                                          static_cast<TR::AbsVPValue*>(objRef)->getConstraint()->isFixedClass(), 
                                          true);
         if (yesNoMaybe == TR_yes)
            {
            if (castClass == static_cast<TR::AbsVPValue*>(objRef)->getConstraint()->getClass()) //cast into the same type, no change
               {
               state->push(objRef);
               return;
               }
            else //cast into a different type
               {
               state->push(createObject(castClass, TR_yes));
               return;   
               }
            }
         }
      }

   state->push(createTopObject());
   }

void J9::AbsInterpreter::get(bool isStatic)
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());

   int32_t cpIndex = next2Bytes();
   TR::DataType type;

   if (isStatic) //getstatic
      {
      void* a; bool b; bool c; bool d; bool e; //we don't care about those vars
      _callerMethod->staticAttributes(comp(), cpIndex, &a, &type, &b, &c, &d, false, &e, false);
      }
   else  //getfield
      {
      TR::AbsValue* objRef = state->pop();
      TR_ASSERT_FATAL(objRef->getDataType() == TR::Address, "Unexpected type");

      if (objRef->isParameter() && !objRef->isImplicitParameter())  
         {
         TR::PotentialOptimizationVPPredicate* p1 = 
            new (region()) TR::PotentialOptimizationVPPredicate(TR::VPNullObject::create(vp()), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::NullCheckFolding, vp());
         TR::PotentialOptimizationVPPredicate* p2 = 
            new (region()) TR::PotentialOptimizationVPPredicate(TR::VPNonNullObject::create(vp()), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::NullCheckFolding, vp());

         _inliningMethodSummary->addPotentialOptimizationByArgument(p1, objRef->getParamPosition());
         _inliningMethodSummary->addPotentialOptimizationByArgument(p2, objRef->getParamPosition());
         }

      uint32_t a; bool b; bool c; bool d; bool e;
      _callerMethod->fieldAttributes(comp(), cpIndex, &a, &type, &b, &c, &d, false, &e, false);
      }

   switch (type)
      {
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
         state->push(createTopInt());
         break;
      
      case TR::Int64:
         state->push(createTopLong());
         state->push(createTopLong());
         break;

      case TR::Float:
         state->push(createTopFloat());
         break;

      case TR::Double:
         state->push(createTopDouble());
         state->push(createTopDouble());
         break;

      case TR::Address:
         state->push(createTopObject());
         break;

      default:
         TR_ASSERT_FATAL(false, "Invalid type");
         break;
      }
   }

void J9::AbsInterpreter::put(bool isStatic)
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());

   int32_t cpIndex = next2Bytes();
   TR::DataType type;

   if (isStatic) //putstatic
      {
      void* a; bool b; bool c; bool d; bool e; //we don't care about those vars
      _callerMethod->staticAttributes(comp(), cpIndex, &a, &type, &b, &c, &d, false, &e, false);
      }
   else  //putfield
      {
      uint32_t a; bool b; bool c; bool d; bool e;
      _callerMethod->fieldAttributes(comp(), cpIndex, &a, &type, &b, &c, &d, false, &e, false);
      }

   if (type.isInt64() || type.isDouble())
      state->pop();
      
   TR::AbsValue* value = state->pop();

   if (!isStatic) //putfield
      {
      TR::AbsValue* objRef = state->pop();
      TR_ASSERT_FATAL(objRef->getDataType() == TR::Address, "Unexpected type");

      if (objRef->isParameter() && !objRef->isImplicitParameter())  
         {
         TR::PotentialOptimizationVPPredicate* p1 = 
            new (region()) TR::PotentialOptimizationVPPredicate(TR::VPNullObject::create(vp()), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::NullCheckFolding, vp());
         TR::PotentialOptimizationVPPredicate* p2 = 
            new (region()) TR::PotentialOptimizationVPPredicate(TR::VPNonNullObject::create(vp()), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::NullCheckFolding, vp());

         _inliningMethodSummary->addPotentialOptimizationByArgument(p1, objRef->getParamPosition());
         _inliningMethodSummary->addPotentialOptimizationByArgument(p2, objRef->getParamPosition());
         }
      }
   
   }

void J9::AbsInterpreter::monitor(bool kind)
   {
   TR::AbsValue* value = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState())->pop();
   TR_ASSERT_FATAL(value->getDataType() == TR::Address, "Unexpected type");
   }

void J9::AbsInterpreter::switch_(bool kind)
   {
   TR::AbsValue* value = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState())->pop();
   TR_ASSERT_FATAL(value->getDataType() == TR::Int32, "Unexpected type");
   }

void J9::AbsInterpreter::iinc(int32_t index, int32_t incVal)
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());

   TR::AbsValue* value = state->at(index);
   TR_ASSERT_FATAL(value->getDataType() == TR::Int32, "Unexpected type");

   if (isIntConst(value))
      {
      TR::AbsValue* result = createIntConst(static_cast<TR::AbsVPValue*>(value)->getConstraint()->asIntConst()->getInt() + incVal);
      state->set(index, result);
      return;
      }
   
   state->set(index, createTopInt());
   }

void J9::AbsInterpreter::athrow()
   {
   }

void J9::AbsInterpreter::invoke(TR::MethodSymbol::Kinds kind) 
   {
   TR::AbsStackMachineState* state = static_cast<TR::AbsStackMachineState*>(currentBlock()->getAbsState());

   int32_t cpIndex = next2Bytes();

   //split
   if (current() == J9BCinvokespecialsplit) cpIndex |= J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG;
   else if (current() == J9BCinvokestaticsplit) cpIndex |= J9_STATIC_SPLIT_TABLE_INDEX_FLAG;

   int32_t bcIndex = currentByteCodeIndex();

   TR::Block* callBlock = currentBlock();

   TR::Method *calleeMethod = comp()->fej9()->createMethod(comp()->trMemory(), _callerMethod->containingClass(), cpIndex);

   TR_CallSite* callsite = getCallSite(kind, bcIndex, cpIndex); // callsite can be NULL

   uint32_t numExplicitParams = calleeMethod->numberOfExplicitParameters();
   uint32_t totalNumParams = numExplicitParams + (kind  == TR::MethodSymbol::Static ? 0 : 1);

   TR::AbsArguments* args = new (region()) TR::AbsArguments(totalNumParams, region());

   for (uint32_t i = 0 ; i < numExplicitParams; i ++) //explicit param
      {
      TR::AbsValue* value = NULL;

      TR::DataType dataType = calleeMethod->parmType(numExplicitParams -i - 1);
      if (dataType == TR::Double || dataType == TR::Int64)
         state->pop();
  
      value = state->pop();
      TR_ASSERT_FATAL(dataType == TR::Int8 || dataType == TR::Int16 ? value->getDataType() == TR::Int32 : value->getDataType() == dataType, "Unexpected type");

      args->set(totalNumParams - i - 1, value);
      }
   
   if (kind != TR::MethodSymbol::Kinds::Static) //implicit param
      {
      TR::AbsValue* objRef = state->pop();
      if (kind == TR::MethodSymbol::Interface || kind == TR::MethodSymbol::Virtual)
         {
         if (objRef->isParameter() && !objRef->isImplicitParameter())  
            {
            TR::PotentialOptimizationVPPredicate* p1 = 
               new (region()) TR::PotentialOptimizationVPPredicate(TR::VPNullObject::create(vp()), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::NullCheckFolding, vp());
            TR::PotentialOptimizationVPPredicate* p2 = 
               new (region()) TR::PotentialOptimizationVPPredicate(TR::VPNonNullObject::create(vp()), currentByteCodeIndex(), TR::PotentialOptimizationPredicate::Kind::NullCheckFolding, vp());

            _inliningMethodSummary->addPotentialOptimizationByArgument(p1, objRef->getParamPosition());
            _inliningMethodSummary->addPotentialOptimizationByArgument(p2, objRef->getParamPosition());
            }
         }
      args->set(0, objRef);
      }

   _visitor->visitCallSite(callsite, _callerIndex, callBlock, args); //callback 

   if (calleeMethod->isConstructor() || calleeMethod->returnType() == TR::NoType )
      return;

   TR::DataType datatype = calleeMethod->returnType();

   switch(datatype) 
         {
         case TR::Int32:
         case TR::Int16:
         case TR::Int8:
            state->push(createTopInt());
            break;
         case TR::Float:
            state->push(createTopFloat());
            break;
         case TR::Address:
            state->push(createTopObject());
            break;
         case TR::Double:
            state->push(createTopDouble());
            state->push(createTopDouble());
            break;
         case TR::Int64:
            state->push(createTopLong());
            state->push(createTopLong());
            break;
            
         default:
            TR_ASSERT_FATAL(false, "Invalid type");
            break;
         }  
   }
   
TR_CallSite* J9::AbsInterpreter::getCallSite(TR::MethodSymbol::Kinds kind, int32_t bcIndex, int32_t cpIndex)
   {
   TR_CallSite* callSite = NULL;

   // settting the bytecode info  

   TR_ByteCodeInfo info;
   info.setByteCodeIndex(bcIndex);
   info.setDoNotProfile(false);
   info.setCallerIndex(_callerIndex);

   TR::SymbolReference *symbolReference = getSymbolReference(cpIndex, kind);

   bool isInterface = kind == TR::MethodSymbol::Kinds::Interface;

   if (symbolReference->isUnresolved() && !isInterface) 
      {
      if (comp()->getOption(TR_TraceAbstractInterpretation))
         {
         traceMsg(comp(), "Do not find a callsite: method is unresolved and is not interface\n");
         }
      return NULL;
      }

   TR::Symbol *symbol = symbolReference->getSymbol();

   TR::ResolvedMethodSymbol *calleeSymbol = !isInterface ? symbol->castToResolvedMethodSymbol() : NULL;

   TR_ResolvedMethod *resolvedCalleeMethod = !isInterface ? calleeSymbol->getResolvedMethod() : NULL;

   TR::Method *interfaceMethod = !isInterface ? calleeSymbol->getMethod() : comp()->fej9()->createMethod(comp()->trMemory(), _callerMethod->containingClass(), cpIndex);
  
   TR_OpaqueClassBlock *callerClass = _callerMethod->classOfMethod();
   TR_OpaqueClassBlock *calleeClass = resolvedCalleeMethod ? resolvedCalleeMethod->classOfMethod() : NULL;

   info.setIsSameReceiver(callerClass == calleeClass);
   
   bool isIndirect = kind == TR::MethodSymbol::Interface || kind == TR::MethodSymbol::Virtual;
   int32_t vftSlotIndex = kind == TR::MethodSymbol::Virtual ? symbolReference->getOffset() : -1;

   TR::TreeTop *callNodeTreeTop = NULL;
   TR::Node *parent = NULL;
   TR::Node *callNode = NULL;
   int32_t depth = -1;
   bool allConsts = false;

   switch (kind) 
      {
      case TR::MethodSymbol::Kinds::Virtual:
         callSite = new (region()) TR_J9VirtualCallSite(_callerMethod,
                                                         callNodeTreeTop, 
                                                         parent, 
                                                         callNode, 
                                                         interfaceMethod, 
                                                         calleeClass, 
                                                         vftSlotIndex, 
                                                         cpIndex, 
                                                         resolvedCalleeMethod, 
                                                         calleeSymbol, 
                                                         isIndirect, 
                                                         isInterface,
                                                         info, 
                                                         comp(), 
                                                         depth, 
                                                         allConsts);
         break;
      case TR::MethodSymbol::Kinds::Static:
      case TR::MethodSymbol::Kinds::Special:
         callSite = new (region()) TR_DirectCallSite(_callerMethod,
                                                      callNodeTreeTop, 
                                                      parent, 
                                                      callNode, 
                                                      interfaceMethod, 
                                                      calleeClass, 
                                                      vftSlotIndex, 
                                                      cpIndex, 
                                                      resolvedCalleeMethod, 
                                                      calleeSymbol, 
                                                      isIndirect, 
                                                      isInterface, 
                                                      info, 
                                                      comp(), 
                                                      depth, 
                                                      allConsts);
         break;
      case TR::MethodSymbol::Kinds::Interface:
         callSite = new (region()) TR_J9InterfaceCallSite(_callerMethod, 
                                                            callNodeTreeTop, 
                                                            parent, 
                                                            callNode, 
                                                            interfaceMethod, 
                                                            calleeClass, 
                                                            vftSlotIndex, 
                                                            cpIndex, 
                                                            resolvedCalleeMethod, 
                                                            calleeSymbol, 
                                                            isIndirect, 
                                                            isInterface, 
                                                            info, 
                                                            comp(), 
                                                            depth, 
                                                            allConsts);
         break;
      }
   
   if (!callSite)
      return NULL;

   callSite->_byteCodeIndex = bcIndex;
   callSite->_bcInfo = info;
   callSite->_cpIndex= cpIndex;

   return callSite;   
   }


TR::SymbolReference* J9::AbsInterpreter::getSymbolReference(int32_t cpIndex, TR::MethodSymbol::Kinds kind)
   {
   TR::SymbolReference *symbolReference = NULL;
   switch(kind)
      {
      case TR::MethodSymbol::Kinds::Virtual:
         symbolReference = comp()->getSymRefTab()->findOrCreateVirtualMethodSymbol(_callerMethodSymbol, cpIndex);
         break;
      case TR::MethodSymbol::Kinds::Static:
         symbolReference = comp()->getSymRefTab()->findOrCreateStaticMethodSymbol(_callerMethodSymbol, cpIndex);
         break;
      case TR::MethodSymbol::Kinds::Interface:
         symbolReference = comp()->getSymRefTab()->findOrCreateInterfaceMethodSymbol(_callerMethodSymbol, cpIndex);
         break;
      case TR::MethodSymbol::Kinds::Special:
         symbolReference = comp()->getSymRefTab()->findOrCreateSpecialMethodSymbol(_callerMethodSymbol, cpIndex);
         break;
      default:
         TR_ASSERT_FATAL(false, "Unexpected method symbol kind");
         break;

      }
   return symbolReference;
   }

TR::AbsValue* J9::AbsInterpreter::createObject(TR_OpaqueClassBlock* opaqueClass, TR_YesNoMaybe isNonNull)
   {
   TR::VPClassPresence *classPresence = isNonNull == TR_yes ? TR::VPNonNullObject::create(vp()) : NULL;
   TR::VPClassType *classType = opaqueClass? TR::VPResolvedClass::create(vp(), opaqueClass) : NULL;

   return new (region()) TR::AbsVPValue(vp(), TR::VPClass::create(vp(), classType, classPresence, NULL, NULL, NULL), TR::Address);
   }

TR::AbsValue* J9::AbsInterpreter::createNullObject()
   {
   return new (region()) TR::AbsVPValue(vp(), TR::VPNullObject::create(vp()), TR::Address);
   }

TR::AbsValue* J9::AbsInterpreter::createArrayObject(TR_OpaqueClassBlock* arrayClass, TR_YesNoMaybe isNonNull, int32_t lengthLow, int32_t lengthHigh, int32_t elementSize)
   {
   TR::VPClassPresence *classPresence = isNonNull == TR_yes ? TR::VPNonNullObject::create(vp()) : NULL;;
   TR::VPArrayInfo *arrayInfo = TR::VPArrayInfo::create(vp(), lengthLow, lengthHigh, elementSize);
   TR::VPClassType *arrayType = arrayClass ? TR::VPResolvedClass::create(vp(), arrayClass) : NULL;

   return new (region()) TR::AbsVPValue(vp(), TR::VPClass::create(vp(), arrayType, classPresence, NULL, arrayInfo, NULL), TR::Address);    
   }
   
TR::AbsValue* J9::AbsInterpreter::createStringObject(TR::SymbolReference* symRef, TR_YesNoMaybe isNonNull)
   {
   TR::VPClassPresence *classPresence = isNonNull == TR_yes ? TR::VPNonNullObject::create(vp()) : NULL;
   TR::VPClassType *stringType = symRef ? TR::VPConstString::create(vp(), symRef) : NULL;

   return new (region()) TR::AbsVPValue(vp(), TR::VPClass::create(vp(), stringType, classPresence, NULL, NULL, NULL), TR::Address);
   }

TR::AbsValue* J9::AbsInterpreter::createIntConst(int32_t value)
   {
   return new (region()) TR::AbsVPValue(vp(), TR::VPIntConst::create(vp(), value), TR::Int32);
   }

TR::AbsValue* J9::AbsInterpreter::createLongConst(int64_t value)
   {
   return new (region()) TR::AbsVPValue(vp(), TR::VPLongConst::create(vp(), value), TR::Int64);
   }

TR::AbsValue* J9::AbsInterpreter::createIntRange(int32_t low, int32_t high)
   {
   return new (region()) TR::AbsVPValue(vp(), TR::VPIntRange::create(vp(), low, high), TR::Int32);
   }

TR::AbsValue* J9::AbsInterpreter::createLongRange(int64_t low, int64_t high)
   {
   return new (region()) TR::AbsVPValue(vp(), TR::VPLongRange::create(vp(), low, high), TR::Int64);
   }

TR::AbsValue* J9::AbsInterpreter::createTopInt()
   {
   return new (region()) TR::AbsVPValue(vp(), TR::VPIntRange::create(vp(), INT32_MIN, INT32_MAX), TR::Int32);
   }

TR::AbsValue* J9::AbsInterpreter::createTopLong()
   {
   return new (region()) TR::AbsVPValue(vp(), TR::VPLongRange::create(vp(), INT64_MIN, INT64_MAX), TR::Int64);
   }

TR::AbsValue* J9::AbsInterpreter::createTopDouble()
   {
   return new (region()) TR::AbsVPValue(vp(), NULL, TR::Double);
   }

TR::AbsValue* J9::AbsInterpreter::createTopFloat()
   {
   return new (region()) TR::AbsVPValue(vp(), NULL, TR::Float);
   }

TR::AbsValue* J9::AbsInterpreter::createTopObject()
   {
   return createObject(comp()->getObjectClassPointer(), TR_maybe);
   }

bool J9::AbsInterpreter::isNullObject(TR::AbsValue* v)
   {
   TR::AbsVPValue* value = static_cast<TR::AbsVPValue*>(v);
   return value->getConstraint() && value->getConstraint()->isNullObject();
   }

bool J9::AbsInterpreter::isNonNullObject(TR::AbsValue* v)
   {
   TR::AbsVPValue* value = static_cast<TR::AbsVPValue*>(v);
   return value->getConstraint() && value->getConstraint()->isNonNullObject();
   }

bool J9::AbsInterpreter::isArrayObject(TR::AbsValue* v)  
   {
   TR::AbsVPValue* value = static_cast<TR::AbsVPValue*>(v);
   return value->getConstraint() && value->getConstraint()->asClass() && value->getConstraint()->getArrayInfo();
   }

bool J9::AbsInterpreter::isObject(TR::AbsValue* v)  
   {
   TR::AbsVPValue* value = static_cast<TR::AbsVPValue*>(v);
   return value->getConstraint() && value->getConstraint()->asClass();
   }

bool J9::AbsInterpreter::isIntConst(TR::AbsValue* v)  
   {
   TR::AbsVPValue* value = static_cast<TR::AbsVPValue*>(v);
   return value->getConstraint() && value->getConstraint()->asIntConst();
   }

bool J9::AbsInterpreter::isIntRange(TR::AbsValue* v)  
   {
   TR::AbsVPValue* value = static_cast<TR::AbsVPValue*>(v);
   return value->getConstraint() && value->getConstraint()->asIntRange();
   }

bool J9::AbsInterpreter::isInt(TR::AbsValue* v)  
   {
   TR::AbsVPValue* value = static_cast<TR::AbsVPValue*>(v);
   return value->getConstraint() && value->getConstraint()->asIntConstraint();
   }

bool J9::AbsInterpreter::isLongConst(TR::AbsValue* v)  
   {
   TR::AbsVPValue* value = static_cast<TR::AbsVPValue*>(v);
   return value->getConstraint() && value->getConstraint()->asLongConst();
   }

bool J9::AbsInterpreter::isLongRange(TR::AbsValue* v)  
   {
   TR::AbsVPValue* value = static_cast<TR::AbsVPValue*>(v);
   return value->getConstraint() && value->getConstraint()->asLongRange();
   }

bool J9::AbsInterpreter::isLong(TR::AbsValue* v)  
   {
   TR::AbsVPValue* value = static_cast<TR::AbsVPValue*>(v);
   return value->getConstraint() && value->getConstraint()->asLongConstraint();
   }
