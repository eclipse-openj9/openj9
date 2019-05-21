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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "rastrace_internal.h"
#include "trctrigger.h"
#include "omrutil.h"
#include "j9trcnls.h"

typedef omr_error_t (*uteOptionFunction)(UtThreadData **thr,const char * value, BOOLEAN atRuntime);

/** 
 * Structure representing a UTE option such maximal or debug
 */
struct uteOption {
	const char * name;
	const int32_t runtimeModifiable;
	const uteOptionFunction optionFunction;
};

/* Prototypes for option configuration functions */
static omr_error_t processResetOption(UtThreadData **thr,const char * value, BOOLEAN atRuntime);
static omr_error_t processPropertiesOption(UtThreadData **thr,const char * value, BOOLEAN atRuntime);
static omr_error_t propertyFileOption(const char *value);
static omr_error_t setMinimal(UtThreadData **thr, const char *value, BOOLEAN atRuntime);
static omr_error_t setMaximal(UtThreadData **thr, const char *value, BOOLEAN atRuntime);
static omr_error_t setCount(UtThreadData **thr, const char *value, BOOLEAN atRuntime);
static omr_error_t setPrint(UtThreadData **thr, const char *value, BOOLEAN atRuntime);
static omr_error_t setNone(UtThreadData **thr, const char *value, BOOLEAN atRuntime);
static omr_error_t setIprint(UtThreadData **thr, const char *value, BOOLEAN atRuntime);
static omr_error_t setPlatform(UtThreadData **thr, const char *value, BOOLEAN atRuntime);
static omr_error_t setExternal(UtThreadData **thr, const char *value, BOOLEAN atRuntime);
static omr_error_t setException(UtThreadData **thr, const char *value, BOOLEAN atRuntime);
static omr_error_t setExceptOut(UtThreadData **thr, const char *value, BOOLEAN atRuntime);
static omr_error_t setStateOut(UtThreadData **thr, const char *value, BOOLEAN atRuntime);
static omr_error_t setOutput(UtThreadData **thr, const char *value, BOOLEAN atRuntime);
omr_error_t setTrigger(UtThreadData **thr, const char *value, BOOLEAN atRuntime);
static omr_error_t setBuffers(UtThreadData **thr, const char *value, BOOLEAN atRuntime);
static omr_error_t setSuspendResumeCount(UtThreadData **thr, const char *value, int32_t resume, BOOLEAN atRuntime);
static omr_error_t processSuspendOption(UtThreadData **thr,const char * value, BOOLEAN atRuntime);
static omr_error_t processResumeOption(UtThreadData **thr,const char * value, BOOLEAN atRuntime);
static omr_error_t processSuspendCountOption(UtThreadData **thr,const char * value, BOOLEAN atRuntime);
static int selectComponent(const char *cmd, int32_t *first, char traceType, int32_t setActive, BOOLEAN atRuntime);
static omr_error_t setFatalAssert(UtThreadData **thr, const char * spec, BOOLEAN atRuntime);
static omr_error_t clearFatalAssert(UtThreadData **thr, const char * spec, BOOLEAN atRuntime);

const struct uteOption UTE_OPTIONS[] = {
/*		{ Keyword, Can be configured at runtime, Configuration function} */
		
		/* These four don't have any action */
		{UT_DEBUG_KEYWORD, FALSE, NULL},
		{UT_SUFFIX_KEYWORD, FALSE, NULL},
		{UT_LIBPATH_KEYWORD, FALSE, NULL},
		{UT_FORMAT_KEYWORD, FALSE, NULL},
		
		{UT_RESET_KEYWORD, FALSE, processResetOption},
		{UT_PROPERTIES_KEYWORD, FALSE, processPropertiesOption},
		{UT_MINIMAL_KEYWORD, TRUE, setMinimal},
		{UT_MAXIMAL_KEYWORD, TRUE, setMaximal},
		{UT_EXCEPTION_KEYWORD, TRUE, setException},
		{UT_COUNT_KEYWORD, TRUE, setCount},
		{UT_PRINT_KEYWORD, TRUE, setPrint},
		{UT_NONE_KEYWORD, TRUE, setNone},
		{UT_IPRINT_KEYWORD, TRUE, setIprint},
		{UT_PLATFORM_KEYWORD, FALSE, setPlatform},
		{UT_EXTERNAL_KEYWORD, TRUE, setExternal},
		{UT_EXCEPT_OUT_KEYWORD, FALSE, setExceptOut},
		{UT_STATE_OUT_KEYWORD, FALSE, setStateOut},
		{UT_OUTPUT_KEYWORD, FALSE, setOutput},
		{UT_BUFFERS_KEYWORD, TRUE, setBuffers}, /* Not all buffers functions are exposed - but are controlled in the set function*/
		{UT_TRIGGER_KEYWORD, TRUE, setTrigger},
		{UT_SUSPEND_KEYWORD, TRUE, processSuspendOption},
		{UT_RESUME_KEYWORD, TRUE, processResumeOption},
		{UT_RESUME_COUNT_KEYWORD, TRUE, processResumeOption},
		{UT_SUSPEND_COUNT_KEYWORD, TRUE, processSuspendCountOption},
		{UT_FATAL_ASSERT_KEYWORD, TRUE, setFatalAssert},
		{UT_NO_FATAL_ASSERT_KEYWORD, TRUE, clearFatalAssert},
		{UT_SLEEPTIME_KEYWORD, TRUE, setSleepTime},
};

#define NUMBER_OF_UTE_OPTIONS (sizeof(UTE_OPTIONS) / sizeof(struct uteOption))

/*******************************************************************************
 * name        - addTraceCmd
 * description - Internal routine to setup a trace cmd
 * parameters  - thr, trace command and value
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
addTraceCmd(UtThreadData **thr, const char *cmd, const char *value, BOOLEAN atRuntime)
{
	omr_error_t rc = OMR_ERROR_NONE;
	char *str;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	str = (char *)j9mem_allocate_memory(strlen(cmd) + (value == NULL ? 1 :
												   strlen(value) + 2), OMRMEM_CATEGORY_TRACE);
	if (str != NULL) {
		strcpy(str, cmd);
		if (value != NULL && strlen(value) > 0) {
			strcat(str, "=");
			strcat(str, value);
		}

		/* Set the trace state under the global trace lock */
		getTraceLock(thr);
		rc = setTraceState(str, atRuntime);
		freeTraceLock(thr);

		j9mem_free_memory(str);
	} else {
		UT_DBGOUT(1, ("<UT> Out of memory in addTraceCmd\n"));
		rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}
	return rc;
}

static omr_error_t
parseBufferSize(const char * const str, const int argSize, BOOLEAN atRuntime)
{
	/* It's either invalid input or a number with an optional suffix
	 * Find the position of the first digit and non-digit character.
	 */
	int32_t newBufferSize;
	intptr_t firstNonDigit = -1;
	intptr_t firstDigit = -1;
	const char * p = str;
	
	while( *p ) {
		if (isdigit(*p)) {
			if (-1 == firstDigit) {
				firstDigit = p - str;
			}
		} else {
			if (-1 == firstNonDigit) {
				firstNonDigit = p - str;
			}
		}
		
		p++;
	}
	
	/* The only valid place for a non-digit is the final character */
	if (firstNonDigit != -1) {
		if (firstNonDigit == (argSize - 1) && firstDigit != -1) {
			int multiplier = 1;
			switch (j9_cmdla_toupper(str[argSize - 1])) {
			case 'K':
				multiplier = 1024;
				break;
			case 'M':
				multiplier = 1024 * 1024;
				break;
			default:
				reportCommandLineError(atRuntime, "Unrecognised suffix %c specified for buffer size",str[argSize - 1]);
				return OMR_ERROR_ILLEGAL_ARGUMENT;
			}
			
			newBufferSize = atoi(str) * multiplier;
		} else {
			/* Invalid */
			reportCommandLineError(atRuntime, "Invalid option for -Xtrace:buffers - \"%s\"",str);
			return OMR_ERROR_ILLEGAL_ARGUMENT;
		}
	} else {
		/* The string contains no non-digits */
		newBufferSize = atoi(str);
	}
	
	if (newBufferSize < UT_MINIMUM_BUFFERSIZE) {
		reportCommandLineError(atRuntime, "Specified buffer size %d bytes is too small. Minimum is %d bytes.", newBufferSize, UT_MINIMUM_BUFFERSIZE);
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	} else {
		UT_GLOBAL(bufferSize) = newBufferSize;
	}
	
	return OMR_ERROR_NONE;
}

