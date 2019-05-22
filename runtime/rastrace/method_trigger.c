/*******************************************************************************
 * Copyright (c) 2014, 2019 IBM Corp. and others
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

/* _GNU_SOURCE forces GLIBC_2.0 sscanf/vsscanf/fscanf for RHEL5 compatibility */
#if defined(LINUX) && !defined(J9ZTPF)
#define _GNU_SOURCE
#endif /* defined(__GNUC__) && !defined(J9ZTPF)*/

#include "rommeth.h"

#define  _UTE_STATIC_
#include "j9rastrace.h"

/* undefs avoid warnings caused by using multiple components in a single file*/
#undef UT_MODULE_LOADED
#undef UT_MODULE_UNLOADED
#include "ut_mt.h"

#undef UT_MODULE_LOADED
#undef UT_MODULE_UNLOADED
#include "ut_j9trc_aux.h"

static void addMethodBlockEntry(J9VMThread *thr, J9Method *method, RasTriggerMethodRule * rule);
static UDATA traceFrameCallBack(J9VMThread *vmThread, J9StackWalkState *state);

typedef enum StackFrameMethodType {NATIVE_METHOD, INTERPRETED_METHOD, COMPILED_METHOD} StackFrameMethodType;
static void uncompressedStackFrameFormatter(J9VMThread *vmThread, J9Method * method, J9UTF8 * className, J9UTF8 * methodName, J9UTF8 * sourceFile, UDATA lineNumber, UDATA programCounter, StackFrameMethodType frameType, UDATA frameCount);
static void compressionLevel1StackFrameFormatter(J9VMThread *vmThread, J9Method * method, J9UTF8 * className, J9UTF8 * methodName, J9UTF8 * sourceFile, UDATA lineNumber, UDATA programCounter, StackFrameMethodType frameType, UDATA frameCount);
static void compressionLevel2StackFrameFormatter(J9VMThread *vmThread, J9Method * method, J9UTF8 * className, J9UTF8 * methodName, J9UTF8 * sourceFile, UDATA lineNumber, UDATA programCounter, StackFrameMethodType frameType, UDATA frameCount);

typedef void (*StackTraceFormattingFunction)(J9VMThread *vmThread, J9Method * method, J9UTF8 * className, J9UTF8 * methodName, J9UTF8 * sourceFile, UDATA lineNumber, UDATA programCounter, StackFrameMethodType frameType, UDATA frameCount);

/**
 * Array for converting stack compression codes (as specified by -Xtrace:stackcompressionlevel) into function pointers
 */
static const StackTraceFormattingFunction stackTraceFormattingFunctions[] = {
	uncompressedStackFrameFormatter,
	compressionLevel1StackFrameFormatter,
	compressionLevel2StackFrameFormatter
};

#define NUM_STACK_TRACE_FORMATTING_FUNCTIONS (sizeof(stackTraceFormattingFunctions) / sizeof(stackTraceFormattingFunctions[0]))


/**************************************************************************
 * name        - rasSetTriggerTrace
 * description - Called whenever a class is loaded.
 *
 *               Checks to see if the class loaded has any methods that
 *               match the trigger method trace spec in the trigger method
 *               rules (one of these exists for each method(...) clause
 *               specified by the user in the trigger property)
 *
 *               if any matches are found, the matching method(s) need to
 *               be traced.  addMethodBlockEntry is called to adds these
 *               methods to the triggerOnMethodBlocks chain and to flick
 *               their methodblock flag bits to say "trigger on entry to or
 *               exit from this method".
 *
 * parameters  - Pointer to the current execution environment
 *               Pointer to Class loaded
 * returns     - none
 *************************************************************************/

U_8
rasSetTriggerTrace(J9VMThread *thr, J9Method *method)
{
	PORT_ACCESS_FROM_VMC(thr);
	RasMethodTable *methodTable;
	RasTriggerMethodRule *rule;
	U_8 flag = 0;


	dbg_err_printf(1, PORTLIB, "<RAS> Check for trigger method match\n");
	/*
	 * loop through all method rules on chain
	 */
	for (rule = RAS_GLOBAL(triggerOnMethods); rule != NULL; rule = rule->next) {

		/* extract method Table from trigger method rule */
		methodTable = rule->methodTable;

		/*
		 * Try to match the method.
		 */
		if (matchMethod(methodTable, method)) {

			/*
			 * create a method entry and add it to the
			 * tmbChain in the appropriate RasTriggerMethodRule
			 */
			addMethodBlockEntry(thr, method, rule);

			flag |= J9_RAS_METHOD_TRIGGERING;
		}
	}
	return flag;
}

/**************************************************************************
 * name        - rasTriggerMethod
 * description - called by rasMethodTrace when a triggered method is
 *               entered or exited.
 * parameters  - J9VMThread
 *               J9Method - ptr to method block being entered
 *               entry - true if method entry, false if method exit
 *               phase - which trigger phase this is - before or after the tracepoint
 * returns     - void
 *************************************************************************/
