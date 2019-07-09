/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#include "codegen/ARM64PrivateLinkage.hpp"
#include "codegen/Linkage_inlines.hpp"

TR::ARM64PrivateLinkage::ARM64PrivateLinkage(TR::CodeGenerator *cg)
   : TR::Linkage(cg)
   {
   int32_t i;

   _properties._properties = 0;

   _properties._registerFlags[TR::RealRegister::NoReg] = 0;
   _properties._registerFlags[TR::RealRegister::x0]    = IntegerArgument|IntegerReturn;
   _properties._registerFlags[TR::RealRegister::x1]    = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x2]    = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x3]    = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x4]    = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x5]    = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x6]    = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x7]    = IntegerArgument;

   for (i = TR::RealRegister::x8; i <= TR::RealRegister::x15; i++)
      _properties._registerFlags[i] = 0; // x8 - x15 volatile

   _properties._registerFlags[TR::RealRegister::x16]   = ARM64_Reserved; // IP0
   _properties._registerFlags[TR::RealRegister::x17]   = ARM64_Reserved; // IP1

   _properties._registerFlags[TR::RealRegister::x18]   = Preserved;

   _properties._registerFlags[TR::RealRegister::x19]   = ARM64_Reserved; // vmThread
   _properties._registerFlags[TR::RealRegister::x20]   = ARM64_Reserved; // Java SP

   for (i = TR::RealRegister::x21; i <= TR::RealRegister::x28; i++)
      _properties._registerFlags[i] = Preserved; // x18 - x28 Preserved

   _properties._registerFlags[TR::RealRegister::x29]   = ARM64_Reserved; // FP
   _properties._registerFlags[TR::RealRegister::x30]   = ARM64_Reserved; // LR
   _properties._registerFlags[TR::RealRegister::sp]    = ARM64_Reserved;
   _properties._registerFlags[TR::RealRegister::xzr]   = ARM64_Reserved;

   _properties._registerFlags[TR::RealRegister::v0]    = FloatArgument|FloatReturn;
   _properties._registerFlags[TR::RealRegister::v1]    = FloatArgument;
   _properties._registerFlags[TR::RealRegister::v2]    = FloatArgument;
   _properties._registerFlags[TR::RealRegister::v3]    = FloatArgument;
   _properties._registerFlags[TR::RealRegister::v4]    = FloatArgument;
   _properties._registerFlags[TR::RealRegister::v5]    = FloatArgument;
   _properties._registerFlags[TR::RealRegister::v6]    = FloatArgument;
   _properties._registerFlags[TR::RealRegister::v7]    = FloatArgument;

   for (i = TR::RealRegister::v8; i <= TR::RealRegister::LastFPR; i++)
      _properties._registerFlags[i] = 0; // v8 - v31 volatile

   _properties._numIntegerArgumentRegisters  = 8;
   _properties._firstIntegerArgumentRegister = 0;
   _properties._numFloatArgumentRegisters    = 8;
   _properties._firstFloatArgumentRegister   = 8;

   _properties._argumentRegisters[0]  = TR::RealRegister::x0;
   _properties._argumentRegisters[1]  = TR::RealRegister::x1;
   _properties._argumentRegisters[2]  = TR::RealRegister::x2;
   _properties._argumentRegisters[3]  = TR::RealRegister::x3;
   _properties._argumentRegisters[4]  = TR::RealRegister::x4;
   _properties._argumentRegisters[5]  = TR::RealRegister::x5;
   _properties._argumentRegisters[6]  = TR::RealRegister::x6;
   _properties._argumentRegisters[7]  = TR::RealRegister::x7;
   _properties._argumentRegisters[8]  = TR::RealRegister::v0;
   _properties._argumentRegisters[9]  = TR::RealRegister::v1;
   _properties._argumentRegisters[10] = TR::RealRegister::v2;
   _properties._argumentRegisters[11] = TR::RealRegister::v3;
   _properties._argumentRegisters[12] = TR::RealRegister::v4;
   _properties._argumentRegisters[13] = TR::RealRegister::v5;
   _properties._argumentRegisters[14] = TR::RealRegister::v6;
   _properties._argumentRegisters[15] = TR::RealRegister::v7;

   _properties._firstIntegerReturnRegister = 0;
   _properties._firstFloatReturnRegister   = 1;

   _properties._returnRegisters[0]  = TR::RealRegister::x0;
   _properties._returnRegisters[1]  = TR::RealRegister::v0;

   _properties._numAllocatableIntegerRegisters = 25;
   _properties._numAllocatableFloatRegisters   = 32;

   _properties._preservedRegisterMapForGC   = 0x1fe40000;
   _properties._methodMetaDataRegister      = TR::RealRegister::x19;
   _properties._stackPointerRegister        = TR::RealRegister::x20;
   _properties._framePointerRegister        = TR::RealRegister::x29;
   _properties._vtableIndexArgumentRegister = TR::RealRegister::x9;
   _properties._j9methodArgumentRegister    = TR::RealRegister::x0;
   _properties._numberOfDependencyGPRegisters = 32;
   _properties._offsetToFirstParm             = 0; // To be determined
   _properties._offsetToFirstLocal            = 0; // To be determined
   }

