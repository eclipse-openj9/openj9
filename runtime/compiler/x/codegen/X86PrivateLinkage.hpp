/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#ifndef X86PRIVATELINKAGE_INCL
#define X86PRIVATELINKAGE_INCL

#include "codegen/Linkage.hpp"

#include "env/jittypes.h"
#include "codegen/RegisterDependency.hpp"
#include "x/codegen/X86Register.hpp"

class TR_J2IThunk;
class TR_ResolvedMethod;
namespace TR { class Instruction; }

//
// Hack markers
//

// This is an overkill way to make sure the preconditions survive up to the call instruction.
// It's also necessary to make sure any spills happen below the internal control flow, due
// to the nature of the backward register assignment pass.
// TODO: Put the preconditions on the call instruction itself.
//
#define COPY_PRECONDITIONS_TO_POSTCONDITIONS (1)

namespace TR {

enum
   {
   BranchJNE     = 0,
   BranchJE      = 1,
   BranchNopJMP  = 2
   };

enum
   {
   PicSlot_NeedsShortConditionalBranch      = 0x01,
   PicSlot_NeedsLongConditionalBranch       = 0x02,
   PicSlot_NeedsPicSlotAlignment            = 0x04,
   PicSlot_NeedsPicCallAlignment            = 0x08,
   PicSlot_NeedsJumpToDone                  = 0x10,
   PicSlot_GenerateNextSlotLabelInstruction = 0x20
   };

class X86PICSlot
   {
   public:

   TR_ALLOC(TR_Memory::Linkage);

   X86PICSlot(uintptrj_t classAddress, TR_ResolvedMethod *method, bool jumpToDone=true, TR_OpaqueMethodBlock *m = NULL, int32_t slot = -1):
     _classAddress(classAddress), _method(method), _helperMethodSymbolRef(NULL), _branchType(BranchJNE), _methodAddress(m), _slot(slot)
      {
      if (jumpToDone) setNeedsJumpToDone(); // TODO: Remove this oddball.  We can tell whether we need a dump to done based on whether a doneLabel is passed to buildPICSlot
      }

   uintptrj_t          getClassAddress()                         { return _classAddress; }
   TR_ResolvedMethod  *getMethod()                               { return _method; }

   TR_OpaqueMethodBlock *getMethodAddress()                      { return _methodAddress; }

   int32_t getSlot()                                           { return _slot; }

   void                setHelperMethodSymbolRef(TR::SymbolReference *symRef)
                                                                 { _helperMethodSymbolRef = symRef; }
   TR::SymbolReference *getHelperMethodSymbolRef()                { return _helperMethodSymbolRef; }

   void                setJumpOnNotEqual()                       { _branchType = BranchJNE; }
   bool                needsJumpOnNotEqual()                     { return _branchType == BranchJNE; }

   void                setJumpOnEqual()                          { _branchType = BranchJE; }
   bool                needsJumpOnEqual()                        { return _branchType == BranchJE; }

   void                setNopAndJump()                           { _branchType = BranchNopJMP; }
   bool                needsNopAndJump()                         { return _branchType == BranchNopJMP; }

   bool                needsShortConditionalBranch()             {return _flags.testAny(PicSlot_NeedsShortConditionalBranch);}
   void                setNeedsShortConditionalBranch()          {_flags.set(PicSlot_NeedsShortConditionalBranch);}

   bool                needsLongConditionalBranch()              {return _flags.testAny(PicSlot_NeedsLongConditionalBranch);}
   void                setNeedsLongConditionalBranch()           {_flags.set(PicSlot_NeedsLongConditionalBranch);}

   bool                needsPicSlotAlignment()                   {return _flags.testAny(PicSlot_NeedsPicSlotAlignment);}
   void                setNeedsPicSlotAlignment()                {_flags.set(PicSlot_NeedsPicSlotAlignment);}

   bool                needsPicCallAlignment()                   {return _flags.testAny(PicSlot_NeedsPicCallAlignment);}
   void                setNeedsPicCallAlignment()                {_flags.set(PicSlot_NeedsPicCallAlignment);}

   bool                needsJumpToDone()                         {return _flags.testAny(PicSlot_NeedsJumpToDone);}
   void                setNeedsJumpToDone()                      {_flags.set(PicSlot_NeedsJumpToDone);}

   bool                generateNextSlotLabelInstruction()        {return _flags.testAny(PicSlot_GenerateNextSlotLabelInstruction);}
   void                setGenerateNextSlotLabelInstruction()     {_flags.set(PicSlot_GenerateNextSlotLabelInstruction);}

   protected:

   flags8_t            _flags;
   uintptrj_t          _classAddress;
   TR_ResolvedMethod  *_method;
   TR::SymbolReference *_helperMethodSymbolRef;
   TR_OpaqueMethodBlock *_methodAddress;
   int32_t             _slot;
   uint8_t             _branchType;
   };

class X86CallSite
   {
   public:

   X86CallSite(TR::Node *callNode, TR::Linkage *calleeLinkage);

