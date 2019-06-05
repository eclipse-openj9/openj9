/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#if defined(WIN32) || defined (WIN64)
#include <Windows.h>
#endif /* defined(WIN32) || defined (WIN64) */

#include "j9protos.h"
#include "omrthread.h"
#include "j9consts.h"
#include "jvmri.h"
#include "j9jvmrinls.h"
#include "VerboseGCInterface.h"
#include <limits.h>
#include <signal.h>
#include <string.h>
#include "ut_j9vm.h"

#if defined(J9ZOS390) && !defined(J9VM_ENV_DATA64)
#define J9RAS_THUNK(fp)  helperCompatibleFunctionPointer( (void *)(fp) )
#else
#define J9RAS_THUNK(fp)  ((void *) fp)
#endif

#define UT_EVENT_SEM omrthread_monitor_t

#include "ute_module.h"
#include "ute.h"

#ifndef MAX_PATH
#define MAX_PATH 1024
#endif

/*The name passed to the dump trigger system describing this component - appears in Javacores and on the console*/
#define DUMP_CALLER_NAME "JVMRI"

static const char *rasinfoNames[] = {"Get types of RAS information available",
									 "Get trace component names",
									 "Get trace component information"};

typedef struct rasThreadArgs_t{
	void *args;
	void (JNICALL *startFunc)(void*);
	JavaVM *vm;
	omrthread_monitor_t monitor;
	UDATA started;
}rasThreadArgs_t;

#define JVMRI_THREAD_NOT_STARTED ((UDATA)0)
#define JVMRI_THREAD_ERROR ((UDATA)-1)
#define JVMRI_THREAD_STARTED ((UDATA)1)

typedef struct rasDeferredThread_t {
	void *args;
	void (JNICALL *startFunc)(void*);
	struct rasDeferredThread_t* next;
} rasDeferredThread_t;

/* monitor to serialise the JVMRI dump calls. This avoids most problems
 * with the shareVMAccess mechanism in the dump code.
 */
static omrthread_monitor_t jvmridumpmonitor = NULL;
static int J9THREAD_PROC rasThreadStartFuncWrapper ( void *arg_struct );
static omr_error_t rasDumpFn (struct J9RASdumpAgent *agent, char *label, struct J9RASdumpContext *context);
static int JNICALL rasGetRasInfo (JNIEnv * env, RasInfo * info_ptr);
static int JNICALL rasInjectOutOfMemory ( JNIEnv *env );
int JNICALL rasInitiateSystemDump ( JNIEnv *env );
static int JNICALL rasCreateThread ( JNIEnv *env, void (JNICALL *startFunc)(void*), void *args, int GCSuspend );
void JNICALL rasTraceSnap (JNIEnv *env, char *buffer);
static void JNICALL rasTraceResumeThis (JNIEnv *env);
static void * rasMemFailToAllocateMem (OMRPortLibrary *portLibrary, UDATA bytes, const char *callSite, U_32 category);
static int rasCreateThreadImmediately ( J9JavaVM* vm, void (JNICALL *startFunc)(void*), void *args);
static int JNICALL rasDumpDeregister (JNIEnv *env, int (JNICALL *func)(JNIEnv *env2, void **threadLocal, int reason));
static void JNICALL rasTraceResume (JNIEnv *env);
static omr_error_t oomHookFn (struct J9RASdumpAgent *agent, char *label, struct J9RASdumpContext *context);
static int JNICALL rasInjectSigsegv (JNIEnv *env );
static void JNICALL rasTraceSuspend (JNIEnv *env);
static int JNICALL rasTraceRegister (JNIEnv *env , TraceListener func);
int JNICALL rasGenerateJavacore ( JNIEnv *env );
static void JNICALL rasDynamicVerbosegc (JNIEnv *env, int vgc_switch, int vgccon, char* file_path, int number_of_files, int number_of_cycles);
static int JNICALL rasRunDumpRoutine ( JNIEnv *env, int componentID, int level, void (*printrtn)(void *env, const char *tagName, const char *fmt, ...) );
static void JNICALL rasNotifySignal (JNIEnv *env, int signal);
static int JNICALL rasReleaseRasInfo (JNIEnv * env, RasInfo * info_ptr);
static int JNICALL rasTraceSet (JNIEnv *env, const char *cmd);
static int JNICALL rasDumpRegister (JNIEnv *env, int (JNICALL *func)(JNIEnv *env2, void **threadLocal, int reason));
int JNICALL rasGenerateHeapdump ( JNIEnv *env );
static int JNICALL rasGetComponentDataArea ( JNIEnv *env, char *componentName, void **dataArea, int *dataSize );
static UDATA protectedInjectSigsegv (J9PortLibrary* portLibrary, void* ignored);
static void JNICALL rasTraceSuspendThis (JNIEnv *env);
static omr_error_t rasDumpAgentShutdownFn (struct J9JavaVM *vm, struct J9RASdumpAgent **agentPtr);
static int JNICALL rasSetOutOfMemoryHook ( JNIEnv *env, void (*OutOfMemoryFunc)(void) );
static void rasListenerWrapper(void *userData, OMR_VMThread *env, void **thrLocal, const char *modName, U_32 traceId, const char *format, va_list varargs);
static int JNICALL rasTraceDeregister (JNIEnv *env, TraceListener func);
static int JNICALL rasTraceDeregister50 (JNIEnv *env, TraceListener50 func);
static UDATA rasThreadProtectedStartFuncWrapper (J9PortLibrary* portLib, void *arg_struct );

