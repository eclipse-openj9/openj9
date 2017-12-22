/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
#include "ClassFileParser.hpp"
#include "ROMClassCreationContext.hpp"
#include "ROMClassVerbosePhase.hpp"
#include "bcutil_api.h"
#include "ut_j9bcu.h"

/*
 * NOTE: Do not use BufferManager for memory allocation in ClassFileParser.
 * In the error case, the allocated memory is passed outside of the ROMClassBuilder.
 */

BuildResult
ClassFileParser::parseClassFile(ROMClassCreationContext *context, UDATA *initialBufferSize, U_8 **classFileBuffer)
{
	BuildResult buildResult = OutOfMemory;
	ROMClassVerbosePhase v(context, ParseClassFile, &buildResult);
	PORT_ACCESS_FROM_PORT(_portLibrary);
	UDATA bufferSize = *initialBufferSize;
	U_8 * buffer = *classFileBuffer;
	UDATA romMethodSortThreshold = UDATA_MAX;
	J9JavaVM *vm = context->javaVM();

	if (NULL != vm) {
		romMethodSortThreshold = vm->romMethodSortThreshold;
	}
	if ( NULL == buffer ) {
		buffer = (U_8 *)j9mem_allocate_memory(bufferSize, J9MEM_CATEGORY_CLASSES);
		*classFileBuffer = buffer;
		if ( NULL == buffer ) {
			return buildResult;
		}
	}

	I_32 result = BCT_ERR_OUT_OF_ROM;
	while( BCT_ERR_OUT_OF_ROM == result ) {
		result = j9bcutil_readClassFileBytes(
				_portLibrary,
				_verifyClassFunction,
				context->classFileBytes(), context->classFileSize(),
				buffer, bufferSize,
				context->bctFlags(),
				NULL,
				context->isVerbose() ? context : NULL, 
				context->findClassFlags(), romMethodSortThreshold);

		if (BCT_ERR_OUT_OF_ROM == result) {
			context->recordOutOfMemory(bufferSize);
			/* free old buffer, update bufferSize and alloc new buffer  */
			context->freeClassFileBuffer(buffer);

			UDATA newBufferSize = bufferSize * 2;
			/* Check for overflow. */
			if (newBufferSize <= bufferSize) {
				buffer = NULL;
			} else {
				bufferSize = newBufferSize;
				buffer = (U_8 *)j9mem_allocate_memory(bufferSize, J9MEM_CATEGORY_CLASSES);
			}

			*classFileBuffer = buffer;
			if ( NULL == buffer ) {
				return buildResult;
			}
		}
	}
	*initialBufferSize = bufferSize;

	if ( BCT_ERR_NO_ERROR == result ) {
		/* return the buffer */
		_j9CfrClassFile = (J9CfrClassFile *)buffer;
		buildResult = BuildResult(result);
	} else if ( BCT_ERR_GENERIC_ERROR == result ) {
		/* error structure filled in, located in buffer */
		context->recordCFRError(buffer);
		Trc_BCU_createRomClassEndian_Error(result, I_32(ClassRead));
		buildResult = ClassRead;
	} else {
		Trc_BCU_createRomClassEndian_Error(result, I_32(GenericError));
		buildResult = GenericError;
	}

	return buildResult;
}

void
ClassFileParser::restoreOriginalMethodBytecodes()
{
	J9CfrMethod *end = &_j9CfrClassFile->methods[_j9CfrClassFile->methodsCount];

	for (J9CfrMethod *method = _j9CfrClassFile->methods; method != end; method++) {
		J9CfrAttributeCode *codeAttribute = method->codeAttribute;
		if (NULL != codeAttribute) {
			memcpy(codeAttribute->code, codeAttribute->originalCode, codeAttribute->codeLength);
		}
	}
}
