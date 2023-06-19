/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/
#include "optimizer/InterpreterEmulator.hpp"
#include "optimizer/J9EstimateCodeSize.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "env/JSR292Methods.h"
#include "optimizer/PreExistence.hpp"
#include "optimizer/J9CallGraph.hpp"
#include "ilgen/IlGenRequest.hpp"
#include "jilconsts.h"
#include "il/ParameterSymbol.hpp"
#include "optimizer/PreExistence.hpp"
#include "optimizer/TransformUtil.hpp"
#include "il/Node_inlines.hpp"
#if defined(J9VM_OPT_JITSERVER)
#include "control/CompilationRuntime.hpp"
#include "env/j9methodServer.hpp"
#endif /* defined(J9VM_OPT_JITSERVER) */

const char* Operand::KnowledgeStrings[] = {"NONE", "OBJECT", "MUTABLE_CALLSITE_TARGET", "PREEXISTENT", "FIXED_CLASS", "KNOWN_OBJECT", "ICONST" };

char*
ObjectOperand::getSignature(TR::Compilation *comp, TR_Memory *trMemory)
   {
   if (!_signature && _clazz)
      _signature = TR::Compiler->cls.classSignature(comp, _clazz, trMemory);
   return _signature;
   }

KnownObjOperand::KnownObjOperand(TR::KnownObjectTable::Index koi, TR_OpaqueClassBlock* clazz)
   : knownObjIndex(koi), FixedClassOperand(clazz)
   {
   TR_ASSERT_FATAL(knownObjIndex != TR::KnownObjectTable::UNKNOWN, "Unexpected unknown object");
   }

TR_OpaqueClassBlock*
KnownObjOperand::getClass()
   {
   if (_clazz)
      return _clazz;

   TR::Compilation* comp = TR::comp();
   auto knot = comp->getOrCreateKnownObjectTable();
   if (!knot || knot->isNull(knownObjIndex))
      return NULL;

   _clazz = comp->fej9()->getObjectClassFromKnownObjectIndex(comp, knownObjIndex);

   return _clazz;
   }

ObjectOperand*
KnownObjOperand::asObjectOperand()
   {
   if (getClass())
      return this;

   return NULL;
   }

// FixedClassOperand need the class, if we can't get the class, return NULL
FixedClassOperand*
KnownObjOperand::asFixedClassOperand()
   {
   if (getClass())
      return this;

   return NULL;
   }

Operand*
Operand::merge(Operand* other)
   {
   if (getKnowledgeLevel() > other->getKnowledgeLevel())
      return other->merge1(this);
   else
      return merge1(other);
   }

Operand*
Operand::merge1(Operand* other)
   {
   if (this == other)
      return this;
   else
      return NULL;
   }

Operand*
IconstOperand::merge1(Operand* other)
   {
   TR_ASSERT(other->getKnowledgeLevel() >= this->getKnowledgeLevel(), "Should be calling other->merge1(this)");
   IconstOperand* otherIconst = other->asIconst();
   if (otherIconst && this->intValue == otherIconst->intValue)
      return this;
   else
      return NULL;
   }

// TODO: check instanceOf relationship and create new Operand if neccessary
Operand*
ObjectOperand::merge1(Operand* other)
   {
   TR_ASSERT(other->getKnowledgeLevel() >= this->getKnowledgeLevel(), "Should be calling other->merge1(this)");
   ObjectOperand* otherObject = other->asObjectOperand();
   if (otherObject && this->_clazz == otherObject->_clazz)
      return this;
   else
      return NULL;
   }

// Both are preexistent objects
Operand*
PreexistentObjectOperand::merge1(Operand* other)
   {
   TR_ASSERT(other->getKnowledgeLevel() >= this->getKnowledgeLevel(), "Should be calling other->merge1(this)");
   PreexistentObjectOperand* otherPreexistentObjectOperand = other->asPreexistentObjectOperand();
   if (otherPreexistentObjectOperand && this->_clazz == otherPreexistentObjectOperand->_clazz)
      return this;
   else
      return NULL;
   }

Operand*
FixedClassOperand::merge1(Operand* other)
   {
   TR_ASSERT(other->getKnowledgeLevel() >= this->getKnowledgeLevel(), "Should be calling other->merge1(this)");
   FixedClassOperand* otherFixedClass = other->asFixedClassOperand();
   if (otherFixedClass && this->_clazz == otherFixedClass->_clazz)
      return this;
   else
      return NULL;
   }

Operand*
KnownObjOperand::merge1(Operand* other)
   {
   TR_ASSERT(other->getKnowledgeLevel() >= this->getKnowledgeLevel(), "Should be calling other->merge1(this)");
   KnownObjOperand* otherKnownObj = other->asKnownObject();
   if (otherKnownObj && this->knownObjIndex == otherKnownObj->knownObjIndex)
      return this;
   else
      return NULL;
   }

Operand*
MutableCallsiteTargetOperand::merge1(Operand* other)
   {
   TR_ASSERT(other->getKnowledgeLevel() >= this->getKnowledgeLevel(), "Should be calling other->merge1(this)");
   MutableCallsiteTargetOperand* otherMutableCallsiteTarget = other->asMutableCallsiteTargetOperand();
   if (otherMutableCallsiteTarget &&
       this->mutableCallsiteIndex== otherMutableCallsiteTarget->mutableCallsiteIndex &&
       this->methodHandleIndex == otherMutableCallsiteTarget->methodHandleIndex)
      return this;
   else
      return NULL;
   }

void
Operand::printToString(TR::StringBuf *buf)
   {
   buf->appendf("(unknown)");
   }

void
IconstOperand::printToString(TR::StringBuf *buf)
   {
   buf->appendf("(iconst=%d)", intValue);
   }

void
ObjectOperand::printToString(TR::StringBuf *buf)
   {
   buf->appendf("(%s=clazz%p)", KnowledgeStrings[getKnowledgeLevel()], getClass());
   }

void
KnownObjOperand::printToString(TR::StringBuf *buf)
   {
   buf->appendf("(obj%d)", getKnownObjectIndex());
   }

void
MutableCallsiteTargetOperand::printToString(TR::StringBuf *buf)
   {
   buf->appendf("(mh=%d, mcs=%d)", getMethodHandleIndex(), getMutableCallsiteIndex());
   }

void
InterpreterEmulator::printOperandArray(OperandArray* operands)
   {
   int32_t size = operands->size();
   for (int32_t i = 0; i < size; i++)
      {
      _operandBuf->clear();
      (*operands)[i]->printToString(_operandBuf);
      traceMsg(comp(), "[%d]=%s, ", i, _operandBuf->text());
      }
   if (size > 0)
      traceMsg(comp(), "\n");
   }

// Merge second OperandArray into the first one
// The merge does a union
//
void InterpreterEmulator::mergeOperandArray(OperandArray *first, OperandArray *second)
   {
   bool enableTrace = tracer()->debugLevel();
   if (enableTrace)
      {
      traceMsg(comp(), "Operands before merging:\n");
      printOperandArray(first);
      }

   bool changed = false;
   for (int i = 0; i < _numSlots; i++)
      {
      Operand* firstObj = (*first)[i];
      Operand* secondObj = (*second)[i];

      firstObj = firstObj->merge(secondObj);
      if (firstObj == NULL)
         firstObj = _unknownOperand;

      if (firstObj != (*first)[i])
         changed = true;
      }

   if (enableTrace)
      {
      if (changed)
         {
         traceMsg(comp(), "Operands after merging:\n");
         printOperandArray(first);
         }
      else
         traceMsg(comp(), "Operands is not changed after merging\n");
      }
   }

void
InterpreterEmulator::maintainStackForIf(TR_J9ByteCode bc)
   {
   TR_ASSERT_FATAL(_iteratorWithState, "has to be called when the iterator has state!");
   TR_ASSERT_FATAL(bc == J9BCificmpeq || bc == J9BCificmpne, "InterpreterEmulator::maintainStackForIf can only be called with J9BCificmpeq and J9BCificmpne\n");
   int32_t branchBC = _bcIndex + next2BytesSigned();
   int32_t fallThruBC = _bcIndex + 3;
   IconstOperand * second = pop()->asIconst();
   IconstOperand * first = pop()->asIconst();
   bool canBranch = true;
   bool canFallThru = true;
   // Comment out the branch folding as all the paths have to be interpreted in order
   // to propagate object info in operand stack or local slots. Since branch folding
   // currently only affects thunk archetypes, with similar branch folding in ilgen,
   // calls in dead path won't be inlined, disabling the following code doesn't affect
   // performance
   // TODO: add code to record dead path and ignore it in object info propagation, enable
   // the following code if branch folding is possible in LambdaForm methods
   //
   if (false && second && first)
      {
      switch (bc)
         {
         case J9BCificmpeq:
            canBranch = second->intValue == first->intValue;
            debugTrace(tracer(), "maintainStackForIf ifcmpeq %d == %d\n", second->intValue, first->intValue);
            break;
         case J9BCificmpne:
            canBranch = second->intValue != first->intValue;
            debugTrace(tracer(), "maintainStackForIf ifcmpne %d != %d\n", second->intValue, first->intValue);
            break;

         default:
            break;
         }
      canFallThru = !canBranch;
      }

   // The branch target can be successor of the fall through, so gen fall through block first such
   // that the predecessor is interpreted before the successor in order to propagate the operand
   // stack and local slots state.
   // This doesn't work when the fall through contain control flow, but there is no functional issue
   // as the object info won't be propagated if there exists unvisited predecessor. This will be
   // fixed when we traverse the bytecodes in reverse post order at CFG level.
   //
   if (canFallThru)
      {
      debugTrace(tracer(), "maintainStackForIf canFallThrough to bcIndex=%d\n", fallThruBC);
      genTarget(fallThruBC);
      }

   if (canBranch)
      {
      debugTrace(tracer(), "maintainStackForIf canBranch to bcIndex=%d\n", branchBC);
      genTarget(branchBC);
      }

   }

