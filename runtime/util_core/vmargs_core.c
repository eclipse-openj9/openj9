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
#if defined(WIN32)
#include <windows.h>
#define J9_MAX_ENV 32767
#endif
#include "vmargs_core_api.h"
#include "omrutil.h"
#include "j9vmutilnls.h"

#if defined(J9ZOS390)
#include "atoe.h"
#endif

#define DOUBLE_QUOTE_CHAR '\"'				/* envy c-dev hack */
#define SLASH_CHAR '\\'
#define IS_SLASH_QUOTE(quoteChar) ((quoteChar[0] == SLASH_CHAR) && (quoteChar[1] == DOUBLE_QUOTE_CHAR))
#define IS_QUOTE_QUOTE(quoteChar) ((quoteChar[0] == DOUBLE_QUOTE_CHAR) && (quoteChar[1] == DOUBLE_QUOTE_CHAR))
#define DELETE_CHARS(start, num) memmove((start), (start)+(num), (strlen((start))+1)-(num))

#define JVMINIT_VERBOSE_INIT_TRACE1(a, b, c)

J9JavaVMArgInfo *
newJavaVMArgInfo(J9JavaVMArgInfoList *vmArgumentsList, char *optString, uintptr_t flags)
{
	J9JavaVMArgInfo *optArg = (J9JavaVMArgInfo*) pool_newElement(vmArgumentsList->pool);
	if (NULL == optArg) {
		return NULL;
	}
	if (NULL == vmArgumentsList->head) {
		vmArgumentsList->head = optArg;
	} else {
		vmArgumentsList->tail->next = optArg;
	}
	vmArgumentsList->tail = optArg;
	optArg->vmOpt.optionString = optString;
	optArg->vmOpt.extraInfo = NULL;
	optArg->cmdLineOpt.mapping = NULL;
	optArg->cmdLineOpt.flags = flags;
	optArg->cmdLineOpt.fromEnvVar = NULL;
	optArg->next = NULL;
	return optArg;
}

/*
 * Tokenize the fileText string into a linked list, removing comments, continuation chars and EOL chars.
 * Tabs and spaces at the end of a line before a continuation (i.e. backslash-newline) are preserved.
 * Tabs and spaces at the beginning of lines (including after a continuation) are removed.
 * Return number of new arguments.
 */

