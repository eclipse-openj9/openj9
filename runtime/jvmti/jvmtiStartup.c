/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "jvmtinls.h"
#include "j9port.h"
#include "omrthread.h"
#include "j9protos.h"
#include "vmaccess.h"
#include "jvminit.h"
#include "vmhook.h"
#include "jvmti_internal.h"
#include "j2sever.h"
#include "zip_api.h"

#if defined(AIXPPC)
#include <errno.h>
#include <sys/ldr.h>
#endif /* defined(AIXPPC) */

#define _UTE_STATIC_
#include "ut_j9jvmti.h"

#include "jvmtiHelpers.h"

#define THIS_DLL_NAME J9_JVMTI_DLL_NAME

typedef enum CreateAgentOption {
	OPTION_AGENTLIB,
	OPTION_AGENTPATH,
	OPTION_XRUNJDWP
} CreateAgentOption;

static void shutDownJVMTI(J9JavaVM *vm);
static jint createAgentLibrary(J9JavaVM *vm, const char *libraryName, UDATA libraryNameLength, const char *options, UDATA optionsLength, UDATA decorate, J9JVMTIAgentLibrary **result);
static jint initializeJVMTI(J9JavaVM *vm);
static void shutDownAgentLibraries(J9JavaVM *vm, UDATA closeLibrary);
static jint loadAgentLibrary(J9JavaVM *vm, J9JVMTIAgentLibrary *agentLibrary);
static jint createXrunLibraries(J9JavaVM *vm);
static J9JVMTIAgentLibrary* findAgentLibrary(J9JavaVM *vm, const char *libraryAndOptions, UDATA libraryLength);
static jint issueAgentOnLoadAttach(J9JavaVM *vm, J9JVMTIAgentLibrary *agentLibrary, const char *options, char *loadFunctionName, BOOLEAN *foundLoadFn);
I_32 JNICALL loadAgentLibraryOnAttach(struct J9JavaVM *vm, const char *library, const char *options, UDATA decorate);
static BOOLEAN isAgentLibraryLoaded(J9JavaVM *vm, const char *library, BOOLEAN decorate);
static jint createAgentLibraryWithOption(J9JavaVM *vm, J9VMInitArgs *argsList, IDATA agentIndex, J9JVMTIAgentLibrary **agentLibrary, CreateAgentOption createAgentOption, BOOLEAN *isJDWPagent);
static BOOLEAN processAgentLibraryFromArgsList(J9JavaVM *vm, J9VMInitArgs *argsList, BOOLEAN loadLibrary, CreateAgentOption createAgentOption);

#define INSTRUMENT_LIBRARY "instrument"

#define J9JVMTI_AGENT_ONATTACH 	"Agent_OnAttach"
#define J9JVMTI_AGENT_ONLOAD 	"Agent_OnLoad"
#define J9JVMTI_AGENT_ONUNLOAD  "Agent_OnUnload"
#define J9JVMTI_BUFFER_LENGTH   256

/* List of system agents expected to be found in jre/bin or jre/lib/$arch depending on the
 * platform and drop shape */


static const char * systemAgentNames[] =
{
	"jdwp",
	"hprof",
	INSTRUMENT_LIBRARY,
	"hyinstrument",
	"healthcenter",
	"dgcollector",
	NULL
};


jint JNICALL JVM_OnLoad(JavaVM *jvm, char* options, void *reserved)
{
	return JNI_OK;
}

/**
 * parseLibraryAndOptions
 * @param parseLibraryAndOptions <library name> or <library name>=<options>, null terminated
 * @param libraryLength library name length, return by reference
 * @param options start of option string, returned by reference
 * @param optionsLength option string length, returned by reference
 */
static void
parseLibraryAndOptions(char *libraryAndOptions, UDATA *libraryLength, char **options, UDATA *optionsLength)
{
	char *optTemp;

	optTemp = strstr(libraryAndOptions, "=");
	if (optTemp == NULL) {
		*optionsLength = 0;
		*libraryLength = strlen(libraryAndOptions);
	} else {
		*libraryLength = optTemp-libraryAndOptions;
		++optTemp; /* move past the '=' */
		*optionsLength = strlen(optTemp);
	}
	*options = optTemp;
}

#define OPTIONSBUFF_LEN 512

/**
 * Create an agent library from an option index.
 *
 * @param[in] vm Java VM
 * @param[in] argsList a J9VMInitArgs
 * @param[in] agentIndex the option index for the agent library
 * @param[in/out] agentLibrary environment for the agent
 * @param[in] createAgentOption the agent option is one of CreateAgentOption
 * @param[in/out] isJDWPagent indicate if DebugOnRestore is enabled and the agent is JDWP
 *
 * @return JNI_OK if succeeded, otherwise JNI_ERR/JNI_ENOMEM
 */
static jint
createAgentLibraryWithOption(
	J9JavaVM *vm, J9VMInitArgs *argsList, IDATA agentIndex, J9JVMTIAgentLibrary **agentLibrary,
	CreateAgentOption createAgentOption, BOOLEAN *isJDWPagent)
{
	jint result = JNI_OK;
	char optionsBuf[OPTIONSBUFF_LEN];
	char *optionsPtr = (char*)optionsBuf;
	UDATA buflen = OPTIONSBUFF_LEN;
	IDATA option_rc = 0;
	PORT_ACCESS_FROM_JAVAVM(vm);

	do {
		option_rc = COPY_OPTION_VALUE_ARGS(argsList, agentIndex, ':', &optionsPtr, buflen);
		if (OPTION_BUFFER_OVERFLOW == option_rc) {
			if (optionsPtr != (char*)optionsBuf) {
				j9mem_free_memory(optionsPtr);
			}
			buflen *= 2;
			optionsPtr = (char*)j9mem_allocate_memory(buflen, OMRMEM_CATEGORY_VM);
			if (NULL == optionsPtr) {
				Trc_JVMTI_createAgentLibraryWithOption_OOM();
				result = JNI_ENOMEM;
				break;
			}
		}
	} while (OPTION_BUFFER_OVERFLOW == option_rc);
	if (JNI_OK == result) {
		BOOLEAN decorate = OPTION_AGENTPATH != createAgentOption;
		UDATA libraryLength = 0;
#define JDWP_AGENT "jdwp"
		if (OPTION_XRUNJDWP == createAgentOption) {
			UDATA optionsLengthTmp = strlen(optionsPtr);
			result = createAgentLibrary(vm, JDWP_AGENT, LITERAL_STRLEN(JDWP_AGENT), optionsPtr, optionsLengthTmp, TRUE, agentLibrary);
			Trc_JVMTI_createAgentLibraryWithOption_Xrunjdwp_result(optionsPtr, optionsLengthTmp, *agentLibrary, result);
		} else {
			char *options = NULL;
			UDATA optionsLength = 0;

			parseLibraryAndOptions(optionsPtr, &libraryLength, &options, &optionsLength);
			result = createAgentLibrary(vm, optionsPtr, libraryLength, options, optionsLength, decorate, agentLibrary);
			Trc_JVMTI_createAgentLibraryWithOption_agentlib_result(optionsPtr, libraryLength, options, optionsLength, *agentLibrary, result);
		}
#if defined(J9VM_OPT_CRIU_SUPPORT)
		if ((JNI_OK == result) && vm->internalVMFunctions->isDebugOnRestoreEnabled(vm)) {
			/* 1. If createAgentOption is OPTION_XRUNJDWP, the agent option is MAPOPT_XRUNJDWP which implies a JDWP agent.
			 * 2. If createAgentOption is OPTION_AGENTLIB, decorate is TRUE, the agent option is VMOPT_AGENTLIB_COLON,
			 *    just compare the platform independent library name.
			 * 3. If createAgentOption is OPTION_AGENTPATH, decorate is FALSE, the agent option is VMOPT_AGENTPATH_COLON,
			 *    compare the actual platform-specific library name with libjdwp.so.
			 *    CRIU only supports Linux OS flavours.
			 */
#if defined(LINUX)
#define LIB_JDWP "lib" JDWP_AGENT ".so"
#else /* defined(LINUX) */
#error "CRIU is only supported on Linux"
#endif /* defined(LINUX) */
			if ((OPTION_XRUNJDWP == createAgentOption)
				|| (decorate && (0 == strncmp(JDWP_AGENT, optionsPtr, libraryLength)))
				|| (!decorate
					&& (libraryLength >= LITERAL_STRLEN(LIB_JDWP))
					&& (0 == strncmp(LIB_JDWP, optionsPtr + libraryLength - LITERAL_STRLEN(LIB_JDWP), LITERAL_STRLEN(LIB_JDWP))))
			) {
				*isJDWPagent = TRUE;
			}
#undef LIB_JDWP
		}
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
#undef JDWP_AGENT

		if (optionsPtr != (char*)optionsBuf) {
			j9mem_free_memory(optionsPtr);
		}
	}

	return result;
}

