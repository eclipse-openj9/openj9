/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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

#include <string.h>

#include "jvmtinls.h"
#include "j9port.h"
#include "j9protos.h"
#include "vm_internal.h"
#include "j9vmnls.h"
#include "j9vmutilnls.h"
#include <ctype.h>
#include "jvminit.h"
#include "util_api.h"

typedef struct J9LogLevel {
	char* level;
	UDATA combine;
	UDATA flag;
} J9LogLevel;

static const J9LogLevel logLevels[] = {
	{"all",    FALSE, J9NLS_ERROR|J9NLS_WARNING|J9NLS_INFO|J9NLS_CONFIG|J9NLS_VITAL},
	{"none",   FALSE, 0},
	{"error",  TRUE,  J9NLS_ERROR},
	{"warn",   TRUE,  J9NLS_WARNING},
	{"info",   TRUE,  J9NLS_INFO},
	{"config", TRUE,  J9NLS_CONFIG},
	{"vital",  TRUE,  J9NLS_VITAL},
};

/* These identify locations in the above array */
#define XLOG_OPTION_FIRST_PRINTABLE 2
#define XLOG_OPTION_NONE 1

static const UDATA numLogLevels = sizeof(logLevels) / sizeof(logLevels[0]);

static const UDATA defaultLogOptions = J9NLS_ERROR | J9NLS_VITAL;

static UDATA parseLogOptions(char *options, UDATA *optionsFlags, UDATA *parseSucceeded);
static void printXlogUsage(struct J9PortLibrary* PORTLIB);

/**
 * Query the settings of the JVM log options.
 *
 * @param[in] vm - The vm
 * @param[in] bufferSize - size of optionsBuffer
 * @param[in] optionsBuffer - pointer to the buffer that will hold the result
 * @param[in] dataSize - size of buffer needed to hold the options
 *
 * @return Boolean: true on success, false on failure
 */
UDATA
queryLogOptions(J9JavaVM *vm, I_32 bufferSize, void *optionsBuffer, I_32 *dataSize)
{
	UDATA opts;
	UDATA i;
	UDATA optionCount;
	I_32 length;
	PORT_ACCESS_FROM_JAVAVM(vm);

	/* basic sanity */
	if (dataSize == NULL) {
		return JVMTI_ERROR_NULL_POINTER;
	}

	if (optionsBuffer == NULL) {
		return JVMTI_ERROR_NULL_POINTER;
	}

	if (bufferSize == 0) {
		return JVMTI_ERROR_ILLEGAL_ARGUMENT;
	}

	/* call the portlibrary to query the JVM log options */
	opts = j9syslog_query();

	/* Test for 'none' case */
	if (opts == 0) {
		length = (I_32)strlen(logLevels[XLOG_OPTION_NONE].level);
		*dataSize = length+1;
		if(bufferSize > length) {
			strcpy(optionsBuffer,logLevels[XLOG_OPTION_NONE].level);
			return JVMTI_ERROR_NONE;
		} else {
			return JVMTI_ERROR_ILLEGAL_ARGUMENT;
		}
	}

	/* get the length of the required buffer */
	for (i = XLOG_OPTION_FIRST_PRINTABLE, optionCount = 0, length = 0; i < numLogLevels; i++) {
		if ((opts & logLevels[i].flag) == logLevels[i].flag) {

			/* +1 as always append ',' or '\0' */
			length += (I_32)strlen(logLevels[i].level) + 1;
			optionCount++;
		}
	}
	*dataSize = length;

	if (bufferSize < length) {
		return JVMTI_ERROR_ILLEGAL_ARGUMENT;
	}

	strcpy(optionsBuffer, "");

	/* create the options string */
	for (i = XLOG_OPTION_FIRST_PRINTABLE, optionCount = 0; i < numLogLevels; i++) {
		if ((opts & logLevels[i].flag) == logLevels[i].flag) {
			if (optionCount > 0) {
				strcat(optionsBuffer, ",");
			}
			strcat(optionsBuffer, logLevels[i].level);
			optionCount++;
		}
	}
	return JVMTI_ERROR_NONE;
}

