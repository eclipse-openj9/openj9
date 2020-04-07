/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

/*
 * \class InterpreterEmulator
 *
 * \brief This class is a bytecode iterator in estimate code size (ECS) of inliner.
 *
 * \notes The iterator has statelss and with state modes.
 *
 *        Stateless mode is the default mode and can be used to iterate
 *        through bytecodes in the method. This mode is currently used in the
 *        in ECS for \ref processBytecodeAndGenerateCFG for all methods and
 *        \ref findAndCreateCallsitesFromBytecodes when the target is not a methodhandle
 *        thunk archetype.
 *
 *        With state mode is used to emulate the interpreter execution with
 *        an operand stack maintained during bytecode iteration to keep useful
 *        information like known object and constant integer so
 *        that inliner can make better decision when creating callsites. For
 *        example, inliner can avoid creating callsites on dead path
 *        \ref maintainStackForIf or can refine callee method based on known
 *        receiver info \ref refineResolvedCalleeForInvokestatic. Operands that
 *        can't be reasoned about are represented by a dummy operand \ref _unknownOperand
 *        which doesn't carry any extra information. Currently, with state mode only
 *        supports methodhandle thunk archetypes.
 */

#ifndef INTERPRETER_EMULATOR_INCL
#define INTERPRETER_EMULATOR_INCL

#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "ilgen/ByteCodeIteratorWithState.hpp"
#include "ilgen/J9ByteCodeIterator.hpp"
#include "optimizer/Inliner.hpp"
#include "optimizer/J9EstimateCodeSize.hpp"
#include "optimizer/J9Inliner.hpp"

class IconstOperand;
class KnownObjOperand;
class MutableCallsiteTargetOperand;

/*
 * \class Operand
 *
 * \brief represent an operand on the operand stack
 *
 * \note this is the most general operand which doesn't carry any specific information.
 */
class Operand
   {
   public:
      TR_ALLOC(TR_Memory::EstimateCodeSize);
      virtual IconstOperand *asIconst(){ return NULL;}
      virtual KnownObjOperand *asKnownObject(){ return NULL;}
      virtual MutableCallsiteTargetOperand* asMutableCallsiteTargetOperand(){ return NULL;}
      virtual bool isUnkownOperand(){ return true;}
      virtual TR::KnownObjectTable::Index getKnownObjectIndex(){ return TR::KnownObjectTable::UNKNOWN;}
      virtual void printToString( char *buffer)
         {
         sprintf(buffer, "(obj%d)", getKnownObjectIndex());
         }
   };

/*
 * \class KnownOperand
 *
 * \brief represent operands that can be reasoned about at compile time
 */
class KnownOperand : public Operand
   {
   public:
      TR_ALLOC(TR_Memory::EstimateCodeSize);
      virtual bool isUnkownOperand(){ return false;}
   };

class IconstOperand : public KnownOperand
   {
   public:
      TR_ALLOC(TR_Memory::EstimateCodeSize);
      IconstOperand (int x): intValue(x) { }
      virtual IconstOperand *asIconst() { return this;}
      virtual void printToString( char *buffer)
         {
         sprintf(buffer, "(iconst=%d)", intValue);
         }
      int32_t intValue;
   };

class KnownObjOperand : public KnownOperand
   {
   public:
      TR_ALLOC(TR_Memory::EstimateCodeSize);
      KnownObjOperand(TR::KnownObjectTable::Index koi):knownObjIndex(koi){ }
      virtual KnownObjOperand *asKnownObject(){ return this;}
      virtual TR::KnownObjectTable::Index getKnownObjectIndex(){ return knownObjIndex;}
      TR::KnownObjectTable::Index knownObjIndex;
   };

/*
 * \class MutableCallsiteTargetOperand
 *
 * \note This class is used to support mutable callsite because both the methodhandle object
 *       and the mutable callsite needs to be tracked so that when creating \c TR_J9MutableCallSite
 *       the mutable callsite object can be set for the callsite even though it's really the
 *       methodhandle object that's on the operand stack.
 *
 * \see getReturnValueForInvokevirtual
 * \see refineResolvedCalleeForInvokestatic
 * \see visitInvokestatic
 */
