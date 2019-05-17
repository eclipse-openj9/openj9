/*******************************************************************************
 * Copyright (c) 1998, 2019 IBM Corp. and others
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
#include <stdio.h>
#include <stddef.h>

#include "j9cfg.h"
#include "j9port.h"

#include "rastrace_internal.h"

const char *EXCEPTION_BUFFER_NAME = "exception";
const char *GCLOGGER_BUFFER_NAME = "gclogger";
const char *THREAD_BUFFER_NAME = "currentThread";

#define ONEMILLION (1000000)

omr_error_t
omr_trc_getTraceFileIterator(OMRPortLibrary* portLib, char* fileName, UtTraceFileIterator** iteratorPtr,
		FormatStringCallback getFormatStringFn)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);
	UtTraceFileIterator *iterator = NULL;
	intptr_t traceFileHandle = -1;
	intptr_t bytesRead = -1;
	UtTraceFileHdr dummyHeader;
	UtTraceFileHdr* header = NULL;

	/* Open the trace file and copy out the header. */
	traceFileHandle = omrfile_open(fileName, EsOpenRead, 0);

	if (traceFileHandle < 0) {
		return OMR_ERROR_FILE_UNAVAILABLE;
	}

	bytesRead = omrfile_read(traceFileHandle, &dummyHeader, sizeof(UtTraceFileHdr));

	if (bytesRead != sizeof(UtTraceFileHdr)) {
		omrmem_free_memory(header);
		return OMR_ERROR_INTERNAL;
	}

	/* Check for a valid looking header. */
	if (dummyHeader.endianSignature != UT_ENDIAN_SIGNATURE) {
		/* TODO - If this is the wrong endianness we'll either need to fix up the header
		 * and change the formatter to cope with files from other platforms OR document
		 * that the native formatter can't cope with that.
		 */
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	/* Now we know how big the header really is, reset the file and read it again. */
	header = omrmem_allocate_memory(dummyHeader.header.length, OMRMEM_CATEGORY_TRACE);

	if ( NULL == header) {
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}

	omrfile_seek(traceFileHandle, 0, EsSeekSet);
	bytesRead = omrfile_read(traceFileHandle, header, dummyHeader.header.length);

	if (bytesRead != dummyHeader.header.length) {
		omrmem_free_memory(header);
		return OMR_ERROR_INTERNAL;
	}

	/* Check for a valid looking header. */
	if (header->endianSignature != UT_ENDIAN_SIGNATURE) {
		/* TODO - If this is the wrong endianness we'll either need to fix up the header
		 * and change the formatter to cope with files from other platforms OR document
		 * that the native formatter can't cope with that.
		 */
		omrmem_free_memory(header);
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	iterator = omrmem_allocate_memory(sizeof(UtTraceFileIterator), OMRMEM_CATEGORY_TRACE);

	if (NULL == iterator) {
		omrmem_free_memory(header);
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}

	iterator->header = header;
	/* Fix up pointers to the initialisation data. */
	iterator->traceSection = ((UtTraceSection*)((char*)header + header->traceStart));
	iterator->serviceSection = ((UtServiceSection*)((char*)header + header->serviceStart));
	iterator->startupSection = ((UtStartupSection*)((char*)header + header->startupStart));
	iterator->activeSection = ((UtActiveSection*)((char*)header + header->activeStart));
	iterator->procSection = ((UtProcSection*)((char*)header + header->processorStart));
	iterator->getFormatStringFn = getFormatStringFn;
	iterator->currentPosition = bytesRead;
	iterator->portLib = portLib;
	iterator->traceFileHandle = traceFileHandle;

	*iteratorPtr = iterator;

	return OMR_ERROR_NONE;

}


/**
 * This returns a structure for iterating over a trace buffer for
 * use with omr_trc_formatNextTracePoint.
 *
 * Important: This holds the global trace lock until trcFreeTracePointIterator is called.
 */
UtTracePointIterator *
trcGetTracePointIteratorForBuffer(UtThreadData **thr, const char *bufferName)
{
	UtTracePointIterator *iter = NULL;
	UtTraceBuffer *trcBuf = NULL;
	uint64_t spanPlatform, spanSystem;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	if (bufferName == NULL) {
		UT_DBGOUT_CHECKED(1, ("<UT> trcGetTracePointIterator called with NULL bufferName\n"));
		return NULL;
	}

	/* get the exception lock so that we're iterating over static data */
	if (getTraceLock(thr) != TRUE) {
		return NULL;
	}

	if ((strcmp(bufferName, GCLOGGER_BUFFER_NAME) == 0) || (strcmp(bufferName, EXCEPTION_BUFFER_NAME) == 0)) {
		if (utGlobal != NULL) {
			/* look up exception trace buffer */
			trcBuf = UT_GLOBAL(exceptionTrcBuf);
			if (trcBuf == NULL) {
				/* this is a valid condition - there is no data in the exception buffer - maybe trace is off or no gc has occurred */
				UT_DBGOUT_CHECKED(1,
						("<UT> trcGetTracePointIteratorForBuffer: %s no data found in buffer\n", bufferName));
			}
		}
	} else if (strcmp(bufferName, THREAD_BUFFER_NAME) == 0) {
		UtThreadData **currentThread = twThreadSelf();
		if (currentThread != NULL && *currentThread != NULL) {
			trcBuf = (*currentThread)->trcBuf; /* No-one should write to this since it's the buffer for this thread! */
		} else {
			goto cleanup;
		}
	} else {
		UT_DBGOUT_CHECKED(1, ("<UT> trcGetTracePointIterator called with unsupported bufferName: %s\n", bufferName));
		goto cleanup;
	}

	if (trcBuf == NULL) {
		goto cleanup;
	}

	iter = (UtTracePointIterator *)j9mem_allocate_memory(sizeof(UtTracePointIterator), OMRMEM_CATEGORY_TRACE);
	if (iter == NULL) {
		UT_DBGOUT_CHECKED(1, ("<UT> trcGetTracePointIteratorForBuffer cannot allocate iterator\n"));
		goto cleanup;
	}

	iter->buffer = (UtTraceBuffer *)j9mem_allocate_memory(UT_GLOBAL(bufferSize) + offsetof(UtTraceBuffer, record),
			OMRMEM_CATEGORY_TRACE);
	if (iter->buffer == NULL) {
		UT_DBGOUT_CHECKED(1, ("<UT> trcGetTracePointIteratorForBuffer cannot allocate iterator's buffer\n"));
		j9mem_free_memory(iter);
		goto cleanup;
	}

	/* set up the iterator */
	memcpy(iter->buffer, trcBuf, UT_GLOBAL(bufferSize) + offsetof(UtTraceBuffer, record));
	iter->recordLength = UT_GLOBAL(bufferSize);
	iter->end = trcBuf->record.nextEntry;
	iter->start = trcBuf->record.firstEntry;
	iter->dataLength = trcBuf->record.nextEntry - trcBuf->record.firstEntry;
	iter->currentUpperTimeWord = (uint64_t)(trcBuf->record.sequence) & J9CONST64(0xFFFFFFFF00000000);
	iter->currentPos = trcBuf->record.nextEntry;
	iter->startPlatform = UT_GLOBAL(startPlatform);
	iter->startSystem = UT_GLOBAL(startSystem);
	iter->endPlatform = j9time_hires_clock();
	iter->endSystem = ((uint64_t) j9time_current_time_millis());
	iter->portLib = UT_GLOBAL(portLibrary);
	iter->getFormatStringFn = &getFormatString;

	spanPlatform = iter->endPlatform - iter->startPlatform;
	spanSystem = iter->endSystem - iter->startSystem;

	iter->timeConversion = spanPlatform / spanSystem;
	if (iter->timeConversion == 0) {
		/* this will be used as the divisor in formatting time stamps */
		iter->timeConversion = 1;
	}

#ifdef J9VM_ENV_LITTLE_ENDIAN
	iter->isBigEndian = FALSE;
#else
	iter->isBigEndian = TRUE;
#endif
	iter->isCircularBuffer = !UT_GLOBAL(externalTrace) && !UT_GLOBAL(extExceptTrace);
	iter->iteratorHasWrapped = FALSE;
	iter->processingIncompleteDueToPartialTracePoint = FALSE;
	iter->longTracePointLength = 0;

	iter->numberOfBytesInPlatformUDATA = (uint32_t)sizeof(uintptr_t);
	iter->numberOfBytesInPlatformPtr = (uint32_t)sizeof(char *);
	iter->numberOfBytesInPlatformShort = (uint32_t)sizeof(short);

	UT_DBGOUT_CHECKED(4,
			("<UT> firstEntry: %d, offset of record: %ld buffer size: %d endianness %s\n", iter->start, offsetof(UtTraceBuffer, record), UT_GLOBAL(bufferSize), (iter->isBigEndian)?"bigEndian":"littleEndian"));
	UT_DBGOUT_CHECKED(2, ("<UT> trcGetTracePointIteratorForBuffer: %s returning iterator %p\n", bufferName, iter));
	return iter;

	cleanup: freeTraceLock(thr);
	return NULL;
}

/**
 * This frees the trace point iterator structure.
 *
 * Important: This releases the global trace lock taken by trcGetTracePointIteratorForBuffer.
 */
omr_error_t
trcFreeTracePointIterator(UtThreadData **thr, UtTracePointIterator *iter)
{
	if (iter != NULL) {
		freeTraceLock(thr);
	}
	return omr_trc_freeTracePointIterator(iter);
}

omr_error_t
omr_trc_freeTracePointIterator(UtTracePointIterator *iter)
{
	if (iter != NULL) {
		PORT_ACCESS_FROM_PORT(iter->portLib);
		j9mem_free_memory(iter->buffer);
		UT_DBGOUT_CHECKED(2, ("<UT> trcFreeTracePointIterator freeing iterator %p\n", iter));
		j9mem_free_memory(iter);
	}
	return OMR_ERROR_NONE;
}

static unsigned char
getUnsignedByteFromBuffer(UtTraceRecord *record, uint32_t offset)
{
	char *tempPtr = (char *)record;
	return tempPtr[offset];
}

static uint32_t
getTraceIdFromBuffer(UtTraceRecord *record, uint32_t offset)
{
	uint32_t ret = 0;
	unsigned char data[3];

	data[0] = getUnsignedByteFromBuffer(record, offset);
	data[1] = getUnsignedByteFromBuffer(record, offset + 1);
	data[2] = getUnsignedByteFromBuffer(record, offset + 2);

	/* trace ID's are stored in hard coded big endian order */
	ret = (data[0] << 16) | (data[1] << 8) | (data[2]);

	/* remove comp Id's */
	ret &= 0x3FFF;
	return ret;
}

static uint16_t
getU_16FromBuffer(UtTraceRecord *record, uint32_t offset, int32_t isBigEndian)
{
	uint16_t ret = 0;
	unsigned char data[2];

	data[0] = getUnsignedByteFromBuffer(record, offset);
	data[1] = getUnsignedByteFromBuffer(record, offset + 1);

	/* integer values are written out in platform endianness */
	if (isBigEndian) {
		ret = (data[0] << 8) | (data[1]);
	} else {
		ret = (data[1] << 8) | (data[0]);
	}
	return ret;
}

static uint32_t
getU_32FromBuffer(UtTraceRecord *record, uint32_t offset, int32_t isBigEndian)
{
	uint32_t ret = 0;
	unsigned char data[4];

	data[0] = getUnsignedByteFromBuffer(record, offset);
	data[1] = getUnsignedByteFromBuffer(record, offset + 1);
	data[2] = getUnsignedByteFromBuffer(record, offset + 2);
	data[3] = getUnsignedByteFromBuffer(record, offset + 3);

	/* integer values are written out in platform endianness */
	if (isBigEndian) {
		ret = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]);
	} else {
		ret = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | (data[0]);
	}
	return ret;
}

