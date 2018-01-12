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

#include <string.h>
#include "libhlp.h"
#include "exelib_internal.h"

#define MIN_GROWTH 128

J9StringBuffer* strBufferCat(struct J9PortLibrary *portLibrary, J9StringBuffer* buffer, const char* string) {
	UDATA len = strlen(string);

	buffer = strBufferEnsure(portLibrary, buffer, len);
	if (buffer) {
		strcat(buffer->data, string);
		buffer->remaining -= len;
	}

	return buffer;
}


J9StringBuffer* strBufferEnsure(struct J9PortLibrary *portLibrary, J9StringBuffer* buffer, UDATA len) {

	if (buffer == NULL) {
		PORT_ACCESS_FROM_PORT(portLibrary);
		UDATA newSize = len > MIN_GROWTH ? len : MIN_GROWTH;
		buffer = j9mem_allocate_memory( newSize + 1 + sizeof(UDATA), OMRMEM_CATEGORY_VM);	/* 1 for null terminator */
		if (buffer != NULL) {
			buffer->remaining = newSize;
			buffer->data[0] = '\0';
		}
		return buffer;
	}

	if (len > buffer->remaining) {
		PORT_ACCESS_FROM_PORT(portLibrary);
		UDATA newSize = len > MIN_GROWTH ? len : MIN_GROWTH;
		J9StringBuffer* new = j9mem_allocate_memory(strlen((const char*)buffer->data) + newSize + sizeof(UDATA) + 1 , OMRMEM_CATEGORY_VM);
		if (new) {
			new->remaining = newSize;
			strcpy(new->data, (const char *)buffer->data);
		}
		j9mem_free_memory(buffer);
		return new;
	}

	return buffer;
}


J9StringBuffer* strBufferPrepend(struct J9PortLibrary *portLibrary, J9StringBuffer* buffer, char* string) {
	UDATA len = strlen(string);

	buffer = strBufferEnsure(portLibrary, buffer, len);
	if (buffer) {
		memmove(buffer->data + len, buffer->data, strlen((const char*)buffer->data) + 1);
		strncpy(buffer->data, string, len);
		buffer->remaining -= len;
	}

	return buffer;
}


char* strBufferData(J9StringBuffer* buffer) {
	return buffer ? buffer->data : NULL;
}