void
rasTriggerMethod(J9VMThread *thr, J9Method *method, I_32 entry, const TriggerPhase phase)
{
	PORT_ACCESS_FROM_VMC(thr);
	RasTriggerMethodRule *rule = NULL;

	dbg_err_printf(1, PORTLIB, "<RAS> Trigger hit for method %s: %.*s.%.*s%.*s\n",
					entry ? "entry" : "return",
					J9ROMCLASS_CLASSNAME(J9_CLASS_FROM_METHOD(method)->romClass)->length,
					J9ROMCLASS_CLASSNAME(J9_CLASS_FROM_METHOD(method)->romClass)->data,
					J9ROMMETHOD_GET_NAME(J9_CLASS_FROM_METHOD(method)->romClass, J9_ROM_METHOD_FROM_RAM_METHOD(method))->length,
					J9ROMMETHOD_GET_NAME(J9_CLASS_FROM_METHOD(method)->romClass, J9_ROM_METHOD_FROM_RAM_METHOD(method))->data,
					J9ROMMETHOD_GET_SIGNATURE(J9_CLASS_FROM_METHOD(method)->romClass, J9_ROM_METHOD_FROM_RAM_METHOD(method))->length,
					J9ROMMETHOD_GET_SIGNATURE(J9_CLASS_FROM_METHOD(method)->romClass, J9_ROM_METHOD_FROM_RAM_METHOD(method))->data);

	/*
	 * Go through each of the rules seeing if this method is one of theirs
	 */
	if( thr->javaVM->j9rasGlobalStorage == NULL ) {
		return; /* VM is terminating but some thread has run java code. */
	}
	for (rule = RAS_GLOBAL(triggerOnMethods); rule != NULL; rule = rule->next) {
		RasTriggeredMethodBlock *tmb;

		/* Is this method block triggering on this rule? */
		for (tmb = rule->tmbChain; tmb != NULL; tmb = tmb->next) {
			if (tmb->mb == method) {
				U_32 oldDelay = 0;

				/* decrement delay first (method ENTRY only) */

				do {
					oldDelay = rule->delay;

					/* This check prevents the delay being decremented
					 * more than once for a single method
					 */
					if (! entry || phase != BEFORE_TRACEPOINT) {
						break;
					}

					if (0 == oldDelay) {
						break;
					}
				} while(compareAndSwapU32(&rule->delay,oldDelay,oldDelay-1) != oldDelay);

				if ( (entry && rule->entryAction && rule->entryAction->phase == phase) ||
					(!entry && rule->exitAction && rule->exitAction->phase == phase) ) {

					if (oldDelay == 0) {
						/* Now check for matchcount value */
						I_32 oldMatch;

						do {
							oldMatch = rule->match;

							if (oldMatch <= 0) {
								break;
							}
						} while(compareAndSwapU32((U_32*)&rule->match,(U_32)oldMatch,(U_32)(oldMatch-1)) != oldMatch);

						if (oldMatch != 0) {
							const struct RasTriggerAction *action;

							if (entry) {
								action = rule->entryAction;
							} else {
								action = rule->exitAction;
							}
							if (action) {
								action->fn(thr->omrVMThread);
							}
						}
					}
				}
			}
		}
	}
}

/**************************************************************************
 * name        - addMethodBlockEntry
 * description - creates a triggered method block entry (tmb) and adds it
 *               to the tmbChain in the appropriate RasTriggerMethodRule
 * parameters  - Pointer to the current execution environment
 *               Pointer to methodBlock loaded
 * returns     - none
 *************************************************************************/
static void
addMethodBlockEntry(J9VMThread *thr, J9Method *method, RasTriggerMethodRule * rule)
{
	PORT_ACCESS_FROM_VMC(thr);
	RasTriggeredMethodBlock *tmb;
	RasTriggeredMethodBlock *ptr;

	/*
	 * Allocate a triggered method Block entry
	 */
	if ((tmb = j9mem_allocate_memory(sizeof(RasTriggeredMethodBlock), OMRMEM_CATEGORY_TRACE)) == NULL) {
		dbg_err_printf(1, PORTLIB, "<UT> Out of memory processing trigger property.");
	} else {
		/*
		 * Initialize the RasTriggeredMethodBlock
		 */
		memset(tmb, '\0', sizeof(RasTriggeredMethodBlock));
		tmb->next = NULL;
		tmb->mb = method;

		/*
		 * add the triggered method block (tmb) into the triggered method
		 * block chain in the trigger method rule
		 */
		if (rule->tmbChain == NULL) {
			rule->tmbChain = tmb;
		} else {
			for (ptr = rule->tmbChain; ptr->next != NULL; ptr = ptr->next) { /* do nothing */
			}
			ptr->next = tmb;
		}
	}
}

