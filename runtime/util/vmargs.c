/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
#include "vmargs_api.h"
#include "vm_api.h"
#include "j2senls.h"
#include "j2sever.h"
#include "j9argscan.h"
#include "jvminit.h"

#if !defined(MAX_PATH)
#if !defined(WIN32)
#include <limits.h>
#if defined(J9ZOS390)
#define MAX_PATH _POSIX_PATH_MAX
#else
#define MAX_PATH PATH_MAX
#endif /* defined(J9ZOS390)*/
#endif /* !defined(WIN32) */
#endif /* !defined(MAX_PATH) */

#include "j9vmutilnls.h"
#include "util_internal.h"
#include "ut_j9util.h"
#include "verbose_api.h"

#if defined(J9ZOS390)
#include "atoe.h"
#endif

#define MAP_TWO_COLONS_TO_ONE 8
#define EXACT_MAP_NO_OPTIONS 16
#define EXACT_MAP_WITH_OPTIONS 32
#define MAP_MEMORY_OPTION 64
#define STARTSWITH_MAP_NO_OPTIONS 128
#define MAP_ONE_COLON_TO_TWO 256
#define MAP_WITH_INCLUSIVE_OPTIONS 512

#if !defined(J9JAVA_PATH_SEPARATOR)
#if defined(WIN32)
#define	J9JAVA_PATH_SEPARATOR ";"
#define ALT_JAVA_HOME_DIR_STR L"_ALT_JAVA_HOME_DIR"
#else
#define	J9JAVA_PATH_SEPARATOR ":"
#endif
#endif /* !defined(J9JAVA_PATH_SEPARATOR) */
#define JAVA_HOME_EQUALS "-Djava.home="
#define JAVA_LIB_PATH_EQUALS "-Djava.library.path="
#define JAVA_EXT_DIRS_EQUALS "-Djava.ext.dirs="
#define JAVA_USER_DIR_EQUALS "-Duser.dir="
#define OPTIONS_DEFAULT "options.default"

#define LARGE_STRING_BUF_SIZE 256

static char * getStartOfOptionValue(J9VMInitArgs* j9vm_args, IDATA element, const char *optionName);

IDATA
findArgInVMArgs(J9PortLibrary *portLibrary, J9VMInitArgs* j9vm_args, UDATA match, const char* optionName, const char* optionValue, UDATA doConsumeArgs) {
	UDATA optionCntr;
	IDATA optionLength, charCntr, returnVal = -1;
	char* cursor, *valueString, *testString, *actualString;
	char testChar;
	UDATA success, hasBeenSet, endCharIsValid;

	/* match is magically split into sections */
	UDATA stopAtIndex = match >> STOP_AT_INDEX_SHIFT;
	UDATA searchForward = (match & SEARCH_FORWARD);
	UDATA realMatch = (match & REAL_MATCH_MASK);
	UDATA searchToIndex = 0;
	UDATA searchFromIndex = 0;

	PORT_ACCESS_FROM_PORT(portLibrary);

	if (optionName==NULL || 0==j9vm_args->nOptions) {
		return returnVal;
	}

	hasBeenSet = FALSE;
	optionLength = strlen(optionName);

	if (searchForward) {
		if (stopAtIndex < j9vm_args->nOptions) {
			searchFromIndex = stopAtIndex;				/* stopAtIndex is misnamed if searching forwards - actually "startFromIndex" - the macro adds 1 for you*/
			searchToIndex = j9vm_args->nOptions - 1;
		} else {
			return -1;
		}
	} else {
		searchFromIndex = 0;
		if ((stopAtIndex > 0) && (stopAtIndex < j9vm_args->nOptions)) {
			searchToIndex = stopAtIndex - 1;
		} else {
			searchToIndex = j9vm_args->nOptions - 1;
		}
	}

	if (searchForward) {
		optionCntr = searchFromIndex;
	} else {
		optionCntr = searchToIndex;
	}

	while ((optionCntr <= searchToIndex) && (optionCntr >= searchFromIndex)) {
		testString = getOptionString(j9vm_args, optionCntr);				/* may return mapped value */
		actualString = j9vm_args->actualVMArgs->options[optionCntr].optionString;
		if (testString!=NULL) {
			success = FALSE;
			/* counts how many chars of testString match the chars of optionName - result is charCntr */
			for (charCntr = 0; testString[charCntr] && (testString[charCntr] == optionName[charCntr]); ++charCntr);
			switch(realMatch) {
				case EXACT_MATCH :
					success = ((testString[charCntr] == '\0') && (optionName[charCntr] == '\0'));
					break;
				case STARTSWITH_MATCH :											/* Will return true if exact match too */
					success = (charCntr == optionLength);
					break;
				case EXACT_MEMORY_MATCH:
				case OPTIONAL_LIST_MATCH :
					if (charCntr == optionLength) {										/* startswith or exact match */
						if (HAS_MAPPING(j9vm_args, optionCntr) && (realMatch == EXACT_MEMORY_MATCH)) {
							/* once we get here, we know we found the right index, but because the specified option and the mapped
								option may be different lengths we have to use the specified option length to correctly get the value. */
							IDATA mapnameLen = strlen(MAPPING_MAPNAME(j9vm_args, optionCntr));
							testChar = actualString[mapnameLen];
						} else {
							testChar = testString[charCntr];
						}
						if (realMatch == EXACT_MEMORY_MATCH) {
							success = (testChar >= '0' && testChar <= '9');
						} else
						if (realMatch == OPTIONAL_LIST_MATCH) {
							success = (testChar == '\0' || testChar == ':');
						}
					}
					break;
				case OPTIONAL_LIST_MATCH_USING_EQUALS:
					if (charCntr == optionLength) {
						testChar = testString[charCntr];
						success = (('\0' == testChar) || ('=' == testChar));
					}
					break;
			}
			if (success) {
				/* Search for a specific parameter after the optionName. eg. -Xverify:none. Only use with STARTSWITH_MATCH or OPTIONAL_LIST_MATCH. */
				/* Assumes that a value will terminate in a comma, space or EOL */
				/* Search is NOT case sensitive and also does not find substrings eg. searching for "no" in -Xverify:none will not succeed */
				if (optionValue && ((realMatch==STARTSWITH_MATCH) || (realMatch==OPTIONAL_LIST_MATCH))) {
					success = FALSE;
					optionValueOperations(PORTLIB, j9vm_args, optionCntr, GET_OPTION, &valueString, 0, ':', 0, NULL);		/* assumes ':' */
					if (valueString) {
						cursor = strrchr(valueString, ':');			/* Skip past any additional sub-options eg. -Xdump:java:<value> */
						if (NULL != cursor) {
							++cursor;
						} else {
							cursor = valueString;
						}
						while (cursor) {
							if (try_scan(&cursor, optionValue)) {		/* do a case-insensitive check of current option */
								endCharIsValid = (*cursor == '\0' || *cursor == '=' || *cursor == ',' || *cursor == ' ');
								if ((cursor >= valueString) && endCharIsValid) {
									success = TRUE;
									break;
								}
							}
							cursor = strchr(cursor, ',');
							if (NULL != cursor) {
								cursor += 1; /* skip to next option */
							}
						}
					}
				}
				if (success) {
					if (doConsumeArgs) {		/* If we are consuming args, we need to sweep the whole array for duplicates */
						if (hasBeenSet) {
							/* duplicate cmd-line value which is going to be ignored */
							j9vm_args->j9Options[optionCntr].flags = NOT_CONSUMABLE_ARG /* override any flags that already exist */
									| (j9vm_args->j9Options[optionCntr].flags & ARG_MEMORY_ALLOCATION);
						} else {
							j9vm_args->j9Options[optionCntr].flags |= ARG_CONSUMED;
						}
					} else {
						return optionCntr;		/* If we're not consuming args, no need to look further */
					}
					if (!hasBeenSet) {
						returnVal = optionCntr;
						hasBeenSet = TRUE;
					}
				}
			}
		}
		if (searchForward) {
			++optionCntr;
		} else {
			--optionCntr;
		}
	}
	return returnVal;
}

/* This function returns the option string at a given index.
	It is important to use this function rather than accessing the array directly, otherwise mappings will not work */

char*
getOptionString(J9VMInitArgs* j9vm_args, IDATA index) {
	if ( HAS_MAPPING(j9vm_args, index) ) {
		return MAPPING_J9NAME(j9vm_args, index);
	} else {
		return j9vm_args->actualVMArgs->options[index].optionString;
	}
}

/* Support function for optionValueOperations.
	Searches the entire command-line and "builds" an values list from a number of options. Required as mapped values need to break "duplicate" rules.
	Eg. -Xcomp means -Xjit:count=0. So, for "j9 -Xcomp -Xjit:lowopt", -Xcomp would be considered a duplicate. This is wrong.
	In this case, GET_COMPOUND should return "lowopt,count=0\0" and GET_COMPOUND_OPTS should return "lowopt\0count=0\0\0".
	Note that cmd-line options added as a consequence of environment variables are treated the same as mapped options. */

