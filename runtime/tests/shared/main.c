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

#if defined WIN32
#include <shlobj.h>
#endif
#include "sharedconsts.h"
#include "exelib_api.h"
#include "jni.h"
#include "j9.h"
#include "main.h"
#include <string.h>
#include "ClassDebugDataTests.h"
#include "SCStoreTransactionTests.hpp"
#include "SCStringTransactionTests.hpp"
#include "SCStoreWithBCITests.hpp"
#include "CompositeCacheSizesTests.hpp"
/* BEWARE: We include the shared classes trace defines here just to make the compiler happy
 * Trace probably doesn't work with unit test like this.
 */
#include "j9cfg.h"

#define _UTE_STATIC_
#include "ut_j9shr.h"

#if defined WIN32
#define	J9JAVA_DIR_SEPARATOR "\\"
#else
#define	J9JAVA_DIR_SEPARATOR "/"
#endif

static IDATA setupArguments(struct j9cmdlineOptions* startupOptions,JavaVMInitArgs* vm_args,void **vmOptionsTable, BOOLEAN useXshareclasses, BOOLEAN enablebci);
IDATA testOSCache(J9JavaVM* vm, struct j9cmdlineOptions *arg, const char *cmdline);
IDATA testOSCacheMisc(J9JavaVM *vm, struct j9cmdlineOptions *arg, const char *cmdline);
IDATA testClasspathCache(J9JavaVM* vm);
IDATA testCompositeCache(J9JavaVM* vm);
IDATA testClasspathItem(J9JavaVM* vm);
IDATA testCacheMap(J9JavaVM* vm);
IDATA testCompositeCacheSizes(J9JavaVM* vm);
IDATA testCompiledMethod(J9JavaVM* vm);
#if defined(REMOVED_FOR_NOW)
IDATA testByteDataManager(J9JavaVM* vm);
#endif
IDATA testSharedCacheAPI(J9JavaVM* vm);
IDATA testCorruptCache(J9JavaVM *vm);
IDATA testAttachedData(J9JavaVM *vm);
IDATA testAttachedDataMinMax(J9JavaVM *vm, U_16 attachedDataType);
IDATA testAOTDataMinMax(J9JavaVM *vm);
IDATA testProtectNewROMClassData(J9JavaVM* vm);
IDATA testCacheDirPerm(J9JavaVM *vm);
IDATA testCacheFull(J9JavaVM *vm);
IDATA testProtectSharedCacheData(J9JavaVM *vm);
IDATA testStartupHints(J9JavaVM *vm);

UDATA
buildChildCmdlineOption(int argc, char **argv, const char *options, char * newargv[SHRTEST_MAX_CMD_OPTS]) {
	int i;
	UDATA argsadded = 0;

	if ((argc+1) > SHRTEST_MAX_CMD_OPTS) {
		return 0;
	}

	memset(newargv, 0, SHRTEST_MAX_CMD_OPTS * sizeof(char*));

	for (i=0; i < argc;i++) {
		newargv[i] = argv[i];
		argsadded+=1;
	}
	newargv[i] = (char *)options;
	argsadded+=1;

	return argsadded;
}

static int
startsWith(char *s, char *prefix)
{
	int sLen=(int)strlen(s);
	int prefixLen=(int)strlen(prefix);
	int i;

	if (sLen < prefixLen) {
		return 0;
	}
	for (i=0; i<prefixLen; i++){
		if (s[i] != prefix[i]) {
			return 0;
		}
	}
	return 1; /*might want to make sure s is not NULL or something*/
}
void HEADING(struct J9PortLibrary *portLibrary,char *string){
		PORT_ACCESS_FROM_PORT(portLibrary);
		char* dash = "----------------------------------------";
		j9tty_printf(PORTLIB,"\n%s\n%s\n%s\n\n",dash,string,dash);
}

