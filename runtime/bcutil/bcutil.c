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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "j9.h"
#include "j2sever.h"
#include "jimagereader.h"
#include "jvminit.h"
#include "bcutil_internal.h"
#include "SCAbstractAPI.h"
#include "j9protos.h"
#ifdef J9VM_OPT_ZIP_SUPPORT
#include "vmi.h"
#endif

#define _UTE_STATIC_

#include "ut_j9bcu.h"

#if defined(WIN32) || defined(WIN64)
#define strdup _strdup
#endif

#define THIS_DLL_NAME J9_DYNLOAD_DLL_NAME

static IDATA initializeTranslationBuffers (J9PortLibrary * portLib, J9TranslationBufferSet * translationBuffers);

/*
	Free memory allocated to dynamic loader buffers.
	Answer zero on success */

IDATA
j9bcutil_freeTranslationBuffers(J9PortLibrary * portLib, J9TranslationBufferSet * translationBuffers)
{
	PORT_ACCESS_FROM_PORT(portLib);
	Trc_BCU_freeTranslationBuffers_Entry(translationBuffers);

	j9mem_free_memory(translationBuffers->classFileError);
	translationBuffers->classFileError = NULL;
	j9mem_free_memory(translationBuffers->sunClassFileBuffer);
	translationBuffers->sunClassFileBuffer = NULL;
	j9mem_free_memory(translationBuffers->searchFilenameBuffer);
	translationBuffers->searchFilenameBuffer = NULL;
	if (NULL != translationBuffers->dynamicLoadStats) {
		j9mem_free_memory(translationBuffers->dynamicLoadStats->name);
		translationBuffers->dynamicLoadStats->name = NULL;
		j9mem_free_memory(translationBuffers->dynamicLoadStats);
		translationBuffers->dynamicLoadStats = NULL;
	}

	Trc_BCU_freeTranslationBuffers_Exit();
	return BCT_ERR_NO_ERROR;
}


/*
	Allocate memory for dynamic loader buffers.
	Answer zero on success */

J9TranslationBufferSet *
j9bcutil_allocTranslationBuffers(J9PortLibrary *portLib)
{
	J9TranslationBufferSet *translationBuffers;
	PORT_ACCESS_FROM_PORT( portLib );

	Trc_BCU_allocTranslationBuffers_Entry(0);

	translationBuffers = (J9TranslationBufferSet*)j9mem_allocate_memory((UDATA) sizeof(J9TranslationBufferSet), J9MEM_CATEGORY_CLASSES);
	if (NULL == translationBuffers) {
		Trc_BCU_allocTranslationBuffers_Exit(NULL);
		return NULL;
	}

	if (BCT_ERR_NO_ERROR != initializeTranslationBuffers( portLib, translationBuffers)) {
		j9bcutil_freeAllTranslationBuffers(portLib, translationBuffers);
		translationBuffers = NULL;
	}

	Trc_BCU_allocTranslationBuffers_Exit(translationBuffers);

	return translationBuffers;
}

/*
	Free memory allocated to dynamic loader buffers.
	Answer zero on success */

IDATA
j9bcutil_freeAllTranslationBuffers( J9PortLibrary *portLib, J9TranslationBufferSet *translationBuffers )
{
	PORT_ACCESS_FROM_PORT( portLib );
	j9bcutil_freeTranslationBuffers( portLib, translationBuffers );
	j9mem_free_memory( translationBuffers );
	return BCT_ERR_NO_ERROR;
}

