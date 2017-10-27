/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include <string.h>

#include "jvmti_test.h"



/**
 *  A list of all known tests. While adding new tesst, this is the only place that
 *  needs to be updated in order for the agent to recognize the test.
 */
static jvmtiTest jvmtiTestList[] =
{
	/* "fer001"       -> Name of the Test
	 *  fer001        -> C agent code to initialize the test on agent start
	 * "com...fer001" -> Class name of the java test code for this test
	 * "Force..."     -> JVMTI API test is testing, or a description */

	{ "ecflh001", ecflh001, "com.ibm.jvmti.tests.eventClassFileLoadHook.ecflh001", "EventClassFIleLoadHook - check for class file load events" },
	{ "ets001", ets001, "com.ibm.jvmti.tests.eventThreadStart.ets001", "EventThreadStart - check for thread start events" },
	{ "evmoa001", evmoa001, "com.ibm.jvmti.tests.eventVMObjectAllocation.evmoa001", "EventVMObjectAllocate - tests based around object allocation events" },
	{ "emeng001", emeng001, "com.ibm.jvmti.tests.eventMethodEntryGrow.emeng001", "EventMethodEntryGrow - check growing the stack during MethodEntry event of a native method" },
	{ "emex001", emex001, "com.ibm.jvmti.tests.eventMethodExit.emex001", "EventMethodExit - check firing of Method Exit events" },
	{ "fer001", fer001, "com.ibm.jvmti.tests.forceEarlyReturn.fer001", "ForceEarlyReturn - check return values - depracated in favour of fer003" },
	{ "fer002", fer002, "com.ibm.jvmti.tests.forceEarlyReturn.fer002", "ForceEarlyReturn - check method exit event and return value passed through it" },
	{ "fer003", fer003, "com.ibm.jvmti.tests.forceEarlyReturn.fer003", "ForceEarlyReturn - check return values" },
	{ "ioioc001", ioioc001, "com.ibm.jvmti.tests.iterateOverInstancesOfClass.ioioc001", "IterateOverInstancesOfClass " },
	{ "ith001", ith001, "com.ibm.jvmti.tests.iterateThroughHeap.ith001", "IterateThroughHeap" },
	{ "ioh001", ioh001, "com.ibm.jvmti.tests.iterateOverHeap.ioh001", "IterateOverHeap" },
	{ "re001", re001, "com.ibm.jvmti.tests.resourceExhausted.re001", "ResourceExhausted OutOfMemory" },
	{ "re002", re002, "com.ibm.jvmti.tests.resourceExhausted.re002", "ResourceExhausted Thread" },
	{ "fr002", fr002, "com.ibm.jvmti.tests.followReferences.fr002", "FollowReferences" },
	{ "fr003", fr003, "com.ibm.jvmti.tests.followReferences.fr003", "FollowReferences" },
	{ "fr004", fr004, "com.ibm.jvmti.tests.followReferences.fr004", "FollowReferences - Primitive Fields" },
	{ "gpc001", gpc001, "com.ibm.jvmti.tests.getPotentialCapabilities.gpc001", "GetPotentialCapabilities" },
	{ "gpc002", gpc002, "com.ibm.jvmti.tests.getPotentialCapabilities.gpc002", "GetPotentialCapabilities - Test retention of capabilities in latter phases" },
	{ "gst001", gst001, "com.ibm.jvmti.tests.getStackTrace.gst001", "GetStackTrace - check a predefined stack trace" },
	{ "gst002", gst002, "com.ibm.jvmti.tests.getStackTrace.gst002", "GetStackTrace - check return of an empty stack for a dead thread" },
	{ "gste001", gste001, "com.ibm.jvmti.tests.getStackTraceExtended.gste001", "GetStackTraceExtended" },
	{ "gaste001", gaste001, "com.ibm.jvmti.tests.getAllStackTracesExtended.gaste001", "GetAllStackTracesExtended" },
	{ "gtlste001", gtlste001, "com.ibm.jvmti.tests.getThreadListStackTracesExtended.gtlste001", "GetThreadListStackTracesExtended" },
	{ "gomsdi001", gomsdi001, "com.ibm.jvmti.tests.getOwnedMonitorStackDepthInfo.gomsdi001", "GetOwnedMonitorStackDepthInfo" },
	{ "gomi001", gomi001, "com.ibm.jvmti.tests.getOwnedMonitorInfo.gomi001", "GetOwnedMonitorInfo" },
	{ "abcl001", abcl001, "com.ibm.jvmti.tests.addToBootstrapClassLoaderSearch.abcl001", "AddToBootstrapClassLoaderSearch during OnLoad" },
	{ "abcl002", abcl002, "com.ibm.jvmti.tests.addToBootstrapClassLoaderSearch.abcl002", "AddToBootstrapClassLoaderSearch during live" },
	{ "abcl003", abcl003, "com.ibm.jvmti.tests.addToBootstrapClassLoaderSearch.abcl003", "AddToBootstrapClassLoaderSearch reject bad jar during live" },
	{ "ascl001", ascl001, "com.ibm.jvmti.tests.addToSystemClassLoaderSearch.ascl001", "AddToSystemClassLoaderSearch during OnLoad" },
	{ "ascl002", ascl002, "com.ibm.jvmti.tests.addToSystemClassLoaderSearch.ascl002", "AddToSystemClassLoaderSearch during live" },
	{ "ascl003", ascl003, "com.ibm.jvmti.tests.addToSystemClassLoaderSearch.ascl003", "AddToSystemClassLoaderSearch reject bad jar during live" },
	{ "gcf001", gcf001, "com.ibm.jvmti.tests.getClassFields.gcf001", "GetClassFields on a class with known fields" },
	{ "gcvn001", gcvn001, "com.ibm.jvmti.tests.getClassVersionNumbers.gcvn001", "GetClassVersionNumbers on a class with known version numbers" },
	{ "gctcti001", gctcti001, "com.ibm.jvmti.tests.getCurrentThreadCpuTimerInfo.gctcti001", "GetCurrentThreadCpuTimerInfo, check return of correct static data" },
	{ "ta001", ta001, "com.ibm.jvmti.tests.BCIWithASM.ta001", "RedefineClasses using ASM" },
	{ "rc001", rc001, "com.ibm.jvmti.tests.redefineClasses.rc001", "RedefineClasses" },
	{ "rc002", rc002, "com.ibm.jvmti.tests.redefineClasses.rc002", "RedefineClasses" },
	{ "rc003", rc003, "com.ibm.jvmti.tests.redefineClasses.rc003", "RedefineClasses" },
	{ "rc004", rc004, "com.ibm.jvmti.tests.redefineClasses.rc004", "RedefineClasses" },
	{ "rc005", rc005, "com.ibm.jvmti.tests.redefineClasses.rc005", "RedefineClasses" },
	{ "rc006", rc006, "com.ibm.jvmti.tests.redefineClasses.rc006", "RedefineClasses" },
	{ "rc007", rc007, "com.ibm.jvmti.tests.redefineClasses.rc007", "RedefineClasses" },
	{ "rc008", rc008, "com.ibm.jvmti.tests.redefineClasses.rc008", "RedefineClasses" },
	{ "rc009", rc009, "com.ibm.jvmti.tests.redefineClasses.rc009", "RedefineClasses" },
	{ "rc010", rc010, "com.ibm.jvmti.tests.redefineClasses.rc010", "RedefineClasses" },
	{ "rc011", rc011, "com.ibm.jvmti.tests.redefineClasses.rc011", "RedefineClasses" },
	{ "rc012", rc012, "com.ibm.jvmti.tests.redefineClasses.rc012", "RedefineClasses" },
	{ "rc013", rc013, "com.ibm.jvmti.tests.redefineClasses.rc013", "RedefineClasses" },
	{ "rc014", rc014, "com.ibm.jvmti.tests.redefineClasses.rc014", "RedefineClasses" },
	{ "rc015", rc015, "com.ibm.jvmti.tests.redefineClasses.rc015", "RedefineClasses" },
	{ "rc016", rc016, "com.ibm.jvmti.tests.redefineClasses.rc016", "RedefineClasses" },
	{ "rc017", rc017, "com.ibm.jvmti.tests.redefineClasses.rc017", "RedefineClasses" },
	{ "rc018", rc018, "com.ibm.jvmti.tests.redefineClasses.rc018", "RedefineClasses" },
	{ "gtgc001", gtgc001, "com.ibm.jvmti.tests.getThreadGroupChildren.gtgc001", "GetThreadGroupChildren" },
	{ "gtgc002", gtgc002, "com.ibm.jvmti.tests.getThreadGroupChildren.gtgc002", "3 bytes name buffer overflow" },
	{ "gts001", gts001, "com.ibm.jvmti.tests.getThreadState.gts001", "GetThreadState" },
	{ "ghftm001", ghftm001, "com.ibm.jvmti.tests.getHeapFreeTotalMemory.ghftm001", "EventGarbageCollectionCycle - check for gc cycle start/end events" },
	{ "rat001",     rat001,   "com.ibm.jvmti.tests.removeAllTags.rat001",                     "RemoveAllTags" },
	{ "ts001",       ts001,   "com.ibm.jvmti.tests.traceSubscription.ts001",                  "Register a trace subscriber" },
	{ "ts002",       ts002,   "com.ibm.jvmti.tests.traceSubscription.ts002",                  "Register a tracepoint subscriber" },
	{ "gmcpn001", gmcpn001,   "com.ibm.jvmti.tests.getMethodAndClassNames.gmcpn001",          "Get Class, Method and Package names for a set of ram method pointers" },
	{ "decomp001", decomp001, "com.ibm.jvmti.tests.decompResolveFrame.decomp001",             "Decompile method resolve frame with stacked args" },
	{ "decomp002", decomp002, "com.ibm.jvmti.tests.decompResolveFrame.decomp002",             "Intermittent single stepping throught some code" },
	{ "decomp003", decomp003, "com.ibm.jvmti.tests.decompResolveFrame.decomp003",             "Decompile at exception catch at various levels of inlining" },
	{ "vmd001",    vmd001,    "com.ibm.jvmti.tests.vmDump.vmd001",                            "VM Dump tests" },
	{ "glc001",    glc001,    "com.ibm.jvmti.tests.getLoadedClasses.glc001",                  "Verify correct return of all relevant loaded classes" },
	{ "rtc001",    rtc001,    "com.ibm.jvmti.tests.retransformClasses.rtc001",                "RetransformClasses on a class loaded by sun.misc.Unsafe" },
	{ "att001",    att001,    "com.ibm.jvmti.tests.attachOptionsTest.att001",                 "sanity test for late attach" },
	{ "log001",    log001,    "com.ibm.jvmti.tests.log.log001",                               "Log tests" },
	{ "jlm001",    jlm001,    "com.ibm.jvmti.tests.javaLockMonitoring.jlm001",                "Java lock monitoring - JlmSet, JlmDump, and JlmDumpStats" },
	{ "sca001",    sca001,    "com.ibm.jvmti.tests.sharedCacheAPI.sca001",                    "SharedCacheAPI" },
	{ "gmc001",    gmc001,    "com.ibm.jvmti.tests.getMemoryCategories.gmc001",               "GetMemoryCategories" },
	{ "gosl001",   gosl001,   "com.ibm.jvmti.tests.getOrSetLocal.gosl001",                    "Get or Set local variables" },
	{ "vgc001",    vgc001,    "com.ibm.jvmti.tests.verboseGC.vgc001",                         "Register a verbose GC subscriber" },
	{ "gjvmt001",  gjvmt001,  "com.ibm.jvmti.tests.getJ9vmThread.gjvmt001",                   "Fetch J9VMThread from a java.lang.thread instance" },
	{ "gj9m001",   gj9m001,   "com.ibm.jvmti.tests.getJ9method.gj9m001",                      "Fetch J9Method from a mid" },
	{ "rrc001",    rrc001,    "com.ibm.jvmti.tests.retransformRedefineCombo.rrc001",          "Test Retransform-Redefine combination" },
	{ "cma001",    cma001,    "com.ibm.jvmti.tests.classModificationAgent.cma001",            "Sanity test for JVMTI class transformer" },
	{ "ria001",    ria001,    "com.ibm.jvmti.tests.retransformationIncapableAgent.ria001",    "Retransformation incapable agent" },
	{ "rca001",    rca001,    "com.ibm.jvmti.tests.retransformationCapableAgent.rca001",      "Retransformation capable agent" },
	{ "rnwr001",   rnwr001,   "com.ibm.jvmti.tests.registerNativesWithRetransformation.rnwr001", "Test RegisterNatives JNI API in FSD" },
	{ "aln001",    aln001,    "com.ibm.jvmti.tests.agentLibraryNatives.aln001",               "Test natives in agent libraries" },
	{ "rbc001",   rbc001,   "com.ibm.jvmti.tests.redefineBreakpointCombo.rbc001", "Test Redefine-breakpoint combination"},
	{ "mt001",   mt001,   "com.ibm.jvmti.tests.modularityTests.mt001", "Test Modularity functions"},
	{ NULL, NULL, NULL, NULL }
};


