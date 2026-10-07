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
#include "infra/Checklist.hpp"
#include "infra/String.hpp"
#include "optimizer/Inliner.hpp"
#include "optimizer/J9EstimateCodeSize.hpp"
#include "optimizer/J9Inliner.hpp"
#include <set>

class IconstOperand;
class KnownObjOperand;
class MutableCallsiteTargetOperand;
class FixedClassOperand;
class PreexistentObjectOperand;
class ObjectOperand;
class TR_PrexArgument;

/*
 * \class Operand
 *
 * \brief represent an operand on the operand stack
 *
 * \note this is the most general operand which doesn't carry any specific information.
 */
class Operand {
public:
    TR_ALLOC(TR_Memory::EstimateCodeSize);

    enum KnowledgeLevel {
        NONE,
        OBJECT,
        MUTABLE_CALLSITE_TARGET,
        PREEXISTENT,
        FIXED_CLASS,
        KNOWN_OBJECT,
        ICONST,
        NULLREF
    };

    static const char *KnowledgeStrings[];

    virtual bool isNull() { return false; }

    virtual IconstOperand *asIconst() { return NULL; }

    virtual KnownObjOperand *asKnownObject() { return NULL; }

    virtual FixedClassOperand *asFixedClassOperand() { return NULL; }

    virtual PreexistentObjectOperand *asPreexistentObjectOperand() { return NULL; }

    virtual ObjectOperand *asObjectOperand() { return NULL; }

    virtual MutableCallsiteTargetOperand *asMutableCallsiteTargetOperand() { return NULL; }

    virtual TR::KnownObjectTable::Index getKnownObjectIndex() { return TR::KnownObjectTable::UNKNOWN; }

    virtual char *getSignature(TR::Compilation *comp, TR_Memory *trMemory) { return NULL; }

    virtual void printToString(TR::StringBuf *buf);

    virtual KnowledgeLevel getKnowledgeLevel() { return NONE; }

    Operand *merge(Operand *other);
    virtual Operand *merge1(Operand *other);
};

class NullOperand : public Operand {
public:
    TR_ALLOC(TR_Memory::EstimateCodeSize);

    NullOperand() {}

    virtual bool isNull() { return true; }

    virtual void printToString(TR::StringBuf *buf);

    virtual KnowledgeLevel getKnowledgeLevel() { return NULLREF; }

    virtual Operand *merge1(Operand *other);
};

class IconstOperand : public Operand {
public:
    TR_ALLOC(TR_Memory::EstimateCodeSize);

    IconstOperand(int x)
        : intValue(x)
    {}

    virtual IconstOperand *asIconst() { return this; }

    virtual void printToString(TR::StringBuf *buf);
    int32_t intValue;

    virtual KnowledgeLevel getKnowledgeLevel() { return ICONST; }

    virtual Operand *merge1(Operand *other);
};

/*
 * \class ObjectOperand
 *
 * \brief Represent a java object
 */
class ObjectOperand : public Operand {
public:
    TR_ALLOC(TR_Memory::EstimateCodeSize);

    ObjectOperand(TR_OpaqueClassBlock *clazz = NULL)
        : _signature(NULL)
        , _clazz(clazz)
    {}

    virtual char *getSignature(TR::Compilation *comp, TR_Memory *trMemory);

    virtual ObjectOperand *asObjectOperand() { return this; }

    virtual TR_OpaqueClassBlock *getClass() { return _clazz; }

    virtual KnowledgeLevel getKnowledgeLevel() { return OBJECT; }

    virtual Operand *merge1(Operand *other);
    virtual void printToString(TR::StringBuf *buf);

protected:
    char *_signature;
    TR_OpaqueClassBlock *_clazz;
};

/*
 * \class PreexistentObjectOperand
 *
 * \brief Represent an object that preexist before the compiled method, i.e. it is
 *        a parm of the compiled method.
 */
class PreexistentObjectOperand : public ObjectOperand {
public:
    TR_ALLOC(TR_Memory::EstimateCodeSize);