IDATA
bcutil_J9VMDllMain(J9JavaVM* vm, IDATA stage, void* reserved)
{
	IDATA returnVal = J9VMDLLMAIN_OK;
	I_32 rc = J9JIMAGE_NO_ERROR;
	J9VMDllLoadInfo* loadInfo;
	J9JImageIntf *jimageIntf = NULL;
	J9TranslationBufferSet* translationBuffers;

	PORT_ACCESS_FROM_JAVAVM(vm);
	VMI_ACCESS_FROM_JAVAVM((JavaVM*)vm);

#define BUFFERS_ALLOC_STAGE BYTECODE_TABLE_SET			/* defined separately to ensure dependencies below */

	switch(stage) {

		/* Note that verbose support should have already been initialized in an earlier stage */
		case BUFFERS_ALLOC_STAGE :
			loadInfo = FIND_DLL_TABLE_ENTRY( THIS_DLL_NAME );
			if (J2SE_VERSION(vm) >= J2SE_V11) {
				rc = initJImageIntf(&jimageIntf, vm, PORTLIB);
				if (J9JIMAGE_NO_ERROR != rc) {
					loadInfo->fatalErrorStr = "failed to initialize JImage interface";
					returnVal = J9VMDLLMAIN_FAILED;
					break;
				}
			}
			translationBuffers = j9bcutil_allocTranslationBuffers(vm->portLibrary);
			if (translationBuffers == NULL) {
				loadInfo->fatalErrorStr = "j9bcutil_allocTranslationBuffers failed";
				returnVal = J9VMDLLMAIN_FAILED;
				break;
			}
#ifdef J9VM_OPT_ZIP_SUPPORT
			translationBuffers->closeZipFileFunction = (I_32 (*)(J9VMInterface* vmi, struct VMIZipFile* zipFile)) ((*VMI)->GetZipFunctions(VMI)->zip_closeZipFile);
#endif

#ifdef J9VM_INTERP_VERBOSE
			if(vm->verboseLevel & VERBOSE_DYNLOAD) {
				vm->verboseStruct->hookDynamicLoadReporting(translationBuffers);
			}
#endif
			vm->jimageIntf = jimageIntf;
			vm->dynamicLoadBuffers = translationBuffers;
			vm->mapMemoryBufferSize = MAP_MEMORY_DEFAULT + MAP_MEMORY_RESULTS_BUFFER_SIZE;
			vm->mapMemoryResultsBuffer = j9mem_allocate_memory(vm->mapMemoryBufferSize, J9MEM_CATEGORY_CLASSES);

			if (omrthread_monitor_init_with_name(&vm->mapMemoryBufferMutex, 0, "global mapMemoryBuffer mutex")
			|| (vm->mapMemoryResultsBuffer == NULL)
			) {
				loadInfo->fatalErrorStr = "initial global mapMemoryBuffer or mapMemoryBufferMutex allocation failed";
				returnVal = J9VMDLLMAIN_FAILED;
			}
			vm->mapMemoryBuffer = vm->mapMemoryResultsBuffer + MAP_MEMORY_RESULTS_BUFFER_SIZE;
			break;

		case AGENTS_STARTED :
			break;
			
		case JCL_INITIALIZED :
			break;

		case LIBRARIES_ONUNLOAD :
			loadInfo = FIND_DLL_TABLE_ENTRY( THIS_DLL_NAME );
			if (IS_STAGE_COMPLETED(loadInfo->completedBits, BUFFERS_ALLOC_STAGE) && vm->dynamicLoadBuffers) {
				shutdownROMClassBuilder(vm);
				j9bcutil_freeAllTranslationBuffers(vm->portLibrary, vm->dynamicLoadBuffers);
				vm->dynamicLoadBuffers = 0;
			}

			j9mem_free_memory(vm->mapMemoryResultsBuffer);
			if (vm->mapMemoryBufferMutex) {
				omrthread_monitor_destroy(vm->mapMemoryBufferMutex);
			}

			if (vm->jimageIntf) {
				closeJImageIntf(vm->jimageIntf);
				vm->jimageIntf = NULL;
			}

			break;
	}
	return returnVal;
}

/*
	Formerly j9bcutil_copyAndCompressUTF8 in jbmapsub

	Walks the UTF-8 encoded buffer specified by source and length (in bytes) and
	checks for well formed UTF-8 bytes well making the output canonical.
	(Canonical means a Unicode character is encoded in the fewest UTF-8 bytes possible)
	The bytes are written as read and if the read data is not canonical, back up and fix.

	As converted from builder.

	UTF-8 encodings:
	1-7F      -> 0xxxxxxx
	0, 80-7FF -> 110xxxxx 10xxxxxx - note a zero byte is illegal in Java UTF-8's
	800-FFFF  -> 1110xxxx 10xxxxxx 10xxxxxx

	Surrogate pairs
	U+D800..U+DBFF followed by one of U+DC00..U+DFFF
			  -> 11101101 1010xxxx 10xxxxxx
                 11101101 1011xxxx 10xxxxxx
    This function ignores surrogates and treats them as two separate three byte
    encodings.  This is valid.

	Returns compressed length or -1.
*/

