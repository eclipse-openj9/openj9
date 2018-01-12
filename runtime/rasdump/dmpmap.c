/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
#include "dmpsup.h"
#include "j9dmpnls.h"
#include "rasdump_internal.h"

#define DG_SYSTEM_DUMPS  "system+snap+tool"


typedef struct DgDumpCondition
{
	char *name;
	char *event;
	char *eventString;
} DgDumpCondition;

/* Mapping for JAVA_DUMP_OPTS conditions */
static const DgDumpCondition dgConditions[] = {
	/* note that the range suboption is placed at the end, it can be varied via JAVA_DUMP_OPTS...[COUNT] */
	{ "ONDUMP",        "user",     "events=user,range=1..0" },
	{ "ONERROR",       "abort",    "events=abort,range=1..0" },
	{ "ONEXCEPTION",   "gpf",      "events=gpf,range=1..0" },
	{ "ONINTERRUPT",   "vmstop",   "events=vmstop,filter=#129..192,range=1..0" },
	{ "ONOUTOFMEMORY", "systhrow", "events=systhrow,filter=java/lang/OutOfMemoryError,range=1..4" }
	/* note that 'ONANYSIGNAL' is handled explicitly in the mapDumpOptions) code below */ 
};

static const int numDumpConditions = ( sizeof(dgConditions) / sizeof(DgDumpCondition) );


typedef struct DgDumpAction
{
	char *name;
	char *typeString;
} DgDumpAction;

/* Mapping for JAVA_DUMP_OPTS actions */
static const DgDumpAction dgActions[] = {
	{ "JAVADUMP",  "java" },
	{ "HEAPDUMP",  "heap" },
	{ "SYSDUMP",   DG_SYSTEM_DUMPS },
	{ "ALL",       DG_SYSTEM_DUMPS "+heap+java+jit" },
	{ "NONE",      "none" },
	{"SNAPDUMP",   "snap"},
#ifdef J9ZOS390
	{ "CEEDUMP",   "ceedump" },
#endif
};

static const int numDumpActions = ( sizeof(dgActions) / sizeof(DgDumpAction) );


typedef struct DgDumpEnvSwitch
{
	char *name;
	char *typeString;
	char *onOpt;
	char *offOpt;
} DgDumpEnvSwitch;

/* Mapping for RAS dump environment variables. Note that these are in precedence order 
 * (later options take precedence over earlier ones in the table) 
 */
static const DgDumpEnvSwitch dgSwitches[] = {
			{
/* name */		"DISABLE_JAVADUMP",
/* kind */		"java",
/* on */		"none",
/* off */		NULL
			},{
/* name */		"IBM_HEAPDUMP",
/* kind */		"heap",
/* on */		"events=user+gpf",
/* off */		"none"
			},{
/* name */		"IBM_HEAP_DUMP",
/* kind */		"heap",
/* on */		"events=user+gpf",
/* off */		"none"
			},{
/* name */		"IBM_JAVADUMP_OUTOFMEMORY",
/* kind */		"java",
/* on */		"events=systhrow,range=1..4,filter=java/lang/OutOfMemoryError",
/* off */		"none"
			},{
/* name */		"IBM_HEAPDUMP_OUTOFMEMORY",
/* kind */		"heap",
/* on */		"events=systhrow,range=1..4,filter=java/lang/OutOfMemoryError",
/* off */		"none"
			},{
/* name */		"IBM_SNAPDUMP_OUTOFMEMORY",
/* kind */		"snap",
/* on */		"events=systhrow,range=1..4,filter=java/lang/OutOfMemoryError",
/* off */		"none"
			},{
/* name */		"J9NO_DUMPS",
/* kind */		DG_SYSTEM_DUMPS "+console+heap+java",
/* on */		"none",
/* off */		NULL
			}
};

static const int numDumpEnvSwitches = ( sizeof(dgSwitches) / sizeof(DgDumpEnvSwitch) );