/**************************************************************************
 * name        - setMethodSpec
 * description - Set class/method selection spec
 * parameters  - thread, selection string, receiving field pointers
 * returns     - JNI return code
 *************************************************************************/
omr_error_t
setMethodSpec(J9JavaVM *vm, char *value, J9UTF8 ** utf8Address, int *matchFlag)
{
	omr_error_t rc = OMR_ERROR_NONE;
	U_32 resultFlag;
	const char *resultString;
	UDATA resultLength;
	PORT_ACCESS_FROM_JAVAVM(vm);

	dbg_err_printf(1, PORTLIB, "<RAS> Set method spec: %s\n", value);

	*utf8Address = NULL;

	if (NULL != value) {
		if (parseWildcard(value, strlen(value), &resultString, &resultLength, &resultFlag)) {
			vaReportJ9VMCommandLineError(PORTLIB, "Invalid wildcard in method trace");
			rc = OMR_ERROR_ILLEGAL_ARGUMENT;
		} else {
			*matchFlag = resultFlag;
			if (resultLength > 0) {
				J9UTF8 *utf8 = j9mem_allocate_memory(resultLength + sizeof(J9UTF8), OMRMEM_CATEGORY_TRACE);
				if (utf8 != NULL) {
					J9UTF8_SET_LENGTH(utf8, (U_16)resultLength);
					memcpy(J9UTF8_DATA(utf8), resultString, resultLength);
					*utf8Address = utf8;
				} else {
					dbg_err_printf(1, PORTLIB, "<UT> Out of memory obtaining UTF8 for method trace\n");
					rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
				}
			}
		}
	}
	return rc;
}

/**************************************************************************
 * name        - getPositionalParm
 * description - Get a positional parameter
 * parameters  - string value of the property
 * returns     - Pointer to keyword or null. If not null, size is set
 *************************************************************************/
static const char *
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
static int
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
 * name        - setMethod
 * description - Set method trace options
 * parameters  - thr, trace options, atRuntime flag
 * returns     - JNI return code
 *************************************************************************/
omr_error_t
setMethod(J9JavaVM *vm, const char *value, BOOLEAN atRuntime)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	omr_error_t rc = OMR_ERROR_NONE;
	RasMethodTable *mt;
	RasMethodTable *nextMt;
	const char *p;
	int length = 0;
	int count;
	int i;
	char *spec;
	char *class;
	char *method;

	/*
	 *  Process methods specification
	 */

	count = getParmNumber(value);
	if (count != 0) {
		if (((mt = j9mem_allocate_memory(sizeof(RasMethodTable) * count, OMRMEM_CATEGORY_TRACE)) != NULL) &&
				((spec = j9mem_allocate_memory(strlen(value) + 1, OMRMEM_CATEGORY_TRACE)) != NULL)) {
			memset(mt, '\0', sizeof(RasMethodTable) * count);
			for (i = 1; i <= count; i++, mt++) {
				p = getPositionalParm(i, value, &length);
				if (length == 0) {
					vaReportJ9VMCommandLineError(PORTLIB, "Null method trace specification");
					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
					break;
				}

				/*
				 * Initialize some fields in the new RasMethodTable.
				 */
				if (i < count) {
					mt->next = mt + 1;
				}
				memcpy(spec, p, length);
				spec[length] = '\0';

				/*
				 *  Trace values ?
				 */

				if (length >= 3 &&
						spec[length - 2] == RAS_INPUT_RETURN_CHAR_START &&
						spec[length - 1] == RAS_INPUT_RETURN_CHAR_END) {
					mt->traceInputRetVals = TRUE;
					spec[length - 1] = '\0';
					spec[length - 2] = '\0';
				}

				if (strchr(spec, RAS_INPUT_RETURN_CHAR_START) ||
						strchr(spec, RAS_INPUT_RETURN_CHAR_END)) {
					vaReportJ9VMCommandLineError(PORTLIB, "Misplaced parentheses in method trace specification");
					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
					break;
				}

				/*
				 * Check first character for a logical not.
				 */

				if (*spec == RAS_DO_NOT_TRACE) {
					mt->includeFlag = FALSE;
					class = spec + 1;
				} else {
					mt->includeFlag = TRUE;
					class = spec;
				}

				/*
				 *  Specific methods to be traced ?
				 */

				if ((method = strchr(spec, RAS_CLASS_METHOD_SEPARATOR)) != NULL) {
					*method++ = '\0';
				}

				/*
				 * Test for a second RAS_CLASS_METHOD_SEPARATOR and return an error if found.
				 */
				if (NULL != method) {
					if (strchr(method, RAS_CLASS_METHOD_SEPARATOR) != NULL){
						vaReportJ9VMCommandLineError(PORTLIB, "Invalid pattern in method trace specification: '.' "
								"character can only be used for separation of class and method, use "
								"'/' for separation of package and class, e.g java/lang/String.length");
						rc = OMR_ERROR_ILLEGAL_ARGUMENT;
						break;
					}
				}

				if ((OMR_ERROR_NONE != setMethodSpec(vm, class, &mt->className,
								&mt->classMatchFlag))  ||
						(OMR_ERROR_NONE != setMethodSpec(vm, method, &mt->methodName, &mt->methodMatchFlag))) {
					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
					break;
				}
			}
			j9mem_free_memory(spec);
		} else {
			dbg_err_printf(1, PORTLIB,"<UT> Out of memory handling methods\n");
			rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		}
	} else {
		vaReportJ9VMCommandLineError(PORTLIB, "At least one method is required");
		rc = OMR_ERROR_ILLEGAL_ARGUMENT;
	}
	if (OMR_ERROR_NONE == rc) {
		if ((nextMt = ((RasGlobalStorage *) vm->j9rasGlobalStorage)->traceMethodTable) == NULL) {
			/*
			 *  This is the start of the chain
			 */
			((RasGlobalStorage *) vm->j9rasGlobalStorage)->traceMethodTable = mt - count;
		} else {
			/*
			 *  Chain to the last entry
			 */
			while (nextMt->next != NULL) {
				nextMt = nextMt->next;
			}
			nextMt->next = mt - count;
		}
	}
	return rc;
}

