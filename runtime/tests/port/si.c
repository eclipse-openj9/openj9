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
 * $RCSfile: si.c,v $
 * $Revision: 1.64 $
 * $Date: 2012-12-05 05:27:54 $
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#if defined(WIN32)
#include <direct.h>
#endif
#if !(defined(WIN32) || defined(WIN64))
#include <grp.h>
#include <errno.h>
#include <limits.h> /* For LLONG_MAX */
#include <sys/resource.h> /* For RLIM_INFINITY */
#endif /* !(defined(WIN32) || defined(WIN64)) */

#if defined(J9ZOS390)
#include "atoe.h"
#endif

#include "testHelpers.h"

#if defined(WIN32)
#define J9DIRECTORY_SEPARATOR_CHARACTER '\\'
#define J9FILE_EXTENSION ".exe"
#define J9FILE_EXTENSION_LENGTH (sizeof(J9FILE_EXTENSION) - 1)
#else
#define J9DIRECTORY_SEPARATOR_CHARACTER '/'
#endif

/**
 * @brief Function takes an expected name for an executable and also the name that was actually
 * found using the Port API.  It then tries to match these, in a OS specific manner.  For example,
 * on Unices (and z/OS), executables names could be ./exec or full-path to the executable; on
 * windows we do not require ./ as long as the executable is in the current directory, and so on.
 *
 * @param expected The expected executable name (typically, argv[0]).
 * @param found The executable name as found by the Port API.
 *
 * @return Boolean true, if the names match; false, otherwise.
 */
BOOLEAN validate_executable_name(const char* expected, const char* found)
{
	const char* expected_base_path = NULL;
	UDATA expected_length = 0;
	const char* found_base_path = NULL;
	UDATA found_length = 0;
	UDATA length = 0;

	if ((NULL == expected) || (NULL == found)) {
		return FALSE;
	}

	/* Extract the executable name from the expected path and path found.  Compare these. */
	expected_base_path = strrchr(expected, J9DIRECTORY_SEPARATOR_CHARACTER);
	/* Move past the directory separator, if found. */
	expected_base_path = (NULL != expected_base_path) ? (expected_base_path + 1) : expected;
	expected_length = strlen(expected_base_path);

	found_base_path = strrchr(found, J9DIRECTORY_SEPARATOR_CHARACTER);
	/* Move past the directory separator, if found. */
	found_base_path = (NULL != found_base_path) ? (found_base_path + 1) : found;
	found_length = strlen(found_base_path);

	length = found_length;

	/* On Windows, disregard comparing the extension, should this be dropped on the command
	 * line (API always returns executable name including the extension (.exe).
	 */
#if defined(WIN32)
	/* Check whether argv0 ends with the extension ".exe".  If not, we need to reduce
	 * the number of characters to compare (against the executable name found by API).
	 */
	if (strncmp(expected_base_path + (expected_length - J9FILE_EXTENSION_LENGTH),
				J9FILE_EXTENSION,
				J9FILE_EXTENSION_LENGTH) != 0) {
		length -= J9FILE_EXTENSION_LENGTH;
	}
#endif
	if (length == expected_length) {
		if (strncmp(found_base_path, expected_base_path, length) == 0) {
			return TRUE;
		}
	}
	return FALSE;
}

int j9sysinfo_test0 (J9PortLibrary* portLibrary, char* argv0) 
{
	IDATA rc = 0;
	char* executable_name = NULL;
	const char* testName = "j9sysinfo_test0";
	PORT_ACCESS_FROM_PORT(portLibrary);

	reportTestEntry(portLibrary, testName);

	rc = j9sysinfo_get_executable_name(NULL, &executable_name);
	if (-1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "  Executable name test failed: (rc = %d).\n", rc);
		goto done;
	}

	if (validate_executable_name(argv0, 		 /* expected */
								 executable_name /* found through API */
								 )) {
		outputComment(PORTLIB, "Executable name test passed.\n"
				      "  Expected (argv0=%s).\n  Found=%s.\n",
				 	  argv0,
				 	  executable_name);
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Executable name test failed.\n"
						   "  Expected (argv0=%s).\n  Found=%s.\n",
				 		   argv0,
				 		   executable_name);
		goto done;
	}

#if 0
	/* TODO: Old tests that has yet to convert to the new format.
	 * Convert these tests to newer form, on an "as need" basis. 
	 */
	PORT_ACCESS_FROM_PORT(portLibrary);
	U_32 pid;
	I_32 bufSize=575;		/*should sizeof(infoString), */
	U_16 classpathSeparator;
	UDATA number_CPUs;
	char *result;
	const char *osName, *osType, *osVersion, 
	char infoString[1000],envVar[100];

	HEADING(PORTLIB,"SysInfo (si) test");


	pid=j9sysinfo_get_pid();
	j9tty_printf(PORTLIB,"PLT pid=%d\n",pid);

	osVersion=j9sysinfo_get_OS_version();
	j9tty_printf(PORTLIB,"OSversion=%s\n" osVersion ? osVersion : "NULL");

	strcpy(envVar,"PATH");		/*case matters?*/
	/*check rc before use: -1=BOGUS  0=good else nowrite, rc=varLength*/
	rc=j9sysinfo_get_env(envVar,infoString,bufSize);
	j9tty_printf(PORTLIB,"%s=%s\nbufSize=%d rc=%d\n\n",envVar,infoString,bufSize,rc);
	/*
	 * note:  tty_printf has a implementation-dependent limit on out chars
	 * printf("\n\n%s=%s bufSize=%d rc=%d\n\n",envVar,infoString,bufSize,rc);
	 */

	osName=j9sysinfo_get_CPU_architecture();
	j9tty_printf(PORTLIB,"CPU architecture=%s\n",osName ? osName : "NULL");

	osType=j9sysinfo_get_OS_type();
	j9tty_printf(PORTLIB,"OS type=%s\n",osType ? osType : "NULL");

	classpathSeparator=j9sysinfo_get_classpathSeparator();
	j9tty_printf(PORTLIB,"cp separator=%c\n",classpathSeparator);

	/*SHOULD j9mem_free_memory() result, eh!!!!!! argv[0] not really used*/
	/*check rc before use: 0=good -1=error ???*/

	number_CPUs=j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE);
	j9tty_printf(PORTLIB,"number of CPU(s)=%d\n",number_CPUs);


	j9tty_printf(PORTLIB,"SI test done!\n\n");

#endif	
done:
	return reportTestExit(portLibrary, testName);
}

/* 
 * Test various aspect of j9sysinfo_get_username 
 */
int j9sysinfo_test1 (J9PortLibrary* portLibrary) {
#define J9SYSINFO_TEST1_USERNAME_LENGTH 1024
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_test1";
	char username[J9SYSINFO_TEST1_USERNAME_LENGTH];
	IDATA length, rc;
	
	reportTestEntry(portLibrary, testName);
	
	rc = j9sysinfo_get_username(username, J9SYSINFO_TEST1_USERNAME_LENGTH);
	if(rc == -1) {
		outputComment(PORTLIB, "j9sysinfo_get_username returns -1.\n");
		outputComment(PORTLIB, "If this is a supported platform, consider this as a failure\n");
		return reportTestExit(portLibrary, testName);
	} else {
		char msg[256]= "";
		j9str_printf(PORTLIB, msg, sizeof(msg), "User name returned = \"%s\"\n", username);
		outputComment(PORTLIB, msg);
	}
	
	length = strlen(username);
	rc = j9sysinfo_get_username(username, length-1);
	
	if(length > rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Error if username buffer is too short: rc= %d, Expecting %d\n", rc, 1);
	}
	
	return reportTestExit(portLibrary, testName);
	
}

int j9sysinfo_test2 (J9PortLibrary* portLibrary) {
#define J9SYSINFO_TEST2_GROUPNAME_LENGTH 1024
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_test2";
	char groupname[J9SYSINFO_TEST2_GROUPNAME_LENGTH];
	IDATA length, rc;
	
	reportTestEntry(portLibrary, testName);
	
	rc = j9sysinfo_get_groupname(groupname, J9SYSINFO_TEST2_GROUPNAME_LENGTH);
	if(rc == -1) {
		outputComment(PORTLIB, "j9sysinfo_get_groupname returns -1.\n");
		outputComment(PORTLIB, "If this is a supported platform, consider this as a failure\n");
		return reportTestExit(portLibrary, testName);
	} else {
		char msg[256]= "";
		j9str_printf(PORTLIB, msg, sizeof(msg), "Group name returned = \"%s\"\n", groupname);
		outputComment(PORTLIB, msg);
	}
	
	length = strlen(groupname);
	rc = j9sysinfo_get_groupname(groupname, length-1);
	
	if(length > rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Error if groupname buffer is too short: rc= %d, Expecting %d\n", rc, 1);
	}
	
	return reportTestExit(portLibrary, testName);
	
}

int j9sysinfo_get_OS_type_test (J9PortLibrary* portLibrary) {
#define J9SYSINFO_TEST3_OSNAME_LENGTH 1024
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_get_OS_type_test";
	const char *osName = NULL;
	const char *osVersion = NULL;
		
	reportTestEntry(portLibrary, testName);
	
	osName = j9sysinfo_get_OS_type();
	if (osName == NULL) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_OS_type returned NULL - expected OS name.\n", 0, 1);
		return reportTestExit(portLibrary, testName);
	} else {
		char msg[256];
		j9str_printf(PORTLIB, msg, sizeof(msg), "j9sysinfo_get_OS_type returned : \"%s\"\n", osName);
		outputComment(PORTLIB, msg);
	}
	
	osVersion = j9sysinfo_get_OS_version();
	if (osVersion == NULL) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_OS_version returned NULL - expected OS name.\n", 0, 1);
		return reportTestExit(portLibrary, testName);
	} else {
		char msg[256];
		j9str_printf(PORTLIB, msg, sizeof(msg), "j9sysinfo_get_OS_version returned : \"%s\"\n", osVersion);
		outputComment(PORTLIB, msg);
	}

#if defined(WIN32) || defined(WIN64)
	if (NULL == strstr(osName, "Windows")) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_OS_version does not contain \"Windows\".\n", 0, 1);
		return reportTestExit(portLibrary, testName);
	} else if (0 == strcmp(osName, "Windows")) {
		/*
		 * This means we are running a new, unrecognized version of Windows.  We need to update  j9sysinfo_get_OS_type
		 * to recognize the specific version.
		 */
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_OS_version returned the default Windows version.\n", 0, 1);
		return reportTestExit(portLibrary, testName);
	}
#endif
	
	return reportTestExit(portLibrary, testName);
}

int j9sysinfo_test3 (J9PortLibrary* portLibrary) {
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_test3";
	J9PortSysInfoLoadData loadData;
	IDATA rc;
	
	reportTestEntry(portLibrary, testName);
	
	rc = j9sysinfo_get_load_average(&loadData);
	if(rc == -1) {
		outputComment(PORTLIB, "j9sysinfo_get_load_average returns -1.\n");
		outputComment(PORTLIB, "If this is a supported platform, consider this as a failure\n");
		return reportTestExit(portLibrary, testName);
	} else {
		outputComment(PORTLIB, "Returned data\n");
		outputComment(PORTLIB, "One Minute Average: %lf\n", loadData.oneMinuteAverage);
		outputComment(PORTLIB, "Five Minute Average: %lf\n", loadData.fiveMinuteAverage);
		outputComment(PORTLIB, "Fifteen Minute Average: %lf\n", loadData.fifteenMinuteAverage);
	}
	
	return reportTestExit(portLibrary, testName);
}