static uint64_t
getU_64FromBuffer(UtTraceRecord *record, uint32_t offset, int32_t isBigEndian)
{
	uint64_t ret = 0;
	unsigned char data[8];

	data[0] = getUnsignedByteFromBuffer(record, offset);
	data[1] = getUnsignedByteFromBuffer(record, offset + 1);
	data[2] = getUnsignedByteFromBuffer(record, offset + 2);
	data[3] = getUnsignedByteFromBuffer(record, offset + 3);
	data[4] = getUnsignedByteFromBuffer(record, offset + 4);
	data[5] = getUnsignedByteFromBuffer(record, offset + 5);
	data[6] = getUnsignedByteFromBuffer(record, offset + 6);
	data[7] = getUnsignedByteFromBuffer(record, offset + 7);

	/* integer values are written out in platform endianness */
	if (isBigEndian) {
		ret = ((uint64_t)data[0] << 56) | ((uint64_t)data[1] << 48) | ((uint64_t)data[2] << 40) | ((uint64_t)data[3] << 32)
				| ((uint64_t)data[4] << 24) | ((uint64_t)data[5] << 16) | ((uint64_t)data[6] << 8) | ((uint64_t)data[7]);
	} else {
		ret = ((uint64_t)data[7] << 56) | ((uint64_t)data[6] << 48) | ((uint64_t)data[5] << 40) | ((uint64_t)data[4] << 32)
				| ((uint64_t)data[3] << 24) | ((uint64_t)data[2] << 16) | ((uint64_t)data[1] << 8) | ((uint64_t)data[0]);
	}
	return ret;
}

