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

#if !defined(vmi_h)
#define vmi_h
#define USING_VMI

/* @ddr_namespace: default */
#if defined(__cplusplus)
extern "C"
{
#endif
#include "jni.h"
#include "hyport.h"
#include "hyvmls.h"
 
typedef enum
{
 VMI_ERROR_NONE = 0,
 VMI_ERROR_UNKNOWN = 1,
 VMI_ERROR_UNIMPLEMENTED = 2,
 VMI_ERROR_UNSUPPORTED_VERSION = 3,
 VMI_ERROR_OUT_OF_MEMORY = 4,
 VMI_ERROR_ILLEGAL_ARG = 5, 
 VMI_ERROR_READ_ONLY = 6,
 VMI_ERROR_INITIALIZATION_FAILED = 7,
 vmiErrorEnsureWideEnum = 0x1000000
} vmiError;

typedef enum
{
 VMI_VERSION_UNKNOWN = 0x00000000,
 VMI_VERSION_1_0 = 0x00010000,
 VMI_VERSION_2_0 = 0x00020000,
 vmiVersionEnsureWideEnum = 0x1000000
} vmiVersion;

/**
 * Constant used in conjunction with GetEnv() to retrieve
 * a Harmony VM Interface table from a JNIEnv / JavaVM.
 */
#define HARMONY_VMI_VERSION_2_0 0xC01D0020

typedef void (JNICALL * vmiSystemPropertyIterator) (char *key, char *value, void *userData);

struct VMIZipFunctionTable;
struct VMInterface_;
struct VMInterfaceFunctions_;

typedef const struct VMInterfaceFunctions_ *VMInterface;

struct VMInterfaceFunctions_
{
 vmiError (JNICALL * CheckVersion) (VMInterface * vmi, vmiVersion * version);
 JavaVM *(JNICALL * GetJavaVM) (VMInterface * vmi);
 HyPortLibrary *(JNICALL * GetPortLibrary) (VMInterface * vmi);
 HyVMLSFunctionTable *(JNICALL * GetVMLSFunctions) (VMInterface * vmi);
 struct VMIZipFunctionTable *(JNICALL * GetZipFunctions) (VMInterface * vmi);
 JavaVMInitArgs *(JNICALL * GetInitArgs) (VMInterface * vmi);
 vmiError (JNICALL * GetSystemProperty) (VMInterface * vmi, char *key, char **valuePtr);
 vmiError (JNICALL * SetSystemProperty) (VMInterface * vmi, char *key, char *value);
 vmiError (JNICALL * CountSystemProperties) (VMInterface * vmi, int *countPtr);
 vmiError (JNICALL * IterateSystemProperties) (VMInterface * vmi, vmiSystemPropertyIterator iterator, void *userData);
};

vmiError JNICALL CheckVersion (VMInterface * vmi, vmiVersion * version);

JavaVM *JNICALL GetJavaVM (VMInterface * vmi);

HyPortLibrary *JNICALL GetPortLibrary (VMInterface * vmi);

HyVMLSFunctionTable *JNICALL GetVMLSFunctions (VMInterface * vmi);

struct VMIZipFunctionTable* JNICALL GetZipFunctions(VMInterface* vmi);

JavaVMInitArgs *JNICALL GetInitArgs (VMInterface * vmi);

vmiError JNICALL GetSystemProperty (VMInterface * vmi, char *key, char **valuePtr);

vmiError JNICALL SetSystemProperty (VMInterface * vmi, char *key, char *value);

vmiError JNICALL CountSystemProperties (VMInterface * vmi, int *countPtr);

vmiError JNICALL IterateSystemProperties (VMInterface * vmi, vmiSystemPropertyIterator iterator, void *userData);

#if defined(__cplusplus)
}
#endif

#endif
