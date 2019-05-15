/*******************************************************************************
 * Copyright (c) 2002, 2019 IBM Corp. and others
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

/*
 * DESCRIPTION: Trace Triggering routines
 *
 * The following options are largely processed by routines in this file
 *   trigger
 *   resumecount
 *   suspendcount
 *
 * The trigger property takes a comma separated list of clauses which can be
 * any of those defined below, including repeated clause types (ie multiple
 * method{...} clauses):
 *   method{methodspec,[entryAction],[exitAction][,delay]}
 *   group{traceGroupName,action[,delay]}
 *   tpnid{componentName.tpidNum1[-tpidNum2],action[,delay]}
 *
 * triggering on tpids:
 *    The trigger bit in the tracepoint active array (in the executable) is
 *    set.  An entry is added for the tpid (or tpid range) specified, anchored
 *    off triggerOnTpids in RasGlobalStorage.  When a call for this tpid is made into
 *    dgtrace(...) et al, the triggerOnTpids chain is checked, to determine
 *    the action and delay.
 *
 * triggering on groups:
 *    The trigger bit is set for all the tracepoints in the group.  An entry
 *    is added for the group, anchored off triggerOnGroups in RasGlobalStorage.  A
 *    linked list is created of all the tracepoints in this group anchored off
 *    this group entry.  When trace is entered, the list of tpids is
 *    searched for all of the triggered groups to determine if tpid X is a
 *    member of group Y.  If it is then the group structure contains the
 *    action and delay.
 *
 * triggering on methods:
 *    An entry is created in the triggerOnMethods chain, anchored off RasGlobalStorage.
 *    This is known as a method rule.  This contains a RasMethodSpec, as used
 *    in method trace, representing the user entered methodspec.  Whenever a
 *    class is loaded, the classloader calls us.  The class and if necessary its
 *    methods are checked against the methodspecs of all the method rules.
 *    When a match is found, a trigger bit is set in the method block and just
 *    as in Method Trace, when this is entered or exited, rasTraceMethod is
 *    called.  This now calls rasTriggerMethod which checks all the method
 *    rules to see which ones(s) wish to trigger on this J9Method.  The
 *    entryAction, exitAction and delay are stored in the method rule.
 *
 * resumecout and suspendcount:
 *    resumecount is the number of resumethis actions required to start a
 *    thread tracing.  suspendcount is the number of suspendthis calls
 *    required to stop a thread tracing.  Both act on the RasGlobalStorage value
 *    initialTraceSuspendResume which is the trace suspend/resume value given
 *    to all newly created threads.  Only one of these properties may be
 *    specified.
 *
 * ===========================================================================
 */

/* _GNU_SOURCE forces GLIBC_2.0 sscanf/vsscanf/fscanf for RHEL5 compatibility */
#if defined(LINUX) && !defined(J9ZTPF)
#define _GNU_SOURCE
#endif /* defined(__GNUC__) && !defined(J9ZTPF) */

#include "trctrigger.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "j9cfg.h"
#include "rastrace_internal.h"
#include "omrutilbase.h"
#include "omrutil.h"

static void doTriggerActionSuspend(OMR_VMThread *);
static void doTriggerActionResume(OMR_VMThread *);
static void doTriggerActionSuspendThis(OMR_VMThread *);
static void doTriggerActionResumeThis(OMR_VMThread *);
static void doTriggerActionAbort(OMR_VMThread *);
static void doTriggerActionSegv(OMR_VMThread *);
static void doTriggerActionSleep(OMR_VMThread *thr);


/**
 * Each possible trigger action has an entry in this array. The
 * order of the entries is not important. These are the default
 * trigger actions defined by OMR. Languages can add actions via
 * addTriggerAction
 */
static struct RasTriggerAction defaultRasTriggerActions[] = {
	{ "suspend", AFTER_TRACEPOINT, doTriggerActionSuspend },
	{ "resume", BEFORE_TRACEPOINT, doTriggerActionResume},
	{ "suspendthis", AFTER_TRACEPOINT, doTriggerActionSuspendThis},
	{ "resumethis", BEFORE_TRACEPOINT, doTriggerActionResumeThis},
	{ "abort", AFTER_TRACEPOINT, doTriggerActionAbort},
	{ "segv", AFTER_TRACEPOINT, doTriggerActionSegv},
	{ "sleep", AFTER_TRACEPOINT, doTriggerActionSleep},
};

struct RasTriggerAction* rasTriggerActions = defaultRasTriggerActions;

#define NUM_DEFAULT_TRIGGER_ACTIONS (sizeof(defaultRasTriggerActions) / sizeof(defaultRasTriggerActions[0]))

int numTriggerActions = NUM_DEFAULT_TRIGGER_ACTIONS;

static omr_error_t processTriggerGroupClause(OMR_VMThread *, char *, BOOLEAN atRuntime);
static omr_error_t processTriggerTpnidClause(OMR_VMThread *, char *, BOOLEAN atRuntime);

/**
 * Each possible trigger type has an entry in this array. The
 * order of the entries is not important.
 */
static struct RasTriggerType defaultRasTriggerTypes[] = {
	/* trigger clause, parse function, runtime configurable */
	{ "group", processTriggerGroupClause, TRUE },
	{ "tpnid", processTriggerTpnidClause, TRUE }
};

struct RasTriggerType* rasTriggerTypes = defaultRasTriggerTypes;

#define NUM_DEFAULT_TRIGGER_TYPES (sizeof(defaultRasTriggerTypes) / sizeof(defaultRasTriggerTypes[0]))

int numTriggerTypes = NUM_DEFAULT_TRIGGER_TYPES;


/*The name passed to the dump trigger system describing this component - appears in Javacores and on the console*/
#define DUMP_CALLER_NAME "-Xtrace:trigger"

/**************************************************************************
 * name        - getPositionalParm
 * description - Get a positional parameter
 * parameters  - string value of the property
 * returns     - Pointer to keyword or null. If not null, size is set
 *************************************************************************/