static void
rasListenerWrapper(void *userData, OMR_VMThread *env, void **thrLocal, const char *modName, U_32 traceId, const char *format, va_list varargs)
{
	((UtListener)userData)(env->_language_vmthread, thrLocal, modName, traceId, format, varargs);
}

static int  JNICALL rasTraceRegister(JNIEnv *env , TraceListener func)
{
	J9VMThread *thr = (J9VMThread *)env;

	PORT_ACCESS_FROM_VMC( thr );

	j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_RI_TRACELISTENER_NOT_SUPPORTED);

	return JNI_ERR;
}

static int  JNICALL rasTraceRegister50(JNIEnv *env , TraceListener50 func)
{
	J9VMThread *thr = (J9VMThread *)env;
	J9UtServerInterface *utserverinterface = UTSINTERFACEFROMVM(thr->javaVM);
	UtThreadData **tempThr = UT_THREAD_FROM_VM_THREAD(thr);

	UtListener listener = (UtListener) J9RAS_THUNK(func);

	return utserverinterface->omrf.TraceRegister(tempThr, rasListenerWrapper, (void *)listener);
}

static int  JNICALL rasTraceDeregister(JNIEnv *env, TraceListener func)
{
	J9VMThread *thr = (J9VMThread *)env;

	PORT_ACCESS_FROM_VMC( thr );

	j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_RI_TRACELISTENER_NOT_SUPPORTED);

	return JNI_ERR;
}

static int  JNICALL rasTraceDeregister50(JNIEnv *env, TraceListener50 func)
{
	J9VMThread *thr = (J9VMThread *)env;
	J9UtServerInterface *utserverinterface = UTSINTERFACEFROMVM(thr->javaVM);
	UtThreadData **tempThr = UT_THREAD_FROM_VM_THREAD(thr);

	UtListener listener = (UtListener) J9RAS_THUNK(func);

	return utserverinterface->omrf.TraceDeregister(tempThr, rasListenerWrapper, (void *)listener);
}

