/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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

#include "j9.h"
#include "jvminit.h"
#include "j9lib.h"
#include "vm_internal.h"
#include "verbose_api.h"

static UDATA isVerboseJni(J9PortLibrary *portLibrary, J9VMInitArgs* j9vm_args, IDATA *verboseIndexPtr);
static IDATA printXcheckUsage(J9JavaVM *vm);
extern void J9RASCheckDump(J9JavaVM* javaVM);

jint 
processXCheckOptions(J9JavaVM * vm, J9Pool* loadTable, J9VMInitArgs* j9vm_args) 
{
	jint rc = 0;
	J9VMDllLoadInfo* entry = NULL;
	IDATA jniIndex, naboundsIndex, gcIndex, vmIndex, allIndex;
	IDATA checkCPIndex, checkCPHelpIndex;
	IDATA noJNIIndex, noGCIndex, noVMIndex, noneIndex, nocheckCPIndex, nohelpIndex;
	IDATA helpIndex, memoryHelpIndex, memoryNoneIndex;
	IDATA dumpIndex, dumpHelpIndex, dumpNoneIndex;

    PORT_ACCESS_FROM_JAVAVM(vm);

	/*
	 * Global options
	 */
	noneIndex = findArgInVMArgs( PORTLIB, j9vm_args, EXACT_MATCH, "-Xcheck:none", NULL, TRUE);
	helpIndex = findArgInVMArgs( PORTLIB, j9vm_args, EXACT_MATCH, "-Xcheck:help", NULL, TRUE);
	allIndex = findArgInVMArgs( PORTLIB, j9vm_args, EXACT_MATCH, "-Xcheck", NULL, TRUE);

	/*
	 * Memory options: Need to deal with the processing of the memory:help option as this should
	 * disable the startup of the jvm 
	 */
	memoryNoneIndex = findArgInVMArgs( PORTLIB, j9vm_args, EXACT_MATCH, "-Xcheck:memory:none", NULL, TRUE);
	memoryHelpIndex = findArgInVMArgs( PORTLIB, j9vm_args, EXACT_MATCH, "-Xcheck:memory:help", NULL, TRUE);
	
	if (memoryNoneIndex < memoryHelpIndex) {
		rc = -1;
	}
	
	/*
	 * Memory options: These were handled earlier -- just recognize and consume the 
	 * remaining options here.
	 */
	findArgInVMArgs( PORTLIB, j9vm_args, OPTIONAL_LIST_MATCH, "-Xcheck:memory", NULL, TRUE);

	/*
	 * JNI options 
	 */
	jniIndex = findArgInVMArgs( PORTLIB, j9vm_args, OPTIONAL_LIST_MATCH, "-Xcheck:jni", NULL, TRUE);
	/* 
	 * -Xcheck:nabounds is a Sun-compatibility option. It is equivalent to -Xcheck:jni.
	 * No sub-options are permitted for -Xcheck:nabounds
	 */
	naboundsIndex = findArgInVMArgs( PORTLIB, j9vm_args, EXACT_MATCH, "-Xcheck:nabounds", NULL, TRUE);
	noJNIIndex = findArgInVMArgs( PORTLIB, j9vm_args, EXACT_MATCH, "-Xcheck:jni:none", NULL, TRUE);

	jniIndex = OMR_MAX(OMR_MAX(jniIndex, naboundsIndex), allIndex);
	noJNIIndex = OMR_MAX(noJNIIndex, noneIndex);

	if ((jniIndex > noJNIIndex) || isVerboseJni(PORTLIB, j9vm_args, NULL)) {
		if (jniIndex >= 0) { /* -1 indicates the argument not found */
			j9vm_args->j9Options[jniIndex].flags |= ARG_REQUIRES_LIBRARY;
		}
		entry = findDllLoadInfo(loadTable, J9_CHECK_JNI_DLL_NAME);
		entry->loadFlags |= LOAD_BY_DEFAULT;
	}

	/*
	 * GC options
	 */
	gcIndex = findArgInVMArgs( PORTLIB, j9vm_args, OPTIONAL_LIST_MATCH, "-Xcheck:gc", NULL, TRUE);
	noGCIndex = findArgInVMArgs( PORTLIB, j9vm_args, EXACT_MATCH, "-Xcheck:gc:none", NULL, TRUE);

	gcIndex = OMR_MAX(gcIndex, allIndex);
	noGCIndex = OMR_MAX(noGCIndex, noneIndex);

	if (gcIndex > noGCIndex) {
		j9vm_args->j9Options[gcIndex].flags |= ARG_REQUIRES_LIBRARY;
		entry = findDllLoadInfo(loadTable, J9_CHECK_GC_DLL_NAME);
		entry->loadFlags |= LOAD_BY_DEFAULT;
	}

	/*
	 * VM options
	 */
	vmIndex = findArgInVMArgs( PORTLIB, j9vm_args, OPTIONAL_LIST_MATCH, "-Xcheck:vm", NULL, TRUE);
	noVMIndex = findArgInVMArgs( PORTLIB, j9vm_args, EXACT_MATCH, "-Xcheck:vm:none", NULL, TRUE);

	vmIndex = OMR_MAX(vmIndex, allIndex);
	noVMIndex = OMR_MAX(noVMIndex, noneIndex);

	if (vmIndex > noVMIndex) {
		j9vm_args->j9Options[vmIndex].flags |= ARG_REQUIRES_LIBRARY;
		entry = findDllLoadInfo(loadTable, J9_CHECK_VM_DLL_NAME);
		entry->loadFlags |= LOAD_BY_DEFAULT;
	}

	/*
	 * Check classpath: all classpath processing is done here.
	 */
	checkCPIndex = findArgInVMArgs( PORTLIB, j9vm_args, EXACT_MATCH, "-Xcheck:classpath", NULL, TRUE);
	nocheckCPIndex = findArgInVMArgs( PORTLIB, j9vm_args, EXACT_MATCH, "-Xcheck:classpath:none", NULL, TRUE);
	checkCPHelpIndex = findArgInVMArgs( PORTLIB, j9vm_args, EXACT_MATCH, "-Xcheck:classpath:help", NULL, TRUE);
	
	checkCPIndex = OMR_MAX(checkCPIndex, allIndex);
	nocheckCPIndex = OMR_MAX(OMR_MAX(nocheckCPIndex, noneIndex), helpIndex);

	if (checkCPHelpIndex > nocheckCPIndex) {
		j9tty_err_printf(PORTLIB, "\nUsage: -Xcheck:classpath[:help|none]\n\n");
		rc = -1;
	}
	
	if (checkCPIndex > nocheckCPIndex) {
		/* classpath checking has been requested, so set a flag to show it is on */
		J9VMSystemProperty * property;
 
		if (getSystemProperty(vm, "com.ibm.jcl.checkClassPath", &property) == J9SYSPROP_ERROR_NONE) {
			setSystemProperty(vm, property, "true");
			property->flags &= ~J9SYSPROP_FLAG_WRITEABLE;
		}
	}

	/*
	 * Dump option
	 */
	dumpIndex = findArgInVMArgs( PORTLIB, j9vm_args, EXACT_MATCH, "-Xcheck:dump", NULL, TRUE);
	dumpNoneIndex = findArgInVMArgs( PORTLIB, j9vm_args, EXACT_MATCH, "-Xcheck:dump:none", NULL, TRUE);
	dumpHelpIndex = findArgInVMArgs( PORTLIB, j9vm_args, EXACT_MATCH, "-Xcheck:dump:help", NULL, TRUE);

	dumpIndex = OMR_MAX(dumpIndex, allIndex);
	dumpNoneIndex = OMR_MAX(OMR_MAX(dumpNoneIndex, noneIndex), helpIndex);

	if (dumpHelpIndex > dumpNoneIndex) {
		j9tty_err_printf(PORTLIB, "\nUsage: -Xcheck:dump\nRun JVM start-up checks for OS system dump settings\n\n");
		rc = -1;
	}

	if (dumpIndex > dumpNoneIndex) {
		/* dump checking has been requested, do it now */
		J9RASCheckDump(vm);
	}

	nohelpIndex = OMR_MAX(OMR_MAX(noneIndex, memoryHelpIndex), checkCPHelpIndex);
	if (helpIndex > nohelpIndex) {
		/* print out the base -Xcheck help */
		printXcheckUsage(vm);
		rc = -1;
	}
	return rc;
}

