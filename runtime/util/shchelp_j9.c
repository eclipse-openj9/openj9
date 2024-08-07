/*******************************************************************************
 * Copyright IBM Corp. and others 2013
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
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

char *
getLastDollarSignOfLambdaClassName(const char *className, UDATA classNameLength)
{
	char *end = NULL;

	if ((NULL == className) || (0 == classNameLength)) {
		return NULL;
	}

	/* Get the pointer to the last '$' sign */
	end = strnrchrHelper(className, '$', classNameLength);

	if ((NULL != end) && ((end - 8) - className > 0)) {
		if (0 == memcmp(end - 8, "$$Lambda", sizeof("$$Lambda") - 1)) {
			/* Check if $$Lambda exists in the class name right before the last '$' sign */
			return end;
		}
	}

	/* return NULL if it is not a lambda class */
	return NULL;
}

BOOLEAN
isLambdaClassName(const char *className, UDATA classNameLength, UDATA *deterministicPrefixLength)
{
	BOOLEAN result = FALSE;
	/*
	 * Lambda class names are in the format:
	 *	HostClassName$$Lambda$<UniqueID>/<zeroed out ROM_ADDRESS>
	 * getLastDollarSignOfLambdaClassName verifies this format and returns
	 * a non-NULL pointer if successfully verified.
	 */
	const char *dollarSign = getLastDollarSignOfLambdaClassName(className, classNameLength);
	if (NULL != dollarSign) {
		result = TRUE;
		if (NULL != deterministicPrefixLength) {
			/*
			 * To reliably identify lambda classes across JVM instances, JITServer AOT cache uses the
			 * deterministic class name prefix (up to and including the last '/') and the ROMClass hash.
			 */
			const char *slash = memchr(dollarSign, '/', classNameLength - (UDATA)(dollarSign - className));
			*deterministicPrefixLength = slash ? (slash + 1 - className) : 0;
		}
	}

	return result;
}

#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
BOOLEAN
isLambdaFormClassName(const char *className, UDATA classNameLength, UDATA *deterministicPrefixLength)
{
	BOOLEAN result = FALSE;

	/*
	 * Lambda form class names are in the format:
	 *  java/lang/invoke/LambdaForm$<method-handle-type>$<UniqueID>/<zeroed out ROM_ADDRESS>
	 * where <method-handle-type> is BMH, DMH, or MH.
	 */
#define J9_LAMBDA_FORM_CLASSNAME "java/lang/invoke/LambdaForm$"
	UDATA lambdaFormComparatorLength = LITERAL_STRLEN(J9_LAMBDA_FORM_CLASSNAME);
	if (classNameLength > lambdaFormComparatorLength) {
		if (0 == memcmp(className, J9_LAMBDA_FORM_CLASSNAME, lambdaFormComparatorLength)) {
			result = TRUE;
			if (NULL != deterministicPrefixLength) {
				/*
				 * To reliably identify lambda form classes across JVM instances, JITServer AOT cache uses the
				 * deterministic class name prefix (up to and including the last '/') and the ROMClass hash.
				 */
				const char *slash = memchr(className + lambdaFormComparatorLength, '/',
				                           classNameLength - lambdaFormComparatorLength);
				*deterministicPrefixLength = slash ? (slash + 1 - className) : 0;
			}
		}
	}
#undef J9_LAMBDA_FORM_CLASSNAME

	return result;
}
#endif /* defined(J9VM_OPT_OPENJDK_METHODHANDLE) */
