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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception

JIT_PRODUCT_BACKEND_SOURCES+=\
    omr/compiler/z/codegen/BinaryAnalyser.cpp \
    omr/compiler/z/codegen/BinaryCommutativeAnalyser.cpp \
    omr/compiler/z/codegen/BinaryEvaluator.cpp \
    omr/compiler/z/codegen/CallSnippet.cpp \
    omr/compiler/z/codegen/CompareAnalyser.cpp \
    omr/compiler/z/codegen/ConstantDataSnippet.cpp \
    omr/compiler/z/codegen/ControlFlowEvaluator.cpp \
    omr/compiler/z/codegen/FPTreeEvaluator.cpp \
    omr/compiler/z/codegen/OMRMemoryReference.cpp \
    omr/compiler/z/codegen/OpMemToMem.cpp \
    omr/compiler/z/codegen/OMRMachine.cpp \
    omr/compiler/z/codegen/S390BranchCondNames.cpp \
    omr/compiler/z/codegen/S390Debug.cpp \
    omr/compiler/z/codegen/S390GenerateInstructions.cpp \
    omr/compiler/z/codegen/S390HelperCallSnippet.cpp \
    omr/compiler/z/codegen/S390Instruction.cpp \
    omr/compiler/z/codegen/OMRLinkage.cpp \
    omr/compiler/z/codegen/SystemLinkage.cpp \
    omr/compiler/z/codegen/S390OutOfLineCodeSection.cpp \
    omr/compiler/z/codegen/OMRRegisterDependency.cpp \
    omr/compiler/z/codegen/OMRSnippet.cpp \
    omr/compiler/z/codegen/S390Snippets.cpp \
    omr/compiler/z/codegen/TranslateEvaluator.cpp \
    omr/compiler/z/codegen/OMRTreeEvaluator.cpp \
    omr/compiler/z/codegen/UnaryEvaluator.cpp \
    omr/compiler/z/codegen/OMRInstruction.cpp \
    omr/compiler/z/codegen/InstOpCode.cpp \
    omr/compiler/z/codegen/InstOpCodeTables.cpp \
    omr/compiler/z/codegen/OMRCodeGenPhase.cpp \
    omr/compiler/z/codegen/OMRRegister.cpp \
    omr/compiler/z/codegen/OMRRealRegister.cpp \
    omr/compiler/z/codegen/OMRRegisterIterator.cpp \
    omr/compiler/z/codegen/OMRRegisterPair.cpp \
    omr/compiler/z/codegen/OMRCodeGenerator.cpp

JIT_PRODUCT_SOURCE_FILES+=\
    compiler/trj9/z/codegen/J9UnresolvedDataSnippet.cpp \
    compiler/trj9/z/codegen/S390Register.cpp \
    compiler/trj9/z/codegen/J9CodeGenerator.cpp \
    compiler/trj9/z/codegen/J9CodeGenPhase.cpp \
    compiler/trj9/z/codegen/J9Instruction.cpp \
    compiler/trj9/z/codegen/J9MemoryReference.cpp \
    compiler/trj9/z/codegen/J9S390SystemLinkage.cpp \
    compiler/trj9/z/codegen/J9S390PrivateLinkage.cpp \
    compiler/trj9/z/codegen/J9S390CHelperLinkage.cpp \
    compiler/trj9/z/codegen/J9TreeEvaluator.cpp \
    compiler/trj9/z/codegen/J9Linkage.cpp \
    compiler/trj9/z/codegen/S390StackCheckFailureSnippet.cpp \
    compiler/trj9/z/codegen/J9S390Snippet.cpp \
    compiler/trj9/z/codegen/J9ZSnippet.cpp \
    compiler/trj9/z/codegen/S390AOTRelocation.cpp \
    compiler/trj9/z/codegen/DFPTreeEvaluator.cpp \
    compiler/trj9/z/codegen/InMemoryLoadStoreMarking.cpp \
    compiler/trj9/z/codegen/ReduceSynchronizedFieldLoad.cpp \
    compiler/trj9/z/codegen/UncommonBCDCHKAddressNode.cpp \
    compiler/trj9/z/codegen/ForceRecompilationSnippet.cpp \
    compiler/trj9/z/codegen/S390Recompilation.cpp \
    compiler/trj9/z/codegen/J9AheadOfTimeCompile.cpp \
    compiler/trj9/z/codegen/S390J9CallSnippet.cpp \
    compiler/trj9/z/env/J9CPU.cpp \
    omr/compiler/z/env/OMRDebugEnv.cpp