#define UT_TRACE_FORMATTER_64BIT_DATA		64
#define UT_TRACE_FORMATTER_32BIT_DATA		32
#define UT_TRACE_FORMATTER_8BIT_DATA		8
#define UT_TRACE_FORMATTER_STRING_DATA	-1

static uint32_t
readConsumeAndSPrintfParameter(J9PortLibrary *portLib, char *rawParameterData, uint32_t rawParameterDataLength,
		uint32_t *offsetInParameterData, char *destBuffer, uint32_t destBufferLength, uint32_t *offsetInDestBuffer,
		const char *format, int32_t dataType, uint32_t nWidthAndPrecisions, int32_t isBigEndian)
{
	uintptr_t temp = 0;
	uint64_t u64data = 0;
	uint32_t u32data = 0;
	int16_t i16data = 0;
	unsigned char u8data = 0;
	char *strValue = NULL;
	uint32_t precisionOrWidth1 = 0;
	uint32_t precisionOrWidth2 = 0;
	PORT_ACCESS_FROM_PORT(portLib);

	if (rawParameterData == NULL || rawParameterDataLength == 0) {
		/* this must be minimal tracing */
		if ((destBufferLength - (*offsetInDestBuffer)) < 3) {
			/* error - just return */
			return 0;
		} else {
			/* give an indication that we haven't filled in the format on purpose */
			destBuffer[*offsetInDestBuffer] = '?';
			destBuffer[*offsetInDestBuffer + 1] = '?';
			destBuffer[*offsetInDestBuffer + 2] = '?';
			*offsetInDestBuffer += 3;
			return 3;
		}
	}

	if (nWidthAndPrecisions == 1) {
		precisionOrWidth1 = getU_32FromBuffer((UtTraceRecord *)rawParameterData, *offsetInParameterData, isBigEndian);
		*offsetInParameterData += 4;
	} else if (nWidthAndPrecisions == 2) {
		precisionOrWidth1 = getU_32FromBuffer((UtTraceRecord *)rawParameterData, *offsetInParameterData, isBigEndian);
		precisionOrWidth2 = getU_32FromBuffer((UtTraceRecord *)rawParameterData, *offsetInParameterData, isBigEndian);
		*offsetInParameterData += 8;
	}

	if (dataType == UT_TRACE_FORMATTER_64BIT_DATA) {
		/* handling a generic 64 bit integer */
		u64data = getU_64FromBuffer((UtTraceRecord *)rawParameterData, *offsetInParameterData, isBigEndian);
		if (nWidthAndPrecisions == 2) {
			temp = j9str_printf(PORTLIB, NULL, 0, format, precisionOrWidth1, precisionOrWidth2, u64data);
			if (temp > destBufferLength) {
				/* can't write data to dest buffer - it's too full, be cautious and just return */
				UT_DBGOUT_CHECKED(1,
						("<UT> readConsumeAndSPrintfParameter destination buffer exhausted: %d [%s]\n", dataType, format));
				return 0;
			}
			temp = j9str_printf(PORTLIB, destBuffer + (*offsetInDestBuffer), destBufferLength - (*offsetInDestBuffer),
					format, precisionOrWidth1, precisionOrWidth2, u64data);
		} else if (nWidthAndPrecisions == 1) {
			temp = j9str_printf(PORTLIB, NULL, 0, format, precisionOrWidth1, u64data);
			if (temp > destBufferLength) {
				/* can't write data to dest buffer - it's too full, be cautious and just return */
				UT_DBGOUT_CHECKED(1,
						("<UT> readConsumeAndSPrintfParameter destination buffer exhausted: %d [%s]\n", dataType, format));
				return 0;
			}
			temp = j9str_printf(PORTLIB, destBuffer + (*offsetInDestBuffer), destBufferLength - (*offsetInDestBuffer),
					format, precisionOrWidth1, u64data);
		} else {
			temp = j9str_printf(PORTLIB, NULL, 0, format, u64data);
			if (temp > destBufferLength) {
				/* can't write data to dest buffer - it's too full, be cautious and just return */
				UT_DBGOUT_CHECKED(1,
						("<UT> readConsumeAndSPrintfParameter destination buffer exhausted: %d [%s]\n", dataType, format));
				return 0;
			}
			temp = j9str_printf(PORTLIB, destBuffer + (*offsetInDestBuffer), destBufferLength - (*offsetInDestBuffer),
					format, u64data);
		}
		*offsetInParameterData += 8;
	} else if (dataType == UT_TRACE_FORMATTER_32BIT_DATA) {
		/* handling a generic 32 bit integer */
		u32data = getU_32FromBuffer((UtTraceRecord *)rawParameterData, *offsetInParameterData, isBigEndian);
		if (nWidthAndPrecisions == 2) {
			temp = j9str_printf(PORTLIB, NULL, 0, format, precisionOrWidth1, precisionOrWidth2, u32data);
			if (temp > destBufferLength) {
				/* can't write data to dest buffer - it's too full, be cautious and just return */
				UT_DBGOUT_CHECKED(1,
						("<UT> readConsumeAndSPrintfParameter destination buffer exhausted: %d [%s]\n", dataType, format));
				return 0;
			}
			temp = j9str_printf(PORTLIB, destBuffer + (*offsetInDestBuffer), destBufferLength - (*offsetInDestBuffer),
					format, precisionOrWidth1, precisionOrWidth2, u32data);
		} else if (nWidthAndPrecisions == 1) {
			temp = j9str_printf(PORTLIB, NULL, 0, format, precisionOrWidth1, u32data);
			if (temp > destBufferLength) {
				/* can't write data to dest buffer - it's too full, be cautious and just return */
				UT_DBGOUT_CHECKED(1,
						("<UT> readConsumeAndSPrintfParameter destination buffer exhausted: %d [%s]\n", dataType, format));
				return 0;
			}
			temp = j9str_printf(PORTLIB, destBuffer + (*offsetInDestBuffer), destBufferLength - (*offsetInDestBuffer),
					format, precisionOrWidth1, u32data);
		} else {
			temp = j9str_printf(PORTLIB, NULL, 0, format, u32data);
			if (temp > destBufferLength) {
				/* can't write data to dest buffer - it's too full, be cautious and just return */
				UT_DBGOUT_CHECKED(1,
						("<UT> readConsumeAndSPrintfParameter destination buffer exhausted: %d [%s]\n", dataType, format));
				return 0;
			}
			temp = j9str_printf(PORTLIB, destBuffer + (*offsetInDestBuffer), destBufferLength - (*offsetInDestBuffer),
					format, u32data);
		}
		*offsetInParameterData += 4;
	} else if (dataType == UT_TRACE_FORMATTER_8BIT_DATA) {
		/* handling a char */
		u8data = getUnsignedByteFromBuffer((UtTraceRecord *)rawParameterData, *offsetInParameterData);
		temp = j9str_printf(PORTLIB, NULL, 0, format, u8data);
		if (temp > destBufferLength) {
			/* can't write data to dest buffer - it's too full, be cautious and just return */
			UT_DBGOUT_CHECKED(1,
					("<UT> readConsumeAndSPrintfParameter destination buffer exhausted: %d [%s]\n", dataType, format));
			return 0;
		}
		temp = j9str_printf(PORTLIB, destBuffer + (*offsetInDestBuffer), destBufferLength - (*offsetInDestBuffer),
				format, u8data);
		*offsetInParameterData += 1;
	} else if (dataType == UT_TRACE_FORMATTER_STRING_DATA) {
		/* handling a string */
		/* any length already read is a false one - the trace engine will have modified the utf8 length field */
		if (nWidthAndPrecisions == 1) {
			*offsetInParameterData -= 4;
			i16data = (int16_t)getU_16FromBuffer((UtTraceRecord *)rawParameterData, *offsetInParameterData, isBigEndian);
			*offsetInParameterData += 2;
			strValue = rawParameterData + (*offsetInParameterData);
			temp = j9str_printf(PORTLIB, NULL, 0, format, i16data, strValue);
			if (temp > destBufferLength) {
				/* can't write data to dest buffer - it's too full, be cautious and just return */
				UT_DBGOUT_CHECKED(1,
						("<UT> readConsumeAndSPrintfParameter destination buffer exhausted: %d [%s]\n", dataType, format));
				return 0;
			}
			temp = j9str_printf(PORTLIB, destBuffer + (*offsetInDestBuffer), destBufferLength - (*offsetInDestBuffer),
					format, i16data, strValue);
		} else {
			/* it's just a plain string */
			strValue = rawParameterData + (*offsetInParameterData);
			temp = j9str_printf(PORTLIB, NULL, 0, format, strValue);
			if (temp > destBufferLength) {
				/* can't write data to dest buffer - it's too full, be cautious and just return */
				UT_DBGOUT_CHECKED(1,
						("<UT> readConsumeAndSPrintfParameter destination buffer exhausted: %d [%s]\n", dataType, format));
				return 0;
			}
			temp = j9str_printf(PORTLIB, destBuffer + (*offsetInDestBuffer), destBufferLength - (*offsetInDestBuffer),
					format, strValue);
			/* for the null in the parm data */
			*offsetInParameterData += 1;
		}
		*offsetInParameterData += (uint32_t)temp;
	} else {
		UT_DBGOUT_CHECKED(1, ("<UT> bad byte width in readConsumeAndSPrintfParameter: %d [%s]\n", dataType, format));
	}

	*offsetInDestBuffer += (uint32_t)temp;
	return (uint32_t)temp;
}