const char *
getPositionalParm(int pos, const char *string, int *size)
{
	const char *p, *q;
	int i;

	/*
	 * Find the parameter
	 */

	p = string;
	for (i = 1; i < pos; i++) {
		p = strchr(p, ',');
		if (p != NULL) {
			p++;
		} else {
			break;
		}
	}
	/*
	 *  Find its size
	 */

	if (p != NULL) {
		q = strchr(p, ',');
		if (q == NULL) {
			*size = (int) strlen(p);
		} else {
			*size = (int) (q - p);
		}
	}
	return p;
}

/**************************************************************************
 * name        - getParmNumber
 * description - Gets the number of positional parameters
 * parameters  - string
 * returns     - Number of parameters
 *************************************************************************/
int
getParmNumber(const char *string)
{
	const char *p;
	int i;

	p = string;
	for (i = 0; p != NULL; i++) {
		p = strchr(p, ',');
		if (p != NULL) {
			p++;
		}
	}
	return i;
}

/**************************************************************************
 * name        - setSleepTime
 * description - Set SleepTime property
 * parameters  - vm, trace options, atRuntime flag
 * returns     - JNI return code
 *************************************************************************/
omr_error_t
setSleepTime(UtThreadData **thr, const char * str, BOOLEAN atRuntime)
{
	int length;
	uint64_t value;
	const char *param;
	const char *suffix;
	BOOLEAN unitsSeconds = FALSE;
	char numberBuffer[13];

	if (getParmNumber(str) != 1) {
		goto err;
	}
	
	param = getPositionalParm(1, str, &length);
	
	/* Max length of 12 because strlen(4294967295ms) = 12 */
	if (length == 0 || length > 12) {
		goto err;
	}

	/* Find the suffix (if there is one) */
	suffix = param;
	while(*suffix!='\0' && strchr("0123456789",*suffix)) {
		suffix++;
	}

	if(suffix == param) {
		/* First character isn't a number or the string is empty */
		goto err;
	}

	if(*suffix != '\0') {
		/* A suffix has been specified, parse it */
		if(j9_cmdla_stricmp((char *)suffix, "s") == 0) {
			unitsSeconds = TRUE;
		} else if (j9_cmdla_stricmp((char *)suffix, "ms") == 0) {
			/* Do nothing - milliseconds is the default */
		} else {
			/* Unrecognised suffix */
			goto err;
		}

		/* Copy the number section into a buffer and parse that */
		memset(numberBuffer,0,sizeof(numberBuffer));
		strncpy(numberBuffer,param,suffix-param);
		value = strtoul(numberBuffer,NULL,0);
	} else {
		/* No suffix - parse the entire number */
		value = strtoul(param,NULL,0);
	}

	/* If the value is larger than will fit in a 32 bit number, errno will be set to ERANGE */
	if(errno == ERANGE) {
		goto err;
	}

	/* The value is stored in milliseconds */
	if(unitsSeconds) {
		value = value * 1000;
	}

	if(value > U_32_MAX) {
		goto err;
	}

	UT_GLOBAL(sleepTimeMillis) = (uint32_t)value;
	return OMR_ERROR_NONE;

err:
	reportCommandLineError(atRuntime, "Sleeptime takes a positive integer value and, optionally, a suffix of s or ms. Maximum sleeptime is 4294967295 milliseconds.");
	return OMR_ERROR_INTERNAL;
}


/**************************************************************************
 * name        - decimalString2Int
 * description - Convert a decimal string into an int
 * parameters  - decString - ptr to the decimal string
 *               rc        - ptr to return code
 * returns     - void
 *************************************************************************/