/**************************************************************************
 * name        - decimalString2Int
 * description - Convert a decimal string into an int
 * parameters  - vm        - J9JavaVM
 *               decString - ptr to the decimal string
 *               rc        - ptr to return code
 * returns     - void
 *************************************************************************/
static int
decimalString2Int(J9PortLibrary* portLibrary, const char *decString, I_32 signedAllowed, omr_error_t *rc)
{
	const char *p = decString;
	int num = -1;
	I_32 inputIsSigned = FALSE;
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
		vaReportJ9VMCommandLineError(portLibrary, "Signed number not permitted in this context \"%s\".", decString);
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
			vaReportJ9VMCommandLineError(portLibrary, "Invalid character(s) encountered in decimal number \"%s\".", decString);
			*rc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == *rc) {
		/*
		 * if it was too long or too short, error, else parse it as num
		 */
		if ((p - decString) < min_string_length || (p - decString) > max_string_length) {
			*rc = OMR_ERROR_INTERNAL;
			vaReportJ9VMCommandLineError(portLibrary, "Number too long or too short \"%s\".", decString);
		} else {
			sscanf(decString, "%d", &num);
		}
	}

	return num;
}

/**************************************************************************
 * name        - addTriggeredMethodSpec
 * description - Take a user specified method trigger rule (from the
 *               trigger property and allocate and populate a method rule
 *               structure for it (this also includes allocating and
 *               populating a method spec and method table structure.
 *
 *               The method rule structure stores the delay count for the
 *               rule, the method table structure that is used to determine
 *               if this rule is applicable to a loaded method and anchors
 *               the chain of triggered method block entries that will be
 *               used at runtime to recognise the methods that should
 *               trigger this rule
 *
 * parameters  - thr            - J9VMThread
 *               ptrMethodSpec - pointer to a single method spec
 *               entryAction   - action on method entry
 *               exitAction    - action on method exit
 *               delay         - delay before action(s) occur
 *               match         - number of times action performed
 * returns     - JNI return code
 *************************************************************************/