static IDATA
getCompoundOptions(J9PortLibrary *portLibrary, J9VMInitArgs* j9vm_args, IDATA element, IDATA action, char** returnString, UDATA bufSize, char delim, char separator) {
	IDATA bufferLeft = bufSize - ((action == GET_COMPOUND_OPTS) ? 1 : 0);		/* Extra NULL required */
	IDATA currElement, errorFound = 0;
	char* delimPos, *cursor;
	char optionSearchStr[LARGE_STRING_BUF_SIZE];
	char sepChar = (separator ? separator : ',');
	UDATA firstElementIsMapped = FALSE;
	PORT_ACCESS_FROM_PORT(portLibrary);

	memset(*returnString, 0, bufSize); 				/* Just to be sure! */
	/* Get first option value */
	if ((errorFound = optionValueOperations(PORTLIB, j9vm_args, element, GET_OPTION, returnString, bufferLeft, delim, 0, NULL)) != OPTION_OK) {
		return errorFound;
	}
	if ((bufferLeft -= (strlen(*returnString)+1)) < 0) {
		return OPTION_BUFFER_OVERFLOW;
	}
	if (element==0) goto allOptionsFound;			/* If we are the 0th element, don't bother to search for others */

	/* Find the name of the option to use as a search key for other options */
	if ( HAS_MAPPING(j9vm_args, element)) {
		strncpy(optionSearchStr, MAPPING_J9NAME(j9vm_args, element), (LARGE_STRING_BUF_SIZE - 1));
		firstElementIsMapped = TRUE;
	} else {
		strncpy(optionSearchStr, j9vm_args->actualVMArgs->options[element].optionString, (LARGE_STRING_BUF_SIZE-1));
		firstElementIsMapped = FROM_ENVVAR(j9vm_args, element);
	}
	if (!(delimPos = strchr(optionSearchStr, delim)))				/* If call is made to GET_COMPOUND on an option without the delimiter, we can't continue */
		return OPTION_ERROR;
	*(++delimPos) = '\0';

	/* Search for duplicate args in cmd line which may have values we want */
	currElement = element;
	while (currElement > 0) {
		currElement = findArgInVMArgs( PORTLIB, j9vm_args, (STARTSWITH_MATCH | (currElement << STOP_AT_INDEX_SHIFT)), optionSearchStr, NULL, FALSE);
		if (currElement < 0)
			break;

		/* If first element was mapped, find all other mapped values PLUS rightmost non-mapped value. Otherwise, find all mapped values. */
		if ( HAS_MAPPING(j9vm_args, currElement) || FROM_ENVVAR(j9vm_args, currElement) || firstElementIsMapped) {
			UDATA cursorLen, returnStrLen;

			if ((errorFound = optionValueOperations(PORTLIB, j9vm_args, currElement, GET_OPTION, &cursor, 0, delim, 0, NULL)) != OPTION_OK) {		/* get correct values string */
				return errorFound;
			}
			cursorLen = strlen(cursor);
			bufferLeft -= (cursorLen + 1);		/* +1 because of separator */

			/* insert at start of buffer and add separator */
			returnStrLen = strlen(*returnString) + (bufferLeft < 0 ? bufferLeft : 0);				/* If bufferLeft has gone negative, truncate end of string */
			memmove(*returnString + (cursorLen+1), *returnString, returnStrLen);				/* Shift right minus null-terminator. The buffer is zero'd so this is safe. */
			strncpy(*returnString, cursor, cursorLen);
			(*returnString)[cursorLen] = sepChar;

			if (firstElementIsMapped) {
				firstElementIsMapped = FALSE;
			}
			if (bufferLeft < 0) {
				return OPTION_BUFFER_OVERFLOW;
			}
		}
	}

	allOptionsFound :

	/* If GET_COMPOUND_OPTS, change all the separators to \0 */
	if (action == GET_COMPOUND_OPTS) {
		for (cursor = *returnString; *cursor; cursor++) {
			if (*cursor == ',') {
				*cursor = '\0';
			}
		}
		*(++cursor) = '\0';			/* Explicitly null-terminate "array" */
	}
	return OPTION_OK;
}

