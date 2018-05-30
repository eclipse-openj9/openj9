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
	env/annotations/AnnotationBase.cpp
	env/annotations/Annotations.cpp
	env/annotations/GPUAnnotation.cpp
	env/annotations/TestAnnotation.cpp
	env/annotations/TRNoSideFXAnnotation.cpp
	env/annotations/TROptAnnotation.cpp
	env/annotations/VMJ9Annotations.cpp
	env/CHTable.cpp
	env/ClassLoaderTable.cpp
	env/CpuUtilization.cpp
	env/FilePointer.cpp
	env/J2IThunk.cpp
	env/J9ArithEnv.cpp
	env/J9ClassEnv.cpp
	env/J9CompilerEnv.cpp
	env/J9CPU.cpp
	env/J9DebugEnv.cpp
	env/j9fieldsInfo.cpp
	env/J9IO.cpp
	env/J9JitMemory.cpp
	env/J9KnownObjectTable.cpp
	env/j9method.cpp
	env/J9ObjectModel.cpp
	env/J9SegmentAllocator.cpp
	env/J9SegmentCache.cpp
	env/J9SegmentProvider.cpp
	env/J9SharedCache.cpp
	env/J9VMEnv.cpp
	env/J9VMMethodEnv.cpp
	env/jitsupport.cpp
	env/PersistentAllocator.cpp
	env/PersistentCHTable.cpp
	env/ProcessorDetection.cpp
	env/Startup.cpp
	env/SystemSegmentProvider.cpp
	env/VMJ9.cpp
)