int j9sysinfo_test_sysinfo_ulimit_iterator (J9PortLibrary* portLibrary) {
	
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_test_sysinfo_ulimit_iterator";
	I_32 rc = 0;
	
	J9SysinfoLimitIteratorState state;
	J9SysinfoUserLimitElement element;
	
	reportTestEntry(portLibrary, testName);
	
	rc = j9sysinfo_limit_iterator_init(&state);
	
	if (0 != rc) {

		if (J9PORT_ERROR_NOT_SUPPORTED_ON_THIS_PLATFORM == rc) {
			j9tty_printf(portLibrary, "\tThis platform does not support the limit iterator\n");
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "\tj9sysinfo_limit_iterator_init returned: %i\n", rc);
		}
		goto done;
	}
	
	while (j9sysinfo_limit_iterator_hasNext(&state)) {
	
		rc = j9sysinfo_limit_iterator_next(&state, &element);
		
		if (0 == rc) {
			j9tty_printf(portLibrary, "\t%s:\n", element.name);
			
			if (J9PORT_LIMIT_UNLIMITED == element.softValue) {
				j9tty_printf(portLibrary, "\t soft: unlimited\n");
			} else {
				j9tty_printf(portLibrary, "\t soft: 0x%zX\n", element.softValue);
			}
			
			if (J9PORT_LIMIT_UNLIMITED == element.hardValue) {
				j9tty_printf(portLibrary, "\t hard: unlimited\n");
			} else {
				j9tty_printf(portLibrary, "\t hard: 0x%zX\n", element.hardValue);
			}
					
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "\tj9sysinfo_limit_iterator_next returned: %i when 0 was expected\n", rc);
		}
	}
	
done:

	return reportTestExit(portLibrary, testName);
}


/**
 *
 * Test the iterator by supplying it with a:
 *
 * 1. NULL buffer
 * 2. Buffer that is big enough for the entire environment
 * 3. Buffer that is big enough to contain an env var, but not big enough for the entire environment
 * 4. non-null buffer that is not big enough to contain an env var.
 *
 * In cases but the first (null buffer) attempt to iterate.
 */
int j9sysinfo_test_sysinfo_env_iterator (J9PortLibrary* portLibrary) {
	
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_test_sysinfo_env_iterator";
	I_32 rc = 0;
	IDATA envSize = 0;
	J9SysinfoEnvIteratorState state;
	J9SysinfoEnvElement element;
	void *buffer = NULL;
	U_32 bufferSizeBytes = 0;
	int l =0;
#undef SI_DEBUG

	reportTestEntry(portLibrary, testName);

	/* Test 1: NULL buffer - Pass in NULL for buffer, make sure we get back a positive integer describing the size, or a crash */
	buffer = NULL;
	rc = j9sysinfo_env_iterator_init(&state, buffer, bufferSizeBytes);
	
	if (rc < 0) {
		if (J9PORT_ERROR_NOT_SUPPORTED_ON_THIS_PLATFORM == rc) {
			j9tty_printf(portLibrary, "\tThis platform does not support the env iterator\n");
		} else if  (J9PORT_ERROR_SYSINFO_ENV_INIT_CRASHED_COPYING_BUFFER == rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "\tCrash passing in NULL buffer while running single-threaded. This is a failure because no-one else should have been able to modify it\n");
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "\tj9sysinfo_env_iterator_init returned: %i\n", rc);
		}

		goto done;

	} else {
		envSize = rc;
#if defined(SI_DEBUG)
		j9tty_printf(portLibrary,"Need a buffer of size %x bytes\n", envSize);
#endif
	}
	
	/* Test 2: Buffer that is big enough for the entire environment */
	
	buffer = j9mem_allocate_memory(envSize, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == buffer) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "OutOfMemory allocating buffer for test - where's all the memory?");
		goto done;
	}
	
	bufferSizeBytes = (U_32)envSize;
	rc = j9sysinfo_env_iterator_init(&state, buffer, bufferSizeBytes);
	
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "\tj9sysinfo_env_iterator_init returned: %i\n", rc);
		goto done;
	}

	j9tty_printf(portLibrary, "\tEnvironment:\n\n");

	l = 0;
	while (j9sysinfo_env_iterator_hasNext(&state)) {
		rc = j9sysinfo_env_iterator_next(&state, &element);
		
		if (0 == rc) {
			j9tty_printf(portLibrary, "\t%s\n", element.nameAndValue);
			l++;
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "\tj9sysinfo_env_iterator_next returned: %i when 0 was expected\n", rc);
			goto done;
			break;
		}
	}

	/* Test3: Buffer that is big enough to contain an env var, but not big enough for the entire environment */
	j9mem_free_memory(buffer);

	bufferSizeBytes = (U_32)envSize-100;
	buffer = j9mem_allocate_memory(bufferSizeBytes, OMRMEM_CATEGORY_PORT_LIBRARY);

	if (NULL == buffer) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "OutOfMemory allocating buffer for test - where's all the memory?");
		goto done;
	}

	rc = j9sysinfo_env_iterator_init(&state, buffer, bufferSizeBytes);

	if (rc < 0) {
		if (J9PORT_ERROR_SYSINFO_ENV_INIT_CRASHED_COPYING_BUFFER == rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "\tCrash passing in NULL buffer while running single-threaded. This is a failure because no-one else should have been able to modify it\n");
		}
		outputErrorMessage(PORTTEST_ERROR_ARGS, "\tj9sysinfo_env_iterator_init returned: %i\n", rc);
		goto done;
	} else {
		envSize = rc;
#if defined(SI_DEBUG)
		j9tty_printf(portLibrary,"Should have a buffer of size %x bytes, using one of size %x instead, which will result in a truncated environment\n", envSize, bufferSizeBytes);
#endif
	}

	l = 0;
	while (j9sysinfo_env_iterator_hasNext(&state)) {

		rc = j9sysinfo_env_iterator_next(&state, &element);

#if defined(SI_DEBUG)
		j9tty_printf(portLibrary, "\tsi.c element.nameAndValue @ 0x%p: %s\n", element.nameAndValue, element.nameAndValue);
#endif

		if (0 == rc) {
			l++;
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "\tj9sysinfo_env_iterator_next returned: %i when 0 was expected\n", rc);
			goto done;
			break;
		}
	}

	/* Test 4. non-null buffer that is not big enough to contain an env var. */
	j9mem_free_memory(buffer);

	bufferSizeBytes = 1;
	buffer = j9mem_allocate_memory(bufferSizeBytes, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == buffer) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "OutOfMemory allocating buffer for test - where's all the memory?");
		goto done;
	}

	rc = j9sysinfo_env_iterator_init(&state, buffer, bufferSizeBytes);

	if (rc < 0) {
		if (J9PORT_ERROR_SYSINFO_ENV_INIT_CRASHED_COPYING_BUFFER == rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "\tCrash passing in NULL buffer while running single-threaded. This is a failure because no-one else should have been able to modify it\n");
		}
		outputErrorMessage(PORTTEST_ERROR_ARGS, "\tj9sysinfo_env_iterator_init returned: %i\n", rc);
		goto done;
	} else {
		envSize = rc;
#if defined(SI_DEBUG)
		j9tty_printf(portLibrary,"Should have a buffer of size %x bytes, using one of size %x instead, which will result in a truncated environment\n", envSize, bufferSizeBytes);
#endif
	}

	l = 0;
	while (j9sysinfo_env_iterator_hasNext(&state)) {

		rc = j9sysinfo_env_iterator_next(&state, &element);

#if defined(SI_DEBUG)
		j9tty_printf(portLibrary, "\tsi.c element.nameAndValue @ 0x%p: %s\n", element.nameAndValue, element.nameAndValue);
#endif

		if (0 == rc) {
			l++;
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "\tj9sysinfo_env_iterator_next returned: %i when 0 was expected\n", rc);
			goto done;
			break;
		}
	}

done:

	if (NULL != buffer) {
		j9mem_free_memory(buffer);
	}

	return reportTestExit(portLibrary, testName);
}


/* sysinfo_set_limit and sysinfo_get_limit tests will not work on windows */
#if !(defined(WIN32) || defined(WIN64))

/**
 *
 * Test j9sysinfo_test_sysinfo_set_limit and j9sysinfo_test_sysinfo_get_limit
 * with resourceID J9PORT_RESOURCE_ADDRESS_SPACE
 * 
 */
int j9sysinfo_test_sysinfo_set_limit_ADDRESS_SPACE(J9PortLibrary* portLibrary) {
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_test_sysinfo_set_limit_ADDRESS_SPACE";
	J9PortSysInfoLoadData loadData;
	IDATA rc;
	U_64 limit;
	U_64 originalCurLimit;
	U_64 originalMaxLimit;
	U_64 currentLimit;
	const U_64 as1 = 300000;

	reportTestEntry(portLibrary, testName);
	
	/* save original soft limit */
	rc = j9sysinfo_get_limit(J9PORT_RESOURCE_ADDRESS_SPACE, &originalCurLimit);
	
	rc = j9sysinfo_set_limit(J9PORT_RESOURCE_ADDRESS_SPACE, as1);
	if (rc == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_set_limit returns -1.\n");
		return reportTestExit(portLibrary, testName);
	}

	rc = j9sysinfo_get_limit(J9PORT_RESOURCE_ADDRESS_SPACE, &currentLimit);

	if (as1 == currentLimit) {
		outputComment(PORTLIB, "j9sysinfo_set_limit set ADDRESS_SPACE soft max successful\n");
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_set_limit set ADDRESS_SPACE soft max FAILED\n");
		return reportTestExit(portLibrary, testName);
	}

	/* save original hard limit */
	rc = j9sysinfo_get_limit(J9PORT_RESOURCE_ADDRESS_SPACE | J9PORT_LIMIT_HARD, &originalMaxLimit);
	
	/* lowering the hard limit is irreversible unless privileged */
	if (geteuid()) {
		/* we should be able to set the hard limit to the current value as an unprivileged user
		   When the hard limit is set to unlimited (-1) a regular user can change it to
		   any value, but not back to unlimited. */
		rc = j9sysinfo_set_limit(J9PORT_RESOURCE_ADDRESS_SPACE | J9PORT_LIMIT_HARD, originalMaxLimit);
		if (rc == -1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_set_limit returns -1\n");
		}
		
		rc = j9sysinfo_get_limit(J9PORT_RESOURCE_ADDRESS_SPACE | J9PORT_LIMIT_HARD, &currentLimit);
		if (originalMaxLimit == currentLimit) {
			outputComment(PORTLIB, "j9sysinfo_set_limit set ADDRESS_SPACE hard max successful\n");
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_set_limit set ADDRESS_SPACE hard max FAILED\n");
			return reportTestExit(portLibrary, testName);
		}
		
	} else {
		/* now try setting the hard limit */
		rc = j9sysinfo_set_limit(J9PORT_RESOURCE_ADDRESS_SPACE | J9PORT_LIMIT_HARD, as1 + 1);
		if (rc == -1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_set_limit returns -1.\n");
			return reportTestExit(portLibrary, testName);
		}

		rc = j9sysinfo_get_limit(J9PORT_RESOURCE_ADDRESS_SPACE | J9PORT_LIMIT_HARD, &currentLimit);
	
		if (as1+1 == currentLimit) {
			outputComment(PORTLIB, "j9sysinfo_set_limit set ADDRESS_SPACE hard max successful\n");
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_set_limit set ADDRESS_SPACE hard max FAILED\n");
			return reportTestExit(portLibrary, testName);
		}
	

		/* restore original hard limit */
		rc = j9sysinfo_set_limit(J9PORT_RESOURCE_ADDRESS_SPACE | J9PORT_LIMIT_HARD, originalMaxLimit);
		if (rc == -1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "restoring the original hard limit failed j9sysinfo_set_limit returns -1.\n");
			return reportTestExit(portLibrary, testName);
		}
	}
	
	/* restore original soft limit
	   The soft limit is always <= the hard limit. If the hard limit is lowered to below the soft limit and
	   then raised again the soft limit isn't automatically raised. */
	rc = j9sysinfo_set_limit(J9PORT_RESOURCE_ADDRESS_SPACE, originalCurLimit);
	if (rc == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "restoring the original soft limit failed j9sysinfo_set_limit returns -1.\n");
		return reportTestExit(portLibrary, testName);
	}

	return reportTestExit(portLibrary, testName);
}

