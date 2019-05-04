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

#include "jni.h"
#include "vmargs_core_api.h"
#include "j9.h"
#include "zip_api.h"

#ifndef vmargs_api_h
#define vmargs_api_h

/* Constants for use with findArgInVMArgs() */
#define EXACT_MATCH 1
#define STARTSWITH_MATCH 2
#define EXACT_MEMORY_MATCH 3
#define OPTIONAL_LIST_MATCH 4
#define SEARCH_FORWARD 0x1000
#define STOP_AT_INDEX_SHIFT 16
#define REAL_MATCH_MASK 0xFFF

/* Return value constants for use with findArgInVMArgs() */
#define INVALID_OPTION 1
#define EXACT_INVALID_OPTION 3
#define STARTSWITH_INVALID_OPTION 5

#define ARG_ENCODING_DEFAULT 0
#define ARG_ENCODING_PLATFORM 1
#define ARG_ENCODING_UTF 2
#define ARG_ENCODING_LATIN 3

#define ENV_LIBPATH "LIBPATH"

#if defined(OSX)
#define ENV_LD_LIB_PATH "DYLD_LIBRARY_PATH"
#else /* defined(OSX) */
#define ENV_LD_LIB_PATH "LD_LIBRARY_PATH"
#endif /* defined(OSX) */

/**
 * Walks the j9vm_args to find the entry for a given argument. optionValue can be
 * specified to find a specific value for the option, such as -Xverify:none or can be NULL.
 * If doConsumedArgs, the argument found is consumed and all previous identical arguments are
 * marked as NOT_CONSUMABLE_ARG
 ** SEE testFindArgs FOR UNIT TESTS. TO RUN, ADD "#define JVMINIT_UNIT_TEST" TO SOURCE TEMPLATE ***
 */
IDATA
findArgInVMArgs(J9PortLibrary *portLibrary, J9VMInitArgs* j9vm_args, UDATA match, const char* optionName, const char* optionValue, UDATA doConsumeArgs);


/**
 * This function returns the option string at a given index.  It is important
 * to use this function rather than accessing the array directly, otherwise
 * mappings will not work
 * @param j9vm_args
 * @param index
 * @return
 **/
char*
getOptionString(J9VMInitArgs* j9vm_args, IDATA index);


/* Covers the range of operations that can be performed on a given option string.
	GET_OPTION returns the value of a command-line option. Eg "-Xfoo:bar" returns "bar".
	GET_OPTION_OPT returns the value of a value. Eg. "-Xfoo:bar:wibble" returns "wibble".
	GET_OPTIONS returns a list of values separated by NULLs. Eg. "-Xfoo:bar=1,wibble=2" returns "bar=1\0wibble=2"
	GET_COMPOUND returns all the option values from the command-line for a given option, separated by commas. Based on certain rules.
	GET_COMPOUND_OPTS returns all the option values from the command-line for a given option, separated by \0s. Based on certain rules.
	GET_MAPPED_OPTION returns the value of a mapped option. Eg. if -Xsov1 maps to Xj91 then "-Xsov1:foo" returns "foo"
	GET_MEM_VALUE returns a rounded value of a memory option. Eg. "-Xfoo32k" returns (32 * 1024).
	GET_INT_VALUE returns the exact integer value of an option. Eg. -Xfoo5 returns 5
	GET_PRC_VALUE returns the a decimal value between 0 and 1 represented as a percentage. Eg. -Xmaxf1.0 returns 100.

	For GET_OPTIONS, the function expects to get a string buffer to write to and a buffer size.
	For GET_OPTION, GET_OPTION_OPT and GET_MAPPED_OPTION, the result is returned either as a pointer in valuesBuffer or, if valuesBuffer is a real buffer, it copies the result.
	For GET_MEM_VALUE, GET_INT_VALUE and GET_PRC_VALUE the function enters the result in a UDATA provided.

	Unless absolutely necessary, don't call this method directly - use the macros defined in jvminit.h.

	*** SEE testOptionValueOps FOR UNIT TESTS. TO RUN, ADD "#define JVMINIT_UNIT_TEST" TO SOURCE TEMPLATE ***
*/
IDATA
optionValueOperations(J9PortLibrary *portLibrary, J9VMInitArgs* j9vm_args, IDATA element, IDATA action, char** valuesBuffer, UDATA bufSize, char delim, char separator, void* reserved);

