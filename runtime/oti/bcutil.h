/*******************************************************************************
 * Copyright IBM Corp. and others 2011
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#define BCT_ERR_NO_ERROR  0
#define BCT_ERR_GENERIC_ERROR  -1
#define BCT_ERR_OUT_OF_ROM  -2
#define BCT_ERR_CLASS_READ  -3
#define BCT_ERR_BYTECODE_TRANSLATION_FAILED  -4
#define BCT_ERR_STACK_MAP_FAILED  -5
#define BCT_ERR_INVALID_BYTECODE  -6
#define BCT_ERR_OUT_OF_MEMORY  -7
#define BCT_ERR_VERIFY_ERROR_INLINING  -8
#define BCT_ERR_NEED_WIDE_BRANCHES  -9
#define BCT_ERR_UNKNOWN_ANNOTATION  -10
#define BCT_ERR_CLASS_NAME_MISMATCH  -11
#define BCT_ERR_ILLEGAL_PACKAGE_NAME  -12
#define BCT_ERR_INVALID_ANNOTATION  -13
#define BCT_ERR_LINE_NUMBER_TABLE_DECOMPRESS_FAILED -14
#define BCT_ERR_INVALID_BYTECODE_SIZE -15
#define BCT_ERR_GENERIC_ERROR_CUSTOM_MSG  -16
#define BCT_ERR_INVALID_CLASS_TYPE -17
#define BCT_ERR_INVALID_ANNOTATION_BAD_CP_INDEX_OUT_OF_RANGE  -18
#define BCT_ERR_INVALID_ANNOTATION_BAD_CP_UTF8_STRING  -19
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
#define BCT_ERR_INVALID_VALUE_TYPE -20
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
#define BCT_ERR_DUPLICATE_NAME -21

