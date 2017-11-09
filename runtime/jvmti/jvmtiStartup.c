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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
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

#define _UTE_STATIC_
#include "ut_j9jvmti.h"

#include "jvmtiHelpers.h"

#define THIS_DLL_NAME J9_JVMTI_DLL_NAME
#define OPT_AGENTPATH_COLON "-agentpath:"
#define OPT_AGENTLIB_COLON "-agentlib:"

static void shutDownJVMTI (J9JavaVM * vm);
static jint createAgentLibrary (J9JavaVM * vm, const char *libraryAndOptions, UDATA libraryLength, const char* options, UDATA optionsLength, UDATA decorate, J9JVMTIAgentLibrary **result);
static jint initializeJVMTI (J9JavaVM * vm);
static void shutDownAgentLibraries (J9JavaVM * vm, UDATA closeLibrary);
static jint loadAgentLibrary (J9JavaVM * vm, J9JVMTIAgentLibrary * agentLibrary);
static jint createXrunLibraries (J9JavaVM * vm);
static J9JVMTIAgentLibrary * findAgentLibrary(J9JavaVM * vm, const char *libraryAndOptions, UDATA libraryLength);
static jint issueAgentOnLoadAttach(J9JavaVM * vm, J9JVMTIAgentLibrary * agentLibrary, const char* options, char *loadFunctionName, BOOLEAN * foundLoadFn);
I_32 JNICALL loadAgentLibraryOnAttach(struct J9JavaVM * vm, const char * library, const char *options, UDATA decorate) ;


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

