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
#include "optimizer/J9DeferredOSRAssumptions.hpp"
#include "optimizer/J9EstimateCodeSize.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "env/JSR292Methods.h"
#include "env/J9ConstProvenanceGraph.hpp"
#include "optimizer/PreExistence.hpp"
#include "optimizer/J9CallGraph.hpp"
#include "ilgen/IlGenRequest.hpp"
#include "jilconsts.h"
#include "il/ParameterSymbol.hpp"
#include "infra/ILWalk.hpp"
#include "optimizer/PreExistence.hpp"
#include "optimizer/TransformUtil.hpp"
#include "il/Node_inlines.hpp"
#if defined(J9VM_OPT_JITSERVER)
#include "control/CompilationRuntime.hpp"
#include "env/j9methodServer.hpp"
#endif /* defined(J9VM_OPT_JITSERVER) */
#include "ras/Logger.hpp"

const char *Operand::KnowledgeStrings[]
    = { "NONE", "OBJECT", "MUTABLE_CALLSITE_TARGET", "PREEXISTENT", "FIXED_CLASS", "KNOWN_OBJECT", "ICONST" };

char *ObjectOperand::getSignature(TR::Compilation *comp, TR_Memory *trMemory)
{
    if (!_signature && _clazz)
        _signature = TR::Compiler->cls.classSignature(comp, _clazz, trMemory);
    return _signature;
}

KnownObjOperand::KnownObjOperand(TR::KnownObjectTable *knot, TR::KnownObjectTable::Index koi,
    TR_OpaqueClassBlock *clazz)
    : knownObjIndex(koi)
    , FixedClassOperand(clazz)
{
    TR_ASSERT_FATAL(knownObjIndex != TR::KnownObjectTable::UNKNOWN, "Unexpected unknown object");
    TR_ASSERT_FATAL(!knot->isNull(knownObjIndex), "Unexpected null index");
    TR_ASSERT_FATAL(clazz != NULL, "missing type of known object");
}

Operand *Operand::merge(Operand *other)
{
    if (getKnowledgeLevel() > other->getKnowledgeLevel())
        return other->merge1(this);
    else
        return merge1(other);
}

Operand *Operand::merge1(Operand *other)
{
    if (this == other)
        return this;
    else
        return NULL;
}

Operand *NullOperand::merge1(Operand *other)
{
    TR_ASSERT(other->getKnowledgeLevel() >= this->getKnowledgeLevel(), "Should be calling other->merge1(this)");
    TR_ASSERT_FATAL(other->isNull(), "knowledge level should restrict other to null");
    return this;
}

Operand *IconstOperand::merge1(Operand *other)
{
    TR_ASSERT(other->getKnowledgeLevel() >= this->getKnowledgeLevel(), "Should be calling other->merge1(this)");
    IconstOperand *otherIconst = other->asIconst();
    if (otherIconst && this->intValue == otherIconst->intValue)
        return this;
    else
        return NULL;
}

// TODO: check instanceOf relationship and create new Operand if neccessary
Operand *ObjectOperand::merge1(Operand *other)
{
    TR_ASSERT(other->getKnowledgeLevel() >= this->getKnowledgeLevel(), "Should be calling other->merge1(this)");
    ObjectOperand *otherObject = other->asObjectOperand();
    if (otherObject && this->_clazz == otherObject->_clazz)
        return this;
    else if (other->isNull())
        return this;
    else
        return NULL;
}

// Both are preexistent objects
Operand *PreexistentObjectOperand::merge1(Operand *other)
{
    TR_ASSERT(other->getKnowledgeLevel() >= this->getKnowledgeLevel(), "Should be calling other->merge1(this)");
    PreexistentObjectOperand *otherPreexistentObjectOperand = other->asPreexistentObjectOperand();
    if (otherPreexistentObjectOperand && this->_clazz == otherPreexistentObjectOperand->_clazz)
        return this;
    else if (other->isNull())
        return this;
    else
        return NULL;
}

Operand *FixedClassOperand::merge1(Operand *other)
{
    TR_ASSERT(other->getKnowledgeLevel() >= this->getKnowledgeLevel(), "Should be calling other->merge1(this)");
    FixedClassOperand *otherFixedClass = other->asFixedClassOperand();
    if (otherFixedClass && this->_clazz == otherFixedClass->_clazz)
        return this;
    else if (other->isNull())
        return this;
    else
        return NULL;
}

Operand *KnownObjOperand::merge1(Operand *other)
{
    // TODO: allow null like in VP? add a non-null boolean?
    TR_ASSERT(other->getKnowledgeLevel() >= this->getKnowledgeLevel(), "Should be calling other->merge1(this)");
    KnownObjOperand *otherKnownObj = other->asKnownObject();
    if (otherKnownObj && this->knownObjIndex == otherKnownObj->knownObjIndex)
        return this;
    else
        return NULL;
}

Operand *MutableCallsiteTargetOperand::merge1(Operand *other)
{
    TR_ASSERT(other->getKnowledgeLevel() >= this->getKnowledgeLevel(), "Should be calling other->merge1(this)");
    MutableCallsiteTargetOperand *otherMutableCallsiteTarget = other->asMutableCallsiteTargetOperand();
    if (otherMutableCallsiteTarget && this->mutableCallsiteIndex == otherMutableCallsiteTarget->mutableCallsiteIndex
        && this->methodHandleIndex == otherMutableCallsiteTarget->methodHandleIndex)
        return this;
    else
        return NULL;
}

void Operand::printToString(TR::StringBuf *buf) { buf->appendf("(unknown)"); }

void NullOperand::printToString(TR::StringBuf *buf) { buf->appendf("(null)"); }

void IconstOperand::printToString(TR::StringBuf *buf) { buf->appendf("(iconst=%d)", intValue); }

void ObjectOperand::printToString(TR::StringBuf *buf)
{
    buf->appendf("(%s=clazz%p)", KnowledgeStrings[getKnowledgeLevel()], getClass());
}

void KnownObjOperand::printToString(TR::StringBuf *buf) { buf->appendf("(obj%d)", getKnownObjectIndex()); }

void MutableCallsiteTargetOperand::printToString(TR::StringBuf *buf)
{
    buf->appendf("(mh=%d, mcs=%d)", getMethodHandleIndex(), getMutableCallsiteIndex());
}

Operand *InterpreterEmulator::knownObjOperand(TR::KnownObjectTable::Index i, TR_OpaqueClassBlock *clazz)
{
    if (i == TR::KnownObjectTable::UNKNOWN)
        return _unknownOperand;

    TR::KnownObjectTable *knot = comp()->getKnownObjectTable();
    if (knot->isNull(i))
        return _nullOperand;

    if (clazz == NULL) {
        clazz = comp()->fej9()->getObjectClassFromKnownObjectIndex(comp(), i);

        // TODO: assuming that #22364 has been merged, clazz can't be null.
        if (clazz == NULL)
            return _unknownOperand;
    }

    return new (trStackMemory()) KnownObjOperand(knot, i, clazz);
}

void InterpreterEmulator::printOperandArray(OperandArray *operands)
{
    int32_t size = operands->size();
    for (int32_t i = 0; i < size; i++) {
        _operandBuf->clear();
        (*operands)[i]->printToString(_operandBuf);
        comp()->log()->printf("[%d]=%s, ", i, _operandBuf->text());
    }
    if (size > 0)
        comp()->log()->println();
}

// Merge second OperandArray into the first one
// The merge does a union
//
void InterpreterEmulator::mergeOperandArray(OperandArray *first, OperandArray *second)
{
    OMR::Logger *log = comp()->log();

    uint32_t size = first->size();
    TR_ASSERT_FATAL(second->size() == size, "attempt to merge operand arrays of different sizes");

    bool enableTrace = tracer()->debugLevel();
    if (enableTrace) {
        log->prints("Operands before merging:\n");
        printOperandArray(first);
    }

    bool changed = false;

    for (uint32_t i = 0; i < size; i++) {
        Operand *firstVal = (*first)[i];
        Operand *secondVal = (*second)[i];
        Operand *merged = firstVal->merge(secondVal);
        if (merged == NULL)
            merged = _unknownOperand;

        if (merged != firstVal) {
            changed = true;
            (*first)[i] = merged;
        }
    }

    if (enableTrace) {
        if (changed) {
            log->prints("Operands after merging:\n");
            printOperandArray(first);
        } else
            log->prints("Operands is not changed after merging\n");
    }
}

/**
 * \brief Add a required constant with \p value at the current bytecode index.
 *
 * There must not already be one.
 *
 * \param value the constant value
 * \return a reference to the created TR::RequiredConst
 */
TR::RequiredConst &InterpreterEmulator::addRequiredConst(TR::AnyConst value)
{
    TR::Region &stackRegion = comp()->trMemory()->currentStackRegion();
    auto insertResult
        = _calltarget->_requiredConsts.insert(std::make_pair(_bcIndex, TR::RequiredConst(value, stackRegion)));

    auto it = insertResult.first;
    bool isNewEntry = insertResult.second;
    TR_ASSERT_FATAL(isNewEntry, "multiple required consts at bcIndex %d", _bcIndex);
    return it->second; // value
}

void InterpreterEmulator::maintainStackForIf(TR_J9ByteCode bc)
{
    assertHasState();

    int32_t branchBC = _bcIndex + next2BytesSigned();

    // The conditional is expected to pop and somehow compare two values from
    // the stack. For unary comparisons (e.g. ifeq, ifnull), the caller should
    // push the implied operand first. The values are irrelevant for now.
    Operand *rhs = pop();
    Operand *lhs = pop();

    if (tracer()->debugLevel()) {
        _operandBuf->clear();
        lhs->printToString(_operandBuf);
        debugTrace(tracer(), "compare lhs %s", _operandBuf->text());
        _operandBuf->clear();
        rhs->printToString(_operandBuf);
        debugTrace(tracer(), "     vs rhs %s\n", _operandBuf->text());
    }

    TR_YesNoMaybe isTaken = TR_maybe;
    switch (bc) {
        case J9BCifacmpeq:
        case J9BCifacmpne: {
            bool lhsConstant = lhs->isNull() || lhs->asKnownObject() != NULL;
            bool rhsConstant = rhs->isNull() || rhs->asKnownObject() != NULL;
            if (lhsConstant && rhsConstant) {
                bool equal = lhs->isNull() == rhs->isNull() && lhs->getKnownObjectIndex() == rhs->getKnownObjectIndex();

                isTaken = (bc == J9BCifacmpeq) == equal ? TR_yes : TR_no;
            }

            break;
        }
    }

    if (isTaken == TR_no) {
        markEdgeUnreachable(branchBC);
    } else {
        debugTrace(tracer(), "conditional can jump to +%d\n", branchBC);
        saveStack(branchBC);
        _outEdgesStillReachable.insert(_currentInlinedBlock->getEdge(blocks(branchBC)));
    }

    if (isTaken == TR_yes) {
        _currentBcCanFallThrough = false;
    } else {
        debugTrace(tracer(), "conditional can fall through\n");
        // fallthrough stack will be saved in findAndCreateCallsitesFromBytecodes()
        // in the same way as for blocks that don't end with control flow
    }
}

void InterpreterEmulator::maintainStackForTableSwitch()
{
    assertHasState();

    _currentBcCanFallThrough = false;

    pop();

    int32_t bcIndex = _bcIndex + 1; // skip opcode
    while ((bcIndex & 3) != 0) // skip padding to align to a 4 byte boundary
        bcIndex++;

    int32_t defTarget = _bcIndex + nextSwitchValue(bcIndex);
    int32_t lo = nextSwitchValue(bcIndex);
    int32_t hi = nextSwitchValue(bcIndex);

    TR_ASSERT_FATAL(lo <= hi, "tableswitch bounds out of order: [%d, %d]", lo, hi);

    for (int32_t i = lo; i <= hi; i++) {
        int32_t target = _bcIndex + nextSwitchValue(bcIndex);
        debugTrace(tracer(), "case %d -> +%d", i, target);
        _outEdgesStillReachable.insert(_currentInlinedBlock->getEdge(blocks(target)));
        saveStack(target);
    }

    debugTrace(tracer(), "default -> +%d", defTarget);
    _outEdgesStillReachable.insert(_currentInlinedBlock->getEdge(blocks(defTarget)));
    saveStack(defTarget);
}

