/*******************************************************************************
 * Copyright (c) 1998, 2019 IBM Corp. and others
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

#include "jniargtests.h"


const jbyte test_jbyte[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, };

const jshort test_jshort[] = { 0x0101, 0x0102, 0x0103, 0x0104, 0x0105, 0x0106, 0x0107, 0x0108, 0x0109, 0x010A, 0x010B, 0x010C, 0x010D, 0x010E, 0x010F, 0x0110, };

const jint test_jint[] = { 0xBEEF0001, 0xBEEF0002, 0xBEEF0003, 0xBEEF0004, 0xBEEF0005, 0xBEEF0006, 0xBEEF0007, 0xBEEF0008, 0xBEEF0009, 0xBEEF000A, 0xBEEF000B, 0xBEEF000C, 0xBEEF000D, 0xBEEF000E, 0xBEEF000F, 0xBEEF0010, };

const jlong test_jlong[] = {
	J9CONST64(0xFEED0101FACE0001),
	J9CONST64(0xFEED0202FACE0002),
	J9CONST64(0xFEED0303FACE0003),
	J9CONST64(0xFEED0404FACE0004),
	J9CONST64(0xFEED0505FACE0005),
	J9CONST64(0xFEED0606FACE0006),
	J9CONST64(0xFEED0707FACE0007),
	J9CONST64(0xFEED0808FACE0008),
	J9CONST64(0xFEED0909FACE0009),
	J9CONST64(0xFEED0A0AFACE000A),
	J9CONST64(0xFEED0B0BFACE000B),
	J9CONST64(0xFEED0C0CFACE000C),
	J9CONST64(0xFEED0D0DFACE000D),
	J9CONST64(0xFEED0E0EFACE000E),
	J9CONST64(0xFEED0F0FFACE000F),
	J9CONST64(0xFEED1010FACE0010),};

const jfloat test_jfloat[] = { 11.1f, 12.2f, 13.3f, 14.4f, 15.5f, 16.6f, 17.7f, 18.8f, 19.9f, 20.10f, 21.11f, 22.12f, 23.13f, 24.14f, 25.15f, 26.16f, };

const jdouble test_jdouble[] = { 11.1, 12.2, 13.3, 14.4, 15.5, 16.6, 17.7, 18.8, 19.9, 20.10, 21.11, 22.12, 23.13, 24.14, 25.15, 26.16, };

const jboolean test_jboolean[] = { JNI_TRUE, JNI_FALSE };

int argFailures = 0;
int retValFailures = 0;
int jniTests = 0;

J9JavaVM *getJ9JavaVM(JNIEnv * env)
{
	JavaVM *jvm;

	if ((*env)->GetJavaVM(env, &jvm)) {
		return NULL;
	}

	if (((struct JavaVM_ *)jvm)->reserved1 != (void*)J9VM_IDENTIFIER) {
		return NULL;
	}

	return (J9JavaVM *)jvm;
}


void cFailure_jboolean(J9PortLibrary *portLib, char *functionName, int index, jboolean arg, jboolean expected)
{
	PORT_ACCESS_FROM_PORT(portLib);
	j9tty_printf(portLib, "Argument error:  %s::arg%i got: %08X expected: %08X\n", functionName, index, arg, expected);
	argFailures++;
}
void cFailure_jbyte(J9PortLibrary *portLib, char *functionName, int index, jbyte arg, jbyte expected)
{
	PORT_ACCESS_FROM_PORT(portLib);
	j9tty_printf( portLib, "Argument error:  %s::arg%i got: %08X expected: %08X\n", functionName, index, arg, expected);
	argFailures ++;
}
void cFailure_jshort(J9PortLibrary *portLib, char *functionName, int index, jshort arg, jshort expected)
{
	PORT_ACCESS_FROM_PORT(portLib);
	j9tty_printf( portLib, "Argument error:  %s::arg%i got: %08X expected: %08X\n", functionName, index, arg, expected);
	argFailures ++;
}
void cFailure_jint(J9PortLibrary *portLib, char *functionName, int index, jint arg, jint expected)
{
	PORT_ACCESS_FROM_PORT(portLib);
	j9tty_printf( portLib, "Argument error:  %s::arg%i got: %08X expected: %08X\n", functionName, index, arg, expected);
	argFailures ++;
}
void cFailure_jlong(J9PortLibrary *portLib, char *functionName, int index, jlong arg, jlong expected)
{
	PORT_ACCESS_FROM_PORT(portLib);
#ifdef J9VM_ENV_LITTLE_ENDIAN
	j9tty_printf( portLib, "Argument error:  %s::arg%i got: %08X%08X expected: %08X%08X\n", functionName, index, ((U_32*)&arg)[1], ((U_32*)&arg)[0], ((U_32*)&expected)[1], ((U_32*)&expected)[0]);
#else
	j9tty_printf( portLib, "Argument error:  %s::arg%i got: %08X%08X expected: %08X%08X\n", functionName, index, ((U_32*)&arg)[0], ((U_32*)&arg)[1], ((U_32*)&expected)[0], ((U_32*)&expected)[1]);
#endif
	argFailures ++;
}
void cFailure_jfloat(J9PortLibrary *portLib, char *functionName, int index, jfloat arg, jfloat expected)
{
	PORT_ACCESS_FROM_PORT(portLib);
	j9tty_printf( portLib, "Argument error:  %s::arg%i got: %08X expected: %08X\n", functionName, index, ((U_32*)&arg)[0], ((U_32*)&expected)[0]);
	argFailures ++;
}
void cFailure_jdouble(J9PortLibrary *portLib, char *functionName, int index, jdouble arg, jdouble expected)
{
	PORT_ACCESS_FROM_PORT(portLib);
#ifdef J9VM_ENV_LITTLE_ENDIAN
	j9tty_printf( portLib, "Argument error:  %s::arg%i got: %08X%08X expected: %08X%08X\n", functionName, index, ((U_32*)&arg)[1], ((U_32*)&arg)[0], ((U_32*)&expected)[1], ((U_32*)&expected)[0]);
#else
	j9tty_printf( portLib, "Argument error:  %s::arg%i got: %08X%08X expected: %08X%08X\n", functionName, index, ((U_32*)&arg)[0], ((U_32*)&arg)[1], ((U_32*)&expected)[0], ((U_32*)&expected)[1]);
#endif
	argFailures ++;
}


void JNICALL Java_JniArgTests_logRetValError( JNIEnv *p_env, jobject p_this, jstring error_message )
{
	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	char *msg;
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	msg = (char*)((*p_env)->GetStringUTFChars(p_env,error_message,0));

	j9tty_printf( PORTLIB, "%s\n", msg );

	(*p_env)->ReleaseStringUTFChars(p_env,error_message,msg);
	retValFailures ++;
}

void JNICALL Java_JniArgTests_summary( JNIEnv *p_env, jobject p_this )
{
	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	j9tty_printf( PORTLIB, "  JNI Tests: %d\n", jniTests );
	j9tty_printf( PORTLIB, "  Argument Failures: %d\n", argFailures );
	j9tty_printf( PORTLIB, "  Return Value Failures: %d\n", retValFailures );

	if ( argFailures || retValFailures ) {
		j9tty_printf( PORTLIB, "JNI ARG Tests FAILED\n" );
	} else {
		j9tty_printf( PORTLIB, "JNI ARG Tests PASSED\n" );
	}
}