/**
 *
 * Test j9sysinfo_test_sysinfo_set_limit and j9sysinfo_test_sysinfo_get_limit
 * with resourceID J9PORT_RESOURCE_CORE_FILE
 * 
 */
int j9sysinfo_test_sysinfo_set_limit_CORE_FILE(J9PortLibrary* portLibrary) {
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_test_sysinfo_set_limit_CORE_FILE";
	J9PortSysInfoLoadData loadData;
	IDATA rc;
	U_64 limit;
	U_64 originalCurLimit;
	U_64 originalMaxLimit;
	U_64 currentLimit;
	const U_64 coreFileSize = 42;

	reportTestEntry(portLibrary, testName);
	
	/* save original soft limit */
	rc = j9sysinfo_get_limit(J9PORT_RESOURCE_CORE_FILE, &originalCurLimit);

	rc = j9sysinfo_set_limit(J9PORT_RESOURCE_CORE_FILE, coreFileSize);
	if (rc == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_set_limit returns -1.\n");
		return reportTestExit(portLibrary, testName);
	}

	rc = j9sysinfo_get_limit(J9PORT_RESOURCE_CORE_FILE, &currentLimit);

	if (coreFileSize == currentLimit) {
		outputComment(PORTLIB, "j9sysinfo_set_limit set CORE_FILE soft max successful\n");
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_set_limit set CORE_FILE soft max FAILED\n");
		return reportTestExit(portLibrary, testName);
	}
		

	/* save original hard limit */
	rc = j9sysinfo_get_limit(J9PORT_RESOURCE_CORE_FILE | J9PORT_LIMIT_HARD, &originalMaxLimit);

	/* lowering the hard limit is irreversible unless privileged */
	if (geteuid()) {
		/* we should be able to set the hard limit to the current value as an unprivileged user
		   When the hard limit is set to unlimited (-1) a regular user can change it to
		   any value, but not back to unlimited. */
		rc = j9sysinfo_set_limit(J9PORT_RESOURCE_CORE_FILE | J9PORT_LIMIT_HARD, originalMaxLimit);
		if (rc == -1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_set_limit returns -1\n");
		}
		
		rc = j9sysinfo_get_limit(J9PORT_RESOURCE_CORE_FILE | J9PORT_LIMIT_HARD, &currentLimit);
		if (originalMaxLimit == currentLimit) {
			outputComment(PORTLIB, "j9sysinfo_set_limit set CORE_FILE hard max successful\n");
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_set_limit set CORE_FILE hard max FAILED\n");
			return reportTestExit(portLibrary, testName);
		}
		
	} else {

		/* now try setting the hard limit */
		rc = j9sysinfo_set_limit(J9PORT_RESOURCE_CORE_FILE | J9PORT_LIMIT_HARD, coreFileSize + 1);
		if (rc == -1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_set_limit returns -1.\n");
			return reportTestExit(portLibrary, testName);
		}

		rc = j9sysinfo_get_limit(J9PORT_RESOURCE_CORE_FILE | J9PORT_LIMIT_HARD, &currentLimit);
		if (coreFileSize + 1 == currentLimit) {
			outputComment(PORTLIB, "j9sysinfo_set_limit set CORE_FILE hard max successful\n");
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_set_limit set CORE_FILE hard max FAILED\n");
			return reportTestExit(portLibrary, testName);
		}
	
		/* restore original hard limit */
		rc = j9sysinfo_set_limit(J9PORT_RESOURCE_CORE_FILE | J9PORT_LIMIT_HARD, originalMaxLimit);
		if (rc == -1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "restoring the original hard limit failed j9sysinfo_set_limit returns -1.\n");
			return reportTestExit(portLibrary, testName);
		}
	}
	
	/* restore original soft limit
	   The soft limit is always <= the hard limit. If the hard limit is lowered to below the soft limit and
	   then raised again the soft limit isn't automatically raised. */
	rc = j9sysinfo_set_limit(J9PORT_RESOURCE_CORE_FILE, originalCurLimit);
	if (rc == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "restoring the original soft limit failed j9sysinfo_set_limit returns -1.\n");
		return reportTestExit(portLibrary, testName);
	}

	return reportTestExit(portLibrary, testName);
}

/**
 *
 * Test j9sysinfo_test_sysinfo_set_limit and j9sysinfo_test_sysinfo_get_limit
 * with resourceID J9PORT_RESOURCE_CORE_FLAGS
 * 
 */
int j9sysinfo_test_sysinfo_set_limit_CORE_FLAGS(J9PortLibrary* portLibrary) {
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_test_sysinfo_set_limit_CORE_FLAGS";
	J9PortSysInfoLoadData loadData;
	IDATA rc;
	U_64 currentLimit;
	U_64 originalLimit;
	I_32 lastErrorNumber = 0;

	reportTestEntry(portLibrary, testName);

	if (geteuid()) {
		outputComment(PORTLIB, "You must be root to set core flags\n");

		rc = j9sysinfo_get_limit(J9PORT_RESOURCE_CORE_FLAGS, &currentLimit);
		if (-1 == rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_set_limit get AIX full core value failed\n");
			return reportTestExit(portLibrary, testName);
		}

		rc = j9sysinfo_set_limit(J9PORT_RESOURCE_CORE_FLAGS, 1);
		lastErrorNumber = j9error_last_error_number();
		
		if (J9PORT_ERROR_FILE_NOPERMISSION == lastErrorNumber) {
			outputComment(PORTLIB, "j9sysinfo_set_limit CORE_FLAGS failed as expected because we aren't root.\n");
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_set_limit CORE_FLAGS did not fail with the proper error.\n");
			return reportTestExit(portLibrary, testName);
		}

	} else {

		/* save original soft limit */
		rc = j9sysinfo_get_limit(J9PORT_RESOURCE_CORE_FLAGS, &originalLimit);

		/* try setting core flags to 0 */
		rc = j9sysinfo_set_limit(J9PORT_RESOURCE_CORE_FLAGS, 0);
		if (rc == -1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_set_limit returns -1.\n");
			return reportTestExit(portLibrary, testName);
		}

		rc = j9sysinfo_get_limit(J9PORT_RESOURCE_CORE_FLAGS, &currentLimit);
		if ((rc == J9PORT_LIMIT_LIMITED) && 
			(0 == currentLimit)) {
			outputComment(PORTLIB, "j9sysinfo_set_limit set AIX full core value to 0 successful\n");
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_set_limit set CORE_FLAGS FAILED\n");
			return reportTestExit(portLibrary, testName);
		}
		
		/* try setting core flags to 1 (unlimited) */
		rc = j9sysinfo_set_limit(J9PORT_RESOURCE_CORE_FLAGS, 1);
		if (rc == -1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_set_limit returns -1.\n");
			return reportTestExit(portLibrary, testName);
		}

		rc = j9sysinfo_get_limit(J9PORT_RESOURCE_CORE_FLAGS, &currentLimit);
		if ((J9PORT_LIMIT_UNLIMITED == rc) && 
			(U_64_MAX == currentLimit)) {
			outputComment(PORTLIB, "j9sysinfo_set_limit set AIX full core value to 1 successful\n");
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_set_limit set CORE_FLAGS FAILED\n");
			return reportTestExit(portLibrary, testName);
		}
		
		
		/* restore original limit */
		rc = j9sysinfo_set_limit(J9PORT_RESOURCE_CORE_FILE, originalLimit);
		if (rc == -1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "restoring the original AIX full core value failed j9sysinfo_set_limit returns -1.\n");
			return reportTestExit(portLibrary, testName);
		}	
	}
	
	return reportTestExit(portLibrary, testName);
}

/**
 *
 * Test j9sysinfo_test_sysinfo_get_limit for resource J9PORT_RESOURCE_FILE_DESCRIPTORS.
 * API available on all Unix platforms.
 */