typedef struct DgDumpEnvSetting
{
	char *name;
	char *typeString;
	char *prefixOpt;
} DgDumpEnvSetting;

/* Mapping for RAS dump filter environment variables */
static const DgDumpEnvSetting dgSettings[] = {
					{
/* name */		"IBM_XE_COE_NAME",
/* kind */			DG_SYSTEM_DUMPS "+java",
/* prefix */		"events=throw,range=1..4,filter="
					}
};

static const int numDumpEnvSettings = ( sizeof(dgSettings) / sizeof(DgDumpEnvSetting) );


typedef struct DgDumpEnvDefault
{
	char *name;
	char *typeString;
	char *defaultOpt;
} DgDumpEnvDefault;

/* Agent defaults */
static const DgDumpEnvDefault dgDefaults[] = {
					{
/* name */		"IBM_JAVA_HEAPDUMP_TEXT",
/* kind */			"heap",
/* default */		"opts=CLASSIC"
					},{
/* name */		"IBM_JAVA_HEAPDUMP_TEST",
/* kind */			"heap",
/* default */		"opts=PHD+CLASSIC"
					},
};

static const int numDumpEnvDefaults = ( sizeof(dgDefaults) / sizeof(DgDumpEnvDefault) );


#if (defined(J9VM_RAS_DUMP_AGENTS)) 
/* Function to process the migration RAS environment variables IBM_JAVA_HEAPDUMP_TEXT
 * and IBM_JAVA_HEAPDUMP_TEST (heapdump type options)
 */
omr_error_t
mapDumpDefaults(J9JavaVM *vm, J9RASdumpOption agentOpts[], IDATA *agentNum)
{
	IDATA i, kind;

	PORT_ACCESS_FROM_JAVAVM(vm);

	char buf[J9_MAX_DUMP_PATH];
	char *defaultString;
	char *finalArg;

	for (i = 0; i < numDumpEnvDefaults; i++) {

		buf[0] = '\0';
		j9sysinfo_get_env(dgDefaults[i].name, buf, J9_MAX_DUMP_PATH);

		switch ( buf[0] ) {
			case '\0' :
			case '0' :
			case 'F' :
			case 'f' :
				defaultString = NULL;
				break;
			default :
				defaultString = dgDefaults[i].defaultOpt;
				break;
		}

		/* Apply default? */
		if ( defaultString ) {

			char *typeString = dgDefaults[i].typeString;

			strcpy(buf, "defaults:");
			strcat(buf, defaultString);

			/* Handle multiple dump types */
			while ( (kind = scanDumpType(&typeString)) >= 0 ) {
				finalArg = j9mem_allocate_memory(strlen(buf) + 1, OMRMEM_CATEGORY_VM);
				if(!finalArg) {
					return OMR_ERROR_INTERNAL;
				}
				strcpy(finalArg, buf);
				agentOpts[*agentNum].kind = kind;
				agentOpts[*agentNum].args = finalArg;
				agentOpts[*agentNum].flags = J9RAS_DUMP_OPT_ARGS_ALLOC;
				agentOpts[*agentNum].pass = J9RAS_DUMP_OPTS_PASS_ONE;
				(*agentNum)++;
			}
		}
	}

	return OMR_ERROR_NONE;
}
#endif /* J9VM_RAS_DUMP_AGENTS */


#if (defined(J9VM_RAS_DUMP_AGENTS)) 
/* Function to process the main migration RAS environment variables JAVA_DUMP_OPTS and 
 * JAVA_DUMP_TOOL. Note that the older RAS environment variables (DISABLE_JAVADUMP etc)
 * are processed earlier in function mapDumpSwitches().
 */
