/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#ifndef ARM64_JNILINKAGE_INCL
#define ARM64_JNILINKAGE_INCL

#include "codegen/ARM64PrivateLinkage.hpp"
#include "codegen/Register.hpp"

namespace J9
{

namespace ARM64
{

class JNILinkage : public PrivateLinkage
   {
   public:

   JNILinkage(TR::CodeGenerator *cg);

   /**
    * @brief Builds method arguments
    * @param[in] callNode : caller node
    * @param[in] dependencies : register dependency conditions
    * @return size of arguments
    */
   virtual int32_t buildArgs(
      TR::Node *callNode,
      TR::RegisterDependencyConditions *dependencies);

   /**
    * @brief Builds direct dispatch to method
    * @param[in] callNode : caller node
    * @return register holding return value
    */
   virtual TR::Register *buildDirectDispatch(TR::Node *callNode);

   /**
    * @brief Builds indirect dispatch to method
    * @param[in] callNode : caller node
    * @return register holding return value
    */
   virtual TR::Register *buildIndirectDispatch(TR::Node *callNode);

   /**
    * @brief Builds virtual dispatch to method
    * @param[in] callNode : caller node
    * @param[in] dependencies : register dependency conditions
    * @param[in] argSize : size of arguments
    */
   virtual void buildVirtualDispatch(
      TR::Node *callNode,
      TR::RegisterDependencyConditions *dependencies,
      uint32_t argSize);

   private:

   /**
    * @brief Releases VM access before calling into JNI method
    * @param[in] callNode : caller node
    * @param[in] vmThreadReg : vm thread register
    * @param[in] scratchReg0 : scratch register
    * @param[in] scratchReg1 : scratch register
    * @param[in] scratchReg2 : scratch register
    * @param[in] scratchReg3 : scratch register
    */
   void releaseVMAccess(TR::Node *callNode, TR::Register *vmThreadReg, TR::Register *scratchReg0, TR::Register *scratchReg1, TR::Register *scratchReg2, TR::Register *scratchReg3);

   /**
    * @brief Acquires VM access after returned from JNI method
    * @param[in] callNode : caller node
    * @param[in] vmThreadReg : vm thread register
    * @param[in] scratchReg0 : scratch register
    * @param[in] scratchReg1 : scratch register
    * @param[in] scratchReg2 : scratch register
    */
   void acquireVMAccess(TR::Node *callNode, TR::Register *vmThreadReg, TR::Register *scratchReg0, TR::Register *scratchReg1, TR::Register *scratchReg2);

#ifdef J9VM_INTERP_ATOMIC_FREE_JNI
   /**
    * @brief Releases VM access in a way described in OpenJ9 issue 2576
    * @param[in] callNode : caller Node
    * @param[in] vmThreadReg : vm thread register
    */
   void releaseVMAccessAtomicFree(TR::Node *callNode, TR::Register *vmThreadReg);

   /**
    * @brief Acquires VM access in a way described in OpenJ9 issue 2576
    * @param[in] callNode : caller Node
    * @param[in] vmThreadReg : vm thread register
    */
   void acquireVMAccessAtomicFree(TR::Node *callNode, TR::Register *vmThreadReg);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */

   /**
    * @brief Builds JNI call out frame
    * @param[in] callNode : caller node
    * @param[in] isWrapperForJNI : true if the current method is simply a wrapper for the JNI call
    * @param[in] returnAddrLabel : label symbol of return address from JNI method
    * @param[in] vmThreadReg : vm thread register
    * @param[in] javaStackReg : java stack register
    * @param[in] scratchReg0 : scratch register
    * @param[in] scratchReg1 : scratch register
    */
   void buildJNICallOutFrame(TR::Node *callNode, bool isWrapperForJNI,
                             TR::LabelSymbol *returnAddrLabel, TR::Register *vmThreadReg,
                             TR::Register *javaStackReg, TR::Register *scratchReg0, TR::Register *scratchReg1);

   /**
    * @brief Restores JNI call out frame
    * @param[in] callNode : caller node
    * @param[in] tearDownJNIFrame : true if we need to clean up ref pool
    * @param[in] vmThreadReg : vm thread register
    * @param[in] javaStackReg : java stack register
    * @param[in] scratchReg : scratch register
    */
   void restoreJNICallOutFrame(TR::Node *callNode, bool tearDownJNIFrame, TR::Register *vmThreadReg, TR::Register *javaStackReg, TR::Register *scratchReg);

   /**
    * @brief Builds JNI method arguments
    * @param[in] callNode : caller node
    * @param[in] deps : register dependency conditions
    * @param[in] passThread : true if we need to pass vm thread as first parameter
    * @param[in] passReceiver : true if we need to pass receiver to the JNI method
    * @param[in] killNonVolatileGPRs : true if we need to kill non-volatile GPRs
    * @return the total size in bytes allocated on stack for parameter passing
    */
   size_t buildJNIArgs(TR::Node *callNode, TR::RegisterDependencyConditions *deps, bool passThread, bool passReceiver, bool killNonVolatileGPRs);

   /**
    * @brief Returns register holding resturn value
    * @param[in] callNode : caller node
    * @param[in] deps : register dependency conditions
    * @return register holding return value. Null if return type is void.
    */
   TR::Register *getReturnRegisterFromDeps(TR::Node *callNode, TR::RegisterDependencyConditions *deps);

   /**
    * @brief Pushes a JNI reference argument
    * @param[in] child : argument node
    * @return register holding argument
    */
   TR::Register *pushJNIReferenceArg(TR::Node *child);

   /**
    * @brief Generates a JNI method dispatch instructions
    * @param[in] callNode : caller node
    * @param[in] isJNIGCPoint : true if the JNI method dispatch is GC point
    * @param[in] deps : register dependency conditions
    * @param[in] targetAddress : address of JNI method
    * @param[in] scratchReg : scratch register
    * @return instruction of method dispatch
    */
   TR::Instruction *generateMethodDispatch(TR::Node *callNode, bool isJNIGCPoint, TR::RegisterDependencyConditions *deps, uintptr_t targetAddress, TR::Register *scratchReg);

   /**
    * @brief Adjusts return value to java semantics
    * @param[in] callNode : caller node
    * @param[in] wrapRefs : true if the reference is wrapped
    * @param[in] returnRegister : register that holds return value
    */
   void adjustReturnValue(TR::Node *callNode, bool wrapRefs, TR::Register *returnRegister);

   /**
    * @brief Throws exception if it is set in JNI method
    * @param[in] callNode : caller node
    * @param[in] vmThreadReg : vm thread register
    * @param[in] scratchReg : scratch register
    */
   void checkForJNIExceptions(TR::Node *callNode, TR::Register *vmThreadReg, TR::Register *scratchReg);

   TR::Linkage *_systemLinkage;



   };

}

}

#endif
