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

#include <stdio.h>
#include <float.h>
#include <stdlib.h>

#if defined(WIN32)
/* windows includes */
#elif defined (OS2)
/* os/2 includes */
#else
/* Default is "some sort of Unix" */
#include <signal.h>
#include <pthread.h>
#endif

#include "j9cfg.h"
#include "jni.h"
#include "j9comp.h"
#include "jcl.h"
#include "ut_gptest.h"

UDATA invalidMemory;

#if defined(J9ZOS390)
#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)
#include <leawi.h>
#include <ceeedcct.h>
#endif
#include "atoe.h"
#endif

/* Avoid error in static analysis */
int gpTestGlobalZero = 0;
UDATA * gpTestGlobalInvalidPointer = (UDATA*)-1;

void JNICALL Java_VMBench_GPTests_GPTest_gpIll(JNIEnv *env, jclass clazz, jobject arg1);
void JNICALL Java_VMBench_GPTests_GPTest_gpFloat(JNIEnv *env, jclass clazz, jobject arg1);
void JNICALL Java_VMBench_GPTests_GPTest_gpRead(JNIEnv *env, jclass clazz, jobject arg1);
void JNICALL Java_VMBench_GPTests_GPTest_gpWrite(JNIEnv *env, jclass clazz, jobject arg1);
void JNICALL Java_VMBench_GPTests_GPTest_callBackIn(JNIEnv *env, jclass clazz, jobject arg1);

void JNICALL Java_VMBench_GPTests_GPTest_gpHardwareFloat(JNIEnv *env, jclass clazz, jobject arg1);
void JNICALL Java_VMBench_GPTests_GPTest_gpHardwareRead(JNIEnv *env, jclass clazz, jobject arg1);

void JNICALL Java_VMBench_GPTests_GPTest_gpSoftwareFloat(JNIEnv *env, jclass clazz, jobject arg1);
void JNICALL Java_VMBench_GPTests_GPTest_gpSoftwareRead(JNIEnv *env, jclass clazz, jobject arg1);

void JNICALL Java_VMBench_GPTests_GPTest_gpAbort(JNIEnv *env, jclass clazz, jobject arg1);

void JNICALL Java_VMBench_GPTests_GPTest_callInAndTriggerGPReadThenResumeAndCallInAgain(JNIEnv *env, jobject rec);
void JNICALL Java_VMBench_GPTests_GPTest_callInAndTriggerGPReadThenResumeAndReturnToJava(JNIEnv *env, jobject rec, jint count);

void callinAndTriggerGPRead(JNIEnv *env, jobject rec);
void callInAndTriggerGPReadResumeCallinImpl(JNIEnv *env, jobject rec, jobject arg1);


#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)
#define GPTEST_CONDITIONHANDLER_CEE3AB2 1 /* throw an abend */
#define GPTEST_CONDITIONHANDLER_PERCOLATE 2	/* percolate */
#define GPTEST_CONDITIONHANDLER_CEEMRCE 3 	/* move resume cursor explicit */
#define GPTEST_CONDITIONHANDLER_CEEMRCR 4	/* resume execution in relation to default location of the resume cursor */
#define GPTEST_CONDITIONHANDLER_SLEEP 5 /* sleep */

void gptest_user_condition_handler (_FEEDBACK *fc, _INT4 *token, _INT4 *leResult, _FEEDBACK *newfc);

typedef struct GPTestConditionHandlerArgs {
	UDATA action;
	_INT4 resumeToken;
} GPTestConditionHandlerArgs;


/* Standard basic LE condition handler
 *
 * Token is a pointer to a UDATA , where possible values are defined above as GPTEST_CONDITION_HANDLER_XXXXXX)
 */
