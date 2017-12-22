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

#ifndef exelib_api_h
#define exelib_api_h

/**
* @file exelib_api.h
* @brief Public API for the EXELIB module.
*
* This file contains public function prototypes and
* type definitions for the EXELIB module.
*/

#include "j9cfg.h"
#include "j9port.h"
#include "j9comp.h"
#include "jni.h"
#include "libhlp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct J9StringBuffer {
    UDATA remaining;
    char data[4];
} J9StringBuffer;

/* ---------------- libargs.c ---------------- */

/**
* @brief
* @param **vmOptionsTable
* @param *argv0
* @return I_32
*/
I_32 
vmOptionsTableAddExeName(
	void **vmOptionsTable, 
	char  *argv0
	);


/**
* @brief
* @param **vmOptionsTable
* @param *optionString
* @param *extraInfo
* @return I_32
*/
I_32 
vmOptionsTableAddOption(
	void **vmOptionsTable, 
	char  *optionString,
	void  *extraInfo
	);


/**
* @brief
* @param **vmOptionsTable
* @param *optionString
* @param *extraInfo
* @return I_32
*/
I_32 
vmOptionsTableAddOptionWithCopy(
	void **vmOptionsTable, 
	char  *optionString,
	void  *extraInfo
	);


/**
* @brief
* @param **vmOptionsTable
* @return void
*/
void 
vmOptionsTableDestroy(
	void **vmOptionsTable
	);


/**
* @brief
* @param **vmOptionsTable
* @return int
*/
int 
vmOptionsTableGetCount(
	void **vmOptionsTable
	);


/**
* @brief
* @param **vmOptionsTable
* @return JavaVMOption *
*/
JavaVMOption *
vmOptionsTableGetOptions(
	void **vmOptionsTable
	);


/**
* @brief
* @param *portLib
* @param **vmOptionsTable
* @param initialCount
* @return I_32
*/
I_32 
vmOptionsTableInit(
	J9PortLibrary    *portLib,
	void            **vmOptionsTable,
	int              initialCount
	);

/**
 * This function scans for -Djava.home=xxx/jre. If found, it uses it to construct a path
 * to redirector jvm.dll and put it in the passed in char buffer:
 * 	 --> [java.home]/bin/j9vm/
 * This function is meant to be used by thrstatetest and shrtest so the test can be invoked
 * in a sdk shape independent way.
 */
BOOLEAN
cmdline_fetchRedirectorDllDir(struct j9cmdlineOptions *args, char *result);

/* ---------------- libhlp.c ---------------- */

/**
* @brief
* @param *portLib
* @param sep
* @param **classPath
* @param *toAppend
* @return I_32
*/
I_32 
main_appendToClassPath( J9PortLibrary *portLib, U_16 sep, J9StringBuffer **classPath, char *toAppend);

/**
* @brief
* @param portLib
* @param **finalBootLibraryPath
* @param *argv0
* @return IDATA
*/
IDATA 
main_initializeBootLibraryPath(J9PortLibrary * portLib, J9StringBuffer **finalBootLibraryPath, char *argv0);


/**
* @brief
* @param *portLib
* @param classPath
* @return I_32
*/
I_32 
main_initializeClassPath( J9PortLibrary *portLib, J9StringBuffer** classPath);


/**
* @brief
* @param portLib
* @param **finalJavaHome
* @param argc
* @param **argv
* @return IDATA
*/
IDATA 
main_initializeJavaHome(J9PortLibrary * portLib, J9StringBuffer **finalJavaHome, int argc, char **argv);


/**
* @brief
* @param portLib
* @param **finalJavaLibraryPath
* @param *argv0
* @return IDATA
*/
IDATA 
main_initializeJavaLibraryPath(J9PortLibrary * portLib, J9StringBuffer **finalJavaLibraryPath, char *argv0);


/**
* @brief
* @param *portLib
* @param sep
* @param **classPath
* @param *toPrepend
* @return I_32
*/
I_32
main_prependToClassPath( J9PortLibrary *portLib, U_16 sep, J9StringBuffer **classPath, char *toPrepend);


/**
* @brief
* @param env
* @param *mainClassName
* @param nameIsUTF
* @param java_argc
* @param **java_argv
* @param j9portLibrary
* @return int
*/
int 
main_runJavaMain(JNIEnv * env, char *mainClassName, int nameIsUTF, int java_argc, char **java_argv, J9PortLibrary * j9portLibrary);


/**
* @brief
* @param portLib
* @param **argv
* @return void
*/
void 
main_setNLSCatalog(J9PortLibrary * portLib, char **argv);

/**
* @brief
* @param *portLib
* @param *argv0
* @param **optionFileName
* @return I_32
*/
I_32 
main_findDefaultOptionsFile(J9PortLibrary *portLib, char *argv0, char **optionFileName);

/**
* @brief
* @param void
* @return char *
*/
char * 
main_vmVersionString(void);


/**
* @brief
* @param *portLib
* @param *detailString
* @param detailStringLength
* @return char *
*/
char * 
vmDetailString( J9PortLibrary *portLib, char *detailString, UDATA detailStringLength );


/* ---------------- memcheck.c ---------------- */

#if (defined(J9VM_OPT_MEMORY_CHECK_SUPPORT))  /* File Level Build Flags */

/**
* @brief
* @param portLib
* @param *modeStr
* @return IDATA
*/
IDATA 
memoryCheck_initialize(J9PortLibrary * portLib, char const *modeStr , char **argv);


/**
* @brief
* @param *portLibrary
* @param lastLegalArg
* @param **argv
* @return UDATA
*/
IDATA 
memoryCheck_parseCmdLine( J9PortLibrary *portLibrary, UDATA lastLegalArg , char **argv );


/**
* @brief
* @param portLib
* @return void
*/
void 
memoryCheck_print_report(J9PortLibrary * portLib);

#endif /* J9VM_OPT_MEMORY_CHECK_SUPPORT */ /* End File Level Build Flags */


/* ---------------- rconsole.c ---------------- */

#if (defined(J9VM_OPT_REMOTE_CONSOLE_SUPPORT))  /* File Level Build Flags */

/**
* @brief
* @param *portLibrary
* @param lastLegalArg
* @param **argv
* @return UDATA
*/
UDATA
remoteConsole_parseCmdLine(J9PortLibrary *portLibrary, UDATA lastLegalArg, char **argv);

#endif /* J9VM_OPT_REMOTE_CONSOLE_SUPPORT */ /* End File Level Build Flags */


/* ---------------- strbuf.c ---------------- */

struct J9PortLibrary;
/**
* @brief
* @param *portLibrary
* @param buffer
* @param string
* @return J9StringBuffer*
*/
J9StringBuffer* 
strBufferCat(struct J9PortLibrary *portLibrary, J9StringBuffer* buffer, const char* string);


/**
* @brief
* @param buffer
* @return char*
*/
char* 
strBufferData(J9StringBuffer* buffer);


struct J9PortLibrary;
/**
* @brief
* @param *portLibrary
* @param buffer
* @param len
* @return J9StringBuffer*
*/
J9StringBuffer* 
strBufferEnsure(struct J9PortLibrary *portLibrary, J9StringBuffer* buffer, UDATA len);


struct J9PortLibrary;
/**
* @brief
* @param *portLibrary
* @param buffer
* @param string
* @return J9StringBuffer*
*/
J9StringBuffer* 
strBufferPrepend(struct J9PortLibrary *portLibrary, J9StringBuffer* buffer, char* string);


#ifdef __cplusplus
}
#endif

#endif /* exelib_api_h */
