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


/*
 * $RCSfile: si_numcpusTest.c,v $
 * $Revision: 1.7 $
 * $Date: 2012-08-28 17:00:47 $
 */
#include "testHelpers.h"

/**
 * Get number of physical CPUs. Validate number > 0.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASSED on success, TEST_FAILED on failure
 */
int j9sysinfo_numcpus_test1 (J9PortLibrary* portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_numcpus_test1";
	UDATA numberPhysicalCPUs = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_PHYSICAL);

	outputComment(PORTLIB,"\n");
	outputComment(PORTLIB,"Get number of physical CPUs.\n");
	outputComment(PORTLIB,"	Expected: >0\n");
	outputComment(PORTLIB,"	Result:   %d\n", numberPhysicalCPUs);

	if (0 == numberPhysicalCPUs) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "\tj9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_PHYSICAL) returned invalid value 0.\n");
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Validates that number of bound CPUs returned is the same as current affinity.
 *
 * @param[in] portLibrary The port library under test
 * @param[in] boundTest Test argument - equal to current affinity
 *
 * @return TEST_PASSED on success, TEST_FAILED on failure
 */
int j9sysinfo_numcpus_test2 (J9PortLibrary* portLibrary, UDATA boundTest)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_numcpus_test2";
	UDATA numberPhysicalCPUs = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE);
	UDATA numberBoundCPUs = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_BOUND);

	outputComment(PORTLIB,"\n");
	outputComment(PORTLIB,"Get number of bound CPUs.\n");
	if (0 == boundTest) {
		outputComment(PORTLIB,"	Expected: %d\n", numberPhysicalCPUs);
		outputComment(PORTLIB,"	Result:   %d\n", numberBoundCPUs);
	} else {
		outputComment(PORTLIB,"	Expected: %d\n", boundTest);
		outputComment(PORTLIB,"	Result:   %d\n", numberBoundCPUs);
	}

	if ((0 == boundTest) && (numberPhysicalCPUs == numberBoundCPUs)) {
		/* Not bound, bound should be equal to physical */
	} else if ((0 != boundTest) && (numberBoundCPUs == boundTest)) {
		/* Bound. Confirm that binding is correct */
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "\nj9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_BOUND) returned unexpected value.\n");
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Get number of target and bound CPUs. Validate target is bound
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASSED on success, TEST_FAILED on failure
 */
int j9sysinfo_numcpus_test3 (J9PortLibrary* portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_numcpus_test3";

	UDATA numberBoundCPUs = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_BOUND);
	UDATA targetNumberCPUs = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_TARGET);

	outputComment(PORTLIB,"Get target number of CPUs.\n");
	outputComment(PORTLIB,"Get bound number of CPUs.\n");
	outputComment(PORTLIB,"Expected target: %d\n", numberBoundCPUs);
	outputComment(PORTLIB,"Result:   %d\n", targetNumberCPUs);

	if (numberBoundCPUs != targetNumberCPUs) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "\tTarget number of CPUs is not equal to bound number of CPUs.\n");
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Get initial target number of CPUS. Set user-specified to 3, then to 0. Get target number of CPUs. Validate number equals initial value.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASSED on success, TEST_FAILED on failure
 */
int j9sysinfo_numcpus_test4 (J9PortLibrary* portLibrary)
{
#define J9SYSINFO_NUMCPUS_TEST4_SPECIFIED 3
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_numcpus_test4";
	UDATA targetNumberCPUsOriginal = 0;
	UDATA targetNumberCPUs = 0;

	targetNumberCPUsOriginal = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_TARGET);
	j9sysinfo_set_number_user_specified_CPUs(J9SYSINFO_NUMCPUS_TEST4_SPECIFIED);
	j9sysinfo_set_number_user_specified_CPUs(0);
	targetNumberCPUs = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_TARGET);

	outputComment(PORTLIB,"\nSet number of entitled CPUs to %d, then to 0. Get target number of CPUs.\n", J9SYSINFO_NUMCPUS_TEST4_SPECIFIED);
	outputComment(PORTLIB,"	Expected: %d\n", targetNumberCPUsOriginal);
	outputComment(PORTLIB,"	Result:   %d\n", targetNumberCPUs);

	if (targetNumberCPUsOriginal != targetNumberCPUs) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "\tTarget number of CPUs is not equal to the initial value (%d).\n", targetNumberCPUsOriginal);
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Set user-specified to 10. get target number of CPUs. Validate number is 10.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASSED on success, TEST_FAILED on failure
 */
