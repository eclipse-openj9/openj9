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

/*
 * z/OS helpers needed to retrieve the parameters required to create a com.ibm.le.conditionhandling.ConditionException
 */

#ifndef leconditionexceptionsup_h
#define leconditionexceptionsup_h

typedef struct ConditionExceptionConstructorArgs {
		char *failingRoutine;	/* this memory needs to be freed using free() when done with it, as it is allocated in e2a_func() */
		U_64 offset;
		U_16 *rawTokenBytes;
		U_32 lenBytesRawTokenBytes;
		char *facilityID;
		U_32 c1;
		U_32 c2;				/* doubles as the message number */
		U_32 caze;
		U_32 severity;
		U_32 control;
		U_64 iSInfo;
} ConditionExceptionConstructorArgs;

long getReturnAddressOfJNINative(void *builderDSA);
void getConditionExceptionConstructorArgs(struct J9PortLibrary* portLibrary, void *gpInfo, struct ConditionExceptionConstructorArgs *ceArgs);

#endif /* leconditionexceptionsup_h */