/**
 * Create an agent library from an option index.
 * Three agent options are expected: VMOPT_AGENTLIB_COLON, VMOPT_AGENTPATH_COLON or MAPOPT_XRUNJDWP.
 *
 * @param[in] vm Java VM
 * @param[in] argsList a J9VMInitArgs
 * @param[in] loadLibrary indicate if the agent library created to be loaded or not
 * @param[in] createAgentOption the agent option is one of CreateAgentOption
 *
 * @return TRUE if succeeded, otherwise FALSE
 */
static BOOLEAN
processAgentLibraryFromArgsList(J9JavaVM *vm, J9VMInitArgs *argsList, BOOLEAN loadLibrary, CreateAgentOption createAgentOption)
{
	IDATA agentIndex = 0;
	BOOLEAN result = TRUE;
	const char *agentColon = NULL;

	if (OPTION_AGENTLIB == createAgentOption) {
		agentColon = VMOPT_AGENTLIB_COLON;
	} else if (OPTION_AGENTPATH == createAgentOption) {
		agentColon = VMOPT_AGENTPATH_COLON;
	} else {
		/* only three options are expected */
		agentColon = MAPOPT_XRUNJDWP;
	}
	agentIndex = FIND_AND_CONSUME_ARG_FORWARD(argsList, STARTSWITH_MATCH, agentColon, NULL);
	while (agentIndex >= 0) {
		J9JVMTIAgentLibrary *agentLibrary = NULL;
		BOOLEAN isJDWPagent = FALSE;
		if (JNI_OK != createAgentLibraryWithOption(vm, argsList, agentIndex, &agentLibrary, createAgentOption, &isJDWPagent)) {
			result = FALSE;
			break;
		}
#if defined(J9VM_OPT_CRIU_SUPPORT)
		if (isJDWPagent) {
			vm->checkpointState.flags |= J9VM_CRIU_IS_JDWP_ENABLED;
		}
		if (loadLibrary) {
			if (JNI_OK != loadAgentLibrary(vm, agentLibrary)) {
				result = FALSE;
				break;
			}
		}
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
		if (OPTION_XRUNJDWP == createAgentOption) {
			/* no need to search more -Xrunjdwp: options */
			break;
		}

		agentIndex = FIND_NEXT_ARG_IN_ARGS_FORWARD(argsList, STARTSWITH_MATCH, agentColon, NULL, agentIndex);
	}

	return result;
}

#if defined(J9VM_OPT_CRIU_SUPPORT)
/**
 * Add JVMTI capabilities before checkpoint.
 * This is required for debugger support when JIT is enabled.
 * This function can be removed when JIT allows capabilities to be added after restore.
 *
 * @param[in] vm Java VM
 * @param[in] jitEnabled FALSE if -Xint, otherwise TRUE
 *
 * @return JNI_OK if succeeded, otherwise JNI_ERR
 */
static jint
criuAddCapabilities(J9JavaVM *vm, BOOLEAN jitEnabled) {
	jvmtiError jvmtiRet = JVMTI_ERROR_NONE;
	JavaVM *javaVM = (JavaVM*)vm;
	jvmtiCapabilities *requiredCapabilities = &vm->checkpointState.requiredCapabilities;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jvmtiEnv *jvmti_env = NULL;
	jint rc = vmFuncs->GetEnv(javaVM, (void **)&jvmti_env, JVMTI_VERSION_1_1);
	if (JNI_OK != rc) {
		if ((JNI_EVERSION != rc) || (JNI_OK != (vmFuncs->GetEnv(javaVM, (void **)&jvmti_env, JVMTI_VERSION_1_0)))) {
			return JNI_ERR;
		}
	}

	memset(requiredCapabilities,0,sizeof(jvmtiCapabilities));
	requiredCapabilities->can_access_local_variables = 1;
	if (jitEnabled) {
		jvmtiCapabilities potentialCapabilities;

		requiredCapabilities->can_tag_objects = 1;
		requiredCapabilities->can_get_source_file_name = 1;
		requiredCapabilities->can_get_line_numbers = 1;
		requiredCapabilities->can_get_source_debug_extension = 1;
		requiredCapabilities->can_maintain_original_method_order = 1;
		requiredCapabilities->can_generate_single_step_events = 1;
		requiredCapabilities->can_generate_exception_events = 1;
		requiredCapabilities->can_generate_frame_pop_events = 1;
		requiredCapabilities->can_generate_breakpoint_events = 1;
		requiredCapabilities->can_generate_method_entry_events = 1;
		requiredCapabilities->can_generate_method_exit_events = 1;
		requiredCapabilities->can_generate_monitor_events = 1;
		requiredCapabilities->can_generate_garbage_collection_events = 1;
#if JAVA_SPEC_VERSION >= 21
		requiredCapabilities->can_support_virtual_threads = 1;
#endif /* JAVA_SPEC_VERSION >= 21 */
		memset(&potentialCapabilities, 0, sizeof(potentialCapabilities));
		jvmtiRet = (*jvmti_env)->GetPotentialCapabilities(jvmti_env, &potentialCapabilities);
		if (JVMTI_ERROR_NONE != jvmtiRet) {
			return JNI_ERR;
		}
		requiredCapabilities->can_generate_field_modification_events = potentialCapabilities.can_generate_field_modification_events;
		requiredCapabilities->can_generate_field_access_events = potentialCapabilities.can_generate_field_access_events;
		requiredCapabilities->can_pop_frame = potentialCapabilities.can_pop_frame;
	}
	jvmtiRet = (*jvmti_env)->AddCapabilities(jvmti_env, requiredCapabilities);
	if (JVMTI_ERROR_NONE != jvmtiRet) {
		return JNI_ERR;
	}

	vm->checkpointState.jvmtienv = jvmti_env;
	return JNI_OK;
}

void
criuRestoreInitializeLib(J9JavaVM *vm, J9JVMTIEnv *j9env)
{
	J9VMInitArgs *criuRestoreArgsList = vm->checkpointState.restoreArgsList;

	processAgentLibraryFromArgsList(vm, criuRestoreArgsList, FALSE, OPTION_AGENTLIB);
	processAgentLibraryFromArgsList(vm, criuRestoreArgsList, FALSE, OPTION_AGENTPATH);
	processAgentLibraryFromArgsList(vm, criuRestoreArgsList, FALSE, OPTION_XRUNJDWP);

	if (J9_ARE_NO_BITS_SET(vm->checkpointState.flags, J9VM_CRIU_IS_JDWP_ENABLED)) {
		J9JVMTIData * jvmtiData = vm->jvmtiData;
		if (NULL != jvmtiData) {
			criuDisableHooks(jvmtiData, j9env);
		}
	}
}

void
criuRestoreStartAgent(J9JavaVM *vm)
{
	J9VMInitArgs *criuRestoreArgsList = vm->checkpointState.restoreArgsList;

	processAgentLibraryFromArgsList(vm, criuRestoreArgsList, TRUE, OPTION_AGENTLIB);
	processAgentLibraryFromArgsList(vm, criuRestoreArgsList, TRUE, OPTION_AGENTPATH);
	processAgentLibraryFromArgsList(vm, criuRestoreArgsList, TRUE, OPTION_XRUNJDWP);
}
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

