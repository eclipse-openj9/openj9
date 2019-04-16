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

#include "codegen/X86HelperLinkage.hpp"

#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "env/VMJ9.h"
#include "x/codegen/X86Instruction.hpp"

// An utility to manage real registers and their corresponding virtual registers
class RealRegisterManager
   {
   public:
   RealRegisterManager(TR::CodeGenerator* cg) :
      _cg(cg),
      _NumberOfRegistersInUse(0)
      {
      memset(_Registers, 0x0, sizeof(_Registers));
      }
   ~RealRegisterManager()
      {
      // RAII: stop using all registers
      for (uint8_t i = (uint8_t)TR::RealRegister::NoReg; i < (uint8_t)TR::RealRegister::NumRegisters; i++)
         {
         if (_Registers[i] != NULL)
            {
            _cg->stopUsingRegister(_Registers[i]);
            }
         }
      }
   // Find or allocate a virtual register which is bind to the given real register
   TR::Register* Use(TR::RealRegister::RegNum RealRegister)
      {
      if (_Registers[RealRegister] == NULL)
         {
         bool isGPR = RealRegister >= TR::RealRegister::FirstGPR && RealRegister <= TR::RealRegister::LastGPR;
         _Registers[RealRegister] = _cg->allocateRegister(isGPR ? TR_GPR : TR_FPR);
         _NumberOfRegistersInUse++;
         }
      return _Registers[RealRegister];
      }
   // Build the dependencies for each virtual-real register pair
   TR::RegisterDependencyConditions* BuildRegisterDependencyConditions()
      {
      TR::RegisterDependencyConditions* deps = generateRegisterDependencyConditions(NumberOfRegistersInUse() + 1, // For VMThread
                                                                                    NumberOfRegistersInUse() + 1, // For VMThread
                                                                                    _cg);
      for (uint8_t i = (uint8_t)TR::RealRegister::NoReg; i < (uint8_t)TR::RealRegister::NumRegisters; i++)
         {
         if (_Registers[i] != NULL)
            {
            deps->addPreCondition(_Registers[i], (TR::RealRegister::RegNum)i, _cg);
            deps->addPostCondition(_Registers[i], (TR::RealRegister::RegNum)i, _cg);
            }
         }
      TR::Register* vmThread = _cg->getVMThreadRegister();
      deps->addPreCondition(vmThread, (TR::RealRegister::RegNum)vmThread->getAssociation(), _cg);
      deps->addPostCondition(vmThread, (TR::RealRegister::RegNum)vmThread->getAssociation(), _cg);
      return deps;
      }
   inline uint8_t NumberOfRegistersInUse() const
      {
      return _NumberOfRegistersInUse;
      }

   private:
   uint8_t            _NumberOfRegistersInUse;
   TR::Register*      _Registers[TR::RealRegister::NumRegisters];
   TR::CodeGenerator* _cg;
};

// On X86-32, fastcall is in-use for both Linux and Windows
// On X86-64 Windows, Microsoft x64 calling convention is an extension of fastcall; and hence this code goes through USE_FASTCALL path
// On X86-64 Linux, the linkage is System V AMD64 ABI.
#if defined(WINDOWS) || defined(TR_TARGET_32BIT)
#define USE_FASTCALL
#endif

const TR::RealRegister::RegNum TR::X86HelperCallSite::IntParamRegisters[] =
   {
#ifdef USE_FASTCALL
   TR::RealRegister::ecx,
   TR::RealRegister::edx,
#ifdef TR_TARGET_64BIT
   TR::RealRegister::r8,
   TR::RealRegister::r9,
#endif
#else
   TR::RealRegister::edi,
   TR::RealRegister::esi,
   TR::RealRegister::edx,
   TR::RealRegister::ecx,
   TR::RealRegister::r8,
   TR::RealRegister::r9,
#endif
   };

const TR::RealRegister::RegNum TR::X86HelperCallSite::CallerSavedRegisters[] =
   {
   TR::RealRegister::eax,
   TR::RealRegister::ecx,
   TR::RealRegister::edx,
#ifndef USE_FASTCALL
   TR::RealRegister::edi,
   TR::RealRegister::esi,
#endif
#ifdef TR_TARGET_64BIT
   TR::RealRegister::r8,
   TR::RealRegister::r9,
   TR::RealRegister::r10,
   TR::RealRegister::r11,
#endif

   TR::RealRegister::xmm0,
   TR::RealRegister::xmm1,
   TR::RealRegister::xmm2,
   TR::RealRegister::xmm3,
   TR::RealRegister::xmm4,
   TR::RealRegister::xmm5,
   TR::RealRegister::xmm6,
   TR::RealRegister::xmm7,
#ifdef TR_TARGET_64BIT
   TR::RealRegister::xmm8,
   TR::RealRegister::xmm9,
   TR::RealRegister::xmm10,
   TR::RealRegister::xmm11,
   TR::RealRegister::xmm12,
   TR::RealRegister::xmm13,
   TR::RealRegister::xmm14,
   TR::RealRegister::xmm15,
#endif
   };

const TR::RealRegister::RegNum TR::X86HelperCallSite::CalleeSavedRegisters[] =
   {
   TR::RealRegister::ebx,
#ifdef USE_FASTCALL
   TR::RealRegister::edi,
   TR::RealRegister::esi,
#endif
   TR::RealRegister::ebp,
   TR::RealRegister::esp,
#ifdef TR_TARGET_64BIT
   TR::RealRegister::r12,
   TR::RealRegister::r13,
   TR::RealRegister::r14,
   TR::RealRegister::r15,
#endif
   };