static uint32_t
formatTracePointParameters(UtTracePointIterator *iter, char *destBuffer, uint32_t destBufferLength, const char *format,
		char *rawParameterData, uint32_t rawParameterDataLength)
{
	uint32_t formatLength;
	uint32_t offsetInFormat = 0, offsetInDestBuffer = 0, offsetInParameterData = 0;
	int numberOfStars = 0;
	char type = '\0';
	int32_t longModifierFound = FALSE;
	int32_t platformUDATAWidthDataFound = FALSE;
	char formatBuffer[20];
	uint32_t offsetInFormatBuffer = 0;
	uint32_t offsetOfType = 0;
	int traceDataType = 0;

	if (destBuffer == NULL || destBufferLength == 0) {
		UT_DBGOUT_CHECKED(1,
				("<UT> formatTracePointParameters called with destination buffer %p and destination buffer length %u\n", destBuffer, destBufferLength));
		return 0;
	}

	if (format == NULL) {
		UT_DBGOUT_CHECKED(1, ("<UT> formatTracePointParameters called with no format string\n"));
		return 0;
	}
	formatLength = (uint32_t)strlen(format);

	/* walk the format string copying it char by char into the destination buffer */
	for (offsetInFormat = 0, offsetInDestBuffer = 0; offsetInFormat <= formatLength;
			offsetInFormat++, offsetInDestBuffer++) {
		if (format[offsetInFormat] == '%') {
			if (format[offsetInFormat + 1] == '%') {
				destBuffer[offsetInDestBuffer] = format[offsetInFormat];
				offsetInFormat++; /* skip the percent, just write a single percent to the destination buffer */
			} else {
				int32_t foundType = FALSE;
				offsetInFormatBuffer = 0;
				platformUDATAWidthDataFound = longModifierFound = FALSE;
				numberOfStars = 0;
				traceDataType = 0;

				for (offsetOfType = 0; foundType == FALSE; offsetOfType++) {
					/* have we read off the end? */
					if (format[offsetInFormat + offsetOfType] == 'x' || format[offsetInFormat + offsetOfType] == 'X'
							|| format[offsetInFormat + offsetOfType] == 'u'
							|| format[offsetInFormat + offsetOfType] == 'i'
							|| format[offsetInFormat + offsetOfType] == 'd'
							|| format[offsetInFormat + offsetOfType] == 'f'
							|| format[offsetInFormat + offsetOfType] == 'p'
							|| format[offsetInFormat + offsetOfType] == 'P'
							|| format[offsetInFormat + offsetOfType] == 'c'
							|| format[offsetInFormat + offsetOfType] == 's') {
						/* we found the type */
						foundType = TRUE;
						type = format[offsetInFormat + offsetOfType];
					} else if (format[offsetInFormat + offsetOfType] == '*') {
						numberOfStars++;
					} else if (format[offsetInFormat + offsetOfType] == 'l') {
						/* 64bit number - its actually 'll' but we'll give it benefit of doubt */
						longModifierFound = TRUE;
					} else if (format[offsetInFormat + offsetOfType] == 'z') {
						/* platform udata width data */
						platformUDATAWidthDataFound = TRUE;
					}
					/* this is both default behaviour and always needs to happen */
					formatBuffer[offsetInFormatBuffer] = format[offsetInFormat + offsetInFormatBuffer];
					offsetInFormatBuffer++;
				}
				offsetInFormat += (offsetOfType - 1);
				formatBuffer[offsetInFormatBuffer] = '\0';
				/*increment offsetInFormatBuffer here if it needs to be used further*/

				switch (type)
				{
				case 'x':
				case 'X':
				case 'u':
				case 'i':
				case 'd':
					if (longModifierFound == TRUE) {
						traceDataType = UT_TRACE_FORMATTER_64BIT_DATA;
					} else if (platformUDATAWidthDataFound == TRUE) {
						if (iter->numberOfBytesInPlatformUDATA == 8) {
							traceDataType = UT_TRACE_FORMATTER_64BIT_DATA;
						} else {
							traceDataType = UT_TRACE_FORMATTER_32BIT_DATA;
						}
					} else {
						/* normal integer */
						traceDataType = UT_TRACE_FORMATTER_32BIT_DATA;
					}
					readConsumeAndSPrintfParameter(iter->portLib, rawParameterData, rawParameterDataLength,
							&offsetInParameterData, destBuffer, destBufferLength, &offsetInDestBuffer,
							(const char *)formatBuffer, traceDataType, numberOfStars, iter->isBigEndian);
					break;
				case 'p':
				case 'P':
					if (iter->numberOfBytesInPlatformPtr == 8) {
						traceDataType = UT_TRACE_FORMATTER_64BIT_DATA;
					} else {
						traceDataType = UT_TRACE_FORMATTER_32BIT_DATA;
					}

					readConsumeAndSPrintfParameter(iter->portLib, rawParameterData, rawParameterDataLength,
							&offsetInParameterData, destBuffer, destBufferLength, &offsetInDestBuffer,
							(const char *)formatBuffer, traceDataType, numberOfStars, iter->isBigEndian);
					break;
				case 'c':
					traceDataType = UT_TRACE_FORMATTER_8BIT_DATA;
					readConsumeAndSPrintfParameter(iter->portLib, rawParameterData, rawParameterDataLength,
							&offsetInParameterData, destBuffer, destBufferLength, &offsetInDestBuffer,
							(const char *)formatBuffer, traceDataType, numberOfStars, iter->isBigEndian);
					break;
				case 's':
					traceDataType = UT_TRACE_FORMATTER_STRING_DATA;
					readConsumeAndSPrintfParameter(iter->portLib, rawParameterData, rawParameterDataLength,
							&offsetInParameterData, destBuffer, destBufferLength, &offsetInDestBuffer,
							(const char *)formatBuffer, traceDataType, numberOfStars, iter->isBigEndian);
					break;
				case 'f':
					/* Floats are promoted to doubles by convention, so all %fs are 64bit */
					traceDataType = UT_TRACE_FORMATTER_64BIT_DATA;
					readConsumeAndSPrintfParameter(iter->portLib, rawParameterData, rawParameterDataLength,
							&offsetInParameterData, destBuffer, destBufferLength, &offsetInDestBuffer,
							(const char *)formatBuffer, traceDataType, numberOfStars, iter->isBigEndian);
					break;
				default:
					UT_DBGOUT_CHECKED(1,
							("<UT> formatTracePointParameters unknown trace format type: %c [%s]\n", type, formatBuffer))
					;
					break;
				}
				offsetInDestBuffer--;

			}
		} else {
			if (offsetInDestBuffer > destBufferLength) {
				/* return now before writing off the end */
				UT_DBGOUT_CHECKED(1,
						("<UT> formatTracePointParameters truncated output due to buffer exhaustion at format type: %c [%s]\n", type, formatBuffer));
				return offsetInDestBuffer;
			}
			destBuffer[offsetInDestBuffer] = format[offsetInFormat];
		}
	}

	/* the number of characters successfully written to destination buffer */
	return offsetInDestBuffer;
}

