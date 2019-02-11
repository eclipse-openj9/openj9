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

#ifndef INLINERTEMPFORJ9_INCL
#define INLINERTEMPFORJ9_INCL

#include "j9cfg.h"
#include "runtime/J9ValueProfiler.hpp"

class OMR_InlinerPolicy;
class OMR_InlinerUtil;
class TR_InlinerBase;
class TR_J9InnerPreexistenceInfo;


/**
 * Class TR_MultipleCallTargetInliner
 * ==================================
 *
 * Exception Directed Optimization (EDO) enables more aggressive 
 * inlining that the JIT does in cases when the call graph of the 
 * callee being inlined has some throw statements that would get 
 * caught in a catch block of the caller.
 *
 * The end goal is to give the JIT optimizer (value propagation in
 * particular) the opportunity to convert a throw statement into a goto
 * statement in cases when it can be proven at compile time that the throw
 * would be caught by a specific catch block in the same compiled method.
 *
 * The advantage of this optimization is being able to avoid the throw/catch
 * flow of control that typically is implemented by some form of stack
 * walking in the JVM; stack walking can be quite an expensive operation
 * especially when compared with other simpler ways of transferring
 * flow of control, like a goto for example.
 *
 * Value propagation propagates type information as part of the analysis
 * and this means that it can determine if the exception thrown can in fact
 * be caught by a particular catch block or not based on the type of the
 * exception and the types that the catch block handles according to the
 * exception table information in the bytecodes.
 *
 * EDO depends on profiling throw statements and catch blocks, and
 * determining those operations that are frequently executed and then
 * attempting to bring the frequently executed throw and catch together
 * into the same compilation unit (method) by inlining aggressively.
 */

class TR_MultipleCallTargetInliner : public TR_InlinerBase
   {
   public:

      template <typename FunctObj>
      void recursivelyWalkCallTargetAndPerformAction(TR_CallTarget *ct, FunctObj &action);
      //void generateNodeEstimate(TR_CallTarget *ct, TR::Compilation *comp);

      class generateNodeEstimate
         {
         public:
            generateNodeEstimate() : _nodeEstimate(0){ }
            void operator()(TR_CallTarget *ct, TR::Compilation *comp);
            int32_t getNodeEstimate() { return _nodeEstimate; }
         private:
            int32_t _nodeEstimate;
         };

      TR_MultipleCallTargetInliner(TR::Optimizer *, TR::Optimization *);

      virtual bool inlineCallTargets(TR::ResolvedMethodSymbol *, TR_CallStack *, TR_InnerPreexistenceInfo *);
      virtual bool exceedsSizeThreshold(TR_CallSite *callSite, int bytecodeSize, TR::Block * callNodeBlock, TR_ByteCodeInfo & bcInfo, int32_t numLocals=0, TR_ResolvedMethod * caller = 0, TR_ResolvedMethod * calleeResolvedMethod = 0, TR::Node * callNode = 0, bool allConsts = false);

      TR_LinkHead<TR_CallTarget> _callTargets;

   protected:
      virtual int32_t scaleSizeBasedOnBlockFrequency(int32_t bytecodeSize, int32_t frequency, int32_t borderFrequency, TR_ResolvedMethod * calleeResolvedMethod, TR::Node *callNode, int32_t coldBorderFrequency = 0);
      float getScalingFactor(float factor);

   private:


      bool analyzeCallSite(TR::ResolvedMethodSymbol *, TR_CallStack *, TR::TreeTop *, TR::Node *, TR::Node *);
      void weighCallSite( TR_CallStack * callStack , TR_CallSite *callsite, bool currentBlockHasExceptionSuccessors,bool dontAddCalls=false);

      int32_t applyArgumentHeuristics(TR_LinkHead<TR_ParameterMapping> &map, int32_t originalWeight, TR_CallTarget *target);
      bool eliminateTailRecursion(TR::ResolvedMethodSymbol *, TR_CallStack *, TR::TreeTop *, TR::Node *, TR::Node *, TR_VirtualGuardSelection *);
      void assignArgumentsToParameters(TR::ResolvedMethodSymbol *, TR::TreeTop *, TR::Node *);
      bool isLargeCompiledMethod(TR_ResolvedMethod *calleeResolvedMethod, int32_t bytecodeSize, int32_t freq);

   protected:
      virtual bool supportsMultipleTargetInlining () { return true ; }

      void walkCallSites(TR::ResolvedMethodSymbol *, TR_CallStack *, TR_InnerPreexistenceInfo *, int32_t walkDepth);
      void walkCallSite( TR::ResolvedMethodSymbol * calleeSymbol, TR_CallStack * callStack,
                         TR::TreeTop * callNodeTreeTop, TR::Node * parent, TR::Node * callNode, TR_VirtualGuardSelection *guard,
                         TR_OpaqueClassBlock * thisClass, bool inlineNonRecursively, int32_t walkDepth);

     //private fields

   };