int j9sysinfo_test_sysinfo_get_limit_FILE_DESCRIPTORS(J9PortLibrary* portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_test_sysinfo_get_limit_FILE_DESCRIPTORS";
	U_32 rc = 0;
	U_64 curLimit = 0;
	U_64 maxLimit = 0;

	reportTestEntry(portLibrary, testName);
	/* First, get the current (soft) limit on the resource nofiles. */
	rc = j9sysinfo_get_limit(J9PORT_RESOURCE_FILE_DESCRIPTORS, &curLimit);
	if (J9PORT_LIMIT_UNLIMITED == rc) { /* Not an error, just that it is not configured. */
		/* If the API reported this limit as set to "unlimited", the resource limit must be
		 * set to implementation-defined limit, RLIM_INFINITY.
		 */
		if (RLIM_INFINITY == curLimit) {
			outputComment(PORTLIB,
				"j9sysinfo_get_limit(nofiles) soft limit (unlimited), RLIM_INFINITY.\n");
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS,
				"j9sysinfo_get_limit(nofiles): soft limit (unlimited), bad maximum reported %lld.\n",
				((I_64) curLimit));
			return reportTestExit(portLibrary, testName);
		}
	} else if (J9PORT_LIMIT_LIMITED == rc) {
		if ((((I_64) curLimit) > 0) && (((I_64) curLimit) <= LLONG_MAX)) {
			outputComment(PORTLIB, "j9sysinfo_get_limit(nofiles) soft limit: %lld.\n",
				((I_64) curLimit));
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS,
				"j9sysinfo_get_limit(nofiles) failed: bad limit received!\n");
			return reportTestExit(portLibrary, testName);
		}
	} else { /* The port library failed! */
		outputErrorMessage(PORTTEST_ERROR_ARGS, 
			"j9sysinfo_get_limit(nofiles): failed with error code=%d.\n",
			j9error_last_error_number());
		return reportTestExit(portLibrary, testName);
	}

	/* Now, for the hard limit. */
	rc = j9sysinfo_get_limit(J9PORT_RESOURCE_FILE_DESCRIPTORS | J9PORT_LIMIT_HARD, &maxLimit);
	if (J9PORT_LIMIT_UNLIMITED == rc) {
		/* Not an error, just that it is not configured.  Ok to compare!. */
		if (RLIM_INFINITY == maxLimit) {
			outputComment(PORTLIB,
				"j9sysinfo_get_limit(nofiles) hard limit (unlimited), RLIM_INFINITY.\n");
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS,
				"j9sysinfo_get_limit(nofiles): hard limit (unlimited), bad maximum reported %lld.\n",
				((I_64) curLimit));
			return reportTestExit(portLibrary, testName);
		}
	} else if (J9PORT_LIMIT_LIMITED == rc) {
		if ((((I_64) maxLimit) > 0) && (((I_64) maxLimit) <= LLONG_MAX)) {
			outputComment(PORTLIB, "j9sysinfo_get_limit(nofiles) hard limit: %lld.\n",
				(I_64) maxLimit);
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS,
				"j9sysinfo_get_limit(nofiles) failed: bad limit received!\n");
			return reportTestExit(portLibrary, testName);
		}
	} else { /* The port library failed! */
		outputErrorMessage(PORTTEST_ERROR_ARGS, 
			"j9sysinfo_get_limit(nofiles): failed with error code=%d.\n",
			j9error_last_error_number());
		return reportTestExit(portLibrary, testName);
	}

	/* Ensure that the resource's current (soft) limit does not exceed the hard limit. */
	if (curLimit > maxLimit) {
		outputErrorMessage(PORTTEST_ERROR_ARGS,
				"j9sysinfo_get_limit(nofiles): current limit exceeds the hard limit.\n");
		return reportTestExit(portLibrary, testName);
	}
	return reportTestExit(portLibrary, testName);
}
#endif /* !(defined(WIN32) || defined(WIN64)) */

/**
 *
 * Test j9sysinfo_test_sysinfo_get_processor_description
 *
 */
int j9sysinfo_test_sysinfo_get_processor_description(J9PortLibrary* portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_test_sysinfo_get_processor_description";
	IDATA rc = 0;
	int i = 0;

	J9ProcessorDesc desc;

	rc = j9sysinfo_get_processor_description(&desc);

#if (defined(J9X86) || defined(J9HAMMER))
	if (desc.processor < PROCESSOR_X86_UNKNOWN) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_test_sysinfo_get_processor_description() processor detection failed.\n");
	}
	if (desc.physicalProcessor < PROCESSOR_X86_UNKNOWN) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_test_sysinfo_get_processor_description() physical processor detection failed.\n");
	}
#elif (defined(AIXPPC) || defined(LINUXPPC))
	if ((desc.processor < PROCESSOR_PPC_UNKNOWN) || (desc.processor >= PROCESSOR_X86_UNKNOWN)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_test_sysinfo_get_processor_description() processor detection failed.\n");
	}
	if ((desc.physicalProcessor < PROCESSOR_PPC_UNKNOWN) || (desc.physicalProcessor >= PROCESSOR_X86_UNKNOWN)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_test_sysinfo_get_processor_description() physical processor detection failed.\n");
	}
#elif (defined(S390) || defined(J9ZOS390))
	if (desc.processor >= (PROCESSOR_PPC_UNKNOWN)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_test_sysinfo_get_processor_description() processor detection failed.\n");
	}
	if (desc.physicalProcessor >= (PROCESSOR_PPC_UNKNOWN)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_test_sysinfo_get_processor_description() physical processor detection failed.\n");
	}
#endif
	
	for (i = 0; i < J9PORT_SYSINFO_FEATURES_SIZE * 32; i++) {
		BOOLEAN feature = j9sysinfo_processor_has_feature(&desc, i);
		if ((TRUE != feature)
		&& (FALSE != feature)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_test_sysinfo_get_processor_description() feature %d detection failed: %d\n", i, feature);
		}
	}

	return reportTestExit(portLibrary, testName);
}



/* Since the processor and memory usage port library APIs are not available on zOS (neither
 * 31-bit not 64-bit) yet, so we exclude these tests from running on zOS. When the zOS
 * implementation is available, we must remove these guards so that they are built and
 * executed on the z as well. Remove this comment too.
 */
#if !defined(J9ZOS390)
/**
 * Test for j9sysinfo_get_memory_info() port library API. Check that we are indeed
 * able to retrieve memory statistics on all supported platforms and also, perform
 * some minimum sanity checks.
 *
 * @param[in] portLibrary The port library under test.
 *
 * @return TEST_PASS on success, TEST_FAIL on failure.
 */
int
j9sysinfo_testMemoryInfo(J9PortLibrary* portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_testMemoryInfo";
	IDATA rc = 0;
	J9MemoryInfo memInfo = {0};

	reportTestEntry(portLibrary, testName);

	rc = j9sysinfo_get_memory_info(&memInfo);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_memory_info() failed.\n");
	} else {
		/* If any of these parameters are set to J9PORT_MEMINFO_NOT_AVAILABLE on platforms
		 * where they are supposed to be defined, flag an error to fail pltest.
		 */
		if ((J9PORT_MEMINFO_NOT_AVAILABLE == memInfo.totalPhysical) ||
			(J9PORT_MEMINFO_NOT_AVAILABLE == memInfo.availPhysical) ||
			(J9PORT_MEMINFO_NOT_AVAILABLE == memInfo.totalSwap) ||
			(J9PORT_MEMINFO_NOT_AVAILABLE == memInfo.availSwap) ||
#if defined(WIN32) || defined(OSX)
			(J9PORT_MEMINFO_NOT_AVAILABLE == memInfo.totalVirtual) ||
			(J9PORT_MEMINFO_NOT_AVAILABLE == memInfo.availVirtual) ||
#else /* defined(WIN32) || defined(OSX) */
			/* We do not check totalVirtual since it may be set to some value or -1, depending
			 * on whether there is a limit set for this or not on the box.
			 */
			(J9PORT_MEMINFO_NOT_AVAILABLE != memInfo.availVirtual) ||
#endif /* defined(WIN32) || defined(OSX) */
#if defined(AIXPPC) || defined(WIN32) || defined(OSX)
			/* Size of the file buffer area is not available on Windows, AIX and OSX.
			 * Therefore, it must be set to J9PORT_MEMINFO_NOT_AVAILABLE.
			 */
			(J9PORT_MEMINFO_NOT_AVAILABLE != memInfo.buffered) ||
#else /* defined(AIXPPC) || defined(WIN32) || defined(OSX) */
			/* On platforms where buffer area is defined, J9PORT_MEMINFO_NOT_AVAILABLE is
			 * surely a failure!
			 */
			(J9PORT_MEMINFO_NOT_AVAILABLE == memInfo.buffered) ||
#endif /* defined(AIXPPC) || defined(WIN32) || defined(OSX) */
#if defined(OSX)
			/* Size of cached area is not available on OSX. So, it is set to
			 * J9PORT_MEMINFO_NOT_AVAILABLE on OSX. */
			(J9PORT_MEMINFO_NOT_AVAILABLE != memInfo.cached))
#else /* defined(OSX) */
			(J9PORT_MEMINFO_NOT_AVAILABLE == memInfo.cached))
#endif /* defined(OSX) */
		{
			/* Fail pltest if one of these memory usage parameters were found inconsistent. */
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Invalid memory usage statistics retrieved.\n");
			return reportTestExit(portLibrary, testName);
		}

		/* Validate the statistics that we obtained. */
		if ((memInfo.totalPhysical > 0) &&
			(memInfo.availPhysical <= memInfo.totalPhysical) &&
#if defined(WIN32) || defined(OSX)
			/* Again, it does not make sense to do checks and comparisons on Virtual Memory
			 * on places other than Windows and OSX.
			 */
			(memInfo.totalVirtual > 0) &&
			(memInfo.availVirtual <= memInfo.totalVirtual) &&
#endif /* defined(WIN32) || defined(OSX) */
			(memInfo.availSwap <= memInfo.totalSwap) &&
			(memInfo.timestamp > 0)) {

			/* Print out the memory usage statistics. */
			outputComment(PORTLIB, "Retrieved memory usage statistics.\n");
			outputComment(PORTLIB, "Total physical memory: %llu bytes.\n", memInfo.totalPhysical);
			outputComment(PORTLIB, "Available physical memory: %llu bytes.\n", memInfo.availPhysical);
#if defined(WIN32) || defined(OSX)
			outputComment(PORTLIB, "Total virtual memory: %llu bytes.\n", memInfo.totalVirtual);
			outputComment(PORTLIB, "Available virtual memory: %llu bytes.\n", memInfo.availVirtual);
#else /* defined(WIN32) || defined(OSX) */
			/* This may or may not be available depending on whether a limit is set. Print out if this
			 * is available or else, call this parameter "undefined".
			 */
			if (J9PORT_MEMINFO_NOT_AVAILABLE != memInfo.totalVirtual) {
				outputComment(PORTLIB, "Total virtual memory: %llu bytes.\n", memInfo.totalVirtual);
			} else {
				outputComment(PORTLIB, "Total virtual memory: <undefined>.\n");
			}
			/* Leave Available Virtual memory parameter as it is on non-Windows Platforms. */
			outputComment(PORTLIB, "Available virtual memory: <undefined>.\n");
#endif /* defined(WIN32) || defined(OSX) */
			outputComment(PORTLIB, "Total swap memory: %llu bytes.\n", memInfo.totalSwap);
			outputComment(PORTLIB, "Swap memory free: %llu bytes.\n", memInfo.availSwap);
#if defined(OSX)
			outputComment(PORTLIB, "Cache memory: <undefined>.\n");
#else /* defined(OSX) */
			outputComment(PORTLIB, "Cache memory: %llu bytes.\n", memInfo.cached);
#endif /* defined(OSX) */
#if defined(AIXPPC) || defined(WIN32) || defined(OSX)
			outputComment(PORTLIB, "Buffers memory: <undefined>.\n");
#else /* defined(AIXPPC) || defined(WIN32) || defined(OSX) */
			outputComment(PORTLIB, "Buffers memory: %llu bytes.\n", memInfo.buffered);
#endif /* defined(AIXPPC) || defined(WIN32) || defined(OSX) */
			outputComment(PORTLIB, "Timestamp: %llu.\n", memInfo.timestamp);
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Invalid memory usage statistics retrieved.\n");
		}
	}

	return reportTestExit(portLibrary, testName);
}

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

