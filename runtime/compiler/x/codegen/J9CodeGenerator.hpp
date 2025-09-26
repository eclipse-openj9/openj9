/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef J9_X86_CODEGENERATOR_INCL
#define J9_X86_CODEGENERATOR_INCL

#include "compiler/codegen/J9CodeGenerator.hpp"

namespace TR { class Recompilation; }

namespace J9
{

namespace X86
{

class OMR_EXTENSIBLE CodeGenerator : public J9::CodeGenerator
   {

protected:

   CodeGenerator(TR::Compilation *comp);

public:

   void initialize();

   TR::Recompilation *allocateRecompilationInfo();

   TR::SymbolReference *getNanoTimeTemp();

   void beginInstructionSelection();

   void endInstructionSelection();

   /**
    * @brief Determines the maximum preferred vector length for compiler intrinsics based on the target microarchitecture.
    *
    * This function queries the underlying hardware capabilities and internal knowledge about
    * CPU microarchitectures to recommend the optimal vector length (e.g., 128-bit, 256-bit, 512-bit)
    * that is likely to yield the best performance without incurring significant frequency scaling
    * penalties. This function does not take into account profiling data or any other runtime
    * information in the decision making process.
    *
    * @note All new vectorized intrinsics that support 256 or 512-bit vectorization should query this
    * function instead of relying strictly on whether the CPU supports vectorization at a given vector
    * length.
    *
    * @return The maximum preferred vector length in bits (e.g., 128, 256, 512).
    * This function will return a minimum of TR::VectorLength128 on all target hardware.
    */
   TR::VectorLength getMaxPreferredVectorLength();

   TR::Instruction *generateSwitchToInterpreterPrePrologue(
         TR::Instruction *prev,
         uint8_t alignment,
         uint8_t alignmentMargin);

   // Stack frame padding
   int32_t getStackFramePaddingSizeInBytes() {return _stackFramePaddingSizeInBytes;}
   int32_t setStackFramePaddingSizeInBytes(int32_t s) {return (_stackFramePaddingSizeInBytes = s);}
   int32_t _stackFramePaddingSizeInBytes;

   bool nopsAlsoProcessedByRelocations();

#ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION
   bool inlineCryptoMethod(TR::Node *node, TR::Register *&resultReg);
#endif

   bool enableAESInHardwareTransformations();

   bool suppressInliningOfRecognizedMethod(TR::RecognizedMethod method);

   /** \brief
    *     Determines whether the code generator supports inlining of java/lang/Class.isAssignableFrom
    */
   bool supportsInliningOfIsAssignableFrom();

   /** \brief 
    *     Determines if the code generator should generate inlined tests or if not, call to a VM helper.
    */
   bool supportsNonHelperIsAssignableFrom();

   /*
    * \brief Reserve space in the code cache for a specified number of trampolines.
    *        This is useful for inline caches where the methods are not yet known at
    *        compile-time but for which trampolines may be required for compiled
    *        bodies in the future.
    *
    * \param[in] numTrampolines : number of trampolines to reserve
    *
    * \return : none
    */
   void reserveNTrampolines(int32_t numTrampolines);

   /**
    * \brief Determines whether the code generator supports stack allocations
    */
   bool supportsStackAllocations() { return true; }
   /** \brief
    *     Determines whether to insert instructions to check DF flag and break on DF set
    */
   bool canEmitBreakOnDFSet();

   // See OMR::CodeGenerator::supportsNonHelper
   bool supportsNonHelper(TR::SymbolReferenceTable::CommonNonhelperSymbol symbol);

   // See J9::CodeGenerator::guaranteesResolvedDirectDispatchForSVM
   bool guaranteesResolvedDirectDispatchForSVM() { return true; }

   // See J9::CodeGenerator::guaranteesResolvedVirtualDispatchForSVM
   bool guaranteesResolvedVirtualDispatchForSVM() { return true; }

   bool willBeEvaluatedAsCallByCodeGen(TR::Node *node, TR::Compilation *comp);

private:
   TR::SymbolReference *_nanoTimeTemp;
   };

}

}

#endif