/* Returns 0 = general failure; JNI_OK = success; -1 = jvm startup failed */
IDATA
createJavaVM(struct j9cmdlineOptions* startupOptions, J9JavaVM** vm_, BOOLEAN useXshareclasses, BOOLEAN enablebci, JNIEnv** env)
{
	char libjvmPath[SHRTEST_MAX_PATH];
	const char * jvmLibName = "jvm";
	void *vmOptionsTable = NULL;
	IDATA rc = 0;
	jint (JNICALL *CreateJavaVM)(JavaVM**, JNIEnv**, JavaVMInitArgs*);

	JavaVMInitArgs vm_args;
	JavaVM *jvm = NULL;
	UDATA handle;

	PORT_ACCESS_FROM_PORT(startupOptions->portLibrary);

	/* now set up the arguments for JNI_CreateJavaVM */
	if (setupArguments(startupOptions,&vm_args,&vmOptionsTable, useXshareclasses, enablebci) != 0){
		j9tty_printf(PORTLIB, "\nCound not create required arguments for JNI_CreateJavaVM...\n");
		rc = FAIL;
		goto cleanup;
	}

	if (cmdline_fetchRedirectorDllDir(startupOptions, libjvmPath) == FALSE) {
		printf("Please provide Java home directory (eg. ./shrtest -Djava.home=/home/[user]/sdk/jre)\n");
		printf("Using compose shape dependent algorithm to figure out libjvm location\n");
		rc = FAIL;
		goto cleanup;
	}

	strcat(libjvmPath, jvmLibName);

	if (j9sl_open_shared_library(libjvmPath, &handle, J9PORT_SLOPEN_DECORATE)) {
		j9tty_printf(PORTLIB, "Failed to open JVM DLL: %s (%s)\n", libjvmPath,
				j9error_last_error_message());
		rc = 1;
		goto cleanup;
	}

	if (j9sl_lookup_name(handle, "JNI_CreateJavaVM", (UDATA*)&CreateJavaVM, "iLLL")) {
		j9tty_printf (PORTLIB, "Failed to find JNI_CreateJavaVM in DLL\n");
		rc = -1;
		goto cleanup;
	}

	if (CreateJavaVM(&jvm, env, &vm_args)) {
		rc = -1;
		goto cleanup;
	}

	rc = JNI_OK;
	*vm_ = (J9JavaVM*)jvm;

	if (FALSE == useXshareclasses) {
		/* register shared classes component with trace engine */
		UT_MODULE_LOADED(J9_UTINTERFACE_FROM_VM((J9JavaVM*)jvm));
	}

  cleanup:
	if (vmOptionsTable) {
		vmOptionsTableDestroy(&vmOptionsTable);
	}

	return rc;
}