/*******************************************************************************
 * name        - setBuffers
 * description - Set the buffer size and type
 * parameters  - thr, string value of the property (nnnk|nnnm[,dynamic]), atRuntime
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
setBuffers(UtThreadData **thr, const char *value, BOOLEAN atRuntime)
{
	char * localBuffer = NULL;
	omr_error_t rc = OMR_ERROR_NONE;
	const int numberOfArgs = getParmNumber(value);
	int i;
	
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	if (NULL == value) {
		reportCommandLineError(atRuntime, "-Xtrace:buffers expects an argument.");
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}
    localBuffer = (char *)j9mem_allocate_memory(strlen(value) + 1, OMRMEM_CATEGORY_TRACE);
    if (NULL == localBuffer) {
    	UT_DBGOUT(1, ("<UT> Out of memory in setBuffers\n"));
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}

	for (i=0;i<numberOfArgs;i++) {
		int argSize;
		const char * startOfThisArg = getPositionalParm(i+1,value,&argSize);
		
		if (argSize == 0) {
			reportCommandLineError(atRuntime, "Empty option passed to -Xtrace:buffers");
			rc = OMR_ERROR_ILLEGAL_ARGUMENT;
			goto end;
		}
		
		strncpy(localBuffer,startOfThisArg,argSize);
		localBuffer[argSize] = '\0';
		
		if (j9_cmdla_stricmp(localBuffer, "DYNAMIC") == 0) {
			UT_GLOBAL(dynamicBuffers) = TRUE;
		} else if (j9_cmdla_stricmp(localBuffer, "NODYNAMIC") == 0) {
			UT_GLOBAL(dynamicBuffers) = FALSE;
		} else {
			if (!atRuntime) {
				rc = parseBufferSize(localBuffer,argSize,atRuntime);
			
				if (rc != OMR_ERROR_NONE) {
					goto end;
				}
			} else {
				/* You can't change the buffer size at runtime */
				UT_DBGOUT(1, ("<UT> Buffer size cannot be changed at run-time\n"));
				rc = OMR_ERROR_ILLEGAL_ARGUMENT;
				goto end;
			}
		}
	}
	
	UT_DBGOUT(1, ("<UT> Trace buffer size: %d\n",UT_GLOBAL(bufferSize)));
	
end:
	if (localBuffer != NULL) {
		j9mem_free_memory(localBuffer);
	}
	
	return rc;
}

/*******************************************************************************
 * name        - setMinimal
 * description - Set the minimal trace options
 * parameters  - thr, trace options, atRuntime
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
setMinimal(UtThreadData **thr, const char *value, BOOLEAN atRuntime)
{
	return  addTraceCmd(thr, UT_MINIMAL_KEYWORD, value, atRuntime);
}

/*******************************************************************************
 * name        - setMaximal
 * description - Set the maximal trace options
 * parameters  - thr, trace options, atRuntime
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
setMaximal(UtThreadData **thr, const char *value, BOOLEAN atRuntime)
{
	return addTraceCmd(thr, UT_MAXIMAL_KEYWORD, value, atRuntime);
}

/*******************************************************************************
 * name        - setCount
 * description - Set the counter trace options
 * parameters  - thr, trace options, atRuntime
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
setCount(UtThreadData **thr, const char *value, BOOLEAN atRuntime)
{
	UT_GLOBAL(traceCount) = TRUE;
	return  addTraceCmd(thr, UT_COUNT_KEYWORD, value, atRuntime);
}

/*******************************************************************************
 * name        - setPrint
 * description - Set the printf trace options
 * parameters  - thr, trace options, atRuntime
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
setPrint(UtThreadData **thr, const char *value, BOOLEAN atRuntime)
{
	/* UT_GLOBAL(loadFormatFile) =  TRUE; */
	return  addTraceCmd(thr, UT_PRINT_KEYWORD, value, atRuntime);
}

/*******************************************************************************
 * name        - setNone
 * description - Set the "off" trace options
 * parameters  - thr, trace options, atRuntime
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
setNone(UtThreadData **thr, const char *value, BOOLEAN atRuntime)
{
	omr_error_t rc = OMR_ERROR_NONE;

	rc = addTraceCmd(thr, UT_NONE_KEYWORD, value, atRuntime);

	if (OMR_ERROR_NONE == rc && NULL == value) {
		/* We are clearing all trace (no sub parameters to none)
		 * so we can delete all trigger actions */
		clearAllTriggerActions();
	}

	return  rc;
}

/*******************************************************************************
 * name        - setIprint
 * description - Set the indented printf trace options
 * parameters  - thr, trace options, atRuntime
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
setIprint(UtThreadData **thr, const char *value, BOOLEAN atRuntime)
{
	/* UT_GLOBAL(loadFormatFile) =  TRUE; */
	return  addTraceCmd(thr, UT_IPRINT_KEYWORD, value, atRuntime);
}

/*******************************************************************************
 * name        - setPlatform
 * description - Set the platform trace options
 * parameters  - thr, trace options, atRuntime
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
setPlatform(UtThreadData **thr, const char *value, BOOLEAN atRuntime)
{
  if (!UT_GLOBAL(platformTraceStarted)) {
	  UT_GLOBAL(platformTraceStarted) = TRUE;
  }
  return  addTraceCmd(thr, UT_PLATFORM_KEYWORD, value, atRuntime);
}

/*******************************************************************************
 * name        - setExternal
 * description - Set the external trace options
 * parameters  - thr, trace options, atRuntime
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
setExternal(UtThreadData **thr, const char *value, BOOLEAN atRuntime)
{
	return  addTraceCmd(thr, UT_EXTERNAL_KEYWORD, value, atRuntime);
}

/*******************************************************************************
 * name        - setException
 * description - Set exception trace options
 * parameters  - thr, trace options, atRuntime
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
setException(UtThreadData **thr, const char *value, BOOLEAN atRuntime)
{
	return addTraceCmd(thr, UT_EXCEPTION_KEYWORD, value, atRuntime);
}

/*******************************************************************************
 * name        - setExceptOut
 * description - Set the exception trace output filename and options
 * parameters  - thr, string value of the property
 *               (filename[,nnnm])
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
setExceptOut(UtThreadData **thr, const char *value, BOOLEAN atRuntime)
{
	omr_error_t rc = OMR_ERROR_NONE;
	const char *p;
	int length;
	int multiplier;
	char str[20];
	char instring[256];

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	UT_GLOBAL(extExceptTrace) = TRUE;


	/*
	 * check for and expand %p or %p characters
	 */
	rc = expandString(instring, value, atRuntime);

	/*
	 * Process filename
	 */
	if (OMR_ERROR_NONE== rc) {
		p = getPositionalParm(1, instring, &length);
		if (p != NULL) {
			UT_GLOBAL(exceptFilename) = j9mem_allocate_memory(length + 1, OMRMEM_CATEGORY_TRACE );
			if (UT_GLOBAL(exceptFilename) != NULL) {
				memcpy(UT_GLOBAL(exceptFilename), p, length);
				UT_GLOBAL(exceptFilename[length]) = '\0';
				UT_DBGOUT(1, ("<UT> Exception filename: %s\n", UT_GLOBAL(exceptFilename)));
			} else {
				UT_DBGOUT(1, ("<UT> Out of memory handling exception property\n"));
				rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
			}
		} else {
			reportCommandLineError(atRuntime, "Filename not supplied in exception specification");
			rc = OMR_ERROR_ILLEGAL_ARGUMENT;
		}
	}

	/*
	 * Process file wrap size
	 */

	if (OMR_ERROR_NONE == rc) {
		p = getPositionalParm(2, instring, &length);
		if (p != NULL) {
			if ((length >= 2) && (length <= 5)) {
				if (j9_cmdla_toupper(p[length - 1]) == 'K') {
					multiplier = 1024;
				} else if (j9_cmdla_toupper(p[length - 1]) == 'M') {
					multiplier = 1024 * 1024;
				} else {
					reportCommandLineError(atRuntime, "Invalid multiplier for exception wrap limit");
					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
				}
				if (OMR_ERROR_NONE == rc) {
					memcpy(str, p, length - 1);
					str[length] = '\0';
					UT_GLOBAL(exceptTraceWrap) = atoi(str) * multiplier;
					UT_DBGOUT(1, ("<UT> Trace exception file wrap: %d\n",
							   UT_GLOBAL(exceptTraceWrap)));
				}
			} else {
				reportCommandLineError(atRuntime, "Length of wrap limit parameter invalid");
				rc = OMR_ERROR_ILLEGAL_ARGUMENT;
			}
		}
	}

	/*
	 *  That should be it
	 */

	if (getParmNumber(instring) > 2) {
		reportCommandLineError(atRuntime, "Too many keywords in exception specification");
		rc = OMR_ERROR_ILLEGAL_ARGUMENT;
	}
	if (OMR_ERROR_ILLEGAL_ARGUMENT == rc) {
		reportCommandLineError(atRuntime, "Usage: exception.output=filename[,nnnn{k|m}]");
	}
	return rc;
}