void
InterpreterEmulator::maintainStackForGetField()
   {
   TR_ASSERT_FATAL(_iteratorWithState, "has to be called when the iterator has state!");
   TR::DataType type = TR::NoType;
   uint32_t fieldOffset;
   int32_t cpIndex = next2Bytes();
   Operand *newOperand = _unknownOperand;
   TR::Symbol *fieldSymbol = TR::Symbol::createPossiblyRecognizedShadowFromCP(
      comp(), trStackMemory(), _calltarget->_calleeMethod, cpIndex, &type, &fieldOffset, false);

   TR::KnownObjectTable *knot = comp()->getKnownObjectTable();
   if (knot &&
       top()->asKnownObject() &&
       !knot->isNull(top()->getKnownObjectIndex())
       && type == TR::Address)
      {
      if (fieldSymbol == NULL)
         {
         debugTrace(tracer(), "field is unresolved");
         }
      else if (!comp()->fej9()->canDereferenceAtCompileTimeWithFieldSymbol(fieldSymbol, cpIndex, _calltarget->_calleeMethod))
         {
         debugTrace(tracer(), "field is not foldable");
         }
      else
         {
         TR::KnownObjectTable::Index baseObjectIndex = top()->getKnownObjectIndex();
         TR::KnownObjectTable::Index resultIndex = TR::KnownObjectTable::UNKNOWN;
         uintptr_t baseObjectAddress = 0;
         uintptr_t fieldAddress = 0;
         bool avoidFolding = true;

#if defined(J9VM_OPT_JITSERVER)
         if (comp()->isOutOfProcessCompilation())
            {
            TR_ResolvedJ9JITServerMethod *serverMethod = static_cast<TR_ResolvedJ9JITServerMethod*>(_calltarget->_calleeMethod);
            TR_ResolvedMethod *clientMethod = serverMethod->getRemoteMirror();

            auto stream = TR::CompilationInfo::getStream();
            stream->write(JITServer::MessageType::KnownObjectTable_dereferenceKnownObjectField,
                  baseObjectIndex, clientMethod, cpIndex);

            auto recv = stream->read<TR::KnownObjectTable::Index, uintptr_t*, uintptr_t, uintptr_t, bool>();
            resultIndex = std::get<0>(recv);
            uintptr_t *objectPointerReference = std::get<1>(recv);
            fieldAddress = std::get<2>(recv);
            baseObjectAddress = std::get<3>(recv);
            avoidFolding = std::get<4>(recv);

            if (resultIndex != TR::KnownObjectTable::UNKNOWN)
               knot->updateKnownObjectTableAtServer(resultIndex, objectPointerReference);
            }
         else
#endif /* defined(J9VM_OPT_JITSERVER) */
            {
            TR::VMAccessCriticalSection dereferenceKnownObjectField(comp()->fej9());
            baseObjectAddress = knot->getPointer(baseObjectIndex);
            TR_OpaqueClassBlock *baseObjectClass = comp()->fej9()->getObjectClass(baseObjectAddress);
            TR_OpaqueClassBlock *fieldDeclaringClass = _calltarget->_calleeMethod->getDeclaringClassFromFieldOrStatic(comp(), cpIndex);

            avoidFolding = TR::TransformUtil::avoidFoldingInstanceField(
                           baseObjectAddress, fieldSymbol, fieldOffset, cpIndex, _calltarget->_calleeMethod, comp());

            if (fieldDeclaringClass && comp()->fej9()->isInstanceOf(baseObjectClass, fieldDeclaringClass, true) == TR_yes)
               {
               fieldAddress = comp()->fej9()->getReferenceFieldAtAddress(baseObjectAddress + fieldOffset);
               resultIndex = knot->getOrCreateIndex(fieldAddress);
               }
            }

         bool fail = resultIndex == TR::KnownObjectTable::UNKNOWN;
         if (fail || avoidFolding)
            {
            int32_t len = 0;
            debugTrace(
               tracer(),
               "%s field in obj%d: %s",
               fail ? "failed to determine value of" : "avoid folding sometimes-foldable",
               baseObjectIndex,
               _calltarget->_calleeMethod->fieldName(cpIndex, len, this->trMemory()));
            }
         else
            {
            // It's OK to print fieldAddress and baseObjectAddress here even
            // without VM access. There's no meaningful difference between:
            // - printing the object's address, then allowing it to move; and
            // - observing the objects's address, then allowing it to move,
            //   then finally printing the observed address.
            newOperand = new (trStackMemory()) KnownObjOperand(resultIndex);
            int32_t len = 0;
            debugTrace(tracer(), "dereference obj%d (%p)from field %s(offset = %d) of base obj%d(%p)\n",
                  newOperand->getKnownObjectIndex(), (void *)fieldAddress, _calltarget->_calleeMethod->fieldName(cpIndex, len, this->trMemory()),
                  fieldOffset, baseObjectIndex, baseObjectAddress);
            }
         }
      }
   pop();
   push(newOperand);
   }

void
InterpreterEmulator::saveStack(int32_t targetIndex)
   {
   if (!_iteratorWithState)
      return;

   // Propagate stack state to successor
   if (!_stack->isEmpty())
      {
      if (!_stacks[targetIndex])
         _stacks[targetIndex] = new (trStackMemory()) ByteCodeStack(*_stack);
      else
         {
         TR_ASSERT_FATAL(_stacks[targetIndex]->size() == _stack->size(), "operand stack from two paths must have the same size, predecessor bci %d target bci %d\n", _bcIndex, targetIndex);
         mergeOperandArray(_stacks[targetIndex], _stack);
         }
      }

   // Propagate local object info to successor
   if (_numSlots)
      {
      if (!_localObjectInfos[targetIndex])
         _localObjectInfos[targetIndex] = new (trStackMemory()) OperandArray(*_currentLocalObjectInfo);
      else
         mergeOperandArray(_localObjectInfos[targetIndex], _currentLocalObjectInfo);
      }
   }

void
InterpreterEmulator::initializeIteratorWithState()
   {
   _iteratorWithState = true;
   _unknownOperand = new (trStackMemory()) Operand();
   uint32_t size = this->maxByteCodeIndex() + 5;
   _flags  = (flags8_t *) this->trMemory()->allocateStackMemory(size * sizeof(flags8_t));
   _stacks = (ByteCodeStack * *) this->trMemory()->allocateStackMemory(size * sizeof(ByteCodeStack *));
   memset(_flags, 0, size * sizeof(flags8_t));
   memset(_stacks, 0, size * sizeof(ByteCodeStack *));
   _stack = new (trStackMemory()) TR_Stack<Operand *>(this->trMemory(), 20, false, stackAlloc);
   _localObjectInfos = (OperandArray**) this->trMemory()->allocateStackMemory(size * sizeof(OperandArray *));
   memset(_localObjectInfos, 0, size * sizeof(OperandArray *));

   int32_t numParmSlots = method()->numberOfParameterSlots();
   _numSlots = numParmSlots + method()->numberOfTemps();

   genBBStart(0);
   setupBBStartContext(0);
   this->setIndex(0);
   }

void
InterpreterEmulator::setupMethodEntryLocalObjectState()
   {
   TR_PrexArgInfo *argInfo = _calltarget->_ecsPrexArgInfo;
   if (argInfo)
      {
      TR_ASSERT_FATAL(argInfo->getNumArgs() == method()->numberOfParameters(), "Prex arg number should match parm number");

      if(tracer()->heuristicLevel())
         {
         heuristicTrace(tracer(), "Save argInfo to slot state array");
         argInfo->dumpTrace();
         }

      method()->makeParameterList(_methodSymbol);
      ListIterator<TR::ParameterSymbol> parms(&_methodSymbol->getParameterList());

      // save prex arg into local var arrays
      for (TR::ParameterSymbol *p = parms.getFirst(); p != NULL; p = parms.getNext())
          {
          int32_t ordinal = p->getOrdinal();
          int32_t slotIndex = p->getSlot();
          TR_PrexArgument *prexArgument = argInfo->get(ordinal);
          if (!prexArgument)
             {
             (*_currentLocalObjectInfo)[slotIndex] = _unknownOperand;
             }
          else
             {
             auto operand = createOperandFromPrexArg(prexArgument);
             if (operand)
                {
                (*_currentLocalObjectInfo)[slotIndex] = operand;
                }
             else
                (*_currentLocalObjectInfo)[slotIndex] = _unknownOperand;
             }
         if (tracer()->heuristicLevel())
            {
            _operandBuf->clear();
            (*_currentLocalObjectInfo)[slotIndex]->printToString(_operandBuf);
            heuristicTrace(
               tracer(),
               "Creating operand %s for parm %d slot %d from PrexArgument %p",
               _operandBuf->text(),
               ordinal,
               slotIndex,
               prexArgument);
            }
         }
      }
   }

bool
InterpreterEmulator::hasUnvisitedPred(TR::Block* block)
   {
   TR_PredecessorIterator pi(block);
   for (TR::CFGEdge *edge = pi.getFirst(); edge != NULL; edge = pi.getNext())
      {
      TR::Block *fromBlock = toBlock(edge->getFrom());
      auto fromBCIndex = fromBlock->getEntry()->getNode()->getByteCodeIndex();
      if (!isGenerated(fromBCIndex))
         {
         return true;
         }
      }

   return false;
   }

void
InterpreterEmulator::setupBBStartStackState(int32_t index)
   {
   if (index == 0)
      return;

   auto block = blocks(index);
   auto stack = _stacks[index];
   if (stack && hasUnvisitedPred(block))
      {
      heuristicTrace(tracer(), "block_%d at bc index %d has unvisited predecessor, setting stack operand info to unknown", block->getNumber(), index);
      for (int32_t i = 0; i < stack->size(); ++i)
         (*stack)[i] = _unknownOperand;
      }
   }

void
InterpreterEmulator::setupBBStartLocalObjectState(int32_t index)
   {
   if (_numSlots == 0)
      return;

   if (!_localObjectInfos[index])
      {
      _localObjectInfos[index] = new (trStackMemory()) OperandArray(trMemory(), _numSlots, false, stackAlloc);
      for (int32_t i = 0; i < _numSlots; i++)
          (*_localObjectInfos[index])[i] = _unknownOperand;
      }
   else if (hasUnvisitedPred(blocks(index)))
      {
      heuristicTrace(tracer(), "block_%d at bc index %d has unvisited predecessor, setting local object info to unknown", blocks(index)->getNumber(), index);
      for (int32_t i = 0; i < _numSlots; i++)
          (*_localObjectInfos[index])[i] = _unknownOperand;
      }

   _currentLocalObjectInfo = _localObjectInfos[index];

   if (index == 0)
      setupMethodEntryLocalObjectState();
   }

