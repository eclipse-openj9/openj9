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
    omr/compiler/x/amd64/codegen/OMRCodeGenerator.cpp \
    omr/compiler/x/amd64/codegen/OMRMachine.cpp \
    omr/compiler/x/amd64/codegen/OMRMemoryReference.cpp \
    omr/compiler/x/amd64/codegen/OMRRealRegister.cpp \
    omr/compiler/x/amd64/codegen/OMRTreeEvaluator.cpp \
    omr/compiler/x/amd64/codegen/AMD64SystemLinkage.cpp \
    omr/compiler/x/amd64/codegen/OMRELFRelocationResolver.cpp \


JIT_PRODUCT_SOURCE_FILES+=\
    tr.source/trj9/x/amd64/codegen/J9CodeGenerator.cpp \
    tr.source/trj9/x/amd64/codegen/AMD64GuardedDevirtualSnippet.cpp \
    tr.source/trj9/x/amd64/codegen/AMD64PrivateLinkage.cpp \
    tr.source/trj9/x/amd64/codegen/AMD64JNILinkage.cpp \
    tr.source/trj9/x/amd64/codegen/AMD64J9SystemLinkage.cpp
