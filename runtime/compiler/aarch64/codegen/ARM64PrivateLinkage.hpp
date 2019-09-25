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
namespace TR { class ResolvedMethodSymbol; }

namespace TR {

class ARM64PrivateLinkage : public TR::Linkage
   {
   protected:

   TR::ARM64LinkageProperties _properties;

   /**
    * @brief Builds method arguments
    * @param[in] node : caller node
    * @param[in] dependencies : register dependency conditions
    * @param[in] linkage : linkage type
    */
   int32_t buildPrivateLinkageArgs(
         TR::Node *callNode,
         TR::RegisterDependencyConditions *dependencies,
         TR_LinkageConventions linkage);

   public:

   /**
    * @brief Constructor
    * @param[in] cg : CodeGenerator
    */
   ARM64PrivateLinkage(TR::CodeGenerator *cg);

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
    * @brief Copy linkage register indices to parameter symbols
    *
    * @param[in] method : the resolved method symbol
    */
   void setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method);

   /**
    * @brief Creates method prologue
    * @param[in] cursor : instruction cursor
    */
   virtual void createPrologue(TR::Instruction *cursor);
   /**
    * @brief Creates method epilogue
    * @param[in] cursor : instruction cursor
    */
   virtual void createEpilogue(TR::Instruction *cursor);

   /**
    * @brief Loads all parameters passed on the stack from the interpreter into
    *        the corresponding JITed method private linkage register.
    *
    * @param[in] cursor : the TR::Instruction to begin generating
    *        the load sequence at.
    *
    * @return : the instruction cursor after the load sequence
    */
   TR::Instruction *loadStackParametersToLinkageRegisters(TR::Instruction *cursor);

   /**
    * @brief Stores parameters passed in linkage registers to the stack where the
    *        method body expects to find them.
    *
    * @param[in] cursor : the instruction cursor to begin inserting copy instructions
    *
    * @return The instruction cursor after copies inserted.
    */
   TR::Instruction *copyParametersToHomeLocation(TR::Instruction *cursor);

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
    * @brief Builds direct call to method
    * @param[in] node : caller node
    * @param[in] callSymRef : target symbol reference
    * @param[in] dependencies : register dependency conditions
    * @param[in] pp : linkage properties
    * @param[in] argSize : size of arguments
    */
   void buildDirectCall(
      TR::Node *callNode,
      TR::SymbolReference *callSymRef,
      TR::RegisterDependencyConditions *dependencies,
      const TR::ARM64LinkageProperties &pp,
      uint32_t argSize);

   /**
    * @brief Builds indirect dispatch to method
    * @param[in] node : caller node
    */
   virtual TR::Register *buildIndirectDispatch(TR::Node *callNode);

   /**
    * @brief Builds virtual dispatch to method
    * @param[in] node : caller node
    * @param[in] dependencies : register dependency conditions
    * @param[in] argSize : size of arguments
    */
   virtual void buildVirtualDispatch(
      TR::Node *callNode,
      TR::RegisterDependencyConditions *dependencies,
      uint32_t argSize);
   };

class ARM64HelperLinkage : public TR::ARM64PrivateLinkage
   {
   public:

   /**
    * @brief Constructor
    * @param[in] cg : CodeGenerator
    * @param[in] helperLinkage : linkage convention
    */
   ARM64HelperLinkage(TR::CodeGenerator *cg, TR_LinkageConventions helperLinkage) : _helperLinkage(helperLinkage), TR::ARM64PrivateLinkage(cg)
      {
      TR_ASSERT(helperLinkage == TR_Helper || helperLinkage == TR_CHelper, "Unexpected helper linkage convention");
      }

   /**
    * @brief Builds method arguments for helper call
    * @param[in] node : caller node
    * @param[in] dependencies : register dependency conditions
    * @return total size that arguments occupy on Java stack
    */
   virtual int32_t buildArgs(
      TR::Node *callNode,
      TR::RegisterDependencyConditions *dependencies);
   protected:

   TR_LinkageConventions _helperLinkage;
   };

}

#endif
