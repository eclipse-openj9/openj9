/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

/*------------------------------------------------------------------
 * libargs. : handle VM init args
 *------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jni.h>
#include "j9.h"
#include "j9port.h"
#include "j9lib.h"
#include "j9exelibnls.h"
#include "exelib_internal.h"
#include "util_api.h"

/*------------------------------------------------------------------
 *
 *------------------------------------------------------------------*/
#if defined(LOGGING)
#define LOG(x) printf x
#else
#define LOG(x)
#endif
/*------------------------------------------------------------------
 *
 *------------------------------------------------------------------*/

typedef struct J9LinkedString {
	struct J9LinkedString *next;
	char data[1];
} J9LinkedString;

typedef struct {
	I_32 size;
	I_32 count;
	JavaVMOption *vmOptions;
	J9PortLibrary *portLib;
	J9LinkedString *strings;
} J9VMOptionsTable;

#define J9CMD_EXACT_MATCH 0
#define J9CMD_PARTIAL_MATCH 1
typedef struct J9CmdLineAction {
	char *cmdLineName;
	UDATA partialMatch;
	char *internalName;
	I_32 (*actionFunction)(J9PortLibrary *portLib, int *argc, char *argv[], void **vmOptionsTable, struct J9CmdLineAction *);
} J9CmdLineAction;

I_32 vmOptionsTableAddExeName(void **vmOptionsTable, char *argv0);
int vmOptionsTableGetCount(void **vmOptionsTable);
I_32 vmOptionsTableInit(J9PortLibrary *portLib, void **vmOptionsTable, int initialCount);
I_32 vmOptionsTableAddOptionWithCopy(void **vmOptionsTable, char *optionString, void *extraInfo);
I_32 vmOptionsTableAddOption(void **vmOptionsTable, char *optionString, void *extraInfo);
JavaVMOption *vmOptionsTableGetOptions(void **vmOptionsTable);
void vmOptionsTableDestroy(void **vmOptionsTable);

static char *allocString(J9VMOptionsTable *table, UDATA size);
static I_32 cmdline_get_jcl(J9PortLibrary *portLib, int *argc, char *argv[], void **vmOptionsTable, J9CmdLineAction *action);
static I_32 copyTable(J9PortLibrary *portLib, J9VMOptionsTable *table, J9VMOptionsTable *oldTable);
static I_32 buildNewTable(J9PortLibrary *portLib, int initialCount, J9VMOptionsTable *oldTable, J9VMOptionsTable **newTable);

#define NO_DEFAULT_VALUE 0
#define PRIVATE_OPTION NULL

/*------------------------------------------------------------------
 *
 *------------------------------------------------------------------*/

#if defined(J9VM_INTERP_VERBOSE)
static char *verboseOptions[] = {
	"\n  -verbose[:{class",
	"|gcterse",
	"|gc",
#ifdef J9VM_OPT_DYNAMIC_LOAD_SUPPORT
	"|dynload",
#endif
	"|sizes",
	"|stack|debug}]\n"
};
#endif /* J9VM_INTERP_VERBOSE */

/*------------------------------------------------------------------
 * create a new table with specified size
 *------------------------------------------------------------------*/
I_32
vmOptionsTableInit(J9PortLibrary *portLib, void **vmOptionsTable, int initialCount)
{
	return buildNewTable(portLib, initialCount, NULL, (J9VMOptionsTable **)vmOptionsTable);
}

/*------------------------------------------------------------------
 * destroy the table
 *------------------------------------------------------------------*/
void
vmOptionsTableDestroy(void **vmOptionsTable)
{
	J9VMOptionsTable *table = *vmOptionsTable;
	J9PortLibrary *portLib = table->portLib;
	J9LinkedString *this = table->strings;
	PORT_ACCESS_FROM_PORT(portLib);

	/* free the linked list of strings */
	while (NULL != this) {
		J9LinkedString *next = this->next;
		j9mem_free_memory(this);
		this = next;
	}

	j9mem_free_memory(table);
}

/*------------------------------------------------------------------
 * get the number of options in the table
 *------------------------------------------------------------------*/
int
vmOptionsTableGetCount(void **vmOptionsTable)
{
	J9VMOptionsTable *table = *vmOptionsTable;

	return table->count;
}

/*------------------------------------------------------------------
 *
 *------------------------------------------------------------------*/
