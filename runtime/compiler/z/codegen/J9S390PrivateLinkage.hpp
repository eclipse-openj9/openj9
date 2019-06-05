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

#ifndef J9S390PRIVATELINKAGE_INCL
#define J9S390PRIVATELINKAGE_INCL

#include "codegen/Linkage.hpp"

namespace TR { class S390JNICallDataSnippet; }
namespace TR { class AutomaticSymbol; }
namespace TR { class CodeGenerator; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class Snippet; }


namespace TR {

////////////////////////////////////////////////////////////////////////////////
//  TR::S390PrivateLinkage Definition for J9
////////////////////////////////////////////////////////////////////////////////

class S390PrivateLinkage : public TR::Linkage
   {
   uint32_t _preservedRegisterMapForGC;

   TR::RealRegister::RegNum _methodMetaDataRegister;

public:

   S390PrivateLinkage(TR::CodeGenerator * cg, TR_S390LinkageConventions elc=TR_JavaPrivate, TR_LinkageConventions lc=TR_Private);

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

   virtual void initS390RealRegisterLinkage();
   virtual void doNotKillSpecialRegsForBuildArgs (TR::Linkage *linkage, bool isFastJNI, int64_t &killMask);
   virtual void addSpecialRegDepsForBuildArgs(TR::Node * callNode, TR::RegisterDependencyConditions * dependencies, int32_t& from, int32_t step);
   virtual int32_t storeExtraEnvRegForBuildArgs(TR::Node * callNode, TR::Linkage* linkage, TR::RegisterDependencyConditions * dependencies, bool isFastJNI, int32_t stackOffset, int8_t gprSize, uint32_t &numIntegerArgs);
   virtual int64_t addFECustomizedReturnRegDependency(int64_t killMask, TR::Linkage* linkage, TR::DataType resType, TR::RegisterDependencyConditions * dependencies);

   virtual void buildVirtualDispatch(TR::Node * callNode, TR::RegisterDependencyConditions * dependencies,
      TR::Register * vftReg, uint32_t sizeOfArguments);

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

protected:

   virtual TR::Register * buildIndirectDispatch(TR::Node * callNode);
   virtual TR::Register * buildDirectDispatch(TR::Node * callNode);
   TR::Register * buildJNIDispatch(TR::Node * callNode);
   TR::Instruction * buildDirectCall(TR::Node * callNode, TR::SymbolReference * callSymRef,
   TR::RegisterDependencyConditions * dependencies, int32_t argSize);

   virtual void mapIncomingParms(TR::ResolvedMethodSymbol *method);

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
//  TR::S390HelperLinkage Definition for J9
////////////////////////////////////////////////////////////////////////////////

class S390HelperLinkage : public TR::S390PrivateLinkage
   {
public:

   S390HelperLinkage(TR::CodeGenerator * cg)
      : TR::S390PrivateLinkage(cg,TR_JavaHelper, TR_Helper)
      {
      setProperty(ParmsInReverseOrder);
      }
   };

class J9S390JNILinkage : public TR::S390PrivateLinkage
   {
public:

   J9S390JNILinkage(TR::CodeGenerator * cg, TR_S390LinkageConventions elc=TR_JavaPrivate, TR_LinkageConventions lc=TR_J9JNILinkage);
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

#endif /* J9S390PRIVATELINKAGE_INCL */
