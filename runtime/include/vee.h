/*******************************************************************************
 * Copyright (c) 1991, 2006 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef vee_h
#define vee_h

#include "j9comp.h"
#include "j9cfg.h"

#if (defined(J9VM_OPT_VEE_SUPPORT))  /* File Level Build Flags */

/* 
 * vee.h 
 */

/** An enumeration of all status codes */
typedef enum rtStatus
{
	RT_STATUS_OK = 0,
	RT_STATUS_GENERAL_FAILURE = 1,
} rtStatus;

typedef struct Runtime
{
	const char* language_id;		/** The name and language level of the runtime environment.  (e.g. "java.v1.4.2") */
	const char* vendor_id;			/** The name and version identifiers of the vendor (e.g. "IBM J9 v2.3") */

	// Options (e.g. classPath, gc, etc...)
	// Runtime-Local-Storage
	// Introspection: Attached threads, uptime

} Runtime;

/**
  * Runtime Creation:
  * 		e.g.  CreateRuntime("php",&runtime);
  * 		e.g.  CreateRuntime("java.v1.4.2",&runtime);
  */ 
rtStatus CreateRuntime(const char *runtime_id, Runtime** pRuntime);

#endif /* J9VM_OPT_VEE_SUPPORT */ /* End File Level Build Flags */

#endif     /* vee_h */
