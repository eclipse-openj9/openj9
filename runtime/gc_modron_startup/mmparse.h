
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

/**
 * @file
 * @ingroup GC_Modron_Startup
 */

#if !defined(MMPARSE_H_)
#define MMPARSE_H_

class MM_GCExtensions;

/**
 * All GC Memory Parameters
 */
typedef enum {
	opt_Xmx = 0,
	opt_Xmca,
	opt_Xmco,
	opt_Xmcrs,
	opt_Xmn,
	opt_Xmns,
	opt_Xmnx,
	opt_Xmo,
	opt_Xmos,
	opt_Xmox,
	opt_Xms,
	opt_Xmoi,
	opt_Xmrx,
	opt_Xmr,
	opt_Xmdx,
	opt_Xsoftmx,
	opt_maxRAMPercent,
	opt_initialRAMPercent,
	opt_none
} gcMemoryParameters;

/* When displaying, maximum number of characters required, plus the "-" i.e. -Xmox */
#define MAXIMUM_GC_MEMORY_PARAMETER_LENGTH 8 /* -Xsoftmx is longer than the others (length 5) */

/**
 * Manipulate GC memory parameters.
 * 
 * GC memory parameters can either be provided by the user, or calculated based
 * on a memoryParameter value stored in GCExtensions (Xmdx/Xmx/Xms).
 * This structure contains the information required to manipulate a non user provided value.
 * @ingroup GC_Modron_Startup
 */
struct J9GcMemoryParameter {
	UDATA MM_GCExtensions::*fieldOffset; 
	gcMemoryParameters optionName;
	UDATA valueMax;
	UDATA valueMin;
	UDATA MM_GCExtensions::*valueBaseOffset;
	UDATA scaleNumerator;
	UDATA scaleDenominator;
	UDATA valueRound;
};

#ifdef __cplusplus
extern "C" {
#endif
	
jint gcParseCommandLineAndInitializeWithValues(J9JavaVM *vm, IDATA *memoryParameters);
bool gcParseTGCCommandLine(J9JavaVM *vm);

jint gcParseXgcArguments(J9JavaVM *vm, char *optArg);
jint gcParseXXgcArguments(J9JavaVM *vm, char *optArg);
bool scan_udata_helper(J9JavaVM *javaVM, char **cursor, UDATA *value, const char *argName);
bool scan_u32_helper(J9JavaVM *javaVM, char **cursor, U_32 *value, const char *argName);
bool scan_u64_helper(J9JavaVM *javaVM, char **cursor, U_64 *value, const char *argName);
bool scan_udata_memory_size_helper(J9JavaVM *javaVM, char **cursor, UDATA *value, const char *argName);
bool scan_u64_memory_size_helper(J9JavaVM *javaVM, char **cursor, U_64 *value, const char *argName);
bool scan_hex_helper(J9JavaVM *javaVM, char **cursor, UDATA *value, const char *argName);
void gcParseXgcpolicy(MM_GCExtensions *extensions);

#ifdef __cplusplus
} /* extern "C" { */
#endif
	
#endif /* MMPARSE_H_ */