int j9sysinfo_numcpus_test5 (J9PortLibrary* portLibrary)
{
#define J9SYSINFO_NUMCPUS_TEST5_SPECIFIED 10
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_numcpus_test5";
	UDATA targetNumberCPUs = 0;

	j9sysinfo_set_number_user_specified_CPUs(J9SYSINFO_NUMCPUS_TEST5_SPECIFIED);
	targetNumberCPUs = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_TARGET);

	outputComment(PORTLIB,"\nSet number of entitled CPUs to %d. Get target number of CPUs.\n", J9SYSINFO_NUMCPUS_TEST5_SPECIFIED);
	outputComment(PORTLIB,"	Expected: %d\n", J9SYSINFO_NUMCPUS_TEST5_SPECIFIED);
	outputComment(PORTLIB,"	Result:   %d\n", targetNumberCPUs);

	if (J9SYSINFO_NUMCPUS_TEST5_SPECIFIED != targetNumberCPUs) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "\tTarget number of CPUs is not equal to entitled CPUs (%d).\n", J9SYSINFO_NUMCPUS_TEST5_SPECIFIED);
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Get number of online CPUs. Validate number > 0.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASSED on success, TEST_FAILED on failure
 */
int j9sysinfo_numcpus_test6 (J9PortLibrary* portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_numcpus_test6";
	UDATA numberOnlineCPUs = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE);

	outputComment(PORTLIB,"\n");
	outputComment(PORTLIB,"Get number of online CPUs.\n");
	outputComment(PORTLIB,"	Expected: >0\n");
	outputComment(PORTLIB,"	Result:   %d\n", numberOnlineCPUs);

	if (0 == numberOnlineCPUs) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "\tj9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE) returned invalid value 0.\n");
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Get number of online and physical CPUs. Validate online <= physical.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASSED on success, TEST_FAILED on failure
 */
int j9sysinfo_numcpus_test7 (J9PortLibrary* portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_numcpus_test7";
	UDATA numberOnlineCPUs = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE);
	UDATA numberPhysicalCPUs = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_PHYSICAL);

	outputComment(PORTLIB,"\nGet number of online and physical CPUs. Validate online <= physical.\n");

	if (numberOnlineCPUs > numberPhysicalCPUs) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "\tj9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE) returned value greater than j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_PHYSICAL).\n");
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Call Port Library function j9sysinfo_get_number_CPUs_by_type with invalid type. Validate that return value is 0.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASSED on success, TEST_FAILED on failure
 */