#undef USE_FASTCALL

// X86-32 requires callee to cleanup stack, X86-64 requires caller to cleanup stack
// Stack slot size is 4 bytes on X86-32 and 8 bytes on X86-64
#ifdef TR_TARGET_32BIT
const bool   TR::X86HelperCallSite::CalleeCleanup = true;
const size_t TR::X86HelperCallSite::StackSlotSize = 4;
#else
const bool   TR::X86HelperCallSite::CalleeCleanup = false;
const size_t TR::X86HelperCallSite::StackSlotSize = 8;
#endif
// Windows X86-64 requires caller reserves shadow space for parameters passed via registers
#if defined(WINDOWS) && defined(TR_TARGET_64BIT)
const bool   TR::X86HelperCallSite::RegisterParameterShadowOnStack = true;
#else
const bool   TR::X86HelperCallSite::RegisterParameterShadowOnStack = false;
#endif
const size_t TR::X86HelperCallSite::NumberOfIntParamRegisters = sizeof(IntParamRegisters) / sizeof(IntParamRegisters[0]);
const size_t TR::X86HelperCallSite::StackIndexAdjustment      = RegisterParameterShadowOnStack ? 0 : NumberOfIntParamRegisters;

// Code below should not need the #define

// Calculate preserved register map.
// The map is a constant that the C++ compiler *should* be able to determine at compile time
static uint32_t X86HelperCallSiteCalculatePreservedRegisterMapForGC()
   {
   uint32_t ret = 0;
   for (size_t i = 0;
        i < sizeof(TR::X86HelperCallSite::CalleeSavedRegisters) / sizeof(TR::X86HelperCallSite::CalleeSavedRegisters[0]);
        i++)
      {
      ret |= TR::RealRegister::gprMask(TR::X86HelperCallSite::CalleeSavedRegisters[i]);
      }
   return ret;
   }
const uint32_t TR::X86HelperCallSite::PreservedRegisterMapForGC = X86HelperCallSiteCalculatePreservedRegisterMapForGC();

TR::Register* TR::X86HelperCallSite::BuildCall()
   {
   TR_ASSERT(CalleeCleanup || cg()->getLinkage()->getProperties().getReservesOutgoingArgsInPrologue(),
             "Caller must reserve outgoing args in prologue unless callee cleans up stack");

   if (cg()->comp()->getOption(TR_TraceCG))
      {
      traceMsg(cg()->comp(), "X86 HelperCall: [%04d] %s\n", _SymRef->getReferenceNumber(), cg()->getDebug()->getName(_SymRef));
      }
   RealRegisterManager RealRegisters(cg());
   TR::RealRegister*   ESP = cg()->machine()->getRealRegister(TR::RealRegister::esp);

   // Preserve caller saved registers
   for (size_t i = 0;
        i < sizeof(CallerSavedRegisters) / sizeof(CallerSavedRegisters[0]);
        i++)
      {
      RealRegisters.Use(CallerSavedRegisters[i]);
      }
   // Find the return register, EAX/RAX
   TR::Register* EAX = RealRegisters.Use(TR::RealRegister::eax);

   // Build parameters, parameters in _Params are Right-to-Left (RTL)
   int NumberOfParamOnStack = 0;
   for (size_t i = 0; i < _Params.size(); i++)
      {
      size_t index = _Params.size() - i - 1;
      if (index < NumberOfIntParamRegisters)
         {
         generateRegRegInstruction(MOVRegReg(),
                                   _Node,
                                   RealRegisters.Use(IntParamRegisters[index]),
                                   _Params[i],
                                   cg());
         }
      else
         {
         NumberOfParamOnStack++;
         if (CalleeCleanup)
            {
            generateRegInstruction(PUSHReg, _Node, _Params[i], cg());
            }
         else
            {
            size_t offset = StackSlotSize * (index - StackIndexAdjustment);
            generateMemRegInstruction(SMemReg(),
                                      _Node,
                                      generateX86MemoryReference(ESP, offset, cg()),
                                      _Params[i],
                                      cg());
            }
         }
      }

   // Call helper
   TR::X86ImmInstruction* instr = generateImmSymInstruction(CALLImm4,
                                                            _Node,
                                                            (uintptrj_t)_SymRef->getMethodAddress(),
                                                            _SymRef,
                                                            RealRegisters.BuildRegisterDependencyConditions(),
                                                            cg());
   instr->setNeedsGCMap(PreservedRegisterMapForGC);

   // Stack adjustment
      {
      int SizeOfParamOnStack = StackSlotSize * (NumberOfParamOnStack + NumberOfIntParamRegisters - StackIndexAdjustment);
      if (CalleeCleanup)
         {
         instr->setAdjustsFramePointerBy(-SizeOfParamOnStack);
         }
      else
         {
         if (SizeOfParamOnStack > cg()->getLargestOutgoingArgSize())
            {
            cg()->setLargestOutgoingArgSize(SizeOfParamOnStack);
            }
         }
      }

   // Return value
   TR::Register* ret = NULL;
   switch (_ReturnType)
      {
      case TR::NoType:
         break;
      case TR::Address:
         ret = cg()->allocateCollectedReferenceRegister();
         break;
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
#ifdef TR_TARGET_64BIT
      case TR::Int64:
#endif
         ret = cg()->allocateRegister();
         break;
      default:
         TR_ASSERT(false, "Unsupported call return data type: #%d", (int)_ReturnType);
         break;
      }
   if (ret)
      {
      generateRegRegInstruction(MOVRegReg(), _Node, ret, EAX, cg());
      }
   return ret;
}
