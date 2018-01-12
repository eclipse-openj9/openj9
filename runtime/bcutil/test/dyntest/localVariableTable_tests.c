/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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

#define variableInfoArraySize  11


/**
 * It will verify the compression and decompression of the line number table
 * It will try every possibilities of
 * -index
 * -startPC
 * -length
 *	@param portLib					Pointer to the port library.
 *	@param testName					Pointer to the test name.
 *	@param indexLow
 *	@param indexHigh
 *	@param startPCLow
 *	@param startPCHigh
 *	@param lengthLow
 *	@param lengthHigh
 *	@return IDATA
 *
 */
static IDATA
testLocalVariableTableCompressionEdge(J9PortLibrary *portLib, const char * testName,
		I_32 indexLow, I_32 indexHigh,
		I_32 startPCLow, I_32 startPCHigh,
		I_32 lengthLow, I_32 lengthHigh)
{
	PORT_ACCESS_FROM_PORT(portLib);
	U_8 buffer[sizeof(J9MethodDebugInfo) + 13];
	I_32 indexIndex, startPCIndex, lengthIndex;

	reportTestEntry(PORTLIB, testName);

	for (indexIndex = indexLow; indexIndex <= indexHigh; indexIndex++) {
		for (startPCIndex = startPCLow; startPCIndex <= startPCHigh; startPCIndex++) {
			for (lengthIndex = lengthLow; lengthIndex <= lengthHigh; lengthIndex++) {
				J9VariableInfoWalkState state;
				J9VariableInfoValues *values = NULL;
				J9MethodDebugInfo *methodDebugInfo = (J9MethodDebugInfo*)&buffer;
				U_8 * variableTable;
				methodDebugInfo->lineNumberCount = 0;
				methodDebugInfo->varInfoCount = 1;
				methodDebugInfo->srpToVarInfo = 1;

				variableTable = getVariableTableForMethodDebugInfo(methodDebugInfo);

				if ((UDATA)variableTable != (UDATA)(methodDebugInfo + 1)) {
					outputErrorMessage(TEST_ERROR_ARGS, "Error in getVariableTableForMethodDebugInfo \n");
				}

				compressLocalVariableTableEntry(indexIndex, startPCIndex, lengthIndex, variableTable);

				values = variableInfoStartDo(methodDebugInfo, &state);

				if (values->slotNumber != indexIndex) {
					outputErrorMessage(TEST_ERROR_ARGS, "Original   slotNumber:%d, indexIndex:%d \n",values->slotNumber, indexIndex);
				}
				if (values->startVisibility != startPCIndex) {
					outputErrorMessage(TEST_ERROR_ARGS, "Original   startVisibility:%d, startPCIndex:%d \n",values->startVisibility, startPCIndex);
				}
				/* Cast to a U_16, because we want to get rid of the flag J9_ROMCLASS_OPTINFO_VARIABLE_TABLE_HAS_GENERIC */
				if ((U_16)values->visibilityLength != (U_16)lengthIndex) {
					outputErrorMessage(TEST_ERROR_ARGS, "Original   visibilityLength:%d, lengthIndex:%d \n",(U_16)values->visibilityLength, (U_16)lengthIndex);
				}
			}
		}
	}
	return reportTestExit(PORTLIB, testName);
}


/**
 * This tests test the delta encoding component of the compression of the line number table
 * Run compression on values between "from" and "to" by steps of "increment"
 *	@param portLib					Pointer to the port library.
 *	@param testName					Pointer to the test name.
 *	@param from
 *	@param to
 *	@param increment
 *	@return IDATA
 *
 */