JavaVMOption *
vmOptionsTableGetOptions(void **vmOptionsTable)
{
	J9VMOptionsTable *table = *vmOptionsTable;

	return table->vmOptions;
}

/*------------------------------------------------------------------
 *
 *------------------------------------------------------------------*/
I_32
vmOptionsTableAddOption(void **vmOptionsTable, char *optionString, void *extraInfo)
{
	J9VMOptionsTable *table = *vmOptionsTable;
	J9VMOptionsTable *newTable = NULL;

	if (table->count == table->size) {
		I_32 returnCode = buildNewTable(table->portLib, table->size * 2, table, &newTable);
		if (J9CMDLINE_OK != returnCode) {
			return returnCode;
		}

		vmOptionsTableDestroy(vmOptionsTable);
		table = newTable;
		*vmOptionsTable = table;
	}

	table->vmOptions[table->count].optionString = optionString;
	table->vmOptions[table->count].extraInfo = extraInfo;

	table->count++;
	return J9CMDLINE_OK;
}

/*------------------------------------------------------------------
 *
 *------------------------------------------------------------------*/
I_32
vmOptionsTableAddExeName(void **vmOptionsTable, char *argv0)
{
	J9VMOptionsTable *table = *vmOptionsTable;
	PORT_ACCESS_FROM_PORT(table->portLib);
	char *value = NULL;
	char *define = NULL;
#define EXE_PROPERTY "-Dcom.ibm.oti.vm.exe="

	if (j9sysinfo_get_executable_name(argv0, &value)) {
		return J9CMDLINE_GENERIC_ERROR;
	}

	define = allocString(*vmOptionsTable, sizeof(EXE_PROPERTY) + strlen(value)); /* sizeof() includes room for the terminating \0 */
	if (NULL == define) {
		j9mem_free_memory(value);
		return J9CMDLINE_OUT_OF_MEMORY;
	}

	strcpy(define, EXE_PROPERTY);
	strcat(define, value);
	/* Do /not/ delete executable name string; system-owned. */
	return vmOptionsTableAddOption(vmOptionsTable, define, NULL);
}

static char *
allocString(J9VMOptionsTable *table, UDATA size)
{
	PORT_ACCESS_FROM_PORT(table->portLib);
	J9LinkedString *node = j9mem_allocate_memory(size + sizeof(J9LinkedString *), OMRMEM_CATEGORY_VM);

	if (NULL == node) {
		return NULL;
	}

	node->next = table->strings;
	table->strings = node;

	return node->data;
}

/*------------------------------------------------------------------
 * create a new table, copying from old if valid
 *------------------------------------------------------------------*/
static I_32
buildNewTable(J9PortLibrary *portLib, int initialCount, J9VMOptionsTable *oldTable, J9VMOptionsTable **newTable)
{
	J9VMOptionsTable *table = NULL;
	int mallocSize = sizeof(J9VMOptionsTable) + (sizeof(JavaVMOption) * initialCount);
	I_32 returnCode = 0;

	PORT_ACCESS_FROM_PORT(portLib);

	LOG(("buildNewTable(%d,%p)\n", initialCount, oldTable));
	LOG(("buildNewtable(): malloc size = %d\n", mallocSize));

	table = j9mem_allocate_memory(mallocSize, OMRMEM_CATEGORY_VM);
	if (NULL == table) {
		return J9CMDLINE_OUT_OF_MEMORY;
	}
	*newTable = table;

	table->size = initialCount;
	table->count = 0;
	table->portLib = portLib;
	table->vmOptions = (JavaVMOption *) (((char *) table) + sizeof(J9VMOptionsTable));
	table->strings = NULL;

	LOG(("buildNewtable(): table:   %#010X\n",table));
	LOG(("buildNewtable(): options: %#010X\n",table->vmOptions));

	if (NULL == oldTable) {
		return J9CMDLINE_OK;
	}

	LOG(("buildNewtable(): copying table\n"));
	returnCode = copyTable(table->portLib, table, oldTable);
	if (J9CMDLINE_OK != returnCode) {
		return returnCode;
	}

	LOG(("buildNewtable(): table->size = %d\n",table->size));
	return J9CMDLINE_OK;
}

/*------------------------------------------------------------------
 * create a new table, copying from old if valid
 *------------------------------------------------------------------*/
