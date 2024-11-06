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

#ifndef INLINERTEMPFORJ9_INCL
#define INLINERTEMPFORJ9_INCL

#include "j9cfg.h"
#include "ras/LogTracer.hpp"
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

      TR_LinkHead<TR_CallTarget> _callTargets; // This list only contains the call targets from top most level

   protected:
      virtual int32_t scaleSizeBasedOnBlockFrequency(int32_t bytecodeSize, int32_t frequency, int32_t borderFrequency, TR_ResolvedMethod * calleeResolvedMethod, TR::Node *callNode, int32_t coldBorderFrequency = 0);
      float getScalingFactor(float factor);
      virtual bool supportsMultipleTargetInlining () { return true ; }

   private:
      bool analyzeCallSite(TR::ResolvedMethodSymbol *, TR_CallStack *, TR::TreeTop *, TR::Node *, TR::Node *);
      void weighCallSite( TR_CallStack * callStack , TR_CallSite *callsite, bool currentBlockHasExceptionSuccessors,bool dontAddCalls=false);

      int32_t applyArgumentHeuristics(TR_LinkHead<TR_ParameterMapping> &map, int32_t originalWeight, TR_CallTarget *target);
      bool eliminateTailRecursion(TR::ResolvedMethodSymbol *, TR_CallStack *, TR::TreeTop *, TR::Node *, TR::Node *, TR_VirtualGuardSelection *);
      void assignArgumentsToParameters(TR::ResolvedMethodSymbol *, TR::TreeTop *, TR::Node *);
      bool isLargeCompiledMethod(TR_ResolvedMethod *calleeResolvedMethod, int32_t bytecodeSize, int32_t freq);
      /* \brief
       *    This API processes the call targets got chopped off from \ref _calltargets
       *
       * \parm firstChoppedOffcalltarget
       *    the start of the targets that should be chopped off from \ref _callTargets
       *
       * \parm lastTargetToInline
       *    the tail of _callTargets
       *
       * \notes
       *    Call targets are chopped off from \ref _callTargets list because of global budget limit like node counts and total weight.
       *    This function chooses to keep some chopped of targets if they meet certain conditions.
       */
      void processChoppedOffCallTargets(TR_CallTarget* lastTargetToInline, TR_CallTarget *firstChoppedOffcalltarget, int estimateAndRefineBytecodeSize);

      /*
       * \brief
       *    Recursively walk through the sub call graph of a given calltarget and clean up all targets
       *    from their respective callsites if chopped of from _callTargets
       *
       * \return
       *    True if the given calltarget should be inlined
       */
      bool inlineSubCallGraph(TR_CallTarget* calltarget);
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
      virtual TR_PrexArgInfo *computePrexInfo(TR_CallTarget *target, TR_PrexArgInfo *callerArgInfo);
      static  TR_PrexArgInfo *computePrexInfo(TR_InlinerBase *inliner, TR_CallSite *site, TR_PrexArgInfo *callerArgInfo = NULL);
      virtual void refineInlineGuard(TR::Node *callNode, TR::Block *&block1, TR::Block *&block2,
                   bool &appendTestToBlock1, TR::ResolvedMethodSymbol * callerSymbol, TR::TreeTop *cursorTree,
                   TR::TreeTop *&virtualGuard, TR::Block *block4);
      virtual void refineInliningThresholds(TR::Compilation *comp, int32_t &callerWeightLimit, int32_t &maxRecursiveCallByteCodeSizeEstimate, int32_t &methodByteCodeSizeThreshold, int32_t &methodInWarmBlockByteCodeSizeThreshold, int32_t &methodInColdBlockByteCodeSizeThreshold, int32_t &nodeCountThreshold, int32_t size);
      static void checkForConstClass(TR_CallTarget *target, TR_LogTracer *tracer);
      virtual bool needTargetedInlining(TR::ResolvedMethodSymbol *callee);
      virtual void requestAdditionalOptimizations(TR_CallTarget *calltarget);
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
   friend class TR_J9EstimateCodeSize;
   public:
      TR_J9InlinerPolicy(TR::Compilation *comp);
      virtual bool inlineRecognizedMethod(TR::RecognizedMethod method);
      virtual bool tryToInlineTrivialMethod (TR_CallStack* callStack, TR_CallTarget* calltarget);
      bool isInlineableJNI(TR_ResolvedMethod *method,TR::Node *callNode);
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
      virtual bool shouldRemoveDifferingTargets(TR::Node *callNode);
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

      /**
       * \brief Generates a diamond that will be used as the outline of the IL generated
       *        for inlining a call to \c Unsafe.get* or \c Unsafe.put*.  It splits the block
       *        that contains \c callNodeTreeTop at that point, adds \c comparisonTree
       *        as the branch for the diamond, branching to a block containing \c ifTree
       *
       * \param callNodeTreeTop A pointer to the \ref TR::TreeTop for the call that is
       *           being inlined
       * \param comparisonTree A pointer to a \ref TR::TreeTop for a \c if node that will
       *           be the root of the diamond
       * \param branchTargetTree A pointer to a \ref TR::TreeTop representing the taken branch of
       *           \c comparisonTree
       * \param fallThroughTree A pointer to a \ref TR::TreeTop reprenting the fall-through branch
       *           of \c comparisonTree
       *
       * \return A pointer to a \ref TR::Block representing the join point of the diamond
       *         after executing either \c branchTargetTree or \c fallThroughTree
       */
      TR::Block * createUnsafeGetPutCallDiamond(TR::TreeTop* callNodeTreeTop, TR::TreeTop* comparisonTree, TR::TreeTop* branchTargetTree, TR::TreeTop* fallThroughTree);
      bool createUnsafePutWithOffset(TR::ResolvedMethodSymbol *, TR::ResolvedMethodSymbol *, TR::TreeTop *, TR::Node *, TR::DataType, bool, bool needNullCheck = false, bool isOrdered = false, bool isUnaligned = false);
      TR::TreeTop* genDirectAccessCodeForUnsafeGetPut(TR::Node* callNode, bool conversionNeeded, bool isUnsafeGet);
      void createTempsForUnsafePutGet(TR::Node*& unsafeAddress, TR::Node* unsafeCall, TR::TreeTop* callNodeTreeTop, TR::Node*& offset, TR::SymbolReference*& newSymbolReferenceForAddress, bool isUnsafeGet);
      bool         createUnsafeGet(TR::ResolvedMethodSymbol *, TR::ResolvedMethodSymbol *, TR::TreeTop *, TR::Node *, TR::DataType, bool compress = true);
      bool         createUnsafePut(TR::ResolvedMethodSymbol *, TR::ResolvedMethodSymbol *, TR::TreeTop *, TR::Node *, TR::DataType, bool compress = true);
      TR::Node *    createUnsafeAddress(TR::Node *);
      bool         createUnsafeGetWithOffset(TR::ResolvedMethodSymbol *, TR::ResolvedMethodSymbol *, TR::TreeTop *, TR::Node *, TR::DataType, bool, bool needNullCheck = false, bool isUnaligned = false);
      TR::Node *    createUnsafeAddressWithOffset(TR::Node *);
      bool         createUnsafeFence(TR::TreeTop *, TR::Node *, TR::ILOpCodes);

      TR::Node *    createUnsafeMonitorOp(TR::ResolvedMethodSymbol *calleeSymbol, TR::ResolvedMethodSymbol *callerSymbol, TR::TreeTop * callNodeTreeTop, TR::Node * unsafeCall, bool isEnter);
      bool         createUnsafeCASCallDiamond(TR::TreeTop *, TR::Node *);

      /**
       * \brief Generates an \c if node that will test whether the low-order bit of
       *           the supplied \c offset is set.  The branch target must be set
       *           by the caller.
       *
       * \param offset A pointer to a \ref TR::Node representing the offset value
       *           whose low-order bit is to be tested
       * \param branchIfLowTagged A value of type \c bool that indicates whether the
       *           branch should be taken if the low-order bit of \c offset is set
       *           or not set
       *
       * \return A pointer to a \ref TR::TreeTop holding the generated \c if node
       */
      TR::TreeTop* genClassCheckForUnsafeGetPut(TR::Node* offset, bool branchIfLowTagged);

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

      /**
       * \brief Puts together fragments of IL that have been generated for parts of the
       *        IL that are needed to generate \c Unsafe.get* or \c Unsafe.put* inline.
       *        Resultant IL is placed in the \ref TR:CFG in place of the existing call.
       *
       * \param unsafeAddress Pointer to a \ref TR::Node representing the \c Object
       *           argument of the call
       * \param unsafeOffset Pointer to a \ref TR::Node representing the \c long
       *           value offset argument of the call
       * \param type A \ref TR::DataType indicating the OMR data type corresponding to
       *           the Java type of the \c Unsafe method call
       * \param callNodeTreeTop A pointer to a \ref TR::TreeTop representing the call
       *           that is being inlined
       * \param prevTreeTop  A pointer to a \ref TR::TreeTop preceding the call in the IL
       * \param newSymbolReferenceForAddress A \ref TR::SymbolReference representing the
       *           the \c Object argument of the \c Unsafe call
       * \param directAccessTreeTop A pointer to a \ref TR::TreeTop for the IL needed
       *           to perform a nearly "direct" reference loading or storing a value
       *           for the \c Unsafe method call, without any conversion for one or
       *           two byte values
       * \param arraydirectAccessTreeTop A pointer to a \ref TR::TreeTop for the IL
       *           needed to perform a nearly "direct" reference loading or storing a
       *           value for the \c Unsafe method call, without any conversion for one
       *           or two byte values
       * \param indirectAccessTreeTop A pointer to a \ref TR::TreeTop for the IL
       *           needed to perform a more "indirect" reference loading or storing a
       *           static field value for the \c Unsafe method call
       * \param directAccessWithConversionTreeTop A pointer to a \ref TR::TreeTop for
       *           the IL needed to perform a nearly "direct" reference loading or
       *           storing a value for the \c Unsafe method call, with a conversion
       *           for one or two byte values
       * \param needNullCheck A \bool value indicating whether a \ref TR::NULLCHK
       *           needs to be generated for the value of \c unsafeAddress
       * \param isUnsafeGet A \bool value indicating whether the call represents an
       *           \c Unsafe.get* operation &mdash; \c true &mdash; or an
       *           \c Unsafe.put* operation &mdash; \c false.
       * \param conversionNeeded Indicates whether the call reprents an \c Unsafe
       *           method call involving any of Java \c char, \c short, \c byte or
       *           \c boolean.
       * \param arrayBlockNeeded Indicates whether a separate access block needs to be
       *           generated to handle the case where the \c Object is an array
       * \param typeTestsNeeded Indicates whether any type tests for \c Object need to
       *           be generated (i.e.: if the type of the \c Object is unknown)
       * \param orderedCallNode Indicates whether the call represents an \c Unsafe
       *           ordered method
       */
      void genCodeForUnsafeGetPut(TR::Node* unsafeAddress, TR::Node *unsafeOffset, TR::DataType type, TR::TreeTop* callNodeTreeTop,
                                  TR::TreeTop* prevTreeTop, TR::SymbolReference* newSymbolReferenceForAddress,
                                  TR::TreeTop* directAccessTreeTop, TR::TreeTop* arraydirectAccessTreeTop,
                                  TR::TreeTop* indirectAccessTreeTop, TR::TreeTop* directAccessWithConversionTreeTop,
                                  bool needNullCheck, bool isUnsafeGet, bool conversionNeeded,
                                  bool arrayBlockNeeded, bool typeTestsNeeded, TR::Node* orderedCallNode);
      virtual bool callMustBeInlined(TR_CallTarget *calltarget);
      virtual bool callMustBeInlinedInCold(TR_ResolvedMethod *method);
      bool mustBeInlinedEvenInDebug(TR_ResolvedMethod * calleeMethod, TR::TreeTop *callNodeTreeTop);
      bool _tryToGenerateILForMethod (TR::ResolvedMethodSymbol* calleeSymbol, TR::ResolvedMethodSymbol* callerSymbol, TR_CallTarget* calltarget);
      bool doCorrectnessAndSizeChecksForInlineCallTarget(TR_CallStack *callStack, TR_CallTarget *calltarget, bool inlinefromgraph, TR_PrexArgInfo *argInfo);
      bool validateArguments(TR_CallTarget *calltarget, TR_LinkHead<TR_ParameterMapping> &map);
      virtual bool supressInliningRecognizedInitialCallee(TR_CallSite* callsite, TR::Compilation* comp);
      virtual TR_InlinerFailureReason checkIfTargetInlineable(TR_CallTarget* target, TR_CallSite* callsite, TR::Compilation* comp);
      /** \brief
       *     This query decides whether the given method is JSR292 related
       */
      static bool isJSR292Method(TR_ResolvedMethod *resolvedMethod);
      /** \brief
       *     This query decides whether the given JSR292 callee is worthing inlining
       *
       *  \notes
       *     The methods are in 3 kinds: 1. VarHandle operation methods 2. small getters 3. method handle thunk
       */
      static bool isJSR292AlwaysWorthInlining(TR_ResolvedMethod *resolvedMethod);
      /** \brief
       *     This query defines a group of methods that are small getters in the java/lang/invoke package
       */
      static bool isJSR292SmallGetterMethod(TR_ResolvedMethod *resolvedMethod);
      /** \brief
       *     This query defines a group of methods that are small helpers in the java/lang/invoke package
       */
      static bool isJSR292SmallHelperMethod(TR_ResolvedMethod *resolvedMethod);

      /**
       * \brief
       *    This query answers whether the method is a simple non-native Unsafe method that contain a call to
       *    a native Unsafe method that would normally be handled in TR_J9InlinerPolicy::inlineUnsafeCall. If
       *    we can determine that the runtime checks in the wrapper method can be determined at compile time,
       *    it may be possible to treat the wrapper method as its underlying native Unsafe method and have it
       *    inlined in TR_J9InlinerPolicy::inlineUnsafeCall.
       *
       * \param
       *    resolvedMethod the TR_ResolvedMethod
       * \return
       *    true if the method is a simple wrapper method for a native unsafe method, false otherwise
       */
      static bool isSimpleWrapperForInlineableUnsafeNativeMethod(TR_ResolvedMethod *resolvedMethod);

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