void gptest_user_condition_handler (_FEEDBACK *fc, _INT4 *token, _INT4 *leResult, _FEEDBACK *newfc) {

	_INT4 code = 0xDED;	/* arbitrarily chosen abend code */
	_INT4 reason = 0;	/* arbitrarily chosen reason code*/
	_INT4 cleanup = 0;	/* terminate the process without invoking other condition handlers */
	_INT4 mrcrType = 0;	/* Move the resume cursor to the call return point of the stack frame associated with the handle cursor (which is the frame that registered this handler )*/
	char *asciiFacilityID = e2a_func(fc->tok_facid, 3);
	GPTestConditionHandlerArgs *args = (GPTestConditionHandlerArgs *) token;
	_FEEDBACK fcCEEMRCX;

#pragma convlit(suspend)
	const char* ceeEbcdic = "CEE";
#pragma convlit(resume)

	fprintf(stderr, "\n\n gptest_user_condition_handler invoked: fc->tok_msgno: %i, fc->tok_facid: %s, fc->tok_sev: %i ***\n", fc->tok_msgno, (NULL == asciiFacilityID) ? "NULL" : asciiFacilityID, fc->tok_sev);fflush(NULL);

	if ( (0 == strncmp(asciiFacilityID, "CEE", 3)) && ( (fc->tok_msgno == 199) || (fc->tok_msgno == 198)) ) {
		/* CEE0198S The termination of a thread was signaled due to an unhandled condition. */
		/* CEE0199W The termination of a thread was signaled due to a STOP statement */

		*leResult = 20; /* percolate */
		return;
	}

	free(asciiFacilityID);

	/* Note the following prototypes for the le condition handling calls we're making:
	 *
	 *	void CEEHDLR( _ENTRY *routine, _INT4 *token, _FEEDBACK *fc);
	 *	void CEEMRCR( _INT4 *type_of_move, _FEEDBACK *fc);
	 *	void CEEMRCE( _INT4 *resume_token, _FEEDBACK *fc);
	 */

	switch (args->action) {
		case GPTEST_CONDITIONHANDLER_PERCOLATE:
			fprintf(stderr, "\t gptest_user_condition_handler: percolating\n\n");fflush(NULL);
			*leResult = 20; /* percolate */
			return;
		case GPTEST_CONDITIONHANDLER_CEE3AB2:
			fprintf(stderr, "\t gptest_user_condition_handler: calling CEE3AB2()\n\n");fflush(NULL);
			CEE3AB2(&code, &reason, &cleanup);
			Assert_GPTEST_unreachable();
		case GPTEST_CONDITIONHANDLER_CEEMRCE:
			fprintf(stderr, "\t gptest_user_condition_handler: calling CEEMRCE()\n\n");fflush(NULL);
			CEEMRCE(&args->resumeToken, &fcCEEMRCX);
			/* if we got here we failed - why did we fail? */
			if ( _FBCHECK ( fcCEEMRCX , CEE000 ) != 0 ) {
				printf("\n\n *** ERROR: gptest_user_condition_handler: CEEMRCE failed with message number %d *** \n\n", fcCEEMRCX.tok_msgno);fflush(NULL);
				exit(31);
			}
			*leResult = 10; /* resume */
			return;
		case GPTEST_CONDITIONHANDLER_CEEMRCR:
			fprintf(stderr, "\t gptest_user_condition_handler: calling CEEMRCR()\n\n");fflush(NULL);
			CEEMRCR(&mrcrType, &fcCEEMRCX);
			/* if we got here we failed - why did we fail? */
			if ( _FBCHECK ( fcCEEMRCX , CEE000 ) != 0 ) {
				printf("\n\n *** ERROR: gptest_user_condition_handler: CEEMRCR failed with message number %d *** \n\n", fcCEEMRCX.tok_msgno);fflush(NULL);
				exit(31);
			}
			*leResult = 10; /* resume */
			return;
		case GPTEST_CONDITIONHANDLER_SLEEP:
			sleep(20000);
		/* default is to fall through */

	}

	Assert_GPTEST_unreachable();
}

