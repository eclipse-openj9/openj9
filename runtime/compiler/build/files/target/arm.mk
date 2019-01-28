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
    omr/compiler/arm/codegen/ARMBinaryEncoding.cpp \
    omr/compiler/arm/codegen/ARMDebug.cpp \
    omr/compiler/arm/codegen/ARMGenerateInstructions.cpp \
    omr/compiler/arm/codegen/ARMOperand2.cpp \
    omr/compiler/arm/codegen/ARMOutOfLineCodeSection.cpp \
    omr/compiler/arm/codegen/ARMSystemLinkage.cpp \
    omr/compiler/arm/codegen/BinaryEvaluator.cpp \
    omr/compiler/arm/codegen/ConstantDataSnippet.cpp \
    omr/compiler/arm/codegen/ControlFlowEvaluator.cpp \
    omr/compiler/arm/codegen/FPTreeEvaluator.cpp \
    omr/compiler/arm/codegen/OMRCodeGenerator.cpp \
    omr/compiler/arm/codegen/OMRInstruction.cpp \
    omr/compiler/arm/codegen/OMRLinkage.cpp \
    omr/compiler/arm/codegen/OMRMachine.cpp \
    omr/compiler/arm/codegen/OMRMemoryReference.cpp \
    omr/compiler/arm/codegen/OMRRealRegister.cpp \
    omr/compiler/arm/codegen/OMRRegisterDependency.cpp \
    omr/compiler/arm/codegen/OMRSnippet.cpp \
    omr/compiler/arm/codegen/OMRTreeEvaluator.cpp \
    omr/compiler/arm/codegen/OpBinary.cpp \
    omr/compiler/arm/codegen/OpProperties.cpp \
    omr/compiler/arm/codegen/StackCheckFailureSnippet.cpp \
    omr/compiler/arm/codegen/SubtractAnalyser.cpp \
    omr/compiler/arm/codegen/UnaryEvaluator.cpp \
    omr/compiler/arm/env/OMRCompilerEnv.cpp \
    omr/compiler/arm/env/OMRCPU.cpp

JIT_PRODUCT_SOURCE_FILES+=\
    compiler/arm/codegen/ARMAOTRelocation.cpp \
    compiler/arm/codegen/ARMHelperCallSnippet.cpp \
    compiler/arm/codegen/ARMJNILinkage.cpp \
    compiler/arm/codegen/ARMPrivateLinkage.cpp \
    compiler/arm/codegen/ARMRecompilation.cpp \
    compiler/arm/codegen/ARMRecompilationSnippet.cpp \
    compiler/arm/codegen/CallSnippet.cpp \
    compiler/arm/codegen/J9AheadOfTimeCompile.cpp \
    compiler/arm/codegen/J9ARMEvaluator.cpp \
    compiler/arm/codegen/J9ARMSnippet.cpp \
    compiler/arm/codegen/J9CodeGenerator.cpp \
    compiler/arm/codegen/J9TreeEvaluator.cpp \
    compiler/arm/codegen/J9UnresolvedDataSnippet.cpp \
    compiler/arm/env/J9CPU.cpp