/*******************************************************************************
 * name        - setStateOut
 * description - Set the state trace output filename and options
 * parameters  - thr, string value of the property
 *               (filename[,nnnm])
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
setStateOut(UtThreadData **thr, const char *value, BOOLEAN atRuntime)
{
	omr_error_t rc = OMR_ERROR_NONE;
	const char *p;
	int length;

	char instring[256];

	/*
	 * check for and expand %p %d or %t characters
	 */
	rc = expandString(instring, value, atRuntime);

	/*
	 * Process filename
	 */
	if (OMR_ERROR_NONE == rc) {
		p = getPositionalParm(1, instring, &length);
		if (p != NULL) {
			/* No-op as state trace has been removed. (But we still need to parse the option.) */
		} else {
			reportCommandLineError(atRuntime, "Filename not supplied in state.output specification");
			rc = OMR_ERROR_ILLEGAL_ARGUMENT;
		}
	}

	/*
	 * Process file wrap size
	 */
	if (OMR_ERROR_NONE == rc) {
		p = getPositionalParm(2, instring, &length);
		if (p != NULL) {
			if ((length >= 2) && (length <= 5)) {
				if (j9_cmdla_toupper(p[length - 1]) == 'K') {
					/* No-op as state trace has been removed. (But we still need to parse the option.) */
				} else if (j9_cmdla_toupper(p[length - 1]) == 'M') {
					/* No-op as state trace has been removed. (But we still need to parse the option.) */
				} else {
					reportCommandLineError(atRuntime, "Invalid multiplier for exception wrap limit");
					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
				}
			} else {
				reportCommandLineError(atRuntime, "Length of wrap limit parameter invalid");
				rc = OMR_ERROR_ILLEGAL_ARGUMENT;
			}
		}
	}

	/*
	 *  That should be it
	 */
	if (getParmNumber(instring) > 2) {
		reportCommandLineError(atRuntime, "Too many keywords in state.output specification");
		rc = OMR_ERROR_ILLEGAL_ARGUMENT;
	}
	if (OMR_ERROR_ILLEGAL_ARGUMENT == rc) {
		reportCommandLineError(atRuntime, "Usage: state.output=filename[,nnnn{k|m}]");
	}
	return rc;
}

/*******************************************************************************
 * name        - setOutput
 * description - Set the output filename and options
 * parameters  - thr, string value of the property
 *               (filename[,nnnm][,generations]])
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
setOutput(UtThreadData **thr, const char *value, BOOLEAN atRuntime)
{
	omr_error_t rc = OMR_ERROR_NONE;
	const char *p;
	int length;
	int multiplier;
	char str[20];
	char instring[256];

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	UT_GLOBAL(externalTrace) = TRUE;


	/*
	 * check for and expand %p or %p characters
	 */
	rc = expandString(instring, value, atRuntime);

	/*
	 * Process filename
	 */
	if (OMR_ERROR_NONE == rc) {
		p = getPositionalParm(1, instring, &length);
		if (p != NULL) {
			UT_GLOBAL(traceFilename) = j9mem_allocate_memory(length + 1, OMRMEM_CATEGORY_TRACE );
			if (UT_GLOBAL(traceFilename) != NULL) {
				memcpy(UT_GLOBAL(traceFilename), p, length);
				UT_GLOBAL(traceFilename[length]) = '\0';
				UT_DBGOUT(1, ("<UT> Output filename: %s\n",
						   UT_GLOBAL(traceFilename)));

			} else {
				UT_DBGOUT(1, ("<UT> Out of memory handling output property\n"));
				rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
			}
		} else {
			reportCommandLineError(atRuntime, "Filename not supplied in output specification");
			rc = OMR_ERROR_ILLEGAL_ARGUMENT;
		}
	}

	/*
	 * Process file wrap size
	 */
	if (OMR_ERROR_NONE == rc) {
		p = getPositionalParm(2, instring, &length);
		if (p != NULL) {
			if ((length >= 2) && (length <= 5)) {
				if (j9_cmdla_toupper(p[length - 1]) == 'K') {
					multiplier = 1024;
				} else if (j9_cmdla_toupper(p[length - 1]) == 'M') {
					multiplier = 1024 * 1024;
				} else {
					reportCommandLineError(atRuntime, "Invalid multiplier for trace wrap limit");
					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
				}
				if (OMR_ERROR_NONE == rc) {
					memcpy(str, p, length - 1);
					str[length] = '\0';
					UT_GLOBAL(traceWrap) = atoi(str) * multiplier;
					UT_DBGOUT(1, ("<UT> Trace file wrap: %d\n", UT_GLOBAL(traceWrap)));
				}
			} else {
				reportCommandLineError(atRuntime, "Length of wrap limit parameter invalid");
				rc = OMR_ERROR_ILLEGAL_ARGUMENT;
			}
		}
	}

	/*
	 * Process generations parameter
	 */
	if (OMR_ERROR_NONE == rc) {
		p = getPositionalParm(3, instring, &length);
		if (p != NULL) {
			if ((length >= 1) && (length <= 2)) {
				memcpy(str, p, length);
				str[length] = '\0';
				UT_GLOBAL(traceGenerations) = atoi(str);
				UT_DBGOUT(1, ("<UT> Trace file generations: %d\n",
						   UT_GLOBAL(traceGenerations)));
				if ((UT_GLOBAL(traceGenerations) < 2) ||
					(UT_GLOBAL(traceGenerations) >36)) {
					reportCommandLineError(atRuntime, "Invalid number of trace generations");
					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
				} else {
					if ((UT_GLOBAL(generationChar) =
						strchr(UT_GLOBAL(traceFilename), '#')) == NULL) {
						reportCommandLineError(atRuntime, "Invalid filename for generation mode," 
								" the filename must contain a \"#\"");
						rc = OMR_ERROR_ILLEGAL_ARGUMENT;
					}
				}
			} else {
				reportCommandLineError(atRuntime, "Length of generation parameter invalid");
				rc = OMR_ERROR_ILLEGAL_ARGUMENT;
			}
		} else {
			UT_GLOBAL(traceGenerations) = 1;
		}
	}

	/*
	 *  That should be it
	 */
	if (getParmNumber(instring) > 3) {
		reportCommandLineError(atRuntime, "Too many keywords in output specification");
		rc = OMR_ERROR_ILLEGAL_ARGUMENT;
	}
	if (OMR_ERROR_ILLEGAL_ARGUMENT == rc) {
		reportCommandLineError(atRuntime, "Usage: output=filename[,nnnn{k|m}[,n]]\n");
	}
	return rc;
}

/*******************************************************************************
 * name        - setTrigger
 * description - Set trigger tracing
 * parameters  - thr, trigger value, runTime
 * returns     - UTE return code
 *
 ******************************************************************************/