    PreexistentObjectOperand(TR_OpaqueClassBlock *clazz)
        : ObjectOperand(clazz)
    {}

    virtual PreexistentObjectOperand *asPreexistentObjectOperand() { return this; }

    virtual KnowledgeLevel getKnowledgeLevel() { return PREEXISTENT; }

    virtual Operand *merge1(Operand *other);
};

/*
 * \class FixedClassOperand
 *
 * \brief An object with known type
 */
class FixedClassOperand : public ObjectOperand {
public:
    TR_ALLOC(TR_Memory::EstimateCodeSize);

    FixedClassOperand(TR_OpaqueClassBlock *clazz)
        : ObjectOperand(clazz)
    {}

    virtual FixedClassOperand *asFixedClassOperand() { return this; }

    virtual KnowledgeLevel getKnowledgeLevel() { return FIXED_CLASS; }

    virtual Operand *merge1(Operand *other);
};

/*
 * \class KnownObjOperand
 *
 * \brief Represent an object with known identity at compile time
 */
class KnownObjOperand : public FixedClassOperand {
public:
    TR_ALLOC(TR_Memory::EstimateCodeSize);
    KnownObjOperand(TR::KnownObjectTable *knot, TR::KnownObjectTable::Index koi, TR_OpaqueClassBlock *clazz);

    virtual KnownObjOperand *asKnownObject() { return this; }

    virtual TR::KnownObjectTable::Index getKnownObjectIndex() { return knownObjIndex; }

    virtual KnowledgeLevel getKnowledgeLevel() { return KNOWN_OBJECT; }

    virtual Operand *merge1(Operand *other);
    virtual void printToString(TR::StringBuf *buf);

private:
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
 * \see getReturnValue
 * \see refineResolvedCalleeForInvokestatic
 * \see visitInvokestatic
 */
class MutableCallsiteTargetOperand : public ObjectOperand {
public:
    TR_ALLOC(TR_Memory::EstimateCodeSize);

    MutableCallsiteTargetOperand(TR::KnownObjectTable::Index methodHandleIndex,
        TR::KnownObjectTable::Index mutableCallsiteIndex)
        : methodHandleIndex(methodHandleIndex)
        , mutableCallsiteIndex(mutableCallsiteIndex)
    {}

    virtual MutableCallsiteTargetOperand *asMutableCallsiteTargetOperand() { return this; }

    virtual Operand *merge1(Operand *other);
    virtual void printToString(TR::StringBuf *buf);

    virtual KnowledgeLevel getKnowledgeLevel() { return MUTABLE_CALLSITE_TARGET; }

    TR::KnownObjectTable::Index getMethodHandleIndex() { return methodHandleIndex; }

    TR::KnownObjectTable::Index getMutableCallsiteIndex() { return mutableCallsiteIndex; }