IDATA J9VMDllMain(J9JavaVM *vm, IDATA stage, void *reserved)
{
	IDATA returnVal = J9VMDLLMAIN_OK;

	switch(stage) {
		case ALL_VM_ARGS_CONSUMED:
		{
			if (JNI_OK != initializeJVMTI(vm)) {
				goto _error;
			}
			if (FALSE == processAgentLibraryFromArgsList(vm, vm->vmArgsArray, FALSE, OPTION_AGENTLIB)) {
				goto _error;
			}
			if (FALSE == processAgentLibraryFromArgsList(vm, vm->vmArgsArray, FALSE, OPTION_AGENTPATH)) {
				goto _error;
			}
			/* -Xrun libraries that have an Agent_OnLoad are treated like -agentlib: */
			if (JNI_OK != createXrunLibraries(vm)) {
				goto _error;
			}
			vm->loadAgentLibraryOnAttach = &loadAgentLibraryOnAttach;
			vm->isAgentLibraryLoaded = &isAgentLibraryLoaded;
			break;
		}

		case JIT_INITIALIZED:
			/* Register this module with trace */
			UT_MODULE_LOADED(J9_UTINTERFACE_FROM_VM(vm));
			Trc_JVMTI_VMInitStages_Event1(vm->mainThread);
			break;

		case ALL_DEFAULT_LIBRARIES_LOADED:
			if (0 != initZipLibrary(vm->portLibrary, vm->j2seRootDirectory)) {
				goto _error;
			}
			break;

		case AGENTS_STARTED:
		{
			pool_state poolState;
			J9JVMTIAgentLibrary *agentLibrary = NULL;
			J9JVMTIData *jvmtiData = J9JVMTI_DATA_FROM_VM(vm);
			if (hookGlobalEvents(jvmtiData)) {
				PORT_ACCESS_FROM_JAVAVM(vm);
				j9tty_err_printf(PORTLIB, "Need NLS message here\n");
				goto _error;
			}

			agentLibrary = pool_startDo(jvmtiData->agentLibraries, &poolState);
			while (NULL != agentLibrary) {
				if (JNI_OK != loadAgentLibrary(vm, agentLibrary)) {
					goto _error;
				}
				agentLibrary = pool_nextDo(&poolState);
			}

			/* Register the hotswap helper trace points */
			hshelpUTRegister(vm);

#if defined(J9VM_OPT_CRIU_SUPPORT)
			/*
			 * Adding capabilities is required before checkpoint if JIT is enabled.
			 * Following code can be removed when JIT allows capabilities to be added after restore.
			 */
			if (vm->internalVMFunctions->isDebugOnRestoreEnabled(vm)) {
				Trc_JVMTI_criuAddCapabilities_invoked();
				/* ignore the failure, it won't cause a problem if JDWP is not enabled later */
				criuAddCapabilities(vm, NULL != vm->jitConfig);
			}
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

			jvmtiData->phase = JVMTI_PHASE_PRIMORDIAL;
			break;
		}

		case LIBRARIES_ONUNLOAD:
			shutDownJVMTI(vm);
			break;

		case JVM_EXIT_STAGE:
			shutDownAgentLibraries(vm, FALSE);
			break;

		_error :
			shutDownJVMTI(vm);
			returnVal = J9VMDLLMAIN_FAILED;
			break;
	}
	return returnVal;
}

/**
 * \brief	Mangle the agent library name to include full path
 * \ingroup jvmti
 *
 * @param[in] vm			J9JavaVM structure
 * @param[in] dllName		Agent library name
 * @return full agent path
 *
 *	Returns a fully qualified, platform specific, pathname of the passed in agent
 *  library. Essentially prepends /full/path/jre/lib/ARCH to the agent library name
 *
 *  NOTE: Caller is responsible for freeing the returned buffer
 */
static char *
prependSystemAgentPath(J9JavaVM *vm, const char *dllName)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	char *localBuffer = NULL;

	if(vm->j2seRootDirectory) {
		UDATA superSeparatorIndex = -1;
		UDATA bufferSize;

		if(J2SE_LAYOUT(vm) & J2SE_LAYOUT_VM_IN_SUBDIR) {
			/* load the DLL from the parent of the j2seRootDir - find the last dir separator and declare that the end. */
			superSeparatorIndex = strrchr(vm->j2seRootDirectory, DIR_SEPARATOR) - vm->j2seRootDirectory;
			bufferSize = superSeparatorIndex + 1 + sizeof(DIR_SEPARATOR_STR) + strlen(dllName);  /* +1 for NUL */
		} else {
			bufferSize = strlen(vm->j2seRootDirectory) + 1 + strlen(DIR_SEPARATOR_STR) + strlen(dllName);  /* +1 for NUL */
		}
		localBuffer = j9mem_allocate_memory(bufferSize, J9MEM_CATEGORY_JVMTI);
		if(!localBuffer) {
			return NULL;
		}

		if(superSeparatorIndex != -1) {  /* will be set if we need to clip right after parent dir - do NOT strcpy j2seRootDirectory as it might be longer than the DLL name, resulting in a buffer overflow */
			memcpy(localBuffer, vm->j2seRootDirectory, superSeparatorIndex + 1);
			localBuffer[superSeparatorIndex+1] = (char) 0;
		} else {
			localBuffer[0] = '\0';
			strcpy(localBuffer, vm->j2seRootDirectory);
			strcat(localBuffer, DIR_SEPARATOR_STR);
		}
		strcat(localBuffer, dllName);
	} else {
		localBuffer = j9mem_allocate_memory(strlen(dllName) + 1, J9MEM_CATEGORY_JVMTI);
		if(!localBuffer) {
			return NULL;
		}
		localBuffer[0] = '\0';
		strcat(localBuffer, dllName);
	}

	return localBuffer;
}


static jint
issueAgentOnLoadAttach(J9JavaVM * vm, J9JVMTIAgentLibrary * agentLibrary, const char* options, char *loadFunctionName, BOOLEAN * foundLoadFn)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	jint (JNICALL * agentInitFunction)(J9InvocationJavaVM *, const char *, void *);
	jint rc = JNI_ERR;
	J9InvocationJavaVM * invocationJavaVM = NULL;
	const char* jarPath = options;

	Trc_JVMTI_issueAgentOnLoadAttach_Entry(agentLibrary->nativeLib.name);

	if (j9sl_lookup_name(agentLibrary->nativeLib.handle, loadFunctionName, (void *) &agentInitFunction, "ILLL") != 0) {
		/* With JEP-178 specification change (for JVMTI native agents), we can't treat
		 * errors with invoking and failure in finding an exported agent OnAttach function
		 * alike.  Indicate absence of a life-cycle function if lookup failed.
		 */
		Trc_JVMTI_issueAgentOnLoadAttach_failedLocatingLoadFunction(loadFunctionName);
		*foundLoadFn = FALSE;
		goto closeLibrary;
	}
	*foundLoadFn = TRUE;

	Trc_JVMTI_issueAgentOnLoadAttach_invokingLoadFunction(loadFunctionName, agentInitFunction, options);

	/* Jazz 99339: create J9InvocationJavaVM for each JVMTI agent so as to pass
	 * J9NativeLibrary into the JVMTI event callbacks.
	 * Note: an agentLibrary can be loaded more than once (the loadCount is incremented).
	 * So we need to check if there is already an invocationJavaVM created before allocating a new one */
	if (J9_ARE_ALL_BITS_SET(vm->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_RESTRICT_IFA)) {
		invocationJavaVM = agentLibrary->invocationJavaVM;
		if (NULL == invocationJavaVM) {
			invocationJavaVM = (J9InvocationJavaVM *)j9mem_allocate_memory(sizeof(J9InvocationJavaVM), OMRMEM_CATEGORY_VM);
			if (NULL == invocationJavaVM) {
				j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_JVMTI_OUT_OF_MEMORY, "J9InvocationJavaVM");
				rc = JNI_ENOMEM;
				goto closeLibrary;
			}
			memcpy(invocationJavaVM, vm, sizeof(invocationJavaVM->functions));
			invocationJavaVM->j9vm = vm;
			invocationJavaVM->reserved1_identifier = NULL;
			invocationJavaVM->reserved2_library = &(agentLibrary->nativeLib);

			agentLibrary->invocationJavaVM = invocationJavaVM;
		}
	} else {
		invocationJavaVM = (J9InvocationJavaVM *)vm;
	}

#if defined(WIN32)
	{
		/* agentInitFunction invokes java.instrument/share/native/libinstrument/InvocationAdapter.c
		 * which expects jarPath in system default code page encoding.
		 */
		IDATA optionLen = strlen(options);
		if (optionLen > 0) {
			char *tempJarPath = NULL;
			BOOLEAN conversionSucceed = FALSE;
			int32_t size = j9str_convert(J9STR_CODE_MUTF8, J9STR_CODE_WINDEFAULTACP, options, optionLen, NULL, 0);
			if (size > 0) {
				size += 1; /* leave room for null */
				tempJarPath = j9mem_allocate_memory(size, OMRMEM_CATEGORY_VM);
				if (NULL != tempJarPath) {
					size = j9str_convert(J9STR_CODE_MUTF8, J9STR_CODE_WINDEFAULTACP, options, optionLen, tempJarPath, size);
					if (size > 0) {
						conversionSucceed = TRUE;
					}
				} else {
					j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_JVMTI_OUT_OF_MEMORY, "j9str_convert");
					rc = JNI_ENOMEM;
				}
			}
			if (conversionSucceed) {
				jarPath = tempJarPath;
			} else {
				Trc_JVMTI_issueAgentOnLoadAttach_strConvertFailed(options);
				goto closeLibrary;
			}
		}
	}
