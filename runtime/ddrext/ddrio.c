/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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

#include "ddrutils.h"
#include "ddrio.h"
#include "com_ibm_j9ddr_corereaders_debugger_JniOutputStream.h"
#include <stdlib.h>

void appendToBuffer(char * str);

char *pendingOutput = NULL;
size_t bufferedLen = 0;

JNIEXPORT void JNICALL
Java_com_ibm_j9ddr_corereaders_debugger_JniOutputStream_write__I(JNIEnv * env, jobject self, jint b)
{
	char str[2];
	str[0] = b;
	str[1] = 0;
	appendToBuffer(str);
}


JNIEXPORT void JNICALL
Java_com_ibm_j9ddr_corereaders_debugger_JniOutputStream_write___3BII(JNIEnv * env, jobject self, jbyteArray buffer, jint off, jint len)
{
	char* str;
	jsize arrayLen = (*env)->GetArrayLength(env, buffer);
	jbyte* outBuffer = (*env)->GetByteArrayElements(env, buffer, NULL);

	str = malloc(len+1);
	memcpy(str, outBuffer, len);
	str[len] = '\0';
	(*env)->ReleaseByteArrayElements(env, buffer, outBuffer, JNI_ABORT);


	appendToBuffer(str);
	free(str);
}

void dbgext_flushoutput() {
	if( pendingOutput != NULL ) {
		// Mark the buffer as free.
		// (But we can't free it until after it's displayed.)
		bufferedLen = 0;
		dbgWriteString(pendingOutput);
	}
}

void appendToBuffer(char * str) {
	if( str == NULL || strlen(str) == 0 ) {
		return;
	}
	// The output has just been cleared.
	if( bufferedLen == 0 && pendingOutput != NULL ) {
		free(pendingOutput);
		pendingOutput = NULL;
	}
	if( bufferedLen == 0 ) {
		// New buffer.
		size_t len = strlen(str);
		pendingOutput = malloc(len+1);
		pendingOutput[0] = '\0';
		strcat(pendingOutput, str);
		bufferedLen = len;
		pendingOutput[bufferedLen] = '\0';
	} else {
		// Extend existing buffer.
		char * newOutput = NULL;
		size_t len = strlen(str) + bufferedLen;
		newOutput = malloc( len + 1);
		newOutput[0] = '\0';
		strcat(newOutput, pendingOutput);
		strcat(newOutput, str);
		bufferedLen = len;
		newOutput[bufferedLen] = '\0';
		free(pendingOutput);
		pendingOutput = newOutput;
	}
}
