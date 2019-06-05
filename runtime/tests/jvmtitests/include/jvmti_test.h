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
#ifndef JVMTI_TEST_H_
#define JVMTI_TEST_H_

#include "j9comp.h"
#include "jvmti.h"
#include "j9cfg.h"

/* TODO: remove */
#define BUFFER_SIZE 65536

/* TODO: remove */
typedef struct threadData {
        char buffer[BUFFER_SIZE];
        char errorStr[1024];
        char threadNameStr[1024];
        char methodNameStr[1024];
        char classNameStr[1024];
        char fieldNameStr[1024];
        char typeStr[1024];
} threadData;


typedef struct agentEnv {
	JavaVM        * vm;
	jvmtiEnv      * jvmtiEnv;
	JNIEnv        * jni_env;
	threadData      globalData;
	jint        jvmtiVersion;
	char          * testName;         /** Name of the testcase being run */
	char          * testArgs;         /** Testcase arguments specified by the user via agent options */
	char          * subtest;          /** Name of specific test method to run */
	int             outputLevel;
	int             errorCount;       /** Total number of errors (not necessarily the same as number of errors on the <code>agentEnv->errorsList</code> linked list */
	struct agentError * errorList;
} agentEnv;

typedef jint (JNICALL *testStartFunction) (agentEnv * env, char *args);

typedef struct jvmtiTest {
	char                * name;
	testStartFunction     fn;
	char                * klass;
	char                * description;
} jvmtiTest;


#define JVMTI_ACCESS_FROM_AGENT(agentEnv) jvmtiEnv *jvmti_env = agentEnv->jvmtiEnv


/** Maximum size of names for various parameters such as class or method signatures etc */
#define JVMTI_TEST_NAME_MAX     1024

/** Error types */
typedef enum agentErrorType {
	JVMTI_TEST_ERR_TYPE_SOFT = 1,            /** Soft errors do not result in test failure */
	JVMTI_TEST_ERR_TYPE_HARD = 2,            /** Hard errors result in test failure */
	agentErrorTypeWideEnum = 0x1000000
} agentErrorType;

typedef struct agentError {
	jvmtiError       error;       /** JVMTI Error code */
	agentErrorType   type;        /** Type of the error, soft vs hard */
	char           * errorName;   /** JVMTI Error name */
	char           * file;        /** Source filename where the error was logged */
	char           * function;    /** Function name where the error was logged */
	int              line;        /** Line number where the error was logged */
	char           * message;     /** Error message */
	struct agentError  * next; /** Link to the next error message */
} agentError;





/* args.c */
void parseArguments(agentEnv * env, char * agentOptions);
void freeArguments(agentEnv * env);

/* tests.c */
jvmtiTest * getTest(char *testName);
jvmtiTest * getTestAtIndex(int index);

/* error.c */
jvmtiError initializeErrorLogger(agentEnv * env);
jboolean   hasErrors(void);
jint       getErrorCount(void);
int        getErrorMessage(agentEnv *env, agentError *e, char *buf, int bufSize);
jboolean   getErrorMessages(agentEnv *env, char *buf, int bufSize);
void       printErrorMessages(agentEnv *env);
agentError * _error(agentEnv * env, jvmtiError err, agentErrorType errType, char *file, const char *fnname, int line, char * format, ...);
char *     errorName(agentEnv * env, jvmtiError err, char * str);

#if defined (WIN32) || defined(WIN64) || defined(J9ZOS390)
	agentError * error(agentEnv * env, jvmtiError err, char * format, ...);
	agentError * softError(agentEnv * env, jvmtiError err, char * format, ...);
#else
	#define error(env,err,format,a...) _error(env, err, JVMTI_TEST_ERR_TYPE_HARD, __FILE__, __FUNCTION__, __LINE__, format,##a)
	#define softError(env,err,format,a...) _error(env, err, JVMTI_TEST_ERR_TYPE_SOFT, __FILE__, __FUNCTION__, __LINE__, format,##a)
#endif


/* thread.c */
threadData * getThreadData(agentEnv *env, jthread currentThread);
char * getThreadName(agentEnv *env, jthread currentThread, jthread thread);
jthread getCurrentThread(JNIEnv * jni_env);

/* message.c */
void JNICALL _jvmtiTest_printf(agentEnv *env, int level, int indentLevel, char * format, ...);
#if defined (WIN32) || defined(WIN64) || defined(J9ZOS390)
	void tprintf(agentEnv *env, int level, char * format, ...);
	void iprintf(agentEnv *env, int level, int indentLevel, char * format, ...);
#else
	#define tprintf(env, level, format, a...) _jvmtiTest_printf(env, level, 0, format, ## a)
	#define iprintf(env, level, indentLevel, format, a...) _jvmtiTest_printf(env, level, indentLevel, format, ## a)
#endif
#define dumpdata(prefix, data, len) _dumpdata(prefix, data, len)
#define dumpdata_hex_ascii(prefix, idx, data, len) _dumpdata_hex_ascii(prefix, idx, data, len)

/* help.c */
void getHelp(char *h, int helpSize);
int  getTestCount(void);
void printHelp(void);

/* version.c */
jboolean ensureVersion(agentEnv * agent_env, jint version);
const char * getVersionName(agentEnv * agent_env, jint version);

/* util.c */
void jvmtitest_usleep(UDATA millis);