#endif /* defined(WIN32) */
	rc = agentInitFunction(invocationJavaVM, jarPath, NULL);
	if (JNI_OK != rc) {
		/* If the load function returned failure (non-zero), we still close the library, but retain emitting
		 * the error message (moving this up the chain is NOT required as the caller loadAgentLibrary() will
		 * no longer attempt at looking this up, having "found" the function already)!
		 */
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_JVMTI_AGENT_INITIALIZATION_FAILED, loadFunctionName, agentLibrary->nativeLib.name, rc);
		Trc_JVMTI_issueAgentOnLoadAttach_loadFunctionFailed(loadFunctionName, rc);

		/* Close the library now to prevent Agent_OnUnload being called during shutdown */
closeLibrary:
		if (0 == agentLibrary->loadCount) {
			if (NULL == agentLibrary->xRunLibrary) {
				Trc_JVMTI_issueAgentOnLoadAttach_close_shared_library(agentLibrary->nativeLib.name);
				j9sl_close_shared_library(agentLibrary->nativeLib.handle);
			}
			agentLibrary->nativeLib.handle = 0;
		}
		Trc_JVMTI_issueAgentOnLoadAttach_Exit(agentLibrary->nativeLib.name, rc);

		if ((NULL != invocationJavaVM) && (invocationJavaVM != (J9InvocationJavaVM *)vm)) {
			j9mem_free_memory(invocationJavaVM);
			agentLibrary->invocationJavaVM = NULL;
		}
	} else {
		Trc_JVMTI_issueAgentOnLoadAttach_loadFunctionSucceeded(loadFunctionName);
		Trc_JVMTI_issueAgentOnLoadAttach_Exit(agentLibrary->nativeLib.name, rc);
	}

#if defined(WIN32)
	if (options != jarPath) {
		/* jarPath is tempJarPath allocated earlier and can be freed. */
		j9mem_free_memory((char*)jarPath);
	}
#endif /* defined(WIN32) */

	return rc;
}

/**
 * Load a JVMTI agent and call the agent's initialization function.
 *
 * @param[in] vm Java VM
 * @param[in] agentLibrary environment for the agent
 * @param[in] loadFunctionName name of the initialization function
 * @param[in] loadStatically a boolean indicating if the library is to be loaded statically
 * @param[in/out] found a pointer to a boolean indicating whether the load function was found
 * @param[in/out] errorMessage a pointer to the error message when opening the shared library
 *
 * @return JNI_ERR if failed, otherwise JNI_OK
 */
static jint
loadAgentLibraryGeneric(J9JavaVM *vm, J9JVMTIAgentLibrary *agentLibrary, char *loadFunctionName, BOOLEAN loadStatically, BOOLEAN *found, const char **errorMessage)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	jint rc = JNI_OK;
	J9JVMTIData *jvmtiData = J9JVMTI_DATA_FROM_VM(vm);
	J9NativeLibrary *nativeLib = &(agentLibrary->nativeLib);

	Trc_JVMTI_loadAgentLibraryGeneric_Entry(agentLibrary->nativeLib.name);
	if (NULL == agentLibrary->xRunLibrary) {
		jint i = 0;
		char *fullLibName = NULL;
		const char *systemAgentName = NULL;
		UDATA openFlags = agentLibrary->decorate ? J9PORT_SLOPEN_DECORATE | J9PORT_SLOPEN_LAZY : J9PORT_SLOPEN_LAZY;
		char *agentPath = NULL; /* Don't free agentPath; may point at persistent, system areas. */

		if (loadStatically) {
			/* If flag is set, the agent library ought to be opened statically, that is, the executable itself. */
			if (0 == j9sysinfo_get_executable_name(NULL, &agentPath)) {
				openFlags |= J9PORT_SLOPEN_OPEN_EXECUTABLE;
			} else {
				/* Report failure to obtain executable name; caller will proceed with dynamic linking. */
				I_32 errorno = j9error_last_error_number();
				Trc_JVMTI_loadAgentLibraryGeneric_execNameNotFound_Exit(errorno, agentLibrary->nativeLib.name);
				return JNI_ERR;
			}
		} else {
			/* If the user did not specify an explicit agent path, then ensure that the library
			 * is loaded from our current jre tree. This avoids picking up stray agents that might
			 * be found on the library path. See CMVC 144382.
			 */
			while (NULL != (systemAgentName = systemAgentNames[i++])) {
				if (0 == strcmp(nativeLib->name, systemAgentName)) {
					fullLibName = prependSystemAgentPath(vm, nativeLib->name);
					Trc_JVMTI_loadAgentLibraryGeneric_loadingAgentAs(fullLibName);
					break;
				}
			}
			agentPath = (NULL != fullLibName) ? fullLibName : nativeLib->name;
		}

		if (0 != j9sl_open_shared_library(agentPath, &(nativeLib->handle), openFlags)) {
			/* We may attempt to open the shared library again so save the error message
			 * and print it once we know it is the final attempt */
			*errorMessage = j9error_last_error_message();

			if (NULL != fullLibName) {
				j9mem_free_memory(fullLibName);
			}
			Trc_JVMTI_loadAgentLibraryGeneric_failedOpeningAgentLibrary_Exit(agentPath, *errorMessage);
			return JNI_ERR;
		}
		Trc_JVMTI_loadAgentLibraryGeneric_openedAgentLibrary(agentPath,
															 nativeLib->handle,
															 loadStatically ? "[statically]" : "[dynamically]");
		if (NULL != fullLibName) {
			j9mem_free_memory(fullLibName);
		}
	} else {
		nativeLib->handle = agentLibrary->xRunLibrary->descriptor;
	}

	rc = issueAgentOnLoadAttach(vm, agentLibrary, agentLibrary->options, loadFunctionName, found);
	if (!(*found)) {
		/* If the load function wasn't found, issueAgentOnLoadAttach already closed the library. */
		Trc_JVMTI_loadAgentLibraryGeneric_agentAttachFailed1_Exit(agentLibrary->nativeLib.name, loadFunctionName);
		return rc;
	}

	/* For errors other than load function not found ... */
	if (JNI_OK != rc) {
		Trc_JVMTI_loadAgentLibraryGeneric_agentAttachFailed2_Exit(agentLibrary->nativeLib.name, loadFunctionName, rc);
		return rc;
	}

	Trc_JVMTI_loadAgentLibraryGeneric_agentAttachedSuccessfully(agentLibrary->nativeLib.name);

#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	agentLibrary->nativeLib.doSwitching = validateLibrary(vm, agentLibrary->nativeLib.name, agentLibrary->nativeLib.handle, JNI_FALSE);
#endif

	/* Add the library to the linked list */
	issueWriteBarrier();
	omrthread_monitor_enter(jvmtiData->mutex);
	if (NULL == jvmtiData->agentLibrariesTail) {
		jvmtiData->agentLibrariesHead = jvmtiData->agentLibrariesTail = nativeLib;
	} else {
		jvmtiData->agentLibrariesTail->next = nativeLib;
		jvmtiData->agentLibrariesTail = nativeLib;
	}
	omrthread_monitor_exit(jvmtiData->mutex);
	Trc_JVMTI_loadAgentLibraryGeneric_succeed_Exit(agentLibrary->nativeLib.name, loadFunctionName);

	return JNI_OK;
}

/**
 * Load a JVMTI agent at run time and call the Agent_OnAttach_L/Agent_OnAttach function
 * @param vm Java VM
 * @param library library name.  Must be non-null.
 * @param options options, if any.  May be null.
 * @return JNI_ERR, JNI_OK
 */