omr_error_t
mapDumpOptions(J9JavaVM *vm, J9RASdumpOption agentOpts[], IDATA *agentNum)
{
	IDATA i, j;
	IDATA kind, saveAgentNum;
	char *cursor;

	PORT_ACCESS_FROM_JAVAVM(vm);

	char buf[J9_MAX_DUMP_PATH] = "";
	if (j9sysinfo_get_env("JAVA_DUMP_OPTS", buf, J9_MAX_DUMP_PATH) == -1) {
		/* JAVA_DUMP_OPTS was not set, so nothing to do */
		return OMR_ERROR_NONE;
	}
	saveAgentNum = *agentNum;

	/* Rule for JAVA_DUMP_OPTS is to parse for ONANYSIGNAL first */
	if ( NULL != (cursor = strstr(buf, "ONANYSIGNAL")) ) {
		cursor += strlen("ONANYSIGNAL");
		if ( *cursor == '(' && ( strchr(cursor, ')')) != NULL ) {	
			
			/* Loop to map all the supported conditions (for ONANYSIGNAL) */
			for (i = 0; i < numDumpConditions; i++) {
				/* Disable any default or earlier envars for this condition */
				for (j = 0; j < saveAgentNum; j++) {
					if (strstr(agentOpts[j].args, dgConditions[i].event)) {
						agentOpts[j].kind = J9RAS_DUMP_OPT_DISABLED;
					}
				}
				/* Map any specified actions for this condition */
				mapDumpActions(vm, agentOpts, agentNum, cursor, i);
			}
		}
	}
	
	/* Then parse for first occurrence (only) of each individual condition */
	for (i = 0; i < numDumpConditions; i++) {

		if ( NULL != (cursor = strstr(buf, dgConditions[i].name)) ) {
			cursor += strlen(dgConditions[i].name);
			
			if ( *cursor == '(' && ( strchr(cursor, ')')) != NULL ) {
				/* Disable any default or earlier envars for this condition */
				for (j = 0; j < saveAgentNum; j++) {
					if (strstr(agentOpts[j].args, dgConditions[i].event)) {
						agentOpts[j].kind = J9RAS_DUMP_OPT_DISABLED;
					}
				}
				/* Map any specifed actions for this condition */
				mapDumpActions(vm, agentOpts, agentNum, cursor, i);
			}
		}
	}
	
	/* Don't map tool dumps if environment variable is missing */
	if (j9sysinfo_get_env("JAVA_DUMP_TOOL", NULL, 0) == -1) {
		char *typeString = "tool";
		kind = scanDumpType(&typeString);
		for (i = 0; i < *agentNum; i++) {
			if (agentOpts[i].kind == kind) {
				agentOpts[i].kind = J9RAS_DUMP_OPT_DISABLED;
			}
		}
	}

	return OMR_ERROR_NONE;
}
#endif /* J9VM_RAS_DUMP_AGENTS */


#if (defined(J9VM_RAS_DUMP_AGENTS))
/* Function to process JAVA_DUMP_OPTS actions. These are multiple comma-separate dump types, with and
 * optional dump count, e.g. JAVA_DUMP_OPTS=ONERROR(JAVADUMP,SYSDUMP,HEAPDUMP[2])
 */