class MutableCallsiteTargetOperand : public KnownObjOperand
   {
   public:
      TR_ALLOC(TR_Memory::EstimateCodeSize);
      MutableCallsiteTargetOperand (TR::KnownObjectTable::Index methodHandleIndex, TR::KnownObjectTable::Index mutableCallsiteIndex):
         KnownObjOperand(methodHandleIndex),
         mutableCallsiteIndex(mutableCallsiteIndex){}
      virtual MutableCallsiteTargetOperand* asMutableCallsiteTargetOperand(){ return this; }
      virtual void printToString(char *buffer)
         {
         sprintf(buffer, "(mh=%d, mcs=%d)", getMethodHandleIndex(), getMutableCallsiteIndex());
         }
      TR::KnownObjectTable::Index getMethodHandleIndex(){ return knownObjIndex; }
      TR::KnownObjectTable::Index getMutableCallsiteIndex() { return mutableCallsiteIndex; }
      TR::KnownObjectTable::Index mutableCallsiteIndex;
   };

class InterpreterEmulator : public TR_ByteCodeIteratorWithState<TR_J9ByteCode, J9BCunknown, TR_J9ByteCodeIterator, Operand *>
   {
   typedef TR_ByteCodeIteratorWithState<TR_J9ByteCode, J9BCunknown, TR_J9ByteCodeIterator, Operand *> Base;

   public:
      InterpreterEmulator(
            TR_CallTarget *calltarget,
            TR::ResolvedMethodSymbol * methodSymbol,
            TR_J9VMBase * fe,
            TR::Compilation * comp,
            TR_LogTracer *tracer,
            TR_EstimateCodeSize *ecs)
         : Base(methodSymbol, comp),
           _calltarget(calltarget),
           _tracer(tracer),
           _ecs(ecs),
           _iteratorWithState(false)
         {
         TR_J9ByteCodeIterator::initialize(static_cast<TR_ResolvedJ9Method *>(methodSymbol->getResolvedMethod()), fe);
         _flags = NULL;
         _stacks = NULL;
         }
      TR_LogTracer *tracer() { return _tracer; }
      /* \brief Initialize data needed for looking for callsites
       *
       * \param blocks
       *       blocks generated from bytecodes
       *
       * \param flags
       *       flags with bits to indicate property of each bytecode. The flags are set by \ref TR_J9EstimateCodeSize::processBytecodeAndGenerateCFG.
       *
       * \param callSites
       *       the call sites array to be filled in with callsites found
       *
       * \param cfg
       *       CFG generated \ref TR_J9EstimateCodeSize::processBytecodeAndGenerateCFG from bytecodes
       *
       * \param recursionDepth
       *       the depth of inlining layers
       *
       * \parm callstack
       *       the call stack from the current inlined call target to the method being compiled
       */
      void prepareToFindAndCreateCallsites(TR::Block **blocks, flags8_t * flags, TR_CallSite ** callSites, TR::CFG* cfg, TR_ByteCodeInfo *newBCInfo, int32_t recursionDepth, TR_CallStack *callstack);

      /*
       * \brief look for calls in bytecodes and create callsites
       *
       * \param wasPeekingSuccessfull
       *       indicate whether trees has been generated by peeking ilgen
       *
       * \param withState
       *       whether the bytecode iteration should be with or without state
       *
       * \return whether callsites are created successfully. Return false if failed for reasons like unexpected bytecodes etc.
       */
      bool findAndCreateCallsitesFromBytecodes(bool wasPeekingSuccessfull, bool withState);
      void setBlocks(TR::Block **blocks) { _blocks = blocks; }
      TR_StackMemory trStackMemory()            { return _trMemory; }
      bool _nonColdCallExists;
      bool _inlineableCallExists;

      enum BytecodePropertyFlag
         {
         bbStart        = 0x01, // whether the current bytecode is at bbstart
         isCold         = 0x02, // whther the bytecode is on a cold path
         isBranch         = 0x04, // whther the bytecode is a branch
         isUnsanitizeable = 0x08,
         };

   private:
      // the following methods can only be called when the iterator has state

      /*
       * Initialize the data structures needed for iterator with state
       */
      void initializeIteratorWithState();
      /*
       * push and pop operands on stack according to given bytecode
       *
       * \return false if some error occurred such as unexpected bytecodes.
       */
      bool maintainStack(TR_J9ByteCode bc);
      void maintainStackForIf(TR_J9ByteCode bc);
      void maintainStackForGetField();
      void maintainStackForAload(int slotIndex);
      /*
       * \brief helper to pop arguments from the stack and push the result for calls
       *
       * \param method
       *    the method being called from the current bytecode
       *
       * \param result
       *    the operand reprenting the call return value
       *
       * \param isDirect
       *    whether it's a direct call or indirect call
       */
      void maintainStackForCall(TR_ResolvedMethod *method, Operand *result, bool isDirect);
      void maintainStackForDirectCall(TR_ResolvedMethod *method, Operand *result = NULL) { maintainStackForCall(method, result, true /* isDirect */); }
      void maintainStackForIndirectCall(TR_ResolvedMethod *method, Operand *result = NULL) { maintainStackForCall(method, result, false/* isDirect */); }
      /*
       * \brief refine the callee method based on operands when possible
       */
      void refineResolvedCalleeForInvokestatic(TR_ResolvedMethod *&callee, TR::KnownObjectTable::Index & mcsIndex, TR::KnownObjectTable::Index & mhIndex, bool & isIndirectCall);
      Operand *getReturnValueForInvokevirtual(TR_ResolvedMethod *callee);
      Operand *getReturnValueForInvokestatic(TR_ResolvedMethod *callee);
      void dumpStack();
      void pushUnknownOperand() { Base::push(_unknownOperand); }
      // doesn't need to handle execeptions yet as they don't exist in method handle thunk archetypes
      virtual void findAndMarkExceptionRanges(){ }
      virtual void saveStack(int32_t targetIndex);

      // the following methods can be used in both stateless and with state mode

      /*
       * \brief look for and set the next bytecode index to visit
       *
       * \return the bytecode value to visit
       */
      TR_J9ByteCode findNextByteCodeToVisit();

      /*
       * \brief tell whether the given bcIndex has been generated.
       *
       * \note This query is used to avoid regenerating bytecodes which shouldn't happen at stateless mode
       */
      bool isGenerated(int32_t bcIndex) { return _iteratorWithState ? Base::isGenerated(bcIndex): false; }
      void visitInvokedynamic();
      void visitInvokevirtual();
      void visitInvokespecial();
      void visitInvokestatic();
      void visitInvokeinterface();
      void findTargetAndUpdateInfoForCallsite(TR_CallSite *callsite);
      bool isCurrentCallUnresolvedOrCold(TR_ResolvedMethod *resolvedMethod, bool isUnresolvedInCP);
      void debugUnresolvedOrCold(TR_ResolvedMethod *resolvedMethod);

      TR_LogTracer *_tracer;
      TR_EstimateCodeSize *_ecs;
      Operand * _unknownOperand; // used whenever the iterator can't reason about an operand
      TR_CallTarget *_calltarget; // the target method to inline
      bool _iteratorWithState;
      flags8_t * _InterpreterEmulatorFlags; // flags with bits to indicate property of each bytecode.
      TR_CallSite ** _callSites;
      TR::CFG* _cfg;
      TR_ByteCodeInfo *_newBCInfo;
      int32_t _recursionDepth;
      TR_CallStack *_callStack; // the call stack from the current inlined call target to the method being compiled
      bool _wasPeekingSuccessfull;
      TR::Block *_currentInlinedBlock;
      TR_prevArgs _pca;
   };
#endif