I_32 JNICALL
loadAgentLibraryOnAttach(struct J9JavaVM * vm, const char * library, const char *options, UDATA decorate)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	UDATA rc = JNI_OK;
	UDATA optionsLength = 0;
	UDATA libraryLength = 0;
	J9JVMTIAgentLibrary *agentLibrary = NULL;
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(vm);
	char loadFunctionName[J9JVMTI_BUFFER_LENGTH + 1] = {0};
	UDATA loadFunctionNameLength = 0;
	BOOLEAN found = FALSE;
	const char *errorMessage = NULL;

	Trc_JVMTI_loadAgentLibraryOnAttach_Entry(library);

	if (J9_ARE_NO_BITS_SET(vm->runtimeFlags, J9_RUNTIME_ALLOW_DYNAMIC_AGENT)) {
		Trc_JVMTI_loadAgentLibraryOnAttach_agentLoadingDisabled();
		rc = JNI_ERR;
		goto exit;
	}

	Assert_JVMTI_true(NULL != library); /* Library name must be non-null. */

	if (NULL != options) {
		optionsLength = strlen(options);
	}

	libraryLength = strlen(library);

	omrthread_monitor_enter(jvmtiData->mutex);
	agentLibrary = findAgentLibrary(vm, library, libraryLength);
	if (NULL != agentLibrary) {
		/* Current thread may need to wait() until the linking thread has linked the library
		 * and initialized linkMode, before notify()ing.  Loop, in order to wake up on spurious
		 * notification, until the linkMode has actually been set.
		 */
		while (J9NATIVELIB_LINK_MODE_UNINITIALIZED == agentLibrary->nativeLib.linkMode) {
			omrthread_monitor_wait(jvmtiData->mutex);
		}
		omrthread_monitor_exit(jvmtiData->mutex);
		/* Try invoking Agent_OnAttach_L function, if agent was linked statically.  */
		if (J9NATIVELIB_LINK_MODE_STATIC == agentLibrary->nativeLib.linkMode) {
			loadFunctionNameLength = j9str_printf(
											PORTLIB,
											loadFunctionName,
											(J9JVMTI_BUFFER_LENGTH + 1),
											"%s_%s",
											J9JVMTI_AGENT_ONATTACH,
											agentLibrary->nativeLib.name);
			if (loadFunctionNameLength >= J9JVMTI_BUFFER_LENGTH) {
				rc = JNI_ERR;
				goto exit;
			}
			Trc_JVMTI_loadAgentLibraryOnAttach_attachingAgentStatically(agentLibrary->nativeLib.name);
		} else /* J9NATIVELIB_LINK_MODE_DYNAMIC == linkMode */ {
			/* If agent was linked dynamically, invoke Agent_OnAttach instead. */
			strcpy(loadFunctionName, J9JVMTI_AGENT_ONATTACH);
			Trc_JVMTI_loadAgentLibraryOnAttach_attachingAgentDynamically(agentLibrary->nativeLib.name);
		}
		rc = issueAgentOnLoadAttach(vm, agentLibrary, options, loadFunctionName, &found);
		if (JNI_OK == rc) {
			omrthread_monitor_enter(jvmtiData->mutex);
			agentLibrary->loadCount++;
			omrthread_monitor_exit(jvmtiData->mutex);
		}
	} else {
		rc = createAgentLibrary(vm, library, libraryLength, options, optionsLength, decorate, &agentLibrary);
		/* Do not notify yet, just exit the monitor so threads competing to enter may do so and
		 * wait for this thread, which is yet to link.
		 */
		omrthread_monitor_exit(jvmtiData->mutex);
		if (JNI_OK != rc) {
			goto exit;
		}
		loadFunctionNameLength = j9str_printf(
			PORTLIB,
			loadFunctionName,
			(J9JVMTI_BUFFER_LENGTH + 1),
			"%s_%s",
			J9JVMTI_AGENT_ONATTACH,
			agentLibrary->nativeLib.name);
		if (loadFunctionNameLength >= J9JVMTI_BUFFER_LENGTH) {
			rc = JNI_ERR;
			goto exit;
		}
		rc = loadAgentLibraryGeneric(vm,
			agentLibrary,
			loadFunctionName,
			TRUE, /* link statically. */
			&found,
			&errorMessage);
		if (found) {
			/* Agent being linked statically. */
			Trc_JVMTI_loadAgentLibraryOnAttach_attachingAgentStatically(agentLibrary->nativeLib.name);
			omrthread_monitor_enter(jvmtiData->mutex);
			agentLibrary->nativeLib.linkMode = J9NATIVELIB_LINK_MODE_STATIC; /* Indicate linking mode. */
		} else /* if (!found) */ {
			/* Either agent could not be linked statically or running j2se version less than 1.8;
			 * try dynamic linking.
			 */
			rc = loadAgentLibraryGeneric(vm,
										 agentLibrary,
										 J9JVMTI_AGENT_ONATTACH,
										 FALSE, /* link dynamically. */
										 &found,
										 &errorMessage);
			if (found) {
				Trc_JVMTI_loadAgentLibraryOnAttach_attachingAgentDynamically(agentLibrary->nativeLib.name);
				omrthread_monitor_enter(jvmtiData->mutex);
				agentLibrary->nativeLib.linkMode = J9NATIVELIB_LINK_MODE_DYNAMIC; /* Indicate linking mode. */
			}
		}
		if (!found) {
			omrthread_monitor_enter(jvmtiData->mutex);
		}
		/* Notify waiting threads irrespective of whether linking succeeded or not. */
		omrthread_monitor_notify_all(jvmtiData->mutex);
		omrthread_monitor_exit(jvmtiData->mutex);
	}
exit:
	if (JNI_OK != rc) {
		Trc_JVMTI_loadAgentLibraryOnAttach_failedAttachingAgent(library);
	}
	Trc_JVMTI_loadAgentLibraryOnAttach_Exit(agentLibrary->nativeLib.name, (I_32)rc);

	return (I_32)rc;
}

/**
 * Load a JVMTI agent at boot time and call the Agent_OnLoad function
 * @param vm Java VM
 * @param agentLibrary environment for the agent
 * @return JNI_ERR, JNI_OK
 */
static jint
loadAgentLibrary(J9JavaVM * vm, J9JVMTIAgentLibrary * agentLibrary)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(vm);
	jint result = 0;
	BOOLEAN found = FALSE;
	const char *errorMessage = NULL;
	char nameBuffer[J9JVMTI_BUFFER_LENGTH + 1] = {0};
	UDATA nameBufferLengh = 0;

	Trc_JVMTI_loadAgentLibrary_Entry(agentLibrary->nativeLib.name);

	/* If dynamic agent loading is disabled, the HCR/OSR flags default to off.
	 * However, adding agents at launch is allowed. In this case we need to
	 * enable the flags for proper functionality.
	 */
	if (J9_ARE_NO_BITS_SET(vm->runtimeFlags, J9_RUNTIME_ALLOW_DYNAMIC_AGENT)) {
		omrthread_monitor_enter(vm->runtimeFlagsMutex);
		vm->extendedRuntimeFlags |= J9_EXTENDED_RUNTIME_OSR_SAFE_POINT | J9_EXTENDED_RUNTIME_ENABLE_HCR;
		omrthread_monitor_exit(vm->runtimeFlagsMutex);
	}

	/* Attempt linking the agent statically, looking for Agent_OnLoad_L.
	 * If this is not found, fall back on the regular, dynamic linking way.
	 */
	nameBufferLengh = j9str_printf(
							PORTLIB,
							nameBuffer,
							(J9JVMTI_BUFFER_LENGTH + 1),
							"%s_%s",
							J9JVMTI_AGENT_ONLOAD,
							agentLibrary->nativeLib.name);
	if (nameBufferLengh >= J9JVMTI_BUFFER_LENGTH) {
		result = JNI_ERR;
		goto exit;
	}

	result = loadAgentLibraryGeneric(vm, agentLibrary, nameBuffer, TRUE, &found, &errorMessage);

	/* Set this TRUE; if it wasn't actually found, the next check will set this FALSE,
	 * or else this indicates to Agent_OnUnload that the agent was loaded via static linking.
	 */
	omrthread_monitor_enter(jvmtiData->mutex);
	agentLibrary->nativeLib.linkMode = J9NATIVELIB_LINK_MODE_STATIC;
	omrthread_monitor_exit(jvmtiData->mutex);

	/* If the initializer "Agent_OnLoad_L" was /not/ found (either not defined OR running
	 * running j2se version less than 1.8), fallback on dynamic linking, with "Agent_OnLoad".
	 */
	if (!found) {
		omrthread_monitor_enter(jvmtiData->mutex);
		agentLibrary->nativeLib.linkMode = J9NATIVELIB_LINK_MODE_DYNAMIC;
		omrthread_monitor_exit(jvmtiData->mutex);
		result = loadAgentLibraryGeneric(vm, agentLibrary, J9JVMTI_AGENT_ONLOAD, FALSE, &found, &errorMessage);

		/* Move error reporting up the call chain, not immediately when it is discovered missing. */
		if (!found) {
			if (NULL != errorMessage) {
				j9nls_printf(PORTLIB,
					 		 J9NLS_ERROR,
					 		 J9NLS_JVMTI_AGENT_LIBRARY_OPEN_FAILED,
					 		 agentLibrary->nativeLib.name,
					 		 errorMessage);
			} else {
				j9nls_printf(PORTLIB,
						    J9NLS_ERROR,
						    J9NLS_JVMTI_AGENT_INITIALIZATION_FUNCTION_NOT_FOUND,
						    J9JVMTI_AGENT_ONLOAD,
						    agentLibrary->nativeLib.name);
			}
		}
	}