class TR_J9InlinerUtil: public OMR_InlinerUtil
   {
   friend class TR_InlinerBase;
   friend class TR_MultipleCallTargetInliner;
   public:
      TR_J9InlinerUtil(TR::Compilation *comp);
      virtual void adjustByteCodeSize(TR_ResolvedMethod *calleeResolvedMethod, bool isInLoop, TR::Block *block, int &bytecodeSize);
      virtual void adjustCallerWeightLimit(TR::ResolvedMethodSymbol *callSymbol, int &callerWeightLimit);
      virtual void adjustMethodByteCodeSizeThreshold(TR::ResolvedMethodSymbol *callSymbol, int &methodByteCodeSizeThreshold);
      virtual bool addTargetIfMethodIsNotOverridenInReceiversHierarchy(TR_IndirectCallSite *callsite);
      virtual bool addTargetIfThereIsSingleImplementer (TR_IndirectCallSite *callsite);
      virtual TR_ResolvedMethod *findSingleJittedImplementer(TR_IndirectCallSite *callsite);
      virtual TR_PrexArgInfo* createPrexArgInfoForCallTarget(TR_VirtualGuardSelection *guard, TR_ResolvedMethod *implementer);
      virtual TR_InlinerTracer * getInlinerTracer(TR::Optimization *optimization);
      virtual TR_PrexArgInfo *computePrexInfo(TR_CallTarget *target);
      virtual void refineInlineGuard(TR::Node *callNode, TR::Block *&block1, TR::Block *&block2,
                   bool &appendTestToBlock1, TR::ResolvedMethodSymbol * callerSymbol, TR::TreeTop *cursorTree,
                   TR::TreeTop *&virtualGuard, TR::Block *block4);
      virtual void refineInliningThresholds(TR::Compilation *comp, int32_t &callerWeightLimit, int32_t &maxRecursiveCallByteCodeSizeEstimate, int32_t &methodByteCodeSizeThreshold, int32_t &methodInWarmBlockByteCodeSizeThreshold, int32_t &methodInColdBlockByteCodeSizeThreshold, int32_t &nodeCountThreshold, int32_t size);
      static void checkForConstClass(TR_CallTarget *target, TR_InlinerTracer *tracer);
      virtual bool needTargetedInlining(TR::ResolvedMethodSymbol *callee);
   protected:
      virtual void refineColdness (TR::Node* node, bool& isCold);
      virtual void computeMethodBranchProfileInfo (TR::Block * cfgBlock, TR_CallTarget* calltarget, TR::ResolvedMethodSymbol* callerSymbol);
      virtual int32_t getCallCount(TR::Node *callNode);
      virtual TR_InnerPreexistenceInfo *createInnerPrexInfo(TR::Compilation * c, TR::ResolvedMethodSymbol *methodSymbol, TR_CallStack *callStack, TR::TreeTop *callTree, TR::Node *callNode, TR_VirtualGuardKind guardKind);
      virtual void estimateAndRefineBytecodeSize(TR_CallSite* callsite, TR_CallTarget* target, TR_CallStack *callStack, int32_t &bytecodeSize);
      virtual TR_TransformInlinedFunction *getTransformInlinedFunction(TR::ResolvedMethodSymbol *, TR::ResolvedMethodSymbol *, TR::Block *, TR::TreeTop *,
                                  TR::Node *, TR_ParameterToArgumentMapper &, TR_VirtualGuardSelection *, List<TR::SymbolReference> &,
                                  List<TR::SymbolReference> &, List<TR::SymbolReference> &);
   };