void
callInAndTriggerGPReadThenResume(JNIEnv *env, jobject rec)
{

#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)
	_FEEDBACK fc;
	_ENTRY leConditionHandler;
	GPTestConditionHandlerArgs handlerArgs;
	handlerArgs.action = GPTEST_CONDITIONHANDLER_CEEMRCR;

	memset(&fc, 0, sizeof(_FEEDBACK));
	memset(&leConditionHandler, 0, sizeof(_ENTRY));

	leConditionHandler.address = (_POINTER)&gptest_user_condition_handler ;
	leConditionHandler.nesting = NULL;

	/* register user condition handler */
	CEEHDLR(&leConditionHandler, (_INT4 *)&handlerArgs, &fc);

	/* verify that CEEHDLR was successful */
	if ( _FBCHECK ( fc , CEE000 ) != 0 ) {

		if (256 == fc.tok_msgno) {	/* 256 indicates handler was already registered */
			printf("\n\n ***Java_VMBench_GPTests_GPTest_callBackInAndResume: CEEHDLR came back with message %d (handler was already registered, this is just a warning)\n\n", fc.tok_msgno);fflush(NULL);
		} else {
			printf("\n\n *** ERROR: Java_VMBench_GPTests_GPTest_callBackInAndResume: CEEHDLR failed with message number %d *** \n\n", fc.tok_msgno);fflush(NULL);
			exit(31);
		}
	}


#endif

	printf("\tcalling into Java\n");fflush(NULL);

	callinAndTriggerGPRead(env, rec);

	printf("\n\n *** Java_VMBench_GPTests_GPTest_callBackIn returning *** \n\n");fflush(NULL);

}

#endif /* J9VM_PORT_ZOS_CEEHDLRSUPPORT */

/*
 * Try calling back into Java after resuming from the condition handler.
 *
 * This should trigger a fatal assert.
 *
 * 0. Sets a resume point
 * 1. Registers a condition handler that resumes execution
 * 2. Calls back into Java.
 * 3. Handler catches condition and resumes execution
 * 4. Attempts to call back into Java
 */
void JNICALL
Java_VMBench_GPTests_GPTest_callInAndTriggerGPReadThenResumeAndCallInAgain(JNIEnv *env, jobject rec)
{
#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)

	/* registers the gptest user condition handler and triggers a gp read */
	callInAndTriggerGPReadThenResume(env, rec);

	/* try calling back in */
	callInAndTriggerGPReadThenResume(env, rec);

#endif /* J9VM_PORT_ZOS_CEEHDLRSUPPORT */
}

/*
 * Try calling back into Java after resuming from the condition handler.
 *
 * This should trigger a fatal assert.
 *
 * 0. Sets a resume point
 * 1. Registers a condition handler that resumes execution
 * 2. Calls back into Java.
 * 3. Handler catches condition and resumes execution
 * 4. Returns back to Java
 */
void JNICALL
Java_VMBench_GPTests_GPTest_callInAndTriggerGPReadThenResumeAndReturnToJava(JNIEnv *env, jobject rec, jint count)
{

#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)

	printf("Java_VMBench_GPTests_GPTest_callInAndTriggerGPReadThenResumeAndReturnToJava: %u\n", count);
	if (count == 10) {
		/* registers the gptest user condition handler and triggers a gp read */
		callInAndTriggerGPReadThenResume(env, rec);
	}

	printf("returning to java\n");fflush(NULL);
	return;

#endif

}

void callinAndTriggerGPRead(JNIEnv *env, jobject rec)
{
	jclass clazz;
	jint intField = 0;
	jmethodID mid = NULL;

	clazz = (*env)->GetObjectClass(env, rec);

	mid = (*env)->GetMethodID(env, clazz, "callOutAndTriggerGPRead", "()V");

	if ( (mid == NULL) || (clazz == NULL) ) {
		printf("\n\n *** ERROR: Java_VMBench_GPTests_GPTest_callBackIn: GetMethodID returned NULL *** \n\n");
		exit(33);
	}

	(*env)->DeleteLocalRef(env, clazz);

	(*env)->CallVoidMethod(env, rec, mid);

}


