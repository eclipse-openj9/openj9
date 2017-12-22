/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
 * $RCSfile: main.c,v $
 * $Revision: 1.66 $
 * $Date: 2013-03-21 14:42:10 $
 */

#include <string.h>
#include <stdlib.h>
#include "testHelpers.h"
#if defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC)
#include <sys/types.h>
#include <unistd.h>
#endif
#include "libhlp.h"

#define J9PORT_TEST_ALL 				((UDATA)0xFC9FFFFF)
#define J9PORT_TEST_J9ERROR 			((UDATA)0x00000001)
#define J9PORT_TEST_J9FILE 				((UDATA)0x00000002)
#define J9PORT_TEST_J9MEM 				((UDATA)0x00000004)
#define J9PORT_TEST_J9PORT 				((UDATA)0x00000008)
#define J9PORT_TEST_J9STR 				((UDATA)0x00000010)
#define J9PORT_TEST_J9TIME 				((UDATA)0x00000020)
#define J9PORT_TEST_J9TTY 				((UDATA)0x00000040)
#define J9PORT_TEST_J9SIGNAL 			((UDATA)0x00000080)
#define J9PORT_TEST_J9SYSINFO 			((UDATA)0x00000100)
#define J9PORT_TEST_J9SHSEM 			((UDATA)0x00000200)
#define J9PORT_TEST_J9SHMEM 			((UDATA)0x00000400)
#define J9PORT_TEST_J9MMAP 				((UDATA)0x00000800)
#define J9PORT_TEST_J9VMEM 				((UDATA)0x00001000)
#define J9PORT_TEST_J9HEAP 				((UDATA)0x00002000)
#define J9PORT_TEST_J9DUMP 				((UDATA)0x00004000)
#define J9PORT_TEST_J9SOCK 				((UDATA)0x00008000)
#define J9PORT_TEST_J9PROCESS 			((UDATA)0x00010000)
#define J9PORT_TEST_J9NLS 				((UDATA)0x00020000)
#define J9PORT_TEST_J9SL 				((UDATA)0x00040000)
#define J9PORT_TEST_J9SHSEM_DEPRECATED 	((UDATA)0x00080000)
#define J9PORT_TEST_J9HYPERVISOR		((UDATA)0x00200000)
#define J9PORT_TEST_NUMCPUS				((UDATA)0x00400000)
#define J9PORT_TEST_INSTRUMENTATION		((UDATA)0x00800000)
#define J9PORT_TEST_J9SIG_EXTENDED 		((UDATA)0x01000000)
#define J9PORT_TEST_J9TTY_EXTENDED 		((UDATA)0x02000000)
#define J9PORT_TEST_J9FILE_BLOCKINGASYNC ((UDATA)0x04000000)
#define J9PORT_TEST_J9CUDA              ((UDATA)0x08000000)