exit:
	Trc_JVMTI_loadAgentLibrary_Exit(result);
	return result;
}

static void
shutDownAgentLibraries(J9JavaVM * vm, UDATA closeLibrary)
{
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(vm);
	char nameBuffer[J9JVMTI_BUFFER_LENGTH] = {0};

	Trc_JVMTI_shutDownAgentLibraries_Entry();

	if (jvmtiData->agentLibraries != NULL) {
		PORT_ACCESS_FROM_JAVAVM(vm);
		J9JVMTIAgentLibrary * agentLibrary;
		pool_state state;

		agentLibrary = pool_startDo(jvmtiData->agentLibraries, &state);
		while(agentLibrary) {
			if (agentLibrary->nativeLib.handle != 0) {
				void (JNICALL * onUnload)(J9JavaVM *);

				/* If the agent was loaded/attached through static linking, check for the
				 * presence of Agent_OnUnload_L.  If this cannot be found, go on to look for
				 * Agent_OnUnload, or else invoke Agent_OnUnload_L.
				 */
				if (J9NATIVELIB_LINK_MODE_STATIC == agentLibrary->nativeLib.linkMode) {
					j9str_printf(PORTLIB,
								 nameBuffer,
								 sizeof(nameBuffer),
								 "%s_%s",
								 J9JVMTI_AGENT_ONUNLOAD,
								 agentLibrary->nativeLib.name);
				} else /* J9NATIVELIB_LINK_MODE_DYNAMIC == linkMode */ {
					strcpy(nameBuffer, J9JVMTI_AGENT_ONUNLOAD);
				}

				if (j9sl_lookup_name(agentLibrary->nativeLib.handle, nameBuffer, (void *) &onUnload, "VL") == 0) {
					UDATA loadCount;

					Trc_JVMTI_shutDownAgentLibraries_invokingAgentShutDown(nameBuffer);
					for (loadCount = 0; loadCount < agentLibrary->loadCount; loadCount++) {
						onUnload(vm);
					}
				}
				if (closeLibrary && (agentLibrary->xRunLibrary == NULL)) {
					j9sl_close_shared_library(agentLibrary->nativeLib.handle);
				}
			}
			if (agentLibrary->xRunLibrary == NULL) {
				j9mem_free_memory(agentLibrary->nativeLib.name);
			}

			/* Jazz 99339: release memory allocated for J9InvocationJavaVM in issueAgentOnLoadAttach() */
			if (NULL != agentLibrary->invocationJavaVM) {
				j9mem_free_memory(agentLibrary->invocationJavaVM);
				agentLibrary->invocationJavaVM = NULL;
			}

			agentLibrary = pool_nextDo(&state);
		}
		pool_kill(jvmtiData->agentLibraries);
		jvmtiData->agentLibraries = NULL;
	}
	Trc_JVMTI_shutDownAgentLibraries_Exit();
}



static jint
initializeJVMTI(J9JavaVM * vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9JVMTIData * jvmtiData;
	J9HookInterface ** gcOmrHook = vm->memoryManagerFunctions->j9gc_get_omr_hook_interface(vm->omrVM);

	/* Ensure the GC end hooks remain available */

	if ((*gcOmrHook)->J9HookReserve(gcOmrHook, J9HOOK_MM_OMR_GLOBAL_GC_END) ||
		(*gcOmrHook)->J9HookReserve(gcOmrHook, J9HOOK_MM_OMR_LOCAL_GC_END)) {
		return JNI_ERR;
	}

	jvmtiData = j9mem_allocate_memory(sizeof(J9JVMTIData), J9MEM_CATEGORY_JVMTI);
	if (jvmtiData == NULL) {
		return JNI_ENOMEM;
	}
	memset(jvmtiData, 0, sizeof(J9JVMTIData));
	vm->jvmtiData = jvmtiData;
	jvmtiData->vm = vm;

	jvmtiData->agentLibraries = pool_new(sizeof(J9JVMTIAgentLibrary), 0, 0, POOL_ALWAYS_KEEP_SORTED, J9_GET_CALLSITE(), J9MEM_CATEGORY_JVMTI, POOL_FOR_PORT(vm->portLibrary));
	if (jvmtiData->agentLibraries == NULL) {
		return JNI_ENOMEM;
	}

	jvmtiData->environments = pool_new(sizeof(J9JVMTIEnv), 0, 0, POOL_ALWAYS_KEEP_SORTED, J9_GET_CALLSITE(), J9MEM_CATEGORY_JVMTI, POOL_FOR_PORT(vm->portLibrary));
	if (jvmtiData->environments == NULL) {
		return JNI_ENOMEM;
	}

	jvmtiData->breakpoints = pool_new(sizeof(J9JVMTIGlobalBreakpoint), 0, 0, POOL_ALWAYS_KEEP_SORTED, J9_GET_CALLSITE(), J9MEM_CATEGORY_JVMTI, POOL_FOR_PORT(vm->portLibrary));
	if (jvmtiData->breakpoints == NULL) {
		return JNI_ENOMEM;
	}

	jvmtiData->breakpointedMethods = pool_new(sizeof(J9JVMTIBreakpointedMethod), 0, 0, POOL_ALWAYS_KEEP_SORTED, J9_GET_CALLSITE(), J9MEM_CATEGORY_JVMTI, POOL_FOR_PORT(vm->portLibrary));
	if (jvmtiData->breakpointedMethods == NULL) {
		return JNI_ENOMEM;
	}

	if (omrthread_monitor_init(&(jvmtiData->mutex), 0) != 0) {
		return JNI_ENOMEM;
	}

	if (omrthread_monitor_init(&(jvmtiData->redefineMutex), 0) != 0) {
		return JNI_ENOMEM;
	}

	if (0 != omrthread_monitor_init(&(jvmtiData->compileEventMutex), 0)) {
		return JNI_ENOMEM;
	}

	jvmtiData->phase = JVMTI_PHASE_ONLOAD;
	jvmtiData->compileEventThreadState = J9JVMTI_COMPILE_EVENT_THREAD_STATE_DEAD;

	return JNI_OK;
}



static void
shutDownJVMTI(J9JavaVM * vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9JVMTIData * jvmtiData = vm->jvmtiData;

	if (jvmtiData != NULL) {
		unhookGlobalEvents(jvmtiData);

		/* Shut down agent libraries, closing them */

		shutDownAgentLibraries(vm, TRUE);

		/* Dispose any remaining environments, completely freeing their data - assume we are single-threaded at this point */

		if (jvmtiData->environments != NULL) {
			pool_state state;
			J9JVMTIEnv * currentEnvironment;

			currentEnvironment = pool_startDo(jvmtiData->environments, &state);
			while (currentEnvironment != NULL) {
				disposeEnvironment(currentEnvironment, TRUE);
				currentEnvironment = pool_nextDo(&state);
			}

			pool_kill(jvmtiData->environments);
		}

		/* Free global data */

		if (jvmtiData->breakpoints != NULL) {
			/* remove any remaining breakpoints? */
			pool_kill(jvmtiData->breakpoints);
		}
		if (jvmtiData->breakpointedMethods != NULL) {
			/* remove any remaining breakpoints? */
			pool_kill(jvmtiData->breakpointedMethods);
		}
		if (jvmtiData->redefineMutex != NULL) {
			omrthread_monitor_destroy(jvmtiData->redefineMutex);
		}
		if (jvmtiData->mutex != NULL) {
			omrthread_monitor_destroy(jvmtiData->mutex);
		}
		if (NULL != jvmtiData->compileEventMutex) {
			omrthread_monitor_destroy(jvmtiData->compileEventMutex);
		}
		j9mem_free_memory(jvmtiData->copiedJNITable);
		j9mem_free_memory(jvmtiData);
		vm->jvmtiData = NULL;
	}
}

/**
 * Allocate an instance of the J9JVMTIAgentLibrary structure.
 * @param vm Java VM
 * @param libraryName library name and/or options concatenated
 * @param libraryNameLength library name length
 * @param options option string start
 * @param optionsLength option string length
 * @param decorate change the library path/name
 * @param result pointer to return new J9JVMTIAgentLibrary by reference
 * @return JNI_OK on success, otherwis JNI_ERR or JNI_ENOMEM
 */