#undef UT_TRACE_FORMATTER_64BIT_DATA
#undef UT_TRACE_FORMATTER_32BIT_DATA
#undef UT_TRACE_FORMATTER_8BIT_DATA
#undef UT_TRACE_FORMATTER_STRING_DATA

static char *
parseTracePoint(J9PortLibrary *portLib, UtTraceRecord *record, uint32_t offset, int tpLength,
		uint64_t *timeStampMostSignificantBytes, UtTracePointIterator *iter, char* buffer, uint32_t bufferLength)
{
	uint32_t traceId;
	uint32_t timeStampLeastSignificantBytes;
	uint64_t tempUpper, tempLower;
	uint64_t timeStamp = 0;
	char *modName;
	uint32_t modNameLength;
	char *tempPtr = (char *)record;
	char *formatString;
	char tempSwapChar = '\0';
	char tempSwapChar2 = '\0';
	char *tempSwapCharLoc;
	uint32_t nanos, millis, seconds, minutes, hours;
	uint64_t splitTime = 0, splitTimeRem = 0;
	uint32_t offsetOfParameters = 0;
	char *rawParameterData;
	uint32_t rawParameterDataLength;
	PORT_ACCESS_FROM_PORT(portLib);

	if (bufferLength == 0 || buffer == NULL) {
		UT_DBGOUT_CHECKED(1, ("<UT> parseTracePoint called with unpopulated iterator formatted tracepoint buffer\n"));
		return NULL;
	}

	if (record == NULL) {
		UT_DBGOUT_CHECKED(1, ("<UT> parseTracePoint called with NULL record\n"));
		return NULL;
	}

	if (0 == offset) {
		UT_DBGOUT_CHECKED(1, ("<UT> parseTracePoint called with zero offset\n"));
		return NULL;
	}

	traceId = getTraceIdFromBuffer(record, offset + TRACEPOINT_RAW_DATA_TP_ID_OFFSET);

	/* check for all the control/internal tracepoints */
	/* 0x0010nnnn8 == lost record tracepoint */
	if (traceId == 0x0010) {
		/* 	too much danger of looping to recurse here - return a warning string and let
		 omr_trc_formatNextTracePoint(thr, iter) make the call on whether it can extract
		 any further data */
		return "*** lost records found in trace buffer - use external formatting tools for details.\n";
	}

	/* 0x0000nnnn8 == sequence wrap tracepoint */
	if ((traceId == 0x0000) && (tpLength == UT_TRC_SEQ_WRAP_LENGTH)) {
		/* read in the timer upper word, write it into iterator struct, and (recursively) return the next tracepoint parsed */
		tempUpper = getU_32FromBuffer(record, offset + TRACEPOINT_RAW_DATA_TIMER_WRAP_UPPER_WORD_OFFSET,
				iter->isBigEndian);
		tempUpper <<= 32;
		*timeStampMostSignificantBytes = tempUpper;
		return omr_trc_formatNextTracePoint(iter, buffer, bufferLength);
	}

	if (tpLength == 4) {
		/* this is a long tracepoint */
		char longTracePointLength;

		longTracePointLength = getUnsignedByteFromBuffer(record, offset + TRACEPOINT_RAW_DATA_TP_ID_OFFSET + 2);
		/* the next call to omr_trc_formatNextTracePoint will pick this up and use it appropriately */
		iter->longTracePointLength = (uint32_t)longTracePointLength;
		return omr_trc_formatNextTracePoint(iter, buffer, bufferLength);
	}

	if (tpLength == UT_TRC_SEQ_WRAP_LENGTH) {
		return omr_trc_formatNextTracePoint(iter, buffer, bufferLength);
	}

	/* otherwise all is well and we have a normal tracepoint */
	timeStampLeastSignificantBytes = getU_32FromBuffer(record, offset + TRACEPOINT_RAW_DATA_TIMESTAMP_OFFSET,
			iter->isBigEndian);
	modNameLength = getU_32FromBuffer(record, offset + TRACEPOINT_RAW_DATA_MODULE_NAME_LENGTH_OFFSET,
			iter->isBigEndian);

	/* Module name must not be longer than the tracepoint length - start of module name string */
	/* modNameLength == 0 -- no modname - most likely partially overwritten - its unformattable as a result! */
	if (modNameLength == 0 || tpLength < TRACEPOINT_RAW_DATA_MODULE_NAME_DATA_OFFSET
			|| modNameLength > (uint32_t)(tpLength - TRACEPOINT_RAW_DATA_MODULE_NAME_DATA_OFFSET)) {
		return omr_trc_formatNextTracePoint(iter, buffer, bufferLength);
	}

	modName = &tempPtr[offset + TRACEPOINT_RAW_DATA_MODULE_NAME_DATA_OFFSET]; /* the modName was written straight to buf as (char *) */
	if (!strncmp(modName, "INTERNALTRACECOMPONENT", 22)) {
		if (traceId == UT_TRC_CONTEXT_ID) {
			/* ignore and recurse - its just a thread id sitting in the buffer used for external trace tool */
			return omr_trc_formatNextTracePoint(iter, buffer, bufferLength);
		}
		modNameLength = 2;
		modName = "dg";
		formatString = "internal Trace Data Point";
	} else {
		if (traceId <= 256) {
			return omr_trc_formatNextTracePoint(iter, buffer, bufferLength);
		}
		traceId -= 257;
		/* temporarily add NULL terminator to the modName in copy of the buffer for getFormatString
		 * to be able to do look up on it - save the overwritten data and reinstate once finished. Doing 
		 * this avoids allocing a temp buffer. We also need to check to see if this is a composite name, 
		 * e.g. pool(j9mm). If it is then we need to take the string before the opening brace as the
		 * component name. We don't do this in getFormatString because that's in the fastpath for
		 * -Xtrace:print
		 */
		tempSwapChar = modName[modNameLength];
		modName[modNameLength] = '\0';

		tempSwapCharLoc = strchr(modName, '(');
		if (tempSwapCharLoc != NULL) {
			tempSwapChar2 = *tempSwapCharLoc;
			*tempSwapCharLoc = '\0';
		}
		formatString = iter->getFormatStringFn((const char*)modName, traceId);
		modName[modNameLength] = tempSwapChar;
		if (tempSwapCharLoc != NULL) {
			*tempSwapCharLoc = tempSwapChar2;
		}
	}

	tempLower = (uint64_t)timeStampLeastSignificantBytes;
	tempUpper = (uint64_t)*timeStampMostSignificantBytes;
	timeStamp = tempUpper | tempLower;

	/* this formula is taken directly from the trace formatter to maintain agreement between representations
	 *	made by this function and those made by the TraceFormat tool. */
	timeStamp -= iter->startPlatform;

	splitTime = timeStamp / iter->timeConversion;
	splitTimeRem = timeStamp % iter->timeConversion;

	getTimestamp(splitTime + iter->startSystem, &hours, &minutes, &seconds, &millis);

	nanos = (uint32_t)((millis * ONEMILLION) + (splitTimeRem * ONEMILLION / iter->timeConversion));

	offsetOfParameters = (uint32_t) j9str_printf(PORTLIB, NULL, 0, "%02u:%02u:%02u:%09u GMT %.*s.%u - ", hours, minutes,
			seconds, nanos, modNameLength, modName, traceId);

	if (offsetOfParameters > bufferLength) {
		UT_DBGOUT_CHECKED(1,
				("<UT> parseTracePoint called with buffer length %d - too small for tracepoint details\n", bufferLength));
		return NULL;
	}

	offsetOfParameters = (uint32_t) j9str_printf(PORTLIB, buffer, bufferLength, "%02u:%02u:%02u:%09u GMT %.*s.%u - ", hours,
			minutes, seconds, nanos, modNameLength, modName, traceId);

	rawParameterData = tempPtr + offset + TRACEPOINT_RAW_DATA_MODULE_NAME_DATA_OFFSET + modNameLength;
	rawParameterDataLength = tpLength - (TRACEPOINT_RAW_DATA_MODULE_NAME_DATA_OFFSET + modNameLength);

	/* the parameters will be formatted as question marks if there is no data to populate them with */
	if (!formatTracePointParameters(iter, buffer + offsetOfParameters, bufferLength - offsetOfParameters, formatString,
			rawParameterData, rawParameterDataLength)) {
		return NULL;
	}

	return buffer;
}

