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

#include "MigrationAgent.hpp"

/* used to store reference to the agent */
static MigrationAgent* migrationAgent;


/*
 * default timein milliseconds that we wait after a check signal before deciding we are not likely to migrate after all
 * The number is currently quite high because the observed times are quite high
 */
#define DEFAULT_MIGRATE_WAIT_INTERVAL 200000

/* these are used when printing out verbose information about the state changes made by the agent */
static const char* STATE_IN_TEXT[] = {"WAITING","PRE_MIGRATE","MIGRATE_COMPLETE","MIGRATION_IMMINENT","PREPARED_FOR_MIGRATION","POST_MIGRATE"};

/*********************************************************
 * Methods exported for agent
 */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * handler which is registered and for dr_reconfig events
 * @param portLibrary[in] port library that can be used by the method
 * @param gpType indicates if signal was synchronous or asynchronous
 * @param gpInfo[in] signal info
 * @param userData[in] pointer to structure passed in when signal was set
 */
static UDATA
reconfigHandler(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* userData)
{
#if defined(AIXPPC)
	/* struct that will be used to hold info about the event when it occurs */
	dr_info_t dr;

	/* obtain event information from the OS.  */
	dr_reconfig(DR_QUERY, &dr);

	/* pass on event info  and then call dr_reconfig with the resulting ok or failure return code */
	return dr_reconfig( ((MigrationAgent*) userData)->reconfigSignalReceived(dr),&dr );
#endif

	return 0;
}

/**
 * callback invoked by JVMTI when the agent is unloaded
 * @param vm [in] jvm that can be used by the agent
 */
void JNICALL
Agent_OnUnload(JavaVM * vm)
{
	J9JavaVM * javaVM = ((J9InvocationJavaVM *)vm)->j9vm;
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	MigrationAgent* agent = MigrationAgent::getAgent(vm, &migrationAgent);
#if defined(AIXPPC)
	j9sig_set_async_signal_handler(reconfigHandler, (void*) migrationAgent,0);
#endif
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
	J9JavaVM * javaVM = ((J9InvocationJavaVM *)vm)->j9vm;
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	int result;
	MigrationAgent* agent = MigrationAgent::getAgent(vm, &migrationAgent);
	result = agent->setup(options);

#if defined(AIXPPC)
	if (JNI_OK == 0 ){
		j9sig_set_async_signal_handler(reconfigHandler, (void*) migrationAgent, J9PORT_SIG_FLAG_SIGRECONFIG);
	}
#endif

	return result;
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
	MigrationAgent* agent = MigrationAgent::getAgent(vm, &migrationAgent);
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
bool MigrationAgent::init(void)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());

	if (!RuntimeToolsIntervalAgent::init()){
		return false;
	}

	_stopping = false;
	_migrationState = STATE_WAITING;
	_preMigrateCount = 0;
	_migrateCount = 0;
	_postMigrateCount = 0;
	_waitForMigrateInterval = DEFAULT_MIGRATE_WAIT_INTERVAL;
	_quiet = false;
	_doGcForPreMigrationPrep = false;
	_adjustSoftmx = false;
	_forceSoftmx = false;
	_softMx = 0;
	_onetimeSoftmx = false;
	_haltThreadsForMigration = false;

	/* create monitor used signal that reconfig signal has been received */
	if (0 != omrthread_monitor_init_with_name(&_reconfigMonitor, 0, "migration agent - reconfig")){
		error("Failed to create monitor for migration agent");
		return false;
	}

	return true;

}

/**
 * This method should be called to destroy the object and free the associated memory
 */
void MigrationAgent::kill(void)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());

	omrthread_monitor_destroy(_reconfigMonitor);
	j9mem_free_memory(this);
}


/**
 * This method is used to create an instance
 *
 * @param vm [in] java vm that can be used by this manager
 * @returns an instance of the class
 */
MigrationAgent* MigrationAgent::newInstance(JavaVM * vm)
{
	J9JavaVM * javaVM = ((J9InvocationJavaVM *)vm)->j9vm;
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	MigrationAgent* obj = (MigrationAgent*) j9mem_allocate_memory(sizeof(MigrationAgent), OMRMEM_CATEGORY_VM);
	if (obj){
		new (obj) MigrationAgent(vm);
		if (!obj->init()){
			obj->kill();
			obj = NULL;
		}
	}
	return obj;
}