static int  JNICALL rasGetRasInfo(JNIEnv * env, RasInfo * info_ptr)
{
	int rc = JNI_ERR;
	int requestType;
	J9VMThread *thr = (J9VMThread *)env;
	J9UtServerInterface *utserverinterface = UTSINTERFACEFROMVM(thr->javaVM);
	UtThreadData **tempThr = UT_THREAD_FROM_VM_THREAD(thr);
	I_32 number = 0;
	IDATA length = 0;
	IDATA dataSize;
	int i;
	char **list;

	PORT_ACCESS_FROM_VMC( thr );

	if (info_ptr == NULL)
	{
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_RI_NULL_RAS_INFO_STRUCTURE);
		return JNI_EINVAL;
	}

	requestType = info_ptr->type;
	switch (requestType)
	{
	case RASINFO_TYPES:
		info_ptr->info.query.number = RASINFO_MAX_TYPES + 1;                          
		for (i = 0; i <= RASINFO_MAX_TYPES; i++) {                        
			length += (strlen(rasinfoNames[i]) + 1 + sizeof(void *));                 
		}                                                                             
		info_ptr->info.query.names = j9mem_allocate_memory(length, OMRMEM_CATEGORY_VM);                               
		if (info_ptr->info.query.names == NULL) {  
			j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_RI_CANT_ALLOCATE_QUERY_NAMES);
			return JNI_ENOMEM;			                                                                                          
		} else {                                                                      
			char *memPtr;                                                             
			memPtr = (char *)&(info_ptr->info.query.names[RASINFO_MAX_TYPES+1]);      
			for (i = 0; i <= RASINFO_MAX_TYPES; i++) {                    
				info_ptr->info.query.names[i] = memPtr;                               
				strcpy(memPtr, rasinfoNames[i]);                                      
				memPtr += (strlen(rasinfoNames[i]) + 1);                              
			}                                                                         
		}
		break;

	case RASINFO_TRACE_COMPONENTS:
		/* get the number and length of available component names */
		rc = utserverinterface->omrf.GetComponents(tempThr, &list, &number);
		/* return type not checked - ute returns error when called without number / length */
		/*if (rc != JNI_OK)
		{
			j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_RI_GETRASINFO_CANT_GET_COMPONENTS);
			return rc;
		} */
				
		info_ptr->info.trace_components.number = number;
		info_ptr->info.trace_components.names = list;
		break;

	case RASINFO_TRACE_COMPONENT:
		dataSize = 0;
		rc = utserverinterface->omrf.GetComponent(info_ptr->info.trace_component.name,
							  &(info_ptr->info.trace_component.bitMap), 
							  &(info_ptr->info.trace_component.first), 
							  &(info_ptr->info.trace_component.last));
		
		if (dataSize > 0)
		{
			(info_ptr->info.trace_component.bitMap) = j9mem_allocate_memory( (UDATA)dataSize , OMRMEM_CATEGORY_VM);
			if ((info_ptr->info.trace_component.bitMap) == NULL)
			{
				j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_RI_GETRASINFO_CANT_ALLOCATE_COMP_BIT_MAP);
				return JNI_ENOMEM;
			}
			rc = utserverinterface->omrf.GetComponent(info_ptr->info.trace_component.name,
								  &(info_ptr->info.trace_component.bitMap), 
								  &(info_ptr->info.trace_component.first), 
								  &(info_ptr->info.trace_component.last));
		} else {
			info_ptr->info.trace_component.bitMap = NULL;
		}

		break;
	default:
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_RI_GETRASINFO_UNSUPPORTED_FIELD_TYPE);
		return JNI_EINVAL;
	}

	return rc;
}

static int  JNICALL rasReleaseRasInfo(JNIEnv * env, RasInfo * info_ptr)
{
	int rc = JNI_ERR;
    int requestType;
    J9VMThread *thr = (J9VMThread *)env;

    PORT_ACCESS_FROM_VMC( thr );

    if (info_ptr == NULL)
    {
        j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_RI_RELEASERASINFO_NULL_INFO_STRUCTURE);
        return JNI_EINVAL;
    }

    requestType = info_ptr->type;
    switch (requestType)
    {
    case RASINFO_TYPES:
        j9mem_free_memory( info_ptr->info.query.names );
        rc = JNI_OK;
        break;
    case RASINFO_TRACE_COMPONENTS:
        j9mem_free_memory( info_ptr->info.trace_components.names );
        rc = JNI_OK;
        break;
    case RASINFO_TRACE_COMPONENT:
		if ( info_ptr->info.trace_component.bitMap != NULL ){
			j9mem_free_memory( info_ptr->info.trace_component.bitMap );
		}
        rc = JNI_OK;
        break;
    default:
        j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_RI_RELEASERASINFO_UNSUPPORTED_FIELD_TYPE);
        rc = JNI_EINVAL;
    }
    return rc;
}

static int JNICALL 
rasCreateThread( JNIEnv *env, void (JNICALL *startFunc)(void*), void *args, int GCSuspend )
{
	J9VMThread* vmThread = (J9VMThread*)env;
	J9JavaVM* vm = vmThread->javaVM;

	/* ensure the vm runtime is ready for the JNI AttachCurrentThread in rasCreateThreadImmediately */
	if (vm->runtimeFlags & J9_RUNTIME_INITIALIZED) {
		return rasCreateThreadImmediately(vm, startFunc, args);
	} else {
		PORT_ACCESS_FROM_JAVAVM(vm);
		rasDeferredThread_t* deferredRecord;
		RasGlobalStorage* rasGlobals = vm->j9rasGlobalStorage;

		deferredRecord = j9mem_allocate_memory(sizeof(*deferredRecord), OMRMEM_CATEGORY_VM);
		if (deferredRecord == NULL) {
	        j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_RI_CANT_ALLOCATE_MEM_FOR_THREAD_ARGS);
	        return JNI_ERR;
		}

		deferredRecord->startFunc = startFunc;
		deferredRecord->args = args;
		deferredRecord->next = rasGlobals->deferredJVMRIThreads;
		rasGlobals->deferredJVMRIThreads = deferredRecord;

		return JNI_OK;
	}
}