I_32
j9bcutil_verifyCanonisizeAndCopyUTF8 (U_8 *dest, U_8 *source, U_32 length)
{
	U_8 *originalDest = dest;
	U_8 *originalSource = source;
	U_8 *sourceEnd = source + length;
	UDATA firstByte, nextByte, outWord, charLength, compression = 0;
	I_32 result = -1;

	Trc_BCU_verifyCanonisizeAndCopyUTF8_Entry(dest, source, length);

	/* Assumes success */
	memcpy (dest, source, length);

	while (source != sourceEnd) {

		/* Handle multibyte */
		if (((UDATA) ((*source++) - 1)) < 0x7F) {
			continue;
		}

		firstByte = *(source - 1);
		if (firstByte == 0) {
			Trc_BCU_verifyCanonisizeAndCopyUTF8_FailedFirstByteZero();
			goto fail;
		}

		/* if multi-byte then first byte must be 11xxxxxx and more to come */
		if (((firstByte & 0x40) == 0) || (source == sourceEnd)) {
			Trc_BCU_verifyCanonisizeAndCopyUTF8_FailedFirstByteFormat(firstByte);
			goto fail;
		}

		charLength = 2;

		nextByte = *source++;
		if ((nextByte & 0xC0) != 0x80) {
			Trc_BCU_verifyCanonisizeAndCopyUTF8_FailedSecondByteFormat(nextByte);
			goto fail;
		}

		/* Assemble first two bytes */
		/* 0x1F mask is okay for 3-byte codes as legal ones have a 0 bit at 0x10 */
		outWord = ((firstByte & 0x1F) << 6) | (nextByte & 0x3F);

		if ((firstByte & 0x20) != 0) {
			/* three bytes */
			if (((firstByte & 0x10) != 0) || (source == sourceEnd)) {
				Trc_BCU_verifyCanonisizeAndCopyUTF8_FailedFirstOfThreeByteFormat(firstByte);
				goto fail;
			}

			charLength = 3;

			nextByte = *source++;
			if ((nextByte & 0xC0) != 0x80) {
				Trc_BCU_verifyCanonisizeAndCopyUTF8_FailedThirdByteFormat(nextByte);
				goto fail;
			}

			/* Add the third byte */
			outWord = (outWord << 6) | (nextByte & 0x3F);
		}

		/* Overwrite the multibyte UTF8 character only if shorter */
		if ((outWord != 0) && (outWord < 0x80)) {
			/* One byte must be shorter in here */
			dest = originalDest + (source - originalSource - charLength - compression);
			*dest++ = (U_8) outWord;
			compression += charLength - 1;
			memcpy (dest, source, (UDATA) (sourceEnd - source));

		} else if ((outWord < 0x800) && (charLength == 3)) {
			dest = originalDest + (source - originalSource - charLength - compression);
			*dest++ = (U_8) ((outWord >> 6) | 0xC0);
			*dest++ = (U_8) ((outWord & 0x3F) | 0x80);
			compression++;
			memcpy (dest, source, (UDATA) (sourceEnd - source));
		}
	}

	result = (I_32) (length - compression);

fail:
	Trc_BCU_verifyCanonisizeAndCopyUTF8_Exit(result);

	return result;
}

/*
	Allocate memory for dynamic loader buffers.
	Answer zero on success */

static IDATA
initializeTranslationBuffers(J9PortLibrary * portLib, J9TranslationBufferSet * translationBuffers)
{
	PORT_ACCESS_FROM_PORT(portLib);

	memset(translationBuffers, 0, sizeof(J9TranslationBufferSet));


	translationBuffers->findLocallyDefinedClassFunction = findLocallyDefinedClass;
	translationBuffers->internalDefineClassFunction = internalDefineClass;

	translationBuffers->dynamicLoadStats =
		(J9DynamicLoadStats *) j9mem_allocate_memory((UDATA) sizeof(struct J9DynamicLoadStats), J9MEM_CATEGORY_CLASSES);
	if (translationBuffers->dynamicLoadStats == NULL) {
		return BCT_ERR_OUT_OF_MEMORY;
	}
	memset(translationBuffers->dynamicLoadStats, 0, sizeof(struct J9DynamicLoadStats));
	translationBuffers->dynamicLoadStats->nameBufferLength = 1024;
	translationBuffers->dynamicLoadStats->name =
	j9mem_allocate_memory(translationBuffers->dynamicLoadStats->nameBufferLength, J9MEM_CATEGORY_CLASSES);
	if (translationBuffers->dynamicLoadStats->name == NULL) {
		return BCT_ERR_OUT_OF_MEMORY;
	}

	translationBuffers->relocatorDLLHandle = 0;

	translationBuffers->internalLoadROMClassFunction = internalLoadROMClass;
	translationBuffers->transformROMClassFunction = j9bcutil_transformROMClass;

#if defined(J9VM_OPT_INVARIANT_INTERNING)
	/* invariant interning support */
	translationBuffers->flags |= BCU_ENABLE_INVARIANT_INTERNING;
#endif

	return BCT_ERR_NO_ERROR;
}