static IDATA
testLocalVariableTableCompressionDeltaEncoding(J9PortLibrary *portLib, const char * testName, I_32 from, I_32 to, I_32 increment)
{
	PORT_ACCESS_FROM_PORT(portLib);
	/* maximum memory possibly needed, the number 13 comes from the fact that compressLocalVariableTableEntry can write from 1 to 13 bytes */
	UDATA bufferSize = sizeof(J9MethodDebugInfo) + 0xFFFF * (13 + 2 * sizeof(J9SRP));
	U_8 * buffer = (U_8*)j9mem_allocate_memory(bufferSize, OMRMEM_CATEGORY_VM);
	U_8 * bufferEnd = buffer + bufferSize;
	I_32 indexIndex, startPCIndex, lengthIndex;
	I_32 lastindexIndex = 0, laststartPCIndex = 0, lastlengthIndex = 0, varInfoCount = 0;
	J9VariableInfoWalkState state;
	J9VariableInfoValues *values;
	J9MethodDebugInfo *methodDebugInfo;
	U_8 * variableTable;

	reportTestEntry(PORTLIB, testName);

	if (NULL == buffer) {
		outputErrorMessage(TEST_ERROR_ARGS, "out of memory error \n" );
		return reportTestExit(PORTLIB, testName);
	}

	values = NULL;
	methodDebugInfo = (J9MethodDebugInfo*)buffer;
	methodDebugInfo->lineNumberCount = 0;
	methodDebugInfo->varInfoCount = 1; /* indicates an in-line table */
	methodDebugInfo->srpToVarInfo = 1;
	variableTable = getVariableTableForMethodDebugInfo(methodDebugInfo);
	if ((UDATA)variableTable != (UDATA)(methodDebugInfo + 1)) {
		outputErrorMessage(TEST_ERROR_ARGS, "Error in getVariableTableForMethodDebugInfo \n");
	}
	startPCIndex = from;
	lengthIndex = from;
	for (indexIndex = from; indexIndex != to; indexIndex += increment, startPCIndex += increment, lengthIndex += increment) {
		variableTable += compressLocalVariableTableEntry(
				indexIndex - lastindexIndex,
				startPCIndex - laststartPCIndex,
				lengthIndex - lastlengthIndex,
				variableTable);
		variableTable += 2 * sizeof(J9SRP);
		if (variableTable < buffer || variableTable > bufferEnd) {
			outputErrorMessage(TEST_ERROR_ARGS, "Buffer overrun \n");
		}
		lastindexIndex = indexIndex;
		laststartPCIndex = startPCIndex;
		lastlengthIndex = lengthIndex;
		varInfoCount++;
	}
	methodDebugInfo->varInfoCount = varInfoCount;

	variableTable = getVariableTableForMethodDebugInfo(methodDebugInfo);
	if ((UDATA)variableTable != (UDATA)(methodDebugInfo + 1)) {
		outputErrorMessage(TEST_ERROR_ARGS, "Error in getVariableTableForMethodDebugInfo \n");
	}
	startPCIndex = from;
	lengthIndex = from;
	values = variableInfoStartDo(methodDebugInfo, &state);
	for (indexIndex = from; indexIndex != to; indexIndex += increment, startPCIndex += increment, lengthIndex += increment) {

		if (values->slotNumber != indexIndex) {
			outputErrorMessage(TEST_ERROR_ARGS, "Original   slotNumber:%d, indexIndex:%d \n",values->slotNumber, indexIndex);
		}
		if (values->startVisibility != startPCIndex) {
			outputErrorMessage(TEST_ERROR_ARGS, "Original   startVisibility:%d, startPCIndex:%d \n",values->startVisibility, startPCIndex);
		}
		/* Cast to a U_16, because we want to get rid of the flag J9_ROMCLASS_OPTINFO_VARIABLE_TABLE_HAS_GENERIC */
		if ((U_16)values->visibilityLength != (U_16)lengthIndex) {
			outputErrorMessage(TEST_ERROR_ARGS, "Original   visibilityLength:%d, lengthIndex:%d \n",(U_16)values->visibilityLength, (U_16)lengthIndex);
		}
		values = variableInfoNextDo(&state);
		if (state.variableTablePtr < buffer || state.variableTablePtr > bufferEnd) {
			outputErrorMessage(TEST_ERROR_ARGS, "Buffer overrun \n");
		}
	}
	j9mem_free_memory(buffer);
	return reportTestExit(PORTLIB, testName);
}

void
printVariableTableValues(J9PortLibrary *portLib, const char * testName,  I_32 * indexIndexes, I_32 * startPCIndexes, I_32 * lengthIndexes)
{
	PORT_ACCESS_FROM_PORT(portLib);
	UDATA i;
	outputErrorMessage(TEST_ERROR_ARGS, "\nAll the VariableInfo values in the Variable Table\n");

	for (i = 1; i < variableInfoArraySize; i++) {
		outputErrorMessage(TEST_ERROR_ARGS, "VariableInfo %d \t slotNumber = %d \t startVisibility = %d \t visibilityLength = %d\n", i, indexIndexes[i], startPCIndexes[i], lengthIndexes[i]);
	}
}