omr_error_t
mapDumpActions(J9JavaVM *vm, J9RASdumpOption agentOpts[], IDATA *agentNum, char *buf, IDATA condition)
{
	IDATA i, j;
	IDATA kind, countChars, length;
	char *actionsRHS, *countRHS, *typeString;
	char *eventString = NULL;
	BOOLEAN allocatedEventStringAlreadyUsed = FALSE;

	PORT_ACCESS_FROM_JAVAVM(vm);
	
	actionsRHS = strchr(buf, ')');
	
	for (i = 0; i < numDumpActions; i++) {
		char *cursor = strstr(buf, dgActions[i].name);

		if ( NULL == cursor || cursor > actionsRHS ) {
			continue;
		}

		/* JAVA_DUMP_OPT action was found, translate it into -Xdump type(s) or 'none' */ 
		typeString = dgActions[i].typeString;
		
		if ( strcmp(typeString, "none") == 0 ) {
			/* just delete all earlier settings for this event type (condition) */
			for (j = 0; j < *agentNum; j++) {
				if (strstr(agentOpts[j].args, dgConditions[condition].event)) {
					agentOpts[j].kind = J9RAS_DUMP_OPT_DISABLED;
				}
			}
			continue; /* (NONE,....) is honoured so continue loop */
		}
			
		/* We found a JAVA_DUMP_OPTS action, now get and process the dump count, if any */
		countChars = 0;
		cursor += strlen(dgActions[i].name);
		
		if (*cursor == '[') { /* found a count parameter for this action */
			countRHS = strchr(cursor, ']');
			cursor++;

			if ( countRHS >= cursor && countRHS < actionsRHS) { /* valid dump count found */
				countChars = countRHS - cursor;
				/* allocate a string for the mapped event string with the count chars appended */
				length = strlen(dgConditions[condition].eventString) + countChars;
				eventString = j9mem_allocate_memory(length, OMRMEM_CATEGORY_VM);
				if (NULL == eventString) {
					j9tty_err_printf(PORTLIB, 
						"Could not allocate memory to handle JAVA_DUMP_OPTS dump count option, option ignored.\n");
					countChars = 0;
				} else {
					memset(eventString, 0, length);
					strncpy(eventString, dgConditions[condition].eventString, 
						strlen(dgConditions[condition].eventString) - 1);
					strncat(eventString, cursor, countChars);
				}
			}
		}
			
		/* Now set up the dump type and event string, for multiple dump actions */
		while ( (kind = scanDumpType(&typeString)) >= 0 ) {
			agentOpts[*agentNum].kind = kind;
			if (countChars) {
				/* there was a dump count specified, so we use the locally allocated event string the first time, and dup it subsequent times*/
				if(allocatedEventStringAlreadyUsed) {
					/*We have to dupe the event string so the free logic later won't double-free. The original version has already been used*/
					agentOpts[*agentNum].flags = J9RAS_DUMP_OPT_ARGS_ALLOC;
					agentOpts[*agentNum].args = j9mem_allocate_memory(strlen(eventString) + 1, OMRMEM_CATEGORY_VM);
					if(NULL == agentOpts[*agentNum].args) {
						j9tty_err_printf(PORTLIB, 
							"Could not allocate memory to handle JAVA_DUMP_OPTS dump count option, option ignored (extra copy failed).\n");
						countChars = 0;
						agentOpts[*agentNum].args = dgConditions[condition].eventString;
						agentOpts[*agentNum].flags = J9RAS_DUMP_OPT_ARGS_STATIC;
					} else {
						strcpy(agentOpts[*agentNum].args,eventString);
					}
				} else {
					/*First time through - use the allocated event string*/
					agentOpts[*agentNum].args = eventString;
					agentOpts[*agentNum].flags = J9RAS_DUMP_OPT_ARGS_ALLOC;
					allocatedEventStringAlreadyUsed = TRUE;
				}
				
			} else {
				/* no dump count specified, use the statically defined event string */
				agentOpts[*agentNum].args = dgConditions[condition].eventString;
				agentOpts[*agentNum].flags = J9RAS_DUMP_OPT_ARGS_STATIC;
			}
			(*agentNum)++;
		}
		
	} /* end for (i = 0; i < DG_DUMP_ACTIONS; i++) */
	
	return OMR_ERROR_NONE;
}
#endif /* J9VM_RAS_DUMP_AGENTS */


#if (defined(J9VM_RAS_DUMP_AGENTS)) 
/* Function to process the older RAS environment variables (DISABLE_JAVADUMP etc). Note that 
 * the JAVA_DUMP_OPTS and JAVA_DUMP_TOOL environment variables are processed separately (later) 
 * in function mapDumpOptions().
 */ 
