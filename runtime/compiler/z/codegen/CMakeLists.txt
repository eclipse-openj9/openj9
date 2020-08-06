################################################################################
# Copyright (c) 2019, 2020 IBM Corp. and others
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
	z/codegen/DFPTreeEvaluator.cpp
	z/codegen/ForceRecompilationSnippet.cpp
	z/codegen/InMemoryLoadStoreMarking.cpp
	z/codegen/J9AheadOfTimeCompile.cpp
	z/codegen/J9CodeGenPhase.cpp
	z/codegen/J9CodeGenerator.cpp
	z/codegen/J9Instruction.cpp
	z/codegen/J9Linkage.cpp
	z/codegen/J9MemoryReference.cpp
	z/codegen/J9Peephole.cpp
	z/codegen/J9S390Snippet.cpp
	z/codegen/J9SystemLinkageLinux.cpp
	z/codegen/J9SystemLinkagezOS.cpp
	z/codegen/J9TreeEvaluator.cpp
	z/codegen/J9BCDTreeEvaluator.cpp
	z/codegen/J9UnresolvedDataSnippet.cpp
	z/codegen/J9ZSnippet.cpp
	z/codegen/ReduceSynchronizedFieldLoad.cpp
	z/codegen/S390AOTRelocation.cpp
	z/codegen/S390CHelperLinkage.cpp
	z/codegen/S390J9CallSnippet.cpp
	z/codegen/S390PrivateLinkage.cpp
	z/codegen/S390Recompilation.cpp
	z/codegen/S390Register.cpp
	z/codegen/S390StackCheckFailureSnippet.cpp
	z/codegen/UncommonBCDCHKAddressNode.cpp
)