omr_error_t
setTrigger(UtThreadData **thr, const char *triggerStr, BOOLEAN atRuntime)
{
	omr_error_t rc = OMR_ERROR_NONE;
	char   *clause = NULL;
	char   *str;
	const char *value = triggerStr;
	int32_t done = FALSE;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	UT_DBGOUT(1, ("<UT> Processing trigger statement: %s\n", value));
	/*
	 * Ignore empty value
	 */
	if(value == NULL || strlen(value) == 0) {
		return rc;
	}

	/*
	 * Keep calling getNextBracketedParm until done comes back TRUE
	 */
	for (;(OMR_ERROR_NONE == rc) && (!done);) {
		clause = getNextBracketedParm(value, &rc, &done, atRuntime);
		if(OMR_ERROR_NONE == rc){
			if(strlen(clause) == 0) {
				reportCommandLineError(atRuntime, "Empty clauses not allowed in trigger property.");
				rc = OMR_ERROR_INTERNAL;
			} else {
				char * workingClause;
				int32_t disabling = FALSE;
				value = value + strlen(clause) + 1;

				if (*clause == '!') {
					workingClause = clause + 1;
					disabling = TRUE;
				} else {
					workingClause = clause;
				}
				
				/*
				 * Tracepoint ID specified ?
				 */
				if (0 == j9_cmdla_strnicmp(workingClause, "tpnid{", strlen("tpnid{"))) {
					
					if (strchr(clause, ',') != NULL) {
						str = strchr(clause, ',');
						str[0] = '}';
						str[1] = '\0';
					}
					
					rc = addTraceCmd(thr, UT_TRIGGER_KEYWORD, clause, atRuntime);
				}
				/*
				 * Group specified ?  =>  map to "all{name}"
				 */
				if (0 == j9_cmdla_strnicmp(workingClause, "group{", strlen("group{"))) {
					
					if (strchr(clause, ',') != NULL) {
						str = strchr(clause, ',');
						str[0] = '}';
						str[1] = '\0';
					}
					
					str = clause + 2;
					if (disabling) {
						strncpy(str, "!all", 4);
					} else {
						strncpy(str, "all", 3);
					}
					
					rc = addTraceCmd(thr, UT_TRIGGER_KEYWORD, str, atRuntime);
				}
			}
		}
		if (clause != NULL) {
			j9mem_free_memory(clause);
		}
	}

	/* The code above has set the trace points that need to fire to UT_TRIGGER
	 * (like setting them UT_MAXIMAL or UT_PRINT).
	 * Now we need to process the trigger actions so we can look up what to
	 * do when one of those trace points fires.
	 */
	if( OMR_ERROR_NONE == rc ) {
		rc = setTriggerActions(thr, triggerStr, atRuntime);
	}

	return rc;
}


/*******************************************************************************
 * name        - setFormat
 * description - Set the formatting file path
 * parameters  - path
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
setFormat(const char *value)
{
	omr_error_t rc = OMR_ERROR_NONE;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	UT_GLOBAL(traceFormatSpec) = (char *)j9mem_allocate_memory(strlen(value) + 1, OMRMEM_CATEGORY_TRACE );
	if (UT_GLOBAL(traceFormatSpec) != NULL) {
		strcpy(UT_GLOBAL(traceFormatSpec), value);
	} else {
		UT_DBGOUT(1, ("<UT> Out of memory for FormatSpecPath\n"));
		rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}

	return rc;
}

/**************************************************************************
 * name        - setSuspendResumeCount
 * description - This handles the resumecount and suspendcount properties.
 *               It sets the value initialSuspendResume in UtGlobalData
 *               This is read by all starting threads and used as their
 *               initial value for thread suspend/resume.  If at any given
 *               time,  it is 0 or greater, trace is enabled for that
 *               thread.
 * parameters  - thr, trace options, Resume (TRUE=resume, FALSE=suspend)
 * returns     - UTE return code
 *************************************************************************/
omr_error_t
setSuspendResumeCount(UtThreadData **thr, const char *value, int32_t resume, BOOLEAN atRuntime)
{
	omr_error_t     rc = OMR_ERROR_NONE;
	const char    *p;
	int     length;
	int     maxLength=5;

	/*
	 * check for basic formatting errors
	 */
	p = getPositionalParm(1, value, &length);

	if (getParmNumber(value) != 1) {  /* problem if not just the one parm */
		rc = OMR_ERROR_INTERNAL;
	} else if (length == 0) {         /* first (only) parm must be non null */
		rc = OMR_ERROR_INTERNAL;
	}

	if (OMR_ERROR_NONE == rc) {               /* max 5 chars long            */
		if (*p == '+' || *p == '-') { /* but must allow 6 if signed  */
			maxLength++;
		}
		if (length > maxLength){
			rc=OMR_ERROR_INTERNAL;
		}
	}

	/*
	 * If any of the above checks failed, issue message.
	 */
	if (OMR_ERROR_NONE != rc) {
		if(resume) {
			reportCommandLineError(atRuntime, "resumecount takes a single integer value from -99999 to +99999");
		} else {
			reportCommandLineError(atRuntime, "suspendcount takes a single integer value from -99999 to +99999");
		}
	}

	if(OMR_ERROR_NONE == rc) {
		/*
		 * Enforce that users may not use resumecount and suspendcount together !
		 */
		if (UT_GLOBAL(initialSuspendResume) != 0) {
			reportCommandLineError(atRuntime, "resumecount and suspendcount may not both be set.");
			rc = OMR_ERROR_INTERNAL;
		} else {
			/* issues error and sets rc if p points to bad number */
			int value = decimalString2Int(p, TRUE, &rc, atRuntime);

			if (OMR_ERROR_NONE == rc) {
				if(resume){
					UT_GLOBAL(initialSuspendResume) = (0 - value);
				} else {
					UT_GLOBAL(initialSuspendResume) = (value - 1);
				}
			}
		}
	}

	/* the current thread (presumably the primordial thread) needs updating too */
	(*thr)->suspendResume = UT_GLOBAL(initialSuspendResume);

	return rc;
}