    TR::KnownObjectTable::Index mutableCallsiteIndex;
    TR::KnownObjectTable::Index methodHandleIndex;
};

class InterpreterEmulator
    : public TR_ByteCodeIteratorWithState<TR_J9ByteCode, J9BCunknown, TR_J9ByteCodeIterator, Operand *> {
    typedef TR_ByteCodeIteratorWithState<TR_J9ByteCode, J9BCunknown, TR_J9ByteCodeIterator, Operand *> Base;

public:
    InterpreterEmulator(TR_CallTarget *calltarget, TR::ResolvedMethodSymbol *methodSymbol, TR_J9VMBase *fe,
        TR::Compilation *comp, TR_LogTracer *tracer, TR_EstimateCodeSize *ecs)
        : Base(methodSymbol, comp)
        , _calltarget(calltarget)
        , _tracer(tracer)
        , _ecs(ecs)
        , _iteratorWithState(false)
        , _currentBcCanFallThrough(true)
        , _unreachableEdges(std::less<TR::CFGEdge *>(), comp->trMemory()->currentStackRegion())
        , _outEdgesStillReachable(std::less<TR::CFGEdge *>(), comp->trMemory()->currentStackRegion())
        , _blockRc(std::less<TR::Block *>(), comp->trMemory()->currentStackRegion())
        , _potentialCycleBlocks(comp)
        , _visitedBlocks(comp)
    {
        TR_J9ByteCodeIterator::initialize(static_cast<TR_ResolvedJ9Method *>(methodSymbol->getResolvedMethod()), fe);
        _flags = NULL;
        _stacks = NULL;
        _currentLocalObjectInfo = NULL;
        _localObjectInfos = NULL;
        _currentCallSite = NULL;
        _currentCallMethod = NULL;
        _currentCallMethodUnrefined = NULL;
        _numSlots = 0;
        _callerIsThunkArchetype = _calltarget->_calleeMethod->convertToMethod()->isArchetypeSpecimen();

        _operandBuf = NULL;
        if (_tracer->heuristicLevel() || _tracer->debugLevel()) {
            TR::Region &stackRegion = comp->trMemory()->currentStackRegion();
            _operandBuf = new (stackRegion) TR::StringBuf(stackRegion);
        }
    }

    TR_LogTracer *tracer() { return _tracer; }

    /* \brief Initialize data needed for looking for callsites
     *
     * \param blocks
     *       blocks generated from bytecodes
     *
     * \param flags
     *       flags with bits to indicate property of each bytecode. The flags are set by \ref
     * TR_J9EstimateCodeSize::processBytecodeAndGenerateCFG.
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
    void prepareToFindAndCreateCallsites(TR::Block **blocks, flags8_t *flags, TR_CallSite **callSites, TR::CFG *cfg,
        TR_ByteCodeInfo *newBCInfo, int32_t recursionDepth, TR_CallStack *callstack);

    /*
     * \brief look for calls in bytecodes and create callsites
     *
     * \param wasPeekingSuccessfull
     *       indicate whether trees has been generated by peeking ilgen
     *
     * \param withState
     *       whether the bytecode iteration should be with or without state
     *
     * \return whether callsites are created successfully. Return false if failed for reasons like unexpected bytecodes
     * etc.
     */
    bool findAndCreateCallsitesFromBytecodes(bool wasPeekingSuccessfull, bool withState);

    void setBlocks(TR::Block **blocks) { _blocks = blocks; }

    TR_StackMemory trStackMemory() { return _trMemory; }

    /*
     * \brief Compute prex arg info for the given call site
     */
    TR_PrexArgInfo *computePrexInfo(TR_CallSite *callsite, TR::KnownObjectTable::Index appendix);
    TR_PrexArgument *createPrexArgFromOperand(Operand *operand);
    Operand *createOperandFromPrexArg(TR_PrexArgument *arg);

    bool _nonColdCallExists;
    bool _inlineableCallExists;

    enum BytecodePropertyFlag {
        bbStart = 0x01, // whether the current bytecode is at bbstart
        isCold = 0x02, // whther the bytecode is on a cold path
        isBranch = 0x04, // whther the bytecode is a branch
        isUnsanitizeable = 0x08,
    };

private:
    // the following methods can only be called when the iterator has state

    /**
     * \brief Determine whether to maintain state during iteration.
     * \return true if state should be used
     */
    bool shouldIterateWithState();

    /*
     * Initialize the data structures needed for iterator with state
     */
    void initializeIteratorWithState();

    void assertHasState();

    /*
     * push and pop operands on stack according to given bytecode
     *
     * \return false if some error occurred such as unexpected bytecodes.
     */
    bool maintainStack(TR_J9ByteCode bc);
    void maintainStackForIf(TR_J9ByteCode bc);
    void maintainStackForTableSwitch();
    void maintainStackForGetField();
    void maintainStackForArraylength();
    void maintainStackForArrayLoad(TR::DataTypes type, TR_J9ByteCode bc);
    Operand *foldArrayLoad(Operand *obj, IconstOperand *indexOperand, TR::DataTypes type, TR_J9ByteCode bc);
    void maintainStackForAload(int slotIndex);
    void maintainStackForReturn();
    /*
     * \brief helper to pop arguments from the stack and push the result for calls
     *
     * \param numArgs
     *    Number of arguments of the call
     *
     * \param result
     *    the operand reprenting the call return value
     *
     * \param returnType
     *    Return type of the call
     */
    void maintainStackForCall(Operand *result, int32_t numArgs, TR::DataType returnType);
    /*
     * \brief helper to pop arguments from the stack and push the result for calls
     */
    void maintainStackForCall();
    /*
     * \brief refine invokestatic callee method based on operands when possible
     */
    void refineResolvedCalleeForInvokestatic(TR_ResolvedMethod *&callee, TR::KnownObjectTable::Index &mcsIndex,
        TR::KnownObjectTable::Index &mhIndex, bool &isIndirectCall, TR_OpaqueClassBlock *&receiverClass);
    /*
     * \brief refine invokevirtual callee method based on operands when possible
     */
    void refineResolvedCalleeForInvokevirtual(TR_ResolvedMethod *&callee, bool &isIndirectCall);
    /*
     * \brief Compute result of the given call based on operand stack state
     */
    Operand *getReturnValue(TR_ResolvedMethod *callee);
    void dumpStack();

    void pushUnknownOperand() { Base::push(_unknownOperand); }

    Operand *knownObjOperand(TR::KnownObjectTable::Index i, TR_OpaqueClassBlock *clazz = NULL);

    // doesn't need to handle execeptions yet as they don't exist in method handle thunk archetypes
    virtual void findAndMarkExceptionRanges() {}

    /*
     * \brief Propagte state state and local variable state to next target
     */
    virtual void saveStack(int32_t targetIndex);

    // the following methods can be used in both stateless and with state mode

    /*
     * \brief tell whether the given bcIndex has been generated.
     *
     * \note This query is used to avoid regenerating bytecodes which shouldn't happen at stateless mode
     */
    bool isGenerated(int32_t bcIndex) { return _iteratorWithState ? Base::isGenerated(bcIndex) : false; }

    /*
     * \brief Set up operand stack and local slot array for block starting at given bytecode index
     *
     * \param index
     *    Bytecode index at the block entry
     */
    virtual int32_t setupBBStartContext(int32_t index);
    /*
     * \brief set up object info in local slots at entry of a block
     *
     * \param index
     *    Bytecode index at the block entry
     */
    void setupBBStartLocalObjectState(int32_t index);
    /*
     * \brief set up object info in local slots for the method entry
     */
    void setupMethodEntryLocalObjectState();
    /*
     * \brief set up operand stack state at entry of a block
     *
     * \param index
     *    Bytecode index at the block entry
     */
    void setupBBStartStackState(int32_t index);

    void visitInvokedynamic();
    void visitInvokevirtual();
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
    void visitInvokehandle();
    void updateKnotAndCreateCallSiteUsingInvokeCacheArray(TR_ResolvedJ9Method *owningMethod,
        uintptr_t *invokeCacheArray, int32_t cpIndex);
#endif
    void visitInvokespecial();
    void visitInvokestatic();
    void visitInvokeinterface();
    void findTargetAndUpdateInfoForCallsite(TR_CallSite *callsite,
        TR::KnownObjectTable::Index appendix = TR::KnownObjectTable::UNKNOWN);
    bool isCurrentCallUnresolvedOrCold(TR_ResolvedMethod *resolvedMethod, bool isUnresolvedInCP);
    void debugUnresolvedOrCold(TR_ResolvedMethod *resolvedMethod);
    void maintainStackForAstore(int slotIndex);
    void maintainStackForldc(int32_t cpIndex);
    void maintainStackForNew(int32_t cpIndex);
    void maintainStackForInstanceof(int32_t cpIndex);
    void maintainStackForGetStatic();
    /*
     * \brief Check if a block has predecessors whose bytecodes haven't been visited
     */
    bool hasUnvisitedPred(TR::Block *block);

    bool hasVisitedPred(TR::Block *block);

    bool isEdgeUnreachable(TR::CFGEdge *edge);
    bool isBlockUnreachable(TR::Block *block);
    void markSuccessorsUnreachable(const TR::CFGEdgeList &edges);
    void markEdgeUnreachable(TR::CFGEdge *edge);
    void markEdgeUnreachable(int32_t destBcIndex);
    void markSweepCFG();

    typedef TR_Array<Operand *> OperandArray;
    void printOperandArray(OperandArray *operands);
    /*
     * \brief Merge second operand into the first, does an intersect operand on every operand in the array
     */
    void mergeOperandArray(OperandArray *first, OperandArray *second);

    TR::RequiredConst &addRequiredConst(TR::AnyConst value);

    TR_LogTracer *_tracer;
    TR_EstimateCodeSize *_ecs;
    Operand *_unknownOperand; // used whenever the iterator can't reason about an operand
    NullOperand *_nullOperand; // represents a null reference - no need for multiple instances
    TR_CallTarget *_calltarget; // the target method to inline
    bool _iteratorWithState;
    bool _currentBcCanFallThrough;
    flags8_t *_InterpreterEmulatorFlags; // flags with bits to indicate property of each bytecode.
    TR_CallSite **_callSites;
    TR_CallSite *_currentCallSite; // Store created callsite if visiting invoke* bytecodes
    TR_ResolvedMethod *_currentCallMethod; // Resolved method for invoke* bytecodes, some calls won't have call site
                                           // created due to coldness info
    TR_ResolvedMethod *_currentCallMethodUnrefined; // Call method without any refinement applied to it
    TR::CFG *_cfg;
    TR_ByteCodeInfo *_newBCInfo;
    int32_t _recursionDepth;
    TR_CallStack *_callStack; // the call stack from the current inlined call target to the method being compiled
    bool _wasPeekingSuccessfull;
    TR::Block *_currentInlinedBlock;
    TR_prevArgs _pca;

    bool _callerIsThunkArchetype;
    // State of local object for current bytecode being visited
    OperandArray *_currentLocalObjectInfo;
    // Array of local object info arrays, indexed by bytecode index
    OperandArray **_localObjectInfos;
    // Number of local slots
    int32_t _numSlots;

    typedef TR::typed_allocator<TR::CFGEdge *, TR::Region &> EdgePtrAlloc;
    typedef std::set<TR::CFGEdge *, std::less<TR::CFGEdge *>, EdgePtrAlloc> EdgeSet;
    EdgeSet _unreachableEdges;
    EdgeSet _outEdgesStillReachable; // from current block when !_currentBcCanFallThrough

    typedef TR::typed_allocator<std::pair<TR::Block * const, uint32_t>, TR::Region &> BlockRcAlloc;
    typedef std::map<TR::Block *, uint32_t, std::less<TR::Block *>, BlockRcAlloc> BlockRcMap;
    BlockRcMap _blockRc; // number of potentially reachable incoming edges

    // Blocks whose refcount has dropped, but hasn't yet reached zero.
    //
    // If a cycle becomes unreachable (based on currently-known unreachable
    // edges), then at least one of its blocks will be in this set. Further,
    // if a proper loop becomes unreachable, exactly one of its blocks will
    // be in this set, and that block will be its loop header.
    //
    // This helps in determining when the CFG should be searched to detect
    // unreachable cycles.
    //
    TR::BlockChecklist _potentialCycleBlocks;

    // Blocks that have been processed already, including the one currently
    // being processed.
    //
    // These were considered to be potentially reachable when they were
    // encountered by the reverse postorder traversal. Operand values have
    // already been propagated along their outgoing edges, and some of their
    // successors may have already been processed as well, so there's little
    // benefit to recognizing after the fact that one of these blocks is
    // actually unreachable. As such, they will be assumed reachable. This
    // assumption helps to avoid unnecessary CFG searches.
    //
    TR::BlockChecklist _visitedBlocks;

    TR::StringBuf *_operandBuf; // for debug printing
};
#endif