/* Covers the range of operations that can be performed on a given option string.
	GET_OPTION returns the value of a command-line option. Eg "-Xfoo:bar" returns "bar".
	GET_OPTION_OPT returns the value of a value. Eg. "-Xfoo:bar:wibble" returns "wibble".
	GET_OPTIONS returns a list of values separated by NULLs. Eg. "-Xfoo:bar=1,wibble=2" returns "bar=1\0wibble=2"
	GET_COMPOUND returns all the option values from the command-line for a given option, separated by commas. Based on certain rules.
	GET_COMPOUND_OPTS returns all the option values from the command-line for a given option, separated by \0s. Based on certain rules.
	GET_MAPPED_OPTION returns the value of a mapped option. Eg. if -Xsov1 maps to Xj91 then "-Xsov1:foo" returns "foo"
	GET_MEM_VALUE returns a rounded value of a memory option. Eg. "-Xfoo32k" returns (32 * 1024).
	GET_INT_VALUE returns the exact integer value of an option. Eg. -Xfoo5 returns 5
	GET_PRC_VALUE returns the a decimal value between 0 and 1 represented as a percentage. Eg. -Xmaxf1.0 returns 100.

	For GET_OPTIONS, the function expects to get a string buffer to write to and a buffer size.
	For GET_OPTION, GET_OPTION_OPT and GET_MAPPED_OPTION, the result is returned either as a pointer in valuesBuffer or, if valuesBuffer is a real buffer, it copies the result.
	For GET_MEM_VALUE, GET_INT_VALUE and GET_PRC_VALUE the function enters the result in a UDATA provided.

	Unless absolutely necessary, don't call this method directly - use the macros defined in jvminit.h.

	*** SEE testOptionValueOps FOR UNIT TESTS. TO RUN, ADD "#define JVMINIT_UNIT_TEST" TO SOURCE TEMPLATE ***
*/
IDATA
optionValueOperations(J9PortLibrary *portLibrary, J9VMInitArgs* j9vm_args, IDATA element, IDATA action, char** valuesBuffer, UDATA bufSize, char delim, char separator, void* reserved) {
	char *values, *scanStart;
	char *cursor;
	IDATA sepCount = 0;
	UDATA value, oldValue, done;
	IDATA errorFound = 0;
	UDATA mapType;
	double dvalue = 0.0;
	uintptr_t rc = 0;
	PORT_ACCESS_FROM_PORT(portLibrary);

	if (element < 0) {			/* If invalid element... don't even try */
		done = TRUE;
	} else {
		done = FALSE;
	}

	if (valuesBuffer) {
		if ((action == GET_MEM_VALUE) || (action == GET_INT_VALUE) || (action == GET_PRC_VALUE) || (action == GET_DBL_VALUE)) {
			if (!(*valuesBuffer)) {
				return OPTION_ERROR;					/* *valuesBuffer is an input value. Make sure it is not NULL. */
			}
		} else {
			if (bufSize > 0) {									/* bufSize > 0 indicates *valuesBuffer is a real buffer, which is return value*/
				if (*valuesBuffer) {
					memset(*valuesBuffer, 0, bufSize);
				} else {
					return OPTION_ERROR;
				}
			} else {
				*valuesBuffer = NULL;					/* valuesBuffer will return just a pointer. Init to NULL. */
			}
		}
	} else {
		return OPTION_ERROR;
	}

	while (!done) {
		switch (action) {

			case GET_OPTION :
				cursor = NULL;
				/* For EXACT_MAP_WITH_OPTIONS and MAP_ONE_COLON_TO_TWO no special mapping code is required */
				if (HAS_MAPPING(j9vm_args, element)) {
					if (!(MAPPING_FLAGS(j9vm_args, element) & (EXACT_MAP_WITH_OPTIONS | EXACT_MAP_NO_OPTIONS | MAP_ONE_COLON_TO_TWO))) {
						action = GET_MAPPED_OPTION;
						continue;
					}
					if (MAPPING_FLAGS(j9vm_args, element) & EXACT_MAP_NO_OPTIONS) {
						cursor = MAPPING_J9NAME(j9vm_args, element);
					}
				}
				if (!cursor)
					cursor = j9vm_args->actualVMArgs->options[element].optionString;
				if (cursor && (cursor = strchr(cursor, delim))) {
					if (bufSize > 0) {
						strncpy(*valuesBuffer, ++cursor, (bufSize-1));
						if (strlen(cursor) > (bufSize-1)) {
							return OPTION_BUFFER_OVERFLOW;
						}
					} else {
						*valuesBuffer = ++cursor;
					}
				}
				done = TRUE;
				break;

			case GET_OPTION_OPT :
				/* Note - currently won't work with MAP_WITH_INCLUSIVE_OPTIONS */
				if ((errorFound = optionValueOperations(PORTLIB, j9vm_args, element, GET_OPTION, &values, 0, delim, 0, NULL)) != OPTION_OK) {		/* get correct values string */
					return errorFound;
				}
				if (values && (cursor = strchr(values, separator))) {
					if (bufSize > 0) {
						strncpy(*valuesBuffer, ++cursor, (bufSize-1));
						if (strlen(cursor) > (bufSize-1)) {
							return OPTION_BUFFER_OVERFLOW;
						}
					} else {
						*valuesBuffer = ++cursor;
					}
				}
				done = TRUE;
				break;

			case GET_OPTIONS :
				/* Takes a buffer and copies in the values, each one separated by a single NULL. Should have at least \0\0 at the end, hence bufSize-1 */
				errorFound = optionValueOperations(PORTLIB, j9vm_args, element, GET_OPTION, valuesBuffer, bufSize-1, delim, 0, NULL);		/* get correct values string */
				if (*valuesBuffer) {
					cursor = *valuesBuffer;
					while (*cursor) {
						if (*cursor == separator) {
							*cursor = '\0';
						}
						++cursor;
					}
					*(++cursor) = '\0';		/* explicitly null-terminate the "array". There is no danger of walking beyond the buffer here because of bufSize-1 in call above. */
				}
				if (errorFound != OPTION_OK) {
					return errorFound;
				}
				done = TRUE;
				break;

			case GET_COMPOUND :
			case GET_COMPOUND_OPTS :
				if ((errorFound = getCompoundOptions(PORTLIB, j9vm_args, element, action, valuesBuffer, bufSize, delim, separator)) != OPTION_OK) {
					return errorFound;
				}
				done = TRUE;
				break;

			case GET_MAPPED_OPTION :
				if ( !HAS_MAPPING(j9vm_args, element) )
					return OPTION_ERROR;
				mapType = MAPPING_FLAGS(j9vm_args, element);
				switch (mapType) {
					case INVALID_OPTION :
					case STARTSWITH_MAP_NO_OPTIONS :
						break;
					case MAP_TWO_COLONS_TO_ONE :
						cursor = j9vm_args->actualVMArgs->options[element].optionString;
						cursor = strchr(cursor, ':');
						if (NULL != cursor) {
							cursor = strchr(++cursor, ':');
							if (NULL != cursor) {
								if (bufSize > 0) {
									strncpy(*valuesBuffer, ++cursor, (bufSize-1));
									if (strlen(cursor) > (bufSize-1)) {
										return OPTION_BUFFER_OVERFLOW;
									}
								} else {
									*valuesBuffer = ++cursor;
								}
							}
						}
						break;
					case MAP_WITH_INCLUSIVE_OPTIONS :
						/* Has to build the result from part of the j9Name and then the actual value on the command-line */
						if (bufSize > 0) {
							char* mapName = MAPPING_MAPNAME(j9vm_args, element);
							/* To get the value of the *actual* arg specified on the command-line, the delimiter may not be the one specified */
							char delimChar = mapName[strlen(mapName)-1];
							char* j9Name = MAPPING_J9NAME(j9vm_args, element);
							char* includeStr = strchr(j9Name, ':');
							char* actualOption = j9vm_args->actualVMArgs->options[element].optionString;
							IDATA bufferLeft = (bufSize-1);
							cursor = *valuesBuffer;
							if (includeStr) {
								strncpy(cursor, includeStr+1, bufferLeft);
								cursor += strlen(cursor);
							}
							if ((bufferLeft -= strlen(*valuesBuffer)) <= 0) {
								return OPTION_BUFFER_OVERFLOW;
							}
							if (actualOption && (actualOption = strchr(actualOption, delimChar))) {
								strncpy(cursor, ++actualOption, bufferLeft);
							}
							if (actualOption && (((IDATA)strlen(actualOption)) > bufferLeft)) {
								return OPTION_BUFFER_OVERFLOW;
							}
						} else {
							return OPTION_ERROR;			/* MAP_WITH_INCLUSIVE_OPTIONS cannot work without a buffer to build the result. Use COPY_OPTION_VALUE. */
						}
						break;
				}
				done = TRUE;
				break;

			case GET_DBL_VALUE:
				*(double *)reserved = 0.0;
				cursor = getStartOfOptionValue(j9vm_args, element, *valuesBuffer);
				rc = scan_double(&cursor, &dvalue);
				if (*cursor != '\0') {
					return OPTION_MALFORMED;
				}
				if (OPTION_OK != rc) {
					return rc;
				}
				*(double *)reserved = dvalue;
				done = TRUE;
				break;

			case GET_MEM_VALUE :
			case GET_INT_VALUE :
			case GET_PRC_VALUE :
				*((UDATA*)(reserved)) = 0;
				scanStart = getStartOfOptionValue(j9vm_args, element, *valuesBuffer);
				cursor = scanStart;
				if (scan_udata(&cursor, &value)) {
					return OPTION_MALFORMED;
				}

				/* For GET_INT_VALUE, operation is simple. Return value below or error. */
				if (action==GET_INT_VALUE) {
					if (*cursor != '\0') {
						return OPTION_MALFORMED;
					}
				} else
				/* GET_PRC_VALUE should return value as a percentage (100 * value) and start with zero. Used for options such as -Xmine. */
				if (action==GET_PRC_VALUE) {
					if (value > 1) {
						return OPTION_OVERFLOW;
					} else {
						char decimalSep = *cursor;
						IDATA intValue = value;
						value = 0;

						if (decimalSep == '.' || decimalSep == ',') {
							char floatingBuffer[3] = "00";
							char* floatingBufferPtr = floatingBuffer;
							UDATA i = 0;

							/* Advance the cursor past the decimal point */
							cursor++;

							/* Copy up to two digits after decimal point */
							for (i = 0; i < 2; i++) {
								if (*cursor >= '0' && *cursor <= '9') {
									floatingBuffer[i] = *cursor;
									cursor++;
								} else {
									break;
								}
							}

							if (scan_udata(&floatingBufferPtr, &value)) {
								return OPTION_MALFORMED;
							}
						}
						if (intValue == 1) {						/* Case for 1.0, cannot return 0! */
							if (value > 0) {
								return OPTION_OVERFLOW;
							}
							value = 100;
						}
					}
				} else {
					switch (*cursor) {
						case '\0':
							oldValue = value;
							value = (value +  sizeof(UDATA) - 1) & ~(sizeof(UDATA) - 1);		/* round to nearest pointer value */
							if (value < oldValue) return OPTION_OVERFLOW;
							break;
						case 'k':
						case 'K':
							if (value <= (((UDATA)-1) >> 10)) {
								value <<= 10;
							} else {
								return OPTION_OVERFLOW;
							}
							cursor++;
							break;
						case 'm':
						case 'M':
							if (value <= (((UDATA)-1) >> 20)) {
								value <<= 20;
							} else {
								return OPTION_OVERFLOW;
							}
							cursor++;
							break;
						case 'g':
						case 'G':
							if (value <= (((UDATA)-1) >> 30)) {
								value <<= 30;
							} else {
								return OPTION_OVERFLOW;
							}
							cursor++;
							break;
						default:
							return OPTION_MALFORMED;
					}
				}
				if ((*cursor != '\0')) {
					/* CMVC 146992: issue a warning so that the user knows they have bad options */
					char *cmdlineoption = *valuesBuffer;
					if ( HAS_MAPPING(j9vm_args, element) ) {
						/* If we have a mapping, then we ignore the option name passed in. Instead we want the one that corresponds to the actual cmd line */
						cmdlineoption = &(j9vm_args->actualVMArgs->options[element].optionString[ strlen(MAPPING_MAPNAME(j9vm_args, element)) ]);
					}
					j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_VMUTIL_MALFORMED_MEMORY_OPTION, cmdlineoption, cursor-scanStart, scanStart, cursor);
				}
				*((UDATA*)(reserved)) = value;				/* Cannot return IDATA, as values are UDATA, so return as reserved */
				done = TRUE;
				break;

			default :
				done = TRUE;		/* should never get here! */
		}		/* switch */
	}			/* while */

	return OPTION_OK;
}

static char * 
getStartOfOptionValue(J9VMInitArgs* j9vm_args, IDATA element, const char *optionName)
{
	const char *option = optionName;
	char *value = NULL;

	if (HAS_MAPPING(j9vm_args, element)) {
		option = MAPPING_MAPNAME(j9vm_args, element);
	}

	Assert_Util_true(NULL != option);
	
	value = j9vm_args->actualVMArgs->options[element].optionString + strlen(option);

	return value;
}

IDATA
addOptionsDefaultFile(J9PortLibrary * portLib, J9JavaVMArgInfoList *vmArgumentsList, char *optionsDirectory, UDATA verboseFlags)
{
	char optionsArgumentBuffer[sizeof(VMOPT_XOPTIONSFILE_EQUALS)+MAX_PATH + sizeof(OPTIONS_DEFAULT) + 1]; /* add an extra byte to detect overflow */
	char* argString = NULL;
	size_t argumentLength = -1;
	UDATA resultLength  = 0;
	PORT_ACCESS_FROM_PORT(portLib);

	resultLength  = j9str_printf(PORTLIB, optionsArgumentBuffer, sizeof(optionsArgumentBuffer), VMOPT_XOPTIONSFILE_EQUALS "%s" DIR_SEPARATOR_STR OPTIONS_DEFAULT, optionsDirectory);
	if (resultLength > (sizeof(VMOPT_XOPTIONSFILE_EQUALS) + MAX_PATH)) {
		return -1; /* overflow */
	}
	return addXOptionsFile(portLib, optionsArgumentBuffer, vmArgumentsList, verboseFlags);
}