extern int j9error_runTests(struct J9PortLibrary *portLibrary); /** @see j9errorTest.c::j9error_runTests */
extern int j9file_runTests(struct J9PortLibrary *portLibrary, char *argv0, char* j9file_child, BOOLEAN asynch); /** @see j9fileTest.c::j9file_runTests */
extern int j9mem_runTests(struct J9PortLibrary *portLibrary, int randomSeed); /** @see j9memTest.c::j9mem_runTests */
extern int j9heap_runTests(struct J9PortLibrary *portLibrary, int randomSeed); /** @see j9heapTest.c::j9heap_runTests */
extern int j9port_runTests(struct J9PortLibrary *portLibrary); /** @see j9ttyTest.c::j9tty_runTests */
extern int j9str_runTests(struct J9PortLibrary *portLibrary); /** @see j9ttyTest.c::j9tty_runTests */
extern int j9time_runTests(struct J9PortLibrary *portLibrary); /** @see j9timeTest.c::j9time_runTests */
extern int j9tty_runTests(struct J9PortLibrary *portLibrary); /** @see j9ttyTest.c::j9tty_runTests */
extern int j9tty_runExtendedTests(struct J9PortLibrary *portLibrary); /** @see j9ttyTest.c::j9tty_runTests */
extern int si_test(struct J9PortLibrary *portLibrary, const char*);
extern int socket_test(struct J9PortLibrary *portLibrary,BOOLEAN isClient,char* serverName);
extern int j9shmem_runTests(struct J9PortLibrary *portLibrary, char* argv0, char* shsem_child);
extern int j9shsem_runTests(struct J9PortLibrary *portLibrary, char* argv0, char* shmem_child);
extern int j9sysinfo_runTests(struct J9PortLibrary *portLibrary, char* argv0);
extern int j9mmap_runTests(struct J9PortLibrary *portLibrary, char* argv0, char* j9mmap_child);
extern int j9vmem_runTests(struct J9PortLibrary *portLibrary); /** @see j9vmemTest.c::j9vmem_runTests */
extern int j9dump_runTests(struct J9PortLibrary *portLibrary); /** @see j9dumpTest.c::j9dump_runTests */
extern int j9sock_runTests(struct J9PortLibrary *portLibrary); /** @see j9sockTest.c::j9sock_runTests */
extern int j9process_runTests(struct J9PortLibrary *portLibrary, char* argv0, char* j9process_child); /** @see j9processTest.c::j9process_runTests */
extern int j9sl_runTests(struct J9PortLibrary *portLibrary); /** @see j9slTest.c::j9sl_runTests */
extern int j9nls_runTests(struct J9PortLibrary *portLibrary); /** @see j9nlsTest.c::j9nls_runTests */
extern int j9shsem_deprecated_runTests(struct J9PortLibrary *portLibrary, char* argv0, char* shmem_child);
extern int j9hypervisor_runTests(struct J9PortLibrary *portLibrary, char *hypervisor, char *hvOptions, BOOLEAN negative);
extern int j9sysinfo_numcpus_runTests(struct J9PortLibrary *portLibrary, UDATA boundTest);
extern int j9vmem_disclaimPerfTests(struct J9PortLibrary *portLibrary, char *disclaimPerfTestArg, BOOLEAN shouldDisclaim); /** @see j9vmemTest.c::j9vmem_runTests */
extern I_32 j9ri_runTests(struct J9PortLibrary *portLibrary); /** @see j9riTest.c::j9ri_runTests */
extern int32_t j9cuda_runTests(struct J9PortLibrary *portLibrary);

static BOOLEAN shouldRunSharedClassesTests(void) {
	BOOLEAN retval = TRUE;
#if (defined(LINUX) && !defined(J9ZTPF)) || defined(J9ZOS390) || defined(AIXPPC)
	if ((uid_t)0 == getuid()) {
		/* pltest has been found to leave files behind that can not be used, or destroyed, by the j9build
		 * user on vmfarm.
		 *
		 * This problem seems to be caused by pressing 'ctrl+c', to escape the test early, while running as
		 *'root'. Specifically files in /tmp/javasharedresources result in failures because they can't be used,
		 * or removed, by 'j9build'.
		 *
		 * To avoid this problem the below tests are now skipped if the current uid is 'root'. This will still allow
		 * most of pltest to be run as root user.
		 */
		retval = FALSE;
	}
#endif /* (defined(LINUX) && !defined(J9ZTPF)) || defined (J9ZOS390) || defined (AIXPPC)*/
	return retval;
}

static int
consumeOption(char **s, char *prefix)
{
	int sLen=(int)strlen(*s);
	int prefixLen=(int)strlen(prefix);
	int i;
	char *option = *s;

	if (sLen < prefixLen) {
		return 0;
	}
	for (i=0; i<prefixLen; i++){
		if (option[i] != prefix[i]) {
			return 0;
		}
	}
	*s += prefixLen;
	if (sLen > prefixLen) {
		if (**s == ',') {
			*s += 1;
		}
	}
	return 1;
}

