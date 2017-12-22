/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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

#ifndef verbose_api_h
#define verbose_api_h

/**
* @file verbose_api.h
* @brief Public API for the VERBOSE module.
*
* This file contains public function prototypes and
* type definitions for the VERBOSE module.
*
*/

#include "j9.h"
#include "j9comp.h"
#include "jni.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- verbose.c ---------------- */

/**
* @brief
* @param vm
* @param stage
* @param reserved
* @return IDATA
*/
IDATA 
J9VMDllMain(J9JavaVM* vm, IDATA stage, void* reserved);


/**
* @brief
* @param jvm
* @param *commandLineOptions
* @param *reserved0
* @return jint
*/
jint JNICALL 
JVM_OnLoad(JavaVM * jvm, char *commandLineOptions, void *reserved0);


/**
* @brief
* @param jvm
* @param *reserved0
* @return jint
*/
jint JNICALL 
JVM_OnUnload(JavaVM * jvm, void *reserved0);

#define OPT_VERBOSE_COLON "-verbose:"
#define OPT_VERBOSE "-verbose"
#define OPT_XVERBOSEGCLOG "-Xverbosegclog"
#define OPT_VERBOSE_INIT "-verbose:init"


#define VERBOSE_SETTINGS_IGNORE 0
#define VERBOSE_SETTINGS_SET 1
#define VERBOSE_SETTINGS_CLEAR 2

/**
 * Indicate which verbose options are to be turned on, turned off, or left as-is
 */

typedef struct J9VerboseSettings {
	U_8 gc;
	U_8 vclass;
	U_8 jni;
	U_8 gcterse;
	U_8 dynload;
	UDATA stackWalkVerboseLevel;
	U_8 stackwalk;
	U_8 stacktrace;
	U_8 sizes;
	U_8 stack;
	U_8 debug;
	U_8 init;
	U_8 relocations;
	U_8 romclass;
	U_8 shutdown;
	U_8 verification;
	U_8 verifyErrorDetails;
} J9VerboseSettings;

/**
* @brief
* @param *vm Java VM
* @param *verboseOptions Structure specifying which options to be turned on, off, or left alone.
* @param **errorString May be null
* @return IDATA success (1), failure (0)
* This may be called multiple times. 
*/

IDATA setVerboseState ( J9JavaVM *vm, J9VerboseSettings *verboseOptions, char **errorString );

#ifdef __cplusplus
}
#endif

#endif /* verbose_api_h */
