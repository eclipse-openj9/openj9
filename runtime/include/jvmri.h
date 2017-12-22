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

#ifndef _IBM_JVMRAS_H_
#define _IBM_JVMRAS_H_

/* @ddr_namespace: default */
/*
 * ======================================================================
 * Allow for inclusion in C++
 * ======================================================================
 */
#ifdef __cplusplus
extern "C" {
#endif

#include "jni.h"
#include "stdarg.h"

/*
 * ======================================================================
 * Forward declarations
 * ======================================================================
 */
typedef void (JNICALL *TraceListener)(JNIEnv *env, void **thrLocal, int traceId,
						  const char * format, va_list varargs); 
typedef void (JNICALL *TraceListener50)(JNIEnv *env, void **thrLocal, const char *moduleName,
		int traceId, const char * format, va_list varargs);
typedef void (*DgRasOutOfMemoryHook)(void); 

/*
 * ======================================================================
 * RasInfo structures
 * ======================================================================
 */
typedef struct RasInfo {
	int  type;
	union {
		struct {
			int    number;
			char **names;
		} query;
		struct {
			int    number;
			char **names;
		} trace_components;
		struct {
			char          *name;
			int            first;
			int            last;
			unsigned char *bitMap;
		} trace_component;
	} info;
} RasInfo;

#define RASINFO_TYPES               				0
#define RASINFO_TRACE_COMPONENTS    	1
#define RASINFO_TRACE_COMPONENT     	2
#define RASINFO_MAX_TYPES           			2

/*
 * ======================================================================
 * External access facade
 * ======================================================================
 */
#define JVMRAS_VERSION_1_1      0x7F000001
#define JVMRAS_VERSION_1_3      0x7F000003
#define JVMRAS_VERSION_1_5      0x7F000005

typedef struct DgRasInterface {
	char    eyecatcher[4];
	int     length;
	int     version;
	int     modification;
	/* Interface level 1_1 */
	int     (JNICALL *TraceRegister)(JNIEnv *env, TraceListener func);
	int     (JNICALL *TraceDeregister)(JNIEnv *env, TraceListener func);
	int     (JNICALL *TraceSet)(JNIEnv *env, const char *);
	void    (JNICALL *TraceSnap)(JNIEnv *env, char *);
	void    (JNICALL *TraceSuspend)(JNIEnv *env);
	void    (JNICALL *TraceResume)(JNIEnv *env);
	int     (JNICALL *GetRasInfo)(JNIEnv * env, RasInfo * info_ptr);
	int     (JNICALL *ReleaseRasInfo)(JNIEnv * env, RasInfo * info_ptr);
	int     (JNICALL *DumpRegister)(JNIEnv *env,
									int (JNICALL *func)(JNIEnv *env2,
														void **threadLocal,
														int reason));
	int     (JNICALL *DumpDeregister)(JNIEnv *env,
									  int (JNICALL *func)(JNIEnv *env2,
														  void **threadLocal,
														  int reason));
	void    (JNICALL *NotifySignal)(JNIEnv *env, int signal);
	int     (JNICALL *CreateThread)( JNIEnv *env, void (JNICALL *startFunc)(void*),
										void *args, int GCSuspend );
	int     (JNICALL *GenerateJavacore)( JNIEnv *env );
	int     (JNICALL *RunDumpRoutine)( JNIEnv *env, int componentID, int level,
									   void (*printrtn)(void *env, const char *tagName,
														const char *fmt, ...) );
	int     (JNICALL *InjectSigsegv)( JNIEnv *env );
	int     (JNICALL *InjectOutOfMemory)( JNIEnv *env );
	int     (JNICALL *SetOutOfMemoryHook)( JNIEnv *env, void (*OutOfMemoryFunc)(void) );
	int     (JNICALL *GetComponentDataArea)( JNIEnv *env, char *componentName,
											   void **dataArea, int *dataSize );
	int     (JNICALL *InitiateSystemDump)( JNIEnv *env );
	/* Interface level 1_3 follows */
	void        (JNICALL *DynamicVerbosegc) (JNIEnv *env, int vgc_switch,
											int vgccon, char* file_path,
											int number_of_files,
											int number_of_cycles);
	void    (JNICALL *TraceSuspendThis)(JNIEnv *env);
	void    (JNICALL *TraceResumeThis)(JNIEnv *env);
	int     (JNICALL *GenerateHeapdump)( JNIEnv *env );
	/* Interface level 1_5 */
	int     (JNICALL *TraceRegister50)(JNIEnv *env, TraceListener50 func);
	int     (JNICALL *TraceDeregister50)(JNIEnv *env, TraceListener50 func);
} DgRasInterface;

/*
 * ======================================================================
 *    Dump exit return codes
 * ======================================================================
 */
#define RAS_DUMP_CONTINUE   	0		/* Continue with diagnostic collection */
#define RAS_DUMP_ABORT      		1		/* No more diagnostics should be taken */

/*
 * ======================================================================
 *    Thread Creation types
 * ======================================================================
 */
#define NO_GC_THREAD_SUSPEND		0       /* Do not suspend thread during CG. */
#define GC_THREAD_SUSPEND       		1       /* Suspend thread during CG. */
#define RAS_THREAD_NAME_SIZE    	50     /* Size of Ras Thread Name. */

/*
 * ======================================================================
 *    Dump Handler types
 * ======================================================================
 */
enum dumpType {
	 NODUMPS      = 0,
	 JAVADUMP     = 0x01,
	 SYSDUMP      = 0x02,
	 CEEDUMP      = 0x04,
	 HEAPDUMP     = 0x08,
	 MAXDUMPTYPES = 6,

	/* ensure 4-byte enum */
	dumpTypeEnsureWideEnum = 0x1000000
};

#define ALLDUMPS (JAVADUMP | SYSDUMP | CEEDUMP | HEAPDUMP)
#define OSDUMP     (ALLDUMPS + 1)

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* !_IBM_JVMRAS_H_ */
