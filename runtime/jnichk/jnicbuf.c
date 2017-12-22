/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#include "j9.h"
#include "rommeth.h"
#include "jnichknls.h"
#include "jnichk_internal.h"

#include <string.h>

extern void* jniOptions;

U_32 
computeStringCRC(const char* buf)
{
	U_32 length;
	U_32 seed;

	if (buf == NULL) {
		return 0;
	}

	length = (U_32)strlen(buf);
	seed = j9crc32(0, NULL, 0);

	return j9crc32(seed, (U_8*)buf, length);
}

void 
checkStringCRC(JNIEnv* env, const char* function, U_32 argNum, const char* buf, U_32 oldCRC)
{
	U_32 newCRC = computeStringCRC(buf);

	if (newCRC != oldCRC) {
		jniCheckFatalErrorNLS(env, J9NLS_JNICHK_BUFFER_MODIFIED, function, argNum, function, function);
	}
}

U_32 
computeDataCRC(const void* buf, IDATA len)
{
	U_32 seed;

	if ( (buf == NULL) || (len < 0) ) {
		return 0;
	}

	seed = j9crc32(0, NULL, 0);

	return j9crc32(seed, (U_8*)buf, (U_32)len);
}

void 
checkDataCRC(JNIEnv* env, const char* function, U_32 argNum, const void* buf, IDATA len, U_32 oldCRC)
{
	U_32 newCRC = computeDataCRC(buf, len);

	if (newCRC != oldCRC) {
		jniCheckFatalErrorNLS(env, J9NLS_JNICHK_BUFFER_MODIFIED, function, argNum, function, function);
	}
}

U_32 
computeArgsCRC(const jvalue *args, jmethodID methodID)
{
	J9Method *ramMethod;
	J9ROMMethod* romMethod;
	J9UTF8* sig;
	U_8* sigArgs = NULL;
	U_32 length;
	U_32 seed;
	J9Class *methodClass;

	if ( (args == NULL) || (methodID == NULL) ) {
		return 0;
	}

	ramMethod = ((J9JNIMethodID*)methodID)->method;
	methodClass = J9_CLASS_FROM_METHOD(ramMethod);
	romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);
	sig = J9ROMMETHOD_GET_SIGNATURE(methodClass->romClass, romMethod);

	/* count the args */
	length = 0;
	sigArgs = J9UTF8_DATA(sig);
	while (*++sigArgs != ')') {
		switch (*sigArgs) {
		case '[':
			/* ignore square brackets and just count the leaf type as one argument */
			break;
		case 'L':
			while (*++sigArgs != ';') {
				/* skip up to the semi-colon */
			}
			length += 1;
			break;
		default:
			length += 1;
			break;
		}
	}

	seed = j9crc32(0, NULL, 0);

	return j9crc32(seed, (U_8*)args, length * sizeof(args[0]));
}

void 
checkArgsCRC(JNIEnv* env, const char* function, U_32 argNum, const jvalue *args, jmethodID methodID, U_32 oldCRC)
{
	U_32 newCRC = computeArgsCRC(args, methodID);

	if (newCRC != oldCRC) {
		jniCheckFatalErrorNLS(env, J9NLS_JNICHK_BUFFER_MODIFIED, function, argNum, function, function);
	}
}