int32_t
InterpreterEmulator::setupBBStartContext(int32_t index)
   {
   if (_iteratorWithState)
      {
      setupBBStartStackState(index);
      setupBBStartLocalObjectState(index);
      }
   Base::setupBBStartContext(index);
   return index;
   }

bool
InterpreterEmulator::maintainStack(TR_J9ByteCode bc)
   {
   TR_ASSERT_FATAL(_iteratorWithState, "has to be called when the iterator has state!");
   int slotIndex = -1;
   switch (bc)
      {
      case J9BCgetfield: maintainStackForGetField(); break;
      case J9BCaload0: slotIndex = 0; maintainStackForAload(slotIndex); break;
      case J9BCaload1: slotIndex = 1; maintainStackForAload(slotIndex); break;
      case J9BCaload2: slotIndex = 2; maintainStackForAload(slotIndex); break;
      case J9BCaload3: slotIndex = 3; maintainStackForAload(slotIndex); break;
      case J9BCaload:  slotIndex = nextByte(); maintainStackForAload(slotIndex); break;
      case J9BCaloadw: slotIndex = next2Bytes(); maintainStackForAload(slotIndex); break;

      case J9BCinvokespecial:
      case J9BCinvokespecialsplit:
      case J9BCinvokevirtual:
      case J9BCinvokestatic:
      case J9BCinvokestaticsplit:
      case J9BCinvokedynamic:
      case J9BCinvokehandle:
         maintainStackForCall();
         break;
      case J9BCiconstm1: push (new (trStackMemory()) IconstOperand(-1)); break;
      case J9BCiconst0:  push (new (trStackMemory()) IconstOperand(0)); break;
      case J9BCiconst1:  push (new (trStackMemory()) IconstOperand(1)); break;
      case J9BCiconst2:  push (new (trStackMemory()) IconstOperand(2)); break;
      case J9BCiconst3:  push (new (trStackMemory()) IconstOperand(3)); break;
      case J9BCiconst4:  push (new (trStackMemory()) IconstOperand(4)); break;
      case J9BCiconst5:  push (new (trStackMemory()) IconstOperand(5)); break;
      case J9BCifne:
         push (new (trStackMemory()) IconstOperand(0));
         maintainStackForIf(J9BCificmpne);
         break;
      case J9BCifeq:
         push (new (trStackMemory()) IconstOperand(0));
         maintainStackForIf(J9BCificmpeq);
         break;
      case J9BCgoto:
         genTarget(bcIndex() + next2BytesSigned());
         break;
      case J9BCpop:
      case J9BCputfield:
      case J9BCputstatic:
         pop();
         break;
      case J9BCladd:
      case J9BCiadd:
      case J9BCisub:
      case J9BCiand:
         popn(2);
         pushUnknownOperand();
         break;
      case J9BCistore: case J9BClstore: case J9BCfstore: case J9BCdstore:
      case J9BCistorew: case J9BClstorew: case J9BCfstorew: case J9BCdstorew:
      case J9BCistore0: case J9BCistore1: case J9BCistore2: case J9BCistore3:
      case J9BClstore0: case J9BClstore1: case J9BClstore2: case J9BClstore3:
      case J9BCfstore0: case J9BCfstore1: case J9BCfstore2: case J9BCfstore3:
      case J9BCdstore0: case J9BCdstore1: case J9BCdstore2: case J9BCdstore3:
         pop();
         break;
      // Maintain stack for object store
      case J9BCastorew: maintainStackForAstore(next2Bytes()); break;
      case J9BCastore: maintainStackForAstore(nextByte()); break;
      case J9BCastore0: maintainStackForAstore(0); break;
      case J9BCastore1: maintainStackForAstore(1); break;
      case J9BCastore2: maintainStackForAstore(2); break;
      case J9BCastore3: maintainStackForAstore(3); break;

      case J9BCiload0: case J9BCiload1: case J9BCiload2: case J9BCiload3:
      case J9BCdload0: case J9BCdload1: case J9BCdload2: case J9BCdload3:
      case J9BClload0: case J9BClload1: case J9BClload2: case J9BClload3:
      case J9BCfload0: case J9BCfload1: case J9BCfload2: case J9BCfload3:
      case J9BCiloadw: case J9BClloadw: case J9BCfloadw: case J9BCdloadw:
      case J9BCiload:  case J9BClload:  case J9BCfload:  case J9BCdload:
         pushUnknownOperand();
         break;
      case J9BCgetstatic:
         maintainStackForGetStatic();
         break;
      case J9BCgenericReturn:
      case J9BCReturnC:
      case J9BCReturnS:
      case J9BCReturnB:
      case J9BCReturnZ:
         maintainStackForReturn();
         break;
      case J9BCi2l:
         break;
      case J9BCcheckcast:
         break;
      case J9BCdup:
         push(top());
         break;
      case J9BCldc:
         maintainStackForldc(nextByte()); break;
      default:
         static const bool assertfatal = feGetEnv("TR_AssertFatalForUnexpectedBytecodeInMethodHandleThunk") ? true: false;
         if (!assertfatal)
            debugTrace(tracer(), "unexpected bytecode in thunk archetype %s (%p) at bcIndex %d %s (%d)\n", _calltarget->_calleeMethod->signature(comp()->trMemory()), _calltarget, bcIndex(), comp()->fej9()->getByteCodeName(nextByte(0)), bc);
         else
            TR_ASSERT_FATAL(0, "unexpected bytecode in thunk archetype %s (%p) at bcIndex %d %s (%d)\n", _calltarget->_calleeMethod->signature(comp()->trMemory()), _calltarget, bcIndex(), comp()->fej9()->getByteCodeName(nextByte(0)), bc);

         TR::DebugCounter::incStaticDebugCounter(comp(),
            TR::DebugCounter::debugCounterName(comp(),
               "InterpreterEmulator.unexpectedBytecode/(root=%s)/(%s)/bc=%d/%s",
               comp()->signature(),
               _calltarget->_calleeMethod->signature(comp()->trMemory()),
               _bcIndex,
               comp()->fej9()->getByteCodeName(nextByte(0)))
            );
         return false;
      }
   return true;
   }

void
InterpreterEmulator::maintainStackForReturn()
   {
   if (method()->returnType() != TR::NoType)
      pop();
   }

void
InterpreterEmulator::maintainStackForGetStatic()
   {
   TR_ASSERT_FATAL(_iteratorWithState, "has to be called when the iterator has state!");
   if (comp()->compileRelocatableCode())
      {
      pushUnknownOperand();
      return;
      }

   int32_t cpIndex = next2Bytes();
   debugTrace(tracer(), "getstatic cpIndex %d", cpIndex);

   void * dataAddress;
   bool isVolatile, isPrivate, isUnresolvedInCP, isFinal;
   TR::DataType type = TR::NoType;
   auto owningMethod = _calltarget->_calleeMethod;
   bool resolved = owningMethod->staticAttributes(
      comp(),
      cpIndex,
      &dataAddress,
      &type,
      &isVolatile,
      &isFinal,
      &isPrivate,
      false,
      &isUnresolvedInCP);

   TR::KnownObjectTable::Index knownObjectIndex = TR::KnownObjectTable::UNKNOWN;
   if (resolved && isFinal && type == TR::Address)
      {
      knownObjectIndex = TR::TransformUtil::knownObjectFromFinalStatic(
          comp(), owningMethod, cpIndex, dataAddress);
      }

   if (knownObjectIndex != TR::KnownObjectTable::UNKNOWN)
      push(new (trStackMemory()) KnownObjOperand(knownObjectIndex));
   else
      pushUnknownOperand();
   }

void
InterpreterEmulator::maintainStackForAload(int slotIndex)
   {
   TR_ASSERT_FATAL(_iteratorWithState, "has to be called when the iterator has state!");

   push((*_currentLocalObjectInfo)[slotIndex]);
   }

void
InterpreterEmulator::maintainStackForAstore(int slotIndex)
   {
   TR_ASSERT_FATAL(_iteratorWithState, "has to be called when the iterator has state!");
   (*_currentLocalObjectInfo)[slotIndex] = pop();
   }

void
InterpreterEmulator::maintainStackForldc(int32_t cpIndex)
   {
   TR::DataType type = method()->getLDCType(cpIndex);
   switch (type)
      {
      case TR::Address:
         // TODO: should add a function to check if cp entry is unresolved for all constant
         // not just for string. Currently only do it for string because it may be patched
         // to a different object in OpenJDK MethodHandle implementation
         //
         if (method()->isStringConstant(cpIndex) && !method()->isUnresolvedString(cpIndex))
            {
            uintptr_t * location = (uintptr_t *)method()->stringConstant(cpIndex);
            TR::KnownObjectTable *knot = comp()->getKnownObjectTable();
            if (knot)
               {
               TR::KnownObjectTable::Index koi = knot->getOrCreateIndexAt(location);
               push(new (trStackMemory()) KnownObjOperand(koi));
               debugTrace(tracer(), "aload known obj%d from ldc %d", koi, cpIndex);
               return;
               }
            }
         break;
      default:
         break;
      }

   pushUnknownOperand();
   }

void
InterpreterEmulator::maintainStackForCall(Operand *result, int32_t numArgs, TR::DataType returnType)
   {
   TR_ASSERT_FATAL(_iteratorWithState, "has to be called when the iterator has state!");

   for (int i = 1; i <= numArgs; i++)
      pop();

   if (result)
      push(result);
   else if (returnType != TR::NoType)
      pushUnknownOperand();
   }

