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

#include "j9protos.h"
#include "util_internal.h"

typedef struct KeyValuePair {
	char* key;
	char* value;
} KeyValuePair;

/**
 * Similar code in the port library:
 * Read a line of text from the file into buf.  Text is converted from the platform file encoding to UTF8.
 * This is mostly equivalent to fgets in standard C.
 *
 * @param[in] portLibrary The port library
 * @param[in] fd File descriptor.
 * @param[in,out] buf Buffer for read in text.
 * @param[in] nbytes Size of buffer.
 *
 * @return buf on success, NULL on failure.
 */
static char*
propsfile_read_text(struct J9PortLibrary *portLibrary, IDATA fd, char *buf, IDATA nbytes)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
#if defined(J9ZOS390)
	char temp[64];
	IDATA count, i, result;
	char* cursor = buf;

	if (nbytes <= 0) {
		return 0;
	}

	/* discount 1 for the trailing NUL */
	nbytes -= 1;

	while (nbytes) {
		count = sizeof(temp)-1 > nbytes ? nbytes : sizeof(temp)-1;
		count = j9file_read(fd, temp, count);

		if (count < 0) {
			if (cursor == buf) {
				return NULL;
			} else {
				break;
			}
		}


        /* nul-terminate the buffer, for the translation*/
        temp[count] = '\0';
        if(__e2a_s(temp) == -1){
                return NULL;
        }

		for (i = 0; i < count; i++) {
			char c = temp[i];
			*cursor++ = c;

			if (c == '\n') { /* EOL */
				j9file_seek(fd, i - count + 1, EsSeekCur);
				*cursor = '\0';
				return buf;
			}
		}
		nbytes -= count;
	}

	*cursor = '\0';
	return buf;
#else
	return j9file_read_text(fd, buf, nbytes);

#endif
}

static UDATA
hashFn(void *entry, void *userData) {
	KeyValuePair* pair = entry;
	UDATA hash = 0;
	char* key = pair->key;
	UDATA length = strlen(key);
	UDATA i;
	
	/* Super-lame xor hash */
	for (i=0; i < length; i++) {
		hash ^= key[i];
	}
	
	return hash;
}


static UDATA
equalsFn(void *leftEntry, void *rightEntry, void *userData) {
	KeyValuePair* lhs = leftEntry;
	KeyValuePair* rhs = rightEntry;
	
	if (strcmp(lhs->key, rhs->key) == 0) {
		return TRUE;
	}
	
	return FALSE;
}


/**
 * Trim trailing whitespace from string
 * @param input The buffer to trim.
 */
static void
trimTrailing(char* input)
{
	UDATA index = strlen(input) - 1;
	while (index != 0) {
		char c = input[index];
		switch (c) {
		case '\t':
		case '\n':
		case '\r':
		case ' ':
			input[index] = 0;
			index--;
			break;
			
		default:
			return;
		}
	}	
}


j9props_file_t 
props_file_open(J9PortLibrary* portLibrary, const char* file, char* errorBuf, UDATA errorBufLength)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	j9props_file_t result = NULL;
	IDATA fileHandle = -1;
	UDATA lineNumber = 0;
	char line[1024] = "";
	/* points to some place in line */
	char * linePtr = line;

	/* Allocate the structure */
	result = (j9props_file_t) j9mem_allocate_memory(sizeof(J9PropsFile), OMRMEM_CATEGORY_VM);
	if (NULL == result) {
		return NULL;
	}
	result->portLibrary = portLibrary;

	/* Allocate the hash table */
	result->properties = hashTableNew(OMRPORT_FROM_J9PORT(PORTLIB), J9_GET_CALLSITE(), 0, sizeof(KeyValuePair), 0, 0, OMRMEM_CATEGORY_VM, hashFn, equalsFn, NULL, NULL);
	if (NULL == result->properties) {
		goto fail;
	}

	fileHandle = j9file_open(file, EsOpenRead, 0);
	if (fileHandle == -1) {
		goto fail;
	}
	
	/* Read a line at a time */
	lineNumber = 0;
	/* previously had a j9file_read_text here, but functionality to do the charset conversion (for zos) is not there yet
	 * so we use this hack for now*/
	while (NULL != propsfile_read_text(portLibrary, fileHandle, linePtr, sizeof(line) - (line - linePtr) )) {
		char* equalsSign;
		
		if(strlen(line) > 1 && line[strlen(line)-1] == '\n' && line[strlen(line)-2] == '\\'){ /* line continuation */
			/* update linePtr to new position and overwrite the '\\' */
			linePtr = linePtr + strlen(linePtr) - 2;
			continue;
		}

		linePtr = line;
		lineNumber++;
		equalsSign = strstr(line,"=");
		if (equalsSign && line[0] != '#') { /*lines starting with # are comments*/
			UDATA lineLength;
			char* copiedLine;
			KeyValuePair pair;

			/* Ensure that we have something on the LHS */
			if (equalsSign == line) {
				goto fail;
			}

			/* Copy the line */
			lineLength = strlen(line);
			copiedLine = j9mem_allocate_memory(lineLength + 1 /* for NULL */, OMRMEM_CATEGORY_VM);
			if (NULL == copiedLine) {
				goto fail;
			}
			memcpy(copiedLine, line, lineLength + 1);
			
			/* Slam a NULL where the equals sign was */
			pair.key = copiedLine;
			pair.value = &copiedLine[equalsSign-line+1];
			pair.value[-1] = 0;
			
			/* Deal with any trailing whitespace on the value */
			trimTrailing(pair.key);
			trimTrailing(pair.value);
			
			/* Insert the halves into the hash table */
			if (NULL == hashTableAdd(result->properties, &pair)) {
				j9mem_free_memory(pair.key);
				goto fail;
			}
		}
	}

	/* Done with the file */
	j9file_close(fileHandle);
	return result;

fail:
	if (fileHandle != -1) {
		j9file_close(fileHandle);
	}
	/* Call close to free memory */
	props_file_close(result);
	return NULL;
}


void 
props_file_close(j9props_file_t file)
{
	PORT_ACCESS_FROM_PORT(file->portLibrary);
	
	if (file->properties != NULL) {
		J9HashTableState walkState;
		KeyValuePair* pair;
		
		/* Loop through and free any keys */
		pair = hashTableStartDo(file->properties, &walkState);
		while (pair != NULL) {
			j9mem_free_memory(pair->key);
			pair = hashTableNextDo(&walkState);
		}

		/* The free the entries */
		hashTableFree(file->properties);
	}
	
	/* Free the heap allocated structure */
	j9mem_free_memory(file);
}


const char* 
props_file_get(j9props_file_t file, const char* key)
{
	KeyValuePair pair, *result;
	pair.key = (char*)key;
	
	result = hashTableFind(file->properties, &pair);
	if (result == NULL) {
		return NULL;
	}
	
	return result->value;
}


void 
props_file_do(j9props_file_t file, j9props_file_iterator iterator, void* userData)
{
	J9HashTableState walkState;
	KeyValuePair* pair;
	
	pair = hashTableStartDo(file->properties, &walkState);
	while (pair != NULL) {
		BOOLEAN keepGoing = (*iterator)(file, pair->key, pair->value, userData);
		if (!keepGoing) {
			return;
		}
		pair = hashTableNextDo(&walkState);
	}
}

