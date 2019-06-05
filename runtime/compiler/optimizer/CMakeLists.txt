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
	optimizer/AllocationSinking.cpp
	optimizer/BoolArrayStoreTransformer.cpp
	optimizer/DataAccessAccelerator.cpp
	optimizer/DynamicLiteralPool.cpp
	optimizer/EscapeAnalysis.cpp
	optimizer/EstimateCodeSize.cpp
	optimizer/FearPointAnalysis.cpp
	optimizer/HCRGuardAnalysis.cpp
	optimizer/IdiomRecognition.cpp
	optimizer/IdiomRecognitionUtils.cpp
	optimizer/IdiomTransformations.cpp
	optimizer/InlinerTempForJ9.cpp
	optimizer/InterProceduralAnalyzer.cpp
	optimizer/J9EstimateCodeSize.cpp
	optimizer/J9Inliner.cpp
	optimizer/J9LocalCSE.cpp
	optimizer/J9OptimizationManager.cpp
	optimizer/J9Optimizer.cpp
	optimizer/J9RecognizedCallTransformer.cpp
	optimizer/J9Simplifier.cpp
	optimizer/J9SimplifierHandlers.cpp
	optimizer/J9SimplifierHelpers.cpp
	optimizer/J9TransformUtil.cpp
	optimizer/J9ValuePropagation.cpp
	optimizer/JProfilingBlock.cpp
	optimizer/JProfilingValue.cpp
	optimizer/JProfilingRecompLoopTest.cpp
	optimizer/JitProfiler.cpp
	optimizer/LiveVariablesForGC.cpp
	optimizer/LoopAliasRefiner.cpp
	optimizer/MonitorElimination.cpp
	optimizer/NewInitialization.cpp
	optimizer/OSRGuardAnalysis.cpp
	optimizer/OSRGuardInsertion.cpp
	optimizer/OSRGuardRemoval.cpp
	optimizer/ProfileGenerator.cpp
	optimizer/SequentialStoreSimplifier.cpp
	optimizer/SignExtendLoads.cpp
	optimizer/SPMDParallelizer.cpp
	optimizer/SPMDPreCheck.cpp
	optimizer/StringBuilderTransformer.cpp
	optimizer/StringPeepholes.cpp
	optimizer/UnsafeFastPath.cpp
	optimizer/VarHandleTransformer.cpp
	optimizer/VPBCDConstraint.cpp
)