int  JNICALL rasGenerateJavacore( JNIEnv *env )
{
#if defined(J9VM_RAS_DUMP_AGENTS)
	int rc = JNI_OK;
	omr_error_t result = OMR_ERROR_NONE;
	J9VMThread *thr = (J9VMThread *)env;
	J9JavaVM *vm = thr->javaVM;

	omrthread_monitor_enter(jvmridumpmonitor);
	result = vm->j9rasDumpFunctions->triggerOneOffDump(vm, "java", DUMP_CALLER_NAME, NULL, 0);
	rc = (int)omrErrorCodeToJniErrorCode(result);
	
	omrthread_monitor_exit(jvmridumpmonitor);

	return rc;
#else
	return JNI_ERR;
#endif
}

static int  JNICALL rasRunDumpRoutine( JNIEnv *env, int componentID, int level,
														void (*printrtn)(void *env, const char *tagName,
														const char *fmt, ...) )
{
	J9VMThread *thr = (J9VMThread *)env;

	PORT_ACCESS_FROM_VMC( thr );
	
	j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_RI_RUNDUMPROUTINE_NOT_SUPPORTED);

	return JNI_ERR;
}

static UDATA
protectedInjectSigsegv(J9PortLibrary* portLibrary, void* ignored)
{
	{ int *ptr = NULL; *ptr = 100; }

	return JNI_ERR; /* if this function returns, it's an error! */
}

static int JNICALL rasInjectSigsegv(JNIEnv *env )
{
	J9VMThread *vmThread = (J9VMThread *)env;
	PORT_ACCESS_FROM_VMC( vmThread );
	UDATA rc;

	j9sig_protect(
		protectedInjectSigsegv, NULL,
		vmThread->javaVM->internalVMFunctions->structuredSignalHandler, vmThread,
		J9PORT_SIG_FLAG_SIGALLSYNC | J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION, 
		&rc);

	return JNI_ERR; /* if this function returns, it's an error! */
}

static int  JNICALL rasInjectOutOfMemory( JNIEnv *env )
{
    PORT_ACCESS_FROM_ENV(env);
    OMRPORT_FROM_J9PORT(PORTLIB)->mem_allocate_memory = &rasMemFailToAllocateMem;
    return JNI_OK; 
}

static int  JNICALL rasSetOutOfMemoryHook( JNIEnv *env, void (*OutOfMemoryFunc)(void) )
{
#if defined(J9VM_RAS_DUMP_AGENTS)
	J9VMThread *thr = (J9VMThread *)env;
	J9JavaVM *vm = thr->javaVM;
	J9RASdumpAgent *rda;
	omr_error_t rc = OMR_ERROR_NONE;

	PORT_ACCESS_FROM_VMC( thr );

	if ( OutOfMemoryFunc == NULL )
	{
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_RI_SETOUTOFMEMORYHOOK_NULL_CALLBACK);
		return JNI_EINVAL;
	}
	rda = (J9RASdumpAgent *) j9mem_allocate_memory( sizeof( J9RASdumpAgent ) , OMRMEM_CATEGORY_VM);
	if ( rda == NULL )
	{
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_RI_SETOUTOFMEMORYHOOK_ALLOC_RDA_FAILED);
		return JNI_ERR;
	}

	memset(rda, 0, sizeof(*rda));
	rda->nextPtr = NULL;
	rda->shutdownFn = rasDumpAgentShutdownFn;
	rda->eventMask = J9RAS_DUMP_ON_EXCEPTION_THROW;
	rda->detailFilter = "java/lang/OutOfMemoryError";
	rda->startOnCount = 1;
	rda->stopOnCount = 0;
	rda->labelTemplate = NULL;
	rda->dumpFn = oomHookFn;
	rda->dumpOptions = NULL;
	rda->userData = J9RAS_THUNK(OutOfMemoryFunc);
	rda->priority = 5;

	rc = vm->j9rasDumpFunctions->insertDumpAgent(vm, rda);

	return (int)omrErrorCodeToJniErrorCode(rc);
#else
	return JNI_ERR;
#endif
}