TR::ARM64LinkageProperties& TR::ARM64PrivateLinkage::getProperties()
   {
   return _properties;
   }

uint32_t TR::ARM64PrivateLinkage::getRightToLeft()
   {
   return getProperties().getRightToLeft();
   }

void TR::ARM64PrivateLinkage::mapStack(TR::ResolvedMethodSymbol *method)
   {
   TR_UNIMPLEMENTED();
   }

void TR::ARM64PrivateLinkage::mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t &stackIndex)
   {
   TR_UNIMPLEMENTED();
   }

static void lockRegister(TR::RealRegister *regToAssign)
   {
   regToAssign->setState(TR::RealRegister::Locked);
   regToAssign->setAssignedRegister(regToAssign);
   }

void TR::ARM64PrivateLinkage::initARM64RealRegisterLinkage()
   {
   TR::Machine *machine = cg()->machine();
   TR::RealRegister *reg;
   int icount;

   reg = machine->getRealRegister(TR::RealRegister::RegNum::x16); // IP0
   lockRegister(reg);

   reg = machine->getRealRegister(TR::RealRegister::RegNum::x17); // IP1
   lockRegister(reg);

   reg = machine->getRealRegister(TR::RealRegister::RegNum::x19); // vmThread
   lockRegister(reg);

   reg = machine->getRealRegister(TR::RealRegister::RegNum::x20); // Java SP
   lockRegister(reg);

   reg = machine->getRealRegister(TR::RealRegister::RegNum::x29); // FP
   lockRegister(reg);

   reg = machine->getRealRegister(TR::RealRegister::RegNum::x30); // LR
   lockRegister(reg);

   reg = machine->getRealRegister(TR::RealRegister::RegNum::sp); // SP
   lockRegister(reg);

   // assign "maximum" weight to registers x0-x15
   for (icount = TR::RealRegister::x0; icount <= TR::RealRegister::x15; icount++)
      machine->getRealRegister((TR::RealRegister::RegNum)icount)->setWeight(0xf000);

   // assign "maximum" weight to registers x18 and x21-x28
   machine->getRealRegister(TR::RealRegister::RegNum::x18)->setWeight(0xf000);
   for (icount = TR::RealRegister::x20; icount <= TR::RealRegister::x28; icount++)
      machine->getRealRegister((TR::RealRegister::RegNum)icount)->setWeight(0xf000);

   // assign "maximum" weight to registers v0-v31
   for (icount = TR::RealRegister::v0; icount <= TR::RealRegister::v31; icount++)
      machine->getRealRegister((TR::RealRegister::RegNum)icount)->setWeight(0xf000);
   }

void TR::ARM64PrivateLinkage::createPrologue(TR::Instruction *cursor)
   {
   TR_UNIMPLEMENTED();
   }

void TR::ARM64PrivateLinkage::createEpilogue(TR::Instruction *cursor)
   {
   TR_UNIMPLEMENTED();
   }

int32_t TR::ARM64PrivateLinkage::buildArgs(TR::Node *callNode,
   TR::RegisterDependencyConditions *dependencies)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

TR::Register *TR::ARM64PrivateLinkage::buildDirectDispatch(TR::Node *callNode)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

TR::Register *TR::ARM64PrivateLinkage::buildIndirectDispatch(TR::Node *callNode)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }
