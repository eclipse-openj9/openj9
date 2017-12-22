/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
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

#include "errormessage_internal.h"
#include "ut_j9vrb.h"
#include "omrstdarg.h"

#define BUFFER_SIZE_1K  1024

static IDATA bufferOverFlowHandler(MessageBuffer* buf, UDATA reqSize);
static void releaseMessageBuffer(MessageBuffer* buf);
static IDATA writeMessageBuffer(MessageBuffer* buf, UDATA* msgLength, const char* msgFormat, va_list args);


/**
 * Extend the buffer size in the case of buffer overflow
 * If the current buffer is using a byte array on stack, it will call j9mem_allocate_memory
 * to request a new memory and copy all contents in the existing buffer to the new memory.
 * @param buf - pointer to the MessageBuffer
 * @param reqSize - the requested buffer size
 * @return MSGBUF_OK - success
 * 		   MSGBUF_OUT_OF_MEMORY - out of memory
 */
static IDATA
bufferOverFlowHandler(MessageBuffer* buf, UDATA reqSize)
{
	PORT_ACCESS_FROM_PORT(buf->portLib);
	UDATA newSize = 0;
	UDATA remainingBuffer = 0;
	IDATA returnCode = MSGBUF_OK;

	Assert_VRB_notNull(buf);
	newSize = buf->size;

	do {
		/* Double the existing buffer size only when it is less than 3K to avoid wasting memory */
		if ((newSize / BUFFER_SIZE_1K) < 3) {
			newSize *= 2;
		} else {
			/* If the buffer gap still exists, we add one 1K every time to the new buffer
			 * so as to ensure the total buffer size is always the multiple of 1K.
			 */
			newSize += BUFFER_SIZE_1K;
		}

		remainingBuffer = newSize - buf->cursor;
	} while (remainingBuffer < reqSize);

	/* Allocate new memory for error message buffer for the first time.
	 * Initially, the buffer is a fixed-sized array on stack (local variable: byteArray[1024])
	 */
	if (buf->_bufOnStack == buf->buffer) {
		buf->buffer = j9mem_allocate_memory(newSize, OMRMEM_CATEGORY_VM);
		if (NULL == buf->buffer) {
			buf->buffer = buf->_bufOnStack;
			Trc_VRB_Allocate_Memory_Failed(newSize);
			returnCode = MSGBUF_OUT_OF_MEMORY;
		} else {
			/* Set the new buffer size and copy all data on stack to the new buffer if memory is successfully allocated */
			buf->size = newSize;
			memcpy(buf->buffer, buf->_bufOnStack, buf->cursor);
		}
	} else {
		/* Extend the buffer memory if already allocated recently */
		U_8* newBuffer = j9mem_reallocate_memory(buf->buffer, newSize, OMRMEM_CATEGORY_VM);

		if (NULL == newBuffer) {
			Trc_VRB_Reallocate_Memory_Failed(buf->size, newSize);
			returnCode = MSGBUF_OUT_OF_MEMORY;
		} else {
			buf->size = newSize;
			buf->buffer = newBuffer;
		}
	}

	return returnCode;
}

/**
 * Release the message buffer if any failure in buffer manipulation
 * If the buffer is using a local byte array on stack as storage, then no action will be taken;
 * otherwise, call j9mem_free_memory() to release the memory if allocated recently.
 * @param buf - pointer to the MessageBuffer
 */
static void
releaseMessageBuffer(MessageBuffer* buf)
{
	Assert_VRB_notNull(buf);

	/* Release the memory if allocated recently */
	if (buf->buffer != buf->_bufOnStack) {
		PORT_ACCESS_FROM_PORT(buf->portLib);
		j9mem_free_memory(buf->buffer);
	}

	buf->buffer = NULL;
	buf->size = 0;
	buf->cursor = 0;
	buf->bufEmpty = TRUE;
}

void
initMessageBuffer(J9PortLibrary* portLibrary, MessageBuffer* buf, U_8* byteArray, UDATA size)
{
	Assert_VRB_notNull(buf);
	Assert_VRB_true(size > 0);
	Assert_VRB_notNull(byteArray);

	buf->portLib = portLibrary;
	buf->cursor = 0;					/* There is no message initially stored in the buffer */
	buf->size = size;					/* Initial buffer size */
	buf->_bufOnStack = byteArray;		/* Save the buffer address for later comparison when releasing new memory */
	buf->buffer = buf->_bufOnStack;
	buf->bufEmpty = FALSE;				/* The buffer is set up for use form now on */
}

void
printMessage(MessageBuffer* buf, const char* msgFormat, ...)
{
	PORT_ACCESS_FROM_PORT(buf->portLib);
	IDATA errorCode = MSGBUF_OK;
	UDATA msgLength = 0;
	va_list args;

	Assert_VRB_notNull(buf);

	/* Return if the message buffer is empty or no message to be printed out */
	if ((TRUE == buf->bufEmpty) || (NULL == msgFormat) || ('\0' == msgFormat[0])) {
		return;
	}

	/* Write the error message (args) to the message buffer */
	va_start(args, msgFormat);
	errorCode = writeMessageBuffer(buf, &msgLength, msgFormat, args);
	va_end(args);

	/* Release the memory of message buffer if any failure in writing error messages to buffer */
	if (MSGBUF_OK != errorCode) {
		Trc_VRB_WriteMessageBuffer_Failed(msgLength, errorCode);
		releaseMessageBuffer(buf);
	}
}

/**
 * Write the specified message to the buffer
 * If the space left in the buffer can not accommodate the message, then call the buffer overflow handler first.
 * @param buf - pointer to the MessageBuffer
 * @param msgLength - pointer to the length of error message
 * @param msgFormat - message format
 * @param args - the error message
 * @return MSGBUF_OK - success
 * 		   MSGBUF_ERROR - failure to write message to buffer
 * 		   MSGBUF_OUT_OF_MEMORY - out of memory
 */
static IDATA
writeMessageBuffer(MessageBuffer* buf, UDATA* msgLength, const char* msgFormat, va_list args)
{
	PORT_ACCESS_FROM_PORT(buf->portLib);
	IDATA errorCode = MSGBUF_OK;
	UDATA remainingBuffer = buf->size - buf->cursor;
	IDATA charLength = 0;
	va_list argsCopy;

	Assert_VRB_notNull(buf);

	/* 1) Copy args (error message) to argsCopy as the content of argsCopy will be clean up by j9str_vprintf. 
	 * 2) Ignore the arguments for buffer in j9str_vprintf(buf, sizeof(buf), (char*)format, args)
	 * so as to determine the actual length of incoming message when calling j9str_vprintf.
	 */
	COPY_VA_LIST(argsCopy, args);
	*msgLength = j9str_vprintf(NULL, 0, msgFormat, argsCopy);
	END_VA_LIST_COPY(argsCopy);

	if (*msgLength > 0) {
		/* Extend the buffer size when the length of the passed-in message
		 * exceeds the remaining space in the existing buffer.
		 */
		if (*msgLength > remainingBuffer) {
			errorCode = bufferOverFlowHandler(buf, *msgLength);
			if (MSGBUF_OK != errorCode) {
				goto exit;
			}
		}

		/* str_vprintf always null terminates and returns the number of characters (excluding the null terminator)
		 * on success or negative value if error occurs.
		 */
		charLength = j9str_vprintf((char*)&buf->buffer[buf->cursor], *msgLength, msgFormat, args);
		buf->cursor += charLength;
	}

exit:
	return errorCode;
}