int
decimalString2Int(const char *decString, int32_t signedAllowed, omr_error_t *rc, BOOLEAN atRuntime)
{
	const char *p = decString;
	int num = -1;
	int32_t inputIsSigned = FALSE;
	int min_string_length = 1;
	int max_string_length = 7;

	/*
	 * is first character a sign character
	 */
	if (*p == '+' || *p == '-') {
		inputIsSigned = TRUE;
		min_string_length++;
		max_string_length++;
		p++;
	}

	/*
	 * check whether an invalid sign character was entered
	 */
	if (inputIsSigned && !signedAllowed) {
		reportCommandLineError(atRuntime, "Signed number not permitted in this context \"%s\".", decString);
		*rc = OMR_ERROR_INTERNAL;
	}

	/*
	 * walk to end of the string
	 */
	if (OMR_ERROR_NONE == *rc) {
		for (; ((*p != '\0') && strchr("0123456789", *p) != NULL); p++) { /*empty */
		}

		/*
		 * check what followed the string.  If it wasn't a ',' or '}' or NULL
		 * then there's garbage in the string
		 */
		if ((*p != ',') && (*p != '}') && (*p != '\0') && (*p != ' ')) {
			reportCommandLineError(atRuntime, "Invalid character(s) encountered in decimal number \"%s\".", decString);
			*rc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == *rc) {
		/*
		 * if it was too long or too short, error, else parse it as num
		 */
		if ((p - decString) < min_string_length || (p - decString) > max_string_length) {
			*rc = OMR_ERROR_INTERNAL;
			reportCommandLineError(atRuntime, "Number too long or too short \"%s\".", decString); 
		} else {
			sscanf(decString, "%d", &num);
		}
	}

	return num;
}

/**************************************************************************
 * name        - parseTriggerIndex
 * description - convert the name of a trigger action into its index in
 *               the rasTriggerActions array.
 * parameters  - thr, a trigger name string.
 * returns     - Index into rasTriggerActions or -1 if the name is invalid
 *************************************************************************/

static uint32_t
parseTriggerIndex(OMR_VMThread *thr, const char * name, BOOLEAN atRuntime)
{
	int i;
	
	for (i = 0; i < numTriggerActions; i++) {
		const struct RasTriggerAction *action = &rasTriggerActions[i];
		
		if (j9_cmdla_stricmp((char *)name, (char *)action->name) == 0) {
			return i;
		}
	}

	reportCommandLineError(atRuntime, "Invalid trigger action \"%s\" selected.", name);
	return -1;
}

/**************************************************************************
 * name        - addTriggerAction
 * description - add an action to be triggered by a trace event.
 *               This function must be called before any options processing
 *               takes place.
 * parameters  - thr,
 *               newAction the RasTriggerAction describing this action
 * returns     - JNI return code
 *************************************************************************/
omr_error_t
addTriggerAction(OMR_VMThread *thr, const struct RasTriggerAction *newAction)
{

	struct RasTriggerAction *newRasTriggerActions = NULL;

	OMRPORT_ACCESS_FROM_OMRVMTHREAD(thr);

	if( NULL == newAction ||  NULL == newAction->name || NULL == newAction->fn ) {
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	/* Append it to the existing list of options and update */
	newRasTriggerActions = omrmem_allocate_memory(sizeof(struct RasTriggerAction) * (numTriggerActions + 1), OMRMEM_CATEGORY_TRACE);

	if( NULL == newRasTriggerActions ) {
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}

	/* Copy the old options access. */
	memcpy( newRasTriggerActions, rasTriggerActions, sizeof(struct RasTriggerAction)*numTriggerActions);

	/* Add the new option as the final element. */
	newRasTriggerActions[numTriggerActions] = *newAction;

	if( defaultRasTriggerActions != rasTriggerActions ) {
		/* We have already allocated one new action, we need to free rasTriggerActions. */
		omrmem_free_memory(rasTriggerActions);
	}
	rasTriggerActions = newRasTriggerActions;
	numTriggerActions++;

	return OMR_ERROR_NONE;
}

/**************************************************************************
 * name        - parseTriggerAction
 * description - convert the name of a trigger action into its entry in
 *               the rasTriggerActions array.
 * parameters  - thr, a trigger name string.
 * returns     - Trigger action or NULL
 *************************************************************************/
const struct RasTriggerAction *
parseTriggerAction(OMR_VMThread *thr, const char *name, BOOLEAN atRuntime)
{
	uint32_t index = parseTriggerIndex(thr,name,atRuntime);
	
	if (index != -1) {
		return &rasTriggerActions[index];
	} else {
		return NULL;
	}
}

/**************************************************************************
 * name        - processTriggerTpidClause
 * description - process a TPNID clause of the Trigger spec
 * parameters  - thr, a clause (tpnid)
 * returns     - JNI return code
 *************************************************************************/
static omr_error_t
processTriggerTpnidClause(OMR_VMThread *thr, char *clause, BOOLEAN atRuntime)
{
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(thr);
	omr_error_t rc = OMR_ERROR_NONE;
	char *p = NULL;
	int length = 0;
	const char *ptrName = NULL;
	const char *ptrAction = NULL;
	const char *ptrDelay = NULL;
	const char *ptrMatch = NULL;
	char *ptrRangeStart = NULL;
	char *ptrRangeEnd = NULL;
	int32_t doneFirstParm = FALSE;
	int tpidRangeStart = 0;
	int tpidRangeEnd = 0;
	uint32_t actionIndex = 0;
	int delayCount = 0;
	int matchCount = -1;
	RasTriggerTpidRange *newTriggerRangeP = NULL;
	char *compName = NULL;

	RAS_DBGOUT((stderr, "<RAS> Processing tpnid clause: \"%s\"\n", clause));

	ptrName = getPositionalParm(1, clause, &length);
	ptrAction = getPositionalParm(2, clause, &length);
	ptrDelay = getPositionalParm(3, clause, &length);
	ptrMatch = getPositionalParm(4, clause, &length);

	/*
	 * check doesn't have more than 4 args
	 */
	if (getParmNumber(clause) > 4 || /* may NOT have more than 4 parameters */
			ptrName == NULL || /* but MUST have name                  */
			ptrAction == NULL) { /* ...and action                       */
		reportCommandLineError(atRuntime,  
				"Invalid tpnid clause, usage:"
				" tpnid{compname.offset[-offset2],action[,delaycount][,matchcount]}"
				" clause is: tpnid{%s}", clause);
		rc = OMR_ERROR_INTERNAL;
	}

	if (OMR_ERROR_NONE == rc) {
		/*
		 * change commas to nulls (all items become null-terminated strings)
		 */
		for (p = clause; *p != '\0'; p++) {
			if (*p == ',') {
				doneFirstParm = TRUE;
				*p = '\0';
			} else {
				if (doneFirstParm == FALSE) {
					if (*p == '.') {
						/* we have a tpid range */
						ptrRangeStart = p + 1;
						*p = '\0';
					} else if (*p == '-') {
						/* we have a tpid range */
						ptrRangeEnd = p + 1;
						*p = '\0';
					}
				}
			}
		}

		/*
		 * convert keyword to trigger action number
		 */
		actionIndex = parseTriggerIndex(thr, ptrAction, atRuntime);
		if (actionIndex == -1) {
			rc = OMR_ERROR_INTERNAL;
		} else {

			/*
			 * process component name
			 */
			compName = omrmem_allocate_memory(strlen(ptrName) + 1, OMRMEM_CATEGORY_TRACE);
			if (compName) {
				strcpy(compName, ptrName);
			} else {
				rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
				UT_DBGOUT(1, ("<UT> Out of memory processing trigger property."));
			}

			/*
			 * Default to first tracepoint if no range/tpid given
			 */
			if (ptrRangeStart == NULL) {
				ptrRangeStart = "0";
			}

			/*
			 * deal with tpid range or single tpid specified
			 */
			tpidRangeStart = decimalString2Int(ptrRangeStart, FALSE, &rc, atRuntime);
			if (OMR_ERROR_NONE == rc) {
				if (ptrRangeEnd == NULL) {
					tpidRangeEnd = tpidRangeStart;
				} else {
					tpidRangeEnd = decimalString2Int(ptrRangeEnd, FALSE, &rc, atRuntime);
				}
			}

			/*
			 * in "tpnid{component.x-y,...", y cannot be < x  (yes, it can be equal)
			 */
			if ((OMR_ERROR_NONE == rc) && (tpidRangeEnd < tpidRangeStart)) {
				reportCommandLineError(atRuntime, "Invalid tpnid range - start value cannot be higher than end value.");
				rc = OMR_ERROR_INTERNAL;
			}

			if (OMR_ERROR_NONE == rc) {
				/*
				 * process matchcount if specified
				 */
				if (ptrMatch != NULL) {
					matchCount = decimalString2Int(ptrMatch, TRUE, &rc, atRuntime);
				}
			}

			if (OMR_ERROR_NONE == rc) {
				/*
				 * process delay count if specified
				 */
				if (ptrDelay != NULL && *ptrDelay != '\0') {
					/* issues error & sets rc if ptrDelay points to a bad number */
					delayCount = decimalString2Int(ptrDelay, FALSE, &rc, atRuntime);
				}

				/*
				 * allocate and populate a tpid range structure
				 * hanging off triggerOnTpids anchor point in RasGlobalStorage
				 */
				if (OMR_ERROR_NONE == rc) {
					newTriggerRangeP = (RasTriggerTpidRange *)
					omrmem_allocate_memory(sizeof(RasTriggerTpidRange), OMRMEM_CATEGORY_TRACE);
					if (newTriggerRangeP == NULL) {
						rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
						UT_DBGOUT(1, ("<UT> Out of memory processing trigger property."));
					}
				}

				/*
				 * initialize the new tpid range
				 */
				if (OMR_ERROR_NONE == rc) {
					/* header */
					memcpy(newTriggerRangeP->header.eyecatcher, "RSTP", 4);
					newTriggerRangeP->header.length = (int) (sizeof(RasTriggerTpidRange));

					/* body */
					newTriggerRangeP->next = NULL;
					newTriggerRangeP->compName = compName;
					newTriggerRangeP->startTpid = tpidRangeStart;
					newTriggerRangeP->endTpid = tpidRangeEnd;
					newTriggerRangeP->delay = delayCount;
					newTriggerRangeP->actionIndex = actionIndex;
					newTriggerRangeP->match = matchCount;

					/*
					 * add the new tpid range to the chain
					 */
					omrthread_monitor_enter(UT_GLOBAL(triggerOnTpidsWriteMutex));

					newTriggerRangeP->next = UT_GLOBAL(triggerOnTpids);
					UT_GLOBAL(triggerOnTpids) = newTriggerRangeP;

					omrthread_monitor_exit(UT_GLOBAL(triggerOnTpidsWriteMutex));
				}
			}
		}
	}
	return rc;
}

/**************************************************************************
 * name        - processTriggerGroupClause
 * description - process a GROUP clause of the Trigger spec
 * parameters  - thr, a clause (group)
 * returns     - JNI return code
 *************************************************************************/
static omr_error_t
processTriggerGroupClause(OMR_VMThread *thr, char *clause, BOOLEAN atRuntime)
{
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(thr);
	omr_error_t rc = OMR_ERROR_NONE;
	int numParms = 0;
	int length = 0;
	unsigned int maxLength = 5;
	const char *ptrGroupName = NULL;
	const char *ptrAction = NULL;
	const char *ptrDelay = NULL;
	const char *ptrMatch = NULL;
	uint32_t actionIndex = 0;
	int delay = 0;
	int match = -1;
	char *p = NULL;
	RasTriggerGroup *newTriggerGroupP = NULL;
	char *copyOfNameP = NULL;

	RAS_DBGOUT((stderr, "<RAS> Processing GROUP clause: \"%s\"\n", clause));

	/*
	 * get number of params and a pointer to each of them
	 */
	numParms = getParmNumber(clause);
	ptrGroupName = getPositionalParm(1, clause, &length);
	ptrAction = getPositionalParm(2, clause, &length);
	ptrDelay = getPositionalParm(3, clause, &length); /* NULL is acceptable */
	ptrMatch = getPositionalParm(4, clause, &length);

	/*
	 * change commas to nulls (all items become null-terminated strings)
	 */
	for (p = clause; *p != '\0'; p++) {
		if (*p == ',') {
			*p = '\0';
		}
	}

	/*
	 * group{....} must have 2 - 4 arguments
	 */

	if (numParms < 2 || numParms > 4) {
		reportCommandLineError(atRuntime, "Trigger groups clause has the following usage: group{<groupname>,<action>[,<delay>][,<matchcount>]}");
		rc = OMR_ERROR_INTERNAL;
	}

	/*
	 * if there's a third parameter, check that it is valid
	 */
	if (numParms >= 3 && ptrDelay != NULL && *ptrDelay != '\0') {
		if (*ptrDelay == '+' || *ptrDelay == '-') { /* allow 6 chars if signed  */
			maxLength++; /* or 5 if not              */
		}
		if (strlen(ptrDelay) > maxLength) {
			reportCommandLineError(atRuntime, "Delay counts must be integer values from -99999 to +99999: group{%s,%s,%s,%s}",
					ptrGroupName, ptrAction, ptrDelay, ptrMatch);
			rc = OMR_ERROR_INTERNAL;
		} else {
			/* issues error and sets rc if ptrDelay points to bad number */
			delay = decimalString2Int(ptrDelay, FALSE, &rc, atRuntime);
		}
	}

	/*
	 * if there's a fourth parameter, check that it is valid
	 */
	if (numParms == 4) {
		if (*ptrMatch == '+' || *ptrMatch == '-') { /* allow 6 chars if signed  */
			maxLength++; /* or 5 if not              */
		}
		if (strlen(ptrMatch) > maxLength) {
			reportCommandLineError(atRuntime,  "Match counts must be integer values from -99999 to +99999: group{%s,%s,%s,%s}"
					, ptrGroupName, ptrAction, ptrDelay, ptrMatch);
			rc = OMR_ERROR_INTERNAL;
		} else {
			/* issues error and sets rc if ptrDelay points to bad number */
			match = decimalString2Int(ptrMatch, TRUE, &rc, atRuntime);
		}
	}

	/*
	 * check trigger action is valid
	 */
	if (OMR_ERROR_NONE == rc) {
		actionIndex = parseTriggerIndex(thr, ptrAction, atRuntime);
		if (actionIndex == -1) {
			rc = OMR_ERROR_INTERNAL;
		}
	}

	/*
	 * create the group entry in the triggerOnGroups chain
	 */
	if (OMR_ERROR_NONE == rc) {
		RAS_DBGOUT((stderr, "<RAS> creating group entry in triggerOnGroups chain\n"));

		/* allocate and populate a trigger Group structure */
		newTriggerGroupP = (RasTriggerGroup *)omrmem_allocate_memory(sizeof(RasTriggerGroup), OMRMEM_CATEGORY_TRACE);
		copyOfNameP = omrmem_allocate_memory(strlen(ptrGroupName) + 1, OMRMEM_CATEGORY_TRACE);
		if (newTriggerGroupP == NULL || copyOfNameP == NULL) {
			rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
			UT_DBGOUT(1, ("<UT> Out of memory processing trigger property."));
		}

		/* initialize the new tpid group */
		if (OMR_ERROR_NONE == rc) {
			/* header */
			memcpy(newTriggerGroupP->header.eyecatcher, "RSGR", 4);
			newTriggerGroupP->header.length = (int) (sizeof(RasTriggerGroup));
			/* body */
			strncpy(copyOfNameP, ptrGroupName, strlen(ptrGroupName) + 1);
			newTriggerGroupP->groupName = copyOfNameP;
			newTriggerGroupP->next = NULL;
			newTriggerGroupP->match = match;
			newTriggerGroupP->delay = delay;
			newTriggerGroupP->actionIndex = actionIndex;
			
			/*
			 * add the new group range to the chain
			 */
			omrthread_monitor_enter(UT_GLOBAL(triggerOnGroupsWriteMutex));
		
			newTriggerGroupP->next = UT_GLOBAL(triggerOnGroups);
			UT_GLOBAL(triggerOnGroups) = newTriggerGroupP;

			omrthread_monitor_exit(UT_GLOBAL(triggerOnGroupsWriteMutex));
		}
	}

	return rc;
}

/**************************************************************************
 * name        - addTriggerType
 * description - add a type of trace trigger (for example method trace for
 *               a java application
 *               This function must be called before any options processing
 *               takes place.
 * parameters  - thr,
 *               optName - the name of this trigger type.
 *               parse - the function to parse the option string for this
 *               trigger type.
 *               atRuntime flag - whether changes to this option can be
 *               processed at runtime.
 * returns     - JNI return code
 *************************************************************************/
omr_error_t
addTriggerType(OMR_VMThread *thr, const struct RasTriggerType *newType)
{

	struct RasTriggerType *newRasTriggerTypes = NULL;

	OMRPORT_ACCESS_FROM_OMRVMTHREAD(thr);

	if( NULL == newType) {
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	/* Append it to the existing list of options and update */
	newRasTriggerTypes = omrmem_allocate_memory(sizeof(struct RasTriggerType) * (numTriggerTypes + 1), OMRMEM_CATEGORY_TRACE);

	if( NULL == newRasTriggerTypes ) {
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}

	/* Copy the old options access. */
	memcpy( newRasTriggerTypes, rasTriggerTypes, sizeof(struct RasTriggerType)*numTriggerTypes);

	/* Add the new option as the final element. */
	newRasTriggerTypes[numTriggerTypes] = *newType;

	if( defaultRasTriggerTypes != rasTriggerTypes ) {
		/* We have already allocated one new type, we need to free rasTriggerTypes. */
		omrmem_free_memory(rasTriggerTypes);
	}
	rasTriggerTypes = newRasTriggerTypes;
	numTriggerTypes++;

	return OMR_ERROR_NONE;
}
void
freeTriggerOptions(J9PortLibrary *portLibrary) {

	PORT_ACCESS_FROM_PORT(portLibrary);

	clearAllTriggerActions();

	if( defaultRasTriggerTypes != rasTriggerTypes ) {
		/* We have allocated new types, we need to free rasTriggerTypes. */
		j9mem_free_memory(rasTriggerTypes);
		rasTriggerTypes = defaultRasTriggerTypes;
		numTriggerTypes = NUM_DEFAULT_TRIGGER_TYPES;
	}

	if( defaultRasTriggerActions != rasTriggerActions ) {
		/* We have allocated new actions, we need to free rasTriggerActions. */
		j9mem_free_memory(rasTriggerActions);
		rasTriggerActions = defaultRasTriggerActions;
		numTriggerActions = NUM_DEFAULT_TRIGGER_ACTIONS;
	}
}

/**************************************************************************
 * name        - processTriggerClause
 * description - process a clause of the Trigger spec
 * parameters  - thr, a clause
 *               i.e. the string "method{.....}"
 *                            or "group{.....}"
 *                            or "tpnid{.....}"
 *               atRuntime flag
 * returns     - JNI return code
 *************************************************************************/
static omr_error_t
processTriggerClause(OMR_VMThread *thr, const char *clause, BOOLEAN atRuntime)
{
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(thr);
	uintptr_t clauseLength = strlen(clause);
	int i;
	BOOLEAN disableSet = FALSE;
	
	if (clauseLength == 0) {
		reportCommandLineError(atRuntime, "Zero length clause in trigger statement.");
		return OMR_ERROR_INTERNAL;
	}
	
	if (clause[clauseLength - 1] != '}') {
		reportCommandLineError(atRuntime, "Trigger clause must end with '}'");
		return OMR_ERROR_INTERNAL;
	}
	
	if (*clause == '!') {
		clause++;
		disableSet = TRUE;
	}

	for (i = 0; i < numTriggerTypes; i++) {
		const struct RasTriggerType *type = &rasTriggerTypes[i];
		
		if (j9_cmdla_strnicmp((char *)clause, (char *)type->name, strlen(type->name)) == 0) {
			uintptr_t typeLength = strlen(type->name);
			uintptr_t subClauseLength;
			char *subClause;
			int rc;
			
			if (atRuntime && !type->runtimeModifiable) {
				UT_DBGOUT(1, ("<UT> Trigger clause %s cannot be modified at run time\n",type->name));
				return OMR_ERROR_INTERNAL;
			}
			
			if (disableSet) {
				/* The trigger is being switched off. We have no processing to do
				 * so return.
				 */
				return OMR_ERROR_NONE;
			}
			
			if (clauseLength <= typeLength) {
				reportCommandLineError(atRuntime, "Empty trigger clause \"%s\" not permitted.", clause);
				return OMR_ERROR_INTERNAL;
			}
			
			if (clause[typeLength] != '{') {
				reportCommandLineError(atRuntime, "Trigger clause must begin with '{'.");
				return OMR_ERROR_INTERNAL;
			}
			
			subClauseLength = clauseLength - typeLength - 2;
			
			subClause = omrmem_allocate_memory(subClauseLength + 1, OMRMEM_CATEGORY_TRACE);
			if (!subClause) {
				UT_DBGOUT(1, ("<UT> Out of memory processing trigger property.\n"));
				return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
			}
			
			strncpy(subClause, &clause[typeLength + 1], subClauseLength);
			subClause[subClauseLength] = '\0';
			
			rc = type->parse(thr, subClause, atRuntime);
			omrmem_free_memory(subClause);
			return rc;
		}
	}
	
	reportCommandLineError(atRuntime, "Invalid trigger clause: \"%s\"", clause);
	return OMR_ERROR_INTERNAL;
}

/**************************************************************************
 * name        - getNextBracketedParm
 * description - get the next piece of a string delimited by commas (and
 *               EndOfString but ALSO taking notice of brackets so that
 *               for "abc{de,fg},hi" we return "abc{de,fg}" and NOT
 *               "abc{de,"
 * parameters  - start of string,
 *               the addr of the return code
 *               the addr of a done flag (we've finished the string now)
 * returns     - JNI return code
 *************************************************************************/
char *
getNextBracketedParm(const char *from, omr_error_t *rc, int32_t * done, BOOLEAN atRuntime)
{
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));
	const char *p;
	int32_t endOfClause = FALSE;
	int indentLevel = 0;
	int length = 0;
	char *returnClause = NULL;

	for (p = from; endOfClause == FALSE; p++) {

		switch (*p) {
			/*
			 * walk down the string incrementing and decrementing indentLevel
			 * for brackets.
			 */
			case '{':
			indentLevel++;
			break;
			case '}':
			indentLevel--;
			break;

			/*
			 * comma may indicate end of clause
			 */
			case ',':
			if (indentLevel == 0) {
				endOfClause = TRUE;
			} else {
				/* comma but still in brackets - ignore */
			}
			break;

			/*
			 * end of string and therefore clause - set flag to say we've finished
			 */
			case '\0':
			endOfClause = TRUE;
			*done = TRUE;
			break;

			default:
			break;
		}
	}

	if (indentLevel == 0) {
		/*
		 * comma or end of string at indent level 0
		 */
		length = (int) (p - from);
		returnClause = j9mem_allocate_memory(length, OMRMEM_CATEGORY_TRACE);
		if (returnClause == NULL) {
			UT_DBGOUT(1, ("<UT> Out of memory processing trigger property."));
			*rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		} else {
			strncpy(returnClause, from, length);
			returnClause[length - 1] = '\0';
			*rc = OMR_ERROR_NONE;
		}
	} else {
		/*
		 * ended string still in brackets - ERROR!
		 */
		reportCommandLineError(atRuntime, "Missing closing brace(s) in trigger property.");
		*rc = OMR_ERROR_INTERNAL;
	}

	return returnClause;
}


/**************************************************************************
 * name        - setTriggerActions
 * description - Set Trigger property
 * parameters  - thr, trace options, atRuntime flag
 * returns     - JNI return code
 *************************************************************************/
omr_error_t
setTriggerActions(UtThreadData **thr, const char *value, BOOLEAN atRuntime)
{
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));
	omr_error_t rc = OMR_ERROR_NONE;
	char *clause = NULL;
	int32_t done = FALSE;

	if (value == NULL || strlen(value) == 0) {
		reportCommandLineError(atRuntime, "Usage error: trigger={[method{args}],[tpnid{args}],[group{args}]...}"); 
		return OMR_ERROR_INTERNAL;
	}

	/*
	 * Keep calling getNextBracketedParm until done comes back TRUE
	 */
	while ((OMR_ERROR_NONE == rc) && (!done)) {
		clause = getNextBracketedParm(value, &rc, &done, atRuntime);
		if ((OMR_ERROR_NONE == rc) && (0 == strlen(clause))) {
			reportCommandLineError(atRuntime, "Empty clauses not allowed in trigger property."); 
			rc = OMR_ERROR_INTERNAL;
		}
		if (OMR_ERROR_NONE == rc) {
			value = value + strlen(clause) + 1;

			rc = processTriggerClause(OMR_VM_THREAD_FROM_UT_THREAD(thr), clause, atRuntime);
		}
		if (clause != NULL) {
			j9mem_free_memory(clause);
		}
	}

	return rc;
}