static IDATA printXcheckUsage(J9JavaVM *vm)
{
        PORT_ACCESS_FROM_JAVAVM(vm);
 
        j9tty_err_printf(PORTLIB, "\n-Xcheck usage:\n\n");
 
        j9tty_err_printf(PORTLIB, "  -Xcheck:help                  Print general Xcheck help\n");
        j9tty_err_printf(PORTLIB, "  -Xcheck:none                  Ignore all previous/default Xcheck options\n");
 
        j9tty_err_printf(PORTLIB, "  -Xcheck:<component>:help      Print detailed Xcheck help\n");
        j9tty_err_printf(PORTLIB, "  -Xcheck:<component>:none      Ignore previous Xcheck options of this type\n");
 
        j9tty_err_printf(PORTLIB, "\nXcheck enabled components:\n\n");

        j9tty_err_printf(PORTLIB, "  classpath\n");
        j9tty_err_printf(PORTLIB, "  dump\n");
        j9tty_err_printf(PORTLIB, "  gc\n");
        j9tty_err_printf(PORTLIB, "  jni\n");
        j9tty_err_printf(PORTLIB, "  memory\n");
        j9tty_err_printf(PORTLIB, "  vm\n\n");

        return JNI_OK;
}
 
/**
 * Non-consuming search for "-verbose:jni"
 * @param list of arguments
 * @param pointer to variable to hold position of argument
 * @returns 1 if argument found. Position returned by reference
 */

static UDATA 
isVerboseJni(J9PortLibrary *portLibrary, J9VMInitArgs* j9vm_args, IDATA *verboseIndexPtr) {
	IDATA argIndex;
	PORT_ACCESS_FROM_PORT(portLibrary);
	argIndex = findArgInVMArgs( PORTLIB, j9vm_args, OPTIONAL_LIST_MATCH, OPT_VERBOSE, "jni", FALSE);
	if (NULL != verboseIndexPtr) {
		*verboseIndexPtr = argIndex;
	}
	 if (argIndex >= 0) {
		return 1;
	} else {
		return 0;
	}
}
