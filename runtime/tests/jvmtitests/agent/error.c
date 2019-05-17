/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
#include <stdlib.h>

#include "jvmti_test.h"

static agentEnv * errorAgentEnv = NULL;

static void errorAppend(agentEnv *env, agentError *e);


jvmtiError
initializeErrorLogger(agentEnv * env)
{
	errorAgentEnv = env;

	env->errorCount = 0;

	return JVMTI_ERROR_NONE;
}


void
throwError(JNIEnv * jni_env, char * error)
{
	jclass clazz;

	clazz = (*jni_env)->FindClass(jni_env, "java/lang/Error");
	if (clazz != NULL) {
		(*jni_env)->ThrowNew(jni_env, clazz, error);
	}
}


void
throwJVMTIError(agentEnv * env, JNIEnv * jni_env, jvmtiError err)
{
	char * error;
	JVMTI_ACCESS_FROM_AGENT(env);

	if ((*jvmti_env)->GetErrorName(jvmti_env, err, &error) == JVMTI_ERROR_NONE) {
		throwError(jni_env, error);
		(*jvmti_env)->Deallocate(jvmti_env, error);
	} else {
		throwError(jni_env, "Failed to retrieve JVMTI error name");
	}
}


#define JVMTI_TEST_ERROR_MSG_MAX 256

char *
errorName(agentEnv * env, jvmtiError err, char * str)
{
	char * errorString;
	char   errorStr[JVMTI_TEST_ERROR_MSG_MAX];
	JVMTI_ACCESS_FROM_AGENT(env);

	(*jvmti_env)->GetErrorName(jvmti_env, err, &errorString);
	sprintf(errorStr, "%s <%s>", str, errorString);
	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) errorString);
	return strdup(errorStr);
}


/**
 * \brief	Add an error to the agents environment structure
 * \ingroup	error
 *
 * @param[in] env 	agent environment
 * @param[in] e 	error to be added
 */
static void
errorAppend(agentEnv *env, agentError *e)
{
	agentError *error;

	if (NULL == env->errorList) {
		env->errorList = e;
	} else {
		error = env->errorList;
		while (error->next) {
			error = error->next;
		}
		error->next = e;
	}

	env->errorCount++;

	/* printErrorMessages(env); */

	return;
}


agentError *
errorAtIndex(agentEnv *env, int index)
{
	int i = 0;
	agentError *e = env->errorList;

	while (e) {

		if (i == index) {
			return e;
		}

		i++;
		e = e->next;
	}

	return NULL;
}


/**
 * \brief	Log an error with the agent environment
 * \ingroup 	error
 *
 * @param[in] env 	agent environment
 * @param[in] err 	jvmti error code
 * @param[in] file 	source file where the error was logged, NULL if unavailable
 * @param[in] fnname	function name where the error was logged, NULL if unavailable
 * @param[in] line 	line where the error was logged, NULL if unavailable
 * @param[in] format 	vaarg list containing user messages
 * @return initialized agent error structure
 *
 *	This call creates and adds an <code>agentError</code> to the agent environment.
 */
static agentError *
__error(agentEnv * env, jvmtiError err, agentErrorType errType, char *file, const char *fnname, int line, char * errorMsg)
{
	agentError *e;
	char *errorName = "Unknown JVMTI error";
	JVMTI_ACCESS_FROM_AGENT(env);


	e = malloc(sizeof(agentEnv));
	if (NULL == e) {
		env->errorCount++;
		return NULL;
	}

	/* get the full error name for err code */
	(*jvmti_env)->GetErrorName(jvmti_env, err, &errorName);

	/* Create an error */
	e->error = err;
	e->type = errType;
	e->errorName = errorName;
	e->file = strdup(file);
	e->function = strdup(fnname);
	e->line = line;
	e->message = strdup(errorMsg);
	e->next = NULL;

	errorAppend(env, e);

	return e;
}