void JNICALL
Java_VMBench_GPTests_GPTest_gpAbort(JNIEnv *env, jclass clazz, jobject arg1)
{
	abort();
}


void JNICALL
Java_VMBench_GPTests_GPTest_gpHardwareFloat(JNIEnv *env, jclass clazz, jobject arg1)
{
#if defined(WIN32)

	float one, zero, error;
	_fpreset();
	_controlfp(EM_INEXACT | EM_DENORMAL , MCW_EM);

	one = (float)atof("1.0");
	zero = (float)atof("0.0");
	error = one / zero;

	/* may be optimized away unless we do something with error */
	printf ("%f / %f = %f\n", one, zero, error);
#else

	int a = 10; /* clang optimizes the case of dividend==1 */
	int b = gpTestGlobalZero; /* Avoid error in static analysis */
	int c = a/b;
	printf ("%i / %i = %i\n", a, b, c);	 /* here to stop compiler from optimizing out the div-by-zero */

#endif
}

void JNICALL
Java_VMBench_GPTests_GPTest_gpHardwareRead(JNIEnv *env, jclass clazz, jobject arg1)
{
	/* Avoid error in static analysis */
	invalidMemory = *gpTestGlobalInvalidPointer;
}

void JNICALL
Java_VMBench_GPTests_GPTest_gpSoftwareFloat(JNIEnv *env, jclass clazz, jobject arg1)
{
#if !defined(WIN32)
#if 1
	pthread_kill(pthread_self(), SIGFPE);
#else
	/* this does not work with the current Java testcase as it might simply complete execution
	 * and hence terminate the process before the abort handler
	 * completes executing.
	 *
	 * We would need to differentiate from pthread_kill(pthread_self()) in the expected results for this to work */
	kill(getpid(), SIGFPE);
#endif
#endif
}

void JNICALL
Java_VMBench_GPTests_GPTest_gpSoftwareRead(JNIEnv *env, jclass clazz, jobject arg1)
{
#if !defined(WIN32)
#if 1
	pthread_kill(pthread_self(), SIGSEGV);
#else
	/* this does not work with the current Java testcase as it might simply complete execution
	 * and hence terminate the process before the abort handler
	 * completes executing.
	 *
	 * We would need to differentiate from pthread_kill(pthread_self()) in the expected results for this to work */
	kill(getpid(), SIGSEGV); /* this is not valid as the java testcase might simply complete execution and hence terminate the process before the abort handler completes executing */
#endif
#endif
}

/*
 * 1. Registers a condition handler that percolates,
 * 2. Calls back into Java.
 */
void JNICALL
Java_VMBench_GPTests_GPTest_callBackIn(JNIEnv *env, jobject rec, jobject arg1)
{

#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)
	_FEEDBACK fc;
	_ENTRY leConditionHandler;
	GPTestConditionHandlerArgs handlerArgs;

	memset(&fc, 0, sizeof(_FEEDBACK));
	memset(&leConditionHandler, 0, sizeof(_ENTRY));

	leConditionHandler.address = (_POINTER)&gptest_user_condition_handler ;
	leConditionHandler.nesting = NULL;

	handlerArgs.action = GPTEST_CONDITIONHANDLER_PERCOLATE;

	/* register user condition handler */
	CEEHDLR(&leConditionHandler, (_INT4 *)&handlerArgs, &fc);

	/* verify that CEEHDLR was successful */
	if ( _FBCHECK ( fc , CEE000 ) != 0 ) {
		printf("\n\n *** ERROR: Java_VMBench_GPTests_GPTest_callBackIn: CEEHDLR failed with message number %d *** \n\n", fc.tok_msgno);fflush(stdout);
		exit(31);
	}
#endif

	callinAndTriggerGPRead(env, rec);

	printf("\n\n *** Java_VMBench_GPTests_GPTest_callBackIn 3 *** \n\n");fflush(NULL);

}