void
dumpVmArgumentsList(J9VMInitArgs *argList);

/*
 * Add -Xoptionsfile=<optionsdirectory>/options.default, plus contents of same.
 * This allocates memory for the options string and contents of the file
 * @param portLib port library
 * @param vmArgumentsList current list of arguments
 * @param optionsDirectory directory containing options.default
 * @param verboseFlags set to VERBOSE_INIT for verbosity
 * @return 0 on success, negative value on failure
 */
IDATA
addOptionsDefaultFile(J9PortLibrary * portLib, J9JavaVMArgInfoList *vmArgumentsList, char *optionsDirectory, UDATA verboseFlags);

/*
 * Add implied VM argument -Xjcl:
 * This allocates memory for the options string and adds the argument to the list.
 * @param portLib port library
 * @param vmArgumentsList current list of arguments
 * @param j2seVersion java version number
 * @return 0 on success, negative value on failure
 */
IDATA
addXjcl(J9PortLibrary * portLib, J9JavaVMArgInfoList *vmArgumentsList, UDATA j2seVersion);

/*
 * Add argument to set com.ibm.oti.vm.bootstrap.library.path or sun.boot.library.path
 * This allocates memory for the options string and contents of the file
 * @param portLib port library
 * @param vmArgumentsList current list of arguments
 * @param propertyNameEquals -D<property name>=
 * @param j9binPath path to library directory
 * @param jrebinPath path to library directory
 * @return 0 on success, negative value on failure
 */
IDATA
addBootLibraryPath(J9PortLibrary * portLib, J9JavaVMArgInfoList *vmArgumentsList, char *propertyNameEquals, char *j9binPath, char *jrebinPath);

/**
 * Add argument to set java.library.path
 * This allocates memory for the options string and contents of the file
 * @param portLib port library
 * @param vmArgumentsList current list of arguments
 * @param argEncoding character set encoding (Windows only)
 * @param jvmInSubdir determine position of VM
 * @param j9binPath path to library directory
 * @param jrebinPath path to library directory
 * @param libpathValue original value of LIBPATH
 * @param ldLibraryPathValue original value of LD_LIBRARY_PATH
 * @return 0 on success, negative value on failure
 */
IDATA
addJavaLibraryPath(J9PortLibrary * portLib, J9JavaVMArgInfoList *vmArgumentsList, UDATA argEncoding,
		BOOLEAN jvmInSubdir, char *j9binPath, char *jrebinPath,
		const char *libpathValue, const char *ldLibraryPathValue);

/**
 * Add argument to set java.library.path
 * This allocates memory for the options string and contents of the file
 * @param portLib port library
 * @param vmArgumentsList current list of arguments
 * @param altJavaHomeSpecified true if _ALT_JAVA_HOME_DIR specified (Windows only)
 * @param jrelibPath path to library directory
 * @return 0 on success, negative value on failure
 */
IDATA
addJavaHome(J9PortLibrary *portLib, J9JavaVMArgInfoList *vmArgumentsList, UDATA altJavaHomeSpecified, char *jrelibPath);

/**
 * Add argument to set java.ext.dirs
 * This allocates memory for the options string and contents of the file
 * @param portLib port library
 * @param vmArgumentsList current list of arguments
 * @param jrelibPath path to library directory
 * @param launcherArgs JavaVMInitArgs passed in from the launcher
 * @param j2seVersion java version number
 * @return 0 on success, negative value on failure
 */
IDATA
addExtDir(J9PortLibrary *portLib, J9JavaVMArgInfoList *vmArgumentsList, char *jrelibPath, JavaVMInitArgs *launcherArgs, UDATA j2seVersion);

/**
 * Add argument to set user.dir
 * This allocates memory for the options string and contents of the file
 * @param portLib port library
 * @param vmArgumentsList current list of arguments
 * @param cwd path to current directory
 * @return 0 on success, negative value on failure
 */
IDATA
addUserDir(J9PortLibrary * portLib, J9JavaVMArgInfoList *vmArgumentsList, char *cwd);

#if !defined(OPENJ9_BUILD)
/**
 * Add -D options to define java properties
 * @param portLib port library
 * @param vmArgumentsList current list of arguments
 * @param verboseFlags set to VERBOSE_INIT for verbosity
 * @return 0 on success, negative value on failure
 */