void
InterpreterEmulator::maintainStackForCall()
   {
   TR_ASSERT_FATAL(_iteratorWithState, "has to be called when the iterator has state!");
   int32_t numOfArgs = 0;
   TR::DataType returnType = TR::NoType;
   Operand* result = NULL;

   if (_currentCallMethod)
      result = getReturnValue(_currentCallMethod);

   // If the caller is thunk archetype, the load of parm `argPlaceholder` can
   // be expanded to loads of multiple arguments, so we can't pop the number
   // of arguments of a refined call
   //
   if (_currentCallSite && !_callerIsThunkArchetype)
      {
      if (_currentCallSite->_isInterface)
         {
         numOfArgs = _currentCallSite->_interfaceMethod->numberOfExplicitParameters() + 1;
         returnType = _currentCallSite->_interfaceMethod->returnType();
         }
      else if (_currentCallSite->_initialCalleeMethod)
         {
         numOfArgs = _currentCallSite->_initialCalleeMethod->numberOfParameters();
         returnType = _currentCallSite->_initialCalleeMethod->returnType();
         }
      }
   else
      {
      int32_t cpIndex = next2Bytes();
      bool isStatic = false;
      switch (current())
         {
         case J9BCinvokespecialsplit:
            cpIndex |= J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG;
            break;
         case J9BCinvokestaticsplit:
            cpIndex |= J9_STATIC_SPLIT_TABLE_INDEX_FLAG;
         case J9BCinvokestatic:
            isStatic = true;
            break;
         case J9BCinvokedynamic:
         case J9BCinvokehandle:
            TR_ASSERT_FATAL(false, "Can't maintain stack for unresolved invokehandle");
            break;

         default:
            break;
         }
      TR::Method * calleeMethod = comp()->fej9()->createMethod(trMemory(), _calltarget->_calleeMethod->containingClass(), cpIndex);
      numOfArgs = calleeMethod->numberOfExplicitParameters() + (isStatic ? 0 : 1);
      returnType = calleeMethod->returnType();
      }
   maintainStackForCall(result, numOfArgs, returnType);
   }

void
InterpreterEmulator::dumpStack()
   {
   if (!tracer()->debugLevel())
      return;

   debugTrace(tracer(), "operandStack after bytecode %d : %s ", _bcIndex, comp()->fej9()->getByteCodeName(nextByte(0)));
   for (int i = 0; i < _stack->size(); i++ )
      {
      Operand *x = (*_stack)[i];
      _operandBuf->clear();
      x->printToString(_operandBuf);
      debugTrace(tracer(), "[%d]=%s", i, _operandBuf->text());
      }
   }

Operand *
InterpreterEmulator::getReturnValue(TR_ResolvedMethod *callee)
   {
   if (!callee)
      return NULL;
   Operand *result = NULL;
   TR::RecognizedMethod recognizedMethod = callee->getRecognizedMethod();
   TR::KnownObjectTable *knot = comp()->getKnownObjectTable();

   TR::IlGeneratorMethodDetails & details = comp()->ilGenRequest().details();
   if (_callerIsThunkArchetype && details.isMethodHandleThunk())
      {
      J9::MethodHandleThunkDetails & thunkDetails = static_cast<J9::MethodHandleThunkDetails &>(details);
      if (!thunkDetails.isCustom())
         recognizedMethod = TR::unknownMethod;
      }

   switch (recognizedMethod)
      {
      case TR::java_lang_invoke_ILGenMacros_isCustomThunk:
         result = new (trStackMemory()) IconstOperand(1);
         break;
      case TR::java_lang_invoke_ILGenMacros_isShareableThunk:
         result = new (trStackMemory()) IconstOperand(0);
         break;

#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
      case TR::java_lang_invoke_DelegatingMethodHandle_getTarget:
         {
         TR::KnownObjectTable::Index dmhIndex = top()->getKnownObjectIndex();
         bool trace = tracer()->debugLevel();
         TR::KnownObjectTable::Index targetIndex =
            comp()->fej9()->delegatingMethodHandleTarget(comp(), dmhIndex, trace);

         if (targetIndex == TR::KnownObjectTable::UNKNOWN)
            return NULL;

         result = new (trStackMemory()) KnownObjOperand(targetIndex);
         break;
         }
#endif

      case TR::java_lang_invoke_MutableCallSite_getTarget:
      case TR::java_lang_invoke_Invokers_getCallSiteTarget:
         {
         // The CallSite object is always topmost on the stack.
         TR::KnownObjectTable::Index callSiteIndex = top()->getKnownObjectIndex();
         if (callSiteIndex == TR::KnownObjectTable::UNKNOWN)
            return NULL;

         TR::KnownObjectTable::Index resultIndex = TR::KnownObjectTable::UNKNOWN;
         const char * const mcsClassName = "java/lang/invoke/MutableCallSite";
         const int mcsClassNameLen = (int)strlen(mcsClassName);
         TR_OpaqueClassBlock *mutableCallsiteClass =
            fe()->getSystemClassFromClassName(mcsClassName, mcsClassNameLen, true);

         debugTrace(tracer(), "potential MCS target: call site obj%d(*%p) mutableCallsiteClass %p\n", callSiteIndex, knot->getPointerLocation(callSiteIndex), mutableCallsiteClass);
         if (mutableCallsiteClass)
            {
            if (recognizedMethod != TR::java_lang_invoke_MutableCallSite_getTarget)
               {
               TR_OpaqueClassBlock *callSiteType =
                  fe()->getObjectClassFromKnownObjectIndex(comp(), callSiteIndex);
               if (callSiteType == NULL)
                  {
                  debugTrace(tracer(), "failed to determine concrete CallSite type");
                  return NULL;
                  }
               else if (fe()->isInstanceOf(callSiteType, mutableCallsiteClass, true) != TR_yes)
                  {
                  debugTrace(tracer(), "not a MutableCallSite");
                  return NULL;
                  }
               }

   #if defined(J9VM_OPT_JITSERVER)
            if (comp()->isOutOfProcessCompilation())
               {
               auto stream = TR::CompilationInfo::getStream();
               stream->write(JITServer::MessageType::KnownObjectTable_dereferenceKnownObjectField2, mutableCallsiteClass, callSiteIndex);

               auto recv = stream->read<TR::KnownObjectTable::Index, uintptr_t*>();
               resultIndex = std::get<0>(recv);
               uintptr_t *objectPointerReference = std::get<1>(recv);

               if (resultIndex != TR::KnownObjectTable::UNKNOWN)
                  {
                  knot->updateKnownObjectTableAtServer(resultIndex, objectPointerReference);
                  }
               result = new (trStackMemory()) MutableCallsiteTargetOperand(resultIndex, callSiteIndex);
               }
            else
   #endif /* defined(J9VM_OPT_JITSERVER) */
               {
               TR::VMAccessCriticalSection dereferenceKnownObjectField(comp()->fej9());
               int32_t targetFieldOffset = comp()->fej9()->getInstanceFieldOffset(mutableCallsiteClass, "target", "Ljava/lang/invoke/MethodHandle;");
               uintptr_t receiverAddress = knot->getPointer(callSiteIndex);
               TR_OpaqueClassBlock *receiverClass = comp()->fej9()->getObjectClass(receiverAddress);
               TR_ASSERT_FATAL(comp()->fej9()->isInstanceOf(receiverClass, mutableCallsiteClass, true) == TR_yes, "receiver of mutableCallsite_getTarget must be instance of MutableCallSite (*%p)", knot->getPointerLocation(callSiteIndex));
               uintptr_t fieldAddress = comp()->fej9()->getReferenceFieldAt(receiverAddress, targetFieldOffset);
               resultIndex = knot->getOrCreateIndex(fieldAddress);
               result = new (trStackMemory()) MutableCallsiteTargetOperand(resultIndex, callSiteIndex);
               }
            }
         }
         break;
      case TR::java_lang_invoke_DirectMethodHandle_internalMemberName:
      case TR::java_lang_invoke_DirectMethodHandle_internalMemberNameEnsureInit:
         {
         Operand* mh = top();
         TR::KnownObjectTable::Index mhIndex = top()->getKnownObjectIndex();
         debugTrace(tracer(), "Known DirectMethodHandle koi %d\n", mhIndex);
         TR::KnownObjectTable *knot = comp()->getKnownObjectTable();
         if (knot && mhIndex != TR::KnownObjectTable::UNKNOWN && !knot->isNull(mhIndex))
            {
            TR::KnownObjectTable::Index memberIndex = comp()->fej9()->getMemberNameFieldKnotIndexFromMethodHandleKnotIndex(comp(), mhIndex, "member");
            debugTrace(tracer(), "Known internal member name koi %d\n", memberIndex);
            result = new (trStackMemory()) KnownObjOperand(memberIndex);
            }
         break;
         }
      case TR::java_lang_invoke_DirectMethodHandle_constructorMethod:
         {
         Operand* mh = top();
         TR::KnownObjectTable::Index mhIndex = top()->getKnownObjectIndex();
         debugTrace(tracer(), "Known DirectMethodHandle koi %d\n", mhIndex);
         TR::KnownObjectTable *knot = comp()->getKnownObjectTable();
         if (knot && mhIndex != TR::KnownObjectTable::UNKNOWN && !knot->isNull(mhIndex))
            {
            TR::KnownObjectTable::Index memberIndex = comp()->fej9()->getMemberNameFieldKnotIndexFromMethodHandleKnotIndex(comp(), mhIndex, "initMethod");
            debugTrace(tracer(), "Known internal member name koi %d\n", memberIndex);
            result = new (trStackMemory()) KnownObjOperand(memberIndex);
            }
         break;
         }

      default:
         break;
      }
   return result;
   }