IDATA
addXjcl(J9PortLibrary * portLib, J9JavaVMArgInfoList *vmArgumentsList, UDATA j2seVersion)
{
	char *dllName = J9_JAVA_SE_DLL_NAME;
	size_t dllNameLength = sizeof(J9_JAVA_SE_DLL_NAME);
	size_t argumentLength = -1;
	char *argString = NULL;
	UDATA j2seReleaseValue = j2seVersion & J2SE_RELEASE_MASK;
	J9JavaVMArgInfo *optArg = NULL;
	
	PORT_ACCESS_FROM_PORT(portLib);
#ifdef J9VM_IVE_RAW_BUILD /* J9VM_IVE_RAW_BUILD is not enabled by default */
	Assert_Util_unreachable();
#endif /* J9VM_IVE_RAW_BUILD */

	argumentLength = sizeof(VMOPT_XJCL_COLON) + dllNameLength - 1; /* sizeof called twice, each includes the \0 */
	argString = j9mem_allocate_memory(argumentLength, OMRMEM_CATEGORY_VM);
	if (NULL == argString) {
		return -1;
	}
	j9str_printf(PORTLIB, argString, argumentLength, VMOPT_XJCL_COLON "%s", dllName);
	optArg = newJavaVMArgInfo(vmArgumentsList, argString, ARG_MEMORY_ALLOCATION | CONSUMABLE_ARG);
	if (NULL == optArg) {
		j9mem_free_memory(argString);
		return -1;
	}
	return 0;
}

IDATA
addBootLibraryPath(J9PortLibrary * portLib, J9JavaVMArgInfoList *vmArgumentsList, char *propertyNameEquals, char *j9binPath, char *jrebinPath)
{
	char *optionsArgumentBuffer = NULL;
	/* argumentLength include the space for the \0 because of the sizeof() */
	size_t argumentLength = 0;
	J9JavaVMArgInfo *optArg = NULL;
	PORT_ACCESS_FROM_PORT(portLib);

	argumentLength = strlen(propertyNameEquals) + strlen(j9binPath) + sizeof(J9JAVA_PATH_SEPARATOR) + strlen(jrebinPath); /* the sizeof operation adds the space for the terminating null */
	optionsArgumentBuffer = j9mem_allocate_memory(argumentLength, OMRMEM_CATEGORY_VM);

	if (NULL == optionsArgumentBuffer) {
		return -1;
	}

	j9str_printf(PORTLIB, optionsArgumentBuffer, argumentLength, "%s%s" J9JAVA_PATH_SEPARATOR "%s", propertyNameEquals, j9binPath, jrebinPath);

	optArg = newJavaVMArgInfo(vmArgumentsList, optionsArgumentBuffer, ARG_MEMORY_ALLOCATION|CONSUMABLE_ARG);
	if (NULL == optArg) {
		j9mem_free_memory(optionsArgumentBuffer);
		return -1;
	}
	return 0;
}

#define MAX_LIBPATH_SUBSTRINGS 16
/**
 * construct an argument -Djava.library.path= +
 * 	- path to jre/bin, then
 * Windows:
 *	- the system directory as returned from getSystemDirectory()
 *	- the Windows directory as returned from getWindowsDirectory()
 *	- the PATH variable
 *	- the current directory
 * AIX:
 * 	- contents of LIBPATH variable
 * 	- contents of LD_LIBRARY_PATH variable
 * 	- /usr/lib
 * Linux:
 * 	- same as AIX, but without LIBPATH
 * z/OS:
 * 	- contents of LIBPATH variable
 * 	- /usr/lib
 * z/TPF:
 * 	- same as Linux
 */
IDATA
addJavaLibraryPath(J9PortLibrary * portLib, J9JavaVMArgInfoList *vmArgumentsList,
		UDATA argEncoding, BOOLEAN jvmInSubdir, char *j9binPath, char *jrebinPath,
		const char *libpathValue, const char *ldLibraryPathValue)
{
	char *substringBuffer[MAX_LIBPATH_SUBSTRINGS];
	BOOLEAN allocated[MAX_LIBPATH_SUBSTRINGS] = {FALSE};
	char *pathBuffer = NULL;
	int substringIndex = 0;
	J9JavaVMArgInfo *optArg = NULL;
	size_t substringLength = 0;
	char *optionsArgumentBuffer = NULL;
	IDATA status = -1;

	PORT_ACCESS_FROM_PORT(portLib);
	substringBuffer[substringIndex] = JAVA_LIB_PATH_EQUALS;
	substringIndex += 1;
	substringLength += strlen(JAVA_LIB_PATH_EQUALS);

	substringBuffer[substringIndex] = j9binPath;
	substringIndex += 1;
	substringLength += strlen(j9binPath);

	if (jvmInSubdir) {
		substringBuffer[substringIndex] = J9JAVA_PATH_SEPARATOR;
		substringIndex += 1;
		substringLength += strlen(J9JAVA_PATH_SEPARATOR);
		substringBuffer[substringIndex] = jrebinPath;
		substringIndex += 1;
		substringLength += strlen(jrebinPath);
	}
#ifdef WIN32
	{
		char dir[MAX_PATH];
		wchar_t unicodeDir[MAX_PATH];
		UDATA rc = 0;
		BOOL isWow64 = FALSE;

		/* CMVC 177267 : Append system and windows directory */
		memset(unicodeDir, 0, MAX_PATH);
		IsWow64Process(GetCurrentProcess(), &isWow64);
		if (TRUE == isWow64) {
			rc = GetSystemWow64DirectoryW(unicodeDir, MAX_PATH);
		} else {
			rc = GetSystemDirectoryW(unicodeDir, MAX_PATH);
		}
		if (0 != rc) {
			size_t dirLen = 0;

			memset(dir, 0, MAX_PATH);
			WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, unicodeDir, -1, dir, MAX_PATH, NULL, NULL);
			dirLen = strlen(dir) + 1;
			pathBuffer = j9mem_allocate_memory(dirLen, OMRMEM_CATEGORY_VM);
			if (NULL == pathBuffer) {
				substringBuffer[substringIndex] = NULL; /* null terminate the list */
				goto freeBuffers;
			}
			strncpy(pathBuffer, dir, dirLen);
			substringBuffer[substringIndex] = J9JAVA_PATH_SEPARATOR;
			substringIndex += 1;
			substringLength += strlen(J9JAVA_PATH_SEPARATOR);
			substringBuffer[substringIndex] = pathBuffer;
			allocated[substringIndex] = TRUE;
			substringLength += (dirLen-1); /* remove the null */
			substringIndex;
			substringIndex += 1;
		}
		memset(unicodeDir, 0, MAX_PATH);
		if (0 != GetWindowsDirectoryW(unicodeDir, MAX_PATH)) {
			size_t dirLen = 0;

			memset(dir, 0, MAX_PATH);
			WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, unicodeDir, -1, dir, MAX_PATH, NULL, NULL);
			dirLen = (strlen(dir) + 1);
			pathBuffer = j9mem_allocate_memory(dirLen, OMRMEM_CATEGORY_VM);
			if (NULL == pathBuffer) {
				substringBuffer[substringIndex] = NULL; /* null terminate the list */
				goto freeBuffers;
			}
			strncpy(pathBuffer, dir, dirLen);
			substringBuffer[substringIndex] = J9JAVA_PATH_SEPARATOR;
			substringIndex += 1;
			substringLength += strlen(J9JAVA_PATH_SEPARATOR);
			substringBuffer[substringIndex] = pathBuffer;
			allocated[substringIndex] = TRUE;
			substringLength += (dirLen - 1); /* remove the null */
			substringIndex;
			substringIndex += 1;
		}
	}

	pathBuffer = j9mem_allocate_memory(J9_MAX_ENV, OMRMEM_CATEGORY_VM);
	if (NULL == pathBuffer) {
		substringBuffer[substringIndex] = NULL; /* null terminate the list */
		goto freeBuffers;
	}
	memset(pathBuffer, 0, J9_MAX_ENV);
	if (0 == j9sysinfo_get_env("PATH", pathBuffer, J9_MAX_ENV)) {
		substringBuffer[substringIndex] = J9JAVA_PATH_SEPARATOR;
		substringIndex += 1;
		substringLength += strlen(J9JAVA_PATH_SEPARATOR);
		substringBuffer[substringIndex] = pathBuffer;
		allocated[substringIndex] = TRUE;
		substringLength += strlen(pathBuffer);
		substringIndex;
		substringIndex += 1;
	}

