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
 * @ingroup VMChk
 */

#include "j9.h"
#include "j9port.h"
#include "j9modron.h"
#include "avl_api.h"
#include "util_api.h"
#include "vmcheck.h"

static BOOLEAN verifyJ9ClassLoader(J9JavaVM *vm, J9ClassLoader *classLoader);

/*
 *	J9LocalInternTableSanity sanity:
 *		if (J9JavaVM->dynamicLoadBuffers != NULL) && (J9JavaVM->dynamicLoadBuffers->romClassBuilder != NULL)
 *			invariantInternTree check:
 *				For each J9InternHashTableEntry
 *					Ensure J9InternHashTableEntry->utf8 is valid
 *					Ensure J9InternHashTableEntry->classLoader is valid
 */
void
checkLocalInternTableSanity(J9JavaVM *vm)
{
	UDATA count = 0;
	J9TranslationBufferSet *dynamicLoadBuffers = NULL;

	vmchkPrintf(vm, "  %s Checking ROM intern string nodes>\n", VMCHECK_PREFIX);

	dynamicLoadBuffers = (J9TranslationBufferSet *)DBG_ARROW(vm, dynamicLoadBuffers);
	if (NULL != dynamicLoadBuffers) {
		J9DbgROMClassBuilder *romClassBuilder = (J9DbgROMClassBuilder *)DBG_ARROW(dynamicLoadBuffers, romClassBuilder);
		if (NULL != romClassBuilder) {
			/* We don't need to use DBG_ARROW() below because we are taking its address (not dereferencing it).. */
			J9DbgStringInternTable *stringInternTable = &(romClassBuilder->stringInternTable);
			J9InternHashTableEntry *node = (J9InternHashTableEntry*)DBG_ARROW(stringInternTable, headNode);

			while (NULL != node) {
				J9ClassLoader *classLoader = (J9ClassLoader *)DBG_ARROW(node, classLoader);
				if (J9_ARE_NO_BITS_SET(DBG_ARROW(classLoader, gcFlags), J9_GC_CLASS_LOADER_DEAD)) {
					J9UTF8 *utf8 = (J9UTF8 *)DBG_ARROW(node, utf8);
					if (FALSE == verifyUTF8(utf8)) {
						vmchkPrintf(vm, " %s - Invalid utf8=0x%p for node=0x%p>\n",
								VMCHECK_FAILED, utf8, node);
					}

					if (FALSE == verifyJ9ClassLoader(vm, classLoader)) {
						vmchkPrintf(vm, " %s - Invalid classLoader=0x%p for node=0x%p>\n",
								VMCHECK_FAILED, classLoader, node);
					}
				}
				count += 1;
				node = (J9InternHashTableEntry *)DBG_ARROW(node, nextNode);
			}
		}
	}

	vmchkPrintf(vm, "  %s Checking %d ROM intern string nodes done>\n", VMCHECK_PREFIX, count);
}

BOOLEAN
verifyUTF8(J9UTF8 *utf8)
{
	U_8 *utf8Data;
	U_32 length;

	if (NULL == utf8) {
		return FALSE;
	}

	length = J9UTF8_LENGTH(utf8);
	utf8Data = J9UTF8_DATA(utf8);

	while (length > 0) {
		U_16 temp;
		U_32 lengthRead = decodeUTF8CharN(utf8Data, &temp, length);

		if (0 == lengthRead) {
			return FALSE;
		}

		length -= lengthRead;
		utf8Data += lengthRead;
	}

	return TRUE;
}

static BOOLEAN
verifyJ9ClassLoader(J9JavaVM *vm, J9ClassLoader *classLoader)
{
	BOOLEAN valid = FALSE;
	J9ClassLoader *walk;
	J9ClassLoaderWalkState walkState;

	walk = vmchkAllClassLoadersStartDo(vm, &walkState);
	while (NULL != walk) {
		if (walk == classLoader) {
			valid = TRUE;
			break;
		}
		walk = vmchkAllClassLoadersNextDo(vm, &walkState);
	}
	vmchkAllClassLoadersEndDo(vm, &walkState);

	return valid;
}