static int  JNICALL rasGetComponentDataArea( JNIEnv *env, char *componentName,
                                      							   void **dataArea, int *dataSize )
{
    J9VMThread *thr = (J9VMThread *)env;

    PORT_ACCESS_FROM_VMC( thr );
	
    j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_RI_NO_COMPONENT_DATA_AREA, componentName);
    return JNI_ERR;
}

int  JNICALL rasInitiateSystemDump( JNIEnv *env )
{
#if defined(J9VM_RAS_DUMP_AGENTS)
	J9VMThread *thr = (J9VMThread *)env;
	J9JavaVM *vm = thr->javaVM;
	omr_error_t rc = vm->j9rasDumpFunctions->triggerOneOffDump(vm, "system", DUMP_CALLER_NAME, NULL, 0);

	return (int)omrErrorCodeToJniErrorCode(rc);
#else
	return JNI_ERR;
#endif
}

static void JNICALL rasDynamicVerbosegc(JNIEnv *env, int vgc_switch,
                                    int vgccon, char* file_path,
                                    int number_of_files,
                                    int number_of_cycles)
{
    J9VMThread *thr = (J9VMThread *)env;
    J9MemoryManagerVerboseInterface *vrbFuncTable = 
      (J9MemoryManagerVerboseInterface *) thr->javaVM->memoryManagerFunctions->getVerboseGCFunctionTable(thr->javaVM);

    vrbFuncTable->configureVerbosegc(thr->javaVM, vgc_switch, file_path, number_of_files, number_of_cycles);
    return;
}

int  JNICALL rasGenerateHeapdump( JNIEnv *env )
{
#if defined(J9VM_RAS_DUMP_AGENTS)
	int rc = JNI_OK;
	J9VMThread *thr = (J9VMThread *)env;
	J9JavaVM *vm = thr->javaVM;
	omr_error_t result = OMR_ERROR_NONE;

	omrthread_monitor_enter(jvmridumpmonitor);
	
	result = vm->j9rasDumpFunctions->triggerOneOffDump(vm, "heap", DUMP_CALLER_NAME, NULL, 0);
	rc = (int)omrErrorCodeToJniErrorCode(result);

	omrthread_monitor_exit(jvmridumpmonitor);
	
	return rc;
#else
	return JNI_ERR;
#endif
}

int fillInDgRasInterface( DgRasInterface *dri )
{
    memcpy(dri->eyecatcher, "DGRI", 4);
    dri->length = sizeof( DgRasInterface );
    dri->version = JVMRAS_VERSION_1_5;
    dri->modification = 0;
	dri->TraceRegister = &rasTraceRegister;
	dri->TraceDeregister = &rasTraceDeregister;
	dri->TraceSet = &rasTraceSet;
	dri->TraceSnap = &rasTraceSnap;
	dri->TraceSuspend = &rasTraceSuspend;
	dri->TraceResume = &rasTraceResume;
	dri->GetRasInfo = &rasGetRasInfo;
	dri->ReleaseRasInfo = &rasReleaseRasInfo;
	dri->DumpRegister = &rasDumpRegister;
	dri->DumpDeregister = &rasDumpDeregister;
	dri->NotifySignal = &rasNotifySignal;
	dri->CreateThread = &rasCreateThread;
	dri->GenerateJavacore = &rasGenerateJavacore;
	dri->RunDumpRoutine = &rasRunDumpRoutine;
	dri->InjectSigsegv = &rasInjectSigsegv;
	dri->InjectOutOfMemory = &rasInjectOutOfMemory;
	dri->SetOutOfMemoryHook = &rasSetOutOfMemoryHook;
	dri->GetComponentDataArea = &rasGetComponentDataArea;
	dri->InitiateSystemDump = &rasInitiateSystemDump;
	dri->DynamicVerbosegc = &rasDynamicVerbosegc;
	dri->TraceSuspendThis = &rasTraceSuspendThis;
	dri->TraceResumeThis = &rasTraceResumeThis;
	dri->GenerateHeapdump = &rasGenerateHeapdump;
	dri->TraceRegister50 = &rasTraceRegister50;
	dri->TraceDeregister50 = &rasTraceDeregister50;

#if defined(DEBUG)
	printf("jvmri interface %p filled in\n", dri);
#endif

	return JNI_OK;
}