IDATA
addJavaPropertiesOptions(J9PortLibrary * portLib, J9JavaVMArgInfoList *vmArgumentsList, UDATA verboseFlags);
#endif /* !defined(OPENJ9_BUILD) */

/**
 * Open the executable JAR file and add arguments from the manifest file.
 * @param portLib port library
 * @param vmArgumentsList current list of arguments
 * @param jarPath path to jar file.  May be NULL.
 * @param zipFuncs table of zipfile access functions.  May be NULL if jarPath is NULL.
 * @param verboseFlags set to VERBOSE_INIT for verbosity
 * @return 0 on success, negative value on failure
 */
IDATA
addJarArguments(J9PortLibrary * portLib, J9JavaVMArgInfoList *vmArgumentsList, const char *jarPath, J9ZipFunctionTable *zipFuncs, UDATA verboseFlags);

/**
 * Add the arguments coming from environment variables
 * @param portLib port library
 * @param launcherArgs JavaVMInitArgs passed in from the launcher
 * @param vmArgumentsList current list of arguments
 * @param verboseFlags set to VERBOSE_INIT for verbosity
 * @return 0 on success, negative value on failure
 */
IDATA
addEnvironmentVariables(J9PortLibrary * portLib, JavaVMInitArgs *launcherArgs, J9JavaVMArgInfoList *vmArgumentsList, UDATA verboseFlags);

/**
 * Copy the arguments given by the launching process to new memory and add to the list.
 * @param [in] portLib port library
 * @param [in] launcherArgs JavaVMInitArgs passed in from the launcher
 * @param [in] launcherArgumentsSize	amount of memory to hold the launcherArgs
 * @param [in] vmArgumentsList current list of arguments
 * @param [out] xServiceBuffer receives the last (if any) -Xservice argument
 * @param [in] argEncoding Source argument encoding (UTF, Latin, or platform), ignored on non-Windows platforms
 * @param [in] verboseFlags set to VERBOSE_INIT for verbosity
 * @return 0 on success, negative value on failure
 */
IDATA
addLauncherArgs(J9PortLibrary * portLib, JavaVMInitArgs *launcherArgs, UDATA launcherArgumentsSize, J9JavaVMArgInfoList *vmArgumentsList, char **xServiceBuffer, UDATA argEncoding, UDATA verboseFlags);

/**
 * Add the arguments given by the launching process
 * @param [in] portLib port library
 * @param [in] vmArgumentsList current list of arguments
 * @param [in] xServiceBuffer receives the last (if any) -Xservice argument
 * @param verboseFlags set to VERBOSE_INIT for verbosity
 * @return 0 on success, negative value on failure
 */
IDATA
addXserviceArgs(J9PortLibrary * portLib, J9JavaVMArgInfoList *vmArgumentsList, char *xServiceBuffer, UDATA verboseFlags);

/**
 * Create the JavaVMInitArgs array and the J9VMInitArgs wrapper from a pool of J9JavaVMArgInfo structs.
 * This allocates memory for the JavaVMInitArgs and J9VMInitArgs arrays,
 * plus the array of JavaVMOption and parallel J9CmdLineOption structs, but not the options strings themselves.
 * The caller is responsible for freeing the vmArgumentsList pool.
 * @param [in] portLib port library
 * @param [in] launcherArgs JavaVMInitArgs passed in from the launcher
 * @param [inout] vmArgumentsList current list of arguments
 * @param [out] argEncoding update the encoding
 * @return J9VMInitArgs struct on success, NULL value on failure, e.g. memory allocate failed.
 */
J9VMInitArgs*
createJvmInitArgs(J9PortLibrary * portLib, JavaVMInitArgs *launcherArgs, J9JavaVMArgInfoList *vmArgumentsList, UDATA* argEncoding);

/**
 * Free the space for the J9VMInitArgs and JavaVMInitArgs structs, plus the arrays of JavaVMOption and J9CmdLineOption structs.
 * Also walk the array of J9VMInitArgs and free the optionStrings which are marked as being memory allocations.
 * @param portLib port library
 * @param vmArgumentsList current list of arguments
 */
void
destroyJvmInitArgs(J9PortLibrary * portLib, J9VMInitArgs *vmArgumentsList);

#endif /* vmargs_api_h */
