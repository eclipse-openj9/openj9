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

#ifndef J9_Z_SYSTEMLINKAGEZOS_INCL
#define J9_Z_SYSTEMLINKAGEZOS_INCL

#include "z/codegen/SystemLinkage.hpp"
#include "z/codegen/SystemLinkagezOS.hpp"

#include <stdint.h>
#include "env/jittypes.h"

namespace TR { class CodeGenerator; }
namespace TR { class Linkage; }
namespace TR { class Register; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class SystemLinkage; }

namespace TR {

class J9S390zOSSystemLinkage: public TR::S390zOSSystemLinkage
   {
public:
   J9S390zOSSystemLinkage(TR::CodeGenerator * cg);

   virtual void generateInstructionsForCall(TR::Node * callNode, TR::RegisterDependencyConditions * dependencies,
         intptrj_t targetAddress, TR::Register * methodAddressReg, TR::Register * javaLitOffsetReg, TR::LabelSymbol * returnFromJNICallLabel,
         TR::S390JNICallDataSnippet * jniCallDataSnippet, bool isJNIGCPoint = true);



   // this set of 3 method called by buildNative Dispatch is the same as the J9S390zLinux methods above
   // omr todo: need to find a better way instead of duplicating them
   virtual void setupRegisterDepForLinkage(TR::Node *, TR_DispatchType, TR::RegisterDependencyConditions * &,
         int64_t &, TR::SystemLinkage *, TR::Node * &, bool &, TR::Register **, TR::Register *&);

   virtual void setupBuildArgForLinkage(TR::Node *, TR_DispatchType, TR::RegisterDependencyConditions *, bool, bool,
         int64_t &, TR::Node *, bool, TR::SystemLinkage *);

   virtual void performCallNativeFunctionForLinkage(TR::Node *, TR_DispatchType, TR::Register *&, TR::SystemLinkage *,
         TR::RegisterDependencyConditions *&, TR::Register *, TR::Register *, bool);

   //called by buildArgs, same as J9S390zLinux methods above
   // omr todo: need to find a better way instead of duplicating them
   virtual void doNotKillSpecialRegsForBuildArgs (TR::Linkage *linkage, bool isFastJNI, int64_t &killMask);
   virtual void addSpecialRegDepsForBuildArgs(TR::Node * callNode, TR::RegisterDependencyConditions * dependencies, int32_t& from, int32_t step);
   virtual int64_t addFECustomizedReturnRegDependency(int64_t killMask, TR::Linkage* linkage, TR::DataType resType, TR::RegisterDependencyConditions * dependencies);
   virtual int32_t storeExtraEnvRegForBuildArgs(TR::Node * callNode, TR::Linkage* linkage, TR::RegisterDependencyConditions * dependencies,
         bool isFastJNI, int32_t stackOffset, int8_t gprSize, uint32_t &numIntegerArgs);
   };

}

#endif
