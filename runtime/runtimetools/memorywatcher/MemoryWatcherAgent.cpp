/*******************************************************************************
 * Copyright (c) 2011, 2019 IBM Corp. and others
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
#if defined(WIN32)
#ifdef __cplusplus
extern "C" {
#endif
#include <windows.h>
#include <stdio.h>
#include <psapi.h>
#ifdef __cplusplus
}
#endif
#endif

#include "MemoryWatcherAgent.hpp"

/* used to store reference to the agent */
static MemoryWatcherAgent* memoryWatcherAgent;

/**
 * Callback that handles printing out the memory categories
 *
 */
static UDATA memCategoryCallBack (U_32 categoryCode, const char * categoryName, UDATA liveBytes, UDATA liveAllocations, BOOLEAN isRoot, U_32 parentCategoryCode, OMRMemCategoryWalkState * state)
{
	MemoryWatcherAgent* agent = (MemoryWatcherAgent*) state->userData1;
	agent->message(categoryName);
	agent->message(":");
	agent->messageUDATA(liveBytes);
	agent->message("[");
	agent->messageIDATA(((IDATA)liveBytes)- (IDATA)(agent->getLastBytes()));
	agent->message("]");
	agent->messageUDATA(agent->getLastBytes());
	agent->message("\n");
	agent->setLastBytes(liveBytes);
	return J9MEM_CATEGORIES_KEEP_ITERATING;
}

/**
 * Callback used by to count memory categories with j9mem_walk_categories
 */
static UDATA
countMemoryCategoriesCallback (U_32 categoryCode, const char * categoryName, UDATA liveBytes, UDATA liveAllocations, BOOLEAN isRoot, U_32 parentCategoryCode, OMRMemCategoryWalkState * state)
{
	state->userData1 = (void *)((UDATA)state->userData1 + 1);
	return J9MEM_CATEGORIES_KEEP_ITERATING;
}

/*********************************************************
 * Methods exported for agent
 */
#ifdef __cplusplus
extern "C" {
#endif

void JNICALL
Agent_OnUnload(JavaVM * vm)
{
	MemoryWatcherAgent* agent = MemoryWatcherAgent::getAgent(vm, &memoryWatcherAgent);
	agent->shutdown();
	agent->kill();
}

jint JNICALL
Agent_OnLoad(JavaVM * vm, char * options, void * reserved)
{
	MemoryWatcherAgent* agent = MemoryWatcherAgent::getAgent(vm, &memoryWatcherAgent);
	return agent->setup(options);
}

jint JNICALL
Agent_OnAttach(JavaVM * vm, char * options, void * reserved)
{
	MemoryWatcherAgent* agent = MemoryWatcherAgent::getAgent(vm, &memoryWatcherAgent);
	return  agent->setup(options);
	/* need to add code here that will get jvmti_env and jni env and then call startGenerationThread */
}

#ifdef __cplusplus
}
#endif

/************************************************************************
 * Start of class methods
 *
 */

/**
 * This method is used to initialize an instance
 * @returns true if initialization was successful
 */
bool MemoryWatcherAgent::init()
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());

	if (!RuntimeToolsIntervalAgent::init()){
		return false;
	}

	_currentCategory = 0;
	_lastUsedVirtual = 0;
	_lastPeakUsedVirtual = 0;
	_lastWorkingSet = 0;
	_lastPeakWorkingSet = 0;
	_cls = NULL;
	_method = NULL;
	_dumpGenerated = false;
	_dumpThreshHold = 0;

	OMRMemCategoryWalkState walkState;

	memset(&walkState, 0, sizeof(OMRMemCategoryWalkState));

	walkState.walkFunction = countMemoryCategoriesCallback;
	walkState.userData1 = 0;
	j9mem_walk_categories(&walkState);
	I_32 total_categories = (I_32)((UDATA)walkState.userData1);
	_category_last = (UDATA*)j9mem_allocate_memory(sizeof(UDATA*)*total_categories, OMRMEM_CATEGORY_VM);
	if( NULL == _category_last ) {
		return false;
	}
	memset(_category_last,0,sizeof(U_32)*total_categories);

	return true;

}

/**
 * This method should be called to destroy the object and free the associated memory
 */
void MemoryWatcherAgent::kill()
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());
	j9mem_free_memory(_category_last);
	j9mem_free_memory(this);
}


/**
 * This method is used to create an instance
 *
 * @param vm java vm that can be used by this manager
 * @returns an instance of the class
 */
MemoryWatcherAgent* MemoryWatcherAgent::newInstance(JavaVM * vm)
{
	J9JavaVM * javaVM = ((J9InvocationJavaVM *)vm)->j9vm;
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	MemoryWatcherAgent* obj = (MemoryWatcherAgent*) j9mem_allocate_memory(sizeof(MemoryWatcherAgent), OMRMEM_CATEGORY_VM);
	if (obj){
		new (obj) MemoryWatcherAgent(vm);
		if (!obj->init()){
			obj->kill();
			obj = NULL;
		}
	}
	return obj;
}