/*******************************************************************************
 * name        - processPropertyFile
 * description - Read trace options from the properties file
 * parameters  - thr, string value of the property (filename)
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
processPropertyFile(UtThreadData **thr, const char *filename, BOOLEAN atRuntime)
{
	intptr_t          propFile;
	int64_t         size;
	int            i;
	omr_error_t            rc = OMR_ERROR_NONE;
	char          *buffer = NULL;
	char          *p;
	char           name[MAX_IMAGE_PATH_LENGTH];
	char          *separator = DIR_SEPARATOR_STR;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	/*
	 *  Process properties file, if any
	 */

	if (filename == NULL || *filename == '\0') {

		*name = '\0';

		if (UT_GLOBAL(propertyFilePath) != NULL) {
			strcat(name, UT_GLOBAL(propertyFilePath));
			strcat(name, separator);
		}
		strcat(name, UT_DEFAULT_PROPERTIES);
	} else {
		strcpy(name, filename);
	}
	rc = OMR_ERROR_INTERNAL;
	/*
	 * Open the property file
	 */
	if ((propFile = j9file_open(name, EsOpenRead | EsOpenText, 0)) < 0) {
		reportCommandLineError(atRuntime, "Unable to open properties file %s", name);
	} else {
		/*
		 *  Get the file size
		 */
		size = j9file_flength(propFile);
		if ( 0 > size ) {
			reportCommandLineError(atRuntime, "Unable to determine size of properties file %s", name);
		} else {
			/*
			 * Obtain enough memory to hold the file
			 */
			buffer = (char *)j9mem_allocate_memory((uintptr_t)size + 1, OMRMEM_CATEGORY_TRACE );
			if (buffer == NULL) {
				UT_DBGOUT(1, ("<UT> Cannot obtain memory to process %s\n", name));
			} else {
				if ((size = j9file_read(propFile, buffer, (int)size)) == 0){
					reportCommandLineError(atRuntime, "Error reading properties file %s", name);
				} else {
					buffer[size] = '\0';
					twE2A(buffer);
					rc = OMR_ERROR_NONE;
				}
			}
		}
		j9file_close(propFile);
	}
	/*
	 *  Transform all CR/LF's to binary zeroes and tabs to blanks
	 */
	if (rc == OMR_ERROR_NONE) {
		*(buffer + size) = '\0';
		for (p = buffer; p < (buffer + size); p++) {
			if (*p == '\n' ||
				*p == '\r') {
				*p = '\0';
			}
			if (*p == '\t') {
				*p = ' ';
			}
		}
		/*
		 *  Parse the file a line at a time
		 */
		for (p = buffer;
			 (rc == OMR_ERROR_NONE) && (p < (buffer + size));
			 p += strlen(p) + 1) {
			if (*p == '\0') {
				continue;
			}
			UT_DBGOUT(1, ("<UT> Properties file line: %s\n", p));
			if (0 == j9_cmdla_strnicmp(p, UT_PROPERTIES_COMMENT, strlen(UT_PROPERTIES_COMMENT))) {
				continue;
			}
			if(0 == j9_cmdla_strnicmp(p, UT_DEBUG_KEYWORD, strlen(UT_DEBUG_KEYWORD))) {
				if (strlen(p) > strlen(UT_DEBUG_KEYWORD)) {
					p += strlen(UT_DEBUG_KEYWORD);
					if (strlen(p) == 2 &&
						p[0] == '=' &&
						p[1] >= '1' &&
						p[1] <= '9') {
						UT_GLOBAL(traceDebug) = atoi(p + 1);
					} else {
						rc = OMR_ERROR_ILLEGAL_ARGUMENT;
					}
				} else {
					UT_GLOBAL(traceDebug) = 9;
				}
				UT_DBGOUT(1, ("<UT> Debug information requested\n"));

			} else if ((0 == j9_cmdla_strnicmp(p, UT_MINIMAL_KEYWORD, strlen(UT_MINIMAL_KEYWORD))) &&
				 (*(p + strlen(UT_MINIMAL_KEYWORD)) == '=')) {
				p += strlen(UT_MINIMAL_KEYWORD) + 1;
				rc = setMinimal(thr, p, 0);
			} else if ((0 == j9_cmdla_strnicmp(p, UT_MAXIMAL_KEYWORD, strlen(UT_MAXIMAL_KEYWORD))) &&
					   (*(p + strlen(UT_MAXIMAL_KEYWORD)) == '=')) {
				p += strlen(UT_MAXIMAL_KEYWORD) + 1;
				rc = setMaximal(thr, p, 0);
			} else if ((0 == j9_cmdla_strnicmp(p, UT_COUNT_KEYWORD, strlen(UT_COUNT_KEYWORD))) &&
					   (*(p + strlen(UT_COUNT_KEYWORD)) == '=')) {
				p += strlen(UT_COUNT_KEYWORD) + 1;
				rc = setCount(thr, p, 0);
			} else if ((0 == j9_cmdla_strnicmp(p, UT_PRINT_KEYWORD, strlen(UT_PRINT_KEYWORD))) &&
					   (*(p + strlen(UT_PRINT_KEYWORD)) == '=')) {
				p += strlen(UT_PRINT_KEYWORD) + 1;
				rc = setPrint(thr, p, 0);
			} else if ((0 == j9_cmdla_strnicmp(p, UT_NONE_KEYWORD, strlen(UT_NONE_KEYWORD))) &&
					   (*(p + strlen(UT_NONE_KEYWORD)) == '=')) {
				p += strlen(UT_NONE_KEYWORD) + 1;
				rc = setNone(thr, p, 0);
			} else if ((0 == j9_cmdla_strnicmp(p, UT_IPRINT_KEYWORD, strlen(UT_IPRINT_KEYWORD))) &&
					   (*(p + strlen(UT_IPRINT_KEYWORD)) == '=')) {
				p += strlen(UT_IPRINT_KEYWORD) + 1;
				rc = setIprint(thr, p, 0);
			} else if ((0 == j9_cmdla_strnicmp(p, UT_PLATFORM_KEYWORD, strlen(UT_PLATFORM_KEYWORD))) &&
					   (*(p + strlen(UT_PLATFORM_KEYWORD)) == '=')) {
				p += strlen(UT_PLATFORM_KEYWORD) + 1;
				rc = setPlatform(thr, p, 0);
			} else if ((0 == j9_cmdla_strnicmp(p, UT_EXTERNAL_KEYWORD, strlen(UT_EXTERNAL_KEYWORD))) &&
					   (*(p + strlen(UT_EXTERNAL_KEYWORD)) == '=')) {
				p += strlen(UT_EXTERNAL_KEYWORD) + 1;
				rc = setExternal(thr, p, 0);
			} else if ((0 == j9_cmdla_strnicmp(p, UT_EXCEPTION_KEYWORD, strlen(UT_EXCEPTION_KEYWORD))) &&
					   (*(p + strlen(UT_EXCEPTION_KEYWORD)) == '=')) {
				p += strlen(UT_EXCEPTION_KEYWORD) + 1;
				rc = setException(thr, p, 0);
			} else if ((0 == j9_cmdla_strnicmp(p, UT_FORMAT_KEYWORD, strlen(UT_FORMAT_KEYWORD))) &&
					   (*(p + strlen(UT_FORMAT_KEYWORD)) == '=')) {
				char * tempSpec;
				p += strlen(UT_FORMAT_KEYWORD) + 1;
				tempSpec =  j9mem_allocate_memory(strlen(p) + 1, OMRMEM_CATEGORY_TRACE );
				if (tempSpec != NULL) {
					strcpy(tempSpec, p);
					UT_GLOBAL(traceFormatSpec) = tempSpec;
				} else {
					UT_DBGOUT(1, ("<UT> Memory allocation failure for FormatSpec\n"));
					rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
				}

			} else if ((0 == j9_cmdla_strnicmp(p, UT_EXCEPT_OUT_KEYWORD, strlen(UT_EXCEPT_OUT_KEYWORD))) &&
					   (*(p + strlen(UT_EXCEPT_OUT_KEYWORD)) == '=')) {
				p += strlen(UT_EXCEPT_OUT_KEYWORD) + 1;
				rc = setExceptOut(thr, p, 0);
			} else if ((0 == j9_cmdla_strnicmp(p, UT_STATE_OUT_KEYWORD, strlen(UT_STATE_OUT_KEYWORD))) &&
					   (*(p + strlen(UT_STATE_OUT_KEYWORD)) == '=')) {
				p += strlen(UT_STATE_OUT_KEYWORD) + 1;
				rc = setStateOut(thr, p, 0);
			} else if ((0 == j9_cmdla_strnicmp(p, UT_OUTPUT_KEYWORD, strlen(UT_OUTPUT_KEYWORD))) &&
					   (*(p + strlen(UT_OUTPUT_KEYWORD)) == '=')) {
				p += strlen(UT_OUTPUT_KEYWORD) + 1;
				rc = setOutput(thr, p, 0);
			} else if ((0 == j9_cmdla_strnicmp(p, UT_BUFFERS_KEYWORD, strlen(UT_BUFFERS_KEYWORD))) &&
					   (*(p + strlen(UT_BUFFERS_KEYWORD)) == '=')) {
				p += strlen(UT_BUFFERS_KEYWORD) + 1;
				rc = setBuffers(thr, p, 0);
			} else if (0 == j9_cmdla_stricmp(p, UT_INIT_KEYWORD)) {
				p += strlen(UT_INIT_KEYWORD);
			} else if (0 == j9_cmdla_stricmp(p, UT_RESET_KEYWORD)) {
				p += strlen(UT_RESET_KEYWORD);
			} else if ((0 == j9_cmdla_strnicmp(p, UT_TRIGGER_KEYWORD, strlen(UT_TRIGGER_KEYWORD))) &&
								(*(p + strlen(UT_TRIGGER_KEYWORD)) == '=')) {
				propertyFileOption(p);
				p += strlen(UT_TRIGGER_KEYWORD) + 1;
				rc = setTrigger(thr, p, 0);
			} else if (0 == j9_cmdla_stricmp(p, UT_SUSPEND_KEYWORD)) {
				p += strlen(UT_SUSPEND_KEYWORD);
				UT_GLOBAL(traceSuspend) = UT_SUSPEND_USER;
			} else if (0 == j9_cmdla_stricmp(p, UT_RESUME_KEYWORD)) {
				p += strlen(UT_RESUME_KEYWORD);
			} else if ((0 == j9_cmdla_strnicmp(p, UT_RESUME_COUNT_KEYWORD, strlen(UT_RESUME_COUNT_KEYWORD))) &&
								(*(p + strlen(UT_RESUME_COUNT_KEYWORD)) == '=')) {
				p += strlen(UT_RESUME_COUNT_KEYWORD) +1;
				rc = setSuspendResumeCount(thr, p, TRUE, atRuntime);
			} else if ((0 == j9_cmdla_strnicmp(p, UT_SUSPEND_COUNT_KEYWORD, strlen(UT_SUSPEND_COUNT_KEYWORD))) &&
								(*(p + strlen(UT_SUSPEND_COUNT_KEYWORD)) == '=')) {
				p += strlen(UT_SUSPEND_COUNT_KEYWORD) +1;
				rc = setSuspendResumeCount(thr, p, FALSE, atRuntime);
			} else if (0 == j9_cmdla_stricmp(p, UT_FATAL_ASSERT_KEYWORD)) {
				p += strlen(UT_FATAL_ASSERT_KEYWORD);
				UT_GLOBAL(fatalassert) = 1;
			} else if (0 == j9_cmdla_stricmp(p, UT_NO_FATAL_ASSERT_KEYWORD)) {
				p += strlen(UT_NO_FATAL_ASSERT_KEYWORD);
				UT_GLOBAL(fatalassert) = 0;
			} else if ((0 == j9_cmdla_strnicmp(p, UT_SLEEPTIME_KEYWORD, strlen(UT_SLEEPTIME_KEYWORD))) &&
								(*(p + strlen(UT_SLEEPTIME_KEYWORD)) == '=')) {
				p += strlen(UT_SLEEPTIME_KEYWORD) +1;
				rc = setSleepTime(thr, p, FALSE);
			} else {
				/*
				 *  Check for options to quietly ignore
				 */

				if (UT_GLOBAL(ignore) != NULL) {
					for(i = 0; UT_GLOBAL(ignore[i]) != NULL; i++) {
						if (0 == j9_cmdla_strnicmp(p, UT_GLOBAL(ignore[i]), strlen(UT_GLOBAL(ignore[i])))) {
							rc = propertyFileOption(p);
							break;
						}
					}
					if (UT_GLOBAL(ignore[i]) != NULL) {
						continue;
					}
				}
				reportCommandLineError(atRuntime, "Unrecognized line in %s: \"%s\"", name, p);
				rc = OMR_ERROR_ILLEGAL_ARGUMENT;
			}
		}
	}
	if (buffer != NULL) {
		j9mem_free_memory(buffer);
	}
	UT_DBGOUT(1, ("<UT> Properties file processing complete, RC=%d\n", rc));
	
	if (UT_GLOBAL(externalTrace) || UT_GLOBAL(extExceptTrace)) {
		UT_GLOBAL(traceInCore) = FALSE;
	}

	return rc;
}
/*******************************************************************************
 * name        - processEarlyOptions
 * description - Process the startup
 * parameters  - char **options
 * returns     - Nothing
 ******************************************************************************/