   TR::Node      *getCallNode(){ return _callNode; }
   TR::Linkage *getLinkage(){ return _linkage; }

   TR::CodeGenerator *cg(){ return _linkage->cg(); }
   TR::Compilation    *comp(){ return _linkage->comp(); }
   TR_FrontEnd         *fe(){ return _linkage->fe(); }

   // Register dependency construction
   TR::RegisterDependencyConditions *getPreConditionsUnderConstruction() { return _preConditionsUnderConstruction; }
   TR::RegisterDependencyConditions *getPostConditionsUnderConstruction(){ return _postConditionsUnderConstruction; }

   void addPreCondition (TR::Register *virtualReg, TR::RealRegister::RegNum realReg){ _preConditionsUnderConstruction->unionPreCondition(virtualReg, realReg, cg()); }
   void addPostCondition(TR::Register *virtualReg, TR::RealRegister::RegNum realReg){ _postConditionsUnderConstruction->unionPostCondition(virtualReg, realReg, cg()); }
   void stopAddingConditions();

   // Immutable call site properties
   TR::ResolvedMethodSymbol *getCallerSym(){ return comp()->getMethodSymbol(); }
   TR::MethodSymbol         *getMethodSymbol(){ return _callNode->getSymbol()->castToMethodSymbol(); }
   TR::ResolvedMethodSymbol *getResolvedMethodSymbol(){ return _callNode->getSymbol()->getResolvedMethodSymbol(); }
   TR_ResolvedMethod       *getResolvedMethod(){ return getResolvedMethodSymbol()? getResolvedMethodSymbol()->getResolvedMethod() : NULL; }
   TR::SymbolReference      *getSymbolReference(){ return _callNode->getSymbolReference(); }
   TR_OpaqueClassBlock     *getInterfaceClassOfMethod(){ return _interfaceClassOfMethod; } // NULL for virtual methods

   // Abstraction of complex decision logic
   bool shouldUseInterpreterLinkage();
   bool vftPointerMayPersist();
   TR_ScratchList<TR::X86PICSlot> *getProfiledTargets(){ return _profiledTargets; } // NULL if there aren't any
   TR_VirtualGuardKind getVirtualGuardKind(){ return _virtualGuardKind; }
   TR_ResolvedMethod *getDevirtualizedMethod(){ return _devirtualizedMethod; }  // The method to be dispatched statically
   TR::SymbolReference *getDevirtualizedMethodSymRef(){ return _devirtualizedMethodSymRef; }
   TR::Register *evaluateVFT();
   TR::Instruction *getImplicitExceptionPoint(){ return _vftImplicitExceptionPoint; }
   TR::Instruction *setImplicitExceptionPoint(TR::Instruction *instr){ return _vftImplicitExceptionPoint = instr; }
   uint32_t getPreservedRegisterMask(){ return _preservedRegisterMask; }
   bool resolvedVirtualShouldUseVFTCall();

   TR::Instruction *getFirstPICSlotInstruction() {return _firstPICSlotInstruction;}
   void setFirstPICSlotInstruction(TR::Instruction *f) {_firstPICSlotInstruction = f;}
   uint8_t *getThunkAddress() {return _thunkAddress;}
   void setThunkAddress(uint8_t *t) {_thunkAddress = t;}

   float getMinProfiledCallFrequency(){ return .075F; }  // Tuned for megamorphic site in jess; so bear in mind before changing

   public:

   bool    argsHaveBeenBuilt(){ return _argSize >= 0; }
   void    setArgSize(int32_t s){ TR_ASSERT(!argsHaveBeenBuilt(), "assertion failure"); _argSize = s; }
   int32_t getArgSize(){ TR_ASSERT(argsHaveBeenBuilt(), "assertion failure"); return _argSize; }

   bool useLastITableCache(){ return _useLastITableCache; }

   private:

   TR::Node       *_callNode;
   TR::Linkage *_linkage;
   TR_OpaqueClassBlock *_interfaceClassOfMethod;
   int32_t  _argSize;
   uint32_t _preservedRegisterMask;
   TR::RegisterDependencyConditions *_preConditionsUnderConstruction;
   TR::RegisterDependencyConditions *_postConditionsUnderConstruction;
   TR::Instruction *_vftImplicitExceptionPoint;
   TR::Instruction *_firstPICSlotInstruction;

   void computeProfiledTargets();
   TR_ScratchList<TR::X86PICSlot> *_profiledTargets;

   void setupVirtualGuardInfo();
   TR_VirtualGuardKind  _virtualGuardKind;
   TR_ResolvedMethod   *_devirtualizedMethod;
   TR::SymbolReference  *_devirtualizedMethodSymRef;

   uint8_t *_thunkAddress;

   bool _useLastITableCache;
   };


struct PicParameters
   {
   intptrj_t defaultSlotAddress;
   int32_t roundedSizeOfSlot;
   int32_t defaultNumberOfSlots;
   };



class X86PrivateLinkage : public TR::Linkage
   {
   protected:

   TR::X86LinkageProperties _properties;

   public:

   X86PrivateLinkage(TR::CodeGenerator *cg);