void InterpreterEmulator::maintainStackForGetField()
{
    assertHasState();
    TR::DataType type = TR::NoType;
    uint32_t fieldOffset;
    int32_t cpIndex = next2Bytes();
    Operand *newOperand = _unknownOperand;
    TR::Symbol *fieldSymbol = TR::Symbol::createPossiblyRecognizedShadowFromCP(comp(), trStackMemory(),
        _calltarget->_calleeMethod, cpIndex, &type, &fieldOffset, false);

    TR::KnownObjectTable *knot = comp()->getKnownObjectTable();
    if (knot && top()->asKnownObject() && type == TR::Address) {
        if (fieldSymbol == NULL) {
            debugTrace(tracer(), "field is unresolved");
        } else if (!comp()->fej9()->canDereferenceAtCompileTimeWithFieldSymbol(fieldSymbol, cpIndex,
                       _calltarget->_calleeMethod)) {
            debugTrace(tracer(), "field is not foldable");
        } else {
            TR::KnownObjectTable::Index baseObjectIndex = top()->getKnownObjectIndex();
            TR::KnownObjectTable::Index resultIndex = TR::KnownObjectTable::UNKNOWN;
            uintptr_t baseObjectAddress = 0;
            uintptr_t fieldAddress = 0;
            bool avoidFolding = true;

#if defined(J9VM_OPT_JITSERVER)
            if (comp()->isOutOfProcessCompilation()) {
                TR_ResolvedJ9JITServerMethod *serverMethod
                    = static_cast<TR_ResolvedJ9JITServerMethod *>(_calltarget->_calleeMethod);
                TR_ResolvedMethod *clientMethod = serverMethod->getRemoteMirror();

                auto stream = comp()->getStream();
                stream->write(JITServer::MessageType::KnownObjectTable_dereferenceKnownObjectField, baseObjectIndex,
                    clientMethod, cpIndex);

                auto recv = stream->read<TR::KnownObjectTable::Index, uintptr_t *, uintptr_t, uintptr_t, bool>();
                resultIndex = std::get<0>(recv);
                uintptr_t *objectPointerReference = std::get<1>(recv);
                fieldAddress = std::get<2>(recv);
                baseObjectAddress = std::get<3>(recv);
                avoidFolding = std::get<4>(recv);

                if (resultIndex != TR::KnownObjectTable::UNKNOWN)
                    knot->updateKnownObjectTableAtServer(resultIndex, objectPointerReference);
            } else
#endif /* defined(J9VM_OPT_JITSERVER) */
            {
                TR::VMAccessCriticalSection dereferenceKnownObjectField(comp()->fej9());
                baseObjectAddress = knot->getPointer(baseObjectIndex);
                TR_OpaqueClassBlock *baseObjectClass = comp()->fej9()->getObjectClass(baseObjectAddress);
                TR_OpaqueClassBlock *fieldDeclaringClass
                    = _calltarget->_calleeMethod->getDeclaringClassFromFieldOrStatic(comp(), cpIndex);

                avoidFolding = TR::TransformUtil::avoidFoldingInstanceField(baseObjectAddress, fieldSymbol, fieldOffset,
                    cpIndex, _calltarget->_calleeMethod, comp());

                if (fieldDeclaringClass
                    && comp()->fej9()->isInstanceOf(baseObjectClass, fieldDeclaringClass, true) == TR_yes) {
                    fieldAddress = comp()->fej9()->getReferenceFieldAtAddress(baseObjectAddress + fieldOffset);
                    resultIndex = knot->getOrCreateIndex(fieldAddress);
                }
            }

            bool fail = resultIndex == TR::KnownObjectTable::UNKNOWN;
            if (fail || avoidFolding) {
                int32_t len = 0;
                debugTrace(tracer(), "%s field in obj%d: %s",
                    fail ? "failed to determine value of" : "avoid folding sometimes-foldable", baseObjectIndex,
                    _calltarget->_calleeMethod->fieldName(cpIndex, len, this->trMemory()));
            } else {
                // It's OK to print fieldAddress and baseObjectAddress here even
                // without VM access. There's no meaningful difference between:
                // - printing the object's address, then allowing it to move; and
                // - observing the objects's address, then allowing it to move,
                //   then finally printing the observed address.
                newOperand = knownObjOperand(resultIndex);
                if (tracer()->debugLevel()) {
                    _operandBuf->clear();
                    newOperand->printToString(_operandBuf);
                    int32_t len = 0;
                    debugTrace(tracer(), "dereference obj%d (%p), field +0x%x %s -> value %p: %s\n", baseObjectIndex,
                        baseObjectAddress, fieldOffset,
                        _calltarget->_calleeMethod->fieldName(cpIndex, len, this->trMemory()), (void *)fieldAddress,
                        _operandBuf->text());
                }

                if (resultIndex != TR::KnownObjectTable::UNKNOWN) {
                    auto value = TR::AnyConst::makeKnownObject(resultIndex);
                    addRequiredConst(value);

                    if (!knot->isNull(resultIndex)) {
                        J9::ConstProvenanceGraph *cpg = comp()->constProvenanceGraph();
                        cpg->addEdge(cpg->knownObject(baseObjectIndex), cpg->knownObject(resultIndex));
                    }
                }
            }
        }
    }
    pop();
    push(newOperand);
}

void InterpreterEmulator::maintainStackForArraylength()
{
    assertHasState();
    Operand *obj = pop();
    TR::KnownObjectTable::Index koi = obj->getKnownObjectIndex();
    if (koi == TR::KnownObjectTable::UNKNOWN
        || !TR::Compiler->cls.isClassArray(comp(), obj->asObjectOperand()->getClass())) {
        pushUnknownOperand();
    } else {
        TR::KnownObjectTable *knot = comp()->getKnownObjectTable();
        TR::VMAccessCriticalSection arrayLength(comp()->fej9());
        uintptr_t objAddr = knot->getPointer(koi);
        auto len = TR::Compiler->om.getArrayLengthInElements(comp(), objAddr);
        push(new (trStackMemory()) IconstOperand((int32_t)len));
    }
}

void InterpreterEmulator::maintainStackForArrayLoad(TR::DataTypes type, TR_J9ByteCode bc)
{
    assertHasState();
    IconstOperand *index = pop()->asIconst();
    Operand *obj = pop();
    Operand *result = foldArrayLoad(obj, index, type, bc);
    push(result == NULL ? _unknownOperand : result);
}

Operand *InterpreterEmulator::foldArrayLoad(Operand *obj, IconstOperand *indexOperand, TR::DataTypes type,
    TR_J9ByteCode bc)
{
#if defined(J9VM_OPT_JITSERVER)
    if (comp()->isOutOfProcessCompilation())
        return NULL;
#endif

    if (type == TR::Int64 || type == TR::Float || type == TR::Double)
        return NULL; // no Operand to represent the result

    TR::KnownObjectTable::Index koi = obj->getKnownObjectIndex();
    if (koi == TR::KnownObjectTable::UNKNOWN || indexOperand == NULL)
        return NULL;

    int32_t index = indexOperand->intValue;
    if (index < 0)
        return NULL;

    TR::KnownObjectTable *knot = comp()->getKnownObjectTable();
    bool constElems = knot->isArrayWithConstantElements(koi);
    int32_t stableArrayRank = knot->getArrayWithStableElementsRank(koi);
    if (!constElems && stableArrayRank == 0)
        return NULL;

    TR_OpaqueClassBlock *arrayClass = obj->asObjectOperand()->getClass();
    bool arrayClassOk = type == TR::Address ? TR::Compiler->cls.isReferenceArray(comp(), arrayClass)
                                            : type == TR::Compiler->cls.primitiveArrayComponentType(comp(), arrayClass);

    if (!arrayClassOk)
        return NULL;

    TR::VMAccessCriticalSection foldArrayLoadCriticalSection(comp()->fej9());
    uintptr_t objAddr = knot->getPointer(koi);
    auto len = (int32_t)TR::Compiler->om.getArrayLengthInElements(comp(), objAddr);
    if (index >= len)
        return NULL;

    int64_t offset = TR::Compiler->om.contiguousArrayHeaderSizeInBytes()
        + index * TR::Compiler->om.getArrayElementWidthInBytes(type);

    // TODO: on success, create a RequiredConst and add a provenance edge
    uintptr_t elemAddr = TR::Compiler->om.getAddressOfElement(comp(), objAddr, offset);
    switch (type) {
        case TR::Address: {
            uintptr_t value = comp()->fej9()->getReferenceFieldAtAddress(elemAddr);
            if (!constElems && value == 0)
                return NULL;

            TR::KnownObjectTable::Index elemKoi = knot->getOrCreateIndex(value);
            if (stableArrayRank >= 2)
                knot->addStableArray(elemKoi, stableArrayRank - 1);

            return knownObjOperand(elemKoi);
        }

        case TR::Int32: {
            int32_t value = *(int32_t *)elemAddr;
            if (!constElems && value == 0)
                return NULL;

            return new (trStackMemory()) IconstOperand(value);
        }

        case TR::Int16: {
            int16_t value = *(int16_t *)elemAddr;
            if (!constElems && value == 0)
                return NULL;

            int32_t value32 = bc == J9BCsaload ? (int32_t)value : (int32_t)(uint16_t)value;

            return new (trStackMemory()) IconstOperand(value32);
        }

        case TR::Int8: {
            int8_t value = *(int8_t *)elemAddr;
            if (!constElems && value == 0)
                return NULL;

            return new (trStackMemory()) IconstOperand((int32_t)value);
        }

        default:
            TR_ASSERT_FATAL(false, "unhandled type %d", type);
            return NULL;
    }
}

void InterpreterEmulator::saveStack(int32_t targetIndex)
{
    if (!_iteratorWithState)
        return;

    TR_ASSERT_FATAL(_currentInlinedBlock->hasSuccessor(blocks(targetIndex)),
        "saving stack for edge %d -> %d which is not in CFG", _currentInlinedBlock->getNumber(),
        blocks(_bcIndex)->getNumber());

    // Propagate stack state to successor
    if (!_stack->isEmpty()) {
        if (!_stacks[targetIndex])
            _stacks[targetIndex] = new (trStackMemory()) ByteCodeStack(*_stack);
        else
            mergeOperandArray(_stacks[targetIndex], _stack);
    }

    // Propagate local object info to successor
    if (_numSlots) {
        if (!_localObjectInfos[targetIndex])
            _localObjectInfos[targetIndex] = new (trStackMemory()) OperandArray(*_currentLocalObjectInfo);
        else
            mergeOperandArray(_localObjectInfos[targetIndex], _currentLocalObjectInfo);
    }
}

void InterpreterEmulator::initializeIteratorWithState()
{
    _iteratorWithState = true;
    _unknownOperand = new (trStackMemory()) Operand();
    _nullOperand = new (trStackMemory()) NullOperand();
    uint32_t size = this->maxByteCodeIndex() + 5;
    _flags = (flags8_t *)this->trMemory()->allocateStackMemory(size * sizeof(flags8_t));
    _stacks = (ByteCodeStack **)this->trMemory()->allocateStackMemory(size * sizeof(ByteCodeStack *));
    memset(_flags, 0, size * sizeof(flags8_t));
    memset(_stacks, 0, size * sizeof(ByteCodeStack *));
    _stack = new (trStackMemory()) TR_Stack<Operand *>(this->trMemory(), 20, false, stackAlloc);
    _localObjectInfos = (OperandArray **)this->trMemory()->allocateStackMemory(size * sizeof(OperandArray *));
    memset(_localObjectInfos, 0, size * sizeof(OperandArray *));

    int32_t numParmSlots = method()->numberOfParameterSlots();
    _numSlots = numParmSlots + method()->numberOfTemps();

    TR::AllBlockIterator it(_cfg, comp());
    for (; it.currentBlock() != NULL; it.stepForward()) {
        TR::Block *block = it.currentBlock();
        uint32_t rc = 0;
        TR_PredecessorIterator preds(block);
        for (auto *pred = preds.getFirst(); pred != NULL; pred = preds.getNext())
            rc++;

        _blockRc[block] = rc;
    }
}

void InterpreterEmulator::setupMethodEntryLocalObjectState()
{
    TR_PrexArgInfo *argInfo = _calltarget->_ecsPrexArgInfo;
    if (argInfo) {
        TR_ASSERT_FATAL(argInfo->getNumArgs() == method()->numberOfParameters(),
            "Prex arg number should match parm number");

        if (tracer()->heuristicLevel()) {
            heuristicTrace(tracer(), "Save argInfo to slot state array");
            argInfo->dumpTrace();
        }

        method()->makeParameterList(_methodSymbol);
        ListIterator<TR::ParameterSymbol> parms(&_methodSymbol->getParameterList());

        // save prex arg into local var arrays
        for (TR::ParameterSymbol *p = parms.getFirst(); p != NULL; p = parms.getNext()) {
            int32_t ordinal = p->getOrdinal();
            int32_t slotIndex = p->getSlot();
            TR_PrexArgument *prexArgument = argInfo->get(ordinal);
            if (!prexArgument) {
                (*_currentLocalObjectInfo)[slotIndex] = _unknownOperand;
            } else {
                auto operand = createOperandFromPrexArg(prexArgument);
                if (operand) {
                    (*_currentLocalObjectInfo)[slotIndex] = operand;
                } else
                    (*_currentLocalObjectInfo)[slotIndex] = _unknownOperand;
            }
            if (tracer()->heuristicLevel()) {
                _operandBuf->clear();
                (*_currentLocalObjectInfo)[slotIndex]->printToString(_operandBuf);
                heuristicTrace(tracer(), "Creating operand %s for parm %d slot %d from PrexArgument %p",
                    _operandBuf->text(), ordinal, slotIndex, prexArgument);
            }
        }
    }
}

bool InterpreterEmulator::hasVisitedPred(TR::Block *block)
{
    TR_PredecessorIterator pi(block);
    for (TR::CFGEdge *edge = pi.getFirst(); edge != NULL; edge = pi.getNext()) {
        if (!isEdgeUnreachable(edge) && _visitedBlocks.contains(toBlock(edge->getFrom())))
            return true;
    }

    return false;
}

bool InterpreterEmulator::hasUnvisitedPred(TR::Block *block)
{
    TR_PredecessorIterator pi(block);
    for (TR::CFGEdge *edge = pi.getFirst(); edge != NULL; edge = pi.getNext()) {
        if (!isEdgeUnreachable(edge) && !_visitedBlocks.contains(toBlock(edge->getFrom())))
            return true;
    }

    return false;
}

