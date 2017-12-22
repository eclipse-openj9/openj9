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

#ifndef j9trace_h
#define j9trace_h

/* @ddr_namespace: default */
#ifdef __cplusplus
extern "C" {
#endif

#include "omrthread.h"
#include "omr.h"
#include "ute_module.h"

/**
 * Function pointer for reconfiguring the trace system at
 * runtime.
 * 
 * Distinct from ute.TraceSet because it can reconfigure
 * rastrace options (-Xtrace:trigger etc.) as well as tracepoints
 */
typedef omr_error_t (*ConfigureTraceFunction)(void *,const char *);

typedef struct RasGlobalStorage {
	/* The utGlobalData reference here is unused by all Java code and inaccessible to OMR code
	 * however it is required to allow DDR debug extensions that work with trace a way to find
	 * the trace data.
	 */
	void *  utGlobalData;
	/* Copy of utInterface populated with trace function pointers that expect
	 * J9VMThread for their env pointers on the module interface.
	 * The version on OMR_VM->utIntf has a module interface that expects
	 * OMR_VMThread pointers as env parameters.
	 */
	UtInterface*  utIntf;
	void *  jvmriInterface;
	void *  deferredJVMRIThreads;

	void *  triggerOnMethods;
	void *  traceMethodTable;
	int     stackdepth;
	unsigned int    stackCompressionLevel;
	ConfigureTraceFunction configureTraceEngine;
} RasGlobalStorage;

#define RAS_GLOBAL(x) ((RasGlobalStorage *)thr->javaVM->j9rasGlobalStorage)->x 

#define RAS_GLOBAL_FROM_JAVAVM(x,vm) ((RasGlobalStorage *)vm->j9rasGlobalStorage)->x

#define UT_THREAD_FROM_VM_THREAD(thr) ((thr) ? &(thr)->omrVMThread->_trace.uteThread : NULL)

/* Used with calls to TraceRegister/TraceDeregister */
typedef void (JNICALL *UtListener)(void *env, void **thrLocal, const char *modName, U_32 traceId, const char * format, va_list varargs);

#ifdef __cplusplus
}
#endif

#endif     /* j9trace_h */