omr_error_t
processEarlyOptions(const char **opts)
{
	int i;
	omr_error_t rc = OMR_ERROR_NONE;

	/*
	 *  Process the options passed
	 */


	/*
	 *  Options are in name / value pairs, and terminate with a NULL
	 */

	for(i = 0; opts[i] != NULL; i += 2) {
		/* DEBUG is handled elsewhere, SUFFIX and LIBPATH are obsolete. */
		if(j9_cmdla_stricmp((char *)opts[i], UT_DEBUG_KEYWORD) == 0) {
		} else if (j9_cmdla_stricmp((char *)opts[i], UT_SUFFIX_KEYWORD) == 0) {
		} else if (j9_cmdla_stricmp((char *)opts[i], UT_LIBPATH_KEYWORD) == 0) {
		} else if (j9_cmdla_stricmp((char *)opts[i], UT_FORMAT_KEYWORD) == 0) {
			if ( opts[i+1] == NULL ){
				return OMR_ERROR_ILLEGAL_ARGUMENT;
			}
			rc = setFormat(opts[i + 1]);
		} else {
			UT_DBGOUT(1, ("<UT> EarlyOptions skipping :%s\n", opts[i]));
		}
	}
	return rc;
}

static omr_error_t
processSuspendCountOption(UtThreadData **thr,const char * value, BOOLEAN atRuntime)
{
	return setSuspendResumeCount(thr, value, FALSE, atRuntime);
}

static omr_error_t
processResumeOption(UtThreadData **thr,const char * value, BOOLEAN atRuntime)
{
	return setSuspendResumeCount(thr, value, TRUE, atRuntime);
}

static omr_error_t
processSuspendOption(UtThreadData **thr,const char * value, BOOLEAN atRuntime)
{
	UT_GLOBAL(traceSuspend) = UT_SUSPEND_USER;
	return OMR_ERROR_NONE;
}

static omr_error_t
processResetOption(UtThreadData **thr,const char * value, BOOLEAN atRuntime)
{
	return OMR_ERROR_NONE;
}

static omr_error_t
processPropertiesOption(UtThreadData **thr,const char * value, BOOLEAN atRuntime)
{
	return processPropertyFile(thr, value, atRuntime);
}

static omr_error_t
setFatalAssert(UtThreadData **thr, const char * spec, BOOLEAN atRuntime)
{
	UT_GLOBAL(fatalassert) = 1;
	return OMR_ERROR_NONE;
}

static omr_error_t
clearFatalAssert(UtThreadData **thr, const char * spec, BOOLEAN atRuntime)
{
	UT_GLOBAL(fatalassert) = 0;
	return OMR_ERROR_NONE;
}

/*******************************************************************************
 * name        - processOptions
 * description - Process the startup
 * parameters  - UtThreadData, char **options, atRuntime
 * returns     - Nothing
 ******************************************************************************/
omr_error_t
processOptions(UtThreadData **thr, const char **opts, BOOLEAN atRuntime)
{
	int i, j;
	omr_error_t rc = OMR_ERROR_NONE;
	const OMR_VM *vm = UT_GLOBAL(vm);
	SetLanguageTraceOptionFunc optionCallback = UT_GLOBAL(languageIntf).SetLanguageTraceOption;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	/*
	 *  Check options for the debug switch. Options are in name / value pairs
	 *  so only look at the first of each pair
	 */

	if (! atRuntime) {
		for(i = 0; opts[i] != NULL; i += 2) {
			if(j9_cmdla_stricmp((char *)opts[i], UT_DEBUG_KEYWORD) == 0) {
				if (opts[i + 1] != NULL &&
						strlen(opts[i + 1]) == 1 &&
						*opts[i + 1] >= '0' &&
						*opts[i + 1] <= '9') {
					UT_GLOBAL(traceDebug) = atoi(opts[i + 1]);
				} else {
					UT_GLOBAL(traceDebug) = 9;
				}
				UT_DBGOUT(1, ("<UT> Debug information requested\n"));
			}
		}
	}

	/*
	 *  Process the options passed
	 *  Options are in name / value pairs, and terminate with a NULL
	 */

	for(i = 0; opts[i] != NULL; i += 2) {
		int32_t found = 0;
		const char *optName = opts[i];
		char *optValue = (char*)opts[i+1];
		size_t optNameLen = strlen(optName);
		UT_DBGOUT(1, ("<UT> Processing option %s=%s\n", optName, (optValue == NULL)? "NULL" : optValue ));
		
		if( NULL != optValue ) {
			size_t optValueLen = strlen(optValue);
			/* Values enclosed in {...} should have the brackets removed */
			if (optValue[0] == '{' && optValue[optValueLen-1] == '}') {
				optValue = (char *)j9mem_allocate_memory(optValueLen-1, OMRMEM_CATEGORY_TRACE);
				if (!optValue) {
					rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
					break;
				}
				strncpy(optValue, opts[i+1]+1, optValueLen-2);
				optValue[optValueLen-2] = '\0';
			}
		}

		for (j=0; j < NUMBER_OF_UTE_OPTIONS; j++) {
			if (optNameLen == strlen(UTE_OPTIONS[j].name) && 0 == j9_cmdla_stricmp((char *)optName,(char *)UTE_OPTIONS[j].name)) {
				found = 1;
				
				if (atRuntime && ! UTE_OPTIONS[j].runtimeModifiable) {
					reportCommandLineError(atRuntime, "Option \"%s\" cannot be set at run-time. Set it on the command line at start-up.", optName);
					
					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
				} else {
					if (NULL != UTE_OPTIONS[j].optionFunction) {
						rc = UTE_OPTIONS[j].optionFunction(thr,optValue,atRuntime);
					}
				}
				
				break;
			}
		}
		
		/*
		 * Handle any non-OMR options by calling back to the language layer.
		 */
		if (NULL != optionCallback) {
			if (OMR_ERROR_NONE == optionCallback(vm, optName, optValue, atRuntime)) {
				found = 1;
			}
		}
		
		/*
		 *  Check for options to quietly ignore
		 */
		if (! found) {
			if (UT_GLOBAL(ignore) != NULL) {
				for(j = 0; UT_GLOBAL(ignore[j]) != NULL; j++) {
					if (j9_cmdla_stricmp((char *)optName, (char *)UT_GLOBAL(ignore[j])) == 0) {
						break;
					}
				}
				if (UT_GLOBAL(ignore[j]) != NULL) {
					continue;
				}
			}
		}
		
		if (! found) {
			if (!atRuntime) {
				j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_TRC_UNRECOGNISED_OPTION_STR, optName);
			}
			rc = OMR_ERROR_ILLEGAL_ARGUMENT;
		}
		
		
		if (OMR_ERROR_NONE != rc) {
			if (!atRuntime) {
				if( optValue == NULL ) {
					j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_TRC_ERROR_IN_OPTION_STR, optName);
				} else {
					j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_TRC_ERROR_IN_OPTION_WITH_ARG_STR, optName, optValue);
				}
			}
			break;
		} else {
			/* Add to trace config if the option was valid and actually set. */
			addTraceConfigKeyValuePair(thr, optName, optValue);
		}
		
		/* Check if we allocated a new string for optValue and free */
		if( optValue != opts[i+1] ) {
			j9mem_free_memory(optValue);
		}
	}

	if (UT_GLOBAL(externalTrace) || UT_GLOBAL(extExceptTrace)) {
		UT_GLOBAL(traceInCore) = FALSE;
	}
	
	/*
	 *  Set active flag
	 */

	UT_GLOBAL(traceActive) = TRUE;
	return rc;
}