void InterpreterEmulator::setupBBStartStackState(int32_t index)
{
    if (index == 0)
        return;

    auto block = blocks(index);
    auto stack = _stacks[index];
    if (stack && hasUnvisitedPred(block)) {
        heuristicTrace(tracer(),
            "block_%d at bc index %d has unvisited predecessor, setting stack operand info to unknown",
            block->getNumber(), index);
        for (int32_t i = 0; i < stack->size(); ++i)
            (*stack)[i] = _unknownOperand;
    }
}

void InterpreterEmulator::setupBBStartLocalObjectState(int32_t index)
{
    if (_numSlots == 0)
        return;

    bool localsAreUnknown = _localObjectInfos[index] == NULL;
    if (localsAreUnknown) {
        _localObjectInfos[index] = new (trStackMemory()) OperandArray(trMemory(), _numSlots, false, stackAlloc);
    } else if (hasUnvisitedPred(_currentInlinedBlock)) {
        localsAreUnknown = true;
        heuristicTrace(tracer(), "block_%d at bc index %d has unvisited predecessor; conservative locals",
            _currentInlinedBlock->getNumber(), index);
    } else if (!_currentInlinedBlock->getExceptionPredecessors().empty()) {
        localsAreUnknown = true;
        heuristicTrace(tracer(), "block_%d at bc index %d is a catch block; conservative locals",
            _currentInlinedBlock->getNumber(), index);
    }

    if (localsAreUnknown) {
        for (int32_t i = 0; i < _numSlots; i++)
            (*_localObjectInfos[index])[i] = _unknownOperand;
    }

    _currentLocalObjectInfo = _localObjectInfos[index];

    if (index == 0 && _currentInlinedBlock->getPredecessors().size() == 1
        && _currentInlinedBlock->getPredecessors().front()->getFrom() == _cfg->getStart()) {
        setupMethodEntryLocalObjectState();
    }
}

int32_t InterpreterEmulator::setupBBStartContext(int32_t index)
{
    if (_iteratorWithState) {
        setupBBStartStackState(index);
        setupBBStartLocalObjectState(index);
    }

    Base::setupBBStartContext(index);

    if (_iteratorWithState && !_currentInlinedBlock->getExceptionPredecessors().empty()) {
        heuristicTrace(tracer(), "block_%d at bc index %d is a catch block; push unknown exception object",
            _currentInlinedBlock->getNumber(), index);
        pushUnknownOperand(); // exception object
    }

    return index;
}

bool InterpreterEmulator::maintainStack(TR_J9ByteCode bc)
{
    assertHasState();
    int slotIndex = -1;
    switch (bc) {
        case J9BCnop:
        case J9BCinvokeinterface2: // ilgen treats this the same way as nop
            break;

        case J9BCgetfield:
            maintainStackForGetField();
            break;
        case J9BCarraylength:
            maintainStackForArraylength();
            break;
        case J9BCaaload:
            maintainStackForArrayLoad(TR::Address, bc);
            break;
        case J9BClaload:
            maintainStackForArrayLoad(TR::Int64, bc);
            break;
        case J9BCiaload:
            maintainStackForArrayLoad(TR::Int32, bc);
            break;
        case J9BCsaload: // fall through
        case J9BCcaload:
            maintainStackForArrayLoad(TR::Int16, bc);
            break;
        case J9BCbaload:
            maintainStackForArrayLoad(TR::Int8, bc);
            break;
        case J9BCfaload:
            maintainStackForArrayLoad(TR::Float, bc);
            break;
        case J9BCdaload:
            maintainStackForArrayLoad(TR::Double, bc);
            break;
        case J9BCaload0:
            slotIndex = 0;
            maintainStackForAload(slotIndex);
            break;
        case J9BCaload1:
            slotIndex = 1;
            maintainStackForAload(slotIndex);
            break;
        case J9BCaload2:
            slotIndex = 2;
            maintainStackForAload(slotIndex);
            break;
        case J9BCaload3:
            slotIndex = 3;
            maintainStackForAload(slotIndex);
            break;
        case J9BCaload:
            slotIndex = nextByte();
            maintainStackForAload(slotIndex);
            break;
        case J9BCaloadw:
            slotIndex = next2Bytes();
            maintainStackForAload(slotIndex);
            break;

        case J9BCinvokespecial:
        case J9BCinvokespecialsplit:
        case J9BCinvokevirtual:
        case J9BCinvokeinterface:
        case J9BCinvokestatic:
        case J9BCinvokestaticsplit:
        case J9BCinvokedynamic:
        case J9BCinvokehandle:
            maintainStackForCall();
            break;

        case J9BCiconstm1:
            push(new (trStackMemory()) IconstOperand(-1));
            break;
        case J9BCiconst0:
            push(new (trStackMemory()) IconstOperand(0));
            break;
        case J9BCiconst1:
            push(new (trStackMemory()) IconstOperand(1));
            break;
        case J9BCiconst2:
            push(new (trStackMemory()) IconstOperand(2));
            break;
        case J9BCiconst3:
            push(new (trStackMemory()) IconstOperand(3));
            break;
        case J9BCiconst4:
            push(new (trStackMemory()) IconstOperand(4));
            break;
        case J9BCiconst5:
            push(new (trStackMemory()) IconstOperand(5));
            break;

        case J9BCaconstnull:
            push(_nullOperand);
            break;

        case J9BCifne:
            push(new (trStackMemory()) IconstOperand(0));
            maintainStackForIf(J9BCificmpne);
            break;

        case J9BCifeq:
            push(new (trStackMemory()) IconstOperand(0));
            maintainStackForIf(J9BCificmpeq);
            break;

        case J9BCifnonnull:
            push(_nullOperand);
            maintainStackForIf(J9BCifacmpne);
            break;

        case J9BCifnull:
            push(_nullOperand);
            maintainStackForIf(J9BCifacmpeq);
            break;

        case J9BCificmpne:
        case J9BCificmpeq:
            maintainStackForIf(bc);
            break;

        case J9BCtableswitch:
            maintainStackForTableSwitch();
            break;

        case J9BCgoto: {
            int32_t target = bcIndex() + next2BytesSigned();
            _outEdgesStillReachable.insert(_currentInlinedBlock->getEdge(blocks(target)));
            saveStack(target);
            _currentBcCanFallThrough = false;
            break;
        }

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

        case J9BCistore:
        case J9BClstore:
        case J9BCfstore:
        case J9BCdstore:
        case J9BCistorew:
        case J9BClstorew:
        case J9BCfstorew:
        case J9BCdstorew:
        case J9BCistore0:
        case J9BCistore1:
        case J9BCistore2:
        case J9BCistore3:
        case J9BClstore0:
        case J9BClstore1:
        case J9BClstore2:
        case J9BClstore3:
        case J9BCfstore0:
        case J9BCfstore1:
        case J9BCfstore2:
        case J9BCfstore3:
        case J9BCdstore0:
        case J9BCdstore1:
        case J9BCdstore2:
        case J9BCdstore3:
            pop();
            break;

        // Maintain stack for object store
        case J9BCastorew:
            maintainStackForAstore(next2Bytes());
            break;
        case J9BCastore:
            maintainStackForAstore(nextByte());
            break;
        case J9BCastore0:
            maintainStackForAstore(0);
            break;
        case J9BCastore1:
            maintainStackForAstore(1);
            break;
        case J9BCastore2:
            maintainStackForAstore(2);
            break;
        case J9BCastore3:
            maintainStackForAstore(3);
            break;

        case J9BCiload0:
        case J9BCiload1:
        case J9BCiload2:
        case J9BCiload3:
        case J9BCdload0:
        case J9BCdload1:
        case J9BCdload2:
        case J9BCdload3:
        case J9BClload0:
        case J9BClload1:
        case J9BClload2:
        case J9BClload3:
        case J9BCfload0:
        case J9BCfload1:
        case J9BCfload2:
        case J9BCfload3:
        case J9BCiloadw:
        case J9BClloadw:
        case J9BCfloadw:
        case J9BCdloadw:
        case J9BCiload:
        case J9BClload:
        case J9BCfload:
        case J9BCdload:
            pushUnknownOperand();
            break;

        case J9BCgetstatic:
            maintainStackForGetStatic();
            break;

        // NOTE: athrow acts like return w.r.t. normal control flow edges.
        // Exception successors will still be processed, which is OK because we
        // get conservative at the start of catch blocks.
        case J9BCathrow:
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
            maintainStackForldc(nextByte());
            break;

        case J9BCnew:
            maintainStackForNew(next2Bytes());
            break;

        case J9BCinstanceof:
            maintainStackForInstanceof(next2Bytes());
            break;

        default:
            static const bool assertfatal
                = feGetEnv("TR_AssertFatalForUnexpectedBytecodeInMethodHandleThunk") ? true : false;
            if (!assertfatal)
                debugTrace(tracer(), "unexpected bytecode in thunk archetype %s (%p) at bcIndex %d %s (%d)\n",
                    _calltarget->_calleeMethod->signature(comp()->trMemory()), _calltarget, bcIndex(),
                    comp()->fej9()->getByteCodeName(nextByte(0)), bc);
            else
                TR_ASSERT_FATAL(0, "unexpected bytecode in thunk archetype %s (%p) at bcIndex %d %s (%d)\n",
                    _calltarget->_calleeMethod->signature(comp()->trMemory()), _calltarget, bcIndex(),
                    comp()->fej9()->getByteCodeName(nextByte(0)), bc);

            TR::DebugCounter::incStaticDebugCounter(comp(),
                TR::DebugCounter::debugCounterName(comp(),
                    "InterpreterEmulator.unexpectedBytecode/(root=%s)/(%s)/bc=%d/%s", comp()->signature(),
                    _calltarget->_calleeMethod->signature(comp()->trMemory()), _bcIndex,
                    comp()->fej9()->getByteCodeName(nextByte(0))));
            return false;
    }
    return true;
}

void InterpreterEmulator::maintainStackForReturn()
{
    assertHasState();
    TR_ASSERT_FATAL(_currentInlinedBlock->getSuccessors().size() == 1
            && _currentInlinedBlock->getSuccessors().front()->getTo() == _cfg->getEnd(),
        "block_%d returns and has incorrect successors", _currentInlinedBlock->getNumber());

    // Because this method doesn't continue running past the return, it doesn't
    // matter what happens to the stack.
    _currentBcCanFallThrough = false;
}

void InterpreterEmulator::maintainStackForGetStatic()
{
    assertHasState();
    if (comp()->compileRelocatableCode()) {
        pushUnknownOperand();
        return;
    }

    int32_t cpIndex = next2Bytes();
    debugTrace(tracer(), "getstatic cpIndex %d", cpIndex);

    void *dataAddress;
    bool isVolatile, isPrivate, isUnresolvedInCP, isFinal;
    TR::DataType type = TR::NoType;
    auto owningMethod = _calltarget->_calleeMethod;
    bool resolved = owningMethod->staticAttributes(comp(), cpIndex, &dataAddress, &type, &isVolatile, &isFinal,
        &isPrivate, false, &isUnresolvedInCP);

    TR_YesNoMaybe canFold = TR_no;
    TR::Symbol::RecognizedField recField = TR::Symbol::UnknownField;
    TR_OpaqueClassBlock *declaringClass = NULL;
    if (resolved && isFinal) {
        bool isStatic = true;
        recField = TR::Symbol::searchRecognizedField(comp(), owningMethod, cpIndex, isStatic);

        declaringClass = owningMethod->getDeclaringClassFromFieldOrStatic(comp(), cpIndex);

        canFold = TR::TransformUtil::canFoldStaticFinalField(comp(), declaringClass, recField, owningMethod, cpIndex);
    }

    TR::KnownObjectTable::Index knownObjectIndex = TR::KnownObjectTable::UNKNOWN;
    if (type == TR::Address
        && (canFold == TR_yes
            || (canFold == TR_maybe && TR::TransformUtil::enableEarlyGuardedStaticFinalFieldFolding()
                && TR::TransformUtil::canDoGuardedStaticFinalFieldFolding(comp())
                && comp()->isFearPointPlacementUnrestricted()))) {
        TR::AnyConst value = TR::AnyConst::makeAddress(0);
        bool gotValue = TR::TransformUtil::staticFinalFieldValue(comp(), owningMethod, cpIndex, dataAddress,
            TR::Address, recField, &value);

        if (gotValue && value.isKnownObject()) {
            knownObjectIndex = value.getKnownObjectIndex();
            TR::RequiredConst &reqConst = addRequiredConst(value);
            if (canFold == TR_maybe) {
                TR::Region &stackRegion = comp()->trMemory()->currentStackRegion();
                reqConst._assumptions.push_back(new (stackRegion) TR::DeferredStaticFinalOSRAssumption(declaringClass));
            }
        }
    }

    push(knownObjOperand(knownObjectIndex));
}

void InterpreterEmulator::maintainStackForAload(int slotIndex)
{
    assertHasState();
    push((*_currentLocalObjectInfo)[slotIndex]);
}

void InterpreterEmulator::maintainStackForAstore(int slotIndex)
{
    assertHasState();
    (*_currentLocalObjectInfo)[slotIndex] = pop();
}

