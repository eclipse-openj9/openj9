/*******************************************************************************
 * Copyright (c) 2007, 2017 IBM Corp. and others
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
#ifndef trace_h
#define trace_h
/*
 * @ddr_namespace: default
 */

#include "jni.h"
#include "ute.h"
#include "jvmri.h"

#include "ut_j9jcl.h"

/*Forward declaration of ArrayList structure defined in jcltrace.c*/
struct ArrayList;

typedef struct traceDotCGlobalMemory {
	/* Module Info arraylist */
	struct ArrayList * modInfoList;	

	/*List containing arrays of words describing the expected arguments for each trace point*/
	struct ArrayList * argumentStructureList;

	/* Count of applications registered for app trace */
	U_32 numberOfAppTraceApplications;

	/* RAS interface pointer */
	DgRasInterface  *rasIntf;

	/* trace interface pointer */
	UtInterface     *utIntf;
} traceDotCGlobalMemory;

extern void terminateTrace(JNIEnv *env);

#endif /* trace_h */
