# Copyright (c) 2019, 2019 IBM Corp. and others
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

JIT_PRODUCT_BACKEND_SOURCES+= \
    omr/compiler/aarch64/codegen/ARM64BinaryEncoding.cpp \
    omr/compiler/aarch64/codegen/ARM64Debug.cpp \
    omr/compiler/aarch64/codegen/ARM64HelperCallSnippet.cpp \
    omr/compiler/aarch64/codegen/ARM64Instruction.cpp \
    omr/compiler/aarch64/codegen/ARM64OutOfLineCodeSection.cpp \
    omr/compiler/aarch64/codegen/ARM64SystemLinkage.cpp \
    omr/compiler/aarch64/codegen/BinaryEvaluator.cpp \
    omr/compiler/aarch64/codegen/ControlFlowEvaluator.cpp \
    omr/compiler/aarch64/codegen/FPTreeEvaluator.cpp \
    omr/compiler/aarch64/codegen/GenerateInstructions.cpp \
    omr/compiler/aarch64/codegen/OMRCodeGenerator.cpp \
    omr/compiler/aarch64/codegen/OMRInstruction.cpp \
    omr/compiler/aarch64/codegen/OMRLinkage.cpp \
    omr/compiler/aarch64/codegen/OMRMachine.cpp \
    omr/compiler/aarch64/codegen/OMRMemoryReference.cpp \
    omr/compiler/aarch64/codegen/OMRRealRegister.cpp \
    omr/compiler/aarch64/codegen/OMRRegisterDependency.cpp \
    omr/compiler/aarch64/codegen/OMRSnippet.cpp \
    omr/compiler/aarch64/codegen/OMRTreeEvaluator.cpp \
    omr/compiler/aarch64/codegen/OpBinary.cpp \
    omr/compiler/aarch64/codegen/UnaryEvaluator.cpp \
    omr/compiler/aarch64/env/OMRCPU.cpp \
    omr/compiler/aarch64/env/OMRDebugEnv.cpp

JIT_PRODUCT_SOURCE_FILES+= \
    compiler/aarch64/codegen/ARM64JNILinkage.cpp \
    compiler/aarch64/codegen/ARM64PrivateLinkage.cpp \
    compiler/aarch64/codegen/ARM64Recompilation.cpp \
    compiler/aarch64/codegen/CallSnippet.cpp \
    compiler/aarch64/codegen/J9ARM64Snippet.cpp \
    compiler/aarch64/codegen/J9AheadOfTimeCompile.cpp \
    compiler/aarch64/codegen/J9CodeGenerator.cpp \
    compiler/aarch64/codegen/J9TreeEvaluator.cpp \
    compiler/aarch64/codegen/J9UnresolvedDataSnippet.cpp \
    compiler/aarch64/codegen/StackCheckFailureSnippet.cpp \
    compiler/aarch64/env/J9CPU.cpp
