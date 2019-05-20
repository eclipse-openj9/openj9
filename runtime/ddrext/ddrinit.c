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

#include "ddr.h"
#include "ddrutils.h"
#include "ddrJniRegistration.h"

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#if defined(LINUX) || defined(AIXPPC) || defined(J9ZOS390) || defined(OSX)
#include <unistd.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif

#define JAVA_CLASSPATH_PARAMETER "-Djava.class.path="

static BOOLEAN hasExited = FALSE;

static ddrExtEnv ddrEnv = 
{
	NULL,
	NULL,
	NULL
};

/** Function pointer **/
typedef jint (JNICALL *fpCreateJavaVM) (JavaVM**, void**, void*);



/** 
 * \brief   Return DDR environment if initialized
 * \ingroup 
 * 
 * @param[in] void 
 * @return NULL or previously initialized DDR environment
 * 
 */
ddrExtEnv *
ddrGetEnv()
{
	if (ddrEnv.jvm && ddrEnv.env && ddrEnv.ddrContext) {
		return &ddrEnv;
	}

	return NULL;
}

void
ddrDumpEnv()
{
	ddrExtEnv * env = &ddrEnv;

	printf("ddrEnv->jvm = %p\n", env->jvm);
	printf("ddrEnv->env = %p\n", env->env);
	printf("ddrEnv->ctx = %p\n", env->ddrContext);
}


#if defined(WIN32) || defined(WIN64)
void *
JNU_FindCreateJavaVM(char* vmlibpath)
{
	HINSTANCE hVM = LoadLibrary(vmlibpath);
	if (hVM == NULL) {
		dbgPrint("DDR: failed to LoadLibrary(%s), error = %d\n", vmlibpath, GetLastError());
		dbgPrint("     64bit plugin trying to load a 32bit VM or vice versa?\n");
		return NULL;
	}
	return GetProcAddress(hVM, "JNI_CreateJavaVM");
}
#endif

#if defined(J9ZOS390)
void *
JNU_FindCreateJavaVM(char* vmlibpath)
{
	dllhandle* libVM = dllload(vmlibpath);
	if (libVM == NULL) {
		return NULL;
	}
	return (void*) dllqueryfn(libVM, "JNI_CreateJavaVM");
}
#endif

#if defined(LINUX) || defined(AIXPPC) || defined(OSX)
void *
JNU_FindCreateJavaVM(char *vmlibpath)
{
	/* TODO: add dlerror */
	void *libVM = dlopen(vmlibpath, RTLD_LAZY);
	if (libVM == NULL) {
		return NULL;
	}
	return dlsym(libVM, "JNI_CreateJavaVM");
}
#endif

/** 
 * \brief   Return default classpath required to run DDR  
 * \ingroup 
 * 
 * @return default ddr classpath string
 * 
 */
char *
ddrFindDefaultClasspath()
{
#if defined(LINUX)
#if defined(J9X86) || defined(J9HAMMER) || defined(LINUXPPC) || defined(LINUXPPC64) || defined(J9ARM) || defined(J9AARCH64)
	return "/bluebird/tools/ddr/j9ddr.jar:/bluebird/tools/ddr/asm.jar";
#endif
#if defined(S390) || defined(S39064)
	return "/j9vm/ascii/tools/ddr/j9ddr.jar:/j9vm/ascii/tools/ddr/asm.jar";
#endif
#endif  /* LINUX */

#if defined(WIN32) || defined(J9HAMMER)
	return "l:\\tools\\ddr\\j9ddr.jar;l:\\tools\\ddr\\asm.jar";
#endif

#if defined(J9ZOS390)
	return "/j9vm/ebcdic/tools/ddr/j9ddr.jar:/j9vm/ebcdic/tools/ddr/asm.jar";
#endif 

#if defined(AIXPPC)
	return "/bluebird/tools/ddr/j9ddr.jar:/bluebird/tools/ddr/asm.jar";
#endif
}

#define MAX_PATH_LEN 1024

