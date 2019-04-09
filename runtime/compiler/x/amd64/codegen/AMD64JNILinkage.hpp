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

#ifndef AMD64_JNILINKAGE_INCL
#define AMD64_JNILINKAGE_INCL

#ifdef TR_TARGET_64BIT

#include "x/amd64/codegen/AMD64SystemLinkage.hpp"

#include "codegen/AMD64PrivateLinkage.hpp"
#include "env/jittypes.h"

namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }
namespace TR { class RegisterDependencyConditions; }

#define IMCOMPLETELINKAGE  "This class is only used to generate call-out sequence but no call-in sequence, so it is not used as a complete linkage."

namespace TR {

class AMD64JNILinkage : public TR::AMD64PrivateLinkage
   {
   public:

   AMD64JNILinkage(TR::AMD64SystemLinkage *systemLinkage, TR::CodeGenerator *cg) :
      TR::AMD64PrivateLinkage(cg),
         _systemLinkage(systemLinkage) {}

   int32_t computeMemoryArgSize(TR::Node *callNode, int32_t first, int32_t last, bool passThread = true);
   int32_t buildArgs(TR::Node *callNode, TR::RegisterDependencyConditions *deps, bool passThread = true, bool passReceiver = true);
   TR::Register *buildVolatileAndReturnDependencies(TR::Node *callNode, TR::RegisterDependencyConditions *deps, bool omitDedicatedFrameRegister);
   void switchToMachineCStack(TR::Node *callNode);
   void switchToJavaStack(TR::Node *callNode);
   void cleanupReturnValue(TR::Node *callNode, TR::Register *linkageReturnReg, TR::Register *targetReg);

   TR::Register *buildDirectDispatch(TR::Node *callNode, bool spillFPRegs);

   void buildJNICallOutFrame(TR::Node *callNode, TR::LabelSymbol *returnAddrLabel);
   void buildJNIMergeLabelDependencies(TR::Node *callNode, bool killNonVolatileGPRs = true);
   void buildOutgoingJNIArgsAndDependencies(TR::Node *callNode, bool passThread = true, bool passReceiver = true, bool killNonVolatileGPRs = true);
   TR::Register *processJNIReferenceArg(TR::Node *child);
   TR::Instruction *generateMethodDispatch(TR::Node *callNode, bool isJNIGCPoint = true, uintptrj_t targetAddress = 0);
   void releaseVMAccess(TR::Node *callNode);
   void acquireVMAccess(TR::Node *callNode);
#ifdef J9VM_INTERP_ATOMIC_FREE_JNI
   void releaseVMAccessAtomicFree(TR::Node *callNode);
   void acquireVMAccessAtomicFree(TR::Node *callNode);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
   void checkForJNIExceptions(TR::Node *callNode);
   void cleanupJNIRefPool(TR::Node *callNode);
   void populateJNIDispatchInfo();

   private:

   TR::Register *buildDirectJNIDispatch(TR::Node *callNode);
   TR::AMD64SystemLinkage *_systemLinkage;

   class TR_JNIDispatchInfo
      {
      public:

      int32_t numJNIFrameSlotsPushed;
      int32_t argSize;
      bool requiresFPstackPop;

      TR::Register *JNIReturnRegister;
      TR::Register *linkageReturnRegister;
      TR::Register *dispatchTrampolineRegister;
      TR::RealRegister::RegNum dedicatedFrameRegisterIndex;

      TR::RegisterDependencyConditions *callPreDeps;
      TR::RegisterDependencyConditions *callPostDeps;
      TR::RegisterDependencyConditions *mergeLabelPostDeps;
      } _JNIDispatchInfo;

   };

}

#endif

#endif
