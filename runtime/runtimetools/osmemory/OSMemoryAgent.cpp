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

#include "OSMemoryAgent.hpp"

#if defined(AIXPPC)	&& !defined(J9OS_I5)
// IBM i does not support libperfstat, and does not plan support it in future.
// Just disable it together with the sync with hive.
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <libperfstat.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

/* used to store reference to the agent */
static OSMemoryAgent* oSMemoryAgent;

#ifdef __cplusplus
extern "C" {
#endif

/* constants */
#define TARGET_SLACK 1024*1024*5
#define SOFTMX_GROW_INCREMENT 25*1024*1024
#define MIN_FREE_HEAP 25*1024*1024
#define DEFAULT_MAX_GCS 2
#define DEFAULT_GC_INTERVAL 2000
#define LOTS_OF_FREE_MEMORY 100*1024*1024
#define MAX_SOFTMX_DECREMENT 100*1024*1024
#define FREE_UNCHANGED_OVERRIDE 10
#define GIG 1024*1024*1024
#define MEG 1024*1024
#define SHARED_POOL_MEMORY GIG*2.5

/* Defaults for HPI */
#define HPI_DISABLED 		   100000000
#define DEFAULT_HPI_TARGET	   HPI_DISABLED
#define DEFAULT_HPITIME_TARGET 100000000

/* ratios for softmx adjustment */
#define SOFTMX_BELOW_DOUBLE_TARGET_RATIO 	0
#define SOFTMX_BELOW_TARGET_RATIO 			1
static  U_64 SOFTMX_RATIO[] = {75,50};

/* defines for hive communication, for initial experimentation these were
 * simply hard coded 
 */
#define MULTICAST_PORT 				25000
#define MULTICAST_GROUP				"225.0.0.37"

/* defines for errors when using multicast sockets between JVMs */
#define FAILED_TO_CREATE_SOCKET 	-1001
#define FAILED_TO_BIND_SOCKET   	-1002
#define FAILED_TO_JOIN_GROUP		-1003

/**
 * callback invoked by JVMTI when the agent is unloaded
 * @param vm [in] jvm that can be used by the agent
 */
void JNICALL
Agent_OnUnload(JavaVM * vm)
{
	OSMemoryAgent* agent = OSMemoryAgent::getAgent(vm, &oSMemoryAgent);
	agent->shutdown();
	agent->kill();
}

/**
 * callback invoked by JVMTI when the agent is loaded
 * @param vm       [in] jvm that can be used by the agent
 * @param options  [in] options specified on command line for agent
 * @param reserved [in/out] reserved for future use
 */
jint JNICALL
Agent_OnLoad(JavaVM * vm, char * options, void * reserved)
{
	OSMemoryAgent* agent = OSMemoryAgent::getAgent(vm, &oSMemoryAgent);
	return agent->setup(options);
}

/**
 * callback invoked when an attach is made on the jvm
 * @param vm       [in] jvm that can be used by the agent
 * @param options  [in] options specified on command line for agent
 * @param reserved [in/out] reserved for future use
 */
jint JNICALL
Agent_OnAttach(JavaVM * vm, char * options, void * reserved)
{
	OSMemoryAgent* agent = OSMemoryAgent::getAgent(vm, &oSMemoryAgent);
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
bool 
OSMemoryAgent::init(void)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());

	if (!RuntimeToolsIntervalAgent::init()){
		return false;
	}

	_adjustSoftMx = false;
	_lastSoftMx = 0;
	_maxHeapSize = getJavaVM()->memoryManagerFunctions->j9gc_get_maximum_heap_size(getJavaVM());
	_currentSoftMx = _maxHeapSize;
	_freeMemoryTarget = 0;
	_gcs_initiated = 0;
	_lastHeapSize = 0;
	_maxgcs = DEFAULT_MAX_GCS;
	_quiet = false;
	_gcInterval = DEFAULT_GC_INTERVAL;
	_startDelay = 0;
	_firstInvocation = true;
	_freeUnchanged = 0;
	_lastFree = _maxHeapSize;
#if defined(AIXPPC)	&& !defined(J9OS_I5)
	_hpiTarget = DEFAULT_HPI_TARGET;
	_hpiTimeTarget = DEFAULT_HPITIME_TARGET;
	_totalFreePhysicalTarget = 0;
	_multicastSocketIn = -1;
	_jvmId = -1;
	memset(_hpiValues,0,sizeof(long)*MAX_JVMS_IN_HIVE);
	memset(_assignedPhysical,0,sizeof(long)*MAX_JVMS_IN_HIVE);
	_physicalDropping = false;
#endif

	return true;

}