/**************************************************************************
 * name        - reportCommandLineError
 * description - Report an error in a command line option and put the given
 * 			   - string in as detail in an NLS enabled message.
 * parameters  - detailStr, args for formatting into detailStr.
 *             - suppressMessages, a flag that says whether messages should
 *               be displayed at the moment.
 *************************************************************************/
void
reportCommandLineError(BOOLEAN suppressMessages, const char* detailStr, ...)
{
	ReportCommandLineErrorFunc errorCallback = UT_GLOBAL(languageIntf).ReportCommandLineError;

	if (NULL != errorCallback && !suppressMessages ) {
		va_list arg_ptr;
		va_start(arg_ptr, detailStr);
		errorCallback(UT_GLOBAL(portLibrary), detailStr, arg_ptr);
		va_end(arg_ptr);
	}
}

/**************************************************************************
 * name        - selectSpecial
 * description - Selects special option - provides argument parsing only to
 * 				 allow backwards command line compatibility.
 * parameters  - Component name, component array,
 * returns     - size of the match
 *************************************************************************/
static int
selectSpecial(const char *string)
{
#define DEFAULT_BACKTRACE_LEVEL 4
	int depth = DEFAULT_BACKTRACE_LEVEL;
	const char *p;

	UT_DBGOUT(2, ("<UT> selectSpecial: %s\n", string));
	p = string;
	if (*p == '\0') {
		return(0);
	}
	if ((0 == j9_cmdla_strnicmp(p, UT_BACKTRACE, strlen(UT_BACKTRACE))) &&
			((p[strlen(UT_BACKTRACE)] == ',') ||
			(p[strlen(UT_BACKTRACE)] == '\0'))) {
		UT_DBGOUT(3, ("<UT> Backtrace specifier found\n"));
		p += strlen(UT_BACKTRACE);
		if((*p == ',') && ((*(p+1) >= '0') && (*(p+1) <= '9'))) {
			depth = 0;
			p++;
			while((*p != '\0') && (*p >= '0') && (*p <= '9')) {
				depth *= 10;
				depth += (*p - '0');
				p++;
			}
		}
		UT_DBGOUT(3, ("<UT> Depth set to %d\n", depth));
	}
	if(*p == ',') {
		p++;
	}
	return((int)(p - string));
}


/*******************************************************************************
 * name        - selectComponent
 * description - Selects a component name
 * parameters  - Component name, component array, first time
  *              through flag
 * returns     - size of the match
 ******************************************************************************/
static int
selectComponent(const char *cmd, int32_t *first, char traceType, int32_t setActive, BOOLEAN atRuntime)
{
	int length = 0;

	/*
	 *  If no component specified and first time through, default to all
	 */
	UT_DBGOUT(2, ("<UT> selectComponent: %s\n", cmd));
	if (*cmd == '\0') {
		if (*first) {
			omr_error_t rc = OMR_ERROR_NONE;
			
			UT_DBGOUT(1, ("<UT> Defaulting to All components\n"));
			/*
			 *  Set all components and applications, but ignore groups
			 */
			rc = setTracePointsTo((const char*) UT_ALL, UT_GLOBAL(componentList), TRUE, 0, 0, traceType, -1, NULL, atRuntime, setActive);
			if (OMR_ERROR_NONE != rc) {
				UT_DBGOUT(1, ("<UT> Can't turn on all tracepoints\n"));
				return -1;
			}
		}
		*first = FALSE;   
	} else {
		omr_error_t rc = OMR_ERROR_NONE;
		
		*first = FALSE;
		
		UT_DBGOUT(2, ("<UT> Component %s selected\n", cmd));
		rc = setTracePointsTo(cmd, UT_GLOBAL(componentList), TRUE, 0, 0, traceType, -1, NULL, atRuntime, setActive);
		if (OMR_ERROR_NONE != rc) {
			UT_DBGOUT(1, ("<UT> Can't turn on all tracepoints\n"));
			return -1;
		}

		length = (int) strlen(cmd);

		if (length == 0) {
			length = -1;
		}
	}
	return length;
}

/*******************************************************************************
 * name        - setTraceState
 * description - Set trace activity state
 * parameters  - trace configuration command
 * returns     - return code
 ******************************************************************************/