UDATA
signalProtectedMain(struct J9PortLibrary *portLibrary, void * vargs)
{
	struct j9cmdlineOptions* args = (struct j9cmdlineOptions*)vargs;
	int argc = args->argc;
	char **argv = args->argv;
	I_32 rc=0;
	I_32 i;
	J9JavaVM* vm;
	JNIEnv* env = NULL;
#if !defined(J9SHR_CACHELET_SUPPORT)
	J9ProcessHandle pid = NULL;
	char * childargv[SHRTEST_MAX_CMD_OPTS];
	UDATA childargc = 0;
#endif

	PORT_ACCESS_FROM_PORT(args->portLibrary);

#if defined(J9VM_OPT_MEMORY_CHECK_SUPPORT)
	/* This should happen before anybody allocates memory!  Otherwise, shutdown will not work properly. */
	memoryCheck_parseCmdLine( PORTLIB, argc-1, argv );
#endif /* J9VM_OPT_MEMORY_CHECK_SUPPORT */

	main_setNLSCatalog(PORTLIB, argv);

	for(i=0;i<argc;i++){
#if !defined(J9SHR_CACHELET_SUPPORT)
		if (startsWith(argv[i],TRANSACTIONTEST_CMDLINE_STARTSWITH)!=0) {
			IDATA procrc = 0;
			if (createJavaVM(args, &vm, TRUE, FALSE, &env) != JNI_OK) {
				j9tty_printf(PORTLIB,"\nCound not create jvm for transaction tests. Exiting unit test...\n");
				return 1;
			}
			HEADING(PORTLIB, "Shared Class Store Transaction Test");
			(*env)->FindClass(env, "Class Does Not Exist");
			(*env)->ExceptionClear(env);

			procrc |= testSCStoreTransaction(vm);

			HEADING(PORTLIB, "Shared Class String Transaction Test");
			procrc |= testSCStringTransaction(vm);

			if ( (*((JavaVM*)vm))->DestroyJavaVM((JavaVM*)vm) != JNI_OK ) {
				args->shutdownPortLib = FALSE;
			}
			return procrc;
		}
		if (startsWith(argv[i],TRANSACTION_WITHBCI_TEST_CMDLINE_STARTSWITH)!=0) {
			IDATA procrc = 0;
			if (createJavaVM(args, &vm, TRUE, TRUE, &env) != JNI_OK) {
				j9tty_printf(PORTLIB,"\nCound not create jvm for transaction tests. Exiting unit test...\n");
				return 1;
			}
			HEADING(PORTLIB, "Shared Class Store With BCI Test");
			(*env)->FindClass(env, "Class Does Not Exist");
			(*env)->ExceptionClear(env);

			procrc |= testSCStoreWithBCITests(vm);

			if ( (*((JavaVM*)vm))->DestroyJavaVM((JavaVM*)vm) != JNI_OK ) {
				args->shutdownPortLib = FALSE;
			}
			return procrc;
		}
#endif

		if(startsWith(argv[i],OSCACHETEST_CMDLINE_STARTSWITH)!=0) {
			IDATA procrc = 1;
			if (createJavaVM(args, &vm, FALSE, FALSE, &env) != JNI_OK) {
				j9tty_printf(PORTLIB,"\nCound not create jvm for testOSCache. Exiting unit test...\n");
				return procrc;
			}

			procrc = testOSCache(vm, args, argv[i]);

			if ( (*((JavaVM*)vm))->DestroyJavaVM((JavaVM*)vm) != JNI_OK ) {
				args->shutdownPortLib = FALSE;
			}
			return procrc;

		}
	}

#if !defined(J9SHR_CACHELET_SUPPORT)
	childargc = buildChildCmdlineOption(argc, argv, TRANSACTIONTEST_CMDLINE_STARTSWITH, childargv);
	pid = LaunchChildProcess (PORTLIB, "testSCStoreTransaction", childargv, childargc);
	if (pid == NULL) {
		j9tty_printf(PORTLIB,"\nLaunchChildProcess has failed for -testSCStoreTransaction\n");
		return 1;
	}
	rc |= WaitForTestProcess(PORTLIB, pid);

	childargc = buildChildCmdlineOption(argc, argv, TRANSACTION_WITHBCI_TEST_CMDLINE_STARTSWITH, childargv);
	pid = LaunchChildProcess (PORTLIB, "testSCStoreWithBCI", childargv, childargc);
	if (pid == NULL) {
		j9tty_printf(PORTLIB,"\nLaunchChildProcess has failed for -testSCStoreWithBCI\n");
		return 1;
	}
	rc |= WaitForTestProcess(PORTLIB, pid);
#endif

	if (createJavaVM(args, &vm, FALSE, FALSE, &env) != JNI_OK) {
		j9tty_printf(PORTLIB,"\nCound not create jvm. Exiting unit test...\n");
		return 1;
	}

	HEADING(PORTLIB, "OSCache Test");
	rc |= testOSCache(vm, args, NULL);

	HEADING(PORTLIB, "ClasspathCache Test");
	rc |= testClasspathCache(vm);

	/* TODO: Temporarily disable due to problems with statics in composite cache on some OS */

	HEADING(PORTLIB, "CompositeCache Test");
	rc |= testCompositeCache(vm);

	HEADING(PORTLIB, "CompositeCacheSizes Test");
	rc |= testCompositeCacheSizes(vm);

	HEADING(PORTLIB, "ClasspathItem Test");
	rc |= testClasspathItem(vm);

	HEADING(PORTLIB, "CacheMap Test");
	rc |= testCacheMap(vm);

	HEADING(PORTLIB, "Compiled Method Test");
	rc |= testCompiledMethod(vm);

	/* TODO: Temporarily disable due to problems with Manager statics and multiple CacheMaps */
#if defined(REMOVED_FOR_NOW)
	HEADING(PORTLIB, "Byte Data Test");
	rc |= testByteDataManager(vm);
#endif /* defined(REMOVED_FOR_NOW) */

	HEADING(PORTLIB, "ClassDebugDataProvider Test");
	rc |= testClassDebugDataTests(vm);

	HEADING(PORTLIB, "SharedCacheAPI Test");
	rc |= testSharedCacheAPI(vm);

	HEADING(PORTLIB, "CorruptCache Test");
	rc |= testCorruptCache(vm);

	/* TODO: Temporarily disable AttachedDataTest and minmax tests on realtime */
#if !defined(J9SHR_CACHELET_SUPPORT)
	HEADING(PORTLIB, "AttachedData Test");
	rc |= testAttachedData(vm);

	HEADING(PORTLIB, "AttachedData JITPROFILE Min/Max Test");
	rc |= testAttachedDataMinMax(vm, J9SHR_ATTACHED_DATA_TYPE_JITPROFILE);

	HEADING(PORTLIB, "AttachedData JITHINT Min/Max Test");
	rc |= testAttachedDataMinMax(vm, J9SHR_ATTACHED_DATA_TYPE_JITHINT);

	HEADING(PORTLIB, "AOT Min/Max Test");
	rc |= testAOTDataMinMax(vm);

    HEADING(PORTLIB, "Cache Full Test");
    rc |= testCacheFull(vm);
#endif

	HEADING(PORTLIB, "Protect New ROM Class Data Test");
	rc |= testProtectNewROMClassData(vm);

	HEADING(PORTLIB, "Protect Shared Cache Data Test");
	rc |= testProtectSharedCacheData(vm);

#if !defined(WIN32)
	HEADING(PORTLIB, "CacheDirPerm Test");
	rc |= testCacheDirPerm(vm);
#endif

	HEADING(PORTLIB, "OSCacheMisc Test");
	rc |= testOSCacheMisc(vm, args, argv[i]);

	HEADING(PORTLIB, "Startup Hints Test");
	rc |= testStartupHints(vm);

	if ( (*((JavaVM*)vm))->DestroyJavaVM((JavaVM*)vm) != JNI_OK ) {
		args->shutdownPortLib = FALSE;
	}

	if (rc) {
		j9tty_printf(PORTLIB,"\nFAILURES DETECTED\n");
	} else {
		j9tty_printf(PORTLIB,"\nALL TESTS COMPLETED AND PASSED\n");
	}

	return rc;
}