/**
 * \brief	Get a testcase definition
 * \ingroup
 *
 * @param[in] testName Name of the test
 * @return test case definition
 */
jvmtiTest *
getTest(char *testName)
{
	jvmtiTest *t;

	if (NULL == testName)
		return NULL;

	t = &jvmtiTestList[0];
	while (t->name) {
		if (!strcmp(t->name, testName))
			return t;
		t++;
	}

	return NULL;
}

int
getTestCount()
{
	int testCount = 0;
	jvmtiTest *t;

	t = &jvmtiTestList[0];
	while (t->name) {
		testCount++;
		t++;
	}

	return testCount;
}

jvmtiTest *
getTestAtIndex(int index)
{
	int testCount = getTestCount();

	if (index > testCount)
		return NULL;

	return &jvmtiTestList[index];
}


jint
Java_com_ibm_jvmti_tests_util_TestSuite_getTestCount(JNIEnv * jni_env, jclass klazz)
{
	return getTestCount();
}

jstring
Java_com_ibm_jvmti_tests_util_TestSuite_getTestClassName(JNIEnv * jni_env, jclass klazz, jint testNumber)
{
	jstring testClassName = (*jni_env)->NewStringUTF(jni_env, jvmtiTestList[testNumber].klass);

	return testClassName;
}

