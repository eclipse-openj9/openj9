/*******************************************************************************
 * Copyright IBM Corp. and others 2024
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
#if !defined(JFRWRITER_HPP_)
#define JFRWRITER_HPP_

#include <stdlib.h>

#include "j9cfg.h"
#include "j9.h"
#include "j9vmnls.h"
#include "vm_api.h"

#if defined(J9VM_OPT_JFR)

#include "JFRChunkWriter.hpp"

#undef DEBUG

extern "C" {
class VM_JFRWriter {
	/*
	 * Data members
	 */
private:

protected:

public:
	static constexpr const char* METADATA_BLOB_FILE_EMVVAR = "OPENJ9_METADATA_BLOB_FILE_PATH";

	static constexpr const char* DEFAULT_JFR_FILE_NAME = "defaultJ9recording.jfr";

	/*
	 * Function members
	 */
private:
	static void
	closeJFRFile(J9JavaVM *vm)
	{
		PORT_ACCESS_FROM_JAVAVM(vm);
		if (-1 != vm->jfrState.blobFileDescriptor) {
			j9file_close(vm->jfrState.blobFileDescriptor);
			vm->jfrState.blobFileDescriptor = -1;
		}
	}

	/**
	 * Long term plan is to package this file in the JDK at a known location.
	 * For the time being, search for the env var and try to open the file. If
	 * there is any failure, then metadata blob variables should remain NULL.
	 */
	static bool
	loadJFRMetadataBlob(J9JavaVM *vm)
	{
		PORT_ACCESS_FROM_JAVAVM(vm);
		I_64 fileLength = 0;
		IDATA fileDescriptor = 0;
		bool result = false;

		char *blobDir = getenv(METADATA_BLOB_FILE_EMVVAR);
		if (NULL == blobDir) {
			goto done;
		}

		fileLength = j9file_length(blobDir);
		/* Restrict file size to < 2G */
		if (fileLength > J9CONST64(0x7FFFFFFF)) {
			goto done;
		}

		vm->jfrState.metaDataBlobFileSize = fileLength;

		vm->jfrState.metaDataBlobFile = (U_8*) j9mem_allocate_memory(vm->jfrState.metaDataBlobFileSize, OMRMEM_CATEGORY_VM);
		if (NULL == vm->jfrState.metaDataBlobFile) {
			goto done;
		}

		if ((fileDescriptor = j9file_open(blobDir, EsOpenRead, 0)) == -1) {
			goto done;
		}

		if (vm->jfrState.metaDataBlobFileSize != (U_64)(I_64)j9file_read(fileDescriptor, vm->jfrState.metaDataBlobFile, vm->jfrState.metaDataBlobFileSize)) {
			vm->jfrState.metaDataBlobFileSize = 0;
			j9mem_free_memory(vm->jfrState.metaDataBlobFile);
			vm->jfrState.metaDataBlobFile = NULL;
		}

		j9file_close(fileDescriptor);
		result = true;

done:
		return result;
	}

	static bool
	openJFRFile(J9JavaVM *vm)
	{
		PORT_ACCESS_FROM_JAVAVM(vm);
		bool result = true;

		vm->jfrState.blobFileDescriptor = j9file_open(vm->jfrState.jfrFileName, EsOpenWrite | EsOpenCreate | EsOpenTruncate , 0666);

		if (-1 == vm->jfrState.blobFileDescriptor) {
			result = false;
		}

		return result;
	}

protected:

public:
	static bool
	initializaJFRWriter(J9JavaVM *vm)
	{
		bool result = true;

		if (NULL == vm->jfrState.jfrFileName) {
			vm->jfrState.jfrFileName = (char*)"defaultJ9recording.jfr";
		}

		if (!openJFRFile(vm)) {
			result = false;
			goto done;
		}

		if (!loadJFRMetadataBlob(vm)) {
			PORT_ACCESS_FROM_JAVAVM(vm);
			j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_VM_OPENJ9_JFR_METADATA_FILE_NOT_FOUND, METADATA_BLOB_FILE_EMVVAR);
		}
done:
		return result;
	}

	static bool
	teardownJFRWriter(J9JavaVM *vm)
	{
		closeJFRFile(vm);
		return true;
	}

	static bool
	flushJFRDataToFile(J9VMThread *currentThread, bool finalWrite)
	{
		bool result = true;
		VM_JFRChunkWriter chunkWriter(currentThread, finalWrite);

		if (!chunkWriter.isOkay()) {
			result = false;
			goto fail;
		}

		chunkWriter.loadEvents();
		if (!chunkWriter.isOkay()) {
			result = false;
			goto fail;
		}

		chunkWriter.writeJFRChunk();
		if (!chunkWriter.isOkay()) {
			result = false;
			goto fail;
		}

done:
		return result;

fail:
#if defined(DEBUG)
		j9tty_printf(PORTLIB, "Failed to write chunk to file error code=%d\n", (int) chunkWriter.buildResult());
#endif /* defined(DEBUG) */
		goto done;
	}

};
} /* extern "C" */

#endif /* defined(J9VM_OPT_JFR) */

#endif /* JFRWRITER_HPP_ */