intptr_t
parseOptionsFileText(J9PortLibrary* portLibrary, const char* fileText, J9JavaVMArgInfoList *vmArgumentsList, uintptr_t verboseFlags)
{
	char* cursor = (char*)fileText;
	uintptr_t isEOL, isEOF, isMultiple;
	char* commentStart, *continueChar, *tokenStart, *lastSpace, *firstCharOfLine, *spaceStart, *quoteStart, *optionStart;
	intptr_t count = 0;
	char *parsedTextBuffer = NULL; /* holds the strings to be added to vmArgumentsList */
	char *nextOptionPosition = NULL; /* points to the remaining free space in parsedTextBuffer */
	size_t bufferLength = 0; /* Memory required to hold copies of arguments. This is an overestimate because of whitespace and comments */

	PORT_ACCESS_FROM_PORT(portLibrary);

	/* If we failed to read the file, fileText will be NULL */
	if (fileText == NULL) {
		return 0;
	}

	bufferLength = strlen(fileText) + 1;
	commentStart = continueChar = tokenStart = spaceStart = lastSpace = quoteStart = optionStart = NULL;
	firstCharOfLine = cursor;
	isEOF = isEOL = isMultiple = 0;

	do {
		/* While walking comment line, skip chars until EOL */
		if ((commentStart != NULL) && !((cursor[0] == '\r') || (cursor[0] == '\n') || (cursor[0] == '\0'))) {
			goto _skip;
		}

		switch (*cursor) {
		case '\r' :
		case '\n' :
		case '\0' :
			isEOL = !(cursor[0]=='\r' && cursor[1]=='\n');					/* EOL can be \r, \n or \r\n */
			isEOF = (cursor[0]=='\0');
			if (continueChar == NULL) {												/* Null-terminate at EOL if not continuing onto next line */
				cursor[0] = '\0';
			}
			break;
		case '\\' :																				/* \ denotes continuation onto next line - only do this if next char is EOL */
			if ((cursor[1] == '\r') || (cursor[1] == '\n')) {
				if (spaceStart == NULL) {
					continueChar = cursor;
				} else {
					continueChar = spaceStart;										/* Remove trailing space before continuation char */
				}
			} else if (IS_SLASH_QUOTE(cursor)) {
				DELETE_CHARS(cursor, 1);
			}
			break;
		case '#' :																				/* # denotes start of a comment line - if # is not the first char, line is not a comment */
			if (firstCharOfLine == cursor) {
				commentStart = cursor;
			}
			break;
		case ' ' :
		case '\t' :
			if (quoteStart == NULL) {
				lastSpace = cursor;
				if (spaceStart == NULL) {													/* Keep track of the start of an area of spaces */
					spaceStart = cursor;
				}
			}
			break;
		case DOUBLE_QUOTE_CHAR :
			if (quoteStart == NULL) {
				quoteStart = cursor;
			} else {
				quoteStart = NULL;
			}
			DELETE_CHARS(cursor, 1);
			--cursor;
			break;
		case '-' :
			optionStart = cursor;
			/* FALLTHROUGH */
		default :
			if (tokenStart == NULL) {													/* If none of the above, start a new token */
				tokenStart = cursor;
				lastSpace = spaceStart = NULL;									/* leading space can be ignored */
			}
			if (!commentStart) {
				if ((quoteStart == NULL) && (lastSpace == (cursor - 1)) && (cursor != optionStart)) {
					spaceStart = NULL;								/* We've just had space outside of quotes and current char is not '-'. This space must be part of an option. Ignore it. */
				}
				if (continueChar) {								/* If continuing from previous line, remove all the chars between cursor and continueChar */
					intptr_t charsToRemove = (cursor - continueChar);

					DELETE_CHARS(continueChar, charsToRemove);
					cursor -= charsToRemove;
					continueChar = spaceStart = NULL;
				}
				/* Test to see whether this is the start of a new token on a line with a previous token */
				isMultiple = ((lastSpace == (cursor - 1)) && (cursor == optionStart));
			}
		}

		/* If end of token... */
		if (isEOL || isEOF || isMultiple) {
			/* If we're not continuing onto a new line... */
			if (continueChar == NULL) {
				if ((commentStart == NULL) && (tokenStart != NULL) && (tokenStart[0] != '\0')) {
					/* ignore empty lines and comments */
					J9JavaVMArgInfo *optArg = NULL;
					size_t argumentLength = 0;

					if (NULL == parsedTextBuffer) {
						/* allocate the buffer on the first argument */
						parsedTextBuffer = 	j9mem_allocate_memory(bufferLength, OMRMEM_CATEGORY_VM);
						if (NULL == parsedTextBuffer) {
							return RC_FAILED;
						}
						nextOptionPosition = parsedTextBuffer;
					}
					/* Remove trailing space */
					if (spaceStart != NULL) {
						*spaceStart = '\0';
						spaceStart = NULL;
					}
					if (quoteStart != NULL) {
						j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_VMUTIL_MALFORMED_OPTIONSFILE_ERROR_STR, firstCharOfLine);
						return RC_MALFORMED;
					}
					JVMINIT_VERBOSE_INIT_TRACE1(verboseFlags, "Adding option \"%s\" from options file\n", tokenStart);
					argumentLength = strlen(tokenStart) + 1; /* include the terminating null */
					/*
					 * memcpy is safe because the sum of the lengths of the arguments
					 * will be less than or equal to the size of fileText, which determines the size of parsedTextBuffer.
					 */
					memcpy(nextOptionPosition, tokenStart, argumentLength);
					/* The first argument is at the head of the buffer and is the same pointer as the allocated buffer */
					optArg = newJavaVMArgInfo(vmArgumentsList, nextOptionPosition,
							((0 == count) ? ARG_MEMORY_ALLOCATION: 0) | CONSUMABLE_ARG);
					if (NULL == optArg) {
						return RC_FAILED;
					}
					nextOptionPosition += argumentLength; /* advance to the next free area of the buffer */
					tokenStart = optionStart = NULL;
					++count;
				} else {
					tokenStart = NULL;													/* if comment, throw away the comment line */
				}
				spaceStart = NULL;
			}
			if (isEOL) {
				firstCharOfLine = (cursor + 1);
				commentStart = NULL;
			}
			if (isMultiple) {
				--cursor;								/* We're already on the first char of the next token... step back one char */
			}
			isEOL = isMultiple = 0;
		}
		_skip :
		++cursor;
	} while (!isEOF);
	return count;
}