static jint
createAgentLibrary(J9JavaVM *vm, const char *libraryName, UDATA libraryNameLength, const char *options, UDATA optionsLength, UDATA decorate, J9JVMTIAgentLibrary **result)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9JVMTIData *jvmtiData = J9JVMTI_DATA_FROM_VM(vm);
	J9JVMTIAgentLibrary *agentLibrary = NULL;

	/* Create a new agent library. */
	omrthread_monitor_enter(jvmtiData->mutex);
	agentLibrary = pool_newElement(jvmtiData->agentLibraries);
	if (NULL == agentLibrary) {
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_JVMTI_OUT_OF_MEMORY, libraryName);
		omrthread_monitor_exit(jvmtiData->mutex);
		return JNI_ENOMEM;
	}

	/* Initialize the structure with default values. */
	vm->internalVMFunctions->initializeNativeLibrary(vm, &agentLibrary->nativeLib);

	/* Make a copy of the library name and options. */
	agentLibrary->nativeLib.name = j9mem_allocate_memory(libraryNameLength + 1 + optionsLength + 1, J9MEM_CATEGORY_JVMTI);
	if (NULL == agentLibrary->nativeLib.name) {
		pool_removeElement(jvmtiData->agentLibraries, agentLibrary);
		/* Print NLS message here? */
		omrthread_monitor_exit(jvmtiData->mutex);
		return JNI_ENOMEM;
	}
	strncpy(agentLibrary->nativeLib.name, libraryName, libraryNameLength);
	*(agentLibrary->nativeLib.name + libraryNameLength) = '\0';
	agentLibrary->options = agentLibrary->nativeLib.name+libraryNameLength+1;
	if (optionsLength > 0) {
		strncpy(agentLibrary->options, options, optionsLength);
	}
	*(agentLibrary->options+optionsLength) = '\0';

	agentLibrary->nativeLib.handle = 0;
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	agentLibrary->nativeLib.doSwitching = FALSE;
#endif
	agentLibrary->nativeLib.linkMode = J9NATIVELIB_LINK_MODE_UNINITIALIZED;
	agentLibrary->decorate = decorate;
	agentLibrary->xRunLibrary = NULL;
	agentLibrary->loadCount = 1;
	/* Jazz 99339: initialize agentLibrary->invocationJavaVM to NULL when a new library is created. */
	agentLibrary->invocationJavaVM = NULL;

	if (NULL != result) {
		*result = agentLibrary;
	}

	omrthread_monitor_exit(jvmtiData->mutex);
	return JNI_OK;
}

static jint
createXrunLibraries(J9JavaVM * vm)
{
	if (vm->dllLoadTable) {
		PORT_ACCESS_FROM_JAVAVM(vm);
		J9JVMTIData * jvmtiData = vm->jvmtiData;
		J9VMDllLoadInfo* entry;
		pool_state aState;

		entry = pool_startDo(vm->dllLoadTable, &aState);
		while(entry) {
			if (entry->loadFlags & AGENT_XRUN) {
				J9JVMTIAgentLibrary * agentLibrary;

				agentLibrary = pool_newElement(jvmtiData->agentLibraries);
				if (agentLibrary == NULL) {
					j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_JVMTI_OUT_OF_MEMORY, entry->dllName);
					return JNI_ENOMEM;
				}

				/* Initialize the structure with default values */
				vm->internalVMFunctions->initializeNativeLibrary(vm, &agentLibrary->nativeLib);

				agentLibrary->nativeLib.name = entry->dllName;
				agentLibrary->nativeLib.handle = 0;
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
				agentLibrary->nativeLib.doSwitching = FALSE;
#endif
				agentLibrary->decorate = FALSE;
				agentLibrary->options = (char*) entry->reserved;				/* reserved is used to store any options for Xruns*/
				agentLibrary->xRunLibrary = entry;
				agentLibrary->loadCount = 1;
			}
			entry = pool_nextDo(&aState);
		}
	}

	return JNI_OK;
}

#if defined(AIXPPC)
/**
 * Check if the first dirLen bytes of the file name is a library path.
 *
 * @param[in] fileName the file name containing a path
 * @param[in] dirLen the number of bytes to be compared
 * @param[in] libpath the library path returned from loadquery(L_GETLIBPATH)
 *
 * @return TRUE if the first dirLen bytes of the file name is a library path, FALSE otherwise
 */
static BOOLEAN
isFileNameInLibPath(const char *fileName, UDATA dirLen, char *libpath)
{
	BOOLEAN result = FALSE;
	char *path = NULL;
	char *strTokPtr = NULL;

	for (path = strtok_r(libpath, ":", &strTokPtr);
			NULL != path;
			path = strtok_r(NULL, ":", &strTokPtr)
	) {
		if ((0 == strncmp(fileName, path, dirLen)) && ('\0' == path[dirLen])) {
			/* a match found */
			result = TRUE;
			Trc_JVMTI_isFileNameInLibPath_path_match(fileName, path, dirLen);
			break;
		}
		/* continue to search the libpath */
		Trc_JVMTI_isFileNameInLibPath_path_not_match(fileName, path, dirLen);
	}
	return result;
}

/**
 * Check if an object file loaded is the same as an agent library to be loaded.
 *
 * @param[in] fileName the object file name retrieved via loadquery(L_GETINFO)
 * @param[in] libName the agent library name to be searched
 * @param[in] decorate a boolean indicating if the prefix/suffix to the library name is to be added
 * @param[in] isLibraryPath a boolean indicating if the libName contains a path separator
 * @param[in] libNameSo the platform independent agent library name added prefix(lib) and suffix(.so)
 * @param[in] libNameAr the platform independent agent library name added prefix(lib) and suffix(.a)
 * @param[in] libpath the library path returned via loadquery(L_GETLIBPATH)
 *
 * @return TRUE if the agent library is the same as the object file, FALSE otherwise
 */
static BOOLEAN
searchObjectFileFromLDInfo(const char *fileName, const char *libName, BOOLEAN decorate, BOOLEAN isLibraryPath,
		const char *libNameSo, const char *libNameAr, char *libpath)
{
	BOOLEAN result = FALSE;
	/* locate the last occurrence of file path separator in the object file */
	const char *tmp = strrchr(fileName, '/');
	if (NULL == tmp) {
		/* this object file has no path separator */
		if (decorate) {
			/* compare with libName added the prefix/suffix */
			if ((0 == strcmp(fileName, libNameSo))
				|| (0 == strcmp(fileName, libNameAr))
			) {
				result = TRUE;
			}
		} else {
			if (!isLibraryPath) {
				/* libName has no path separator, compare the whole file names */
				if (0 == strcmp(fileName, libName)) {
					result = TRUE;
				}
			}
			/* if libName contains a path separator, it can't be the same library as this fileName */
		}
	} else {
		/* find the library name of this object file */
		const char *tmpLibName = tmp + 1;
		if (decorate) {
			/* compare with libName added the prefix/suffix */
			if ((0 == strcmp(tmpLibName, libNameSo))
				|| (0 == strcmp(tmpLibName, libNameAr))
			) {
				/* check if this object file path is part of libpath */
				UDATA dirLen = tmp - fileName;
				result = isFileNameInLibPath(fileName, dirLen, libpath);
			}
		} else {
			/* check if libName contains a path separator */
			if (isLibraryPath) {
				/* compare the library directly with the object file loaded */
				if (0 == strcmp(fileName, libName)) {
					Trc_JVMTI_searchObjectFileFromLDInfo_library_object_file_found(fileName);
					result = TRUE;
				} else {
					Trc_JVMTI_searchObjectFileFromLDInfo_library_object_file_not_found(fileName, libName);
				}
			} else {
				/* libName has no path separator, compare the whole file names */
				if (0 == strcmp(tmpLibName, libName)) {
					/* check if this object file path is part of libpath */
					UDATA dirLen = tmp - fileName;
					result = isFileNameInLibPath(fileName, dirLen, libpath);
				}
			}
		}
	}
	return result;
}

/**
 * Retrieve a list of all object files loaded for the current process via loadquery(L_GETINFO).
 * This helper method reallocates the buffer if the initial allocation is insufficient.
 * The caller is responsible for freeing the buffer allocated.
 *
 * @param[in] vm Java VM
 *
 * @return a pointer to struct ld_info holding the object files if succeeded, otherwise NULL
 */