#define CPU_BURNER_BUFF_SIZE 10000
static UDATA 
cpuBurner(J9PortLibrary* portLibrary, char *myText) 
{
	/* burn up CPU */
	UDATA counter = 0;
	char buffer[CPU_BURNER_BUFF_SIZE];
	char *result = NULL;
	for (counter = 0; counter < CPU_BURNER_BUFF_SIZE; ++counter) {
		buffer[counter] = 0;
	}
	for (counter = 0; (strlen(buffer) + strlen(myText) + 1) < CPU_BURNER_BUFF_SIZE; ++counter) {
		result = strcat(buffer, myText);
		if (NULL != strstr(result, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab")) {
			return 0;
		}
	}
	return 1;
}
/**
 * Test for j9sysinfo_get_processor_info() port library API. Ensure that we are
 * able to obtain processor usage data as also, that it is consistent.
 *
 * @param[in] portLibrary The port library under test.
 *
 * @return TEST_PASS on success, TEST_FAIL on failure.
 */
int
j9sysinfo_testProcessorInfo(J9PortLibrary* portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_testProcessorInfo";
	IDATA rc = 0;
	IDATA cntr = 0;
	IDATA n_onln = 0;
	U_64 deltaTotalIdleTime = 0;
	U_64 deltaTotalBusyTime = 0;

	J9ProcessorInfos prevInfo = {0};
	J9ProcessorInfos currInfo = {0};

	reportTestEntry(portLibrary, testName);

	/* Take a snapshot of processor usage - at t1 (the first iteration). */
	rc = j9sysinfo_get_processor_info(&prevInfo);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_processor_info() failed.\n");

		/* Should not try freeing memory unless it was actually allocated! */
		if (J9PORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED != rc) {
			j9sysinfo_destroy_processor_info(&prevInfo);
		}
		return reportTestExit(portLibrary, testName);
	}

	/* Sleep for 3 seconds before re-sampling processor usage stats.
	 * This allows other processes and the operating system to use the CPU and drive up the
	 * user and kernel utilization.  Use the result of the call to cpuBurner to ensure it isn't optimized out.
	 * Used for validating that the total
	 * processor usage time is approximately in the range of the time elapsed.
	 * Note that this assumption sees deviations of upto 50% at times when the system is lightly loaded
	 * but under much system load, the relation indeed becomes accurate:
	 * 		(Busy-time(t2) - Busy-time(t1)) + (Idle-time(t2) - Idle-time(t1)) ~ T2 - T1.
	 */
	omrthread_sleep(3000 + cpuBurner(portLibrary, "a"));

	rc = j9sysinfo_get_processor_info(&currInfo);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_processor_info() failed.\n");
		j9sysinfo_destroy_processor_info(&prevInfo);

		/* Should not try freeing memory unless it was actually allocated! */
		if (J9PORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED != rc) {
			j9sysinfo_destroy_processor_info(&currInfo);
		}
		return reportTestExit(portLibrary, testName);
	}

	n_onln = onlineProcessorCount(&currInfo);
	if ((currInfo.totalProcessorCount > 0) &&
		(n_onln > 0) &&
		(currInfo.totalProcessorCount >= n_onln) &&
		(prevInfo.timestamp > 0) &&
		(prevInfo.timestamp < currInfo.timestamp)) {

		/* Print out some vital statistics of processor usage - use the current snapshot. */
		outputComment(PORTLIB, "Available processors: %d.\n", currInfo.totalProcessorCount);
		outputComment(PORTLIB, "Online processors: %d.\n", n_onln);
		outputComment(PORTLIB, "Timestamp: %llu.\n", currInfo.timestamp);
	}

	/* First, get the diffs in the Totals over iterations t1 to t2. */
	deltaTotalIdleTime = currInfo.procInfoArray[0].idleTime - prevInfo.procInfoArray[0].idleTime;
	deltaTotalBusyTime = currInfo.procInfoArray[0].busyTime - prevInfo.procInfoArray[0].busyTime;

	outputComment(PORTLIB, "CPUID: Total\n");
	outputComment(PORTLIB, "   User time:   %lld.\n", currInfo.procInfoArray[0].userTime);
	outputComment(PORTLIB, "   System time: %lld.\n", currInfo.procInfoArray[0].systemTime);
	outputComment(PORTLIB, "   Idle time:   %lld.\n", currInfo.procInfoArray[0].idleTime);
#if defined(WIN32) || defined(OSX)
	outputComment(PORTLIB, "   Wait time:   <undefined>.\n");
#else /* defined(WIN32) || defined(OSX) */
	outputComment(PORTLIB, "   Wait time:   %lld.\n", currInfo.procInfoArray[0].waitTime);
#endif /* defined(WIN32) || defined(OSX) */
	outputComment(PORTLIB, "   Busy time:   %lld.\n", currInfo.procInfoArray[0].busyTime);

	/* Start iterating from 1 since 0^th entry represents Totals - already accounted for above. */
	for (cntr = 1; cntr < currInfo.totalProcessorCount + 1; cntr++) {

		/* Sanity check. Ensure that we successfully retrieved processor usage data in various
		 * modes, or else, signal an error. Add platform-specific exceptions (undefined parameter).
		 */
		if (J9PORT_PROCINFO_PROC_ONLINE == currInfo.procInfoArray[cntr].online) {
			if ((J9PORT_PROCINFO_NOT_AVAILABLE != currInfo.procInfoArray[cntr].userTime) &&
				(J9PORT_PROCINFO_NOT_AVAILABLE != currInfo.procInfoArray[cntr].systemTime) &&
				(J9PORT_PROCINFO_NOT_AVAILABLE != currInfo.procInfoArray[cntr].idleTime) &&
#if defined(WIN32) || defined(OSX)
				/* Windows and OSX don't have the notion of Wait times. */
				(J9PORT_PROCINFO_NOT_AVAILABLE == currInfo.procInfoArray[cntr].waitTime) &&
#else /* defined(WIN32) || defined(OSX) */
				(J9PORT_PROCINFO_NOT_AVAILABLE != currInfo.procInfoArray[cntr].waitTime) &&
#endif /* defined(WIN32) || defined(OSX) */
				(J9PORT_PROCINFO_NOT_AVAILABLE != currInfo.procInfoArray[cntr].busyTime)) {

				/* Print out processor times in each mode for each CPU that is online. */
				outputComment(PORTLIB, "CPUID: %d\n",  currInfo.procInfoArray[cntr].proc_id);
				outputComment(PORTLIB, "   User time:   %lld.\n", currInfo.procInfoArray[cntr].userTime);
				outputComment(PORTLIB, "   System time: %lld.\n", currInfo.procInfoArray[cntr].systemTime);
				outputComment(PORTLIB, "   Idle time:   %lld.\n", currInfo.procInfoArray[cntr].idleTime);
#if defined(WIN32) || defined(OSX)
				outputComment(PORTLIB, "   Wait time:   <undefined>.\n");
#else /* defined(WIN32) || defined(OSX) */
				outputComment(PORTLIB, "   Wait time:   %lld.\n", currInfo.procInfoArray[cntr].waitTime);
#endif /* defined(WIN32) || defined(OSX) */
				outputComment(PORTLIB, "   Busy time:   %lld.\n", currInfo.procInfoArray[cntr].busyTime);
			} else {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Invalid processor usage statistics retrieved.\n");
				goto _cleanup;
			}
		}
	} /* end for(;;) */

	/* Check whether the processor times have increased since the last iteration. This ensures a
	 * monotonically increasing nature of processor times. Note: We cannot do this for each individual
	 * processor since there may be architectures such as AIXPPC where times don't change for certain
	 * processors that otherwise seem online (actually are in sleep mode); not even the Idle ticks.
	 */
	if (0 < (deltaTotalBusyTime + deltaTotalIdleTime)) {
		outputComment(PORTLIB, "Processor times in monotonically increasing order.\n");
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected change in processor time deltas\ndeltaTotalBusyTime=%lld deltaTotalIdleTime=%lld\n",
				deltaTotalBusyTime, deltaTotalIdleTime);
	}

_cleanup:
	j9sysinfo_destroy_processor_info(&prevInfo);
	j9sysinfo_destroy_processor_info(&currInfo);
	return reportTestExit(portLibrary, testName);
}

/**
 * Test j9sysinfo_get_number_CPUs_by_type() port library API for the flag J9PORT_CPU_ONLINE.
 * We obtain the online processor count using other (indirect) method - calling the other
 * port library API j9sysinfo_get_processor_info() and cross-check against this.
 *
 * @param[in] portLibrary The port library under test.
 *
 * @return TEST_PASS on success, TEST_FAIL on failure.
 */
int
j9sysinfo_testOnlineProcessorCount(J9PortLibrary* portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_testOnlineProcessorCount";
	IDATA rc = 0;
	IDATA cntr = 0;
	J9ProcessorInfos procInfo = {0};

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
		IDATA n_cpus_online = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE);
		if (-1 == n_cpus_online) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_number_online_CPUs() failed.\n");
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
 * Test j9sysinfo_get_number_CPUs_by_type() port library API for the flag J9PORT_CPU_PHYSICAL.
 * Validate the number of available (configured) logical CPUs by cross-checking with what is
 * obtained from invoking the other port library API j9sysinfo_get_processor_info().
 *
 * @param[in] portLibrary The port library under test.
 *
 * @return TEST_PASS on success, TEST_FAIL on failure.
 */
int
j9sysinfo_testTotalProcessorCount(J9PortLibrary* portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_testTotalProcessorCount";
	IDATA rc = 0;
	IDATA cntr = 0;
	J9ProcessorInfos procInfo = {0};

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
		IDATA n_cpus_total = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_PHYSICAL);
		if (-1 == n_cpus_total) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_number_total_CPUs() failed.\n");
			goto _cleanup;
		}

		if ((procInfo.totalProcessorCount > 0) &&
			(procInfo.totalProcessorCount == n_cpus_total)) {
			outputComment(PORTLIB, "Total number of processors: %d\n",  n_cpus_total);
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Invalid processor count retrieved.\n");
		}
	}

_cleanup:
	j9sysinfo_destroy_processor_info(&procInfo);
	return reportTestExit(portLibrary, testName);
}

int
j9sysinfo_test_get_CPU_utilization(J9PortLibrary* portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_test_get_CPU_utilization";
	J9SysinfoCPUTime OldUtil;
	J9SysinfoCPUTime NewUtil;
	IDATA portLibraryStatus = 0;

	reportTestEntry(portLibrary, testName);

	/*
	 * Call j9sysinfo_get_CPU_utilization() to retrieve the current CPU utilization.
	 * Sanity check the results.
	 */
	portLibraryStatus = j9sysinfo_get_CPU_utilization(&OldUtil);
	if (0 != portLibraryStatus) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_test_get_CPU_utilization() non-zero return code.\n");
		return reportTestExit(portLibrary, testName);
	} else {
		outputComment(PORTLIB, "Old utilization timestamp=%llu cpuTime=%lld numberOfCpus=%d.\n",
				OldUtil.timestamp, OldUtil.cpuTime, OldUtil.numberOfCpus);
		if ((OldUtil.cpuTime < 0) || (OldUtil.numberOfCpus <= 0)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_test_get_CPU_utilization() invalid results.\n");
		}
	}
	/* Sleep for 3 seconds before re-sampling processor usage stats.
	 * This allows other processes and the operating system to use the CPU and drive up the
	 * user and kernel utilization.
	 * The call to cpuBurner probably won't be optimized out, but use the result to make absolutely sure that it isn't.
	 */
	omrthread_sleep(3000 + cpuBurner(portLibrary, "a"));
	portLibraryStatus = j9sysinfo_get_CPU_utilization(&NewUtil);
	if (0 != portLibraryStatus) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_test_get_CPU_utilization() non-zero return code.\n");
		return reportTestExit(portLibrary, testName);
	} else {
		outputComment(PORTLIB, "New utilization timestamp=%llu cpuTime=%lld numberOfCpus=%d.\n",
				NewUtil.timestamp, NewUtil.cpuTime, NewUtil.numberOfCpus);
		outputComment(PORTLIB, "timestamp delta=%llu cpuTime delta=%lld\n",
				NewUtil.timestamp-OldUtil.timestamp, NewUtil.cpuTime -  OldUtil.cpuTime);
		if ((NewUtil.cpuTime < OldUtil.cpuTime) || (NewUtil.numberOfCpus != OldUtil.numberOfCpus)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_test_get_CPU_utilization() invalid results.\n");
		}
	}

	return reportTestExit(portLibrary, testName);
}

