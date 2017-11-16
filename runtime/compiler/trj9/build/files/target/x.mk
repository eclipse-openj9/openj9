# Copyright (c) 2000, 2017 IBM Corp. and others
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0

JIT_PRODUCT_BACKEND_SOURCES+=\
    omr/compiler/x/codegen/BinaryCommutativeAnalyser.cpp \
    omr/compiler/x/codegen/BinaryEvaluator.cpp \
    omr/compiler/x/codegen/CompareAnalyser.cpp \
    omr/compiler/x/codegen/ConstantDataSnippet.cpp \
    omr/compiler/x/codegen/ControlFlowEvaluator.cpp \
    omr/compiler/x/codegen/DataSnippet.cpp \
    omr/compiler/x/codegen/DivideCheckSnippet.cpp \
    omr/compiler/x/codegen/FPBinaryArithmeticAnalyser.cpp \
    omr/compiler/x/codegen/FPCompareAnalyser.cpp \
    omr/compiler/x/codegen/FPTreeEvaluator.cpp \
    omr/compiler/x/codegen/SIMDTreeEvaluator.cpp \
    omr/compiler/x/codegen/HelperCallSnippet.cpp \
    omr/compiler/x/codegen/IA32LinkageUtils.cpp \
    omr/compiler/x/codegen/IntegerMultiplyDecomposer.cpp \
    omr/compiler/x/codegen/OMRMemoryReference.cpp \
    omr/compiler/x/codegen/OpBinary.cpp \
    omr/compiler/x/codegen/OpNames.cpp \
    omr/compiler/x/codegen/OutlinedInstructions.cpp \
    omr/compiler/x/codegen/RegisterRematerialization.cpp \
    omr/compiler/x/codegen/SubtractAnalyser.cpp \
    omr/compiler/x/codegen/Trampoline.cpp \
    omr/compiler/x/codegen/OMRTreeEvaluator.cpp \
    omr/compiler/x/codegen/UnaryEvaluator.cpp \
    omr/compiler/x/codegen/X86BinaryEncoding.cpp \
    omr/compiler/x/codegen/X86Debug.cpp \
    omr/compiler/x/codegen/X86FPConversionSnippet.cpp \
    omr/compiler/x/codegen/OMRInstruction.cpp \
    omr/compiler/x/codegen/OMRX86Instruction.cpp \
    omr/compiler/x/codegen/OMRMachine.cpp \
    omr/compiler/x/codegen/OMRLinkage.cpp \
    omr/compiler/x/codegen/OMRRegister.cpp \
    omr/compiler/x/codegen/OMRRealRegister.cpp \
    omr/compiler/x/codegen/OMRRegisterDependency.cpp \
    omr/compiler/x/codegen/OMRSnippet.cpp \
    omr/compiler/x/codegen/X86SystemLinkage.cpp \
    omr/compiler/x/codegen/XMMBinaryArithmeticAnalyser.cpp \
    omr/compiler/x/codegen/OMRCodeGenerator.cpp \
    omr/compiler/x/codegen/OMRRegisterIterator.cpp

JIT_PRODUCT_SOURCE_FILES+=\
    compiler/trj9/x/codegen/J9UnresolvedDataSnippet.cpp \
    compiler/trj9/x/codegen/J9CodeGenerator.cpp \
    compiler/trj9/x/codegen/AllocPrefetchSnippet.cpp \
    compiler/trj9/x/codegen/J9Linkage.cpp \
    compiler/trj9/x/codegen/J9Snippet.cpp \
    compiler/trj9/x/codegen/CallSnippet.cpp \
    compiler/trj9/x/codegen/CheckFailureSnippet.cpp \
    compiler/trj9/x/codegen/ForceRecompilationSnippet.cpp \
    compiler/trj9/x/codegen/GuardedDevirtualSnippet.cpp \
    compiler/trj9/x/codegen/J9TreeEvaluator.cpp \
    compiler/trj9/x/codegen/J9X86Instruction.cpp \
    compiler/trj9/x/codegen/JNIPauseSnippet.cpp \
    compiler/trj9/x/codegen/PassJNINullSnippet.cpp \
    compiler/trj9/x/codegen/RecompilationSnippet.cpp \
    compiler/trj9/x/codegen/ScratchArgHelperCallSnippet.cpp \
    compiler/trj9/x/codegen/WriteBarrierSnippet.cpp \
    compiler/trj9/x/codegen/J9AheadOfTimeCompile.cpp \
    compiler/trj9/x/codegen/X86PrivateLinkage.cpp \
    compiler/trj9/x/codegen/X86HelperLinkage.cpp \
    compiler/trj9/x/codegen/X86Recompilation.cpp \
    compiler/trj9/x/codegen/J9LinkageUtils.cpp \
    compiler/trj9/x/env/J9CPU.cpp \
    omr/compiler/x/env/OMRDebugEnv.cpp \
    omr/compiler/x/env/OMRCPU.cpp

include $(JIT_MAKE_DIR)/files/target/$(TARGET_SUBARCH).mk