/**
 * Called when the vm stops
 * @param jvmti_env jvmti_evn that can be used by the agent
 * @param env JNIEnv that can be used by the agent
 */
void MigrationAgent::vmStop(jvmtiEnv *jvmti_env,JNIEnv* env)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());
	omrthread_monitor_enter(_reconfigMonitor);
	_stopping = true;
	omrthread_monitor_notify(_reconfigMonitor);
	omrthread_monitor_exit(_reconfigMonitor);
	RuntimeToolsIntervalAgent::vmStop(jvmti_env, env);
}


/**
 * method that generates the calls to runAction at the appropriate intervals *.
 */
void MigrationAgent::runAction(void)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());
	int currentStep = 0;
	I_64 currentStateStart = 0;
	_migrationState = STATE_WAITING;

	while(!_stopping){
		UDATA lastState = _migrationState;
		omrthread_monitor_enter(_reconfigMonitor);

		/* get rid of any signals that we can ignore because it is already too late */
		normalizeCounts();

		/* now wait if we need an event to occur before moving to the next state */
		while (   ( (STATE_WAITING == _migrationState) && (0 == _migrateCount) && (_preMigrateCount ==0))
			   || ( (STATE_PRE_MIGRATE_COMPLETE == _migrationState) && (_migrateCount == 0))
			   || ( (STATE_PREPARED_FOR_MIGRATION == _migrationState) && (_postMigrateCount == 0))

			  ){
			if ((STATE_PRE_MIGRATE_COMPLETE ==_migrationState)){
				/* after a preMigrate the migrate may or may not come.  Only wait for a certain amount of time */
				IDATA waitResult = 0;
				waitResult = omrthread_monitor_wait_timed(_reconfigMonitor, _waitForMigrateInterval, 0);
				if (J9THREAD_TIMED_OUT == waitResult){
					doAbortPreMigrate(currentStep);
					_migrationState = STATE_WAITING;
					break;
				}
			} else {
				omrthread_monitor_wait(_reconfigMonitor);
			}
			if (_stopping){
				break;
			}
			normalizeCounts();
		}

		normalizeCounts();

		/* figure out the next state */
		if (STATE_WAITING == _migrationState){
			/* first see if a migration is already started */
			if  (_migrateCount > 0){
				/* ok we need to prepare for migration */
				_preMigrateCount = 0;
				_migrateCount = _migrateCount - 1;
				_migrationState = STATE_MIGRATION_IMMINENT;
			} else if (_preMigrateCount > 0 ){
				_preMigrateCount = 0;
				_migrationState = STATE_PRE_MIGRATE;
			}
		} else if ((STATE_PRE_MIGRATE == _migrationState)||(STATE_PRE_MIGRATE_COMPLETE == _migrationState)){
			if (_migrateCount > 0){
				/* ok we need to prepare for migration */
				_preMigrateCount = 0;
				_migrateCount = _migrateCount - 1;
				_migrationState = STATE_MIGRATION_IMMINENT;
			}
		}  else if ((STATE_MIGRATION_IMMINENT == _migrationState)||(STATE_PREPARED_FOR_MIGRATION == _migrationState)){
			if (_postMigrateCount > 0){
				_postMigrateCount = _postMigrateCount - 1;
				_migrationState = STATE_POST_MIGRATE;
			}
		}
		omrthread_monitor_exit(_reconfigMonitor);

		/* capture step/start time based on state change */
		if (lastState == _migrationState){
			currentStep = currentStep + 1;
		} else {
			currentStep = 0;
			currentStateStart =  j9time_current_time_millis();
		}

		/* now take action based on next state */
		if (!_quiet){
			message("MigrationAgent");
			messageU64(j9time_current_time_millis());
			message(":Current State:");
			message(STATE_IN_TEXT[_migrationState]);
			message("\n");
		}
		if (STATE_PRE_MIGRATE == _migrationState){
			if ((j9time_current_time_millis() - currentStateStart) > _waitForMigrateInterval){
				/* the migration signal did not come within the expected time frame so assume migration was aborted
				 * if it comes later we will simply go directly to the migration imminent state */
				if (!_quiet){
					message("MigrationAgent");
					messageU64(j9time_current_time_millis());
					message(":Pre-migrate aborted\n");
				}
				doAbortPreMigrate(currentStep);
				_migrationState = STATE_WAITING;
			} else if (true == doNextPreMigrateStep(currentStep)){
				_migrationState = STATE_PRE_MIGRATE_COMPLETE;
			}
		} else if (STATE_MIGRATION_IMMINENT ==_migrationState){
			if (true == doNextFinalMigrateStep(currentStep)){
				_migrationState = STATE_PREPARED_FOR_MIGRATION;
			}
		} else if (STATE_POST_MIGRATE ==_migrationState){
			completePostMigration();
			_migrationState = STATE_WAITING;
		}
	}
}