int j9sysinfo_numcpus_test8 (J9PortLibrary* portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_numcpus_test8";
	UDATA result = j9sysinfo_get_number_CPUs_by_type(500);

	outputComment(PORTLIB,"\n");
	outputComment(PORTLIB,"Call j9sysinfo_get_number_CPUs_by_type with invalid type.\n");
	outputComment(PORTLIB,"	Expected: 0\n");
	outputComment(PORTLIB,"	Result:   %d\n", result);

	if (result > 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "\tj9sysinfo_get_number_CPUs_by_type returned non-zero value when given invalid type.\n");
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Measure run time for getting number of CPUs 10000 times.
 *
 * @param[in] portLibrary The port library under test
 * @param[in] type The type of CPU number to query (physical, online, bound, target).
 *
 * @return TEST_PASSED on success, TEST_FAILED on failure
 */
int j9sysinfo_numcpus_runTime(J9PortLibrary* portLibrary, UDATA type)
{
#define J9SYSINFO_NUMCPUS_RUNTIME_LOOPS 1000
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_numcpus_runTime";
	int i = 0;
    const char* testType = "";
    I_64 difference = 0;
    I_64 after = 0;
    I_64 before = j9time_nano_time();

	for (i = 0; i < J9SYSINFO_NUMCPUS_RUNTIME_LOOPS; i++) {
		j9sysinfo_get_number_CPUs_by_type(type);
	}

	after = j9time_nano_time();
	difference = after - before;

    switch (type) {
    case J9PORT_CPU_PHYSICAL:
    	testType = "physical";
    	break;
    case J9PORT_CPU_ONLINE:
    	testType = "online";
    	break;
    case J9PORT_CPU_BOUND:
    	testType = "bound";
    	break;
    case J9PORT_CPU_TARGET:
    	testType = "target";
    	break;
    default:
    	testType = "";
    	break;
    }
    outputComment(PORTLIB,"\n");
    outputComment(PORTLIB,"Get number of %s CPUs %d times: %d ns\n",
    		testType,
    		J9SYSINFO_NUMCPUS_RUNTIME_LOOPS,
    		difference);

    return reportTestExit(portLibrary, testName);
}

#if !defined(J9ZOS390)
/**
 * @internal
 * Internal function: Counts up the number of processors that are online as per the
 * records delivered by the port library routine j9sysinfo_get_processor_info().
 *
 * @param[in] procInfo Pointer to J9ProcessorInfos filled in with processor info records.
 *
 * @return Number (count) of online processors.
 */
static I_32
onlineProcessorCount(const struct J9ProcessorInfos *procInfo)
{
	register I_32 cntr = 0;
	register I_32 n_onln = 0;

	for (cntr = 1; cntr < procInfo->totalProcessorCount + 1; cntr++) {
		if (J9PORT_PROCINFO_PROC_ONLINE == procInfo->procInfoArray[cntr].online) {
			n_onln++;
		}
	}
	return n_onln;
}

/**
 * Test for j9sysinfo_get_number_online_CPUs() port library API. We obtain the online
 * processor count using other (indirect) method - calling the other port library API
 * j9sysinfo_get_processor_info() and cross-check against this.
 *
 * @param[in] portLibrary The port library under test.
 *
 * @return TEST_PASS on success, TEST_FAIL on failure.
 */
int
j9sysinfo_numcpus_testOnlineProcessorCount(J9PortLibrary* portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_testOnlineProcessorCount";
	IDATA rc = 0;
	IDATA cntr = 0;
	J9ProcessorInfos procInfo = {0};

	outputComment(PORTLIB,"\n");
	reportTestEntry(portLibrary, testName);

	/* Call j9sysinfo_get_processor_info() to retrieve a set of processor records from
	 * which we may then ascertain the number of processors online. This will help us
	 * cross-check against the API currently under test.
	 */
	rc = j9sysinfo_get_processor_info(&procInfo);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_processor_info() failed.\n");

		/* Should not try freeing memory unless it was actually allocated! */
		if (J9PORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED != rc) {
			j9sysinfo_destroy_processor_info(&procInfo);
		}
		return reportTestExit(portLibrary, testName);

	} else {
		/* Call the port library API j9sysinfo_get_number_online_CPUs() to check that the online
		 * processor count received is valid (that is, it does not fail) and that this indeed
		 * matches the online processor count as per the processor usage retrieval API.
		 */
		UDATA n_cpus_online = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE);
		if (0 == n_cpus_online) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE) failed.\n");
			goto _cleanup;
		}

		if ((n_cpus_online > 0) &&
			(onlineProcessorCount(&procInfo) == n_cpus_online)) {
			outputComment(PORTLIB, "Number of online processors: %d\n",  n_cpus_online);
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Invalid online processor count found.\n");
		}
	}

_cleanup:
	j9sysinfo_destroy_processor_info(&procInfo);
	return reportTestExit(portLibrary, testName);
}

/**
 * Test for j9sysinfo_get_number_total_CPUs() port library API. Validate the number of
 * available (configured) logical CPUs by cross-checking with what is obtained from
 * invoking the other port library API j9sysinfo_get_processor_info().
 *
 * @param[in] portLibrary The port library under test.
 *
 * @return TEST_PASS on success, TEST_FAIL on failure.
 */