omr_error_t
setTraceState(const char *cmd, BOOLEAN atRuntime)
{
	omr_error_t	rc = OMR_ERROR_NONE;
	int	i;
	int32_t	first = TRUE;
	int	context = UT_CONTEXT_INITIAL;
	char	traceType = UT_NONE;
	int	length;
	const char  *p;
	int32_t	explicitClass;
	int32_t	setActive = TRUE;

	/*
	 *  Initialize temporary boolean arrays to reflect whether a particular
	 *  component or class is the target of this command
	 */
	UT_DBGOUT(1, ("<UT> setTraceState %s\n", cmd));


	/*
	 * Parse the command using a finite state machine
	 *   Possible state transitions:
	 *
	 *   Initial > Component > Class > Process > Final
	 *                 V__________________V        |
	 *                 |___________________________|
	 */

	p = cmd;
	explicitClass = FALSE;
	while((OMR_ERROR_NONE == rc)  &&
		  (UT_CONTEXT_FINAL != context)) {
		switch (context) {
		/*
		 *  Start of line
		 */
		case UT_CONTEXT_INITIAL:
			UT_DBGOUT(2, ("<UT> setTraceState: Initial\n"));
			if (0 == j9_cmdla_strnicmp(p, UT_MINIMAL_KEYWORD, strlen(UT_MINIMAL_KEYWORD))) {
				if (!UT_GLOBAL(traceEnabled)) {
					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
					break;
				}
				traceType = UT_MINIMAL;
				p += strlen(UT_MINIMAL_KEYWORD);
			} else if (0 == j9_cmdla_strnicmp(p, UT_MAXIMAL_KEYWORD, strlen(UT_MAXIMAL_KEYWORD))) {
				if (!UT_GLOBAL(traceEnabled)) {
					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
					break;
				}
				traceType = UT_MAXIMAL;
				p += strlen(UT_MAXIMAL_KEYWORD);
			} else if (0 == j9_cmdla_strnicmp(p, UT_COUNT_KEYWORD, strlen(UT_COUNT_KEYWORD))) {
				if (!UT_GLOBAL(traceEnabled)) {
					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
					break;
				}
				traceType = UT_COUNT;
				UT_GLOBAL(traceCount) = TRUE;
				p += strlen(UT_COUNT_KEYWORD);
			} else if (0 == j9_cmdla_strnicmp(p, UT_PRINT_KEYWORD, strlen(UT_PRINT_KEYWORD))) {
				traceType = UT_PRINT;
				UT_GLOBAL(indentPrint) = FALSE;
				p += strlen(UT_PRINT_KEYWORD);
			} else if (0 == j9_cmdla_strnicmp(p, UT_NONE_KEYWORD, strlen(UT_NONE_KEYWORD))) {
				traceType = UT_NONE;
				p += strlen(UT_NONE_KEYWORD);
			} else if (0 == j9_cmdla_strnicmp(p, UT_IPRINT_KEYWORD, strlen(UT_IPRINT_KEYWORD))) {
				traceType = UT_PRINT;
				UT_GLOBAL(indentPrint) = TRUE;
				p += strlen(UT_IPRINT_KEYWORD);
			} else if (0 == j9_cmdla_strnicmp(p, UT_PLATFORM_KEYWORD, strlen(UT_PLATFORM_KEYWORD))) {
				traceType = UT_PLATFORM;
				p += strlen(UT_PLATFORM_KEYWORD);
			} else if (0 == j9_cmdla_strnicmp(p, UT_EXTERNAL_KEYWORD, strlen(UT_EXTERNAL_KEYWORD))) {
				traceType = UT_EXTERNAL;
				p += strlen(UT_EXTERNAL_KEYWORD);
			} else if (0 == j9_cmdla_strnicmp(p, UT_TRIGGER_KEYWORD, strlen(UT_TRIGGER_KEYWORD))) {
				unsigned char temp = UT_TRIGGER;
				traceType = (char)temp;
				p += strlen(UT_TRIGGER_KEYWORD);
			} else if (0 == j9_cmdla_strnicmp(p, UT_EXCEPTION_KEYWORD, strlen(UT_EXCEPTION_KEYWORD))) {
				if (!UT_GLOBAL(traceEnabled)) {
					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
					break;
				}
				traceType = UT_EXCEPTION;
				p += strlen(UT_EXCEPTION_KEYWORD);
			} else {
				/*
				 *  Check for options to quietly ignore
				 */
				if (UT_GLOBAL(ignore) != NULL) {
					for(i = 0; UT_GLOBAL(ignore[i]) != NULL; i++) {
						if (0 == j9_cmdla_strnicmp(p, UT_GLOBAL(ignore[i]), strlen(UT_GLOBAL(ignore[i])))) {
							p += strlen(p);
							break;
						}
					}
					if (*p == '\0') {
						context = UT_CONTEXT_FINAL;
						break;
					}
				}
				rc = OMR_ERROR_ILLEGAL_ARGUMENT;
				break;
			}

			if (*p == '=') {
				p++;
			} 
			
			if (*p == '\0' && traceType != UT_NONE) {
				/* Only the keyword has been specified and the option isn't -Xtrace:none */
				reportCommandLineError(atRuntime, "Option %s requires an argument.",cmd);
				rc = OMR_ERROR_ILLEGAL_ARGUMENT;
			}
			
			context = UT_CONTEXT_COMPONENT;
			UT_GLOBAL(traceDests) |= traceType;
			break;

		/*
		 *  Looking for a component or TPID
		 */
		case UT_CONTEXT_COMPONENT:
			UT_DBGOUT(2, ("<UT> setTraceState: Component\n"));
			setActive = TRUE;

			if (*p == '!') {
				UT_DBGOUT(2, ("<UT> Reset of tracepoints requested\n"));
				p++;
				setActive = FALSE;                
			}
			/* Parse deprecated option -Xtrace:maximal={backtrace[,depth]} */
			if((length = selectSpecial(p)) != 0) {
				if (length < 0) {
					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
				} else {
					p += length;
				}
			} else {
				/*
				 * Check for component specification
				 */

				length = selectComponent(p, &first,
										 traceType, setActive, atRuntime);
				if (length < 0) {
					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
				} else {
					p += length;
					if (!first) {
						context = UT_CONTEXT_CLASS;
					}
					if (*p == '(') {
						p++;
						explicitClass=TRUE;
					}
				}
			}
			break;
		/*
		 *  Looking for classes
		 */
		case UT_CONTEXT_CLASS:
			UT_DBGOUT(2, ("<UT> setTraceState: Class\n"));
						/* handled in selectComponent */
						context = UT_CONTEXT_PROCESS;
			break;
		/*
		 *  Process the state so far.
		 */
		case UT_CONTEXT_PROCESS:
			UT_DBGOUT(2, ("<UT> setTraceState: Process\n"));
						if (*p == '\0'){
							context = UT_CONTEXT_FINAL;
						} else {
							setActive = TRUE;
							context = UT_CONTEXT_COMPONENT;
						}
			break;
		}


		/*
		 *  Check for more input
		 */
		if (OMR_ERROR_ILLEGAL_ARGUMENT != rc) {
			if (*p =='\0') {
				if (context == UT_CONTEXT_COMPONENT &&
					!first) {
					context = UT_CONTEXT_FINAL;
				}
			}
			if (*p == ',') {
				p++;
				if (*p == '\0') {
					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
				}
			}
		}
	}

	/*
	 *  Expecting more input ?
	 */
	if (explicitClass) {
		rc = OMR_ERROR_ILLEGAL_ARGUMENT;
	}
	if (OMR_ERROR_NONE == rc) {
		if (context != UT_CONTEXT_FINAL) {
			reportCommandLineError(atRuntime, "Trace selection specification incomplete: %s", cmd);
			rc = OMR_ERROR_ILLEGAL_ARGUMENT;
		}
	} else {
		reportCommandLineError(atRuntime, "Syntax error encountered at offset %d in: %s", p - cmd, cmd);
	}

	return rc;
}

/*******************************************************************************
 * name        - setOptions
 * description - Universal trace options
 * parameters  - char **options, atRuntime
 * returns     - OMR return code
 ******************************************************************************/

omr_error_t 
setOptions(UtThreadData **thr, const char **opts, BOOLEAN atRuntime)
{
	omr_error_t	rc = OMR_ERROR_NONE;

	UT_DBGOUT(1, ("<UT> Initializing options \n"));

	if (!atRuntime) {
		/*
		 *  Process any additional early options
		 */
		rc = processEarlyOptions(opts);
		if (OMR_ERROR_NONE != rc) {
			/* Can only fail due to OOM copying path to format file, no message required. */
			return OMR_ERROR_ILLEGAL_ARGUMENT;
		}
	}

	/*
	 *  Process the rest of the options
	 */
	rc = processOptions(thr, opts, atRuntime);
	if (OMR_ERROR_NONE != rc) {
		/* An error message will have been reported by processOptions
		 * J9VMDLLMain will display general -Xtrace:help output.
		 */
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}
	return OMR_ERROR_NONE;
}

/**************************************************************************
 * name        - propertyFileOption
 * description - Handle property file options passed back by UTE
 * parameters  - thread and option.
 * returns     - UTE error code
 *************************************************************************/
static omr_error_t 
propertyFileOption(const char *value)
{

	const OMR_VM *vm = UT_GLOBAL(vm);
	SetLanguageTraceOptionFunc optionCallback = UT_GLOBAL(languageIntf).SetLanguageTraceOption;

	char *tempStr;
	char *optName;
	char *optValue = NULL;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	if (!value) {
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}
	
	if (NULL == optionCallback) {
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	tempStr = j9mem_allocate_memory(strlen(value) + 1, OMRMEM_CATEGORY_TRACE);
	if (!tempStr) {
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}

	strcpy(tempStr, value);

	/* If this is a name/value pair split it up. */
	optName = tempStr;
	optValue = strchr(tempStr, '=');
	if( optValue != NULL ) {
		size_t optValueLen = strlen(optValue);
		*optValue = '\0';
		optValue++;
		optValueLen = strlen(optValue);
		/* Values enclosed in {...} should have the brackets removed */
		if (optValue[0] == '{' && optValue[optValueLen-1] == '}') {
			optValue++;
			optValue[strlen(optValue)-1] = '\0';
		}
	}


	/*
	 * Handle any non-OMR options by calling back to the language layer.
	 */
	if (OMR_ERROR_NONE != optionCallback(vm, optName, optValue, FALSE)) {
		j9mem_free_memory(tempStr);
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}
	
	j9mem_free_memory(tempStr);
	return OMR_ERROR_NONE;
}