/**
 * This method should be called to destroy the object and free the associated memory
 */
void 
OSMemoryAgent::kill(void)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());

	j9mem_free_memory(this);
}


/**
 * This method is used to create an instance
 *
 * @param vm [in] java vm that can be used by this manager
 * @returns an instance of the class
 */
OSMemoryAgent* 
OSMemoryAgent::newInstance(JavaVM * vm)
{
	J9JavaVM * javaVM = ((J9InvocationJavaVM *)vm)->j9vm;
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	OSMemoryAgent* obj = (OSMemoryAgent*) j9mem_allocate_memory(sizeof(OSMemoryAgent), OMRMEM_CATEGORY_VM);
	if (obj){
		new (obj) OSMemoryAgent(vm);
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
void 
OSMemoryAgent::runAction(void)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());
	J9MemoryInfo memoryInfo;
	I_32 result = 0;
	J9JavaVM *vm = getJavaVM();
	bool first = true;
	UDATA heapTotalMemory = 0;
	UDATA freeMemory = 0;

	if ((_firstInvocation)&&(_startDelay != 0)){
		omrthread_sleep(_startDelay);
		_firstInvocation = false;
	}

	heapTotalMemory = heapTotalMemory = vm->memoryManagerFunctions->j9gc_heap_total_memory(vm);
	freeMemory = vm->memoryManagerFunctions->j9gc_heap_free_memory(vm);

	/* we need to loop in the case were we have set a target but the heap has not yet contracted to that
	 * target as we have to force gcs so that the heap contracts.  The interval between gcs needs to be
	 * shorter than the polling interface to make the contraction happen fast enough so we introduce a loop
	 * were and special case the first time we enter it when the runAction() is called for each polling interval
	 */
	while(first ||((heapTotalMemory > (_currentSoftMx + TARGET_SLACK ))&&(_gcs_initiated < _maxgcs))){
#if defined(AIXPPC)	&& !defined(J9OS_I5)
		u_longlong_t currentHPI = 0;
		u_longlong_t currentHPITime = 0;
		PerfStatData perfstatData;
		u_longlong_t tempHPI = _lastHPI;
		u_longlong_t tempHPITime = _lastHPITime;
		u_longlong_t tempAssignedPhysical = _lastAssignedPhysical;
		longlong_t physicalDecrease = 0;
		memset(&perfstatData,0,sizeof(perfstatData));
#endif
		I_64 currentRunTime = j9time_current_time_millis();

		if (first){
			first = false;
		} else {
			// wait for interval between gc
			if (_gcInterval != 0){
				omrthread_sleep(_gcInterval);
			}
			heapTotalMemory = vm->memoryManagerFunctions->j9gc_heap_total_memory(vm);
			freeMemory = vm->memoryManagerFunctions->j9gc_heap_free_memory(vm);
		}

		/* we need to keep track of how much free memory there was on the heap and if it is changing so
		 * capture it here
		 */
		if (_lastFree  >= freeMemory){
			_freeUnchanged++;
		} else {
			_lastFree = freeMemory;
			_freeUnchanged = 0;
		}

		/* get the memory info, if we can't get this then we are not going to be able to do anything */
		result = j9sysinfo_get_memory_info(&memoryInfo);

#if defined(AIXPPC)	&& !defined(J9OS_I5)
		getPerfStatData(&perfstatData);
		_lastHPI = perfstatData.hpi;
		_lastHPITime = perfstatData.hpit;
		if (perfstatData.pmem < _lastAssignedPhysical){
			physicalDecrease = _lastAssignedPhysical - perfstatData.pmem;
		}
		_lastAssignedPhysical = perfstatData.pmem;

		if ((tempHPI != 0)&&(_lastRunTime != 0)) {
			currentHPI = (_lastHPI - tempHPI)/((currentRunTime - _lastRunTime)/1000);
			currentHPITime = ((_lastHPITime - tempHPITime)/((currentRunTime - _lastRunTime)/1000))/1000000;
		} else {
			currentHPI = 0;
			currentHPITime =0;
		}

		if (_jvmId != -1){
			HiveData data;
			data.jvmId = _jvmId;
			data.currentHpi = currentHPI;
			data.assignedPhysical = perfstatData.pmem;

			/* broadcast our new values to the rest of the hive */
			broadcastToHive(&data);

			/* update the local hive copy with our updated data */
			_hpiValues[_jvmId] =  currentHPI;
			_assignedPhysical[_jvmId] = perfstatData.pmem;

			/* now read all pending broadcast messages to populate data from other sources
			 * and build the consolidated values
			 */
			updateFromHive();

			/* at this point we replace the currentHpi with the combined one as that is what we want
			 * to use for adjustments as opposed to just the HPI for this partition
			 */
			currentHPI = _totalHPI;
		}
#endif

		if (0 == result){
			if (_adjustSoftMx){
				UDATA newSoftMx = _currentSoftMx;

				if ((_currentSoftMx <= heapTotalMemory) 
				&& ((_lastHeapSize < heapTotalMemory) || ((_lastHeapSize == heapTotalMemory) && (0 == freeMemory) && (_freeUnchanged > 1)))
				) {
					/* if the size of the heap is growing, and is greater than the current softmx value
					 * we better get out of the way to avoid causing an OOM
					 */
					newSoftMx = OMR_MIN(heapTotalMemory + SOFTMX_GROW_INCREMENT,_maxHeapSize);
					vm->memoryManagerFunctions->j9gc_set_softmx( vm, newSoftMx);
				} else if (   (0 != _freeMemoryTarget)&&(memoryInfo.availPhysical <= 2*_freeMemoryTarget)&&((freeMemory> LOTS_OF_FREE_MEMORY)||(FREE_UNCHANGED_OVERRIDE < _freeUnchanged))
#if defined(AIXPPC)	&& !defined(J9OS_I5)
						    ||((_hpiTarget != HPI_DISABLED)&&(currentHPI > _hpiTarget)&&(freeMemory> LOTS_OF_FREE_MEMORY))
						    ||((_physicalDropping)&&(physicalDecrease > 0) && (freeMemory> LOTS_OF_FREE_MEMORY))
						    ||((_totalFreePhysicalTarget != 0) && ( _totalPhysical > 0)&&((GIG*2.5 - _totalPhysical) < (MEG*_totalFreePhysicalTarget)))
#endif
						  ){
#if defined(AIXPPC)	&& !defined(J9OS_I5)
					if (_hpiTarget != HPI_DISABLED){
						if (  (currentHPI > 2* _hpiTarget)
							||(currentHPITime > 2* _hpiTimeTarget)
						   ){
							newSoftMx = (UDATA)( ((U_64)_maxHeapSize*SOFTMX_RATIO[SOFTMX_BELOW_TARGET_RATIO])/100);
							message("Adjusted based on hpiTarget\n");
						} else if (  (currentHPI > _hpiTarget)
								   ||(currentHPITime > _hpiTimeTarget)
								  ){
							newSoftMx = (UDATA)( ((U_64)_maxHeapSize*SOFTMX_RATIO[SOFTMX_BELOW_DOUBLE_TARGET_RATIO])/100);
							message("Adjusted based on hpiTarget\n");
						}
					} else if ((_physicalDropping)&&(physicalDecrease > 0)) {
						newSoftMx = (UDATA)( ((U_64)_maxHeapSize*SOFTMX_RATIO[SOFTMX_BELOW_DOUBLE_TARGET_RATIO])/100);
						message("Adjusted based on physical decrease\n");
					} else if ((_totalFreePhysicalTarget != 0) && ( _totalPhysical > 0)){
						if ((SHARED_POOL_MEMORY - _totalPhysical) < (MEG*_totalFreePhysicalTarget) ) {
							if ((SHARED_POOL_MEMORY - _totalPhysical) < ((MEG*_totalFreePhysicalTarget)/2) ){
								newSoftMx = (UDATA)( ((U_64)_maxHeapSize*SOFTMX_RATIO[SOFTMX_BELOW_TARGET_RATIO])/100);
							} else {
								newSoftMx = (UDATA)( ((U_64)_maxHeapSize*SOFTMX_RATIO[SOFTMX_BELOW_DOUBLE_TARGET_RATIO])/100);
							}
							message("Adjusted based on total physical\n");
						}
					} else if ((_freeMemoryTarget != 0)&&(memoryInfo.availPhysical <= 2*_freeMemoryTarget)){
#endif
						if (memoryInfo.availPhysical <= _freeMemoryTarget){
							newSoftMx = (UDATA)( ((U_64)_maxHeapSize*SOFTMX_RATIO[SOFTMX_BELOW_TARGET_RATIO])/100);
							message("Adjusted based on free memory target\n");
						} else {
							newSoftMx = (UDATA)( ((U_64)_maxHeapSize*SOFTMX_RATIO[SOFTMX_BELOW_DOUBLE_TARGET_RATIO])/100);
							message("Adjusted based on free memory target\n");
						}
#if defined(AIXPPC)	&& !defined(J9OS_I5)
					}
#endif

					/* in order to avoid OOM conditions we avoid adjusting the softmx value too radically in respect
					 * to the amount of memory free in the heap
					 */
					if (((IDATA)_currentSoftMx - (IDATA)newSoftMx) > MAX_SOFTMX_DECREMENT){
						newSoftMx = OMR_MAX(newSoftMx, _currentSoftMx - freeMemory/2);
					}

					/* if we have a new target then we need to set it and trigger a gc to start the process of
					 * moving towards it
					 */
					if (newSoftMx != _currentSoftMx){
						vm->memoryManagerFunctions->j9gc_set_softmx( vm, newSoftMx);
						if (newSoftMx < heapTotalMemory){
							if (!_quiet){
								message("OSMemoryAgent: Triggering initial gc after softmx adjustment\n");
							}
							vm->internalVMFunctions->internalAcquireVMAccess((J9VMThread *)getEnv());
							vm->memoryManagerFunctions->j9gc_modron_global_collect_with_overrides((J9VMThread*)getEnv(),  J9MMCONSTANT_IMPLICIT_GC_AGGRESSIVE);
							vm->internalVMFunctions->internalReleaseVMAccess((J9VMThread *)getEnv());
						}
					}
				} else {
					if (   (  (freeMemory> LOTS_OF_FREE_MEMORY)
							||(FREE_UNCHANGED_OVERRIDE < _freeUnchanged)
#if defined(AIXPPC)	&& !defined(J9OS_I5)
							||((_hpiTarget != HPI_DISABLED)&&(currentHPI < _hpiTarget))
							||(_physicalDropping && (physicalDecrease  == 0))
							||(  (_totalFreePhysicalTarget != 0) 
							   &&((GIG*2.5 - _totalPhysical) > (MEG*_totalFreePhysicalTarget)))
#endif
						   )
						 &&(_currentSoftMx < _maxHeapSize)
						){
						// this is lots of memory and we are above target so allow softmx to grow back to max
						newSoftMx = OMR_MIN(_currentSoftMx + SOFTMX_GROW_INCREMENT,_maxHeapSize);
						vm->memoryManagerFunctions->j9gc_set_softmx( vm, newSoftMx);
					}
				}

				if (_currentSoftMx != newSoftMx){
					/* we adjusted the softmx and already triggered a gc, so simply update and reset stats */
					_currentSoftMx = newSoftMx;
					_gcs_initiated = 0;
				} else {
					/*
					 * ok there was no change in target, now let us see if the heap has actually gone down to our previous target
					 * if not we will trigger a gc to attempt to force this.  If after _maxgcs attempts the heap size has
					 * not changed, we give up to avoid triggering too many gcs
					 */
					if ((_gcs_initiated < _maxgcs)&&(freeMemory>LOTS_OF_FREE_MEMORY)){
						// is the size around the target ?
						if (heapTotalMemory > (_currentSoftMx + TARGET_SLACK )){
							// we are not yet at the target so trigger another gc to see if that helps get us there
							vm->internalVMFunctions->internalAcquireVMAccess((J9VMThread *)getEnv());
							vm->memoryManagerFunctions->j9gc_modron_global_collect_with_overrides((J9VMThread*)getEnv(),  J9MMCONSTANT_IMPLICIT_GC_AGGRESSIVE);
							vm->internalVMFunctions->internalReleaseVMAccess((J9VMThread *)getEnv());
							if (_lastHeapSize == heapTotalMemory) {
								_gcs_initiated++;
							} else {
								_gcs_initiated = 0;
							}
							if (!_quiet){
								message("OSMemoryAgent: Not at target, initiating gc\n");
							}
						}
					}
				/*	else if ((_currentSoftMx != _maxHeapSize)&&(_currentSoftMx <heapTotalMemory)&&(MIN_FREE_HEAP > freeMemory)) {
						_currentSoftMx = OMR_MIN(heapTotalMemory + SOFTMX_GROW_INCREMENT,_maxHeapSize);
						vm->memoryManagerFunctions->j9gc_set_softmx( vm, _currentSoftMx);
					}*/
				}
			}

			if (!_quiet){
				message("OSMemoryAgent(");
				messageU64(currentRunTime);
				message("}:Physical(");
				messageU64(memoryInfo.availPhysical);
				message("/");
				messageU64((memoryInfo.totalPhysical));
				message("),Heap(");
				messageU64(heapTotalMemory);
				message("/");
				messageU64(_currentSoftMx);
				message("),HeapFree(");
				messageU64(freeMemory);
#if defined(AIXPPC)	&& !defined(J9OS_I5)
				message("), currentHPI(");
				messageU64(currentHPI);
				message("), currentHPITime(");
				messageU64(currentHPITime);
				message("), assignedPhysical(");
				messageU64(_lastAssignedPhysical);
				message("), physicalDecrease(");
				messageU64(physicalDecrease);
				if (_jvmId != -1){
					message("), totalPhysical(");
					messageU64(_totalPhysical);
				}
#endif
				message(")\n");
			}

			_lastHeapSize = heapTotalMemory;
			_lastRunTime = currentRunTime;
		} else {
			message("Failed to get memory info\n");
		}
	}
}

