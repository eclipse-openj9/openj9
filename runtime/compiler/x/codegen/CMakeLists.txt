################################################################################
# Copyright (c) 2017, 2019 IBM Corp. and others
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
	x/codegen/AllocPrefetchSnippet.cpp
	x/codegen/CallSnippet.cpp
	x/codegen/CheckFailureSnippet.cpp
	#x/codegen/CryptoEvaluator.cpp
	x/codegen/ForceRecompilationSnippet.cpp
	x/codegen/GuardedDevirtualSnippet.cpp
	x/codegen/J9AheadOfTimeCompile.cpp
	x/codegen/J9CodeGenerator.cpp
	x/codegen/J9Linkage.cpp
	x/codegen/J9LinkageUtils.cpp
	x/codegen/J9Snippet.cpp
	x/codegen/J9TreeEvaluator.cpp
	x/codegen/J9UnresolvedDataSnippet.cpp
	x/codegen/J9X86Instruction.cpp
	x/codegen/RecompilationSnippet.cpp
	x/codegen/X86HelperLinkage.cpp
	x/codegen/X86PrivateLinkage.cpp
	x/codegen/X86Recompilation.cpp
	x/env/J9CPU.cpp
)
