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

#include "j9comp.h"
#include "j9.h"

#include "testHelpers.h"
#include "cfr.h"
#include "rommeth.h"

#include <stdlib.h> /* used for rand() */

/**
 * It will verify the compression and decompression of the line number table
 * It will try every possibilities of PC between rangePCLow and rangePCHigh
 * and of lineNumber between rangeLineLow and rangeLineHigh
 *	@param portLib					Pointer to the port library.
 *	@param id						Pointer to the test name.
 *	@param rangePCLow
 *	@param rangePCHigh
 *	@param rangeLineLow
 *	@param rangeLineHigh
 *	@return IDATA
 *
 */
static IDATA
testLineNumberCompressionEdge(J9PortLibrary *portLib, const char * testName, U_16 rangePCLow, U_16 rangePCHigh, I_32 rangeLineLow, I_32 rangeLineHigh, I_32 maximumSize)
{
	PORT_ACCESS_FROM_PORT(portLib);
	U_8 * buffer = (U_8*)j9mem_allocate_memory(0xFFFF * 5, OMRMEM_CATEGORY_VM); /* maximum memory possibly needed */
	U_8 * bufferInitial = buffer;
	U_8 * bufferPtr = NULL;
	U_16 startPCIndex;
	I_32 lineNumberIndex;
	J9LineNumber lineNumber;

	reportTestEntry(PORTLIB, testName);

	if (NULL == buffer) {
		outputErrorMessage(TEST_ERROR_ARGS, "out of memory error \n" );
		return reportTestExit(PORTLIB, testName);
	}

	for (startPCIndex = rangePCLow; startPCIndex < rangePCHigh; startPCIndex++) {
		bufferPtr = buffer;
		for (lineNumberIndex = rangeLineLow; lineNumberIndex < rangeLineHigh; lineNumberIndex++) {
			J9CfrLineNumberTableEntry lineNumberTableEntry;
			lineNumberTableEntry.startPC = startPCIndex;
			lineNumberTableEntry.lineNumber = lineNumberIndex;

			if (!compressLineNumbers(&lineNumberTableEntry, 1, NULL, &bufferPtr)) {
				outputErrorMessage(TEST_ERROR_ARGS, "error, the line numbers are not in order\n" );
			}
		}

		bufferPtr = buffer;
		for (lineNumberIndex = rangeLineLow; lineNumberIndex < rangeLineHigh; lineNumberIndex++) {
			I_32 lineNumberOffsetIntern;
			if (lineNumberIndex < 0) {
				/* For the test to work, set the previous line number to 0xFFFF so that the result is positive with a negative offset */
				lineNumberOffsetIntern = 0xFFFF + lineNumberIndex;
				lineNumber.lineNumber = 0xFFFF;
			} else {
				lineNumberOffsetIntern = lineNumberIndex;
				lineNumber.lineNumber = 0;
			}

			lineNumber.location = 0;
			if (!getNextLineNumberFromTable(&bufferPtr, &lineNumber)) {
				outputErrorMessage(TEST_ERROR_ARGS, "error in getNextLineNumberFromTable, byte format not recognized\n" );
			}
			if ((lineNumber.lineNumber != lineNumberOffsetIntern) || (lineNumber.location != startPCIndex)) {
				outputErrorMessage(TEST_ERROR_ARGS, "Original   lineNumber:%d, location:%d \n",lineNumberOffsetIntern, startPCIndex);
				outputErrorMessage(TEST_ERROR_ARGS, "Compressed lineNumber:%d, location:%d \n",lineNumber.lineNumber, lineNumber.location);
			}
		}
	}

	outputComment(PORTLIB, "Total size of: %d\n", bufferPtr - buffer);
	if ((bufferPtr - buffer) > maximumSize) {
		outputErrorMessage(TEST_ERROR_ARGS, "Compression is less efficient, it used to take %d bytes and it takes %d bytes \n", maximumSize, bufferPtr - buffer);
	}

	j9mem_free_memory(buffer);
	return reportTestExit(PORTLIB, testName);
}


/**
 * This tests test the delta encoding component of the compression of the line number table
 *	@param portLib					Pointer to the port library.
 *	@param id						Pointer to the test name.
 *	@return IDATA
 *
 */
