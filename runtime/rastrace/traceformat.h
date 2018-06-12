/*******************************************************************************
 * Copyright (c) 2014, 2018 IBM Corp. and others
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

#ifndef TRACEFORMAT_H_
#define TRACEFORMAT_H_

/**
 * Trace formatting functions. Exposed as external symbols.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A callback to obtain a format string for a trace point with id tracepoint in
 * component componentName.
 *
 * Typically the format string will be obtained from a trace format .dat file though
 * this is not required. Care must be taken that the format file matches the original
 * trace point data as it will contain format specifiers which will be filled in when
 * the trace data for this trace point is formatted.
 *
 * If the string cannot be found a constant string with no inserts should be returned
 * rather than null.
 * (For example "UNKNOWN TRACEPOINT ID")
 *
 * @param[in] componentName the name of the component this trace point belongs to, for example j9mm
 * @param[in] tracepoint the trace point number within the component, for example 123
 */
typedef char *(*FormatStringCallback)(const char *componentName, int32_t tracepoint);
struct UtTraceFileIterator;
typedef struct  UtTraceFileIterator UtTraceFileIterator;
struct UtTracePointIterator;
typedef struct  UtTracePointIterator UtTracePointIterator;

/**
 * Obtain a UtTraceFileIterator for the file named in fileName.
 *
 * This iterator can then be used to obtain trace point iterators over each
 * buffer in the file via calls to omr_trc_getTracePointIteratorForNextBuffer
 *
 * @param[in] portLib An initialized OMRPortLibrary structure.
 * @param[in] fileName The name of the trace file to open.
 * @param[in,out] iteratorPtr A pointer to a location where the initialized UtTraceFileIterator pointer can be stored.
 * @param[in] getFormatString A callback the formatter can use to obtain a format string for a trace point id in a named module.
 *
 * @return OMR_ERROR_NONE on success
 * @return OMR_ERROR_NOT_AVAILABLE if the specified file cannot be opened
 * @return OMR_ERROR_ILLEGAL_ARGUMENT if the specified file does not contain valid trace data.
 * @return OMR_ERROR_OUT_OF_NATIVE_MEMORY if memory for the iterator structure cannot be allocated.
 * @return OMR_ERROR_NOT_AVAILABLE if the specified file cannot be opened.
 */
omr_error_t omr_trc_getTraceFileIterator(OMRPortLibrary *portLib, char *fileName, UtTraceFileIterator **iteratorPtr, FormatStringCallback getFormatString);

/**
 * Format the next trace point available from iter into buffer.
 * Returns a pointer to buffer on success or NULL if no more trace
 * can be formatted from this buffer.
 *
 * @param[in] iter the UtTracePointIterator to obtain the next trace point from.
 * @param[in,out] buffer the buffer to format the trace point into
 * @param[in] buffLen the length of the buffer
 * @return a pointer to buffer or NULL if there are no more trace points available
 */
char *omr_trc_formatNextTracePoint(UtTracePointIterator *iter, char *buffer, uint32_t buffLen);

/**
 * Free a trace point iterator and any memory associated with it
 * All UtTracePointIterators obtained from a file iterator should
 * be free'd before the UtTraceFileIterator is free'd.
 *
 * @param[in] iter the UtTracePointIterator to free
 * @return OMR_ERROR_NONE on success
 */
omr_error_t omr_trc_freeTracePointIterator(UtTracePointIterator *iter);

#ifdef __cplusplus
}
#endif

#endif /* TRACEFORMAT_H_ */