   virtual const TR::X86LinkageProperties& getProperties();

   virtual void createPrologue(TR::Instruction *cursor);
   virtual void createEpilogue(TR::Instruction *cursor);
   virtual TR::Register *buildDirectDispatch(TR::Node *callNode, bool spillFPRegs);

   // This interface pre-dates buildCallArguments and could be removed as a cleanup item
   //
   virtual int32_t buildArgs(TR::Node *callNode, TR::RegisterDependencyConditions *dependencies)=0;
   virtual TR::Register *buildIndirectDispatch(TR::Node *callNode);

   // Building the actual call instruction (or equivalent)
   //
   // If entryLabel is NULL, these functions assume the caller will want to run the
   // sequence as-is (ie. the entry point of the sequence is its first instruction).
   // If it's non-NULL, they will use it in a labelInstruction at the start of the
   // call sequence, which may allow them to produce a more efficient sequence.
   //
   // doneLabel (always provided) is placed at the end of the call sequence by the caller of these functions.
   //
   virtual void buildDirectCall(TR::SymbolReference *methodSymRef, TR::X86CallSite &site); // NOTE: the methodSymRef being called is not necessarily site.getSymbolReference()
   virtual void buildVirtualOrComputedCall(TR::X86CallSite &site, TR::LabelSymbol *entryLabel, TR::LabelSymbol *doneLabel, uint8_t *thunk)=0;
   virtual void buildInterfaceCall(TR::X86CallSite &site, TR::LabelSymbol *entryLabel, TR::LabelSymbol *doneLabel, uint8_t *thunk);

   // Building various parts of a call site
   //
   // buildPicSlot and buildVFTCall return the FIRST instruction generated, which is
   // suitable for passing to a TR::X86PatchableCodeAlignmentInstruction if necessary.
   //
   virtual void buildCallArguments(TR::X86CallSite &site);
   virtual bool buildVirtualGuard(TR::X86CallSite &site, TR::LabelSymbol *revirtualizeLabel); // returns false if it can't build the guard
   virtual void buildRevirtualizedCall(TR::X86CallSite &site, TR::LabelSymbol *revirtualizeLabel, TR::LabelSymbol *doneLabel);
   virtual TR::Register *buildCallPostconditions(TR::X86CallSite &site); // Returns the result register if any
   virtual TR::Instruction *buildPICSlot(TR::X86PICSlot picSlot, TR::LabelSymbol *mismatchLabel, TR::LabelSymbol *doneLabel, TR::X86CallSite &site)=0;
   virtual void buildVPIC(TR::X86CallSite &site, TR::LabelSymbol *entryLabel, TR::LabelSymbol *doneLabel);
   virtual void buildIPIC(TR::X86CallSite &site, TR::LabelSymbol *entryLabel, TR::LabelSymbol *doneLabel, uint8_t *thunk)=0;
   virtual TR::Instruction *buildVFTCall(TR::X86CallSite &site, TR_X86OpCode dispatchOp, TR::Register *targetAddressReg, TR::MemoryReference *targetAddressMemref);
   virtual void buildInterfaceDispatchUsingLastITable (TR::X86CallSite &site, int32_t numIPicSlots, TR::X86PICSlot &lastPicSlot, TR::Instruction *&slotPatchInstruction, TR::LabelSymbol *doneLabel, TR::LabelSymbol *lookupDispatchSnippetLabel, TR_OpaqueClassBlock *declaringClass, uintptrj_t itableIndex);

   // Creates a thunk for interpreted virtual calls, used to initialize
   // the vTable slot for the called method.
   //
   virtual uint8_t *generateVirtualIndirectThunk(TR::Node *callNode){ TR_ASSERT(0, "Thunks not implemented in X86Linkage"); return NULL; }
   virtual TR_J2IThunk *generateInvokeExactJ2IThunk(TR::Node *callNode, char *signature){ TR_ASSERT(0, "Thunks not implemented in X86Linkage"); return NULL; }

   struct TR::PicParameters IPicParameters;
   struct TR::PicParameters VPicParameters;

   protected:

   virtual TR::Instruction *savePreservedRegisters(TR::Instruction *cursor)=0;
   virtual TR::Instruction *restorePreservedRegisters(TR::Instruction *cursor)=0;
   virtual bool needsFrameDeallocation();
   virtual TR::Instruction *deallocateFrameIfNeeded(TR::Instruction *cursor, int32_t size);

   void copyLinkageInfoToParameterSymbols();
   void copyGlRegDepsToParameterSymbols(TR::Node *bbStart, TR::CodeGenerator *cg);
   TR::Instruction *copyStackParametersToLinkageRegisters(TR::Instruction *procEntryInstruction);
   TR::Instruction *movLinkageRegisters(TR::Instruction *cursor, bool isStore);
   TR::Instruction *copyParametersToHomeLocation(TR::Instruction *cursor, bool parmsHaveBeenStored);

   };

}

inline TR::X86PrivateLinkage *toX86PrivateLinkage(TR::Linkage *l) {return (TR::X86PrivateLinkage *)l;}

#endif