agentError *
_error(agentEnv * env, jvmtiError err, agentErrorType errType, char *file, const char *fnname, int line, char * format, ...)
{
	va_list args;
	agentError *agentErr;
	char errorMsg[JVMTI_TEST_ERROR_MSG_MAX];

	va_start(args, format);
	vsnprintf(errorMsg, JVMTI_TEST_ERROR_MSG_MAX, format, args);
	va_end(args);

	agentErr = __error(env, err, errType, file, fnname, line, errorMsg);

	return agentErr;
}


#if defined (WIN32) || defined(WIN64) || defined(J9ZOS390)
/**
 * Do a crazy chicken dance since MSVC does not support passing va_args via a macro
 */
agentError *
error(agentEnv * env, jvmtiError err, char * format, ...)
{
	va_list args;
	agentError *agentErr;
	char errorMsg[JVMTI_TEST_ERROR_MSG_MAX];

	va_start(args, format);

	vsnprintf(errorMsg, JVMTI_TEST_ERROR_MSG_MAX, format, args);

	va_end(args);

	agentErr = __error(env, err, JVMTI_TEST_ERR_TYPE_HARD, NULL, NULL, 0, errorMsg);

	return agentErr;
}
#endif

#if defined (WIN32) || defined(WIN64) || defined(J9ZOS390)
/**
 * Do a crazy chicken dance since MSVC does not support passing va_args via a macro
 */
agentError *
softError(agentEnv * env, jvmtiError err, char * format, ...)
{
	va_list args;
	agentError *agentErr;
	char errorMsg[JVMTI_TEST_ERROR_MSG_MAX];

	va_start(args, format);

	vsnprintf(errorMsg, JVMTI_TEST_ERROR_MSG_MAX, format, args);

	va_end(args);

	agentErr = __error(env, err, JVMTI_TEST_ERR_TYPE_SOFT, NULL, NULL, 0, errorMsg);

	return agentErr;
}
#endif


#if 0
static char *
_errorPrettyString(agentEnv *env, agentError *e)
{
	int bytesWritten;
	char *errorPtr;
	char errorMsg[JVMTI_TEST_ERROR_MSG_MAX];
	JVMTI_ACCESS_FROM_AGENT(env);

	errorPtr = errorMsg;


	/* dump the error name to the return buffer */
	if (NULL != e->file && e->function != NULL) {
		bytesWritten = sprintf(errorMsg, "[%s %s:%d <%s> ", e->file, e->function, e->line, e->errorName);			} else {
		bytesWritten = sprintf(errorMsg, "<%s> ", e->errorName);
	}
	if (bytesWritten >=0) {
		/* append the user message */
		errorPtr += bytesWritten;
		strcpy(errorPtr, e->message);
	} else {
		/* we've failed to do the pretty printing.. fallback to a regular copy */
		strncpy(errorMsg, e->errorName, strlen(e->errorName));
	}

	errorPtr = strdup(errorMsg);

printf("\t%s\n", errorPtr);

	return errorPtr;
}
#endif



jboolean
hasErrors()
{
	if (errorAgentEnv->errorCount == 0) {
		return JNI_FALSE;
	} else {
		return JNI_TRUE;
	}
}

JNIEXPORT jboolean JNICALL
Java_com_ibm_jvmti_tests_util_ErrorControl_hasErrors()
{
	return hasErrors();
}


jint
getErrorCount(void)
{
	return errorAgentEnv->errorCount;
}

JNIEXPORT jint JNICALL
Java_com_ibm_jvmti_tests_util_ErrorControl_getErrorCount()
{
	return getErrorCount();
}


/**
 * \brief Retrieve Error details
 * @param errorIndex	index of the error. Index of 0 indicates the first error
 * @return true on successful error retrieval or false in case the index does not indicate a valid error
 *
 * Initialize the error detail fields of this <code>Error</code> instance.
 */
