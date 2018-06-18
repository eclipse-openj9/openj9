/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef PPC_JNILINKAGE_INCL
#define PPC_JNILINKAGE_INCL

#include "p/codegen/PPCPrivateLinkage.hpp"

#include "codegen/Linkage.hpp"
#include "infra/Assert.hpp"

extern "C"
   {
   unsigned int crc32_oneByte(unsigned int crc, unsigned int b);
   unsigned int crc32_no_vpmsum(unsigned int crc, unsigned char *p, unsigned long len);
   unsigned int crc32_vpmsum(unsigned int crc, unsigned char *p, unsigned long len);
   }

class TR_BitVector;
namespace TR { class AutomaticSymbol; }
namespace TR { class CodeGenerator; }
namespace TR { class MemoryReference; }
namespace TR { class Node; }
namespace TR { class ParameterSymbol; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class ResolvedMethodSymbol; }

namespace TR {

class PPCJNILinkage : public TR::PPCPrivateLinkage
   {
   protected:
   TR::PPCLinkageProperties _properties;

   public:
   PPCJNILinkage(TR::CodeGenerator *cg);

   void releaseVMAccess(TR::Node* callNode, TR::RegisterDependencyConditions* deps, TR::RealRegister* metaReg, TR::Register* zeroReg, TR::Register* tempReg1, TR::Register* tempReg2);
   void acquireVMAccess(TR::Node* callNode, TR::RegisterDependencyConditions* deps, TR::RealRegister* metaReg, TR::Register* tempReg0, TR::Register* tempReg1, TR::Register* tempReg2);
#ifdef J9VM_INTERP_ATOMIC_FREE_JNI
   void releaseVMAccessAtomicFree(TR::Node* callNode, TR::RegisterDependencyConditions* deps, TR::RealRegister* metaReg, TR::Register* cr0Reg, TR::Register* tempReg1, TR::Register* tempReg2);
   void acquireVMAccessAtomicFree(TR::Node* callNode, TR::RegisterDependencyConditions* deps, TR::RealRegister* metaReg, TR::Register* cr0Reg, TR::Register* tempReg1, TR::Register* tempReg2);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */

   virtual int32_t buildJNIArgs(TR::Node *callNode,
                             TR::RegisterDependencyConditions *dependencies,
                             const TR::PPCLinkageProperties &properties,
                             bool isFastJNI = true,
                             bool passReceiver = true,
                             bool implicitEnvArg = true);

   TR::Register *pushJNIReferenceArg(TR::Node *child);

   virtual TR::Register *buildDirectDispatch(TR::Node *callNode);
   virtual int32_t      buildArgs( TR::Node *callNode,
                                   TR::RegisterDependencyConditions *dependencies,
                                   const TR::PPCLinkageProperties &properties);

   virtual TR::Register *buildIndirectDispatch(TR::Node *callNode);
   virtual void         buildVirtualDispatch( TR::Node                          *callNode,
                                              TR::RegisterDependencyConditions *dependencies,
                                              uint32_t                           sizeOfArguments);

   virtual const TR::PPCLinkageProperties& getProperties();
   };

}

#endif //PPC_JNILINKAGE_INCL
