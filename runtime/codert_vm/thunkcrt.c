/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
#include "j9protos.h"
#include "jitprotos.h"
#include "ut_j9codertvm.h"
#include <string.h>

#undef DEBUG

/* Note these values must all be odd since the encoded bytes overlay a pointer field in the hash table entry where we use low-tag to indicate an inline encoding */

#define J9_THUNK_TYPE_VOID		1
#define J9_THUNK_TYPE_INT		3
#define J9_THUNK_TYPE_LONG		5
#define J9_THUNK_TYPE_FLOAT		7
#define J9_THUNK_TYPE_DOUBLE	9
#define J9_THUNK_TYPE_OBJECT	11
#define J9_THUNK_TYPE_UNUSED	13
#define J9_THUNK_TYPE_FILL		15

/* This value should be a multiple of 8 bytes (to maintain pointer alignment).
 *
 * Also note that the value must be smaller than 64 (128 types) so that argCount * 2 + 
 * may be stored in the inline argCount byte.
 */

#define J9_THUNK_INLINE_ENCODING_BYTES	8

/* 4 bits per arg + 4 bits return type, rounded up to nearest byte */

#define J9_THUNK_ENCODED_SIGNATURE_LENGTH(argCount)  ((((U_32) (argCount)) + 1 + 1) / 2)

/* Encoding format is:
 *
 * 1 byte			argCount
 * 128 bytes		encoded bytes (up to 255 args + 1 return type)
 */

#define J9_THUNK_MAX_ENCODED_BYTES	129

typedef struct J9ThunkTableEntry {
	void * thunkAddress;
	union {
		U_8 * outOfLineBytes;
		U_8 inlineBytes[J9_THUNK_INLINE_ENCODING_BYTES];
	} encodedSignature;
} J9ThunkTableEntry;

#define J9_THUNK_BYTES_ARE_INLINE(entry) (((UDATA) ((entry)->encodedSignature.outOfLineBytes)) & 1)

#if defined(J9ZOS390)
extern void icallVMprJavaSendVirtual0();
extern void icallVMprJavaSendVirtual1();
extern void icallVMprJavaSendVirtualJ();
extern void icallVMprJavaSendVirtualF();
extern void icallVMprJavaSendVirtualD();
#if defined(J9VM_ENV_DATA64)
extern void icallVMprJavaSendVirtualL();
#endif
extern void icallVMprJavaSendInvokeExact0();
extern void icallVMprJavaSendInvokeExact1();
extern void icallVMprJavaSendInvokeExactJ();
extern void icallVMprJavaSendInvokeExactF();
extern void icallVMprJavaSendInvokeExactD();
extern void icallVMprJavaSendInvokeExactL();
#else
extern void * icallVMprJavaSendVirtual0;
extern void * icallVMprJavaSendVirtual1;
extern void * icallVMprJavaSendVirtualJ;
extern void * icallVMprJavaSendVirtualF;
extern void * icallVMprJavaSendVirtualD;
#if defined(J9VM_ENV_DATA64)
extern void * icallVMprJavaSendVirtualL;
#endif
extern void * icallVMprJavaSendInvokeExact0;
extern void * icallVMprJavaSendInvokeExact1;
extern void * icallVMprJavaSendInvokeExactJ;
extern void * icallVMprJavaSendInvokeExactF;
extern void * icallVMprJavaSendInvokeExactD;
extern void * icallVMprJavaSendInvokeExactL;
#endif

static U_8 j9ThunkGetEncodedSignature(J9ThunkTableEntry * entry, U_8 ** encodedSignaturePtr);
static UDATA j9ThunkEncodeSignature(char *signatureData, U_8 * encodedSignature);
static UDATA j9ThunkTableHash(void *key, void *userData);
static UDATA j9ThunkTableEquals(void *leftKey, void *rightKey, void *userData);


void *
j9ThunkLookupNameAndSig(void * jitConfig, void *parm)
{
	void * thunkAddress = NULL;
	J9ROMNameAndSignature *nameAndSignature = (J9ROMNameAndSignature *) parm;

	Trc_Thunk_j9ThunkLookupNameAndSig_Entry();

	thunkAddress = j9ThunkLookupSignature(jitConfig, J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature)), (char *) &J9UTF8_DATA((J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature))));

	if (NULL == thunkAddress) {
		Trc_Thunk_j9ThunkLookupNameAndSig_Exit_ThunkNotFound();
	} else {
		Trc_Thunk_j9ThunkLookupNameAndSig_Exit_Success(thunkAddress);
	}

	return thunkAddress;
}

