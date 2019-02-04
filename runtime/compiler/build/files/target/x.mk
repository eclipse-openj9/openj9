# Copyright (c) 2000, 2019 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception

JIT_PRODUCT_BACKEND_SOURCES+=\
    omr/compiler/x/codegen/BinaryCommutativeAnalyser.cpp \
    omr/compiler/x/codegen/BinaryEvaluator.cpp \
    omr/compiler/x/codegen/CompareAnalyser.cpp \
    omr/compiler/x/codegen/ControlFlowEvaluator.cpp \
    omr/compiler/x/codegen/DataSnippet.cpp \
    omr/compiler/x/codegen/DivideCheckSnippet.cpp \
    omr/compiler/x/codegen/FPBinaryArithmeticAnalyser.cpp \
    omr/compiler/x/codegen/FPCompareAnalyser.cpp \
    omr/compiler/x/codegen/FPTreeEvaluator.cpp \
    omr/compiler/x/codegen/HelperCallSnippet.cpp \
    omr/compiler/x/codegen/IA32LinkageUtils.cpp \
    omr/compiler/x/codegen/IntegerMultiplyDecomposer.cpp \
    omr/compiler/x/codegen/OMRCodeGenerator.cpp \
    omr/compiler/x/codegen/OMRInstruction.cpp \
    omr/compiler/x/codegen/OMRLinkage.cpp \
    omr/compiler/x/codegen/OMRMachine.cpp \
    omr/compiler/x/codegen/OMRMemoryReference.cpp \
    omr/compiler/x/codegen/OMRRealRegister.cpp \
    omr/compiler/x/codegen/OMRRegister.cpp \
    omr/compiler/x/codegen/OMRRegisterDependency.cpp \
    omr/compiler/x/codegen/OMRSnippet.cpp \
    omr/compiler/x/codegen/OMRTreeEvaluator.cpp \
    omr/compiler/x/codegen/OMRX86Instruction.cpp \
    omr/compiler/x/codegen/OpBinary.cpp \
    omr/compiler/x/codegen/OpNames.cpp \
    omr/compiler/x/codegen/OutlinedInstructions.cpp \
    omr/compiler/x/codegen/RegisterRematerialization.cpp \
    omr/compiler/x/codegen/SIMDTreeEvaluator.cpp \
    omr/compiler/x/codegen/SubtractAnalyser.cpp \
    omr/compiler/x/codegen/UnaryEvaluator.cpp \
    omr/compiler/x/codegen/X86BinaryEncoding.cpp \
    omr/compiler/x/codegen/X86Debug.cpp \
    omr/compiler/x/codegen/X86FPConversionSnippet.cpp \
    omr/compiler/x/codegen/X86SystemLinkage.cpp \
    omr/compiler/x/codegen/XMMBinaryArithmeticAnalyser.cpp

JIT_PRODUCT_SOURCE_FILES+=\
    compiler/x/codegen/AllocPrefetchSnippet.cpp \
    compiler/x/codegen/CallSnippet.cpp \
    compiler/x/codegen/CheckFailureSnippet.cpp \
    compiler/x/codegen/ForceRecompilationSnippet.cpp \
    compiler/x/codegen/GuardedDevirtualSnippet.cpp \
    compiler/x/codegen/J9AheadOfTimeCompile.cpp \
    compiler/x/codegen/J9CodeGenerator.cpp \
    compiler/x/codegen/J9Linkage.cpp \
    compiler/x/codegen/J9LinkageUtils.cpp \
    compiler/x/codegen/J9Snippet.cpp \
    compiler/x/codegen/J9TreeEvaluator.cpp \
    compiler/x/codegen/J9UnresolvedDataSnippet.cpp \
    compiler/x/codegen/J9X86Instruction.cpp \
    compiler/x/codegen/RecompilationSnippet.cpp \
    compiler/x/codegen/X86HelperLinkage.cpp \
    compiler/x/codegen/X86PrivateLinkage.cpp \
    compiler/x/codegen/X86Recompilation.cpp \
    compiler/x/env/J9CPU.cpp \
    omr/compiler/x/env/OMRCPU.cpp \
    omr/compiler/x/env/OMRDebugEnv.cpp

include $(JIT_MAKE_DIR)/files/target/$(TARGET_SUBARCH).mk
