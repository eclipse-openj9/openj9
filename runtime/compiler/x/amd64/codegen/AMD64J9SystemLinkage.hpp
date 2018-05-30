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

#if !defined(AMD64J9SYSTEMLINKAGE_INCL)
#define AMD64J9SYSTEMLINKAGE_INCL

#if defined(TR_TARGET_64BIT)

#include "x/amd64/codegen/AMD64SystemLinkage.hpp"
#include "codegen/X86Instruction.hpp"

namespace TR { class CodeGenerator; }

#define INCOMPLETELINKAGE  "This class is only used to generate call-out sequence but no call-in sequence, so it is not used as a complete linkage."

namespace TR {

class AMD64J9SystemLinkage: public virtual TR::AMD64SystemLinkage
   {
protected:
   AMD64J9SystemLinkage(TR::CodeGenerator *cg) : TR::AMD64SystemLinkage(cg) {}

   virtual TR::Register *buildDirectDispatch(TR::Node *callNode, bool spillFPRegs);
   virtual TR::Register *buildVolatileAndReturnDependencies(TR::Node *callNode, TR::RegisterDependencyConditions *deps);
   };

class AMD64J9Win64FastCallLinkage: public TR::AMD64J9SystemLinkage,  protected TR::AMD64Win64FastCallLinkage
   {
public:
   AMD64J9Win64FastCallLinkage(TR::CodeGenerator *cg);
protected:
   TR::Register *buildDirectDispatch(TR::Node *callNode, bool spillFPRegs) {return TR::AMD64J9SystemLinkage::buildDirectDispatch(callNode, spillFPRegs);}
   TR::Register *buildVolatileAndReturnDependencies(TR::Node *callNode, TR::RegisterDependencyConditions *deps) {return TR::AMD64J9SystemLinkage::buildVolatileAndReturnDependencies(callNode, deps);}
   };

class AMD64J9ABILinkage : public TR::AMD64J9SystemLinkage, protected TR::AMD64ABILinkage
   {
public:
   AMD64J9ABILinkage(TR::CodeGenerator *cg);
protected:
   TR::Register *buildDirectDispatch(TR::Node *callNode, bool spillFPRegs) {return TR::AMD64J9SystemLinkage::buildDirectDispatch(callNode, spillFPRegs);}
   TR::Register *buildVolatileAndReturnDependencies(TR::Node *callNode, TR::RegisterDependencyConditions *deps) {return TR::AMD64J9SystemLinkage::buildVolatileAndReturnDependencies(callNode, deps);}
   };

}

#endif

#endif