static omr_error_t
addTriggeredMethodSpec(J9VMThread *thr, const char *ptrMethodSpec, const struct RasTriggerAction *entryAction, const struct RasTriggerAction *exitAction, int delay, int match)
{
	PORT_ACCESS_FROM_VMC(thr);
	omr_error_t rc = OMR_ERROR_NONE;
	RasMethodTable *mt;
	char *spec;
	char *method;
	RasTriggerMethodRule *methodRule;

	dbg_err_printf(1, PORTLIB, "<RAS> Add trigger method spec to chain\n");

	/*
	 * Allocate a method rule, a method spec and a method table
	 */
	if (((mt = j9mem_allocate_memory(sizeof(RasMethodTable), OMRMEM_CATEGORY_TRACE)) == NULL) ||
			((spec = j9mem_allocate_memory(strlen(ptrMethodSpec) + 1, OMRMEM_CATEGORY_TRACE)) == NULL) ||
			((methodRule = j9mem_allocate_memory(sizeof(RasTriggerMethodRule), OMRMEM_CATEGORY_TRACE)) == NULL)) {
		rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		dbg_err_printf(1, PORTLIB, "<UT> Out of memory processing trigger property.");
	} else {

		/*
		 * First populate the RasMethodTable
		 */

		memset(mt, '\0', sizeof(RasMethodTable));

		/* copy the method spec into the allocated buffer */
		memcpy(spec, ptrMethodSpec, strlen(ptrMethodSpec));
		spec[strlen(ptrMethodSpec)] = '\0';

		/* break spec into two if it includes methods to be traced */
		if ((method = strchr(spec, RAS_CLASS_METHOD_SEPARATOR)) != NULL) {
			*method++ = '\0';
		}
		/* Test for a second RAS_CLASS_METHOD_SEPARATOR and return an error if found. */
		if (NULL != method) {
			if (strchr(method, RAS_CLASS_METHOD_SEPARATOR) != NULL){
				vaReportJ9VMCommandLineError(PORTLIB, "Invalid pattern in method trace specification: '.' "
						"character can only be used for separation of class and method, use "
						"'/' for separation of packages and class, e.g java/lang/String.length");
				return OMR_ERROR_ILLEGAL_ARGUMENT;
			}
		}

		/*
		 * set a few variables for tidiness in case we end up seeing
		 * these in a dump
		 */
		mt->traceInputRetVals = FALSE;
		mt->includeFlag = TRUE;

		/* Set the method and class name ptrs in the method table */
		if ((OMR_ERROR_NONE != setMethodSpec(thr->javaVM, spec, &mt->className, &mt->classMatchFlag)) ||
				(setMethodSpec(thr->javaVM, method, &mt->methodName, &mt->methodMatchFlag) != OMR_ERROR_NONE)) {
			rc = OMR_ERROR_ILLEGAL_ARGUMENT;
		}

		/*
		 * Now populate the RasTriggerMethodRule
		 */

		/* initialize header */
		memset(methodRule, '\0', sizeof(RasTriggerMethodRule));

		/* initialize body */
		methodRule->next = NULL; /* next method rule           */
		methodRule->tmbChain = NULL; /* method block entry chain   */
		methodRule->methodTable = mt; /* method spec etc            */
		methodRule->delay = delay; /* delay count                */
		methodRule->entryAction = entryAction; /* action on entry            */
		methodRule->exitAction = exitAction; /* action on exit             */
		methodRule->match = match; /* match count   */

		/* add this on to the triggerOnMethods chain */
		if (RAS_GLOBAL(triggerOnMethods) == NULL) {
			RAS_GLOBAL(triggerOnMethods) = methodRule;
		} else {
			RasTriggerMethodRule *ptr;
			for (ptr = RAS_GLOBAL(triggerOnMethods); ptr->next != NULL; ptr = ptr->next);
			ptr->next = methodRule;
		}

		if (methodRule->entryAction != NULL && methodRule->entryAction->name != NULL
		    && j9_cmdla_stricmp((char *)methodRule->entryAction->name, "jstacktrace") == 0) {
			/* set up the current method spec to be enabled for trace */
			setMethod(thr->javaVM, ptrMethodSpec, FALSE);
		}
		if (methodRule->exitAction != NULL && methodRule->exitAction->name != NULL
		    && j9_cmdla_stricmp((char *)methodRule->exitAction->name, "jstacktrace") == 0) {
			/* set up the current method spec to be enabled for trace */
			setMethod(thr->javaVM, ptrMethodSpec, FALSE);
		}
	}

	return rc;
}

/**************************************************************************
 * name        - processTriggerMethodClause
 * description - process a METHOD clause of the Trigger spec
 * parameters  - thr, a clause (method)
 * returns     - JNI return code
 *************************************************************************/