/**
 * Set the JVM log flags.
 *
 * @param[in] portLibrary The port library
 * @param[in] options - string containing the options to set
 *
 * @return Boolean: true on success, false on failure
 */
UDATA
setLogOptions(J9JavaVM *vm, char *options)
{
	UDATA rc = FALSE;
	UDATA optsParsed = FALSE;
	UDATA setOptions = 0;
	char *buffer;
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (options == NULL) {
		return JVMTI_ERROR_NULL_POINTER;
	}

	/* +1 for trailing \0 */
	buffer = j9mem_allocate_memory(strlen((char*)options)+1, OMRMEM_CATEGORY_VM);
	if (buffer == NULL) {
		return JVMTI_ERROR_OUT_OF_MEMORY;
	}

	strcpy(buffer, (char*)options);
	rc = parseLogOptions(buffer, &setOptions, &optsParsed);

	if (optsParsed == TRUE) {
		 /* call the port library to set the JVM log options */
		j9syslog_set(setOptions);
	}
	j9mem_free_memory(buffer);

	return rc;
}

UDATA
parseLogOptions(char *options, UDATA *optionsFlags, UDATA *parseSucceeded)
{
	UDATA optlen;
	UDATA i, j;
	UDATA seenNonCombining, optCount;
	UDATA tempFlags;
	char *token;

	/* quick validity checks */
	if (options == NULL) {
		return JVMTI_ERROR_NULL_POINTER;
	}

	if (optionsFlags == NULL) {
		return JVMTI_ERROR_NULL_POINTER;
	}

	if (parseSucceeded == NULL) {
		return JVMTI_ERROR_NULL_POINTER;
	}

	*parseSucceeded = FALSE;
	optlen = strlen(options);

	/* remove any whitespace from the options string */
	for (i = 0, j = 0; i < optlen; i++) {
		if (!OMR_ISSPACE(options[i])) {
			options[j] = options[i];
			j++;
		}
	}
	options[j] = '\0';

	/* recalculate length after collapsing whitespace */
	optlen = strlen(options);

	if (optlen == 0) {
		/* empty string has been passed, raise error */
		return JVMTI_ERROR_ILLEGAL_ARGUMENT;
	}

	token = strtok(options,",");
	seenNonCombining = FALSE;
	optCount = 0;
	tempFlags = 0;

	/* loop over all tokens in the options string */
	while (token != NULL) {
		UDATA isValidToken = FALSE;

		/* check for a match against the log options table */
		for (i = 0; i < numLogLevels; i++) {
			if (j9_cmdla_stricmp(logLevels[i].level, token) == 0) {

				/* check and set validity of combinations */
				if (logLevels[i].combine == FALSE) {
					if (optCount != 0) {
						return JVMTI_ERROR_ILLEGAL_ARGUMENT;
					}
					seenNonCombining = TRUE;
				} else if (seenNonCombining) {
					return JVMTI_ERROR_ILLEGAL_ARGUMENT;
				}

				optCount++;
				tempFlags |= logLevels[i].flag;
				isValidToken = TRUE;
				break;
			}
		}

		/* Found a token that was unexpected */
		if (isValidToken != TRUE) {
			return JVMTI_ERROR_ILLEGAL_ARGUMENT;
		}

		token = strtok(NULL,",");
	}

	*parseSucceeded = TRUE;
	*optionsFlags = tempFlags;
	return JVMTI_ERROR_NONE;
}