/**
 *  Tries random generated variables tables multiple times and check if they uncompress correctly.
 *	@param portLib					Pointer to the port library.
 *	@param testName					Pointer to the test name.
 *	@param randomSeed				Seed to pass to srand()
 *	@return IDATA
 *
 */
static IDATA
testLocalVariableTableCompressionDecompression(J9PortLibrary *portLib, const char * testName, int randomSeed)
{
	PORT_ACCESS_FROM_PORT(portLib);
	/* maximum memory possibly needed, the number 13 comes from the fact that compressLocalVariableTableEntry can write from 1 to 13 bytes */
	UDATA bufferSize = sizeof(J9MethodDebugInfo) + 0xFFFF * (13 + 2 * sizeof(J9SRP));
	U_8 * buffer = (U_8*)j9mem_allocate_memory(bufferSize, OMRMEM_CATEGORY_VM);
	U_8 * bufferEnd = buffer + bufferSize;
	UDATA pass = 0;


	reportTestEntry(PORTLIB, testName);

	if (NULL == buffer) {
		outputErrorMessage(TEST_ERROR_ARGS, "out of memory error \n" );
		return reportTestExit(PORTLIB, testName);
	}

	for (pass = 0; pass < 1000000; pass++) {
		I_32 indexIndexes[(UDATA)variableInfoArraySize];
		I_32 startPCIndexes[variableInfoArraySize];
		I_32 lengthIndexes[variableInfoArraySize];

		I_32 varInfoCount = 0;
		J9VariableInfoWalkState state;
		J9VariableInfoValues *values;
		J9MethodDebugInfo *methodDebugInfo;
		U_8 * variableTable;

		I_32 i;

		randomSeed++; /* tries with a different seed each time */
		srand(randomSeed);

		indexIndexes[0] = 0;
		startPCIndexes[0] = 0;
		lengthIndexes[0] =	0;

		/* reset new values */
		for (i = 1; i < variableInfoArraySize; i++) {
			indexIndexes[i] = rand() % 0xFFFF;
			startPCIndexes[i] = rand() % 0xFFFF;
			lengthIndexes[i] =	rand() % 0xFFFF;
		}

		values = NULL;
		methodDebugInfo = (J9MethodDebugInfo*)buffer;
		methodDebugInfo->lineNumberCount = 0;
		methodDebugInfo->varInfoCount = 1;
		methodDebugInfo->srpToVarInfo = 1; /* indicates an in-line table */
		variableTable = getVariableTableForMethodDebugInfo(methodDebugInfo);

		if ((UDATA)variableTable != (UDATA)(methodDebugInfo + 1)) {
			outputErrorMessage(TEST_ERROR_ARGS, "Error in getVariableTableForMethodDebugInfo \n");
		}


		for (i = 1; i < variableInfoArraySize; i++) {

			variableTable += compressLocalVariableTableEntry(
					indexIndexes[i] - indexIndexes[i-1],
					startPCIndexes[i] - startPCIndexes[i-1],
					lengthIndexes[i] - lengthIndexes[i-1],
					variableTable);
			variableTable += 2 * sizeof(J9SRP);
			if (variableTable < buffer || variableTable > bufferEnd) {
				outputErrorMessage(TEST_ERROR_ARGS, "Buffer overrun \n");
			}
			varInfoCount++;
		}

		methodDebugInfo->varInfoCount = varInfoCount;

		variableTable = getVariableTableForMethodDebugInfo(methodDebugInfo);
		if ((UDATA)variableTable != (UDATA)(methodDebugInfo + 1)) {
			outputErrorMessage(TEST_ERROR_ARGS, "Error in getVariableTableForMethodDebugInfo \n");
		}
		values = variableInfoStartDo(methodDebugInfo, &state);


		for (i = 1; i < variableInfoArraySize; i++) {

			if (values->slotNumber != indexIndexes[i]) {
				outputErrorMessage(TEST_ERROR_ARGS, "Original   slotNumber:%d, indexIndex:%d , Failed variableInfo = %d, randomSeed = %d \n",values->slotNumber, indexIndexes[i], i, randomSeed);
				printVariableTableValues(portLib, testName,  indexIndexes, startPCIndexes, lengthIndexes);
			}
			if (values->startVisibility != startPCIndexes[i]) {
				outputErrorMessage(TEST_ERROR_ARGS, "Original   startVisibility:%d, startPCIndex:%d , Failed variableInfo = %d, randomSeed = %d  \n",values->startVisibility, startPCIndexes[i], i, randomSeed);
				printVariableTableValues(portLib, testName, indexIndexes, startPCIndexes, lengthIndexes);
			}
			/* Cast to a U_16, because we want to get rid of the flag J9_ROMCLASS_OPTINFO_VARIABLE_TABLE_HAS_GENERIC */
			if ((U_16)values->visibilityLength != lengthIndexes[i]) {
				outputErrorMessage(TEST_ERROR_ARGS, "Original   visibilityLength:%d, lengthIndex:%d , Failed variableInfo = %d, randomSeed = %d  \n",(U_16)values->visibilityLength, (U_16)lengthIndexes[i], i, randomSeed);
				printVariableTableValues(portLib, testName, indexIndexes, startPCIndexes, lengthIndexes);
			}

			values = variableInfoNextDo(&state);
			if (state.variableTablePtr < buffer || state.variableTablePtr > bufferEnd) {
				outputErrorMessage(TEST_ERROR_ARGS, "Buffer overrun \n");
			}
		}
	}
	j9mem_free_memory(buffer);
	return reportTestExit(PORTLIB, testName);
}