UDATA
j9ThunkTableAllocate(J9JavaVM * vm)
{
	J9JITConfig * jitConfig = vm->jitConfig;

	if (omrthread_monitor_init_with_name(&jitConfig->thunkHashTableMutex, 0, "JIT thunk table")) {
		return 1;
	}

	jitConfig->thunkHashTable = hashTableNew(
		OMRPORT_FROM_J9PORT(vm->portLibrary),				/* portLibrary */
		J9_GET_CALLSITE(),				/* tableName */
		0,								/* tableSize */
		sizeof(J9ThunkTableEntry),		/* entrySize */
		0,								/* entryAlignment */
		0,								/* flags */
		OMRMEM_CATEGORY_JIT,				/* memoryCategory */
		j9ThunkTableHash,				/* hashFn */
		j9ThunkTableEquals,				/* hashEqualFn */
		NULL,							/* printFn */
		NULL							/* functionUserData */
	);

	return jitConfig->thunkHashTable == NULL;
}

void
j9ThunkTableFree(J9JavaVM * vm)
{
	J9JITConfig * jitConfig = vm->jitConfig;

	if (jitConfig->thunkHashTable != NULL) {
		PORT_ACCESS_FROM_JAVAVM(vm);
		J9HashTableState state;
		J9ThunkTableEntry * entry;

		entry = hashTableStartDo(jitConfig->thunkHashTable, &state);
		while (entry != NULL) {
			if (!J9_THUNK_BYTES_ARE_INLINE(entry)) {
				j9mem_free_memory(entry->encodedSignature.outOfLineBytes);
			}
			entry = hashTableNextDo(&state);
		}
		hashTableFree(jitConfig->thunkHashTable);
		jitConfig->thunkHashTable = NULL;
	}

	if (jitConfig->thunkHashTableMutex != NULL) {
		omrthread_monitor_destroy(jitConfig->thunkHashTableMutex);
		jitConfig->thunkHashTableMutex = NULL;
	}
}

static UDATA
j9ThunkTableHash(void *key, void *userData) 
{
	U_8 * encodedSignature;
	U_8 argCount;

	argCount = j9ThunkGetEncodedSignature(key, &encodedSignature);
	return j9crc32(0, encodedSignature + 1, J9_THUNK_ENCODED_SIGNATURE_LENGTH(argCount));
}
static UDATA 
j9ThunkTableEquals(void *leftKey, void *rightKey, void *userData) 
{
	U_8 * leftSig;
	U_8 * rightSig;
	U_8 leftArgCount = j9ThunkGetEncodedSignature(leftKey, &leftSig);
	U_8 rightArgCount = j9ThunkGetEncodedSignature(rightKey, &rightSig);

	if (leftArgCount != rightArgCount) {
		return FALSE;
	}

	return memcmp(leftSig + 1, rightSig + 1, J9_THUNK_ENCODED_SIGNATURE_LENGTH(leftArgCount)) == 0;
}
void *
j9ThunkLookupSignature(void * jitConfig, UDATA signatureLength, char *signatureChars)
{
	J9ThunkTableEntry exemplar;
	J9ThunkTableEntry * entry;
	U_8 encodedSignatureArray[J9_THUNK_MAX_ENCODED_BYTES + 1];
	U_8* encodedSignature = encodedSignatureArray;

	if ((UDATA)encodedSignature & (UDATA)1) {
		/* J9_THUNK_BYTES_ARE_INLINE() needs encodedSignature to be even */
		encodedSignature++;
	}

	j9ThunkEncodeSignature(signatureChars, encodedSignature);
	exemplar.encodedSignature.outOfLineBytes = encodedSignature;

	omrthread_monitor_enter(((J9JITConfig *) jitConfig)->thunkHashTableMutex);
	entry = hashTableFind(((J9JITConfig *) jitConfig)->thunkHashTable, &exemplar);
	omrthread_monitor_exit(((J9JITConfig *) jitConfig)->thunkHashTableMutex);
	if (entry != NULL ) {
		return entry->thunkAddress;
	}

	return NULL;
}

IDATA
j9ThunkNewNameAndSig(void * jitConfig, void *parm, void *thunkAddress)
{
	J9ROMNameAndSignature *nameAndSignature = (J9ROMNameAndSignature *) parm;

	return j9ThunkNewSignature(jitConfig, J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature)), (char *) &J9UTF8_DATA((J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature))), thunkAddress);
}