jint
processXLogOptions(J9JavaVM * vm)
{
	IDATA xlogindex = FIND_ARG_IN_VMARGS_FORWARD(OPTIONAL_LIST_MATCH, VMOPT_XSYSLOG_OPT, NULL);
	/* The default for -Xsyslog is to be turned on and capture 'error' and 'vital' type messages */
	UDATA xlogflags = defaultLogOptions;
	UDATA printhelp = FALSE;
	UDATA tempflags = 0;
	jint rc = JNI_OK;
	J9VMInitArgs* j9vm_args = vm->vmArgsArray;

	PORT_ACCESS_FROM_JAVAVM(vm);

	/* If xlogindex == -1 nothing was specified, so use the default setting from above */

	while (xlogindex >= 0) {
		IDATA optionsfound = 0;
		UDATA optionsparsed = FALSE;
		char xlogoptionsbuf[SMALL_STRING_BUF_SIZE];
		char *xlogoptions = xlogoptionsbuf;

		CONSUME_ARG(j9vm_args, xlogindex);
		optionsfound = COPY_OPTION_VALUE(xlogindex, ':', &xlogoptions, sizeof(xlogoptionsbuf));

		if (optionsfound != OPTION_OK) {
			rc = JNI_ERR;
			goto _couldnotparse;
		}

		if (xlogoptions == NULL) {
			/* Reached if e.g. just -Xsyslog is specified */
			printhelp = TRUE;
			j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_VM_OPTION_MALFORMED, j9vm_args->actualVMArgs->options[xlogindex].optionString);

		} else if (j9_cmdla_stricmp(logLevels[XLOG_OPTION_NONE].level, xlogoptions) == 0) {
			/* -Xsyslog:none suppresses everything seen so far */
			printhelp = FALSE;
			xlogflags = 0;

		} else if (j9_cmdla_stricmp("help", xlogoptions) == 0) {
			printhelp = TRUE;

		} else {
			/* Found some options to try and process */
			parseLogOptions(xlogoptions, &tempflags, &optionsparsed);

			if (optionsparsed != TRUE) {
				j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_VM_OPTION_MALFORMED, j9vm_args->actualVMArgs->options[xlogindex].optionString);
			} else {
				xlogflags |= tempflags;
			}
		}

		xlogindex = FIND_NEXT_ARG_IN_VMARGS_FORWARD(STARTSWITH_MATCH, VMOPT_XSYSLOG_OPT, NULL, xlogindex);
	}

	/* Options are good, start logging */
	j9port_control(J9PORT_CTLDATA_SYSLOG_OPEN, xlogflags);

_couldnotparse:

	if (printhelp) {
		printXlogUsage(PORTLIB);
	}

	return rc;
}

static void
printXlogUsage(struct J9PortLibrary *PORTLIB)
{
	j9tty_err_printf(j9nls_lookup_message(J9NLS_INFO | J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_VM_XLOG_HELP01, NULL));
	j9tty_err_printf(j9nls_lookup_message(J9NLS_INFO | J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_VM_XLOG_HELP02, NULL));
	j9tty_err_printf(j9nls_lookup_message(J9NLS_INFO | J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_VM_XLOG_HELP03, NULL));
	j9tty_err_printf(j9nls_lookup_message(J9NLS_INFO | J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_VM_XLOG_HELP04, NULL));
	j9tty_err_printf(j9nls_lookup_message(J9NLS_INFO | J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_VM_XLOG_HELP05, NULL));
	j9tty_err_printf(j9nls_lookup_message(J9NLS_INFO | J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_VM_XLOG_HELP06, NULL));
	j9tty_err_printf(j9nls_lookup_message(J9NLS_INFO | J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_VM_XLOG_HELP07, NULL));
	j9tty_err_printf(j9nls_lookup_message(J9NLS_INFO | J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_VM_XLOG_HELP08, NULL));
	j9tty_err_printf(j9nls_lookup_message(J9NLS_INFO | J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_VM_XLOG_HELP09, NULL));
	j9tty_err_printf(j9nls_lookup_message(J9NLS_INFO | J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_VM_XLOG_HELP10, NULL));
	j9tty_err_printf(j9nls_lookup_message(J9NLS_INFO | J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_VM_XLOG_HELP11, NULL));
	j9tty_err_printf(j9nls_lookup_message(J9NLS_INFO | J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_VM_XLOG_HELP12, NULL));
}