IDATA J9VMDllMain(J9JavaVM* vm, IDATA stage, void* reserved)
{
	IDATA returnVal = J9VMDLLMAIN_OK;
	IDATA agentIndex;
	pool_state poolState;
	J9JVMTIAgentLibrary * agentLibrary;
	J9JVMTIData * jvmtiData;

	switch(stage) {

		case ALL_VM_ARGS_CONSUMED :
		{
			PORT_ACCESS_FROM_JAVAVM(vm);
			char optionsBuf[OPTIONSBUFF_LEN];
			char* optionsPtr = (char*)optionsBuf;
			UDATA buflen = OPTIONSBUFF_LEN;
			IDATA option_rc = 0;

			if (initializeJVMTI(vm) != JNI_OK) {
				goto _error;
			}

			agentIndex = FIND_AND_CONSUME_ARG_FORWARD( STARTSWITH_MATCH, OPT_AGENTLIB_COLON, NULL);
			while (agentIndex >= 0) {
				UDATA libraryLength;
				char *options;
				UDATA optionsLength;

				do {
					option_rc = COPY_OPTION_VALUE(agentIndex, ':', &optionsPtr, buflen);
					if (OPTION_BUFFER_OVERFLOW == option_rc) {
						if (optionsPtr != (char*)optionsBuf) {
							j9mem_free_memory(optionsPtr);
							optionsPtr = NULL;
						}
						buflen *= 2;
						optionsPtr = (char*)j9mem_allocate_memory(buflen, OMRMEM_CATEGORY_VM);
						if (NULL == optionsPtr) {
							goto _error;
						}
					}
				} while (OPTION_BUFFER_OVERFLOW == option_rc);

				parseLibraryAndOptions(optionsPtr, &libraryLength, &options, &optionsLength);
				if (createAgentLibrary(vm, optionsPtr, libraryLength, options, optionsLength, TRUE, NULL) != JNI_OK) {
					if (optionsPtr != (char*)optionsBuf) {
						j9mem_free_memory(optionsPtr);
						optionsPtr = NULL;
					}
					goto _error;
				}
				agentIndex = FIND_NEXT_ARG_IN_VMARGS_FORWARD( STARTSWITH_MATCH, OPT_AGENTLIB_COLON, NULL, agentIndex);
			}
			if (optionsPtr != (char*)optionsBuf) {
				j9mem_free_memory(optionsPtr);
				optionsPtr = NULL;
			}

			agentIndex = FIND_AND_CONSUME_ARG_FORWARD( STARTSWITH_MATCH, OPT_AGENTPATH_COLON, NULL);
			optionsPtr = (char*)optionsBuf;
			buflen = OPTIONSBUFF_LEN;
			while (agentIndex >= 0) {
				UDATA libraryLength;
				char *options;
				UDATA optionsLength;

				do {
					option_rc = COPY_OPTION_VALUE(agentIndex, ':', &optionsPtr, buflen);
					if (OPTION_BUFFER_OVERFLOW == option_rc) {
						if (optionsPtr != (char*)optionsBuf) {
							j9mem_free_memory(optionsPtr);
							optionsPtr = NULL;
						}
						buflen *= 2;
						optionsPtr = (char*)j9mem_allocate_memory(buflen, OMRMEM_CATEGORY_VM);
						if (NULL == optionsPtr) {
							goto _error;
						}
					}
				} while (OPTION_BUFFER_OVERFLOW == option_rc);

				parseLibraryAndOptions(optionsPtr, &libraryLength, &options, &optionsLength);
				if (createAgentLibrary(vm, optionsPtr, libraryLength, options, optionsLength, FALSE, NULL) != JNI_OK) {
					if (optionsPtr != (char*)optionsBuf) {
						j9mem_free_memory(optionsPtr);
						optionsPtr = NULL;
					}
					goto _error;
				}
				agentIndex = FIND_NEXT_ARG_IN_VMARGS_FORWARD( STARTSWITH_MATCH, OPT_AGENTPATH_COLON, NULL, agentIndex);
			}
			if (optionsPtr != (char*)optionsBuf) {
				j9mem_free_memory(optionsPtr);
				optionsPtr = NULL;
			}

			/* -Xrun libraries that have an Agent_OnLoad are treated like -agentlib: */

			if (createXrunLibraries(vm) != JNI_OK) {
				goto _error;
			}

			vm->loadAgentLibraryOnAttach = &loadAgentLibraryOnAttach;

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
			jvmtiData = J9JVMTI_DATA_FROM_VM(vm);
			if (hookGlobalEvents(jvmtiData)) {
				PORT_ACCESS_FROM_JAVAVM(vm);
				j9tty_err_printf(PORTLIB, "Need NLS message here\n");
				goto _error;
			}

			agentLibrary = pool_startDo(jvmtiData->agentLibraries, &poolState);
			while (agentLibrary != NULL) {
				if (loadAgentLibrary(vm, agentLibrary) != JNI_OK) {
					goto _error;
				}
				agentLibrary = pool_nextDo(&poolState);
			}

			/* Register the hotswap helper trace points */
            hshelpUTRegister(vm);

			jvmtiData->phase = JVMTI_PHASE_PRIMORDIAL;
			break;

		case LIBRARIES_ONUNLOAD :
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
 * @param[in] agentLibrary	Current agent library
 * @param[in] dllName		Agent library name
 * @return full agent path
 *
 *	Returns a fully qualified, platform specific, pathname of the passed in agent
 *  library. Essentially prepends /full/path/jre/lib/ARCH to the agent library name
 *
 *  NOTE: Caller is responsible for freeing the returned buffer
 */
static char *
prependSystemAgentPath(J9JavaVM * vm, J9JVMTIAgentLibrary * agentLibrary, char * dllName)
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
	jint (JNICALL * agentInitFunction)(J9InvocationJavaVM *, char *, void *);
	jint rc;
	J9InvocationJavaVM * invocationJavaVM = NULL;

	Trc_JVMTI_issueAgentOnLoadAttach_Entry(agentLibrary->nativeLib.name);

	if (j9sl_lookup_name(agentLibrary->nativeLib.handle, loadFunctionName, (void *) &agentInitFunction, "ILLL") != 0) {
		/* With JEP-178 specification change (for JVMTI native agents), we can't treat
		 * errors with invoking and failure in finding an exported agent OnAttach function
		 * alike.  Indicate absence of a life-cycle function if lookup failed.
		 */
		Trc_JVMTI_issueAgentOnLoadAttach_failedLocatingLoadFunction(loadFunctionName);
		*foundLoadFn = FALSE;
		rc = JNI_ERR;
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

	rc = agentInitFunction(invocationJavaVM, (char *) options, NULL);
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
		return rc;
	}

	Trc_JVMTI_issueAgentOnLoadAttach_loadFunctionSucceeded(loadFunctionName);
	Trc_JVMTI_issueAgentOnLoadAttach_Exit(agentLibrary->nativeLib.name, rc);
    return rc;
}

/**
 * Load a JVMTI agent and call the agent's intialization function
 * @param vm Java VM
 * @param agentLibrary environemtn for the agent
 * @param name of the initialization function
 * @return JNI_ERR, JNI_OK
 */
static jint
loadAgentLibraryGeneric(J9JavaVM * vm, J9JVMTIAgentLibrary * agentLibrary, char *loadFunctionName, BOOLEAN loadStatically, BOOLEAN * found, const char **errorMessage)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	jint rc;
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(vm);
	J9NativeLibrary * nativeLib = &(agentLibrary->nativeLib);

	Trc_JVMTI_loadAgentLibraryGeneric_Entry(agentLibrary->nativeLib.name);
	if (NULL == agentLibrary->xRunLibrary) {
		jint i = 0;
		char * fullLibName = NULL;
		const char * systemAgentName;		
		UDATA openFlags = agentLibrary->decorate ? J9PORT_SLOPEN_DECORATE | J9PORT_SLOPEN_LAZY : J9PORT_SLOPEN_LAZY;
		char * agentPath = NULL; /* Don't free agentPath; may point at persistent, system areas. */

		if (loadStatically) {
			/* If flag is set, the agent library ought to be opened statically, that is, the executable itself. */
			if (0 == j9sysinfo_get_executable_name(NULL, &agentPath)) {
				openFlags |= J9PORT_SLOPEN_OPEN_EXECUTABLE;
			} else {
				/* Report failure to obtain executable name; caller will proceed with dynamic linking. */
				Trc_JVMTI_loadAgentLibraryGeneric_execNameNotFound(j9error_last_error_number());
				Trc_JVMTI_loadAgentLibraryGeneric_Exit(agentLibrary->nativeLib.name);
				return JNI_ERR;
			}
		} else {
			/* If the user did not specify an explicit agent path, then ensure that the library
			 * is loaded from our current jre tree. This avoids picking up stray agents that might
			 * be found on the library path. See CMVC 144382.
			 */
			while ((systemAgentName = systemAgentNames[i++]) != NULL) {
				if (strcmp(nativeLib->name, systemAgentName) == 0) {
					fullLibName = prependSystemAgentPath(vm, agentLibrary, nativeLib->name);
					Trc_JVMTI_loadAgentLibraryGeneric_loadingAgentAs(fullLibName);
					break;
				}
			}
			agentPath = (NULL != fullLibName) ? fullLibName : nativeLib->name;
		}

		if (j9sl_open_shared_library(agentPath, &(nativeLib->handle), openFlags) != 0) {
			Trc_JVMTI_loadAgentLibraryGeneric_failedOpeningAgentLibrary(agentPath);

			/* We may attempt to open the shared library again so save the error message
			 * and print it once we know it is the final attempt */
			*errorMessage = j9error_last_error_message();

			if (NULL != fullLibName) {
				j9mem_free_memory(fullLibName);
			}
			Trc_JVMTI_loadAgentLibraryGeneric_Exit(agentPath);
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
		Trc_JVMTI_loadAgentLibraryGeneric_agentAttachFailed1(loadFunctionName);
		Trc_JVMTI_loadAgentLibraryGeneric_Exit(agentLibrary->nativeLib.name);
		return rc;
	}

	/* For errors other than load function not found ... */
	if (JNI_OK != rc) {
		Trc_JVMTI_loadAgentLibraryGeneric_agentAttachFailed2(rc);
		Trc_JVMTI_loadAgentLibraryGeneric_Exit(agentLibrary->nativeLib.name);
		return rc;
	}

	Trc_JVMTI_loadAgentLibraryGeneric_agentAttachedSuccessfully(agentLibrary->nativeLib.name);

#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	validateLibrary(vm, &agentLibrary->nativeLib);
#endif

	/* Add the library to the linked list */
	issueWriteBarrier();
	omrthread_monitor_enter(jvmtiData->mutex);
	if (jvmtiData->agentLibrariesTail == NULL) {
		jvmtiData->agentLibrariesHead = jvmtiData->agentLibrariesTail = nativeLib;
	} else {
		jvmtiData->agentLibrariesTail->next = nativeLib;
		jvmtiData->agentLibrariesTail = nativeLib;
	}
	omrthread_monitor_exit(jvmtiData->mutex);
	Trc_JVMTI_loadAgentLibraryGeneric_Exit(agentLibrary->nativeLib.name);

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
	UDATA rc;	
	UDATA optionsLength = 0;
	UDATA libraryLength;
	J9JVMTIAgentLibrary *agentLibrary = NULL;
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(vm);
	char loadFunctionName[J9JVMTI_BUFFER_LENGTH + 1] = {0};
	UDATA loadFunctionNameLength = 0;
	BOOLEAN found = FALSE;
	const char *errorMessage = NULL;

	Trc_JVMTI_loadAgentLibraryOnAttach_Entry(library);

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
		if (J2SE_VERSION(vm) >= J2SE_18) {
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
		}
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

	Trc_JVMTI_loadAgentLibrary_Entry(agentLibrary->nativeLib.name);

	/* For java 1.8 and above attempt linking the agent statically, looking for Agent_OnLoad_L.  
	 * If this is not found, fall back on the regular, dynamic linking way.
	 */
	if (J2SE_VERSION(vm) >= J2SE_18) {
		char nameBuffer[J9JVMTI_BUFFER_LENGTH + 1] = {0};
		UDATA nameBufferLengh = 0;
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
	}
 
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
 * @param libraryAndOptions library name and options concatenated
 * @param libraryLength library name substring length
 * @param options option substring start
 * @param optionsLength option
 * @param decorate change the library path/name
 * @param result pointer to return new J9JVMTIAgentLibrary by reference
 * @return JNI_ERR, JNI_ENOMEM, or JNI_OK
 */
static jint
createAgentLibrary(J9JavaVM * vm, const  char *libraryAndOptions, UDATA libraryLength, const char* options, UDATA optionsLength, UDATA decorate, J9JVMTIAgentLibrary **result)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(vm);
	J9JVMTIAgentLibrary * agentLibrary = NULL;

	/* Create a new agent library */
	omrthread_monitor_enter(jvmtiData->mutex);
	agentLibrary = pool_newElement(jvmtiData->agentLibraries);
	if (NULL == agentLibrary) {
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_JVMTI_OUT_OF_MEMORY, libraryAndOptions);
		omrthread_monitor_exit(jvmtiData->mutex);
		return JNI_ENOMEM;
	}

	/* Initialize the structure with default values */
	vm->internalVMFunctions->initializeNativeLibrary(vm, &agentLibrary->nativeLib);

	/* Make a copy of the library name and options */

	agentLibrary->nativeLib.name = j9mem_allocate_memory(libraryLength + 1 + optionsLength + 1, J9MEM_CATEGORY_JVMTI);
	if (NULL == agentLibrary->nativeLib.name) {
		pool_removeElement(jvmtiData->agentLibraries, agentLibrary);
		/* Print NLS message here? */
		omrthread_monitor_exit(jvmtiData->mutex);
		return JNI_ENOMEM;
	}
	strncpy(agentLibrary->nativeLib.name, libraryAndOptions, libraryLength);
	*(agentLibrary->nativeLib.name+libraryLength) = '\0';
	agentLibrary->options = agentLibrary->nativeLib.name+libraryLength+1;
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
	/* Jazz 99339: initialize agentLibrary->invocationJavaVM to NULL when a new library is created */
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