static I_32
copyTable(J9PortLibrary *portLib, J9VMOptionsTable *table, J9VMOptionsTable *oldTable)
{
	int i = 0;

	LOG(("copyTable(%p, %p, %d)\n", table, oldTable, copyCount));

	table->strings = oldTable->strings;
	oldTable->strings = NULL;
	table->count = oldTable->count;

	for (i = 0; i < oldTable->count; i++) {
		table->vmOptions[i].optionString = oldTable->vmOptions[i].optionString;
		table->vmOptions[i].extraInfo = oldTable->vmOptions[i].extraInfo;
	}

	LOG(("copyTable(): table->count = %d\n",table->count));

	return J9CMDLINE_OK;
}

/*------------------------------------------------------------------
 *
 *------------------------------------------------------------------*/
I_32
vmOptionsTableAddOptionWithCopy(void **vmOptionsTable, char *optionString, void *extraInfo)
{
	char *copyOptionString = allocString(*vmOptionsTable, strlen(optionString) + 1);

	if (NULL == copyOptionString) {
		return J9CMDLINE_OUT_OF_MEMORY;
	}
	strcpy(copyOptionString, optionString);
	return vmOptionsTableAddOption(vmOptionsTable, copyOptionString, extraInfo);
}

#if defined(WIN32)
  /* On Windows, the subdirectory containing the redirector hasn't changed with Java versions. */
  #define J9JAVA_REDIRECTOR_SUBDIR "\\bin\\j9vm\\"
#elif (JAVA_SPEC_VERSION >= 10) || ((JAVA_SPEC_VERSION == 9) && defined(OPENJ9_BUILD)) || defined(OSX)
  /* On other platforms, the subdirectory containing the redirector is common in recent Java versions. */
  #define J9JAVA_REDIRECTOR_SUBDIR "/lib/j9vm/"
#else /* WIN32 */
  /* In Java 8 and early versions of Java 9, the path depends on the platform architecture. */
  #if defined(J9HAMMER)
	#define JVM_ARCH_DIR "amd64"
  #elif defined(J9ARM)
	#define JVM_ARCH_DIR "arm"
  #elif defined(J9X86)
	#define JVM_ARCH_DIR "i386"
  #elif defined(S39064) || defined(J9ZOS39064)
	#define JVM_ARCH_DIR "s390x"
  #elif defined(S390) || defined(J9ZOS390)
	#define JVM_ARCH_DIR "s390"
  #elif defined(RS6000) || defined(LINUXPPC)
	#if !defined(PPC64)
	  #define JVM_ARCH_DIR "ppc"
	#elif defined(J9VM_ENV_LITTLE_ENDIAN)
	  #define JVM_ARCH_DIR "ppc64le"
	#else /* J9VM_ENV_LITTLE_ENDIAN */
	  #define JVM_ARCH_DIR "ppc64"
	#endif /* PPC64 */
  #else
	#error "Must define an architecture"
  #endif
  #define J9JAVA_REDIRECTOR_SUBDIR "/lib/" JVM_ARCH_DIR "/j9vm/"
#endif /* WIN32 */

#define JAVAHOMEDIR "-Djava.home="
#define JAVAHOMEDIR_LEN (sizeof(JAVAHOMEDIR) - 1)

/**
 * This function scans for -Djava.home=xxx/jre. If found, it uses it to construct a path
 * to redirector jvm.dll and put it in the passed in char buffer:
 *   --> [java.home]/bin/j9vm/
 * This function is meant to be used by thrstatetest and shrtest so the test can be invoked
 * in a sdk shape independent way.
 */
BOOLEAN
cmdline_fetchRedirectorDllDir(struct j9cmdlineOptions *args, char *result)
{
	char *tmpresult = NULL;
	struct j9cmdlineOptions *arg = args;
	int argc = arg->argc;
	char **argv = arg->argv;
	int i = 0;

	/* Find the last -Djava.home= in the args */
	for (i = 0; i < argc; i++) {
		char *javaHomeDir = strstr(argv[i], JAVAHOMEDIR);
		if (NULL != javaHomeDir) {
			tmpresult = javaHomeDir + JAVAHOMEDIR_LEN;
		}
	}

	if (NULL == tmpresult) {
		printf("ERROR: -Djava.home was not specified on command line.\n");
		return FALSE;
	}

	strcpy(result, tmpresult);
	strcat(result, J9JAVA_REDIRECTOR_SUBDIR);

	return TRUE;
}