static int JNICALL rasTraceSet(JNIEnv *env, const char *cmd)
{
	J9JavaVM * vm =((J9VMThread *) (env))->javaVM;
	RasGlobalStorage * j9ras = (RasGlobalStorage *)vm->j9rasGlobalStorage;

	if ( j9ras && j9ras->configureTraceEngine) {
		/* Pass option to the trace engine */
		omr_error_t rc = j9ras->configureTraceEngine((J9VMThread *) (env), cmd);
		int jniRc = JNI_OK;
		switch (rc) {
		case OMR_ERROR_NONE:
			jniRc = JNI_OK;
			break;
		case OMR_ERROR_OUT_OF_NATIVE_MEMORY:
			jniRc = JNI_ENOMEM;
			break;
		case OMR_ERROR_ILLEGAL_ARGUMENT:
			jniRc = JNI_EINVAL;
			break;
		case OMR_ERROR_INTERNAL:
		default:
			jniRc = JNI_ERR;
			break;
		}
		return jniRc;
	} else {
		return JNI_ERR;
	}
}

void JNICALL rasTraceSnap(JNIEnv *env, char *buffer)
{
	J9VMThread *thr = (J9VMThread *)env;

#ifdef J9VM_RAS_DUMP_AGENTS

	J9JavaVM *vm = thr->javaVM;
	vm->j9rasDumpFunctions->triggerOneOffDump(vm, "snap", DUMP_CALLER_NAME, NULL, 0);

#else /* !J9VM_RAS_DUMP_AGENTS */

	J9UtServerInterface *utserverinterface = UTSINTERFACEFROMVM(thr->javaVM);
	UtThreadData **tempThr = UT_THREAD_FROM_VM_THREAD(thr);

	utserverinterface->TraceSnap(tempThr, NULL, NULL);

#endif /* !J9VM_RAS_DUMP_AGENTS */
}

static void JNICALL rasTraceSuspend(JNIEnv *env)
{
	J9VMThread *thr = (J9VMThread *)env;
	J9UtServerInterface *utserverinterface = UTSINTERFACEFROMVM(thr->javaVM);
	UtThreadData **tempThr = UT_THREAD_FROM_VM_THREAD(thr);

	utserverinterface->omrf.Suspend(tempThr, UT_SUSPEND_GLOBAL);
}

static void JNICALL rasTraceResume(JNIEnv *env)
{
	J9VMThread *thr = (J9VMThread *)env;
	J9UtServerInterface *utserverinterface = UTSINTERFACEFROMVM(thr->javaVM);
	UtThreadData **tempThr = UT_THREAD_FROM_VM_THREAD(thr);

	utserverinterface->omrf.Resume(tempThr, UT_RESUME_GLOBAL);
}

static int JNICALL rasDumpRegister(JNIEnv *env, int (JNICALL *func)(JNIEnv *env2, void **threadLocal, int reason))
{
#if defined(J9VM_RAS_DUMP_AGENTS)
	J9VMThread *thr = (J9VMThread *)env;
	J9JavaVM *vm = thr->javaVM;
	J9RASdumpAgent *rda;
	omr_error_t rc = OMR_ERROR_NONE;

	PORT_ACCESS_FROM_VMC( thr );

	if ( func == NULL )
	{
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_RI_DUMP_REGISTER_NULL_CALLBACK);
		return JNI_EINVAL;
	}
	rda = (J9RASdumpAgent *) j9mem_allocate_memory( sizeof( J9RASdumpAgent ) , OMRMEM_CATEGORY_VM);
	if ( rda == NULL )
	{
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_RI_DUMP_REGISTER_INTERNAL_ALLOC_ERROR);
		return JNI_ERR;
	}
	
	memset(rda, 0, sizeof(*rda));
	/* change this to filter on a java core when that facility is available. */
	/* for now just run when anything unexpected happens! */
	rda->nextPtr = NULL;
	rda->shutdownFn = rasDumpAgentShutdownFn;
	rda->eventMask = J9RAS_DUMP_ON_GP_FAULT | J9RAS_DUMP_ON_USER_SIGNAL | J9RAS_DUMP_ON_EXCEPTION_DESCRIBE;
	rda->detailFilter = "java/lang/OutOfMemoryError";
	rda->startOnCount = 1;
	rda->stopOnCount = 0;
	rda->labelTemplate = NULL;
	rda->dumpFn = rasDumpFn;
	rda->dumpOptions = NULL;
	rda->userData = J9RAS_THUNK(func);
	rda->priority = 5;

	rc = vm->j9rasDumpFunctions->insertDumpAgent(vm, rda);
	return (int)omrErrorCodeToJniErrorCode(rc);
#else
	return JNI_ERR;
#endif
}