#else /* defined(J9UNIX) || defined(J9ZOS390) */
	if (NULL != libpathValue) {
		size_t pathLen = strlen(libpathValue) + 1;
		pathBuffer = j9mem_allocate_memory(pathLen, OMRMEM_CATEGORY_VM);
		if (NULL == pathBuffer) {
			substringBuffer[substringIndex] = NULL; /* null terminate the list */
			goto freeBuffers;
		}

		strcpy(pathBuffer, libpathValue);
		substringBuffer[substringIndex] = J9JAVA_PATH_SEPARATOR;
		substringIndex += 1;
		substringLength += strlen(J9JAVA_PATH_SEPARATOR);
		substringBuffer[substringIndex] = pathBuffer;
		allocated[substringIndex] = TRUE;
		substringLength += strlen(pathBuffer);
		substringIndex += 1;
	}

	if (NULL != ldLibraryPathValue) {
		size_t pathLen = strlen(ldLibraryPathValue) + 1;
		pathBuffer = j9mem_allocate_memory(pathLen, OMRMEM_CATEGORY_VM);
		if (NULL == pathBuffer) {
			substringBuffer[substringIndex] = NULL; /* null terminate the list */
			goto freeBuffers;
		}

		strcpy(pathBuffer, ldLibraryPathValue);
		substringBuffer[substringIndex] = J9JAVA_PATH_SEPARATOR;
		substringIndex += 1;
		substringLength += strlen(J9JAVA_PATH_SEPARATOR);
		substringBuffer[substringIndex] = pathBuffer;
		allocated[substringIndex] = TRUE;
		substringLength += strlen(pathBuffer);
		substringIndex += 1;
	}
#endif /* defined(J9UNIX) || defined(J9ZOS390) */

#if defined(J9UNIX)
	/* On OSX, /usr/lib64 doesn't exist. Only /usr/lib needs to be appended on OSX. */
#if defined(J9VM_ENV_DATA64) && !defined(OSX)
	/* JAZZ103 117105: 64-bit JDKs on Linux and AIX should add /usr/lib64 to java.library.path ahead of /usr/lib. */
#define USRLIB64 ":/usr/lib64"
	substringBuffer[substringIndex] = USRLIB64;
	substringIndex += 1;
	substringLength += (sizeof(USRLIB64) - 1) ;
#undef USRLIB64
#endif /* defined(J9VM_ENV_DATA64) && !defined(OSX) */

	substringBuffer[substringIndex] = ":/usr/lib";
	substringIndex += 1;
	substringLength += strlen(":/usr/lib");
#endif /* defined(J9UNIX) */
#ifdef WIN32
	/* CMVC 177267, RTC 87362 : On windows, current directory is added at the end */
	substringBuffer[substringIndex] = ";.";
	substringIndex += 1;
	substringLength += strlen(";.");

#endif
	substringBuffer[substringIndex] = NULL; /* null terminate the list */
	optionsArgumentBuffer = j9mem_allocate_memory(substringLength + 1, OMRMEM_CATEGORY_VM);
	if (NULL == optionsArgumentBuffer) {
		goto freeBuffers;
	}
	*optionsArgumentBuffer = '\0';

	Assert_Util_true(substringIndex < MAX_LIBPATH_SUBSTRINGS); /* programming error */
	/* note that the buffer size is calculated from the sizes of the substrings. In this case strcat() cannot overflow. */
	for (substringIndex = 0; NULL != substringBuffer[substringIndex]; ++substringIndex) {
		strcat(optionsArgumentBuffer, substringBuffer[substringIndex]);
	}

	optArg = newJavaVMArgInfo(vmArgumentsList, optionsArgumentBuffer, ARG_MEMORY_ALLOCATION|CONSUMABLE_ARG);
	if (NULL == optArg) {
		j9mem_free_memory(optionsArgumentBuffer);
		goto freeBuffers;
	}
	status = 0;

freeBuffers:
	for (substringIndex = 0; NULL != substringBuffer[substringIndex]; ++substringIndex) {
		if (allocated[substringIndex]) {
			j9mem_free_memory(substringBuffer[substringIndex]);
		}
	}

	return status;
}

IDATA
addJavaHome(J9PortLibrary *portLib, J9JavaVMArgInfoList *vmArgumentsList, UDATA altJavaHomeSpecified, char *jrelibPath)
{
	char *javaHomeEnd = strrchr(jrelibPath, DIR_SEPARATOR);
	char *optionsArgumentBuffer = NULL;
	size_t pathLength = -1;
	J9JavaVMArgInfo *optArg = NULL;
	PORT_ACCESS_FROM_PORT(portLib);

#ifdef WIN32
	if (altJavaHomeSpecified) {
		wchar_t unicodeTemp[MAX_PATH];
		char altJavaHomeBuffer[MAX_PATH];
		size_t argumentLength = 0;

		memset(unicodeTemp, 0, sizeof(unicodeTemp));	/* Safety, since we're not checking the returned sizes */
		GetEnvironmentVariableW(ALT_JAVA_HOME_DIR_STR, unicodeTemp, MAX_PATH);
		memset(altJavaHomeBuffer, 0, sizeof(altJavaHomeBuffer));	/* Safety, since we're not checking the returned sizes */
		pathLength = WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, unicodeTemp, -1, altJavaHomeBuffer, MAX_PATH, NULL, NULL);
		if (0 == pathLength) {
			return -1;
		}

		argumentLength = sizeof(JAVA_HOME_EQUALS) + pathLength + strlen(J9JAVA_PATH_SEPARATOR) + strlen(jrelibPath);

		optionsArgumentBuffer = j9mem_allocate_memory(argumentLength, OMRMEM_CATEGORY_VM);
		if (NULL == optionsArgumentBuffer) {
			return -1;
		}
		j9str_printf(PORTLIB, optionsArgumentBuffer, argumentLength, JAVA_HOME_EQUALS "%s" J9JAVA_PATH_SEPARATOR "%s", altJavaHomeBuffer, jrelibPath);
	} else
#endif /* WIN32 */
	{
		if (NULL == javaHomeEnd) {
			pathLength = strlen(jrelibPath) + sizeof(DIR_SEPARATOR_STR "..") - 1;
		} else {
			pathLength = javaHomeEnd - jrelibPath;
		}

		optionsArgumentBuffer = j9mem_allocate_memory(sizeof(JAVA_HOME_EQUALS) + pathLength, OMRMEM_CATEGORY_VM);
		if (NULL == optionsArgumentBuffer) {
			return -1;
		}

		strcpy(optionsArgumentBuffer, JAVA_HOME_EQUALS);
		if (NULL == javaHomeEnd) {
			strcat(optionsArgumentBuffer, jrelibPath);
			strcat(optionsArgumentBuffer, DIR_SEPARATOR_STR "..");
		} else {
			memcpy(optionsArgumentBuffer + sizeof(JAVA_HOME_EQUALS) - 1, jrelibPath, pathLength);
			*(optionsArgumentBuffer + sizeof(JAVA_HOME_EQUALS) + pathLength - 1) = '\0';
		}
	}

	optArg = newJavaVMArgInfo(vmArgumentsList, optionsArgumentBuffer, ARG_MEMORY_ALLOCATION | CONSUMABLE_ARG);
	if (NULL == optArg) {
		j9mem_free_memory(optionsArgumentBuffer);
		return -1;
	}
	return 0;
}

IDATA
addExtDir(J9PortLibrary *portLib, J9JavaVMArgInfoList *vmArgumentsList, char *jrelibPath, JavaVMInitArgs *launcherArgs, UDATA j2seVersion)
{
	char *javaHomeEnd = strrchr(jrelibPath, DIR_SEPARATOR);
	size_t argumentLength = 1; /* add the \0 */
	char *optionsArgumentBuffer = NULL;
	J9JavaVMArgInfo *optArg = NULL;
	const char *libExt = DIR_SEPARATOR_STR "lib" DIR_SEPARATOR_STR "ext";
	I_32 optIndex = 0;
	PORT_ACCESS_FROM_PORT(portLib);

	Assert_Util_notNull(javaHomeEnd);

	argumentLength += strlen(JAVA_EXT_DIRS_EQUALS);
	argumentLength += javaHomeEnd - jrelibPath;
	argumentLength += strlen(libExt);

	optionsArgumentBuffer = j9mem_allocate_memory(argumentLength, OMRMEM_CATEGORY_VM);
	if (NULL == optionsArgumentBuffer) {
		return -1;
	}

	/*
	 * strcpy is safe because we allocated the buffer based on the size of the strings.
	 * Also strncpy null-pads the buffer.
	 */
	strcpy(optionsArgumentBuffer, JAVA_EXT_DIRS_EQUALS);
	/* need to use strncat because we are copying a substring */
	strncat(optionsArgumentBuffer, jrelibPath, javaHomeEnd - jrelibPath);
	strcat(optionsArgumentBuffer, libExt);

	optArg = newJavaVMArgInfo(vmArgumentsList, optionsArgumentBuffer, ARG_MEMORY_ALLOCATION | CONSUMABLE_ARG);
	if (NULL == optArg) {
		j9mem_free_memory(optionsArgumentBuffer);
		return -1;
	}

	return 0;
}

IDATA
addUserDir(J9PortLibrary * portLib, J9JavaVMArgInfoList *vmArgumentsList, char *cwd) 
{
	size_t argumentLength = 1; /* space for the \0 */
	char *optionsArgumentBuffer = NULL;
	J9JavaVMArgInfo *optArg = NULL;
	PORT_ACCESS_FROM_PORT(portLib);

	argumentLength += strlen(JAVA_USER_DIR_EQUALS);
	argumentLength += strlen(cwd);
	optionsArgumentBuffer = j9mem_allocate_memory(argumentLength, OMRMEM_CATEGORY_VM);

	if (NULL == optionsArgumentBuffer) {
		return -1;
	}

	j9str_printf(PORTLIB, optionsArgumentBuffer, argumentLength, JAVA_USER_DIR_EQUALS "%s", cwd);

	optArg = newJavaVMArgInfo(vmArgumentsList, optionsArgumentBuffer, ARG_MEMORY_ALLOCATION|CONSUMABLE_ARG);
	if (NULL == optArg) {
		j9mem_free_memory(optionsArgumentBuffer);
		return -1;
	}
	return 0;

}