class TR_J9InlinerPolicy : public OMR_InlinerPolicy
   {
   friend class TR_J9InlinerUtil;
   friend class TR_InlinerBase;
   friend class TR_MultipleCallTargetInliner;
   public:
      TR_J9InlinerPolicy(TR::Compilation *comp);
      virtual bool inlineRecognizedMethod(TR::RecognizedMethod method);
      virtual bool tryToInlineTrivialMethod (TR_CallStack* callStack, TR_CallTarget* calltarget);
      static bool isInlineableJNI(TR_ResolvedMethod *method,TR::Node *callNode);
      virtual bool alwaysWorthInlining(TR_ResolvedMethod * calleeMethod, TR::Node *callNode);
      bool adjustFanInSizeInExceedsSizeThreshold(int bytecodeSize,
                                                      uint32_t& calculatedSize,
                                                      TR_ResolvedMethod* callee,
                                                      TR_ResolvedMethod* caller,
                                                      int32_t bcIndex);
      void adjustFanInSizeInWeighCallSite(int32_t& weight,
                                                int32_t size,
                                                TR_ResolvedMethod* callee,
                                                TR_ResolvedMethod* caller,
                                                int32_t bcIndex);
      virtual bool aggressivelyInlineInLoops();
      virtual void determineInliningHeuristic(TR::ResolvedMethodSymbol *callerSymbol);
      virtual void determineAggressionInLoops(TR::ResolvedMethodSymbol *callerSymbol);
      virtual int32_t getInitialBytecodeSize(TR_ResolvedMethod *feMethod, TR::ResolvedMethodSymbol * methodSymbol, TR::Compilation *comp);
      virtual bool tryToInline(TR_CallTarget *, TR_CallStack *, bool);
      virtual bool inlineMethodEvenForColdBlocks(TR_ResolvedMethod *method);
      virtual bool willBeInlinedInCodeGen(TR::RecognizedMethod method);
      virtual bool canInlineMethodWhileInstrumenting(TR_ResolvedMethod *method);
      virtual bool skipHCRGuardForCallee(TR_ResolvedMethod* callee);
      virtual bool dontPrivatizeArgumentsForRecognizedMethod(TR::RecognizedMethod recognizedMethod);
      virtual bool replaceSoftwareCheckWithHardwareCheck(TR_ResolvedMethod *calleeMethod);
      virtual bool suitableForRemat(TR::Compilation *comp, TR::Node *callNode, TR_VirtualGuardSelection *guard);

#ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION
      virtual bool willInlineCryptoMethodInCodeGen(TR::RecognizedMethod method);
#endif

   protected:
      bool _aggressivelyInlineInLoops;
      void createTempsForUnsafeCall( TR::TreeTop *callNodeTreeTop, TR::Node * unsafeCallNode );
      TR::Node *   inlineGetClassAccessFlags(TR::ResolvedMethodSymbol *, TR::ResolvedMethodSymbol *, TR::TreeTop *, TR::Node *);
      bool         inlineUnsafeCall(TR::ResolvedMethodSymbol *, TR::ResolvedMethodSymbol *, TR::TreeTop *, TR::Node *);
      TR::Block * addNullCheckForUnsafeGetPut(TR::Node* unsafeAddress, TR::SymbolReference* newSymbolReferenceForAddress, TR::TreeTop* callNodeTreeTop, TR::TreeTop* directAccessTreeTop, TR::TreeTop* arrayDirectAccessTreeTop, TR::TreeTop* indirectAccessTreeTop);
      bool createUnsafePutWithOffset(TR::ResolvedMethodSymbol *, TR::ResolvedMethodSymbol *, TR::TreeTop *, TR::Node *, TR::DataType, bool, bool needNullCheck = false, bool isOrdered = false);
      TR::TreeTop* genDirectAccessCodeForUnsafeGetPut(TR::Node* callNode, bool conversionNeeded, bool isUnsafeGet);
      void createTempsForUnsafePutGet(TR::Node*& unsafeAddress, TR::Node* unsafeCall, TR::TreeTop* callNodeTreeTop, TR::Node*& offset, TR::SymbolReference*& newSymbolReferenceForAddress, bool isUnsafeGet);
      bool         createUnsafeGet(TR::ResolvedMethodSymbol *, TR::ResolvedMethodSymbol *, TR::TreeTop *, TR::Node *, TR::DataType, bool compress = true);
      bool         createUnsafePut(TR::ResolvedMethodSymbol *, TR::ResolvedMethodSymbol *, TR::TreeTop *, TR::Node *, TR::DataType, bool compress = true);
      TR::Node *    createUnsafeAddress(TR::Node *);
      bool         createUnsafeGetWithOffset(TR::ResolvedMethodSymbol *, TR::ResolvedMethodSymbol *, TR::TreeTop *, TR::Node *, TR::DataType, bool, bool needNullCheck = false);
      TR::Node *    createUnsafeAddressWithOffset(TR::Node *);
      bool         createUnsafeFence(TR::TreeTop *, TR::Node *, TR::ILOpCodes);

      TR::Node *    createUnsafeMonitorOp(TR::ResolvedMethodSymbol *calleeSymbol, TR::ResolvedMethodSymbol *callerSymbol, TR::TreeTop * callNodeTreeTop, TR::Node * unsafeCall, bool isEnter);
      bool         createUnsafeCASCallDiamond(TR::TreeTop *, TR::Node *);
      TR::TreeTop* genClassCheckForUnsafeGetPut(TR::Node* offset);
      TR::TreeTop* genClassCheckForUnsafeGetPut(TR::Node* offset, bool isNotLowTagged);

      /** \brief
       *     Generates the indirect access for an unsafe get/put operation given a direct access. This function will
       *     replicate the entire direct access tree passed in (see \p directAccessOrTempStoreNode) and update the
       *     symbol reference of the unsafe get/put operation to represent an indirect access.
       *
       *  \param directAccessOrTempStoreNode
       *     The direct access of the unsafe operation or a temp store of the direct access.
       *
       *  \param unsafeAddress
       *     The unsafe address to access.
       *
       *  \return
       *     The tree top representing the indirect access along with the potential temp store in case of a get
       *     operation.
       */
      TR::TreeTop* genIndirectAccessCodeForUnsafeGetPut(TR::Node* directAccessOrTempStoreNode, TR::Node* unsafeAddress);

      void createAnchorNodesForUnsafeGetPut(TR::TreeTop* treeTop, TR::DataType type, bool isUnsafeGet);
      TR::Node *     genCompressedRefs(TR::Node *, bool genTT = true, int32_t isLoad = 1);
      void genCodeForUnsafeGetPut(TR::Node* unsafeAddress, TR::TreeTop* callNodeTreeTop, TR::TreeTop* prevTreeTop, TR::SymbolReference* newSymbolReferenceForAddress, TR::TreeTop* directAccessTreeTop, TR::TreeTop* lowTagCmpTree, bool needNullCheck, bool isUnsafeGet, bool conversionNeeded, TR::Block * joinBlock, TR_OpaqueClassBlock *javaLangClass, TR::Node* orderedCallNode);
      virtual bool callMustBeInlined(TR_CallTarget *calltarget);
      bool mustBeInlinedEvenInDebug(TR_ResolvedMethod * calleeMethod, TR::TreeTop *callNodeTreeTop);
      bool _tryToGenerateILForMethod (TR::ResolvedMethodSymbol* calleeSymbol, TR::ResolvedMethodSymbol* callerSymbol, TR_CallTarget* calltarget);
      bool doCorrectnessAndSizeChecksForInlineCallTarget(TR_CallStack *callStack, TR_CallTarget *calltarget, bool inlinefromgraph, TR_PrexArgInfo *argInfo);
      bool validateArguments(TR_CallTarget *calltarget, TR_LinkHead<TR_ParameterMapping> &map);
      virtual bool supressInliningRecognizedInitialCallee(TR_CallSite* callsite, TR::Compilation* comp);
      virtual TR_InlinerFailureReason checkIfTargetInlineable(TR_CallTarget* target, TR_CallSite* callsite, TR::Compilation* comp);
   };

