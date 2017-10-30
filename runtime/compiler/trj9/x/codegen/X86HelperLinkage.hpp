/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#ifndef X86_HELPERLINKAGE_INCL
#define X86_HELPERLINKAGE_INCL

#include "codegen/X86FastCallLinkage.hpp"

namespace TR {

class X86HelperLinkage : public X86FastCallLinkage
   {
   public:
   X86HelperLinkage(TR::CodeGenerator *cg) : X86FastCallLinkage(cg) {}
   virtual TR::Register* buildDirectDispatch(TR::Node* callNode, bool spillFPRegs)
      {
      switch (callNode->getSymbolReference()->getReferenceNumber())
         {
         case TR_checkCast:
         case TR_checkCastForArrayStore:
         case TR_instanceOf:
         case TR_monitorEntry:
         case TR_methodMonitorEntry:
         case TR_monitorExit:
         case TR_methodMonitorExit:
         case TR_newObject:
         case TR_newArray:
         case TR_aNewArray:
         case TR_newObjectNoZeroInit:
         case TR_newArrayNoZeroInit:
         case TR_aNewArrayNoZeroInit:
         case TR_typeCheckArrayStore:
         case TR_jitCheckIfFinalizeObject:
#ifdef TR_TARGET_64BIT
         case TR_AMD64JitMonitorEnterReserved:
         case TR_AMD64JitMonitorEnterReservedPrimitive:
         case TR_AMD64JitMonitorEnterPreservingReservation:
         case TR_AMD64JitMonitorExitReserved:
         case TR_AMD64JitMonitorExitReservedPrimitive:
         case TR_AMD64JitMonitorExitPreservingReservation:
         case TR_AMD64JitMethodMonitorEnterReserved:
         case TR_AMD64JitMethodMonitorEnterReservedPrimitive:
         case TR_AMD64JitMethodMonitorEnterPreservingReservation:
         case TR_AMD64JitMethodMonitorExitPreservingReservation:
         case TR_AMD64JitMethodMonitorExitReservedPrimitive:
         case TR_AMD64JitMethodMonitorExitReserved:
#endif
#ifdef TR_TARGET_32BIT
         case TR_IA32JitMonitorEnterReserved:
         case TR_IA32JitMonitorEnterReservedPrimitive:
         case TR_IA32JitMonitorEnterPreservingReservation:
         case TR_IA32JitMonitorExitReserved:
         case TR_IA32JitMonitorExitReservedPrimitive:
         case TR_IA32JitMonitorExitPreservingReservation:
         case TR_IA32JitMethodMonitorEnterReserved:
         case TR_IA32JitMethodMonitorEnterReservedPrimitive:
         case TR_IA32JitMethodMonitorEnterPreservingReservation:
         case TR_IA32JitMethodMonitorExitPreservingReservation:
         case TR_IA32JitMethodMonitorExitReservedPrimitive:
         case TR_IA32JitMethodMonitorExitReserved:
#endif
            return buildCall(callNode, cg()->getVMThreadRegister());
         default:
            return buildCall(callNode);
         }
      }
   };

}
#endif