/**************************************************************************
 * name        - doTriggerActionSuspend
 * description - a suspend action has been triggered, do it
 * parameters  - thr
 * returns     - void
 *************************************************************************/
static void
doTriggerActionSuspend(OMR_VMThread *thr)
{
	RAS_DBGOUT((stderr, "<RAS> Trigger suspend\n"));
	trcSuspend(UT_THREAD_FROM_OMRVM_THREAD(thr), UT_SUSPEND_GLOBAL);
}

/**************************************************************************
 * name        - doTriggerActionResume
 * description - a resume action has been triggered, do it
 * parameters  - thr
 * returns     - void
 *************************************************************************/
static void
doTriggerActionResume(OMR_VMThread *thr)
{
	RAS_DBGOUT((stderr, "<RAS> Trigger resume\n"));
	trcResume(UT_THREAD_FROM_OMRVM_THREAD(thr), UT_RESUME_GLOBAL);
}

/**************************************************************************
 * name        - doTriggerActionSuspendThis
 * description - a suspendthis action has been triggered, do it
 * parameters  - thr
 * returns     - void
 *************************************************************************/
static void
doTriggerActionSuspendThis(OMR_VMThread *thr)
{
	RAS_DBGOUT((stderr, "<RAS> Trigger suspendthis\n"));
	trcSuspend(UT_THREAD_FROM_OMRVM_THREAD(thr), UT_SUSPEND_THREAD);
}

