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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef TR_J9_X86_CODEGENERATORBASE_INCL
#define TR_J9_X86_CODEGENERATORBASE_INCL

#include "trj9/codegen/J9CodeGenerator.hpp"

namespace TR { class Recompilation; }

namespace J9
{

namespace X86
{

class OMR_EXTENSIBLE CodeGenerator : public J9::CodeGenerator
   {
   public:

   CodeGenerator();

   TR::Recompilation *allocateRecompilationInfo();

   void beginInstructionSelection();

   void endInstructionSelection();

   TR::Instruction *generateSwitchToInterpreterPrePrologue(
         TR::Instruction *prev,
         uint8_t alignment,
         uint8_t alignmentMargin);

   // Stack frame padding
   int32_t getStackFramePaddingSizeInBytes() {return _stackFramePaddingSizeInBytes;}
   int32_t setStackFramePaddingSizeInBytes(int32_t s) {return (_stackFramePaddingSizeInBytes = s);}
   int32_t _stackFramePaddingSizeInBytes;

   bool allowGuardMerging();

   bool nopsAlsoProcessedByRelocations();

#ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION
   bool inlineCryptoMethod(TR::Node *node, TR::Register *&resultReg);
#endif

   bool enableAESInHardwareTransformations();

   bool suppressInliningOfRecognizedMethod(TR::RecognizedMethod method);

   };

}

}

#endif
