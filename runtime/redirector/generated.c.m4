changequote(`[',`]')dnl
/*******************************************************************************
 * Copyright IBM Corp. and others 2001
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
/* generated.c */
#if defined(WIN32)
#include <windows.h>
#include <tchar.h>
#include <io.h>
#endif /* defined(WIN32) */

#if defined(AIXPPC) || defined(J9ZOS390) || defined(LINUX) || defined(OSX)
#include <dlfcn.h>
#endif /* defined(AIXPPC) || defined(J9ZOS390) || defined(LINUX) || defined(OSX) */

#include "j9.h"
#include "jni.h"

/* Avoid renaming malloc/free. */
#define LAUNCHERS
#include "jvm.h"

#if defined(J9ZOS390)
#include <dll.h>
#include "atoe.h"
#include <stdlib.h>
#define dlsym   dllqueryfn
#define dlopen(a,b)     dllload(a)
#define dlclose dllfree
#endif /* defined(J9ZOS390) */

include(helpers.m4)

/* Manual typedefs for functions that can't be generated easily. */
typedef int (*jio_fprintf_Type)(FILE *stream, const char *format, ...);
typedef int (*jio_snprintf_Type)(char *str, int n, const char *format, ...);
typedef void (JNICALL *JVM_OnExit_Type)(void (*func)(void));

/* Generated typedefs for all forwarded functions. */
dnl (1-name, 2-cc, 3-decorate, 4-ret, 5-args...)
define([_X],
[typedef $4 ($2 *$1_Type)(join([, ],mshift(4,$@)));])dnl
include([forwarders.m4])dnl
typedef void *(JNICALL *JVM_LoadSystemLibrary_Type)(const char *libName);

/* Manually declared functions for non-generated forwarders. */
static JVM_OnExit_Type global_JVM_OnExit;

/* Declare a static variable to hold each dynamically resolved function pointer. */
dnl (1-name, 2-cc, 3-decorate, 4-ret, 5-args...)
define([_X],[static $1_Type global_$1;])
include([forwarders.m4])dnl

static volatile JVM_LoadSystemLibrary_Type global_JVM_LoadSystemLibrary;

#if defined(AIXPPC)
static int table_initialized = 0;

/* defined in redirector.c */
int openLibraries(const char *libraryDir);
#endif /* defined(AIXPPC) */

int
jio_fprintf(FILE *stream, const char *format, ...)
{
	va_list args;
	int response = 0;
	va_start(args, format);
	response = jio_vfprintf(stream, format, args);
	va_end(args);
	return response;
}

int
jio_snprintf(char *str, int n, const char *format, ...)
{
	va_list args;
	int response = 0;
	va_start(args, format);
	response = jio_vsnprintf(str, n, format, args);
	va_end(args);
	return response;
}

void JNICALL
JVM_OnExit(void (*func)(void))
{
	if (NULL != global_JVM_OnExit) {
		global_JVM_OnExit(func);
	} else {
		exit(999);
	}
}

dnl (1-name, 2-cc, 3-decorate, 4-ret, 5-args...)
define([_X],
[dnl
$4 $2
$1(join([, ],mshift(4,$@)))
{
	while (NULL == global_$1) {
#if defined(AIXPPC)
		if (!table_initialized) {
			/* attempt to open the 'main redirector' so we can try again */
			int openedLibraries = openLibraries("");
			if (JNI_ERR == openedLibraries) {
				fprintf(stdout, "Internal Error: Failed to initialize redirector - exiting\n");
				exit(998);
			}
		} else
#endif /* defined(AIXPPC) */
		{
			fprintf(stdout, "Fatal Error: Missing forwarder for %s[]()\n", "$1");
			exit(969);
		}
	}
	invokePrefix($4)[]global_$1(arg_names_list(mshift(4,$@)));
}
])dnl
include([forwarders.m4])dnl

static void *
functionLookup(void *vmdll, const char *functionName)
{
#if defined(WIN32)
	return GetProcAddress(vmdll, functionName);
#else /* defined(WIN32) */
	return (void *)dlsym(vmdll, functionName);
#endif /* defined(WIN32) */
}

