/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#include <limits.h>
#include "j9ddr.h"
#include "ddrtable.h"
#include "env/defines.h" /* for TR_HOST_XXX */

/* @ddr_namespace: map_to_type=TRBuildFlags */

/* Pseudo-structure to hold build constants */
J9DDRConstantTableBegin(TRBuildFlags)
#if defined(TR_HOST_ARM)
	#define host_ARM 1
#else
	#define host_ARM 0
#endif
J9DDRConstantTableEntryWithValue("host_ARM", host_ARM)

#if defined(TR_HOST_AARCH64)
	#define host_AARCH64 1
#else
	#define host_AARCH64 0
#endif
J9DDRConstantTableEntryWithValue("host_AARCH64", host_AARCH64)

#if defined(TR_HOST_IA32)
	#define host_IA32 1
#else
	#define host_IA32 0
#endif
J9DDRConstantTableEntryWithValue("host_IA32", host_IA32)

#if defined(TR_HOST_POWER)
	#define host_POWER 1
#else
	#define host_POWER 0
#endif
J9DDRConstantTableEntryWithValue("host_POWER",host_POWER)

#if defined(TR_HOST_PPC)
	#define host_PPC 1
#else
	#define host_PPC 0
#endif
J9DDRConstantTableEntryWithValue("host_PPC", host_PPC)

#if defined(TR_HOST_S390)
	#define host_S390 1
#else
	#define host_S390 0
#endif
J9DDRConstantTableEntryWithValue("host_S390", host_S390)

#if defined(TR_HOST_X86)
	#define host_X86 1
#else
	#define host_X86 0
#endif
J9DDRConstantTableEntryWithValue("host_X86", host_X86)

#if defined(TR_HOST_32BIT)
	#define host_32BIT 1
#else
	#define host_32BIT 0
#endif
J9DDRConstantTableEntryWithValue("host_32BIT", host_32BIT)

#if defined(TR_HOST_64BIT)
	#define host_64BIT 1
#else
	#define host_64BIT 0
#endif
J9DDRConstantTableEntryWithValue("host_64BIT", host_64BIT)

J9DDRConstantTableEnd

J9DDRConstantTableBegin(CLimits)
	J9DDRConstantTableEntryWithValue("CHAR_MAX",CHAR_MAX)
	J9DDRConstantTableEntryWithValue("CHAR_MIN",CHAR_MIN)
	J9DDRConstantTableEntryWithValue("INT_MAX",INT_MAX)
	J9DDRConstantTableEntryWithValue("INT_MIN",INT_MIN)
	J9DDRConstantTableEntryWithValue("LONG_MAX",LONG_MAX)
	J9DDRConstantTableEntryWithValue("LONG_MIN",LONG_MIN)
	J9DDRConstantTableEntryWithValue("SCHAR_MAX",SCHAR_MAX)
	J9DDRConstantTableEntryWithValue("SCHAR_MIN",SCHAR_MIN)
	J9DDRConstantTableEntryWithValue("SHRT_MAX",SHRT_MAX)
	J9DDRConstantTableEntryWithValue("SHRT_MIN",SHRT_MIN)
	J9DDRConstantTableEntryWithValue("UCHAR_MAX",UCHAR_MAX)
	J9DDRConstantTableEntryWithValue("UINT_MAX",UINT_MAX)
	J9DDRConstantTableEntryWithValue("ULONG_MAX",ULONG_MAX)
	J9DDRConstantTableEntryWithValue("USHRT_MAX",USHRT_MAX)
J9DDRConstantTableEnd

J9DDRStructTableBegin(JIT)
	J9DDREmptyStruct(TRBuildFlags, NULL)
	J9DDREmptyStruct(CLimits, NULL)
J9DDRStructTableEnd

const J9DDRStructDefinition *
getJITStructTable()
{
	return J9DDR_JIT_structs;
}