IDATA
j9ThunkNewSignature(void * jitConfig, int signatureLength, char *signatureChars, void *thunkAddress)
{
	PORT_ACCESS_FROM_JAVAVM(((J9JITConfig *) jitConfig)->javaVM);
	J9ThunkTableEntry exemplar;
	J9ThunkTableEntry * entry;
	U_8 encodedSignatureArray[J9_THUNK_MAX_ENCODED_BYTES + 1];
	U_8* encodedSignature = encodedSignatureArray;
	UDATA length;

	if ((UDATA)encodedSignature & (UDATA)1) {
		/* J9_THUNK_BYTES_ARE_INLINE() needs encodedSignature to be even */
		encodedSignature++;
	}

	length = j9ThunkEncodeSignature(signatureChars, encodedSignature);

	/* J9_THUNK_TYPE_FILL * 0x11 puts J9_THUNK_TYPE_FILL * 0x11 in both nybbles */
	memset(exemplar.encodedSignature.inlineBytes, J9_THUNK_TYPE_FILL * 0x11, sizeof(exemplar.encodedSignature));

	if (length > J9_THUNK_INLINE_ENCODING_BYTES) {
		U_8 * allocatedSignature = j9mem_allocate_memory(length, OMRMEM_CATEGORY_JIT);

#ifdef DEBUG
		printf("allocating bytes\n");
#endif

		if (allocatedSignature == NULL) {
			return -1;
		}

		memcpy(allocatedSignature, encodedSignature, length);
		exemplar.encodedSignature.outOfLineBytes = allocatedSignature;		
	} else {
		/* Inline encoding bytes must all be odd - multiply argCount by 2 and add 1 to make it odd */
		encodedSignature[0] = encodedSignature[0] * 2 + 1;

		memcpy(exemplar.encodedSignature.inlineBytes, encodedSignature, length);
	}
	exemplar.thunkAddress = thunkAddress;

	omrthread_monitor_enter(((J9JITConfig *) jitConfig)->thunkHashTableMutex);
	entry = hashTableAdd(((J9JITConfig *) jitConfig)->thunkHashTable, &exemplar);
	omrthread_monitor_exit(((J9JITConfig *) jitConfig)->thunkHashTableMutex);
	if (entry == NULL) {
		if (!J9_THUNK_BYTES_ARE_INLINE(&exemplar)) {
			j9mem_free_memory(exemplar.encodedSignature.outOfLineBytes);
		}
		return -1;
	} else {
		if (!J9_THUNK_BYTES_ARE_INLINE(&exemplar)) {
			if (exemplar.encodedSignature.outOfLineBytes != entry->encodedSignature.outOfLineBytes) {
				/* Existing entry was found in the table */
				j9mem_free_memory(exemplar.encodedSignature.outOfLineBytes);
			}
		}
	}

	return 0;
}

/* Returns the total number of bytes used in the encodedSignature buffer */

static UDATA
j9ThunkEncodeSignature(char *signatureData, U_8 * encodedSignature)
{
	U_8 * encodedTypes = encodedSignature + 1;
	U_8 argCount = 0;
	U_8 encodedTypeByte = 0;
	UDATA encodedTypeByteStored = TRUE;
	UDATA done = FALSE;
	UDATA totalSize;
#ifdef DEBUG
	char * origSig = signatureData;
	UDATA i;
#endif

	/* Skip opening bracket */
	++signatureData;

	/* Encode the signature (including return type), considering like types to be identical */

	do {
		char c = *signatureData++;
		U_8 encodedType;

		/* Include the return type in the encoding, but do not increment the argCount for it */

		if (c == ')') {
			done = TRUE;
			c = *signatureData++;
		} else {
			++argCount;
		}

		/* Consume signature element and convert to canonical type */

		switch (c) {
			case 'V':
				encodedType = J9_THUNK_TYPE_VOID;
				break;
			case 'F':
				encodedType = J9_THUNK_TYPE_FLOAT;
				break;
			case 'D':
				encodedType = J9_THUNK_TYPE_DOUBLE;
				break;
			case 'J':
				encodedType = J9_THUNK_TYPE_LONG;
				break;
			case '[':
				while ((c = *signatureData++) == '[') ;
				/* intentional fall-through */
			case 'L':
				if (c == 'L') {
					while (*signatureData++ != ';') ;
				}
#if defined(J9VM_ENV_DATA64)
				encodedType = J9_THUNK_TYPE_OBJECT;
				break;
#endif
				/* intentional fall-through */
			default:
				encodedType = J9_THUNK_TYPE_INT;
				break;
		}

		/* Store encoded value into nybble */

		encodedTypeByte = (encodedTypeByte << 4) | encodedType;
		encodedTypeByteStored = !encodedTypeByteStored;
		if (encodedTypeByteStored) {
			*encodedTypes++ = encodedTypeByte;
		}
	} while (!done);

	/* Store the final byte if necessary */

	if (!encodedTypeByteStored) {
		*encodedTypes++ = (encodedTypeByte << 4) | J9_THUNK_TYPE_FILL;
	}

	/* Store arg count and compute total size */

	encodedSignature[0] = argCount;
	totalSize = encodedTypes - encodedSignature;

#ifdef DEBUG
	printf("encode: %.*s -> ", signatureData - origSig, origSig);
	for (i = 0; i < totalSize; ++i) {
		printf("%02X", encodedSignature[i]);
	}
	printf(" (length %d)\n", totalSize);
#endif

	return totalSize;
}