JNIEXPORT jboolean JNICALL
Java_com_ibm_jvmti_tests_util_Error_getErrorDetails(JNIEnv *env, jobject obj, int errorIndex)
{
	jfieldID fid;   /* store the field ID */
	jstring jstr;
	agentError * e;
	jclass cls;

	e = errorAtIndex(errorAgentEnv, errorIndex);
	if (NULL == e) {
		return JNI_FALSE;
	}

	cls = (*env)->GetObjectClass(env, obj);

	/* Set the Error Code */

	fid = (*env)->GetFieldID(env, cls, "error", "I");
	if (fid == NULL) {
		return JNI_TRUE;
	}
	(*env)->SetIntField(env, obj, fid, e->error);

	/* Set the Error Type */

	fid = (*env)->GetFieldID(env, cls, "type", "I");
	if (fid == NULL) {
		return JNI_TRUE;
	}
	(*env)->SetIntField(env, obj, fid, e->type);


	/* Set the ErrorName */

	fid = (*env)->GetFieldID(env, cls, "errorName", "Ljava/lang/String;");
	if (fid == NULL) {
		return JNI_TRUE;
	}
	if (e->errorName == NULL) {
		jstr = (*env)->NewStringUTF(env, "UNKNOWN - FIXME");
	} else {
		jstr = (*env)->NewStringUTF(env, e->errorName);
	}
	if (jstr == NULL) {
		return JNI_TRUE;
	}
	(*env)->SetObjectField(env, obj, fid, jstr);

	/* Set the File */

	fid = (*env)->GetFieldID(env, cls, "file", "Ljava/lang/String;");
	if (fid == NULL) {
		return JNI_TRUE;
	}
	if (e->file == NULL) {
 		jstr = (*env)->NewStringUTF(env, "");
	} else {
 		jstr = (*env)->NewStringUTF(env, e->file);
	}
	if (jstr == NULL) {
		return JNI_TRUE;
	}
	(*env)->SetObjectField(env, obj, fid, jstr);


	/* Set the Function */

	fid = (*env)->GetFieldID(env, cls, "function", "Ljava/lang/String;");
	if (fid == NULL) {
		return JNI_TRUE;
	}
	if (e->function == NULL) {
		jstr = (*env)->NewStringUTF(env, "");
	} else {
		jstr = (*env)->NewStringUTF(env, e->function);
	}
	if (jstr == NULL) {
		return JNI_TRUE;
	}
	(*env)->SetObjectField(env, obj, fid, jstr);


	/* Set the Line number */

	fid = (*env)->GetFieldID(env, cls, "line", "I");
	if (fid == NULL) {
		return JNI_TRUE;
	}
	(*env)->SetIntField(env, obj, fid, e->line);


	/* Set the Message */

	fid = (*env)->GetFieldID(env, cls, "message", "Ljava/lang/String;");
	if (fid == NULL) {
		return JNI_TRUE;
	}
	if (e->message == NULL) {
		jstr = (*env)->NewStringUTF(env, "");
	} else {
		jstr = (*env)->NewStringUTF(env, e->message);
	}
	if (jstr == NULL) {
		return JNI_TRUE;
	}
	(*env)->SetObjectField(env, obj, fid, jstr);

	return JNI_TRUE;
}



int
getErrorMessage(agentEnv *env, agentError *e, char *buf, int bufSize)
{
	char *p = buf;

	p += sprintf(p, "\tError: %d %s  %s\n", e->error,
				e->type == JVMTI_TEST_ERR_TYPE_HARD ? "" : "[SOFT]",
				e->errorName);
	p += sprintf(p, "\t\t%s\n", e->message);
	p += sprintf(p, "\t\tLocation: %s -> [%s():%d]\n",
						e->file ? e->file : "null",
						e->function ? e->function : "null",
						e->line);

	return (I_32)(p - buf);
}

jboolean
getErrorMessages(agentEnv *env, char *buf, int bufSize)
{
	char *b = buf;
	int writeSize, bSize = bufSize;
	agentError *e = env->errorList;

	while (e) {
		writeSize = getErrorMessage(env, e, b, bSize);
		bSize -= writeSize;
		b += writeSize;
		e = e->next;
		if (bSize < 0) {
			return JNI_FALSE;
		}
	}

	return JNI_TRUE;
}


void
printErrorMessages(agentEnv *env)
{
	char *b;
	jboolean rc;

	b = malloc(32 * 1024);

	rc = getErrorMessages(env, b, 32 * 1024);

	fprintf(stderr, "%s\n", b);

	return;
}