omr_error_t
processTriggerMethodClause(OMR_VMThread *omrThr, char *clause, BOOLEAN atRuntime)
{
	J9VMThread *thr = (J9VMThread *)omrThr->_language_vmthread;
	PORT_ACCESS_FROM_VMC(thr);
	omr_error_t rc = OMR_ERROR_NONE;
	int numParms;
	int delay = 0;
	int match = -1;
	const struct RasTriggerAction *entryAction = NULL;
	const struct RasTriggerAction *exitAction = NULL;
	const char *ptrMethodSpec = NULL;
	const char *ptrEntryAction = NULL;
	const char *ptrExitAction = NULL;
	const char *ptrDelay = NULL;
	const char *ptrMatch = NULL;
	int length = 0;
	char * localClause = NULL;

	dbg_err_printf(1, PORTLIB, "<RAS> Processing method clause: \"%s\"\n", clause);
	numParms = getParmNumber(clause);

	/*
	 * Check for clearly bogus - wrong number of parms
	 */
	if (numParms > 5) {
		vaReportJ9VMCommandLineError(PORTLIB, "Too many parameters on trigger property "
				"method clause usage: "
				"method{methodSpec[,entryAction][,exitAction][,delay][,matchcount]}");
		rc = OMR_ERROR_INTERNAL;
	}

	if (OMR_ERROR_NONE == rc) {
		UDATA bufferSize = strlen(clause) + 1;
		localClause = j9mem_allocate_memory(bufferSize, OMRMEM_CATEGORY_TRACE);

		if (NULL == localClause) {
			dbg_err_printf(1, PORTLIB,"<UT> Native allocation failure parsing -Xtrace:trigger=method{ clause");
			rc = OMR_ERROR_INTERNAL;
		} else {
			memcpy(localClause,clause,bufferSize);
		}
	}

	if (OMR_ERROR_NONE == rc) {
		char *p;

		ptrMethodSpec = getPositionalParm(1, localClause, &length);
		ptrEntryAction = getPositionalParm(2, localClause, &length);
		ptrExitAction = getPositionalParm(3, localClause, &length);
		ptrDelay = getPositionalParm(4, localClause, &length);
		ptrMatch = getPositionalParm(5, localClause, &length);

		/*
		 * change commas to nulls (all items become null-terminated strings)
		 */
		for (p = localClause; *p != '\0'; p++) {
			if (*p == ',') {
				*p = '\0';
			}
		}
	}

	/*
	 * method spec cannot be null
	 */
	if ((OMR_ERROR_NONE == rc) && ( NULL != ptrMethodSpec) && (0 == strlen(ptrMethodSpec))) {
		vaReportJ9VMCommandLineError(PORTLIB, "Method specification on trigger property (method clause) may not be null.");
		rc = OMR_ERROR_INTERNAL;
	}

	/*
	 * method spec cannot include '!', '(' or ')' - unlike Method trace
	 */
	if (OMR_ERROR_NONE == rc) {
		const char * cp;
		for (cp = ptrMethodSpec; *cp != 0; cp++) {
			if ((*cp == RAS_INPUT_RETURN_CHAR_START) || /* '(' character */
					(*cp == RAS_INPUT_RETURN_CHAR_END) || /* ')' character */
					(*cp == RAS_DO_NOT_TRACE)) { /* '!' character */
				vaReportJ9VMCommandLineError(PORTLIB, "Method specification for trigger may not include '!', '(' or ')'.");
				rc = OMR_ERROR_INTERNAL;
				break;
			}
		}
	}

	/*
	 * convert input match string into an int
	 */
	if ((OMR_ERROR_NONE == rc) && (NULL != ptrMatch)) {
		/* bad number sets rc to OMR_ERROR_INTERNAL */
		match = decimalString2Int(PORTLIB, ptrMatch, TRUE, &rc);
	}

	/*
	 * convert input delay string into an int
	 */
	if ((OMR_ERROR_NONE == rc) && (NULL != ptrDelay) && ('\0' !=*ptrDelay)) {
		/* bad number sets rc to OMR_ERROR_INTERNAL */
		delay = decimalString2Int(PORTLIB, ptrDelay, FALSE, &rc);
	}

	/*
	 * Parse entry action string
	 */
	if ((OMR_ERROR_NONE == rc) && (ptrEntryAction != NULL) && (strlen(ptrEntryAction) > 0)) {
		entryAction = parseTriggerAction(omrThr, ptrEntryAction, atRuntime);
		if (entryAction == NULL) {
			/*
			 * parseTriggerAction issues TRCx message on fail,
			 * just set rc to bad
			 */
			rc = OMR_ERROR_INTERNAL;
		}
	}

	/*
	 * Parse exit action string
	 */
	if ((OMR_ERROR_NONE == rc) && (NULL != ptrExitAction) && (strlen(ptrExitAction) > 0)) {
		exitAction = parseTriggerAction(omrThr, ptrExitAction, atRuntime);
		if (exitAction == NULL) {
			/*
			 * parseTriggerAction issues TRCx message on fail,
			 * just set rc to bad
			 */
			rc = OMR_ERROR_INTERNAL;
		}
	}

	/*
	 * check we have either an entry action or an exit action
	 */
	if ((OMR_ERROR_NONE == rc) && (NULL == entryAction && NULL == exitAction)) {
		vaReportJ9VMCommandLineError(PORTLIB, "You must specify an entry action, an exit action or both.");
		rc = OMR_ERROR_INTERNAL;
	}

	/*
	 * Create the triggered method rule
	 */
	if (OMR_ERROR_NONE == rc) {
		rc = addTriggeredMethodSpec(thr, ptrMethodSpec, entryAction, exitAction, delay, match);
	}

	if (localClause != NULL) {
		j9mem_free_memory(localClause);
	}

	return rc;
}

/*************************************************************************
 * name        - setStackCompressionLevel
 * description - Set the stackCompressionLevel global.
 * parameters  - vm, trace options, atRuntime
 * returns     - OMR error code
 *************************************************************************/
omr_error_t
setStackCompressionLevel(J9JavaVM *vm,const char *str, BOOLEAN atRuntime)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	int value, length;
	omr_error_t rc = OMR_ERROR_NONE;
	const char *p;

	if (getParmNumber(str) != 1) {
		goto err;
	}

	p = getPositionalParm(1, str, &length);

	if (length == 0 || length > 5) {
		goto err;
	}

	value = decimalString2Int(PORTLIB, p, FALSE, &rc);
	if (OMR_ERROR_NONE != rc) {
		goto err;
	}

	if (value < 0) {
		goto err;
	}

	if (value > (NUM_STACK_TRACE_FORMATTING_FUNCTIONS - 1)) {
		goto err;
	}

	RAS_GLOBAL_FROM_JAVAVM(stackCompressionLevel,vm) = (unsigned int)value;
	return OMR_ERROR_NONE;