jint 
OSMemoryAgent::setup(char * options){
	jint rc = RuntimeToolsIntervalAgent::setup(options);
	if (rc == 0){
#if defined(AIXPPC)	&& !defined(J9OS_I5)
		/* if either of the "total" options which require information across JVMs is enabled
		 * then join the multicast group
		 */
		if (_jvmId != -1){
			struct sockaddr_in multicastAddrIn;
			struct ip_mreq multicastReq;
			int flags = 0;
			memset(&multicastAddrIn,0,sizeof(sockaddr_in));
			memset(&multicastReq,0,sizeof(multicastReq));

			/* Currently we don't all socket re-use in case there are multiple JVMs
			 * which limits us to a single JVM
			 */

			/* set up the input socket and bind to it */
			_multicastSocketIn = socket(AF_INET,SOCK_DGRAM,0);
			flags = fcntl(_multicastSocketIn,F_GETFL);
			fcntl(_multicastSocketIn, F_SETFL,flags | O_NONBLOCK);
			if (_multicastSocketIn <0){
				message("Failed to create/set nonblock on socket\n");
				return FAILED_TO_CREATE_SOCKET;
			}
			multicastAddrIn.sin_family=AF_INET;
			multicastAddrIn.sin_addr.s_addr=htonl(INADDR_ANY);
			multicastAddrIn.sin_port=(MULTICAST_PORT);
			rc = bind(_multicastSocketIn,(struct sockaddr*)& multicastAddrIn,sizeof(multicastAddrIn));
			if (rc <0){
				message("Failed to bind socket\n");
				return FAILED_TO_BIND_SOCKET;
			}
			multicastReq.imr_multiaddr.s_addr=inet_addr(MULTICAST_GROUP);
			multicastReq.imr_interface.s_addr=htonl(INADDR_ANY);
		    if (setsockopt(_multicastSocketIn,IPPROTO_IP,IP_ADD_MEMBERSHIP,&multicastReq,sizeof(multicastReq)) < 0) {
		    	message("Failed to join the group\n");
		    	return FAILED_TO_JOIN_GROUP;
		    }
		}
#endif
	}
	return rc;
}


