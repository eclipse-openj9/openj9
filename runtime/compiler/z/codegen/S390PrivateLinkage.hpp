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

#ifndef S390PRIVATELINKAGE_INCL
#define S390PRIVATELINKAGE_INCL

#include "codegen/PrivateLinkage.hpp"
#include "codegen/Register.hpp"

namespace TR { class S390JNICallDataSnippet; }
namespace TR { class AutomaticSymbol; }
namespace TR { class CodeGenerator; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class Snippet; }


namespace J9
{

namespace Z
{

////////////////////////////////////////////////////////////////////////////////
//  J9::Z::PrivateLinkage Definition for J9
////////////////////////////////////////////////////////////////////////////////

class PrivateLinkage : public J9::PrivateLinkage
   {
   uint32_t _preservedRegisterMapForGC;

   TR::RealRegister::RegNum _methodMetaDataRegister;

public:

   PrivateLinkage(TR::CodeGenerator * cg, TR_S390LinkageConventions elc=TR_JavaPrivate, TR_LinkageConventions lc=TR_Private);

   virtual void createPrologue(TR::Instruction * cursor);
   virtual void createEpilogue(TR::Instruction * cursor);

   /** \brief
    *     Align the stackIndex to a multiple of localObjectAlignment and update the numberOfSlotMapped.
    *
    *  \param stackIndex
    *     The current stack index to be aligned to \p localObjectAlignment.
    *
    *  \param localObjectAlignment
    *     The stack object alignment.
    */
   void alignLocalsOffset(uint32_t &stackIndex, uint32_t localObjectAlignment);

   void         mapCompactedStack(TR::ResolvedMethodSymbol * symbol);
   virtual void mapStack(TR::ResolvedMethodSymbol * symbol);
   virtual void mapSingleAutomatic(TR::AutomaticSymbol * p, uint32_t & stackIndex);
   void         mapSingleAutomatic(TR::AutomaticSymbol * p, uint32_t size, uint32_t & stackIndex);
   uint32_t     getS390RoundedSize(uint32_t size);
   virtual bool hasToBeOnStack(TR::ParameterSymbol * parm);
   virtual void setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol * method);
   virtual void setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method, List<TR::ParameterSymbol>&parmList);

   virtual void initS390RealRegisterLinkage();
   virtual void doNotKillSpecialRegsForBuildArgs (TR::Linkage *linkage, bool isFastJNI, int64_t &killMask);
   virtual void addSpecialRegDepsForBuildArgs(TR::Node * callNode, TR::RegisterDependencyConditions * dependencies, int32_t& from, int32_t step);
   virtual int32_t storeExtraEnvRegForBuildArgs(TR::Node * callNode, TR::Linkage* linkage, TR::RegisterDependencyConditions * dependencies, bool isFastJNI, int32_t stackOffset, int8_t gprSize, uint32_t &numIntegerArgs);
   virtual int64_t addFECustomizedReturnRegDependency(int64_t killMask, TR::Linkage* linkage, TR::DataType resType, TR::RegisterDependencyConditions * dependencies);

   virtual void buildVirtualDispatch(TR::Node * callNode, TR::RegisterDependencyConditions * dependencies,
      TR::Register * vftReg, uint32_t sizeOfArguments);
   
   virtual TR::Instruction* buildNoPatchingVirtualDispatchWithResolve(TR::Node *callNode, TR::RegisterDependencyConditions *deps,
      TR::LabelSymbol *doneLabel, intptr_t virtualThunk);

   virtual TR::Instruction *buildNoPatchingIPIC(TR::Node *callNode, TR::RegisterDependencyConditions *deps, TR::Register *vftReg, intptr_t virtualThunk);
   virtual TR::RealRegister::RegNum setMethodMetaDataRegister(TR::RealRegister::RegNum r) { return _methodMetaDataRegister = r; }
   virtual TR::RealRegister::RegNum getMethodMetaDataRegister() { return _methodMetaDataRegister; }
   virtual TR::RealRegister *getMethodMetaDataRealRegister() {return getRealRegister(_methodMetaDataRegister);}

   virtual uint32_t setPreservedRegisterMapForGC(uint32_t m)  { return _preservedRegisterMapForGC = m; }
   virtual uint32_t getPreservedRegisterMapForGC()        { return _preservedRegisterMapForGC; }

   virtual TR::RealRegister::RegNum getSystemStackPointerRegister();
   virtual TR::RealRegister *getSystemStackPointerRealRegister() {return getRealRegister(getSystemStackPointerRegister());}

   virtual int32_t setupLiteralPoolRegister(TR::Snippet *firstSnippet);

   //called by buildNativeDispatch
   virtual void setupRegisterDepForLinkage(TR::Node *, TR_DispatchType, TR::RegisterDependencyConditions * &, int64_t &, TR::SystemLinkage *, TR::Node * &, bool &, TR::Register **, TR::Register *&);
   virtual void setupBuildArgForLinkage(TR::Node *, TR_DispatchType, TR::RegisterDependencyConditions *, bool, bool, int64_t &, TR::Node *, bool, TR::SystemLinkage *);