/**************************************************************************
 * name        - doTriggerActionResumeThis
 * description - a resumethis action has been triggered, do it
 * parameters  - thr
 * returns     - void
 *************************************************************************/
static void
doTriggerActionResumeThis(OMR_VMThread *thr)
{
	RAS_DBGOUT((stderr, "<RAS> Trigger resumethis\n"));
	trcResume(UT_THREAD_FROM_OMRVM_THREAD(thr), UT_RESUME_THREAD);
}

/**************************************************************************
 * name        - doTriggerActionAbort
 * description - abort JVM when trigger action met
 * parameters  - thr, retcode
 * returns     - void
 *************************************************************************/
static void
doTriggerActionAbort(OMR_VMThread *thr)
{
	 abort();
}

/**************************************************************************
 * name        - doTriggerActionSegv
 * description - throw seg. violation when trigger action met
 * parameters  - thr
 * returns     - void
 *************************************************************************/
static void
doTriggerActionSegv(OMR_VMThread *thr)
{
	long *p = NULL;
	*p = 10L; /* Generates a SIGSEGV because p is NULL */
}

/**************************************************************************
 * name        - doTriggerSleep
 * description - sleep for some time when trigger action met
 * parameters  - thr
 * returns     - void
 *************************************************************************/
static void
doTriggerActionSleep(OMR_VMThread *thr)
{
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(thr);

	omrtty_printf("TRCx289: Trace sleep action triggered. Sleeping for %u ms.\n",UT_GLOBAL(sleepTimeMillis));

	omrthread_sleep(UT_GLOBAL(sleepTimeMillis));

	omrtty_printf("TRCx290: Finished sleeping.\n");
}