/**
 * This is called once for each argument, the agent should parse the arguments
 * it supports and pass any that it does not understand to parent classes
 *
 * @param options string containing the argument
 *
 * @returns one of the AGENT_ERROR_ values
 */
jint MigrationAgent::parseArgument(char* option)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());
	jint rc = AGENT_ERROR_NONE;

	if (try_scan(&option,"quiet")){
			_quiet = true;
			return rc;
	} else if (try_scan(&option,"gcBeforeMigrate")){
		_doGcForPreMigrationPrep = true;
		return rc;
	} else if (try_scan(&option,"adjustSoftmx")){
		_adjustSoftmx = true;
		return rc;
	} else if (try_scan(&option,"forceSoftmx")){
		_forceSoftmx = true;
		return rc;
	} else if (try_scan(&option,"onetimeSoftmx")){
		_onetimeSoftmx = true;
		return rc;
	} else if (try_scan(&option,"haltThreads")){
		_haltThreadsForMigration = true;
		return rc;
	}else if (try_scan(&option,"maxMigrateWait:")){
			if (1 != sscanf(option,"%d",&_waitForMigrateInterval)){
				error("invalid maxMigrateWait value passed to agent\n");
				return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
			} else if (_waitForMigrateInterval < 0){
				error("negative maxMigrateWait is invalid\n");
				return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
			}
	}else {
		/* must give super class agent opportunity to handle option */
		rc = RuntimeToolsIntervalAgent::parseArgument(option);
	}

	return rc;
}



/**
 * this method is called by the handler registered to pass on information when
 * a reconfig signal is received
 *
 * @param dr [in] structure containing information about reconfig event
 * @return DR_RECONFIG_DONE if successful, DR_EVENT_FAIL  otherwise
 */
#if defined(AIXPPC)
int MigrationAgent::reconfigSignalReceived(dr_info_t dr)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());
	int result = DR_RECONFIG_DONE;

	if (dr.migrate){
		if (!_quiet){
			message("MigrationAgent:");
			messageU64(j9time_current_time_millis());
		}
		omrthread_monitor_enter(_reconfigMonitor);
		if (dr.check){
			_preMigrateCount = _preMigrateCount + 1;
			if (!_quiet){
				message(":Signal:MIGRATE-CHECK\n");
			}
		} else if (dr.pre){
			_migrateCount = _migrateCount + 1;
			if (!_quiet){
				message(":Signal:MIGRATE-NOW\n");
			}
		} else if (dr.post){
			_postMigrateCount = _postMigrateCount + 1;
			if (!_quiet){
				message(":Signal:MIGRATE-POST\n");
			}
		} else if (dr.posterror){
			if (!_quiet){
				message(":Signal:MIGRATE-POST-ERROR\n");
			}
		} else if (dr.force){
			if (!_quiet){
				message(":Signal:MIGRATE-FORCE\n");
			}
		} else {
			if (!_quiet){
				message(":Signal:MIGRATE-UNKNOWN\n");
			}
		}
		omrthread_monitor_notify(_reconfigMonitor);
		omrthread_monitor_exit(_reconfigMonitor);
	}

	return result;
}
#endif

/**
 * Normalize the counts to ignore events that we are already too late to deal with
 */
void MigrationAgent::normalizeCounts(void)
{
	while (_postMigrateCount >1){
		/* strip out each matching migrate/post migrate as we have already missed them */
		if (_migrateCount >0){
			_migrateCount = _migrateCount -1;
		}
		_postMigrateCount = _postMigrateCount -1;
	}

	if (_preMigrateCount >1){
		/* if we have multiple preMigrates pending, just reduce to 1 as we missed the earlier ones */
		_preMigrateCount = 1;
	}
}


/**
 * Does the next step for pre-migration work
 * @param step the next step to do
 * @return true if done, false if more steps to complete
 *
 */