jboolean
ddrValidatePath(char * path)
{
#if defined(WIN32)
	DWORD result;
	wchar_t unicodeBuffer[MAX_PATH_LEN + 1];
#endif

	if (path == NULL) {
		return JNI_FALSE;
	}

#if defined(WIN32)
	if (strlen(path) >= MAX_PATH_LEN) {
		dbgPrint("DDR: invalid path - [%s] %d, longer then max of %d\n", path, strlen(path), MAX_PATH_LEN);
		return -1;
	}
	/* Convert the filename from UTF8 to Unicode */
	if(MultiByteToWideChar(OS_ENCODING_CODE_PAGE, OS_ENCODING_MB_FLAGS, path, -1, unicodeBuffer, MAX_PATH_LEN) == 0) {
		dbgPrint("DDR: unable to convert path [%s] to unicode\n", path);
		return -1;
	}
	result = GetFileAttributesW(unicodeBuffer);
	if (result == 0xFFFFFFFF) {
		return JNI_FALSE;
	} else {
		return JNI_TRUE;
	}
#endif
#if defined(LINUX) || defined(AIXPPC)
	struct stat statbuf;

	if (stat(path, &statbuf)) {
		return JNI_FALSE;
	} else {
		return JNI_TRUE;
	}
#endif	
	return JNI_FALSE;
}

jboolean
ddrValidateClassPath(char * path)
{
#if defined(WIN32)
	const char * delimiter = ";";
#else
	const char * delimiter = ":";
#endif
	char * cp, *cp_base;
	UDATA cp_len;

	cp_len = strlen(path);
	if (cp_len == 0) {
		return JNI_FALSE;
	}

	cp = cp_base = malloc(cp_len + 1);
	if (cp == NULL) {
		dbgPrint("DDR: unable to malloc %d bytes used for classpath validation\n", cp_len);
		return JNI_FALSE;
	}
	strncpy(cp_base, path, cp_len + 1);

	cp = strtok(cp , delimiter);
	while (cp != NULL) {
		if (ddrValidatePath(cp) == JNI_FALSE) {
			dbgPrint("DDR: invalid classpath component [%s]\n", cp);
			free(cp_base);
			return JNI_FALSE;
		}
		cp = strtok (NULL, delimiter);
	} 

	free(cp_base);
	return JNI_TRUE;
}

void
ddrStop(ddrExtEnv * env)
{
	JavaVM * jvm = env->jvm;

	if (jvm != NULL) {
		(*jvm)->DestroyJavaVM(jvm);
		env->jvm = NULL;
		hasExited = TRUE;
	}

}


static int 
aliasCommandNames(char ** names)
{
	int i = 0;
    char * name;

	if (dbgAliasInitialize()) {
		return -1;   	
	}

	name = names[i++];
	while (name) {
		if (strcmp(name,"quit") != STRING_MATCHED) {
			dbgAliasCommand(name);
		}
		name = names[i++];
	}

	return 0;
}

static char **
retrieveCommandNames(ddrExtEnv * ddrenv)
{
	char ** names;
	jarray commandNames;
	jsize commandNamesCount;
	jmethodID mid;
	jclass cls;
	jsize i;

	cls = (*ddrenv->env)->FindClass(ddrenv->env, "com/ibm/j9ddr/tools/ddrinteractive/DDRInteractive");
	if (cls == NULL) {
		dbgWriteString("DDREXT: failed to find class com.ibm.j9ddr.tools.ddrinteractive.DDRInteractive\n");
		return JNI_FALSE;
	}

	mid = (*ddrenv->env)->GetMethodID(ddrenv->env, cls, "getCommandNames", "()[Ljava/lang/Object;");
	if (mid == NULL) {
		dbgWriteString("DDREXT: failed to find getCommandNames method in com.ibm.j9ddr.tools.ddrinteractive.DDRInteractive\n");
		return JNI_FALSE;
	}

	commandNames = (*ddrenv->env)->CallObjectMethod(ddrenv->env, ddrenv->ddrContext, mid);
	if ((*ddrenv->env)->ExceptionCheck(ddrenv->env)) {
		dbgWriteString("DDREXT: exception in com.ibm.j9ddr.tools.ddrinteractive.DDRInteractive.getCommandNames()\n");
		(*ddrenv->env)->ExceptionDescribe(ddrenv->env);
		return JNI_FALSE;
	}
	if (commandNames == NULL) {
		dbgWriteString("DDREXT: query for command names returned NULL\n");
		return JNI_FALSE;
	}

	commandNamesCount = (*ddrenv->env)->GetArrayLength(ddrenv->env, commandNames);
	if (commandNamesCount == 0) {
		dbgWriteString("DDREXT: query for count of command names returned 0\n");
		return JNI_FALSE;
	}

	names = malloc(sizeof(char *) * (commandNamesCount + 1));
	memset(names, 0x00, sizeof(char *) * (commandNamesCount + 1));

	for (i = 0; i < commandNamesCount; i++) {
		jstring commandName;
		const char * name;

		commandName = (jstring) (*ddrenv->env)->GetObjectArrayElement(ddrenv->env, commandNames, i);
		if (commandName == NULL) {
			dbgWrite("DDREXT: retrieved an unexpected null command name at idx %d\n", i);
			goto failed;	
		}

		name = (*ddrenv->env)->GetStringUTFChars(ddrenv->env, commandName, 0);

		/*printf("got name: [%s]\n", name);*/

#ifdef WIN32
		names[i] = _strdup(name);
#else
		names[i] = strdup(name);
#endif
		(*ddrenv->env)->ReleaseStringUTFChars(ddrenv->env, commandName, name);

	}

	names[commandNamesCount] = NULL;

	return names;

failed:

	if (names) {
		for (i = 0; i < commandNamesCount; i++) {
			if (names[i]) {
				free(names[i]);
			}
		}
		free(names);
	}

	return NULL;
}