void
InterpreterEmulator::refineResolvedCalleeForInvokestatic(TR_ResolvedMethod *&callee, TR::KnownObjectTable::Index & mcsIndex, TR::KnownObjectTable::Index & mhIndex, bool &isIndirectCall)
   {
   TR_ASSERT_FATAL(_iteratorWithState, "has to be called when the iterator has state!");
   if (!comp()->getOrCreateKnownObjectTable())
      return;

   bool isVirtual = false;
   TR::RecognizedMethod rm = callee->getRecognizedMethod();
   switch (rm)
      {
      // refine the ILGenMacros_invokeExact* callees
      case TR::java_lang_invoke_ILGenMacros_invokeExact:
      case TR::java_lang_invoke_ILGenMacros_invokeExact_X:
      case TR::java_lang_invoke_ILGenMacros_invokeExactAndFixup:
         {
         int argNum = callee->numberOfExplicitParameters();
         if (argNum > 0)
            {
            Operand *operand = topn(argNum-1); // for the ILGenMacros_invokeExact* methods, the first argument is always the methodhandle object
            MutableCallsiteTargetOperand * mcsOperand = operand->asMutableCallsiteTargetOperand();
            if (mcsOperand)
               {
               mhIndex = mcsOperand->getMethodHandleIndex();
               mcsIndex = mcsOperand->getMutableCallsiteIndex();
               }
            else
               mhIndex = operand->getKnownObjectIndex();
            }

         if (mhIndex != TR::KnownObjectTable::UNKNOWN)
            {
            debugTrace(tracer(), "refine java_lang_invoke_MethodHandle_invokeExact with obj%d to archetype specimen at bcIndex=%d\n", mhIndex, _bcIndex);
            callee = comp()->fej9()->createMethodHandleArchetypeSpecimen(this->trMemory(), comp()->getKnownObjectTable()->getPointerLocation(mhIndex), _calltarget->_calleeMethod);
            }
         return;
         }
      // refine the leaf method handle callees
      case TR::java_lang_invoke_VirtualHandle_virtualCall:
         isVirtual = true;
      case TR::java_lang_invoke_DirectHandle_directCall:
         {
         TR_J9VMBase *fej9 = comp()->fej9();
         TR_J9VMBase::MethodOfHandle moh = fej9->methodOfDirectOrVirtualHandle(
            _calltarget->_calleeMethod->getMethodHandleLocation(), isVirtual);

         TR_ASSERT_FATAL(moh.j9method != NULL, "Must have a j9method to generate a custom call");
         uint32_t vTableSlot = isVirtual ? (uint32_t)moh.vmSlot : 0;
         TR_ResolvedMethod *newCallee = fej9->createResolvedMethodWithVTableSlot(
            trMemory(), vTableSlot, moh.j9method, _calltarget->_calleeMethod);

         // Don't refine virtualCall to an interface method, which will confuse
         // the virtual call site logic in visitInvokestatic()
         TR_OpaqueClassBlock *defClass = newCallee->classOfMethod();
         if (isVirtual && TR::Compiler->cls.isInterfaceClass(comp(), defClass))
            return;

         isIndirectCall = isVirtual;
         callee = newCallee;
         return;
         }
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
      case TR::java_lang_invoke_MethodHandle_linkToStatic:
      case TR::java_lang_invoke_MethodHandle_linkToSpecial:
      case TR::java_lang_invoke_MethodHandle_linkToVirtual:
         {
         TR::KnownObjectTable::Index memberNameIndex = top()->getKnownObjectIndex();
         TR_J9VMBase* fej9 = comp()->fej9();
         auto targetMethod = fej9->targetMethodFromMemberName(comp(), memberNameIndex);
         if (!targetMethod)
            return;

         uint32_t vTableSlot = 0;
         if (rm == TR::java_lang_invoke_MethodHandle_linkToVirtual)
            {
            uintptr_t slot = fej9->vTableOrITableIndexFromMemberName(comp(), memberNameIndex);
            if ((slot & J9_JNI_MID_INTERFACE) == 0)
               {
               vTableSlot = (uint32_t)slot;
               }
            else
               {
               // TODO: Refine to the method identified by the itable slot
               // together with the defining (interface) class of the method
               // from the MemberName. For such a refinement to matter,
               // TR_J9InterfaceCallSite will need to be able to find call
               // targets without a CP index.
               //
               // For the moment, just leave the call unrefined.
               //
               return;
               }
            }

         // Direct or virtual dispatch. A vTableSlot of 0 indicates a direct
         // call, in which case vTableSlot won't really be used as such.
         callee = fej9->createResolvedMethodWithVTableSlot(comp()->trMemory(), vTableSlot, targetMethod, _calltarget->_calleeMethod);

         isIndirectCall = vTableSlot != 0;
         TR_ASSERT_FATAL(
            !isIndirectCall
            || rm == TR::java_lang_invoke_MethodHandle_linkToVirtual
            || rm == TR::java_lang_invoke_MethodHandle_linkToInterface,
            "indirect linkTo call should only be linkToVirtual or linkToInterface");

         heuristicTrace(tracer(), "Refine linkTo to %s\n", callee->signature(trMemory(), stackAlloc));
         // The refined method doesn't take MemberName as an argument, pop MemberName out of the operand stack
         pop();
         return;
         }
#endif //J9VM_OPT_OPENJDK_METHODHANDLE

      default:
         break;
      }
   }

bool
InterpreterEmulator::findAndCreateCallsitesFromBytecodes(bool wasPeekingSuccessfull, bool withState)
   {
   heuristicTrace(tracer(),"Find and create callsite %s\n", withState ? "with state" : "without state");

   if (withState)
      initializeIteratorWithState();
   _wasPeekingSuccessfull = wasPeekingSuccessfull;
   _currentInlinedBlock = NULL;
   TR_J9ByteCode bc = first();
   while (bc != J9BCunknown)
      {
      heuristicTrace(tracer(), "%4d: %s\n", _bcIndex, comp()->fej9()->getByteCodeName(_code[_bcIndex]));

      _currentCallSite = NULL;
      _currentCallMethod = NULL;
      _currentCallMethodUnrefined = NULL;

      if (_InterpreterEmulatorFlags[_bcIndex].testAny(InterpreterEmulator::BytecodePropertyFlag::bbStart))
         {
         _currentInlinedBlock = TR_J9EstimateCodeSize::getBlock(comp(), _blocks, _calltarget->_calleeMethod, _bcIndex, *_cfg);
         debugTrace(tracer(),"Found current block %p, number %d for bci %d\n", _currentInlinedBlock, (_currentInlinedBlock) ? _currentInlinedBlock->getNumber() : -1, _bcIndex);
         }


      TR_ASSERT_FATAL(!isGenerated(_bcIndex), "InterpreterEmulator::findCallsitesFromBytecodes bcIndex %d has been generated\n", _bcIndex);
      _newBCInfo->setByteCodeIndex(_bcIndex);

      switch (bc)
         {
         case J9BCinvokedynamic: visitInvokedynamic(); break;
         case J9BCinvokevirtual: visitInvokevirtual(); break;
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
         case J9BCinvokehandle: visitInvokehandle(); break;
#endif
         case J9BCinvokespecial:
         case J9BCinvokespecialsplit: visitInvokespecial(); break;
         case J9BCinvokestatic:
         case J9BCinvokestaticsplit: visitInvokestatic(); break;
         case J9BCinvokeinterface: visitInvokeinterface(); break;

         default:
            break;
         }

      if (_iteratorWithState)
         {
         if (maintainStack(bc))
            dumpStack();
         else
            return false;
         }

      _pca.updateArg(bc);
      bc = findNextByteCodeToVisit();
      }

   heuristicTrace(tracer(), "Finish findAndCreateCallsitesFromBytecodes\n");
   return true;
   }

TR_J9ByteCode
InterpreterEmulator::findNextByteCodeToVisit()
   {
   if (!_iteratorWithState)
      next();
   else
      {
      setIsGenerated(_bcIndex);
      if (_InterpreterEmulatorFlags[_bcIndex].testAny(InterpreterEmulator::BytecodePropertyFlag::isBranch))
         {
         setIndex(Base::findNextByteCodeToGen());
         debugTrace(tracer(), "current bc is branch next bytecode to generate is %d\n", _bcIndex);
         }
      else next();
      }

   if (_bcIndex < _maxByteCodeIndex && _InterpreterEmulatorFlags[_bcIndex].testAny(InterpreterEmulator::BytecodePropertyFlag::bbStart))
      {
      if (isGenerated(_bcIndex))
         setIndex(Base::findNextByteCodeToGen());
      }
   return current();
   }

void
InterpreterEmulator::prepareToFindAndCreateCallsites(TR::Block **blocks, flags8_t * flags, TR_CallSite ** callSites, TR::CFG *cfg, TR_ByteCodeInfo *newBCInfo, int32_t recursionDepth, TR_CallStack *callStack)
   {
   _blocks = blocks;
   _InterpreterEmulatorFlags = flags;
   _callSites = callSites;
   _cfg = cfg;
   _newBCInfo = newBCInfo;
   _recursionDepth = recursionDepth;
   _callStack = callStack;
   _nonColdCallExists = false;
   _inlineableCallExists = false;
   }