intptr_t
addXOptionsFile(J9PortLibrary* portLib, const char *xOptionsfileArg, J9JavaVMArgInfoList *vmArgumentsList, uintptr_t verboseFlags)
{

	PORT_ACCESS_FROM_PORT(portLib);
	char* filePath = strchr(xOptionsfileArg, '=');
	intptr_t fd = -1;
	J9JavaVMArgInfo *optArg = NULL;
	int64_t length = -1;
	char *optionsFileBuffer = NULL;
	char *optionsFileContents = NULL;
	size_t argLen = strlen(xOptionsfileArg);
	BOOLEAN argOkay = TRUE;

	if ((NULL == filePath) || ('\0' == filePath[1U])) { /* if we found no "=" or nothing after the "=" */
		JVMINIT_VERBOSE_INIT_TRACE1(verboseFlags, "File path missing from -Xoptionsfile\n", filePath);
		argOkay = FALSE;
		/* missing paths are ignored */
	} else {
		filePath += 1; /* advance to the start of the path itself */
		length = j9file_length(filePath);
		/* Restrict file size to < 2G */
		if (length > J9CONST64(0x7FFFFFFF)) {
			j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_VMUTIL_OPTIONS_FILE_INVALID_STR, filePath);
			return -1;
		}
		JVMINIT_VERBOSE_INIT_TRACE1(verboseFlags, "Opening options file %s\n", filePath);

		if ((fd = j9file_open(filePath, EsOpenRead, 0)) == -1) {
			j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_VMUTIL_OPTIONS_FILE_NOT_FOUND_STR, filePath);
			argOkay = FALSE;
		}
	}
	if (!argOkay) {
		/* add the -Xoptionsfile argument, even though it is ignored */
		optArg = newJavaVMArgInfo(vmArgumentsList, NULL, ARG_MEMORY_ALLOCATION|CONSUMABLE_ARG);
		optionsFileBuffer = j9mem_allocate_memory(argLen + 1, OMRMEM_CATEGORY_VM);
		memcpy(optionsFileBuffer, xOptionsfileArg, argLen + 1);
		optArg->vmOpt.optionString = optionsFileBuffer;
		return 0;
	}

	optionsFileBuffer = j9mem_allocate_memory(argLen + (uintptr_t)length + 2, OMRMEM_CATEGORY_VM);

	if (NULL == optionsFileBuffer) {
		return -1;
	}

	/* add the -Xoptionsfile argument */
	optArg = newJavaVMArgInfo(vmArgumentsList, NULL, ARG_MEMORY_ALLOCATION|CONSUMABLE_ARG);
	if (NULL == optArg) {
		j9mem_free_memory(optionsFileBuffer);
		return -1;
	}
	memcpy(optionsFileBuffer, xOptionsfileArg, argLen + 1);
	optArg->vmOpt.optionString = optionsFileBuffer;

	optionsFileContents = optionsFileBuffer + strlen(xOptionsfileArg) + 1; /* leave the \0 */
	optionsFileContents[length] = '\0';
	if (j9file_read(fd, optionsFileContents, (intptr_t)length) != -1) {
		intptr_t parseResult = -1;
		/* convert to ascii if on OS390 */
#if defined(J9ZOS390)
		{
			char *abuf;
			abuf = e2a(optionsFileContents, length);
			memcpy(optionsFileContents, abuf, length);
			free(abuf);
		}
#endif
		parseResult = parseOptionsFileText(PORTLIB, optionsFileContents, vmArgumentsList, verboseFlags);

		if (parseResult < 0) {
			j9mem_free_memory(optionsFileBuffer);
			return -1;
		}
	}
	j9file_close(fd);
	return 0;
}