/**
 * Start VM and start DDR thread on it.
 */
JavaVM * 
ddrStart()
{
	fpCreateJavaVM createJVM; /* pointer to JNI_CreateJavaVM function */

	jint rc;
	JavaVMInitArgs vm_args;
	JavaVM * jvm = NULL;
	JNIEnv * env = NULL;
	jobject ddrContext = NULL;

	jclass cls;
	jmethodID mid;
	JavaVMOption *options;

	char* ddr_vm = 0;
	char* ddr_classpath  = 0;
	char* separator = 0;
	char *classpath = 0; /** user provided classpath **/
	char *vm_options = 0; /** user provided VM options **/

	char *option = 0; /** user provided classpath **/
	const char delimiters[] = " ";


	unsigned int optionCount = 1; /** classpath is always provided **/
	unsigned int i = 0;
	size_t classpathStringLength;

	jboolean default_vm = JNI_FALSE;
	jboolean default_cp = JNI_FALSE;

	/* starting DDR again once it has been exited results in errors*/
	if(ddrHasExited()) {
		dbgPrint("DDR: DDR has exited, close GDB and start it again\n");
		return NULL;
	}

	/* J9DDR_VM env var takes precedence over the sniffed location of the DDR VM on the network */

	ddr_vm = getenv("J9DDR_VM");
	if (ddr_vm) {
		if (ddrValidatePath(ddr_vm) == JNI_FALSE) {
			dbgPrint("DDR: J9DDR_VM set to invalid libjvm.so [%s]\n", ddr_vm);
			goto failed_detection;
		}
	}
	if (ddr_vm == NULL) {
		goto failed_detection;
	}

	/* Get the vm options to pass to the DDR VM */

	vm_options = getenv("J9DDR_VMOPTIONS");
	if (vm_options) {
		dbgPrint("DDR: Using specified J9DDR_VMOPTIONS\n");
	}


	/* J9DDR_CLASSPATH env var takes precedence over the sniffed / default class path */

	classpath = getenv("J9DDR_CLASSPATH");
	if (classpath == NULL) {
		classpath = ddrFindDefaultClasspath();
		if (ddrValidateClassPath(classpath) == JNI_FALSE) {
			dbgPrint("DDR: Invalid default ddr classpath [%s]\n", classpath);
			goto failed_detection;
		} else {
			default_cp = JNI_TRUE;
		}
	} else {
		if (ddrValidateClassPath(classpath) == JNI_FALSE) {
			dbgPrint("DDR: Invalid env var J9DDR_CLASSPATH classpath [%s]\n", classpath);
			goto failed_detection;
		}
	}
	
	dbgPrint("DDR: J9DDR_VM        set to: [%s] %s\n", ddr_vm, (default_vm == JNI_TRUE) ? "nfs default" : "from env var" );
	dbgPrint("DDR: J9DDR_CLASSPATH set to: [%s] %s\n", classpath, (default_cp == JNI_TRUE) ? "nfs default" : "from env var" );
	dbgPrint("DDR: J9DDR_VMOPTIONS set to: [%s]\n", vm_options);


	classpathStringLength = strlen(JAVA_CLASSPATH_PARAMETER) + strlen(classpath) + 1;
	ddr_classpath = malloc(classpathStringLength);

	strcpy(ddr_classpath, JAVA_CLASSPATH_PARAMETER);
	strcat(ddr_classpath, classpath);

	if (vm_options != NULL) {
		/* Allocate a copy of vm_options, strtok is destructive */
		char *vm_options_copy = malloc(strlen(vm_options) + 1);

		/* Bail if allocation failed.  If we couldn't get the requested */
		/* vm options, don't bother starting at all */
		if (NULL == vm_options_copy) {
			dbgPrint("DDR: failed allocate memory for J9DDR_VMOPTIONS\n");
			goto destroy;
		}

		strcpy(vm_options_copy, vm_options);

		option = strtok(vm_options_copy, delimiters);
		while (option != NULL)
		{
			optionCount++;
			option = strtok (NULL, delimiters);
		}
		
		free(vm_options_copy);
	}

	options = (JavaVMOption*) malloc(sizeof(JavaVMOption) * optionCount);
	if (NULL == options) {
		dbgPrint("DDR: failed allocate memory for J9DDR_VMOPTIONS\n");
		goto destroy;
	}
	memset(options, 0, sizeof(JavaVMOption) * optionCount);


	/** specify all the classes & JAR files that need to be in the CLASSPATH **/
	options[0].optionString	= ddr_classpath;

	/** set VM properties **/
	if (vm_options != NULL) {
		i = 1;
		option = strtok(vm_options , delimiters);
		while (option != NULL)
		{
			options[i].optionString = option;
			i++;
			option = strtok (NULL, delimiters);
		}
	}

	vm_args.version = JNI_VERSION_1_6;
	vm_args.nOptions = optionCount;
	vm_args.options = options;
	vm_args.ignoreUnrecognized = JNI_FALSE;

	createJVM = (fpCreateJavaVM) JNU_FindCreateJavaVM(ddr_vm);
	if (createJVM == NULL) {
		dbgPrint("DDR: failed to obtain JNI_CreateJavaVM ptr\n");
		goto destroy;
	}

	/** Load and initialize a virtual machine. Start DDR on main thread **/
	if ((rc = (*createJVM)(&jvm, (void**) &env, &vm_args)) != 0) {
		dbgPrint("DDR: Failed to create JavaVM. rc = %lx %d\n", rc, rc);
		goto destroy;
	} 
	
	if ((*jvm)->AttachCurrentThread(jvm, (void **) &env, NULL) != 0) {
		dbgWriteString("DDR: Failed to attach the VM to current thread\n");
	}


	if (register_ddr_natives(env) == FALSE) {
		dbgWriteString("DDR: failed to register natives with JVM\n");
		goto destroy;
	}

	/** initialize DDR instance **/
	cls = (*env)->FindClass(env, "com/ibm/j9ddr/tools/ddrinteractive/DDRInteractive");
	if (cls == NULL) {
		dbgWriteString("DDR: failed to find class com.ibm.j9ddr.tools.ddrinteractive.DDRInteractive\n");
		goto destroy;
	}

	mid = (*env)->GetStaticMethodID(env, cls, "instantiateDDR", "()Lcom/ibm/j9ddr/tools/ddrinteractive/DDRInteractive;");
	if (mid == NULL) {
		dbgWriteString("DDR: failed to find main method\n");
		goto destroy;
	}

	ddrContext = (*env)->CallStaticObjectMethod(env, cls, mid);
	if ((*env)->ExceptionCheck(env) || (ddrContext == NULL)) {
		dbgWriteString("DDR: failed to instantiate com/ibm/j9ddr/tools/ddrinteractive/DDRInteractive\n");
		goto destroy;
	}

	ddrEnv.env = env;
	ddrEnv.jvm = jvm;
	ddrEnv.ddrContext = ddrContext;

	{
		char ** commandNames = retrieveCommandNames(&ddrEnv);
		aliasCommandNames(commandNames);
	}

	/**  everything ok. Give control back to the debugger **/
	ddrDumpEnv();

	dbgWriteString("J9 DDR plugin loaded\n");
	dbgWriteString("Type 'j9help' for usage information\n");

	return jvm;

destroy:

	if (env != NULL && (*env)->ExceptionOccurred(env)) {
		(*env)->ExceptionDescribe(env);
	}

	if (jvm != NULL) {
		(*jvm)->DestroyJavaVM(jvm);
	}

	return NULL;
	
failed_detection:

	dbgPrint("DDR: Boiler plate help\n");
	dbgPrint("DDR: Quite likely running on a machine that does not have /bluebird l:\\ (QV) or /j9vm (Torolab) mounted\n");
	dbgPrint("DDR: Set following environment variables to point at a 6.0 JRE and j9ddr.jar / asm.jar\n");  
	dbgPrint("DDR: export J9DDR_VM=/some/path/jre/bin/j9vm/libjvm.so\n");
	dbgPrint("DDR: export J9DDR_CLASSPATH=/workspace/debugtools/DDR_Artifacts/libs/j9ddr.jar:/workspace/debugtools/DDR_VM/lib/asm-3.1.jar\n");
	dbgPrint("DDR: export J9DDR_VMOPTIONS=some_opts_if_required_else_skipped\n");
	
	return NULL;
}

BOOLEAN
ddrHasExited()
{
	return hasExited;
}

#ifdef __cplusplus
}
#endif
