################################################################################
# Copyright (c) 2017, 2018 IBM Corp. and others
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
################################################################################

j9jit_files(
	p/codegen/J9UnresolvedDataSnippet.cpp
	p/codegen/J9CodeGenerator.cpp
	p/codegen/J9TreeEvaluator.cpp
	p/codegen/GenerateInstructions.cpp
	p/codegen/J9PPCSnippet.cpp
	p/codegen/J9AheadOfTimeCompile.cpp
	p/codegen/PPCPrivateLinkage.cpp
	p/codegen/PPCJNILinkage.cpp
	p/codegen/InterfaceCastSnippet.cpp
	p/codegen/CallSnippet.cpp
	p/codegen/StackCheckFailureSnippet.cpp
	p/codegen/DFPTreeEvaluator.cpp
	p/codegen/J9PPCInstruction.cpp
	p/codegen/ForceRecompilationSnippet.cpp
	p/codegen/PPCRecompilation.cpp
	p/codegen/PPCRecompilationSnippet.cpp
	p/codegen/Trampoline.cpp
	${omr_SOURCE_DIR}/compiler/p/env/OMRDebugEnv.cpp
)