void JNICALL
Java_VMBench_GPTests_GPTest_gpFloat(JNIEnv *env, jclass clazz, jobject arg1)
{
#if defined(WIN32)
	float one, zero, error;
#endif

#if defined(WIN32)
	_fpreset();
	_controlfp(EM_INEXACT | EM_DENORMAL , MCW_EM);

	one = (float)atof("1.0");
	zero = (float)atof("0.0");
	error = one / zero;

	/* may be optimized away unless we do something with error */
	printf ("%f / %f = %f\n", one, zero, error);
	
#elif defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)

	int a = 1;
	int b = 0;
	int c;

	_FEEDBACK fc;
	_ENTRY leConditionHandler;
	_INT4 handlerAction = GPTEST_CONDITIONHANDLER_PERCOLATE;

	memset(&fc, 0, sizeof(_FEEDBACK));
	memset(&leConditionHandler, 0, sizeof(_ENTRY));

	leConditionHandler.address = (_POINTER)&gptest_user_condition_handler ;
	leConditionHandler.nesting = NULL;

	CEEHDLR(&leConditionHandler, &handlerAction, &fc);

	/* verify that CEEHDLR was successful */
	if ( _FBCHECK ( fc , CEE000 ) != 0 ) {
		printf("\n\n *** ERROR: Java_VMBench_GPTests_GPTest_gpFloat: CEEHDLR failed with message number %d *** \n\n", fc.tok_msgno);fflush(stdout);
		exit(31);
	}

	c = a/b;
	printf ("%i / %i = %i\n", a, b, c);	 /* here to stop compiler from optimizing out the div-by-zero */

#else
	/* Default is "some sort of Unix" */
	pthread_kill(pthread_self(), SIGFPE);
#endif
}



/* Java_VMBench_GPTests_GPTest_gpIll */
void JNICALL 
Java_VMBench_GPTests_GPTest_gpIll(JNIEnv *env, jclass clazz, jobject arg1) {

#ifdef WIN32

	/* SIGILL test needs to use an an invalid opcode. 0x0b0f is the invalid opcode "UD2" reserved for testing on IA32 and AMD64.
	 * 		and is guaranteed not to become legal. 
     * On AMD64, however, we get an "ACCESS_VIOLATION" when we execute it instead of an "ILLEGAL_INSTRUCTION",
	 * 		which is likely a bug in the AMD64 Windows OS.
	*/
 
	static unsigned int buffer[] = {  0x0B0F0B0F };
	void (*function)() = (void (*)()) buffer;
	function();

#else

	pthread_kill(pthread_self(), SIGILL);

#endif
}



void JNICALL 
Java_VMBench_GPTests_GPTest_gpRead(JNIEnv *env, jclass clazz, jobject arg1)
{
#if defined(LINUX) && defined(S390)

	/* above allow reads from memory at 0 and -ve locations */
	raise(SIGSEGV);

#else

	/* all other platforms */
	/* try to read some memory we shouldn't be allowed to see */
	char* ptr = (char*)gpTestGlobalInvalidPointer; /* Avoid error in static analysis */
	ptr -= 4;

	if (*ptr) {
		printf ("Oops -- we read it and it's non-zero");
	} else {
		printf ("Oops -- we read it and it's zero");
	}

#endif
}


void JNICALL 
Java_VMBench_GPTests_GPTest_gpWrite(JNIEnv *env, jclass clazz, jobject arg1)
{
#if defined(LINUX) && defined(S390)

	/* s390 linux allows reads and writes from memory at 0 and -ve locations */
	raise(SIGSEGV);

#else

	/* all other platforms */
	/* try to write some memory we shouldn't be allowed to see */
	char* ptr = (char*)gpTestGlobalInvalidPointer; /* Avoid error in static analysis */
	ptr -= 4;
	*ptr = '@';

#endif
}