static UDATA
parseForAreasToTests(struct J9PortLibrary *portLibrary, char *options)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA userParm = 0;
	char *allOptions = options;

	while (strlen(allOptions) > 0) {
		if (consumeOption(&allOptions, "j9error")) {
			userParm |= J9PORT_TEST_J9ERROR;
		} else if (consumeOption(&allOptions, "j9file_blockingasync")) {
			userParm |= J9PORT_TEST_J9FILE_BLOCKINGASYNC;
		} else if (consumeOption(&allOptions, "j9file")) {
			userParm |= J9PORT_TEST_J9FILE;
		} else if (consumeOption(&allOptions, "j9mem")) {
			userParm |= J9PORT_TEST_J9MEM;
		} else if (consumeOption(&allOptions, "j9heap")) {
			userParm |= J9PORT_TEST_J9HEAP;
		} else if (consumeOption(&allOptions, "j9port")) {
			userParm |= J9PORT_TEST_J9PORT;
		} else if (consumeOption(&allOptions, "j9str")) {
			userParm |= J9PORT_TEST_J9STR;
		} else if (consumeOption(&allOptions, "j9time")) {
			userParm |= J9PORT_TEST_J9TIME;
		} else if (consumeOption(&allOptions, "j9tty")) {
			userParm |= J9PORT_TEST_J9TTY;
		} else if (consumeOption(&allOptions, "tty_extended")) {
			userParm |= J9PORT_TEST_J9TTY_EXTENDED;
		} else if (consumeOption(&allOptions, "j9signal")) {
			userParm |= J9PORT_TEST_J9SIGNAL;
		} else if (consumeOption(&allOptions, "j9sig_extended")) {
			userParm |= J9PORT_TEST_J9SIG_EXTENDED;
		} else if (consumeOption(&allOptions, "j9sysinfo")) {
			userParm |= J9PORT_TEST_J9SYSINFO;
		} else if (consumeOption(&allOptions, "j9shsem_deprecated")) {
			userParm |= J9PORT_TEST_J9SHSEM_DEPRECATED;
		} else if (consumeOption(&allOptions, "j9shsem")) {
			userParm |= J9PORT_TEST_J9SHSEM;
		} else if (consumeOption(&allOptions, "j9shmem")) {
			userParm |= J9PORT_TEST_J9SHMEM;
		} else if (consumeOption(&allOptions, "j9mmap")) {
			userParm |= J9PORT_TEST_J9MMAP;
		} else if (consumeOption(&allOptions, "j9vmem")) {
			userParm |= J9PORT_TEST_J9VMEM;
		} else if (consumeOption(&allOptions, "j9dump")) {
			userParm |= J9PORT_TEST_J9DUMP;
		} else if (consumeOption(&allOptions, "j9sock")) {
			userParm |= J9PORT_TEST_J9SOCK;
		} else if (consumeOption(&allOptions, "j9process")) {
			userParm |= J9PORT_TEST_J9PROCESS;
		} else if (consumeOption(&allOptions, "j9sl")) {
			userParm |= J9PORT_TEST_J9SL;
		} else if (consumeOption(&allOptions, "j9nls")) {
			userParm |= J9PORT_TEST_J9NLS;
		}else if (consumeOption(&allOptions, "j9hypervisor")) {
			userParm |= J9PORT_TEST_J9HYPERVISOR;
		} else if (consumeOption(&allOptions, "j9numcpus")) {
			userParm |= J9PORT_TEST_NUMCPUS;
		} else if (consumeOption(&allOptions, "j9ri")) {
			userParm |= J9PORT_TEST_INSTRUMENTATION;
		} else if (consumeOption(&allOptions, "j9cuda")) {
			userParm |= J9PORT_TEST_J9CUDA;
		} else {
			j9tty_printf(PORTLIB, "\n\nWarning: invalid option (%s) ignored\n\n", allOptions);
			break;
		}
	}
	return userParm;
}