/**************************************************************************
 * name        - checkTriggerGroupsForTpid
 * description - called by dgtrace (et al), processes groups chain checking
 *               for group clause looking for this tpid.
 * parameters  - thr, compName, traceId, phase, actionArray
 * returns     - void
 *************************************************************************/
static void
checkTriggerGroupsForTpid(OMR_VMThread *thr, char *compName, int traceId, const TriggerPhase phase, BOOLEAN actionArray[])
{
	RasTriggerGroup *groupPtr;
	int32_t *tracePts;
	int32_t count, i;
	intptr_t oldValue;
	
	/* Increment the reference count */
	
	do {
		oldValue = UT_GLOBAL(triggerOnGroupsReferenceCount);
		
		if (oldValue < 0) {
			/* The queue is being cleared - bail out */
			return;
		}
	} while (compareAndSwapUDATA((uintptr_t *)&UT_GLOBAL(triggerOnGroupsReferenceCount),(uintptr_t)oldValue,(uintptr_t)oldValue+1) != (uintptr_t)oldValue);

	
	/*
	 * we're in here because a tracepoint has been hit for which the trigger
	 * bit was set in the active array and because the triggerOnGroups chain
	 * was non-null.
	 *
	 * So we are triggering on one or more groups.  We just have to determine
	 * whether this tpid belongs to one of the groups we are triggering on.
	 */

	/* walk down the triggerOnGroups chain */
	for (groupPtr = UT_GLOBAL(triggerOnGroups); groupPtr != NULL; groupPtr = groupPtr->next) {

		if (rasTriggerActions[groupPtr->actionIndex].phase == phase) {
			
			/*
			 * Only need to look for groups inside this component
			 */
			getComponentGroup(compName, groupPtr->groupName, &count, &tracePts);
			
			/*
			 * Check each tracepoint in the group
			 */
			for (i = 0; i < count; i++) {
				if (traceId == tracePts[i]) {
					uint32_t oldDelay;
					
					do {
						oldDelay = groupPtr->delay;
						
						if (0 == oldDelay) {
							break;
						}
					} while(compareAndSwapU32(&groupPtr->delay,oldDelay,oldDelay-1) != oldDelay);
					
					if (0 == oldDelay) {
						int32_t oldMatch;
						
						do {
							oldMatch = groupPtr->match;
							
							if (oldMatch <= 0) {
								break;
							}
						} while(compareAndSwapU32((uint32_t*)&groupPtr->match,(uint32_t)oldMatch,(uint32_t)(oldMatch-1)) != oldMatch);
						
						if (oldMatch != 0) {
							/*
							 * Do a trigger action
							 */
							RAS_DBGOUT((stderr, "tpnid %X matches group tpnid"
											" %s.%X, action=%d\n",
											traceId, compName, tracePts[i], groupPtr->action));
							actionArray[groupPtr->actionIndex] = TRUE;
						}
					} else {
						RAS_DBGOUT((stderr, "tpnid %X matches group tpnid"
										" %s.%X, action=%d\n",
										traceId, compName, tracePts[i]));
					}
				}
			}
		}
	}
	
	do {
		oldValue = UT_GLOBAL(triggerOnGroupsReferenceCount);
	} while (compareAndSwapUDATA((uintptr_t *)&UT_GLOBAL(triggerOnGroupsReferenceCount),(uintptr_t)oldValue,(uintptr_t)oldValue-1) != (uintptr_t)oldValue);
}