void
InterpreterEmulator::visitInvokedynamic()
   {
   TR_ResolvedJ9Method * owningMethod = static_cast<TR_ResolvedJ9Method*>(_methodSymbol->getResolvedMethod());
   int32_t callSiteIndex = next2Bytes();
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
   if (owningMethod->isUnresolvedCallSiteTableEntry(callSiteIndex)
         || comp()->compileRelocatableCode()
      ) return; // do nothing if unresolved, is AOT compilation
   uintptr_t * invokeCacheArray = (uintptr_t *) owningMethod->callSiteTableEntryAddress(callSiteIndex);
   // CallSite table entry is expected to be an array object upon successful resolution, but this is not
   // the case when an exception occurs during the invokedynamic resolution, in which case an exception
   // object is placed in the slot instead.
   if (!comp()->fej9()->isInvokeCacheEntryAnArray(invokeCacheArray))
      return;

   updateKnotAndCreateCallSiteUsingInvokeCacheArray(owningMethod, invokeCacheArray, -1);
#else
   bool isInterface = false;
   bool isIndirectCall = false;
   TR::Method *interfaceMethod = 0;
   TR::TreeTop *callNodeTreeTop = 0;
   TR::Node *parent = 0;
   TR::Node *callNode = 0;
   TR::ResolvedMethodSymbol *resolvedSymbol = 0;
   Operand *result = NULL;
   TR::KnownObjectTable *knot = comp()->getOrCreateKnownObjectTable();
   if (knot && !owningMethod->isUnresolvedCallSiteTableEntry(callSiteIndex))
      {
      isIndirectCall = true;
      uintptr_t *entryLocation = (uintptr_t*)owningMethod->callSiteTableEntryAddress(callSiteIndex);
      // Add callsite handle to known object table
      knot->getOrCreateIndexAt((uintptr_t*)entryLocation);
      _currentCallMethod = comp()->fej9()->createMethodHandleArchetypeSpecimen(this->trMemory(), entryLocation, owningMethod);
      _currentCallMethodUnrefined = _currentCallMethod;
      bool allconsts= false;

      heuristicTrace(tracer(),"numberOfExplicitParameters = %d  _pca.getNumPrevConstArgs = %d\n", _currentCallMethod->numberOfExplicitParameters() , _pca.getNumPrevConstArgs(_currentCallMethod->numberOfExplicitParameters()));
      if (_currentCallMethod->numberOfExplicitParameters() > 0 && _currentCallMethod->numberOfExplicitParameters() <= _pca.getNumPrevConstArgs(_currentCallMethod->numberOfExplicitParameters()))
         allconsts = true;

      TR_CallSite *callsite = new (comp()->trHeapMemory()) TR_J9MethodHandleCallSite(_calltarget->_calleeMethod, callNodeTreeTop,   parent,
                                                                        callNode, interfaceMethod, _currentCallMethod->classOfMethod(),
                                                                        -1, -1, _currentCallMethod,
                                                                        resolvedSymbol, isIndirectCall, isInterface, *_newBCInfo, comp(),
                                                                        _recursionDepth, allconsts);

      findTargetAndUpdateInfoForCallsite(callsite);
      }
#endif //J9VM_OPT_OPENJDK_METHODHANDLE
   }

#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
void
InterpreterEmulator::visitInvokehandle()
   {
   int32_t cpIndex = next2Bytes();
   TR_ResolvedJ9Method * owningMethod = static_cast<TR_ResolvedJ9Method*>(_methodSymbol->getResolvedMethod());
   if (owningMethod->isUnresolvedMethodTypeTableEntry(cpIndex)
         || comp()->compileRelocatableCode()
      ) return; // do nothing if unresolved, is an AOT compilation
   uintptr_t * invokeCacheArray = (uintptr_t *) owningMethod->methodTypeTableEntryAddress(cpIndex);
   updateKnotAndCreateCallSiteUsingInvokeCacheArray(owningMethod, invokeCacheArray, cpIndex);
   }

void
InterpreterEmulator::updateKnotAndCreateCallSiteUsingInvokeCacheArray(TR_ResolvedJ9Method* owningMethod, uintptr_t * invokeCacheArray, int32_t cpIndex)
   {
   TR_J9VMBase *fej9 = comp()->fej9();
   TR::KnownObjectTable::Index idx = fej9->getKnotIndexOfInvokeCacheArrayAppendixElement(comp(), invokeCacheArray);
   if (_iteratorWithState)
      {
      if (idx != TR::KnownObjectTable::UNKNOWN)
         push(new (trStackMemory()) KnownObjOperand(idx));
      else
         pushUnknownOperand();
      }

   TR_ResolvedMethod * targetMethod = fej9->targetMethodFromInvokeCacheArrayMemberNameObj(comp(), owningMethod, invokeCacheArray);
   bool isInterface = false;
   bool isIndirectCall = false;
   TR::Method *interfaceMethod = 0;
   TR::TreeTop *callNodeTreeTop = 0;
   TR::Node *parent = 0;
   TR::Node *callNode = 0;
   TR::ResolvedMethodSymbol *resolvedSymbol = 0;
   TR::KnownObjectTable *knot = comp()->getOrCreateKnownObjectTable();
   bool allconsts = false;
   if (targetMethod->numberOfExplicitParameters() > 0 && targetMethod->numberOfExplicitParameters() <= _pca.getNumPrevConstArgs(targetMethod->numberOfExplicitParameters()))
         allconsts = true;
   TR_CallSite *callsite = new (comp()->trHeapMemory()) TR_DirectCallSite(_calltarget->_calleeMethod, callNodeTreeTop,   parent,
                                                                        callNode, interfaceMethod, targetMethod->classOfMethod(),
                                                                        -1, cpIndex, targetMethod,
                                                                        resolvedSymbol, isIndirectCall, isInterface, *_newBCInfo, comp(),
                                                                        _recursionDepth, allconsts);

   findTargetAndUpdateInfoForCallsite(callsite, idx);
   }

#endif //J9VM_OPT_OPENJDK_METHODHANDLE

bool
InterpreterEmulator::isCurrentCallUnresolvedOrCold(TR_ResolvedMethod *resolvedMethod, bool isUnresolvedInCP)
   {
   if (!resolvedMethod)
      return true;

   bool isIndirectCall = false;
   if (current() == J9BCinvokevirtual)
      isIndirectCall = true;

   // Since bytecodes in a thunk archetype are never interpreted,
   // most of the cp entries may appear unresolved, and we always
   // compile-time resolve the cp entries. Thus ignore resolution
   // status of cp entries of thunk arthetype
   //
   if (_callerIsThunkArchetype)
      return resolvedMethod->isCold(comp(), isIndirectCall);
   else
      return (isUnresolvedInCP || resolvedMethod->isCold(comp(), isIndirectCall));
   }

void
InterpreterEmulator::debugUnresolvedOrCold(TR_ResolvedMethod *resolvedMethod)
   {
   int32_t cpIndex = next2Bytes();
   if(tracer()->heuristicLevel())
      {
      if (resolvedMethod)
         heuristicTrace(tracer(), "Depth %d: Call at bc index %d is Cold.  Not searching for targets. Signature %s", _recursionDepth, _bcIndex, resolvedMethod->signature(comp()->trMemory()));
      else
         {
         switch (current())
            {
            case J9BCinvokespecialsplit:
               cpIndex |= J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG;
               break;
            case J9BCinvokestaticsplit:
               cpIndex |= J9_STATIC_SPLIT_TABLE_INDEX_FLAG;
               break;

            default:
               break;
            }
         TR::Method *meth = comp()->fej9()->createMethod(this->trMemory(), _calltarget->_calleeMethod->containingClass(), cpIndex);
         heuristicTrace(tracer(), "Depth %d: Call at bc index %d is Cold.  Not searching for targets. Signature %s", _recursionDepth, _bcIndex, meth->signature(comp()->trMemory()));
         }
      }
   }

void
InterpreterEmulator::refineResolvedCalleeForInvokevirtual(TR_ResolvedMethod *&callee, bool &isIndirectCall)
   {
   TR_ASSERT_FATAL(_iteratorWithState, "has to be called when the iterator has state!");
   if (!comp()->getOrCreateKnownObjectTable())
      return;

   TR::RecognizedMethod rm = callee->getRecognizedMethod();
   switch (rm)
      {
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
      case TR::java_lang_invoke_MethodHandle_invokeBasic:
         {
         int argNum = callee->numberOfExplicitParameters();
         TR::KnownObjectTable::Index receiverIndex = topn(argNum)->getKnownObjectIndex();
         TR_J9VMBase* fej9 = comp()->fej9();
         auto targetMethod = fej9->targetMethodFromMethodHandle(comp(), receiverIndex);
         if (!targetMethod) return;

         isIndirectCall = false;
         callee = fej9->createResolvedMethod(comp()->trMemory(), targetMethod, callee->owningMethod());
         heuristicTrace(tracer(), "Refine invokeBasic to %s\n", callee->signature(trMemory(), stackAlloc));
         return;
         }
#endif //J9VM_OPT_OPENJDK_METHODHANDLE
      default:
         return;
      }
   }