#endif /* !defined(J9ZOS390) */


/*
 * Test j9sysinfo_get_tmp when the buffer size == 0
 * Expected result size of buffer required
 */
I_32
j9sysinfo_test_get_tmp1(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	IDATA rc = 0;
	const char* testName = "j9sysinfo_test_get_tmp1";

	reportTestEntry(portLibrary, testName);

	rc = j9sysinfo_get_tmp(NULL, 0, FALSE);
	if (rc <= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "unexpected return code rc: %d\n", rc);
	} else {
		char *buffer = (char *) j9mem_allocate_memory(rc, OMRMEM_CATEGORY_PORT_LIBRARY );
		outputComment(PORTLIB, "rc = %d\n", rc);

		rc = j9sysinfo_get_tmp(buffer, rc, FALSE);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "unexpected return code rc: %d\n", rc);
		}
		j9mem_free_memory(buffer);
	}
	return reportTestExit(portLibrary, testName);
}

/*
 * Test j9sysinfo_get_tmp when the buffer size is smaller then required
 * Expected result size of buffer required
 */
I_32
j9sysinfo_test_get_tmp2(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	IDATA rc = 0;
	const char* testName = "j9sysinfo_test_get_tmp2";
	char *buffer = NULL;
	const UDATA smallBufferSize = 4;

	reportTestEntry(portLibrary, testName);

	buffer = (char*)j9mem_allocate_memory( smallBufferSize, OMRMEM_CATEGORY_PORT_LIBRARY );
	rc = j9sysinfo_get_tmp(buffer, smallBufferSize, FALSE);

	if (0 >= rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "unexpected return code rc: %d\n", rc);
	} else {
		outputComment(PORTLIB, "rc = %d\n", rc);
		j9mem_free_memory(buffer);

		buffer = (char *) j9mem_allocate_memory(rc, OMRMEM_CATEGORY_PORT_LIBRARY );
		rc = j9sysinfo_get_tmp(buffer, rc, FALSE);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "unexpected return code rc: %d\n", rc);
		}
	}
	j9mem_free_memory(buffer);
	return reportTestExit(portLibrary, testName);
}

/*
 * Test j9sysinfo_get_tmp when TMPDIR is non-ascii (Unicode on windows, UTF-8 on other platforms)
 * Expected result: Successfully create non-ascii directory, and create a file there.
 */
I_32
j9sysinfo_test_get_tmp3(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9systeminfo_test_get_tmp3";
	char *buffer = NULL;
	IDATA rc = 0;
	const char* data = "Hello World!";
	IDATA tmpFile = 0;

#if defined(WIN32)
	wchar_t *origEnv = NULL;
	const char utf8[]       = {0x63, 0x3A, 0x5C, 0xD0, 0xB6, 0xD0, 0xB0, 0xD0, 0xB1, 0xD0, 0xB0, 0x5C, 0x00};
	const char utf8_file[]  = {0x63, 0x3A, 0x5C, 0xD0, 0xB6, 0xD0, 0xB0, 0xD0, 0xB1, 0xD0, 0xB0, 0x5C, 0x74, 0x65, 0x73, 0x74, 0x2E, 0x74, 0x78, 0x74, 0x00};
	const wchar_t unicode[] = {0x0063, 0x003A, 0x005C, 0x0436, 0x0430, 0x0431, 0x0430, 0x005C, 0x00};

	reportTestEntry(portLibrary, testName);

	origEnv = (wchar_t*)j9mem_allocate_memory( EsMaxPath, OMRMEM_CATEGORY_PORT_LIBRARY );
	wcscpy(origEnv, _wgetenv(L"TMP"));
	rc = _wputenv_s(L"TMP", unicode);
#else /* defined(WIN32) */
	char *origEnv = NULL;
	const char *utf8 = "/tmp/test/";
	const char *utf8_file = "/tmp/test/test.txt";
	char *origEnvRef = getenv("TMPDIR");
#if defined(J9ZOS390)
	char *envVarInEbcdic = a2e_string("TMPDIR");
	char *origEnvInEbcdic = NULL;
	char *utf8InEbcdic = a2e_string(utf8);
#endif /* defined(J9ZOS390) */

	reportTestEntry(portLibrary, testName);

	if (NULL != origEnvRef) {
		origEnv = (char*)j9mem_allocate_memory( strlen(origEnvRef) + 1, OMRMEM_CATEGORY_PORT_LIBRARY );
		if (NULL != origEnv) {
			strcpy(origEnv, origEnvRef);
#if defined(J9ZOS390)
			origEnvInEbcdic = a2e_string(origEnv);
#endif /* defined(J9ZOS390) */
		}
	}

#if defined(J9ZOS390)
	rc = setenv(envVarInEbcdic, utf8InEbcdic, 1);
#else /* defined(J9ZOS390) */
	rc = setenv("TMPDIR", utf8, 1);
#endif /* defined(J9ZOS390) */

#endif /* defined(WIN32) */

	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error to update environment variable rc: %d\n", rc);
	}

	buffer = (char*)j9mem_allocate_memory( EsMaxPath, OMRMEM_CATEGORY_PORT_LIBRARY );
	rc = j9sysinfo_get_tmp(buffer, EsMaxPath, FALSE);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error failed to get temp directory rc: %d\n", rc);
	} else {
		outputComment(PORTLIB, "TMP = %s\n", buffer);
	}

	rc = memcmp(utf8, buffer, strlen(utf8));
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "invalid directory rc: %d buffer %s, utf8 = %s\n", rc, buffer, utf8);
	}

	rc = j9file_mkdir(utf8);
	if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error failed to create temp directory rc: %d\n", rc);
	}

	tmpFile = j9file_open(utf8_file, EsOpenWrite | EsOpenCreateNew, 0666);
	if (-1 == tmpFile) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error failed to create temp file rc: %d\n", rc);
	}

	rc = j9file_write(tmpFile, data, strlen(data));
	if (0 > rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error failed to write to temp file rc: %d\n", rc);
	}

	rc = j9file_close(tmpFile);
	if (-1 == rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error failed to close temp file rc: %d\n", rc);
	}

	rc = j9file_unlink(utf8_file);
	if (-1 == rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error failed to delete temp file rc: %d\n", rc);
	}

	rc = j9file_unlinkdir(utf8);
	if (-1 == rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error failed to delete temp directory rc: %d\n", rc);
	}

	if (NULL != origEnv) {
#if defined(WIN32)
		_wputenv_s(L"TMP", origEnv);
#elif defined(J9ZOS390) /* defined(WIN32) */
		setenv(envVarInEbcdic, origEnvInEbcdic, 1);
#else /* defined(J9ZOS390) */
		setenv("TMPDIR", origEnv, 1);
#endif /* defined(J9ZOS390) */
		j9mem_free_memory(origEnv);
	} else {
#if defined(WIN32)
		_wputenv_s(L"TMP", L"");
#elif defined(J9ZOS390) /* defined(WIN32) */
		unsetenv(envVarInEbcdic);
#else /* defined(J9ZOS390) */
		unsetenv("TMPDIR");
#endif /* defined(WIN32) */
	}

#if defined(J9ZOS390)
	if (NULL != envVarInEbcdic) {
		free(envVarInEbcdic);
	}
	if (NULL != origEnvInEbcdic) {
		free(origEnvInEbcdic);
	}
	if (NULL != utf8InEbcdic) {
		free(utf8InEbcdic);
	}
#endif /* defined(J9ZOS390) */

	if (NULL != buffer) {
		j9mem_free_memory(buffer);
	}
	return reportTestExit(portLibrary, testName);
}

#if !defined(WIN32)
/*
 * Test j9sysinfo_get_tmp when ignoreEnvVariable is FALSE/TRUE
 * Expected result size of buffer required
 */
I_32
j9sysinfo_test_get_tmp4(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	IDATA rc = 0;
	const char *testName = "j9sysinfo_test_get_tmp4";
	char *envVar = "TMPDIR";
	char *oldTmpDir = NULL;
	char *oldTmpDirValue = NULL;
	char *modifiedTmpDir = "j9sysinfo_test_get_tmp4_dir";
#if defined(J9ZOS390)
	char *envVarInEbcdic = a2e_string(envVar);
	char *oldTmpDirValueInEbcdic = NULL;
	char *modifiedTmpDirInEbcdic = a2e_string(modifiedTmpDir);
#endif /* defined(J9ZOS390) */

	reportTestEntry(portLibrary, testName);

	oldTmpDir = getenv(envVar);
	if (NULL != oldTmpDir) {
		oldTmpDirValue = (char*)j9mem_allocate_memory(strlen(oldTmpDir) + 1, OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL != oldTmpDirValue) {
			strcpy(oldTmpDirValue, oldTmpDir);
#if defined(J9ZOS390)
			oldTmpDirValueInEbcdic = a2e_string(oldTmpDirValue);
#endif /* defined(J9ZOS390) */
		}
	}

#if defined(J9ZOS390)
	rc = setenv(envVarInEbcdic, modifiedTmpDirInEbcdic, 1);
#else /* defined(J9ZOS390) */
	rc = setenv(envVar, modifiedTmpDir, 1);
#endif /* defined(J9ZOS390) */
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error in updating environment variable TMPDIR, rc: %zd\n", rc);
	}

	rc = j9sysinfo_get_tmp(NULL, 0, FALSE);
	if (rc <= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "unexpected return code rc: %zd\n", rc);
	} else {
		char *buffer = (char *) j9mem_allocate_memory(rc, OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == buffer) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "failed to allocate memory for buffer\n");
		} else {
			rc = j9sysinfo_get_tmp(buffer, rc, FALSE);
			if (0 != rc) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_tmp failed with rc: %zd\n", rc);
			} else {
				if (strcmp(modifiedTmpDir, buffer)) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "expected j9sysinfo_get_tmp to return same value as TMPDIR. TMPDIR: %s, returned: %s\n", modifiedTmpDir, buffer);
				}
			}
			j9mem_free_memory(buffer);
		}
	}

	rc = j9sysinfo_get_tmp(NULL, 0, TRUE);
	if (rc <= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "unexpected return code rc: %d\n", rc);
	} else {
		char *buffer = (char *) j9mem_allocate_memory(rc, OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == buffer) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "failed to allocate memory for buffer\n");
		} else {
			rc = j9sysinfo_get_tmp(buffer, rc, TRUE);
			if (0 != rc) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_tmp failed with rc: %d\n", rc);
			} else {
				if (!strcmp(modifiedTmpDir, buffer)) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "expected j9sysinfo_get_tmp to ignore TMPDIR. TMPDIR: %s, returned: %s\n", modifiedTmpDir, buffer);
				}
			}
			j9mem_free_memory(buffer);
		}
	}

	/* restore TMPDIR */