#if !defined(OPENJ9_BUILD)
/* Function reads the J9NLS_J2SE_EXTRA_OPTIONS to get a -D define to set the IBM java version.
 * This isn't needed in OpenJ9 and using this is one of the items that forces the NLS message
 * catalogs to be parsed  early in startup
 *
 * Disable for OpenJ9 but leave in place for IBM to be handled in a separate cleanup item
 */
IDATA
addJavaPropertiesOptions(J9PortLibrary * portLib, J9JavaVMArgInfoList *vmArgumentsList, UDATA verboseFlags)
{
	char* msgBuffer = NULL;
	J9JavaVMArgInfo *optArg = NULL;
	UDATA atBeginning = TRUE;
	char* cursor = NULL;
	const char* message = NULL;

	PORT_ACCESS_FROM_PORT(portLib);

	message = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_J2SE_EXTRA_OPTIONS, "");
	/* this may be null in raw specs */
	if ((0 == strlen(message)) || ('\n' == message[0])) {
		return 0;
	}
	msgBuffer = j9mem_allocate_memory(strlen(message) + 1 , OMRMEM_CATEGORY_VM);
	if (NULL == msgBuffer) {
		return -1;
	}
	strcpy(msgBuffer, message);
	for (cursor = msgBuffer; *cursor != '\0'; cursor++) {
		if (atBeginning) {
			optArg = newJavaVMArgInfo(vmArgumentsList, cursor, CONSUMABLE_ARG);
			if (NULL == optArg) {
				return -1;
			}
			if (cursor == msgBuffer) {
				optArg->cmdLineOpt.flags |= ARG_MEMORY_ALLOCATION; /* mark the first argument as being the pointer to free */
			}
		}
		if (*cursor == '\n') {
			*cursor = '\0'; /* split the buffer into strings */
			atBeginning = TRUE;
		} else {
			atBeginning = FALSE;
		}
	}
	if (NULL == optArg) { /* no valid options in the the buffer */
		j9mem_free_memory(msgBuffer);
	}
	return 0;
}
#endif /* !OPENJ9_BUILD */

/*
 * parseOptionsFileText() removes tabs and spaces following a newline.
 * Replace a continuation followed by whitespace (sequence of tabs or spaces) with
 * the same whitespace followed by a backslash, a newline and zero or one tabs or spaces.
 * This causes parseOptionsFileText() to preserve the whitespace and discard the
 * continuation.
 */
static char *
adjustNewline(char* continuationStart, U_32 continuationLength) {
	char *whitespaceEnd = continuationStart + continuationLength;
	char testChar = *whitespaceEnd; /* Points to character after the last tab or space */
	char *newlinePosition = NULL;
	Assert_Util_true(continuationLength > 1); /* programming error */
	while (('\0' != testChar) && (('\t' == testChar) || (' ' == testChar))) {
		whitespaceEnd += 1;
		testChar = *whitespaceEnd;
	}
	/* slide the whitespace from the next line to the end of the current line */
	memmove(continuationStart, continuationStart + continuationLength,  whitespaceEnd - (continuationStart + continuationLength));
	/* and put  dummy newline after it */
	newlinePosition = whitespaceEnd - continuationLength;
	newlinePosition[0] = '\\';
	newlinePosition[1] = '\n';
	return whitespaceEnd;
}

#define MANIFEST_FILE "META-INF/MANIFEST.MF"
#define IBM_JAVA_OPTIONS  "IBM-Java-Options"

IDATA
addJarArguments(J9PortLibrary * portLib, J9JavaVMArgInfoList *vmArgumentsList, const char *jarPath, J9ZipFunctionTable *zipFuncs, UDATA verboseFlags)
{

	I_32 status = 0;
	PORT_ACCESS_FROM_PORT(portLib);

	if (NULL != jarPath) {
		J9ZipFile zipFile;
		U_32 zipEntryFlags = J9ZIP_GETENTRY_READ_DATA_POINTER;

		status = zipFuncs->zip_openZipFile(portLib, (char *) jarPath, &zipFile, NULL, J9ZIP_OPEN_NO_FLAGS);
		if (ZIP_ERR_UNKNOWN_FILE_TYPE == status) {
			status = zipFuncs->zip_openZipFile(portLib, (char *) jarPath, &zipFile, NULL, J9ZIP_OPEN_ALLOW_NONSTANDARD_ZIP);
			zipEntryFlags |= J9ZIP_GETENTRY_USE_CENTRAL_DIRECTORY;
		}

		if (0 == status) {
			J9ZipEntry entry;
			U_32 dataBufferSize = 0;
			U_8 *dataBuffer = NULL; /* executable JARs tend to have huge manifests, so go straight to a heap-allocated buffer */
			status = zipFuncs->zip_getZipEntry(portLib, &zipFile, &entry, MANIFEST_FILE, sizeof(MANIFEST_FILE) - 1, zipEntryFlags);
			if (0 != status) {
				zipFuncs->zip_releaseZipFile(PORTLIB, &zipFile);
				status = OMRPORT_ERROR_INVALID;
			} else {
				dataBufferSize = entry.uncompressedSize;
				/*
				 * We may read past the actual end of the data while searching for nulls or line breaks.
				 *  Allocate extra space so the accesses are safe.
				 */
				dataBuffer = j9mem_allocate_memory(dataBufferSize + 3, J9MEM_CATEGORY_VM);
				if (NULL == dataBuffer) {
					status = J9PORT_ERROR_STRING_MEM_ALLOCATE_FAILED;
					zipFuncs->zip_releaseZipFile(PORTLIB, &zipFile);
				} else {
					status = zipFuncs->zip_getZipEntryData(portLib, &zipFile, &entry, dataBuffer, dataBufferSize);
					dataBuffer[dataBufferSize] = '\0';

					zipFuncs->zip_freeZipEntry(portLib, &entry);
					zipFuncs->zip_releaseZipFile(PORTLIB, &zipFile);
					if (0 == status) {
						char *cursor = (char*)dataBuffer;
						char *argStart = cursor;
						BOOLEAN foundStart = FALSE;
						do {
							/*
							 * Find the manifest section
							 * A manifest section starts with the header name at the start of a line.
							 * newline:                      CR LF | LF | CR (not followed by LF)
							 */
							argStart = strstr(cursor, IBM_JAVA_OPTIONS);
							if (argStart == (char *)dataBuffer) {
								argStart = NULL;  /* illegal position.  Just ignore it. */
							}
							if (NULL != argStart) {
								/* verify that the search string starts on a section.  Note that the main section precedes this so backing up is safe. */
								char nl0 = *(argStart - 1);
								if (('\r' == nl0) || ('\n' == nl0)) { /* CR, LF or CRLF  */
									foundStart = TRUE;
								} else {
									cursor += (sizeof(IBM_JAVA_OPTIONS) - 1);
								}
							}
						} while (!foundStart && (NULL != argStart)) ;

						if (foundStart) {
							BOOLEAN foundEnd = FALSE;
							char * argEnd = NULL;
							char testChar = '\0';

							argStart += (sizeof(IBM_JAVA_OPTIONS) - 1);
							testChar = *argStart;
							while ((':' != testChar) && ('\0' != testChar) && ('\n' != testChar) &&('\r' != testChar)) {
								argStart += 1;
								testChar = *argStart;
							}
							if (':' == testChar) {
								argStart += 1;
							} else {
								/* format error */
								status = OMRPORT_ERROR_INVALID;
							}
							argEnd = argStart;
							while (!foundEnd) {
								char nl0 = *argEnd;
								char nl1 ='\0';
								char nl2 ='\0';

								while (('\0' != nl0 ) && ('\n' != nl0 ) &&('\r' != nl0 )) {
									argEnd += 1;
									nl0 = *argEnd;
								}
								nl1 = *(argEnd + 1);
								nl2 = *(argEnd + 2);
								if (('\0' == nl0) || ('\0' == nl1) || ('\0' == nl2)) { /* end of the buffer*/
									foundEnd = TRUE;
								}
								/*
								 * The  attribute is terminated by a newline followed by a non-space character.
								 * Manifest generators may insert header continuations (newline+space) at any point.
								 * parseOptionsFileText() will eliminate whitespace (a sequence of spaces and tab) at the beginning of a line.
								 * Move the newline so that the whitespace in the original text is preserved.
								 */
								if ((('\n' == nl0) || ('\r' == nl0)) && (' ' == nl1)) { /* CR or LF followed by space */
									argEnd = adjustNewline(argEnd, 2);
								} else if (('\r' == nl0) && ('\n' == nl1) && (' ' == nl2)) { /* CRLF followed by space */
									argEnd = adjustNewline(argEnd, 3);
								} else {
									foundEnd = TRUE;
								}
							}
							*argEnd = '\0';
							if (parseOptionsFileText(portLib, argStart, vmArgumentsList, verboseFlags) < 0) {
								status = OMRPORT_ERROR_INVALID;
							}
						}
					}
					j9mem_free_memory(dataBuffer);
				}
			}
		}
	}
	return status;
}