err:
	vaReportJ9VMCommandLineError(PORTLIB, "stackcompressionlevel takes an unsigned integer value from 0 to %d", NUM_STACK_TRACE_FORMATTING_FUNCTIONS - 1);
	return OMR_ERROR_INTERNAL;
}

/**************************************************************************
 * name        - doTriggerActionJStacktrace
 * description - generate java stack trace when trigger action met
 * parameters  - thr
 * returns     - void
 *************************************************************************/
void
doTriggerActionJstacktrace(OMR_VMThread *omrThr)
{
	J9VMThread *thr = (J9VMThread*)omrThr->_language_vmthread;
	J9JavaVM *vm = thr->javaVM;
	J9StackWalkState stackWalkState;
	IDATA framesRemaining = RAS_GLOBAL(stackdepth);

	Trc_JavaStackStart(thr);

	if (!thr->threadObject) {
		Trc_MethodStackFrame(thr, "(thread has no thread object)");
		return;
	}

	stackWalkState.walkThread = thr;
	stackWalkState.flags = J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET;
	stackWalkState.skipCount = 0;
	stackWalkState.userData1 = NULL;
	stackWalkState.userData2 = (void *) framesRemaining;
	stackWalkState.frameWalkFunction = traceFrameCallBack;

	vm->walkStackFrames(thr, &stackWalkState);

	if (stackWalkState.userData1 == NULL) {
		/* No Java stack, write a tracepoint saying we have no stack*/
		Trc_NoJavaStack(thr);
	}
}

static UDATA
traceFrameCallBack(J9VMThread *vmThread, J9StackWalkState *state)
{
	IDATA framesRemaining = (IDATA) state->userData2;
	J9Method *method = state->method;
	J9JavaVM *vm = vmThread->javaVM;
	J9ROMMethod *romMethod;
	J9Class *methodClass;
	J9UTF8 *methodName;
	J9UTF8 *sourceFile = NULL;
	J9UTF8 *className;
	UDATA offsetPC = 0;
	UDATA lineNumber = -1;
	StackFrameMethodType frameType = INTERPRETED_METHOD;
	const unsigned int stackCompressionLevel = ((RasGlobalStorage *)vmThread->javaVM->j9rasGlobalStorage)->stackCompressionLevel;
	StackTraceFormattingFunction stackTraceFormatter;
	UDATA frameCount = (UDATA)state->userData1 + 1;

	if (framesRemaining == 0) {
		return J9_STACKWALK_STOP_ITERATING;
	}

	stackTraceFormatter = stackTraceFormattingFunctions[stackCompressionLevel];

	/* Store frame count in userData1 - used for next call to traceFrameCallBack and to signal that we found a frame */
	state->userData1 = (void*) frameCount;

	if (!method) {
		Trc_MissingJavaStackFrame(vmThread);
		goto out;
	}

	methodClass = J9_CLASS_FROM_METHOD(method);
	className = J9ROMCLASS_CLASSNAME(methodClass->romClass);
	romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
	methodName = J9ROMMETHOD_GET_NAME(methodClass->romClass, romMethod);

	if (romMethod->modifiers & J9AccNative) {
		frameType = NATIVE_METHOD;
	} else {
		offsetPC = state->bytecodePCOffset;

#ifdef J9VM_OPT_DEBUG_INFO_SERVER
		sourceFile = getSourceFileNameForROMClass(vm, methodClass->classLoader, methodClass->romClass);
		if (sourceFile) {
			lineNumber = getLineNumberForROMClass(vm, method, offsetPC);

		}
#endif

#ifdef J9VM_INTERP_NATIVE_SUPPORT
		if (state->jitInfo != NULL) {
			frameType = COMPILED_METHOD;
		}
#endif
	}

	stackTraceFormatter(vmThread,
		method,
		className,
		methodName,
		sourceFile,
		lineNumber,
		offsetPC,
		frameType,
		frameCount);

out:
	if (framesRemaining != -1) {
		state->userData2 = (void *) (framesRemaining - 1);
	}
	return J9_STACKWALK_KEEP_ITERATING;
}

/**
 * Walks buf until end and replaces all '/' characters with '.'
 *
 * Used for transforming VM class names into forms recognised by Java programmers
 */
static void
slashesToDots(char * const buf,const char * const end)
{
	char * p = buf;

	while(p < end) {
		if (*p == '/') {
			*p = '.';
		}

		p++;
	}
}

/**
 * Formats a stack frame into a human-readable string.
 */
