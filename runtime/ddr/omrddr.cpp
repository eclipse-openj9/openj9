/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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

#include "../compiler/env/defines.h"
#include "../compiler/env/jittypes.h"
#include "hashtable_api.h"
#include "omr.h"
#include "omrthread_generated.h"
#include "omrtrace.h"
#include "ute_core.h"
#include "ddrhelp.h"

#include <limits.h>

#define OMR_DdrDebugLink(type) DdrDebugLink(omr, type)

OMR_DdrDebugLink(J9AbstractThread)
OMR_DdrDebugLink(J9HashTable)
OMR_DdrDebugLink(J9HashTableState)
OMR_DdrDebugLink(OMR_TraceThread)
OMR_DdrDebugLink(OMR_VMThread)
OMR_DdrDebugLink(TR_InlinedCallSite)
OMR_DdrDebugLink(UtThreadData)

/* @ddr_namespace: map_to_type=CLimits */

#define DDR_CHAR_MAX CHAR_MAX
#define DDR_CHAR_MIN CHAR_MIN
#define DDR_INT_MAX INT_MAX
#define DDR_INT_MIN INT_MIN
#define DDR_LONG_MAX LONG_MAX
#define DDR_LONG_MIN LONG_MIN
#define DDR_SCHAR_MAX SCHAR_MAX
#define DDR_SCHAR_MIN SCHAR_MIN
#define DDR_SHRT_MAX SHRT_MAX
#define DDR_SHRT_MIN SHRT_MIN
#define DDR_UCHAR_MAX UCHAR_MAX
#define DDR_UINT_MAX UINT_MAX
#define DDR_ULONG_MAX ULONG_MAX
#define DDR_USHRT_MAX 65535

/* @ddr_namespace: map_to_type=TRBuildFlags */

/* unsupported host architectures */
#if defined(TR_HOST_MIPS)
# define host_MIPS 1
#else
# define host_MIPS 0
#endif
#if defined(TR_HOST_SH4)
# define host_SH4 1
#else
# define host_SH4 0
#endif