static struct ld_info *
loadqueryGetObjectFiles(J9JavaVM *vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	/* Use the same initial buffer size as omrintrospect_backtrace_symbols_raw()
	 * in omr/port/aix/omrosbacktrace_impl.c.
	 */
	UDATA objectFileSize = 150 * sizeof(struct ld_info);
	void *tmpBuffer = j9mem_allocate_memory(objectFileSize, J9MEM_CATEGORY_JVMTI);
	if (NULL == tmpBuffer) {
		Trc_JVMTI_loadqueryGetObjectFiles_OOM(objectFileSize);
		vmFuncs->setNativeOutOfMemoryError(vm->mainThread, 0, 0);
	} else {
		Trc_JVMTI_loadqueryGetObjectFiles_allocate_memory(tmpBuffer, objectFileSize);
		for (;;) {
			int rc = loadquery(L_GETINFO, tmpBuffer, objectFileSize);
			if (-1 != rc) {
				/* loadquery() completed successfully */
				break;
			}
			if (ENOMEM == errno) {
				void *newBuffer = NULL;
				/* insufficient buffer size, increase it */
				objectFileSize *= 2;
				newBuffer = j9mem_reallocate_memory(tmpBuffer, objectFileSize, J9MEM_CATEGORY_JVMTI);
				if (NULL == newBuffer) {
					Trc_JVMTI_loadqueryGetObjectFiles_OOM(objectFileSize);
					vmFuncs->setNativeOutOfMemoryError(vm->mainThread, 0, 0);
					j9mem_free_memory(tmpBuffer);
					tmpBuffer = NULL;
					break;
				}
				tmpBuffer = newBuffer;
				Trc_JVMTI_loadqueryGetObjectFiles_allocate_memory(tmpBuffer, objectFileSize);
			} else {
				/* other errors - EINVAL or EFAULT */
				Trc_JVMTI_loadqueryGetObjectFiles_loadquery_errno(errno);
				j9mem_free_memory(tmpBuffer);
				tmpBuffer = NULL;
				break;
			}
		}
	}
	return (struct ld_info *)tmpBuffer;
}
#endif /* defined(AIXPPC) */

/**
 * Test if an agent library was already loaded.
 *
 * @param vm Java VM
 * @param library library name
 * @param decorate a boolean indicating if the prefix/suffix to the library name is to be added
 *
 * @return TRUE if the agent library was loaded successfully, FALSE otherwise
 */
static BOOLEAN
isAgentLibraryLoaded(J9JavaVM *vm, const char *library, BOOLEAN decorate)
{
	BOOLEAN result = FALSE;
	UDATA libraryLen = strlen(library);

	Trc_JVMTI_isAgentLibraryLoaded_library_decorate(library, decorate);
	if (NULL != findAgentLibrary(vm, library, libraryLen)) {
		result = TRUE;
	} else {
		/* J9PORT_SLOPEN_NO_LOAD is only supported on Linux/OSX/Win32 platforms. */
#if defined(LINUX) || defined(OSX) || defined(WIN32)
		UDATA i = 0;
		UDATA handle = 0;
		/* One of RTLD_LAZY and RTLD_NOW must be included in the flag. */
		UDATA openFlags = J9PORT_SLOPEN_NO_LOAD | OMRPORT_SLOPEN_LAZY;
		char *agentPath = (char*)library;
		PORT_ACCESS_FROM_JAVAVM(vm);

		if (decorate) {
			openFlags |= J9PORT_SLOPEN_DECORATE;
		}
		for (i = 0;; ++i) {
			const char *systemAgentName = systemAgentNames[i];
			if (NULL == systemAgentName) {
				break;
			}
			if (0 == strcmp(library, systemAgentName)) {
				agentPath = prependSystemAgentPath(vm, library);
				break;
			}
		}
		if (0 == j9sl_open_shared_library(agentPath, &handle, openFlags)) {
			if (0 != handle) {
				result = TRUE;
				j9sl_close_shared_library(handle);
			}
		}
		if (library != (const char*)agentPath) {
			j9mem_free_memory(agentPath);
		}
		Trc_JVMTI_isAgentLibraryLoaded_result(library, agentPath, result);
#elif defined(AIXPPC) /* defined(LINUX) || defined(OSX) || defined(WIN32) */
		/* The incoming library & decorate could be in the following cases:
		 * - JvmtiAgent1 & TRUE
		 * - libJvmtiAgent1.so & FALSE
		 * - /path/to/libJvmtiAgent1.so & FALSE
		 */
		PORT_ACCESS_FROM_JAVAVM(vm);
		/* retrieve a list of all object files loaded for the current process */
		struct ld_info *objectFiles = loadqueryGetObjectFiles(vm);
		if (NULL != objectFiles) {
			J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
			char *libNameSo = NULL;
			char *libNameAr = NULL;
			char *libpath = NULL;
			UDATA libpathSize = 0;
			char *envLibPath = NULL;
			char *fileName = NULL;
			BOOLEAN isLibraryPath = FALSE;
			struct ld_info *cursor = NULL;

			if (decorate) {
				/* the library is a platform independent name */
				/* either object suffix is acceptable */
				UDATA sizeLibNameSo = libraryLen + sizeof("lib.so");
				UDATA sizeLibNameAr = libraryLen + sizeof("lib.a");
				libNameSo = (char *)j9mem_allocate_memory(sizeLibNameSo, J9MEM_CATEGORY_JVMTI);
				libNameAr = (char *)j9mem_allocate_memory(sizeLibNameAr, J9MEM_CATEGORY_JVMTI);
				if ((NULL != libNameSo) && (NULL != libNameAr)) {
					j9str_printf(PORTLIB, libNameSo, sizeLibNameSo, "lib%s.so", library);
					j9str_printf(PORTLIB, libNameAr, sizeLibNameAr, "lib%s.a", library);
					Trc_JVMTI_isAgentLibraryLoaded_libNames(libNameSo, libNameAr);
				} else {
					Trc_JVMTI_isAgentLibraryLoaded_OOM("libNameSo or libNameAr");
					vmFuncs->setNativeOutOfMemoryError(vm->mainThread, 0, 0);
					goto failed;
				}
			} else {
				/* check if library contains a path separator */
				isLibraryPath = (NULL != strrchr(library, '/'));
			}

			/* Because redirector adds onto LIBPATH, fetching LIBPATH via getenv is more
			 * accurate than fetching LIBPATH from loadquery using L_GETLIBATH flag.
			 */
			envLibPath = getenv("LIBPATH");
			if (NULL != envLibPath) {
				libpathSize = strlen(envLibPath) + 1;
				libpath = j9mem_allocate_memory(libpathSize, J9MEM_CATEGORY_JVMTI);
				if (NULL == libpath) {
					Trc_JVMTI_isAgentLibraryLoaded_OOM("libpath");
					vmFuncs->setNativeOutOfMemoryError(vm->mainThread, 0, 0);
					goto failed;
				}
				/* copy it to avoid the modification by putenv() */
				memcpy(libpath, envLibPath, libpathSize);
			} else {
				/* assign it an empty string */
				libpath = (char *)"";
			}
			Trc_JVMTI_isAgentLibraryLoaded_loadquery_getlibpath(libpath, libpathSize);
			for (cursor = objectFiles;;) {
				/* objectFiles is not NULL */
				if (0 == cursor->ldinfo_next) {
					/* last entry, exit */
					break;
				}
				fileName = cursor->ldinfo_filename;
				Trc_JVMTI_isAgentLibraryLoaded_ldinfo_filename_decorate(fileName, decorate);
				result = searchObjectFileFromLDInfo(fileName, library, decorate, isLibraryPath, libNameSo, libNameAr, libpath);
				if (result) {
					break;
				}
				cursor = (struct ld_info *)((char *)cursor + cursor->ldinfo_next);
			}
failed:
			if (libpathSize > 0) {
				j9mem_free_memory(libpath);
			}
			j9mem_free_memory(libNameSo);
			j9mem_free_memory(libNameAr);
			j9mem_free_memory(objectFiles);
		}
#endif /* defined(LINUX) || defined(OSX) || defined(WIN32) */
	}

	return result;
}

static J9JVMTIAgentLibrary *
findAgentLibrary(J9JavaVM * vm, const char *libraryAndOptions, UDATA libraryLength)
{
	J9JVMTIData * jvmtiData = vm->jvmtiData;
	J9JVMTIAgentLibrary * agentLibrary;
	J9NativeLibrary * nativeLibrary = NULL;
	UDATA nativeLibraryNameLength;
    pool_state state;

    Trc_JVMTI_findAgentLibrary_Entry(libraryAndOptions);
	agentLibrary = pool_startDo(jvmtiData->agentLibraries, &state);
	while (agentLibrary) {
        nativeLibrary = &(agentLibrary->nativeLib);
 		nativeLibraryNameLength = strlen(nativeLibrary->name);
		if (nativeLibraryNameLength == libraryLength) {
			if (!strncmp(libraryAndOptions, nativeLibrary->name, libraryLength)) {
				Trc_JVMTI_findAgentLibrary_successExit(nativeLibrary->name);
				return agentLibrary;
			}
		}
		agentLibrary = pool_nextDo(&state);
	}
	Trc_JVMTI_findAgentLibrary_failureExit(libraryAndOptions);

	return NULL;
}