/* Test entry point prototypes */
jint JNICALL ecflh001(agentEnv *env, char *args);
jint JNICALL ets001(agentEnv *env, char *args);
jint JNICALL evmoa001(agentEnv *env, char *args);
jint JNICALL emeng001(agentEnv *env, char *args);
jint JNICALL emex001(agentEnv *env, char *args);
jint JNICALL fer001(agentEnv *env, char *args);
jint JNICALL fer002(agentEnv *env, char *args);
jint JNICALL fer003(agentEnv *env, char *args);
jint JNICALL ioioc001(agentEnv * env, char * args);
jint JNICALL ith001(agentEnv * env, char * args);
jint JNICALL ioh001(agentEnv * env, char * args);
jint JNICALL ta001(agentEnv * env, char * args);
jint JNICALL rc001(agentEnv * env, char * args);
jint JNICALL rc002(agentEnv * env, char * args);
jint JNICALL rc003(agentEnv * env, char * args);
jint JNICALL rc004(agentEnv * env, char * args);
jint JNICALL rc005(agentEnv * env, char * args);
jint JNICALL rc006(agentEnv * env, char * args);
jint JNICALL rc007(agentEnv * env, char * args);
jint JNICALL rc008(agentEnv * env, char * args);
jint JNICALL rc009(agentEnv * env, char * args);
jint JNICALL rc010(agentEnv * env, char * args);
jint JNICALL rc011(agentEnv * env, char * args);
jint JNICALL rc012(agentEnv * env, char * args);
jint JNICALL rc013(agentEnv * env, char * args);
jint JNICALL rc014(agentEnv * env, char * args);
jint JNICALL rc015(agentEnv * env, char * args);
jint JNICALL rc016(agentEnv * env, char * args);
jint JNICALL rc017(agentEnv * env, char * args);
jint JNICALL rc018(agentEnv * env, char * args);
jint JNICALL rc019a(agentEnv * env, char * args);
jint JNICALL rc019b(agentEnv * env, char * args);
jint JNICALL re001(agentEnv * env, char * args);
jint JNICALL re002(agentEnv * env, char * args);
jint JNICALL fr001(agentEnv * env, char * args);
jint JNICALL fr002(agentEnv * env, char * args);
jint JNICALL fr003(agentEnv * env, char * args);
jint JNICALL fr004(agentEnv * env, char * args);
jint JNICALL gpc001(agentEnv * env, char * args);
jint JNICALL gpc002(agentEnv * env, char * args);
jint JNICALL gst001(agentEnv * env, char * args);
jint JNICALL gst002(agentEnv * env, char * args);
jint JNICALL gste001(agentEnv * env, char * args);
jint JNICALL gaste001(agentEnv * env, char * args);
jint JNICALL gtlste001(agentEnv * env, char * args);
jint JNICALL abcl001(agentEnv * env, char * args);
jint JNICALL abcl002(agentEnv * env, char * args);
jint JNICALL abcl003(agentEnv * env, char * args);
jint JNICALL ascl001(agentEnv * env, char * args);
jint JNICALL ascl002(agentEnv * env, char * args);
jint JNICALL ascl003(agentEnv * env, char * args);
jint JNICALL jni001(agentEnv * env, char * args);
jint JNICALL gcf001(agentEnv * env, char * args);
jint JNICALL gcvn001(agentEnv * env, char * args);
jint JNICALL gctcti001(agentEnv * env, char * args);
jint JNICALL gtgc001(agentEnv * env, char * args);
jint JNICALL gtgc002(agentEnv * env, char * args);
jint JNICALL gomsdi001(agentEnv * env, char * args);
jint JNICALL gomsdi002(agentEnv * env, char * args);
jint JNICALL gomi001(agentEnv * env, char * args);
jint JNICALL gomi002(agentEnv * env, char * args);
jint JNICALL gts001(agentEnv * env, char * args);
jint JNICALL ghftm001(agentEnv * env, char * args);
jint JNICALL rat001(agentEnv * env, char * args);
jint JNICALL ts001(agentEnv * env, char * args);
jint JNICALL ts002(agentEnv * env, char * args);
jint JNICALL gmcpn001(agentEnv * env, char * args);
jint JNICALL decomp001(agentEnv * env, char * args);
jint JNICALL decomp002(agentEnv * env, char * args);
jint JNICALL decomp003(agentEnv * env, char * args);
jint JNICALL decomp004(agentEnv * env, char * args);
jint JNICALL vmd001(agentEnv * env, char * args);
jint JNICALL glc001(agentEnv * env, char * args);
jint JNICALL rtc001(agentEnv * env, char * args);
jint JNICALL att001(agentEnv * env, char * args);
jint JNICALL log001(agentEnv * env, char * args);
jint JNICALL jlm001(agentEnv * env, char * args);
jint JNICALL sca001(agentEnv * env, char * args);
jint JNICALL gmc001(agentEnv * env, char * args);
jint JNICALL gosl001(agentEnv * env, char * args);
jint JNICALL vgc001(agentEnv * env, char * args);
jint JNICALL gjvmt001(agentEnv * agent_env, char * args);
jint JNICALL gj9m001(agentEnv * agent_env, char * args);
jint JNICALL rrc001(agentEnv * agent_env, char * args);
jint JNICALL rbc001(agentEnv * agent_env, char * args);
jint JNICALL cma001(agentEnv * agent_env, char * args);
jint JNICALL rca001(agentEnv * agent_env, char * args);
jint JNICALL ria001(agentEnv * agent_env, char * args);
jint JNICALL rnwr001(agentEnv * agent_env, char * args);
jint JNICALL aln001(agentEnv * agent_env, char * args);
jint JNICALL mt001(agentEnv * agent_env, char * args);
jint JNICALL nmr001(agentEnv * agent_env, char * args);
jint JNICALL snmp001(agentEnv * agent_env, char * args);
jint JNICALL soae001(agentEnv * agent_env, char * args);
jint JNICALL gsp001(agentEnv *agent_env, char *args);
jint JNICALL ee001(agentEnv *agent_env, char *args);

#endif /*JVMTI_TEST_H_*/