#if defined(J9ZOS390)
	if (NULL != oldTmpDirValue) {
		setenv(envVarInEbcdic, oldTmpDirValueInEbcdic, 1);
		j9mem_free_memory(oldTmpDirValue);
	} else {
		unsetenv(envVarInEbcdic);
	}
	if (NULL != envVarInEbcdic) {
		free(envVarInEbcdic);
	}
	if (NULL != oldTmpDirValueInEbcdic) {
		free(oldTmpDirValueInEbcdic);
	}
	if (NULL != modifiedTmpDirInEbcdic) {
		free(modifiedTmpDirInEbcdic);
	}
#else /* defined(J9ZOS390) */
	if (NULL != oldTmpDirValue) {
		setenv(envVar, oldTmpDirValue, 1);
		j9mem_free_memory(oldTmpDirValue);
	} else {
		unsetenv(envVar);
	}
#endif /* defined(J9ZOS390) */

	return reportTestExit(portLibrary, testName);
}
#endif /* !defined(WIN32) */

/*
 * Test j9sysinfo_get_cwd when the buffer size == 0, then allocate required amount of bites and try again.
 * Expected result size of buffer required
 */
I_32
j9sysinfo_test_get_cwd1(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	IDATA rc = 0;
	const char* testName = "j9sysinfo_test_get_cwd1";

	reportTestEntry(portLibrary, testName);

	rc = j9sysinfo_get_cwd(NULL, 0);
	if (rc <= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "unexpected return code rc: %d\n", rc);
	} else {
		char *buffer = (char *) j9mem_allocate_memory(rc, OMRMEM_CATEGORY_PORT_LIBRARY );
		outputComment(PORTLIB, "rc = %d\n", rc);

		rc = j9sysinfo_get_cwd(buffer, rc);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "unexpected return code rc: %d\n", rc);
		}
		j9mem_free_memory(buffer);
	}
	return reportTestExit(portLibrary, testName);
}
/*
 * Test j9sysinfo_get_cwd when the buffer size is smaller then required
 * Expected result size of buffer required
 */
I_32
j9sysinfo_test_get_cwd2(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	IDATA rc = 0;
	const char* testName = "j9sysinfo_test_get_cwd2";
	char *buffer = NULL;
	const UDATA smallBufferSize = 4;

	reportTestEntry(portLibrary, testName);

	buffer = (char*)j9mem_allocate_memory( smallBufferSize, OMRMEM_CATEGORY_PORT_LIBRARY );
	rc = j9sysinfo_get_cwd(buffer, smallBufferSize);

	if (0 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "unexpected return code rc: %d\n", rc);
	} else {
		outputComment(PORTLIB, "rc = %d\n", rc);
		j9mem_free_memory(buffer);

		buffer = (char *) j9mem_allocate_memory(rc, OMRMEM_CATEGORY_PORT_LIBRARY );
		rc = j9sysinfo_get_cwd(buffer, rc);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "unexpected return code rc: %d\n", rc);
		}
	}
	j9mem_free_memory(buffer);
	return reportTestExit(portLibrary, testName);
}

/*
 * Test j9sysinfo_get_cwd in not ascii directory.
 * Expected result: Successfully create not ascii directory, change current current directory, verify that j9sysinfo_get_cwd returns valid value.
 */
I_32
j9sysinfo_test_get_cwd3(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	IDATA rc = 0;
	const char* testName = "j9sysinfo_test_get_cwd3";
	char *buffer = NULL;
	char *orig_cwd = NULL;

#if defined(WIN32)
	/* c:\U+6211 U+7684 U+7236 U+4EB2 U+662F U+6536 U+68D2 U+5B50 U+7684 */
	const wchar_t unicode[] = {0x0063, 0x003A, 0x005C, 0x6211, 0x7684, 0x7236, 0x4EB2, 0x662F, 0x6536, 0x68D2, 0x5B50, 0x7684, 0x005C, 0x00};
	const char utf8[]       = {0x63, 0x3A, 0x5C, 0xE6, 0x88, 0x91, 0xE7, 0x9A, 0x84, 0xE7, 0x88, 0xB6, 0xE4, 0xBA, 0xB2, 0xE6, 0x98, 0xAF, 0xE6, 0x94, 0xB6, 0xE6, 0xA3, 0x92, 0xE5, 0xAD, 0x90, 0xE7, 0x9A, 0x84, 0x5C, 0x00};

	reportTestEntry(portLibrary, testName);

	orig_cwd = (char*)j9mem_allocate_memory( EsMaxPath, OMRMEM_CATEGORY_PORT_LIBRARY );
	j9sysinfo_get_cwd(orig_cwd, EsMaxPath);

	rc = j9file_mkdir(utf8);
	if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error failed to create directory rc: %d\n", rc);
	}
	rc = _wchdir(unicode);
	if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error failed to change current directory rc: %d\n", rc);
	}
#else /* defined(WIN32) */
#if defined(OSX)
	const char *utf8 = "/private/tmp/j9sysinfo_test_get_cwd3/";
#else /* defined(OSX) */
	const char *utf8 = "/tmp/j9sysinfo_test_get_cwd3/";
#endif /* defined(OSX) */

	reportTestEntry(portLibrary, testName);

	orig_cwd = (char*)j9mem_allocate_memory( EsMaxPath, OMRMEM_CATEGORY_PORT_LIBRARY );
	j9sysinfo_get_cwd(orig_cwd, EsMaxPath);

	rc = j9file_mkdir(utf8);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error failed to create directory rc: %d\n", rc);
	} else {
		outputComment(portLibrary, "mkdir %s\n", utf8);
	}
#if defined(J9ZOS390)
	rc = atoe_chdir(utf8);
#else /* defined(J9ZOS390) */
	rc = chdir(utf8);
#endif /* defined(J9ZOS390) */
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cd %s failed rc: %d\n", utf8, rc);
	} else {
		outputComment(portLibrary, "cd %s\n", utf8);
	}
#endif /* defined(WIN32) */

	buffer = (char*)j9mem_allocate_memory( EsMaxPath, OMRMEM_CATEGORY_PORT_LIBRARY );
	rc = j9sysinfo_get_cwd(buffer, EsMaxPath);

	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error failed to get current working directory rc: %d\n", rc);
	} else {
		outputComment(PORTLIB, "CWD = %s\n", buffer);
	}

	rc = memcmp(utf8, buffer, strlen(buffer));
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "invalid directory rc: %d\n", rc);
	}

#if defined(WIN32)
	rc = _chdir(orig_cwd); /* we need to exit current directory before deleting it*/
#elif defined(J9ZOS390)
	rc = atoe_chdir(orig_cwd);
#else /* defined(WIN32) */
	rc = chdir(orig_cwd);
#endif /* defined(WIN32) */

	if (-1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cd %s failed rc: %d\n", orig_cwd, rc);
	}

	rc = j9file_unlinkdir(utf8);
	if (-1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error failed to delete directory %s rc: %d\n", utf8, rc);
	}

	j9mem_free_memory(orig_cwd);
	j9mem_free_memory(buffer);
	return reportTestExit(portLibrary, testName);
}

#if !(defined(WIN32) || defined(WIN64))
/*
 * Test j9sysinfo_get_groups.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on failure
 */
I_32
j9sysinfo_test_get_groups(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	IDATA rc = 0;
	IDATA i;
	const char *testName = "j9sysinfo_test_get_groups";
	U_32 *gidList = NULL;

	reportTestEntry(portLibrary, testName);

	rc = j9sysinfo_get_groups(&gidList, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (-1 != rc) {
		IDATA rc1;
		struct group *grent = NULL;

		outputComment(PORTLIB, "group list size=%zi\n", rc);

		for (i = 0; i < rc; i++) {
			int error = 0;
			/* Set errno to 0 before calling getgrgid() to correctly handle NULL return values */
			errno = 0;
			/* No portlib api to get group name for a give group id */
			grent = getgrgid(gidList[i]);
			error = errno;
			outputComment(PORTLIB, "\tgid=%u", gidList[i]);
			if (NULL == grent) {
				if (0 == error) {
					outputComment(PORTLIB, "\tthis group id is not found in group database (not an error as per getgrgid documentation)\n");
				} else {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "\ngetgrgid() returned NULL with errno=%d for group id=%u\n", error, gidList[i]);
					break;
				}
			} else {
				char *group = grent->gr_name;
				if (NULL != group) {
					outputComment(PORTLIB, "\tgroup name=%s\n", group);
				} else {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "\ngetgrgid() returned NULL as the group name for group id=%u\n", gidList[i]);
					break;
				}
			}
		}
	} else {
		char *lastErrorMessage = (char *)j9error_last_error_message();
		I_32 lastErrorNumber = j9error_last_error_number();

		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_groups returned %zi\n"
						"\tlastErrorNumber=%d, lastErrorMessage=%s\n", rc, lastErrorNumber, lastErrorMessage);
	}
	return reportTestExit(portLibrary, testName);
}
#endif /* !(defined(WIN32) || defined(WIN64)) */

/*
 * Test j9sysinfo_test_get_levels_and_types.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on failure
 */