static void
uncompressedStackFrameFormatter(J9VMThread *vmThread, J9Method * method, J9UTF8 * className, J9UTF8 * methodName, J9UTF8 * sourceFile, UDATA lineNumber, UDATA offsetPC, StackFrameMethodType frameType,UDATA frameCount)
{
	PORT_ACCESS_FROM_VMC(vmThread);
	char buf[1024] = "";
	char *cursor = buf;
	char *end = buf + sizeof(buf);

	cursor += j9str_printf(PORTLIB, cursor, end - cursor, "%.*s.%.*s", J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName));

	slashesToDots(buf,cursor);

	if (NATIVE_METHOD == frameType) {
		/*increment cursor here by the return value of j9str_printf if it needs to be used further*/
		j9str_printf(PORTLIB, cursor, end - cursor, " (Native Method)");
	} else {
		if (sourceFile) {
			cursor += j9str_printf(PORTLIB, cursor, end - cursor, " (%.*s", J9UTF8_LENGTH(sourceFile), J9UTF8_DATA(sourceFile));
			if (lineNumber != -1) {
				cursor += j9str_printf(PORTLIB, cursor, end - cursor, ":%zu", lineNumber);
			}
			cursor += j9str_printf(PORTLIB, cursor, end - cursor, ")");
		} else {
			cursor += j9str_printf(PORTLIB, cursor, end - cursor, " (Bytecode PC: %zu)", offsetPC);
		}

		if (COMPILED_METHOD == frameType) {
			/*increment cursor here by the return value of j9str_printf if it needs to be used further*/
			j9str_printf(PORTLIB, cursor, end - cursor, " (Compiled Code)", offsetPC);
		}
	}

	Trc_UncompressedJavaStackFrame(vmThread, frameCount, buf);
}

static char
getFrameTypeChar(const StackFrameMethodType type)
{
	switch(type) {
	case NATIVE_METHOD:
		return 'N';
	case COMPILED_METHOD:
		return 'C';
	case INTERPRETED_METHOD:
		return 'I';
	default:
		return 'U';
	};
}

/**
 * Formats a stack frame as a compressed tracepoint, replacing class.method with a method ID
 * and keeping source/line/program counter.
 */
static void
compressionLevel1StackFrameFormatter(J9VMThread *vmThread, J9Method * method, J9UTF8 * className, J9UTF8 * methodName, J9UTF8 * sourceFile, UDATA lineNumber, UDATA programCounter, StackFrameMethodType frameType,UDATA frameCount)
{
	char typeChar = getFrameTypeChar(frameType);

	if (NATIVE_METHOD == frameType) {
		Trc_CompressedJavaStackFrame_PointerWithType(vmThread,frameCount,method,typeChar);
	} else {
		if (sourceFile) {
			if (lineNumber != -1) {
				Trc_CompressedJavaStackFrame_WithFileAndLine(vmThread,frameCount,method,typeChar,J9UTF8_LENGTH(sourceFile),J9UTF8_DATA(sourceFile),lineNumber);
			} else {
				Trc_CompressedJavaStackFrame_WithFile(vmThread,frameCount,method,typeChar,J9UTF8_LENGTH(sourceFile),J9UTF8_DATA(sourceFile));
			}
		} else {
			Trc_CompressedJavaStackFrame_WithByteCode(vmThread,frameCount,method,typeChar,programCounter);
		}
	}
}

/**
 * Higher compression formatting function that formats a frame as a method ID only.
 */
static void
compressionLevel2StackFrameFormatter(J9VMThread *vmThread, J9Method * method, J9UTF8 * className, J9UTF8 * methodName, J9UTF8 * sourceFile, UDATA lineNumber, UDATA programCounter, StackFrameMethodType frameType,UDATA frameCount)
{
	Trc_CompressedJavaStackFrame_Pointer_Only(vmThread,frameCount,method);
}

/**************************************************************************
 * name        - setStackDepth
 * description - Set the stackdepth global.
 * parameters  - thr, trace options, atRuntime
 * returns     - JNI return code
 *************************************************************************/
omr_error_t
setStackDepth(J9JavaVM *vm,const char *str, BOOLEAN atRuntime)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	int value, length;
	omr_error_t rc = OMR_ERROR_NONE;
	const char *p;

	if (getParmNumber(str) != 1) {
		goto err;
	}

	p = getPositionalParm(1, str, &length);

	if (length == 0 || length > 5) {
		goto err;
	}

	value = decimalString2Int(PORTLIB, p, FALSE, &rc);
	if (rc != OMR_ERROR_NONE) {
		goto err;
	}

	if (value == 0) {
		/* Outputting zero frames of a stack is meaningless, so prevent this */
		goto err;
	}

	RAS_GLOBAL_FROM_JAVAVM(stackdepth,vm) = value;
	return OMR_ERROR_NONE;

err:
	vaReportJ9VMCommandLineError(PORTLIB, "stackdepth takes an integer value from 1 to 99999");
	return OMR_ERROR_INTERNAL;
}