intptr_t
parseOptionsBuffer(J9PortLibrary* portLib, char* argumentBuffer, J9JavaVMArgInfoList *vmArgumentsList, uintptr_t verboseFlags, BOOLEAN parseOptionsFileFlag)
{
	char* cursor = argumentBuffer;
	char* tokenStart = NULL;
	char *tokenEnd = NULL;
	char *quotesStart = NULL;
	char *quotesEnd = NULL;
	intptr_t count = 0;
	PORT_ACCESS_FROM_PORT(portLib);

	while (*cursor) {
		char *escapeQuote = NULL; /* indicate "ignore next quote" */
		if (*cursor == ' ') {
			if (NULL == tokenStart) {					/* ignore leading spaces */
				cursor += 1;
				continue;
			} else if ((NULL == quotesStart) || (NULL != quotesEnd)) {			/* if not within quotes, terminate the token */
				*cursor = '\0';
				tokenEnd = cursor;
			}
		}
		if (NULL == tokenStart) {					/* if starting new token */
			tokenStart = cursor;
		}
		if (IS_SLASH_QUOTE(cursor)) {		/* if \" */
			DELETE_CHARS(cursor, 1);
			escapeQuote = cursor;
		}
		if (DOUBLE_QUOTE_CHAR == *cursor) {
			if (NULL == escapeQuote && IS_QUOTE_QUOTE(cursor)) {		/* if "" */
				DELETE_CHARS(cursor, 1);
				escapeQuote = cursor;
			}
			if (NULL == escapeQuote) {
				if (NULL == quotesStart) {
					quotesStart = cursor;
				} else {
					quotesEnd = cursor;
				}
				DELETE_CHARS(cursor, 1);		/* delete non-escape quote char */
				--cursor;
			}
		}
		if (*(cursor + 1) == '\0') {							/* peek for end of string */
			tokenEnd = cursor;
		}
		if (tokenEnd) {
			if ((0 == strncmp(tokenStart, VMOPT_XOPTIONSFILE_EQUALS, strlen(VMOPT_XOPTIONSFILE_EQUALS))) && (TRUE == parseOptionsFileFlag)) {
				/* addXOptionsFile allocates a new buffer for the -Xoptionsfile argument and the file contents */
				if (0 != addXOptionsFile(portLib, tokenStart, vmArgumentsList, verboseFlags)) {
					return -1;
				}
			} else {
				J9JavaVMArgInfo *optArg = newJavaVMArgInfo(vmArgumentsList, NULL, CONSUMABLE_ARG);
				if (NULL == optArg) {
					return -1;
				}
				if ((0 == count) && (tokenStart != argumentBuffer)) { /* the first argument string must point to the start of the argumentBuffer so it can be used to free the buffer */
				/* cannot use memcpy because it does not allow the areas to overlap */
					memmove(argumentBuffer, argumentBuffer, strlen(tokenStart + 1));
				}
				optArg->vmOpt.optionString = tokenStart;
				if (tokenStart == argumentBuffer) {
					optArg->cmdLineOpt.flags |= ARG_MEMORY_ALLOCATION; /* mark the first argument as being the pointer to free */
				}
				count += 1;
			}
			tokenStart = tokenEnd = quotesStart = quotesEnd = NULL;
		}
		cursor += 1;
	}
	if (0 == count) { /* didn't create any arguments using this buffer */
		j9mem_free_memory(argumentBuffer);
	}
	return 0;
}


intptr_t
parseHypervisorEnvVar(J9PortLibrary *portLib, char *argBuffer, J9JavaVMArgInfoList *hypervisorArgumentsList)
{
	/*
	 * The parsing of the options file (provided by -Xoptionsfile) is not
	 * required for parsing the IBM_JAVA_HYPERVISOR_SETTINGS env variable. Hence
	 * calling the parseOptionsBuffer with the parseOptionsFileFlag = FALSE
	 * which controls the parsing of the -Xoptionsfile
	 */
	return parseOptionsBuffer(portLib, argBuffer, hypervisorArgumentsList, 0, FALSE);
}