UDATA
signalProtectedMain(struct J9PortLibrary *portLibrary, void *arg)
{
	struct j9cmdlineOptions * args = arg;
	int argc = args->argc;
	char **argv = args->argv;
	I_32 rc,i;
	BOOLEAN isClient = 0;
	BOOLEAN isHost = 0;
	BOOLEAN isChild = 0;
	BOOLEAN shsem_child = 0;
	BOOLEAN negative = FALSE;
	char serverName[99]="";
	char testName[99]="";
	char *hypervisor = NULL;
	char *hvOptions = NULL;
	char *boundTestArg = NULL;
	IDATA boundTest = 0;
	UDATA areasToTest = 0;
	PORT_ACCESS_FROM_PORT(args->portLibrary);
	char srand[99]="";
	int randomSeed = 0;
	const char *paths[1] = {"./"};
	char *disclaimPerfTestArg = NULL;
	BOOLEAN shouldDisclaim = FALSE;

	/* set up nls */
	j9nls_set_catalog(paths, 1, "java", "properties");

	/* host, shsem_child and shmem_child preclude any other test running */
	for (i=1; i<argc; i++){
		if (0 == strcmp(argv[i],"-client")) {
			isClient=1;
		} else if (startsWith(argv[i],"-host:")) {
			strcpy(serverName,&argv[i][6]);
		} else if (0 != strstr(argv[i],"-child_")) {
			isChild=1;
			strcpy(testName, &argv[i][7]);
		} else if (startsWith(argv[i],"-include:")) {
			areasToTest = parseForAreasToTests(PORTLIB, &argv[i][9]);
		} else if (startsWith(argv[i],"-exclude:")) {
			areasToTest = J9PORT_TEST_ALL & ~parseForAreasToTests(PORTLIB, &argv[i][9]);
		} else if (startsWith(argv[i],"-srand:")) {
			strcpy(srand,&argv[i][7]);
			randomSeed = atoi(srand);
		} else if (startsWith(argv[i],"-hypervisor:")) {
			hypervisor = &argv[i][12];
		} else if (startsWith(argv[i],"-hvOptions:")) {
			hvOptions = &argv[i][11];
		} else if (strcmp(argv[i], "-negative") == 0) {
			/* Option "-negative" does /not/ take sub-options so we don't do startsWith() but an
			 * exact match. Should this need sub-options, use startsWith instead.
			 */
			negative = TRUE;
		} else if (startsWith(argv[i],BOUNDCPUS)) {
			boundTest = -1;
			boundTestArg = &argv[i][strlen(BOUNDCPUS)];
			boundTest = atoi(boundTestArg);
			if (-1 == boundTest) {
				outputComment(PORTLIB,"Invalid (non-numeric) format for %s value (%s).\n", BOUNDCPUS, boundTestArg);
				boundTest = 0;
			}
		} else if ((startsWith(argv[i],"-disclaimPerfTest:")) || (startsWith(argv[i],"+disclaimPerfTest:"))) {
			disclaimPerfTestArg  = &argv[i][18];
			shouldDisclaim = ('+' == argv[i][0]);
		}
	}

	if(isChild) {
		if(startsWith(testName,"j9shmem")) {
			return j9shmem_runTests(PORTLIB, argv[0], testName);
		} else if(startsWith(testName,"j9shsem_deprecated")) {
			return j9shsem_deprecated_runTests(PORTLIB, argv[0], testName);
		} else if(startsWith(testName,"j9shsem")) {
			return j9shsem_runTests(PORTLIB, argv[0], testName);
#if defined (WIN32) | defined (WIN64)
		} else if(startsWith(testName,"j9file_blockingasync")) {
			return j9file_runTests(PORTLIB, argv[0], testName, TRUE);
#endif
		} else if(startsWith(testName,"j9file")) {
			return j9file_runTests(PORTLIB, argv[0], testName, FALSE);
		} else if(startsWith(testName,"j9mmap")) {
			return j9mmap_runTests(PORTLIB, argv[0], testName);
		} else if(startsWith(testName,"j9process")) {
			return j9process_runTests(PORTLIB, argv[0], testName);
		}
	}

	rc = 0;
	if (serverName[0]) {
		socket_test(PORTLIB,isClient,serverName);
	} else {
		/* If nothing specified, then run all tests */
		if (0 == areasToTest) {
			areasToTest = J9PORT_TEST_ALL;
		}

		if (J9PORT_TEST_J9MEM == (areasToTest & J9PORT_TEST_J9MEM)) {
			rc |= j9mem_runTests(PORTLIB, randomSeed);
		}
		if (J9PORT_TEST_J9HEAP == (areasToTest & J9PORT_TEST_J9HEAP)) {
			rc |= j9heap_runTests(PORTLIB, randomSeed);
		}
		if (J9PORT_TEST_J9TTY ==(areasToTest & J9PORT_TEST_J9TTY)) {
			rc |= j9tty_runTests(PORTLIB);
		}
		if (J9PORT_TEST_J9TTY_EXTENDED ==(areasToTest & J9PORT_TEST_J9TTY_EXTENDED)) {
			rc |= j9tty_runExtendedTests(PORTLIB);
		}
		if (J9PORT_TEST_J9ERROR ==(areasToTest & J9PORT_TEST_J9ERROR)) {
			rc |= j9error_runTests(PORTLIB);
		}
		if (J9PORT_TEST_J9PORT == (areasToTest & J9PORT_TEST_J9PORT)) {
			rc |= j9port_runTests(PORTLIB);
		}
		if (J9PORT_TEST_J9FILE ==(areasToTest & J9PORT_TEST_J9FILE)) {
			rc |= j9file_runTests(PORTLIB, argv[0], NULL, FALSE);
		}
#if defined (WIN32) | defined (WIN64)
		if (J9PORT_TEST_J9FILE_BLOCKINGASYNC == (areasToTest & J9PORT_TEST_J9FILE_BLOCKINGASYNC)) {
			rc |= j9file_runTests(PORTLIB, argv[0], NULL, TRUE);
		}
#endif
		if (J9PORT_TEST_J9STR == (areasToTest & J9PORT_TEST_J9STR)) {
			rc |= j9str_runTests(PORTLIB);
		}
		if (J9PORT_TEST_J9TIME == (areasToTest & J9PORT_TEST_J9TIME)){
			rc |= j9time_runTests(PORTLIB);
		}
		if (J9PORT_TEST_J9SYSINFO ==(areasToTest & J9PORT_TEST_J9SYSINFO)) {
			rc |= j9sysinfo_runTests(PORTLIB, argv[0]);
		}

		/* Shared semaphore and shared memory tests only works with J2SE platforms*/
#if defined(LINUX) | defined (J9ZOS390) | defined (AIXPPC) | defined (WIN32)
		if (TRUE == shouldRunSharedClassesTests()) {
			if (J9PORT_TEST_J9SHMEM == (areasToTest & J9PORT_TEST_J9SHMEM)) {
				rc |= j9shmem_runTests(PORTLIB, argv[0], NULL);
			}
			if (J9PORT_TEST_J9SHSEM == (areasToTest & J9PORT_TEST_J9SHSEM)) {
				rc |= j9shsem_runTests(PORTLIB, argv[0], NULL);
			}
			if (J9PORT_TEST_J9SHSEM_DEPRECATED == (areasToTest & J9PORT_TEST_J9SHSEM_DEPRECATED)) {
				rc |= j9shsem_deprecated_runTests(PORTLIB, argv[0], NULL);
			}
		} else {
			j9tty_printf(PORTLIB,"\nINFO SKIPPING TESTS: The following tests will not be run if the current userid is 'root': \n\t" \
				"j9shmem_runTests(), j9shsem_runTests(), and j9shsem_deprecated_runTests().\n\n");
		}
#endif /* LINUX | J9ZOS390 | AIXPPC | WIN32 */

		if (J9PORT_TEST_J9MMAP ==(areasToTest & J9PORT_TEST_J9MMAP)) {
			rc |= j9mmap_runTests(PORTLIB, argv[0], NULL);
		}
		if (J9PORT_TEST_J9VMEM ==(areasToTest & J9PORT_TEST_J9VMEM)) {
			if (NULL != disclaimPerfTestArg){
				/* disclaimPerfTestArg is the string after the ':' from +/-disclaimPerfTest:<pageSize>,<byteAmount>,<totalMemory>[,<numIterations>]
				 * shouldDisclaim is set from the +/- prefix, numIterations is optional and defaults to 50 */
				rc |= j9vmem_disclaimPerfTests(PORTLIB, disclaimPerfTestArg, shouldDisclaim);
			} else {
				rc |= j9vmem_runTests(PORTLIB);
			}
		}
		if (J9PORT_TEST_J9DUMP == (areasToTest & J9PORT_TEST_J9DUMP)) {
			rc |= j9dump_runTests(PORTLIB);
		}
		if (J9PORT_TEST_J9SOCK == (areasToTest & J9PORT_TEST_J9SOCK)) {
			rc |= j9sock_runTests(PORTLIB);
		}
		if (J9PORT_TEST_J9PROCESS == (areasToTest & J9PORT_TEST_J9PROCESS)) {
			rc |= j9process_runTests(PORTLIB, argv[0], NULL);
		}
		if (J9PORT_TEST_J9SL == (areasToTest & J9PORT_TEST_J9SL)) {
			rc |= j9sl_runTests(PORTLIB);
		}
		if (J9PORT_TEST_J9NLS == (areasToTest & J9PORT_TEST_J9NLS)) {
			rc |= j9nls_runTests(PORTLIB);
		}
		/* j9hypervisor Tests are run only when specified by -include */
		if(J9PORT_TEST_J9HYPERVISOR == (areasToTest & J9PORT_TEST_J9HYPERVISOR)) {
			rc |= j9hypervisor_runTests(PORTLIB, hypervisor, hvOptions, negative);
		}
		if (J9PORT_TEST_NUMCPUS == (areasToTest & J9PORT_TEST_NUMCPUS)) {
			rc |= j9sysinfo_numcpus_runTests(PORTLIB, boundTest);
		}
		if (J9PORT_TEST_INSTRUMENTATION == (areasToTest & J9PORT_TEST_INSTRUMENTATION)) {
			rc |= j9ri_runTests(PORTLIB);
		}
		if (J9PORT_TEST_J9CUDA == (areasToTest & J9PORT_TEST_J9CUDA)) {
			rc |= j9cuda_runTests(PORTLIB);
		}

		if (rc) {
			dumpTestFailuresToConsole(portLibrary);
		} else {
			j9tty_printf(PORTLIB,"\nALL TESTS COMPLETED AND PASSED\n");
		}
	}
	return 0;
}
