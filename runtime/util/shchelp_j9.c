/*******************************************************************************
 * Copyright (c) 2013, 2019 IBM Corp. and others
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
#include "util_api.h"
#include "j9version.h"
#include "ut_j9vmutil.h"

#define OPENJ9_SHA_MIN_BITS 28

/**
 * Populate a J9PortShcVersion struct with the version data of the running JVM
 *
 * @param [in] vm  pointer to J9JavaVM structure.*
 * @param [in] j2seVersion  The j2se version the JVM is running
 * @param [out] result  The struct to populate
 */
void
setCurrentCacheVersion(J9JavaVM *vm, UDATA j2seVersion, J9PortShcVersion* result)
{
	result->esVersionMajor = EsVersionMajor;
	result->esVersionMinor = EsVersionMinor;
	result->modlevel = getShcModlevelForJCL(j2seVersion);
	result->addrmode = J9SH_ADDRMODE;
	result->cacheType = 0;			/* initialize to 0 */
	result->feature = getJVMFeature(vm);
}

/**
 * Get the running JVM feature
 *
 * @param [in] vm  pointer to J9JavaVM structure.*
 * @return U_32	J9SH_FEATURE_COMPRESSED_POINTERS is set if compressed reference is used on 64-bit platform.
 * 				J9SH_FEATURE_NON_COMPRESSED_POINTERS is set if compressed reference is not used on 64-bit platform.
 * 				J9SH_FEATURE_DEFAULT otherwise.
 */
U_32
getJVMFeature(J9JavaVM *vm)
{
	U_32 ret = J9SH_FEATURE_DEFAULT;

#if defined(J9VM_ENV_DATA64)
	if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm)) {
		ret |= J9SH_FEATURE_COMPRESSED_POINTERS;
	} else {
		ret |= J9SH_FEATURE_NON_COMPRESSED_POINTERS;
	}
#endif /* defined(J9VM_ENV_DATA64) */
	return ret;
}

/**
 * Get the OpenJ9 SHA
 *
 * @return uint64_t The OpenJ9 SHA
 */
uint64_t
getOpenJ9Sha()
{
	uint64_t sha = 0;
	char *str = J9VM_VERSION_STRING;
	
	if (scan_hex_u64(&str, &sha) < OPENJ9_SHA_MIN_BITS) {
		Assert_VMUtil_ShouldNeverHappen();
	}
	if (0 == sha) {
		Assert_VMUtil_ShouldNeverHappen();
	}

	return sha;
}
