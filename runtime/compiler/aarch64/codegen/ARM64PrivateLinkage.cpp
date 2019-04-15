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

TR::ARM64LinkageProperties TR::ARM64PrivateLinkage::properties =
   {
   };

TR::ARM64LinkageProperties& TR::ARM64PrivateLinkage::getProperties()
   {
   return properties;
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

void TR::ARM64PrivateLinkage::initARM64RealRegisterLinkage()
   {
   TR_UNIMPLEMENTED();
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