/**
 * Add the argument string corresponding to an environment variable to the list
 * @param portLibrary port library
 * @param envVar string literal for the source environment variable
 * @param j9opt string literal for the argument to create
 * @param mapType indicates if the variable is a boolean flag or contains data
 * @param vmArgumentsList current list of arguments
 * @param verboseFlags set to VERBOSE_INIT for verbosity
 * @return 0 on success, negative on failure
 */
static IDATA
mapEnvVarToArgument(J9PortLibrary* portLibrary, const char* envVar, const char* j9opt, J9JavaVMArgInfoList *vmArgumentsList, UDATA mapType, UDATA verboseFlags) 
{
	J9JavaVMArgInfo *optArg = NULL;
	IDATA valueSize = 0;

	PORT_ACCESS_FROM_PORT(portLibrary);

	valueSize = j9sysinfo_get_env(envVar, NULL, 0);
	/*
	 * PR 75469
	 * If the envVar isn't defined, simply return.
	 * If the option requires a value but the value is a null string, also ignore it.
	 */
	if (valueSize <= ((EXACT_MAP_WITH_OPTIONS == mapType)? 1: 0)) {
		return 0;
	}

	JVMINIT_VERBOSE_INIT_TRACE2(verboseFlags, "Mapping environment variable %s to command-line option %s\n", envVar, j9opt);

	optArg = newJavaVMArgInfo(vmArgumentsList, NULL, CONSUMABLE_ARG);
	if (NULL == optArg) {
		return -1;
	}
	optArg->cmdLineOpt.fromEnvVar = (char *) envVar;
	switch (mapType) {

	/* Maps an environment variable to an exact string, ignoring the value: eg. IBM_FOO_BAR=1 maps to -Xj9foobar */
	case EXACT_MAP_NO_OPTIONS:
		optArg->vmOpt.optionString = (char *) j9opt;
		break;

	/* Maps an environment variable and its options to a command-line option: eg. IBM_FOO_BAR=20 maps to -Xj9foobar=20 */
	case EXACT_MAP_WITH_OPTIONS:
	{
		char *argumentBuffer = NULL;
		char *cursor = NULL;
		size_t j9optLength = strlen(j9opt);
		size_t bufferSize = 0;

		bufferSize = j9optLength + valueSize + 1;
		argumentBuffer = j9mem_allocate_memory(bufferSize, OMRMEM_CATEGORY_VM);
		if (NULL == argumentBuffer) {
			return -1;
		}
		cursor = argumentBuffer;
		memcpy(cursor, j9opt, j9optLength);
		cursor += j9optLength;
		j9sysinfo_get_env(envVar, cursor, valueSize);
		cursor += valueSize;
		*cursor = '\0';
		optArg->vmOpt.optionString = argumentBuffer;
		optArg->cmdLineOpt.flags |= ARG_MEMORY_ALLOCATION;
		break;
	} /* case EXACT_MAP_WITH_OPTIONS */
	default: Assert_Util_unreachable();
	}
	return 0;
}


static IDATA
addEnvironmentVariableArguments(J9PortLibrary* portLib, const char* envVarName, J9JavaVMArgInfoList *vmArgumentsList, UDATA verboseFlags) 
{
	PORT_ACCESS_FROM_PORT(portLib);
	IDATA envVarSize = j9sysinfo_get_env(envVarName, NULL, 0);
	char* argumentBuffer = NULL;

	if (envVarSize <= 0) {
		return 0; /* variable not defined or empty */
	}
	argumentBuffer = j9mem_allocate_memory(envVarSize, OMRMEM_CATEGORY_VM);
	if (NULL == argumentBuffer) {
		return -1;
	}
	JVMINIT_VERBOSE_INIT_TRACE1(verboseFlags, "Parsing environment variable %s\n", envVarName);
	j9sysinfo_get_env(envVarName, argumentBuffer, envVarSize);

	return parseOptionsBuffer(portLib, argumentBuffer, vmArgumentsList, verboseFlags, TRUE);
}

IDATA
addEnvironmentVariables(J9PortLibrary * portLib, JavaVMInitArgs *launcherArgs, J9JavaVMArgInfoList *vmArgumentsList, UDATA verboseFlags) 
{
	IDATA status = 0;
	if (
			(0 != mapEnvVarToArgument(portLib, ENVVAR_IBM_MIXED_MODE_THRESHOLD, MAPOPT_XJIT_COUNT_EQUALS, vmArgumentsList, EXACT_MAP_WITH_OPTIONS, verboseFlags))
			|| (0 != mapEnvVarToArgument(portLib, ENVVAR_JAVA_COMPILER, SYSPROP_DJAVA_COMPILER_EQUALS, vmArgumentsList, EXACT_MAP_WITH_OPTIONS, verboseFlags))
			|| (0 != mapEnvVarToArgument(portLib, ENVVAR_IBM_NOSIGHANDLER, VMOPT_XRS, vmArgumentsList, EXACT_MAP_NO_OPTIONS, verboseFlags))
#if defined(J9ZOS390)
			|| (0 != mapEnvVarToArgument(portLib, ENVVAR_JAVA_THREAD_MODEL, MAPOPT_XTHR_TW_EQUALS, vmArgumentsList, EXACT_MAP_WITH_OPTIONS, verboseFlags))
			|| (0 != mapEnvVarToArgument(portLib, ENVVAR_IBM_JAVA_ENABLE_ASCII_FILETAG, VMOPT_XASCII_FILETAG, vmArgumentsList, EXACT_MAP_NO_OPTIONS, verboseFlags))
#endif
			|| (0 != addEnvironmentVariableArguments(portLib, ENVVAR_JAVA_TOOL_OPTIONS, vmArgumentsList, verboseFlags))
			|| (0 != addEnvironmentVariableArguments(portLib, ENVVAR_OPENJ9_JAVA_OPTIONS, vmArgumentsList, verboseFlags))
			|| (0 != addEnvironmentVariableArguments(portLib, ENVVAR_IBM_JAVA_OPTIONS, vmArgumentsList, verboseFlags))
			|| (0 != mapEnvVarToArgument(portLib, ENVVAR_IBM_JAVA_JITLIB, MAPOPT_XXJITDIRECTORY, vmArgumentsList, EXACT_MAP_WITH_OPTIONS, verboseFlags))
	) {
		status = -1;
	}
	return status;
}