   virtual int32_t calculateRegisterSaveSize(TR::RealRegister::RegNum f,
                                             TR::RealRegister::RegNum l,
                                             int32_t &rsd,
                                             int32_t &numInts, int32_t &numFloats);

   /**
    * For the interface dispatch that does not patch the code cache, following data structure
    * describes the metadata that is necessary in order for a patchless dispatch sequence to work.
    * The structure must be allocated in data area that can be written to at runtime also the structure should be
    * aligned to double word boundary to make sure that we can load the Class and Method Address in single Paired Quadword
    * instruction.
    * In Regular JIT Compiled code which can be patched, we have a liberty of chnaging of number of dynamic
    * cache slots we have for interface call at compile time.
    * TODO: For patchless code, we have to fix number of dynamic cache slots to 3 which is default at the moment for 
    * regular JIT code, for patchless code, we should provide a feature to change the number of dynamic cache slots at
    * compilation time same as we can do in the regular JIT compiled code. One way we can achieve this is by decoupling of
    * following structure into two parts, one contains the metadata for the interface call and other contains the array of
    * cached dynamic cached slots.
    */
   struct ccInterfaceData
      {
      intptr_t slot1Class;
      intptr_t slot1Method;
      intptr_t slot2Class;
      intptr_t slot2Method;
      intptr_t slot3Class;
      intptr_t slot3Method;
      intptr_t cpAddress;
      intptr_t cpIndex;
      intptr_t interfaceClass;
      intptr_t iTableIndex;
      intptr_t j2iThunkAddress;
      intptr_t totalSizeOfPicSlots;
      intptr_t isCacheFull;
      static const int8_t numberOfPICSlots = 3; 
      };
protected:

   virtual TR::Register * buildIndirectDispatch(TR::Node * callNode);
   virtual TR::Register * buildDirectDispatch(TR::Node * callNode);
   TR::Register * buildJNIDispatch(TR::Node * callNode);
   TR::Instruction * buildDirectCall(TR::Node * callNode, TR::SymbolReference * callSymRef,
   TR::RegisterDependencyConditions * dependencies, int32_t argSize);

   void callPreJNICallOffloadCheck(TR::Node * callNode);
   void callPostJNICallOffloadCheck(TR::Node * callNode);
   void collapseJNIReferenceFrame(TR::Node * callNode, TR::RealRegister * javaStackPointerRealRegister,
      TR::Register * javaLitPoolVirtualRegister, TR::Register * tempReg);

   void setupJNICallOutFrame(TR::Node * callNode,
      TR::RealRegister * javaStackPointerRealRegister,
      TR::Register * methodMetaDataVirtualRegister,
      TR::LabelSymbol * returnFromJNICallLabel,
      TR::S390JNICallDataSnippet *jniCallDataSnippet);

   };


////////////////////////////////////////////////////////////////////////////////
//  J9::Z::HelperLinkage Definition for J9
////////////////////////////////////////////////////////////////////////////////

class HelperLinkage : public PrivateLinkage
   {
public:

   HelperLinkage(TR::CodeGenerator * cg)
      : PrivateLinkage(cg,TR_JavaHelper, TR_Helper)
      {
      setProperty(ParmsInReverseOrder);
      }
   };


class JNILinkage : public PrivateLinkage
   {
public:

   JNILinkage(TR::CodeGenerator * cg, TR_S390LinkageConventions elc=TR_JavaPrivate, TR_LinkageConventions lc=TR_J9JNILinkage);
   virtual TR::Register * buildDirectDispatch(TR::Node * callNode);

   /**
    * \brief
    *   JNI return value processing:
    *   1) Unwrap return value if needed for object return types, or
    *   2) Enforce a return value of 0 or 1 for boolean return type
    *
    * \param callNode
    *   The JNI call node to be evaluated.
    *
    * \param cg
    *   The code generator object.
    *
    * \param javaReturnRegister
    *   Register for the JNI call return value.
   */
   void processJNIReturnValue(TR::Node * callNode,
                              TR::CodeGenerator* cg,
                              TR::Register* javaReturnRegister);

   void checkException(TR::Node * callNode, TR::Register *methodMetaDataVirtualRegister, TR::Register * tempReg);
   void releaseVMAccessMask(TR::Node * callNode, TR::Register * methodMetaDataVirtualRegister,
         TR::Register * methodAddressReg, TR::Register * javaLitOffsetReg, TR::S390JNICallDataSnippet * jniCallDataSnippet, TR::RegisterDependencyConditions * deps);
   void acquireVMAccessMask(TR::Node * callNode, TR::Register * javaLitPoolVirtualRegister,
      TR::Register * methodMetaDataVirtualRegister, TR::Register * methodAddressReg, TR::Register * javaLitOffsetReg);

#ifdef J9VM_INTERP_ATOMIC_FREE_JNI
   void releaseVMAccessMaskAtomicFree(TR::Node * callNode,
                                      TR::Register * methodMetaDataVirtualRegister,
                                      TR::Register * tempReg1);

   void acquireVMAccessMaskAtomicFree(TR::Node * callNode,
                                      TR::Register * methodMetaDataVirtualRegister,
                                      TR::Register * tempReg1);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
   };

}

}

#endif /* S390PRIVATELINKAGE_INCL */