jint 
OSMemoryAgent::parseArgument(char* option)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());
	jint rc = AGENT_ERROR_NONE;

	/* check for threshHold option */
	if (try_scan(&option,"maxgcs")){
		if (1 != sscanf(option,"%d",&_maxgcs)){
			error("invalid maxgcs value passed to agent\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		} else if (_maxgcs < 0){
			error("negative maxgcs is invalid\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		}
	} else if (try_scan(&option,"adjustSoftMx")){
		_adjustSoftMx = true;
		return rc;
	} else if (try_scan(&option,"quiet")){
		_quiet = true;
		return rc;
	} else if (try_scan(&option,"freeMemoryTarget:")){
		int tempValue = 0;
		if (1 != sscanf(option,"%d",&tempValue)){
			error("invalid freeMemoryTarget value passed to agent\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		} else if (tempValue < 0){
			error("negative freeMemoryTarget is invalid\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		}
		/* convert to MB */
		_freeMemoryTarget = tempValue*1024*1024;
	} else if (try_scan(&option,"gcinterval:")){
		if (1 != sscanf(option,"%d",&_gcInterval)){
			error("invalid gcinvertal value passed to agent\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		} else if (_gcInterval < 0){
			error("negative gcinterval is invalid\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		}
	} else if (try_scan(&option,"startDelay:")){
		if (1 != sscanf(option,"%d",&_startDelay)){
			error("invalid startDelay value passed to agent\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		} else if (_startDelay < 0){
			error("negative startDelay is invalid\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		}
	}
#if defined(AIXPPC)	&& !defined(J9OS_I5)
	else if (try_scan(&option,"hpiTarget:")){
		if (1 != sscanf(option,"%d",&_hpiTarget)){
			error("invalid hpiTarget value passed to agent\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		} else if (_hpiTarget < 0){
			error("negative hpiTarget is invalid\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		}
	} else if (try_scan(&option,"hpiTargetTime:")){
		if (1 != sscanf(option,"%d",&_hpiTimeTarget)){
			error("invalid hpiTargetTime value passed to agent\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		} else if (_hpiTimeTarget < 0){
			error("negative hpiTargetTime is invalid\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		}
	} else if (try_scan(&option,"totalFreePhysicalTarget:")){
		if (1 != sscanf(option,"%d",&_totalFreePhysicalTarget)){
			error("invalid totalFreePhysicalTarget value passed to agent\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		} else if (_totalFreePhysicalTarget < 0){
			error("negative totalFreePhysicalTarget is invalid\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		}
	} else if (try_scan(&option,"jvmId:")){
		if (1 != sscanf(option,"%d",&_jvmId)){
			error("invalid jvmId value passed to agent\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		} else if (_jvmId < 0){
			error("negative jvmId is invalid\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		}
	} else if (try_scan(&option,"physicalDropping")){
		_physicalDropping = true;
	}
#endif
	else {
		/* must give super class agent opportunity to handle option */
		rc = RuntimeToolsIntervalAgent::parseArgument(option);
	}

	return rc;
}

#if defined(AIXPPC)	&& !defined(J9OS_I5)
void 
OSMemoryAgent::getPerfStatData(PerfStatData* data)
{
	memset(data, 0, sizeof(PerfStatData));
	perfstat_partition_total_t perfinfo;
	int result = perfstat_partition_total(NULL, &perfinfo, sizeof(perfinfo), 1);
	if (result == 1){
		data->hpi  = perfinfo.hpi;
		data->hpit = perfinfo.hpit;
		data->pmem = perfinfo.pmem;
	} else {
		error("Failed to update perfstat data\n");
	}
}

void 
OSMemoryAgent::broadcastToHive(HiveData* data){
	struct sockaddr_in multicastAddrOut;
	multicastAddrOut.sin_family = AF_INET;
	multicastAddrOut.sin_addr.s_addr =  inet_addr(MULTICAST_GROUP);
	multicastAddrOut.sin_port = htonl(MULTICAST_PORT);
	if (sendto(_multicastSocketIn,data,sizeof(HiveData),0,(struct sockaddr *) &multicastAddrOut, sizeof(multicastAddrOut))<0){
		error("Failed to broadcast to hive\n");
	}
}

void 
OSMemoryAgent::updateFromHive(void){
	/* read each of the outstanding messages and update the hive values */
	struct sockaddr_in fromAddress;
	socklen_t addressLength = sizeof(fromAddress);
	HiveData data;
	ssize_t bytesReceived = 0;
	int i = 0;
	memset((void*)&data,0,sizeof(HiveData));

	/* !not safe need to handle cases were we could read in multiple chunks */
	while ((bytesReceived=recvfrom(_multicastSocketIn,&data,sizeof(HiveData),0,(struct sockaddr *) &fromAddress,&addressLength)) >= 0) {
		if (data.jvmId < MAX_JVMS_IN_HIVE){
			if (data.jvmId != _jvmId){
				_hpiValues[data.jvmId] = data.currentHpi;
				_assignedPhysical[data.jvmId] = data.assignedPhysical;
			}
		} else {
			error("Invalid JVM id in hive message\n");
		}
	}
	if ((errno != EAGAIN) && (errno != EWOULDBLOCK)){
		error("Error while reading broadcasts to hive\n");
	}

	/* now do the roll up so that the summary numbers reflect the latest info from the hive */
	_totalHPI = 0;
	_totalPhysical = 0;
	for (i=0;i<MAX_JVMS_IN_HIVE;i++){
		_totalHPI = _totalHPI + _hpiValues[i];
		_totalPhysical = _totalPhysical + _assignedPhysical[i];
	}
}
#endif