bool MigrationAgent::doNextPreMigrateStep(int step)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());
	J9JavaVM *vm = getJavaVM();
	UDATA maxHeapSize = vm->memoryManagerFunctions->j9gc_get_maximum_heap_size(getJavaVM());
	UDATA heapFreeMemory = vm->memoryManagerFunctions->j9gc_heap_free_memory(vm);
	UDATA heapTotalMemory = heapTotalMemory = vm->memoryManagerFunctions->j9gc_heap_total_memory(vm);

	if ((0 == step) &&(_adjustSoftmx)){
		_softMx = OMR_MIN(maxHeapSize/2,OMR_MAX(heapTotalMemory - heapFreeMemory,maxHeapSize/16));
		vm->memoryManagerFunctions->j9gc_set_softmx( vm, _softMx);
		if (!_quiet){
			message("MigrationAgent:");
			messageU64(j9time_current_time_millis());
			message(":Softmx set:");
			messageU64(_softMx);
		}
	}

	if ((0==step)&&(_doGcForPreMigrationPrep)){
		vm->internalVMFunctions->internalAcquireVMAccess((J9VMThread *)getEnv());
		vm->memoryManagerFunctions->j9gc_modron_global_collect_with_overrides((J9VMThread*)getEnv(),  J9MMCONSTANT_IMPLICIT_GC_AGGRESSIVE);
		vm->internalVMFunctions->internalReleaseVMAccess((J9VMThread *)getEnv());
	}

	if (_onetimeSoftmx){
		if (!_quiet){
			message("MigrationAgent:");
			messageU64(j9time_current_time_millis());
			message(":Softmx reset due to one time option");
		}
	}

	if (_forceSoftmx){
		if (step == 0){
			heapFreeMemory = vm->memoryManagerFunctions->j9gc_heap_free_memory(vm);
			heapTotalMemory = heapTotalMemory = vm->memoryManagerFunctions->j9gc_heap_total_memory(vm);
			_lastTotalMemory = maxHeapSize+1;
		}
		if ((_lastTotalMemory > heapTotalMemory)&&(heapTotalMemory > _softMx)){
			vm->internalVMFunctions->internalAcquireVMAccess((J9VMThread *)getEnv());
			vm->memoryManagerFunctions->j9gc_modron_global_collect_with_overrides((J9VMThread*)getEnv(),  J9MMCONSTANT_IMPLICIT_GC_AGGRESSIVE);
			vm->internalVMFunctions->internalReleaseVMAccess((J9VMThread *)getEnv());
			_lastTotalMemory = heapTotalMemory;
		} else {
			return true;
		}
		return false;
	}
	return true;
}

/**
 * Indicates that we believe migration was aborted
 * @param step [in] the next step to do
 */
void MigrationAgent::doAbortPreMigrate(int step)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());
	J9JavaVM *vm = getJavaVM();
	if (_adjustSoftmx){
		vm->memoryManagerFunctions->j9gc_set_softmx( vm, vm->memoryManagerFunctions->j9gc_get_maximum_heap_size(getJavaVM()));
		if (!_quiet){
			message("MigrationAgent:");
			messageU64(j9time_current_time_millis());
			message(":Softmx reset on premigrate abort");
		}
	}
}

/**
 * Does the next step for final-migration work
 * @param step [in] the next step to do
 * @return true if done, false if more steps to complete
 *
 */
bool MigrationAgent::doNextFinalMigrateStep(int step)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());
	J9JavaVM *vm = getJavaVM();

	/* at this point we would rather avoid a gc so set the softmx back to the maximum */
	if (_adjustSoftmx){
		vm->memoryManagerFunctions->j9gc_set_softmx( vm,vm->memoryManagerFunctions->j9gc_get_maximum_heap_size(getJavaVM()) );
		if (!_quiet){
			message("MigrationAgent:");
			messageU64(j9time_current_time_millis());
			message(":Softmx reset in because migration is imminent");
		}
	}

	if (_haltThreadsForMigration){
		J9JavaVM *vm = getJavaVM();
		vm->internalVMFunctions->internalAcquireVMAccess((J9VMThread*)getEnv());
		vm->internalVMFunctions->acquireExclusiveVMAccess((J9VMThread*)getEnv());
	}
	return true;
}

/**
 * Does all post migration work
 */
void MigrationAgent::completePostMigration(void)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());
	J9JavaVM *vm = getJavaVM();

	if (_haltThreadsForMigration){
		vm->internalVMFunctions->releaseExclusiveVMAccess((J9VMThread*)getEnv());
		vm->internalVMFunctions->internalReleaseVMAccess((J9VMThread*)getEnv());
	}

}
