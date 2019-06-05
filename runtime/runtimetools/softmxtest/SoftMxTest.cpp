/*******************************************************************************
 * Copyright (c) 2012, 2019 IBM Corp. and others
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

#include "SoftMxTest.hpp"

#include "mmomrhook.h"
#include "vmaccess.h"

/* used to store reference to the agent */
static SoftMxTest* softMxTest;

#ifdef __cplusplus
extern "C" {
#endif

/* constants */
#define SOFTMX_EXPAND_AMOUNT  (25*1024*1024)

/**
 * callback invoked by JVMTI when the agent is unloaded
 * @param vm [in] jvm that can be used by the agent
 */
void JNICALL
Agent_OnUnload(JavaVM * vm)
{
	SoftMxTest* agent = SoftMxTest::getAgent(vm, &softMxTest);
	agent->shutdown();
	agent->kill();
}

/**
 * callback invoked by JVMTI when the agent is loaded
 * @param vm       [ in] jvm that can be used by the agent
 * @param options   [in] options specified on command line for agent
 * @param reserved [in/out] reserved for future use
 */
jint JNICALL
Agent_OnLoad(JavaVM * vm, char * options, void * reserved)
{
	SoftMxTest* agent = SoftMxTest::getAgent(vm, &softMxTest);
	return agent->setup(options);
}

/**
 * callback invoked when an attach is made on the jvm
 * @param vm        [in] jvm that can be used by the agent
 * @param options   [in] options specified on command line for agent
 * @param reserved [in/out] reserved for future use
 */
jint JNICALL
Agent_OnAttach(JavaVM * vm, char * options, void * reserved)
{
	SoftMxTest* agent = SoftMxTest::getAgent(vm, &softMxTest);
	return  agent->setup(options);
}

/**
 * function that we add to the gc hook to catch when softmx would have caused an OOM
 * 
 * @param hook hook interface that can be used by the function
 * @param number of the event which occured
 * @param eventData structure with the values passed to the hook
 * @param userData  opaque pointer passed in when hook was registered
 */
static void
hookSoftmxOOMEvent(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_SoftmxOOMEvent *event = (MM_SoftmxOOMEvent*)eventData;
	SoftMxTest* agent = (SoftMxTest*) userData;
	agent->adjustSoftmxForOOM(event->timestamp, event->maxHeapSize, event->currentHeapSize, event->currentSoftMX, event->bytesRequired);
}

#ifdef __cplusplus
}
#endif

/************************************************************************
 * Start of class methods
 *
 */

bool 
SoftMxTest::init(void)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());

	if (!RuntimeToolsIntervalAgent::init()){
		return false;
	}

	_gcOmrHooks = getJavaVM()->memoryManagerFunctions->j9gc_get_omr_hook_interface(getJavaVM()->omrVM);
	if (_gcOmrHooks == NULL){
		return false;
	}

	_testCallbackOOM = false;
	_testCallbackExpand = false;

	return true;

}

void 
SoftMxTest::kill(void)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());

	j9mem_free_memory(this);
}

SoftMxTest*
SoftMxTest::newInstance(JavaVM * vm)
{
	J9JavaVM * javaVM = ((J9InvocationJavaVM *)vm)->j9vm;
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	SoftMxTest* obj = (SoftMxTest*) j9mem_allocate_memory(sizeof(SoftMxTest), OMRMEM_CATEGORY_VM);
	if (obj){
		new (obj) SoftMxTest(vm);
		if (!obj->init()){
			obj->kill();
			obj = NULL;
		}
	}
	return obj;
}

void 
SoftMxTest::runAction(void)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());
}

jint 
SoftMxTest::setup(char * options){
	jint rc = RuntimeToolsIntervalAgent::setup(options);
	if (rc == 0){
		if (-1 == (*_gcOmrHooks)->J9HookRegisterWithCallSite(_gcOmrHooks, J9HOOK_MM_OMR_OOM_DUE_TO_SOFTMX, hookSoftmxOOMEvent, OMR_GET_CALLSITE(), (void*) this)) {
			message("Failed to register GC hook");
			return -1;
		}
	}
	return rc;
}


jint 
SoftMxTest::parseArgument(char* option)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());
	jint rc = AGENT_ERROR_NONE;

	if (try_scan(&option,"testCallback:OOM")){
		_testCallbackOOM = true;
		return rc;
	} else if (try_scan(&option,"testCallback:Expand")){
		_testCallbackExpand = true;
		return rc;
	} else {
		/* must give super class agent opportunity to handle option */
		rc = RuntimeToolsIntervalAgent::parseArgument(option);
	}

	return rc;
}

void
SoftMxTest::adjustSoftmxForOOM(U_64 timestamp, UDATA maxHeapSize, UDATA currentHeapSize, UDATA currentSoftMx, UDATA bytesRequired)
{
	if (_testCallbackOOM){
		message("SoftMxTest: Softmx OOM Callback triggered, expect OOM\n");
		fflush(stdout);
		fflush(stderr);
	}

	if (_testCallbackExpand){
		UDATA newSoftMx = OMR_MIN(OMR_MAX(currentHeapSize,currentSoftMx) + OMR_MAX(bytesRequired,SOFTMX_EXPAND_AMOUNT),maxHeapSize);
		getJavaVM()->memoryManagerFunctions->j9gc_set_softmx( getJavaVM(),newSoftMx);
		message("SoftMxTest: Softmx OOM Callback triggered, adjusted softmx up to avoid OOM:");
		messageUDATA(newSoftMx);
		message("\n");
		fflush(stdout);
		fflush(stderr);
	}
}