IDATA
j9dyn_localvariabletable_tests(J9PortLibrary *portLib, int randomSeed)
{
	PORT_ACCESS_FROM_PORT(portLib);
	IDATA rc = 0;

	HEADING(PORTLIB, "j9dyn_lineNumber_tests");

	if (0 == randomSeed) {
		randomSeed = (int) j9time_current_time_millis();
	}

	outputComment(portLib, "NOTE: Run 'dyntest -srand:%d' to reproduce this test manually.\n\n", randomSeed);

	rc |= testLocalVariableTableCompressionEdge(portLib, "testLocalVariableTableCompressionEdge 1bit", 0     , 1  , 0     , 0    ,-32,31);
	rc |= testLocalVariableTableCompressionEdge(portLib, "testLocalVariableTableCompressionEdge 2bits", 0     , 1  , -16     , 15    ,-128,127);
	rc |= testLocalVariableTableCompressionEdge(portLib, "testLocalVariableTableCompressionEdge 3bit", 0     , 1  , -256     , 255    ,-1024,1023);

	rc |= testLocalVariableTableCompressionEdge(portLib, "testLocalVariableTableCompressionEdge 5bit-1", 0     , 3  , -32768     , -30000    ,-131072,-130000);
	rc |= testLocalVariableTableCompressionEdge(portLib, "testLocalVariableTableCompressionEdge 5bit-2", 0     , 3  , 30000     , 32767    ,-131072,-130000);
	rc |= testLocalVariableTableCompressionEdge(portLib, "testLocalVariableTableCompressionEdge 5bit-3", 0     , 3  , -32768     , -30000    ,130000,131071);
	rc |= testLocalVariableTableCompressionEdge(portLib, "testLocalVariableTableCompressionEdge 5bit-4", 0     , 3  , 30000     , 32767    ,130000,131071);

	rc |= testLocalVariableTableCompressionEdge(portLib, "testLocalVariableTableCompressionEdge 13bit", 0xFFFFFEFF     , 0xFFFFFFFF  , 0xFFFFFEFF     , 0xFFFFFFFF,0xFEFF     , 0xFFFF);


	rc |= testLocalVariableTableCompressionDeltaEncoding(portLib, "testLocalVariableTableCompressionDeltaEncoding: 0,      0xFFFF, 1  ", 0,      0xFFFF, 1  );
	rc |= testLocalVariableTableCompressionDeltaEncoding(portLib, "testLocalVariableTableCompressionDeltaEncoding: 0xFFFF, 0,      -1 ", 0xFFFF, 0, -    1  );
	rc |= testLocalVariableTableCompressionDeltaEncoding(portLib, "testLocalVariableTableCompressionDeltaEncoding: 0,      0xFFFF, 15 ", 0,      0xFFFF, 15 );
	rc |= testLocalVariableTableCompressionDeltaEncoding(portLib, "testLocalVariableTableCompressionDeltaEncoding: 0xFFFF, 0,      -15", 0xFFFF, 0,      -15);

	rc |= testLocalVariableTableCompressionDecompression(portLib, "testLineNumberRandomCompressionDecompression", randomSeed);

	return rc;
}