/**
 * method that generates the calls to runAction at the appropriate intervals *.
 */
void MemoryWatcherAgent::runAction()
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());

	/* if this is the first time lookup the class/method id we need to generate the javacore */
	if (_method == NULL){
		_cls = getEnv()->FindClass("com/ibm/jvm/Dump");
		if (!getEnv()->ExceptionCheck()) {
			_method = getEnv()->GetStaticMethodID(_cls, "SystemDump", "()V");
			if (getEnv()->ExceptionCheck()) {
				_method = NULL;
				error("failed to lookup JavaDump method\n");
			}
		} else {
			error("failed to lookup Dump class\n");
		}
	}

	message("\n");
	_currentCategory = 0;
	OMRMemCategoryWalkState walkState;
	memset(&walkState, 0, sizeof(OMRMemCategoryWalkState));
	walkState.walkFunction = &memCategoryCallBack;
	walkState.userData1 = this;
	j9mem_walk_categories(&walkState);
	message("\n");

#if defined(XXXX)
	MEMORYSTATUSEX memStats;
	GlobalMemoryStatusEx(&memStats);
/*	UDATA availVirtual = (UDATA)(memStats.ullAvailPageFile/1024);
	message("Available virtual memory in K:");
	messageUDATA(availVirtual);
	message("/");
	messageUDATA((UDATA)(memStats.ullTotalPageFile/1024));
	message("[");
	messageIDATA((IDATA)(availVirtual-_lastAvailVirtual));
	message("]");
	_lastAvailVirtual = availVirtual;
	message("\n");
*/

	HANDLE process;
	PROCESS_MEMORY_COUNTERS_EX counters;
	process = OpenProcess(PROCESS_QUERY_INFORMATION |PROCESS_VM_READ,FALSE, GetCurrentProcessId());
	GetProcessMemoryInfo(process, (PPROCESS_MEMORY_COUNTERS)&counters, sizeof(counters));
	message("Working Set in K     :");
	UDATA workingSet = counters.WorkingSetSize/1024;
	UDATA peakWorkingSet = counters.PeakWorkingSetSize/1024;
//	UDATA usedVirtual = (UDATA) (counters.PrivateUsage/1024);
//	UDATA peakUsedVirtual = (UDATA) (counters.PeakPagefileUsage/1024);
	messageUDATA(counters.WorkingSetSize);
	message("[");
	messageIDATA((IDATA)(workingSet-_lastWorkingSet));
	message("]");
	message("\n");
	_lastWorkingSet=workingSet;
	message("Peak Working Set in K:");
	messageUDATA(counters.PeakWorkingSetSize);
	message("[");
	messageIDATA((IDATA)(peakWorkingSet-_lastPeakWorkingSet));
	message("]");
	message("\n");
	_lastPeakWorkingSet=peakWorkingSet;

/*	message("Virtual memory in K   :");
	messageUDATA(usedVirtual);
	message("/");
	messageUDATA((UDATA)(0));
	message("[");
	messageIDATA((IDATA)(usedVirtual-_lastUsedVirtual));
	message("]");
	_lastUsedVirtual = usedVirtual;
	message("\n");
	message("Peak virtual memory in K:");
	messageUDATA(peakUsedVirtual);/dump
	message("/");
	messageUDATA((UDATA)(0));
	message("[");
	messageIDATA((IDATA)(peakUsedVirtual-_lastPeakUsedVirtual));
	message("]");
	_lastPeakUsedVirtual = peakUsedVirtual;
	message("\n");
*/
	CloseHandle(process);

#endif
	message("Threshhold:");
	messageIDATA(_dumpThreshHold);
	message("\n");

#if defined(J9ZOS390)

	int  ascb, rax;
	IDATA raxfmct;

	ascb    = *((int*) 0x224);
	rax     = *((int*) (ascb + 0x16c));
	raxfmct = *((int*) (rax + 0x2c));
	message("Total Real Frames Being Used =");
	messageIDATA(raxfmct);
	message("\n");

	/* generate a system dump */
	if ((_dumpGenerated == false)&&(_dumpThreshHold!=0)&&(raxfmct >_dumpThreshHold)){
		_dumpGenerated = true;
		if (_method != NULL){
			getEnv()->CallStaticVoidMethod(_cls, _method);
			message("Dump Requested");
		}
	}
#endif
}

/**
 * This is called once for each argument, the agent should parse the arguments
 * it supports and pass any that it does not understand to parent classes
 *
 * @param options string containing the argument
 *
 * @returns one of the AGENT_ERROR_ values
 */
jint MemoryWatcherAgent::parseArgument(char* option)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());
	jint rc = AGENT_ERROR_NONE;

	/* check for threshHold option */
	if (try_scan(&option,"dumpThreshHold:")){
		if (1 != sscanf(option,"%d",&_dumpThreshHold)){
			error("invalid dumpThreshold value passed to agent\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		}
	} else {
		rc = RuntimeToolsIntervalAgent::parseArgument(option);
	}

	return rc;
}