IDATA
addLauncherArgs(J9PortLibrary * portLib, JavaVMInitArgs *launcherArgs, UDATA launcherArgumentsSize, J9JavaVMArgInfoList *vmArgumentsList, char **xServiceBuffer, UDATA argEncoding, UDATA verboseFlags)
{
	/* allocate memory to hold all argument strings */
	char *optionsArgumentBuffer = NULL;
	char *cursor;
	size_t lastArgumentSize = 0;
	jint optInd = 0;
	UDATA convertEncoding = FALSE;

	PORT_ACCESS_FROM_PORT(portLib);

#ifdef WIN32
	if (ARG_ENCODING_UTF != argEncoding) {
		convertEncoding = TRUE;
	}
#endif
	if (FALSE == convertEncoding) {
		optionsArgumentBuffer = j9mem_allocate_memory(launcherArgumentsSize, OMRMEM_CATEGORY_VM);
		if (NULL == optionsArgumentBuffer) {
			return -1;
		}
	}
	*xServiceBuffer = NULL;
	cursor = optionsArgumentBuffer;
	JVMINIT_VERBOSE_INIT_TRACE1(verboseFlags, "Adding command line arguments\n", NULL);
	for (optInd = 0; optInd < launcherArgs-> nOptions; ++optInd) {
		UDATA xOptLen = strlen(VMOPT_XOPTIONSFILE_EQUALS);
		JavaVMOption *currentOpt = &(launcherArgs->options[optInd]);
		J9JavaVMArgInfo *optArg = NULL;
		char *optString = currentOpt->optionString;
		char *transcodedString = optString;
		UDATA consumableFlag = CONSUMABLE_ARG;

		Assert_Util_notNull(optString);
		if (0 == strcmp(optString, "-Xprod") ) {
			continue; /* legacy feature.  Retaining for compatibility */
		}
#ifdef WIN32
		if (TRUE == convertEncoding) {
			I_32 mutf8Size = 0;
			UDATA optStringSize = strlen(optString);
			/* Using the default ANSI code page for backward compatibility.  We should really be using the current ACP. */
			I_32 fromCode = J9STR_CODE_WINDEFAULTACP;

			switch (argEncoding) {
			case ARG_ENCODING_LATIN:
				fromCode = J9STR_CODE_LATIN1;
				break;
			case ARG_ENCODING_UTF:
				fromCode = J9STR_CODE_UTF8;
				break;
			case ARG_ENCODING_DEFAULT:
			default:
				fromCode = J9STR_CODE_WINDEFAULTACP;
			}

			mutf8Size = j9str_convert(fromCode, J9STR_CODE_MUTF8, (U_8 *) optString, optStringSize,
					NULL, 0);

			if (mutf8Size < 0) {
				switch (mutf8Size) {
				case J9PORT_ERROR_STRING_ILLEGAL_STRING:
				case J9PORT_ERROR_STRING_ICONV_OPEN_FAILED:
				case J9PORT_ERROR_STRING_MEM_ALLOCATE_FAILED:
					return -1; /* recoverable error */
					break;
				case J9PORT_ERROR_STRING_BUFFER_TOO_SMALL:
				case J9PORT_ERROR_STRING_UNSUPPORTED_ENCODING:
				default:
					Assert_Util_true(FALSE); /* programming error */
				}
			}
			++mutf8Size; /* leave enough space that j9str_convert will null-terminate */

			transcodedString = j9mem_allocate_memory(mutf8Size, OMRMEM_CATEGORY_VM);
			if (NULL == transcodedString) {
				return -1;
			}

			mutf8Size = j9str_convert(fromCode, J9STR_CODE_MUTF8, (U_8 *) optString, optStringSize,
					transcodedString, mutf8Size);
			if (mutf8Size < 0) {
				switch (mutf8Size) {
				case J9PORT_ERROR_STRING_ICONV_OPEN_FAILED:
				case J9PORT_ERROR_STRING_MEM_ALLOCATE_FAILED:
					return -1; /* recoverable error */
					break;
				case J9PORT_ERROR_STRING_ILLEGAL_STRING: /* should have caught this on the first call */
				case J9PORT_ERROR_STRING_BUFFER_TOO_SMALL: /* should have allocated enough memory */
				case J9PORT_ERROR_STRING_UNSUPPORTED_ENCODING:
				default:
					Assert_Util_true(FALSE); /* programming error */
				}
			}
		}
#endif
		if (strncmp(optString, VMOPT_XOPTIONSFILE_EQUALS, xOptLen) == 0) {
			UDATA rc = addXOptionsFile(portLib, transcodedString, vmArgumentsList, verboseFlags);
#ifdef WIN32
			if (TRUE == convertEncoding) {
				/* addXOptionsFile makes a copy of the text */
				j9mem_free_memory(transcodedString);
			}
#endif
			if (0 != rc) {
				return -1;
			}
			continue;
		} else if (0 == strncmp(optString, VMOPT_XSERVICE_EQUALS, strlen(VMOPT_XSERVICE_EQUALS)) ) {
			*xServiceBuffer = transcodedString + strlen(VMOPT_XSERVICE_EQUALS);
		}

		if (TRUE == convertEncoding) {
			optArg = newJavaVMArgInfo(vmArgumentsList, transcodedString, ARG_MEMORY_ALLOCATION|consumableFlag);
		} else {

			strcpy(cursor, optString); /* safe because the lengths of all strings have been measured and included in argumentLength */
			optArg = newJavaVMArgInfo(vmArgumentsList, cursor, (consumableFlag | ((0 == optInd) ? ARG_MEMORY_ALLOCATION: 0)));
			/* the first argument points to the head of the buffer */
			cursor += (strlen(cursor) + 1);
		}
		if (NULL == optArg) {
			return -1;
		}
		optArg->vmOpt.extraInfo = currentOpt->extraInfo;
	}
	return 0;
}

IDATA
addXserviceArgs(J9PortLibrary * portLib, J9JavaVMArgInfoList *vmArgumentsList, char *xServiceBuffer, UDATA verboseFlags) 
{

	PORT_ACCESS_FROM_PORT(portLib);
	if (xServiceBuffer) {
		size_t xServiceLen = strlen(xServiceBuffer) + 1;
		char *argumentBuffer = j9mem_allocate_memory(xServiceLen, OMRMEM_CATEGORY_VM);
		if (NULL == argumentBuffer) {
			return -1;
		}
		memcpy(argumentBuffer, xServiceBuffer, xServiceLen);
		JVMINIT_VERBOSE_INIT_TRACE1(verboseFlags, "Parsing -Xservice argument %s\n", xServiceBuffer);
		if (0 != parseOptionsBuffer(portLib, argumentBuffer, vmArgumentsList, verboseFlags, TRUE)) {
			return -1;
		}
	}
	return 0;
}

J9VMInitArgs*
createJvmInitArgs(J9PortLibrary * portLib, JavaVMInitArgs *launcherArgs, J9JavaVMArgInfoList *vmArgumentsList, UDATA* argEncoding)
{
	jint numArgs = 0;
	UDATA memoryRequired = 0;
	JavaVMOption *javaVMOptionCursor = NULL;
	J9CmdLineOption *cmdLineOptionCursor = NULL;
	J9VMInitArgs *result = NULL;
	JavaVMInitArgs *initArgs;
	void *argsBuffer = NULL;
	J9JavaVMArgInfo *current;
	const size_t ARGENCODING_LENGTH = strlen(VMOPT_XARGENCODING);
	PORT_ACCESS_FROM_PORT(portLib);

	if (NULL != vmArgumentsList) {
		numArgs = (jint) pool_numElements(vmArgumentsList->pool);
	}

	memoryRequired = sizeof(J9VMInitArgs) + sizeof(JavaVMInitArgs) + numArgs*(sizeof(JavaVMInitArgs) + numArgs*sizeof(J9CmdLineOption));
	argsBuffer = j9mem_allocate_memory(memoryRequired, OMRMEM_CATEGORY_VM);
	if (NULL == argsBuffer) {
		return NULL;
	}

	/* create pointers into the buffer for the various datastructures and arrays */
	result = (J9VMInitArgs *) argsBuffer;
	initArgs = (JavaVMInitArgs *) (((U_8 *) result) + sizeof(*result));
	javaVMOptionCursor = (JavaVMOption *) (((U_8 *) initArgs) + sizeof(*initArgs));
	cmdLineOptionCursor = (J9CmdLineOption *) (((U_8 *) javaVMOptionCursor) + (numArgs * sizeof(*javaVMOptionCursor)));
	result->actualVMArgs = initArgs;
	result->nOptions = numArgs;
	result->j9Options = cmdLineOptionCursor;
	initArgs->nOptions = numArgs;
	initArgs->version = launcherArgs->version;
	initArgs->options = javaVMOptionCursor;
	initArgs->ignoreUnrecognized = launcherArgs->ignoreUnrecognized;
	if (NULL == vmArgumentsList) {
		return result;
	}
	current = vmArgumentsList->head;
	while (current) {
		JavaVMOption currOpt = current->vmOpt;
		if (NULL != currOpt.optionString) {
			/* CMVC 201804 - need to parse all sources for argument encoding so we can transcode system properties */
			const char *optString = currOpt.optionString;
			UDATA consumableFlagMask = 0;

			if (0 == strncmp(optString, VMOPT_XARGENCODING, ARGENCODING_LENGTH)) { /* -Xargencoding* */
				consumableFlagMask = 0;
				if ('\0' == optString[ARGENCODING_LENGTH]) { /* exact match to "-Xargencoding"*/
					*argEncoding = ARG_ENCODING_PLATFORM;
					consumableFlagMask = ~CONSUMABLE_ARG;
				} else if (':' == optString[ARGENCODING_LENGTH]) { /* -Xargencoding:* */
					consumableFlagMask = ~CONSUMABLE_ARG;
					if (0 == strcmp(optString, VMOPT_XARGENCODINGUTF8)) {
						*argEncoding = ARG_ENCODING_UTF;
					} else if (0 == strcmp(optString, VMOPT_XARGENCODINGLATIN)) {
						*argEncoding = ARG_ENCODING_LATIN;
					}
					/* silently ignore other encodings */
				}
			} else if (0 == strcmp(optString, VMOPT_XNOARGSCONVERSION)) {
				*argEncoding = ARG_ENCODING_LATIN;
				consumableFlagMask = ~CONSUMABLE_ARG;
			}
			if (0 != consumableFlagMask) {
				current->cmdLineOpt.flags &= consumableFlagMask; /* mark the option as consumed */
			}
		}
		*javaVMOptionCursor = currOpt;
		javaVMOptionCursor += 1;
		*cmdLineOptionCursor = current->cmdLineOpt;
		cmdLineOptionCursor += 1;
		current = current->next;
	}
	return result;
}

void
destroyJvmInitArgs(J9PortLibrary * portLib, J9VMInitArgs *vmArgumentsList)
{
	UDATA i;
	JavaVMInitArgs *actualArgs = NULL;
	PORT_ACCESS_FROM_PORT(portLib);

	if (NULL == vmArgumentsList) {
		return;
	}
	actualArgs = vmArgumentsList->actualVMArgs;
	for (i = 0; i < vmArgumentsList->nOptions; ++i) {
		J9CmdLineOption* j9Option = &(vmArgumentsList->j9Options[i]);
		if (ARG_MEMORY_ALLOCATION == (j9Option->flags & ARG_MEMORY_ALLOCATION)) {
			j9mem_free_memory(actualArgs->options[i].optionString);
		}
		if (NULL != j9Option->mapping) {
			j9mem_free_memory(j9Option->mapping);
		}
	}
	j9mem_free_memory(vmArgumentsList);
}