#if defined(WIN32) && !defined(J9VM_ENV_DATA64)
	/*
	 * Use Microsoft-style C name mangling for stdcall functions;
	 * see https://docs.microsoft.com/en-us/cpp/build/reference/decorated-names#FormatC.
	 */
	#define DECORATED_NAME(name, arg_size) "_" #name "@" #arg_size
#else /* defined(WIN32) && !defined(J9VM_ENV_DATA64) */
	/* No name mangling required. */
	#define DECORATED_NAME(name, arg_size) #name
#endif /* defined(WIN32) && !defined(J9VM_ENV_DATA64) */

void
lookupJVMFunctions(void *vmdll)
{
	global_JVM_OnExit = (JVM_OnExit_Type)functionLookup(vmdll, DECORATED_NAME(JVM_OnExit, 4));
dnl (1-name, 2-cc, 3-decorate, 4-ret, 5-args...)
define([_X],[	global_$1 = ($1_Type)functionLookup(vmdll, decorate_function_name($@));])dnl
include([forwarders.m4])dnl
	global_JVM_LoadSystemLibrary = (JVM_LoadSystemLibrary_Type)functionLookup(vmdll, DECORATED_NAME(JVM_LoadSystemLibrary, 4));
#if defined(AIXPPC)
	table_initialized = 1;
#endif /* defined(AIXPPC) */
}

void * JNICALL
JVM_LoadSystemLibrary(const char *libName)
{
	int count = 0;
#if defined(AIXPPC)
retry:
#endif /* defined(AIXPPC) */
	while ((NULL == global_JVM_LoadSystemLibrary) && (count < 6000)) {
#if defined(WIN32)
		Sleep(5); /* 5 milliseconds */
#else /* defined(WIN32) */
		usleep(5000); /* 5 milliseconds */
#endif /* defined(WIN32) */
		count += 1;
	}
	if (NULL != global_JVM_LoadSystemLibrary) {
		return global_JVM_LoadSystemLibrary(libName);
#if defined(AIXPPC)
	} else if (!table_initialized) {
		/* attempt to open the 'main redirector' so we can try again */
		int openedLibraries = openLibraries("");
		if (JNI_ERR == openedLibraries) {
			fprintf(stdout, "Internal Error: Failed to initialize redirector - exiting\n");
			exit(998);
		}
		count = 0;
		goto retry;
#endif /* defined(AIXPPC) */
	} else {
		fprintf(stdout, "Fatal Error: Missing forwarder for %s()\n", "JVM_LoadSystemLibrary");
		exit(969);
	}
}

/*
 * Following method JVM_InitAgentProperties() has been copied from actual JVM dll (implemented within \VM_Sidecar\j9vm\j7vmi.c) to
 * this redirector dll. This is to address the issue identified by "RTC PR 104487: SVT:Eclipse:jdtdebug org.eclipse.core -
 * Fails with Cannot load module libjvm.so file".
 *
 * This PR exposed that the JVM can be quite slow to finish lookupJVMFunctions() and complete the function table initialization such that
 * a separated thread calling JVM_InitAgentProperties can fail with error "Cannot load module  libjvm.so file".
 * Coping this method into redirector allows the call without finishing the function table initialization.
 *
 * The method is still kept within the actual jvm dll in case that a launcher uses that jvm dll directly without going through this redirector.
 * If this method need to be modified, the changes have to be synchronized for both versions.
 *
 * The reason that this method can be copied into here (redirector) is that it doesn't require any other VM function support.
 * As following comments, this method simply returns incoming agent_props to make the agent happy.
 * If there is a need in the future to modify this method and other VM function support are required, this method need to be moved back to JVM dll.
 * In such case, other means have to be developed to ensure this method still accessible in situations identified by PR 104487 mentioned above.
 */

/*
 * com.sun.tools.attach.VirtualMachine support
 *
 * Initialize the agent properties with the properties maintained in the VM.
 * The following properties are apparently set by the reference implementation:
 *  sun.java.command = name of the main class
 *  sun.jvm.flags = vm arguments passed to the launcher
 *  sun.jvm.args =
 */
jobject JNICALL
JVM_InitAgentProperties(JNIEnv *env, jobject agent_props)
{
	/* CMVC 150259 : Assert in JDWP Agent
	 *   Simply returning the non-null properties instance is
	 *   sufficient to make the agent happy.  Do the simple
	 *   thing for now. */
	return agent_props;
}
