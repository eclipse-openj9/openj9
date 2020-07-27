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
    omr/compiler/p/codegen/BinaryEvaluator.cpp \
    omr/compiler/p/codegen/ControlFlowEvaluator.cpp \
    omr/compiler/p/codegen/FPTreeEvaluator.cpp \
    omr/compiler/p/codegen/GenerateInstructions.cpp \
    omr/compiler/p/codegen/OMRCodeGenerator.cpp \
    omr/compiler/p/codegen/OMRConstantDataSnippet.cpp \
    omr/compiler/p/codegen/OMRInstOpCode.cpp \
    omr/compiler/p/codegen/OMRInstruction.cpp \
    omr/compiler/p/codegen/OMRLinkage.cpp \
    omr/compiler/p/codegen/OMRMachine.cpp \
    omr/compiler/p/codegen/OMRMemoryReference.cpp \
    omr/compiler/p/codegen/OMRRealRegister.cpp \
    omr/compiler/p/codegen/OMRRegisterDependency.cpp \
    omr/compiler/p/codegen/OMRSnippet.cpp \
    omr/compiler/p/codegen/OMRTreeEvaluator.cpp \
    omr/compiler/p/codegen/PPCAOTRelocation.cpp \
    omr/compiler/p/codegen/PPCBinaryEncoding.cpp \
    omr/compiler/p/codegen/PPCDebug.cpp \
    omr/compiler/p/codegen/PPCHelperCallSnippet.cpp \
    omr/compiler/p/codegen/PPCInstruction.cpp \
    omr/compiler/p/codegen/PPCOutOfLineCodeSection.cpp \
    omr/compiler/p/codegen/PPCSystemLinkage.cpp \
    omr/compiler/p/codegen/PPCTableOfConstants.cpp \
    omr/compiler/p/codegen/TreeEvaluatorVMX.cpp \
    omr/compiler/p/codegen/UnaryEvaluator.cpp \
    omr/compiler/p/env/OMRCPU.cpp

JIT_PRODUCT_SOURCE_FILES+=\
    compiler/p/codegen/CallSnippet.cpp \
    compiler/p/codegen/DFPTreeEvaluator.cpp \
    compiler/p/codegen/ForceRecompilationSnippet.cpp \
    compiler/p/codegen/GenerateInstructions.cpp \
    compiler/p/codegen/InterfaceCastSnippet.cpp \
    compiler/p/codegen/J9AheadOfTimeCompile.cpp \
    compiler/p/codegen/J9CodeGenerator.cpp \
    compiler/p/codegen/J9PPCInstruction.cpp \
    compiler/p/codegen/J9PPCSnippet.cpp \
    compiler/p/codegen/J9TreeEvaluator.cpp \
    compiler/p/codegen/J9UnresolvedDataSnippet.cpp \
    compiler/p/codegen/PPCJNILinkage.cpp \
    compiler/p/codegen/PPCPrivateLinkage.cpp \
    compiler/p/codegen/PPCRecompilation.cpp \
    compiler/p/codegen/PPCRecompilationSnippet.cpp \
    compiler/p/codegen/StackCheckFailureSnippet.cpp \
    compiler/p/codegen/Trampoline.cpp \
    compiler/p/env/J9CPU.cpp \
    omr/compiler/p/env/OMRDebugEnv.cpp
