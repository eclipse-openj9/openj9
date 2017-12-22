/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

/*
 * BuildResult.hpp
 */

#ifndef BUILDRESULT_HPP_
#define BUILDRESULT_HPP_

/* @ddr_namespace: default */
#include "j9.h"

enum BuildResult {
	OK = BCT_ERR_NO_ERROR,
	GenericError = BCT_ERR_GENERIC_ERROR,
	OutOfROM = BCT_ERR_OUT_OF_ROM,
	ClassRead = BCT_ERR_CLASS_READ,
	BytecodeTranslationFailed = BCT_ERR_BYTECODE_TRANSLATION_FAILED,
	StackMapFailed = BCT_ERR_STACK_MAP_FAILED,
	InvalidBytecode = BCT_ERR_INVALID_BYTECODE,
	OutOfMemory = BCT_ERR_OUT_OF_MEMORY,
	VerifyErrorInlining = BCT_ERR_VERIFY_ERROR_INLINING,
	NeedWideBranches = BCT_ERR_NEED_WIDE_BRANCHES,
	UnknownAnnotation = BCT_ERR_UNKNOWN_ANNOTATION,
	ClassNameMismatch = BCT_ERR_CLASS_NAME_MISMATCH,
	IllegalPackageName = BCT_ERR_ILLEGAL_PACKAGE_NAME,
	InvalidAnnotation = BCT_ERR_INVALID_ANNOTATION,
	LineNumberTableDecompressFailed = BCT_ERR_LINE_NUMBER_TABLE_DECOMPRESS_FAILED,
	InvalidBytecodeSize = BCT_ERR_INVALID_BYTECODE_SIZE,
};

#endif /* BUILDRESULT_HPP_ */
