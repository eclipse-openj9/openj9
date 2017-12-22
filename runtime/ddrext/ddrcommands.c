/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#include "ddrcommands.h"
#include "ddrutils.h"
#include "ddrio.h"

static void
ddrProcessLine(const char *args)
{
	static int initialized = 0;
	static jclass ddrInteractiveCls;
	static jmethodID processLineMethod;
	static jstring commandString;
	static jobjectArray methodArgs;
	ddrExtEnv * ddrEnv = NULL;

	ddrEnv = ddrGetEnv();
	if (ddrEnv == NULL) {
		dbgWriteString("DDR: failed obtaining DDR plugin extension environment\n");
		return;
	}

	if ((strcmp("exit",args) == STRING_MATCHED)) {
		dbgext_stopddr(args);
		return;
	}

	if (initialized == 0) {

		ddrInteractiveCls = (*ddrEnv->env)->FindClass(ddrEnv->env, "com/ibm/j9ddr/tools/ddrinteractive/DDRInteractive");
		if (ddrInteractiveCls == NULL) {
			dbgWriteString("DDR: failed to find class com.ibm.j9ddr.tools.ddrinteractive.DDRInteractive\n");
			return;
		}

		processLineMethod = (*ddrEnv->env)->GetMethodID(ddrEnv->env, ddrInteractiveCls, "processLine", "(Ljava/lang/String;)V");
		if (processLineMethod == NULL) {
			dbgWriteString("DDR: failed to find method: processLine\n");
			return;
		}

		initialized = 1;
	}

	commandString = (*ddrEnv->env)->NewStringUTF(ddrEnv->env, args);  

	(*ddrEnv->env)->CallVoidMethod(ddrEnv->env, ddrEnv->ddrContext, processLineMethod, commandString);
	if((*ddrEnv->env)->ExceptionCheck(ddrEnv->env)) {
		dbgWriteString("DDR: exception while calling com/ibm/j9ddr/tools/ddrinteractive/DDRInteractive/processLine()\n");
	}
	// Write the output once the JNI method call is complete so that
	// using "q" to quit a command when it pages can't force an unexpected
	// return from JNI.
	dbgext_flushoutput();

	return;
}


void
dbgext_ddr(const char *args) 
{
	ddrExtEnv * ddrEnv = ddrGetEnv();

	if (ddrEnv == NULL) {
		if (ddrHasExited()) {
			dbgWriteString("DDR: DDR has exited, close the debugger and start it again\n");
			return;
		} else if (ddrStart() == NULL) {
			dbgWriteString("DDR: Failed to start the DDR JVM\n");
			return;
		}
	}

	ddrProcessLine(args);
}


void
dbgext_startddr(const char *args)
{
	ddrStart();
}
 
void
dbgext_stopddr(const char *args)
{
	ddrExtEnv * ddrEnv = ddrGetEnv();

	if (ddrEnv != NULL) {
		ddrStop(ddrEnv);
	}
}
 