void
InterpreterEmulator::visitInvokevirtual()
   {
   int32_t cpIndex = next2Bytes();
   auto calleeMethod = (TR_ResolvedJ9Method*)_calltarget->_calleeMethod;
   bool isUnresolvedInCP;
   // Calls in thunk archetype won't be executed by interpreter, so they may appear as unresolved
   bool ignoreRtResolve = _callerIsThunkArchetype;
   _currentCallMethod = calleeMethod->getResolvedPossiblyPrivateVirtualMethod(comp(), cpIndex, ignoreRtResolve, &isUnresolvedInCP);
   _currentCallMethodUnrefined = _currentCallMethod;
   Operand *result = NULL;
   if (isCurrentCallUnresolvedOrCold(_currentCallMethod, isUnresolvedInCP))
      {
      debugUnresolvedOrCold(_currentCallMethod);
      }
   else if (_currentCallMethod)
      {
      bool isIndirectCall = !_currentCallMethod->isFinal() && !_currentCallMethod->isPrivate();
      if (_iteratorWithState)
         refineResolvedCalleeForInvokevirtual(_currentCallMethod, isIndirectCall);

      // Customization logic is not needed in customized thunk or in inlining
      // with known MethodHandle object
      // Since branch folding is disabled and we're ignoring the coldness info
      // in thunk archetype, calls to the following method will be added to the
      // call site list and take up some inlining budget, causing less methods
      // to be inlined. Don't create call site for them
      //
      switch (_currentCallMethod->getRecognizedMethod())
         {
         case TR::java_lang_invoke_MethodHandle_doCustomizationLogic:
         case TR::java_lang_invoke_MethodHandle_undoCustomizationLogic:
            if (_callerIsThunkArchetype)
               return;
         
         default:
            break;
         }

      bool allconsts= false;
      heuristicTrace(tracer(),"numberOfExplicitParameters = %d  _pca.getNumPrevConstArgs = %d\n",_currentCallMethod->numberOfExplicitParameters() ,_pca.getNumPrevConstArgs(_currentCallMethod->numberOfExplicitParameters()));
      if ( _currentCallMethod->numberOfExplicitParameters() > 0 && _currentCallMethod->numberOfExplicitParameters() <= _pca.getNumPrevConstArgs(_currentCallMethod->numberOfExplicitParameters()))
         allconsts = true;

      TR_CallSite *callsite;
      bool isInterface = false;
      TR::Method *interfaceMethod = 0;
      TR::TreeTop *callNodeTreeTop = 0;
      TR::Node *parent = 0;
      TR::Node *callNode = 0;
      TR::ResolvedMethodSymbol *resolvedSymbol = 0;

      Operand *receiver = NULL;
      if (_iteratorWithState)
         receiver = topn(_currentCallMethodUnrefined->numberOfExplicitParameters());

      if (_currentCallMethod->convertToMethod()->isArchetypeSpecimen() && _currentCallMethod->getMethodHandleLocation())
         {
         callsite = new (comp()->trHeapMemory()) TR_J9MethodHandleCallSite(_calltarget->_calleeMethod, callNodeTreeTop,   parent,
                                                                        callNode, interfaceMethod, _currentCallMethod->classOfMethod(),
                                                                        -1, cpIndex, _currentCallMethod,
                                                                        resolvedSymbol, isIndirectCall, isInterface, *_newBCInfo, comp(),
                                                                        _recursionDepth, allconsts);
         }
      else if (_currentCallMethod->getRecognizedMethod() == TR::java_lang_invoke_MethodHandle_invokeExact
               || (_currentCallMethod->getRecognizedMethod() == TR::java_lang_invoke_MethodHandle_invokeBasic
                   && receiver != NULL && receiver->asMutableCallsiteTargetOperand() != NULL))
         {
         TR_J9MutableCallSite *inlinerMcs = new (comp()->trHeapMemory()) TR_J9MutableCallSite(_calltarget->_calleeMethod, callNodeTreeTop,   parent,
                                                      callNode, interfaceMethod, _currentCallMethod->classOfMethod(),
                                                      (int32_t) _currentCallMethod->virtualCallSelector(cpIndex), cpIndex, _currentCallMethod,
                                                      resolvedSymbol, isIndirectCall, isInterface, *_newBCInfo, comp(),
                                                      _recursionDepth, allconsts);
         if (_currentCallMethod->getRecognizedMethod() == TR::java_lang_invoke_MethodHandle_invokeBasic)
            {
            // Set the MCS reference location so that TR_J9MutableCallSite
            // doesn't go rummaging through the trees (with
            // isMutableCallSiteTargetInvokeExact()) looking for
            // mcs.target.invokeExact() or mcs.getTarget().invokeExact(). Those
            // patterns won't be found:
            // - the final call is invokeBasic(), not invokeExact()
            // - rather than a load or getTarget() call, the target will come
            //   from Invokers.getCallSiteTarget() (or, once inlining works for
            //   dynamic invoker handles, another invokeBasic())
            //
            // But there's no need to look through the trees anyway, since the
            // MCS is already known at this point.

            TR::KnownObjectTable *knot = comp()->getOrCreateKnownObjectTable();
            MutableCallsiteTargetOperand *mcsOperand = receiver->asMutableCallsiteTargetOperand();
            TR::KnownObjectTable::Index mcsIndex = mcsOperand->getMutableCallsiteIndex();
            inlinerMcs->setMCSReferenceLocation(knot->getPointerLocation(mcsIndex));
            }

         callsite = inlinerMcs;
         }
      else if (isIndirectCall)
         {
         callsite = new (comp()->trHeapMemory()) TR_J9VirtualCallSite(_calltarget->_calleeMethod, callNodeTreeTop, parent,
                                                                        callNode, interfaceMethod, _currentCallMethod->classOfMethod(),
                                                                        (int32_t) _currentCallMethod->virtualCallSelector(cpIndex), cpIndex, _currentCallMethod,
                                                                        resolvedSymbol, isIndirectCall, isInterface, *_newBCInfo, comp(),
                                                                        _recursionDepth, allconsts);

         }
      else
         {
         callsite = new (comp()->trHeapMemory()) TR_DirectCallSite(_calltarget->_calleeMethod, callNodeTreeTop, parent,
                                                                        callNode, interfaceMethod, _currentCallMethod->classOfMethod(),
                                                                        -1, cpIndex, _currentCallMethod,
                                                                        resolvedSymbol, isIndirectCall, isInterface, *_newBCInfo, comp(),
                                                                        _recursionDepth, allconsts);

         }

      if(tracer()->debugLevel())
         _pca.printIndexes(comp());
      findTargetAndUpdateInfoForCallsite(callsite);
      }

   }

void
InterpreterEmulator::visitInvokespecial()
   {
   int32_t cpIndex = next2Bytes();
   bool isUnresolvedInCP;
   _currentCallMethod = _calltarget->_calleeMethod->getResolvedSpecialMethod(comp(), (current() == J9BCinvokespecialsplit)?cpIndex |= J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG:cpIndex, &isUnresolvedInCP);
   _currentCallMethodUnrefined = _currentCallMethod;
   if (isCurrentCallUnresolvedOrCold(_currentCallMethod, isUnresolvedInCP))
      {
      debugUnresolvedOrCold(_currentCallMethod);
      }
   else
      {
      bool allconsts= false;
      heuristicTrace(tracer(),"numberOfExplicitParameters = %d  _pca.getNumPrevConstArgs = %d\n",_currentCallMethod->numberOfExplicitParameters() ,_pca.getNumPrevConstArgs(_currentCallMethod->numberOfExplicitParameters()));
      if (_currentCallMethod->numberOfExplicitParameters() > 0 && _currentCallMethod->numberOfExplicitParameters() <= _pca.getNumPrevConstArgs(_currentCallMethod->numberOfExplicitParameters()))
         allconsts = true;

      bool isIndirectCall = false;
      bool isInterface = false;
      TR::Method *interfaceMethod = 0;
      TR::TreeTop *callNodeTreeTop = 0;
      TR::Node *parent = 0;
      TR::Node *callNode = 0;
      TR::ResolvedMethodSymbol *resolvedSymbol = 0;
      TR_CallSite *callsite = new (comp()->trHeapMemory()) TR_DirectCallSite(_calltarget->_calleeMethod, callNodeTreeTop, parent,
                                                                        callNode, interfaceMethod, _currentCallMethod->classOfMethod(), -1, cpIndex,
                                                                        _currentCallMethod, resolvedSymbol, isIndirectCall, isInterface, *_newBCInfo, comp(),
                                                                        _recursionDepth, allconsts);
      findTargetAndUpdateInfoForCallsite(callsite);
      }
   }

void
InterpreterEmulator::visitInvokestatic()
   {
   int32_t cpIndex = next2Bytes();
   bool isUnresolvedInCP;
   _currentCallMethod = _calltarget->_calleeMethod->getResolvedStaticMethod(comp(), (current() == J9BCinvokestaticsplit) ? cpIndex |= J9_STATIC_SPLIT_TABLE_INDEX_FLAG:cpIndex, &isUnresolvedInCP);
   _currentCallMethodUnrefined = _currentCallMethod;
   if (isCurrentCallUnresolvedOrCold(_currentCallMethod, isUnresolvedInCP))
      {
      debugUnresolvedOrCold(_currentCallMethod);
      }
   else
      {
      bool allconsts= false;

      heuristicTrace(tracer(),"numberOfExplicitParameters = %d  _pca.getNumPrevConstArgs = %d\n",_currentCallMethod->numberOfExplicitParameters() ,_pca.getNumPrevConstArgs(_currentCallMethod->numberOfExplicitParameters()));
      if (_currentCallMethod->numberOfExplicitParameters() > 0 && _currentCallMethod->numberOfExplicitParameters() <= _pca.getNumPrevConstArgs(_currentCallMethod->numberOfExplicitParameters()))
         allconsts = true;

      TR::KnownObjectTable::Index mhIndex = TR::KnownObjectTable::UNKNOWN;
      TR::KnownObjectTable::Index mcsIndex = TR::KnownObjectTable::UNKNOWN;
      bool isIndirectCall = false;
      if (_iteratorWithState)
         refineResolvedCalleeForInvokestatic(_currentCallMethod, mcsIndex, mhIndex, isIndirectCall);

      bool isInterface = false;
      TR_CallSite *callsite = NULL;
      TR::Method *interfaceMethod = 0;
      TR::TreeTop *callNodeTreeTop = 0;
      TR::Node *parent = 0;
      TR::Node *callNode = 0;
      TR::ResolvedMethodSymbol *resolvedSymbol = 0;

      if (_currentCallMethod->convertToMethod()->isArchetypeSpecimen() &&
            _currentCallMethod->getMethodHandleLocation() &&
            mcsIndex == TR::KnownObjectTable::UNKNOWN)
         {
         callsite = new (comp()->trHeapMemory()) TR_J9MethodHandleCallSite( _calltarget->_calleeMethod, callNodeTreeTop,   parent,
               callNode, interfaceMethod, _currentCallMethod->classOfMethod(),
               -1, cpIndex, _currentCallMethod,
               resolvedSymbol, isIndirectCall, isInterface, *_newBCInfo, comp(),
               _recursionDepth, allconsts);
         }
      else if (_currentCallMethod->convertToMethod()->isArchetypeSpecimen() &&
            _currentCallMethod->getMethodHandleLocation() &&
            mcsIndex != TR::KnownObjectTable::UNKNOWN)
         {
         TR_J9MutableCallSite *mcs = new (comp()->trHeapMemory()) TR_J9MutableCallSite( _calltarget->_calleeMethod, callNodeTreeTop,   parent,
               callNode, interfaceMethod, _currentCallMethod->classOfMethod(),
               (int32_t) _currentCallMethod->virtualCallSelector(cpIndex), cpIndex, _currentCallMethod,
               resolvedSymbol, isIndirectCall, isInterface, *_newBCInfo, comp(),
               _recursionDepth, allconsts);
         if (mcsIndex != TR::KnownObjectTable::UNKNOWN)
            {
            if (comp()->getKnownObjectTable())
               mcs->setMCSReferenceLocation(comp()->getKnownObjectTable()->getPointerLocation(mcsIndex));
            }
         callsite = mcs;
         }
      else if (isIndirectCall)
         {
         int32_t noCPIndex = -1; // The method is not referenced via the constant pool
         callsite = new (comp()->trHeapMemory()) TR_J9VirtualCallSite(
               _calltarget->_calleeMethod, callNodeTreeTop, parent, callNode,
               interfaceMethod, _currentCallMethod->classOfMethod(), (int32_t) _currentCallMethod->virtualCallSelector(cpIndex), noCPIndex,
               _currentCallMethod, resolvedSymbol, isIndirectCall, isInterface,
               *_newBCInfo, comp(), _recursionDepth, allconsts);
         }
      else
         {
         callsite = new (comp()->trHeapMemory()) TR_DirectCallSite(_calltarget->_calleeMethod, callNodeTreeTop, parent, callNode, interfaceMethod,
               _currentCallMethod->classOfMethod(), -1, cpIndex, _currentCallMethod, resolvedSymbol,
               isIndirectCall, isInterface, *_newBCInfo, comp(),
               _recursionDepth, allconsts);
         }
      findTargetAndUpdateInfoForCallsite(callsite);
      }

   }