static IDATA
testLineNumberCompressionDeltaEncoding(J9PortLibrary *portLib, const char * testName, I_32 from, I_32 to, I_32 increment)
{
	PORT_ACCESS_FROM_PORT(portLib);
	U_8 * buffer = (U_8*)j9mem_allocate_memory(0xFFFF * 5, OMRMEM_CATEGORY_VM); /* maximum memory possibly needed */
	U_8 * bufferPtr;
	I_32 lineNumberOffset, pcOffset, lastPCOffset = 0;
	J9LineNumber lineNumber;
	J9CfrLineNumberTableEntry lastLineNumberTableEntry;
	I_32 incrementPositive = increment > 0 ? increment : -increment;
	lastLineNumberTableEntry.startPC = 0;
	lastLineNumberTableEntry.lineNumber = 0;

	reportTestEntry(PORTLIB, testName);

	if (NULL == buffer) {
		outputErrorMessage(TEST_ERROR_ARGS, "out of memory error \n" );
		return reportTestExit(PORTLIB, testName);
	}

	bufferPtr = buffer;
	pcOffset = 0;
	/* The pcOffset should always increment, it's delta cannot be negative */
	for (lineNumberOffset = from; lineNumberOffset != to; lineNumberOffset += increment, pcOffset += incrementPositive) {
		J9CfrLineNumberTableEntry lineNumberTableEntry;
		lineNumberTableEntry.startPC = pcOffset;
		lineNumberTableEntry.lineNumber = lineNumberOffset;

		if (!compressLineNumbers(&lineNumberTableEntry, 1, &lastLineNumberTableEntry, &bufferPtr)) {
			outputErrorMessage(TEST_ERROR_ARGS, "error, the line numbers are not in order\n" );
		}
		lastLineNumberTableEntry = lineNumberTableEntry;
	}

	lineNumber.lineNumber = 0;
	lineNumber.location = 0;
	bufferPtr = buffer;
	pcOffset = 0;
	for (lineNumberOffset = from; lineNumberOffset != to; lineNumberOffset += increment, pcOffset += incrementPositive) {
		if (!getNextLineNumberFromTable(&bufferPtr, &lineNumber)) {
			outputErrorMessage(TEST_ERROR_ARGS, "error in getNextLineNumberFromTable, byte format not recognized\n" );
		}
		if ((lineNumber.lineNumber != lineNumberOffset) || (lineNumber.location != pcOffset)) {
			outputErrorMessage(TEST_ERROR_ARGS, "Original   lineNumber:%d, location:%d \n",lineNumberOffset, pcOffset);
			outputErrorMessage(TEST_ERROR_ARGS, "Compressed lineNumber:%d, location:%d \n",lineNumber.lineNumber, lineNumber.location);
		}
	}
	j9mem_free_memory(buffer);
	return reportTestExit(PORTLIB, testName);
}

static IDATA
testLineNumberCompressionNegativeOffsetError(J9PortLibrary *portLib, const char * testName)
{
	PORT_ACCESS_FROM_PORT(portLib);
	U_8 * buffer = (U_8*)j9mem_allocate_memory(5, OMRMEM_CATEGORY_VM);
	U_8 * bufferPtr = buffer;
	J9CfrLineNumberTableEntry line1, line2;
	line1.startPC = 4;
	line1.lineNumber = 4;

	line2.startPC = 3;
	line2.lineNumber = 4;

	if (NULL == buffer) {
		outputErrorMessage(TEST_ERROR_ARGS, "out of memory error \n" );
		return reportTestExit(PORTLIB, testName);
	}

	reportTestEntry(PORTLIB, testName);

	if (compressLineNumbers(&line2, 1, &line1, &bufferPtr)) {
		outputErrorMessage(TEST_ERROR_ARGS, "Error, compressLineNumbers should give an error when the startPC are not increasing. \n" );
	}
	j9mem_free_memory(buffer);
	return reportTestExit(PORTLIB, testName);
}

static IDATA
testLineNumberRandomCompressionDecompression(J9PortLibrary *portLib, const char * testName)
{
	PORT_ACCESS_FROM_PORT(portLib);
	J9CfrLineNumberTableEntry * bufferOriginal = (J9CfrLineNumberTableEntry*)j9mem_allocate_memory(0xFFFF * sizeof(J9CfrLineNumberTableEntry), OMRMEM_CATEGORY_VM);
	J9CfrLineNumberTableEntry * bufferOriginalPtr;

	U_8 * bufferCompressed = (U_8*)j9mem_allocate_memory(0xFFFF * 5, OMRMEM_CATEGORY_VM);
	U_8 * bufferCompressedPtr;

	U_16 countLineNumber;
	UDATA i, pass = 0;
	J9LineNumber lineNumber;
	J9CfrLineNumberTableEntry lineNumberTableEntry;

	reportTestEntry(PORTLIB, testName);


	if (NULL == bufferOriginal) {
		outputErrorMessage(TEST_ERROR_ARGS, "out of memory error \n" );
		return reportTestExit(PORTLIB, testName);
	}
	if (NULL == bufferCompressed) {
		outputErrorMessage(TEST_ERROR_ARGS, "out of memory error \n" );
		return reportTestExit(PORTLIB, testName);
	}

	for (pass = 0; pass < 1000000; pass++) {
		countLineNumber = 0;
		bufferCompressedPtr = bufferCompressed;
		bufferOriginalPtr = bufferOriginal;

		lineNumberTableEntry.startPC = 0;
		lineNumberTableEntry.lineNumber = 0;

		lineNumber.lineNumber = 0;
		lineNumber.location = 0;

		while ((lineNumberTableEntry.startPC < 0xFFFF) && (lineNumberTableEntry.lineNumber < 0xFFFF)) {
			*bufferOriginalPtr = lineNumberTableEntry;
			bufferOriginalPtr++;
			countLineNumber++;

			lineNumberTableEntry.startPC += rand() % 0xFFFF;
			lineNumberTableEntry.lineNumber += rand() % 0xFFFF;
		}

		if (!compressLineNumbers(bufferOriginal, countLineNumber, NULL, &bufferCompressedPtr)) {
			outputErrorMessage(TEST_ERROR_ARGS, "error, the line numbers are not in order\n" );
		}

		bufferCompressedPtr = bufferCompressed;
		bufferOriginalPtr = bufferOriginal;
		for (i = 0; i < countLineNumber; i++) {
			if (!getNextLineNumberFromTable(&bufferCompressedPtr, &lineNumber)) {
				outputErrorMessage(TEST_ERROR_ARGS, "error in getNextLineNumberFromTable, byte format not recognized\n" );
			}

			if ((bufferOriginalPtr->startPC != lineNumber.location) || (bufferOriginalPtr->lineNumber != lineNumber.lineNumber)) {
				outputErrorMessage(TEST_ERROR_ARGS, "The compressed value is different to the original value. \n\
	bufferOriginalPtr->startPC(%d) != lineNumber.location(%d) || bufferOriginalPtr->lineNumber(%d) != lineNumber.lineNumber(%d)",
						bufferOriginalPtr->startPC, lineNumber.location, bufferOriginalPtr->lineNumber, lineNumber.lineNumber );
			}

			bufferOriginalPtr++;
		}
	}

	j9mem_free_memory(bufferOriginal);
	j9mem_free_memory(bufferCompressed);
	return reportTestExit(PORTLIB, testName);
}