static int JNICALL rasDumpDeregister(JNIEnv *env, int (JNICALL *func)(JNIEnv *env2, void **threadLocal, int reason))
{
	int rc = JNI_ERR;
#if defined(J9VM_RAS_DUMP_AGENTS)
	J9VMThread *thr = (J9VMThread *)env;
	J9JavaVM *vm = thr->javaVM;
	J9RASdumpAgent *rda = NULL;
							   
	while (vm->j9rasDumpFunctions->seekDumpAgent(vm, &rda, rasDumpFn) == OMR_ERROR_NONE) {
		if (rda->userData == J9RAS_THUNK(func))
		{
			rasDumpAgentShutdownFn( vm, &rda );
		}       
	}
	rc = JNI_OK;
#endif
	return rc;
}

static void JNICALL rasNotifySignal(JNIEnv *env, int signal)
{
 
#if defined(WIN32) || defined (WIN64)
    J9VMThread *thr = (J9VMThread *)env;
    PORT_ACCESS_FROM_VMC( thr );

    j9nls_printf(PORTLIB, J9NLS_INFO, J9NLS_RI_RASNOTIFYSIGNAL_RAISING_SIGNAL, signal); 
#endif /*defined(WIN32) || defined (WIN64)*/ 
 
    raise( signal );
    return; 
}

static void JNICALL rasTraceSuspendThis(JNIEnv *env)
{
	J9VMThread *thr = (J9VMThread *)env;
	J9UtServerInterface *utserverinterface = UTSINTERFACEFROMVM(thr->javaVM);
	UtThreadData **tempThr = UT_THREAD_FROM_VM_THREAD(thr);

	utserverinterface->omrf.Suspend(tempThr, UT_SUSPEND_THREAD);
}

static void JNICALL rasTraceResumeThis(JNIEnv *env)
{
	J9VMThread *thr = (J9VMThread *)env;
	J9UtServerInterface *utserverinterface = UTSINTERFACEFROMVM(thr->javaVM);
	UtThreadData **tempThr = UT_THREAD_FROM_VM_THREAD(thr);

	utserverinterface->omrf.Resume(tempThr, UT_RESUME_THREAD);
}

static omr_error_t oomHookFn(struct J9RASdumpAgent *agent, char *label, struct J9RASdumpContext *context){
#if defined(J9VM_RAS_DUMP_AGENTS)
	void (*OutOfMemoryFunc)(void) = ( void (*)(void) )agent->userData;

	OutOfMemoryFunc();

	return OMR_ERROR_NONE;
#else
	/* This is not valid without RAS_DUMP */
	return OMR_ERROR_INTERNAL;
#endif
}

static omr_error_t rasDumpFn(struct J9RASdumpAgent *agent, char *label, struct J9RASdumpContext *context){
#if defined(J9VM_RAS_DUMP_AGENTS)
	int (*dumpFn)(JNIEnv *, void **, int) = ( int (*)(JNIEnv *, void **, int) )agent->userData;
	J9VMThread *thr = context->onThread;
	JNIEnv *env = (JNIEnv *)thr;
	void **threadLocal = (void **) &thr;

	dumpFn(env, threadLocal, (int)context->eventFlags);

	return OMR_ERROR_NONE;
#else
	return OMR_ERROR_INTERNAL;
#endif
}

static omr_error_t
rasDumpAgentShutdownFn(struct J9JavaVM *vm, struct J9RASdumpAgent **agentPtr)
{
	omr_error_t rc = OMR_ERROR_INTERNAL;

#if defined(J9VM_RAS_DUMP_AGENTS)
	PORT_ACCESS_FROM_JAVAVM( vm );

	rc = vm->j9rasDumpFunctions->removeDumpAgent(vm, *agentPtr);
	
	j9mem_free_memory( *agentPtr );
	*agentPtr = NULL;
	rc = OMR_ERROR_NONE;
#endif

	return rc;
}

static int J9THREAD_PROC
rasThreadStartFuncWrapper( void *arg_struct )
{
	rasThreadArgs_t *arg_s = (rasThreadArgs_t *)arg_struct;
	J9JavaVM *vm = (J9JavaVM*)arg_s->vm;
	UDATA result = JNI_ERR;
	J9VMThread* vmThread = (J9VMThread*) vm->internalVMFunctions->currentVMThread(vm);

	PORT_ACCESS_FROM_JAVAVM(vm);

	j9sig_protect(
		rasThreadProtectedStartFuncWrapper, 
		arg_struct,
		vm->internalVMFunctions->structuredSignalHandler,
		vmThread,
		J9PORT_SIG_FLAG_SIGALLSYNC | J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION, 
		&result);

	return (int)result;
}