IDATA
setupArguments(struct j9cmdlineOptions* startupOptions,JavaVMInitArgs* vm_args,void **vmOptionsTable, BOOLEAN useXshareclasses, BOOLEAN enablebci)
{
	char **argv = startupOptions->argv;
	int argc = startupOptions->argc;
	int counter = 0;
	PORT_ACCESS_FROM_PORT(startupOptions->portLibrary);
	IDATA rc = 0;

	vmOptionsTableInit(PORTLIB, vmOptionsTable, 15);
	if (NULL == *vmOptionsTable)
		goto cleanup;

	/*
	 * -Xint is disabled b/c this test is not intended to test this
	 * -Dcom.ibm.tools.attach.enable=no is set b/c of CMVC 169171
	 * -Xmx64m is passed because realtime uses a big heap
	 */
	if ((vmOptionsTableAddOption(vmOptionsTable, "_port_library", (void *) PORTLIB) != J9CMDLINE_OK)
	|| (vmOptionsTableAddOption(vmOptionsTable, "-Xint", NULL) != J9CMDLINE_OK)
	|| (vmOptionsTableAddOption(vmOptionsTable, "-Dcom.ibm.tools.attach.enable=no", NULL) != J9CMDLINE_OK)
	|| (vmOptionsTableAddOption(vmOptionsTable, "-Xmx64m", NULL) != J9CMDLINE_OK)
	|| (vmOptionsTableAddOption(vmOptionsTable, "-Xlp:objectheap:pagesize=4K,pageable", NULL) != J9CMDLINE_OK)
	|| (vmOptionsTableAddOption(vmOptionsTable, "-Xlp:codecache:pagesize=4K,pageable", NULL) != J9CMDLINE_OK)
	) {
			goto cleanup;
	}

	if (TRUE == useXshareclasses) {
		if (FALSE == enablebci) {
			if (vmOptionsTableAddOption(vmOptionsTable, "-Xshareclasses:nonpersistent,name=testSCTransactions,reset", NULL) != J9CMDLINE_OK) {
				goto cleanup;
			}
		} else {
			if (vmOptionsTableAddOption(vmOptionsTable, "-Xshareclasses:nonpersistent,name=testSCTransactions,enablebci,reset", NULL) != J9CMDLINE_OK) {
				goto cleanup;
			}
		}
		/* The test needs a debug area size of 2MB (using -Xscdmx2m). The default cache size is 300MB on 64-bit platforms on Java 9 and up, which might not be allowed
		 * by the OS due to the shmmax setting. The cache size will be adjusted to shmmax and debug area size will also be adjusted proportionally, resulting in the debug area
		 * size to be much smaller than 2MB. Set cache size to 16MB for this test so that it won't be unstable due to the shmmax setting on the test machines.
		 */
		if (vmOptionsTableAddOption(vmOptionsTable, "-Xscmx16m", NULL) != J9CMDLINE_OK) {
			goto cleanup;
		}
		if (vmOptionsTableAddOption(vmOptionsTable, "-Xscdmx2m", NULL) != J9CMDLINE_OK) {
			goto cleanup;
		}
	}

	if ((useXshareclasses == TRUE) && (vmOptionsTableAddOption(vmOptionsTable, "-Xits1m", NULL) != J9CMDLINE_OK)) {
		goto cleanup;
	}

	if (vmOptionsTableAddExeName(vmOptionsTable, argv[0]) != J9CMDLINE_OK) {
		rc = -1;
		goto cleanup;
	}

	/* add any other command line options */
	for (counter=1;counter<argc;counter++){
		if (startsWith(argv[counter],TRANSACTIONTEST_CMDLINE_STARTSWITH)!=0) {
			/*go to next argument*/
			continue;
		}
		if (startsWith(argv[counter],TRANSACTION_WITHBCI_TEST_CMDLINE_STARTSWITH)!=0) {
			/*go to next argument*/
			continue;
		}
		if (startsWith(argv[counter],OSCACHETEST_CMDLINE_STARTSWITH)!=0) {
			/*go to next argument*/
			continue;
		}
		if (vmOptionsTableAddOption(vmOptionsTable, argv[counter], NULL) != J9CMDLINE_OK) {

			rc = FAIL;
			goto cleanup;
		}
	}

	vm_args->version = JNI_VERSION_1_2;
	vm_args->nOptions = vmOptionsTableGetCount(vmOptionsTable);
	vm_args->options = vmOptionsTableGetOptions(vmOptionsTable);
	vm_args->ignoreUnrecognized = JNI_FALSE;

cleanup:
	return rc;
}