omr_error_t
mapDumpSwitches(J9JavaVM *vm, J9RASdumpOption agentOpts[], IDATA *agentNum)
{
	IDATA i, kind;

	PORT_ACCESS_FROM_JAVAVM(vm);

	char *optionString, *typeString;
	char buf[J9_MAX_DUMP_PATH];
	BOOLEAN offSelected;

	for (i = 0; i < numDumpEnvSwitches; i++) {

		buf[0] = '\0';
		j9sysinfo_get_env(dgSwitches[i].name, buf, J9_MAX_DUMP_PATH);

		switch ( buf[0] ) {
			case '\0' :
				optionString = NULL;
				offSelected = FALSE;
				break;
			case '0' :
			case 'F' :
			case 'f' :
				offSelected = TRUE;
				optionString = dgSwitches[i].offOpt;
				break;
			default :
				offSelected = FALSE;
				optionString = dgSwitches[i].onOpt;
				break;
		}

		typeString = dgSwitches[i].typeString;
		
		if (offSelected && 
			((strcmp(dgSwitches[i].name, "IBM_HEAPDUMP_OUTOFMEMORY") == 0) ||
			 (strcmp(dgSwitches[i].name, "IBM_JAVADUMP_OUTOFMEMORY") == 0) || 
			 (strcmp(dgSwitches[i].name, "IBM_SNAPDUMP_OUTOFMEMORY") == 0) )) {
			/* special case processing for specific OOM environment variables            */
			/* The "off" option for these variables should only disable the default dump */
			/* agents that match the option values. Other agents for the same dump type  */
			/* should remain untouched.                                                  */
			int j;
			IDATA disableKind = scanDumpType(&typeString);

			/* walk through the agentOpts and disable those that match the type and options */
			for (j = 0; j < *agentNum; j++) {
				if (NULL != agentOpts[j].args) { 
					if (agentOpts[j].kind == disableKind) {
						if (strcmp(agentOpts[j].args, dgSwitches[i].onOpt) == 0) {
							agentOpts[j].kind = J9RAS_DUMP_OPT_DISABLED;
						}
					}
				}
			}
		} else {
			/* set up the dump type and event string for multiple dump actions */
			while ( optionString && (kind = scanDumpType(&typeString)) >= 0 ) {
				agentOpts[*agentNum].kind = kind;
				agentOpts[*agentNum].args = optionString;
				agentOpts[*agentNum].flags = J9RAS_DUMP_OPT_ARGS_STATIC;
				(*agentNum)++;
			}
		}
	}

	return OMR_ERROR_NONE;
}
#endif /* J9VM_RAS_DUMP_AGENTS */


#if (defined(J9VM_RAS_DUMP_AGENTS)) 
/* Function to process additional RAS environment variable IBM_XE_COE_NAME
 * (support for dump on filtered exception).
 */
omr_error_t
mapDumpSettings(J9JavaVM *vm, J9RASdumpOption agentOpts[], IDATA *agentNum)
{
	IDATA i, kind, len;

	PORT_ACCESS_FROM_JAVAVM(vm);

	char buf[J9_MAX_DUMP_PATH];
	char *finalArg;

	for (i = 0; i < numDumpEnvSettings; i++) {

		len = strlen(dgSettings[i].prefixOpt);
		strncpy(buf, dgSettings[i].prefixOpt, len);

		/* Tack on user setting */
		if ( j9sysinfo_get_env(dgSettings[i].name, buf+len, J9_MAX_DUMP_PATH-len) == 0 ) {

			char *typeString = dgSettings[i].typeString;
			buf[J9_MAX_DUMP_PATH-1] = '\0';

			/* Handle multiple dump types */
			while ( (kind = scanDumpType(&typeString)) >= 0 ) {
				finalArg = j9mem_allocate_memory(strlen(buf) + 1, OMRMEM_CATEGORY_VM);
				if(!finalArg) {
					return OMR_ERROR_INTERNAL;
				}
				strcpy(finalArg, buf);
				agentOpts[*agentNum].kind = kind;
				agentOpts[*agentNum].args = finalArg;
				agentOpts[*agentNum].flags = J9RAS_DUMP_OPT_ARGS_ALLOC;
				(*agentNum)++;
			}
		}
	}

	return OMR_ERROR_NONE;
}
#endif /* J9VM_RAS_DUMP_AGENTS */