void
rasStartDeferredThreads(J9JavaVM* vm) 
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	RasGlobalStorage* rasGlobals = vm->j9rasGlobalStorage;
	rasDeferredThread_t* deferredRecord = rasGlobals->deferredJVMRIThreads;

	while (deferredRecord) {
		rasDeferredThread_t* next;

		rasCreateThreadImmediately(vm, deferredRecord->startFunc, deferredRecord->args);

		next = deferredRecord->next;
		j9mem_free_memory(deferredRecord);
		deferredRecord = next;
	}

	rasGlobals->deferredJVMRIThreads = NULL;

}

static int 
rasCreateThreadImmediately( J9JavaVM* vm, void (JNICALL *startFunc)(void*), void *args)
{
	int ret;
	omrthread_t  thread;
	rasThreadArgs_t t_args;

	t_args.args = args;
	t_args.startFunc = (void (JNICALL *)(void*)) J9RAS_THUNK(startFunc);
	t_args.vm = (JavaVM*)vm;
	t_args.started = JVMRI_THREAD_NOT_STARTED;

	if (omrthread_monitor_init_with_name(&t_args.monitor, 0, "jvmriCreateThread")) {
		PORT_ACCESS_FROM_JAVAVM(vm);
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_RI_CREATE_THREAD_CANT_ALLOCATE_MONITOR);
		return JNI_ERR;
	}

	omrthread_monitor_enter(t_args.monitor);

	ret = (int)vm->internalVMFunctions->createThreadWithCategory(
				&thread,
				0,
				J9THREAD_PRIORITY_NORMAL,
				0,
				rasThreadStartFuncWrapper,
				&t_args,
				J9THREAD_CATEGORY_SYSTEM_THREAD);

	if (ret == 0) {
		while (t_args.started == JVMRI_THREAD_NOT_STARTED) {
			omrthread_monitor_wait(t_args.monitor);
		}

		if (t_args.started != JVMRI_THREAD_STARTED) {
			ret = JNI_ERR;
		} else {
			ret = JNI_OK;
		}
	} else {
		ret = JNI_ERR;
	}

	omrthread_monitor_exit(t_args.monitor);
	omrthread_monitor_destroy(t_args.monitor);

	return ret;
}

static void *
rasMemFailToAllocateMem(OMRPortLibrary *portLibrary, UDATA bytes, const char *callSite, U_32 category)
{
	/* hardwired memory allocation failure */
	return NULL;
}

static UDATA
rasThreadProtectedStartFuncWrapper(J9PortLibrary* portLib, void *arg_struct )
{
	rasThreadArgs_t *arg_s = (rasThreadArgs_t *)arg_struct;
	JavaVM *vm = arg_s->vm;
	void (JNICALL *startFunc)(void*) = arg_s->startFunc;
	void* args = arg_s->args;
	JNIEnv *env = NULL;
	jint result;

	omrthread_monitor_enter(arg_s->monitor);

	result = (*vm)->AttachCurrentThread(vm, (void **)&env, NULL); 
	if (result != JNI_OK) {
		PORT_ACCESS_FROM_JAVAVM((J9JavaVM*)vm);
		j9tty_err_printf(PORTLIB, "J9RI0018: jvmri->CreateThread cannot attach new thread\n");

		arg_s->started = JVMRI_THREAD_ERROR;
		omrthread_monitor_notify(arg_s->monitor);
		omrthread_monitor_exit(arg_s->monitor);

		return 0;
	}

	arg_s->started = JVMRI_THREAD_STARTED;
	omrthread_monitor_notify(arg_s->monitor);
	omrthread_monitor_exit(arg_s->monitor);

	startFunc(args);

	(*vm)->DetachCurrentThread(vm);

	return 0;
}

int
initJVMRI( J9JavaVM * vm )
{
	if (omrthread_monitor_init_with_name(&jvmridumpmonitor, 0, "jvmriDumpThread")) {
		PORT_ACCESS_FROM_JAVAVM(vm);
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_RI_INITIALIZE_CANT_ALLOCATE_MONITOR);
		return JNI_ERR;
	}
	
	return JNI_OK;
}

int 
shutdownJVMRI( J9JavaVM * vm )
{
	if (jvmridumpmonitor != NULL){
		omrthread_monitor_destroy(jvmridumpmonitor);
	}
	jvmridumpmonitor = NULL;
	return JNI_OK;
}
