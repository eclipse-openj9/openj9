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

#ifndef IlGenerator_h
#define IlGenerator_h

#include "ilgen/J9ByteCodeIteratorWithState.hpp"

#include "env/CompilerEnv.hpp"
#include "env/ExceptionTable.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/SymbolReference.hpp"
#include "ilgen/IlGen.hpp"
#include "infra/Checklist.hpp"
#include "infra/Link.hpp"
#include "infra/Stack.hpp"
#include "env/VMJ9.h"

class TR_PersistentClassInfo;
class TR_BitVector;
namespace TR { class IlGeneratorMethodDetails; }

class TR_J9ByteCodeIlGenerator : public TR_IlGenerator, public TR_J9ByteCodeIteratorWithState
   {
public:
   TR_J9ByteCodeIlGenerator(TR::IlGeneratorMethodDetails & details,
                            TR::ResolvedMethodSymbol *,
                            TR_J9VMBase *fe,
                            TR::Compilation *,
                            TR::SymbolReferenceTable *,
                            bool forceClassLookahead = false,
                            TR_InlineBlocks *blocksToInline=0,
                            int32_t argPlaceholderSlot=-1);

   TR_ALLOC(TR_Memory::IlGenerator)

   TR::CodeGenerator * cg() { return _compilation->cg(); }
   TR::Compilation * comp() { return _compilation; }
   TR_J9VMBase *      fej9() { return (TR_J9VMBase *)_fe; }

   TR_Memory *               trMemory()                    { return _trMemory; }
   TR_StackMemory            trStackMemory()               { return _trMemory; }
   TR_HeapMemory             trHeapMemory()                { return _trMemory; }
   TR_PersistentMemory *     trPersistentMemory()          { return _trMemory->trPersistentMemory(); }

   TR::IlGeneratorMethodDetails & methodDetails()      { return _methodDetails; }

   virtual bool    genIL();
   virtual int32_t currentByteCodeIndex() { return TR_J9ByteCodeIterator::currentByteCodeIndex(); }
   virtual TR::Block * getCurrentBlock() { return _block; }

   virtual void performClassLookahead(TR_PersistentClassInfo *);

   bool                      isPeekingMethod()   { return _compilation->isPeekingMethod();}
   bool                      inliningCheckIfFinalizeObjectIsBeneficial()
   {
       return (comp()->getOption(TR_FullSpeedDebug) || comp()->getOptLevel() <= cold ||
         (!comp()->getOption(TR_DisableInlineCheckIfFinalizeObject) && fej9()->isBenefitInliningCheckIfFinalizeObject()) ||
           (comp()->getCurrentMethod()->isConstructor() && !comp()->getCurrentMethod()->isFinal()));
   }
   virtual TR::ResolvedMethodSymbol *methodSymbol() const { return _methodSymbol;}

private:

   bool trace(){ return comp()->getOption(TR_TraceBC) || comp()->getOption(TR_TraceILGen); }

   virtual void         saveStack(int32_t);
   void                 saveStack(int32_t targetIndex, bool anchorLoads);

   // GenBranch
   //
   int32_t      genGoto(int32_t);
   int32_t      genIfOneOperand(TR::ILOpCodes);
   int32_t      genIfTwoOperand(TR::ILOpCodes);
   int32_t      genIfImpl(TR::ILOpCodes);

   /** \brief
    *     Generates IL for a return bytecode.
    *
    *  \param nodeop
    *     The IL opcode to use for the return.
    *
    *  \param monitorExit
    *     Determines whether the method which contains this return is synchronized in which case <c>TR::monitorExit</c>
    *     nodes may need to be generated.
    *
    *  \return
    *     The index of the next bytecode to generate.
    */
   int32_t genReturn(TR::ILOpCodes nodeop, bool monitorExit);

   int32_t      genLookupSwitch();
   int32_t      genTableSwitch();

   // GenCall
   //
   void         genInvokeStatic(int32_t);
   void         genInvokeSpecial(int32_t);
   void         genInvokeVirtual(int32_t);
   void         genInvokeInterface(int32_t);
   void         genInvokeDynamic(int32_t callSiteIndex);
   TR::Node *    genInvokeHandle(int32_t cpIndex);
   TR::Node *    genInvokeHandleGeneric(int32_t cpIndex);

   void         genHandleTypeCheck();
   void         genHandleTypeCheck(TR::Node *handle, TR::Node *expectedType) { push(handle); push(expectedType); genHandleTypeCheck(); }

   TR::Node *    genInvokeHandle(TR::SymbolReference *invokeExactSymRef, TR::Node *invokedynamicReceiver = NULL);
   TR::Node *    genILGenMacroInvokeExact(TR::SymbolReference *invokeExactSymRef);

   bool         runMacro(TR::SymbolReference *);
   bool         runFEMacro(TR::SymbolReference *);
   TR::Node *    genInvoke(TR::SymbolReference *, TR::Node *indirectCallFirstChild, TR::Node *invokedynamicReceiver = NULL);

   TR::Node *    genInvokeDirect(TR::SymbolReference *symRef){ return genInvoke(symRef, NULL); }
   TR::Node *    genInvokeWithVFTChild(TR::SymbolReference *);
   TR::Node *    getReceiverFor(TR::SymbolReference *);
   void          stashArgumentsForOSR(TR_J9ByteCode byteCode);

   // Placeholder manipulation
   //
   void         genArgPlaceholderCall();
   int32_t      expandPlaceholderCall(); // TODO:JSR292: Combine these into a single function?
   int32_t      numPlaceholderCalls(int32_t depthLimit);
   int32_t      expandPlaceholderCalls(int32_t depthLimit);
   TR::SymbolReference *expandPlaceholderSignature(TR::SymbolReference *symRef, int32_t numArgs);
   TR::SymbolReference *expandPlaceholderSignature(TR::SymbolReference *symRef, int32_t numArgs, int32_t firstArgStackDepth);
   TR::SymbolReference *placeholderWithDummySignature();
   TR::SymbolReference *placeholderWithSignature(char *prefix, int prefixLength, char *middle, int middleLength, char *suffix, int suffixLength);

   void chopPlaceholder(TR::Node *placeholder, int32_t firstChild, int32_t numChildren);

   char               *artificialSignature (TR_AllocationKind alloc, char *format, ...);
   char               *vartificialSignature(TR_AllocationKind alloc, char *format, va_list args);
   TR::SymbolReference *symRefWithArtificialSignature(TR::SymbolReference *original, char *effectiveSigFormat, ...);

   // GenLoadStore
   //
   void         loadInstance(int32_t);
   void         loadStatic(int32_t);
   void         loadAuto(TR::DataType type, int32_t slot, bool isAdjunct = false);
   TR::Node     *loadSymbol(TR::ILOpCodes, TR::SymbolReference *);
   void         loadConstant(TR::ILOpCodes, int32_t);
   void         loadConstant(TR::ILOpCodes, int64_t);
   void         loadConstant(TR::ILOpCodes, float);
   void         loadConstant(TR::ILOpCodes, double);
   void         loadConstant(TR::ILOpCodes, void *);
   void         loadFromCP(TR::DataType, int32_t);
   void         loadFromCallSiteTable(int32_t);

   void         loadClassObjectAndIndirect(int32_t cpIndex);

   void         loadClassObject(int32_t cpIndex);
   TR::SymbolReference *loadClassObjectForTypeTest(int32_t cpIndex, TR_CompilationOptions aotInhibit);
   void         loadClassObject(TR_OpaqueClassBlock *opaqueClass);
   void         loadArrayElement(TR::DataType dt){ loadArrayElement(dt, comp()->il.opCodeForIndirectArrayLoad(dt)); }
   void         loadArrayElement(TR::DataType dt, TR::ILOpCodes opCode, bool checks = true);
   void         loadMonitorArg();

   void         storeInstance(int32_t);
   void         storeStatic(int32_t);
   void         storeAuto(TR::DataType type, int32_t slot, bool isAdjunct = false);
   void         storeArrayElement(TR::DataType dt){ storeArrayElement(dt, comp()->il.opCodeForIndirectArrayStore(dt)); }
   void         storeArrayElement(TR::DataType dt, TR::ILOpCodes opCode, bool checks = true);

   void         calculateElementAddressInContiguousArray(int32_t, int32_t);
   void         calculateIndexFromOffsetInContiguousArray(int32_t, int32_t);
   void         calculateArrayElementAddress(TR::DataType, bool checks);

   // GenMisc
   //
   TR::TreeTop * genTreeTop(TR::Node *);

   TR::Node    * loadConstantValueIfPossible(TR::Node *, uintptrj_t, TR::DataType type = TR::Int32, bool isArrayLength = true);

   void         genArrayLength();
   void         genContiguousArrayLength(int32_t width);
   void         genArrayBoundsCheck(TR::Node *, int32_t);
   void         genDivCheck();
   void         genIDiv();
   void         genLDiv();
   void         genIRem();
   void         genLRem();
   TR::Node *    genNullCheck(TR::Node *);
   TR::Node *    genResolveCheck(TR::Node *);
   TR::Node *    genResolveAndNullCheck(TR::Node *);
   void         genInc();
   void         genIncLong();
   void         genAsyncCheck();
   void         genNew(int32_t cpIndex);
   void         genNew(TR::ILOpCodes opCode=TR::New);
   void         genNewArray(int32_t typeIndex);
   void         genANewArray(int32_t cpIndex);
   void         genANewArray();
   void         genMultiANewArray(int32_t cpIndex, int32_t dims);
   void         genMultiANewArray(int32_t dims);
   void         genInstanceof(int32_t cpIndex);
   void         genCheckCast(int32_t);
   void         genCheckCast();
   int32_t      genAThrow();
   void         genMonitorEnter();
   void         genMonitorExit(bool);
   void         genFlush(int32_t nargs);
   void         genFullFence(TR::Node *node);
   void         handlePendingPushSaveSideEffects(TR::Node *, int32_t stackSize = -1);
   void         handlePendingPushSaveSideEffects(TR::Node *, TR::NodeChecklist&, int32_t stackSize = -1);
   void         handleSideEffect(TR::Node *);
   bool         valueMayBeModified(TR::Node *, TR::Node *);
   TR::Node *    genCompressedRefs(TR::Node *, bool genTT = true, int32_t isLoad = 1);

   // IlGenerator
   //
   bool         internalGenIL();
   bool         genILFromByteCodes();

   void         prependEntryCode(TR::Block *);
   void         prependGuardedCountForRecompilation(TR::Block * firstBlock);
   TR::Node *    genMethodEnterHook();
   TR::Block *   genExceptionHandlers(TR::Block *);
   TR::Block *   cloneHandler(TryCatchInfo *, TR::Block *, TR::Block *, TR::Block *, List<TR::Block> *);
   void         createGeneratedFirstBlock();
   bool         genJNIIL();
   bool         genNewInstanceImplThunk();
   void         genJavaLangSystemIdentityHashCode();
   void         genDFPGetHWAvailable();
   void         genHWOptimizedStrProcessingAvailable();
   void         genJITIntrinsicsEnabled();
   void         genIsORBDeepCopyAvailable();

   TR::SymbolReference *getVectorElementSymRef();
   TR::Node            *genVectorUnaryOp(TR::Node *node, TR::ILOpCodes op1, TR::DataType dt1, TR::ILOpCodes op2, TR::DataType dt2);
   TR::Node            *genVectorBinaryOp(TR::Node *node, TR::ILOpCodes op1, TR::DataType dt1, TR::ILOpCodes op2, TR::DataType dt2);
   TR::Node            *genVectorBinaryOpNoTrg(TR::Node *node, TR::ILOpCodes op);
   TR::Node            *genVectorTernaryOp(TR::Node *node, TR::ILOpCodes op);
   TR::Node            *genVectorCompareBranchOp(TR::Node *node, TR::ILOpCodes op);
   TR::Node            *genVectorLoadOp(TR::Node *node, TR::ILOpCodes loadOp, TR::ILOpCodes storeOp);
   TR::Node            *genVectorStoreOp(TR::Node *node, TR::ILOpCodes loadOp, TR::ILOpCodes storeOp);
   TR::Node            *handleVectorIntrinsicCall(TR::Node *node, TR::MethodSymbol *symbol);

   TR::Node            *genVectorSplatsOp(TR::Node *node, TR::ILOpCodes loadOp, TR::DataType vectorType, TR::DataType scalarType);
   TR::Node            *genVectorStore(TR::Node *node, TR::DataType dt, TR::DataType dt2);
   TR::Node            *genVectorGetElementOp(TR::Node *node, TR::ILOpCodes loadOp);
   TR::Node            *genVectorSetElementOp(TR::Node *node, TR::ILOpCodes loadOp);
   TR::Node            *genVectorAddReduceDouble(TR::Node *node);
   TR::Node            *genVectorLoadWithStrideDouble(TR::Node *node);
   TR::Node            *genVectorLogDouble(TR::Node *node);
   TR::Node            *genVectorAddress(TR::Node *node, TR::ILOpCodes op);

   TR::Node     *genNewInstanceImplCall(TR::Node *classNode);
   //TR::Node *  transformNewInstanceImplCall(TR::TreeTop *, TR::Node *);

   // Dynamic Loop Transfer
   void         genDLTransfer(TR::Block *);

   // OSR
   void stashPendingPushLivenessForOSR(int32_t offset = 0);

   void inlineJitCheckIfFinalizeObject(TR::Block *);

   // support for dual symbols, to generate adjunct symbol of the dual
   TR::Node* genOrFindAdjunct(TR::Node* node);
   void storeDualAuto(TR::Node * storeValue, int32_t slot);

   // Walker
   //
   TR::Block *   walker(TR::Block *);

   int32_t      cmp(TR::ILOpCodes, TR::ILOpCodes *, int32_t &);
   int32_t      cmpFollowedByIf(uint8_t, TR::ILOpCodes, int32_t &);

   // [PR 101228] To avoid class resolution on (null instanceof T) and (T)null.
   void expandUnresolvedClassCheckcast(TR::TreeTop *tree);
   void expandUnresolvedClassInstanceof(TR::TreeTop *tree);

   // [PR 124693] To check types on invokespecial within interface methods.
   bool skipInvokeSpecialInterfaceTypeChecks();
   void expandInvokeSpecialInterface(TR::TreeTop *tree);

   // Expand MethodHandle invoke
   void expandInvokeHandle(TR::TreeTop *tree);
   void expandInvokeExact(TR::TreeTop *tree);
   void expandInvokeDynamic(TR::TreeTop *tree);
   void expandInvokeHandleGeneric(TR::TreeTop *tree);
   void expandMethodHandleInvokeCall(TR::TreeTop *tree);
   void insertCustomizationLogicTreeIfEnabled(TR::TreeTop *tree, TR::Node* methodHandle);
   TR::Node* loadCallSiteMethodTypeFromCP(TR::Node* methodHandleInvokeCall);
   TR::Node* loadFromMethodTypeTable(TR::Node* methodHandleInvokeCall);
   TR::Node* loadCallSiteMethodType(TR::Node* methodHandleInvokeCall);

   // inline functions
   //
   TR::SymbolReferenceTable * symRefTab()                      { return _symRefTab; }
   TR::CFG *                  cfg()                            { return _methodSymbol->getFlowGraph(); }
   bool                      isOutermostMethod() { return comp()->isOutermostMethod(); }

   bool swapChildren(TR::ILOpCodes, TR::Node *);
   void removeIfNotOnStack(TR::Node *n);
   void popAndDiscard(int n);
   void discardEntireStack();
   void startCountingStackRefs();
   void stopCountingStackRefs();
   void eat1()
      {
      popAndDiscard(1);
      }
   void eat2()
      {
      bool narrow = numberOfByteCodeStackSlots(node(_stack->topIndex())) == 4;
      popAndDiscard(narrow ? 2 : 1);
      }

   bool fieldsAreSame(TR::SymbolReference * s1, TR::SymbolReference * s2)
      {
      //bool sigSame = true;
      //return s1->getOwningMethod(_compilation)->fieldsAreSame(s1->getCPIndex(), s2->getOwningMethod(_compilation), s2->getCPIndex(), sigSame);
      return TR::Compiler->cls.jitFieldsAreSame(_compilation, s1->getOwningMethod(_compilation), s1->getCPIndex(), s2->getOwningMethod(_compilation), s2->getCPIndex(), false);
      }

   bool staticsAreSame(TR::SymbolReference * s1, TR::SymbolReference * s2)
      {
      //bool sigSame = true;
      //return s1->getOwningMethod(_compilation)->staticsAreSame(s1->getCPIndex(), s2->getOwningMethod(_compilation), s2->getCPIndex(), sigSame);
      return TR::Compiler->cls.jitStaticsAreSame(_compilation, s1->getOwningMethod(_compilation), s1->getCPIndex(), s2->getOwningMethod(_compilation), s2->getCPIndex());
      }

   bool isFinalFieldFromSuperClasses(J9Class *, int32_t);

   TR::Node * genNodeAndPopChildren(TR::ILOpCodes, int32_t, TR::SymbolReference *, int32_t = 0);
   TR::Node * genNodeAndPopChildren(TR::ILOpCodes, int32_t, TR::SymbolReference *, int32_t firstIndex, int32_t lastIndex);

   void genUnary(TR::ILOpCodes unaryOp, bool isForArrayAccess= false);
   void genBinary(TR::ILOpCodes nodeop, int numChildren = 2);

   bool replaceMembersOfFormat();
   bool replaceMethods(TR::TreeTop *tt, TR::Node *node);
   bool replaceFieldsAndStatics(TR::TreeTop *tt, TR::Node *node);
   bool replaceField(TR::Node* node, char* destClass, char* destFieldName, char* destFieldSignature, int ParmIndex);
   bool replaceStatic(TR::Node* node, char* dstClassName, char* staticName, char* type);

   uintptrj_t walkReferenceChain(TR::Node *node, uintptrj_t receiver);
   bool hasFPU();

   // data
   //
   TR::SymbolReferenceTable *         _symRefTab;
   TR::SymbolReferenceTable *         _classLookaheadSymRefTab;
   int32_t                           _firstTempCookie;
   TR_PersistentClassInfo *          _classInfo;
   TR_ScratchList<TR::Node>           _implicitMonitorExits;
   TR_ScratchList<TR::Node>           _finalizeCallsBeforeReturns;
   TR::IlGeneratorMethodDetails &_methodDetails;

   // TAROK only.  Suppress spine check trees.  This is a debug feature only and will be
   // removed when full support is available.
   //
   bool                              _suppressSpineChecks;

   bool                              _generateWriteBarriersForGC;
   bool                              _generateWriteBarriersForFieldWatch;
   bool                              _generateReadBarriersForFieldWatch;
   vcount_t                           _blockAddedVisitCount;
   bool                              _noLookahead;
   bool                              _thisChanged;
   TR_InlineBlocks                  *_blocksToInline;
   bool                              _intrinsicErrorHandling;
   bool                              _couldOSRAtNextBC;

   // JSR292
   int32_t                           _argPlaceholderSlot;
   int32_t                           _argPlaceholderSignatureOffset;
   TR_BitVector                     *_methodHandleInvokeCalls;
   TR_BitVector                     *_invokeHandleCalls;
   TR_BitVector                     *_invokeHandleGenericCalls;
   TR_BitVector                     *_invokeDynamicCalls;
   TR_BitVector                     *_ilGenMacroInvokeExactCalls;

   // TenantScope field support, set to 'true' if a get/put static is encountered
   // when option TR_DisableMultiTenancy is on and current method contains static
   // field and method reference/invoke, skip compilation for such method
   bool                              _staticFieldReferenceEncountered;
   bool                              _staticMethodInvokeEncountered;

   TR_OpaqueClassBlock              *_invokeSpecialInterface;
   TR_BitVector                     *_invokeSpecialInterfaceCalls;
   bool                              _invokeSpecialSeen;

   // OSR
   TR::NodeChecklist                *_processedOSRNodes;

   // DecimalFormatPeephole
   struct methodRenamePair{
      char* srcMethodSignature;
      char* dstMethodSignature;
   };

   static const int32_t              _numDecFormatRenames = 9;
   static struct methodRenamePair    _decFormatRenames[_numDecFormatRenames];
   TR::SymbolReference               *_decFormatRenamesDstSymRef[_numDecFormatRenames];
   };

#endif