int
j9sysinfo_numcpus_testTotalProcessorCount(J9PortLibrary* portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_testTotalProcessorCount";
	IDATA rc = 0;
	IDATA cntr = 0;
	J9ProcessorInfos procInfo = {0};

	outputComment(PORTLIB,"\n");
	reportTestEntry(portLibrary, testName);

	/* Call j9sysinfo_get_processor_info() to retrieve a set of processor records from
	 * which we may then ascertain the total number of processors configured. We then
	 * cross-check this against what the API currently under test returns.
	 */
	rc = j9sysinfo_get_processor_info(&procInfo);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_processor_info() failed.\n");

		/* Should not try freeing memory unless it was actually allocated! */
		if (J9PORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED != rc) {
			j9sysinfo_destroy_processor_info(&procInfo);
		}
		return reportTestExit(portLibrary, testName);
	} else {
		/* Ensure first that the API doesn't fail. If not, check that we obtained the correct total
		 * processor count by checking against what j9sysinfo_get_processor_info() returned.
		 */
		UDATA n_cpus_physical = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_PHYSICAL);
		if (0 == n_cpus_physical) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_PHYSICAL) failed.\n");
			goto _cleanup;
		}

		if ((procInfo.totalProcessorCount > 0) &&
			(procInfo.totalProcessorCount == n_cpus_physical)) {
			outputComment(PORTLIB, "Total number of processors: %d\n",  n_cpus_physical);
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Invalid processor count retrieved.\n");
		}
	}

_cleanup:
	j9sysinfo_destroy_processor_info(&procInfo);
	return reportTestExit(portLibrary, testName);
}
#endif /* !defined(J9ZOS390) */

/*
 * Pass in the port library to do sysinfo tests
 *
 * @param[in] portLibrary The port library under test
 * @param[in] userSpecificedBoundCPUs Test argument - indicates whether the process is expected to have CPU restrictions.
 *
 * @return TEST_PASSED on success, TEST_FAILED on failure
 */
int 
j9sysinfo_numcpus_runTests(struct J9PortLibrary *portLibrary, UDATA userSpecificedBoundCPUs)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc=TEST_PASS;
	UDATA boundCPUs = userSpecificedBoundCPUs;
	
	HEADING(portLibrary, "Number of CPUs Test");
	
	if (0 == boundCPUs) {
		UDATA numberPhysicalCPUs = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE);
		UDATA numberBoundCPUs = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_BOUND);
		if (numberBoundCPUs < numberPhysicalCPUs) {
			outputComment(PORTLIB, "The test was configured to run without CPU restrictions (e.g. taskset), but CPU restrictions were detected.\n");
			outputComment(PORTLIB, "The test will continue with CPU restrictions configuration.\n\n");
			boundCPUs = numberBoundCPUs;
		}
	}

	rc |= j9sysinfo_numcpus_test1(portLibrary);
	rc |= j9sysinfo_numcpus_test2(portLibrary, boundCPUs);
	rc |= j9sysinfo_numcpus_test3(portLibrary);
	rc |= j9sysinfo_numcpus_test4(portLibrary);
	rc |= j9sysinfo_numcpus_test5(portLibrary);
	rc |= j9sysinfo_numcpus_test6(portLibrary);
	rc |= j9sysinfo_numcpus_test7(portLibrary);
	rc |= j9sysinfo_numcpus_test8(portLibrary);
	rc |= j9sysinfo_numcpus_runTime(portLibrary, J9PORT_CPU_PHYSICAL);
	rc |= j9sysinfo_numcpus_runTime(portLibrary, J9PORT_CPU_ONLINE);
	rc |= j9sysinfo_numcpus_runTime(portLibrary, J9PORT_CPU_BOUND);
#if !defined(J9ZOS390)
	rc |= j9sysinfo_numcpus_testOnlineProcessorCount(portLibrary);
	rc |= j9sysinfo_numcpus_testTotalProcessorCount(portLibrary);
#endif /* !defined(J9ZOS390) */
	
	/* Output results */
	outputComment(PORTLIB, "\nSysinfo Num CPUs test done%s\n", rc == TEST_PASS ? "." : ", failures detected.");
	return TEST_PASS == rc ? 0 : -1;
}