/**************************************************************************
 * name        - checkTriggerTpidForTpid
 * description - called by dgtrace (et al), processes tpids chain checking
 *               for tpids clause looking for this tpid.
 * parameters  - thr, compName, traceId, phase, actionArray
 * returns     - void
 *************************************************************************/
static void
checkTriggerTpidForTpid(OMR_VMThread *thr, char *compName, unsigned int traceId, const TriggerPhase phase, BOOLEAN actionArray[])
{
	RasTriggerTpidRange *ptr;
	intptr_t oldValue;

	/* Increment the reference count */
	
	do {
		oldValue = UT_GLOBAL(triggerOnTpidsReferenceCount);
		
		if (oldValue < 0) {
			/* The queue is being cleared - bail out */
			return;
		}
	} while (compareAndSwapUDATA((uintptr_t *)&UT_GLOBAL(triggerOnTpidsReferenceCount),(uintptr_t)oldValue,(uintptr_t)oldValue+1) != (uintptr_t)oldValue);

	
	/*
	 * we're in here because a tracepoint has been hit for which the trigger
	 * bit was set in the active array and because the triggerOnTpids chain
	 * was non-null.
	 *
	 * So we are triggering on some tpids.  We just have to determine whether
	 * this is one of them.
	 */

	/* walk down the triggerOnTpids chain */
	for (ptr = UT_GLOBAL(triggerOnTpids); ptr != NULL; ptr = ptr->next) {
		if (rasTriggerActions[ptr->actionIndex].phase == phase) {
			if (strcmp(compName, ptr->compName) == 0 && traceId >= ptr->startTpid && traceId <= ptr->endTpid) {
				
				uint32_t oldDelay;
				
				do {
					oldDelay = ptr->delay;
					
					if (0 == oldDelay) {
						break;
					}
				} while(compareAndSwapU32(&ptr->delay,oldDelay,oldDelay-1) != oldDelay);
				
				if (0 == oldDelay) {
					int32_t oldMatch;
					
					do {
						oldMatch = ptr->match;
						
						if (oldMatch <= 0) {
							break;
						}
					} while(compareAndSwapU32((uint32_t*)&ptr->match,(uint32_t)oldMatch,(uint32_t)(oldMatch-1)) != oldMatch);
				
					if (oldMatch != 0) {
						/*
						 * Do a trigger action
						 */
						RAS_DBGOUT((stderr, "tpnid %X matches tpnid range %s.%X-%X, "
											"action=%d\n",
											traceId, ptr->compName, ptr->startTpid, ptr->endTpid, ptr->action));
						actionArray[ptr->actionIndex] = TRUE;
					}
				} else {
					RAS_DBGOUT((stderr, "tpnid %X matches tpnid range %s.%X-%X, "
										"decrement delay\n", traceId, ptr->compName, ptr->startTpid, ptr->endTpid));				}
			}
		}
	}

	do {
		oldValue = UT_GLOBAL(triggerOnTpidsReferenceCount);
	} while (compareAndSwapUDATA((uintptr_t *)&UT_GLOBAL(triggerOnTpidsReferenceCount),(uintptr_t)oldValue,(uintptr_t)oldValue-1) != (uintptr_t)oldValue);
}