void InterpreterEmulator::maintainStackForldc(int32_t cpIndex)
{
    assertHasState();
    TR::DataType type = method()->getLDCType(cpIndex);
    switch (type) {
        case TR::Address:
            // TODO: should add a function to check if cp entry is unresolved for all constant
            // not just for string. Currently only do it for string because it may be patched
            // to a different object in OpenJDK MethodHandle implementation
            //
            if (method()->isStringConstant(cpIndex) && !method()->isUnresolvedString(cpIndex)) {
                uintptr_t *location = (uintptr_t *)method()->stringConstant(cpIndex);
                TR::KnownObjectTable *knot = comp()->getKnownObjectTable();
                if (knot) {
                    TR::KnownObjectTable::Index koi = knot->getOrCreateIndexAt(location);
                    push(knownObjOperand(koi));
                    debugTrace(tracer(), "aload known obj%d from ldc %d", koi, cpIndex);

                    J9::ConstProvenanceGraph *cpg = comp()->constProvenanceGraph();
                    cpg->addEdge(method(), cpg->knownObject(koi));

                    return;
                }
            }
            break;
        default:
            break;
    }

    pushUnknownOperand();
}

void InterpreterEmulator::maintainStackForNew(int32_t cpIndex)
{
    assertHasState();

    TR_OpaqueClassBlock *clazz = method()->getClassFromConstantPool(comp(), cpIndex);
    if (clazz != NULL)
        push(new (trStackMemory()) FixedClassOperand(clazz));
    else
        pushUnknownOperand();
}

void InterpreterEmulator::maintainStackForInstanceof(int32_t cpIndex)
{
    assertHasState();

    ObjectOperand *obj = pop()->asObjectOperand();
    Operand *result = NULL;
    if (obj != NULL) {
        TR_OpaqueClassBlock *castClass = method()->getClassFromConstantPool(comp(), cpIndex);
        TR_OpaqueClassBlock *objClass = obj->getClass();
        if (castClass != NULL && objClass != NULL) {
            Operand::KnowledgeLevel level = obj->getKnowledgeLevel();
            bool fixedCast = true;
            bool fixedObj = level == Operand::FIXED_CLASS || level == Operand::KNOWN_OBJECT;

            TR_YesNoMaybe isInstance = comp()->fe()->isInstanceOf(objClass, castClass, fixedObj, fixedCast);

            if (isInstance == TR_yes)
                result = new (trStackMemory()) IconstOperand(1);
            else if (isInstance == TR_no)
                result = new (trStackMemory()) IconstOperand(0);
        }
    }

    if (result != NULL)
        push(result);
    else
        pushUnknownOperand();
}

void InterpreterEmulator::maintainStackForCall(Operand *result, int32_t numArgs, TR::DataType returnType)
{
    assertHasState();

    for (int i = 1; i <= numArgs; i++)
        pop();

    if (result)
        push(result);
    else if (returnType != TR::NoType)
        pushUnknownOperand();
}

void InterpreterEmulator::maintainStackForCall()
{
    assertHasState();

    int32_t numOfArgs = -1;
    TR::DataType returnType = TR::NoType;
    Operand *result = NULL;

    if (_currentCallMethod)
        result = getReturnValue(_currentCallMethod);

    // If the caller is thunk archetype, the load of parm `argPlaceholder` can
    // be expanded to loads of multiple arguments, so we can't pop the number
    // of arguments of a refined call
    //
    if (_currentCallSite && !_callerIsThunkArchetype) {
        if (_currentCallSite->_isInterface) {
            numOfArgs = _currentCallSite->_interfaceMethod->numberOfExplicitParameters() + 1;
            returnType = _currentCallSite->_interfaceMethod->returnType();
        } else if (_currentCallSite->_initialCalleeMethod) {
            numOfArgs = _currentCallSite->_initialCalleeMethod->numberOfParameters();
            returnType = _currentCallSite->_initialCalleeMethod->returnType();
        }
    } else {
        int32_t cpIndex = next2Bytes();
        bool isStatic = false;
        switch (current()) {
            case J9BCinvokespecialsplit:
                cpIndex |= J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG;
                break;
            case J9BCinvokestaticsplit:
                cpIndex |= J9_STATIC_SPLIT_TABLE_INDEX_FLAG;
            case J9BCinvokestatic:
                isStatic = true;
                break;
            case J9BCinvokedynamic: {
                // Find the signature, which corresponds to the arguments on the stack.
                // cpIndex is really the invokedynamic call site index
                J9ROMClass *romClass = TR::Compiler->cls.romClassOf(method()->classOfMethod());
                J9SRP *namesAndSigs = (J9SRP *)J9ROMCLASS_CALLSITEDATA(romClass);
                J9ROMNameAndSignature *nameAndSig = NNSRP_GET(namesAndSigs[cpIndex], J9ROMNameAndSignature *);
                J9UTF8 *sig = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);

                // Parse the signature to determine the number of arguments and
                // whether or not there is a return value.
                U_8 sigTypes[256]; // signatures are limited to 255 params + 1 return type
                UDATA numParams = 0;
                UDATA numParamSlots = 0;
                jitParseSignature(sig, sigTypes, &numParams, &numParamSlots);
                numOfArgs = numParams;

                // returnType is only used to distinguish void return (TR::NoType)
                // from non-void return (any other value), so it's not necessary to
                // get the correct non-void type here.
                if (sigTypes[numParams] == J9_NATIVE_TYPE_VOID)
                    returnType = TR::NoType;
                else
                    returnType = TR::Int32;
                break;
            }

            default:
                break;
        }
        if (numOfArgs < 0) {
            TR::Method *calleeMethod
                = comp()->fej9()->createMethod(trMemory(), _calltarget->_calleeMethod->containingClass(), cpIndex);
            numOfArgs = calleeMethod->numberOfExplicitParameters() + (isStatic ? 0 : 1);
            returnType = calleeMethod->returnType();
        }
    }
    maintainStackForCall(result, numOfArgs, returnType);
}

void InterpreterEmulator::dumpStack()
{
    if (!tracer()->debugLevel())
        return;

    if (_stack->size() == 0) {
        debugTrace(tracer(), "        (empty)");
    } else {
        for (int i = 0; i < _stack->size(); i++) {
            Operand *x = (*_stack)[i];
            _operandBuf->clear();
            x->printToString(_operandBuf);
            debugTrace(tracer(), "        [%d]=%s", i, _operandBuf->text());
        }
    }

    debugTrace(tracer(), ""); // blank line
}

Operand *InterpreterEmulator::getReturnValue(TR_ResolvedMethod *callee)
{
    if (!callee)
        return NULL;
    Operand *result = NULL;
    TR::RecognizedMethod recognizedMethod = callee->getRecognizedMethod();
    TR::KnownObjectTable *knot = comp()->getKnownObjectTable();

    TR::IlGeneratorMethodDetails &details = comp()->ilGenRequest().details();
    if (_callerIsThunkArchetype && details.isMethodHandleThunk()) {
        J9::MethodHandleThunkDetails &thunkDetails = static_cast<J9::MethodHandleThunkDetails &>(details);
        if (!thunkDetails.isCustom())
            recognizedMethod = TR::unknownMethod;
    }

    switch (recognizedMethod) {
        case TR::java_lang_invoke_ILGenMacros_isCustomThunk:
            result = new (trStackMemory()) IconstOperand(1);
            break;
        case TR::java_lang_invoke_ILGenMacros_isShareableThunk:
            result = new (trStackMemory()) IconstOperand(0);
            break;

#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
        case TR::java_lang_invoke_DelegatingMethodHandle_getTarget: {
            TR::KnownObjectTable::Index dmhIndex = top()->getKnownObjectIndex();
            bool trace = tracer()->debugLevel();
            TR::KnownObjectTable::Index targetIndex
                = comp()->fej9()->delegatingMethodHandleTarget(comp(), dmhIndex, trace);

            if (targetIndex == TR::KnownObjectTable::UNKNOWN)
                return NULL;

            result = knownObjOperand(targetIndex);
            break;
        }
#endif

        case TR::java_lang_invoke_MutableCallSite_getTarget:
        case TR::java_lang_invoke_Invokers_getCallSiteTarget: {
            // The CallSite object is always topmost on the stack.
            TR::KnownObjectTable::Index callSiteIndex = top()->getKnownObjectIndex();
            if (callSiteIndex == TR::KnownObjectTable::UNKNOWN)
                return NULL;

            TR::KnownObjectTable::Index resultIndex = TR::KnownObjectTable::UNKNOWN;
            const char * const mcsClassName = "java/lang/invoke/MutableCallSite";
            const int mcsClassNameLen = (int)strlen(mcsClassName);
            TR_OpaqueClassBlock *mutableCallsiteClass
                = fe()->getSystemClassFromClassName(mcsClassName, mcsClassNameLen, true);

            debugTrace(tracer(), "potential MCS target: call site obj%d(*%p) mutableCallsiteClass %p\n", callSiteIndex,
                knot->getPointerLocation(callSiteIndex), mutableCallsiteClass);
            if (mutableCallsiteClass) {
                if (recognizedMethod != TR::java_lang_invoke_MutableCallSite_getTarget) {
                    TR_OpaqueClassBlock *callSiteType = fe()->getObjectClassFromKnownObjectIndex(comp(), callSiteIndex);

                    if (fe()->isInstanceOf(callSiteType, mutableCallsiteClass, true) != TR_yes) {
                        debugTrace(tracer(), "not a MutableCallSite");
                        return NULL;
                    }
                }

#if defined(J9VM_OPT_JITSERVER)
                if (comp()->isOutOfProcessCompilation()) {
                    auto stream = comp()->getStream();
                    stream->write(JITServer::MessageType::KnownObjectTable_dereferenceKnownObjectField2,
                        mutableCallsiteClass, callSiteIndex);

                    auto recv = stream->read<TR::KnownObjectTable::Index, uintptr_t *>();
                    resultIndex = std::get<0>(recv);
                    uintptr_t *objectPointerReference = std::get<1>(recv);

                    if (resultIndex != TR::KnownObjectTable::UNKNOWN) {
                        knot->updateKnownObjectTableAtServer(resultIndex, objectPointerReference);
                    }
                    result = new (trStackMemory()) MutableCallsiteTargetOperand(resultIndex, callSiteIndex);
                } else
#endif /* defined(J9VM_OPT_JITSERVER) */
                {
                    TR::VMAccessCriticalSection dereferenceKnownObjectField(comp()->fej9());
                    int32_t targetFieldOffset = comp()->fej9()->getInstanceFieldOffset(mutableCallsiteClass, "target",
                        "Ljava/lang/invoke/MethodHandle;");
                    uintptr_t receiverAddress = knot->getPointer(callSiteIndex);
                    TR_OpaqueClassBlock *receiverClass = comp()->fej9()->getObjectClass(receiverAddress);
                    TR_ASSERT_FATAL(comp()->fej9()->isInstanceOf(receiverClass, mutableCallsiteClass, true) == TR_yes,
                        "receiver of mutableCallsite_getTarget must be instance of MutableCallSite (*%p)",
                        knot->getPointerLocation(callSiteIndex));
                    uintptr_t fieldAddress = comp()->fej9()->getReferenceFieldAt(receiverAddress, targetFieldOffset);
                    resultIndex = knot->getOrCreateIndex(fieldAddress);
                    result = new (trStackMemory()) MutableCallsiteTargetOperand(resultIndex, callSiteIndex);
                }

                if (resultIndex != TR::KnownObjectTable::UNKNOWN) {
                    J9::ConstProvenanceGraph *cpg = comp()->constProvenanceGraph();
                    cpg->addEdge(cpg->knownObject(callSiteIndex), cpg->knownObject(resultIndex));
                }
            }
        } break;
        case TR::java_lang_invoke_DirectMethodHandle_internalMemberName:
        case TR::java_lang_invoke_DirectMethodHandle_internalMemberNameEnsureInit: {
            Operand *mh = top();
            TR::KnownObjectTable::Index mhIndex = top()->getKnownObjectIndex();
            debugTrace(tracer(), "Known DirectMethodHandle koi %d\n", mhIndex);
            TR::KnownObjectTable *knot = comp()->getKnownObjectTable();
            if (knot && mhIndex != TR::KnownObjectTable::UNKNOWN) {
                TR::KnownObjectTable::Index memberIndex
                    = comp()->fej9()->getMemberNameFieldKnotIndexFromMethodHandleKnotIndex(comp(), mhIndex, "member");
                debugTrace(tracer(), "Known internal member name koi %d\n", memberIndex);
                result = knownObjOperand(memberIndex);
            }
            break;
        }
        case TR::java_lang_invoke_DirectMethodHandle_constructorMethod: {
            Operand *mh = top();
            TR::KnownObjectTable::Index mhIndex = top()->getKnownObjectIndex();
            debugTrace(tracer(), "Known DirectMethodHandle koi %d\n", mhIndex);
            TR::KnownObjectTable *knot = comp()->getKnownObjectTable();
            if (knot && mhIndex != TR::KnownObjectTable::UNKNOWN) {
                TR::KnownObjectTable::Index memberIndex
                    = comp()->fej9()->getMemberNameFieldKnotIndexFromMethodHandleKnotIndex(comp(), mhIndex,
                        "initMethod");
                debugTrace(tracer(), "Known internal member name koi %d\n", memberIndex);
                result = knownObjOperand(memberIndex);
            }
            break;
        }
        case TR::java_lang_invoke_Invokers_checkVarHandleGenericType: {
            Operand *varHandleOperand = topn(1);
            Operand *accessDescriptorOperand = topn(0);
            TR::KnownObjectTable::Index vhIndex = varHandleOperand->getKnownObjectIndex();
            TR::KnownObjectTable::Index adIndex = accessDescriptorOperand->getKnownObjectIndex();
            TR::KnownObjectTable *knot = comp()->getKnownObjectTable();
            if (knot && vhIndex != TR::KnownObjectTable::UNKNOWN && adIndex != TR::KnownObjectTable::UNKNOWN
                && !knot->isNull(vhIndex) && !knot->isNull(adIndex)) {
                TR::KnownObjectTable::Index mhIndex
                    = comp()->fej9()->getMethodHandleTableEntryIndex(comp(), vhIndex, adIndex);
                result = knownObjOperand(mhIndex);
            }
            break;
        }
        case TR::java_lang_invoke_Invokers_checkGenericType: {
            if (!comp()->useConstRefs())
                break;

            Operand *receiverMHOperand = topn(1);
            Operand *desiredMTOperand = topn(0);
            TR::KnownObjectTable::Index mhIndex = receiverMHOperand->getKnownObjectIndex();
            TR::KnownObjectTable::Index mtIndex = desiredMTOperand->getKnownObjectIndex();
            debugTrace(tracer(), "Known MethodHandle koi %d\n", mhIndex);
            debugTrace(tracer(), "Known MethodType koi %d\n", mtIndex);
            TR::KnownObjectTable *knot = comp()->getKnownObjectTable();
            if (knot && mhIndex != TR::KnownObjectTable::UNKNOWN && mtIndex != TR::KnownObjectTable::UNKNOWN
                && !knot->isNull(mhIndex) && !knot->isNull(mtIndex)) {
                if (comp()->fej9()->isMethodHandleExpectedType(comp(), mhIndex, mtIndex)) {
                    result = knownObjOperand(mhIndex);
                    debugTrace(tracer(), "MH.asType: exact match\n");
                    break;
                }

                TR::KnownObjectTable::Index convertedMHIndex
                    = comp()->fej9()->getConvertedMethodHandle(comp(), mhIndex, mtIndex);
                if (TR::KnownObjectTable::UNKNOWN != convertedMHIndex) {
                    J9::ConstProvenanceGraph *cpg = comp()->constProvenanceGraph();
                    cpg->addEdge(cpg->knownObject(mhIndex), cpg->knownObject(convertedMHIndex));
                    result = knownObjOperand(convertedMHIndex);
                    debugTrace(tracer(), "MH.asType: subtype match\n");
                }
            }
            break;
        }
        case TR::jdk_internal_foreign_layout_ValueLayouts_AbstractValueLayout_accessHandle: {
            Operand *layoutOperand = top();
            TR::KnownObjectTable::Index layoutIndex = layoutOperand->getKnownObjectIndex();
            TR::KnownObjectTable *knot = comp()->getKnownObjectTable();
            if (knot && layoutIndex != TR::KnownObjectTable::UNKNOWN && !knot->isNull(layoutIndex)) {
                TR::KnownObjectTable::Index vhIndex = comp()->fej9()->getLayoutVarHandle(comp(), layoutIndex);
                result = knownObjOperand(vhIndex);
            }
            break;
        }

        default:
            break;
    }

    if (result != NULL && result != _unknownOperand) {
        if (result->asIconst() != NULL) {
            auto value = TR::AnyConst::makeInt32(result->asIconst()->intValue);
            addRequiredConst(value);
        } else if (result->asKnownObject() != NULL) {
            auto i = result->asKnownObject()->getKnownObjectIndex();
            auto value = TR::AnyConst::makeKnownObject(i);
            addRequiredConst(value);
        } else if (result->asMutableCallsiteTargetOperand() != NULL) {
            // These are handled differently, so nothing to do here.
        } else {
            TR::StringBuf buf(comp()->trMemory()->currentStackRegion());
            result->printToString(&buf);
            TR_ASSERT_FATAL(false, "failed to record required constant call result: %s", buf.text());
        }
    }

    return result;
}