I_32
j9sysinfo_test_get_levels_and_types(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	I_32 rc = 0;
	const char *testName = "j9sysinfo_test_get_levels_and_types";
	J9CacheInfoQuery cQuery;
	I_32 levelCount = 0;
	I_32 numLevels = 0;

	reportTestEntry(portLibrary, testName);
	memset(&cQuery, 0, sizeof(cQuery));
	numLevels = 2;
	cQuery.cmd = J9PORT_CACHEINFO_QUERY_LEVELS;
	rc = j9sysinfo_get_cache_info(&cQuery);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_cache_info returned %d\n", rc);
		return reportTestExit(portLibrary, testName);
	}
	numLevels = rc;
	outputComment(PORTLIB, "levels=%d\n", numLevels);
	for (levelCount = 1; levelCount <= numLevels; ++levelCount) {
		BOOLEAN isUCache = FALSE;
		BOOLEAN hasICache = FALSE;
		BOOLEAN hasDCache = FALSE;
		IDATA DCacheLineSize = 0;
		IDATA ICacheLineSize = 0;
		IDATA DCacheSize = 0;
		IDATA ICacheSize = 0;

		cQuery.cmd = J9PORT_CACHEINFO_QUERY_TYPES;
		cQuery.level = levelCount;
		rc = j9sysinfo_get_cache_info(&cQuery);
		if (rc < 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_cache_info returned %d\n", rc);
			return reportTestExit(portLibrary, testName);
		}
		outputComment(PORTLIB, "level=%d is: ", levelCount);

		if (J9_ARE_ANY_BITS_SET(rc, J9PORT_CACHEINFO_DCACHE)) {
			outputComment(PORTLIB, "DCache, ");
			hasDCache = TRUE;
		}
		if (J9_ARE_ANY_BITS_SET(rc, J9PORT_CACHEINFO_ICACHE)) {
			outputComment(PORTLIB, "ICache, ");
			hasICache = TRUE;
		}
		if (J9_ARE_ANY_BITS_SET(rc, J9PORT_CACHEINFO_UCACHE)) {
			outputComment(PORTLIB, "UCache, ");
			hasICache = TRUE;
			hasDCache = TRUE;
		}
		if (J9_ARE_ANY_BITS_SET(rc, J9PORT_CACHEINFO_TCACHE)) {
			outputComment(PORTLIB, "TCache, ");
		}


		if (hasDCache) {
			cQuery.cmd = J9PORT_CACHEINFO_QUERY_LINESIZE;
			cQuery.cacheType = J9PORT_CACHEINFO_DCACHE;
			rc = j9sysinfo_get_cache_info(&cQuery);
			if (rc < 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_cache_info returned %d\n", rc);
				return reportTestExit(portLibrary, testName);
			}
			DCacheLineSize = rc;

			cQuery.cmd = J9PORT_CACHEINFO_QUERY_CACHESIZE;
			rc = j9sysinfo_get_cache_info(&cQuery);
			if (rc < 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_cache_info returned %d\n", rc);
				return reportTestExit(portLibrary, testName);
			}
			DCacheSize = rc;

		}

		if (hasICache) {
			cQuery.cmd = J9PORT_CACHEINFO_QUERY_LINESIZE;
			cQuery.cacheType = J9PORT_CACHEINFO_ICACHE;
			rc = j9sysinfo_get_cache_info(&cQuery);
			if (rc < 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_cache_info returned %d\n", rc);
				return reportTestExit(portLibrary, testName);
			}
			ICacheLineSize = rc;

			cQuery.cmd = J9PORT_CACHEINFO_QUERY_CACHESIZE;
			rc = j9sysinfo_get_cache_info(&cQuery);
			if (rc < 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_cache_info returned %d\n", rc);
				return reportTestExit(portLibrary, testName);
			}
			ICacheSize = rc;

		}

		outputComment(PORTLIB, "DCache size = %d line size = %d, ICache size %d line size = %d\n",
				DCacheSize, DCacheLineSize, ICacheSize, ICacheLineSize);
		if (isUCache && (ICacheSize != DCacheSize)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "DCache size = %d ICache size = %d do not match for unified cache\n",
					DCacheLineSize, ICacheLineSize, rc);
			return reportTestExit(portLibrary, testName);
		}
		if (isUCache && (ICacheLineSize != DCacheLineSize)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "DCache line size = %d ICache line size = %d do not match for unified cache\n",
					DCacheLineSize, ICacheLineSize, rc);
			return reportTestExit(portLibrary, testName);
		}
	}

	return reportTestExit(portLibrary, testName);
}

#if defined(LINUX) || defined(AIXPPC)
/*
 * Test j9sysinfo_test_get_open_file_count.
 * Available only on Linux and AIX.
 */
int j9sysinfo_test_get_open_file_count(J9PortLibrary* portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sysinfo_test_get_open_file_count";
    I_32 ret = 0;
	U_64 openCount = 0;
	U_64 curLimit = 0;
	U_32 rc = 0;

	reportTestEntry(portLibrary, testName);
	/* Get the number of files opened till this point. */
    ret = j9sysinfo_get_open_file_count(&openCount);
    if (ret < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_open_file_count() failed.\n");
		return reportTestExit(portLibrary, testName);
    }
	outputComment(PORTLIB, "j9sysinfo_get_open_file_count(): Files opened by this process=%lld\n",
		openCount);

	/* Now, get the current (soft) limit on the resource "nofiles".  We check the current
	 * number of files opened, against this.
	 */
	rc = j9sysinfo_get_limit(J9PORT_RESOURCE_FILE_DESCRIPTORS, &curLimit);
	if (J9PORT_LIMIT_UNLIMITED == rc) {
		/* Not really an error, just a sentinel.  Comparisons can still work! */
		if (RLIM_INFINITY == curLimit) {
			outputComment(PORTLIB,
				"j9sysinfo_get_limit(nofiles): soft limit=RLIM_INFINITY (unlimited).\n");
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS,
				"j9sysinfo_get_limit(nofiles): soft limit (unlimited), bad maximum reported %lld.\n",
				((I_64) curLimit));
			return reportTestExit(portLibrary, testName);
		}
	} else if (J9PORT_LIMIT_LIMITED == rc) {
		/* Check that the limits received are sane, before comparing against files opened. */
		if ((((I_64) curLimit) > 0) && (((I_64) curLimit) <= LLONG_MAX)) {
			outputComment(PORTLIB, "j9sysinfo_get_limit(nofiles): soft limit=%lld.\n",
				((I_64) curLimit));
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS,
				"j9sysinfo_get_limit(nofiles) failed: bad limits received!\n");
			return reportTestExit(portLibrary, testName);
		}
	} else { /* The port library failed! */
		outputErrorMessage(PORTTEST_ERROR_ARGS, 
			"j9sysinfo_get_limit(nofiles): failed with error code=%d.\n",
			j9error_last_error_number());
		return reportTestExit(portLibrary, testName);
	}
	/* Sanity check: are more files reported as opened than the limit? */
	if (((I_64) openCount) > ((I_64) curLimit)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS,
			"j9sysinfo_get_open_file_count() failed: reports more files opened than allowed!\n");
		return reportTestExit(portLibrary, testName);
	}
	return reportTestExit(portLibrary, testName);
}
#endif /* defined(LINUX) || defined(AIXPPC) */

I_32
j9sysinfo_test_get_l1dcache_line_size(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	IDATA rc = 0;
	IDATA l1DcacheSize = 0;
	const char *testName = "j9sysinfo_test_get_l1dcache_line_size";
	J9CacheInfoQuery cQuery;

	reportTestEntry(portLibrary, testName);
	memset(&cQuery, 0, sizeof(cQuery));
	cQuery.cmd = J9PORT_CACHEINFO_QUERY_LINESIZE;
	cQuery.level = 1;
	cQuery.cacheType = J9PORT_CACHEINFO_DCACHE;
	rc = j9sysinfo_get_cache_info(&cQuery);
#if !(defined(J9ARM))
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_cache_info returned %d\n", rc);
	} else {
		l1DcacheSize = rc;
		outputComment(PORTLIB, "DCache line size = %d\n",l1DcacheSize);

		rc = j9sysinfo_get_cache_info(&cQuery);
		if (rc < 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_cache_info returned %d\n", rc);
		}
		if (rc != l1DcacheSize) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_cache_info expected %d got %d\n",
					l1DcacheSize, rc);
		}
	}
#else /*!(defined(J9ARM))*/
	outputComment(PORTLIB, "j9sysinfo_get_cache_info is not supported on this platform\n");
	if (J9PORT_ERROR_SYSINFO_NOT_SUPPORTED != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_cache_info should have returned %d but returned %d\n", 
			J9PORT_ERROR_SYSINFO_NOT_SUPPORTED, rc);
	}
#endif /*!(defined(J9ARM))*/

	return reportTestExit(portLibrary, testName);
}

/*
 * pass in the port library to do sysinfo tests
 */
int 
j9sysinfo_runTests(struct J9PortLibrary *portLibrary, char *argv0) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc=TEST_PASS;
	
	HEADING(portLibrary, "Sysinfo Test");
	
	rc |= j9sysinfo_test0(portLibrary, argv0);
	rc |= j9sysinfo_test1(portLibrary);
	rc |= j9sysinfo_test2(portLibrary);
	rc |= j9sysinfo_test3(portLibrary);
	rc |= j9sysinfo_get_OS_type_test(portLibrary);
	rc |= j9sysinfo_test_sysinfo_ulimit_iterator(portLibrary);
	rc |= j9sysinfo_test_sysinfo_env_iterator(portLibrary);
	rc |= j9sysinfo_test_sysinfo_get_processor_description(portLibrary);
#if !(defined(WIN32) || defined(WIN64))
#if !(defined(AIXPPC) || defined(J9ZOS390))
	/* unable to set RLIMIT_AS on AIX and z/OS */
	rc |= j9sysinfo_test_sysinfo_set_limit_ADDRESS_SPACE(portLibrary);
#endif /* !(defined(AIXPPC) || defined(J9ZOS390)) */
	rc |= j9sysinfo_test_sysinfo_set_limit_CORE_FILE(portLibrary);
#endif /* !(defined(WIN32) || defined(WIN64)) */

#if defined(AIXPPC)
	rc |= j9sysinfo_test_sysinfo_set_limit_CORE_FLAGS(portLibrary);
#endif /* defined(AIXPPC) */

#if !(defined(WIN32) || defined(WIN64))
	/* Supported on all our Unix platforms - AIX, Linux, z/OS, and OSX. */
	rc |= j9sysinfo_test_sysinfo_get_limit_FILE_DESCRIPTORS(portLibrary);
#endif /* !(defined(WIN32) || defined(WIN64)) */

#if !defined(J9ZOS390)
	/* Since the processor and memory usage port library APIs are not available on zOS (neither
	 * 31-bit not 64-bit) yet, so we exclude these tests from running on zOS. When the zOS
	 * implementation is available, we must remove these guards so that they are built and
	 * executed on the z as well. Remove this comment too.
	 */
	rc |= j9sysinfo_testMemoryInfo(portLibrary);
	rc |= j9sysinfo_testProcessorInfo(portLibrary);
	rc |= j9sysinfo_testOnlineProcessorCount(portLibrary);
	rc |= j9sysinfo_testTotalProcessorCount(portLibrary);
	rc |= j9sysinfo_test_get_CPU_utilization(portLibrary);
#endif /* !defined(J9ZOS390) */
	rc |= j9sysinfo_test_get_tmp1(portLibrary);
	rc |= j9sysinfo_test_get_tmp2(portLibrary);
	rc |= j9sysinfo_test_get_tmp3(portLibrary);

#if !defined(WIN32)
	rc |= j9sysinfo_test_get_tmp4(portLibrary);
#endif /* !defined(WIN32) */

	rc |= j9sysinfo_test_get_cwd1(portLibrary);
	rc |= j9sysinfo_test_get_cwd2(portLibrary);
	rc |= j9sysinfo_test_get_cwd3(portLibrary);
#if !(defined(WIN32) || defined(WIN64))
	rc |= j9sysinfo_test_get_groups(portLibrary);
#endif /* !(defined(WIN32) || defined(WIN64)) */
	rc |= j9sysinfo_test_get_l1dcache_line_size(portLibrary);
#if !(defined(LINUXPPC) || defined(S390) || defined(J9ZOS390) || defined(J9ARM) || defined(OSX))
	rc |= j9sysinfo_test_get_levels_and_types(portLibrary);
#endif /* !(defined(LINUXPPC) || defined(S390) || defined(J9ZOS390) || defined(J9ARM) || defined(OSX)) */
#if defined(LINUX) || defined(AIXPPC)
	/* Not supported on Z & OSX (and Windows, of course).  Enable, when available. */
	rc |= j9sysinfo_test_get_open_file_count(portLibrary);
#endif /* defined(LINUX) || defined(AIXPPC) */

	/* Output results */
	j9tty_printf(PORTLIB, "\nSysinfo test done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	return TEST_PASS == rc ? 0 : -1;
	
	return 0;
}