class TR_J9JSR292InlinerPolicy : public TR_J9InlinerPolicy
   {
   friend class TR_J9InlinerUtil;
   friend class TR_InlinerBase;
   friend class TR_MultipleCallTargetInliner;
   public:
      TR_J9JSR292InlinerPolicy(TR::Compilation *comp);
   protected:
      virtual TR_InlinerFailureReason checkIfTargetInlineable(TR_CallTarget* target, TR_CallSite* callsite, TR::Compilation* comp);
   };

class TR_J9TransformInlinedFunction : public TR_TransformInlinedFunction
   {
   public:
   TR_J9TransformInlinedFunction(
      TR::Compilation *c, TR_InlinerTracer *tracer,TR::ResolvedMethodSymbol * callerSymbol, TR::ResolvedMethodSymbol * calleeSymbol,
      TR::Block * callNodeBlock, TR::TreeTop * callNodeTreeTop, TR::Node * callNode,
      TR_ParameterToArgumentMapper & mapper, TR_VirtualGuardSelection *guard,
      List<TR::SymbolReference> & temps, List<TR::SymbolReference> & availableTemps,
      List<TR::SymbolReference> & availableTemps2);
      virtual void transform();
   private:
      void                  transformSynchronizedMethod(TR_ResolvedMethod *);
      TR::Block *           appendCatchBlockForInlinedSyncMethod(TR_ResolvedMethod *, TR::TreeTop * , int32_t, int32_t, bool addBlocks = true);
      bool isSyncReturnBlock(TR::Compilation *comp, TR::Block * b);
      // { RTSJ Support begins
      void                 wrapCalleeInTryRegion(bool, bool, TR_ResolvedMethod *);
      TR::TreeTop *         createThrowCatchBlock(bool, bool, TR::CFG *, TR::Block *, TR::TreeTop *, TR::SymbolReference *, int32_t, TR_ScratchList<TR::Block> & newCatchBlocks);
      TR::Block *           appendCatchBlockToRethrowException(TR_ResolvedMethod *, TR::TreeTop *, bool, int32_t, int32_t, bool addBlocks = true);
      // } RTSJ Support ends
   };