void
InterpreterEmulator::visitInvokeinterface()
   {
   int32_t cpIndex = next2Bytes();
   auto calleeMethod = (TR_ResolvedJ9Method*)_calltarget->_calleeMethod;
   _currentCallMethod = calleeMethod->getResolvedImproperInterfaceMethod(comp(), cpIndex);
   _currentCallMethodUnrefined = _currentCallMethod;
   bool isIndirectCall = true;
   bool isInterface = true;
   if (_currentCallMethod)
      {
      isInterface = false;
      isIndirectCall = !_currentCallMethod->isPrivate() &&
                       !_currentCallMethod->convertToMethod()->isFinalInObject();
      }

   TR::Method * interfaceMethod = NULL;
   if (isInterface)
      interfaceMethod = comp()->fej9()->createMethod(this->trMemory(), _calltarget->_calleeMethod->containingClass(), cpIndex);

   TR::TreeTop *callNodeTreeTop = 0;
   TR::Node *parent = 0;
   TR::Node *callNode = 0;
   TR::ResolvedMethodSymbol *resolvedSymbol = 0;

   uint32_t explicitParams = 0;
   if (isInterface)
      explicitParams = interfaceMethod->numberOfExplicitParameters();
   else
      explicitParams = _currentCallMethod->numberOfExplicitParameters();

   bool allconsts= false;
   heuristicTrace(tracer(), "numberOfExplicitParameters = %d  _pca.getNumPrevConstArgs = %d\n", explicitParams, _pca.getNumPrevConstArgs(explicitParams));
   if (explicitParams > 0 && explicitParams <= _pca.getNumPrevConstArgs(explicitParams))
      allconsts = true;

   TR_CallSite *callsite = NULL;
   if (isInterface)
      {
      TR_OpaqueClassBlock * thisClass = NULL;
      callsite = new (comp()->trHeapMemory()) TR_J9InterfaceCallSite(
         _calltarget->_calleeMethod, callNodeTreeTop, parent, callNode,
         interfaceMethod, thisClass, -1, cpIndex, _currentCallMethod,
         resolvedSymbol, isIndirectCall, isInterface, *_newBCInfo,
         comp(), _recursionDepth, allconsts);
      }
   else if (isIndirectCall)
      {
      callsite = new (comp()->trHeapMemory()) TR_J9VirtualCallSite(
         _calltarget->_calleeMethod, callNodeTreeTop, parent, callNode,
         interfaceMethod, _currentCallMethod->classOfMethod(), (int32_t) _currentCallMethod->virtualCallSelector(cpIndex), cpIndex,
         _currentCallMethod, resolvedSymbol, isIndirectCall, isInterface,
         *_newBCInfo, comp(), _recursionDepth, allconsts);
      }
   else
      {
      callsite = new (comp()->trHeapMemory()) TR_DirectCallSite(
         _calltarget->_calleeMethod, callNodeTreeTop, parent, callNode,
         interfaceMethod, _currentCallMethod->classOfMethod(), -1, cpIndex,
         _currentCallMethod, resolvedSymbol, isIndirectCall, isInterface,
         *_newBCInfo, comp(), _recursionDepth, allconsts);
      }

   if(tracer()->debugLevel())
      {
      _pca.printIndexes(comp());
      }
   findTargetAndUpdateInfoForCallsite(callsite);
   }

Operand*
InterpreterEmulator::createOperandFromPrexArg(TR_PrexArgument* prexArgument)
   {
   auto prexKnowledge = TR_PrexArgument::knowledgeLevel(prexArgument);
   switch (prexKnowledge)
      {
      case KNOWN_OBJECT:
         return new (trStackMemory()) KnownObjOperand(prexArgument->getKnownObjectIndex(), prexArgument->getClass());
      case FIXED_CLASS:
         return new (trStackMemory()) FixedClassOperand(prexArgument->getClass());
      case PREEXISTENT:
         return new (trStackMemory()) PreexistentObjectOperand(prexArgument->getClass());
      case NONE:
         return prexArgument->getClass() ? new (trStackMemory()) ObjectOperand(prexArgument->getClass()) : NULL;
      }
   return NULL;
   }

TR_PrexArgument*
InterpreterEmulator::createPrexArgFromOperand(Operand* operand)
   {
   if (operand->asKnownObject())
      {
      auto koi = operand->getKnownObjectIndex();
      auto knot = comp()->getOrCreateKnownObjectTable();
      if (knot && !knot->isNull(koi))
         return new (comp()->trHeapMemory()) TR_PrexArgument(operand->getKnownObjectIndex(), comp());
      }
   else if (operand->asObjectOperand() && operand->asObjectOperand()->getClass())
      {
      TR_OpaqueClassBlock* clazz = operand->asObjectOperand()->getClass();
      TR_PrexArgument::ClassKind kind = TR_PrexArgument::ClassIsUnknown;
      if (operand->asFixedClassOperand())
         kind = TR_PrexArgument::ClassIsFixed;
      else if (operand->asPreexistentObjectOperand())
         kind = TR_PrexArgument::ClassIsPreexistent;

      return new (comp()->trHeapMemory()) TR_PrexArgument(kind, clazz);
      }

   return NULL;
   }

TR_PrexArgInfo*
InterpreterEmulator::computePrexInfo(
   TR_CallSite *callsite, TR::KnownObjectTable::Index appendix)
   {
   if (tracer()->heuristicLevel())
      _ecs->getInliner()->tracer()->dumpCallSite(callsite, "Compute prex info for call site %p\n", callsite);

   int32_t numOfArgs = 0;
   if (callsite->_isInterface)
      {
      numOfArgs = callsite->_interfaceMethod->numberOfExplicitParameters() + 1;
      }
   else if (callsite->_initialCalleeMethod)
      {
      numOfArgs = callsite->_initialCalleeMethod->numberOfParameters();
      }

   if (numOfArgs == 0)
      return NULL;

   // Always favor prex arg from operand if we're iterating with state
   // But not for thunk archetype as the method's bytecodes manipulate
   // the operand stack differently, and one int `argPlacehowler`
   // argument can represent more than one arguments
   //
   if (!_callerIsThunkArchetype && _iteratorWithState)
      {
      TR_PrexArgInfo* prexArgInfo = new (comp()->trHeapMemory()) TR_PrexArgInfo(numOfArgs, comp()->trMemory());
      for (int32_t i = 0; i < numOfArgs; i++)
         {
         int32_t posInStack = numOfArgs - i - 1;
         prexArgInfo->set(i, createPrexArgFromOperand(topn(posInStack)));
         }

      if (tracer()->heuristicLevel())
         {
         alwaysTrace(tracer(), "argInfo from operand stack:");
         prexArgInfo->dumpTrace();
         }
      return prexArgInfo;
      }
   else if (_wasPeekingSuccessfull)
      {
      auto callNodeTT = TR_PrexArgInfo::getCallTree(_methodSymbol, callsite, tracer());
      if (callNodeTT)
         {
         // Temporarily set call tree and call node of callsite such that computePrexInfo can use it
         callsite->_callNodeTreeTop = callNodeTT;
         callsite->_callNode = callNodeTT->getNode()->getChild(0);
         auto prexArgInfo = TR_J9InlinerUtil::computePrexInfo(_ecs->getInliner(), callsite, _calltarget->_ecsPrexArgInfo);

         // Reset call tree and call node
         callsite->_callNodeTreeTop = NULL;
         callsite->_callNode = NULL;
         return prexArgInfo;
         }
      }
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
   else if (appendix != TR::KnownObjectTable::UNKNOWN)
      {
      TR_ASSERT_FATAL(!callsite->isIndirectCall(), "appendix with indirect call");
      TR_ASSERT_FATAL(
         comp()->fej9()->isLambdaFormGeneratedMethod(callsite->_initialCalleeMethod),
         "appendix with non-LambdaForm method - expected a call site adapter");

      TR::KnownObjectTable *knot = comp()->getKnownObjectTable();
      if (!knot->isNull(appendix))
         {
         TR_PrexArgInfo* prexArgInfo =
            new (comp()->trHeapMemory()) TR_PrexArgInfo(numOfArgs, comp()->trMemory());

         auto arg = new (comp()->trHeapMemory()) TR_PrexArgument(appendix, comp());
         prexArgInfo->set(numOfArgs - 1, arg);

         if (tracer()->heuristicLevel())
            {
            alwaysTrace(tracer(), "argInfo from appendix object:");
            prexArgInfo->dumpTrace();
            }

         return prexArgInfo;
         }
      }
#endif

   return NULL;
   }

void
InterpreterEmulator::findTargetAndUpdateInfoForCallsite(
   TR_CallSite *callsite, TR::KnownObjectTable::Index appendix)
   {
   _currentCallSite = callsite;
   callsite->_callerBlock = _currentInlinedBlock;
   callsite->_ecsPrexArgInfo = computePrexInfo(callsite, appendix);

   if (_ecs->isInlineable(_callStack, callsite))
      {
      _callSites[_bcIndex] = callsite;
      _inlineableCallExists = true;

      if (!_currentInlinedBlock->isCold())
            _nonColdCallExists = true;

      for (int i = 0; i < callsite->numTargets(); i++)
         callsite->getTarget(i)->_originatingBlock = _currentInlinedBlock;
      }
   else
      {
      //support counters
      _calltarget->addDeadCallee(callsite);
      }
   }