char *
omr_trc_formatNextTracePoint(UtTracePointIterator *iter, char *buffer, uint32_t bufferLength)
{
	UtTraceRecord *record;
	int32_t recordDataStart;
	int32_t recordDataLength;
	uint32_t tpLength = 0;
	uint32_t offset = 0;

	PORT_ACCESS_FROM_PORT(iter->portLib);

	if (iter == NULL) {
		UT_DBGOUT_CHECKED(1, ("<UT> omr_trc_formatNextTracePoint called with NULL iterator\n"));
		return NULL;
	}

	if (iter->buffer == NULL) {
		UT_DBGOUT_CHECKED(1, ("<UT> omr_trc_formatNextTracePoint called with unpopulated iterator buffer\n"));
		return NULL;
	}

	if (iter->currentPos <= iter->start) {
		/* currentPosition is at or before the start of the data - not possible to continue
		 *	or the normal exit condition - either way NULL is the right thing to return */
		return NULL;
	}

	record = &iter->buffer->record;
	recordDataStart = record->firstEntry;
	recordDataLength = iter->recordLength;
	offset = iter->currentPos;

	tpLength = getUnsignedByteFromBuffer(record, offset);

	if (iter->longTracePointLength != 0) {
		/* the previous tracepoint was a long tracepoint marker, so it 
		 * contained the upper word of a two byte length descriptor */
		tpLength |= (iter->longTracePointLength << 8);
		iter->longTracePointLength = 0;
	}

	/* long trace points have length stored in 2bytes, so check we've not overflowed that
	 * limit
	 */
	if (tpLength == 0 || tpLength > UT_MAX_EXTENDED_LENGTH) {
		/* prevent looping in case of bad data */
		return NULL;
	}

	/* if this is a circular buffer, that we have already moved to the end of, and 
	 * the current tracepoint length takes us back before where formatting originally
	 * started in this buffer, then we have found the partially overwritten tracepoint
	 * that means we have got to the end of this buffer. */
	if ((iter->isCircularBuffer) && iter->iteratorHasWrapped && ((offset - tpLength) < iter->end)) {
		return NULL;
	}


	/* Check whether we are about to process a wrapped tracepoint.
	 *
	 * 'offset' points to location of 'tpLength' for the current tracepoint.
	 * 'tpLength' is the length of the tracepoint, including the length byte.
	 * 'offset - tpLength' points to the previous tracepoint's 'tpLength', if it exists.
	 * 'offset - tpLength + 1' points to the start of the current tracepoint's data, which
	 * must be at least 'record->firstEntry' or 'iter->start' if the tracepoint fits in this buffer.
	 */
	if ((tpLength > offset) || ((offset - tpLength + 1) < iter->start)) {
		if (iter->isCircularBuffer) {
			uint32_t remainder = 0;
			char *returnString;
			char *tempChrPtr;
			/* the tracepoint wraps back into the end of the current buffer */
			iter->iteratorHasWrapped = TRUE;

			remainder = (uint32_t)(tpLength - (offset - iter->start));
			UT_DBGOUT_CHECKED(4,
					("<UT> getNextTracePointForIterator: remainder found at end of buffer: %d tplength = %d tracedata start: %llu end %llu\n", remainder, tpLength, iter->start, iter->end));

			/* set the iterator's position to be before the incomplete data at the end of the buffer */
			iter->currentPos =  iter->recordLength - remainder;

			/* check if the wrapped tracepoint takes us back past the tracepoint data end point (i.e. record.nextEntry) */
			if (iter->currentPos < iter->end) {
				return NULL; /* we are done, discard this partial tracepoint */
			}

			iter->tempBuffForWrappedTP = (char *)j9mem_allocate_memory(tpLength + 2, OMRMEM_CATEGORY_TRACE);
			if (iter->tempBuffForWrappedTP == NULL) {
				UT_DBGOUT_CHECKED(1, ("<UT> omr_trc_formatNextTracePoint: cannot allocate %d bytes\n", tpLength + 2));
				return NULL;
			}

			/* copy the two ends of tracepoints into the temporary buffer
			 * the beginning of the data is at the end of the buffer
			 * end of data is at the start of the buffer */
			tempChrPtr = (char *)record;
			/* get the start of the tracepoint from the end of the buffer */
			memcpy(iter->tempBuffForWrappedTP, tempChrPtr + recordDataLength - remainder, remainder);
			/* get the end of the tracepoint from the start of the buffer */
			memcpy(iter->tempBuffForWrappedTP + remainder, tempChrPtr + recordDataStart, tpLength - remainder);

			/* and parse the temp buffer */
			returnString = parseTracePoint(iter->portLib, (UtTraceRecord *)iter->tempBuffForWrappedTP, 0, tpLength,
					&iter->currentUpperTimeWord, iter, buffer, bufferLength);

			UT_DBGOUT_CHECKED(1,
					("<UT> getNextTracePointForIterator: recombined a tracepoint into %s\n", returnString ? returnString : "NULL"));
			j9mem_free_memory(iter->tempBuffForWrappedTP);
			iter->tempBuffForWrappedTP = NULL;
			iter->processingIncompleteDueToPartialTracePoint = FALSE;
			return returnString;
		} else {
			/* the tracepoint is in another buffer and some magic is needed elsewhere to process */
			iter->processingIncompleteDueToPartialTracePoint = TRUE;
			return NULL;
		}
	}

	/* parse the relevant section into a tracepoint */
	iter->currentPos -= tpLength;
	return parseTracePoint(iter->portLib, record, (offset - tpLength), tpLength, &iter->currentUpperTimeWord, iter,
			buffer, bufferLength);
}