static U_8
j9ThunkGetEncodedSignature(J9ThunkTableEntry * entry, U_8 ** encodedSignaturePtr) 
{
	U_8 * encodedSignature;
	U_8 argCount;

	if (J9_THUNK_BYTES_ARE_INLINE(entry)) {
		encodedSignature = entry->encodedSignature.inlineBytes;
		/* Inline encoding bytes must all be odd - argCount is stored multiplied by 2 and 1 added to make it odd */
		argCount = encodedSignature[0] >> 1;
	} else {
		encodedSignature = entry->encodedSignature.outOfLineBytes;
		argCount = encodedSignature[0];
	}

	*encodedSignaturePtr = encodedSignature;
	return argCount;
}

void *
j9ThunkVMHelperFromSignature(void * jitConfig, UDATA signatureLength, char *signatureChars)
{
	void * helper;

	while ((*signatureChars++) != ')') ;
	switch (*signatureChars) {
		case 'V':
			helper = J9_BUILDER_SYMBOL(icallVMprJavaSendVirtual0);
			break;
		case 'F':
			helper = J9_BUILDER_SYMBOL(icallVMprJavaSendVirtualF);
			break;
		case 'D':
			helper = J9_BUILDER_SYMBOL(icallVMprJavaSendVirtualD);
			break;
		case 'J':
			helper = J9_BUILDER_SYMBOL(icallVMprJavaSendVirtualJ);
			break;
		case '[':
			/* intentional fall-through */
		case 'L':
#if defined(J9VM_ENV_DATA64)
			helper = J9_BUILDER_SYMBOL(icallVMprJavaSendVirtualL);
			break;
#endif
			/* intentional fall-through */
		default:
			helper = J9_BUILDER_SYMBOL(icallVMprJavaSendVirtual1);
			break;
	}

	return helper;
}

void *
j9ThunkInvokeExactHelperFromSignature(void * jitConfig, UDATA signatureLength, char *signatureChars)
{
	void * helper;

	while ((*signatureChars++) != ')') ;
	switch (*signatureChars) {
		case 'V':
			helper = J9_BUILDER_SYMBOL(icallVMprJavaSendInvokeExact0);
			break;
		case 'F':
			helper = J9_BUILDER_SYMBOL(icallVMprJavaSendInvokeExactF);
			break;
		case 'D':
			helper = J9_BUILDER_SYMBOL(icallVMprJavaSendInvokeExactD);
			break;
		case 'J':
			helper = J9_BUILDER_SYMBOL(icallVMprJavaSendInvokeExactJ);
			break;
		case '[':
			/* intentional fall-through */
		case 'L':
			helper = J9_BUILDER_SYMBOL(icallVMprJavaSendInvokeExactL);
			break;
		default:
			helper = J9_BUILDER_SYMBOL(icallVMprJavaSendInvokeExact1);
			break;
	}

	return helper;
}

void *
j9ThunkVMHelperFromNameAndSig(void * jitConfig, void *parm)
{
	J9ROMNameAndSignature *nameAndSignature = (J9ROMNameAndSignature *) parm;

	return j9ThunkVMHelperFromSignature(jitConfig, J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature)), (char *) &J9UTF8_DATA((J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature))));
}