void InterpreterEmulator::refineResolvedCalleeForInvokestatic(TR_ResolvedMethod *&callee,
    TR::KnownObjectTable::Index &mcsIndex, TR::KnownObjectTable::Index &mhIndex, bool &isIndirectCall,
    TR_OpaqueClassBlock *&receiverClass)
{
    assertHasState();
    receiverClass = NULL;

    if (!comp()->getOrCreateKnownObjectTable())
        return;

    bool isVirtual = false;
    TR::RecognizedMethod rm = callee->getRecognizedMethod();
    switch (rm) {
        // refine the ILGenMacros_invokeExact* callees
        case TR::java_lang_invoke_ILGenMacros_invokeExact:
        case TR::java_lang_invoke_ILGenMacros_invokeExact_X:
        case TR::java_lang_invoke_ILGenMacros_invokeExactAndFixup: {
            int argNum = callee->numberOfExplicitParameters();
            if (argNum > 0) {
                Operand *operand = topn(argNum - 1); // for the ILGenMacros_invokeExact* methods, the first argument is
                                                     // always the methodhandle object
                MutableCallsiteTargetOperand *mcsOperand = operand->asMutableCallsiteTargetOperand();
                if (mcsOperand) {
                    mhIndex = mcsOperand->getMethodHandleIndex();
                    mcsIndex = mcsOperand->getMutableCallsiteIndex();
                } else
                    mhIndex = operand->getKnownObjectIndex();
            }

            if (mhIndex != TR::KnownObjectTable::UNKNOWN) {
                debugTrace(tracer(),
                    "refine java_lang_invoke_MethodHandle_invokeExact with obj%d to archetype specimen at bcIndex=%d\n",
                    mhIndex, _bcIndex);
                callee = comp()->fej9()->createMethodHandleArchetypeSpecimen(this->trMemory(),
                    comp()->getKnownObjectTable()->getPointerLocation(mhIndex), _calltarget->_calleeMethod);
            }
            return;
        }
        // refine the leaf method handle callees
        case TR::java_lang_invoke_VirtualHandle_virtualCall:
            isVirtual = true;
        case TR::java_lang_invoke_DirectHandle_directCall: {
            TR_J9VMBase *fej9 = comp()->fej9();
            TR_J9VMBase::MethodOfHandle moh
                = fej9->methodOfDirectOrVirtualHandle(_calltarget->_calleeMethod->getMethodHandleLocation(), isVirtual);

            TR_ASSERT_FATAL(moh.j9method != NULL, "Must have a j9method to generate a custom call");
            uint32_t vTableSlot = isVirtual ? (uint32_t)moh.vmSlot : 0;
            TR_ResolvedMethod *newCallee = fej9->createResolvedMethodWithVTableSlot(trMemory(), vTableSlot,
                moh.j9method, _calltarget->_calleeMethod);

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
        case TR::java_lang_invoke_MethodHandle_linkToVirtual: {
            TR::KnownObjectTable::Index memberNameIndex = top()->getKnownObjectIndex();
            TR_J9VMBase *fej9 = comp()->fej9();
            TR_J9VMBase::MemberNameMethodInfo info = {};
            if (!fej9->getMemberNameMethodInfo(comp(), memberNameIndex, &info)) {
                const char *reason = (memberNameIndex == TR::KnownObjectTable::UNKNOWN) ? "unknownMemberName"
                                                                                        : "memberNameInfoUnavailable";
                heuristicTrace(tracer(), "Failed to refine linkTo call: %s at bcIndex=%d\n", reason, _bcIndex);
                TR::DebugCounter::incStaticDebugCounter(comp(),
                    TR::DebugCounter::debugCounterName(comp(),
                        "InterpreterEmulator/MHInliningFailure/linkTo/(root=%s)/(%s)/%s", comp()->signature(),
                        _calltarget->_calleeMethod->signature(comp()->trMemory()), reason));
                return;
            }

            if (info.vmtarget == NULL) {
                heuristicTrace(tracer(), "Failed to refine linkTo call: nullVmtarget at bcIndex=%d\n", _bcIndex);
                TR::DebugCounter::incStaticDebugCounter(comp(),
                    TR::DebugCounter::debugCounterName(comp(),
                        "InterpreterEmulator/MHInliningFailure/linkTo/(root=%s)/(%s)/nullVmtarget", comp()->signature(),
                        _calltarget->_calleeMethod->signature(comp()->trMemory())));
                return;
            }

            uint32_t vTableSlot = 0;
            if (rm == TR::java_lang_invoke_MethodHandle_linkToVirtual) {
                if (info.refKind != MH_REF_INVOKEVIRTUAL) {
                    heuristicTrace(tracer(),
                        "Failed to refine linkToVirtual call: unexpectedRefKind=%d at bcIndex=%d\n", info.refKind,
                        _bcIndex);
                    TR::DebugCounter::incStaticDebugCounter(comp(),
                        TR::DebugCounter::debugCounterName(comp(),
                            "InterpreterEmulator/MHInliningFailure/linkTo/(root=%s)/(%s)/unexpectedRefKind",
                            comp()->signature(), _calltarget->_calleeMethod->signature(comp()->trMemory())));
                    return;
                }

                vTableSlot = info.vmindex;
            }

            // Direct or virtual dispatch. A vTableSlot of 0 indicates a direct
            // call, in which case vTableSlot won't really be used as such.
            callee = fej9->createResolvedMethodWithVTableSlot(comp()->trMemory(), vTableSlot, info.vmtarget,
                _calltarget->_calleeMethod);
            receiverClass = info.clazz;
            isIndirectCall = vTableSlot != 0;
            TR_ASSERT_FATAL(!isIndirectCall || rm == TR::java_lang_invoke_MethodHandle_linkToVirtual
                    || rm == TR::java_lang_invoke_MethodHandle_linkToInterface,
                "indirect linkTo call should only be linkToVirtual or linkToInterface");

            heuristicTrace(tracer(), "Refine linkTo to %s\n", callee->signature(trMemory(), stackAlloc));
            // The refined method doesn't take MemberName as an argument, pop MemberName out of the operand stack
            pop();
            return;
        }
#endif // J9VM_OPT_OPENJDK_METHODHANDLE

        default:
            break;
    }
}

bool InterpreterEmulator::shouldIterateWithState()
{
    if (comp()->compileRelocatableCode())
        return false;

    // Use state if any argument is a known object.
    TR_PrexArgInfo *argInfo = _calltarget->_ecsPrexArgInfo;
    if (argInfo != NULL) {
        TR_ASSERT_FATAL(argInfo->getNumArgs() == method()->numberOfParameters(),
            "wrong number of prex args %d, should be %d", argInfo->getNumArgs(), method()->numberOfParameters());

        method()->makeParameterList(_methodSymbol);
        ListIterator<TR::ParameterSymbol> parms(&_methodSymbol->getParameterList());
        for (TR::ParameterSymbol *p = parms.getFirst(); p != NULL; p = parms.getNext()) {
            TR_PrexArgument *prexArg = argInfo->get(p->getOrdinal());
            if (prexArg != NULL && TR_PrexArgument::knowledgeLevel(prexArg) == KNOWN_OBJECT) {
                heuristicTrace(tracer(), "known object argument found: iterating with state");
                return true;
            }
        }
    }

    // Look for getstatic instructions loading from final static fields.
    TR_J9ByteCode bc = first();
    for (TR_J9ByteCode bc = first(); bc != J9BCunknown; bc = next()) {
        if (bc != J9BCgetstatic)
            continue;

        int32_t cpIndex = next2Bytes();

        void *dataAddress;
        bool isVolatile, isPrivate, isUnresolvedInCP, isFinal;
        TR::DataType type = TR::NoType;
        auto owningMethod = _calltarget->_calleeMethod;
        bool resolved = owningMethod->staticAttributes(comp(), cpIndex, &dataAddress, &type, &isVolatile, &isFinal,
            &isPrivate, false, &isUnresolvedInCP);

        if (resolved && isFinal && type == TR::Address) {
            heuristicTrace(tracer(), "static final load found: iterating with state");
            return true;
        }
    }

    // Nothing interesting to propagate
    return false;
}

bool InterpreterEmulator::findAndCreateCallsitesFromBytecodes(bool wasPeekingSuccessfull, bool withState)
{
    static const bool enableMore = feGetEnv("TR_moreInterpreterEmulator") != NULL;
    if (enableMore) {
        // ignore withState and determine on our own whether to use state
        withState = shouldIterateWithState();
    }

    heuristicTrace(tracer(), "Find and create callsite %s\n", withState ? "with state" : "without state");

    if (withState)
        initializeIteratorWithState();

    _wasPeekingSuccessfull = wasPeekingSuccessfull;

    TR::ReversePostorderSnapshotBlockIterator blockIt(_cfg, comp());
    _currentInlinedBlock = blockIt.currentBlock();
    for (; blockIt.currentBlock() != NULL; blockIt.stepForward()) {
        TR::Block *block = blockIt.currentBlock();
        if (block == _cfg->getStart() || block == _cfg->getEnd()) {
            _visitedBlocks.add(block);
            continue; // no corresponding bytecode
        }

        int32_t blockStartBci = block->getBlockBCIndex();
        debugTrace(tracer(), "Start block_%d [%p], bci %+d\n", block->getNumber(), block, blockStartBci);

        _currentInlinedBlock = block;

        if (_iteratorWithState) {
            _outEdgesStillReachable.clear();

            if (_potentialCycleBlocks.contains(block)) {
                // Some but not all of block's incoming edges have been found to be
                // unreachable. Block could belong to an unreachable cycle.
                //
                // On the simplifying assumption that visited blocks are always
                // reachable, block must also be reachable if it has a visited
                // predecessor (from which the edge is not known to be unreachable).
                //
                if (!hasVisitedPred(block)) {
                    // Do mark/sweep to determine precise reachability based on the
                    // current state of the analysis. This will search through all
                    // of the current CFG, but it shouldn't happen too often. If the
                    // CFG is reducible - which it should almost always be - then
                    // at this point block must be an unreachable loop header. To
                    // see why, note that back-edges don't affect the order in which
                    // blocks appear in a DFS, so reverse postorder is a topological
                    // sort of the CFG after deleting back-edges. Furthermore, back-
                    // edges are also irrelevant to reachability. So if block is
                    // reachable, it has a visited predecessor, and otherwise only a
                    // back-edge could prevent its refcount from reaching zero.
                    //
                    // Unfortunately we can't correctly conclude that block is
                    // unreachable here without performing a search. It might still
                    // be reachable if it's part of an improper loop.
                    //
                    markSweepCFG();
                }
            }

            if (isBlockUnreachable(block)) {
                debugTrace(tracer(), "unreachable block");

                // Because it's not possible to enter block at all, all of its
                // outgoing edges are unreachable, including exception edges.
                markSuccessorsUnreachable(block->getSuccessors());
                markSuccessorsUnreachable(block->getExceptionSuccessors());
                debugTrace(tracer(), "skip\n");

                continue;
            }

            _visitedBlocks.add(block);
            setupBBStartContext(blockStartBci);
            if (tracer()->debugLevel()) {
                debugTrace(tracer(), "      operand stack at start of block");
                dumpStack();
            }
        }

        setIndex(blockStartBci);

        TR_J9ByteCode bc = current();
        while (true) {
            heuristicTrace(tracer(), "%4d: %s\n", _bcIndex, comp()->fej9()->getByteCodeName(_code[_bcIndex]));

            _currentCallSite = NULL;
            _currentCallMethod = NULL;
            _currentCallMethodUnrefined = NULL;

            TR_ASSERT_FATAL(!isGenerated(_bcIndex),
                "InterpreterEmulator::findCallsitesFromBytecodes bcIndex %d has been generated\n", _bcIndex);
            _newBCInfo->setByteCodeIndex(_bcIndex);

            switch (bc) {
                case J9BCinvokedynamic:
                    visitInvokedynamic();
                    break;
                case J9BCinvokevirtual:
                    visitInvokevirtual();
                    break;
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
                case J9BCinvokehandle:
                    visitInvokehandle();
                    break;
#endif
                case J9BCinvokespecial:
                case J9BCinvokespecialsplit:
                    visitInvokespecial();
                    break;
                case J9BCinvokestatic:
                case J9BCinvokestaticsplit:
                    visitInvokestatic();
                    break;
                case J9BCinvokeinterface:
                    visitInvokeinterface();
                    break;

                default:
                    break;
            }

            if (_iteratorWithState) {
                _currentBcCanFallThrough = true; // may be reset by maintainStack()
                if (!maintainStack(bc))
                    return false;

                setIsGenerated(_bcIndex);

                if (tracer()->debugLevel()) {
                    debugTrace(tracer(), "      operand stack after bytecode %+d : %s", _bcIndex,
                        comp()->fej9()->getByteCodeName(nextByte(0)));

                    dumpStack();
                }

                if (!_currentBcCanFallThrough) {
                    debugTrace(tracer(), "instruction cannot fall through to next");
                    // Only mark regular successors unreachable. There may have been
                    // an exception point in the current block.
                    markSuccessorsUnreachable(block->getSuccessors());
                    break;
                }
            }

            _pca.updateArg(bc);

            bc = next();
            if (bc == J9BCunknown) {
                break;
            } else if (_InterpreterEmulatorFlags[_bcIndex].testAny(
                           InterpreterEmulator::BytecodePropertyFlag::bbStart)) {
                if (_iteratorWithState) {
                    saveStack(_bcIndex);
                    debugTrace(tracer(), "fall through to block_%d at +%d", blocks(_bcIndex)->getNumber(), _bcIndex);
                }

                break;
            }
        }

        debugTrace(tracer(), "End of block_%d\n", _currentInlinedBlock->getNumber());
    }

    heuristicTrace(tracer(), "Finish findAndCreateCallsitesFromBytecodes\n");
    return true;
}

void InterpreterEmulator::prepareToFindAndCreateCallsites(TR::Block **blocks, flags8_t *flags, TR_CallSite **callSites,
    TR::CFG *cfg, TR_ByteCodeInfo *newBCInfo, int32_t recursionDepth, TR_CallStack *callStack)
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

void InterpreterEmulator::visitInvokedynamic()
{
    TR_ResolvedJ9Method *owningMethod = static_cast<TR_ResolvedJ9Method *>(_methodSymbol->getResolvedMethod());
    int32_t callSiteIndex = next2Bytes();
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
    if (owningMethod->isUnresolvedCallSiteTableEntry(callSiteIndex) || comp()->compileRelocatableCode())
        return; // do nothing if unresolved, is AOT compilation
    uintptr_t *invokeCacheArray = (uintptr_t *)owningMethod->callSiteTableEntryAddress(callSiteIndex);
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
    if (knot && !owningMethod->isUnresolvedCallSiteTableEntry(callSiteIndex)) {
        isIndirectCall = true;
        uintptr_t *entryLocation = (uintptr_t *)owningMethod->callSiteTableEntryAddress(callSiteIndex);
        // Add callsite handle to known object table
        knot->getOrCreateIndexAt((uintptr_t *)entryLocation);
        _currentCallMethod
            = comp()->fej9()->createMethodHandleArchetypeSpecimen(this->trMemory(), entryLocation, owningMethod);
        _currentCallMethodUnrefined = _currentCallMethod;
        bool allconsts = false;

        heuristicTrace(tracer(), "numberOfExplicitParameters = %d  _pca.getNumPrevConstArgs = %d\n",
            _currentCallMethod->numberOfExplicitParameters(),
            _pca.getNumPrevConstArgs(_currentCallMethod->numberOfExplicitParameters()));
        if (_currentCallMethod->numberOfExplicitParameters() > 0
            && _currentCallMethod->numberOfExplicitParameters()
                <= _pca.getNumPrevConstArgs(_currentCallMethod->numberOfExplicitParameters()))
            allconsts = true;

        OMR::RetainedMethodSet *retainedMethods = _calltarget->_retainedMethods;
        bool wasRefinedFromKnownObject = false;
        TR_CallSite *callsite = new (comp()->trHeapMemory()) TR_J9MethodHandleCallSite(_calltarget->_calleeMethod,
            callNodeTreeTop, parent, callNode, interfaceMethod, _currentCallMethod->classOfMethod(), -1, -1,
            _currentCallMethod, resolvedSymbol, isIndirectCall, isInterface, *_newBCInfo, comp(), _recursionDepth,
            allconsts, retainedMethods, wasRefinedFromKnownObject);

        findTargetAndUpdateInfoForCallsite(callsite);
    }
#endif // J9VM_OPT_OPENJDK_METHODHANDLE
}

#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
void InterpreterEmulator::visitInvokehandle()
{
    int32_t cpIndex = next2Bytes();
    TR_ResolvedJ9Method *owningMethod = static_cast<TR_ResolvedJ9Method *>(_methodSymbol->getResolvedMethod());
    if (owningMethod->isUnresolvedMethodTypeTableEntry(cpIndex) || comp()->compileRelocatableCode())
        return; // do nothing if unresolved, is an AOT compilation
    uintptr_t *invokeCacheArray = (uintptr_t *)owningMethod->methodTypeTableEntryAddress(cpIndex);
    updateKnotAndCreateCallSiteUsingInvokeCacheArray(owningMethod, invokeCacheArray, cpIndex);
}

void InterpreterEmulator::updateKnotAndCreateCallSiteUsingInvokeCacheArray(TR_ResolvedJ9Method *owningMethod,
    uintptr_t *invokeCacheArray, int32_t cpIndex)
{
    TR_J9VMBase *fej9 = comp()->fej9();
    TR::KnownObjectTable::Index idx = fej9->getKnotIndexOfInvokeCacheArrayAppendixElement(comp(), invokeCacheArray);
    if (_iteratorWithState)
        push(knownObjOperand(idx));

    TR_ResolvedMethod *targetMethod
        = fej9->targetMethodFromInvokeCacheArrayMemberNameObj(comp(), owningMethod, invokeCacheArray);

    // The caller got invokeCacheArray from owningMethod.
    J9::ConstProvenanceGraph *cpg = comp()->constProvenanceGraph();
    cpg->addEdge(owningMethod, cpg->knownObject(idx));
    cpg->addEdge(owningMethod, targetMethod);

    bool isInterface = false;
    bool isIndirectCall = false;
    TR::Method *interfaceMethod = 0;
    TR::TreeTop *callNodeTreeTop = 0;
    TR::Node *parent = 0;
    TR::Node *callNode = 0;
    TR::ResolvedMethodSymbol *resolvedSymbol = 0;
    TR::KnownObjectTable *knot = comp()->getOrCreateKnownObjectTable();
    bool allconsts = false;
    if (targetMethod->numberOfExplicitParameters() > 0
        && targetMethod->numberOfExplicitParameters()
            <= _pca.getNumPrevConstArgs(targetMethod->numberOfExplicitParameters()))
        allconsts = true;

    OMR::RetainedMethodSet *retainedMethods = _calltarget->_retainedMethods;
    bool wasRefinedFromKnownObject = false;
    TR_CallSite *callsite
        = new (comp()->trHeapMemory()) TR_DirectCallSite(_calltarget->_calleeMethod, callNodeTreeTop, parent, callNode,
            interfaceMethod, targetMethod->classOfMethod(), -1, cpIndex, targetMethod, resolvedSymbol, isIndirectCall,
            isInterface, *_newBCInfo, comp(), _recursionDepth, allconsts, retainedMethods, wasRefinedFromKnownObject);

    findTargetAndUpdateInfoForCallsite(callsite, idx);
}

#endif // J9VM_OPT_OPENJDK_METHODHANDLE

bool InterpreterEmulator::isCurrentCallUnresolvedOrCold(TR_ResolvedMethod *resolvedMethod, bool isUnresolvedInCP)
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

void InterpreterEmulator::debugUnresolvedOrCold(TR_ResolvedMethod *resolvedMethod)
{
    int32_t cpIndex = next2Bytes();
    if (tracer()->heuristicLevel()) {
        if (resolvedMethod)
            heuristicTrace(tracer(), "Depth %d: Call at bc index %d is Cold.  Not searching for targets. Signature %s",
                _recursionDepth, _bcIndex, resolvedMethod->signature(comp()->trMemory()));
        else {
            switch (current()) {
                case J9BCinvokespecialsplit:
                    cpIndex |= J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG;
                    break;
                case J9BCinvokestaticsplit:
                    cpIndex |= J9_STATIC_SPLIT_TABLE_INDEX_FLAG;
                    break;

                default:
                    break;
            }
            TR::Method *meth = comp()->fej9()->createMethod(this->trMemory(),
                _calltarget->_calleeMethod->containingClass(), cpIndex);
            heuristicTrace(tracer(), "Depth %d: Call at bc index %d is Cold.  Not searching for targets. Signature %s",
                _recursionDepth, _bcIndex, meth->signature(comp()->trMemory()));
        }
    }
}

void InterpreterEmulator::refineResolvedCalleeForInvokevirtual(TR_ResolvedMethod *&callee, bool &isIndirectCall)
{
    assertHasState();
    if (!comp()->getOrCreateKnownObjectTable())
        return;

    TR::RecognizedMethod rm = callee->getRecognizedMethod();
    switch (rm) {
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
        case TR::java_lang_invoke_MethodHandle_invokeBasic: {
            int argNum = callee->numberOfExplicitParameters();
            TR::KnownObjectTable::Index receiverIndex = topn(argNum)->getKnownObjectIndex();
            TR_J9VMBase *fej9 = comp()->fej9();
            auto targetMethod = fej9->targetMethodFromMethodHandle(comp(), receiverIndex);
            if (!targetMethod) {
                const char *reason
                    = (receiverIndex == TR::KnownObjectTable::UNKNOWN) ? "unknownMHReceiver" : "noTargetFromMH";
                heuristicTrace(tracer(), "Failed to refine invokeBasic call: %s at bcIndex=%d\n", reason, _bcIndex);
                TR::DebugCounter::incStaticDebugCounter(comp(),
                    TR::DebugCounter::debugCounterName(comp(),
                        "InterpreterEmulator/MHInliningFailure/invokeBasic/(root=%s)/(%s)/%s", comp()->signature(),
                        _calltarget->_calleeMethod->signature(comp()->trMemory()), reason));
                return;
            }

            TR_ResolvedMethod *refinedMethod
                = fej9->createResolvedMethod(comp()->trMemory(), targetMethod, callee->owningMethod());
            heuristicTrace(tracer(), "Pre-refinement invokebasic numargs: %d. Refined invokeBasic numArgs: %d\n",
                argNum, refinedMethod->numberOfExplicitParameters());
            if (refinedMethod->numberOfExplicitParameters() != (argNum + 1)) {
                heuristicTrace(tracer(),
                    "Failed to refine invokeBasic call due unexpected number of args in the potential refined "
                    "method.\n");
                TR::DebugCounter::incStaticDebugCounter(comp(),
                    TR::DebugCounter::debugCounterName(comp(),
                        "InterpreterEmulator/MHInliningFailure/invokeBasic/(root=%s)/(%s)/unexpectedArgCount",
                        comp()->signature(), _calltarget->_calleeMethod->signature(comp()->trMemory())));
                return;
            }

            isIndirectCall = false;
            callee = refinedMethod;
            heuristicTrace(tracer(), "Refine invokeBasic to %s\n", callee->signature(trMemory(), stackAlloc));
            return;
        }
#endif // J9VM_OPT_OPENJDK_METHODHANDLE
        default:
            return;
    }
}

void InterpreterEmulator::visitInvokevirtual()
{
    int32_t cpIndex = next2Bytes();
    auto calleeMethod = (TR_ResolvedJ9Method *)_calltarget->_calleeMethod;
    bool isUnresolvedInCP;
    // Calls in thunk archetype won't be executed by interpreter, so they may appear as unresolved
    bool ignoreRtResolve = _callerIsThunkArchetype;
    _currentCallMethod
        = calleeMethod->getResolvedPossiblyPrivateVirtualMethod(comp(), cpIndex, ignoreRtResolve, &isUnresolvedInCP);
    _currentCallMethodUnrefined = _currentCallMethod;
    Operand *result = NULL;
    if (isCurrentCallUnresolvedOrCold(_currentCallMethod, isUnresolvedInCP)) {
        debugUnresolvedOrCold(_currentCallMethod);
    } else if (_currentCallMethod) {
        bool isIndirectCall = !_currentCallMethod->isFinal() && !_currentCallMethod->isPrivate();
        bool wasRefinedFromKnownObject = false;
        if (_iteratorWithState) {
            refineResolvedCalleeForInvokevirtual(_currentCallMethod, isIndirectCall);
            wasRefinedFromKnownObject = _currentCallMethod != _currentCallMethodUnrefined;
        }

        // Customization logic is not needed in customized thunk or in inlining
        // with known MethodHandle object
        // Since branch folding is disabled and we're ignoring the coldness info
        // in thunk archetype, calls to the following method will be added to the
        // call site list and take up some inlining budget, causing less methods
        // to be inlined. Don't create call site for them
        //
        switch (_currentCallMethod->getRecognizedMethod()) {
            case TR::java_lang_invoke_MethodHandle_doCustomizationLogic:
            case TR::java_lang_invoke_MethodHandle_undoCustomizationLogic:
                if (_callerIsThunkArchetype)
                    return;

            default:
                break;
        }

        bool allconsts = false;
        heuristicTrace(tracer(), "numberOfExplicitParameters = %d  _pca.getNumPrevConstArgs = %d\n",
            _currentCallMethod->numberOfExplicitParameters(),
            _pca.getNumPrevConstArgs(_currentCallMethod->numberOfExplicitParameters()));
        if (_currentCallMethod->numberOfExplicitParameters() > 0
            && _currentCallMethod->numberOfExplicitParameters()
                <= _pca.getNumPrevConstArgs(_currentCallMethod->numberOfExplicitParameters()))
            allconsts = true;

        TR_CallSite *callsite;
        bool isInterface = false;
        TR::Method *interfaceMethod = 0;
        TR::TreeTop *callNodeTreeTop = 0;
        TR::Node *parent = 0;
        TR::Node *callNode = 0;
        TR::ResolvedMethodSymbol *resolvedSymbol = 0;
        OMR::RetainedMethodSet *retainedMethods = _calltarget->_retainedMethods;

        Operand *receiver = NULL;
        if (_iteratorWithState)
            receiver = topn(_currentCallMethodUnrefined->numberOfExplicitParameters());

        if (_currentCallMethod->convertToMethod()->isArchetypeSpecimen()
            && _currentCallMethod->getMethodHandleLocation()) {
            callsite = new (comp()->trHeapMemory()) TR_J9MethodHandleCallSite(_calltarget->_calleeMethod,
                callNodeTreeTop, parent, callNode, interfaceMethod, _currentCallMethod->classOfMethod(), -1, cpIndex,
                _currentCallMethod, resolvedSymbol, isIndirectCall, isInterface, *_newBCInfo, comp(), _recursionDepth,
                allconsts, retainedMethods, wasRefinedFromKnownObject);
        } else if (_currentCallMethod->getRecognizedMethod() == TR::java_lang_invoke_MethodHandle_invokeExact
            || (_currentCallMethod->getRecognizedMethod() == TR::java_lang_invoke_MethodHandle_invokeBasic
                && receiver != NULL && receiver->asMutableCallsiteTargetOperand() != NULL)) {
            TR_J9MutableCallSite *inlinerMcs
                = new (comp()->trHeapMemory()) TR_J9MutableCallSite(_calltarget->_calleeMethod, callNodeTreeTop, parent,
                    callNode, interfaceMethod, _currentCallMethod->classOfMethod(), -1, cpIndex, _currentCallMethod,
                    resolvedSymbol, isIndirectCall, isInterface, *_newBCInfo, comp(), _recursionDepth, allconsts,
                    retainedMethods, wasRefinedFromKnownObject);
            if (_currentCallMethod->getRecognizedMethod() == TR::java_lang_invoke_MethodHandle_invokeBasic) {
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
                inlinerMcs->setMCS(mcsIndex);
            }

            callsite = inlinerMcs;
        } else if (isIndirectCall) {
            callsite = new (comp()->trHeapMemory())
                TR_J9VirtualCallSite(_calltarget->_calleeMethod, callNodeTreeTop, parent, callNode, interfaceMethod,
                    _currentCallMethod->classOfMethod(), (int32_t)_currentCallMethod->virtualCallSelector(), cpIndex,
                    _currentCallMethod, resolvedSymbol, isIndirectCall, isInterface, *_newBCInfo, comp(),
                    _recursionDepth, allconsts, retainedMethods, wasRefinedFromKnownObject);

        } else {
            callsite = new (comp()->trHeapMemory()) TR_DirectCallSite(_calltarget->_calleeMethod, callNodeTreeTop,
                parent, callNode, interfaceMethod, _currentCallMethod->classOfMethod(), -1, cpIndex, _currentCallMethod,
                resolvedSymbol, isIndirectCall, isInterface, *_newBCInfo, comp(), _recursionDepth, allconsts,
                retainedMethods, wasRefinedFromKnownObject);
        }

        if (tracer()->debugLevel())
            _pca.printIndexes(comp());
        findTargetAndUpdateInfoForCallsite(callsite);
    }
}

void InterpreterEmulator::visitInvokespecial()
{
    int32_t cpIndex = next2Bytes();
    bool isUnresolvedInCP;
    _currentCallMethod = _calltarget->_calleeMethod->getResolvedSpecialMethod(comp(),
        (current() == J9BCinvokespecialsplit) ? cpIndex |= J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG : cpIndex,
        &isUnresolvedInCP);
    _currentCallMethodUnrefined = _currentCallMethod;
    if (isCurrentCallUnresolvedOrCold(_currentCallMethod, isUnresolvedInCP)) {
        debugUnresolvedOrCold(_currentCallMethod);
    } else {
        bool allconsts = false;
        heuristicTrace(tracer(), "numberOfExplicitParameters = %d  _pca.getNumPrevConstArgs = %d\n",
            _currentCallMethod->numberOfExplicitParameters(),
            _pca.getNumPrevConstArgs(_currentCallMethod->numberOfExplicitParameters()));
        if (_currentCallMethod->numberOfExplicitParameters() > 0
            && _currentCallMethod->numberOfExplicitParameters()
                <= _pca.getNumPrevConstArgs(_currentCallMethod->numberOfExplicitParameters()))
            allconsts = true;

        bool isIndirectCall = false;
        bool isInterface = false;
        TR::Method *interfaceMethod = 0;
        TR::TreeTop *callNodeTreeTop = 0;
        TR::Node *parent = 0;
        TR::Node *callNode = 0;
        TR::ResolvedMethodSymbol *resolvedSymbol = 0;
        OMR::RetainedMethodSet *retainedMethods = _calltarget->_retainedMethods;
        bool wasRefinedFromKnownObject = false;
        TR_CallSite *callsite = new (comp()->trHeapMemory()) TR_DirectCallSite(_calltarget->_calleeMethod,
            callNodeTreeTop, parent, callNode, interfaceMethod, _currentCallMethod->classOfMethod(), -1, cpIndex,
            _currentCallMethod, resolvedSymbol, isIndirectCall, isInterface, *_newBCInfo, comp(), _recursionDepth,
            allconsts, retainedMethods, wasRefinedFromKnownObject);
        findTargetAndUpdateInfoForCallsite(callsite);
    }
}

void InterpreterEmulator::visitInvokestatic()
{
    int32_t cpIndex = next2Bytes();
    bool isUnresolvedInCP;
    _currentCallMethod = _calltarget->_calleeMethod->getResolvedStaticMethod(comp(),
        (current() == J9BCinvokestaticsplit) ? cpIndex |= J9_STATIC_SPLIT_TABLE_INDEX_FLAG : cpIndex,
        &isUnresolvedInCP);
    _currentCallMethodUnrefined = _currentCallMethod;
    if (isCurrentCallUnresolvedOrCold(_currentCallMethod, isUnresolvedInCP)) {
        debugUnresolvedOrCold(_currentCallMethod);
    } else {
        bool allconsts = false;

        heuristicTrace(tracer(), "numberOfExplicitParameters = %d  _pca.getNumPrevConstArgs = %d\n",
            _currentCallMethod->numberOfExplicitParameters(),
            _pca.getNumPrevConstArgs(_currentCallMethod->numberOfExplicitParameters()));
        if (_currentCallMethod->numberOfExplicitParameters() > 0
            && _currentCallMethod->numberOfExplicitParameters()
                <= _pca.getNumPrevConstArgs(_currentCallMethod->numberOfExplicitParameters()))
            allconsts = true;

        TR::KnownObjectTable::Index mhIndex = TR::KnownObjectTable::UNKNOWN;
        TR::KnownObjectTable::Index mcsIndex = TR::KnownObjectTable::UNKNOWN;
        TR_OpaqueClassBlock *receiverClass = NULL;
        bool isIndirectCall = false;
        bool wasRefinedFromKnownObject = false;
        if (_iteratorWithState) {
            refineResolvedCalleeForInvokestatic(_currentCallMethod, mcsIndex, mhIndex, isIndirectCall, receiverClass);

            wasRefinedFromKnownObject = _currentCallMethod != _currentCallMethodUnrefined;
        }

        if (receiverClass == NULL)
            receiverClass = _currentCallMethod->classOfMethod();

        bool isInterface = false;
        TR_CallSite *callsite = NULL;
        TR::Method *interfaceMethod = 0;
        TR::TreeTop *callNodeTreeTop = 0;
        TR::Node *parent = 0;
        TR::Node *callNode = 0;
        TR::ResolvedMethodSymbol *resolvedSymbol = 0;
        OMR::RetainedMethodSet *retainedMethods = _calltarget->_retainedMethods;

        if (_currentCallMethod->convertToMethod()->isArchetypeSpecimen()
            && _currentCallMethod->getMethodHandleLocation() && mcsIndex == TR::KnownObjectTable::UNKNOWN) {
            callsite = new (comp()->trHeapMemory()) TR_J9MethodHandleCallSite(_calltarget->_calleeMethod,
                callNodeTreeTop, parent, callNode, interfaceMethod, receiverClass, -1, cpIndex, _currentCallMethod,
                resolvedSymbol, isIndirectCall, isInterface, *_newBCInfo, comp(), _recursionDepth, allconsts,
                retainedMethods, wasRefinedFromKnownObject);
        } else if (_currentCallMethod->convertToMethod()->isArchetypeSpecimen()
            && _currentCallMethod->getMethodHandleLocation() && mcsIndex != TR::KnownObjectTable::UNKNOWN) {
            TR_J9MutableCallSite *mcs = new (comp()->trHeapMemory())
                TR_J9MutableCallSite(_calltarget->_calleeMethod, callNodeTreeTop, parent, callNode, interfaceMethod,
                    receiverClass, -1, cpIndex, _currentCallMethod, resolvedSymbol, isIndirectCall, isInterface,
                    *_newBCInfo, comp(), _recursionDepth, allconsts, retainedMethods, wasRefinedFromKnownObject);
            if (mcsIndex != TR::KnownObjectTable::UNKNOWN) {
                if (comp()->getKnownObjectTable()) {
                    mcs->setMCS(mcsIndex);
                }
            }
            callsite = mcs;
        } else if (isIndirectCall) {
            int32_t noCPIndex = -1; // The method is not referenced via the constant pool
            callsite = new (comp()->trHeapMemory()) TR_J9VirtualCallSite(_calltarget->_calleeMethod, callNodeTreeTop,
                parent, callNode, interfaceMethod, receiverClass, (int32_t)_currentCallMethod->virtualCallSelector(),
                noCPIndex, _currentCallMethod, resolvedSymbol, isIndirectCall, isInterface, *_newBCInfo, comp(),
                _recursionDepth, allconsts, retainedMethods, wasRefinedFromKnownObject);
        } else {
            callsite = new (comp()->trHeapMemory())
                TR_DirectCallSite(_calltarget->_calleeMethod, callNodeTreeTop, parent, callNode, interfaceMethod,
                    receiverClass, -1, cpIndex, _currentCallMethod, resolvedSymbol, isIndirectCall, isInterface,
                    *_newBCInfo, comp(), _recursionDepth, allconsts, retainedMethods, wasRefinedFromKnownObject);
        }

        findTargetAndUpdateInfoForCallsite(callsite);
    }
}

void InterpreterEmulator::visitInvokeinterface()
{
    int32_t cpIndex = next2Bytes();
    auto calleeMethod = (TR_ResolvedJ9Method *)_calltarget->_calleeMethod;
    _currentCallMethod = calleeMethod->getResolvedImproperInterfaceMethod(comp(), cpIndex);
    _currentCallMethodUnrefined = _currentCallMethod;
    bool isIndirectCall = true;
    bool isInterface = true;
    if (_currentCallMethod) {
        isInterface = false;
        isIndirectCall = !_currentCallMethod->isPrivate() && !_currentCallMethod->convertToMethod()->isFinalInObject();
    }

    TR::Method *interfaceMethod = NULL;
    if (isInterface)
        interfaceMethod
            = comp()->fej9()->createMethod(this->trMemory(), _calltarget->_calleeMethod->containingClass(), cpIndex);

    TR::TreeTop *callNodeTreeTop = 0;
    TR::Node *parent = 0;
    TR::Node *callNode = 0;
    TR::ResolvedMethodSymbol *resolvedSymbol = 0;

    uint32_t explicitParams = 0;
    if (isInterface)
        explicitParams = interfaceMethod->numberOfExplicitParameters();
    else
        explicitParams = _currentCallMethod->numberOfExplicitParameters();

    bool allconsts = false;
    heuristicTrace(tracer(), "numberOfExplicitParameters = %d  _pca.getNumPrevConstArgs = %d\n", explicitParams,
        _pca.getNumPrevConstArgs(explicitParams));
    if (explicitParams > 0 && explicitParams <= _pca.getNumPrevConstArgs(explicitParams))
        allconsts = true;

    OMR::RetainedMethodSet *retainedMethods = _calltarget->_retainedMethods;
    bool wasRefinedFromKnownObject = false;
    TR_CallSite *callsite = NULL;
    if (isInterface) {
        TR_OpaqueClassBlock *thisClass = NULL;
        callsite = new (comp()->trHeapMemory())
            TR_J9InterfaceCallSite(_calltarget->_calleeMethod, callNodeTreeTop, parent, callNode, interfaceMethod,
                thisClass, -1, cpIndex, _currentCallMethod, resolvedSymbol, isIndirectCall, isInterface, *_newBCInfo,
                comp(), _recursionDepth, allconsts, retainedMethods, wasRefinedFromKnownObject);
    } else if (isIndirectCall) {
        callsite = new (comp()->trHeapMemory())
            TR_J9VirtualCallSite(_calltarget->_calleeMethod, callNodeTreeTop, parent, callNode, interfaceMethod,
                _currentCallMethod->classOfMethod(), (int32_t)_currentCallMethod->virtualCallSelector(), cpIndex,
                _currentCallMethod, resolvedSymbol, isIndirectCall, isInterface, *_newBCInfo, comp(), _recursionDepth,
                allconsts, retainedMethods, wasRefinedFromKnownObject);
    } else {
        callsite = new (comp()->trHeapMemory()) TR_DirectCallSite(_calltarget->_calleeMethod, callNodeTreeTop, parent,
            callNode, interfaceMethod, _currentCallMethod->classOfMethod(), -1, cpIndex, _currentCallMethod,
            resolvedSymbol, isIndirectCall, isInterface, *_newBCInfo, comp(), _recursionDepth, allconsts,
            retainedMethods, wasRefinedFromKnownObject);
    }

    if (tracer()->debugLevel()) {
        _pca.printIndexes(comp());
    }
    findTargetAndUpdateInfoForCallsite(callsite);
}

Operand *InterpreterEmulator::createOperandFromPrexArg(TR_PrexArgument *prexArgument)
{
    auto prexKnowledge = TR_PrexArgument::knowledgeLevel(prexArgument);
    switch (prexKnowledge) {
        case KNOWN_OBJECT:
            return knownObjOperand(prexArgument->getKnownObjectIndex(), prexArgument->getClass());
        case FIXED_CLASS:
            return new (trStackMemory()) FixedClassOperand(prexArgument->getClass());
        case PREEXISTENT:
            return new (trStackMemory()) PreexistentObjectOperand(prexArgument->getClass());
        case NONE:
            return prexArgument->getClass() ? new (trStackMemory()) ObjectOperand(prexArgument->getClass()) : NULL;
    }
    return NULL;
}

TR_PrexArgument *InterpreterEmulator::createPrexArgFromOperand(Operand *operand)
{
    if (operand->asKnownObject()) {
        auto koi = operand->getKnownObjectIndex();
        auto knot = comp()->getOrCreateKnownObjectTable();
        if (knot)
            return new (comp()->trHeapMemory()) TR_PrexArgument(operand->getKnownObjectIndex(), comp());
    } else if (operand->asObjectOperand() && operand->asObjectOperand()->getClass()) {
        TR_OpaqueClassBlock *clazz = operand->asObjectOperand()->getClass();
        TR_PrexArgument::ClassKind kind = TR_PrexArgument::ClassIsUnknown;
        if (operand->asFixedClassOperand())
            kind = TR_PrexArgument::ClassIsFixed;
        else if (operand->asPreexistentObjectOperand())
            kind = TR_PrexArgument::ClassIsPreexistent;

        return new (comp()->trHeapMemory()) TR_PrexArgument(kind, clazz);
    }

    return NULL;
}

TR_PrexArgInfo *InterpreterEmulator::computePrexInfo(TR_CallSite *callsite, TR::KnownObjectTable::Index appendix)
{
    if (tracer()->heuristicLevel())
        _ecs->getInliner()->tracer()->dumpCallSite(callsite, "Compute prex info for call site %p\n", callsite);

    int32_t numOfArgs = 0;
    if (callsite->_isInterface) {
        numOfArgs = callsite->_interfaceMethod->numberOfExplicitParameters() + 1;
    } else if (callsite->_initialCalleeMethod) {
        numOfArgs = callsite->_initialCalleeMethod->numberOfParameters();
    }

    if (numOfArgs == 0)
        return NULL;

    // Always favor prex arg from operand if we're iterating with state
    // But not for thunk archetype as the method's bytecodes manipulate
    // the operand stack differently, and one int `argPlacehowler`
    // argument can represent more than one arguments
    //
    if (!_callerIsThunkArchetype && _iteratorWithState) {
        TR_PrexArgInfo *prexArgInfo = new (comp()->trHeapMemory()) TR_PrexArgInfo(numOfArgs, comp()->trMemory());
        for (int32_t i = 0; i < numOfArgs; i++) {
            int32_t posInStack = numOfArgs - i - 1;
            prexArgInfo->set(i, createPrexArgFromOperand(topn(posInStack)));
        }

        if (tracer()->heuristicLevel()) {
            alwaysTrace(tracer(), "argInfo from operand stack:");
            prexArgInfo->dumpTrace();
        }
        return prexArgInfo;
    } else if (_wasPeekingSuccessfull) {
        auto callNodeTT = TR_PrexArgInfo::getCallTree(_methodSymbol, callsite, tracer());
        if (callNodeTT) {
            // Temporarily set call tree and call node of callsite such that computePrexInfo can use it
            callsite->_callNodeTreeTop = callNodeTT;
            callsite->_callNode = callNodeTT->getNode()->getChild(0);
            auto prexArgInfo
                = TR_J9InlinerUtil::computePrexInfo(_ecs->getInliner(), callsite, _calltarget->_ecsPrexArgInfo);

            // Reset call tree and call node
            callsite->_callNodeTreeTop = NULL;
            callsite->_callNode = NULL;
            return prexArgInfo;
        }
    }
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
    else if (appendix != TR::KnownObjectTable::UNKNOWN) {
        TR_ASSERT_FATAL(!callsite->isIndirectCall(), "appendix with indirect call");
        TR_ASSERT_FATAL(comp()->fej9()->isLambdaFormGeneratedMethod(callsite->_initialCalleeMethod),
            "appendix with non-LambdaForm method - expected a call site adapter");

        // Since the appendix is described not by an Operand but only by a known
        // object index, the null index can occur here.
        TR::KnownObjectTable *knot = comp()->getKnownObjectTable();
        if (!knot->isNull(appendix)) {
            TR_PrexArgInfo *prexArgInfo = new (comp()->trHeapMemory()) TR_PrexArgInfo(numOfArgs, comp()->trMemory());

            auto arg = new (comp()->trHeapMemory()) TR_PrexArgument(appendix, comp());
            prexArgInfo->set(numOfArgs - 1, arg);

            if (tracer()->heuristicLevel()) {
                alwaysTrace(tracer(), "argInfo from appendix object:");
                prexArgInfo->dumpTrace();
            }

            return prexArgInfo;
        }
    }
#endif

    return NULL;
}

void InterpreterEmulator::findTargetAndUpdateInfoForCallsite(TR_CallSite *callsite,
    TR::KnownObjectTable::Index appendix)
{
    _currentCallSite = callsite;
    callsite->_callerBlock = _currentInlinedBlock;
    callsite->_ecsPrexArgInfo = computePrexInfo(callsite, appendix);

    if (_ecs->isInlineable(_callStack, callsite)) {
        _callSites[_bcIndex] = callsite;
        _inlineableCallExists = true;

        if (!_currentInlinedBlock->isCold())
            _nonColdCallExists = true;

        for (int i = 0; i < callsite->numTargets(); i++)
            callsite->getTarget(i)->_originatingBlock = _currentInlinedBlock;
    } else {
        // support counters
        _calltarget->addDeadCallee(callsite);
    }
}

void InterpreterEmulator::assertHasState() { TR_ASSERT_FATAL(_iteratorWithState, "expected iteration with state"); }

bool InterpreterEmulator::isEdgeUnreachable(TR::CFGEdge *edge) { return _unreachableEdges.count(edge) != 0; }

bool InterpreterEmulator::isBlockUnreachable(TR::Block *block) { return _blockRc[block] == 0; }

void InterpreterEmulator::markSuccessorsUnreachable(const TR::CFGEdgeList &edges)
{
    assertHasState();
    auto end = edges.end();
    for (auto it = edges.begin(); it != end; it++) {
        TR::CFGEdge *edge = *it;
        if (_outEdgesStillReachable.count(edge) == 0)
            markEdgeUnreachable(edge);
    }
}

void InterpreterEmulator::markEdgeUnreachable(TR::CFGEdge *edge)
{
    TR::Block *src = toBlock(edge->getFrom());
    TR::Block *dest = toBlock(edge->getTo());
    debugTrace(tracer(), "%sedge %d (bci +%d) -> %d (bci +%d) is unreachable", dest->isCatchBlock() ? "exception " : "",
        src->getNumber(), src->getEntry()->getNode()->getByteCodeIndex(), dest->getNumber(),
        dest->getEntry()->getNode()->getByteCodeIndex());

    bool newlyAdded = _unreachableEdges.insert(edge).second;
    TR_ASSERT_FATAL(newlyAdded, ".."); // TODO: message

    uint32_t &rc = _blockRc[dest];
    TR_ASSERT_FATAL(rc != 0, "dest block %d already unreachable", dest->getNumber());
    rc--;
    if (rc == 0)
        _potentialCycleBlocks.remove(dest);
    else
        _potentialCycleBlocks.add(dest);
}

void InterpreterEmulator::markEdgeUnreachable(int32_t destBcIndex)
{
    markEdgeUnreachable(_currentInlinedBlock->getEdge(blocks(destBcIndex)));
}

void InterpreterEmulator::markSweepCFG()
{
    TR::BlockChecklist reachable(comp());
    TR::list<TR::Block *, TR::Region &> queue(comp()->trMemory()->currentStackRegion());

    TR::AllBlockIterator rootIt(_cfg, comp());
    for (; rootIt.currentBlock() != NULL; rootIt.stepForward()) {
        TR::Block *block = rootIt.currentBlock();
        if (_visitedBlocks.contains(block)) {
            reachable.add(block);
            queue.push_back(block);
        }
    }

    TR::Block *startBlock = toBlock(_cfg->getStart());
    if (!reachable.contains(startBlock)) {
        reachable.add(startBlock);
        queue.push_back(startBlock);
    }

    while (!queue.empty()) {
        TR::Block *block = queue.front();
        queue.pop_front();

        TR_SuccessorIterator succs(block);
        for (auto *succ = succs.getFirst(); succ != NULL; succ = succs.getNext()) {
            if (isEdgeUnreachable(succ))
                continue;

            TR::Block *dest = toBlock(succ->getTo());
            if (!reachable.contains(dest)) {
                reachable.add(dest);
                queue.push_back(dest);
            }
        }
    }

    TR::AllBlockIterator sweepIt(_cfg, comp());
    for (; sweepIt.currentBlock() != NULL; sweepIt.stepForward()) {
        TR::Block *block = sweepIt.currentBlock();
        if (reachable.contains(block))
            continue;

        // block is unreachable
        uint32_t &rc = _blockRc[block];
        if (rc == 0)
            continue; // already known to be unreachable

        rc = 0;

        debugTrace(tracer(), "CFG mark/sweep: block %d is unreachable", block->getNumber());

        TR_PredecessorIterator preds(block);
        for (auto *edge = preds.getFirst(); edge != NULL; edge = preds.getNext())
            _unreachableEdges.insert(edge); // might already be known to be unreachable

        TR_SuccessorIterator succs(block);
        for (auto *edge = succs.getFirst(); edge != NULL; edge = succs.getNext()) {
            if (reachable.contains(toBlock(edge->getTo())))
                markEdgeUnreachable(edge);
        }
    }

    _potentialCycleBlocks.clear();
}