class TR_J9InnerPreexistenceInfo : public TR_InnerPreexistenceInfo
   {
   public:
      TR_J9InnerPreexistenceInfo(TR::Compilation * c, TR::ResolvedMethodSymbol *methodSymbol, TR_CallStack *callStack,
                               TR::TreeTop *callTree, TR::Node *callNode,
                               TR_VirtualGuardKind _guardKind);
      virtual bool perform(TR::Compilation *comp, TR::Node *guardNode, bool & disableTailRecursion);
      class ParmInfo
         {
         public:
            TR_ALLOC(TR_Memory::Inliner);
            ParmInfo(TR::ParameterSymbol *innerParm, TR::ParameterSymbol *outerParm = 0);

            void setOuterSymbol(TR::ParameterSymbol *outerParm)  { _outerParm = outerParm; }
            void setNotInvariant()                              { _isInvariant = false; }
            bool isInvariant()                                  { return _isInvariant;  }

            TR::ParameterSymbol *_outerParm; // may be null
            TR::ParameterSymbol *_innerParm; // never null
            bool                _isInvariant;
         };

      struct PreexistencePoint
         {
         TR_ALLOC(TR_Memory::Inliner);
         PreexistencePoint(TR_CallStack *callStack, int32_t ordinal) :
            _callStack(callStack), _ordinal(ordinal) {}

         TR_CallStack *_callStack;
         int32_t       _ordinal;
         };

      ParmInfo *getParmInfo(int32_t ordinal)                    { return _parameters[ordinal]; }
      PreexistencePoint *getPreexistencePoint(int32_t ordinal);
      void addInnerAssumption(TR_InnerAssumption *a)            { _assumptions.add(a); }
      List<TR_InnerAssumption> &getInnerAssumptions()           { return _assumptions; }
   private:
      PreexistencePoint *getPreexistencePointImpl(int32_t ordinal, TR_CallStack *prevCallStack);
      ParmInfo               **_parameters;   // information about the address type parameters
   };

class TR_J9InlinerTracer : public TR_InlinerTracer
   {
   public:
      TR_J9InlinerTracer(TR::Compilation *comp, TR_FrontEnd *fe, TR::Optimization *opt);
      void dumpProfiledClasses (ListIterator<TR_ExtraAddressInfo>& sortedValuesIt, uint32_t totalFrequency = 1);
   };
#endif