IDATA
j9dyn_lineNumber_tests(J9PortLibrary *portLib, int randomSeed)
{
	PORT_ACCESS_FROM_PORT(portLib);
	IDATA rc = 0;

	HEADING(PORTLIB, "j9dyn_lineNumber_tests");

	if (0 == randomSeed) {
		randomSeed = (int) j9time_current_time_millis();
	}

	outputComment(portLib, "NOTE: Run 'dyntest -srand:%d' to reproduce this test manually.\n\n", randomSeed);
	srand(randomSeed);

	rc |= testLineNumberCompressionEdge(portLib, "testLineNumberCompressionEdge: 0     , 0x1F  , 0     , 3     , 3    ", 0     , 0x1F  , 0     , 3     , 3);
	rc |= testLineNumberCompressionEdge(portLib, "testLineNumberCompressionEdge: 0     , 0x1F  , -256  , 0xFF  , 1786 ", 0     , 0x1F  , -256  , 0xFF  , 1786);
	rc |= testLineNumberCompressionEdge(portLib, "testLineNumberCompressionEdge: 0     , 0x7F  , -8192 , 0x1FFF, 65533", 0     , 0x7F  , -8192 , 0x1FFF, 65533);
	rc |= testLineNumberCompressionEdge(portLib, "testLineNumberCompressionEdge: 0x80  , 0x100 , 0x2000, 0x4000, 40960", 0x80  , 0x100 , 0x2000, 0x4000, 40960);
	rc |= testLineNumberCompressionEdge(portLib, "testLineNumberCompressionEdge: 0x80  , 0x100 , -10192, -8193 , 9995 ", 0x80  , 0x100 , -10192, -8193 , 9995);
	rc |= testLineNumberCompressionEdge(portLib, "testLineNumberCompressionEdge: 0xBFFF, 0xFFFF, 0xBFFF, 0xFFFF, 81920", 0xBFFF, 0xFFFF, 0xBFFF, 0xFFFF, 81920);
	rc |= testLineNumberCompressionEdge(portLib, "testLineNumberCompressionEdge: 0xBFFF, 0xFFFF, -65535, -49150, 81925", 0xBFFF, 0xFFFF, -65535, -49150, 81925);

	rc |= testLineNumberCompressionDeltaEncoding(portLib, "testLineNumberCompressionDeltaEncoding: 0,      0xFFFF, 1  ", 0,      0xFFFF, 1  );
	rc |= testLineNumberCompressionDeltaEncoding(portLib, "testLineNumberCompressionDeltaEncoding: 0xFFFF, 0,      -1 ", 0xFFFF, 0, -    1  );
	rc |= testLineNumberCompressionDeltaEncoding(portLib, "testLineNumberCompressionDeltaEncoding: 0,      0xFFFF, 15 ", 0,      0xFFFF, 15 );
	rc |= testLineNumberCompressionDeltaEncoding(portLib, "testLineNumberCompressionDeltaEncoding: 0xFFFF, 0,      -15", 0xFFFF, 0,      -15);

	rc |= testLineNumberCompressionNegativeOffsetError(portLib, "testLineNumberCompressionNegativeOffsetError");

	rc |= testLineNumberRandomCompressionDecompression(portLib, "testLineNumberRandomCompressionDecompression");

	return rc;
}