void triggerHit(OMR_VMThread *thr, char *compName, uint32_t traceId, TriggerPhase phase)
{
	BOOLEAN *actionArray = NULL;
	int i;
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));
	
	actionArray = j9mem_allocate_memory( sizeof(BOOLEAN) * numTriggerActions, OMRMEM_CATEGORY_TRACE);
	memset(actionArray,0,sizeof(BOOLEAN) * numTriggerActions);
	
	/* check for which trigger group clause(s) brought us here */
	checkTriggerGroupsForTpid(thr, compName, traceId, phase,actionArray);
	
	/* check for which trigger tpnid clause(s) brought us here */
	checkTriggerTpidForTpid(thr, compName, traceId, phase,actionArray);
	
	for (i=0;i!=numTriggerActions;i++) {
		if (actionArray[i]) {
			rasTriggerActions[i].fn(thr);
		}
	}

	j9mem_free_memory(actionArray);
}

void
clearAllTriggerActions()
{
	RasTriggerTpidRange *tpidHead = NULL;
	RasTriggerGroup *groupHead = NULL;
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));
	intptr_t oldValue;
	
	/* tpid ranges */

	/* Take the modification lock */
	omrthread_monitor_enter(UT_GLOBAL(triggerOnTpidsWriteMutex));
	
	/* Decrement the reference count - to keep the readers out */
	do {
		oldValue = UT_GLOBAL(triggerOnTpidsReferenceCount);
		
		if (oldValue > 0) {
			/* The queue is in use */
			omrthread_yield();
			continue;
		}
	} while (compareAndSwapUDATA((uintptr_t *)&UT_GLOBAL(triggerOnTpidsReferenceCount),(uintptr_t)oldValue,(uintptr_t)oldValue-1) != (uintptr_t)oldValue);

	
	tpidHead = UT_GLOBAL(triggerOnTpids);
	UT_GLOBAL(triggerOnTpids) = NULL;

	/* Increment the reference count again */
	do {
		oldValue = UT_GLOBAL(triggerOnTpidsReferenceCount);
	} while (compareAndSwapUDATA((uintptr_t *)&UT_GLOBAL(triggerOnTpidsReferenceCount),(uintptr_t)oldValue,(uintptr_t)oldValue+1) != (uintptr_t)oldValue);
	
	omrthread_monitor_exit(UT_GLOBAL(triggerOnTpidsWriteMutex));
	
	if (tpidHead != NULL) {
		RasTriggerTpidRange * current = tpidHead;
		
		do {
			RasTriggerTpidRange * toFree = current;
			current = current->next;
			
			j9mem_free_memory(toFree);
			
		} while(current != NULL);
	}
	
	/* Take the modification lock */
	omrthread_monitor_enter(UT_GLOBAL(triggerOnGroupsWriteMutex));

	/* Decrement the reference count - to keep the readers out */
	do {
		oldValue = UT_GLOBAL(triggerOnGroupsReferenceCount);
		
		if (oldValue > 0) {
			/* The queue is in use */
			omrthread_yield();
			continue;
		}
	} while (compareAndSwapUDATA((uintptr_t *)&UT_GLOBAL(triggerOnGroupsReferenceCount),(uintptr_t)oldValue,(uintptr_t)oldValue-1) != (uintptr_t)oldValue);

	groupHead = UT_GLOBAL(triggerOnGroups);
	UT_GLOBAL(triggerOnGroups) = NULL;
	
	/* Increment the reference count again */
	do {
		oldValue = UT_GLOBAL(triggerOnGroupsReferenceCount);
	} while (compareAndSwapUDATA((uintptr_t *)&UT_GLOBAL(triggerOnGroupsReferenceCount),(uintptr_t)oldValue,(uintptr_t)oldValue+1) != (uintptr_t)oldValue);
	
	omrthread_monitor_exit(UT_GLOBAL(triggerOnGroupsWriteMutex));
	
	if (groupHead != NULL) {
		RasTriggerGroup * groupCurrent = groupHead;
		
		do {
			RasTriggerGroup * groupToFree = groupCurrent;
			groupCurrent = groupCurrent->next;
			
			j9mem_free_memory(groupToFree->groupName);
			j9mem_free_memory(groupToFree);
		} while(groupCurrent != NULL);
	}
}
