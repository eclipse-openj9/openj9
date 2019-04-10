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

#ifndef ARM64_PRIVATELINKAGE_INCL
#define ARM64_PRIVATELINKAGE_INCL

#include "codegen/Linkage.hpp"

#include "infra/Assert.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class Register; }

namespace TR {

class ARM64PrivateLinkage : public TR::Linkage
   {
   static TR::ARM64LinkageProperties properties;

   public:

   /**
    * @brief Constructor
    * @param[in] cg : CodeGenerator
    */
   ARM64PrivateLinkage(TR::CodeGenerator *cg) : TR::Linkage(cg) {}

   /**
    * @brief Answers linkage properties
    * @return linkage properties
    */
   virtual TR::ARM64LinkageProperties& getProperties();

   /**
    * @brief Gets the RightToLeft flag
    * @return RightToLeft flag
    */
   virtual uint32_t getRightToLeft();

   /**
    * @brief Maps symbols to locations on stack
    * @param[in] method : method for which symbols are mapped on stack
    */
   virtual void mapStack(TR::ResolvedMethodSymbol *method);
   /**
    * @brief Maps an automatic symbol to an index on stack
    * @param[in] p : automatic symbol
    * @param[in/out] stackIndex : index on stack
    */
   virtual void mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t &stackIndex);

   /**
    * @brief Initializes ARM64 RealRegister linkage
    */
   virtual void initARM64RealRegisterLinkage();

   /**
    * @brief Creates method prologue
    * @param[in] cursor : instruction cursor
    */
   virtual void createPrologue(TR::Instruction *cursor);
   /**
    * @brief Creates method prologue
    * @param[in] cursor : instruction cursor
    */
   virtual void createEpilogue(TR::Instruction *cursor);

   /**
    * @brief Builds method arguments
    * @param[in] node : caller node
    * @param[in] dependencies : register dependency conditions
    */
   virtual int32_t buildArgs(
      TR::Node *callNode,
      TR::RegisterDependencyConditions *dependencies);

   /**
    * @brief Builds direct dispatch to method
    * @param[in] node : caller node
    */
   virtual TR::Register *buildDirectDispatch(TR::Node *callNode);

   /**
    * @brief Builds indirect dispatch to method
    * @param[in] node : caller node
    */
   virtual TR::Register *buildIndirectDispatch(TR::Node *callNode);
   };

class ARM64HelperLinkage : public TR::ARM64PrivateLinkage
   {
   public:

   /**
    * @brief Constructor
    * @param[in] cg : CodeGenerator
    */
   ARM64HelperLinkage(TR::CodeGenerator *cg) : TR::ARM64PrivateLinkage(cg) {}
   };

}

#endif
